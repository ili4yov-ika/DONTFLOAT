#include "dontfloat_version.h"

#include "../core/dontfloat_plugin_core.h"
#include "../core/plugin_host_config.h"
#include "../ui/dontfloat_plugin_editor_shell.h"
#include "../ui/dontfloat_qt_hosting.h"
#if defined(DONTFLOAT_WITH_ARA)
#include "../ara/dontfloat_ara_document_controller.h"
#include "ARA_API/ARAVST3.h"
// Идентификаторы интерфейсов ARA для VST3 объявлены в заголовке, а определить
// их должен тот, кто их использует (как и прочие iid в VST3 SDK)
DEF_CLASS_IID(ARA::IMainFactory)
DEF_CLASS_IID(ARA::IPlugInEntryPoint)
DEF_CLASS_IID(ARA::IPlugInEntryPoint2)
#endif

#include <QSize>
#include <QCoreApplication>
#include <QMetaObject>
#include <QString>

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#if defined(DONTFLOAT_HAS_VST3_SDK)
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Steinberg {
namespace Vst {
#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
static const FUID DontfloatProcessorUid (0x7B1F68A1, 0x52D54D80, 0xA69C5B4E, 0xE1DA7201);
static const FUID DontfloatControllerUid (0xA1BFD4C3, 0x3E2D4E4D, 0x94C94972, 0xA7B31235);
static const FUID DontfloatAraFactoryUid (0x1D9F5A21, 0x4C8B4E10, 0x9A2F7B33, 0xC5E10001);
#define DONTFLOAT_VST3_ARA_FACTORY_NAME "DONTFLOAT ARA Factory"
#define DONTFLOAT_VST3_DISPLAY_NAME "DONTFLOAT"
#define DONTFLOAT_VST3_CONTROLLER_NAME "DONTFLOAT Controller"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
static const FUID DontfloatProcessorUid (0x8C2A79B2, 0x63E65E91, 0xB7AD6C5F, 0xF2EB8312);
static const FUID DontfloatControllerUid (0xB2C0E5D4, 0x4F3E5F5E, 0xA5DA5A83, 0xB8C42346);
static const FUID DontfloatAraFactoryUid (0x2E8A6B32, 0x5D9C5F21, 0xAB307C44, 0xD6F20002);
#define DONTFLOAT_VST3_ARA_FACTORY_NAME "DONTFLOAT Scratch ARA Factory"
#define DONTFLOAT_VST3_DISPLAY_NAME "DONTFLOAT Scratch"
#define DONTFLOAT_VST3_CONTROLLER_NAME "DONTFLOAT Scratch Controller"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
static const FUID DontfloatProcessorUid (0x9D3B8AC3, 0x74F76FA2, 0xC8BE7D70, 0x03FC9413);
static const FUID DontfloatControllerUid (0xC3D1F6E5, 0x604F706F, 0xB6EB6B94, 0xC9D53457);
static const FUID DontfloatAraFactoryUid (0x3F7B7C43, 0x6EAD6032, 0xBC418D55, 0xE7030003);
#define DONTFLOAT_VST3_ARA_FACTORY_NAME "DONTFLOAT Pitcher ARA Factory"
#define DONTFLOAT_VST3_DISPLAY_NAME "DONTFLOAT Pitcher"
#define DONTFLOAT_VST3_CONTROLLER_NAME "DONTFLOAT Pitcher Controller"
#else
#error "Invalid DONTFLOAT_PLUGIN_PRODUCT_INDEX"
#endif
} // namespace Vst
} // namespace Steinberg

