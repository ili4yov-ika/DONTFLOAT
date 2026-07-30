#include "plugin_host_probe.h"

#include "../../plugins/clap/clap_minimal.h"
#include "../../plugins/lv2/lv2_minimal.h"

#include <QDir>
#include <QFileInfo>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Dontfloat::PluginTester {
namespace {

struct ProductMeta {
    const char* clapId;
    const char* clapFile;
    const char* lv2Uri;
    const char* lv2Bundle;
    const char* lv2Binary;
    const char* vst3Bundle;
    const char* displayName;
};

const ProductMeta& meta(PluginProduct product)
{
    static const ProductMeta kMeta[] = {
        {
            "com.dontfloat.full",
            "dontfloat.clap",
            "https://github.com/ili4yov-ika/DONTFLOAT/plugins/full",
            "dontfloat.lv2",
            "dontfloat.dll",
            "DONTFLOAT.vst3",
            "DONTFLOAT",
        },
        {
            "com.dontfloat.scratch",
            "dontfloat_scratch.clap",
            "https://github.com/ili4yov-ika/DONTFLOAT/plugins/scratch",
            "dontfloat_scratch.lv2",
            "dontfloat_scratch.dll",
            "DONTFLOAT Scratch.vst3",
            "DONTFLOAT Scratch",
        },
        {
            "com.dontfloat.pitcher",
            "dontfloat_pitcher.clap",
            "https://github.com/ili4yov-ika/DONTFLOAT/plugins/pitcher",
            "dontfloat_pitcher.lv2",
            "dontfloat_pitcher.dll",
            "DONTFLOAT Pitcher.vst3",
            "DONTFLOAT Pitcher",
        },
    };
    return kMeta[static_cast<int>(product)];
}

QString commonFilesRoot()
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH] = {};
    const UINT n = GetEnvironmentVariableW(L"COMMONPROGRAMFILES", buffer, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return QString::fromWCharArray(buffer);
    }
#endif
    return QStringLiteral("C:/Program Files/Common Files");
}

#if defined(_WIN32)
class ScopedModule {
public:
    explicit ScopedModule(const QString& path)
    {
        SetLastError(0);
        handle_ = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        error_ = GetLastError();
    }
    ~ScopedModule()
    {
        if (handle_) {
            FreeLibrary(handle_);
        }
    }
    HMODULE get() const { return handle_; }
    bool ok() const { return handle_ != nullptr; }
    QString lastError() const
    {
        wchar_t* msg = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                           | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, error_, 0, reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
        QString text = msg ? QString::fromWCharArray(msg).trimmed()
                           : QStringLiteral("LoadLibrary failed (%1)").arg(error_);
        if (msg) {
            LocalFree(msg);
        }
        return text;
    }

private:
    HMODULE handle_ = nullptr;
    DWORD error_ = 0;
};

template <typename T>
T getProc(HMODULE module, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(module, name));
}
#endif

bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) <= 1.0e-6f;
}

