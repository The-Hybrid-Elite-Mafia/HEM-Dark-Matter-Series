// HEM Dark Matter Series - faceplate drawing.
#include "dm/ui/PanelView.hpp"

#include "hybrid/ui/Parts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dm::ui {

using hc::ui::Align;
using hc::ui::Blend;
using hc::ui::Rect;
using hc::ui::Renderer;
using hc::ui::Rgba;
using hc::ui::rgb8;
using hc::ui::Sprite;
using hc::ui::TextFx;
using hc::ui::TextStyle;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kKnobMin = -0.75f * kPi, kKnobMax = 0.75f * kPi;
const Rgba kText = rgb8(0xE6E8EE);
const Rgba kTextDim = rgb8(0x8A90A0);
const Rgba kGlass = rgb8(0x040608, 0.92f);
const Rgba kCyan = rgb8(0x00F0FF);
const Rgba kOrange = rgb8(0xFF5500);
const Rgba kAmber = rgb8(0xFF8A1E);

Rect R(const Box& b) { return {b.x, b.y, b.w, b.h}; }

// ---- CPU plate helpers ----
float sdRound(float px, float py, const Box& b, float rad) {
    const float hx = b.w * 0.5f - rad, hy = b.h * 0.5f - rad;
    const float qx = std::fabs(px - b.cx()) - hx, qy = std::fabs(py - b.cy()) - hy;
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - rad;
}
float hash2(int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
}
float brushed(float ux, float uy) {
    const int row = static_cast<int>(uy * 1.7f);
    return 0.55f * hash2(static_cast<int>(ux / 53.0f), row) + 0.30f * hash2(static_cast<int>(ux / 9.0f), row) + 0.15f * hash2(static_cast<int>(ux), static_cast<int>(uy));
}
// Light from the top-left: shading of a bevel whose outward normal is (nx, ny).
float bevelLight(float nx, float ny) { return -(nx * -0.6f + ny * -0.8f); }
} // namespace

Rgba accentColour(Accent a) noexcept {
    switch (a) {
    case Accent::Cyan: return kCyan;
    case Accent::Orange: return kOrange;
    case Accent::Pink: return rgb8(0xFF2E88);
    case Accent::Purple: return rgb8(0xA855F7);
    case Accent::Gold: return rgb8(0xFFC233);
    }
    return kCyan;
}

// =====================================================================================================
//  Static plate (CPU)
// =====================================================================================================
hc::ui::Image renderPlate(const PanelDef& def, float ppu) {
    hc::ui::Image img;
    img.width = static_cast<std::uint32_t>(std::lround(kPlateW * ppu));
    img.height = static_cast<std::uint32_t>(std::lround(kPlateH * ppu));
    img.rgba.assign(static_cast<std::size_t>(img.width) * img.height * 4, 0);
    const Box frame{10, 10, kPlateW - 20, kPlateH - 20};
    const Box field{66, 66, kPlateW - 132, kPlateH - 132};
    const Box topBar{108, 56, kPlateW - 216, 168};
    const Box botBar{108, 1972, kPlateW - 216, 136};
    std::vector<Box> bays, glass;
    for (const auto& w : def.widgets) {
        if (w.kind == Kind::Bay) bays.push_back(w.box);
        if (w.kind == Kind::Display) glass.push_back(w.box);
    }
    const std::array<std::array<float, 2>, 4> screws{{{72, 72}, {kPlateW - 72, 72}, {72, kPlateH - 72}, {kPlateW - 72, kPlateH - 72}}};

    for (std::uint32_t py = 0; py < img.height; ++py) {
        for (std::uint32_t px = 0; px < img.width; ++px) {
            const float ux = (static_cast<float>(px) + 0.5f) / ppu, uy = (static_cast<float>(py) + 0.5f) / ppu;
            const float dF = sdRound(ux, uy, frame, 110);
            if (dF > 1.0f) continue;   // transparent outside the chassis
            float L;                    // linear luminance
            float tintR = 1.0f, tintG = 1.0f, tintB = 1.04f;
            const float dI = sdRound(ux, uy, field, 64);
            if (dI > 0.0f) {
                // Chamfered gunmetal bezel.
                L = 0.085f + 0.05f * (1.0f - uy / kPlateH) + 0.025f * brushed(ux, uy);
                const float e = 0.5f;
                const float nx = sdRound(ux + e, uy, frame, 110) - sdRound(ux - e, uy, frame, 110);
                const float ny = sdRound(ux, uy + e, frame, 110) - sdRound(ux, uy - e, frame, 110);
                const float len = std::sqrt(nx * nx + ny * ny) + 1e-6f;
                const float edge = std::clamp(1.0f + dF / 18.0f, 0.0f, 1.0f);           // outer 18 units
                L += 0.16f * edge * bevelLight(nx / len, ny / len);
                const float inner = std::clamp(1.0f - dI / 14.0f, 0.0f, 1.0f);          // inner 14 units (falls inward)
                const float ix = sdRound(ux + e, uy, field, 64) - sdRound(ux - e, uy, field, 64);
                const float iy = sdRound(ux, uy + e, field, 64) - sdRound(ux, uy - e, field, 64);
                const float il = std::sqrt(ix * ix + iy * iy) + 1e-6f;
                L += 0.12f * inner * bevelLight(-ix / il, -iy / il);
            } else {
                // Brushed titanium field.
                L = 0.050f + 0.016f * (1.0f - uy / kPlateH) + 0.020f * brushed(ux, uy);
                for (const Box* bar : {&topBar, &botBar}) {
                    const float d = sdRound(ux, uy, *bar, 30);
                    if (d < 0.0f) {
                        L = 0.072f + 0.022f * brushed(ux, uy);
                        if (d > -5.0f) L += (uy < bar->cy() ? 0.07f : -0.03f) * (1.0f + d / 5.0f);
                    } else if (d < 16.0f && uy > bar->cy()) {
                        L *= 0.55f + 0.45f * d / 16.0f;   // drop shadow below the raised bar
                    }
                }
                for (const Box& b : bays) {
                    const float d = sdRound(ux, uy, b, 30);
                    if (d < 0.0f) {
                        L = 0.016f + 0.006f * brushed(ux * 0.5f, uy);
                        tintB = 1.10f;
                        const float fromTop = (uy - b.y), fromLeft = (ux - b.x);
                        L *= 0.6f + 0.4f * std::clamp(std::min(fromTop, fromLeft) / 34.0f, 0.0f, 1.0f);   // inner shadow
                        if (d > -3.0f && (uy > b.cy() || ux > b.cx())) L += 0.05f;                           // lit lip
                    } else if (d < 6.0f) {
                        L += (uy > b.cy() ? 0.05f : -0.02f) * (1.0f - d / 6.0f);                             // chamfer
                    }
                }
                for (const Box& g : glass) {
                    const float d = sdRound(ux, uy, g, 18);
                    if (d < 0.0f) {
                        L = 0.008f + 0.010f * (1.0f - (uy - g.y) / g.h);
                        tintB = 1.25f; tintG = 1.08f;
                        if (d > -2.5f) L += 0.06f;
                    }
                }
            }
            // Hex screws.
            for (const auto& s : screws) {
                const float dx = ux - s[0], dy = uy - s[1], rr = std::sqrt(dx * dx + dy * dy);
                if (rr < 24.0f) {
                    const float nx = dx / 24.0f, ny = dy / 24.0f, nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                    const float diff = std::max(0.0f, -0.48f * nx - 0.62f * ny + 0.62f * nz);
                    L = 0.08f + 0.30f * diff + 0.5f * std::pow(diff, 24.0f);
                    const float ax = std::fabs(dx), ay = std::fabs(dy);
                    if (std::max(ax * 0.866f + ay * 0.5f, ay) < 9.0f) L = 0.01f;   // hex socket
                    tintB = 1.0f;
                }
            }
            const float a = std::clamp(0.5f - dF, 0.0f, 1.0f);
            std::uint8_t* p = &img.rgba[(static_cast<std::size_t>(py) * img.width + px) * 4];
            p[0] = hc::ui::linearToSrgb8(std::min(1.0f, L * tintR));
            p[1] = hc::ui::linearToSrgb8(std::min(1.0f, L * tintG));
            p[2] = hc::ui::linearToSrgb8(std::min(1.0f, L * tintB));
            p[3] = static_cast<std::uint8_t>(std::lround(a * 255.0f));
        }
    }
    return img;
}

