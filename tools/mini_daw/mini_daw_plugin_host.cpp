#include "mini_daw_plugin_host.h"

#include "clap_minimal.h"
#include "lv2_minimal.h"
#include "plugin_product.h"

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QFileInfo>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#if defined(DONTFLOAT_HAS_VST3_SDK)
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace MiniDaw {
namespace {

using Dontfloat::PluginCore::productDescByIndex;

const Dontfloat::PluginCore::PluginProductDesc& descFor(PluginProduct product)
{
    return productDescByIndex(static_cast<int>(product));
}

#if defined(_WIN32)

/**
 * Модуль плагина. Загруженные модули кэшируются и НЕ выгружаются: плагин
 * тянет за собой Qt-DLL с мета-объектами и регистрациями типов, а выгрузка
 * такого модуля из Qt-хоста оставляет в Qt висячие указатели (падение в
 * Qt6Core при повторной загрузке — например при смене редакции плагина).
 */
class Module {
public:
    bool open(const QString& path, QString* error)
    {
        handle_ = nullptr;
        const QString key = QFileInfo(path).absoluteFilePath();
        auto& cache = loadedModules();
        const auto it = cache.find(key);
        if (it != cache.end()) {
            handle_ = it.value();
            return true;
        }

        SetLastError(0);
        // ALTERED_SEARCH_PATH: рядом с плагином лежат его impl-DLL и Qt
        HMODULE module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            if (error) {
                *error = QStringLiteral("LoadLibrary(%1) не удался: %2")
                             .arg(QDir::toNativeSeparators(path), lastErrorText());
            }
            return false;
        }
        cache.insert(key, module);
        handle_ = module;
        return true;
    }

    /** Отпускает ссылку на модуль; сам модуль остаётся в процессе (см. выше). */
    void close() { handle_ = nullptr; }

    template <typename T>
    T symbol(const char* name) const
    {
        return handle_ ? reinterpret_cast<T>(GetProcAddress(handle_, name)) : nullptr;
    }

    bool isOpen() const { return handle_ != nullptr; }

private:
    static QHash<QString, HMODULE>& loadedModules()
    {
        static QHash<QString, HMODULE> cache;
        return cache;
    }

    static QString lastErrorText()
    {
        const DWORD code = GetLastError();
        wchar_t* text = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                           | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<LPWSTR>(&text), 0, nullptr);
        QString message = text ? QString::fromWCharArray(text).trimmed()
                               : QStringLiteral("код %1").arg(code);
        if (text) {
            LocalFree(text);
        }
        return message;
    }

    HMODULE handle_ = nullptr;
};

/** Инициализированные точки входа CLAP: deinit не зовём (см. Module). */
QSet<const clap_plugin_entry_t*>& initialisedClapEntries()
{
    static QSet<const clap_plugin_entry_t*> entries;
    return entries;
}

// ---------------------------------------------------------------- CLAP ----

class ClapHost final : public PluginHost {
public:
    ~ClapHost() override { unload(); }

