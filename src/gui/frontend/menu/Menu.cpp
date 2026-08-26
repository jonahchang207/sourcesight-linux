#include "Menu.hpp"

#include "core/engine/cache/Cache.hpp"
#include "core/input/MouseAim.hpp"
#include "gui/renderer/Renderer.hpp" // Circular dependency
#include "gui/renderer/window/Window.hpp" // Circular dependency
#include "assets/fonts/Icons.h"

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

						ImGui::Spacing();
						ImGui::Separator();
						ImGui::Spacing();

						// Body part target selector
						static const char* target_parts[] = { "Head", "Body", "Legs", "Neck / Mid-body" };
						int tp = cfg::aim::target_part;
						if (ImGui::Combo("Target", &tp, target_parts, 4))
							cfg::aim::target_part = tp;

						ImGui::Spacing();

						// FOV radius
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
						"Only enable in environments where input automation is allowed."
					);
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
