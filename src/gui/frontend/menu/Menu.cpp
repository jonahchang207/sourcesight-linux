#include "Menu.hpp"

#include "config/Config.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/classes/MapRaytrace.hpp"
#include "core/input/MouseAim.hpp"
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"
#include "assets/fonts/Icons.h"
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




// ───────────────────────────────────────────────────────────────────────────
// The palette and glass/aurora drawing helpers live in Theme.hpp (single
// source of truth for the "Sapphire Glass" design language). This TU opts
// into those tokens so every existing reference keeps working.
// ───────────────────────────────────────────────────────────────────────────
using namespace theme;
const ImVec4 studioAccent(.48f,.66f,.45f,1);
const ImVec4 studioCanvas(.12f,.13f,.14f,1);
const ImVec4 studioCard(.17f,.18f,.19f,1);
const ImVec4 studioBorder(.27f,.29f,.30f,1);
const ImVec4 studioText(.91f,.93f,.92f,1);
const ImVec4 studioMuted(.64f,.68f,.66f,1);
const ImVec4 studioAccentSoft(.77f,.88f,.70f,1);
const ImVec4 studioAccentStrong(.18f,.30f,.21f,1);
const ImVec4 studioAccentBright(.37f,.56f,.38f,1);

// ── Collapsible Section State (animated) ──────────────────────────────────

// Opaque cards share a table grid; each owns its padding and natural height.
bool BeginSettingsCard(const char* label, bool body_enabled = true) {
    ImGui::TableNextColumn();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, studioCard);
    ImGui::PushStyleColor(ImGuiCol_Border, studioBorder);
    ImGui::BeginChild(label, ImVec2(-1, 0),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    const std::string heading = std::string(label).substr(0, std::string(label).find("##"));
    ImGui::TextColored(studioText, "%s", heading.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushItemWidth(std::min(145.0f, ImGui::GetContentRegionAvail().x * 0.42f));
    if (!body_enabled) ImGui::BeginDisabled();
    return true;
}

void EndSettingsCard(bool is_open, bool body_enabled = true) {
    if (!is_open) return;
    if (!body_enabled) ImGui::EndDisabled();
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// Seven distinct line icons, drawn in the same 20px coordinate system.
void CategoryIcon(ImDrawList* d, ImVec2 p, int kind, ImU32 c) {
    const auto line = [&](float x, float y, float xx, float yy) {
        d->AddLine(p + ImVec2(x,y), p + ImVec2(xx,yy), c, 1.6f);
    };
    if (kind == 0) {
        d->AddCircle(p + ImVec2(10,5), 3, c, 16, 1.6f);
        d->AddRect(p + ImVec2(3,11), p + ImVec2(17,19), c, 4, 0, 1.6f);
    } else if (kind == 1) {
        d->AddCircle(p + ImVec2(10,10), 8, c, 24, 1.6f);
        d->AddEllipse(p + ImVec2(10,10), ImVec2(3,8), c, 0, 24, 1.6f);
        line(2,10,18,10);
    } else if (kind == 2) {
        d->AddCircle(p + ImVec2(10,10), 6, c, 24, 1.6f);
        line(10,0,10,6); line(10,14,10,20);
        line(0,10,6,10); line(14,10,20,10);
    } else if (kind == 3) {
        line(12,1,4,11); line(4,11,11,11);
        line(11,11,8,19); line(8,19,17,8); line(17,8,10,8);
    } else if (kind == 4) {
        d->AddRect(p+ImVec2(1,4),p+ImVec2(19,16),c,3,0,1.6f);
        line(5,8,7,8); line(10,8,12,8); line(15,8,16,8); line(5,12,15,12);
    } else if (kind == 5) {
        for (int i=0;i<5;++i) {
            float h = i==2 ? 9 : (i%2 ? 6 : 3);
            line(2+i*4,10-h,2+i*4,10+h);
        }
    } else {
        for(int i=0;i<3;++i) {
            float x=3+i*7, y=i==1?13:6;
            line(x,1,x,19);
            d->AddCircleFilled(p+ImVec2(x,y),3,c);
        }
    }
}

bool Toggle(const char* label, bool* value) {
    ImGui::PushID(label);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const std::string visible = std::string(label).substr(0, std::string(label).find("##"));
    const float width=38 + 10 + ImGui::CalcTextSize(visible.c_str()).x;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.35f,.45f,.35f,.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.35f,.45f,.35f,.14f));
    bool changed=ImGui::Button("##switch", ImVec2(width,24));
    ImGui::PopStyleColor(3);
    if(changed) *value=!*value;
    auto* d=ImGui::GetWindowDrawList();
    const float alpha=ImGui::GetStyle().Alpha;
    d->AddRectFilled(p,p+ImVec2(36,22),
        ImGui::GetColorU32(*value ? studioAccent : ImVec4(.34f,.36f,.37f,1)),11);
    d->AddCircleFilled(p+ImVec2(*value?25:11,11),8,IM_COL32(255,255,255,int(255*alpha)));
    d->AddText(p+ImVec2(46,3),ImGui::GetColorU32(ImGuiCol_Text),visible.c_str());
    ImGui::PopID();
    return changed;
}