    bool load(const QString& path, PluginProduct product,
              double sampleRate, int blockSize, QString* error) override
    {
        unload();
        if (!module_.open(path, error)) {
            return false;
        }
        entry_ = module_.symbol<const clap_plugin_entry_t*>("clap_entry");
        if (!entry_ || !entry_->init || !entry_->get_factory) {
            fail(error, QStringLiteral("в модуле нет экспорта clap_entry"));
            return false;
        }
        // init() зовём один раз на модуль: парный deinit выгружает impl-DLL
        if (!initialisedClapEntries().contains(entry_)) {
            if (!entry_->init(path.toUtf8().constData())) {
                fail(error, QStringLiteral("clap_entry.init() вернул false"));
                entry_ = nullptr;
                return false;
            }
            initialisedClapEntries().insert(entry_);
        }

        const auto* factory = static_cast<const clap_plugin_factory_t*>(
            entry_->get_factory(CLAP_PLUGIN_FACTORY_ID));
        if (!factory || factory->get_plugin_count(factory) < 1) {
            fail(error, QStringLiteral("фабрика CLAP недоступна"));
            return false;
        }
        const clap_plugin_descriptor_t* descriptor = factory->get_plugin_descriptor(factory, 0);
        if (!descriptor) {
            fail(error, QStringLiteral("дескриптор CLAP недоступен"));
            return false;
        }
        const char* expectedId = descFor(product).clapId;
        if (descriptor->id && expectedId && std::strcmp(descriptor->id, expectedId) != 0) {
            fail(error, QStringLiteral("id плагина не совпадает: %1 вместо %2")
                            .arg(QString::fromUtf8(descriptor->id), QString::fromUtf8(expectedId)));
            return false;
        }
        displayName_ = QString::fromUtf8(descriptor->name ? descriptor->name : "CLAP");

        host_ = {};
        host_.clap_version = CLAP_VERSION_INIT;
        host_.name = "DONTFLOAT mini-DAW";
        host_.vendor = "DONTFLOAT";
        host_.url = "https://github.com/ili4yov-ika/DONTFLOAT";
        host_.version = "0.0.1";
        host_.host_data = this;
        host_.get_extension = [](const clap_host_t* host, const char* id) -> const void* {
            // Хост крутит свой Qt-цикл, clap.timer-support плагину не нужен.
            // Своё расширение транспорта отдаём: по нему плагин двигает каретку DAW
            if (id && std::strcmp(id, CLAP_EXT_DONTFLOAT_TRANSPORT) == 0) {
                static const clap_host_dontfloat_transport_t kTransport {
                    [](const clap_host_t* self, double seconds) {
                        auto* owner = static_cast<ClapHost*>(self->host_data);
                        if (owner && owner->seekHandler_ && owner->sampleRate_ > 0.0) {
                            owner->seekHandler_(qint64(seconds * owner->sampleRate_));
                        }
                    }
                };
                return &kTransport;
            }
            Q_UNUSED(host);
            return nullptr;
        };

        plugin_ = factory->create_plugin(factory, &host_, descriptor->id);
        if (!plugin_ || !plugin_->init(plugin_)) {
            fail(error, QStringLiteral("create_plugin/init не удался"));
            return false;
        }
        if (!plugin_->activate(plugin_, sampleRate, 1, uint32_t(blockSize))
            || !plugin_->start_processing(plugin_)) {
            fail(error, QStringLiteral("activate/start_processing не удался"));
            return false;
        }
        // Сессия продукта в impl-DLL переживает пересоздание экземпляра —
        // чистим её, иначе прошлый трек дособерётся к новому
        if (plugin_->reset) {
            plugin_->reset(plugin_);
        }
        processing_ = true;
        sampleRate_ = sampleRate;
        transport_ = {};
        setTransport(bpm_, beatsPerBar_);
        return true;
    }

    bool embedEditor(WId parent, QSize* editorSize, QString* error) override
    {
        if (!plugin_) {
            fail(error, QStringLiteral("плагин не загружен"));
            return false;
        }
        gui_ = static_cast<const clap_plugin_gui_t*>(plugin_->get_extension(plugin_, CLAP_EXT_GUI));
        if (!gui_) {
            fail(error, QStringLiteral("расширение clap.gui недоступно"));
            return false;
        }
        const char* api = CLAP_WINDOW_API_WIN32;
        if (!gui_->is_api_supported(plugin_, api, false) || !gui_->create(plugin_, api, false)) {
            gui_ = nullptr;
            fail(error, QStringLiteral("gui.create(win32) не удался"));
            return false;
        }
        guiCreated_ = true;

        uint32_t w = 960;
        uint32_t h = 640;
        gui_->get_size(plugin_, &w, &h);

        clap_window_t window {};
        window.api = api;
        window.win32 = reinterpret_cast<void*>(parent);
        if (!gui_->set_parent(plugin_, &window)) {
            fail(error, QStringLiteral("gui.set_parent не удался"));
            return false;
        }
        gui_->set_size(plugin_, w, h);
        gui_->show(plugin_);
        if (editorSize) {
            *editorSize = QSize(int(w), int(h));
        }
        return true;
    }

    void resizeEditor(QSize size) override
    {
        if (gui_ && guiCreated_ && size.isValid()) {
            gui_->set_size(plugin_, uint32_t(size.width()), uint32_t(size.height()));
        }
    }

    void process(float* left, float* right, int frames, qint64 timelineFrame) override
    {
        if (!plugin_ || !processing_ || frames <= 0) {
            return;
        }
        // Позиция блока на таймлайне — в транспорт: плагин пишет захват по ней
        if (timelineFrame >= 0 && sampleRate_ > 0.0) {
            updateTransportPosition(double(timelineFrame) / sampleRate_, true);
        }
        float* channels[2] = { left, right };
        clap_audio_buffer_t input {};
        input.data32 = channels;
        input.channel_count = 2;
        clap_audio_buffer_t output {};
        output.data32 = channels;  // обработка на месте
        output.channel_count = 2;

        clap_process_t process {};
        process.audio_inputs = &input;
        process.audio_outputs = &output;
        process.frames_count = uint32_t(frames);
        process.steady_time = steadyTime_;
        process.transport = &transport_;
        plugin_->process(plugin_, &process);
        steadyTime_ += frames;
    }

