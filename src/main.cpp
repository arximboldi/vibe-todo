#include "state.hpp"         // State, Action, Reducer, Effects
#include "persistence.hpp"   // save_state, load_state, get_default_data_path

#include <imtui/imtui.h>
#include <imtui/imtui-impl-ncurses.h>

#include <lager/store.hpp>
#include <lager/watch.hpp>
#include <lager/event_loop/manual.hpp>

// Logging with spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

void renderUI(lager::store<Action, AppState>& store) {
    auto& state = store.get();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    // Remove the default focus rectangle by customizing style
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    ImGui::Begin("TODO List Manager (Lager)", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // Title
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "TODO List Manager (Lager)");
    ImGui::Separator();

    // Preserve input text between focus changes
    static std::string preserved_input = state.current_input;

    // Input field
    static char input_buffer[1024];
    strncpy(input_buffer, preserved_input.c_str(), sizeof(input_buffer) - 1);
    input_buffer[sizeof(input_buffer) - 1] = '\0';

    ImGui::Text("New Todo:");

    // Make input field stand out when focused
    bool input_focused = ImGui::IsItemFocused() || ImGui::IsItemActive();
    if (input_focused) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    }

    if (ImGui::InputText("##input", input_buffer, sizeof(input_buffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        // When Enter is pressed in the input field, update state and add todo
        store.dispatch(SetInputTextAction{input_buffer});
        store.dispatch(AddTodoAction{});
        preserved_input = ""; // Clear after adding
        input_buffer[0] = '\0';
    } else {
        // Just update our preserved input without dispatching action
        preserved_input = input_buffer;
    }
    if (input_focused) {
        ImGui::PopStyleColor();
    }

    input_focused = ImGui::IsItemFocused() || ImGui::IsItemActive();

    // Custom highlight colors for buttons when focused
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.9f, 1.0f));

    // Buttons row with keyboard shortcuts (Ctrl+letter)
    bool add_pressed = ImGui::Button("Add [a]") ||
                      (!input_focused && ImGui::IsKeyPressed('a')) ||
                      (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('a'));
    if (add_pressed) {
        store.dispatch(SetInputTextAction{preserved_input});
        store.dispatch(AddTodoAction{});
        preserved_input = ""; // Clear after adding
    }

    ImGui::SameLine();
    bool remove_pressed = ImGui::Button("Remove [r]") ||
                         (!input_focused && ImGui::IsKeyPressed('r')) ||
                         (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('r'));
    if (remove_pressed) {
        store.dispatch(RemoveSelectedTodoAction{});
    }

    ImGui::SameLine();
    bool toggle_pressed = ImGui::Button("Toggle [t]") ||
                         (!input_focused && ImGui::IsKeyPressed('t')) ||
                         (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('t'));
    if (toggle_pressed) {
        store.dispatch(ToggleSelectedTodoAction{});
    }

    ImGui::SameLine();
    bool save_pressed = ImGui::Button("Save [s]") ||
                       (!input_focused && ImGui::IsKeyPressed('s')) ||
                       (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('s'));
    if (save_pressed) {
        store.dispatch(RequestSaveAction{});
    }

    ImGui::SameLine();
    bool load_pressed = ImGui::Button("Load [l]") ||
                       (!input_focused && ImGui::IsKeyPressed('l')) ||
                       (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('l'));
    if (load_pressed) {
        store.dispatch(RequestLoadAction{});
    }

    ImGui::SameLine();
    bool quit_pressed = ImGui::Button("Quit [q]") ||
                       (!input_focused && ImGui::IsKeyPressed('q')) ||
                       (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('q'));
    if (quit_pressed) {
        store.dispatch(QuitAction{});
    }

    ImGui::PopStyleColor(2); // Pop button style colors

    ImGui::Separator();

    // Improve todo list highlight style
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.6f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.7f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.5f, 0.8f, 0.7f));

    // Todo list with keyboard navigation
    ImGui::BeginChild("TodoList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    // Handle keyboard navigation in the list
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && state.selected_index > 0) {
        store.dispatch(SelectTodoAction{state.selected_index - 1});
    }
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) &&
        state.selected_index < static_cast<int>(state.todos.size()) - 1) {
        store.dispatch(SelectTodoAction{state.selected_index + 1});
    }
    // Enter key to toggle selected item
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))) {
        store.dispatch(ToggleSelectedTodoAction{});
    }
    // Delete key to remove selected item
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
        store.dispatch(RemoveSelectedTodoAction{});
    }

    for (int i = 0; i < state.todos.size(); i++) {
        const auto& todo = state.todos[i];
        bool is_selected = (i == state.selected_index);
        std::string label = (todo.done ? "[x] " : "[ ] ") + todo.text;

        // Use ImGui's built-in selection highlighting
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            store.dispatch(SelectTodoAction{i});

            // Double-click to toggle
            static int last_clicked_idx = -1;
            static auto last_click_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();

            if (last_clicked_idx == i &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count() < 500) {
                store.dispatch(ToggleSelectedTodoAction{});
                last_clicked_idx = -1; // Reset to prevent triple-click
            } else {
                last_clicked_idx = i;
                last_click_time = now;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(3); // Pop todo list style colors

    // Status bar with keyboard shortcut help
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: %s", state.status_message.c_str());

    // Help text for keyboard shortcuts
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                     "Shortcuts: Tab to navigate, Ctrl+A (add), Ctrl+R (remove), Ctrl+T (toggle)");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                     "In list: Arrows to select, Enter to toggle, Delete to remove");

    ImGui::End();

    ImGui::PopStyleColor(); // Pop NavHighlight style
    ImGui::PopStyleVar();   // Pop FrameBorderSize style
}


