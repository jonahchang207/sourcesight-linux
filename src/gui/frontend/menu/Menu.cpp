#include "Menu.hpp"

#include "config/Config.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/input/MouseAim.hpp"
#include "core/engine/classes/SkinChanger.hpp"
#include "core/engine/classes/SkinDatabase.hpp"
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"
#include "assets/fonts/Icons.h"
#include "assets/fonts/WeaponIcons.h"
#include "Theme.hpp"

#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <filesystem>

// ═══════════════════════════════════════════════════════════════════════════
// Animation Utilities
// ═══════════════════════════════════════════════════════════════════════════

namespace {


float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// Ease-out cubic: fast start, soft landing. Used for all transitions so the
// UI never feels linear/jerky. `t` must be 0..1.
float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - Clamp01(t), 3.0f); }

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t),
                  Lerp(a.z, b.z, t), Lerp(a.w, b.w, t));
}

// Advance an animated scalar toward its target and return the eased value.
// Frame-rate-independent exponential smoothing; safe for any positive speed.
// (The old form computed pow(1 - speed, dt*60) with speed > 1, i.e. a
// negative base, which yielded NaN and silently killed every animation.)
float AnimateScalar(float& current, float target, float speed, float dt) {
    const float t = 1.0f - std::exp(-speed * dt);
    current = Lerp(current, target, t);
    return current;
}

// ───────────────────────────────────────────────────────────────────────────
// The palette and glass/aurora drawing helpers live in Theme.hpp (single
// source of truth for the "Sapphire Glass" design language). This TU opts
// into those tokens so every existing reference keeps working.
// ───────────────────────────────────────────────────────────────────────────
using namespace theme;

// ── Collapsible Section State (animated) ──────────────────────────────────

struct SectionState {
    bool open = true;      // logical state
    float reveal = 1.0f;   // 0..1 animated open/close progress (ease-out)
    float hover = 0.0f;    // header hover glow
};
static std::unordered_map<ImGuiID, SectionState> g_section_states;

bool BeginGlassSection(const char* label, bool body_enabled = true) {
    auto& io = ImGui::GetIO();
    ImGuiID id = ImGui::GetID(label);
    auto& state = g_section_states[id];

    // Pull smoothly toward the target state every frame.
    const float target = state.open ? 1.0f : 0.0f;
    AnimateScalar(state.reveal, target, 5.0f, io.DeltaTime);
    if (state.open && state.reveal > 0.999f)
        state.reveal = 1.0f;
    if (!state.open && state.reveal < 0.001f)
        state.reveal = 0.0f;

    const float ease = EaseOutCubic(state.reveal);
    const bool drawing = ease > 0.001f;

    // ── Header row ───────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    // Frosted strip that draws toward sapphire as it is hovered. `state.hover`
    // is eased every frame, so the tint animates with the cursor.
    ImVec4 hdr_bg = LerpColor(kSurfaceElev1, kAccentDim, state.hover * 0.30f);
    hdr_bg.w = 0.82f;
    ImGui::PushStyleColor(ImGuiCol_Button, hdr_bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LerpColor(kSurfaceElev1, kAccentDim, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentDim);
    ImGui::PushStyleColor(ImGuiCol_Text, state.open ? kTextPrimary : kTextSecondary);

    // Pad the label so the caret (drawn at ~x=11) never overlaps the text.
    if (ImGui::Button((std::string("    ") + label).c_str(), ImVec2(-1, 38))) {
        state.open = !state.open;
        // Reverse direction for the collapse so it feels responsive.
        state.reveal = state.open ? std::max(state.reveal, 0.12f)
                                  : std::min(state.reveal, 0.88f);
    }

    const bool hovered = ImGui::IsItemHovered();
    AnimateScalar(state.hover, hovered ? 1.0f : 0.0f, 8.0f, io.DeltaTime);

    // Draw the animated caret + accent bar over the header.
    if (drawing) {
        const ImVec2 hmin = ImGui::GetItemRectMin();
        const ImVec2 hmax = ImGui::GetItemRectMax();
        const float cx = hmin.x + 11.0f;
        const float cy = (hmin.y + hmax.y) * 0.5f;
        const float r = 3.2f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Chevron rotation: down (open, 0°) ↔ right (closed, 90°), eased.
        const float rot = (state.open ? ease : 1.0f - ease) * (3.14159265f / 2.0f);
        const float cr = std::cos(rot), sr = std::sin(rot);
        // Points of a "›" chevron scaled by rotation.
        ImVec2 p1(-r, -r), p2(0, 0), p3(-r, r);
        auto rotp = [&](ImVec2 p) {
            return ImVec2(cx + p.x * cr - p.y * sr, cy + p.x * sr + p.y * cr);
        };
        ImU32 col = IM_COL32(kAccent.x * 255, kAccent.y * 255, kAccent.z * 255, (int)(210 * ease));
        const ImVec2 chevron_points[] = { rotp(p1), rotp(p2), rotp(p3) };
        dl->AddPolyline(chevron_points, 3, col, 0, 1.8f);

        // Accent active-bar on the header left edge.
        if (state.open) {
            dl->AddRectFilled(ImVec2(hmin.x + 1, hmin.y + 4),
                              ImVec2(hmin.x + 3, hmax.y - 4),
                              IM_COL32(kAccent.x * 255, kAccent.y * 255, kAccent.z * 255,
                                       (int)(150 * ease)));
        }
        (void)sr;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3); // ItemSpacing + FrameRounding + FramePadding

    // ── Body (revealed area) ─────────────────────────────────────────
    if (drawing) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, WithAlpha(kSurfaceElev2, 0.55f * ease));
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(kBorderBase, 0.95f * ease));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ease);
        // Module-level disables (e.g. aim off) must not lock the header;
        // they are re-applied here so only the settings inside go inert.
        if (!body_enabled)
            ImGui::BeginDisabled(true);
        // Let the section body use its natural height. The parent content
        // child owns scrolling, so expanded sections remain reachable instead
        // of being clipped by a viewport-sized nested child.
        ImGui::BeginChild((std::string("##sec_") + label).c_str(),
                          ImVec2(-1, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::PopStyleVar(); // Pop Alpha after begin; keep the rest beneath it
    }
    return drawing;
}

void EndGlassSection(bool is_open, bool body_enabled = true) {
    if (is_open) {
        ImGui::EndChild();
        if (!body_enabled)
            ImGui::EndDisabled();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2); // ChildBorderSize + Child
    }
}

// ── Button Helpers ────────────────────────────────────────────────────────

// Primary action button — sapphire CTA, clearly the main action.
bool SapphireButton(const char* label, const ImVec2& size = ImVec2(-1, 30)) {
    ImGui::PushStyleColor(ImGuiCol_Button, kAccentSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentStrong);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    bool p = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return p;
}

// Destructive / disable action — jewel-red, muted enough to signal danger
// without shouting.
bool DangerButton(const char* label, const ImVec2& size = ImVec2(-1, 26)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.24f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.26f, 0.32f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.12f, 0.16f, 1.00f));
    bool p = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return p;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// Menu Implementation
// ═══════════════════════════════════════════════════════════════════════════

