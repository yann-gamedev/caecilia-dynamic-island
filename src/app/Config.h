#pragma once
#include "spec/NotchIslandSpec.h"
#include <filesystem>
#include <string_view>

namespace caecilia {
struct Config {
    NotchIsland::UserConfig user{NotchIsland::kDefaultConfig};
    bool showDebugFps{NotchIsland::DebugHud::ShowDebugFpsDefault};
};
// Requires an initialized WinRT apartment. An empty path selects RoamingAppData.
Config LoadConfig(const std::filesystem::path& path = {});
Config ParseConfig(std::string_view utf8);
} // namespace caecilia