// =====================================================================================================
//  Moving parts (GPU)
// =====================================================================================================
void PanelView::draw(Renderer& r, PanelBinding& b, const PanelController& c) {
    drawChassisLights(r);
    for (std::size_t i = 0; i < def_.widgets.size(); ++i) {
        const Hit h = c.active().t != Hit::T::None ? c.active() : c.hover();
        const bool hot = h.w == static_cast<int>(i) && h.t != Hit::T::None;
        drawWidget(r, b, def_.widgets[i], hot);
    }
    drawTopBar(r, b, c);
    drawBottomBar(r, b, c);
    if (c.menuOpen()) drawPresetMenu(r, b, c);
}

void PanelView::drawChassisLights(Renderer& r) {
    // Dual-tone neon edge light: electric cyan down the left / top-left, molten orange on the right.
    const float t = 7.0f;
    r.glow({30, 300, t, 1560}, 4, 40, kCyan.withAlpha(0.55f), true);
    r.fillRound({28, 300, t, 1560}, 3, kCyan, Blend::Over, true);
    r.glow({kPlateW - 37, 300, t, 1560}, 4, 40, kOrange.withAlpha(0.55f), true);
    r.fillRound({kPlateW - 37, 300, t, 1560}, 3, kOrange, Blend::Over, true);
    r.glow({420, 22, 1300, 6}, 3, 30, kCyan.withAlpha(0.45f), true);
    r.fillRound({420, 22, 1300, 5}, 2, kCyan, Blend::Over, true);
    r.glow({2120, 22, 1300, 6}, 3, 30, kCyan.withAlpha(0.45f), true);
    r.fillRound({2120, 22, 1300, 5}, 2, kCyan, Blend::Over, true);
    r.glow({300, kPlateH - 28, 1500, 6}, 3, 30, kOrange.withAlpha(0.40f), true);
    r.fillRound({300, kPlateH - 28, 1500, 5}, 2, kOrange, Blend::Over, true);
    r.glow({2040, kPlateH - 28, 1500, 6}, 3, 30, kOrange.withAlpha(0.40f), true);
    r.fillRound({2040, kPlateH - 28, 1500, 5}, 2, kOrange, Blend::Over, true);
}

void PanelView::drawWidget(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    switch (w.kind) {
    case Kind::Bay: drawBay(r, w); break;
    case Kind::Knob: case Kind::Trim: drawKnob(r, b, w, hot); break;
    case Kind::Fader: drawFader(r, b, w, hot); break;
    case Kind::Range: drawRange(r, b, w, hot); break;
    case Kind::Button: drawButton(r, b, w, hot); break;
    case Kind::Selector: drawSelector(r, b, w, hot); break;
    case Kind::Readout: drawReadout(r, b, w, hot); break;
    case Kind::Meter: drawMeter(r, b, w); break;
    case Kind::Display: drawDisplay(r, b, w); break;
    case Kind::Label:
        r.text(TextStyle::Label, w.label, w.box.cx(), w.box.cy() + 10, kText, Align::Center, TextFx::Shadowed);
        break;
    }
}

