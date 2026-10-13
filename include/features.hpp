#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include "roblox.hpp"
#include "esp.hpp"
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <mutex>

extern std::vector<PlayerInfo> g_players;
extern std::mutex              g_players_mtx;

namespace feat {
    inline uintptr_t g_local_player{ 0 };
    inline uintptr_t g_cached_char{ 0 };
    inline uintptr_t g_cached_hum{ 0 };
    inline uintptr_t g_cached_hrp_prim{ 0 };
    inline uintptr_t g_cached_cam{ 0 };
    inline std::vector<uintptr_t> g_cached_part_prims;

    inline uintptr_t local_player_ptr() {
        if (g_local_player) return g_local_player;
        RbxDataModel dm = RbxDataModel::get(); if (!dm.valid()) return 0;
        RbxInstance psvc = dm.get_service("Players"); if (!psvc.valid()) return 0;
        g_local_player = rpm<uintptr_t>(psvc.ptr + offsets::Players::LocalPlayer);
        return g_local_player;
    }

    inline uintptr_t ensure_cam() { if (!g_cached_cam) { RbxDataModel dm = RbxDataModel::get(); if (dm.valid()) { RbxInstance ws = dm.get_service("Workspace"); if (ws.valid()) g_cached_cam = rpm<uintptr_t>(ws.ptr + offsets::Workspace::CurrentCamera); } } return g_cached_cam; }

    inline uintptr_t g_cached_lighting{ 0 };
    inline uintptr_t ensure_lighting() { if (!g_cached_lighting) { RbxDataModel dm = RbxDataModel::get(); if (dm.valid()) g_cached_lighting = dm.get_service("Lighting").ptr; } return g_cached_lighting; }

    inline uintptr_t g_cached_world{ 0 };
    inline uintptr_t ensure_world() { if (!g_cached_world && offsets::Workspace::World) { RbxDataModel dm = RbxDataModel::get(); if (dm.valid()) { uintptr_t ws = dm.get_service("Workspace").ptr; if (ws) g_cached_world = rpm<uintptr_t>(ws + offsets::Workspace::World); } } return g_cached_world; }

    inline void refresh_char_cache(uintptr_t ch) {
        g_cached_char = ch; g_cached_hum = 0; g_cached_hrp_prim = 0; g_cached_cam = 0; g_cached_part_prims.clear();
        if (!ch) return;
        RbxInstance character{ ch };
        g_cached_hum = character.find_child_by_class("Humanoid").ptr;
        uintptr_t hrp = character.find_child_by_name("HumanoidRootPart").ptr;
        if (hrp) g_cached_hrp_prim = rpm<uintptr_t>(hrp + offsets::BasePart::Primitive);
        for (auto& c : character.get_children()) {
            std::string cls = c.get_class();
            if (cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" || cls == "WedgePart" || cls == "TrussPart" || cls == "SpawnLocation" || cls == "Seat" || cls == "VehicleSeat") {
                uintptr_t prim = rpm<uintptr_t>(c.ptr + offsets::BasePart::Primitive);
                if (prim) g_cached_part_prims.push_back(prim);
            }
        }
    }

    inline void set_collision(bool enabled) {
        if (!offsets::Primitive::PrimitiveFlags) return;
        const uint8_t mask = offsets::PrimitiveFlags::CanCollide;
        for (uintptr_t prim : g_cached_part_prims) { uint8_t flags = rpm<uint8_t>(prim + offsets::Primitive::PrimitiveFlags); uint8_t next = enabled ? (flags | mask) : (flags & ~mask); if (next != flags) wpm<uint8_t>(prim + offsets::Primitive::PrimitiveFlags, next); }
    }

    inline void invalidate_caches_if_dm_changed() {
        static uintptr_t last_dm = 0; RbxDataModel dm = RbxDataModel::get(); if (!dm.valid()) return;
        if (dm.ptr != last_dm) { last_dm = dm.ptr; g_cached_lighting = 0; g_cached_world = 0; g_local_player = 0; }
    }

