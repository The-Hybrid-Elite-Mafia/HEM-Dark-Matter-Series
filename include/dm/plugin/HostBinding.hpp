#pragma once
// HEM Dark Matter Series - the faceplate's binding inside a plug-in host.
// Parameters: read from and written through the host (HostParams = the VST3 edit controller), so
// automation, the host's undo and saved songs all see every move. Meters and live displays: read
// from the running engine (the processor's), when the host lets us reach it. Everything else that
// depends only on settings (curves, handle positions, factory presets, Chainbreaker's quick-load
// chains) uses a "shadow" engine the controller owns and never runs audio through.
// User presets live on disk (Registry: userPresetRoot()/<slug>/).
#include "dm/plugin/Registry.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dm::plugin {

class HostParams {
public:
    virtual ~HostParams() = default;
    [[nodiscard]] virtual float normalized(int engineParam) const = 0;
    virtual void beginEdit(int engineParam) = 0;
    virtual void perform(int engineParam, float normalized) = 0;   // sets the controller value and tells the host
    virtual void endEdit(int engineParam) = 0;
    virtual void latencyChanged() {}                                // host must re-read the plug-in's latency
};

class HostBinding final : public ui::PanelBinding {
public:
    // live(): the processor engine's binding while it is reachable, else nullptr.
    HostBinding(const PluginInfo& info, HostParams& host, std::filesystem::path userDir,
                std::function<ui::PanelBinding*()> live = {});

    [[nodiscard]] std::size_t paramCount() const override { return shadow_->paramCount(); }
    [[nodiscard]] const ParamDesc& desc(int i) const override { return shadow_->desc(static_cast<std::size_t>(i)); }
    [[nodiscard]] float value(int i) const override;
    void setValue(int i, float plain) override;
    void beginEdit(int i) override;
    void endEdit(int i) override;
    [[nodiscard]] float meter(int source) const override;
    void feed(ui::DisplayKind k, int source, std::vector<float>& out) const override;
    [[nodiscard]] std::vector<std::string> presetNames() const override;
    [[nodiscard]] std::vector<std::string> presetCategories() const override;
    [[nodiscard]] int currentPreset() const override { return current_; }
    void loadPreset(int i) override;
    void savePreset() override;
    [[nodiscard]] bool modified() const override;
    bool undo() override;
    bool redo() override;
    void action(int id) override;
    [[nodiscard]] bool actionLit(int id) const override { return id == lastAction_; }

    // Host loaded a song / state: forget undo history and rebase "modified".
    void stateReloaded();
    [[nodiscard]] std::size_t factoryCount() const noexcept { return factoryNames_.size(); }
    [[nodiscard]] std::string lastError() const { return error_; }

private:
    std::vector<float> snapshot() const;            // plain values of every parameter, from the host
    void syncShadow() const;                        // host values -> shadow engine
    void pushShadow();                              // shadow engine values -> host (preset loads)
    void apply(const std::vector<float>& values);   // plain values -> host
    void commit();
    void checkLatency();
    void reloadUser();

    const PluginInfo& info_;
    HostParams& host_;
    std::filesystem::path userDir_;
    std::function<ui::PanelBinding*()> live_;
    std::unique_ptr<EngineApi> shadow_;
    std::unique_ptr<ui::PanelBinding> shadowBinding_;
    std::vector<std::string> factoryNames_, factoryCats_;
    std::vector<UserPreset> user_;
    std::vector<bool> editing_;
    std::vector<std::vector<float>> history_;
    std::size_t historyPos_ = 0;
    std::vector<float> baseline_;
    int current_ = 0, lastAction_ = -1, lastLatency_ = -1;
    std::string error_;
};

} // namespace dm::plugin
