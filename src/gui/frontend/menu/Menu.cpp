#include "Menu.hpp"

#include "core/engine/cache/Cache.hpp"
#include "core/input/MouseAim.hpp"
#include "core/engine/classes/SkinChanger.hpp"
#include "core/engine/classes/SkinDatabase.hpp"
#include "gui/renderer/Renderer.hpp" // Circular dependency
#include "gui/renderer/window/Window.hpp" // Circular dependency
#include "assets/fonts/Icons.h"
#include "assets/fonts/WeaponIcons.h"

#include <cmath>


bool Menu::Init() {
	return GetInstance().InitImpl();
}

void Menu::Render() {
	return GetInstance().RenderImpl();
}

void Menu::RenderStartupHelp() {
	return GetInstance().RenderStartupHelpImpl();
}

ImVec2 Menu::GetPos() {
	return GetInstance().pos;
}

ImVec2 Menu::GetSize() {
	return GetInstance().size;
}

bool Menu::InitImpl() {
	SetupStyles();

	LOGF(INFO, "Successfully initialized menu...");
	return true;
}

void Menu::RenderImpl() {
	if (!isSetup)
		return;

	auto& io = ImGui::GetIO();
	const auto screen = io.DisplaySize;
	static auto color_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;

#ifdef _DEBUG
	static auto title = "SourceSight [DEV]";
#else
	static auto title = "SourceSight";
#endif

	ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(640, 420), ImVec2(960, 760));
	ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

	if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse)) {
		this->pos = ImGui::GetWindowPos();
		this->size = ImGui::GetWindowSize();

		static int active_tab = 0;

		const bool main_split_visible = ImGui::BeginChild("##main_split", ImVec2(0, 0), ImGuiChildFlags_None);
		if (main_split_visible)
		{
			auto size = ImGui::GetContentRegionAvail();

			ImGui::BeginChild("##tab_buttons", ImVec2(148, size.y), ImGuiChildFlags_Borders);
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.08f, 0.5f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));
				for (const auto& tab : tabs)
				{
					bool is_active = (active_tab == tab.id);

					if (is_active)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.36f, 0.56f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.42f, 0.64f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.30f, 0.48f, 1.0f));
					}

					if (ImGui::Button((tab.icon + " " + tab.label).c_str(), ImVec2(-1, 28)))
						active_tab = tab.id;

					if (is_active) 
						ImGui::PopStyleColor(3);
				}
				ImGui::PopStyleVar(2);

				auto space = ImGui::GetContentRegionAvail().y;
				ImGui::SetCursorPosY(ImGui::GetCursorPos().y + std::max(0.0f, space - 76.0f));

				ImGui::Dummy(ImVec2(11, 0)); ImGui::SameLine();
				ImGui::TextLinkOpenURL(Icons::GITHUB, "https://github.com/jonahchang207/sourcesight-linux");
				ImGui::Separator();

				ImGui::TextDisabled("STATUS");
				ImGui::Checkbox("Overlay enabled", &cfg::enabled);
			}
			ImGui::EndChild();

			ImGui::SameLine();


			ImGui::BeginDisabled(!cfg::enabled);

			ImGui::BeginChild("##tab_content", ImVec2(0, size.y), ImGuiChildFlags_Borders);
			{
				if (active_tab == Tab::PLAYER)
				{
					ImGui::Text("Visuals");
					ImGui::Separator();

					ImGui::BeginGroup();
					{
						ImGui::Checkbox("Box", &cfg::esp::box);
						ImGui::BeginDisabled(!cfg::esp::box);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team box color", cfg::esp::colors::box_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy box color", cfg::esp::colors::box_enemy.data(), color_flags);
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
							ImGui::ColorEdit4("Team skeleton color", cfg::esp::colors::skeleton_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy skeleton color", cfg::esp::colors::skeleton_enemy.data(), color_flags);
							ImGui::SliderFloat("Skeleton thickness", &cfg::esp::skeleton_thickness, 1.0f, 4.0f, "%.1f");
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Head Tracker", &cfg::esp::head_tracker);
						ImGui::BeginDisabled(!cfg::esp::head_tracker);
						{
							ImGui::Checkbox("Filled Head Tracker", &cfg::esp::head_tracker_filled);
							ImGui::SliderFloat("Head tracker size", &cfg::esp::head_tracker_size, 2.0f, 14.0f, "%.1f");
							// One clearly labeled picker per team, on its own row, so the
							// team and enemy head colors are independently editable.
							ImGui::ColorEdit4("Team head color", cfg::esp::colors::tracker_team.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
							ImGui::ColorEdit4("Enemy head color", cfg::esp::colors::tracker_enemy.data(), color_flags & ~ImGuiColorEditFlags_NoLabel);
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Tracers", &cfg::esp::tracers);
						ImGui::BeginDisabled(!cfg::esp::tracers);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team tracer color", cfg::esp::colors::tracer_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy tracer color", cfg::esp::colors::tracer_enemy.data(), color_flags);
							ImGui::SliderFloat("Tracer thickness", &cfg::esp::tracer_thickness, 1.0f, 4.0f, "%.1f");
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Bullet Tracer", &cfg::esp::bullet_tracer::enabled);
						ImGui::SetItemTooltip(
							"Draws a line from the shooter's gun tip to the impact point (first\n"
							"player hit) whenever a shot is fired, keeping it visible for the\n"
							"configured duration. Only renders while the shooter is in your\n"
							"line of sight. Includes your own shots."
						);
						ImGui::BeginDisabled(!cfg::esp::bullet_tracer::enabled);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team bullet color", cfg::esp::bullet_tracer::team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy bullet color", cfg::esp::bullet_tracer::enemy.data(), color_flags);
							ImGui::SliderFloat("Bullet trace length", &cfg::esp::bullet_tracer::length, 50.0f, 1000.0f, "%.0f u");
							ImGui::SliderFloat("Bullet muzzle offset", &cfg::esp::bullet_tracer::muzzle_offset, 10.0f, 150.0f, "%.0f u");
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
						ImGui::SetItemTooltip("Uses CS2's spotted state. This is not a geometric line-of-sight check.");

						ImGui::Checkbox("Show Team", &cfg::esp::team);
						ImGui::Checkbox("Spotted only", &cfg::esp::spotted_only);
						ImGui::SetItemTooltip("Only render enemies you can actually see");
						ImGui::Checkbox("Distance", &cfg::esp::distance);
						ImGui::Checkbox("Headshot line", &cfg::esp::headshot_line);
						ImGui::SetItemTooltip("Line from crosshair to enemy head");
					}
					ImGui::EndGroup();

					//ImGui::SameLine();
					ImGui::Spacing();

					ImGui::Text("Flags");
					ImGui::Separator();

					ImGui::BeginGroup();
					{
						ImGui::Checkbox("Flashed", &cfg::esp::flags::flashed);
						ImGui::BeginDisabled(!cfg::esp::flags::flashed);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team flashed color", cfg::esp::colors::flags::flashed_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy flashed color", cfg::esp::colors::flags::flashed_enemy.data(), color_flags);
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Reloading", &cfg::esp::flags::reloading);
						ImGui::BeginDisabled(!cfg::esp::flags::reloading);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team reloading color", cfg::esp::colors::flags::reloading_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy reloading color", cfg::esp::colors::flags::reloading_enemy.data(), color_flags);
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Defusing", &cfg::esp::flags::defusing);
						ImGui::BeginDisabled(!cfg::esp::flags::defusing);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team defusing color", cfg::esp::colors::flags::defusing_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy defusing color", cfg::esp::colors::flags::defusing_enemy.data(), color_flags);
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Scoped", &cfg::esp::flags::scoped);
						ImGui::BeginDisabled(!cfg::esp::flags::scoped);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team scoped color", cfg::esp::colors::flags::scoped_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy scoped color", cfg::esp::colors::flags::scoped_enemy.data(), color_flags);
						}
						ImGui::EndDisabled();

						ImGui::Checkbox("Has C4", &cfg::esp::flags::has_c4);
						ImGui::BeginDisabled(!cfg::esp::flags::has_c4);
						{
							ImGui::SameLine();
							ImGui::ColorEdit4("Team C4 color", cfg::esp::colors::flags::c4_team.data(), color_flags);
							ImGui::SameLine();
							ImGui::ColorEdit4("Enemy C4 color", cfg::esp::colors::flags::c4_enemy.data(), color_flags);
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
				}
				else if (active_tab == Tab::WORLD)
				{
					ImGui::Text("Bomb");
					ImGui::Separator();
					{
						ImGui::Checkbox("Bomb ESP", &cfg::esp::bomb);
						ImGui::SameLine();
						ImGui::ColorEdit4("Bomb color", cfg::esp::colors::bomb.data(), color_flags);
					}
					ImGui::Checkbox("Bomb Location", &cfg::world::bomb::location);
					ImGui::Checkbox("Bomb Timer", &cfg::world::bomb::timer);

					ImGui::Spacing();

					ImGui::Text("Spectator list");
					ImGui::Separator();

					ImGui::Checkbox("Enable", &cfg::world::spectators::enabled);
					if (cfg::world::spectators::enabled) {
						ImGui::Checkbox("Detailed", &cfg::world::spectators::detailed);
						ImGui::Checkbox("Only Self", &cfg::world::spectators::self_only);
						ImGui::SetItemTooltip("Only display users spectating you");
					}

					ImGui::Spacing();

					ImGui::Text("Misc");
					ImGui::Separator();

					ImGui::Checkbox("Crosshair", &cfg::world::crosshair::enabled);
					ImGui::BeginDisabled(!cfg::world::crosshair::enabled);
					{
						ImGui::Checkbox("Snipers only", &cfg::world::crosshair::sniper_only);
						ImGui::Checkbox("Center dot", &cfg::world::crosshair::center_dot);
						ImGui::Checkbox("Outline", &cfg::world::crosshair::outline);
						ImGui::ColorEdit4("Crosshair color", cfg::world::crosshair::color.data(), color_flags);
						ImGui::SliderFloat("Crosshair gap", &cfg::world::crosshair::gap, 0.0f, 20.0f, "%.1f");
						ImGui::SliderFloat("Crosshair length", &cfg::world::crosshair::length, 1.0f, 20.0f, "%.1f");
						ImGui::SliderFloat("Crosshair thickness", &cfg::world::crosshair::thickness, 1.0f, 5.0f, "%.1f");
						if (cfg::world::crosshair::center_dot)
							ImGui::SliderFloat("Center dot size", &cfg::world::crosshair::center_dot_size, 0.5f, 5.0f, "%.1f");
						if (cfg::world::crosshair::outline)
							ImGui::SliderFloat("Outline thickness", &cfg::world::crosshair::outline_thickness, 0.5f, 3.0f, "%.1f");
					}
					ImGui::EndDisabled();

					ImGui::Checkbox("Velocity Graph", &cfg::world::velocity::enabled);
					ImGui::BeginDisabled(!cfg::world::velocity::enabled);
					{
						ImGui::SliderInt("Sample rate", &cfg::world::velocity::sample_rate, 1, 100);
						ImGui::SliderFloat("Sample length", &cfg::world::velocity::sample_length, 1.0f, 20.0f, "%.1f");
					}
					ImGui::EndDisabled();
					ImGui::Spacing();

					ImGui::Text("Radar");
					ImGui::Separator();

					ImGui::Checkbox("Radar", &cfg::world::radar::enabled);
					ImGui::BeginDisabled(!cfg::world::radar::enabled);
					{
						ImGui::SameLine();
						ImGui::SliderFloat("Range", &cfg::world::radar::range, 100.f, 8000.f, "%.0f u");
						ImGui::Checkbox("Disable Rotation", &cfg::world::radar::no_rotate);
					}
					ImGui::EndDisabled();
				}
				else if (active_tab == Tab::AIM)
				{
					ImGui::Text("Aim");
					ImGui::Separator();

					// Driver status
					if (MouseAim::DriverInstalled())					ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.56f, 1.0f),
						"Driver: ready");
					else
						ImGui::TextColored(ImVec4(0.92f, 0.34f, 0.34f, 1.0f),
							"Driver: MISSING");

					// Enable / Disable button
					ImGui::Spacing();
					if (!cfg::aim::enabled) {
						ImGui::PushStyleColor(ImGuiCol_Button,
							ImVec4(0.12f, 0.28f, 0.46f, 0.90f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
							ImVec4(0.16f, 0.34f, 0.54f, 0.95f));
						if (ImGui::Button("Enable Aim", ImVec2(-1, 32)))
							cfg::aim::enabled = true;
						ImGui::PopStyleColor(2);
					}
					else {
						ImGui::PushStyleColor(ImGuiCol_Button,
							ImVec4(0.38f, 0.12f, 0.16f, 0.90f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
							ImVec4(0.48f, 0.16f, 0.20f, 0.95f));
						if (ImGui::Button("Disable Aim", ImVec2(-1, 32)))
							cfg::aim::enabled = false;
						ImGui::PopStyleColor(2);
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
						ImGui::Separator();
						ImGui::Spacing();

						static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
						int tp = cfg::aim::target_part;
						if (ImGui::Combo("Target", &tp, target_parts, 4))
							cfg::aim::target_part = tp;

						ImGui::Spacing();

						// Weapon speed multipliers
						ImGui::Text("Weapon Speed");
						ImGui::SliderFloat("Rifle", &cfg::aim::rifle_mult, 0.2f, 3.0f, "x%.1f");
						ImGui::SliderFloat("Pistol", &cfg::aim::pistol_mult, 0.2f, 3.0f, "x%.1f");
						ImGui::SliderFloat("Sniper", &cfg::aim::sniper_mult, 0.2f, 3.0f, "x%.1f");
						ImGui::SliderFloat("SMG", &cfg::aim::smg_mult, 0.2f, 3.0f, "x%.1f");

						ImGui::Spacing();
						ImGui::SliderFloat("Recoil comp", &cfg::aim::recoil_compensation, 0.0f, 1.0f, "%.0f%%");
						ImGui::SliderFloat("Switch delay", &cfg::aim::target_switch_delay, 0.0f, 0.5f, "%.2fs");

						ImGui::Spacing();
						ImGui::Separator();
						ImGui::Spacing();

						float aim_w = 1920.0f;
						float aim_h = 1080.0f;
						MouseAim::ScreenSize(aim_w, aim_h);
						const float fsr =
							std::sqrt(aim_w * aim_w + aim_h * aim_h) * 0.5f;
						ImGui::SliderFloat("FOV radius", &cfg::aim::fov_radius, 0.0f,
							fsr, "%.0f px");
					}
					ImGui::EndDisabled();

					ImGui::Spacing();
					ImGui::TextWrapped(
						"F9: panic key (disables all). MB5: toggle aim."
					);
				}
				else if (active_tab == Tab::SKINS)
				{
					// ── Skin Changer: Grid Browser ─────────────────────────────────
					// Layout: category tabs on top, weapon grid on left, skin config on right.

					static int selected_weapon = WEAPON_AK47;
					static int selected_skin = 0;
					static float selected_wear = 0.0f;
					static int selected_seed = 0;
					static int selected_stattrak = -1;
					static bool stattrak_on = false;
					static int skin_category = 0; // 0=All, 1=Rifles, 2=SMGs, 3=Shotguns, 4=Snipers, 5=Pistols, 6=Knives

					// ── Category tabs ──────────────────────────────────────────────
					static const char* categories[] = { "All", "Rifles", "SMGs", "Shotguns", "Snipers", "Pistols", "Knives" };
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 5));
					for (int c = 0; c < 7; ++c) {
						if (c > 0) ImGui::SameLine(0, 4);
						bool active = (skin_category == c);
						if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.46f, 0.90f));
						if (ImGui::Button(categories[c])) skin_category = c;
						if (active) ImGui::PopStyleColor();
					}
					ImGui::PopStyleVar();
					ImGui::Spacing();

					// ── Weapon grid (left 60%) + Skin config (right 40%) ─────────
					const float region_w = ImGui::GetContentRegionAvail().x;
					const float grid_w = region_w * 0.58f;
					const float config_w = region_w - grid_w - 8.0f;

					ImGui::BeginChild("##skin_grid", ImVec2(grid_w, 0), ImGuiChildFlags_Borders);
					{
						// Build weapon list for current category
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
							{ WEAPON_SAWEDOFF,"Sawed-Off",    WeaponIcons::SAWEDOFF },
							{ WEAPON_NEGEV,   "Negev",       WeaponIcons::NEGEV },
							{ WEAPON_M249,    "M249",        WeaponIcons::M249 },
							{ WEAPON_DEAGLE,  "Desert Eagle", WeaponIcons::DEAGLE },
							{ WEAPON_USP,     "USP-S",       WeaponIcons::USP_SILENCER },
							{ WEAPON_GLOCK,   "Glock-18",    WeaponIcons::GLOCK },
							{ WEAPON_P250,    "P250",        WeaponIcons::P250 },
							{ WEAPON_TEC9,    "Tec-9",       WeaponIcons::TEC9 },
							{ WEAPON_FIVESEVEN,"Five-SeveN",  WeaponIcons::FIVESEVEN },
							{ WEAPON_ELITE,   "Dual Berettas",WeaponIcons::ELITE },
							{ WEAPON_CZ75,    "CZ75-Auto",   WeaponIcons::CZ75A },
							{ WEAPON_REVOLVER,"R8 Revolver",  WeaponIcons::REVOLVER },
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

						// Category filter: 0=All, 1=Rifles, 2=SMGs, 3=Shotguns, 4=Snipers, 5=Pistols, 6=Knives
						auto fits = [&](int id) -> bool {
							if (skin_category == 0) return true;
							if (skin_category == 6) return id >= 500;
							if (skin_category == 1) {
								return id==WEAPON_AK47||id==WEAPON_M4A4||id==WEAPON_M4A1S||
									id==WEAPON_AUG||id==WEAPON_SG553||id==WEAPON_FAMAS||
									id==WEAPON_GALIL;
							}
							if (skin_category == 2) {
								return id==WEAPON_MAC10||id==WEAPON_UMP45||id==WEAPON_MP7||
									id==WEAPON_MP9||id==WEAPON_P90||id==WEAPON_MP5SD||
									id==WEAPON_PPBIZON;
							}
							if (skin_category == 3) {
								return id==WEAPON_XM1014||id==WEAPON_NOVA||id==WEAPON_MAG7||
									id==WEAPON_SAWEDOFF||id==WEAPON_NEGEV||id==WEAPON_M249;
							}
							if (skin_category == 4) {
								return id==WEAPON_AWP||id==WEAPON_SSG08||id==WEAPON_G3SG1||
									id==WEAPON_SCAR20;
							}
							if (skin_category == 5) {
								return id==WEAPON_DEAGLE||id==WEAPON_USP||id==WEAPON_GLOCK||
									id==WEAPON_P250||id==WEAPON_TEC9||id==WEAPON_FIVESEVEN||
									id==WEAPON_ELITE||id==WEAPON_CZ75||id==WEAPON_REVOLVER||
									id==WEAPON_P2000;
							}
							return true;
						};

						// Render weapon grid
						const float card_w = 100.0f;
						const float card_h = 52.0f;
						const float spacing = 6.0f;
						const int cols = std::max(1, static_cast<int>((grid_w + spacing) / (card_w + spacing)));
						int col = 0;

						for (const auto& w : all_weapons) {
							if (!fits(w.id)) continue;

							if (col > 0) ImGui::SameLine(0, spacing);

							// Check if this weapon has an active skin
							const auto& all_skins = SkinDB::SkinsByWeapon();
							auto skin_it = SkinChanger::GetAll().find(w.id);
							bool has_skin = (skin_it != SkinChanger::GetAll().end() && skin_it->second.paint_kit > 0);

							bool is_sel = (selected_weapon == w.id);
							ImVec4 bg = is_sel
								? ImVec4(0.12f, 0.28f, 0.46f, 0.90f)
								: has_skin
									? ImVec4(0.08f, 0.20f, 0.12f, 0.85f)
									: ImVec4(0.09f, 0.11f, 0.16f, 0.85f);
							ImVec4 border = is_sel
								? ImVec4(0.26f, 0.56f, 0.92f, 0.80f)
								: has_skin
									? ImVec4(0.20f, 0.50f, 0.30f, 0.60f)
									: ImVec4(0.14f, 0.22f, 0.34f, 0.30f);

							ImGui::PushStyleColor(ImGuiCol_Button, bg);
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg.x + 0.04f, bg.y + 0.04f, bg.z + 0.06f, bg.w));
							ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

							// Card button: icon on left, name on right
							if (ImGui::Button(w.name, ImVec2(card_w, card_h))) {
								selected_weapon = w.id;
								selected_skin = 0;
							}

							ImGui::PopStyleVar();
							ImGui::PopStyleColor(2);

							// Draw icon overlay
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("%s%s", w.name, has_skin ? " (active)" : "");

							col = (col + 1) % cols;
						}
					}
					ImGui::EndChild();

					ImGui::SameLine(0, 8);

					// ── Skin config panel (right side) ─────────────────────────────
					ImGui::BeginChild("##skin_config", ImVec2(config_w, 0), ImGuiChildFlags_Borders);
					{
						const auto& winfo = SkinDB::Weapons().count(selected_weapon)
							? SkinDB::Weapons().at(selected_weapon)
							: SkinDB::WeaponInfo{ "Unknown", 0 };
						ImGui::Text("%s", winfo.display_name);
						ImGui::Separator();
						ImGui::Spacing();

						// Skin selector — per-weapon
						const auto& all_skins = SkinDB::SkinsByWeapon();
						const auto weapon_it = all_skins.find(selected_weapon);
						const auto& skins = (weapon_it != all_skins.end())
							? weapon_it->second
							: SkinDB::PopularSkins();
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

						// Wear
						ImGui::Text("Wear");
						ImGui::SliderFloat("##wear", &selected_wear, 0.0f, 1.0f, "%.3f");
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", SkinDB::WearName(selected_wear));

						// Seed
						ImGui::Text("Seed");
						ImGui::SliderInt("##seed", &selected_seed, 0, 1000);

						// StatTrak
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

						// Apply button
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.46f, 0.90f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.34f, 0.54f, 0.95f));
						if (ImGui::Button("Apply Skin", ImVec2(-1, 32))) {
							auto& skin = SkinChanger::Get(selected_weapon);
							skin.paint_kit = skins[selected_skin].paint_kit;
							skin.wear = selected_wear;
							skin.seed = selected_seed;
							skin.stattrak = selected_stattrak;
							SkinChanger::ForceUpdate();
						}
						ImGui::PopStyleColor(2);

						// Reset button
						if (ImGui::Button("Reset to Default", ImVec2(-1, 26))) {
							SkinChanger::Get(selected_weapon) = SkinOverride{};
						}

						ImGui::Spacing();

						// Active skins summary
						const auto& active = SkinChanger::GetAll();
						if (!active.empty()) {
							ImGui::Text("Active (%d)", (int)active.size());
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
					ImGui::Text("AWP Quickswitch");
					ImGui::Separator();

					ImGui::Checkbox("AWP Quickswitch", &cfg::macro::awp_quickswitch);
					ImGui::SetItemTooltip(
						"When the AWP is equipped and left-click is detected, automatically\n"
						"pulls out the knife (key 3), waits, then switches back (key 1)\n"
						"to cancel the bolt animation."
					);

					ImGui::BeginDisabled(!cfg::macro::awp_quickswitch);
					{
						ImGui::SliderInt("Switch delay (ms)", &cfg::macro::delay_ms, 20, 500);
						ImGui::Checkbox("All bolt-action weapons", &cfg::macro::auto_switch_all);
						ImGui::SetItemTooltip("Apply quick-switch to Scout, G3SG1, SCAR-20 too");
					}
					ImGui::EndDisabled();

					ImGui::Spacing();
					ImGui::TextWrapped(
						"Runs while you are alive and in-game, and never while the menu is open.\n"
						"The keys are injected at the OS level, so they reach the game even when\n"
						"CS2 has the mouse grabbed."
					);
				}
				else if (active_tab == Tab::SETTINGS)
				{
					ImGui::Text("Misc");
					ImGui::Separator();

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
					ImGui::SetItemTooltip("Let the CPU sleep to Free Resources\nNOTE: might cause performance issues in lower end computers!");
					ImGui::Checkbox("Panic key (F9)", &cfg::settings::panic_key);
					ImGui::SetItemTooltip("Press F9 to instantly disable all cheats");

					ImGui::Spacing();
					ImGui::Text("Bypass");
					ImGui::Separator();
					ImGui::Checkbox("Timing jitter", &cfg::bypass::timing_jitter);
					ImGui::SetItemTooltip("Randomise write timing to avoid detection");
					ImGui::Checkbox("Humanize movement", &cfg::bypass::humanize_movement);
					ImGui::SetItemTooltip("Add micro-noise to mouse deltas");
					ImGui::SliderInt("Write delay min (us)", &cfg::bypass::write_delay_min_us, 0, 500);
					ImGui::SliderInt("Write delay max (us)", &cfg::bypass::write_delay_max_us, 0, 1000);
					ImGui::SliderFloat("Noise amplitude", &cfg::bypass::noise_amplitude, 0.0f, 2.0f, "%.1f px");

					ImGui::Spacing();
					ImGui::Text("Notes");
					ImGui::Separator();
					ImGui::TextWrapped(
						"If you experience bad performance/lag try the following:\n"
						"\t- Disable ESP VSync: Look up > VSync: Un-Check\n"
						"\t- Disable VSync in game: ...Advanced Video > V-Sync: Disabled\n"
						"\t- Last resort: Disable the \"Free CPU\" option. It will impact overall performance but improve latency.\n"
					);

#ifdef _DEBUG
					ImGui::Text("Dev");
					ImGui::Separator();

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
#endif
				}
			}
			ImGui::EndChild();

			ImGui::EndDisabled();


		}
		ImGui::EndChild();
	}

	ImGui::End();
}

