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

    ImGui::Begin("TODO List Manager (Lager)", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // Title
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "TODO List Manager (Lager)");
    ImGui::Separator();

    // Input field
    static char input_buffer[256];
    strncpy(input_buffer, state.current_input.c_str(), sizeof(input_buffer) - 1);
    input_buffer[sizeof(input_buffer) - 1] = '\0';

    ImGui::Text("New Todo:");
    bool input_active = ImGui::IsItemActive();
    if (ImGui::InputText("##input", input_buffer, sizeof(input_buffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        store.dispatch(SetInputTextAction{input_buffer});
        store.dispatch(AddTodoAction{});
    } else if (ImGui::IsItemDeactivatedAfterEdit()) {
        store.dispatch(SetInputTextAction{input_buffer});
    }

    // Track if input is focused
    bool input_focused = ImGui::IsItemActive();

    // Rest of the code...
    // Buttons row
    if (ImGui::Button("Add")) {
        store.dispatch(AddTodoAction{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        store.dispatch(RemoveSelectedTodoAction{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Toggle")) {
        store.dispatch(ToggleSelectedTodoAction{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        store.dispatch(RequestSaveAction{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        store.dispatch(RequestLoadAction{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Quit")) {
        store.dispatch(QuitAction{});
    }

    ImGui::Separator();

    // Todo list - using a custom list rendering
    ImGui::BeginChild("TodoList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    for (int i = 0; i < state.todos.size(); i++) {
        const auto& todo = state.todos[i];
        bool is_selected = (i == state.selected_index);
        std::string label = (todo.done ? "[x] " : "[ ] ") + todo.text;

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

    // Status bar
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: %s", state.status_message.c_str());

    // Keyboard shortcuts info
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Press 'q' to quit");

    ImGui::End();

    // Global keyboard shortcuts - only work when input box is not focused
    if (!input_focused && ImGui::IsKeyPressed('q')) {
        store.dispatch(QuitAction{});
    }
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
