#pragma once

#include <filesystem> // Requires C++17
#include <optional>
#include <string>

struct AppState;

// Function declarations
namespace Persistence {
std::filesystem::path get_default_data_path();
bool save_state(const std::filesystem::path& path, const AppState& state);
std::optional<AppState> load_state(const std::filesystem::path& path);
} // namespace Persistence
