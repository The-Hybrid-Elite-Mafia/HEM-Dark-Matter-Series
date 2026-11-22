// HEM Dark Matter Series - loads the built "HEM Dark Matter.vst3" the way a music program does
// (Steinberg's hosting classes) and proves, for every plug-in in it:
//   1. it is listed with its name, and its host parameters match the engine's (minus TEMPO);
//   2. what comes out of the plug-in is exactly what the engine itself makes from the same audio;
//   3. a parameter change sent by the host reaches the sound (and matches the engine with that value);
//   4. a saved state loads into a fresh instance and gives the same sound;
//   5. plug-ins with EXT KEY: switched on and fed a key on the sidechain input, they follow the key;
//   6. HEM IRON LOCK: switching REALTIME -> HQ changes the delay the plug-in reports.
// Usage: dm_vst3_hosttest "<path to HEM Dark Matter.vst3>"     exit code 0 = all passed
#include "dm/core/EngineShell.hpp"
#include "dm/plugin/Registry.hpp"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"

#include <cmath>
#include <optional>
#include <cstdio>
#include <string>
#include <vector>

using namespace Steinberg;

namespace {

int failures = 0, checks = 0;
void check(bool ok, const std::string& what, double v = 0) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  [%s] %-70s %12.4f\n", ok ? "PASS" : "FAIL", what.c_str(), v);
    std::fflush(stdout);
}

constexpr int kBlock = 512;
constexpr double kSr = 48000.0;

struct Audio { std::vector<float> l, r; };
Audio music(std::size_t n) {   // kick / bass / chord test material, about -6 dBFS
    Audio a{std::vector<float>(n), std::vector<float>(n)};
    double kp = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / kSr, tb = std::fmod(t, 0.5);
        kp += 6.283185307179586 * (50.0 + 120.0 * std::exp(-tb * 30.0)) / kSr;
        const double v = 0.45 * std::sin(kp) * std::exp(-tb * 8.0) + 0.12 * std::sin(6.283185307179586 * 110.0 * t) +
                         0.06 * std::sin(6.283185307179586 * 329.63 * t);
        a.l[i] = float(v);
        a.r[i] = float(v * 0.9);
    }
    return a;
}
Audio pulse(std::size_t n) {   // sidechain key: 100 Hz tone pumping on / off
    Audio a{std::vector<float>(n), std::vector<float>(n)};
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / kSr;
        a.l[i] = a.r[i] = float((std::fmod(t, 0.25) < 0.1 ? 0.5 : 0.0) * std::sin(6.283185307179586 * 100.0 * t));
    }
    return a;
}
double diffRms(const Audio& a, const Audio& b) {
    double s = 0;
    for (std::size_t i = 0; i < a.l.size(); ++i) s += double(a.l[i] - b.l[i]) * (a.l[i] - b.l[i]) + double(a.r[i] - b.r[i]) * (a.r[i] - b.r[i]);
    return std::sqrt(s / (2.0 * double(a.l.size())));
}
double db(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }

struct Instance {
    IPtr<Vst::PlugProvider> provider;
    IPtr<Vst::IComponent> component;
    FUnknownPtr<Vst::IAudioProcessor> processor;
    IPtr<Vst::IEditController> controller;
    bool ok = false;
};

Instance open(const VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& ci, bool keyOn) {
    Instance in;
    in.provider = owned(new Vst::PlugProvider(factory, ci, true));
    if (!in.provider->initialize()) return in;
    in.component = in.provider->getComponentPtr();
    in.controller = in.provider->getControllerPtr();
    in.processor = FUnknownPtr<Vst::IAudioProcessor>(in.component);
    if (!in.component || !in.processor) return in;
    Vst::ProcessSetup setup{Vst::kRealtime, Vst::kSample32, kBlock, kSr};
    if (in.processor->setupProcessing(setup) != kResultOk) return in;
    if (in.component->getBusCount(Vst::kAudio, Vst::kInput) > 1) in.component->activateBus(Vst::kAudio, Vst::kInput, 1, keyOn);
    in.component->setActive(true);
    in.processor->setProcessing(true);
    in.ok = true;
    return in;
}
void close(Instance& in) {
    if (!in.ok) return;
    in.processor->setProcessing(false);
    in.component->setActive(false);
}

