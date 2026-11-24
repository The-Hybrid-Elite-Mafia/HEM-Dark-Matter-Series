// =====================================================================================================
//  HEM Dark Matter Series - every plug-in as VST3, all in one bundle ("HEM Dark Matter.vst3").
//  One generic Processor / Controller pair serves all 13 plug-ins: the factory registers each
//  plug-in's two classes with a pointer to its dm::plugin::PluginInfo as the creation context.
//
//  Per plug-in: stereo in / stereo out (+ a stereo sidechain input where the plug-in has EXT KEY);
//  host parameter ids = the engine's parameter indices (every one except TEMPO, which follows the
//  host's tempo); the engine's BYPASS is the host's bypass switch. Latency and tails are reported.
//  Saved songs / presets: dm::plugin::stateText ("HEMDM1" id=value text).
//  Editor (Windows): the plug-in's faceplate (dm::plugin::PanelWindow, OpenGL) in the host's window.
//  Live meters / displays: the controller reaches the processor's engine through a per-process
//  registry once the processor announces itself (works in hosts that run both halves in one process,
//  which is the normal case; otherwise the faceplate still works, its meters just rest).
//  Steinberg VST3 SDK: MIT licence.
// =====================================================================================================
#include "dm/core/EngineShell.hpp"
#include "dm/plugin/HostBinding.hpp"
#include "dm/plugin/Registry.hpp"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#if defined(_WIN32)
#include "dm/plugin/PanelWindow.hpp"
#include "public.sdk/source/common/pluginview.h"
#include "ScalableView.hpp"
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef DM_VERSION_STR
#define DM_VERSION_STR "0.0.0"
#endif

using namespace Steinberg;

namespace dm::vst {

using plugin::EngineApi;
using plugin::PluginInfo;

FUID toFuid(const std::array<std::uint32_t, 4>& u) { return FUID(u[0], u[1], u[2], u[3]); }
std::u16string toU16(std::string_view s) { return std::u16string(s.begin(), s.end()); }   // ASCII text
constexpr const char* kLiveMessage = "dm.live";

// ---- processors this process is running, so a controller can find its live engine -------------------
class Processor;
struct LiveRegistry {
    std::mutex mutex;
    std::map<std::int64_t, Processor*> map;
    std::atomic<std::int64_t> next{1};
};
LiveRegistry& live() {
    static LiveRegistry r;
    return r;
}

// ---- state text <-> IBStream -------------------------------------------------------------------------
bool writeText(IBStream* s, const std::string& t) {
    if (!s) return false;
    int32 w = 0;
    return s->write(const_cast<char*>(t.data()), static_cast<int32>(t.size()), &w) == kResultOk && w == static_cast<int32>(t.size());
}
bool readText(IBStream* s, std::string& text) {
    if (!s) return false;
    char buf[4096];
    for (;;) {
        int32 got = 0;
        if (s->read(buf, sizeof buf, &got) != kResultOk || got <= 0) break;
        text.append(buf, static_cast<std::size_t>(got));
        if (text.size() > (1u << 16)) return false;   // ours is a few KB; refuse junk
    }
    return true;
}

// ---- processor ---------------------------------------------------------------------------------------
class Processor final : public Vst::AudioEffect {
public:
    explicit Processor(const PluginInfo* info) : info_(info), engine_(info->create()) { setControllerClass(toFuid(info->controllerUid)); }
    ~Processor() override { unregister(); }

    static FUnknown* createInstance(void* ctx) {
        return static_cast<Vst::IAudioProcessor*>(new Processor(static_cast<const PluginInfo*>(ctx)));
    }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
        const tresult r = AudioEffect::initialize(context);
        if (r != kResultOk) return r;
        addAudioInput(STR16("Stereo In"), Vst::SpeakerArr::kStereo);
        if (info_->sidechain) addAudioInput(STR16("Sidechain"), Vst::SpeakerArr::kStereo, Vst::kAux, 0);   // off until the host routes it
        addAudioOutput(STR16("Stereo Out"), Vst::SpeakerArr::kStereo);
        engine_->prepare(48000.0f, 1024);
        liveBinding_ = engine_->binding();
        {
            std::lock_guard lock(live().mutex);
            token_ = live().next++;
            live().map[token_] = this;
        }
        return kResultOk;
    }
    tresult PLUGIN_API terminate() SMTG_OVERRIDE {
        unregister();
        return AudioEffect::terminate();
    }