namespace Dontfloat::Vst3 {

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::sharedSession;
using Dontfloat::PluginHost::desc;
using Dontfloat::PluginHost::product;
using Dontfloat::Plugins::Ui::DontfloatPluginEditorShell;
using Dontfloat::Plugins::Ui::ensureQtApplication;
using Steinberg::Vst::DontfloatControllerUid;
using Steinberg::Vst::DontfloatProcessorUid;

namespace {

constexpr Steinberg::int32 kEditorWidth = 960;
constexpr Steinberg::int32 kEditorHeight = 640;
/** Ниже этого редактор нечитаем — рамку хоста подтягиваем до минимума. */
constexpr Steinberg::int32 kEditorMinWidth = 640;
constexpr Steinberg::int32 kEditorMinHeight = 420;

/**
 * Открытые редакторы этого модуля. Процессор и вьюха — разные объекты VST3,
 * поэтому о приходе аудио редактор узнаёт через этот список (в CLAP плагин
 * держит редактор прямо в экземпляре). Уведомление уходит очередью Qt:
 * process() зовётся из аудиопотока хоста.
 */
std::mutex& editorsMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::vector<DontfloatPluginEditorShell*>& openEditors()
{
    static std::vector<DontfloatPluginEditorShell*> editors;
    return editors;
}

#if defined(DONTFLOAT_WITH_ARA)
/**
 * Привязка ARA живёт на процессоре, а редактор — отдельный объект VST3.
 * Держим последнюю привязку модуля, чтобы окно, открытое позже, тоже
 * получило модель, а привязка после открытия окна дошла до редакторов.
 */
const void*& boundAraExtension()
{
    static const void* extension = nullptr;
    return extension;
}
#endif

void registerEditor(DontfloatPluginEditorShell* editor)
{
    if (!editor) {
        return;
    }
    const std::lock_guard<std::mutex> lock(editorsMutex());
    openEditors().push_back(editor);
}

void unregisterEditor(DontfloatPluginEditorShell* editor)
{
    const std::lock_guard<std::mutex> lock(editorsMutex());
    auto& editors = openEditors();
    editors.erase(std::remove(editors.begin(), editors.end(), editor), editors.end());
}

/**
 * Каретка DAW → каретки редакторов. process() зовётся из аудиопотока, поэтому
 * позиция уходит в UI очередью Qt и склеивается: одно уведомление за раз.
 */
void notifyEditorsHostPlayhead(Steinberg::int64 samplePosition)
{
    static std::atomic_bool pending { false };
    if (pending.exchange(true)) {
        return;
    }

    const std::lock_guard<std::mutex> lock(editorsMutex());
    if (openEditors().empty()) {
        pending.store(false);
        return;
    }
    for (DontfloatPluginEditorShell* editor : openEditors()) {
        QMetaObject::invokeMethod(editor, [editor, samplePosition]() {
            pending.store(false);
            editor->setHostPlayhead(static_cast<qint64>(samplePosition));
        }, Qt::QueuedConnection);
    }
}

/** Тактовая сетка DAW → сетка редакторов (темп, доли, начало такта). */
void notifyEditorsHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
{
    static std::atomic_bool pending { false };
    if (pending.exchange(true)) {
        return;
    }

    const std::lock_guard<std::mutex> lock(editorsMutex());
    if (openEditors().empty()) {
        pending.store(false);
        return;
    }
    for (DontfloatPluginEditorShell* editor : openEditors()) {
        QMetaObject::invokeMethod(editor, [editor, bpm, beatsPerBar, barStartSample]() {
            pending.store(false);
            editor->setHostBeatGrid(bpm, beatsPerBar, barStartSample);
        }, Qt::QueuedConnection);
    }
}

void notifyEditorsHostAudioAppended()
{
    // Склейка: пока предыдущее уведомление не разобрано, новых не ставим —
    // иначе очередь UI забивается одним уведомлением на каждый блок аудио
    static std::atomic_bool pending { false };
    if (pending.exchange(true)) {
        return;
    }

    // Отправляемся в поток Qt через сам QApplication, а не через редактор:
    // очередь захвата надо разгрести и тогда, когда окна ещё нет, иначе
    // блоки копились бы в ней до переполнения и терялись
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        pending.store(false);
        return;
    }
    QMetaObject::invokeMethod(app, []() {
        pending.store(false);
        // Единственное место, где общий буфер меняется, — этот поток
        sharedSession(product()).drainHostCapture();
        const std::lock_guard<std::mutex> lock(editorsMutex());
        for (DontfloatPluginEditorShell* editor : openEditors()) {
            editor->notifyHostAudioAppended();
        }
    }, Qt::QueuedConnection);
}

} // namespace