void PanelView::drawBay(Renderer& r, const Widget& w) {
    const Rgba c = accentColour(w.accent);
    // thin neon rim along the top edge of every bay + soft accent rim around it
    r.glow({w.box.x + 60, w.box.y - 2, w.box.w - 120, 4}, 2, 18, c.withAlpha(0.45f), true);
    r.fillRound({w.box.x + 60, w.box.y - 2, w.box.w - 120, 4}, 2, c.withAlpha(0.95f), Blend::Over, true);
    r.fillRound({w.box.x + 2, w.box.y + 2, w.box.w - 4, 2}, 1, c.withAlpha(0.18f), Blend::Over, true);
    if (!w.label.empty()) {
        r.text(TextStyle::Panel, w.label, w.box.cx(), w.box.y + 92, c, Align::Center, TextFx::Neon, w.box.w - 300);
    }
}

void PanelView::drawKnob(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const auto& d = b.desc(w.param);
    const Rgba c = accentColour(w.accent);
    const float cx = w.box.cx(), cy = w.box.cy();
    const float R0 = w.box.w * 0.5f;
    const float n = toNorm(d, b.value(w.param));
    const bool bip = d.min < 0.0f && d.max > 0.0f;
    const float ang = kKnobMin + (kKnobMax - kKnobMin) * n;
    const float a0 = bip ? 0.0f : kKnobMin;
    // dotted LED tick ring outside the socket (lit up to the value)
    const int ticks = w.size < 0.8f ? 0 : 31;
    for (int i = 0; i < ticks; ++i) {
        const float t = static_cast<float>(i) / (ticks - 1);
        const float a = kKnobMin + (kKnobMax - kKnobMin) * t;
        const float rr = R0 * 1.12f;
        const float x = cx + std::sin(a) * rr, y = cy - std::cos(a) * rr;
        const bool lit = bip ? ((a >= std::min(a0, ang)) && (a <= std::max(a0, ang))) : t <= n + 1e-4f;
        const float s = R0 * 0.045f;
        r.fillRound({x - s, y - s, 2 * s, 2 * s}, s, lit ? c : c.withAlpha(0.16f), Blend::Over, lit);
    }
    // socket + track + value arc
    const float R = R0 * 0.95f;
    r.sprite(Sprite::Socket, {cx - R, cy - R, 2 * R, 2 * R});
    r.sprite(Sprite::KnobTrack, {cx - R, cy - R, 2 * R, 2 * R});
    const float ar = hc::ui::knob::kArcRadius * R, aw = hc::ui::knob::kArcWidth * R * 1.15f;
    r.arc(cx, cy, ar, aw, kKnobMin, kKnobMax, c.withAlpha(0.14f), false);
    if (std::fabs(ang - a0) > 0.004f) {
        r.arc(cx, cy, ar, aw, std::min(a0, ang), std::max(a0, ang), c, true);
        const float tx = cx + std::sin(ang) * ar, ty = cy - std::cos(ang) * ar;
        r.sprite(Sprite::GlowDot, {tx - aw * 2.4f, ty - aw * 2.4f, aw * 4.8f, aw * 4.8f}, c.withAlpha(0.95f), Blend::Add, true);
    }
    const float bodyR = hc::ui::knob::kBody * R;
    r.sprite(Sprite::KnobShadow, {cx - bodyR * 1.14f + 0.06f * bodyR, cy - bodyR * 1.14f + 0.10f * bodyR, bodyR * 2.28f, bodyR * 2.28f});
    const float bd = bodyR * 2.0f * 256.0f / 254.0f;
    r.spriteRotated(Sprite::KnobBody, cx, cy, bd, bd, ang);
    const float len = (hc::ui::knob::kIndicatorTo - hc::ui::knob::kIndicatorFrom) * bodyR;
    const float mid = 0.5f * (hc::ui::knob::kIndicatorTo + hc::ui::knob::kIndicatorFrom) * bodyR;
    const float ih = len * 96.0f / 72.0f, iw = ih * 24.0f / 96.0f;
    r.spriteRotated(Sprite::KnobIndicator, cx + std::sin(ang) * mid, cy - std::cos(ang) * mid, iw, ih, ang,
                    {0.8f + 0.2f * c.r, 0.8f + 0.2f * c.g, 0.8f + 0.2f * c.b, 1.0f}, Blend::Over, true);
    r.sprite(Sprite::KnobLight, {cx - bd / 2, cy - bd / 2, bd, bd});
    if (hot) r.glow({cx - R * 0.8f, cy - R * 0.8f, R * 1.6f, R * 1.6f}, R * 0.8f, 22, c.withAlpha(0.30f), true);
    // caption (+ value while hot)
    if (w.size >= 0.8f) {
        const std::string text = hot ? formatValue(d, b.value(w.param)) : w.label;
        r.text(TextStyle::Title, text, cx, cy + R0 * 1.12f + 58, hot ? c : kText, Align::Center, hot ? TextFx::NeonCrisp : TextFx::Shadowed);
    }
}

void PanelView::capChrome(Renderer& r, float cx, float cy, float w, float h, Rgba line, bool horizontal) {
    if (horizontal) std::swap(w, h);
    r.sprite(Sprite::FaderShadow, {cx - w * 0.62f + w * 0.06f, cy - h * 0.75f + h * 0.16f, w * 1.24f, h * 1.5f}, {1, 1, 1, 0.95f});
    if (horizontal) {
        r.spriteRotated(Sprite::FaderCap, cx, cy, h, w, kPi * 0.5f);
        r.spriteRotated(Sprite::FaderLine, cx, cy, h * 0.86f, w * 0.17f, kPi * 0.5f, line, Blend::Over, true);
    } else {
        r.sprite(Sprite::FaderCap, {cx - w / 2, cy - h / 2, w, h});
        r.sprite(Sprite::FaderLine, {cx - w * 0.43f, cy - h * 0.085f, w * 0.86f, h * 0.17f}, line, Blend::Over, true);
    }
}