    void setTransport(double bpm, int beatsPerBar) override
    {
        bpm_ = bpm > 0.0 ? bpm : 120.0;
        beatsPerBar_ = std::max(1, beatsPerBar);
        transport_.tempo = bpm_;
        transport_.tsig_num = uint16_t(beatsPerBar_);
        transport_.tsig_denom = 4;
    }

    /** Заполняет транспорт позицией \a seconds (общая часть process/setPlayhead). */
    void updateTransportPosition(double seconds, bool playing)
    {
        transport_.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_TIME_SIGNATURE
            | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
        if (playing) {
            transport_.flags |= CLAP_TRANSPORT_IS_PLAYING;
        }
        transport_.song_pos_seconds = clap_sectime(seconds * CLAP_SECTIME_FACTOR);
        const double beats = seconds * (bpm_ / 60.0);
        transport_.song_pos_beats = clap_beattime(beats * CLAP_BEATTIME_FACTOR);

        // Тактовая сетка хоста начинается в нуле: границу такта плагин берёт
        // отсюда и рисует свою сетку по ней (см. syncEditorBeatGrid)
        const double barBeats = double(std::max(1, beatsPerBar_));
        const double barIndex = std::floor(beats / barBeats);
        transport_.bar_start = clap_beattime(barIndex * barBeats * CLAP_BEATTIME_FACTOR);
        transport_.bar_number = int32_t(barIndex);
    }

    void setPlayhead(qint64 frame, bool playing) override
    {
        if (!plugin_ || !processing_ || sampleRate_ <= 0.0) {
            return;
        }
        updateTransportPosition(double(frame) / sampleRate_, playing);

        // Пустой блок: плагин прочитает транспорт, но аудио не получит
        clap_audio_buffer_t input {};
        input.channel_count = 2;
        clap_audio_buffer_t output {};
        output.channel_count = 2;
        clap_process_t process {};
        process.audio_inputs = &input;
        process.audio_outputs = &output;
        process.frames_count = 0;
        process.steady_time = steadyTime_;
        process.transport = &transport_;
        plugin_->process(plugin_, &process);
    }

    void setSeekRequestHandler(std::function<void(qint64)> handler) override
    {
        seekHandler_ = std::move(handler);
    }

    void unload() override
    {
        if (gui_ && guiCreated_) {
            gui_->hide(plugin_);
            gui_->destroy(plugin_);
        }
        gui_ = nullptr;
        guiCreated_ = false;
        if (plugin_) {
            if (processing_) {
                plugin_->stop_processing(plugin_);
                plugin_->deactivate(plugin_);
            }
            plugin_->destroy(plugin_);
            plugin_ = nullptr;
        }
        processing_ = false;
        // entry_->deinit() не зовём: он выгрузит impl-DLL вместе с Qt-объектами
        entry_ = nullptr;
        steadyTime_ = 0;
        module_.close();
    }

    QString displayName() const override { return displayName_; }

    /** Запрос перемотки от плагина (см. расширение dontfloat.transport). */
    std::function<void(qint64)> seekHandler_;

private:
    void fail(QString* error, const QString& text)
    {
        if (error) {
            *error = text;
        }
        unload();
    }

    Module module_;
    const clap_plugin_entry_t* entry_ = nullptr;
    const clap_plugin_t* plugin_ = nullptr;
    const clap_plugin_gui_t* gui_ = nullptr;
    clap_host_t host_ {};
    bool processing_ = false;
    bool guiCreated_ = false;
    int64_t steadyTime_ = 0;
    double sampleRate_ = 0.0;
    double bpm_ = 120.0;
    int beatsPerBar_ = 4;
    clap_event_transport_t transport_ {};
    QString displayName_;
};

// ----------------------------------------------------------------- LV2 ----

class Lv2Host final : public PluginHost {
public:
    ~Lv2Host() override { unload(); }

