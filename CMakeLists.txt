cmake_minimum_required(VERSION 3.14)
project(DevShell
    VERSION 1.0.0
    DESCRIPTION "A custom TUI Developer Shell built with FTXUI"
    LANGUAGES CXX
)

# ──────────────────────────────────────────────
# C++20 Standard — Required, no fallback
# ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────
# FetchContent — Pull FTXUI at configure time
# ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)

# Disable FTXUI examples and docs to speed up build
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ftxui)

# ──────────────────────────────────────────────
# Project Sources
# ──────────────────────────────────────────────
add_executable(devshell
    src/main.cpp
    src/parser.cpp
    src/registry.cpp
    src/builtins.cpp
)

target_include_directories(devshell PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link FTXUI component libraries
target_link_libraries(devshell PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)
