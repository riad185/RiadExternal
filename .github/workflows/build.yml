cmake_minimum_required(VERSION 3.20)
project(RiadExternal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RiadExternal WIN32
    src/main.cpp
    src/overlay.cpp
    src/esp.cpp
    src/aimbot.cpp
    src/executor.cpp
    src/menu.cpp
    resources.rc
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_dx11.cpp
    vendor/imgui/backends/imgui_impl_win32.cpp
)

target_include_directories(RiadExternal PRIVATE
    include/
    vendor/imgui/
    vendor/imgui/backends/
)

target_link_libraries(RiadExternal PRIVATE
    d3d11
    dxgi
    dwmapi
    winhttp
    windowscodecs
    winmm
)

target_compile_definitions(RiadExternal PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

set_target_properties(RiadExternal PROPERTIES OUTPUT_NAME "RiadExternal")

if(MSVC)
    set_property(TARGET RiadExternal APPEND_STRING PROPERTY LINK_FLAGS " /MANIFEST:NO")
endif()

set_property(TARGET RiadExternal PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
