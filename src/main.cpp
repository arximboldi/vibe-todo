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

Component AppUI(lager::store<Action, AppState>& store) {
    // Create component state that persists with the component
    auto input_buffer = std::make_shared<std::string>("");
    auto current_selection = std::make_shared<int>(-1);
    auto menu_entries = std::make_shared<std::vector<std::string>>();

    // Input component
    auto input_component = Input(
        input_buffer.get(),
        "New Todo Text",
        InputOption{
            .on_change = [&store, input_buffer] {
                store.dispatch(SetInputTextAction{*input_buffer});
            },
                .on_enter = [&store] {
                    store.dispatch(AddTodoAction{});
                }
                }
        );

    // Update menu entries based on current state
    auto& state = store.get();
    menu_entries->clear();
    for (const auto& todo : state.todos) {
        menu_entries->push_back((todo.done ? "[x] " : "[ ] ") + todo.text);
    }

    auto menu_options = MenuOption::Vertical();
    menu_options.on_change = [&store, current_selection] {
        store.dispatch(SelectTodoAction{*current_selection});
    };
    menu_options.on_enter = [&store] {
        store.dispatch(ToggleSelectedTodoAction{});
    };

    // Menu setup
    auto todo_menu = Menu(menu_entries.get(), current_selection.get(), menu_options);

    // Create the menu as a component
    auto menu_component = Renderer([&store, current_selection, todo_menu](bool focused) {
        // Add focus styling
        return todo_menu->Render() | frame | flex_grow;
    });

    // Add decorations and status display through a renderer

    // Buttons
    auto add_button = Button("Add", [&store] {
        store.dispatch(AddTodoAction{});
    });
    auto remove_button = Button("Remove", [&store] {
        store.dispatch(RemoveSelectedTodoAction{});
    });
    auto toggle_button = Button("Toggle", [&store] {
        store.dispatch(ToggleSelectedTodoAction{});
    });
    auto save_button = Button("Save", [&store] {
        store.dispatch(RequestSaveAction{});
    });
    auto load_button = Button("Load", [&store] {
        store.dispatch(RequestLoadAction{});
    });
    auto quit_button = Button("Quit", [&store] {
        store.dispatch(QuitAction{});
    });

    // Group buttons in a horizontal container
    auto button_container = Container::Horizontal({
            add_button, remove_button, toggle_button,
            save_button, load_button, quit_button
        });

    // Layout container with all focusable components
    auto main_container = Container::Vertical({
            input_component,   // Input field (focus 1)
            menu_component,    // Todo list (focus 2)
            button_container   // Buttons (focus 3)
        });

    return Renderer([&store, current_selection, menu_entries, main_container] {
        return vbox({
                text("TODO List Manager (Lager)") | bold | hcenter,
                separator(),
                main_container->Render(),  // Renders all components
                separator(),
                text("Status: " + store->status_message) | dim  // Status text (not focusable)
            }) | border;
    }) | CatchEvent([&store](Event event) {
        if (event == Event::Character('q')) {
            store.dispatch(QuitAction{});
            return true;
        }
        return false;  // Allow other events to pass through
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
