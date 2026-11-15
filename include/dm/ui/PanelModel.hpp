#pragma once
// =====================================================================================================
//  HEM Dark Matter Series - faceplate model (no graphics code; unit-tested on any OS).
//  Every faceplate is a hand-placed list of widgets in DESIGN UNITS on a 3840 x 2160 plate (4K), each
//  bound to a real engine parameter index or a real engine meter / display feed.
//  Global chassis (every plug-in): top bar = crown + "HYBRID ELITE MAFIA // NAME", undo / redo,
//  preset box with < > +, BYPASS; bottom bar = INPUT trim + LED meter, "~ HYBRID ELITE MAFIA ~",
//  OUTPUT LED meter + trim.
// =====================================================================================================
#include "dm/core/Params.hpp"

#include <functional>
#include <string>
#include <vector>

namespace dm::ui {

struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    [[nodiscard]] float cx() const noexcept { return x + w * 0.5f; }
    [[nodiscard]] float cy() const noexcept { return y + h * 0.5f; }
    [[nodiscard]] float r() const noexcept { return x + w; }
    [[nodiscard]] float b() const noexcept { return y + h; }
    [[nodiscard]] bool contains(float px, float py) const noexcept { return px >= x && px < x + w && py >= y && py < y + h; }
    [[nodiscard]] bool overlaps(const Box& o) const noexcept { return x < o.r() && o.x < r() && y < o.b() && o.y < b(); }
    [[nodiscard]] bool inside(const Box& o) const noexcept { return x >= o.x && y >= o.y && r() <= o.r() && b() <= o.b(); }
};
inline Box circleBox(float cx, float cy, float radius) { return {cx - radius, cy - radius, 2 * radius, 2 * radius}; }

inline constexpr float kPlateW = 3840.0f, kPlateH = 2160.0f;

enum class Accent : unsigned char { Cyan, Orange, Pink, Purple, Gold };
enum class Icon : unsigned char { None, Wave, Ring, Atom, Tube, TubeXL, TapeReel, VuMeter, Keys, Max, Grid, Power, Bolt, Tv, Tunnel, Crosshair, KeyWhite, KeyBlack };

// Displays: each kind reads its own live feed from the binding.
enum class DisplayKind : unsigned char {
    Analyzer,        // FFT spectrum + EQ curve overlay (curve id = display arg)
    Transfer,        // compressor transfer curve
    Envelope,        // attack / release envelope curves
    Tilt,            // colour / tilt curve
    Waveform,        // scrolling in/out waveform with gain trace (rider)
    Tunnel,          // 3-D depth tunnel (reverb)
    Taps,            // stereo delay taps
    Lissajous,       // stereo phase / modulation wave
    XYPad,           // 2-D pad (stutter)
    Grain,           // grain wave display
    Keyboard,        // 12-note scale lock keyboard
    NoteGrid,        // quantum key confidence grid
    ShapeCurve,      // ducking shape curve
    TransientCurve,  // live transient envelope
    BandMeters,      // 3 vertical band meters (clip / maximize)
    Handles,         // feed: out[0] = live centre Hz of the band behind a curve handle
    Panel,           // plain glass window (no drawing of its own; holds other widgets)
};

// Handle: a dot on an Analyzer curve. Drag left / right = param (band frequency move), up / down =
// param2 (band gain). box = the display it lives in; source = band index for the Handles feed.
// Pad: 2-D control, param = x (left -> right), param2 = y (bottom -> top).
enum class Kind : unsigned char { Knob, Fader, Range, Button, Selector, Readout, Meter, Display, Bay, Label, Trim, Handle, Pad };

struct Widget {
    Kind   kind = Kind::Label;
    Box    box;                 // full hit / draw area
    int    param = -1;          // bound parameter (index into the engine table)
    int    param2 = -1;         // Range: second handle; Selector: power param; Bay: solo param
    int    param3 = -1;         // Bay: trim param
    int    choice = -1;         // Button: which choice of `param` this button selects (-1 = toggle)
    int    source = -1;         // Meter / Display: feed id (engine-specific)
    Accent accent = Accent::Cyan;
    Icon   icon = Icon::None;
    DisplayKind display = DisplayKind::Analyzer;
    std::string label;          // caption (empty = none)
    std::string prefix;         // Readout: text before the value ("COLOR")
    float  size = 1.0f;         // Knob: 1 = standard, >1 bigger rings
    bool   vertical = true;     // Fader / Range orientation
    bool   fromTop = false;     // Meter: gain-reduction style (hangs from the top)
    float  lo = -60, hi = 0;    // Meter range (dB) / Display range
    Box    screen{};            // Bay: recessed glass screen inside the bay (w = 0: none)
    int    showParam = -1;      // >= 0: only shown (and clickable) while round(value(showParam)) == showValue
    int    showValue = 0;
    float  titleBase = 100;     // Bay: title baseline below the bay top
};