    // Tell the controller which live engine is ours (UI thread; never from process()).
    tresult PLUGIN_API connect(Vst::IConnectionPoint* other) SMTG_OVERRIDE {
        const tresult r = AudioEffect::connect(other);
        if (r == kResultOk) {
            if (Vst::IMessage* msg = allocateMessage()) {
                msg->setMessageID(kLiveMessage);
                msg->getAttributes()->setInt("token", token_);
                sendMessage(msg);
                msg->release();
            }
        }
        return r;
    }

    tresult PLUGIN_API setBusArrangements(Vst::SpeakerArrangement* inputs, int32 numIns, Vst::SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE {
        const int32 wantIns = info_->sidechain ? 2 : 1;
        if (numIns != wantIns || numOuts != 1) return kResultFalse;
        for (int32 i = 0; i < numIns; ++i)
            if (inputs[i] != Vst::SpeakerArr::kStereo) return kResultFalse;
        if (outputs[0] != Vst::SpeakerArr::kStereo) return kResultFalse;
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    }
    tresult PLUGIN_API activateBus(Vst::MediaType type, Vst::BusDirection dir, int32 index, TBool state) SMTG_OVERRIDE {
        if (type == Vst::kAudio && dir == Vst::kInput && index == 1) keyActive_ = state != 0;
        return AudioEffect::activateBus(type, dir, index, state);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) SMTG_OVERRIDE { return size == Vst::kSample32 ? kResultTrue : kResultFalse; }

    tresult PLUGIN_API setupProcessing(Vst::ProcessSetup& setup) SMTG_OVERRIDE {
        sampleRate_ = setup.sampleRate;
        maxBlock_ = std::clamp<int32>(setup.maxSamplesPerBlock, 32, 8192);
        engine_->prepare(static_cast<float>(sampleRate_), maxBlock_);
        return AudioEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE {
        if (state) engine_->reset();
        return AudioEffect::setActive(state);
    }
    uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE { return static_cast<uint32>(std::max(0, engine_->latency())); }
    uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE {
        return static_cast<uint32>(info_->tailSeconds * sampleRate_) + getLatencySamples();
    }

    tresult PLUGIN_API process(Vst::ProcessData& data) SMTG_OVERRIDE {
        if (data.inputParameterChanges) {
            const int32 n = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < n; ++i) {
                Vst::IParamValueQueue* q = data.inputParameterChanges->getParameterData(i);
                if (!q) continue;
                const int32 points = q->getPointCount();
                Vst::ParamValue v = 0;
                int32 off = 0;
                if (points <= 0 || q->getPoint(points - 1, off, v) != kResultTrue) continue;
                const Vst::ParamID id = q->getParameterId();
                if (id < engine_->paramCount() && plugin::hostVisible(*info_, id))
                    engine_->set(id, ui::fromNorm(engine_->desc(id), static_cast<float>(v)));
            }
        }
        if (info_->tempoParam >= 0 && data.processContext && (data.processContext->state & Vst::ProcessContext::kTempoValid)) {
            const auto& d = engine_->desc(static_cast<std::size_t>(info_->tempoParam));
            engine_->set(static_cast<std::size_t>(info_->tempoParam), std::clamp(static_cast<float>(data.processContext->tempo), d.min, d.max));
        }
        if (data.numSamples <= 0 || data.numOutputs < 1) return kResultOk;
        Vst::AudioBusBuffers& out = data.outputs[0];
        if (!out.channelBuffers32 || out.numChannels < 2) return kResultOk;
        float* oL = out.channelBuffers32[0];
        float* oR = out.channelBuffers32[1];
        const auto total = data.numSamples;
        const bool haveIn = data.numInputs >= 1 && data.inputs[0].channelBuffers32 && data.inputs[0].numChannels >= 2;
        if (haveIn) {
            const float* iL = data.inputs[0].channelBuffers32[0];
            const float* iR = data.inputs[0].channelBuffers32[1];
            if (oL != iL) std::memcpy(oL, iL, static_cast<std::size_t>(total) * sizeof(float));
            if (oR != iR) std::memcpy(oR, iR, static_cast<std::size_t>(total) * sizeof(float));
        } else {
            std::memset(oL, 0, static_cast<std::size_t>(total) * sizeof(float));
            std::memset(oR, 0, static_cast<std::size_t>(total) * sizeof(float));
        }
        // Sidechain: only when the host has switched the bus on and sends audio on it.
        const float* kL = nullptr;
        const float* kR = nullptr;
        if (info_->sidechain && keyActive_ && data.numInputs >= 2 && data.inputs[1].channelBuffers32 && data.inputs[1].numChannels >= 1) {
            kL = data.inputs[1].channelBuffers32[0];
            kR = data.inputs[1].numChannels >= 2 ? data.inputs[1].channelBuffers32[1] : kL;
        }
        for (int32 done = 0; done < total;) {
            const int32 n = std::min(total - done, maxBlock_);
            engine_->process(oL + done, oR + done, n, kL ? kL + done : nullptr, kR ? kR + done : nullptr);
            done += n;
        }
        out.silenceFlags = 0;
        // Faceplate meters show live readings only while audio is really flowing through an active engine.
        bypassed_.store(engine_->paramCount() > dm::G_Bypass && engine_->get(dm::G_Bypass) >= 0.5f, std::memory_order_relaxed);
        lastProcessMs_.store(nowMs(), std::memory_order_relaxed);
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE {
        std::string text;
        if (!readText(state, text) || !plugin::applyStateText(*engine_, text)) return kResultFalse;
        return kResultOk;
    }
    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE {
        return writeText(state, plugin::stateText(*engine_)) ? kResultOk : kResultFalse;
    }

    // The live engine's meters while audio flows through it; nullptr when the host has stopped calling
    // process() (transport stopped) or BYPASS is on, so the faceplate falls back to resting meters
    // instead of freezing on the last reading.
    [[nodiscard]] ui::PanelBinding* liveBinding() const noexcept {
        const std::int64_t last = lastProcessMs_.load(std::memory_order_relaxed);
        if (last == 0 || nowMs() - last > kLiveStaleMs || bypassed_.load(std::memory_order_relaxed)) return nullptr;
        return liveBinding_.get();
    }

private:
    static constexpr std::int64_t kLiveStaleMs = 300;
    static std::int64_t nowMs() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void unregister() {
        std::lock_guard lock(live().mutex);
        if (token_ != 0) live().map.erase(token_);
        token_ = 0;
    }

    const PluginInfo* info_;
    std::unique_ptr<EngineApi> engine_;
    std::unique_ptr<ui::PanelBinding> liveBinding_;   // meters / live displays for the faceplate (read from the UI thread)
    std::int64_t token_ = 0;
    bool keyActive_ = false;   // host switched the sidechain input on
    std::atomic<std::int64_t> lastProcessMs_{0};   // steady-clock ms of the last process() call that ran audio
    std::atomic<bool> bypassed_{false};
    double sampleRate_ = 48000.0;
    int32 maxBlock_ = 1024;
};

// ---- controller --------------------------------------------------------------------------------------
class DmParameter final : public Vst::Parameter {
public:
    DmParameter(const ParamDesc& d, Vst::ParamID id, bool bypass)
        : Vst::Parameter(toU16(d.label).c_str(), id, unitFor(d), ui::toNorm(d, d.def), stepsFor(d),
                         Vst::ParameterInfo::kCanAutomate | (d.isChoice() ? Vst::ParameterInfo::kIsList : 0) |
                             (bypass ? Vst::ParameterInfo::kIsBypass : 0)),
          d_(d) {}

    void toString(Vst::ParamValue norm, Vst::String128 out) const SMTG_OVERRIDE {
        const std::string s = ui::formatValue(d_, ui::fromNorm(d_, static_cast<float>(norm)));
        const std::size_t n = std::min<std::size_t>(s.size(), 127);
        for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<Vst::TChar>(static_cast<unsigned char>(s[i]));
        out[n] = 0;
    }
    bool fromString(const Vst::TChar* text, Vst::ParamValue& norm) const SMTG_OVERRIDE {
        std::string s;
        for (const Vst::TChar* t = text; t && *t && s.size() < 64; ++t) s.push_back(static_cast<char>(*t & 0x7F));
        if (d_.isChoice()) {
            for (std::size_t i = 0; i < d_.choices.size(); ++i)
                if (s == d_.choices[i]) { norm = ui::toNorm(d_, static_cast<float>(i)); return true; }
            return false;
        }
        char* end = nullptr;
        const float v = std::strtof(s.c_str(), &end);
        if (end == s.c_str()) return false;
        norm = ui::toNorm(d_, v);
        return true;
    }
    Vst::ParamValue toPlain(Vst::ParamValue norm) const SMTG_OVERRIDE { return ui::fromNorm(d_, static_cast<float>(norm)); }
    Vst::ParamValue toNormalized(Vst::ParamValue plain) const SMTG_OVERRIDE { return ui::toNorm(d_, static_cast<float>(plain)); }

private:
    static int32 stepsFor(const ParamDesc& d) {
        if (d.isChoice()) return static_cast<int32>(d.choices.size()) - 1;
        if (d.isToggle()) return 1;
        return 0;
    }
    static const Vst::TChar* unitFor(const ParamDesc& d) {
        if (d.unit == "dB") return STR16("dB");
        if (d.unit == "Hz") return STR16("Hz");
        if (d.unit == "ms") return STR16("ms");
        if (d.unit == "s") return STR16("s");
        if (d.unit == "%") return STR16("%");
        return STR16("");
    }
    const ParamDesc& d_;
};

class Controller;

#if defined(_WIN32)
// The bundle's Resources folder: <bundle>/Contents/x86_64-win/<dll> -> <bundle>/Contents/Resources.
std::filesystem::path bundleResources() {
    HMODULE m = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&bundleResources), &m);
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = ::GetModuleFileNameW(m, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path().parent_path() / "Resources";
}

// The editor: an OpenGL child window inside the host's window. It follows the host's display scaling and
// can be resized; the faceplate keeps its shape (see ScalableView.hpp).
class View final : public vst3view::ScalableView {
public:
    View(const ui::PanelDef& def, ui::PanelBinding& binding)
        : ScalableView(base(def, true), base(def, false), plugin::PanelWindow::defaultScale(def)), def_(def), binding_(binding) {}
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) SMTG_OVERRIDE {
        return std::strcmp(type, kPlatformTypeHWND) == 0 ? kResultTrue : kResultFalse;
    }
    void attachedToParent() SMTG_OVERRIDE {
        std::string err;
        if (!window_.open(systemWindow, def_, binding_, bundleResources(), scale(), &err))
            ::OutputDebugStringA(("HEM Dark Matter: faceplate failed: " + err + "\n").c_str());
        else
            window_.resize(rect.getWidth(), rect.getHeight());   // exactly the size the host was given
    }
    void removedFromParent() SMTG_OVERRIDE { window_.close(); }

protected:
    void resized(int width, int height) override { window_.resize(width, height); }

private:
    static float base(const ui::PanelDef& def, bool width) {
        int w = 0, h = 0;
        plugin::PanelWindow::sizeFor(def, 1.0f, w, h);
        return static_cast<float>(width ? w : h);
    }