bool Menu::Init() { return GetInstance().InitImpl(); }
void Menu::Render() { return GetInstance().RenderImpl(); }
void Menu::RenderStartupHelp() { return GetInstance().RenderStartupHelpImpl(); }
ImVec2 Menu::GetPos() { return GetInstance().pos; }
ImVec2 Menu::GetSize() { return GetInstance().size; }

bool Menu::InitImpl() {
    SetupStyles();
    LOGF(INFO, "Successfully initialized menu...");
    return true;
}

void Menu::RenderImpl() {
    if (!isSetup) return;

    auto& io = ImGui::GetIO();
    const auto screen = io.DisplaySize;
    static auto color_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;

#ifdef _DEBUG
    static auto title = "SourceSight [DEV]";
#else
    static auto title = "SourceSight";
#endif

    // ── Window ────────────────────────────────────────────────────────
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660, 440), ImVec2(1000, 800));
    ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kSurfaceDeep);

    // Keep the top-level window free of scrollbars, but allow the content
    // child to scroll. Collapsible sections can otherwise extend below the
    // fixed-size window and their headers become unreachable.
    ImGuiWindowFlags wflags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin(title, nullptr, wflags)) {
        this->pos = ImGui::GetWindowPos();
        this->size = ImGui::GetWindowSize();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wpos = ImGui::GetWindowPos();
        ImVec2 wsize = ImGui::GetWindowSize();

        // Sapphire aurora backdrop — deep navy canvas + soft radial glows
        // that the translucent glass panels sit in front of.
        DrawBackdrop(dl, wpos, wsize, 12.0f);

        // ── Title bar ─────────────────────────────────────────────────
        const float title_h = 42.0f;
        // Frosted strip over the aurora, plus a two-tone sapphire underline.
        DrawGlass(dl, wpos, ImVec2(wsize.x, title_h), 12.0f, kSurfaceElev1, 0.55f, false);
        const float sep_y = wpos.y + title_h;
        dl->AddRectFilled(ImVec2(wpos.x, sep_y - 1), ImVec2(wpos.x + wsize.x, sep_y),
                          Pack(WithAlpha(kAccent, 0.30f)));
        dl->AddRectFilled(ImVec2(wpos.x, sep_y), ImVec2(wpos.x + wsize.x, sep_y + 1),
                          Pack(WithAlpha(kBorderBase, 0.60f)));

        // Brand gem (cut-sapphire mark).
        const ImVec2 gem(wpos.x + 14, wpos.y + 11);
        dl->AddRectFilled(gem, ImVec2(gem.x + 20, gem.y + 20), Pack(kAccent), 6.0f);
        dl->AddRectFilled(ImVec2(gem.x + 2, gem.y + 2), ImVec2(gem.x + 8, gem.y + 7),
                          Pack(WithAlpha(ImVec4(1, 1, 1, 0.9f), 0.90f)), 3.0f);
        dl->AddLine(ImVec2(gem.x + 3, gem.y + 16), ImVec2(gem.x + 16, gem.y + 4),
                    Pack(WithAlpha(ImVec4(1, 1, 1, 0.55f), 0.75f)), 1.0f);
        dl->AddCircleFilled(ImVec2(gem.x + 14, gem.y + 14), 2.2f,
                            Pack(WithAlpha(ImVec4(0.9f, 1, 1, 0.9f), 0.85f)));

        ImGui::SetCursorScreenPos(ImVec2(wpos.x + 44, wpos.y + 9));
        ImGui::TextColored(kTextPrimary, "%s", title);
        ImGui::SetCursorScreenPos(ImVec2(wpos.x + 44, wpos.y + 24));
        ImGui::TextColored(kTextMuted, "cheat overlay");

        // Status + version glass chips on the right.
        const char* status_txt = cfg::enabled ? "ACTIVE" : "PAUSED";
        static float status_time = 0.0f;
        status_time += io.DeltaTime;
        const float pulse = 0.5f + 0.5f * std::sin(status_time * 6.2831853f);
        const ImVec4 status_color = cfg::enabled ? kSignalOK : kSignalWarn;
        const ImVec2 status_sz = ImGui::CalcTextSize(status_txt);
        const float chip_h = 22.0f;
        const float right = wpos.x + wsize.x - 14.0f;

        const float vw = ImGui::CalcTextSize("v0.5.0").x + 16.0f;
        const ImVec2 vmin(right - 6.0f - vw, wpos.y + 10);
        DrawGlass(dl, vmin, ImVec2(vw, chip_h), 11.0f, kSurfaceElev2, 0.85f);
        ImGui::SetCursorScreenPos(ImVec2(vmin.x + 8, wpos.y + 14));
        ImGui::TextColored(kGold, "v0.5.0");

        const float sw = status_sz.x + 30.0f;
        const ImVec2 smin(right - 6.0f - vw - 8.0f - sw, wpos.y + 10);
        DrawGlass(dl, smin, ImVec2(sw, chip_h), 11.0f, kSurfaceElev2, 0.85f);
        dl->AddCircleFilled(ImVec2(smin.x + 10, smin.y + chip_h * 0.5f), 3.2f,
            Pack(WithAlpha(status_color, cfg::enabled ? 0.55f + pulse * 0.45f : 0.85f)));
        ImGui::SetCursorScreenPos(ImVec2(smin.x + 19, wpos.y + 14));
        ImGui::TextColored(kTextSecondary, "%s", status_txt);

        ImGui::SetCursorScreenPos(ImVec2(wpos.x, wpos.y + title_h));

        // ── Tab selection (tracked via static, set from sidebar buttons) ──
        static int active_tab = 0;
        static int prev_tab = 0;
        static float tab_alpha = 1.0f;

        // Animate tab transition
        if (active_tab != prev_tab) {
            tab_alpha = std::max(0.0f, tab_alpha - io.DeltaTime * 6.0f);
            if (tab_alpha <= 0.0f) {
                prev_tab = active_tab;
                tab_alpha = 0.0f;
            }
        } else if (tab_alpha < 1.0f) {
            tab_alpha = std::min(1.0f, tab_alpha + io.DeltaTime * 6.0f);
        }

        // ── Main layout ───────────────────────────────────────────────
        const bool main_visible = ImGui::BeginChild("##main_split", ImVec2(0, 0), ImGuiChildFlags_None);
        if (main_visible) {
            auto avail = ImGui::GetContentRegionAvail();

            // ── Sidebar ───────────────────────────────────────────────
            const float sidebar_w = 164.0f;
            ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, avail.y), ImGuiChildFlags_None);
            {
                auto sp = ImGui::GetCursorScreenPos();
                auto ss = ImGui::GetContentRegionAvail();
                DrawGlass(dl, sp, ss, 8.0f, kSurfaceElev1, 0.82f);

                ImGui::SetCursorPos(ImVec2(10, 14));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 5));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11, 8));

                for (int i = 0; i < 7; ++i) {
                    const auto& tab = tabs[i];
                    const bool is_active = (active_tab == tab.id);

                    // Active tab renders as a frosted sapphire pill; the rest
                    // are quiet translucent glass tiles.
                    ImVec4 bg = is_active
                        ? LerpColor(kAccentDim, kAccentSoft, 0.35f)
                        : LerpColor(kSurfaceDeep, kAccentDim, 0.14f);
                    bg.w = is_active ? 0.92f : 0.48f;
                    ImVec4 hover = LerpColor(kSurfaceElev1, kAccentDim, 0.45f);
                    hover.w = 0.88f;
                    ImVec4 text = is_active ? kTextPrimary : kTextSecondary;

                    ImGui::PushStyleColor(ImGuiCol_Button, bg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentDim);
                    ImGui::PushStyleColor(ImGuiCol_Text, text);

                    if (ImGui::Button((tab.icon + "  " + tab.label).c_str(), ImVec2(-1, 34)))
                        active_tab = tab.id;

                    ImGui::PopStyleColor(4);

                    if (is_active) {
                        // Soft sapphire halo hugging the active pill.
                        const ImVec2 amin = ImGui::GetItemRectMin();
                        const ImVec2 amax = ImGui::GetItemRectMax();
                        dl->AddRectFilled(amin - ImVec2(2, 2), amax + ImVec2(2, 2),
                                          Pack(WithAlpha(kAccent, 0.10f)), 10.0f);
                    }
                }

                ImGui::PopStyleVar(3);

                // Sliding accent bar — the animated "you are here" marker.
                static float active_indicator_y = 20.0f;
                const float target_indicator_y = 14.0f + static_cast<float>(active_tab) * 39.0f + 17.0f;
                AnimateScalar(active_indicator_y, target_indicator_y, 8.0f, io.DeltaTime);
                dl->AddRectFilled(ImVec2(sp.x + 8, sp.y + active_indicator_y - 9),
                                  ImVec2(sp.x + 11, sp.y + active_indicator_y + 10),
                                  Pack(kAccentBright), 2.0f);

                // Bottom section
                auto space_left = ImGui::GetContentRegionAvail().y;
                ImGui::SetCursorPosY(ImGui::GetCursorPos().y + std::max(0.0f, space_left - 78.0f));
                ImGui::SetCursorPosX(8);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
                ImGui::TextDisabled("STATUS");
                ImGui::Spacing();
                ImGui::Checkbox("Overlay", &cfg::enabled);
                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            // ── Content area ──────────────────────────────────────────
            // The content area must own scrolling: opening a section increases
            // its height, and the fixed-height parent cannot display all of it.
            ImGui::BeginChild("##content", ImVec2(0, avail.y), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar);
            {
                auto cp = ImGui::GetCursorScreenPos();
                auto cs = ImGui::GetContentRegionAvail();
                DrawGlass(dl, cp, cs, 8.0f, kSurfaceElev2, 1.0f);

                ImGui::SetCursorPos(ImVec2(10, 10));
                ImGui::BeginDisabled(!cfg::enabled);

                // Fade plus a small eased vertical slide for tab transitions.
                const float transition = EaseOutCubic(tab_alpha);
                ImGui::SetCursorPosY(10.0f + (1.0f - transition) * 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, transition);

                if (active_tab == Tab::PLAYER)
                {
                    ImGui::TextColored(kAccent, "Player ESP");
                    ImGui::Separator();

                    if (BeginGlassSection("Visuals")) {
                        ImGui::BeginGroup();
                        {
                            ImGui::Checkbox("Box", &cfg::esp::box);
                            ImGui::BeginDisabled(!cfg::esp::box);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##box", cfg::esp::colors::box_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##box", cfg::esp::colors::box_enemy.data(), color_flags);
                                ImGui::Checkbox("Filled", &cfg::esp::box_filled);
                                if (cfg::esp::box_filled)
                                    ImGui::SliderFloat("Fill alpha", &cfg::esp::box_fill_alpha, 0.02f, 1.0f, "%.2f");
                                ImGui::SliderFloat("Box thickness", &cfg::esp::box_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Skeleton", &cfg::esp::skeleton);
                            ImGui::BeginDisabled(!cfg::esp::skeleton);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##skel", cfg::esp::colors::skeleton_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##skel", cfg::esp::colors::skeleton_enemy.data(), color_flags);
                                ImGui::SliderFloat("Skeleton thickness", &cfg::esp::skeleton_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Head Tracker", &cfg::esp::head_tracker);
                            ImGui::BeginDisabled(!cfg::esp::head_tracker);
                            {
                                ImGui::Checkbox("Filled Head", &cfg::esp::head_tracker_filled);
                                ImGui::SliderFloat("Head size", &cfg::esp::head_tracker_size, 2.0f, 14.0f, "%.1f");
                                ImGui::ColorEdit4("Team##head", cfg::esp::colors::tracker_team.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
                                ImGui::ColorEdit4("Enemy##head", cfg::esp::colors::tracker_enemy.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Tracers", &cfg::esp::tracers);
                            ImGui::BeginDisabled(!cfg::esp::tracers);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##trc", cfg::esp::colors::tracer_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##trc", cfg::esp::colors::tracer_enemy.data(), color_flags);
                                ImGui::SliderFloat("Tracer thickness", &cfg::esp::tracer_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Bullet Tracer", &cfg::esp::bullet_tracer::enabled);
                            ImGui::SetItemTooltip("Line from gun tip to impact point when a shot is fired.");
                            ImGui::BeginDisabled(!cfg::esp::bullet_tracer::enabled);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##bt", cfg::esp::bullet_tracer::team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##bt", cfg::esp::bullet_tracer::enemy.data(), color_flags);
                                ImGui::SliderFloat("Bullet length", &cfg::esp::bullet_tracer::length, 50.0f, 1000.0f, "%.0f u");
                                ImGui::SliderFloat("Muzzle offset", &cfg::esp::bullet_tracer::muzzle_offset, 10.0f, 150.0f, "%.0f u");
                                ImGui::SliderFloat("Bullet duration", &cfg::esp::bullet_tracer::duration, 0.5f, 10.0f, "%.1f s");
                                ImGui::SliderFloat("Bullet thickness", &cfg::esp::bullet_tracer::thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();
                        }
                        ImGui::EndGroup();

                        ImGui::SameLine();

                        ImGui::BeginGroup();
                        {
                            ImGui::Checkbox("Health", &cfg::esp::health);
                            if (cfg::esp::health)
                                ImGui::Checkbox("Health Number", &cfg::esp::health_number);
                            ImGui::Checkbox("Armor", &cfg::esp::armor);
                            ImGui::Checkbox("Game-spotted only", &cfg::esp::spotted);
                            ImGui::SetItemTooltip("Uses CS2's spotted state.");
                            ImGui::Checkbox("Show Team", &cfg::esp::team);
                            ImGui::Checkbox("Spotted only", &cfg::esp::spotted_only);
                            ImGui::SetItemTooltip("Only render enemies you can actually see");
                            ImGui::Checkbox("Distance", &cfg::esp::distance);
                            ImGui::Checkbox("Headshot line", &cfg::esp::headshot_line);
                            ImGui::SetItemTooltip("Line from crosshair to enemy head");
                        }
                        ImGui::EndGroup();
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Flags")) {
                        ImGui::BeginGroup();
                        {
                            ImGui::Checkbox("Flashed", &cfg::esp::flags::flashed);
                            ImGui::BeginDisabled(!cfg::esp::flags::flashed);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##fl", cfg::esp::colors::flags::flashed_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##fl", cfg::esp::colors::flags::flashed_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Reloading", &cfg::esp::flags::reloading);
                            ImGui::BeginDisabled(!cfg::esp::flags::reloading);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##rl", cfg::esp::colors::flags::reloading_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##rl", cfg::esp::colors::flags::reloading_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Defusing", &cfg::esp::flags::defusing);
                            ImGui::BeginDisabled(!cfg::esp::flags::defusing);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##df", cfg::esp::colors::flags::defusing_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##df", cfg::esp::colors::flags::defusing_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Scoped", &cfg::esp::flags::scoped);
                            ImGui::BeginDisabled(!cfg::esp::flags::scoped);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##sc", cfg::esp::colors::flags::scoped_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##sc", cfg::esp::colors::flags::scoped_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            ImGui::Checkbox("Has C4", &cfg::esp::flags::has_c4);
                            ImGui::BeginDisabled(!cfg::esp::flags::has_c4);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##c4", cfg::esp::colors::flags::c4_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##c4", cfg::esp::colors::flags::c4_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();
                        }
                        ImGui::EndGroup();

                        ImGui::SameLine();

                        ImGui::BeginGroup();
                        {
                            ImGui::Checkbox("Name", &cfg::esp::flags::name);
                            ImGui::Checkbox("Money", &cfg::esp::flags::money);
                            ImGui::Checkbox("Weapon", &cfg::esp::flags::weapon);
                            ImGui::Checkbox("Ammo", &cfg::esp::flags::ammo);
                            ImGui::Checkbox("Ping", &cfg::esp::flags::ping);
                        }
                        ImGui::EndGroup();
                        EndGlassSection(true);
                    }
                }
                else if (active_tab == Tab::WORLD)
                {
                    ImGui::TextColored(kAccent, "World");
                    ImGui::Separator();

                    if (BeginGlassSection("Bomb")) {
                        ImGui::Checkbox("Bomb ESP", &cfg::esp::bomb);
                        ImGui::SameLine();
                        ImGui::ColorEdit4("Bomb color", cfg::esp::colors::bomb.data(), color_flags);
                        ImGui::Checkbox("Bomb Location", &cfg::world::bomb::location);
                        ImGui::Checkbox("Bomb Timer", &cfg::world::bomb::timer);
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Spectators")) {
                        ImGui::Checkbox("Enable", &cfg::world::spectators::enabled);
                        if (cfg::world::spectators::enabled) {
                            ImGui::Checkbox("Detailed", &cfg::world::spectators::detailed);
                            ImGui::Checkbox("Only Self", &cfg::world::spectators::self_only);
                            ImGui::SetItemTooltip("Only display users spectating you");
                        }
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Crosshair")) {
                        ImGui::Checkbox("Enable", &cfg::world::crosshair::enabled);
                        ImGui::BeginDisabled(!cfg::world::crosshair::enabled);
                        {
                            ImGui::Checkbox("Snipers only", &cfg::world::crosshair::sniper_only);
                            ImGui::Checkbox("Center dot", &cfg::world::crosshair::center_dot);
                            ImGui::Checkbox("Outline", &cfg::world::crosshair::outline);
                            ImGui::ColorEdit4("Color", cfg::world::crosshair::color.data(), color_flags);
                            ImGui::SliderFloat("Gap", &cfg::world::crosshair::gap, 0.0f, 20.0f, "%.1f");
                            ImGui::SliderFloat("Length", &cfg::world::crosshair::length, 1.0f, 20.0f, "%.1f");
                            ImGui::SliderFloat("Thickness", &cfg::world::crosshair::thickness, 1.0f, 5.0f, "%.1f");
                            if (cfg::world::crosshair::center_dot)
                                ImGui::SliderFloat("Dot size", &cfg::world::crosshair::center_dot_size, 0.5f, 5.0f, "%.1f");
                            if (cfg::world::crosshair::outline)
                                ImGui::SliderFloat("Outline size", &cfg::world::crosshair::outline_thickness, 0.5f, 3.0f, "%.1f");
                        }
                        ImGui::EndDisabled();
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Velocity")) {
                        ImGui::Checkbox("Enable", &cfg::world::velocity::enabled);
                        ImGui::BeginDisabled(!cfg::world::velocity::enabled);
                        {
                            ImGui::SliderInt("Sample rate", &cfg::world::velocity::sample_rate, 1, 100);
                            ImGui::SliderFloat("Sample length", &cfg::world::velocity::sample_length, 1.0f, 20.0f, "%.1f");
                        }
                        ImGui::EndDisabled();
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Radar")) {
                        ImGui::Checkbox("Enable", &cfg::world::radar::enabled);
                        ImGui::BeginDisabled(!cfg::world::radar::enabled);
                        {
                            ImGui::SameLine();
                            ImGui::SliderFloat("Range", &cfg::world::radar::range, 100.f, 8000.f, "%.0f u");
                            ImGui::Checkbox("Disable Rotation", &cfg::world::radar::no_rotate);
                        }
                        ImGui::EndDisabled();
                        EndGlassSection(true);
                    }
                }
                else if (active_tab == Tab::AIM)
                {
                    ImGui::TextColored(kAccent, "Aim");
                    ImGui::Separator();

                    // Driver status
                    if (MouseAim::DriverInstalled())
                        ImGui::TextColored(kSignalOK, "Driver: ready");
                    else
                        ImGui::TextColored(kSignalErr, "Driver: MISSING");

                    ImGui::Spacing();

                    // Enable / Disable toggle
                    if (!cfg::aim::enabled) {
                        if (SapphireButton("Enable Aim"))
                            cfg::aim::enabled = true;
                    } else {
                        if (DangerButton("Disable Aim", ImVec2(-1, 30)))
                            cfg::aim::enabled = false;
                    }

                    ImGui::Spacing();
                    ImGui::Checkbox("MB5 hotkey toggle", &cfg::aim::hotkey);

                    ImGui::Spacing();
                    {
                        ImGui::Checkbox("Game mode (CS2)", &cfg::aim::game_mode);
                        ImGui::Checkbox("Aim at enemies", &cfg::aim::aim_at_enemies);
                        ImGui::Checkbox("Visible only", &cfg::aim::visible_only);
                        ImGui::SetItemTooltip("Only lock onto targets you can see");
                        ImGui::Checkbox("Auto-start aim", &cfg::aim::auto_start);

                        ImGui::Spacing();

                        if (BeginGlassSection("Target##aim", cfg::aim::enabled)) {
                            static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
                            int tp = cfg::aim::target_part;
                            if (ImGui::Combo("Target", &tp, target_parts, 4))
                                cfg::aim::target_part = tp;
                            EndGlassSection(true, cfg::aim::enabled);
                        }

                        if (BeginGlassSection("Weapon Speed", cfg::aim::enabled)) {
                            ImGui::SliderFloat("Rifle", &cfg::aim::rifle_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Pistol", &cfg::aim::pistol_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Sniper", &cfg::aim::sniper_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("SMG", &cfg::aim::smg_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::Spacing();
                            ImGui::SliderFloat("Recoil comp", &cfg::aim::recoil_compensation, 0.0f, 1.0f, "%.0f%%");
                            ImGui::SliderFloat("Switch delay", &cfg::aim::target_switch_delay, 0.0f, 0.5f, "%.2fs");
                            EndGlassSection(true, cfg::aim::enabled);
                        }

                        if (BeginGlassSection("FOV", cfg::aim::enabled)) {
                            float aim_w = 1920.0f, aim_h = 1080.0f;
                            MouseAim::ScreenSize(aim_w, aim_h);
                            const float fsr = std::sqrt(aim_w * aim_w + aim_h * aim_h) * 0.5f;
                            ImGui::SliderFloat("FOV radius", &cfg::aim::fov_radius, 0.0f, fsr, "%.0f px");
                            ImGui::SliderFloat("Exit FOV multiplier", &cfg::aim::exit_fov_mult, 1.0f, 2.5f, "x%.2f");
                            ImGui::SetItemTooltip("Targets are released only beyond FOV x this, so a moving target just past the ring edge keeps its lock (hysteresis).");
                            EndGlassSection(true, cfg::aim::enabled);
                        }

                        if (BeginGlassSection("Movement", cfg::aim::enabled)) {
                            ImGui::SliderFloat("Smoothness", &cfg::aim::smoothness, 0.02f, 1.0f, "%.2f");
                            ImGui::SetItemTooltip("Proportional gain; lower = softer, less twitchy finish.");
                            ImGui::SliderFloat("Lead time", &cfg::aim::lead_time, 0.0f, 0.5f, "%.3fs");
                            ImGui::SetItemTooltip("Extrapolate the aim ahead of a moving target by this many seconds of its screen velocity.");
                            EndGlassSection(true, cfg::aim::enabled);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::TextWrapped("F9: panic key (disables all). MB5 or F10: toggle aim.");
                }
                else if (active_tab == Tab::TRIGGERBOT)
                {
                    ImGui::TextColored(kAccent, "Triggerbot");
                    ImGui::Separator();

                    // Enable / Disable toggle
                    if (!cfg::triggerbot::enabled) {
                        if (SapphireButton("Enable Triggerbot"))
                            cfg::triggerbot::enabled = true;
                    } else {
                        if (DangerButton("Disable Triggerbot", ImVec2(-1, 30)))
                            cfg::triggerbot::enabled = false;
                    }

                    ImGui::Spacing();
                    ImGui::Checkbox("Hold key to fire", &cfg::triggerbot::hotkey);
                    ImGui::SetItemTooltip("Hold Left Alt to activate triggerbot");

                    ImGui::Spacing();
                    {
                        ImGui::Checkbox("Visible only", &cfg::triggerbot::visible_only);
                        ImGui::SetItemTooltip("Only fire at enemies you can see");

                        ImGui::Spacing();

                        if (BeginGlassSection("Target##trigger", cfg::triggerbot::enabled)) {
                            static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
                            int tp = cfg::aim::target_part;
                            if (ImGui::Combo("Target", &tp, target_parts, 4))
                                cfg::aim::target_part = tp;
                            EndGlassSection(true, cfg::triggerbot::enabled);
                        }

                        if (BeginGlassSection("Fire Settings", cfg::triggerbot::enabled)) {
                            ImGui::SliderInt("Fire delay (ms)", &cfg::triggerbot::delay_ms, 0, 200);
                            ImGui::SetItemTooltip("Delay before firing (0 = instant)");
                            ImGui::SliderInt("Burst count", &cfg::triggerbot::burst_count, 1, 10);
                            ImGui::SetItemTooltip("Shots per trigger activation");
                            ImGui::SliderInt("Burst delay (ms)", &cfg::triggerbot::burst_delay_ms, 20, 300);
                            ImGui::SetItemTooltip("Delay between burst shots");
                            EndGlassSection(true, cfg::triggerbot::enabled);
                        }

                        if (BeginGlassSection("Weapon Filter", cfg::triggerbot::enabled)) {
                            ImGui::Checkbox("Pistols only", &cfg::triggerbot::pistols_only);
                            ImGui::Checkbox("Rifles only", &cfg::triggerbot::rifles_only);
                            if (cfg::triggerbot::pistols_only && cfg::triggerbot::rifles_only) {
                                cfg::triggerbot::rifles_only = false;
                            }
                            EndGlassSection(true, cfg::triggerbot::enabled);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::TextWrapped("Hold Left Alt to fire when crosshair is on an enemy.");
                }
                else if (active_tab == Tab::SKINS)
                {
                    // ── Skin Changer: Grid Browser ─────────────────────
                    ImGui::TextColored(kAccent, "Skin Changer");
                    ImGui::Separator();

                    static int selected_weapon = WEAPON_AK47;
                    static int selected_skin = 0;
                    static float selected_wear = 0.0f;
                    static int selected_seed = 0;
                    static int selected_stattrak = -1;
                    static bool stattrak_on = false;
                    static int skin_category = 0;

                    // Category tabs
                    static const char* categories[] = { "All", "Rifles", "SMGs", "Shotguns", "Snipers", "Pistols", "Knives" };
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
                    for (int c = 0; c < 7; ++c) {
                        if (c > 0) ImGui::SameLine(0, 3);
                        bool active = (skin_category == c);
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(kAccentDim, 0.92f));
                        if (ImGui::Button(categories[c])) skin_category = c;
                        if (active) ImGui::PopStyleColor();
                    }
                    ImGui::PopStyleVar(2);
                    ImGui::Spacing();

                    // Weapon grid + config panel
                    const float region_w = ImGui::GetContentRegionAvail().x;
                    const float grid_w = region_w * 0.56f;
                    const float config_w = region_w - grid_w - 8.0f;

                    ImGui::BeginChild("##skin_grid", ImVec2(grid_w, 0), ImGuiChildFlags_Borders);
                    {
                        struct WeaponCard { int id; const char* name; const char* icon; };
                        static const WeaponCard all_weapons[] = {
                            { WEAPON_AK47,    "AK-47",       WeaponIcons::AK47 },
                            { WEAPON_AWP,     "AWP",         WeaponIcons::AWP },
                            { WEAPON_M4A4,    "M4A4",        WeaponIcons::M4A1 },
                            { WEAPON_M4A1S,   "M4A1-S",      WeaponIcons::M4A1_SILENCER },
                            { WEAPON_SSG08,   "SSG 08",      WeaponIcons::SSG08 },
                            { WEAPON_AUG,     "AUG",         WeaponIcons::AUG },
                            { WEAPON_SG553,   "SG 553",      WeaponIcons::SG556 },
                            { WEAPON_FAMAS,   "FAMAS",       WeaponIcons::FAMAS },
                            { WEAPON_GALIL,   "Galil AR",    WeaponIcons::GALILAR },
                            { WEAPON_G3SG1,   "G3SG1",       WeaponIcons::G3SG1 },
                            { WEAPON_SCAR20,  "SCAR-20",     WeaponIcons::SCAR20 },
                            { WEAPON_MAC10,   "MAC-10",      WeaponIcons::MAC10 },
                            { WEAPON_UMP45,   "UMP-45",      WeaponIcons::UMP45 },
                            { WEAPON_MP7,     "MP7",         WeaponIcons::MP7 },
                            { WEAPON_MP9,     "MP9",         WeaponIcons::MP9 },
                            { WEAPON_P90,     "P90",         WeaponIcons::P90 },
                            { WEAPON_MP5SD,   "MP5-SD",      WeaponIcons::MP7 },
                            { WEAPON_PPBIZON, "PP-Bizon",    WeaponIcons::BIZON },
                            { WEAPON_XM1014,  "XM1014",      WeaponIcons::XM1014 },
                            { WEAPON_NOVA,    "Nova",        WeaponIcons::NOVA },
                            { WEAPON_MAG7,    "MAG-7",       WeaponIcons::MAG7 },
                            { WEAPON_SAWEDOFF,"Sawed-Off",   WeaponIcons::SAWEDOFF },
                            { WEAPON_NEGEV,   "Negev",       WeaponIcons::NEGEV },
                            { WEAPON_M249,    "M249",        WeaponIcons::M249 },
                            { WEAPON_DEAGLE,  "Desert Eagle", WeaponIcons::DEAGLE },
                            { WEAPON_USP,     "USP-S",       WeaponIcons::USP_SILENCER },
                            { WEAPON_GLOCK,   "Glock-18",    WeaponIcons::GLOCK },
                            { WEAPON_P250,    "P250",        WeaponIcons::P250 },
                            { WEAPON_TEC9,    "Tec-9",       WeaponIcons::TEC9 },
                            { WEAPON_FIVESEVEN,"Five-SeveN", WeaponIcons::FIVESEVEN },
                            { WEAPON_ELITE,   "Dual Berettas",WeaponIcons::ELITE },
                            { WEAPON_CZ75,    "CZ75-Auto",   WeaponIcons::CZ75A },
                            { WEAPON_REVOLVER,"R8 Revolver", WeaponIcons::REVOLVER },
                            { WEAPON_P2000,   "P2000",       WeaponIcons::HKP2000 },
                            { WEAPON_KNIFE_BAYONET,  "Bayonet",      WeaponIcons::KNIFE_BAYONET },
                            { WEAPON_KNIFE_FLIP,     "Flip Knife",   WeaponIcons::KNIFE_FLIP },
                            { WEAPON_KNIFE_GUT,      "Gut Knife",    WeaponIcons::KNIFE_GUT },
                            { WEAPON_KNIFE_KARAMBIT, "Karambit",     WeaponIcons::KNIFE_KARAMBIT },
                            { WEAPON_KNIFE_M9,       "M9 Bayonet",   WeaponIcons::KNIFE_M9_BAYONET },
                            { WEAPON_KNIFE_BUTTERFLY,"Butterfly",    WeaponIcons::KNIFE_BUTTERFLY },
                            { WEAPON_KNIFE_SKELETON, "Skeleton",     WeaponIcons::KNIFE_SKELETON },
                            { WEAPON_KNIFE_KUKRI,    "Kukri",        WeaponIcons::KNIFE_KUKRI },
                        };

                        auto fits = [&](int id) -> bool {
                            if (skin_category == 0) return true;
                            if (skin_category == 6) return id >= 500;
                            if (skin_category == 1) return id==WEAPON_AK47||id==WEAPON_M4A4||id==WEAPON_M4A1S||id==WEAPON_AUG||id==WEAPON_SG553||id==WEAPON_FAMAS||id==WEAPON_GALIL;
                            if (skin_category == 2) return id==WEAPON_MAC10||id==WEAPON_UMP45||id==WEAPON_MP7||id==WEAPON_MP9||id==WEAPON_P90||id==WEAPON_MP5SD||id==WEAPON_PPBIZON;
                            if (skin_category == 3) return id==WEAPON_XM1014||id==WEAPON_NOVA||id==WEAPON_MAG7||id==WEAPON_SAWEDOFF||id==WEAPON_NEGEV||id==WEAPON_M249;
                            if (skin_category == 4) return id==WEAPON_AWP||id==WEAPON_SSG08||id==WEAPON_G3SG1||id==WEAPON_SCAR20;
                            if (skin_category == 5) return id==WEAPON_DEAGLE||id==WEAPON_USP||id==WEAPON_GLOCK||id==WEAPON_P250||id==WEAPON_TEC9||id==WEAPON_FIVESEVEN||id==WEAPON_ELITE||id==WEAPON_CZ75||id==WEAPON_REVOLVER||id==WEAPON_P2000;
                            return true;
                        };

                        const float card_w = 98.0f;
                        const float card_h = 50.0f;
                        const float spacing = 5.0f;
                        const int cols = std::max(1, static_cast<int>((grid_w + spacing) / (card_w + spacing)));
                        int col = 0;

                        for (const auto& w : all_weapons) {
                            if (!fits(w.id)) continue;
                            if (col > 0) ImGui::SameLine(0, spacing);

                            auto skin_it = SkinChanger::GetAll().find(w.id);
                            bool has_skin = (skin_it != SkinChanger::GetAll().end() && skin_it->second.paint_kit > 0);

                            bool is_sel = (selected_weapon == w.id);
                            ImVec4 bg = is_sel ? WithAlpha(kAccentDim, 0.95f)
                                : has_skin ? WithAlpha(kSignalOK, 0.16f)
                                : WithAlpha(kSurfaceElev1, 0.80f);

                            ImGui::PushStyleColor(ImGuiCol_Button, bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LerpColor(bg, kAccentDim, 0.40f));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                            if (ImGui::Button(w.name, ImVec2(card_w, card_h))) {
                                selected_weapon = w.id;
                                selected_skin = 0;
                            }

                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor(2);

                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s%s", w.name, has_skin ? " (active)" : "");

                            col = (col + 1) % cols;
                        }
                    }
                    ImGui::EndChild();

                    ImGui::SameLine(0, 8);

                    // ── Skin config panel ──────────────────────────────
                    ImGui::BeginChild("##skin_config", ImVec2(config_w, 0), ImGuiChildFlags_Borders);
                    {
                        const auto& winfo = SkinDB::Weapons().count(selected_weapon)
                            ? SkinDB::Weapons().at(selected_weapon)
                            : SkinDB::WeaponInfo{ "Unknown", 0 };
                        ImGui::TextColored(kAccent, "%s", winfo.display_name);
                        ImGui::Separator();
                        ImGui::Spacing();

                        const auto& all_skins = SkinDB::SkinsByWeapon();
                        const auto weapon_it = all_skins.find(selected_weapon);
                        const auto& skins = (weapon_it != all_skins.end()) ? weapon_it->second : SkinDB::PopularSkins();
                        if (selected_skin >= static_cast<int>(skins.size()))
                            selected_skin = 0;

                        if (ImGui::BeginCombo("Skin", skins[selected_skin].name)) {
                            for (int i = 0; i < static_cast<int>(skins.size()); ++i) {
                                bool is_sel = (selected_skin == i);
                                if (ImGui::Selectable(skins[i].name, is_sel))
                                    selected_skin = i;
                                if (is_sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::Spacing();
                        ImGui::Text("Wear");
                        ImGui::SliderFloat("##wear", &selected_wear, 0.0f, 1.0f, "%.3f");
                        ImGui::SameLine();
                        ImGui::TextColored(kAccentBright, "%s", SkinDB::WearName(selected_wear));

                        ImGui::Text("Seed");
                        ImGui::SliderInt("##seed", &selected_seed, 0, 1000);

                        ImGui::Checkbox("StatTrak", &stattrak_on);
                        if (stattrak_on) {
                            if (selected_stattrak < 0) selected_stattrak = 0;
                            ImGui::SliderInt("Kills", &selected_stattrak, 0, 999999);
                        } else {
                            selected_stattrak = -1;
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        if (SapphireButton("Apply Skin")) {
                            auto& skin = SkinChanger::Get(selected_weapon);
                            skin.paint_kit = skins[selected_skin].paint_kit;
                            skin.wear = selected_wear;
                            skin.seed = selected_seed;
                            skin.stattrak = selected_stattrak;
                            SkinChanger::ForceUpdate();
                        }

                        if (DangerButton("Reset to Default")) {
                            SkinChanger::Get(selected_weapon) = SkinOverride{};
                        }

                        ImGui::Spacing();

                        const auto& active = SkinChanger::GetAll();
                        if (!active.empty()) {
                            ImGui::TextColored(kSignalOK, "Active (%d)", (int)active.size());
                            ImGui::Separator();
                            ImGui::BeginChild("##active_skins", ImVec2(0, 80));
                            for (const auto& [id, skin] : active) {
                                const auto& wn = SkinDB::Weapons().count(id)
                                    ? SkinDB::Weapons().at(id).display_name : "?";
                                ImGui::TextColored(kSignalOK, "%s", wn);
                                ImGui::SameLine();
                                ImGui::TextDisabled("kit %d  wear %.2f", skin.paint_kit, skin.wear);
                            }
                            ImGui::EndChild();
                        }
                    }
                    ImGui::EndChild();
                }
                else if (active_tab == Tab::MACRO)
                {
                    ImGui::TextColored(kAccent, "Macro");
                    ImGui::Separator();

                    if (BeginGlassSection("AWP Quickswitch")) {
                        ImGui::Checkbox("Enable", &cfg::macro::awp_quickswitch);
                        ImGui::SetItemTooltip("Auto-switch to knife and back to cancel bolt animation.");

                        ImGui::BeginDisabled(!cfg::macro::awp_quickswitch);
                        {
                            ImGui::SliderInt("Switch delay (ms)", &cfg::macro::delay_ms, 20, 500);
                            ImGui::Checkbox("All bolt-action weapons", &cfg::macro::auto_switch_all);
                            ImGui::SetItemTooltip("Apply to Scout, G3SG1, SCAR-20 too");
                        }
                        ImGui::EndDisabled();

                        ImGui::Spacing();
                        ImGui::TextWrapped(
                            "Runs while alive and in-game, never while menu is open.\n"
                            "Keys are injected at the OS level."
                        );
                        EndGlassSection(true);
                    }
                }
                else if (active_tab == Tab::SETTINGS)
                {
                    ImGui::TextColored(kAccent, "Settings");
                    ImGui::Separator();

                    if (BeginGlassSection("Profiles")) {
                        static std::vector<std::string> profiles;
                        static int sel = -1;
                        static char new_name[48]{};
                        static std::string pending_delete;
                        static bool profiles_ready = false;

                        auto refresh_profiles = [&]() {
                            profiles = Config::ListProfiles();
                            const std::string active = Config::GetActiveProfile();
                            sel = 0;
                            for (size_t i = 0; i < profiles.size(); ++i)
                                if (profiles[i] == active) { sel = (int)i; break; }
                            if (profiles.empty()) sel = -1;
                        };

                        if (!profiles_ready) {
                            profiles_ready = true;
                            refresh_profiles();
                        }

                        ImGui::TextColored(kTextSecondary, "Active profile: %s",
                                           Config::GetActiveProfile().c_str());
                        ImGui::SetNextItemWidth(-1);
                        const char* preview = (sel >= 0 && sel < (int)profiles.size())
                                                  ? profiles[sel].c_str()
                                                  : "none";
                        if (ImGui::BeginCombo("##profiles", preview)) {
                            for (int i = 0; i < (int)profiles.size(); ++i) {
                                if (ImGui::Selectable(profiles[i].c_str(), i == sel))
                                    sel = i;
                            }
                            ImGui::EndCombo();
                        }

                        if (SapphireButton("Load Profile", ImVec2(-1, 30))) {
                            if (sel >= 0 && Config::LoadProfile(profiles[sel]))
                                refresh_profiles();
                        }
                        if (SapphireButton("Save to this Profile", ImVec2(-1, 30))) {
                            if (sel >= 0 && Config::SaveProfile(profiles[sel]))
                                refresh_profiles();
                        }
                        ImGui::SetItemTooltip(
                            "Overwrite the selected profile with the current settings.");

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputTextWithHint("##new_profile", "New profile name",
                                                 new_name, sizeof(new_name));
                        if (SapphireButton("Create New Profile from Current Settings",
                                           ImVec2(-1, 30))) {
                            const std::string name(new_name);
                            if (!name.empty() && Config::SaveProfile(name)) {
                                new_name[0] = '\0';
                                refresh_profiles();
                            }
                        }

                        if (sel >= 0) {
                            ImGui::Spacing();
                            if (DangerButton("Delete Selected Profile", ImVec2(-1, 26))) {
                                pending_delete = profiles[sel];
                                ImGui::OpenPopup("Delete Profile?");
                            }
                        }

                        if (ImGui::BeginPopupModal("Delete Profile?", nullptr,
                                                   ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::TextWrapped("Delete profile '%s'?\nThis cannot be undone.",
                                               pending_delete.c_str());
                            ImGui::Spacing();
                            if (DangerButton("Delete", ImVec2(110, 26))) {
                                if (Config::DeleteProfile(pending_delete))
                                    refresh_profiles();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine();
                            if (SapphireButton("Cancel", ImVec2(110, 26)))
                                ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }

                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Display")) {
                        if (ImGui::Checkbox("Streamproof", &cfg::settings::streamproof))
                        {
                            Window::SetAffinity(
                                Window::hwnd,
                                cfg::settings::streamproof ? WindowAffinity::Invisible : WindowAffinity::Disabled
                            );
                        }
                        ImGui::Checkbox("Watermark", &cfg::settings::watermark);
                        if (ImGui::Checkbox("VSync", &cfg::settings::vsync))
                            Window::vsync = cfg::settings::vsync;
                        ImGui::Checkbox("Free CPU", &cfg::settings::free_cpu);
                        ImGui::SetItemTooltip("Let the CPU sleep to free resources.");
                        ImGui::Checkbox("Panic key (F9)", &cfg::settings::panic_key);
                        ImGui::SetItemTooltip("Press F9 to instantly disable all cheats.");
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Bypass")) {
                        ImGui::Checkbox("Timing jitter", &cfg::bypass::timing_jitter);
                        ImGui::SetItemTooltip("Randomise write timing to avoid detection");
                        ImGui::Checkbox("Humanize movement", &cfg::bypass::humanize_movement);
                        ImGui::SetItemTooltip("Add micro-noise to mouse deltas");
                        ImGui::SliderInt("Write delay min (us)", &cfg::bypass::write_delay_min_us, 0, 500);
                        ImGui::SliderInt("Write delay max (us)", &cfg::bypass::write_delay_max_us, 0, 1000);
                        ImGui::SliderFloat("Noise amplitude", &cfg::bypass::noise_amplitude, 0.0f, 2.0f, "%.1f px");
                        EndGlassSection(true);
                    }

                    if (BeginGlassSection("Notes")) {
                        ImGui::TextWrapped(
                            "If you experience bad performance/lag:\n"
                            "  - Disable ESP VSync\n"
                            "  - Disable VSync in game: Advanced Video > V-Sync: Disabled\n"
                            "  - Last resort: Disable Free CPU option"
                        );
                        EndGlassSection(true);
                    }

#ifdef _DEBUG
                    if (BeginGlassSection("Dev")) {
                        if (ImGui::Checkbox("Console", &cfg::dev::console))
                            if (!cfg::dev::console) LogHelper::Free();

                        static int key_out;
                        if (ImGui::Button("Open Menu Key"))
                        {
                            for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++)
                            {
                                if (ImGui::IsKeyPressed((ImGuiKey)i))
                                {
                                    key_out = i;
                                    LOGF(VERBOSE, "Changed the open menu key to {}", key_out);
                                    break;
                                }
                            }
                        }

                        ImGui::SliderInt("Cache Refresh Rate", &cfg::dev::cache_refresh_rate, 0, 100, "%dms");
                        ImGui::Checkbox("Force Show Flags", &cfg::dev::force_show_flags);
                        EndGlassSection(true);
                    }
#endif
                }

                ImGui::PopStyleVar(); // Alpha
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Menu::SetupStyles() {
    ImGuiStyle& style = ImGui::GetStyle();

    // ── Sapphire Glass theme (tokens from Theme.hpp) ───────────────────
    style.Colors[ImGuiCol_Text] = kTextPrimary;
    style.Colors[ImGuiCol_TextDisabled] = kTextMuted;

    style.Colors[ImGuiCol_WindowBg] = kSurfaceDeep;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = kSurfaceElev2;

    style.Colors[ImGuiCol_Border] = kBorderBase;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    style.Colors[ImGuiCol_FrameBg] = kSurfaceBase;
    style.Colors[ImGuiCol_FrameBgHovered] = kSurfaceElev1;
    style.Colors[ImGuiCol_FrameBgActive] = kSurfaceElev2;

    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.04f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.10f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.03f, 0.04f, 0.08f, 0.60f);

    style.Colors[ImGuiCol_MenuBarBg] = kSurfaceElev1;
    style.Colors[ImGuiCol_ScrollbarBg] = WithAlpha(kSurfaceDeep, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab] = kAccentDim;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = LerpColor(kAccentDim, kAccent, 0.55f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = kAccent;

    style.Colors[ImGuiCol_Button] = kSurfaceElev1;
    style.Colors[ImGuiCol_ButtonHovered] = kSurfaceElev2;
    style.Colors[ImGuiCol_ButtonActive] = kAccentDim;

    style.Colors[ImGuiCol_Header] = kSurfaceElev1;
    style.Colors[ImGuiCol_HeaderHovered] = kSurfaceElev2;
    style.Colors[ImGuiCol_HeaderActive] = kAccentDim;

    style.Colors[ImGuiCol_CheckMark] = kAccent;
    style.Colors[ImGuiCol_SliderGrab] = kAccent;
    style.Colors[ImGuiCol_SliderGrabActive] = kAccentBright;

    style.Colors[ImGuiCol_Separator] = kBorderBase;
    style.Colors[ImGuiCol_SeparatorHovered] = kAccentGlow;
    style.Colors[ImGuiCol_SeparatorActive] = kAccent;

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = WithAlpha(kAccent, 0.35f);
    style.Colors[ImGuiCol_ResizeGripActive] = WithAlpha(kAccentBright, 0.70f);

    style.Colors[ImGuiCol_Tab] = WithAlpha(kSurfaceDeep, 0.85f);
    style.Colors[ImGuiCol_TabHovered] = kSurfaceElev2;
    style.Colors[ImGuiCol_TabActive] = kAccentDim;
    style.Colors[ImGuiCol_TabUnfocused] = WithAlpha(kSurfaceDeep, 0.75f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = WithAlpha(kAccentDim, 0.80f);

    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.30f, 0.40f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.40f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = kAccent;
    style.Colors[ImGuiCol_PlotHistogramHovered] = kAccentBright;

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.36f, 0.58f, 0.40f);
    style.Colors[ImGuiCol_DragDropTarget] = kAccent;
    style.Colors[ImGuiCol_NavHighlight] = kAccent;
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.76f, 0.88f, 0.45f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.04f, 0.06f, 0.10f, 0.55f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.04f, 0.06f, 0.10f, 0.65f);

    style.FrameBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(11, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 5);
    style.ScrollbarSize = 8.0f;

    style.WindowRounding = 14.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 4.0f;

    auto& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Prefer JetBrainsMono Nerd Font when installed. Keep platform fallbacks
    // so the UI remains usable on machines without the optional font package.
    const char* font_paths[] = {
#ifdef _WIN32
        "C:\\Windows\\Fonts\\JetBrainsMonoNerdFont-Regular.ttf",
        "C:\\Windows\\Fonts\\JetBrainsMonoNerdFontMono-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
#else
        "/usr/share/fonts/truetype/jetbrains-mono-nl/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
    };

    ImFont* ui_font = nullptr;
    for (const char* path : font_paths) {
        if (std::filesystem::exists(path)) {
            ui_font = io.Fonts->AddFontFromFileTTF(path, 15.0f);
            if (ui_font)
                break;
        }
    }
    if (!ui_font)
        ui_font = io.Fonts->AddFontDefault();

    ImFontConfig merge_icon_cfg{};
    merge_icon_cfg.FontDataOwnedByAtlas = false;
    merge_icon_cfg.MergeMode = true;
    merge_icon_cfg.GlyphOffset = Vec2_t(0, 3.5f);

    static const ImWchar icon_ranges[] = { 0xE100, 0xE108, 0 };
    io.Fonts->AddFontFromMemoryTTF(icons_font, icons_font_len, 18.f, &merge_icon_cfg, icon_ranges);
}

void Menu::RenderStartupHelpImpl() {
    static bool has_opened_menu = false;
    if (has_opened_menu) return;

    auto& io = ImGui::GetIO();
    auto screen = io.DisplaySize;
    auto d = ImGui::GetBackgroundDrawList();

    if (Renderer::IsOpen())
        has_opened_menu = true;

    auto help = "To OPEN the menu: Insert key"
        "\n\t\t\t\tWhile the menu is open, all clicks go to the menu/overlay, none to the game"
        "\n\t\t\t\tMid-round CS2 grabs the mouse, so clicks may also fire your weapon"
        "\n\t\t\t\tKeyboard navigation also works: Arrow/Tab to move, Space/Enter to toggle"
        "\n\t\t\t\tEnd: save config and exit";
    auto size = ImGui::CalcTextSize(help);

    ImVec2 help_pos(screen.x / 2 - size.x / 2 - 20, 70);
    ImVec2 help_size(size.x + 40, size.y + 20);
    DrawGlass(d, help_pos, help_size, 8.0f, kSurfaceDeep, 0.95f);

    d->AddText(
        ImVec2(screen.x / 2 - size.x / 2, 80),
        IM_COL32(220, 225, 235, 255),
        help
    );
}