    bool load(const QString& path, PluginProduct product,
              double sampleRate, int blockSize, QString* error) override
    {
        unload();
        const auto& meta = descFor(product);

        // path — бандл *.lv2 либо сам бинарник внутри него
        const QFileInfo info(path);
        bundleDir_ = info.isDir() ? path : info.absolutePath();
        const QString dsp = info.isDir()
            ? QDir(bundleDir_).filePath(QString::fromUtf8(meta.lv2BinaryBase) + QStringLiteral(".dll"))
            : path;
        if (!QFileInfo::exists(dsp)) {
            fail(error, QStringLiteral("бинарник LV2 не найден: %1").arg(QDir::toNativeSeparators(dsp)));
            return false;
        }
        if (!module_.open(dsp, error)) {
            return false;
        }

        using DescriptorFn = const LV2_Descriptor* (*)(uint32_t);
        auto descriptorFn = module_.symbol<DescriptorFn>("lv2_descriptor");
        if (!descriptorFn) {
            fail(error, QStringLiteral("в модуле нет экспорта lv2_descriptor"));
            return false;
        }
        descriptor_ = descriptorFn(0);
        if (!descriptor_ || !descriptor_->URI) {
            fail(error, QStringLiteral("дескриптор LV2 недоступен"));
            return false;
        }
        if (std::strcmp(descriptor_->URI, meta.lv2Uri) != 0) {
            fail(error, QStringLiteral("URI не совпадает: %1").arg(QString::fromUtf8(descriptor_->URI)));
            return false;
        }
        displayName_ = QString::fromUtf8(meta.clapName);

        // Хосты вроде Reaper передают именно bundle_path — держим тот же контракт
        const QByteArray bundle =
            QDir::toNativeSeparators(bundleDir_ + QLatin1Char('/')).toUtf8();
        instance_ = descriptor_->instantiate(descriptor_, sampleRate, bundle.constData(), nullptr);
        if (!instance_) {
            fail(error, QStringLiteral("instantiate() вернул null"));
            return false;
        }

        blockSize_ = std::max(16, blockSize);
        inL_.assign(std::size_t(blockSize_), 0.0f);
        inR_.assign(std::size_t(blockSize_), 0.0f);
        outL_.assign(std::size_t(blockSize_), 0.0f);
        outR_.assign(std::size_t(blockSize_), 0.0f);
        descriptor_->connect_port(instance_, 0, inL_.data());
        descriptor_->connect_port(instance_, 1, inR_.data());
        descriptor_->connect_port(instance_, 2, outL_.data());
        descriptor_->connect_port(instance_, 3, outR_.data());
        descriptor_->activate(instance_);
        active_ = true;
        product_ = product;
        return true;
    }

    bool embedEditor(WId parent, QSize* editorSize, QString* error) override
    {
        const auto& meta = descFor(product_);
        const QString uiBinary = QDir(bundleDir_)
            .filePath(QString::fromUtf8(meta.lv2UiBinaryBase) + QStringLiteral(".dll"));
        if (!QFileInfo::exists(uiBinary)) {
            fail(error, QStringLiteral("UI-бинарник LV2 не найден: %1")
                            .arg(QDir::toNativeSeparators(uiBinary)));
            return false;
        }
        if (!uiModule_.open(uiBinary, error)) {
            return false;
        }
        using UiDescriptorFn = const LV2UI_Descriptor* (*)(uint32_t);
        auto uiDescriptorFn = uiModule_.symbol<UiDescriptorFn>("lv2ui_descriptor");
        if (!uiDescriptorFn) {
            fail(error, QStringLiteral("в UI-модуле нет экспорта lv2ui_descriptor"));
            return false;
        }
        uiDescriptor_ = uiDescriptorFn(0);
        if (!uiDescriptor_ || !uiDescriptor_->instantiate) {
            fail(error, QStringLiteral("UI-дескриптор LV2 недоступен"));
            return false;
        }

        // WindowsUI: данные фичи ui:parent — само значение HWND
        LV2_Feature parentFeature { LV2_UI__parent, reinterpret_cast<void*>(parent) };
        const LV2_Feature* features[] = { &parentFeature, nullptr };

        LV2UI_Widget widget = nullptr;
        const QByteArray bundle =
            QDir::toNativeSeparators(bundleDir_ + QLatin1Char('/')).toUtf8();
        uiHandle_ = uiDescriptor_->instantiate(uiDescriptor_, meta.lv2Uri, bundle.constData(),
                                               nullptr, nullptr, &widget, features);
        if (!uiHandle_) {
            fail(error, QStringLiteral("UI instantiate() вернул null"));
            return false;
        }
        editorWindow_ = reinterpret_cast<HWND>(widget);
        if (uiDescriptor_->extension_data) {
            idle_ = static_cast<const LV2UI_Idle_Interface*>(
                uiDescriptor_->extension_data(LV2_UI__idleInterface));
        }
        if (editorSize && editorWindow_) {
            RECT rect {};
            if (GetWindowRect(editorWindow_, &rect)) {
                *editorSize = QSize(int(rect.right - rect.left), int(rect.bottom - rect.top));
            }
        }
        return true;
    }

    void resizeEditor(QSize size) override
    {
        if (editorWindow_ && size.isValid()) {
            MoveWindow(editorWindow_, 0, 0, size.width(), size.height(), TRUE);
        }
    }

    void setTransport(double, int) override
    {
        // LV2-транспорт идёт atom-событиями time:Position — пока не реализовано
    }

