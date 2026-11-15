#pragma once
// HEM Dark Matter Series - connects a faceplate to an engine (UI thread).
// Parameters go straight to the engine's thread-safe ParamSet; every finished edit is an undo step;
// presets = the engine's factory presets plus user presets saved with "+".
#include "dm/core/Params.hpp"
#include "dm/ui/PanelModel.hpp"

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace dm::ui {

template <class E>
class EngineBinding final : public PanelBinding {
public:
    using Snap = decltype(std::declval<E&>().params().snapshot());
    static constexpr std::size_t N = std::tuple_size_v<Snap>;
    using MeterFn = std::function<float(E&, int)>;
    using FeedFn = std::function<void(E&, DisplayKind, int, std::vector<float>&)>;

    EngineBinding(E& e, std::span<const Preset> presets, MeterFn meter, FeedFn feed)
        : e_(e), presets_(presets), meter_(std::move(meter)), feed_(std::move(feed)) {
        baseline_ = e_.params().snapshot();
        history_.commit(baseline_);
    }

    [[nodiscard]] std::size_t paramCount() const override { return N; }
    [[nodiscard]] const ParamDesc& desc(int p) const override { return e_.params().desc(static_cast<std::size_t>(p)); }
    [[nodiscard]] float value(int p) const override { return e_.params().get(static_cast<std::size_t>(p)); }
    void setValue(int p, float v) override { e_.params().set(static_cast<std::size_t>(p), v); }
    void endEdit(int) override { history_.commit(e_.params().snapshot()); }
    [[nodiscard]] float meter(int source) const override { return meter_ ? meter_(e_, source) : -120.0f; }
    void feed(DisplayKind k, int source, std::vector<float>& out) const override { if (feed_) feed_(e_, k, source, out); }

    [[nodiscard]] std::vector<std::string> presetNames() const override {
        std::vector<std::string> n;
        for (const auto& p : presets_) n.emplace_back(p.name);
        for (const auto& u : user_) n.push_back(u.name);
        return n;
    }
    [[nodiscard]] int currentPreset() const override { return current_; }
    void loadPreset(int i) override {
        if (i < 0) return;
        if (static_cast<std::size_t>(i) < presets_.size()) e_.loadPreset(static_cast<std::size_t>(i));
        else if (static_cast<std::size_t>(i) - presets_.size() < user_.size()) e_.params().restore(user_[static_cast<std::size_t>(i) - presets_.size()].values);
        else return;
        current_ = i;
        baseline_ = e_.params().snapshot();
        history_.commit(baseline_);
    }
    void savePreset() override {
        user_.push_back({"User " + std::to_string(user_.size() + 1), e_.params().snapshot()});
        current_ = static_cast<int>(presets_.size() + user_.size() - 1);
        baseline_ = user_.back().values;
    }
    [[nodiscard]] bool modified() const override { return e_.params().snapshot() != baseline_; }
    bool undo() override { Snap s{}; if (!history_.undo(s)) return false; e_.params().restore(s); return true; }
    bool redo() override { Snap s{}; if (!history_.redo(s)) return false; e_.params().restore(s); return true; }

private:
    struct User { std::string name; Snap values; };
    E& e_;
    std::span<const Preset> presets_;
    MeterFn meter_;
    FeedFn feed_;
    std::vector<User> user_;
    Snap baseline_{};
    History<N> history_{};
    int current_ = 0;
};

} // namespace dm::ui
