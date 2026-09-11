#include "common.hpp"
#include "config/Config.hpp"
#include "config/Current.hpp"
#include "core/diagnostics/Diagnostics.hpp"
#include "core/engine/cache/Cache.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

nlohmann::json read_json(const std::filesystem::path& path) {
    std::ifstream input(path);
    nlohmann::json result;
    input >> result;
    return result;
}

void write_json(const std::filesystem::path& path, const nlohmann::json& value) {
    std::ofstream output(path, std::ios::trunc);
    output << value.dump(2) << '\n';
}
} // namespace

int main() {
    LogHelper::Init();
    const auto root = std::filesystem::current_path();
    const auto profiles = root / "configs";

    cfg::enabled = false;
    cfg::esp::bullet_tracer::length = 2048.f;
    cfg::world::radar::calibration_height = 1024.f;
    require(Config::SaveProfile("roundtrip"), "initial save");
    const auto roundtrip = profiles / "roundtrip.json";
    auto saved = read_json(roundtrip);
    require(saved.value("schema_version", -1) == Config::SchemaVersion(), "schema version written");
    saved["unknown_extension"] = { {"preserved", true} };
    write_json(roundtrip, saved);
    cfg::enabled = true;
    cfg::world::radar::calibration_height = 0.f;
    require(Config::LoadProfile("roundtrip"), "round trip load");
    require(cfg::world::radar::calibration_height == 1024.f, "radar resolution calibration persists");
    require(!cfg::enabled, "round trip applies stored setting");
    require(Config::Write(), "round trip rewrite");
    require(read_json(roundtrip)["unknown_extension"]["preserved"].get<bool>(), "unknown field preserved");
    require(std::filesystem::exists(roundtrip.string() + ".bak"), "previous-good profile backup created");

    const auto corrupt = profiles / "corrupt.json";
    write_json(corrupt, read_json(roundtrip));
    const std::string before_corruption = [] (const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    }(corrupt);
    { std::ofstream output(corrupt, std::ios::trunc); output << "{ truncated"; }
    cfg::enabled = true;
    require(!Config::LoadProfile("corrupt"), "corrupt profile rejected");
    require(cfg::enabled, "corrupt profile does not partially apply globals");
    require(Config::LastError().code == Config::ErrorCode::ParseFailed, "corrupt profile error exposed");
    require(std::filesystem::file_size(corrupt) < before_corruption.size(), "corrupt profile is not overwritten");

    auto missing = read_json(roundtrip);
    missing.erase("world");
    missing.erase("macro");
    write_json(profiles / "missing.json", missing);
    require(Config::LoadProfile("missing"), "missing optional sections migrate");
    require(Config::GetActiveProfile() == "missing", "active profile only changes after load success");

    auto future = read_json(roundtrip);
    future["schema_version"] = Config::SchemaVersion() + 1;
    write_json(profiles / "future.json", future);
    cfg::enabled = true;
    require(!Config::LoadProfile("future"), "future profile rejected");
    require(cfg::enabled, "future profile leaves globals untouched");
    require(Config::LastError().code == Config::ErrorCode::FutureSchema, "future schema error exposed");

    future["schema_version"] = 4294967296ULL;
    write_json(profiles / "future-overflow.json", future);
    require(!Config::LoadProfile("future-overflow"), "oversized future schema rejected");
    require(Config::LastError().code == Config::ErrorCode::FutureSchema,
            "oversized schema cannot wrap into a legacy migration");

    auto bounded = read_json(roundtrip);
    bounded["esp"]["bullet_tracer"]["length"] = -9999.f;
    bounded["esp"]["wireframe_budget"] = 999999;
    write_json(profiles / "bounded.json", bounded);
    require(Config::LoadProfile("bounded"), "bounded profile loads");
    require(cfg::esp::bullet_tracer::length == 50.f && cfg::esp::wireframe_budget == 8000,
            "numeric values are clamped to supported boundaries");

    require(Config::SaveProfile("write-failure"), "write failure fixture");
    const auto write_failure = profiles / "write-failure.json";
    std::ifstream original_file(write_failure, std::ios::binary);
    const std::string original(std::istreambuf_iterator<char>(original_file), {});
    Config::SetWriteFailureForTesting(true);
    require(!Config::SaveProfile("write-failure"), "injected write failure reported");
    Config::SetWriteFailureForTesting(false);
    std::ifstream after_file(write_failure, std::ios::binary);
    require(std::string(std::istreambuf_iterator<char>(after_file), {}) == original,
            "failed write preserves old profile");

    require(Cache::PublishForTesting({}), "fresh cache test frame ready");
    require(Cache::Status().ready(), "fresh cache status ready");
    require(!Cache::PublishForTesting({.age = 251ms}), "stale cache test frame unusable");
    require(Diagnostics::Snapshot().cache.state == "stale", "diagnostics reports stale state");
    require(!Cache::PublishForTesting({.in_match = false}), "no match unusable");
    require(Diagnostics::Snapshot().cache.state == "no_match", "diagnostics reports no match");
    require(Cache::PublishForTesting({.local_present = false}), "spectator missing-local frame remains usable");
    require(Diagnostics::Snapshot().cache.state == "missing_local", "diagnostics reports missing local");
    require(!Cache::PublishForTesting({.local_present = false, .age = 251ms}), "stale missing-local frame unusable");
    require(Diagnostics::Snapshot().cache.state == "stale", "diagnostics reports stale missing local");
    require(!Cache::PublishForTesting({.process_available = false}), "disconnect unusable");
    require(Diagnostics::Snapshot().cache.state == "process_unavailable", "diagnostics reports disconnect");

    Diagnostics::SetTimings(1.25, std::nullopt);
    const auto report = Diagnostics::BuildSanitizedJson();
    require(report.find("player") == std::string::npos && report.find("configs") == std::string::npos,
            "diagnostic report is redacted");
    std::string export_error;
    const auto report_path = root / "diagnostics.json";
    require(Diagnostics::ExportSanitizedReport(report_path.string(), export_error), "explicit diagnostic export");
    require(!Diagnostics::ExportSanitizedReport(report_path.string(), export_error), "diagnostic export never overwrites");
    LogHelper::Destroy();
    std::cout << "PASS: profile validation, backups, write failures, lifecycle and diagnostic redaction\n";
    return 0;
}