    inline void apply_self_mods() {
        invalidate_caches_if_dm_changed();
        bool need_char = esp::cfg.speed_enabled || esp::cfg.jump_enabled || esp::cfg.noclip_enabled || esp::cfg.infinite_jump || esp::cfg.fly_enabled || esp::cfg.godmode || esp::cfg.jump_height_enabled;
        if (need_char) { uintptr_t lp = local_player_ptr(); uintptr_t ch = lp ? rpm<uintptr_t>(lp + offsets::Player::Character) : 0; if (ch != g_cached_char) refresh_char_cache(ch); }

        static struct { uintptr_t hum = 0; float ws = -1.f, jp = -1.f, hh = -1.f, jh = -1.f; } orig;
        if (g_cached_hum && orig.hum != g_cached_hum) { orig = { g_cached_hum, -1.f, -1.f, -1.f, -1.f }; }

        if (g_cached_hum && offsets::Humanoid::WalkSpeed) {
            if (esp::cfg.speed_enabled) { if (orig.ws < 0.f) orig.ws = rpm<float>(g_cached_hum + offsets::Humanoid::WalkSpeed); wpm<float>(g_cached_hum + offsets::Humanoid::WalkSpeed, esp::cfg.speed_value); if (offsets::Humanoid::WalkSpeedCheck) wpm<float>(g_cached_hum + offsets::Humanoid::WalkSpeedCheck, esp::cfg.speed_value); }
            else if (orig.ws >= 0.f) { wpm<float>(g_cached_hum + offsets::Humanoid::WalkSpeed, orig.ws); if (offsets::Humanoid::WalkSpeedCheck) wpm<float>(g_cached_hum + offsets::Humanoid::WalkSpeedCheck, orig.ws); orig.ws = -1.f; }
        }
        if (g_cached_hum && offsets::Humanoid::JumpPower) {
            if (esp::cfg.jump_enabled) { if (orig.jp < 0.f) orig.jp = rpm<float>(g_cached_hum + offsets::Humanoid::JumpPower); wpm<float>(g_cached_hum + offsets::Humanoid::JumpPower, esp::cfg.jump_value); }
            else if (orig.jp >= 0.f) { wpm<float>(g_cached_hum + offsets::Humanoid::JumpPower, orig.jp); orig.jp = -1.f; }
        }
        if (esp::cfg.noclip_enabled) set_collision(false); else set_collision(true);
        if (esp::cfg.infinite_jump && g_cached_hum && offsets::Humanoid::Jump && (GetAsyncKeyState(VK_SPACE) & 0x8000)) wpm<uint8_t>(g_cached_hum + offsets::Humanoid::Jump, 1);
        if ((esp::cfg.antiaim_spin) && g_cached_hrp_prim && offsets::Primitive::CFrame) {
            float cf[9]; mem::vm_read(mem::g_proc, (LPCVOID)(g_cached_hrp_prim + offsets::Primitive::CFrame), cf, sizeof(cf), nullptr);
            static float ang = 0.f; ang += esp::cfg.antiaim_spin_speed * 3.14159265f / 180.f * 0.001f;
            float c = cosf(ang), s = sinf(ang); cf[0]=c; cf[1]=0; cf[2]=s; cf[3]=0; cf[4]=1; cf[5]=0; cf[6]=-s; cf[7]=0; cf[8]=c;
            mem::vm_write(mem::g_proc, (LPVOID)(g_cached_hrp_prim + offsets::Primitive::CFrame), cf, sizeof(cf), nullptr);
        }
        if (esp::cfg.fov_changer_enabled && offsets::Camera::FieldOfView) { uintptr_t cam = ensure_cam(); if (cam) wpm<float>(cam + offsets::Camera::FieldOfView, esp::cfg.fov_value); }
        if (esp::cfg.godmode && g_cached_hum && offsets::Humanoid::MaxHealth && offsets::Humanoid::Health) { float mh = rpm<float>(g_cached_hum + offsets::Humanoid::MaxHealth); if (mh > 0.f) wpm<float>(g_cached_hum + offsets::Humanoid::Health, mh); }
        if (g_cached_hum && offsets::Humanoid::HipHeight) { if (esp::cfg.hover_enabled) { if (orig.hh < 0.f) orig.hh = rpm<float>(g_cached_hum + offsets::Humanoid::HipHeight); wpm<float>(g_cached_hum + offsets::Humanoid::HipHeight, esp::cfg.hover_height); } else if (orig.hh >= 0.f) { wpm<float>(g_cached_hum + offsets::Humanoid::HipHeight, orig.hh); orig.hh = -1.f; } }
        if (g_cached_hum && offsets::Humanoid::JumpHeight) { if (esp::cfg.jump_height_enabled) { if (orig.jh < 0.f) orig.jh = rpm<float>(g_cached_hum + offsets::Humanoid::JumpHeight); wpm<float>(g_cached_hum + offsets::Humanoid::JumpHeight, esp::cfg.jump_height); } else if (orig.jh >= 0.f) { wpm<float>(g_cached_hum + offsets::Humanoid::JumpHeight, orig.jh); orig.jh = -1.f; } }
    }

    inline void apply_movement() {
        if (!esp::cfg.fly_enabled) return;
        uintptr_t lp = local_player_ptr(); uintptr_t ch = lp ? rpm<uintptr_t>(lp + offsets::Player::Character) : 0;
        if (ch != g_cached_char) refresh_char_cache(ch);
        if (esp::cfg.fly_enabled && g_cached_hrp_prim && offsets::Primitive::AssemblyLinearVelocity && offsets::Camera::CFrame) {
            uintptr_t cam = ensure_cam(); if (!cam) return;
            float cf[9]{}; mem::vm_read(mem::g_proc, (LPCVOID)(cam + offsets::Camera::CFrame), cf, sizeof(cf), nullptr);
            Vec3 look = { -cf[2], -cf[5], -cf[8] }; Vec3 right = { cf[0], cf[3], cf[6] };
            Vec3 dir{ 0.f, 0.f, 0.f };
            if (GetAsyncKeyState('W') & 0x8000) dir = dir + look; if (GetAsyncKeyState('S') & 0x8000) dir = dir - look;
            if (GetAsyncKeyState('D') & 0x8000) dir = dir + right; if (GetAsyncKeyState('A') & 0x8000) dir = dir - right;
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) dir.y += 1.f; if (GetAsyncKeyState(VK_CONTROL) & 0x8000) dir.y -= 1.f;
            float len = dir.length(); if (len > 1e-3f) { float inv = esp::cfg.fly_speed / len; Vec3 vel = { dir.x * inv, dir.y * inv, dir.z * inv }; wpm<Vec3>(g_cached_hrp_prim + offsets::Primitive::AssemblyLinearVelocity, vel); }
            else wpm<Vec3>(g_cached_hrp_prim + offsets::Primitive::AssemblyLinearVelocity, Vec3{ 0.f, 0.f, 0.f });
            if (offsets::Primitive::AssemblyAngularVelocity) wpm<Vec3>(g_cached_hrp_prim + offsets::Primitive::AssemblyAngularVelocity, Vec3{ 0.f, 0.f, 0.f });
        }
    }
}