class ProductEditorView final : public Steinberg::CPluginView {
public:
    ProductEditorView()
        : Steinberg::CPluginView()
    {
        setRect(Steinberg::ViewRect(0, 0, kEditorWidth, kEditorHeight));
    }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override
    {
#if defined(_WIN32)
        return std::strcmp(type, Steinberg::kPlatformTypeHWND) == 0
            ? Steinberg::kResultTrue
            : Steinberg::kResultFalse;
#else
        (void)type;
        return Steinberg::kResultFalse;
#endif
    }

    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override
    {
#if defined(_WIN32)
        if (!parent || std::strcmp(type, Steinberg::kPlatformTypeHWND) != 0) {
            return Steinberg::kInvalidArgument;
        }

        ensureQtApplication(desc().clapName);

        editor_ = std::make_unique<DontfloatPluginEditorShell>(product());
        editor_->bindSession(&sharedSession(product()));
#if defined(DONTFLOAT_WITH_ARA)
        if (const void* extension = boundAraExtension()) {
            // Хост шлёт выбор клипов только при открытом окне редактора
            if (auto* view = static_cast<const ARA::PlugIn::PlugInExtension*>(extension)
                                 ->getEditorView<Dontfloat::Ara::AraEditorView>()) {
                view->setEditorOpenState(true);
            }
            editor_->setAraBinding(extension);
        }
#endif
        editor_->setWindowTitle(QString::fromUtf8(desc().clapName));
        // Без рамки: хост двигает окно целиком, клиентская область должна
        // совпадать с ним (иначе по краям остаются пустые полосы)
        editor_->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        editor_->setAttribute(Qt::WA_NativeWindow, true);
        editor_->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
        editor_->resize(kEditorWidth, kEditorHeight);

        // Native HWND first, parent into host, then show — avoids reentrancy hangs
        // when the host waits for attached() while Qt waits for a message pump.
        (void)editor_->winId();
        const HWND child = reinterpret_cast<HWND>(editor_->winId());
        const HWND hostParent = reinterpret_cast<HWND>(parent);
        LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX
                   | WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= WS_CHILD | WS_VISIBLE;
        SetWindowLongPtrW(child, GWL_STYLE, style);
        SetParent(child, hostParent);
        MoveWindow(child, 0, 0, kEditorWidth, kEditorHeight, TRUE);
        ShowWindow(child, SW_SHOW);
        editor_->show();
        registerEditor(editor_.get());
        Dontfloat::Plugins::Ui::pumpQtEvents(8);

        return Steinberg::CPluginView::attached(parent, type);
#else
        (void)parent;
        (void)type;
        return Steinberg::kNotImplemented;
#endif
    }

    Steinberg::tresult PLUGIN_API removed() override
    {
#if defined(_WIN32)
#if defined(DONTFLOAT_WITH_ARA)
        if (const void* extension = boundAraExtension()) {
            if (auto* view = static_cast<const ARA::PlugIn::PlugInExtension*>(extension)
                                 ->getEditorView<Dontfloat::Ara::AraEditorView>()) {
                view->setEditorOpenState(false);
            }
        }
#endif
        if (editor_) {
            unregisterEditor(editor_.get());
            editor_->hide();
            SetParent(reinterpret_cast<HWND>(editor_->winId()), nullptr);
            editor_.reset();
        }
#endif
        return Steinberg::CPluginView::removed();
    }

