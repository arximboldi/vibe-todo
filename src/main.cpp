#include "state.hpp"         // State, Action, Reducer, Effects
#include "persistence.hpp"   // save_state, load_state, get_default_data_path

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

#include <lager/store.hpp>
#include <lager/watch.hpp>
#include <lager/event_loop/manual.hpp>

// Logging with spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // Or basic_stdout_sink
#include <spdlog/sinks/basic_file_sink.h>    // Optional: File logging

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

using namespace ftxui;

// --- UI Rendering --- (AppUI - Same as before, takes lager::store)
// Takes the lager::store to dispatch actions and get state
Component AppUI(lager::store<Action, AppState>& store) {

    // Input field component state (local to ftxui component)
    // Reads initial value from store, dispatches action on change/enter
    // NOTE: Using store.get() here might be slightly inefficient if called often.
    // For high-frequency updates, consider caching or a different binding.
    // Input component needs a mutable string pointer. We can't directly use
    // store.get().current_input. We need a temporary or manage it differently.

    // Let's manage input text purely via actions. Input displays store state.
    // Option 1: Update store on every keystroke (might be slow)
    // Option 2: Use ftxui local state for Input, dispatch AddTodoAction on Enter.

    // Let's try Option 2: Use ftxui's Input state, but update AppState only on Enter.
    // This requires AppState.current_input to be less tightly coupled to the Input field display.
    // Or, we use SetInputTextAction more carefully.
    // Let's stick to the pattern: Input reads from AppState, Enter dispatches Add.

    static auto new_todo_text_input = std::string{};
    auto input_component = Input(                       // component
        &new_todo_text_input,                  // data model -> string reference
        "New Todo Text",                             // placeholder
        InputOption{.on_change = [&] {          // on_change callback
                        // Dispatch action on *every* change
                        // Note: store.get().current_input is already updated by Input component
                        store.dispatch(SetInputTextAction{new_todo_text_input});
                    },
                   .on_enter = [&store] { store.dispatch(AddTodoAction{}); } // on_enter
                   });

    // --- Todo List Display (Using Menu) ---
    static int current_selection = 0;
    auto menu_options = ftxui::MenuOption::Vertical();
    // Use store.dispatch for menu actions
    menu_options.on_change = [&]() {
        store.dispatch(SelectTodoAction{current_selection});
    };
    menu_options.on_enter = [&]() {
        store.dispatch(ToggleSelectedTodoAction{});
    };

    // Renderer for the main layout
    // Captures the store to access current state via store.get()
    auto layout = Renderer([&](bool /* focused */) {
        const auto& state = store.get(); // Get current state snapshot from store

        std::vector<std::string> menu_entries;
        for (const auto& todo : state.todos) {
            menu_entries.push_back((todo.done ? "[x] " : "[ ] ") + todo.text);
        }

        // Need to pass a pointer to the *current* selected index from the state
        // Since state is immutable, we pass state.selected_index directly (by value copy).
        // If Menu requires a pointer, this pattern needs adjustment.
        // FTXUI Menu takes int* selected. This is problematic with immutable state.
        // Workaround: Create a temporary copy for Menu's use within this render cycle.
        // This is not ideal, as Menu might try to modify it.
        // Let's check if Menu can work with just `on_change`. Maybe `selected` pointer isn't strictly needed if we rely on `on_change`?
        // Okay, Menu *needs* the pointer for its internal state.
        // Hacky workaround: use a static or member variable in a class wrapper? No, keep it functional.
        // Best approach: Pass a *copy* of the index. The Menu component will update its internal visual state.
        // Our *actual* state update happens via dispatch(SelectTodoAction) triggered by on_change.
        auto todo_menu = Menu(&menu_entries, &current_selection, menu_options);

        // --- Buttons ---
        auto add_button = Button("Add", [&] { store.dispatch(AddTodoAction{}); });
        auto remove_button = Button("Remove Sel.", [&] { store.dispatch(RemoveSelectedTodoAction{}); });
        auto toggle_button = Button("Toggle Sel.", [&] { store.dispatch(ToggleSelectedTodoAction{}); });
        auto save_button = Button("Save", [&] { store.dispatch(RequestSaveAction{}); });
        auto quit_button = Button("Quit", [&] { store.dispatch(QuitAction{}); });

        auto buttons_bar = hbox({ /* ... buttons ... */ });

        return vbox({
                   text("TODO List Manager (Lager)") | bold | hcenter,
                   separator(),
                   todo_menu->Render() | vscroll_indicator | frame | flex_grow,
                   separator(),
                   hbox(text(" New: "), input_component->Render()),
                   separator(),
                   buttons_bar,
                   separator(),
                   text("Status: " + state.status_message) | dim
               }) | border;
    });

    // Container for focus handling
    return Container::Vertical({
        input_component,
        layout // Contains the Menu
    }) | CatchEvent([layout](Event event) {
        return layout->OnEvent(event);
    });
}


