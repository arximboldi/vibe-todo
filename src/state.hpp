#pragma once

#include <immer/flex_vector.hpp>
#include <lager/effect.hpp>
#include <lager/context.hpp>
#include <string>
#include <variant>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem> // Needed by effects

// Include spdlog for logging within effects
#include <spdlog/spdlog.h>

// Forward declarations
#include "persistence.hpp" // For Persistence::save_state/load_state

// --- Data Structures ---
struct TodoItem {
    std::string text;
    bool done = false;
    bool operator==(const TodoItem&) const = default;
};

struct AppState {
    immer::flex_vector<TodoItem> todos;
    std::string current_input = "";
    int selected_index = -1;
    std::string status_message = "Ready";
    bool exit_requested = false; // Flag for clean exit

    bool operator==(const AppState&) const = default;
};

// --- Actions --- (Same as before)
struct SetInputTextAction { std::string text; };
struct AddTodoAction {};
struct RemoveSelectedTodoAction {};
struct ToggleSelectedTodoAction {};
struct SelectTodoAction { int index; };
struct RequestSaveAction {};
struct RequestLoadAction {};
struct LoadCompleteAction { std::optional<AppState> loaded_state; std::string message; }; // Optional state
struct SetStatusAction { std::string message; };
struct QuitAction {};

// SetInputTextAction, AddTodoAction, RemoveSelectedTodoAction, ToggleSelectedTodoAction,
// SelectTodoAction, RequestSaveAction, RequestLoadAction, LoadCompleteAction,
// SetStatusAction, QuitAction
using Action = std::variant<SetInputTextAction, AddTodoAction, RemoveSelectedTodoAction, ToggleSelectedTodoAction,
                            SelectTodoAction, RequestSaveAction, RequestLoadAction, LoadCompleteAction,
                            SetStatusAction, QuitAction>;

// --- Effect Type Alias ---
using AppEffect = lager::effect<Action>;

// --- Reducer --- (Same signature and logic as before)
inline std::pair<AppState, AppEffect> reducer(AppState current_state, const Action& action); // Implementation below or in .cpp

// --- Effect Implementations ---
// Now use spdlog directly. Need data_path.

// We need a way for effects to know the data_path. Let's make it a static
// variable within this translation unit, initialized from main.
namespace { // Anonymous namespace to limit scope
inline std::filesystem::path global_data_path;
}

inline void initialize_persistence_path(const std::filesystem::path& path) {
    global_data_path = path;
    spdlog::debug("Persistence path initialized: {}", global_data_path.string());
}

inline AppEffect save_effect(AppState state_to_save) {
    return [state_to_save](lager::context<Action> ctx) {
        if (global_data_path.empty()) {
            spdlog::error("Save effect failed: Data path not initialized!");
            ctx.dispatch(SetStatusAction{"ERROR: Save path not configured."});
            return;
        }
        spdlog::debug("Executing save effect to {}", global_data_path.string());
        bool success = Persistence::save_state(global_data_path, state_to_save);
        std::string msg = success ? "State saved successfully." : "ERROR saving state!";
        if(success) spdlog::info("Save successful."); else spdlog::error("Save failed.");
        ctx.dispatch(SetStatusAction{msg});
    };
}

inline AppEffect load_effect() {
    return [](lager::context<Action> ctx) {
        if (global_data_path.empty()) {
            spdlog::error("Load effect failed: Data path not initialized!");
            ctx.dispatch(LoadCompleteAction{std::nullopt, "ERROR: Load path not configured."});
            return;
        }
        spdlog::debug("Executing load effect from {}", global_data_path.string());
        auto loaded_state_opt = Persistence::load_state(global_data_path);
        std::string msg;
        if (loaded_state_opt) {
            msg = "State loaded successfully.";
            spdlog::info("Load successful.");
        } else {
            msg = "ERROR loading state or file not found.";
            spdlog::warn("Load failed or file not found.");
        }
        ctx.dispatch(LoadCompleteAction{loaded_state_opt, msg});
    };
}


// --- Reducer Implementation ---
inline std::pair<AppState, AppEffect> reducer(AppState current_state, const Action& action) {
    // Use lager::match for action handling
    return lager::match(action)(
        // Each lambda handles one action type
        [&](SetInputTextAction act) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            next_state.current_input = act.text;
            return {std::move(next_state), lager::noop};
        },
        [&](AddTodoAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            if (!next_state.current_input.empty()) {
                next_state.todos = next_state.todos.push_back({next_state.current_input, false});
                next_state.current_input = "";
                next_state.selected_index = next_state.todos.size() - 1;
                next_state.status_message = "Todo added.";
            } else {
                next_state.status_message = "Input is empty.";
            }
            return {std::move(next_state), lager::noop};
        },
        [&](RemoveSelectedTodoAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            if (next_state.selected_index >= 0 && next_state.selected_index < next_state.todos.size()) {
                size_t index_to_remove = static_cast<size_t>(next_state.selected_index);
                next_state.todos = next_state.todos.erase(index_to_remove);
                if (next_state.todos.empty()) {
                    next_state.selected_index = -1;
                } else if (next_state.selected_index >= next_state.todos.size()) {
                    next_state.selected_index = next_state.todos.size() - 1;
                }
                next_state.status_message = "Todo removed.";
            } else {
                next_state.status_message = "No item selected to remove.";
            }
            return {std::move(next_state), lager::noop};
        },
        [&](ToggleSelectedTodoAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            if (next_state.selected_index >= 0 && next_state.selected_index < next_state.todos.size()) {
                size_t index_to_toggle = static_cast<size_t>(next_state.selected_index);
                TodoItem updated_item = next_state.todos[index_to_toggle];
                updated_item.done = !updated_item.done;
                next_state.todos = next_state.todos.set(index_to_toggle, updated_item);
                next_state.status_message = "Todo toggled.";
            } else {
                next_state.status_message = "No item selected to toggle.";
            }
            return {std::move(next_state), lager::noop};
        },
        [&](SelectTodoAction act) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            if (act.index >= -1 && act.index < next_state.todos.size()) {
                next_state.selected_index = act.index;
            }
            return {std::move(next_state), lager::noop};
        },
        // --- Effects ---
        [&](RequestSaveAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            next_state.status_message = "Saving...";
            // Pass the state /to be saved/ to the effect creator
            return {std::move(next_state), save_effect(current_state)};
        },
        [&](RequestLoadAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            next_state.status_message = "Loading...";
            return {std::move(next_state), load_effect()};
        },
        [&](LoadCompleteAction act) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            if(act.loaded_state) {
                next_state.todos = act.loaded_state->todos;
                next_state.selected_index = next_state.todos.empty() ? -1 : 0;
            }
            next_state.status_message = act.message;
            return {std::move(next_state), lager::noop};
        },
        // --- Other ---
        [&](SetStatusAction act) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            next_state.status_message = act.message;
            return {std::move(next_state), lager::noop};
        },
        [&](QuitAction) -> std::pair<AppState, AppEffect> {
            AppState next_state = current_state;
            next_state.exit_requested = true;
            next_state.status_message = "Exiting...";
            return {std::move(next_state), lager::noop};
        }
        ); // End lager::match
}
