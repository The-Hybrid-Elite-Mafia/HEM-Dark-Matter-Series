#pragma once
// HEM Dark Matter Series - parameter descriptors, thread-safe values, presets and undo/redo history.
// The UI thread writes values; the audio thread reads them once per processing chunk.
#include "dm/core/Math.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace dm {

struct ParamDesc {
    std::string_view id;      // stable automation / preset key
    std::string_view label;   // faceplate label
    float min, max, def;
    std::string_view unit;    // "dB", "%", "Hz", "s", "" ...
    std::span<const char* const> choices{}; // non-empty -> stepped selector (value = index)
    [[nodiscard]] bool isChoice() const noexcept { return !choices.empty(); }
    [[nodiscard]] bool isToggle() const noexcept { return choices.empty() && min == 0.0f && max == 1.0f && unit == "toggle"; }
};

template <std::size_t N>
class ParamSet {
public:
    explicit ParamSet(const std::array<ParamDesc, N>& descs) : d_(descs) { resetToDefaults(); }

    void resetToDefaults() noexcept {
        for (std::size_t i = 0; i < N; ++i) v_[i].store(d_[i].def, std::memory_order_relaxed);
    }
    void set(std::size_t i, float value) noexcept {
        const auto& d = d_[i];
        float x = clampf(value, d.min, d.max);
        if (d.isChoice() || d.unit == "toggle") x = std::round(x);
        v_[i].store(x, std::memory_order_relaxed);
    }
    [[nodiscard]] float get(std::size_t i) const noexcept { return v_[i].load(std::memory_order_relaxed); }
    [[nodiscard]] int getInt(std::size_t i) const noexcept { return static_cast<int>(std::lround(get(i))); }
    [[nodiscard]] bool getBool(std::size_t i) const noexcept { return get(i) >= 0.5f; }
    [[nodiscard]] const ParamDesc& desc(std::size_t i) const noexcept { return d_[i]; }
    [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }

    [[nodiscard]] std::array<float, N> snapshot() const noexcept {
        std::array<float, N> s{};
        for (std::size_t i = 0; i < N; ++i) s[i] = get(i);
        return s;
    }
    void restore(const std::array<float, N>& s) noexcept {
        for (std::size_t i = 0; i < N; ++i) set(i, s[i]);
    }
    [[nodiscard]] int indexOf(std::string_view id) const noexcept {
        for (std::size_t i = 0; i < N; ++i) if (d_[i].id == id) return static_cast<int>(i);
        return -1;
    }

private:
    const std::array<ParamDesc, N>& d_;
    std::array<std::atomic<float>, N> v_{};
};

// Preset = name + (param id, value) overrides on top of defaults.
struct PresetValue { std::string_view id; float value; };
struct Preset {
    std::string_view name;
    std::string_view category;
    std::span<const PresetValue> values;
};

// Undo/redo over full parameter snapshots. UI thread only.
template <std::size_t N>
class History {
public:
    using Snap = std::array<float, N>;
    explicit History(std::size_t depth = 64) : depth_(depth) {}

    void commit(const Snap& s) {
        if (!undo_.empty() && undo_.back() == s) return;
        undo_.push_back(s);
        if (undo_.size() > depth_) undo_.erase(undo_.begin());
        redo_.clear();
    }
    // current = state now; returns false if nothing to undo.
    bool undo(Snap& current) {
        if (undo_.size() < 2) return false;
        redo_.push_back(undo_.back());
        undo_.pop_back();
        current = undo_.back();
        return true;
    }
    bool redo(Snap& current) {
        if (redo_.empty()) return false;
        current = redo_.back();
        undo_.push_back(current);
        redo_.pop_back();
        return true;
    }
    [[nodiscard]] bool canUndo() const noexcept { return undo_.size() >= 2; }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.empty(); }

private:
    std::size_t depth_;
    std::vector<Snap> undo_, redo_;
};

} // namespace dm
