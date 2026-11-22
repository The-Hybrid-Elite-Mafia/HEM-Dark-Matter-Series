// HEM Dark Matter Series - faceplate binding inside a plug-in host (see HostBinding.hpp).
#include "dm/plugin/HostBinding.hpp"

#include <algorithm>
#include <cmath>

namespace dm::plugin {

using ui::fromNorm;
using ui::toNorm;

namespace {
constexpr std::size_t kHistoryMax = 200;
float roundLike(const ParamDesc& d, float v) {
    v = std::clamp(v, d.min, d.max);
    return d.isChoice() || d.isToggle() ? std::round(v) : v;
}
} // namespace

HostBinding::HostBinding(const PluginInfo& info, HostParams& host, std::filesystem::path userDir, std::function<ui::PanelBinding*()> live)
    : info_(info), host_(host), userDir_(std::move(userDir)), live_(std::move(live)), shadow_(info.create()) {
    shadow_->prepare(48000.0f, 512);
    shadowBinding_ = shadow_->binding();
    factoryNames_ = shadowBinding_->presetNames();
    factoryCats_ = shadowBinding_->presetCategories();
    editing_.assign(shadow_->paramCount(), false);
    reloadUser();
    stateReloaded();
}

float HostBinding::value(int i) const {
    const auto& d = desc(i);
    if (!hostVisible(info_, static_cast<std::size_t>(i))) return shadow_->get(static_cast<std::size_t>(i));
    return roundLike(d, fromNorm(d, host_.normalized(i)));
}

void HostBinding::setValue(int i, float plain) {
    if (i < 0 || static_cast<std::size_t>(i) >= shadow_->paramCount()) return;
    const auto& d = desc(i);
    plain = roundLike(d, plain);
    if (!hostVisible(info_, static_cast<std::size_t>(i))) { shadow_->set(static_cast<std::size_t>(i), plain); return; }
    const bool own = !editing_[static_cast<std::size_t>(i)];
    if (own) host_.beginEdit(i);
    host_.perform(i, toNorm(d, plain));
    if (own) { host_.endEdit(i); commit(); checkLatency(); }
}

void HostBinding::beginEdit(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= editing_.size() || editing_[static_cast<std::size_t>(i)]) return;
    editing_[static_cast<std::size_t>(i)] = true;
    if (hostVisible(info_, static_cast<std::size_t>(i))) host_.beginEdit(i);
}

void HostBinding::endEdit(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= editing_.size() || !editing_[static_cast<std::size_t>(i)]) return;
    editing_[static_cast<std::size_t>(i)] = false;
    if (hostVisible(info_, static_cast<std::size_t>(i))) host_.endEdit(i);
    commit();
    checkLatency();
}

float HostBinding::meter(int source) const {
    if (ui::PanelBinding* l = live_ ? live_() : nullptr) return l->meter(source);
    return -120.0f;   // not connected to a running engine: meters rest
}

void HostBinding::feed(ui::DisplayKind k, int source, std::vector<float>& out) const {
    if (ui::PanelBinding* l = live_ ? live_() : nullptr) { l->feed(k, source, out); return; }
    syncShadow();   // settings-only displays still draw from the current values
    shadowBinding_->feed(k, source, out);
}

std::vector<std::string> HostBinding::presetNames() const {
    std::vector<std::string> n = factoryNames_;
    for (const auto& u : user_) n.push_back(u.name);
    return n;
}

std::vector<std::string> HostBinding::presetCategories() const {
    if (factoryCats_.size() < factoryNames_.size() && user_.empty()) return {};
    std::vector<std::string> c(factoryNames_.size(), "Factory");
    for (std::size_t i = 0; i < factoryCats_.size() && i < c.size(); ++i)
        if (!factoryCats_[i].empty()) c[i] = factoryCats_[i];
    for (std::size_t i = 0; i < user_.size(); ++i) c.emplace_back("User");
    return c;
}

