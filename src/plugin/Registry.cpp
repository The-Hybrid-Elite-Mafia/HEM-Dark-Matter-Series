// HEM Dark Matter Series - plug-in catalogue (see Registry.hpp).
#include "dm/plugin/Registry.hpp"

#include "dm/ui/Faceplates.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace dm::plugin {

namespace {

template <class E>
class EngineImpl final : public EngineApi {
public:
    using BindFn = std::unique_ptr<ui::PanelBinding> (*)(E&);
    explicit EngineImpl(BindFn bind) : bind_(bind) {}
    void prepare(float sr, int maxBlock) override { e_.prepare(sr, maxBlock); }
    void reset() noexcept override { e_.reset(); }
    void process(float* l, float* r, int n, const float* kl, const float* kr) noexcept override {
        if constexpr (requires(E& x) { x.setSidechain(kl, kr); })
            if (kl && kr) e_.setSidechain(kl, kr);
        e_.process(l, r, n);
    }
    [[nodiscard]] int latency() const noexcept override {
        if constexpr (requires(const E& x) { x.latencyFromParams(); }) return e_.latencyFromParams();
        else return e_.latencySamples();
    }
    [[nodiscard]] std::size_t paramCount() const noexcept override { return e_.params().size(); }
    [[nodiscard]] const ParamDesc& desc(std::size_t i) const noexcept override { return e_.params().desc(i); }
    [[nodiscard]] float get(std::size_t i) const noexcept override { return e_.params().get(i); }
    void set(std::size_t i, float v) noexcept override { e_.params().set(i, v); }
    [[nodiscard]] std::unique_ptr<ui::PanelBinding> binding() override { return bind_(e_); }

private:
    E e_{};
    BindFn bind_;
};

template <class E>
std::function<std::unique_ptr<EngineApi>()> maker(typename EngineImpl<E>::BindFn bind) {
    return [bind] { return std::make_unique<EngineImpl<E>>(bind); };
}
template <class M>
std::unique_ptr<ui::PanelBinding> splitBind(dm::sr::SplitPlugin<M>& e) { return ui::bindSplit(e); }

std::vector<int> pvHidden() { return ui::phantomVoxHidden(); }
std::vector<int> shellHidden() { return ui::shellHidden(); }
std::vector<int> dmHidden() { return ui::tempoHidden(static_cast<int>(dm::d4::Tempo)); }
std::vector<int> cbHidden() { return ui::tempoHidden(static_cast<int>(dm::cb::Tempo)); }

const std::vector<PluginInfo>& catalogue() {
    using namespace dm;
    static const std::vector<PluginInfo> list = {
        {"HEM PHANTOM VOX", "PhantomVox", "Fx|Dynamics|EQ",
         {0x3D1D9255, 0x3F2F45E4, 0x98B77044, 0x2386F000}, {0x74AAE3F5, 0xCBD14C4A, 0x93D06123, 0xF3DE5E36},
         false, static_cast<int>(pv::Tempo), 8.0, maker<pv::PhantomVox>(&ui::bindPhantomVox), &ui::phantomVoxPanel, &pvHidden},
        {"HEM SYNDICATE RACK", "SyndicateRack", "Fx|Mastering",
         {0x22CCA93F, 0x0AAE4969, 0x9E0607B9, 0x76010CA9}, {0xC7887825, 0x61D94D9B, 0xAF8A394F, 0xA47EF612},
         true, -1, 0.0, maker<sr::SyndicateRack>(&ui::bindSyndicate), &ui::syndicatePanel, &shellHidden},
        {"HEM SAT", "HemSat", "Fx|Distortion",
         {0x1309020E, 0x638245B5, 0x92718126, 0xE7676F2B}, {0x7CBB869C, 0xB67040FF, 0x85B6E096, 0x13EFF244},
         false, -1, 0.0, maker<sr::HemSat>(&splitBind<sr::SatModule>), [] { return ui::splitPanel(ui::SplitKind::Sat); }, &shellHidden},
        {"HEM COMP", "HemComp", "Fx|Dynamics",
         {0x99EA7542, 0x407B4254, 0xBF1A8F14, 0xA6850975}, {0xA2134A1D, 0x781B472A, 0x85151633, 0x268CC87E},
         true, -1, 0.0, maker<sr::HemComp>(&splitBind<sr::CompModule>), [] { return ui::splitPanel(ui::SplitKind::Comp); }, &shellHidden},
        {"HEM TRANSIENT", "HemTransient", "Fx|Dynamics",
         {0x9458E5CE, 0x34A143AC, 0xAFA094FA, 0xC377850E}, {0xF7ADA3EF, 0xF1D94F2F, 0xB4385A7B, 0x92D7B7FF},
         false, -1, 0.0, maker<sr::HemTransient>(&splitBind<sr::TransientModule>), [] { return ui::splitPanel(ui::SplitKind::Transient); }, &shellHidden},
        {"HEM TONE", "HemTone", "Fx|EQ",
         {0x0BFB6BF6, 0x25D5485E, 0xB27762D8, 0x69F04F92}, {0x28A4D963, 0xFEB94266, 0xA2754E5E, 0x1B4CD026},
         false, -1, 0.0, maker<sr::HemTone>(&splitBind<sr::ToneModule>), [] { return ui::splitPanel(ui::SplitKind::Tone); }, &shellHidden},
        {"HEM MOD", "HemMod", "Fx|Modulation",
         {0x03DC33C3, 0xDA5D4E73, 0xBBFE9645, 0x63B25EF8}, {0x4D867D81, 0x3D8A4A52, 0xA6D06183, 0x6EDE07BB},
         false, -1, 0.0, maker<sr::HemMod>(&splitBind<sr::ModModule>), [] { return ui::splitPanel(ui::SplitKind::Mod); }, &shellHidden},
        {"HEM MAX", "HemMax", "Fx|Mastering",
         {0x41502065, 0xE458437F, 0xBF33AFF1, 0xF1549785}, {0xB4D9ABE2, 0x8F6E42E0, 0xAE4B36F3, 0xE254E70D},
         false, -1, 0.0, maker<sr::HemMax>(&splitBind<sr::MaxModule>), [] { return ui::splitPanel(ui::SplitKind::Max); }, &shellHidden},
        {"HEM APEX CORE", "ApexCore", "Fx|Dynamics|Mastering",
         {0xEA98AC44, 0x18BC4989, 0x9C32B705, 0x9681815B}, {0x840DBC3F, 0x2F4D4D15, 0xA448EB30, 0xD8A1DD07},
         false, -1, 0.0, maker<ax::ApexCore>(&ui::bindApex), &ui::apexPanel, &shellHidden},
        {"HEM IRON LOCK", "IronLock", "Fx|Pitch Shift",
         {0xB2A19149, 0x647F4199, 0x97CAB50F, 0x4D56E17C}, {0xBE735626, 0x1DEE41CE, 0x92BD4A8A, 0xDA72C6A2},
         false, -1, 0.0, maker<il::IronLock>(&ui::bindIronLock), &ui::ironLockPanel, &shellHidden},
        {"HEM OUTLAW RIDER", "OutlawRider", "Fx|Dynamics",
         {0xEB6DE304, 0x4A794FD6, 0x93CCC4E3, 0x8872B2FA}, {0x8C817FDF, 0xB8BE49A0, 0x96ABAD68, 0x3D69C2B1},
         false, -1, 0.0, maker<rd::OutlawRider>(&ui::bindRider), &ui::riderPanel, &shellHidden},
        {"HEM DARK MATTER 4D", "DarkMatter4D", "Fx|Reverb|Delay",
         {0x1AF0D56A, 0x503E4557, 0x9E9AE3D8, 0x5BE04122}, {0xE47A6A49, 0x5BFE4427, 0x8F347A20, 0x6A6FFE3E},
         true, static_cast<int>(d4::Tempo), 20.0, maker<d4::DarkMatter4D>(&ui::bindDarkMatter), &ui::darkMatterPanel, &dmHidden},
        {"HEM CHAINBREAKER 108", "Chainbreaker108", "Fx|Modulation|Distortion",
         {0xAB7C7CDF, 0x319645B7, 0xADDAF35C, 0x363290B0}, {0x18BFFC3D, 0x6F244385, 0x824A251C, 0x261924C0},
         false, static_cast<int>(cb::Tempo), 4.0, maker<cb::Chainbreaker108>(&ui::bindChainbreaker), &ui::chainbreakerPanel, &cbHidden},
    };
    return list;
}

} // namespace

