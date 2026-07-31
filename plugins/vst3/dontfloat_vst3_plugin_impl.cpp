#include "../core/dontfloat_plugin_core.h"
#include "../core/plugin_host_config.h"
#include "../ui/dontfloat_plugin_editor_shell.h"
#include "../ui/dontfloat_qt_hosting.h"

#include <QString>

#include <algorithm>
#include <cstring>
#include <memory>

#if defined(DONTFLOAT_HAS_VST3_SDK)
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
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
#define DONTFLOAT_VST3_DISPLAY_NAME "DONTFLOAT"
#define DONTFLOAT_VST3_CONTROLLER_NAME "DONTFLOAT Controller"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
static const FUID DontfloatProcessorUid (0x8C2A79B2, 0x63E65E91, 0xB7AD6C5F, 0xF2EB8312);
static const FUID DontfloatControllerUid (0xB2C0E5D4, 0x4F3E5F5E, 0xA5DA5A83, 0xB8C42346);
#define DONTFLOAT_VST3_DISPLAY_NAME "DONTFLOAT Scratch"
#define DONTFLOAT_VST3_CONTROLLER_NAME "DONTFLOAT Scratch Controller"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
static const FUID DontfloatProcessorUid (0x9D3B8AC3, 0x74F76FA2, 0xC8BE7D70, 0x03FC9413);
static const FUID DontfloatControllerUid (0xC3D1F6E5, 0x604F706F, 0xB6EB6B94, 0xC9D53457);
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
        editor_->setWindowTitle(QString::fromUtf8(desc().clapName));
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
        if (editor_) {
            editor_->hide();
            SetParent(reinterpret_cast<HWND>(editor_->winId()), nullptr);
            editor_.reset();
        }
#endif
        return Steinberg::CPluginView::removed();
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override
    {
        const Steinberg::tresult result = Steinberg::CPluginView::onSize(newSize);
#if defined(_WIN32)
        if (editor_ && newSize) {
            const int width = std::max<int>(newSize->getWidth(), 320);
            const int height = std::max<int>(newSize->getHeight(), 240);
            editor_->resize(width, height);
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

class ProductProcessorVst3 final : public Steinberg::Vst::AudioEffect {
public:
    ProductProcessorVst3()
    {
        setControllerClass(DontfloatControllerUid);
    }

    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new ProductProcessorVst3());
    }

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

        if (data.numInputs > 0 && data.inputs[0].channelBuffers32) {
            TrackToolSession& session = sharedSession(product());
            session.appendHostFrames(
                const_cast<const float* const*>(data.inputs[0].channelBuffers32),
                channelCount,
                data.numSamples);
        }

        return Steinberg::kResultOk;
    }

private:
    TrackAudioInfo audioInfo_;
};

} // namespace Dontfloat::Vst3

BEGIN_FACTORY_DEF("DONTFLOAT", "https://github.com/ili4yov-ika/DONTFLOAT", "mailto:support@dontfloat.local")

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::DontfloatProcessorUid),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           DONTFLOAT_VST3_DISPLAY_NAME,
           Vst::kDistributable,
           "Fx|Tools",
           "0.0.0.1",
           kVstVersionString,
           Dontfloat::Vst3::ProductProcessorVst3::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::DontfloatControllerUid),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           DONTFLOAT_VST3_CONTROLLER_NAME,
           0,
           "",
           "0.0.0.1",
           kVstVersionString,
           Dontfloat::Vst3::ProductControllerVst3::createInstance)

END_FACTORY

#endif // DONTFLOAT_HAS_VST3_SDK