void PanelView::drawFader(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const auto& d = b.desc(w.param);
    const Rgba c = accentColour(w.accent);
    const float n = toNorm(d, b.value(w.param));
    if (w.vertical) {
        const float sx = w.box.cx(), slotW = 22;
        const float y0 = w.box.y + 40, y1 = w.box.b() - 40;
        r.fillRound({sx - slotW / 2, y0, slotW, y1 - y0}, slotW / 2, rgb8(0x020304), Blend::Over);
        r.sprite(Sprite::InnerShadow, {sx - slotW / 2, y0, slotW, y1 - y0});
        for (int t = 0; t <= 8; ++t) {
            const float y = y0 + (y1 - y0) * static_cast<float>(t) / 8.0f;
            r.fillRound({sx - 52, y - 1.5f, 22, 3}, 1.5f, kTextDim.withAlpha(0.6f), Blend::Over);
            r.fillRound({sx + 30, y - 1.5f, 22, 3}, 1.5f, kTextDim.withAlpha(0.6f), Blend::Over);
        }
        const float cy = y1 - (y1 - y0) * n;
        // lit track from the bottom to the cap
        r.fillRound({sx - 4, cy, 8, y1 - cy}, 4, c.withAlpha(0.85f), Blend::Over, true);
        r.glow({sx - 4, cy, 8, y1 - cy}, 4, 16, c.withAlpha(0.35f), true);
        capChrome(r, sx, cy, std::min(w.box.w * 0.9f, 120.0f), 64, c, false);
        const std::string text = hot ? formatValue(d, b.value(w.param)) : w.label;
        r.text(TextStyle::Title, text, sx, w.box.b() + 52, hot ? c : kText, Align::Center, hot ? TextFx::NeonCrisp : TextFx::Shadowed);
    } else {
        const float sy = w.box.cy(), slotH = 20;
        const float x0 = w.box.x + 70, x1 = w.box.r() - 70;
        r.fillRound({x0, sy - slotH / 2, x1 - x0, slotH}, slotH / 2, rgb8(0x020304), Blend::Over);
        const bool bip = d.min < 0.0f && d.max > 0.0f;
        const float cx = x0 + (x1 - x0) * n, mx = bip ? (x0 + x1) * 0.5f : x0;
        r.fillRound({std::min(mx, cx), sy - 4, std::fabs(cx - mx), 8}, 4, c.withAlpha(0.85f), Blend::Over, true);
        // end icons (dark / bright tilt or simple ends)
        r.text(TextStyle::Label, bip ? "-" : "", x0 - 40, sy + 10, kTextDim, Align::Center);
        capChrome(r, cx, sy, 54, std::min(w.box.h * 0.9f, 90.0f), c, true);
        r.text(TextStyle::Title, hot ? formatValue(d, b.value(w.param)) : w.label, w.box.cx(), w.box.y - 14,
               hot ? c : kText, Align::Center, hot ? TextFx::NeonCrisp : TextFx::Shadowed);
    }
}

void PanelView::drawRange(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const auto& d1 = b.desc(w.param);
    const auto& d2 = b.desc(w.param2);
    const Rgba c = accentColour(w.accent);
    const float sy = w.box.cy();
    const float x0 = w.box.x + 70, x1 = w.box.r() - 70;
    // the range bar maps HPF over the left 0..1 of its own range and LPF over its own range; both drawn
    // on one log-frequency track 20 Hz .. 22 kHz so the handles read as real frequencies.
    auto pos = [&](float hz) { return x0 + (x1 - x0) * std::log(std::max(hz, 20.0f) / 20.0f) / std::log(22000.0f / 20.0f); };
    const float pa = pos(b.value(w.param)), pb = pos(b.value(w.param2));
    r.fillRound({x0, sy - 10, x1 - x0, 20}, 10, rgb8(0x020304), Blend::Over);
    r.fillRound({pa, sy - 4, pb - pa, 8}, 4, c.withAlpha(0.85f), Blend::Over, true);
    r.glow({pa, sy - 4, pb - pa, 8}, 4, 14, c.withAlpha(0.3f), true);
    // filter-shape end marks
    const float hx[3] = {x0 - 52, x0 - 38, x0 - 14}, hy[3] = {sy + 18, sy - 16, sy - 16};
    r.polyline(hx, hy, 3, 4, kTextDim, Blend::Over);
    const float lx[3] = {x1 + 14, x1 + 38, x1 + 52}, ly[3] = {sy - 16, sy - 16, sy + 18};
    r.polyline(lx, ly, 3, 4, kTextDim, Blend::Over);
    capChrome(r, pa, sy, 50, 84, c, true);
    capChrome(r, pb, sy, 50, 84, c, true);
    if (hot) {
        const std::string t = formatValue(d1, b.value(w.param)) + "  /  " + formatValue(d2, b.value(w.param2));
        r.text(TextStyle::Title, t, w.box.cx(), w.box.y - 14, c, Align::Center, TextFx::NeonCrisp);
    }
}

