#pragma once
#include <vector>
#include <cstdint>
#include "roblox.hpp"
#include "math.hpp"

namespace aimbot {
    void start_thread();
    extern uintptr_t g_target;
    extern Matrix4 g_view_matrix;
    extern Vec2 g_screen;
}
