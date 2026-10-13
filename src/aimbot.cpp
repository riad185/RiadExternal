#include "../include/aimbot.hpp"
#include "../include/esp.hpp"
#include "../include/memory.hpp"
#include "../include/roblox.hpp"
#include "../include/math.hpp"
#include <thread>
#include <atomic>
#include <cmath>

namespace aimbot {
    static std::atomic<bool> g_running{ false };
    static uintptr_t g_target{ 0 };

    static bool visible_heuristic(Vec3 cam_pos, Vec3 target_pos, const std::vector<PlayerInfo>& players, const Matrix4& vm, Vec2 screen) {
        Vec2 s = world_to_screen(vm, target_pos, screen.x, screen.y); if (s.x < 0.f && s.y < 0.f) return false; if (s.x < 0.f || s.x > screen.x || s.y < 0.f || s.y > screen.y) return false;
        float tdist = (target_pos - cam_pos).length();
        for (const auto& op : players) { if (!op.valid) continue; if ((op.position - target_pos).length() < 0.5f) continue; float odist = (op.position - cam_pos).length(); if (odist + 3.f >= tdist) continue; Vec2 os = world_to_screen(vm, op.position, screen.x, screen.y); if (os.x < 0.f && os.y < 0.f) continue; if (fabsf(s.x - os.x) < 25.f && fabsf(s.y - os.y) < 60.f) return false; }
        return true;
    }

    static void aim_thread() {
        g_running = true;
        while (g_running) {
            if (!esp::cfg.aim_enabled || !mem::alive()) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            RbxDataModel dm = RbxDataModel::get(); if (!dm.valid()) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            RbxInstance psvc = dm.get_service("Players"); if (!psvc.valid()) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            uintptr_t lp = rpm<uintptr_t>(psvc.ptr + offsets::Players::LocalPlayer); if (!lp) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            uintptr_t lteam = esp::cfg.team_check ? rpm<uintptr_t>(lp + offsets::Player::Team) : 0;
            uintptr_t lchar = rpm<uintptr_t>(lp + offsets::Player::Character); if (!lchar) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            uintptr_t hrp = 0; RbxInstance character{ lchar }; hrp = character.find_child_by_name("HumanoidRootPart").ptr; if (!hrp) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            uintptr_t prim = rpm<uintptr_t>(hrp + offsets::BasePart::Primitive); if (!prim) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            Vec3 my_pos = rpm<Vec3>(prim + offsets::Primitive::Position);
            uintptr_t cam = 0; RbxInstance ws = dm.get_service("Workspace"); if (ws.valid()) cam = rpm<uintptr_t>(ws.ptr + offsets::Workspace::CurrentCamera); if (!cam) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            float cf[9]{}; mem::vm_read(mem::g_proc, (LPCVOID)(cam + offsets::Camera::CFrame), cf, sizeof(cf), nullptr);
            Vec3 cam_pos = rpm<Vec3>(cam + offsets::Camera::CFrame + 0x24);
            Vec3 look = { -cf[2], -cf[5], -cf[8] };
            std::vector<PlayerInfo> players; { std::lock_guard<std::mutex> lk(g_players_mtx); players = g_players; }
            uintptr_t best = 0; float best_score = 1e9f;
            for (auto& p : players) { if (!p.valid) continue; if (esp::cfg.team_check && p.team_ptr == lteam) continue; if (esp::cfg.aim_visibility_check && !visible_heuristic(cam_pos, p.head_pos, players, g_view_matrix, g_screen)) continue; Vec3 target = p.head_pos; float dist = (target - cam_pos).length(); if (esp::cfg.aim_max_dist > 0.f && dist > esp::cfg.aim_max_dist) continue; Vec2 s = world_to_screen(g_view_matrix, target, g_screen.x, g_screen.y); if (s.x < 0.f && s.y < 0.f) continue; float dx = s.x - g_screen.x * 0.5f; float dy = s.y - g_screen.y * 0.5f; float fov_dist = sqrtf(dx * dx + dy * dy); if (fov_dist > esp::cfg.fov_radius) continue; float score = fov_dist; if (score < best_score) { best_score = score; best = p.player_ptr; } }
            if (best) { g_target = best; uintptr_t bchar = rpm<uintptr_t>(best + offsets::Player::Character); if (bchar) { RbxInstance bc{ bchar }; uintptr_t bhrp = bc.find_child_by_name("HumanoidRootPart").ptr; if (bhrp) { uintptr_t bprim = rpm<uintptr_t>(bhrp + offsets::BasePart::Primitive); if (bprim) { Vec3 target = rpm<Vec3>(bprim + offsets::Primitive::Position); Vec3 dir = (target - cam_pos).normalized(); float new_cf[9]; new_cf[0] = -dir.x; new_cf[1] = 0; new_cf[2] = -dir.z; new_cf[3] = 0; new_cf[4] = 1; new_cf[5] = 0; new_cf[6] = -dir.z; new_cf[7] = 0; new_cf[8] = dir.x; mem::vm_write(mem::g_proc, (LPVOID)(cam + offsets::Camera::CFrame), new_cf, sizeof(new_cf), nullptr); } } } }
            else g_target = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void start_thread() { std::thread(aim_thread).detach(); }
}