ProbeResult probeClap(PluginProduct product, const QString& path)
{
    ProbeResult result;
    result.path = path;
#if !defined(_WIN32)
    result.summary = QStringLiteral("CLAP probe is only implemented on Windows");
    return result;
#else
    ScopedModule module(path);
    if (!module.ok()) {
        result.summary = QStringLiteral("LoadLibrary failed: %1").arg(module.lastError());
        return result;
    }
    result.details << QStringLiteral("LoadLibrary OK");

    auto* entry = getProc<const clap_plugin_entry_t*>(module.get(), "clap_entry");
    if (!entry || !entry->init || !entry->get_factory) {
        result.summary = QStringLiteral("Missing clap_entry export");
        return result;
    }
    result.details << QStringLiteral("clap_entry export OK");

    if (!entry->init(path.toUtf8().constData())) {
        result.summary = QStringLiteral("clap_entry.init failed");
        return result;
    }

    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) < 1) {
        entry->deinit();
        result.summary = QStringLiteral("CLAP factory unavailable");
        return result;
    }

    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    const ProductMeta& m = meta(product);
    if (!desc || std::strcmp(desc->id, m.clapId) != 0) {
        entry->deinit();
        result.summary = QStringLiteral("CLAP id mismatch (got %1)")
                             .arg(desc ? QString::fromUtf8(desc->id) : QStringLiteral("<null>"));
        return result;
    }
    result.details << QStringLiteral("Descriptor: %1 / %2")
                          .arg(QString::fromUtf8(desc->id), QString::fromUtf8(desc->name));

    clap_host_t host {};
    host.clap_version = CLAP_VERSION_INIT;
    host.name = "DONTFLOAT Plugin Tester";
    host.vendor = "DONTFLOAT";
    host.url = "https://github.com/ili4yov-ika/DONTFLOAT";
    host.version = "0.0.1";
    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, desc->id);
    if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, 48000.0, 32, 128)
        || !plugin->start_processing(plugin)) {
        if (plugin) {
            plugin->destroy(plugin);
        }
        entry->deinit();
        result.summary = QStringLiteral("CLAP lifecycle failed");
        return result;
    }

    constexpr uint32_t frames = 64;
    std::vector<float> inL(frames, 0.25f);
    std::vector<float> inR(frames, -0.25f);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);
    float* inPtrs[] = {inL.data(), inR.data()};
    float* outPtrs[] = {outL.data(), outR.data()};
    clap_audio_buffer_t input {};
    input.data32 = inPtrs;
    input.channel_count = 2;
    clap_audio_buffer_t output {};
    output.data32 = outPtrs;
    output.channel_count = 2;
    clap_process_t process {};
    process.frames_count = frames;
    process.audio_inputs = &input;
    process.audio_outputs = &output;

    const clap_process_status status = plugin->process(plugin, &process);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();

    if (status != CLAP_PROCESS_CONTINUE || !nearlyEqual(outL[0], 0.25f) || !nearlyEqual(outR[0], -0.25f)) {
        result.summary = QStringLiteral("CLAP process/passthrough failed");
        return result;
    }

    result.ok = true;
    result.summary = QStringLiteral("CLAP OK — %1").arg(QString::fromUtf8(m.displayName));
    result.details << QStringLiteral("Process passthrough OK (%1 frames)").arg(frames);
    return result;
#endif
}

ProbeResult probeVst3(PluginProduct product, const QString& path)
{
    ProbeResult result;
    result.path = path;
#if !defined(_WIN32)
    result.summary = QStringLiteral("VST3 probe is only implemented on Windows");
    return result;
#else
    QString modulePath = path;
    const QFileInfo info(path);
    if (info.isDir()) {
        modulePath = QDir(path).filePath(
            QStringLiteral("Contents/x86_64-win/%1").arg(meta(product).vst3Bundle));
    }
    result.path = modulePath;
    if (!QFileInfo::exists(modulePath)) {
        result.summary = QStringLiteral("VST3 module not found: %1").arg(modulePath);
        return result;
    }

    ScopedModule module(modulePath);
    if (!module.ok()) {
        result.summary = QStringLiteral("LoadLibrary failed: %1").arg(module.lastError());
        return result;
    }
    result.details << QStringLiteral("LoadLibrary OK");

    using InitDllFn = bool (*)();
    using ExitDllFn = bool (*)();
    using GetFactoryFn = void* (*)();

    auto initDll = getProc<InitDllFn>(module.get(), "InitDll");
    auto exitDll = getProc<ExitDllFn>(module.get(), "ExitDll");
    auto getFactory = getProc<GetFactoryFn>(module.get(), "GetPluginFactory");

    if (!getFactory) {
        result.summary = QStringLiteral("Missing GetPluginFactory export");
        return result;
    }
    result.details << QStringLiteral("GetPluginFactory export OK");

    if (initDll) {
        if (!initDll()) {
            result.summary = QStringLiteral("InitDll returned false");
            return result;
        }
        result.details << QStringLiteral("InitDll OK");
    } else {
        result.details << QStringLiteral("WARNING: InitDll export missing (some hosts may skip plugin)");
    }

    void* factory = getFactory();
    if (!factory) {
        if (exitDll) {
            exitDll();
        }
        result.summary = QStringLiteral("GetPluginFactory returned null");
        return result;
    }
    result.details << QStringLiteral("Factory pointer OK");

    if (exitDll) {
        exitDll();
        result.details << QStringLiteral("ExitDll OK");
    }

    result.ok = true;
    result.summary = QStringLiteral("VST3 OK — %1").arg(QString::fromUtf8(meta(product).displayName));
    return result;
#endif
}