void HostBinding::loadPreset(int i) {
    if (i < 0) return;
    const auto fi = static_cast<std::size_t>(i);
    if (fi < factoryNames_.size()) {
        shadowBinding_->loadPreset(i);
    } else if (fi - factoryNames_.size() < user_.size()) {
        if (!applyStateText(*shadow_, user_[fi - factoryNames_.size()].text)) return;
    } else {
        return;
    }
    pushShadow();
    current_ = i;
    baseline_ = snapshot();
    commit();
    checkLatency();
}

void HostBinding::savePreset() {
    reloadUser();
    int n = 1;
    auto taken = [&](const std::string& name) {
        return std::any_of(user_.begin(), user_.end(), [&](const UserPreset& u) { return u.name == name; });
    };
    while (taken("User " + std::to_string(n))) ++n;
    const std::string name = "User " + std::to_string(n);
    syncShadow();
    if (!saveUserPreset(userDir_, name, stateText(*shadow_), &error_)) return;
    reloadUser();
    for (std::size_t k = 0; k < user_.size(); ++k)
        if (user_[k].name == name) current_ = static_cast<int>(factoryNames_.size() + k);
    baseline_ = snapshot();
}

bool HostBinding::modified() const {
    const auto now = snapshot();
    for (std::size_t i = 0; i < now.size() && i < baseline_.size(); ++i)
        if (hostVisible(info_, i) && std::fabs(now[i] - baseline_[i]) > 1e-5f) return true;
    return false;
}

bool HostBinding::undo() {
    if (historyPos_ == 0) return false;
    --historyPos_;
    apply(history_[historyPos_]);
    checkLatency();
    return true;
}

bool HostBinding::redo() {
    if (historyPos_ + 1 >= history_.size()) return false;
    ++historyPos_;
    apply(history_[historyPos_]);
    checkLatency();
    return true;
}

void HostBinding::action(int id) {
    syncShadow();
    shadowBinding_->action(id);   // e.g. Chainbreaker: loads quick chain `id` into the shadow
    pushShadow();
    lastAction_ = id;
    commit();
    checkLatency();
}

void HostBinding::stateReloaded() {
    history_.assign(1, snapshot());
    historyPos_ = 0;
    baseline_ = history_.front();
    syncShadow();
    lastLatency_ = shadow_->latency();
}

std::vector<float> HostBinding::snapshot() const {
    std::vector<float> v(shadow_->paramCount());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = value(static_cast<int>(i));
    return v;
}

void HostBinding::syncShadow() const {
    for (std::size_t i = 0; i < shadow_->paramCount(); ++i)
        if (hostVisible(info_, i)) shadow_->set(i, value(static_cast<int>(i)));
}

void HostBinding::pushShadow() {
    std::vector<float> v(shadow_->paramCount());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = shadow_->get(i);
    apply(v);
}

void HostBinding::apply(const std::vector<float>& values) {
    for (std::size_t i = 0; i < values.size() && i < shadow_->paramCount(); ++i) {
        if (!hostVisible(info_, i)) continue;
        const auto& d = shadow_->desc(i);
        const float n = toNorm(d, roundLike(d, values[i]));
        if (std::fabs(n - host_.normalized(static_cast<int>(i))) < 1e-7f) continue;
        host_.beginEdit(static_cast<int>(i));
        host_.perform(static_cast<int>(i), n);
        host_.endEdit(static_cast<int>(i));
    }
}

void HostBinding::commit() {
    auto now = snapshot();
    if (!history_.empty() && history_[historyPos_] == now) return;
    history_.resize(historyPos_ + 1);
    history_.push_back(std::move(now));
    if (history_.size() > kHistoryMax) history_.erase(history_.begin());
    historyPos_ = history_.size() - 1;
}

void HostBinding::checkLatency() {
    syncShadow();
    const int lat = shadow_->latency();
    if (lat != lastLatency_) {
        lastLatency_ = lat;
        host_.latencyChanged();
    }
}

void HostBinding::reloadUser() { user_ = loadUserPresets(userDir_); }

} // namespace dm::plugin