void PanelView::drawButton(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const Rgba c = accentColour(w.accent);
    const float v = b.value(w.param);
    const bool lit = w.choice >= 0 ? static_cast<int>(std::lround(v)) == w.choice : v >= 0.5f;
    if (w.icon == Icon::Power) {
        const float cx = w.box.cx(), cy = w.box.cy(), rr = w.box.w * 0.30f;
        const Rgba ic = lit ? c : kTextDim.withAlpha(0.6f);
        r.arc(cx, cy + 2, rr, 7, 0.30f * kPi, 1.70f * kPi, ic, lit);
        r.fillRound({cx - 3.5f, cy - rr - 4, 7, rr + 2}, 3.5f, ic, Blend::Over, lit);
        if (lit) r.glow({cx - rr, cy - rr, 2 * rr, 2 * rr}, rr, 18, c.withAlpha(0.35f), true);
        return;
    }
    if (w.label == "S") {   // round solo button
        const float cx = w.box.cx(), cy = w.box.cy(), rr = w.box.w * 0.34f;
        r.fillRound({cx - rr, cy - rr, 2 * rr, 2 * rr}, rr, lit ? c : rgb8(0x14161B), Blend::Over, lit);
        r.arc(cx, cy, rr, 3, 0, 2 * kPi, lit ? c : kTextDim.withAlpha(0.5f), lit);
        r.text(TextStyle::Label, "S", cx, cy + 11, lit ? rgb8(0x0A0B0E) : kTextDim, Align::Center, TextFx::Plain);
        return;
    }
    const Rect bx = R(w.box);
    if (lit) {
        r.glow(bx.inset(4), 14, 16, c.withAlpha(0.40f), true);
        r.fillRound(bx.inset(2), 18, rgb8(0x0A0B0E, 0.95f), Blend::Over);
        r.fillRound({bx.x + 2, bx.y + 2, bx.w - 4, bx.h - 4}, 18, c.withAlpha(0.20f), Blend::Over, true);
        r.fillRound({bx.x + 2, bx.y + 2, bx.w - 4, 3}, 2, c, Blend::Over, true);
        r.fillRound({bx.x + 2, bx.bottom() - 5, bx.w - 4, 3}, 2, c, Blend::Over, true);
    } else {
        r.fillRound(bx.inset(2), 18, rgb8(0x101216), Blend::Over);
        r.fillRound({bx.x + 10, bx.y + 4, bx.w - 20, bx.h * 0.4f}, 12, {1, 1, 1, 0.05f}, Blend::Add);
    }
    if (hot) r.glow(bx.inset(6), 12, 10, c.withAlpha(0.25f), true);
    r.text(TextStyle::Label, w.label, bx.cx(), bx.cy() + 11, lit ? c : kText, Align::Center, lit ? TextFx::NeonCrisp : TextFx::Shadowed, bx.w - 20);
}

void PanelView::drawIcon(Renderer& r, Icon icon, float cx, float cy, float s, Rgba c, bool lit) {
    const float wgt = 7.0f;
    auto line = [&](const float* xs, const float* ys, int n) {
        r.polyline(xs, ys, n, wgt * 2.4f, c.withAlpha(lit ? 0.30f : 0.10f), Blend::Add, lit);
        r.polyline(xs, ys, n, wgt, lit ? c : c.withAlpha(0.45f), Blend::Over, lit);
    };
    switch (icon) {
    case Icon::Wave: {
        float xs[64], ys[64];
        for (int i = 0; i < 64; ++i) {
            const float t = static_cast<float>(i) / 63.0f;
            xs[i] = cx - s + 2 * s * t;
            ys[i] = cy - std::sin(t * 6.0f * kPi) * s * 0.55f * std::sin(t * kPi);
        }
        line(xs, ys, 64);
        break;
    }
    case Icon::Ring: {
        float xs[48], ys[48];
        for (int i = 0; i < 48; ++i) {
            const float a = 0.25f * kPi + 1.75f * kPi * static_cast<float>(i) / 47.0f;
            xs[i] = cx + std::sin(a) * s * 0.62f;
            ys[i] = cy - std::cos(a) * s * 0.62f;
        }
        line(xs, ys, 48);
        const float tx[3] = {cx + s * 0.2f, cx + s * 0.55f, cx + s * 0.45f}, ty[3] = {cy - s * 0.6f, cy - s * 0.95f, cy - s * 0.35f};
        line(tx, ty, 3);
        break;
    }
    case Icon::Atom: {
        for (int k = 0; k < 3; ++k) {
            float xs[49], ys[49];
            const float rot = static_cast<float>(k) * kPi / 3.0f;
            for (int i = 0; i < 49; ++i) {
                const float a = 2.0f * kPi * static_cast<float>(i) / 48.0f;
                const float ex = std::cos(a) * s * 0.85f, ey = std::sin(a) * s * 0.32f;
                xs[i] = cx + ex * std::cos(rot) - ey * std::sin(rot);
                ys[i] = cy + ex * std::sin(rot) + ey * std::cos(rot);
            }
            line(xs, ys, 49);
        }
        r.fillRound({cx - s * 0.13f, cy - s * 0.13f, s * 0.26f, s * 0.26f}, s * 0.13f, c, Blend::Over, lit);
        break;
    }
    case Icon::Tube: case Icon::Max: {
        for (int k = 0; k < 3; ++k) {
            const float bx = cx - s * 0.55f + static_cast<float>(k) * s * 0.55f;
            float xs[5] = {bx - s * 0.18f, bx - s * 0.18f, bx + s * 0.18f, bx + s * 0.18f, bx - s * 0.18f};
            float ys[5] = {cy + s * 0.1f, cy - s * 0.55f, cy - s * 0.55f, cy + s * 0.1f, cy + s * 0.1f};
            line(xs, ys, 5);
        }
        float bxs[5] = {cx - s * 0.85f, cx - s * 0.85f, cx + s * 0.85f, cx + s * 0.85f, cx - s * 0.85f};
        float bys[5] = {cy + s * 0.1f, cy + s * 0.45f, cy + s * 0.45f, cy + s * 0.1f, cy + s * 0.1f};
        line(bxs, bys, 5);
        if (icon == Icon::Tube) r.text(TextStyle::Title, "XL", cx, cy + s * 0.95f, c, Align::Center, lit ? TextFx::Neon : TextFx::Plain);
        break;
    }
    default: break;
    }
}