    // Окно тянется хостом. Раньше вид был нерастяжимым (CPluginView::canResize
    // по умолчанию false), и редактор жил в DAW жёстким прямоугольником
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }

    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override
    {
        if (!rect) {
            return Steinberg::kInvalidArgument;
        }
        // Ниже минимума интерфейс нечитаем — подтягиваем рамку до него.
        // У собранного редактора минимум спрашиваем сам (в пикселях экрана)
        Steinberg::int32 minWidth = kEditorMinWidth;
        Steinberg::int32 minHeight = kEditorMinHeight;
        if (editor_) {
            const qreal dpr = editor_->devicePixelRatioF() > 0.0
                ? editor_->devicePixelRatioF()
                : 1.0;
            const QSize hint = editor_->minimumSizeHint().expandedTo(editor_->minimumSize());
            minWidth = Steinberg::int32(std::lround(hint.width() * dpr));
            minHeight = Steinberg::int32(std::lround(hint.height() * dpr));
        }
        if (rect->getWidth() < minWidth) {
            rect->right = rect->left + minWidth;
        }
        if (rect->getHeight() < minHeight) {
            rect->bottom = rect->top + minHeight;
        }
        return Steinberg::kResultTrue;
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override
    {
        const Steinberg::tresult result = Steinberg::CPluginView::onSize(newSize);
#if defined(_WIN32)
        if (editor_ && newSize) {
            const int width = std::max<int>(newSize->getWidth(), kEditorMinWidth);
            const int height = std::max<int>(newSize->getHeight(), kEditorMinHeight);
            // Хост считает в пикселях экрана, Qt — в логических: на мониторе
            // с масштабом 125/150% без деления окно вылезает за рамку хоста
            const qreal dpr = editor_->devicePixelRatioF() > 0.0
                ? editor_->devicePixelRatioF()
                : 1.0;
            editor_->resize(int(std::lround(double(width) / dpr)),
                            int(std::lround(double(height) / dpr)));
            MoveWindow(reinterpret_cast<HWND>(editor_->winId()), 0, 0, width, height, TRUE);
        }
#endif
        return result;
    }

    void bindSession(TrackToolSession*) {}

private:
    std::unique_ptr<DontfloatPluginEditorShell> editor_;
};

class ProductControllerVst3 final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IEditController*>(new ProductControllerVst3());
    }

    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override
    {
        if (name && std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
            auto* view = new ProductEditorView();
            view->bindSession(&sharedSession(product()));
            return view;
        }
        return nullptr;
    }
};

class ProductProcessorVst3 final : public Steinberg::Vst::AudioEffect
#if defined(DONTFLOAT_WITH_ARA)
    , public ARA::IPlugInEntryPoint
    , public ARA::IPlugInEntryPoint2
#endif
{
public:
    ProductProcessorVst3()
    {
        setControllerClass(DontfloatControllerUid);
    }

    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new ProductProcessorVst3());
    }

#if defined(DONTFLOAT_WITH_ARA)
    // --- ARA 2: хост берёт фабрику и привязывает экземпляр к документу ---
    const ARA::ARAFactory* PLUGIN_API getFactory() SMTG_OVERRIDE
    {
        return Dontfloat::Ara::AraDocumentController::getARAFactory();
    }

    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentController(
        ARA::ARADocumentControllerRef documentControllerRef) SMTG_OVERRIDE
    {
        // ARA 1 знала только эту форму привязки; роли считаем полными
        return bindToDocumentControllerWithRoles(documentControllerRef,
                                                 ARA::kARAPlaybackRendererRole
                                                     | ARA::kARAEditorRendererRole
                                                     | ARA::kARAEditorViewRole,
                                                 ARA::kARAPlaybackRendererRole
                                                     | ARA::kARAEditorRendererRole
                                                     | ARA::kARAEditorViewRole);
    }

    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles(
        ARA::ARADocumentControllerRef documentControllerRef,
        ARA::ARAPlugInInstanceRoleFlags knownRoles,
        ARA::ARAPlugInInstanceRoleFlags assignedRoles) SMTG_OVERRIDE
    {
        const ARA::ARAPlugInExtensionInstance* instance =
            araExtension_.bindToARA(documentControllerRef, knownRoles, assignedRoles);
        if (instance) {
            boundAraExtension() = &araExtension_;
            // Окна, открытые до привязки, узнают о модели сразу
            const std::lock_guard<std::mutex> lock(editorsMutex());
            for (DontfloatPluginEditorShell* editor : openEditors()) {
                QMetaObject::invokeMethod(editor, [editor]() {
                    editor->setAraBinding(boundAraExtension());
                }, Qt::QueuedConnection);
            }
        }
        return instance;
    }

    OBJ_METHODS(ProductProcessorVst3, Steinberg::Vst::AudioEffect)
    DEFINE_INTERFACES
        DEF_INTERFACE(ARA::IPlugInEntryPoint)
        DEF_INTERFACE(ARA::IPlugInEntryPoint2)
    END_DEFINE_INTERFACES(Steinberg::Vst::AudioEffect)
    REFCOUNT_METHODS(Steinberg::Vst::AudioEffect)
