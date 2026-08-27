#include "Cache.hpp"

#include "core/engine/Engine.hpp" // Circular dep
#include "core/offsets/Dumper.hpp"
#include "core/engine/classes/MapRaytrace.hpp"

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

	// Auto-reload map geometry when the map changes
	if (globals.map_name[0] != '\0') {
		static std::string last_map;
		std::string current_map(globals.map_name);
		if (current_map != last_map) {
			MapRaytrace::LoadMap(current_map);
			last_map = current_map;
		}
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