std::span<const PluginInfo> plugins() { return catalogue(); }

bool hostVisible(const PluginInfo& p, std::size_t engineParam) noexcept {
    return static_cast<int>(engineParam) != p.tempoParam;
}

std::string stateText(const EngineApi& e) {
    std::string out(kStateMagic);
    char b[64];
    for (std::size_t i = 0; i < e.paramCount(); ++i) {
        std::snprintf(b, sizeof b, "%.9g", static_cast<double>(e.get(i)));
        out += std::string(e.desc(i).id) + "=" + b + "\n";
    }
    return out;
}

bool applyStateText(EngineApi& e, std::string_view text) {
    if (text.substr(0, kStateMagic.size()) != kStateMagic) return false;
    for (std::size_t i = 0; i < e.paramCount(); ++i) e.set(i, e.desc(i).def);
    std::istringstream in{std::string(text.substr(kStateMagic.size()))};
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string_view k(line.data(), eq);
        const float v = std::strtof(line.c_str() + eq + 1, nullptr);
        for (std::size_t i = 0; i < e.paramCount(); ++i)
            if (e.desc(i).id == k) { const auto& d = e.desc(i); e.set(i, std::clamp(v, d.min, d.max)); break; }
    }
    return true;
}

std::filesystem::path userPresetRoot() {
#if defined(_WIN32)
    wchar_t* a = nullptr;
    std::size_t len = 0;
    if (_wdupenv_s(&a, &len, L"APPDATA") == 0 && a) {
        std::filesystem::path p = std::filesystem::path(a) / "HEM" / "Dark Matter";
        std::free(a);
        return p;
    }
#else
    if (const char* x = std::getenv("XDG_CONFIG_HOME")) return std::filesystem::path(x) / "HEM" / "Dark Matter";
    if (const char* h = std::getenv("HOME")) return std::filesystem::path(h) / ".config" / "HEM" / "Dark Matter";
#endif
    return std::filesystem::temp_directory_path() / "HEM" / "Dark Matter";
}

std::vector<UserPreset> loadUserPresets(const std::filesystem::path& dir) {
    std::vector<UserPreset> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return out;
    for (const auto& f : std::filesystem::directory_iterator(dir, ec)) {
        if (f.path().extension() != ".hempreset") continue;
        std::ifstream in(f.path(), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        std::string text = ss.str();
        if (text.size() > (1u << 16) || text.rfind(std::string(kStateMagic), 0) != 0) continue;   // not ours
        out.push_back({f.path().stem().string(), std::move(text)});
    }
    std::sort(out.begin(), out.end(), [](const UserPreset& a, const UserPreset& b) { return a.name < b.name; });
    return out;
}

bool saveUserPreset(const std::filesystem::path& dir, const std::string& name, const std::string& text, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) { if (error) *error = "cannot create " + dir.string() + ": " + ec.message(); return false; }
    std::string safe;
    for (char ch : name) safe.push_back((ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ? '_' : ch);
    const auto path = dir / (safe + ".hempreset");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out) { if (error) *error = "cannot write " + path.string(); return false; }
    return true;
}

} // namespace dm::plugin