void PanelView::drawSelector(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const auto& d = b.desc(w.param);
    const Rgba c = accentColour(w.accent);
    const bool on = w.param2 < 0 || b.value(w.param2) >= 0.5f;
    const int cur = static_cast<int>(std::lround(b.value(w.param)));
    const float cx = w.box.cx();
    drawIcon(r, w.icon, cx, w.box.y + 95, 95, c, on);
    // < > arrows
    const Rgba ac = hot ? c : kText.withAlpha(0.75f);
    const float ay = w.box.y + 105;
    const float lx[3] = {w.box.x + 70, w.box.x + 40, w.box.x + 70}, ly[3] = {ay - 30, ay, ay + 30};
    r.polyline(lx, ly, 3, 7, ac, Blend::Over, hot);
    const float rx[3] = {w.box.r() - 70, w.box.r() - 40, w.box.r() - 70};
    r.polyline(rx, ly, 3, 7, ac, Blend::Over, hot);
    // mode name + position dots
    r.text(TextStyle::Panel, formatValue(d, b.value(w.param)), cx, w.box.y + 245, on ? kText : kTextDim, Align::Center, TextFx::Shadowed, w.box.w - 60);
    const int n = static_cast<int>(d.choices.size());
    const float gap = 26.0f, x0 = cx - gap * static_cast<float>(n - 1) * 0.5f;
    for (int i = 0; i < n; ++i) {
        const float x = x0 + gap * static_cast<float>(i);
        r.fillRound({x - 6, w.box.y + 276, 12, 12}, 6, i == cur ? c : kTextDim.withAlpha(0.35f), Blend::Over, i == cur);
    }
}

void PanelView::drawReadout(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const Rgba c = accentColour(w.accent);
    const Rect bx = R(w.box);
    r.fillRound(bx, 14, kGlass, Blend::Over);
    r.fillRound({bx.x + 2, bx.y + 2, bx.w - 4, 2}, 1, {1, 1, 1, 0.08f}, Blend::Add);
    if (hot) r.glow(bx.inset(4), 12, 12, c.withAlpha(0.25f), true);
    const std::string v = formatValue(b.desc(w.param), b.value(w.param));
    const float pw = r.textWidth(TextStyle::Title, w.prefix + "  ");
    const float vw = r.textWidth(TextStyle::Title, v);
    const float x0 = bx.cx() - (pw + vw) * 0.5f;
    r.text(TextStyle::Title, w.prefix, x0, bx.cy() + 12, kText, Align::Left, TextFx::Shadowed);
    r.text(TextStyle::Title, v, x0 + pw, bx.cy() + 12, c, Align::Left, TextFx::NeonCrisp);
}

void PanelView::drawMeter(Renderer& r, PanelBinding& b, const Widget& w) {
    const std::size_t s = static_cast<std::size_t>(std::max(w.source, 0));
    if (smooth_.size() <= s) smooth_.resize(s + 1, w.lo);
    const float raw = b.meter(w.source);
    float& v = smooth_[s];
    v = raw > v ? raw : v + (raw - v) * 0.2f;
    const Rect bx = R(w.box);
    r.fillRound(bx, 8, rgb8(0x020304, 0.96f), Blend::Over);
    const int segs = std::max(8, static_cast<int>(bx.h / 15.0f));
    const float segH = (bx.h - 10) / static_cast<float>(segs);
    const float f = std::clamp((v - w.lo) / (w.hi - w.lo), 0.0f, 1.0f);
    for (int i = 0; i < segs; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(segs);   // 0 = bottom
        const float y = w.fromTop ? bx.y + 5 + segH * static_cast<float>(i) : bx.bottom() - 5 - segH * static_cast<float>(i + 1);
        const bool lit = w.fromTop ? t < (1.0f - f) : t < f;
        Rgba sc = w.fromTop ? kCyan : (t > 0.85f ? rgb8(0xFF5A1F) : t > 0.65f ? rgb8(0xFFC233) : kCyan);
        if (w.accent == Accent::Orange && !w.fromTop && t <= 0.65f) sc = kAmber;
        r.fillRound({bx.x + 6, y + 1.5f, bx.w - 12, segH - 3}, 2, lit ? sc : sc.withAlpha(0.10f), Blend::Over, lit);
    }
    if (!w.label.empty())
        r.text(TextStyle::Caption, w.label, bx.cx(), bx.bottom() + 34, kText, Align::Center, TextFx::Shadowed);
}

