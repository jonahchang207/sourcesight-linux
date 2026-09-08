#include "Cache.hpp"

#include "core/engine/Engine.hpp" // Circular dep
#include "core/offsets/Dumper.hpp"
#include "core/engine/classes/MapRaytrace.hpp"
#include "core/engine/classes/MapExtractor.hpp"
#include <future>

bool Cache::Refresh() {
    return Get().RefreshImpl();
}

Snapshot Cache::CopySnapshot() {
    std::lock_guard<std::mutex> lock(Get().mtx);
    return {
        Get().game,
        Get().bomb,
        Get().local,
        Get().globals,
        Get().players
    };
}

bool Cache::RefreshImpl() {
    auto p = Engine::GetProcess();
    auto client = Engine::GetClient();

    if (!p)
        return false;

    std::lock_guard<std::mutex> lock(mtx);
    auto now = steady_clock::now();

    // Without this, we are pointless :c
    // This calls game.UpdateMatrix(), which must be updated as fast as possible.
    if (!game.Update())
        return false;

#ifdef _DEBUG
    // Testing performance
    if (now - last < (cfg::dev::cache_refresh_rate * 1ms)) 
        return true;
#else
    // Just refresh every 5ms good for most people
    if (now - last < 5ms) 
        return true; // All good
#endif

    game.UpdateEntityList();
    globals.Update();
    bomb.Update();

    // Extraction and BVH construction must never run under the cache mutex.
    // One worker at a time; failed maps retry without logging every cache tick.
    static std::future<bool> map_job;
    static std::string requested_map;
    static auto retry_at = steady_clock::time_point{};
    const std::string current_map = globals.in_match
        ? std::filesystem::path(std::string(globals.map_name,
              strnlen(globals.map_name, sizeof(globals.map_name)))).stem().string() : "";
    if (requested_map != current_map) {
        requested_map = current_map;
        MapRaytrace::SetDesiredMap(current_map);
        retry_at = now;
    }
    if (map_job.valid() && map_job.wait_for(0ms) == std::future_status::ready) {
        map_job.get();
    }
    if (!map_job.valid() && !current_map.empty() && !MapRaytrace::IsReady() && now >= retry_at) {
        retry_at = now + 30s;
        map_job = std::async(std::launch::async, [current_map] {
            try { return MapExtractor::EnsureMapLoaded(current_map); }
            catch (const std::exception& e) {
                LOGF(WARNING, "[raytrace] map load failed: {}", e.what());
                return false;
            }
        });
    }

    // Aggressive diagnostics: log on FIRST tick, then every ~3 seconds
    static int cache_tick = 0;
    cache_tick++;
    static auto last_cache_diag = steady_clock::now();
    auto now_cache_diag = steady_clock::now();
    bool log_now = (cache_tick <= 5) || (now_cache_diag - last_cache_diag > 3s);
    if (log_now) {
        last_cache_diag = now_cache_diag;
        LOGF(INFO, "[cache] tick={} el=0x{:X} le=0x{:X} mc={} map='{}' c4=0x{:X}",
             cache_tick, game.entity_list, game.list_entry, globals.max_clients,
             globals.map_name, bomb.carrier);
    }

    std::vector<Player> scan;
    scan.reserve(globals.max_clients);
    int failed_updates = 0;
    for (int i = 0; i < globals.max_clients; i++) {
        auto player = Player(i, game.entity_list, game.list_entry);

        if (!player.Update()) {
            failed_updates++;
            continue;
        }
        if (player.localplayer)
            this->local = player;

        player.has_c4 = bomb.carrier != 0 && player.pawn_controller_addr == bomb.carrier;

        scan.push_back(player);
    }
    if (log_now) {
        LOGF(INFO, "[cache] players: {} succeeded, {} failed, max_clients={}"
             , scan.size(), failed_updates, globals.max_clients);
    }
	players = std::move(scan);
	duration = duration_cast<std::chrono::milliseconds>(steady_clock::now() - now);
	last = now;

	return true;
}