    const ui::PanelDef& def_;
    ui::PanelBinding& binding_;
    plugin::PanelWindow window_;
};
#endif

class Controller final : public Vst::EditController, public plugin::HostParams {
public:
    explicit Controller(const PluginInfo* info) : info_(info), descs_(info->create()), def_(info->panel()) {}
    static FUnknown* createInstance(void* ctx) {
        return static_cast<Vst::IEditController*>(new Controller(static_cast<const PluginInfo*>(ctx)));
    }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
        const tresult r = EditController::initialize(context);
        if (r != kResultOk) return r;
        for (std::size_t i = 0; i < descs_->paramCount(); ++i)
            if (plugin::hostVisible(*info_, i))
                parameters.addParameter(new DmParameter(descs_->desc(i), static_cast<Vst::ParamID>(i), i == dm::G_Bypass));
        binding_ = std::make_unique<plugin::HostBinding>(*info_, *this, plugin::userPresetRoot() / info_->slug,
                                                         [this]() -> ui::PanelBinding* { return liveBinding(); });
        return kResultOk;
    }
    tresult PLUGIN_API terminate() SMTG_OVERRIDE {
        binding_.reset();
        return EditController::terminate();
    }

    tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE {
        std::string text;
        if (!readText(state, text)) return kResultFalse;
        auto tmp = info_->create();
        if (!plugin::applyStateText(*tmp, text)) return kResultFalse;
        for (std::size_t i = 0; i < tmp->paramCount(); ++i)
            if (plugin::hostVisible(*info_, i)) setParamNormalized(static_cast<Vst::ParamID>(i), ui::toNorm(tmp->desc(i), tmp->get(i)));
        if (binding_) binding_->stateReloaded();
        return kResultOk;
    }

    tresult PLUGIN_API notify(Vst::IMessage* message) SMTG_OVERRIDE {
        if (message && message->getMessageID() && std::strcmp(message->getMessageID(), kLiveMessage) == 0) {
            int64 t = 0;
            if (message->getAttributes() && message->getAttributes()->getInt("token", t) == kResultOk) liveToken_ = t;
            return kResultOk;
        }
        return EditController::notify(message);
    }

    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE {
#if defined(_WIN32)
        if (name && std::strcmp(name, Vst::ViewType::kEditor) == 0 && binding_) return new View(def_, *binding_);
#else
        (void)name;
#endif
        return nullptr;
    }

    // ---- HostParams (the faceplate's edits) ----
    float normalized(int i) const override {
        // EditController::getParamNormalized is not const in the SDK, but only reads.
        return static_cast<float>(const_cast<Controller*>(this)->getParamNormalized(static_cast<Vst::ParamID>(i)));
    }
    void beginEdit(int i) override { EditController::beginEdit(static_cast<Vst::ParamID>(i)); }
    void perform(int i, float n) override {
        setParamNormalized(static_cast<Vst::ParamID>(i), n);
        performEdit(static_cast<Vst::ParamID>(i), n);
    }
    void endEdit(int i) override { EditController::endEdit(static_cast<Vst::ParamID>(i)); }
    void latencyChanged() override {
        if (componentHandler) componentHandler->restartComponent(Vst::kLatencyChanged);
    }