    void setPlayhead(qint64, bool) override
    {
        // Каретка LV2 приедет тем же time:Position (см. setTransport)
    }

    void process(float* left, float* right, int frames, qint64 timelineFrame) override
    {
        // Позиция таймлайна LV2 передаётся событием time:Position — не реализовано,
        // поэтому плагин пишет захват в конец (см. appendHostFrames)
        Q_UNUSED(timelineFrame);
        if (!instance_ || !active_ || frames <= 0) {
            return;
        }
        int done = 0;
        while (done < frames) {
            const int n = std::min(blockSize_, frames - done);
            std::copy_n(left + done, n, inL_.begin());
            std::copy_n(right + done, n, inR_.begin());
            descriptor_->run(instance_, uint32_t(n));
            std::copy_n(outL_.begin(), n, left + done);
            std::copy_n(outR_.begin(), n, right + done);
            done += n;
        }
    }

    void unload() override
    {
        if (uiDescriptor_ && uiHandle_ && uiDescriptor_->cleanup) {
            uiDescriptor_->cleanup(uiHandle_);
        }
        uiHandle_ = nullptr;
        uiDescriptor_ = nullptr;
        idle_ = nullptr;
        editorWindow_ = nullptr;
        uiModule_.close();

        if (descriptor_ && instance_) {
            if (active_) {
                descriptor_->deactivate(instance_);
            }
            descriptor_->cleanup(instance_);
        }
        instance_ = nullptr;
        descriptor_ = nullptr;
        active_ = false;
        module_.close();
    }

    QString displayName() const override { return displayName_; }

private:
    void fail(QString* error, const QString& text)
    {
        if (error) {
            *error = text;
        }
        unload();
    }

    Module module_;
    Module uiModule_;
    const LV2_Descriptor* descriptor_ = nullptr;
    const LV2UI_Descriptor* uiDescriptor_ = nullptr;
    LV2_Handle instance_ = nullptr;
    LV2UI_Handle uiHandle_ = nullptr;
    const LV2UI_Idle_Interface* idle_ = nullptr;
    HWND editorWindow_ = nullptr;
    QString bundleDir_;
    QString displayName_;
    PluginProduct product_ = PluginProduct::Full;
    bool active_ = false;
    int blockSize_ = 512;
    std::vector<float> inL_, inR_, outL_, outR_;
};

// ---------------------------------------------------------------- VST3 ----

#if defined(DONTFLOAT_HAS_VST3_SDK)

/** Минимальный IPlugFrame: плагин просит изменить размер своего окна. */
class HostPlugFrame final : public Steinberg::IPlugFrame {
public:
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::IPlugFrame::iid)
            || Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid)) {
            addRef();
            *obj = static_cast<Steinberg::IPlugFrame*>(this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override
    {
        if (--refCount_ == 0) {
            delete this;
            return 0;
        }
        return refCount_;
    }

    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect* newSize) override
    {
        if (view && newSize) {
            view->onSize(newSize);
        }
        return Steinberg::kResultTrue;
    }

private:
    std::atomic<Steinberg::uint32> refCount_ { 1 };
};

/**
 * VST3-хост поверх Steinberg SDK: модуль грузится через VST3::Hosting::Module,
 * компонент и контроллер создаются фабрикой и соединяются, редактор
 * (IPlugView) прикрепляется к HWND панели, аудио идёт через IAudioProcessor.
 */
class Vst3Host final : public PluginHost {
public:
    ~Vst3Host() override { unload(); }