// ── Button Helpers ────────────────────────────────────────────────────────

// Primary action button — sapphire CTA, clearly the main action.
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(-1, 30)) {
    ImGui::PushStyleColor(ImGuiCol_Button, studioAccentSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.84f,.93f,.79f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.62f,.76f,.56f,1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.10f,.18f,.13f,1));
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

    static int active_tab = 0;
    static float saved_until = 0;
    static bool save_ok = true;
    const char* descriptions[] = {
        "Shape the information you see in the field.",
        "Keep your surroundings in view.",
        "Fine-tune targeting and response.",
        "Configure activation and timing.",
        "Make repeated actions easier.",
        "See the sounds around you.",
        "Your profiles, preferences and application controls."
    };
    ImGui::SetNextWindowSize(ImVec2(1160, 760), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(940, 600), ImVec2(1600, 1100));
    ImGui::SetNextWindowPos(screen * 0.5f, ImGuiCond_FirstUseEver, ImVec2(0.5f,0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, studioCanvas);
    if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {
        pos=ImGui::GetWindowPos();
        size=ImGui::GetWindowSize();
        constexpr float rail=196;
        auto* d=ImGui::GetWindowDrawList();
        d->AddRectFilled(pos, pos+ImVec2(rail,size.y), IM_COL32(24,27,28,255),24,
                         ImDrawFlags_RoundCornersLeft);
        // Aperture mark and wordmark.
        const ImVec2 mark=pos+ImVec2(34,38);
        d->AddCircle(mark,14,IM_COL32(195,224,178,255),6,2);
        d->AddCircleFilled(mark,4,IM_COL32(195,224,178,255));
        d->AddText(pos+ImVec2(58,23),IM_COL32(248,249,243,255),"SOURCESIGHT");
        d->AddText(pos+ImVec2(58,44),IM_COL32(156,173,162,255),"CONTROL STUDIO");
        ImGui::SetCursorPos(ImVec2(22,103));
        ImGui::TextColored(ImVec4(.57f,.66f,.60f,1),"WORKSPACE");
        for(int i=0;i<7;++i) {
            ImGui::PushID(i);
            ImGui::SetCursorPos(ImVec2(12,134+i*52));
            const bool selected=active_tab==i;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,12);
            ImGui::PushStyleColor(ImGuiCol_Button,selected?ImVec4(.77f,.88f,.70f,1):ImVec4(.094f,.106f,.110f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,selected?ImVec4(.82f,.92f,.76f,1):ImVec4(.23f,.31f,.26f,1));
            ImGui::PushStyleColor(ImGuiCol_Text,selected?ImVec4(.10f,.18f,.13f,1):ImVec4(.80f,.85f,.80f,1));
            const ImVec2 p=ImGui::GetCursorScreenPos();
            if(ImGui::Button(("      "+tabs[i].label).c_str(),ImVec2(172,42))) active_tab=i;
            CategoryIcon(d,p+ImVec2(14,11),i,ImGui::GetColorU32(ImGuiCol_Text));
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
        ImGui::SetCursorPos(ImVec2(22,size.y-67));
        ImGui::TextColored(ImVec4(.80f,.85f,.80f,1),"SourceSight / 0.6.0");
        ImGui::SetCursorPos(ImVec2(22,size.y-42));
        ImGui::TextColored(ImVec4(.57f,.66f,.60f,1),"Insert to close");

        ImGui::SetCursorPos(ImVec2(rail+28,24));
        ImGui::TextDisabled("WORKSPACE  /  %s",tabs[active_tab].label.c_str());
        ImGui::SetCursorPos(ImVec2(size.x-270,22));
        Toggle("Overlay", &cfg::enabled);
        ImGui::SameLine(0,18);
        if(ImGui::Button("Save profile",ImVec2(116,32))) {
            save_ok=Config::Write();
            saved_until=float(ImGui::GetTime())+3;
        }
        ImGui::SetCursorPos(ImVec2(rail+28,69));
        ImGui::PushFont(font_heading);
        ImGui::TextUnformatted(tabs[active_tab].label.c_str());
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(rail+28,108));
        ImGui::TextDisabled("%s",descriptions[active_tab]);
        ImGui::SetCursorPos(ImVec2(rail+28,146));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::BeginChild("##workspace",ImVec2(size.x-rail-56,size.y-198),ImGuiChildFlags_None);
        ImGui::PopStyleVar();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(8,8));
        const int columns=ImGui::GetContentRegionAvail().x>=790?2:1;
        if(ImGui::BeginTable("##cards",columns,ImGuiTableFlags_SizingStretchSame)) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,1.0f);
                if (active_tab == Tab::PLAYER)
                {

                    if (BeginSettingsCard("Player boxes")) {
                        ImGui::BeginGroup();
                        {
                            Toggle("Box", &cfg::esp::box);
                            ImGui::BeginDisabled(!cfg::esp::box);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##box", cfg::esp::colors::box_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##box", cfg::esp::colors::box_enemy.data(), color_flags);
                                Toggle("Filled", &cfg::esp::box_filled);
                                if (cfg::esp::box_filled)
                                    ImGui::SliderFloat("Fill alpha", &cfg::esp::box_fill_alpha, 0.02f, 1.0f, "%.2f");
                                ImGui::SliderFloat("Box thickness", &cfg::esp::box_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::EndGroup();
                            EndSettingsCard(true);
                            BeginSettingsCard("Skeleton");
                            ImGui::BeginGroup();
                            Toggle("Skeleton", &cfg::esp::skeleton);
                            ImGui::BeginDisabled(!cfg::esp::skeleton);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##skel", cfg::esp::colors::skeleton_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##skel", cfg::esp::colors::skeleton_enemy.data(), color_flags);
                                ImGui::SliderFloat("Skeleton thickness", &cfg::esp::skeleton_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::EndGroup();
                            EndSettingsCard(true);
                            BeginSettingsCard("Head tracking");
                            ImGui::BeginGroup();
                            Toggle("Head Tracker", &cfg::esp::head_tracker);
                            ImGui::BeginDisabled(!cfg::esp::head_tracker);
                            {
                                Toggle("Filled Head", &cfg::esp::head_tracker_filled);
                                ImGui::SliderFloat("Head size", &cfg::esp::head_tracker_size, 2.0f, 14.0f, "%.1f");
                                ImGui::ColorEdit4("Team##head", cfg::esp::colors::tracker_team.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
                                ImGui::ColorEdit4("Enemy##head", cfg::esp::colors::tracker_enemy.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
                            }
                            ImGui::EndDisabled();

                            ImGui::EndGroup();
                            EndSettingsCard(true);
                            BeginSettingsCard("Player tracers");
                            ImGui::BeginGroup();
                            Toggle("Tracers", &cfg::esp::tracers);
                            ImGui::BeginDisabled(!cfg::esp::tracers);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##trc", cfg::esp::colors::tracer_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##trc", cfg::esp::colors::tracer_enemy.data(), color_flags);
                                ImGui::SliderFloat("Tracer thickness", &cfg::esp::tracer_thickness, 1.0f, 4.0f, "%.1f");
                            }
                            ImGui::EndDisabled();

                            ImGui::EndGroup();
                            EndSettingsCard(true);
                            BeginSettingsCard("Bullet trails");
                            ImGui::BeginGroup();
                            Toggle("Bullet Tracer", &cfg::esp::bullet_tracer::enabled);
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
                        ImGui::Spacing();
                        ImGui::BeginGroup();
                        {
                            ImGui::EndGroup();
                            EndSettingsCard(true);
                            BeginSettingsCard("Player information");
                            ImGui::BeginGroup();
                            Toggle("Health", &cfg::esp::health);
                            if (cfg::esp::health)
                                Toggle("Health Number", &cfg::esp::health_number);
                            Toggle("Armor", &cfg::esp::armor);
                            Toggle("Game-spotted only", &cfg::esp::spotted);
                            ImGui::SetItemTooltip("Uses CS2's spotted state.");
                            Toggle("Show Team", &cfg::esp::team);
                            Toggle("Spotted only", &cfg::esp::spotted_only);
                            ImGui::SetItemTooltip("Only render enemies you can actually see");
                            Toggle("Distance", &cfg::esp::distance);
                            Toggle("Headshot line", &cfg::esp::headshot_line);
                            ImGui::SetItemTooltip("Line from crosshair to enemy head");
                        }
                        ImGui::EndGroup();
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Flags")) {
                        ImGui::BeginGroup();
                        {
                            Toggle("Flashed", &cfg::esp::flags::flashed);
                            ImGui::BeginDisabled(!cfg::esp::flags::flashed);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##fl", cfg::esp::colors::flags::flashed_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##fl", cfg::esp::colors::flags::flashed_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            Toggle("Reloading", &cfg::esp::flags::reloading);
                            ImGui::BeginDisabled(!cfg::esp::flags::reloading);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##rl", cfg::esp::colors::flags::reloading_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##rl", cfg::esp::colors::flags::reloading_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            Toggle("Defusing", &cfg::esp::flags::defusing);
                            ImGui::BeginDisabled(!cfg::esp::flags::defusing);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##df", cfg::esp::colors::flags::defusing_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##df", cfg::esp::colors::flags::defusing_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            Toggle("Scoped", &cfg::esp::flags::scoped);
                            ImGui::BeginDisabled(!cfg::esp::flags::scoped);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Team##sc", cfg::esp::colors::flags::scoped_team.data(), color_flags);
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Enemy##sc", cfg::esp::colors::flags::scoped_enemy.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            Toggle("Has C4", &cfg::esp::flags::has_c4);
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
                        ImGui::Spacing();
                        ImGui::BeginGroup();
                        {
                            Toggle("Name", &cfg::esp::flags::name);
                            Toggle("Money", &cfg::esp::flags::money);
                            Toggle("Weapon", &cfg::esp::flags::weapon);
                            Toggle("Ammo", &cfg::esp::flags::ammo);
                            Toggle("Ping", &cfg::esp::flags::ping);
                        }
                        ImGui::EndGroup();
                        EndSettingsCard(true);
                    }
                }
                else if (active_tab == Tab::WORLD)
                {

                    if (BeginSettingsCard("Bomb")) {
                        Toggle("Bomb ESP", &cfg::esp::bomb);
                        ImGui::SameLine();
                        ImGui::ColorEdit4("Bomb color", cfg::esp::colors::bomb.data(), color_flags);
                        Toggle("Bomb Location", &cfg::world::bomb::location);
                        Toggle("Bomb Timer", &cfg::world::bomb::timer);
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Spectators")) {
                        Toggle("Enable", &cfg::world::spectators::enabled);
                        if (cfg::world::spectators::enabled) {
                            Toggle("Detailed", &cfg::world::spectators::detailed);
                            Toggle("Only Self", &cfg::world::spectators::self_only);
                            ImGui::SetItemTooltip("Only display users spectating you");
                        }
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Crosshair")) {
                        Toggle("Enable", &cfg::world::crosshair::enabled);
                        ImGui::BeginDisabled(!cfg::world::crosshair::enabled);
                        {
                            Toggle("Snipers only", &cfg::world::crosshair::sniper_only);
                            Toggle("Center dot", &cfg::world::crosshair::center_dot);
                            Toggle("Outline", &cfg::world::crosshair::outline);
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
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Velocity")) {
                        Toggle("Enable", &cfg::world::velocity::enabled);
                        ImGui::BeginDisabled(!cfg::world::velocity::enabled);
                        {
                            ImGui::SliderInt("Sample rate", &cfg::world::velocity::sample_rate, 1, 100);
                            ImGui::SliderFloat("Sample length", &cfg::world::velocity::sample_length, 1.0f, 20.0f, "%.1f");
                        }
                        ImGui::EndDisabled();
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Map geometry")) {
                        Toggle("Wireframe Map", &cfg::esp::wireframe);
                        if (MapRaytrace::IsReady())
                            ImGui::TextDisabled("%s / %zu triangles", MapRaytrace::CurrentMap().c_str(),
                                                MapRaytrace::TriangleCount());
                        else
                            ImGui::TextWrapped("Geometry unavailable or loading. Missing map files are retried automatically.");
                        ImGui::BeginDisabled(!cfg::esp::wireframe);
                        ImGui::SliderFloat("Max distance", &cfg::esp::wireframe_max_dist, 500, 10000, "%.0f u");
                        ImGui::SliderFloat("Opacity", &cfg::esp::wireframe_opacity, 0, 1, "%.2f");
                        ImGui::ColorEdit3("Color", cfg::esp::wireframe_color.data(), ImGuiColorEditFlags_NoInputs);
                        ImGui::SliderInt("Detail", &cfg::esp::wireframe_budget, 500, 8000, "%d edges");
                        ImGui::SetItemTooltip("Maximum edges per frame. Lower detail reduces rendering cost; nearby geometry is prioritized.");
                        ImGui::EndDisabled();
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Radar")) {
                        Toggle("Enable", &cfg::world::radar::enabled);
                        ImGui::BeginDisabled(!cfg::world::radar::enabled);
                        {
                            ImGui::SliderFloat("Opacity", &cfg::world::radar::opacity, 0, 1, "%.2f");
                            ImGui::SliderFloat("Range", &cfg::world::radar::range, 100.f, 8000.f, "%.0f u");
                            Toggle("Disable Rotation", &cfg::world::radar::no_rotate);
                        }
                        ImGui::EndDisabled();
                        EndSettingsCard(true);
                    }
                }
                else if (active_tab == Tab::AIM)
                {

                    BeginSettingsCard("Activation");
                    // Driver status
                    if (MouseAim::DriverInstalled())
                        ImGui::TextColored(kSignalOK, "Driver: ready");
                    else
                        ImGui::TextColored(kSignalErr, "Driver: MISSING");

                    ImGui::Spacing();

                    // Enable / Disable toggle
                    if (!cfg::aim::enabled) {
                        if (PrimaryButton("Enable Aim"))
                            cfg::aim::enabled = true;
                    } else {
                        if (DangerButton("Disable Aim", ImVec2(-1, 30)))
                            cfg::aim::enabled = false;
                    }

                    ImGui::Spacing();
                    Toggle("MB5 hotkey toggle", &cfg::aim::hotkey);

                    EndSettingsCard(true);
                    if (BeginSettingsCard("On Lock", cfg::aim::enabled)) {
                        Toggle("Release A/D + burst on target", &cfg::aim::lock_burst);
                        ImGui::SetItemTooltip(
                            "Links the aim and trigger: once the locked target is reached the "
                            "strafe keys (A/D) are released and bursts are fired using the "
                            "Triggerbot burst settings until the target is lost. The manual "
                            "crosshair trigger is disabled while this is on.");
                        EndSettingsCard(true, cfg::aim::enabled);
                    }

                    ImGui::Spacing();
                    {
                        BeginSettingsCard("Behavior");
                        Toggle("Game mode (CS2)", &cfg::aim::game_mode);
                        Toggle("Aim at enemies", &cfg::aim::aim_at_enemies);
                        Toggle("Visible only", &cfg::aim::visible_only);
                        ImGui::SetItemTooltip("Only lock onto targets you can see");
                        Toggle("Auto-start aim", &cfg::aim::auto_start);

                        ImGui::Spacing();

                        EndSettingsCard(true);
                        if (BeginSettingsCard("Target##aim", cfg::aim::enabled)) {
                            static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
                            int tp = cfg::aim::target_part;
                            if (ImGui::Combo("Target", &tp, target_parts, 4))
                                cfg::aim::target_part = tp;
                            static const char* priorities[] = { "Closest to crosshair", "Nearest enemy", "Lowest HP", "Farthest enemy" };
                            int prio = cfg::aim::priority;
                            if (ImGui::Combo("Priority", &prio, priorities, 4)) {
                                cfg::aim::priority = std::clamp(prio, 0, 3);
                            }
                            ImGui::SetItemTooltip("Which enemy to lock when several are in the FOV ring. Closest to crosshair is the classic feel; Lowest HP hunts the weakest target first.");
                            EndSettingsCard(true, cfg::aim::enabled);
                        }

                        if (BeginSettingsCard("Weapon Speed", cfg::aim::enabled)) {
                            ImGui::SliderFloat("Rifle", &cfg::aim::rifle_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Pistol", &cfg::aim::pistol_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("Sniper", &cfg::aim::sniper_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::SliderFloat("SMG", &cfg::aim::smg_mult, 0.2f, 3.0f, "x%.1f");
                            ImGui::Spacing();
                            ImGui::SliderFloat("Recoil comp", &cfg::aim::recoil_compensation, 0.0f, 1.0f, "%.0f%%");
                            ImGui::SliderFloat("Switch delay", &cfg::aim::target_switch_delay, 0.0f, 0.5f, "%.2fs");
                            EndSettingsCard(true, cfg::aim::enabled);
                        }

                        if (BeginSettingsCard("FOV", cfg::aim::enabled)) {
                            float aim_w = 1920.0f, aim_h = 1080.0f;
                            MouseAim::ScreenSize(aim_w, aim_h);
                            const float fsr = std::sqrt(aim_w * aim_w + aim_h * aim_h) * 0.5f;
                            ImGui::SliderFloat("FOV radius", &cfg::aim::fov_radius, 0.0f, fsr, "%.0f px");
                            ImGui::SliderFloat("Exit FOV multiplier", &cfg::aim::exit_fov_mult, 1.0f, 2.5f, "x%.2f");
                            ImGui::SetItemTooltip("Targets are released only beyond FOV x this, so a moving target just past the ring edge keeps its lock (hysteresis).");
                            EndSettingsCard(true, cfg::aim::enabled);
                        }

                        if (BeginSettingsCard("Movement", cfg::aim::enabled)) {
                            ImGui::SliderFloat("Smoothness", &cfg::aim::smoothness, 0.02f, 1.0f, "%.2f");
                            ImGui::SetItemTooltip("Proportional gain; lower = softer, less twitchy finish.");
                            ImGui::SliderFloat("Target smoothing", &cfg::aim::aim_smoothing, 0.05f, 1.0f, "%.2f");
                            ImGui::SetItemTooltip("EMA on the tracked aim point. 1.0 = raw (bone jitter makes it shiver); lower values are steadier but trail a moving target slightly.");
                            ImGui::SliderFloat("Lead time", &cfg::aim::lead_time, 0.0f, 0.5f, "%.3fs");
                            ImGui::SetItemTooltip("Extrapolate the aim ahead of a moving target by this many seconds of its screen velocity.");
                            EndSettingsCard(true, cfg::aim::enabled);
                        }

                        if (BeginSettingsCard("Spinbot")) {
                            Toggle("Spin continuously", &cfg::spinbot::enabled);
                            ImGui::SetItemTooltip("Spins your view (and the player model with it) through the kernel mouse driver — no memory writes. The model rotates so incoming shots fan out across a moving hitbox. Pauses while the menu is open and uses the F9 panic key. Disable Aim while spinning so the two don't fight.");
                            Toggle("Shoot while spinning", &cfg::spinbot::shoot);
                            ImGui::SetItemTooltip("Automatically fire exactly one clean shot the instant the crosshair sweeps onto an enemy, using your Triggerbot's visibility/target/threshold rules — a shot per pass, no sprayed rounds.");
                            int dir_i = cfg::spinbot::direction > 0 ? 0 : 1;
                            if (ImGui::Combo("Direction", &dir_i, "Clockwise\0Counter-clockwise\0"))
                                cfg::spinbot::direction = dir_i == 0 ? 1 : -1;
                            ImGui::SliderFloat("Spin speed", &cfg::spinbot::speed, 200.0f, 6000.0f, "%.0f px/s");
                            ImGui::SliderFloat("Pitch sway", &cfg::spinbot::pitch_sway, 0.0f, 120.0f, "%.0f px");
                            ImGui::SetItemTooltip("Vertical swing added to the spin (0 = flat yaw circle).");
                            ImGui::SliderFloat("Sway rate", &cfg::spinbot::sway_hz, 0.5f, 6.0f, "%.1f Hz");
                            EndSettingsCard(true);
                        }
                    }

                    ImGui::Spacing();

                }
                else if (active_tab == Tab::TRIGGERBOT)
                {

                    BeginSettingsCard("Activation");
                    // Enable / Disable toggle
                    if (!cfg::triggerbot::enabled) {
                        if (PrimaryButton("Enable Triggerbot"))
                            cfg::triggerbot::enabled = true;
                    } else {
                        if (DangerButton("Disable Triggerbot", ImVec2(-1, 30)))
                            cfg::triggerbot::enabled = false;
                    }

                    ImGui::Spacing();
                    Toggle("Hold key to fire", &cfg::triggerbot::hotkey);
                    ImGui::SetItemTooltip("Hold Left Alt to activate triggerbot");

                    ImGui::Spacing();
                    {
                        Toggle("Visible only", &cfg::triggerbot::visible_only);
                        ImGui::SetItemTooltip("Only fire at enemies you can see");

                        ImGui::Spacing();

                        EndSettingsCard(true);
                        if (BeginSettingsCard("Target##trigger", cfg::triggerbot::enabled)) {
                            static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
                            int tp = cfg::aim::target_part;
                            if (ImGui::Combo("Target", &tp, target_parts, 4))
                                cfg::aim::target_part = tp;
                            ImGui::SliderFloat("On-target radius", &cfg::triggerbot::threshold, 4.0f, 60.0f, "%.0f px");
                            ImGui::SetItemTooltip("How close the crosshair must be to the enemy before firing. Raise it to trigger sooner when strafing or if the aim is slightly off.");
                            EndSettingsCard(true, cfg::triggerbot::enabled);
                        }

                        if (BeginSettingsCard("Fire Settings", cfg::triggerbot::enabled)) {
                            ImGui::SliderInt("Fire delay (ms)", &cfg::triggerbot::delay_ms, 0, 200);
                            ImGui::SetItemTooltip("Delay before firing (0 = instant)");
                            ImGui::SliderInt("Burst count", &cfg::triggerbot::burst_count, 1, 10);
                            ImGui::SetItemTooltip("Shots per trigger activation");
                            ImGui::SliderInt("Burst delay (ms)", &cfg::triggerbot::burst_delay_ms, 20, 300);
                            ImGui::SetItemTooltip("Delay between burst shots");
                            ImGui::SliderInt("Dwell (ms)", &cfg::triggerbot::dwell_ms, 0, 200);
                            ImGui::SetItemTooltip("Debounce: hold the crosshair on the enemy this long before firing (0 = instant). Higher suppresses accidental bursts from aim jitter; 25ms is near-instant.");
                            EndSettingsCard(true, cfg::triggerbot::enabled);
                        }

                        if (BeginSettingsCard("Weapon Filter", cfg::triggerbot::enabled)) {
                            Toggle("Pistols only", &cfg::triggerbot::pistols_only);
                            Toggle("Rifles only", &cfg::triggerbot::rifles_only);
                            if (cfg::triggerbot::pistols_only && cfg::triggerbot::rifles_only) {
                                cfg::triggerbot::rifles_only = false;
                            }
                            EndSettingsCard(true, cfg::triggerbot::enabled);
                        }
                    }

                    ImGui::Spacing();

                }
                else if (active_tab == Tab::MACRO)
                {

                    if (BeginSettingsCard("AWP Quickswitch")) {
                        Toggle("Enable", &cfg::macro::awp_quickswitch);
                        ImGui::SetItemTooltip("Auto-switch to knife and back to cancel bolt animation.");

                        ImGui::BeginDisabled(!cfg::macro::awp_quickswitch);
                        {
                            ImGui::SliderInt("Switch delay (ms)", &cfg::macro::delay_ms, 20, 500);
                            Toggle("All bolt-action weapons", &cfg::macro::auto_switch_all);
                            ImGui::SetItemTooltip("Apply to Scout, G3SG1, SCAR-20 too");
                        }
                        ImGui::EndDisabled();

                        ImGui::Spacing();
                        ImGui::TextWrapped(
                            "Runs while alive and in-game, never while menu is open.\n"
                            "Keys are injected at the OS level."
                        );
                        EndSettingsCard(true);
                    }
                }
                else if (active_tab == Tab::SOUND_ESP)
                {

                    BeginSettingsCard("Activation");
                    // Enable checkbox outside the section so it's always clickable
                    Toggle("Enable Sound ESP", &cfg::sound_esp::enabled);
                    ImGui::Spacing();

                    EndSettingsCard(true);
                    if (BeginSettingsCard("Sound ESP", cfg::sound_esp::enabled)) {
                        ImGui::BeginGroup();
                        {
                            Toggle("Footsteps", &cfg::sound_esp::footsteps);
                            ImGui::BeginDisabled(!cfg::sound_esp::footsteps);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Footsteps color", cfg::sound_esp::footsteps_color.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            Toggle("Gunshots", &cfg::sound_esp::gunshots);
                            ImGui::BeginDisabled(!cfg::sound_esp::gunshots);
                            {
                                ImGui::SameLine();
                                ImGui::ColorEdit4("Gunshots color", cfg::sound_esp::gunshots_color.data(), color_flags);
                            }
                            ImGui::EndDisabled();

                            ImGui::SliderFloat("Max distance", &cfg::sound_esp::max_distance, 100.0f, 5000.0f, "%.0f u");
                            ImGui::SliderFloat("Duration", &cfg::sound_esp::duration, 0.5f, 10.0f, "%.1f s");
                            ImGui::SliderFloat("Fade time", &cfg::sound_esp::fade_time, 0.1f, 5.0f, "%.1f s");
                            ImGui::SliderFloat("Footprint size", &cfg::sound_esp::footprint_size, 2.0f, 32.0f, "%.1f");
                        }
                        ImGui::EndGroup();
                        EndSettingsCard(true, cfg::sound_esp::enabled);
                    }

                    ImGui::Spacing();

                }
                else if (active_tab == Tab::SETTINGS)
                {

                    if (BeginSettingsCard("Profiles")) {
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

                        ImGui::TextColored(studioMuted, "Active profile: %s",
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

                        if (PrimaryButton("Load Profile", ImVec2(-1, 30))) {
                            if (sel >= 0 && Config::LoadProfile(profiles[sel]))
                                refresh_profiles();
                        }
                        if (PrimaryButton("Save to this Profile", ImVec2(-1, 30))) {
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
                        if (PrimaryButton("Create New Profile from Current Settings",
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
                            if (PrimaryButton("Cancel", ImVec2(110, 26)))
                                ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }

                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Display")) {
                        if (Toggle("Streamproof", &cfg::settings::streamproof))
                        {
                            Window::SetAffinity(
                                Window::hwnd,
                                cfg::settings::streamproof ? WindowAffinity::Invisible : WindowAffinity::Disabled
                            );
                        }
                        Toggle("Watermark", &cfg::settings::watermark);
                        if (Toggle("VSync", &cfg::settings::vsync))
                            Window::vsync = cfg::settings::vsync;
                        Toggle("Free CPU", &cfg::settings::free_cpu);
                        ImGui::SetItemTooltip("Let the CPU sleep to free resources.");
                        Toggle("Panic key (F9)", &cfg::settings::panic_key);
                        ImGui::SetItemTooltip("Press F9 to instantly disable all cheats.");
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Bypass")) {
                        Toggle("Timing jitter", &cfg::bypass::timing_jitter);
                        ImGui::SetItemTooltip("Randomise write timing to avoid detection");
                        Toggle("Humanize movement", &cfg::bypass::humanize_movement);
                        ImGui::SetItemTooltip("Add micro-noise to mouse deltas");
                        ImGui::SliderInt("Write delay min (us)", &cfg::bypass::write_delay_min_us, 0, 500);
                        ImGui::SliderInt("Write delay max (us)", &cfg::bypass::write_delay_max_us, 0, 1000);
                        ImGui::SliderFloat("Noise amplitude", &cfg::bypass::noise_amplitude, 0.0f, 2.0f, "%.1f px");
                        EndSettingsCard(true);
                    }

                    if (BeginSettingsCard("Notes")) {
                        ImGui::TextWrapped(
                            "If you experience bad performance/lag:\n"
                            "  - Disable ESP VSync\n"
                            "  - Disable VSync in game: Advanced Video > V-Sync: Disabled\n"
                            "  - Last resort: Disable Free CPU option"
                        );
                        EndSettingsCard(true);
                    }

#ifdef _DEBUG
                    if (BeginSettingsCard("Dev")) {
                        if (Toggle("Console", &cfg::dev::console))
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
                        Toggle("Force Show Flags", &cfg::dev::force_show_flags);
                        EndSettingsCard(true);
                    }
#endif
                }

            ImGui::PopStyleVar();
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::SetCursorPos(ImVec2(rail+28,size.y-34));
        const auto profile=Config::GetActiveProfile();
        ImGui::TextDisabled("PROFILE  /  %s",profile.c_str());
        ImGui::SameLine(0,24);
        if(float(ImGui::GetTime())<saved_until)
            ImGui::TextColored(save_ok?kSignalOK:kSignalErr,save_ok?"Changes saved":"Save failed");
        else
            ImGui::TextDisabled(cfg::enabled?"Overlay active":"Overlay paused");
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Menu::SetupStyles() {
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::StyleColorsDark(&style);
    style.Colors[ImGuiCol_Text] = studioText;
    style.Colors[ImGuiCol_TextDisabled] = studioMuted;
    style.Colors[ImGuiCol_WindowBg] = studioCanvas;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_PopupBg] = studioCard;
    style.Colors[ImGuiCol_Border] = studioBorder;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(.22f,.24f,.25f,1);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(.28f,.31f,.30f,1);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(.33f,.38f,.34f,1);
    style.Colors[ImGuiCol_Button] = studioCard;
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.29f,.34f,.30f,1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.35f,.41f,.35f,1);
    style.Colors[ImGuiCol_Header] = ImVec4(.26f,.32f,.27f,1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(.33f,.40f,.33f,1);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(.36f,.44f,.36f,1);
    style.Colors[ImGuiCol_CheckMark] = studioAccent;
    style.Colors[ImGuiCol_SliderGrab] = studioAccent;
    style.Colors[ImGuiCol_SliderGrabActive] = studioAccentBright;
    style.Colors[ImGuiCol_Separator] = studioBorder;
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(.39f,.43f,.41f,1);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = studioAccent;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = studioAccent;
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_ResizeGripHovered] = studioAccentSoft;
    style.Colors[ImGuiCol_ResizeGripActive] = studioAccent;
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(.65f,.80f,.58f,.5f);
    style.FrameBorderSize = 0;
    style.ChildBorderSize = 1;
    style.WindowPadding = ImVec2(20,20);
    style.FramePadding = ImVec2(12,7);
    style.ItemSpacing = ImVec2(12,12);
    style.ItemInnerSpacing = ImVec2(10,6);
    style.ScrollbarSize = 7;
    style.WindowRounding = 24;
    style.ChildRounding = 18;
    style.FrameRounding = 10;
    style.PopupRounding = 12;
    style.GrabRounding = 8;
    style.DisabledAlpha = .48f;

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
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
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
    io.FontDefault = ui_font;
    font_heading = ui_font;
    for (const char* path : font_paths) {
        if (std::filesystem::exists(path)) {
            if (auto* heading = io.Fonts->AddFontFromFileTTF(path, 27.0f)) {
                font_heading = heading;
                break;
            }
        }
    }
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