void PanelView::drawDisplay(Renderer& r, PanelBinding& b, const Widget& w) {
    const Rgba c = accentColour(w.accent);
    const Rect bx = R(w.box);
    if (w.display == DisplayKind::BandMeters) {   // plain glass backdrop with a divider
        r.fillRound({bx.cx() - 1.5f, bx.y + 20, 3, bx.h - 40}, 1.5f, kTextDim.withAlpha(0.25f), Blend::Over);
        return;
    }
    r.pushClip(bx.inset(4));
    // grid
    for (int i = 1; i < 10; ++i) {
        const float x = bx.x + bx.w * static_cast<float>(i) / 10.0f;
        r.fillRound({x - 0.75f, bx.y, 1.5f, bx.h}, 0.5f, c.withAlpha(0.08f), Blend::Over);
    }
    for (int i = 1; i < 6; ++i) {
        const float y = bx.y + bx.h * static_cast<float>(i) / 6.0f;
        r.fillRound({bx.x, y - 0.75f, bx.w, 1.5f}, 0.5f, c.withAlpha(0.08f), Blend::Over);
    }
    if (w.display == DisplayKind::Analyzer) {
        constexpr int K = 160;
        feed_.assign(2 * K, -120.0f);
        b.feed(DisplayKind::Analyzer, w.source, feed_);
        float xs[K], ys[K], cxs[K], cys[K];
        const float top = bx.y + 30, bot = bx.bottom() - 50;
        auto specY = [&](float db) { return bot - (bot - top) * std::clamp((db + 84.0f) / 84.0f, 0.0f, 1.0f); };
        auto curveY = [&](float db) { return (top + bot) * 0.5f - (bot - top) * 0.5f * std::clamp(db / 12.0f, -1.0f, 1.0f); };
        for (int i = 0; i < K; ++i) {
            const float x = bx.x + bx.w * static_cast<float>(i) / (K - 1);
            xs[i] = cxs[i] = x;
            ys[i] = specY(feed_[static_cast<std::size_t>(i)]);
            cys[i] = curveY(feed_[static_cast<std::size_t>(K + i)]);
        }
        for (int i = 0; i + 1 < K; ++i) {   // filled spectrum
            const float qx[4] = {xs[i], xs[i + 1], xs[i + 1], xs[i]};
            const float qy[4] = {ys[i], ys[i + 1], bot, bot};
            const Rgba qc[4] = {c.withAlpha(0.35f), c.withAlpha(0.35f), c.withAlpha(0.02f), c.withAlpha(0.02f)};
            r.colorQuad(qx, qy, qc, Blend::Over);
        }
        r.polyline(xs, ys, K, 3, c.withAlpha(0.8f), Blend::Over, true);
        r.polyline(cxs, cys, K, 10, kOrange.withAlpha(0.25f), Blend::Add, true);
        r.polyline(cxs, cys, K, 4, kAmber, Blend::Over, true);
        // labels: dB scale and frequency marks
        r.text(TextStyle::Caption, "+12 dB", bx.x + 18, top + 8, kTextDim, Align::Left);
        r.text(TextStyle::Caption, "0 dB", bx.x + 18, (top + bot) * 0.5f + 8, kTextDim, Align::Left);
        r.text(TextStyle::Caption, "-12 dB", bx.x + 18, bot - 14, kTextDim, Align::Left);
        const float freqs[3] = {250.0f, 1000.0f, 10000.0f};
        const char* names[3] = {"250 Hz", "1 kHz", "10 kHz"};
        for (int i = 0; i < 3; ++i) {
            const float x = bx.x + bx.w * std::log(freqs[i] / 20.0f) / std::log(22000.0f / 20.0f);
            r.text(TextStyle::Caption, names[i], x, bx.bottom() - 16, kTextDim, Align::Center);
        }
        r.text(TextStyle::Label, w.label, bx.x + 18, bx.bottom() - 14, c, Align::Left, TextFx::NeonCrisp);
    }
    r.popClip();
}

void PanelView::drawTopBar(Renderer& r, PanelBinding& b, const PanelController& c) {
    // crown + title
    const float cx0 = 205, cy0 = 135;
    const float xs[8] = {cx0 - 60, cx0 - 60, cx0 - 30, cx0, cx0 + 30, cx0 + 60, cx0 + 60, cx0 - 60};
    const float ys[8] = {cy0 + 30, cy0 - 35, cy0 - 5, cy0 - 48, cy0 - 5, cy0 - 35, cy0 + 30, cy0 + 30};
    r.polyline(xs, ys, 8, 16, kAmber.withAlpha(0.30f), Blend::Add, true);
    r.polyline(xs, ys, 8, 7, kAmber, Blend::Over, true);
    r.text(TextStyle::Heading, "HYBRID ELITE MAFIA", 300, 156, kAmber, Align::Left, TextFx::Neon);
    const float tw = r.textWidth(TextStyle::Heading, "HYBRID ELITE MAFIA ");
    r.text(TextStyle::Heading, "// " + def_.name, 300 + tw, 156, rgb8(0xFFB070), Align::Left, TextFx::Neon, 1730 - tw - 300 + 150);
    auto pill = [&](const Box& bx, Hit::T t, bool lit) {
        const bool hot = c.hover().t == t;
        r.fillRound(R(bx).inset(4), 18, rgb8(0x0C0E12, 0.96f), Blend::Over);
        r.fillRound({bx.x + 8, bx.y + 6, bx.w - 16, bx.h * 0.35f}, 12, {1, 1, 1, 0.04f}, Blend::Add);
        if (lit || hot) r.glow(R(bx).inset(6), 14, 14, kOrange.withAlpha(lit ? 0.45f : 0.2f), true);
        if (lit) { r.fillRound({bx.x + 6, bx.y + 6, bx.w - 12, 3}, 2, kOrange, Blend::Over, true); r.fillRound({bx.x + 6, bx.b() - 9, bx.w - 12, 3}, 2, kOrange, Blend::Over, true); }
    };
    // undo / redo arrows
    pill(def_.undo, Hit::T::Undo, false);
    pill(def_.redo, Hit::T::Redo, false);
    {
        const float ux = def_.undo.cx(), uy = def_.undo.cy();
        float ax[24], ay[24];
        for (int i = 0; i < 24; ++i) { const float a = -0.1f * kPi + 1.1f * kPi * static_cast<float>(i) / 23.0f; ax[i] = ux + 4 + std::cos(a) * 26; ay[i] = uy + 8 - std::sin(a) * 26; }
        r.polyline(ax, ay, 24, 6, kText, Blend::Over);
        const float hx[3] = {ux - 36, ux - 22, ux - 8}, hy[3] = {uy - 4, uy + 14, uy - 4};
        r.polyline(hx, hy, 3, 6, kText, Blend::Over);
        const float rx = def_.redo.cx(), ry = def_.redo.cy();
        float bx2[24], by2[24];
        for (int i = 0; i < 24; ++i) { const float a = -0.1f * kPi + 1.1f * kPi * static_cast<float>(i) / 23.0f; bx2[i] = rx - 4 - std::cos(a) * 26; by2[i] = ry + 8 - std::sin(a) * 26; }
        r.polyline(bx2, by2, 24, 6, kTextDim, Blend::Over);
        const float gx[3] = {rx + 36, rx + 22, rx + 8}, gy[3] = {ry - 4, ry + 14, ry - 4};
        r.polyline(gx, gy, 3, 6, kTextDim, Blend::Over);
    }
    // preset box
    pill(def_.preset, Hit::T::Preset, false);
    const auto names = b.presetNames();
    const int cur = b.currentPreset();
    std::string pn = (cur >= 0 && cur < static_cast<int>(names.size())) ? names[static_cast<std::size_t>(cur)] : "Init";
    if (b.modified()) pn += " *";
    r.text(TextStyle::Title, pn, def_.preset.x + 34, def_.preset.cy() + 12, kText, Align::Left, TextFx::Shadowed, def_.preset.w - 60);
    pill(def_.presetPrev, Hit::T::PresetPrev, false);
    pill(def_.presetNext, Hit::T::PresetNext, false);
    pill(def_.presetSave, Hit::T::PresetSave, false);
    {
        const float px = def_.presetPrev.cx(), py = def_.presetPrev.cy();
        const float lx[3] = {px + 10, px - 12, px + 10}, ly[3] = {py - 22, py, py + 22};
        r.polyline(lx, ly, 3, 6, kText, Blend::Over);
        const float nx = def_.presetNext.cx(), ny = def_.presetNext.cy();
        const float rx[3] = {nx - 10, nx + 12, nx - 10};
        r.polyline(rx, ly, 3, 6, kText, Blend::Over);
        (void)ny;
        const float sx = def_.presetSave.cx(), sy = def_.presetSave.cy();
        r.fillRound({sx - 24, sy - 3, 48, 6}, 3, kText, Blend::Over);
        r.fillRound({sx - 3, sy - 24, 6, 48}, 3, kText, Blend::Over);
    }
    // BYPASS
    const bool byp = b.value(def_.bypassParam) >= 0.5f;
    pill(def_.bypass, Hit::T::Bypass, byp);
    r.text(TextStyle::Title, "BYPASS", def_.bypass.cx(), def_.bypass.cy() + 12, byp ? kAmber : kText, Align::Center, byp ? TextFx::NeonCrisp : TextFx::Shadowed);
}