ProbeResult probeLv2(PluginProduct product, const QString& path)
{
    ProbeResult result;
    result.path = path;
#if !defined(_WIN32)
    result.summary = QStringLiteral("LV2 probe is only implemented on Windows");
    return result;
#else
    const ProductMeta& m = meta(product);
    QString bundleDir = path;
    QString binaryPath = path;
    const QFileInfo info(path);
    if (info.isDir()) {
        bundleDir = path;
        binaryPath = QDir(path).filePath(QString::fromUtf8(m.lv2Binary));
    } else {
        bundleDir = info.absolutePath();
    }
    result.path = binaryPath;
    if (!QFileInfo::exists(binaryPath)) {
        result.summary = QStringLiteral("LV2 binary not found: %1").arg(binaryPath);
        return result;
    }

    ScopedModule module(binaryPath);
    if (!module.ok()) {
        result.summary = QStringLiteral("LoadLibrary failed: %1").arg(module.lastError());
        return result;
    }
    result.details << QStringLiteral("LoadLibrary OK");

    using Lv2DescriptorFn = const LV2_Descriptor* (*)(uint32_t);
    auto lv2Descriptor = getProc<Lv2DescriptorFn>(module.get(), "lv2_descriptor");
    if (!lv2Descriptor) {
        result.summary = QStringLiteral("Missing lv2_descriptor export");
        return result;
    }

    const LV2_Descriptor* descriptor = lv2Descriptor(0);
    if (!descriptor || !descriptor->URI || std::strcmp(descriptor->URI, m.lv2Uri) != 0) {
        result.summary = QStringLiteral("LV2 URI mismatch (got %1)")
                             .arg(descriptor && descriptor->URI ? QString::fromUtf8(descriptor->URI)
                                                                : QStringLiteral("<null>"));
        return result;
    }
    result.details << QStringLiteral("Descriptor URI OK: %1").arg(QString::fromUtf8(descriptor->URI));

    // Critical Reaper case: third arg is bundle_path, not URI.
    const QByteArray bundlePath = QDir::toNativeSeparators(bundleDir + QLatin1Char('/')).toUtf8();
    LV2_Handle instance =
        descriptor->instantiate(descriptor, 48000.0, bundlePath.constData(), nullptr);
    if (!instance) {
        result.summary = QStringLiteral(
            "instantiate() returned null with bundle_path — plugin would show as damaged in Reaper");
        return result;
    }
    result.details << QStringLiteral("instantiate(bundle_path) OK");

    constexpr uint32_t frames = 64;
    std::vector<float> inL(frames, 0.5f);
    std::vector<float> inR(frames, -0.5f);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);
    descriptor->connect_port(instance, 0, inL.data());
    descriptor->connect_port(instance, 1, inR.data());
    descriptor->connect_port(instance, 2, outL.data());
    descriptor->connect_port(instance, 3, outR.data());
    descriptor->activate(instance);
    descriptor->run(instance, frames);
    descriptor->deactivate(instance);
    descriptor->cleanup(instance);

    if (!nearlyEqual(outL[0], 0.5f) || !nearlyEqual(outR[0], -0.5f)) {
        result.summary = QStringLiteral("LV2 run/passthrough failed");
        return result;
    }

    result.ok = true;
    result.summary = QStringLiteral("LV2 OK — %1").arg(QString::fromUtf8(m.displayName));
    result.details << QStringLiteral("Process passthrough OK (%1 frames)").arg(frames);
    return result;
#endif
}

} // namespace

QString formatLabel(PluginFormat format)
{
    switch (format) {
    case PluginFormat::Clap: return QStringLiteral("CLAP");
    case PluginFormat::Vst3: return QStringLiteral("VST3");
    case PluginFormat::Lv2: return QStringLiteral("LV2");
    }
    return QStringLiteral("?");
}

QString productLabel(PluginProduct product)
{
    return QString::fromUtf8(meta(product).displayName);
}

