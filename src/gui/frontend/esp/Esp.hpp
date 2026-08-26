#pragma once

#include <unordered_map>

#include "core/engine/cache/Cache.hpp"

class Esp {
public:
    ~Esp() = default;
    Esp(const Esp&) = delete;
    Esp(Esp&&) = delete;
    Esp& operator=(const Esp&) = delete;
    Esp& operator=(Esp&&) = delete;

    static bool Init();
    static void Render();

private:
    ImGuiIO io;
    ImFont* font;
    ImFont* font_merged_icons;
    ImDrawList* d;

    // Temporary storage for ease
    view_matrix_t matrix;
private:
    Esp() {};

    static Esp& GetInstance()
    {
        static Esp i{};
        return i;
    }

    bool InitImpl();
    void RenderImpl();

    void RenderPlayer(Player player, bool mate = false);
    void RenderPlayerBones(Player player, bool mate = false);
    void RenderPlayerBars(Player player, std::pair<Vec2_t, Vec2_t> bounds);
    void RenderPlayerFlags(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate = false);
    void RenderPlayerTracker(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate = false);
    void RenderPlayerTracers(Player source, Player player, bool mate = false);
    void RenderBulletTracers(Player player, const std::vector<Player>& players, bool mate = false);
    
    void RenderBombBox(Bomb bomb);
	void RenderCrosshair(Player local);
	void RenderAimFov();

    // One glowing line per fired shot: from the shooter's gun tip to the
    // collision point, frozen at fire time and kept visible for the configured
    // duration. Only rendered while the shot origin is in our line of sight.
    struct BulletTracer {
        Vec3_t origin;    // world-space start: the shooter's gun tip
        Vec3_t end;       // world-space impact: first player hit, or max trace
        float start_time; // ImGui::GetTime() when the shot was detected
        bool mate;        // same team as local -> team color
        bool from_local;  // own shot -> always in our line of sight
    };

    // player index -> (last clip ammo, weapon id): a clip-ammo decrease means
    // a shot was fired (m_iClip1 is a verified offset, unlike the drifted
    // subclass shot-time fields). The weapon id guards against weapon switches.
    std::unordered_map<int, std::pair<int32_t, short>> last_ammo;
    std::vector<BulletTracer> tracers;
};
