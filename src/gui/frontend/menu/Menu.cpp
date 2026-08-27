#include "Menu.hpp"

#include "core/engine/cache/Cache.hpp"
#include "core/input/MouseAim.hpp"
#include "core/engine/classes/SkinChanger.hpp"
#include "core/engine/classes/SkinDatabase.hpp"
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"
#include "assets/fonts/Icons.h"
#include "assets/fonts/WeaponIcons.h"

#include <cmath>
#include <algorithm>
#include <unordered_map>

// ═══════════════════════════════════════════════════════════════════════════
// Animation Utilities
// ═══════════════════════════════════════════════════════════════════════════

namespace {

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t),
                  Lerp(a.z, b.z, t), Lerp(a.w, b.w, t));
}

// ── Sapphire Glassmorphism Palette ────────────────────────────────────────
constexpr ImVec4 kGlassBg         = ImVec4(0.04f, 0.05f, 0.08f, 0.88f);
constexpr ImVec4 kGlassBgLight    = ImVec4(0.06f, 0.07f, 0.11f, 0.82f);
constexpr ImVec4 kGlassBgDeep     = ImVec4(0.03f, 0.04f, 0.06f, 0.92f);
constexpr ImVec4 kGlassBorder     = ImVec4(0.12f, 0.18f, 0.30f, 0.50f);
constexpr ImVec4 kGlassGlow       = ImVec4(0.20f, 0.40f, 0.70f, 0.15f);
constexpr ImVec4 kSapphire        = ImVec4(0.26f, 0.56f, 0.92f, 1.00f);
constexpr ImVec4 kSapphireBright  = ImVec4(0.32f, 0.62f, 0.96f, 1.00f);
constexpr ImVec4 kSapphireMuted   = ImVec4(0.10f, 0.18f, 0.30f, 0.90f);
constexpr ImVec4 kTextPrimary     = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
constexpr ImVec4 kTextSecondary   = ImVec4(0.56f, 0.62f, 0.72f, 1.00f);
constexpr ImVec4 kTextMuted       = ImVec4(0.36f, 0.40f, 0.48f, 1.00f);
constexpr ImVec4 kAccentGreen     = ImVec4(0.30f, 0.78f, 0.56f, 1.00f);
constexpr ImVec4 kAccentRed       = ImVec4(0.92f, 0.34f, 0.34f, 1.00f);

// ── Glassmorphism Drawing Helpers ─────────────────────────────────────────

void DrawGlassCard(ImDrawList* dl, const ImVec2& pos, const ImVec2& size,
                   const ImVec4& tint = kGlassBg, float rounding = 10.0f) {
    dl->AddRectFilled(
        ImVec2(pos.x - 1, pos.y - 1), ImVec2(pos.x + size.x + 1, pos.y + size.y + 1),
        IM_COL32(kGlassGlow.x * 255, kGlassGlow.y * 255, kGlassGlow.z * 255, 35), rounding + 1);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(tint.x * 255, tint.y * 255, tint.z * 255, tint.w * 255), rounding);
    dl->AddRectFilled(
        ImVec2(pos.x + 2, pos.y + 1), ImVec2(pos.x + size.x - 2, pos.y + 3),
        IM_COL32(kSapphire.x * 255, kSapphire.y * 255, kSapphire.z * 255, 25), rounding);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(kGlassBorder.x * 255, kGlassBorder.y * 255, kGlassBorder.z * 255, kGlassBorder.w * 255),
        rounding, 0, 1.0f);
}

// ── Collapsible Section State ─────────────────────────────────────────────

struct SectionState { bool open = true; };
static std::unordered_map<ImGuiID, SectionState> g_section_states;

bool BeginGlassSection(const char* label) {
    ImGuiID id = ImGui::GetID(label);
    auto& state = g_section_states[id];

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.10f, 0.18f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f, 0.14f, 0.24f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.18f, 0.30f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Text, kSapphire);

    std::string arrow = state.open ? "> " : "> ";
    if (ImGui::Button((arrow + label).c_str(), ImVec2(-1, 22)))
        state.open = !state.open;

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (state.open) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.04f, 0.07f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.10f, 0.16f, 0.26f, 0.30f));
        ImGui::BeginChild((std::string("##sec_") + label).c_str(), ImVec2(-1, 0), ImGuiChildFlags_Borders);
    }
    return state.open;
}

