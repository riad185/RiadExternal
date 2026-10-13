#pragma once
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <imgui.h>

namespace menu {
    void init();
    void set_visible(bool v);
    bool is_visible();
    bool is_injected();
    void set_scan_ok(bool ok);
    void render();
}

namespace teleport {
    inline uintptr_t local_camera() {
        RbxDataModel dm = RbxDataModel::get(); if (!dm.valid()) return 0;
        RbxInstance ws = dm.get_service("Workspace"); if (!ws.valid()) return 0;
        return rpm<uintptr_t>(ws.ptr + offsets::Workspace::CurrentCamera);
    }
    inline Vec3 normalize(const Vec3& v) { float l = v.length(); return l > 1e-6f ? Vec3{ v.x / l, v.y / l, v.z / l } : Vec3{}; }
    inline Vec3 cross(const Vec3& a, const Vec3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    inline void start_spin(uintptr_t hum) { /* spin logic */ }
    inline void stop_spin() {}
    inline std::atomic<bool> g_spin{ false };
    inline std::atomic<bool> g_lock{ false };
    inline std::atomic<bool> g_spectate{ false };
    inline std::atomic<uintptr_t> g_lock_target{ 0 };
    inline std::atomic<uintptr_t> g_spectate_hum{ 0 };
    inline void stop_lock() {}
    inline void stop_spectate() {}
    inline void service() {}
    inline void request_teleport(Vec3 dest, int mode) {}
}