// Runs `src` through the plug-in. `changes` (id -> normalized) are sent with the first block.
Audio run(Instance& in, const Audio& src, const std::vector<std::pair<Vst::ParamID, double>>& changes, const Audio* key) {
    Audio out{src.l, src.r};
    Vst::HostProcessData data;
    data.prepare(*in.component, kBlock, Vst::kSample32);
    Vst::ParameterChanges pc(64);
    for (std::size_t pos = 0; pos < src.l.size(); pos += kBlock) {
        const int n = int(std::min<std::size_t>(kBlock, src.l.size() - pos));
        data.numSamples = n;
        pc.clearQueue();
        if (pos == 0)
            for (auto [id, v] : changes) {
                int32 qi = 0, pi = 0;
                if (auto* q = pc.addParameterData(id, qi)) q->addPoint(0, v, pi);
            }
        data.inputParameterChanges = &pc;
        for (int c = 0; c < 2; ++c) {
            std::copy_n((c ? src.r : src.l).data() + pos, n, data.inputs[0].channelBuffers32[c]);
            if (data.numInputs > 1 && data.inputs[1].channelBuffers32 && c < data.inputs[1].numChannels)
                for (int k = 0; k < n; ++k) data.inputs[1].channelBuffers32[c][k] = key ? (c ? key->r : key->l)[pos + std::size_t(k)] : 0.0f;
        }
        in.processor->process(data);
        for (int c = 0; c < 2; ++c) std::copy_n(data.outputs[0].channelBuffers32[c], n, (c ? out.r : out.l).data() + pos);
    }
    return out;
}