    bool load(const QString& path, PluginProduct product,
              double sampleRate, int blockSize, QString* error) override
    {
        unload();

        // Модуль грузим сами (как probe и остальные бэкенды): загрузчик SDK
        // возвращает отказ без описания, а нам нужен ещё и кэш модулей
        const QFileInfo bundleInfo(path);
        const QString modulePath = bundleInfo.isDir()
            ? QDir(path).filePath(QStringLiteral("Contents/x86_64-win/%1").arg(bundleInfo.fileName()))
            : path;
        if (!QFileInfo::exists(modulePath)) {
            fail(error, QStringLiteral("модуль не найден: %1")
                            .arg(QDir::toNativeSeparators(modulePath)));
            return false;
        }
        if (!module_.open(modulePath, error)) {
            return false;
        }

        using InitDllFn = bool (*)();
        using GetFactoryFn = Steinberg::IPluginFactory* (*)();
        auto initDll = module_.symbol<InitDllFn>("InitDll");
        auto getFactory = module_.symbol<GetFactoryFn>("GetPluginFactory");
        if (!getFactory) {
            fail(error, QStringLiteral("в модуле нет экспорта GetPluginFactory"));
            return false;
        }
        if (initDll && !initDll()) {
            fail(error, QStringLiteral("InitDll() вернул false"));
            return false;
        }
        Steinberg::IPluginFactory* rawFactory = getFactory();
        if (!rawFactory) {
            fail(error, QStringLiteral("GetPluginFactory() вернул null"));
            return false;
        }

        // Классы перебираем сырым IPluginFactory: VST3::Hosting::PluginFactory
        // ::classInfos() при неудачном getClassInfo зовёт back() на пустом
        // векторе и пишет в мусор — на редакции Full это валило хост
        factory_ = Steinberg::owned(rawFactory);
        Steinberg::PClassInfo effectInfo {};
        bool found = false;
        const Steinberg::int32 classCount = factory_->countClasses();
        for (Steinberg::int32 i = 0; i < classCount; ++i) {
            Steinberg::PClassInfo info {};
            if (factory_->getClassInfo(i, &info) != Steinberg::kResultOk) {
                continue;
            }
            if (std::strcmp(info.category, kVstAudioEffectClass) == 0) {
                effectInfo = info;
                found = true;
                break;
            }
        }
        if (!found) {
            fail(error, QStringLiteral("в фабрике нет класса аудио-эффекта (классов: %1)")
                            .arg(classCount));
            return false;
        }
        // name — фиксированный ASCII-буфер PClassInfo, читаем его безопасно
        displayName_ = QString::fromUtf8(
            effectInfo.name, int(qstrnlen(effectInfo.name, sizeof(effectInfo.name))));
        if (displayName_.isEmpty()) {
            displayName_ = QString::fromUtf8(descFor(product).vst3DisplayName);
        }

        Steinberg::Vst::IComponent* rawComponent = nullptr;
        if (factory_->createInstance(effectInfo.cid, Steinberg::Vst::IComponent::iid,
                                     reinterpret_cast<void**>(&rawComponent)) != Steinberg::kResultOk
            || !rawComponent) {
            fail(error, QStringLiteral("не создался IComponent"));
            return false;
        }
        component_ = Steinberg::owned(rawComponent);
        if (component_->initialize(hostContext_) != Steinberg::kResultOk) {
            fail(error, QStringLiteral("IComponent::initialize не удался"));
            return false;
        }
        componentInitialised_ = true;

        processor_ = Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor>(component_);
        if (!processor_) {
            fail(error, QStringLiteral("компонент не отдал IAudioProcessor"));
            return false;
        }

        // Контроллер: отдельный класс (как у DONTFLOAT) либо сам компонент
        Steinberg::TUID controllerCid {};
        if (component_->getControllerClassId(controllerCid) == Steinberg::kResultTrue) {
            Steinberg::Vst::IEditController* rawController = nullptr;
            if (factory_->createInstance(controllerCid, Steinberg::Vst::IEditController::iid,
                                         reinterpret_cast<void**>(&rawController))
                    == Steinberg::kResultOk
                && rawController) {
                controller_ = Steinberg::owned(rawController);
            }
            if (controller_ && controller_->initialize(hostContext_) == Steinberg::kResultOk) {
                controllerInitialised_ = true;
                connectComponentAndController();
            }
        }
        if (!controller_) {
            controller_ = Steinberg::FUnknownPtr<Steinberg::Vst::IEditController>(component_);
        }

        Steinberg::Vst::ProcessSetup setup {};
        setup.processMode = Steinberg::Vst::kRealtime;
        setup.symbolicSampleSize = Steinberg::Vst::kSample32;
        setup.maxSamplesPerBlock = std::max(16, blockSize);
        setup.sampleRate = sampleRate;
        if (processor_->setupProcessing(setup) != Steinberg::kResultOk) {
            fail(error, QStringLiteral("setupProcessing не удался"));
            return false;
        }

        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, true);
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, true);
        if (component_->setActive(true) != Steinberg::kResultOk) {
            fail(error, QStringLiteral("setActive не удался"));
            return false;
        }
        processor_->setProcessing(true);
        processing_ = true;

        context_ = {};
        context_.sampleRate = sampleRate;
        context_.state = Steinberg::Vst::ProcessContext::kPlaying
            | Steinberg::Vst::ProcessContext::kTempoValid
            | Steinberg::Vst::ProcessContext::kTimeSigValid;
        setTransport(bpm_, beatsPerBar_);
        Q_UNUSED(product);
        return true;
    }

    bool embedEditor(WId parent, QSize* editorSize, QString* error) override
    {
        if (!controller_) {
            fail(error, QStringLiteral("плагин не отдал IEditController"));
            return false;
        }
        view_ = controller_->createView(Steinberg::Vst::ViewType::kEditor);
        if (!view_) {
            fail(error, QStringLiteral("createView(editor) вернул null"));
            return false;
        }
        if (view_->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND) != Steinberg::kResultTrue) {
            fail(error, QStringLiteral("редактор не поддерживает HWND"));
            return false;
        }

        frame_ = new HostPlugFrame();
        view_->setFrame(frame_);
        if (view_->attached(reinterpret_cast<void*>(parent), Steinberg::kPlatformTypeHWND)
            != Steinberg::kResultOk) {
            fail(error, QStringLiteral("IPlugView::attached не удался"));
            return false;
        }
        attached_ = true;

        Steinberg::ViewRect rect {};
        if (view_->getSize(&rect) == Steinberg::kResultOk && editorSize) {
            *editorSize = QSize(rect.getWidth(), rect.getHeight());
        }
        return true;
    }

    void resizeEditor(QSize size) override
    {
        if (!view_ || !attached_ || !size.isValid()) {
            return;
        }
        Steinberg::ViewRect rect(0, 0, size.width(), size.height());
        // checkSizeConstraint только уточняет размер: гейтить им onSize нельзя,
        // иначе вьюхи без этой проверки остаются в геометрии момента attached()
        view_->checkSizeConstraint(&rect);
        view_->onSize(&rect);
    }

    void process(float* left, float* right, int frames, qint64 timelineFrame) override
    {
        if (!processor_ || !processing_ || frames <= 0) {
            return;
        }
        // Позиция блока на таймлайне — в ProcessContext: по ней плагин пишет
        // захват (и только на «играющем» транспорте, см. обёртку плагина)
        if (timelineFrame >= 0) {
            context_.projectTimeSamples = Steinberg::Vst::TSamples(timelineFrame);
            context_.state |= Steinberg::Vst::ProcessContext::kPlaying;
        }
        updateMusicalPosition();
        float* channels[2] = { left, right };
        Steinberg::Vst::AudioBusBuffers input {};
        input.numChannels = 2;
        input.channelBuffers32 = channels;
        Steinberg::Vst::AudioBusBuffers output {};
        output.numChannels = 2;
        output.channelBuffers32 = channels;  // обработка на месте

        Steinberg::Vst::ProcessData data {};
        data.processMode = Steinberg::Vst::kRealtime;
        data.symbolicSampleSize = Steinberg::Vst::kSample32;
        data.numSamples = frames;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &input;
        data.outputs = &output;
        data.inputParameterChanges = &parameterChanges_;
        data.outputParameterChanges = &outputParameterChanges_;
        data.processContext = &context_;

        processor_->process(data);
        outputParameterChanges_.clearQueue();

        context_.projectTimeSamples += frames;
        context_.continousTimeSamples += frames;
    }

    void setTransport(double bpm, int beatsPerBar) override
    {
        bpm_ = bpm > 0.0 ? bpm : 120.0;
        beatsPerBar_ = std::max(1, beatsPerBar);
        context_.tempo = bpm_;
        context_.timeSigNumerator = beatsPerBar_;
        context_.timeSigDenominator = 4;
        updateMusicalPosition();
    }

    /** Музыкальное время и начало такта — по ним плагин строит свою сетку. */
    void updateMusicalPosition()
    {
        if (context_.sampleRate <= 0.0 || bpm_ <= 0.0) {
            return;
        }
        const double quarters =
            (double(context_.projectTimeSamples) / context_.sampleRate) * (bpm_ / 60.0);
        const double barQuarters = double(std::max(1, beatsPerBar_));
        context_.projectTimeMusic = quarters;
        context_.barPositionMusic = std::floor(quarters / barQuarters) * barQuarters;
        context_.state |= Steinberg::Vst::ProcessContext::kProjectTimeMusicValid
            | Steinberg::Vst::ProcessContext::kBarPositionValid;
    }

    void setPlayhead(qint64 frame, bool playing) override
    {
        if (!processor_ || !processing_) {
            return;
        }
        context_.projectTimeSamples = frame;
        context_.continousTimeSamples = frame;
        updateMusicalPosition();
        if (playing) {
            context_.state |= Steinberg::Vst::ProcessContext::kPlaying;
        } else {
            context_.state &= ~Steinberg::Vst::ProcessContext::kPlaying;
        }

        // Пустой блок: плагин прочитает ProcessContext, но аудио не получит
        Steinberg::Vst::ProcessData data {};
        data.processMode = Steinberg::Vst::kRealtime;
        data.symbolicSampleSize = Steinberg::Vst::kSample32;
        data.numSamples = 0;
        data.numInputs = 0;
        data.numOutputs = 0;
        data.inputParameterChanges = &parameterChanges_;
        data.outputParameterChanges = &outputParameterChanges_;
        data.processContext = &context_;
        processor_->process(data);
        outputParameterChanges_.clearQueue();
    }

    void unload() override
    {
        if (view_) {
            if (attached_) {
                view_->removed();
            }
            view_->setFrame(nullptr);
            view_->release();
            view_ = nullptr;
        }
        attached_ = false;
        if (frame_) {
            frame_->release();
            frame_ = nullptr;
        }

        if (processor_ && processing_) {
            processor_->setProcessing(false);
        }
        processing_ = false;
        if (component_) {
            component_->setActive(false);
        }
        disconnectComponentAndController();
        if (controller_ && controllerInitialised_) {
            controller_->terminate();
        }
        controllerInitialised_ = false;
        controller_ = nullptr;
        processor_ = nullptr;
        if (component_ && componentInitialised_) {
            component_->terminate();
        }
        componentInitialised_ = false;
        component_ = nullptr;
        factory_ = nullptr;
        // Модуль остаётся в процессе (кэш Module) — он тянет Qt-DLL плагина
        module_.close();
    }

    QString displayName() const override { return displayName_; }

