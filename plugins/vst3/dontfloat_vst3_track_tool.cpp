// VST3 Track Tool starter implementation.
//
// This file intentionally depends on the official Steinberg VST3 SDK and is
// compiled only when DONTFLOAT_BUILD_VST3=ON and DONTFLOAT_VST3_SDK_ROOT is set.
// The first MVP declares the future DONTFLOAT Track Tool and keeps realtime
// processing lightweight while analysis/render APIs live in plugins/core.

#include "../core/dontfloat_plugin_core.h"
#include "../ui/dontfloat_qt_hosting.h"
#include "../ui/dontfloat_track_tool_editor.h"

#include <algorithm>
#include <cstring>
#include <memory>

#if defined(DONTFLOAT_HAS_VST3_SDK)
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Dontfloat::Vst3 {

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::sharedTrackToolSession;
using Dontfloat::Plugins::Ui::DontfloatTrackToolEditor;
using Dontfloat::Plugins::Ui::ensureQtApplication;

DECLARE_UID(kTrackToolProcessorUid, 0x7B1F68A1, 0x52D54D80, 0xA69C5B4E, 0xE1DA7201)
DECLARE_UID(kTrackToolControllerUid, 0xA1BFD4C3, 0x3E2D4E4D, 0x94C94972, 0xA7B31235)

namespace {

constexpr Steinberg::int32 kEditorWidth = 960;
constexpr Steinberg::int32 kEditorHeight = 640;

} // namespace

class TrackToolEditorView final : public Steinberg::CPluginView {
public:
    TrackToolEditorView()
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
        return Steinberg::kResultFalse;
#endif
    }

    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override
    {
#if defined(_WIN32)
        if (!parent || std::strcmp(type, Steinberg::kPlatformTypeHWND) != 0) {
            return Steinberg::kInvalidArgument;
        }

        ensureQtApplication();
        editor_ = std::make_unique<DontfloatTrackToolEditor>();
        editor_->bindSession(&sharedTrackToolSession());
        editor_->setAttribute(Qt::WA_NativeWindow, true);
        editor_->resize(kEditorWidth, kEditorHeight);
        editor_->show();

        const HWND child = reinterpret_cast<HWND>(editor_->winId());
        const HWND hostParent = reinterpret_cast<HWND>(parent);
        SetParent(child, hostParent);
        SetWindowLongPtr(child, GWL_STYLE, WS_CHILD | WS_VISIBLE);
        MoveWindow(child, 0, 0, kEditorWidth, kEditorHeight, TRUE);

        return Steinberg::CPluginView::attached(parent, type);
#else
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

private:
    std::unique_ptr<DontfloatTrackToolEditor> editor_;
};

class TrackToolControllerVst3 final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IEditController*>(new TrackToolControllerVst3());
    }

    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override
    {
        if (name && std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
            return new TrackToolEditorView();
        }
        return nullptr;
    }
};

class TrackToolProcessorVst3 final : public Steinberg::Vst::AudioEffect {
public:
    TrackToolProcessorVst3()
    {
        setControllerClass(kTrackToolControllerUid);
    }

    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new TrackToolProcessorVst3());
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
            session_.prepare(audioInfo_);
        }
        return AudioEffect::setActive(state);
    }

    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override
    {
        audioInfo_.sampleRate = std::max(1, int(setup.sampleRate));
        audioInfo_.channelCount = 2;
        audioInfo_.frameCount = std::max(1, int(setup.maxSamplesPerBlock));
        session_.prepare(audioInfo_);
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
            TrackToolSession& session = sharedTrackToolSession();
            session.appendHostFrames(
                const_cast<const float* const*>(data.inputs[0].channelBuffers32),
                channelCount,
                data.numSamples);
        }

        return Steinberg::kResultOk;
    }

private:
    TrackAudioInfo audioInfo_;
    TrackToolSession session_;
};

} // namespace Dontfloat::Vst3
#endif // DONTFLOAT_HAS_VST3_SDK