#endif

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override
    {
        const Steinberg::tresult result = AudioEffect::initialize(context);
        if (result != Steinberg::kResultOk) {
            return result;
        }

        addAudioInput(STR16("Track Input"), Steinberg::Vst::SpeakerArr::kStereo);
        addAudioOutput(STR16("Track Output"), Steinberg::Vst::SpeakerArr::kStereo);
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override
    {
        if (state) {
            sharedSession(product()).prepare(audioInfo_);
        }
        return AudioEffect::setActive(state);
    }

    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override
    {
        audioInfo_.sampleRate = std::max(1, int(setup.sampleRate));
        audioInfo_.channelCount = 2;
        audioInfo_.frameCount = std::max(1, int(setup.maxSamplesPerBlock));
        sharedSession(product()).prepare(audioInfo_);
        return AudioEffect::setupProcessing(setup);
    }

    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override
    {
        // Транспорт читаем и на пустых блоках: так хост шлёт чистые тики
        // позиции каретки, не добавляя аудио в сессию
        if (data.processContext
            && (data.processContext->state & Steinberg::Vst::ProcessContext::kPlaying)) {
            notifyEditorsHostPlayhead(data.processContext->projectTimeSamples);
        }

        // Тактовая сетка хоста: темп, доли в такте и начало текущего такта.
        // barPositionMusic — начало такта в четвертях от начала проекта.
        if (data.processContext
            && (data.processContext->state & Steinberg::Vst::ProcessContext::kTempoValid)
            && data.processContext->tempo > 0.0
            && data.processContext->sampleRate > 0.0) {
            const Steinberg::Vst::ProcessContext& context = *data.processContext;
            const int beatsPerBar =
                (context.state & Steinberg::Vst::ProcessContext::kTimeSigValid)
                    ? std::max(1, int(context.timeSigNumerator))
                    : 4;
            qint64 barStartSample = 0;
            if (context.state & Steinberg::Vst::ProcessContext::kBarPositionValid) {
                const double samplesPerQuarter = (60.0 / context.tempo) * context.sampleRate;
                barStartSample =
                    qint64(std::max(0.0, context.barPositionMusic * samplesPerQuarter));
            }
            notifyEditorsHostBeatGrid(context.tempo, beatsPerBar, barStartSample);
        }

        if (data.numSamples <= 0 || data.numOutputs <= 0 || data.outputs[0].numChannels <= 0) {
            return Steinberg::kResultOk;
        }

        const Steinberg::int32 inputChannels =
            data.numInputs > 0 ? data.inputs[0].numChannels : 0;
        const Steinberg::int32 outputChannels = data.outputs[0].numChannels;
        const Steinberg::int32 channelCount = std::min(inputChannels, outputChannels);

        for (Steinberg::int32 ch = 0; ch < outputChannels; ++ch) {
            float* out = data.outputs[0].channelBuffers32[ch];
            if (!out) {
                continue;
            }
            const float* in = (ch < channelCount) ? data.inputs[0].channelBuffers32[ch] : nullptr;
            if (in) {
                if (in != out) {
                    std::copy(in, in + data.numSamples, out);
                }
            } else {
                std::fill(out, out + data.numSamples, 0.0f);
            }
        }

        // На остановленном транспорте хост шлёт тишину на позиции курсора —
        // такой блок затёр бы захваченную дорожку, поэтому пишем только на ходу
        const bool transportStopped =
            data.processContext
            && !(data.processContext->state & Steinberg::Vst::ProcessContext::kPlaying);

        if (data.numInputs > 0 && data.inputs[0].channelBuffers32 && !transportStopped) {
            TrackToolSession& session = sharedSession(product());
            // Позиция блока на таймлайне: захват повторяет дорожку DAW, поэтому
            // перемещение клипа видно плагину как сдвиг содержимого
            const std::int64_t timelineFrame =
                data.processContext ? std::int64_t(data.processContext->projectTimeSamples) : -1;
            session.writeHostFrames(
                const_cast<const float* const*>(data.inputs[0].channelBuffers32),
                channelCount,
                data.numSamples,
                timelineFrame);
            // Редактор — отдельный объект VST3: сообщаем ему о новом аудио,
            // иначе он не покажет дорожку и не запустит анализ
            notifyEditorsHostAudioAppended();
        }

        // Обработанный звук отдаём в выход — строго после захвата: вход и
        // выход у хоста часто один буфер, иначе захватили бы свой же выход
        {
            TrackToolSession& session = sharedSession(product());
            const std::int64_t timelineFrame =
                data.processContext ? std::int64_t(data.processContext->projectTimeSamples) : -1;
            session.readRenderedOutput(data.outputs[0].channelBuffers32, outputChannels,
                                       data.numSamples, timelineFrame);
        }

        return Steinberg::kResultOk;
    }

private:
    TrackAudioInfo audioInfo_;
#if defined(DONTFLOAT_WITH_ARA)
    /** ARA-часть экземпляра: связь с общим document controller проекта. */
    ARA::PlugIn::PlugInExtension araExtension_;
#endif
};

