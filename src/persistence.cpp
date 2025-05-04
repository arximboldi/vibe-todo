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

// --- JSON Serialization Helpers ---

// How to serialize/deserialize a single TodoItem
// Place this *before* it's used by AppState's functions
void to_json(nlohmann::json& j, const TodoItem& item) {
    j = nlohmann::json{{"text", item.text}, {"done", item.done}};
}

void from_json(const nlohmann::json& j, TodoItem& item) {
    j.at("text").get_to(item.text);
    j.at("done").get_to(item.done);
}

// How to serialize/deserialize AppState (specifically its todos)
void to_json(nlohmann::json& j, const AppState& state) {
    // Convert immer::flex_vector to std::vector for serialization
    std::vector<TodoItem> todos_vec(state.todos.begin(), state.todos.end());
    j = nlohmann::json{{"todos", todos_vec}}; // Only save todos
}

void from_json(const nlohmann::json& j, AppState& state) {
    // Check if "todos" key exists and is an array
    if (j.contains("todos") && j["todos"].is_array()) {
        // Deserialize into a std::vector first
        std::vector<TodoItem> todos_vec = j.at("todos").get<std::vector<TodoItem>>();
        // Convert std::vector to immer::flex_vector
        state.todos = immer::flex_vector<TodoItem>(todos_vec.begin(), todos_vec.end());
    } else {
        // Use spdlog eventually (warning)
        std::cerr << "Warning: State file format invalid or missing 'todos'. Loading empty list." << std::endl;
        state.todos = immer::flex_vector<TodoItem>{}; // Ensure it's empty
    }
    // Reset other fields to default or sensible values upon loading
    state.current_input = "";
    state.selected_index = state.todos.empty() ? -1 : 0;
    state.status_message = "State loaded."; // Updated status
    state.exit_requested = false;
}


namespace Persistence {

// --- Persistence Logic ---

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
        data_dir = std::filesystem::current_path() / app_name; // Fallback
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
        data_dir = std::filesystem::current_path() / app_name; // Fallback
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
             config_dir = std::filesystem::current_path(); // Fallback
        }
    }
    data_dir = config_dir / app_name;
#endif

    // Create directory if it doesn't exist
    try {
        if (!std::filesystem::exists(data_dir)) {
             std::filesystem::create_directories(data_dir);
        }
    } catch (const std::exception& e) {
        // Use spdlog here eventually instead of cerr
        std::cerr << "Error creating data directory " << data_dir.string() << ": " << e.what() << std::endl;
        // Fallback to current directory if creation fails
        data_dir = std::filesystem::current_path(); // Use current dir, not inside app_name subdir
    }

    return data_dir / filename;
}

bool save_state(const std::filesystem::path& path, const AppState& state) {
    try {
        nlohmann::json j = state; // Use the defined to_json function

        std::ofstream ofs(path);
        if (!ofs) {
            // Use spdlog eventually
            std::cerr << "Error opening file for writing: " << path << std::endl;
            return false;
        }
        ofs << j.dump(4); // Pretty print with 4 spaces
        return true;
    } catch (const nlohmann::json::exception& e) {
        // Use spdlog eventually
        std::cerr << "JSON serialization error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        // Use spdlog eventually
        std::cerr << "Error saving state to " << path << ": " << e.what() << std::endl;
        return false;
    }
}

std::optional<AppState> load_state(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        // Use spdlog eventually (info level)
         // Not necessarily an error if the file doesn't exist on first run
        // std::cerr << "State file not found: " << path << std::endl;
        return std::nullopt; // File doesn't exist, return empty optional
    }

    try {
        std::ifstream ifs(path);
        if (!ifs) {
            // Use spdlog eventually
            std::cerr << "Error opening file for reading: " << path << std::endl;
            return std::nullopt;
        }

        nlohmann::json j;
        ifs >> j;

        // Use the defined from_json function
        AppState loaded_state = j.get<AppState>();

        return loaded_state;

    } catch (const nlohmann::json::parse_error& e) {
         // Use spdlog eventually
        std::cerr << "JSON parsing error: " << e.what() << " at byte " << e.byte << std::endl;
        return std::nullopt; // Return empty optional on parsing error
    } catch (const nlohmann::json::exception& e) { // Catch other json errors (e.g., type errors)
        // Use spdlog eventually
        std::cerr << "JSON processing error: " << e.what() << std::endl;
        return std::nullopt;
    } catch (const std::exception& e) {
        // Use spdlog eventually
        std::cerr << "Error loading state from " << path << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace Persistence