private:
    ui::PanelBinding* liveBinding() const {
        if (liveToken_ == 0) return nullptr;
        std::lock_guard lock(live().mutex);
        const auto it = live().map.find(liveToken_);
        return it == live().map.end() ? nullptr : it->second->liveBinding();
    }

    const PluginInfo* info_;
    std::unique_ptr<EngineApi> descs_;   // parameter descriptions (never processes audio)
    ui::PanelDef def_;
    std::unique_ptr<plugin::HostBinding> binding_;
    std::int64_t liveToken_ = 0;
};

} // namespace dm::vst

// ---- factory: two classes per plug-in, created with the plug-in's info as context ----------------------
SMTG_EXPORT_SYMBOL IPluginFactory* PLUGIN_API GetPluginFactory() {
    if (!gPluginFactory) {
        static PFactoryInfo factoryInfo("Hybrid Elite Mafia", "https://github.com/The-Hybrid-Elite-Mafia", "", PFactoryInfo::kUnicode);
        gPluginFactory = new CPluginFactory(factoryInfo);
        const auto list = dm::plugin::plugins();
        static std::vector<PClassInfo2> infos;   // registration keeps pointers into these
        infos.reserve(list.size() * 2);
        for (const dm::plugin::PluginInfo& p : list) {
            TUID pid, cid;
            dm::vst::toFuid(p.processorUid).toTUID(pid);
            dm::vst::toFuid(p.controllerUid).toTUID(cid);
            const std::string ctrlName = std::string(p.name) + " Controller";
            infos.emplace_back(pid, PClassInfo::kManyInstances, kVstAudioEffectClass, p.name, Vst::kDistributable, p.subCategory,
                               nullptr, DM_VERSION_STR, kVstVersionString);
            gPluginFactory->registerClass(&infos.back(), dm::vst::Processor::createInstance, const_cast<dm::plugin::PluginInfo*>(&p));
            infos.emplace_back(cid, PClassInfo::kManyInstances, kVstComponentControllerClass, ctrlName.c_str(), 0, "", nullptr,
                               DM_VERSION_STR, kVstVersionString);
            gPluginFactory->registerClass(&infos.back(), dm::vst::Controller::createInstance, const_cast<dm::plugin::PluginInfo*>(&p));
        }
    } else {
        gPluginFactory->addRef();
    }
    return gPluginFactory;
}