#if defined(DONTFLOAT_WITH_ARA)
/**
 * Главная фабрика ARA: отдельный класс VST3-фабрики, по которому хост находит
 * ARA ещё до создания экземпляра плагина.
 */
class ProductAraMainFactoryVst3 final : public ARA::IMainFactory {
public:
    ProductAraMainFactoryVst3() { FUNKNOWN_CTOR }
    virtual ~ProductAraMainFactoryVst3() { FUNKNOWN_DTOR }

    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<ARA::IMainFactory*>(new ProductAraMainFactoryVst3());
    }

    const ARA::ARAFactory* PLUGIN_API getFactory() SMTG_OVERRIDE
    {
        return Dontfloat::Ara::AraDocumentController::getARAFactory();
    }

    DECLARE_FUNKNOWN_METHODS
};

IMPLEMENT_FUNKNOWN_METHODS(ProductAraMainFactoryVst3, ARA::IMainFactory, ARA::IMainFactory::iid)
#endif

} // namespace Dontfloat::Vst3

BEGIN_FACTORY_DEF("DONTFLOAT", "https://github.com/ili4yov-ika/DONTFLOAT", "mailto:support@dontfloat.local")

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::DontfloatProcessorUid),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           DONTFLOAT_VST3_DISPLAY_NAME,
           Vst::kDistributable,
           "Fx|Tools",
           DONTFLOAT_VERSION_STRING,
           kVstVersionString,
           Dontfloat::Vst3::ProductProcessorVst3::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::DontfloatControllerUid),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           DONTFLOAT_VST3_CONTROLLER_NAME,
           0,
           "",
           DONTFLOAT_VERSION_STRING,
           kVstVersionString,
           Dontfloat::Vst3::ProductControllerVst3::createInstance)

#if defined(DONTFLOAT_WITH_ARA)
// Класс главной фабрики ARA: по нему хост понимает, что плагин ARA-совместим
DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::DontfloatAraFactoryUid),
           PClassInfo::kManyInstances,
           kARAMainFactoryClass,
           DONTFLOAT_VST3_ARA_FACTORY_NAME,
           0,
           "",
           DONTFLOAT_VERSION_STRING,
           kVstVersionString,
           Dontfloat::Vst3::ProductAraMainFactoryVst3::createInstance)
#endif

END_FACTORY

#endif // DONTFLOAT_HAS_VST3_SDK
