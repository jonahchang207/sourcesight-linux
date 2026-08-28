#pragma once
#include <imgui.h>

// ═══════════════════════════════════════════════════════════════════════════
// SourceSight "Sapphire Glass" design language.
//
// Single source of truth for the menu palette and the low-level drawing
// helpers that give panels their frosted-glass identity. Everything in the
// UI flows from here — no magic numbers in call sites.
//
// Palette notes (dark-theme best practice):
//   - No pure black / pure white anywhere.
//   - Surfaces live on a deep-sapphire ramp (midnight navy → indigo) so the
//     translucent panels have something colourful behind them to refract.
//   - The accent is a royal-sapphire blue; it brightens only on interactive
//     states. A warm gold is reserved as a jewellery-style punctuation mark
//     (currently only the version chip).
// ═══════════════════════════════════════════════════════════════════════════

namespace theme {

// ── Sapphire surface ramp (0..1 normalized) ──────────────────────────────
inline constexpr ImVec4 kSurfaceBase   = ImVec4(0.055f, 0.080f, 0.160f, 0.86f); // inputs, frames
inline constexpr ImVec4 kSurfaceElev1  = ImVec4(0.090f, 0.150f, 0.290f, 0.62f); // panels, sidebar
inline constexpr ImVec4 kSurfaceElev2  = ImVec4(0.110f, 0.190f, 0.360f, 0.55f); // raised glass
inline constexpr ImVec4 kSurfaceDeep   = ImVec4(0.047f, 0.075f, 0.150f, 0.94f); // window base

// ── Borders ──────────────────────────────────────────────────────────────
inline constexpr ImVec4 kBorderBase    = ImVec4(0.450f, 0.600f, 0.920f, 0.20f);
inline constexpr ImVec4 kBorderStrong  = ImVec4(0.620f, 0.760f, 1.000f, 0.38f);

// ── Royal sapphire accent ramp ───────────────────────────────────────────
inline constexpr ImVec4 kAccent        = ImVec4(0.290f, 0.560f, 1.000f, 1.00f);
inline constexpr ImVec4 kAccentBright  = ImVec4(0.520f, 0.720f, 1.000f, 1.00f);
inline constexpr ImVec4 kAccentDim     = ImVec4(0.130f, 0.260f, 0.550f, 0.88f);
inline constexpr ImVec4 kAccentSoft    = ImVec4(0.200f, 0.400f, 0.820f, 0.95f); // primary CTA
inline constexpr ImVec4 kAccentStrong  = ImVec4(0.100f, 0.200f, 0.440f, 1.00f); // pressed CTA
inline constexpr ImVec4 kAccentGlow    = ImVec4(0.290f, 0.560f, 1.000f, 0.16f);

// ── Text ─────────────────────────────────────────────────────────────────
inline constexpr ImVec4 kTextPrimary   = ImVec4(0.930f, 0.960f, 1.000f, 1.00f);
inline constexpr ImVec4 kTextSecondary = ImVec4(0.660f, 0.740f, 0.920f, 1.00f);
inline constexpr ImVec4 kTextMuted     = ImVec4(0.450f, 0.530f, 0.720f, 1.00f);

// ── Semantic signals ─────────────────────────────────────────────────────
inline constexpr ImVec4 kSignalOK      = ImVec4(0.310f, 0.800f, 0.600f, 1.00f);
inline constexpr ImVec4 kSignalWarn    = ImVec4(0.950f, 0.660f, 0.260f, 1.00f);
inline constexpr ImVec4 kSignalErr     = ImVec4(0.960f, 0.420f, 0.440f, 1.00f);

// Warm gold, used sparingly as a gemstone-set accent.
inline constexpr ImVec4 kGold          = ImVec4(0.930f, 0.790f, 0.450f, 1.00f);

// ── Small conversion helpers ─────────────────────────────────────────────

inline ImU32 Pack(const ImVec4& c) {
    return IM_COL32((int)(c.x * 255.0f), (int)(c.y * 255.0f),
                    (int)(c.z * 255.0f), (int)(c.w * 255.0f));
}

inline ImVec4 WithAlpha(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, c.w * a); }

// ── Aurora backdrop ──────────────────────────────────────────────────────
// Deep-sapphire canvas plus a few large, very soft radial glows. This is
// what the translucent glass panels sit in front of — the colour variation
// behind the frost is what makes the glass read as glass.
inline void DrawBackdrop(ImDrawList* d, const ImVec2& p, const ImVec2& s, float r) {
    d->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), Pack(kSurfaceDeep), r);
    d->PushClipRect(p, ImVec2(p.x + s.x, p.y + s.y), true);
    const float w = s.x, h = s.y;
    d->AddCircleFilled(ImVec2(p.x + w * 0.20f, p.y + h * 0.12f), w * 0.60f,
                       Pack(ImVec4(0.16f, 0.30f, 0.66f, 0.12f)));
    d->AddCircleFilled(ImVec2(p.x + w * 0.88f, p.y + h * 0.16f), w * 0.46f,
                       Pack(ImVec4(0.11f, 0.22f, 0.52f, 0.10f)));
    d->AddCircleFilled(ImVec2(p.x + w * 0.70f, p.y + h * 0.95f), w * 0.55f,
                       Pack(ImVec4(0.07f, 0.13f, 0.30f, 0.12f)));
    d->AddCircleFilled(ImVec2(p.x + w * 1.02f, p.y + h * 0.52f), w * 0.34f,
                       Pack(ImVec4(0.28f, 0.20f, 0.44f, 0.07f)));
    d->PopClipRect();
    d->AddRect(p, ImVec2(p.x + s.x, p.y + s.y),
               Pack(WithAlpha(kBorderBase, 0.55f)), r, 0, 1.0f);
}

// ── Frosted glass panel ──────────────────────────────────────────────────
// Layered translucent fill + a bright top-edge catch-light + a soft inner
// shade at the base. `alpha` scales the whole panel so it can be animated
// in/out with collapse transitions without losing the glass treatment.
inline void DrawGlass(ImDrawList* d, const ImVec2& p, const ImVec2& s, float r,
                      const ImVec4& fill, float alpha = 1.0f, bool border = true) {
    const ImVec4 f = WithAlpha(fill, alpha);
    d->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), Pack(f), r);
    // Light catches the top edge — the signature of frosted glass.
    d->AddRectFilled(ImVec2(p.x + 2, p.y), ImVec2(p.x + s.x - 2, p.y + 1.0f + 0.5f * alpha),
                     Pack(WithAlpha(ImVec4(0.72f, 0.84f, 1.00f, 0.22f), alpha)), r * 0.4f);
    // Faint sheen down the leading edge.
    d->AddRectFilled(ImVec2(p.x, p.y + 2), ImVec2(p.x + 1.0f, p.y + s.y - 2),
                     Pack(WithAlpha(ImVec4(0.70f, 0.82f, 1.00f, 0.10f), alpha)), 1.0f);
    // Soft inner shade for depth.
    d->AddRectFilled(ImVec2(p.x + 2, p.y + s.y - 1.0f - 0.5f * alpha),
                     ImVec2(p.x + s.x - 2, p.y + s.y),
                     Pack(WithAlpha(ImVec4(0.00f, 0.02f, 0.07f, 0.14f), alpha)), r * 0.4f);
    if (border)
        d->AddRect(p, ImVec2(p.x + s.x, p.y + s.y),
                   Pack(WithAlpha(kBorderBase, alpha)), r, 0, 1.0f);
}

} // namespace theme