QString defaultPluginPath(PluginFormat format, PluginProduct product)
{
    const ProductMeta& m = meta(product);
    const QString root = commonFilesRoot();
    switch (format) {
    case PluginFormat::Clap:
        return QDir(root).filePath(QStringLiteral("CLAP/%1").arg(QString::fromUtf8(m.clapFile)));
    case PluginFormat::Vst3:
        return QDir(root).filePath(QStringLiteral("VST3/%1").arg(QString::fromUtf8(m.vst3Bundle)));
    case PluginFormat::Lv2:
        return QDir(root).filePath(QStringLiteral("LV2/%1").arg(QString::fromUtf8(m.lv2Bundle)));
    }
    return {};
}

QString buildTreeCandidate(PluginFormat format, PluginProduct product, const QString& buildRoot)
{
    if (buildRoot.isEmpty()) {
        return {};
    }
    const ProductMeta& m = meta(product);
    const QDir root(buildRoot);
    switch (format) {
    case PluginFormat::Clap:
        return root.filePath(QString::fromUtf8(m.clapFile));
    case PluginFormat::Vst3:
        return root.filePath(QStringLiteral("plugins/vst3/%1").arg(QString::fromUtf8(m.vst3Bundle)));
    case PluginFormat::Lv2:
        return root.filePath(QStringLiteral("plugins/lv2/%1").arg(QString::fromUtf8(m.lv2Bundle)));
    }
    return {};
}

QString resolvePluginPath(PluginFormat format, PluginProduct product, const QString& overridePath)
{
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    QString buildRoot = qEnvironmentVariable("DONTFLOAT_PLUGIN_BUILD_ROOT");
#ifdef DONTFLOAT_PLUGIN_BUILD_ROOT
    if (buildRoot.isEmpty()) {
        buildRoot = QString::fromUtf8(DONTFLOAT_PLUGIN_BUILD_ROOT);
    }
#endif

    const auto tryBuildTree = [&]() -> QString {
        const QString roots[] = {
            buildRoot,
            QDir(buildRoot).filePath(QStringLiteral("Release")),
            QDir(buildRoot).filePath(QStringLiteral("Debug")),
            QDir(buildRoot).filePath(QStringLiteral("RelWithDebInfo")),
        };
        if (!buildRoot.isEmpty() && format != PluginFormat::Clap) {
            const QString fromBinaryDir = buildTreeCandidate(format, product, buildRoot);
            if (!fromBinaryDir.isEmpty() && QFileInfo::exists(fromBinaryDir)) {
                return fromBinaryDir;
            }
        }
        for (const QString& root : roots) {
            if (root.isEmpty()) {
                continue;
            }
            const QString candidate = buildTreeCandidate(format, product, root);
            if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
                return candidate;
            }
            if (format == PluginFormat::Vst3) {
                const QString flat = QDir(root).filePath(QString::fromUtf8(meta(product).vst3Bundle));
                if (QFileInfo::exists(flat)) {
                    return flat;
                }
            }
        }
        return {};
    };

    // Prefer freshly built modules when a build root is known (avoids stale Common Files installs).
    if (!buildRoot.isEmpty()) {
        const QString built = tryBuildTree();
        if (!built.isEmpty()) {
            return built;
        }
    }

    const QString installed = defaultPluginPath(format, product);
    if (QFileInfo::exists(installed)) {
        return installed;
    }

    const QString built = tryBuildTree();
    if (!built.isEmpty()) {
        return built;
    }
    return installed;
}

ProbeResult probePlugin(PluginFormat format, PluginProduct product, const QString& overridePath)
{
    const QString path = resolvePluginPath(format, product, overridePath);
    ProbeResult missing;
    missing.path = path;
    if (!QFileInfo::exists(path)) {
        missing.summary = QStringLiteral("File/bundle not found: %1").arg(path);
        return missing;
    }

    switch (format) {
    case PluginFormat::Clap: return probeClap(product, path);
    case PluginFormat::Vst3: return probeVst3(product, path);
    case PluginFormat::Lv2: return probeLv2(product, path);
    }
    missing.summary = QStringLiteral("Unknown format");
    return missing;
}

} // namespace Dontfloat::PluginTester