struct PanelDef {
    std::string name;          // "HEM PHANTOM VOX"
    float w = kPlateW, h = kPlateH;   // plate size in design units (split plug-ins are smaller)
    bool twoLineTitle = false;        // brand line over the plug-in name (crowded top bars)
    std::string footer = "~ HYBRID ELITE MAFIA ~";
    std::vector<Widget> widgets;
    // global chassis areas
    Box title, undo, redo, preset, presetPrev, presetNext, presetSave, bypass, inputTrim, inputMeter, outputMeter, outputTrim, footerBox;
    int bypassParam = 2, inputParam = 0, outputParam = 1;
    int meterInL = 0, meterInR = 1, meterOutL = 2, meterOutR = 3;   // feed ids for the bottom-bar meters
    std::vector<Box> extraTopButtons;  // e.g. chain slots 1-8, GROUPS / REALTIME-HQ
};

// ---- What the faceplate needs from an engine (UI thread). ------------------------------------------
class PanelBinding {
public:
    virtual ~PanelBinding() = default;
    [[nodiscard]] virtual std::size_t paramCount() const = 0;
    [[nodiscard]] virtual const ParamDesc& desc(int p) const = 0;
    [[nodiscard]] virtual float value(int p) const = 0;
    virtual void setValue(int p, float v) = 0;          // clamps + rounds like the engine
    virtual void beginEdit(int) {}
    virtual void endEdit(int) {}                         // commits an undo step
    [[nodiscard]] virtual float meter(int source) const = 0;              // dB for level meters, raw for others
    // Display feed: fills `out` with samples / points for the display kind (x in 0..1, y in display units).
    virtual void feed(DisplayKind kind, int source, std::vector<float>& out) const = 0;
    // Presets / history
    [[nodiscard]] virtual std::vector<std::string> presetNames() const = 0;
    // Category of each preset (same order as presetNames; empty = flat list).
    [[nodiscard]] virtual std::vector<std::string> presetCategories() const { return {}; }
    [[nodiscard]] virtual int currentPreset() const = 0;
    virtual void loadPreset(int i) = 0;
    virtual void savePreset() = 0;                       // "+": stores the current settings as a new user preset
    [[nodiscard]] virtual bool modified() const = 0;
    // One-shot actions that are not parameters (e.g. Chainbreaker's quick-load chain slots).
    virtual void action(int) {}
    [[nodiscard]] virtual bool actionLit(int) const { return false; }
    virtual bool undo() = 0;
    virtual bool redo() = 0;
};

// ---- Value mapping shared by the view and the controller -------------------------------------------
[[nodiscard]] bool isLogScale(const ParamDesc& d) noexcept;
[[nodiscard]] float toNorm(const ParamDesc& d, float v) noexcept;
[[nodiscard]] float fromNorm(const ParamDesc& d, float n) noexcept;
[[nodiscard]] std::string formatValue(const ParamDesc& d, float v);

// Fader / Range track ends (design units) shared by the view and the controller.
struct Track { float a, b; };
[[nodiscard]] Track faderTrack(const Widget& w) noexcept;      // vertical: a = top y; horizontal: a = left x
[[nodiscard]] float rangeX(const Widget& w, float hz) noexcept;  // log 20 Hz .. 22 kHz along the Range track
[[nodiscard]] float rangeHz(const Widget& w, float x) noexcept;
// Handle dot geometry inside its display (gain axis = +/- 12 dB; frequency axis = log 20 Hz .. 22 kHz).
[[nodiscard]] float curveX(const Box& display, float hz) noexcept;
[[nodiscard]] float curveY(const Box& display, float db, float rangeDb = 12.0f) noexcept;
[[nodiscard]] float curveRange(const Widget& w) noexcept;   // a display's / handle's +/- dB span (hi, default 12)
// Visibility (Widget::showParam).
[[nodiscard]] bool isShown(const Widget& w, const PanelBinding& b);

// ---- Layout checks (used by tests and by the snapshot tool) -----------------------------------------
struct LayoutIssue { std::string what; };
// Every param in [firstOwn, paramCount) must be bound by some widget unless listed in `notOnPlate`;
// controls must sit inside the plate and must not overlap each other.
[[nodiscard]] std::vector<LayoutIssue> checkLayout(const PanelDef& def, const PanelBinding& b, std::vector<int> notOnPlate = {});

// ---- Faceplates -----------------------------------------------------------------------------------
[[nodiscard]] PanelDef phantomVoxPanel();
[[nodiscard]] PanelDef syndicatePanel();
enum class SplitKind : unsigned char { Sat, Comp, Transient, Tone, Mod, Max };
[[nodiscard]] PanelDef splitPanel(SplitKind k);
[[nodiscard]] PanelDef apexPanel();
[[nodiscard]] PanelDef ironLockPanel();
[[nodiscard]] PanelDef riderPanel();
[[nodiscard]] PanelDef darkMatterPanel();
[[nodiscard]] PanelDef chainbreakerPanel();

} // namespace dm::ui