int main() {
    // --- Determine Paths FIRST ---
    // We need the data path to determine the log file path.
    std::filesystem::path data_path;
    std::filesystem::path log_file_path;
    try {
        data_path = Persistence::get_default_data_path();
        // Place log file in the same directory as the data file.
        log_file_path = data_path.parent_path() / "tui_todo_log.txt";

        // Ensure the directory exists (spdlog might do this, but let's be safe)
        if (!log_file_path.parent_path().empty() && !std::filesystem::exists(log_file_path.parent_path())) {
             std::filesystem::create_directories(log_file_path.parent_path());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error determining file paths: " << e.what() << std::endl;
        return 1;
    }


    // --- Logger Setup (spdlog) ---
    try {
        // Create a file sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path.string(), true); // true = truncate log on startup
        file_sink->set_level(spdlog::level::trace); // Log trace level and above to the file

        // Create a logger using the file sink
        auto file_logger = std::make_shared<spdlog::logger>("file_logger", file_sink);

        // Set this file logger as the default logger
        spdlog::set_default_logger(file_logger);

        spdlog::set_level(spdlog::level::trace); // Set global log level (must be <= sink level to pass)
        spdlog::flush_on(spdlog::level::trace); // Flush frequently, useful for TUI/debugging

        spdlog::info("--- Log Start ---");
        spdlog::info("Spdlog file logger initialized. Logging to: {}", log_file_path.string());

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return 1;
     } catch (const std::exception& e) { // Catch potential filesystem errors during sink creation
         std::cerr << "Log file sink creation failed: " << e.what() << std::endl;
         return 1;
    }

    spdlog::info("Application starting");

    // --- Persistence Path ---
    spdlog::info("Data file path: {}", data_path.string());
    initialize_persistence_path(data_path); // Set the path for effects


    // --- Initial State ---
    auto initial_state_opt = Persistence::load_state(data_path);
    AppState initial_state;
    if (initial_state_opt) {
        initial_state = *initial_state_opt;
        initial_state.status_message = "State loaded.";
        spdlog::info("Loaded initial state from disk");
    } else {
        initial_state = AppState{}; // Default initial state
        initial_state.status_message = "Ready (new list).";
        spdlog::info("No saved state found or error loading, starting fresh.");
    }
    initial_state.exit_requested = false;

    // --- FTXUI Screen ---
    auto screen = ScreenInteractive::Fullscreen();

    // --- Lager Store Setup ---
    auto store = lager::make_store<Action>(
        initial_state,
        lager::with_manual_event_loop{},
        lager::with_reducer(reducer)
        );

    // --- Connect Lager Store to FTXUI ---
    lager::watch(
        store,
        [&](AppState const& state) {
            if (state.exit_requested) {
                spdlog::info("Exit requested flag detected, stopping loop.");
                screen.Exit();
            } else {
                // Only trigger redraw if not exiting
                screen.PostEvent(Event::Custom);
            }
        });

    // --- Build UI ---
    auto ui = AppUI(store);

    // --- Run Event Loop ---
    spdlog::info("Starting UI loop");
    screen.Loop(ui);

    // --- Cleanup ---
    spdlog::info("Application finished cleanly");
    spdlog::shutdown(); // Flush and release logger resources
    return 0;
}