void EndGlassSection(bool is_open) {
    if (is_open) {
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }
}

// ── Button Helpers ────────────────────────────────────────────────────────

bool SapphireButton(const char* label, const ImVec2& size = ImVec2(-1, 30)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.18f, 0.32f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.24f, 0.40f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.14f, 0.26f, 0.95f));
    bool p = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return p;
}

bool DangerButton(const char* label, const ImVec2& size = ImVec2(-1, 26)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.10f, 0.12f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.14f, 0.16f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.08f, 0.10f, 0.90f));
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
    ImGui::SetNextWindowSize(ImVec2(740, 540), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660, 440), ImVec2(1000, 800));
    ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.04f, 0.07f, 0.90f));

    ImGuiWindowFlags wflags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin(title, nullptr, wflags)) {
        this->pos = ImGui::GetWindowPos();
        this->size = ImGui::GetWindowSize();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wpos = ImGui::GetWindowPos();
        ImVec2 wsize = ImGui::GetWindowSize();

        // Glassmorphism background
        DrawGlassCard(dl, wpos, wsize, kGlassBgDeep, 12.0f);

        // ── Title bar ─────────────────────────────────────────────────
        const float title_h = 40.0f;
        dl->AddRectFilled(wpos, ImVec2(wpos.x + wsize.x, wpos.y + title_h),
            IM_COL32(10, 13, 22, 230), 12.0f);
        dl->AddRectFilled(ImVec2(wpos.x, wpos.y + title_h - 1),
            ImVec2(wpos.x + wsize.x, wpos.y + title_h),
            IM_COL32(kSapphire.x * 255, kSapphire.y * 255, kSapphire.z * 255, 50));

        ImGui::SetCursorScreenPos(ImVec2(wpos.x + 14, wpos.y + 11));
        ImGui::TextColored(kSapphireBright, "%s", title);
        ImGui::SameLine(wpos.x + wsize.x - 100);
        ImGui::TextDisabled("v0.1.0");

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
            const float sidebar_w = 150.0f;
            ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, avail.y), ImGuiChildFlags_None);
            {
                auto sp = ImGui::GetCursorScreenPos();
                auto ss = ImGui::GetContentRegionAvail();
                DrawGlassCard(dl, sp, ss, kGlassBgLight, 8.0f);

                ImGui::SetCursorPos(ImVec2(6, 6));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));

                for (int i = 0; i < 6; ++i) {
                    const auto& tab = tabs[i];
                    bool is_active = (active_tab == tab.id);

                    ImVec4 bg = is_active
                        ? ImVec4(0.10f, 0.20f, 0.36f, 0.90f)
                        : ImVec4(0.05f, 0.07f, 0.12f, 0.55f);
                    ImVec4 hover = is_active
                        ? ImVec4(0.14f, 0.26f, 0.44f, 0.95f)
                        : ImVec4(0.07f, 0.10f, 0.18f, 0.65f);
                    ImVec4 text = is_active ? kSapphireBright : kTextSecondary;

                    ImGui::PushStyleColor(ImGuiCol_Button, bg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.14f, 0.26f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_Text, text);

                    if (ImGui::Button((tab.icon + " " + tab.label).c_str(), ImVec2(-1, 28)))
                        active_tab = tab.id;

                    ImGui::PopStyleColor(4);
                }

                ImGui::PopStyleVar(3);

                // Bottom section
                auto space_left = ImGui::GetContentRegionAvail().y;
                ImGui::SetCursorPosY(ImGui::GetCursorPos().y + std::max(0.0f, space_left - 72.0f));
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
            ImGui::BeginChild("##content", ImVec2(0, avail.y), ImGuiChildFlags_None);
            {
                auto cp = ImGui::GetCursorScreenPos();
                auto cs = ImGui::GetContentRegionAvail();
                DrawGlassCard(dl, cp, cs, kGlassBg, 8.0f);

                ImGui::SetCursorPos(ImVec2(10, 10));
                ImGui::BeginDisabled(!cfg::enabled);

                // Tab alpha for fade transition
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab_alpha);

                if (active_tab == Tab::PLAYER)
                {
                    ImGui::TextColored(kSapphire, "Player ESP");
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
                    ImGui::TextColored(kSapphire, "World");
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
                    ImGui::TextColored(kSapphire, "Aim");
                    ImGui::Separator();

                    // Driver status
                    if (MouseAim::DriverInstalled())
                        ImGui::TextColored(kAccentGreen, "Driver: ready");
                    else
                        ImGui::TextColored(kAccentRed, "Driver: MISSING");

                    ImGui::Spacing();

                    // Enable / Disable toggle
                    if (!cfg::aim::enabled) {
                        if (SapphireButton("Enable Aim"))
                            cfg::aim::enabled = true;
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.12f, 0.16f, 0.90f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.16f, 0.20f, 0.95f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.10f, 0.14f, 0.95f));
                        if (ImGui::Button("Disable Aim", ImVec2(-1, 30)))
                            cfg::aim::enabled = false;
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::Spacing();
                    ImGui::Checkbox("MB5 hotkey toggle", &cfg::aim::hotkey);

                    ImGui::Spacing();
                    ImGui::BeginDisabled(!cfg::aim::enabled);
                    {
                        ImGui::Checkbox("Game mode (CS2)", &cfg::aim::game_mode);
                        ImGui::Checkbox("Aim at enemies", &cfg::aim::aim_at_enemies);
                        ImGui::Checkbox("Visible only", &cfg::aim::visible_only);
                        ImGui::SetItemTooltip("Only lock onto targets you can see");
                        ImGui::Checkbox("Auto-start aim", &cfg::aim::auto_start);

                        ImGui::Spacing();

                        if (BeginGlassSection("Target")) {
                            static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
                            int tp = cfg::aim::target_part;
                            if (ImGui::Combo("Target", &tp, target_parts, 4))
                                cfg::aim::target_part = tp;
                            EndGlassSection(true);
                        }

                        if (BeginGlassSection("Weapon Speed")) {
                            ImGui::SliderFloat("Rifle", &cfg::aim::rifle_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Pistol", &cfg::aim::pistol_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Sniper", &cfg::aim::sniper_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("SMG", &cfg::aim::smg_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::Spacing();
                            ImGui::SliderFloat("Recoil comp", &cfg::aim::recoil_compensation, 0.0f, 1.0f, "%.0f%%");
                            ImGui::SliderFloat("Switch delay", &cfg::aim::target_switch_delay, 0.0f, 0.5f, "%.2fs");
                            EndGlassSection(true);
                        }

                        if (BeginGlassSection("FOV")) {
                            float aim_w = 1920.0f, aim_h = 1080.0f;
                            MouseAim::ScreenSize(aim_w, aim_h);
                            const float fsr = std::sqrt(aim_w * aim_w + aim_h * aim_h) * 0.5f;
                            ImGui::SliderFloat("FOV radius", &cfg::aim::fov_radius, 0.0f, fsr, "%.0f px");
                            EndGlassSection(true);
                        }
                    }
                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::TextWrapped("F9: panic key (disables all). MB5: toggle aim.");
                }
                else if (active_tab == Tab::SKINS)
                {
                    // ── Skin Changer: Grid Browser ─────────────────────
                    ImGui::TextColored(kSapphire, "Skin Changer");
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
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.20f, 0.36f, 0.90f));
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
                            ImVec4 bg = is_sel ? ImVec4(0.10f, 0.20f, 0.36f, 0.90f)
                                : has_skin ? ImVec4(0.06f, 0.16f, 0.10f, 0.85f)
                                : ImVec4(0.07f, 0.09f, 0.14f, 0.80f);

                            ImGui::PushStyleColor(ImGuiCol_Button, bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg.x + 0.03f, bg.y + 0.03f, bg.z + 0.05f, bg.w));
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
                        ImGui::TextColored(kSapphire, "%s", winfo.display_name);
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
                        ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", SkinDB::WearName(selected_wear));

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
                            ImGui::TextColored(kAccentGreen, "Active (%d)", (int)active.size());
                            ImGui::Separator();
                            ImGui::BeginChild("##active_skins", ImVec2(0, 80));
                            for (const auto& [id, skin] : active) {
                                const auto& wn = SkinDB::Weapons().count(id)
                                    ? SkinDB::Weapons().at(id).display_name : "?";
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.5f, 1.0f), "%s", wn);
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
                    ImGui::TextColored(kSapphire, "Macro");
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
                    ImGui::TextColored(kSapphire, "Settings");
                    ImGui::Separator();

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

    // ── Glassmorphism Sapphire Theme ──────────────────────────────────
    style.Colors[ImGuiCol_Text] = kTextPrimary;
    style.Colors[ImGuiCol_TextDisabled] = kTextMuted;

    style.Colors[ImGuiCol_WindowBg] = kGlassBgDeep;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.07f, 0.11f, 0.96f);

    style.Colors[ImGuiCol_Border] = kGlassBorder;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.07f, 0.09f, 0.14f, 0.80f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.14f, 0.22f, 0.88f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.16f, 0.28f, 0.92f);

    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.04f, 0.06f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.06f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.03f, 0.03f, 0.05f, 0.60f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.05f, 0.06f, 0.09f, 0.95f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.04f, 0.06f, 0.50f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.16f, 0.24f, 0.65f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.22f, 0.32f, 0.75f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.20f, 0.28f, 0.40f, 0.88f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.08f, 0.12f, 0.20f, 0.80f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.18f, 0.28f, 0.88f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.22f, 0.38f, 0.95f);

    style.Colors[ImGuiCol_Header] = ImVec4(0.08f, 0.14f, 0.24f, 0.75f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.20f, 0.32f, 0.85f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.10f, 0.24f, 0.40f, 0.92f);

    style.Colors[ImGuiCol_CheckMark] = kSapphire;
    style.Colors[ImGuiCol_SliderGrab] = kSapphire;
    style.Colors[ImGuiCol_SliderGrabActive] = kSapphireBright;

    style.Colors[ImGuiCol_Separator] = ImVec4(0.10f, 0.16f, 0.26f, 0.45f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.18f, 0.28f, 0.44f, 0.55f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.22f, 0.40f, 0.64f, 0.75f);

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.40f, 0.65f, 0.30f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.20f, 0.40f, 0.65f, 0.60f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.05f, 0.06f, 0.10f, 0.80f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.24f, 0.38f, 0.88f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.10f, 0.18f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.04f, 0.05f, 0.08f, 0.75f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.06f, 0.08f, 0.13f, 0.88f);

    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.30f, 0.40f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.40f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = kSapphire;
    style.Colors[ImGuiCol_PlotHistogramHovered] = kSapphireBright;

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.36f, 0.58f, 0.40f);
    style.Colors[ImGuiCol_DragDropTarget] = kSapphire;
    style.Colors[ImGuiCol_NavHighlight] = kSapphire;
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.76f, 0.88f, 0.45f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.04f, 0.06f, 0.10f, 0.55f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.04f, 0.06f, 0.10f, 0.65f);

    style.FrameBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(10, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 8.0f;

    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 4.0f;

    auto& io = ImGui::GetIO();
    io.Fonts->Clear();
#ifdef _WIN32
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 15.0f);
#else
    if (!io.Fonts->AddFontFromFileTTF("/usr/share/fonts/TTF/DejaVuSans.ttf", 15.0f))
        io.Fonts->AddFontDefault();
#endif

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
    DrawGlassCard(d, help_pos, help_size, ImVec4(0.04f, 0.05f, 0.08f, 0.90f), 8.0f);

    d->AddText(
        ImVec2(screen.x / 2 - size.x / 2, 80),
        IM_COL32(220, 225, 235, 255),
        help
    );
}
