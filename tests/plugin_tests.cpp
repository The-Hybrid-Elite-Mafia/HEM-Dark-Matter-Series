// HEM Dark Matter Series - plug-in layer tests (no plug-in SDK, no host needed).
// The catalogue (13 plug-ins, unique permanent ids), every engine through the run-time interface at
// 44.1 / 48 / 96 kHz with and without a sidechain, saved state round trips, and the faceplate inside a
// host: every control, preset, undo and quick-load goes through the host's edit calls (a fake host
// that records them), user presets on disk, and latency notices for Iron Lock's REALTIME / HQ.
#include "Suite.hpp"
#include "TestHarness.hpp"

#include "dm/plugin/HostBinding.hpp"
#include "dm/ui/PanelController.hpp"
#include "dm/plugin/Registry.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace dm::plugin;
using namespace dmt;

void testPanel(const char* name, const dm::ui::PanelDef& def, dm::ui::PanelBinding& b, const std::vector<int>& hidden);

namespace {

class FakeHost final : public HostParams {
public:
    explicit FakeHost(const EngineApi& e) {
        for (std::size_t i = 0; i < e.paramCount(); ++i) norm.push_back(dm::ui::toNorm(e.desc(i), e.desc(i).def));
    }
    float normalized(int i) const override { return norm[static_cast<std::size_t>(i)]; }
    void beginEdit(int i) override { ++begins; open.insert(i); }
    void perform(int i, float n) override {
        if (!open.count(i)) ++unbracketed;   // VST3 rule: perform only between begin and end
        norm[static_cast<std::size_t>(i)] = n;
        ++performs;
    }
    void endEdit(int i) override { ++ends; open.erase(i); }
    void latencyChanged() override { ++latency; }
    std::vector<float> norm;
    std::set<int> open;
    int begins = 0, performs = 0, ends = 0, unbracketed = 0, latency = 0;
};

std::filesystem::path tempDir(const std::string& leaf) {
    auto p = std::filesystem::temp_directory_path() / "hem-dm-tests" / leaf;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    return p;
}

void testCatalogue() {
    section("PLUG-IN CATALOGUE");
    const auto list = plugins();
    check(list.size() == 13, "plug-ins in the bundle (count)", double(list.size()));
    std::set<std::string> ids, names;
    for (const auto& p : list) {
        char b[80];
        std::snprintf(b, sizeof b, "%08X%08X%08X%08X", p.processorUid[0], p.processorUid[1], p.processorUid[2], p.processorUid[3]);
        ids.insert(b);
        std::snprintf(b, sizeof b, "%08X%08X%08X%08X", p.controllerUid[0], p.controllerUid[1], p.controllerUid[2], p.controllerUid[3]);
        ids.insert(b);
        names.insert(p.name);
    }
    check(ids.size() == list.size() * 2, "every class id is unique (count)", double(ids.size()));
    check(names.size() == list.size(), "every plug-in name is unique (count)", double(names.size()));
}

void testEngines() {
    for (const auto& p : plugins()) {
        section((std::string(p.name) + " - through the plug-in interface").c_str());
        for (float sr : {44100.0f, 48000.0f, 96000.0f}) {
            auto e = p.create();
            e->prepare(sr, 1024);
            const auto in = musicSignal(sr, 1.0, -6.0);
            auto key = pulseKey(sr, in.size());
            Stereo out = in;
            bool finite = true;
            double pk = 0;
            for (std::size_t pos = 0; pos < out.size(); pos += 1024) {
                const int n = int(std::min<std::size_t>(1024, out.size() - pos));
                e->process(out.l.data() + pos, out.r.data() + pos, n, p.sidechain ? key.l.data() + pos : nullptr,
                           p.sidechain ? key.r.data() + pos : nullptr);
            }
            for (std::size_t i = 0; i < out.size(); ++i) {
                finite = finite && std::isfinite(out.l[i]) && std::isfinite(out.r[i]);
                pk = std::max({pk, double(std::fabs(out.l[i])), double(std::fabs(out.r[i]))});
            }
            char label[96];
            std::snprintf(label, sizeof label, "%.1f kHz, 1024-sample blocks: finite, below 0 dBFS (peak dBFS)", sr / 1000.0f);
            check(finite && pk <= 1.0, label, db(pk), "dBFS");
            check(e->latency() >= 0 && e->latency() < int(sr), "latency is sane (samples)", e->latency());
        }
        // saved state: random settings survive the round trip exactly
        auto a = p.create();
        std::uint32_t seed = 77u;
        for (std::size_t i = 0; i < a->paramCount(); ++i) {
            seed = seed * 1664525u + 1013904223u;
            const auto& d = a->desc(i);
            float v = d.min + (d.max - d.min) * float(seed >> 8) / 16777216.0f;
            if (d.isChoice() || d.isToggle()) v = std::round(v);
            a->set(i, v);
        }
        const std::string text = stateText(*a);
        auto b = p.create();
        const bool ok = applyStateText(*b, text);
        int same = 0;
        for (std::size_t i = 0; i < a->paramCount(); ++i) same += std::fabs(a->get(i) - b->get(i)) <= 1e-6f * std::max(1.0f, std::fabs(a->get(i)));
        check(ok && same == int(a->paramCount()), "saved state round trip: every parameter identical (count)", same);
        auto c = p.create();
        check(!applyStateText(*c, "not a preset\nx=1\n"), "refuses text that is not a HEM state", 1);
        const bool partial = applyStateText(*c, std::string(kStateMagic) + "no_such_control=5\n");
        check(partial, "unknown ids (other versions) are skipped, not an error", 1);
    }
}

void testHostBinding() {
    for (const auto& p : plugins()) {
        auto shadow = p.create();
        FakeHost host(*shadow);
        HostBinding b(p, host, tempDir(p.slug));
        const auto def = p.panel();
        testPanel((std::string("IN A HOST: ") + p.name).c_str(), def, b, p.hidden());
        check(host.unbracketed == 0, "every host edit is inside begin / end (stray performs)", host.unbracketed);
        check(host.begins == host.ends && host.open.empty(), "every begin edit is ended (open edits)", double(host.open.size()));
    }

    section("IN A HOST: user presets on disk");
    {
        const PluginInfo& p = plugins()[1];   // Syndicate Rack
        const auto dir = tempDir("presets");
        auto shadow = p.create();
        FakeHost host(*shadow);
        HostBinding b(p, host, dir);
        int drive = -1;
        for (std::size_t i = 0; i < b.paramCount(); ++i) if (b.desc(int(i)).id == "saturate") drive = int(i);
        check(drive >= 0, "found SATURATE", drive);
        b.setValue(drive, 77.0f);
        b.savePreset();
        check(std::filesystem::exists(dir / "User 1.hempreset"), "\"+\" writes User 1.hempreset", 1);
        b.setValue(drive, 5.0f);
        HostBinding b2(p, host, dir);   // a new plug-in window / session sees it
        const auto names = b2.presetNames();
        const int idx = int(names.size()) - 1;
        check(names.back() == "User 1", "the saved preset is listed after the factory ones", idx);
        const auto cats = b2.presetCategories();
        check(!cats.empty() && cats.back() == "User", "it sits in the User category", 1);
        b2.loadPreset(idx);
        check(std::fabs(b2.value(drive) - 77.0f) < 0.01f, "loading it restores SATURATE (%)", b2.value(drive));
        check(!b2.modified(), "just loaded: not modified", 1);
        b2.setValue(drive, 60.0f);
        check(b2.modified(), "after a change: modified", 1);
        b2.undo();
        check(std::fabs(b2.value(drive) - 77.0f) < 0.01f, "undo goes back (%)", b2.value(drive));
        b2.savePreset();
        check(std::filesystem::exists(dir / "User 2.hempreset"), "the next one is User 2", 1);
    }

    section("IN A HOST: Iron Lock REALTIME / HQ tells the host the delay changed");
    {
        const PluginInfo* il = nullptr;
        for (const auto& p : plugins()) if (std::string(p.name) == "HEM IRON LOCK") il = &p;
        auto shadow = il->create();
        FakeHost host(*shadow);
        HostBinding b(*il, host, tempDir("iron"));
        int q = -1;
        for (std::size_t i = 0; i < b.paramCount(); ++i) if (b.desc(int(i)).id == "quality") q = int(i);
        check(q >= 0, "found the REALTIME / HQ switch", q);
        b.setValue(q, 1.0f);
        check(host.latency == 1, "switch to HQ: one latency notice", host.latency);
        auto e = il->create();
        e->prepare(48000, 512);
        const int rt = e->latency();
        e->set(std::size_t(q), 1.0f);
        const int hq = e->latency();
        check(hq > rt, "HQ reports more delay than REALTIME straight away (samples)", hq - rt);
        b.setValue(q, 0.0f);
        check(host.latency == 2, "back to REALTIME: another notice", host.latency);
    }

    section("IN A HOST: Chainbreaker quick-load goes through the host");
    {
        const PluginInfo* cbp = nullptr;
        for (const auto& p : plugins()) if (std::string(p.name) == "HEM CHAINBREAKER 108") cbp = &p;
        auto shadow = cbp->create();
        FakeHost host(*shadow);
        HostBinding b(*cbp, host, tempDir("chain"));
        const int before = host.performs;
        b.action(3);
        check(host.performs > before, "quick chain 4 changes parameters in the host (edits)", host.performs - before);
        check(b.actionLit(3), "and lights slot 4", 1);
        check(b.undo(), "the host-side undo can take it back", 1);
    }
}

// Faceplate meters: live readings while audio flows; when the host stops calling process() or BYPASS is
// on, the wrapper hands the faceplate no live engine and the meters must fall back to rest, not freeze.
void testUndoPresetName() {
    section("IN A HOST: undo brings back the preset name");
    const PluginInfo& p = plugins()[3];   // HEM COMP
    auto shadow = p.create();
    FakeHost host(*shadow);
    HostBinding b(p, host, tempDir("undo-name"));
    b.loadPreset(0);
    int comp = -1;
    for (std::size_t i = 0; i < b.paramCount(); ++i) if (b.desc(int(i)).id == "comp") comp = int(i);
    check(comp >= 0, "found COMP", comp);
    b.setValue(comp, 100.0f);
    check(b.currentPreset() == 0 && b.modified(), "preset 1 changed: shows its name with *", b.currentPreset());
    b.loadPreset(1);
    check(b.currentPreset() == 1 && !b.modified(), "next preset loaded: its name, no *", b.currentPreset());
    b.undo();
    check(b.currentPreset() == 0, "undo: back to preset 1's name (index)", b.currentPreset());
    check(b.modified(), "and its * (the change is back)", 1);
    b.undo();
    check(b.currentPreset() == 0 && !b.modified(), "undo again: preset 1 as loaded, no *", b.currentPreset());
    b.redo();
    b.redo();
    check(b.currentPreset() == 1 && !b.modified(), "redo twice: next preset again, no *", b.currentPreset());
}

// INPUT / OUTPUT: the whole capsule (number, word, meter) turns the trim, and it covers no other control.
void testTrimCapsules() {
    section("INPUT / OUTPUT capsules grab the trims");
    for (const auto& p : plugins()) {
        auto e = p.create();
        e->prepare(48000.0f, 512);
        auto b = e->binding();
        const auto def = p.panel();
        const std::string n = p.name;
        const dm::ui::Box wells[2] = {dm::ui::inputWell(def), dm::ui::outputWell(def)};
        const int params[2] = {def.inputParam, def.outputParam};
        const dm::ui::Box spots[2][2] = {{def.inputMeter, def.inputTrim}, {def.outputMeter, def.outputTrim}};
        for (int k = 0; k < 2; ++k) {
            int clash = 0;
            for (const auto& w : def.widgets)
                if (w.kind != dm::ui::Kind::Label && w.kind != dm::ui::Kind::Bay && w.box.overlaps(wells[k])) ++clash;
            check(clash == 0, (n + (k ? ": OUTPUT" : ": INPUT") + " capsule covers no other control (count)").c_str(), clash);
            for (const auto& spot : spots[k]) {
                dm::ui::PanelController c(def, *b);
                const float before = b->value(params[k]);
                c.wheel(spot.cx(), spot.cy(), 3.0f, false);
                check(b->value(params[k]) != before, (n + (k ? ": wheel on OUTPUT " : ": wheel on INPUT ") + (spot.x == spots[k][0].x ? "meter" : "number") + " turns the trim (dB)").c_str(), b->value(params[k]));
                b->setValue(params[k], 0.0f);
            }
            dm::ui::PanelController c(def, *b);
            const float wordX = k ? def.outputMeter.x - 120 : def.inputMeter.x - 70;
            c.wheel(wordX, wells[k].cy(), -3.0f, false);
            check(b->value(params[k]) < 0.0f, (n + (k ? ": wheel on the word OUTPUT" : ": wheel on the word INPUT") + " turns the trim (dB)").c_str(), b->value(params[k]));
            b->setValue(params[k], 0.0f);
        }
    }
}

void testMetersRest() {
    section("IN A HOST: meters rest when audio stops");
    for (const auto& p : plugins()) {
        auto running = p.create();
        running->prepare(48000.0f, 512);
        auto src = musicSignal(48000.0f, 1.0);
        for (std::size_t o = 0; o + 512 <= src.l.size(); o += 512) running->process(&src.l[o], &src.r[o], 512, nullptr, nullptr);
        auto liveB = running->binding();
        auto rest = p.create();
        rest->prepare(48000.0f, 512);
        auto restB = rest->binding();
        const auto def = p.panel();
        auto shadow = p.create();
        FakeHost host(*shadow);
        dm::ui::PanelBinding* current = liveB.get();
        HostBinding b(p, host, tempDir(p.slug), [&current]() -> dm::ui::PanelBinding* { return current; });
        const float playing = b.meter(def.meterInL);
        current = nullptr;   // transport stopped / bypassed
        const float stopped = b.meter(def.meterInL);
        check(playing > restB->meter(def.meterInL) + 20.0f, (std::string(p.name) + ": INPUT meter is live while audio flows (dB)").c_str(), playing);
        check(stopped == restB->meter(def.meterInL), (std::string(p.name) + ": and rests when it stops (dB)").c_str(), stopped);
    }
}

} // namespace

void runPluginTests() {
    testCatalogue();
    testEngines();
    testHostBinding();
    testMetersRest();
    testUndoPresetName();
    testTrimCapsules();
}