void Menu::SetupStyles() {
	ImGuiStyle& style = ImGui::GetStyle();

	// ── Sapphire Acrylic Glass palette ─────────────────────────────────
	// Deep navy backgrounds with frosted transparency, sapphire blue
	// accents, clean white text, and refined subtle borders.

	style.Colors[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.48f, 0.56f, 1.00f);

	// Window: deep navy with slight transparency for the glass feel
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.07f, 0.10f, 0.94f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.06f, 0.09f, 0.90f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.08f, 0.12f, 0.96f);

	// Borders: soft sapphire tint
	style.Colors[ImGuiCol_Border] = ImVec4(0.14f, 0.22f, 0.34f, 0.45f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

	// Frames (checkboxes, sliders, inputs): dark translucent with sapphire hover
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.11f, 0.16f, 0.85f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.18f, 0.28f, 0.90f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.22f, 0.38f, 0.95f);

	// Title bar: almost black with a hint of blue
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.08f, 0.12f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.04f, 0.04f, 0.06f, 0.60f);

	// Menu bar and scrollbars: subtle dark layers
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.95f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.05f, 0.07f, 0.55f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.14f, 0.18f, 0.25f, 0.70f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.24f, 0.34f, 0.80f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.22f, 0.30f, 0.42f, 0.90f);

	// Buttons: sapphire-tinted glass
	style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.14f, 0.22f, 0.85f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.22f, 0.34f, 0.92f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.28f, 0.46f, 0.98f);

	// Headers (tree nodes, selectable): frosted sapphire
	style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.16f, 0.26f, 0.80f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.14f, 0.22f, 0.34f, 0.88f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.12f, 0.26f, 0.42f, 0.95f);

	// Accent elements: sapphire blue
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.56f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.56f, 0.92f, 0.90f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.62f, 0.96f, 1.00f);

	// Separators: subtle sapphire line
	style.Colors[ImGuiCol_Separator] = ImVec4(0.12f, 0.18f, 0.28f, 0.50f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.22f, 0.34f, 0.50f, 0.60f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.46f, 0.72f, 0.80f);

	// Resize grip: transparent sapphire
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.56f, 0.92f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.56f, 0.92f, 0.40f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.56f, 0.92f, 0.70f);

	// Tabs: deep navy inactive, sapphire active
	style.Colors[ImGuiCol_Tab] = ImVec4(0.06f, 0.07f, 0.10f, 0.85f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.26f, 0.42f, 0.90f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.20f, 0.34f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.06f, 0.08f, 0.80f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08f, 0.10f, 0.15f, 0.90f);

	// Plots
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.36f, 0.46f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.40f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.56f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.36f, 0.66f, 0.96f, 1.00f);

	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.46f, 0.72f, 0.40f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.56f, 0.92f, 0.90f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.56f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.80f, 0.84f, 0.92f, 0.50f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.06f, 0.08f, 0.12f, 0.60f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.08f, 0.12f, 0.70f);

	// Refined spacing and rounding for the premium feel
	style.FrameBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.WindowPadding = ImVec2(14.0f, 14.0f);
	style.FramePadding = ImVec2(10.0f, 5.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	style.ScrollbarSize = 10.0f;

	// Smooth, rounded edges for the liquid glass aesthetic
	style.WindowRounding = 10.0f;
	style.ChildRounding = 8.0f;
	style.FrameRounding = 6.0f;
	style.PopupRounding = 8.0f;
	style.GrabRounding = 4.0f;

	auto& io = ImGui::GetIO();

	io.Fonts->Clear();
#ifdef _WIN32
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f);
#else
	if (!io.Fonts->AddFontFromFileTTF("/usr/share/fonts/TTF/DejaVuSans.ttf", 16.0f))
		io.Fonts->AddFontDefault();
#endif

	ImFontConfig merge_icon_cfg{};
	merge_icon_cfg.FontDataOwnedByAtlas = false;
	merge_icon_cfg.MergeMode = true;
	merge_icon_cfg.GlyphOffset = Vec2_t(0, 3.5f);

	// the icons will use the size specified when getting added so it ignores the base size
	static const ImWchar icon_ranges[] = { 0xE100, 0xE108, 0 };
	io.Fonts->AddFontFromMemoryTTF(icons_font, icons_font_len, 20.f, &merge_icon_cfg, icon_ranges);
}

void Menu::RenderStartupHelpImpl() {
	static bool has_opened_menu = false;

	if (has_opened_menu)
		return;

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

	d->AddText(
		ImVec2(screen.x / 2 - size.x / 2, 80),
		IM_COL32(255, 255, 255, 255),
		help
	);
}