private:
    void connectComponentAndController()
    {
        componentConnection_ =
            Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint>(component_);
        controllerConnection_ =
            Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint>(controller_);
        if (componentConnection_ && controllerConnection_) {
            componentConnection_->connect(controllerConnection_);
            controllerConnection_->connect(componentConnection_);
        }
    }

    void disconnectComponentAndController()
    {
        if (componentConnection_ && controllerConnection_) {
            componentConnection_->disconnect(controllerConnection_);
            controllerConnection_->disconnect(componentConnection_);
        }
        componentConnection_ = nullptr;
        controllerConnection_ = nullptr;
    }

    void fail(QString* error, const QString& text)
    {
        if (error) {
            *error = text;
        }
        unload();
    }

    Module module_;
    /**
     * Контекст хоста — COM-объект со счётчиком ссылок: плагин его удерживает и
     * освобождает, поэтому он живёт в куче под IPtr, а не полем по значению
     * (иначе release() удаляет объект-член и рушит кучу).
     */
    Steinberg::IPtr<Steinberg::Vst::HostApplication> hostContext_ {
        Steinberg::owned(new Steinberg::Vst::HostApplication) };
    Steinberg::IPtr<Steinberg::IPluginFactory> factory_;
    Steinberg::IPtr<Steinberg::Vst::IComponent> component_;
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller_;
    Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor> processor_;
    Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> componentConnection_;
    Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> controllerConnection_;
    Steinberg::IPlugView* view_ = nullptr;
    HostPlugFrame* frame_ = nullptr;
    Steinberg::Vst::ProcessContext context_ {};
    Steinberg::Vst::ParameterChanges parameterChanges_;
    Steinberg::Vst::ParameterChanges outputParameterChanges_;
    QString displayName_;
    double bpm_ = 120.0;
    int beatsPerBar_ = 4;
    bool processing_ = false;
    bool attached_ = false;
    bool componentInitialised_ = false;
    bool controllerInitialised_ = false;
};

