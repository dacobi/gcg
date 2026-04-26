cmake_minimum_required(VERSION 3.20)
project(gcg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dear ImGui (from local clone) ---
set(IMGUI_DIR "$ENV{HOME}/src/ImGui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# --- SDL3 (system-installed, found via pkg-config) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(SDL3_TTF REQUIRED IMPORTED_TARGET sdl3-ttf)
pkg_check_modules(SDL3_image REQUIRED IMPORTED_TARGET sdl3-image)
pkg_check_modules(libavcodec REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(libavutil REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(libavformat REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(libavdevice REQUIRED IMPORTED_TARGET libavdevice)
pkg_check_modules(libswscale REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(libswresample REQUIRED IMPORTED_TARGET libswresample)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)


target_link_libraries(imgui PUBLIC PkgConfig::SDL3)
#target_compile_options(gcg PRIVATE -g -O2)
# --- Our executable ---
add_executable(gcg main.cpp)
target_compile_options(gcg PRIVATE "-g")
target_link_libraries(gcg PRIVATE imgui PkgConfig::SDL3_TTF  PkgConfig::SDL3_image PkgConfig::libavcodec PkgConfig::libavutil PkgConfig::libavdevice PkgConfig::libavformat PkgConfig::libswscale PkgConfig::libswresample PkgConfig::ALSA)
