#pragma once
// HEM Dark Matter Series - a faceplate inside a plug-in host's editor window (Windows, OpenGL).
// A child window of the host's HWND; redraws itself about 30 times a second on the UI thread.
// Fonts and knob parts are loaded once per process and shared by every open faceplate.
#include "dm/ui/PanelController.hpp"
#include "dm/ui/PanelModel.hpp"
#include "dm/ui/PanelView.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace dm::plugin {

class PanelWindow {
public:
    PanelWindow();
    ~PanelWindow();
    PanelWindow(const PanelWindow&) = delete;
    PanelWindow& operator=(const PanelWindow&) = delete;

    // Pixel size for a plate at `scale` pixels per design unit.
    static void sizeFor(const ui::PanelDef& def, float scale, int& w, int& h) noexcept;
    // ~0.36 px per unit at 100 % Windows scaling (a 3840-unit plate = 1382 px), smaller when the
    // plate would not fit in 90 % of the screen's work area.
    [[nodiscard]] static float defaultScale(const ui::PanelDef& def) noexcept;

    // `assets` holds fonts/ and faceplates/Main-Faceplate.png. def and binding must outlive the window.
    bool open(void* parentHwnd, const ui::PanelDef& def, ui::PanelBinding& binding, const std::filesystem::path& assets,
              float scale, std::string* error);
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dm::plugin
