#pragma once
// HEM Dark Matter Series - the plug-in catalogue, shared by the VST3 wrapper and the tests.
// Every plug-in is one engine behind EngineApi (a small run-time interface), plus its faceplate.
// No plug-in SDK code in here, so all of it is unit-tested on any machine.
#include "dm/core/Params.hpp"
#include "dm/ui/PanelModel.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dm::plugin {

class EngineApi {
public:
    virtual ~EngineApi() = default;
    virtual void prepare(float sampleRate, int maxBlock) = 0;
    virtual void reset() noexcept = 0;
    // In-place stereo; keyL / keyR = sidechain input for this call (nullptr = none).
    virtual void process(float* l, float* r, int n, const float* keyL, const float* keyR) noexcept = 0;
    [[nodiscard]] virtual int latency() const noexcept = 0;
    [[nodiscard]] virtual std::size_t paramCount() const noexcept = 0;
    [[nodiscard]] virtual const ParamDesc& desc(std::size_t i) const noexcept = 0;
    [[nodiscard]] virtual float get(std::size_t i) const noexcept = 0;
    virtual void set(std::size_t i, float plain) noexcept = 0;
    // The old coloured defaults the factory presets were written over (the plug-in itself loads untouched).
    [[nodiscard]] virtual std::span<const PresetValue> presetBase() const noexcept = 0;
    // Faceplate binding straight onto this engine: its meters, displays and factory presets.
    [[nodiscard]] virtual std::unique_ptr<ui::PanelBinding> binding() = 0;
};

struct PluginInfo {
    const char* name;                       // "HEM PHANTOM VOX" (host plug-in list, never changes)
    const char* slug;                       // "PhantomVox" (user-preset folder name)
    const char* subCategory;                // VST3 sub-category, e.g. "Fx|Dynamics"
    std::array<std::uint32_t, 4> processorUid, controllerUid;   // permanent: hosts find saved songs by these
    bool sidechain;                         // offers a stereo sidechain input bus (EXT KEY)
    int tempoParam;                         // engine parameter fed from the host tempo (-1 = none)
    double tailSeconds;                     // reverb / delay tail
    std::function<std::unique_ptr<EngineApi>()> create;
    std::function<ui::PanelDef()> panel;
    std::function<std::vector<int>()> hidden;   // engine parameters deliberately not on the faceplate
};

[[nodiscard]] std::span<const PluginInfo> plugins();

// Host-visible parameters: every engine parameter except the tempo one (that follows the host).
[[nodiscard]] bool hostVisible(const PluginInfo& p, std::size_t engineParam) noexcept;

// ---- saved state: "HEMDM1\n" then id=value lines (plain values) ------------------------------------
inline constexpr std::string_view kStateMagic = "HEMDM1\n";
[[nodiscard]] std::string stateText(const EngineApi& e);
// Applies a saved state; unknown ids (older / newer versions) are skipped, missing ones keep defaults.
// Returns false if the text is not a HEM Dark Matter state.
bool applyStateText(EngineApi& e, std::string_view text);

// ---- user presets on disk: <root>/<slug>/<name>.hempreset (same text as the saved state) -----------
[[nodiscard]] std::filesystem::path userPresetRoot();   // %APPDATA%/HEM/Dark Matter (Windows), ~/.config/HEM/Dark Matter
struct UserPreset { std::string name; std::string text; };
[[nodiscard]] std::vector<UserPreset> loadUserPresets(const std::filesystem::path& dir);
bool saveUserPreset(const std::filesystem::path& dir, const std::string& name, const std::string& text, std::string* error);

} // namespace dm::plugin
