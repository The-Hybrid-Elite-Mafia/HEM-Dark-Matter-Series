#pragma once
// HEM Dark Matter Series - one call per plug-in: its faceplate layout plus the binding that feeds it.
#include "dm/phantom_vox/PhantomVox.hpp"
#include "dm/syndicate/SyndicateRack.hpp"
#include "dm/ui/SyndicateFeeds.hpp"
#include "dm/ui/Bindings.hpp"
#include "dm/ui/PanelModel.hpp"

#include <memory>

namespace dm::ui {

// Phantom Vox meter feed ids (see phantomVoxPanel()).
enum PvFeed : int { PvInL, PvInR, PvOutL, PvOutR, PvGr, PvDs, PvRevIn, PvRevDuck, PvRevOut, PvDlyIn, PvDlyDuck, PvDlyOut };

[[nodiscard]] std::unique_ptr<PanelBinding> bindPhantomVox(dm::pv::PhantomVox& e);
// Params intentionally not on the plate (host-driven or internal), per plug-in.
[[nodiscard]] std::vector<int> phantomVoxHidden();

[[nodiscard]] std::unique_ptr<PanelBinding> bindSyndicate(dm::sr::SyndicateRack& e);
template <class M> [[nodiscard]] std::unique_ptr<PanelBinding> bindSplit(dm::sr::SplitPlugin<M>& e);
// Engines built on EngineShell keep AUTO GAIN (on by default) off the plate, like Phantom Vox.
[[nodiscard]] std::vector<int> shellHidden();

} // namespace dm::ui
