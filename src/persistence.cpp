#include "persistence.hpp"
#include "state.hpp" // Needs TodoItem definition for JSON
#include <nlohmann/json.hpp> // JSON library
#include <fstream>
#include <iostream> // For error reporting (can replace with logger later)

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h> // For SHGetFolderPath
#else // Linux, macOS
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace Persistence {

// JSON Serialization for TodoItem
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TodoItem, text, done)
// JSON Serialization for AppState (only saving/loading todos)
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AppState, todos) // This would work if AppState only had todos

std::filesystem::path get_default_data_path() {
    std::filesystem::path data_dir;
    const std::string app_name = "TuiTodoCpp";
    const std::string filename = "todos.json";

#ifdef _WIN32
    // Windows: %APPDATA%\TuiTodoCpp\todos.json
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        data_dir = std::filesystem::path(path) / app_name;
    } else {
        // Fallback or error handling
        data_dir = std::filesystem::current_path() / app_name;
    }
#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/TuiTodoCpp/todos.json
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd) home = pwd->pw_dir;
    }
    if (home) {
        data_dir = std::filesystem::path(home) / "Library" / "Application Support" / app_name;
    } else {
        // Fallback
        data_dir = std::filesystem::current_path() / app_name;
    }
#else // Linux (and other Unix-like)
    // Linux: ~/.config/TuiTodoCpp/todos.json or $XDG_CONFIG_HOME/TuiTodoCpp/todos.json
    const char* config_home = getenv("XDG_CONFIG_HOME");
    std::filesystem::path config_dir;
    if (config_home) {
        config_dir = config_home;
    } else {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pwd = getpwuid(getuid());
            if (pwd) home = pwd->pw_dir;
        }
        if (home) {
            config_dir = std::filesystem::path(home) / ".config";
        } else {
             // Fallback
             config_dir = std::filesystem::current_path();
        }
    }
    data_dir = config_dir / app_name;
#endif

    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(data_dir);
    } catch (const std::exception& e) {
        // Consider logging this error instead of cerr
        std::cerr << "Error creating directory " << data_dir << ": " << e.what() << std::endl;
        // Use current directory as fallback for the file path itself
        return std::filesystem::current_path() / filename;
    }

    return data_dir / filename;
}


bool save_state(const std::filesystem::path& path, const AppState& state) {
    nlohmann::json j;
    // Manually serialize the vector part of the state
    j["todos"] = state.todos; // Uses NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE for TodoItem

    try {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(4); // Pretty print JSON
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving state to " << path << ": " << e.what() << std::endl;
        return false;
    }
}

std::optional<AppState> load_state(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt; // File doesn't exist, return default state later
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) return std::nullopt;

        nlohmann::json j = nlohmann::json::parse(file);

        AppState loaded_state; // Start with a default state
        // Manually deserialize the vector part
        if (j.contains("todos")) {
            loaded_state.todos = j.at("todos").get<immer::vector<TodoItem>>();
        }
        // Other parts of AppState (like current_input, selected_index)
        // will remain default unless also saved/loaded. We reset them here.
        loaded_state.selected_index = loaded_state.todos.empty() ? -1 : 0; // Select first item if list not empty

        return loaded_state;

    } catch (const std::exception& e) {
        std::cerr << "Error loading or parsing state from " << path << ": " << e.what() << std::endl;
        return std::nullopt; // Error loading, return default state later
    }
}

} // namespace Persistence
