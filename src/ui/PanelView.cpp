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
const Rgba kText = rgb8(0xC9CDD4);      // engraved-white panel lettering
const Rgba kTextBright = rgb8(0xE4E7EC);
const Rgba kTextDim = rgb8(0x7D838E);
const Rgba kGlass = rgb8(0x07090C, 0.94f);
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
    // Heavy machined frame -> raised inner lip -> recessed brushed field that carries the bays.
    const Box frame{8, 8, kPlateW - 16, kPlateH - 16};
    const Box lip{70, 40, kPlateW - 140, kPlateH - 80};
    const Box field{118, 222, kPlateW - 236, 1752};
    std::vector<Box> bays, glass, wells;
    for (const auto& w : def.widgets) {
        if (w.kind == Kind::Bay) bays.push_back(w.box);
        if (w.kind == Kind::Display) glass.push_back(w.box);
    }
    // recessed wells in the frame: preset strip, top buttons, input / output capsules
    wells.push_back({def.preset.x - 6, def.preset.y - 6, def.presetNext.r() - def.preset.x + 12, def.preset.h + 12});
    wells.push_back({def.inputTrim.x - 30, 1998, def.inputMeter.r() - def.inputTrim.x + 60, 114});
    wells.push_back({def.outputMeter.x - 200, 1998, def.outputTrim.r() - def.outputMeter.x + 230, 114});
    const std::array<std::array<float, 2>, 4> screws{{{60, 60}, {kPlateW - 60, 60}, {60, kPlateH - 60}, {kPlateW - 60, kPlateH - 60}}};

    auto grad = [](float (*sd)(float, float, const Box&, float), float x, float y, const Box& b, float rad, float& nx, float& ny) {
        const float e = 0.75f;
        nx = sd(x + e, y, b, rad) - sd(x - e, y, b, rad);
        ny = sd(x, y + e, b, rad) - sd(x, y - e, b, rad);
        const float l = std::sqrt(nx * nx + ny * ny) + 1e-6f;
        nx /= l; ny /= l;
    };
    for (std::uint32_t py = 0; py < img.height; ++py) {
        for (std::uint32_t px = 0; px < img.width; ++px) {
            const float ux = (static_cast<float>(px) + 0.5f) / ppu, uy = (static_cast<float>(py) + 0.5f) / ppu;
            const float dF = sdRound(ux, uy, frame, 120);
            if (dF > 1.0f) continue;
            float L;
            float tR = 1.0f, tG = 1.0f, tB = 1.05f;
            const float dLip = sdRound(ux, uy, lip, 90);
            const float dFld = sdRound(ux, uy, field, 34);
            float nx, ny;
            if (dLip > 0.0f) {
                // outer frame: dark anodised gunmetal, rolled outer edge
                L = 0.008f + 0.007f * brushed(ux * 0.6f, uy) + 0.005f * (1.0f - uy / kPlateH);
                grad(sdRound, ux, uy, frame, 120, nx, ny);
                const float roll = std::clamp(1.0f + dF / 26.0f, 0.0f, 1.0f);
                L += 0.16f * roll * roll * std::max(0.0f, bevelLight(-nx, -ny)) - 0.004f * roll;
                grad(sdRound, ux, uy, lip, 90, nx, ny);
                const float wall = std::clamp(1.0f - dLip / 10.0f, 0.0f, 1.0f);   // step down onto the lip
                L += 0.10f * wall * bevelLight(-nx, -ny);
            } else if (dFld > 0.0f) {
                // machined top / bottom deck and side rails: brushed steel, horizontal grain
                L = 0.013f + 0.011f * brushed(ux, uy) + 0.008f * std::exp(-std::pow((uy - 120.0f) / 160.0f, 2.0f));
                grad(sdRound, ux, uy, lip, 90, nx, ny);
                const float edge = std::clamp(1.0f + dLip / 8.0f, 0.0f, 1.0f);
                L += 0.10f * edge * bevelLight(-nx, -ny);
                grad(sdRound, ux, uy, field, 34, nx, ny);
                const float ch = std::clamp(1.0f - dFld / 12.0f, 0.0f, 1.0f);       // chamfer down into the field
                L += 0.08f * ch * bevelLight(nx, ny);
                for (const Box& w : wells) {
                    const float d = sdRound(ux, uy, w, 22);
                    if (d < 0.0f) {
                        L = 0.012f + 0.006f * brushed(ux * 0.4f, uy);
                        L *= 0.55f + 0.45f * std::clamp((uy - w.y) / 26.0f, 0.0f, 1.0f);
                        if (d > -2.5f && uy > w.cy()) L += 0.07f;
                    } else if (d < 5.0f) {
                        L *= 0.7f + 0.3f * d / 5.0f;
                    }
                }
            } else {
                // recessed field: dark brushed charcoal with a soft vertical sheen
                const float sheen = std::exp(-std::pow((ux - kPlateW * 0.5f) / 1400.0f, 2.0f));
                L = 0.0055f + 0.0035f * brushed(ux, uy) + 0.003f * sheen;
                L *= 0.6f + 0.4f * std::clamp(-dFld / 30.0f, 0.0f, 1.0f);       // shadow at the recess wall
                for (const Box& b : bays) {
                    const float d = sdRound(ux, uy, b, 26);
                    if (d < 0.0f) {
                        // smoked satin panel, lighter at the top, faint vertical brushing
                        const float t = std::clamp((uy - b.y) / b.h, 0.0f, 1.0f);
                        L = 0.019f - 0.008f * t + 0.004f * brushed(uy * 0.7f, ux * 0.6f);
                        L += 0.005f * std::exp(-std::pow((ux - b.cx()) / (b.w * 0.35f), 2.0f)) * (1.0f - t);
                        grad(sdRound, ux, uy, b, 26, nx, ny);
                        const float rim = std::clamp(1.0f + d / 3.0f, 0.0f, 1.0f);
                        L += 0.07f * rim * std::max(0.0f, bevelLight(-nx, -ny)) + 0.015f * rim;
                    } else if (d < 22.0f) {
                        L *= 0.45f + 0.55f * (d / 22.0f);   // soft contact shadow round each panel
                    }
                }
                for (const Box& g : glass) {
                    const float d = sdRound(ux, uy, g, 16);
                    if (d < 0.0f) {
                        const float t = std::clamp((uy - g.y) / g.h, 0.0f, 1.0f);
                        L = 0.010f + 0.008f * (1.0f - t);
                        tB = 1.45f; tG = 1.18f;
                        L *= 0.55f + 0.45f * std::clamp(std::min(uy - g.y, ux - g.x) / 22.0f, 0.0f, 1.0f);
                        if (d > -3.0f) { L += 0.05f; tB = 1.6f; tG = 1.35f; }
                    } else if (d < 4.0f) {
                        L = 0.09f;   // bright bezel lip around the glass
                    }
                }
            }
            for (const auto& sc : screws) {
                const float dx = ux - sc[0], dy = uy - sc[1], rr = std::sqrt(dx * dx + dy * dy);
                if (rr < 26.0f) {
                    const float qx = dx / 26.0f, qy = dy / 26.0f, qz = std::sqrt(std::max(0.0f, 1.0f - qx * qx - qy * qy));
                    const float diff = std::max(0.0f, -0.48f * qx - 0.62f * qy + 0.62f * qz);
                    L = 0.05f + 0.24f * diff + 0.45f * std::pow(diff, 24.0f);
                    const float ax = std::fabs(dx), ay = std::fabs(dy);
                    if (std::max(ax * 0.866f + ay * 0.5f, ay) < 10.0f) L = 0.008f;
                    tB = 1.0f; tG = 1.0f;
                } else if (rr < 31.0f) {
                    L *= 0.5f;
                }
            }
            const float a = std::clamp(0.5f - dF, 0.0f, 1.0f);
            std::uint8_t* p = &img.rgba[(static_cast<std::size_t>(py) * img.width + px) * 4];
            p[0] = hc::ui::linearToSrgb8(std::min(1.0f, L * tR));
            p[1] = hc::ui::linearToSrgb8(std::min(1.0f, L * tG));
            p[2] = hc::ui::linearToSrgb8(std::min(1.0f, L * tB));
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
    // Light strip set into the outer frame: electric cyan down the left, molten orange down the
    // right, blending across the top and bottom runs. Drawn as smooth shaded strips + corner arcs.
    const float in = 26, rad = 100;
    const float L = in, T = in, Rr = kPlateW - in, B = kPlateH - in;
    auto mixc = [](Rgba a, Rgba b, float t) { return Rgba{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f}; };
    auto colourAt = [&](float x) { return mixc(kCyan, kOrange, std::clamp((x - 1100.0f) / 1640.0f, 0.0f, 1.0f)); };
    // one straight run from (x0,y0) to (x1,y1) (axis-aligned), core + soft falloff both sides
    auto run = [&](float x0, float y0, float x1, float y1) {
        const bool horiz = std::fabs(y1 - y0) < 0.5f;
        const int pieces = horiz ? 24 : 1;
        for (int k = 0; k < pieces; ++k) {
            const float t0 = static_cast<float>(k) / pieces, t1 = static_cast<float>(k + 1) / pieces;
            const float ax = x0 + (x1 - x0) * t0, ay = y0 + (y1 - y0) * t0, bx = x0 + (x1 - x0) * t1, by = y0 + (y1 - y0) * t1;
            const Rgba ca = colourAt(ax), cb = colourAt(bx);
            const struct { float w0, w1, a0, a1; } bands[3] = {{0, 2.4f, 1.0f, 0.9f}, {2.4f, 9, 0.45f, 0.12f}, {9, 30, 0.12f, 0.0f}};
            for (const auto& bd : bands)
                for (float side : {-1.0f, 1.0f}) {
                    float qx[4], qy[4];
                    if (horiz) {
                        qx[0] = ax; qx[1] = bx; qx[2] = bx; qx[3] = ax;
                        qy[0] = ay + side * bd.w0; qy[1] = by + side * bd.w0; qy[2] = by + side * bd.w1; qy[3] = ay + side * bd.w1;
                    } else {
                        qy[0] = ay; qy[1] = by; qy[2] = by; qy[3] = ay;
                        qx[0] = ax + side * bd.w0; qx[1] = bx + side * bd.w0; qx[2] = bx + side * bd.w1; qx[3] = ax + side * bd.w1;
                    }
                    const Rgba cols[4] = {ca.withAlpha(bd.a0), cb.withAlpha(bd.a0), cb.withAlpha(bd.a1), ca.withAlpha(bd.a1)};
                    r.colorQuad(qx, qy, cols, Blend::Add, true);
                }
        }
    };
    run(L + rad, T, Rr - rad, T);
    run(L + rad, B, Rr - rad, B);
    run(L, T + rad, L, B - rad);
    run(Rr, T + rad, Rr, B - rad);
    const struct { float cx, cy, a0; Rgba c; } corners[4] = {
        {L + rad, T + rad, -0.5f * kPi, kCyan}, {Rr - rad, T + rad, 0.0f, kOrange},
        {Rr - rad, B - rad, 0.5f * kPi, kOrange}, {L + rad, B - rad, -kPi, kCyan}};
    for (const auto& c : corners) {
        r.arc(c.cx, c.cy, rad, 40, c.a0, c.a0 + 0.5f * kPi, c.c.withAlpha(0.06f), true);
        r.arc(c.cx, c.cy, rad, 16, c.a0, c.a0 + 0.5f * kPi, c.c.withAlpha(0.22f), true);
        r.arc(c.cx, c.cy, rad, 4.8f, c.a0, c.a0 + 0.5f * kPi, c.c.withAlpha(0.95f), true);
    }
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
    // short accent light-pipe centred on the top edge of the panel
    if (w.label.empty()) {   // control deck panels carry a short accent light-pipe on the top edge
        const float lw = std::min(w.box.w * 0.5f, 440.0f);
        const Rect pipe{w.box.cx() - lw * 0.5f, w.box.y - 2, lw, 4};
        r.glow(pipe, 2.0f, 14, c.withAlpha(0.25f), true);
        r.fillRound(pipe, 2.0f, c.withAlpha(0.75f), Blend::Over, true);
    }
    if (!w.label.empty())
        r.text(TextStyle::Tab, w.label, w.box.cx(), w.box.y + 92, c, Align::Center, TextFx::NeonCrisp, w.box.w - 320);
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
    const bool big = w.size >= 0.8f;
    // LED dot ring: lit in the accent up to the value, unlit dots stay as dark pin-holes
    if (big) {
        const int ticks = 41;
        for (int i = 0; i < ticks; ++i) {
            const float t = static_cast<float>(i) / (ticks - 1);
            const float a = kKnobMin + (kKnobMax - kKnobMin) * t;
            const float rr = R0 * 1.13f;
            const float x = cx + std::sin(a) * rr, y = cy - std::cos(a) * rr;
            const bool lit = bip ? ((a >= std::min(a0, ang) - 1e-4f) && (a <= std::max(a0, ang) + 1e-4f)) : t <= n + 1e-4f;
            const float sz = R0 * 0.032f;
            if (lit) {
                r.sprite(Sprite::GlowDot, {x - sz * 4, y - sz * 4, sz * 8, sz * 8}, c.withAlpha(0.55f), Blend::Add, true);
                r.fillRound({x - sz, y - sz, 2 * sz, 2 * sz}, sz, c, Blend::Over, true);
            } else {
                r.fillRound({x - sz, y - sz, 2 * sz, 2 * sz}, sz, rgb8(0x454A54), Blend::Over);
            }
        }
    }
    // knob: dark well, black knurled body, spun cap, white pointer
    const float R = R0 * 0.96f;
    r.fillRound({cx - R, cy - R, 2 * R, 2 * R}, R, rgb8(0x050608), Blend::Over);
    r.arc(cx, cy, R - 2, 3, -kPi, kPi, {1, 1, 1, 0.07f}, false);
    const float bodyR = R * 0.93f;
    r.sprite(Sprite::KnobShadow, {cx - bodyR * 1.18f + 0.08f * bodyR, cy - bodyR * 1.18f + 0.14f * bodyR, bodyR * 2.36f, bodyR * 2.36f});
    r.spriteRotated(Sprite::KnobBody, cx, cy, bodyR * 2.0f, bodyR * 2.0f, ang, {0.34f, 0.35f, 0.37f, 1.0f});
    const float len = (hc::ui::knob::kIndicatorTo - hc::ui::knob::kIndicatorFrom) * bodyR / hc::ui::knob::kBody * 0.9f;
    const float mid = bodyR * 0.62f;
    r.spriteRotated(Sprite::KnobIndicator, cx + std::sin(ang) * mid, cy - std::cos(ang) * mid, len * 0.25f, len, ang,
                    {1.0f, 1.0f, 1.0f, 1.0f}, Blend::Over, true);
    r.sprite(Sprite::KnobLight, {cx - bodyR, cy - bodyR, 2 * bodyR, 2 * bodyR}, {1, 1, 1, 0.8f});
    if (hot) r.glow({cx - R * 0.9f, cy - R * 0.9f, R * 1.8f, R * 1.8f}, R * 0.9f, 20, c.withAlpha(0.22f), true);
    if (big) {
        const std::string text = hot ? formatValue(d, b.value(w.param)) : w.label;
        r.text(TextStyle::Title, text, cx, cy + R0 * 1.13f + 50, hot ? c : kText, Align::Center, hot ? TextFx::NeonCrisp : TextFx::Shadowed);
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
    auto thumb = [&](float x, float y, float rad) {
        r.sprite(Sprite::KnobShadow, {x - rad * 1.3f + rad * 0.12f, y - rad * 1.3f + rad * 0.22f, rad * 2.6f, rad * 2.6f});
        r.fillRound({x - rad, y - rad, 2 * rad, 2 * rad}, rad, rgb8(0x0B0C0F), Blend::Over);
        r.spriteRotated(Sprite::KnobBody, x, y, rad * 1.86f, rad * 1.86f, 0.0f, {0.62f, 0.64f, 0.68f, 1.0f});
        r.sprite(Sprite::KnobLight, {x - rad * 0.93f, y - rad * 0.93f, rad * 1.86f, rad * 1.86f});
        r.fillRound({x - rad * 0.16f, y - rad * 0.16f, rad * 0.32f, rad * 0.32f}, rad * 0.16f, c.withAlpha(0.9f), Blend::Over, true);
    };
    if (w.vertical) {
        const float sx = w.box.cx(), slotW = 16;
        const float y0 = w.box.y + 40, y1 = w.box.b() - 40;
        r.fillRound({sx - slotW / 2, y0, slotW, y1 - y0}, slotW / 2, rgb8(0x010203), Blend::Over);
        r.sprite(Sprite::InnerShadow, {sx - slotW / 2, y0, slotW, y1 - y0});
        const bool tall = (y1 - y0) > 600.0f;
        const int nt = tall ? 7 : 6;
        for (int t = 0; t < nt; ++t) {
            const float f = static_cast<float>(t) / static_cast<float>(nt - 1);
            const float y = y0 + (y1 - y0) * f;
            if (tall) {
                // real scale read back from the parameter's own range
                const std::string v = formatValue(d, fromNorm(d, 1.0f - f));
                r.text(TextStyle::Caption, v, sx + 62, y + 10, kTextDim, Align::Left);
                r.fillRound({sx + 26, y - 1.5f, 24, 3}, 1.5f, kTextDim.withAlpha(0.6f), Blend::Over);
            } else {
                r.fillRound({sx - 44, y - 1.5f, 18, 3}, 1.5f, kTextDim.withAlpha(0.5f), Blend::Over);
                r.fillRound({sx + 26, y - 1.5f, 18, 3}, 1.5f, kTextDim.withAlpha(0.5f), Blend::Over);
            }
        }
        const float cy = y1 - (y1 - y0) * n;
        r.fillRound({sx - 4, cy, 8, y1 - cy}, 4, c.withAlpha(0.9f), Blend::Over, true);
        r.glow({sx - 4, cy, 8, y1 - cy}, 4, 14, c.withAlpha(0.30f), true);
        thumb(sx, cy, tall ? 40.0f : 34.0f);
        const std::string text = hot ? formatValue(d, b.value(w.param)) : w.label;
        r.text(TextStyle::Title, text, sx, w.box.b() + 56, hot ? c : kText, Align::Center, hot ? TextFx::NeonCrisp : TextFx::Shadowed);
    } else {
        const float sy = w.box.cy(), slotH = 14;
        const float x0 = w.box.x + 70, x1 = w.box.r() - 70;
        r.fillRound({x0, sy - slotH / 2, x1 - x0, slotH}, slotH / 2, rgb8(0x010203), Blend::Over);
        const bool bip = d.min < 0.0f && d.max > 0.0f;
        const float cx = x0 + (x1 - x0) * n, mx = bip ? (x0 + x1) * 0.5f : x0;
        r.fillRound({std::min(mx, cx), sy - 4, std::fabs(cx - mx), 8}, 4, c.withAlpha(0.9f), Blend::Over, true);
        if (bip) {   // tilt end marks: dark (low shelf) on the left, bright (high shelf) on the right
            const float lx[4] = {x0 - 58, x0 - 40, x0 - 24, x0 - 8}, ly[4] = {sy - 12, sy - 12, sy + 10, sy + 10};
            r.polyline(lx, ly, 4, 4, kTextDim, Blend::Over);
            const float rx[4] = {x1 + 8, x1 + 24, x1 + 40, x1 + 58}, ry[4] = {sy + 10, sy + 10, sy - 12, sy - 12};
            r.polyline(rx, ry, 4, 4, kTextDim, Blend::Over);
        }
        capChrome(r, cx, sy, 40, std::min(w.box.h * 0.85f, 74.0f), c, true);
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
    if (w.label == "S") {   // small round solo button, ringed like the mockup's corner icons
        const float cx = w.box.cx(), cy = w.box.cy(), rr = w.box.w * 0.27f;
        r.fillRound({cx - rr, cy - rr, 2 * rr, 2 * rr}, rr, lit ? c.withAlpha(0.85f) : rgb8(0x0E1013), Blend::Over, lit);
        r.arc(cx, cy, rr, 2.5f, -kPi, kPi, lit ? c : kTextDim.withAlpha(0.7f), lit);
        r.text(TextStyle::Caption, "S", cx, cy + 10, lit ? rgb8(0x0A0B0E) : kTextDim, Align::Center, TextFx::Plain);
        if (hot) r.glow({cx - rr, cy - rr, 2 * rr, 2 * rr}, rr, 10, c.withAlpha(0.3f), true);
        return;
    }
    // small rounded switch with a lit lens when on
    const Rect bx = R(w.box);
    r.fillRound(bx, bx.h * 0.5f, rgb8(0x08090B), Blend::Over);
    r.fillRound(bx.inset(3), bx.h * 0.5f - 3, lit ? c.withAlpha(0.16f) : rgb8(0x1A1C21), Blend::Over, lit);
    r.fillRound({bx.x + 10, bx.y + 5, bx.w - 20, bx.h * 0.38f}, bx.h * 0.2f, {1, 1, 1, lit ? 0.05f : 0.06f}, Blend::Add);
    if (lit) r.glow(bx.inset(4), bx.h * 0.5f, 14, c.withAlpha(0.35f), true);
    if (hot) r.glow(bx.inset(6), bx.h * 0.5f, 10, c.withAlpha(0.22f), true);
    r.text(TextStyle::Label, w.label, bx.cx(), bx.cy() + 13, lit ? c : kText, Align::Center, lit ? TextFx::NeonCrisp : TextFx::Shadowed, bx.w - 24);
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
    const Rgba ac = hot ? c : kText.withAlpha(0.65f);
    const float ay = w.box.y + 105;
    const float lx[3] = {w.box.x + 70, w.box.x + 40, w.box.x + 70}, ly[3] = {ay - 30, ay, ay + 30};
    r.polyline(lx, ly, 3, 5, ac, Blend::Over, hot);
    const float rx[3] = {w.box.r() - 70, w.box.r() - 40, w.box.r() - 70};
    r.polyline(rx, ly, 3, 5, ac, Blend::Over, hot);
    // mode name + position dots
    r.text(TextStyle::Tab, formatValue(d, b.value(w.param)), cx, w.box.y + 245, on ? kTextBright : kTextDim, Align::Center, TextFx::Shadowed, w.box.w - 60);
    const int n = static_cast<int>(d.choices.size());
    const float gap = 26.0f, x0 = cx - gap * static_cast<float>(n - 1) * 0.5f;
    for (int i = 0; i < n; ++i) {
        const float x = x0 + gap * static_cast<float>(i);
        r.fillRound({x - 5, w.box.y + 276, 10, 10}, 5, i == cur ? c : kTextDim.withAlpha(0.35f), Blend::Over, i == cur);
    }
}

void PanelView::drawReadout(Renderer& r, PanelBinding& b, const Widget& w, bool hot) {
    const Rgba c = accentColour(w.accent);
    const Rect bx = R(w.box);
    r.fillRound(bx, 12, rgb8(0x3A3E46), Blend::Over);          // thin machined rim
    r.fillRound(bx.inset(2.5f), 10, kGlass, Blend::Over);
    r.fillRound({bx.x + 4, bx.y + 4, bx.w - 8, bx.h * 0.4f}, 8, {1, 1, 1, 0.03f}, Blend::Add);
    if (hot) r.glow(bx.inset(4), 10, 12, c.withAlpha(0.25f), true);
    const std::string v = formatValue(b.desc(w.param), b.value(w.param));
    const float pw = r.textWidth(TextStyle::Title, w.prefix + "  ");
    const float vw = r.textWidth(TextStyle::Title, v);
    const float x0 = bx.cx() - (pw + vw) * 0.5f;
    r.text(TextStyle::Title, w.prefix, x0, bx.cy() + 15, kText, Align::Left, TextFx::Shadowed);
    r.text(TextStyle::Title, v, x0 + pw, bx.cy() + 15, kAmber, Align::Left, TextFx::NeonCrisp);
}

void PanelView::drawMeter(Renderer& r, PanelBinding& b, const Widget& w) {
    const std::size_t s = static_cast<std::size_t>(std::max(w.source, 0));
    if (smooth_.size() <= s) smooth_.resize(s + 1, w.lo);
    const float raw = b.meter(w.source);
    float& v = smooth_[s];
    v = raw > v ? raw : v + (raw - v) * 0.2f;
    const Rect bx = R(w.box);
    if (bx.h > 400) {   // tall slim gain-reduction bar: continuous lit column
        const float tw = 12, tx = bx.cx() - tw * 0.5f;
        r.fillRound({tx, bx.y, tw, bx.h}, tw * 0.5f, rgb8(0x010203), Blend::Over);
        r.sprite(Sprite::InnerShadow, {tx, bx.y, tw, bx.h});
        const float f = std::clamp((v - w.lo) / (w.hi - w.lo), 0.0f, 1.0f);
        const float len = w.fromTop ? (1.0f - f) * bx.h : f * bx.h;
        const Rect fill = w.fromTop ? Rect{tx + 2, bx.y + 2, tw - 4, std::max(0.0f, len - 4)} : Rect{tx + 2, bx.bottom() - len + 2, tw - 4, std::max(0.0f, len - 4)};
        if (fill.h > 1) { r.fillRound(fill, 4, kCyan, Blend::Over, true); r.glow(fill, 4, 12, kCyan.withAlpha(0.3f), true); }
        for (int t = 0; t <= 6; ++t) {
            const float y = bx.y + bx.h * static_cast<float>(t) / 6.0f;
            r.fillRound({tx + tw + 8, y - 1.5f, 14, 3}, 1.5f, kTextDim.withAlpha(0.5f), Blend::Over);
        }
        if (!w.label.empty())
            r.text(TextStyle::Label, w.label, bx.cx(), bx.y - 22, kText, Align::Center, TextFx::Shadowed);
        return;
    }
    r.fillRound(bx, 6, rgb8(0x020304, 0.96f), Blend::Over);
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
    const float cx0 = 330, cy0 = 122;
    const float xs[8] = {cx0 - 58, cx0 - 66, cx0 - 30, cx0, cx0 + 30, cx0 + 66, cx0 + 58, cx0 - 58};
    const float ys[8] = {cy0 + 34, cy0 - 30, cy0 - 2, cy0 - 46, cy0 - 2, cy0 - 30, cy0 + 34, cy0 + 34};
    r.polyline(xs, ys, 8, 14, kAmber.withAlpha(0.22f), Blend::Add, true);
    r.polyline(xs, ys, 8, 6, kAmber, Blend::Over, true);
    r.text(TextStyle::Heading, "HYBRID ELITE MAFIA", 420, 148, kAmber, Align::Left, TextFx::NeonCrisp);
    const float tw = r.textWidth(TextStyle::Heading, "HYBRID ELITE MAFIA ");
    r.text(TextStyle::Heading, "// " + def_.name, 420 + tw, 148, rgb8(0xFFA65A), Align::Left, TextFx::NeonCrisp, def_.preset.x - 50 - (420 + tw));
    // square machined key with a thin rim
    auto key = [&](const Box& bx, Hit::T t, Rgba rim, bool lit) {
        const bool hot = c.hover().t == t || c.active().t == t;
        r.fillRound(R(bx), 14, rim, Blend::Over, lit);
        r.fillRound(R(bx).inset(2.5f), 12, rgb8(0x111317), Blend::Over);
        r.fillRound({bx.x + 6, bx.y + 5, bx.w - 12, bx.h * 0.42f}, 10, {1, 1, 1, 0.05f}, Blend::Add);
        if (lit) r.glow(R(bx).inset(4), 14, 16, rim.withAlpha(0.45f), true);
        if (hot) r.glow(R(bx).inset(6), 12, 10, kCyan.withAlpha(0.18f), true);
    };
    const Rgba rimDim = rgb8(0x343840);
    // preset strip
    r.fillRound(R(def_.preset), 12, rgb8(0x07080A), Blend::Over);
    r.fillRound({def_.preset.x + 2, def_.preset.b() - 4, def_.preset.w - 4, 2}, 1, {1, 1, 1, 0.06f}, Blend::Add);
    if (c.hover().t == Hit::T::Preset) r.glow(R(def_.preset).inset(4), 12, 10, kCyan.withAlpha(0.15f), true);
    const auto names = b.presetNames();
    const int cur = b.currentPreset();
    std::string pn = (cur >= 0 && cur < static_cast<int>(names.size())) ? names[static_cast<std::size_t>(cur)] : "Init";
    if (b.modified()) pn += " *";
    r.text(TextStyle::Title, pn, def_.preset.x + 32, def_.preset.cy() + 16, kTextBright, Align::Left, TextFx::Shadowed, def_.preset.w - 56);
    key(def_.presetPrev, Hit::T::PresetPrev, rimDim, false);
    key(def_.presetNext, Hit::T::PresetNext, rimDim, false);
    key(def_.presetSave, Hit::T::PresetSave, rimDim, false);
    key(def_.undo, Hit::T::Undo, rimDim, false);
    key(def_.redo, Hit::T::Redo, rimDim, false);
    {
        const float px = def_.presetPrev.cx(), py = def_.presetPrev.cy();
        const float lx[3] = {px + 9, px - 11, px + 9}, ly[3] = {py - 20, py, py + 20};
        r.polyline(lx, ly, 3, 5, kText, Blend::Over);
        const float nx = def_.presetNext.cx();
        const float rx[3] = {nx - 9, nx + 11, nx - 9};
        r.polyline(rx, ly, 3, 5, kText, Blend::Over);
        const float sx = def_.presetSave.cx(), sy = def_.presetSave.cy();
        r.fillRound({sx - 22, sy - 2.5f, 44, 5}, 2.5f, kText, Blend::Over);
        r.fillRound({sx - 2.5f, sy - 22, 5, 44}, 2.5f, kText, Blend::Over);
        // undo / redo hooked arrows
        auto hook = [&](float x, float y, float dir, Rgba col) {
            float ax[20], ay[20];
            for (int i = 0; i < 20; ++i) {
                const float a = -0.55f * kPi + 1.1f * kPi * static_cast<float>(i) / 19.0f;
                ax[i] = x + dir * (std::sin(a) * 22.0f) + dir * 4; ay[i] = y - std::cos(a) * 18.0f + 4;
            }
            r.polyline(ax, ay, 20, 5, col, Blend::Over);
            const float hx[3] = {x - dir * 28, x - dir * 18, x - dir * 6}, hy[3] = {y - 22, y - 12, y - 24};
            r.polyline(hx, hy, 3, 5, col, Blend::Over);
        };
        hook(def_.undo.cx(), def_.undo.cy(), 1.0f, kText);
        hook(def_.redo.cx(), def_.redo.cy(), -1.0f, kTextDim);
    }
    // BYPASS: orange-rimmed key, lights up when engaged
    const bool byp = b.value(def_.bypassParam) >= 0.5f;
    key(def_.bypass, Hit::T::Bypass, kAmber.withAlpha(byp ? 1.0f : 0.75f), byp);
    if (byp) r.fillRound(R(def_.bypass).inset(3), 11, kAmber.withAlpha(0.18f), Blend::Over, true);
    r.text(TextStyle::Title, "BYPASS", def_.bypass.cx(), def_.bypass.cy() + 16, byp ? rgb8(0xFFD3A0) : kAmber, Align::Center, TextFx::NeonCrisp);
}

void PanelView::hMeter(Renderer& r, Box box, float dbL, float dbR) {
    r.fillRound(R(box), 6, rgb8(0x010203), Blend::Over);
    const int segs = 44;
    const float sw = (box.w - 10) / segs, rowH = (box.h - 12) * 0.5f;
    for (int row = 0; row < 2; ++row) {
        const float f = std::clamp(((row ? dbR : dbL) + 60.0f) / 60.0f, 0.0f, 1.0f);
        for (int i = 0; i < segs; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / segs;
            const Rgba sc = t > 0.92f ? rgb8(0xFF3B1F) : t > 0.82f ? rgb8(0xFF7A1E) : t > 0.70f ? rgb8(0xFFC233) : kCyan;
            const bool lit = t < f;
            r.fillRound({box.x + 5 + sw * static_cast<float>(i) + 1, box.y + 4 + row * (rowH + 4), sw - 2.5f, rowH}, 1.0f,
                        lit ? sc : sc.withAlpha(0.13f), Blend::Over, lit);
        }
    }
}

void PanelView::drawBottomBar(Renderer& r, PanelBinding& b, const PanelController& c) {
    const auto& di = b.desc(def_.inputParam);
    const auto& dout = b.desc(def_.outputParam);
    const bool hi = c.hover().t == Hit::T::InTrim || c.active().t == Hit::T::InTrim;
    const bool ho = c.hover().t == Hit::T::OutTrim || c.active().t == Hit::T::OutTrim;
    const float base = def_.inputMeter.cy() + 15;
    r.text(TextStyle::Title, formatValue(di, b.value(def_.inputParam)), def_.inputTrim.x + 10, base, hi ? kCyan : kAmber, Align::Left, TextFx::NeonCrisp);
    r.text(TextStyle::Title, "INPUT", def_.inputMeter.x - 18, base, kTextBright, Align::Right, TextFx::Shadowed);
    hMeter(r, def_.inputMeter, b.meter(def_.meterInL), b.meter(def_.meterInR));
    hMeter(r, def_.outputMeter, b.meter(def_.meterOutL), b.meter(def_.meterOutR));
    r.text(TextStyle::Title, "OUTPUT", def_.outputMeter.x - 18, base, kTextBright, Align::Right, TextFx::Shadowed);
    r.text(TextStyle::Title, formatValue(dout, b.value(def_.outputParam)), def_.outputTrim.r() - 10, base, ho ? kCyan : kAmber, Align::Right, TextFx::NeonCrisp);
    r.text(TextStyle::Heading, def_.footer, def_.footerBox.cx(), def_.footerBox.cy() + 24, rgb8(0xB4B8C0), Align::Center, TextFx::Shadowed, def_.footerBox.w);
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