#else  // DONTFLOAT_HAS_VST3_SDK

/** Без Steinberg SDK VST3 не хостится — честно сообщаем об этом. */
class Vst3Host final : public PluginHost {
public:
    bool load(const QString& path, PluginProduct, double, int, QString* error) override
    {
        if (error) {
            *error = QStringLiteral("сборка без Steinberg VST3 SDK — модуль %1 не загрузить")
                         .arg(QDir::toNativeSeparators(path));
        }
        return false;
    }

    bool embedEditor(WId, QSize*, QString* error) override
    {
        if (error) {
            *error = QStringLiteral("сборка без Steinberg VST3 SDK");
        }
        return false;
    }

    void resizeEditor(QSize) override {}
    void process(float*, float*, int) override {}
    void setTransport(double, int) override {}
    void unload() override {}
    QString displayName() const override { return QStringLiteral("VST3"); }
};

#endif // DONTFLOAT_HAS_VST3_SDK

#endif // _WIN32

} // namespace

std::unique_ptr<PluginHost> createPluginHost(PluginFormat format)
{
#if defined(_WIN32)
    switch (format) {
    case PluginFormat::Clap: return std::make_unique<ClapHost>();
    case PluginFormat::Lv2:  return std::make_unique<Lv2Host>();
    case PluginFormat::Vst3: return std::make_unique<Vst3Host>();
    }
#else
    Q_UNUSED(format);
#endif
    return nullptr;
}

} // namespace MiniDaw
