cmake_minimum_required(VERSION 3.17)
project(tui_lager_spdlog_todo_app CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set default build type if not specified (e.g., for spdlog Release optimizations)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
endif()

include(FetchContent)

set(immer_BUILD_TESTS off)
set(immer_BUILD_EXAMPLES off)
set(immer_BUILD_EXTRAS off)
set(immer_BUILD_DOCS off)
set(zug_BUILD_TESTS off)
set(zug_BUILD_EXAMPLES off)
set(zug_BUILD_DOCS off)
set(lager_BUILD_TESTS off)
set(lager_BUILD_EXAMPLES off)
set(lager_BUILD_DOCS off)

# FTXUI, immer, zug, lager, nlohmann_json FetchContent declarations...
# (Same as the previous Todo example)
FetchContent_Declare(ftxui GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git GIT_TAG v5.0.0)
FetchContent_MakeAvailable(ftxui)
FetchContent_Declare(immer GIT_REPOSITORY https://github.com/arximboldi/immer.git GIT_TAG v0.8.1)
FetchContent_MakeAvailable(immer)
FetchContent_Declare(zug GIT_REPOSITORY https://github.com/arximboldi/zug.git GIT_TAG v0.1.1)
FetchContent_MakeAvailable(zug)
FetchContent_Declare(lager GIT_REPOSITORY https://github.com/arximboldi/lager.git GIT_TAG v0.1.1)
set(LAGER_USE_SYSTEM_IMMER ON)
set(LAGER_USE_SYSTEM_ZUG ON)
FetchContent_MakeAvailable(lager)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1 # Or latest stable tag
)
# Prevent spdlog from installing itself when built as part of this project
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
# Recommended settings for header-only usage (faster compile)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE) # Build static lib if not header only
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE) # Use bundled fmt
# If you want header-only spdlog:
# set(SPDLOG_HEADER_ONLY ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog)

add_executable(tui_app
    src/main.cpp
    src/persistence.cpp
)

target_include_directories(tui_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tui_app PRIVATE
    ftxui::screen
    ftxui::component
    # immer::immer # Might not be needed to link directly if lager links it
    immer
    zug
    lager        # Links immer and zug transitively
    nlohmann_json::nlohmann_json
    spdlog::spdlog      # Link spdlog (header-only or static lib)
    # Filesystem linking if needed (see previous examples)
)

# Ensure C++17 features enabled
target_compile_features(tui_app PRIVATE cxx_std_17)

# If using header-only spdlog, you might need this define
# target_compile_definitions(tui_app PRIVATE SPDLOG_HEADER_ONLY)

# Link Threads if spdlog requires it (usually does for async/multi-threaded sinks)
find_package(Threads REQUIRED)
target_link_libraries(tui_app PRIVATE Threads::Threads)
