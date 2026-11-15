#pragma once
// HEM Dark Matter Series - one call per plug-in: its faceplate layout plus the binding that feeds it.
#include "dm/phantom_vox/PhantomVox.hpp"
#include "dm/ui/Bindings.hpp"
#include "dm/ui/PanelModel.hpp"

#include <memory>

namespace dm::ui {

// Phantom Vox meter feed ids (see phantomVoxPanel()).
enum PvFeed : int { PvInL, PvInR, PvOutL, PvOutR, PvGr, PvDs, PvRevIn, PvRevDuck, PvRevOut, PvDlyIn, PvDlyDuck, PvDlyOut };

[[nodiscard]] std::unique_ptr<PanelBinding> bindPhantomVox(dm::pv::PhantomVox& e);
// Params intentionally not on the plate (host-driven or internal), per plug-in.
[[nodiscard]] std::vector<int> phantomVoxHidden();

} // namespace dm::ui