void PanelView::hMeter(Renderer& r, Box box, float dbL, float dbR) {
    r.fillRound(R(box), 8, rgb8(0x020304, 0.96f), Blend::Over);
    const int segs = 48;
    const float sw = (box.w - 12) / segs, rowH = (box.h - 14) * 0.5f;
    for (int row = 0; row < 2; ++row) {
        const float f = std::clamp(((row ? dbR : dbL) + 60.0f) / 60.0f, 0.0f, 1.0f);
        for (int i = 0; i < segs; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / segs;
            const Rgba sc = t > 0.92f ? rgb8(0xFF3B1F) : t > 0.80f ? rgb8(0xFF8A1E) : t > 0.66f ? rgb8(0xFFC233) : kCyan;
            const bool lit = t < f;
            r.fillRound({box.x + 6 + sw * static_cast<float>(i) + 1, box.y + 5 + row * (rowH + 4), sw - 2, rowH}, 1.5f,
                        lit ? sc : sc.withAlpha(0.09f), Blend::Over, lit);
        }
    }
}

void PanelView::drawBottomBar(Renderer& r, PanelBinding& b, const PanelController& c) {
    const auto& di = b.desc(def_.inputParam);
    const auto& dout = b.desc(def_.outputParam);
    const bool hi = c.hover().t == Hit::T::InTrim || c.active().t == Hit::T::InTrim;
    const bool ho = c.hover().t == Hit::T::OutTrim || c.active().t == Hit::T::OutTrim;
    r.text(TextStyle::Title, formatValue(di, b.value(def_.inputParam)), def_.inputTrim.x + 20, def_.inputTrim.cy() + 12, hi ? kCyan : kAmber, Align::Left, TextFx::NeonCrisp);
    r.text(TextStyle::Label, "INPUT", def_.inputMeter.x - 18, def_.inputMeter.y - 12, kText, Align::Right, TextFx::Shadowed);
    hMeter(r, def_.inputMeter, b.meter(def_.meterInL), b.meter(def_.meterInR));
    hMeter(r, def_.outputMeter, b.meter(def_.meterOutL), b.meter(def_.meterOutR));
    r.text(TextStyle::Label, "OUTPUT", def_.outputMeter.r() + 18, def_.outputMeter.y - 12, kText, Align::Left, TextFx::Shadowed);
    r.text(TextStyle::Title, formatValue(dout, b.value(def_.outputParam)), def_.outputTrim.r() - 20, def_.outputTrim.cy() + 12, ho ? kCyan : kAmber, Align::Right, TextFx::NeonCrisp);
    r.text(TextStyle::Heading, def_.footer, def_.footerBox.cx(), def_.footerBox.cy() + 20, rgb8(0xE9C9A0), Align::Center, TextFx::Engraved, def_.footerBox.w);
}

void PanelView::drawPresetMenu(Renderer& r, PanelBinding& b, const PanelController& c) {
    const Box m = c.menuBox();
    r.glow(R(m), 20, 30, kOrange.withAlpha(0.35f), true);
    r.fillRound(R(m), 20, rgb8(0x0A0B0E, 0.97f), Blend::Over);
    const auto names = b.presetNames();
    const int n = static_cast<int>(std::min<std::size_t>(names.size(), 18));
    for (int i = 0; i < n; ++i) {
        const Box it = c.menuItem(i);
        const bool hot = c.hover().t == Hit::T::MenuItem && c.hover().w == i;
        const bool cur = i == b.currentPreset();
        if (hot || cur) r.fillRound(R(it), 12, kOrange.withAlpha(cur ? 0.22f : 0.12f), Blend::Over, cur);
        r.text(TextStyle::Title, names[static_cast<std::size_t>(i)], it.x + 24, it.cy() + 12, cur ? kAmber : kText, Align::Left, TextFx::Shadowed, it.w - 40);
    }
}

} // namespace dm::ui