// The same audio straight through the engine, with the same settings.
// loaded = true: settings in place before audio starts (a restored song), not changed while playing.
Audio direct(const dm::plugin::PluginInfo& p, const Audio& src, const std::vector<std::pair<std::size_t, float>>& plain, const Audio* key,
             bool loaded = false) {
    auto e = p.create();
    e->prepare(float(kSr), kBlock);
    for (auto [i, v] : plain) e->set(i, v);
    if (loaded) e->reset();
    Audio out{src.l, src.r};
    for (std::size_t pos = 0; pos < src.l.size(); pos += kBlock) {
        const int n = int(std::min<std::size_t>(kBlock, src.l.size() - pos));
        e->process(out.l.data() + pos, out.r.data() + pos, n, key ? key->l.data() + pos : nullptr, key ? key->r.data() + pos : nullptr);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: dm_vst3_hosttest <HEM Dark Matter.vst3>\n"); return 2; }
    std::string err;
    auto module = VST3::Hosting::Module::create(argv[1], err);
    if (!module) { std::printf("FAIL cannot load %s: %s\n", argv[1], err.c_str()); return 1; }
    Vst::HostApplication hostApp;
    Vst::PluginContextFactory::instance().setPluginContext(&hostApp);
    const auto factory = module->getFactory();
    const auto src = music(std::size_t(kSr * 1.5));
    const auto key = pulse(src.l.size());

    for (const auto& p : dm::plugin::plugins()) {
        std::printf("== %s ==\n", p.name);
        std::optional<VST3::Hosting::ClassInfo> foundInfo;
        for (const auto& ci : factory.classInfos())   // classInfos() returns a temporary list: keep a copy
            if (ci.category() == kVstAudioEffectClass && ci.name() == p.name) foundInfo = ci;
        check(foundInfo.has_value(), "listed in the bundle");
        if (!foundInfo) continue;
        const VST3::Hosting::ClassInfo* found = &*foundInfo;

        Instance a = open(factory, *found, false);
        check(a.ok, "loads, sets up at 48 kHz / 512, starts processing");
        if (!a.ok) continue;
        auto shadow = p.create();
        int visible = 0;
        for (std::size_t i = 0; i < shadow->paramCount(); ++i) visible += dm::plugin::hostVisible(p, i);
        check(a.controller && a.controller->getParameterCount() == visible, "host parameters = engine parameters minus TEMPO (count)",
              a.controller ? a.controller->getParameterCount() : -1);

        // 2. identical to the engine at defaults
        const double d0 = diffRms(run(a, src, {}, nullptr), direct(p, src, {}, nullptr));
        check(d0 < 1e-6, "defaults: plug-in output = engine output (diff RMS dBFS)", db(d0));

        // 3. a host parameter change reaches the sound: the first own control, to the other end
        const std::size_t pid = dm::G_Count;
        const auto& d = shadow->desc(pid);
        const double nTarget = dm::ui::toNorm(d, d.def) > 0.5 ? 0.0 : 1.0;
        const float plain = dm::ui::fromNorm(d, float(nTarget));
        Instance b = open(factory, *found, false);
        const auto changed = run(b, src, {{Vst::ParamID(pid), nTarget}}, nullptr);
        const double d1 = diffRms(changed, direct(p, src, {{pid, plain}}, nullptr));
        check(d1 < 1e-6, std::string(d.label) + " from the host = engine with that value (diff RMS dBFS)", db(d1));

        // 4. saved state -> fresh instance -> same sound
        MemoryStream stream;
        check(b.component->getState(&stream) == kResultOk, "saves its state");
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        Instance c = open(factory, *found, false);
        c.processor->setProcessing(false);
        c.component->setActive(false);
        const bool loaded = c.component->setState(&stream) == kResultOk;
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        const bool ctrlLoaded = c.controller && c.controller->setComponentState(&stream) == kResultOk;
        c.component->setActive(true);
        c.processor->setProcessing(true);
        check(loaded && ctrlLoaded, "a fresh instance loads it (processor + controller)");
        const double d2 = diffRms(run(c, src, {}, nullptr), direct(p, src, {{pid, plain}}, nullptr, true));
        check(d2 < 1e-6, "restored instance sounds the same (diff RMS dBFS)", db(d2));
        if (c.controller)
            check(std::fabs(c.controller->getParamNormalized(Vst::ParamID(pid)) - nTarget) < 1e-6, "its controller shows the restored value");

        // 5. sidechain
        if (p.sidechain) {
            std::size_t extKey = 0;
            for (std::size_t i = 0; i < shadow->paramCount(); ++i) if (shadow->desc(i).id == "ext_key") extKey = i;
            check(extKey != 0, "has an EXT KEY switch");
            std::vector<std::pair<Vst::ParamID, double>> hostSet{{Vst::ParamID(extKey), 1.0}};
            std::vector<std::pair<std::size_t, float>> engSet{{extKey, 1.0f}};
            // make the keyed process clearly active
            for (std::size_t i = 0; i < shadow->paramCount(); ++i) {
                const auto id = shadow->desc(i).id;
                if (id == "comp" || id == "ducking") {
                    hostSet.push_back({Vst::ParamID(i), dm::ui::toNorm(shadow->desc(i), 80.0f)});
                    engSet.push_back({i, 80.0f});
                }
                if (id == "reverb") { hostSet.push_back({Vst::ParamID(i), dm::ui::toNorm(shadow->desc(i), 50.0f)}); engSet.push_back({i, 50.0f}); }
            }
            Instance k = open(factory, *found, true);
            check(k.component->getBusCount(Vst::kAudio, Vst::kInput) == 2, "offers a sidechain input");
            const auto keyed = run(k, src, hostSet, &key);
            const double dk = diffRms(keyed, direct(p, src, engSet, &key));
            check(dk < 1e-6, "EXT KEY + sidechain audio = engine with that key (diff RMS dBFS)", db(dk));
            Instance nk = open(factory, *found, true);
            const double dn = diffRms(keyed, run(nk, src, hostSet, nullptr));
            check(dn > 1e-4, "the key really changes the sound vs a silent key (diff RMS dBFS)", db(dn));
            Instance off = open(factory, *found, false);
            const double doff = diffRms(run(off, src, hostSet, &key), direct(p, src, engSet, nullptr));
            check(doff < 1e-6, "sidechain input switched off by the host: falls back to the track (diff RMS dBFS)", db(doff));
            close(k); close(nk); close(off);
        }

        // 6. Iron Lock latency
        if (std::string(p.name) == "HEM IRON LOCK") {
            std::size_t q = 0;
            for (std::size_t i = 0; i < shadow->paramCount(); ++i) if (shadow->desc(i).id == "quality") q = i;
            Instance il = open(factory, *found, false);
            const uint32 rt = il.processor->getLatencySamples();
            (void)run(il, Audio{std::vector<float>(kBlock), std::vector<float>(kBlock)}, {{Vst::ParamID(q), 1.0}}, nullptr);
            const uint32 hq = il.processor->getLatencySamples();
            check(hq > rt, "REALTIME -> HQ: reported delay grows (samples)", double(hq) - double(rt));
            close(il);
        }
        close(a); close(b); close(c);
    }
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