int main() {
    // --- Determine Paths FIRST ---
    std::filesystem::path data_path;
    std::filesystem::path log_file_path;
    try {
        data_path = Persistence::get_default_data_path();
        log_file_path = data_path.parent_path() / "tui_todo_log.txt";

        if (!log_file_path.parent_path().empty() && !std::filesystem::exists(log_file_path.parent_path())) {
            std::filesystem::create_directories(log_file_path.parent_path());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error determining file paths: " << e.what() << std::endl;
        return 1;
    }

    // --- Logger Setup (spdlog) ---
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path.string(), true);
        file_sink->set_level(spdlog::level::trace);
        auto file_logger = std::make_shared<spdlog::logger>("file_logger", file_sink);
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::info("--- Log Start ---");
        spdlog::info("Spdlog file logger initialized. Logging to: {}", log_file_path.string());
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Log file sink creation failed: " << e.what() << std::endl;
        return 1;
    }

    spdlog::info("Application starting");

    // --- Persistence Path ---
    spdlog::info("Data file path: {}", data_path.string());
    initialize_persistence_path(data_path);

    // --- Initial State ---
    auto initial_state_opt = Persistence::load_state(data_path);
    AppState initial_state;
    if (initial_state_opt) {
        initial_state = *initial_state_opt;
        initial_state.status_message = "State loaded.";
        spdlog::info("Loaded initial state from disk");
    } else {
        initial_state = AppState{};
        initial_state.status_message = "Ready (new list).";
        spdlog::info("No saved state found or error loading, starting fresh.");
    }
    initial_state.exit_requested = false;

    // --- ImTUI Setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();
    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // --- Lager Store Setup ---
    auto store = lager::make_store<Action>(
        initial_state,
        lager::with_manual_event_loop{},
        lager::with_reducer(reducer)
        );

    // --- Store watcher for handling exit ---
    bool should_exit = false;
    lager::watch(store, [&should_exit](AppState const& state) {
        if (state.exit_requested) {
            spdlog::info("Exit requested flag detected, stopping loop.");
            should_exit = true;
        }
    });

    // --- Main loop ---
    spdlog::info("Starting UI loop");
    const int renderDelayMs = 33; // ~30 FPS

    while (!should_exit) {
        // Start the Dear ImGui frame
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();

        // Render our UI
        renderUI(store);

        // Rendering
        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();

        // Sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(renderDelayMs));
    }

    // --- Cleanup ---
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
    ImGui::DestroyContext();

    spdlog::info("Application finished cleanly");
    spdlog::shutdown();
    return 0;
}
