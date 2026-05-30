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
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
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
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
find_package(OpenCL REQUIRED)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(hdf5 REQUIRED)

option(USE_USD "Build with OpenUSD support" OFF)

if (USE_USD)
    set(pxr_DIR "/usr/local")
    find_package(pxr REQUIRED)
    find_package(Python3 COMPONENTS Development REQUIRED)
    set(USD_SOURCES usd_manager.cpp usd_hydra_renderer.cpp object3d.cpp)
    set(USD_LIBS usd_usd usd_usdGeom usd_usdImaging usd_usdImagingGL usd_hd usd_hdSt usd_hgi usd_hgiGL usd_sdf usd_tf usd_gf usd_vt usd_python ${Python3_LIBRARIES} /usr/local/lib/libtbb.so)
    set(USD_INCLUDES /usr/local/include ${Python3_INCLUDE_DIRS})
else()
    set(USD_SOURCES "")
    set(USD_LIBS "")
    set(USD_INCLUDES "")
endif()

find_package(SDL3_shadercross REQUIRED)
find_package(OpenGL REQUIRED)

link_directories(/usr/local/lib)

target_link_libraries(imgui PUBLIC PkgConfig::SDL3 SDL3_shadercross::SDL3_shadercross-shared)
#target_compile_options(gcg PRIVATE -g -O2)
# --- Our executable ---
add_executable(gcg main.cpp clplasma.cpp clmandelbrot.cpp luascripting.cpp input_manager.cpp renderer.cpp shplasma.cpp godot_manager.cpp godot_renderer.cpp resource_format_loader_gltf.cpp resource_format_loader_wav.cpp ${USD_SOURCES})

if (USE_USD)
    target_compile_definitions(gcg PRIVATE USE_USD)
endif()

target_include_directories(gcg PRIVATE ${USD_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/godot ${CMAKE_CURRENT_SOURCE_DIR}/godot/platform/linuxbsd)
target_compile_options(gcg PRIVATE "-g")
target_link_libraries(gcg PRIVATE imgui PkgConfig::SDL3_TTF  PkgConfig::SDL3_image PkgConfig::libavcodec PkgConfig::libavutil PkgConfig::libavdevice PkgConfig::libavformat PkgConfig::libswscale PkgConfig::libswresample PkgConfig::ALSA PkgConfig::LUA ${USD_LIBS} OpenCL::OpenCL m SDL3_shadercross::SDL3_shadercross-shared OpenGL::GL ${CMAKE_CURRENT_SOURCE_DIR}/godot/bin/libgodot.linuxbsd.template_release.x86_64.so)
set_target_properties(gcg PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")

