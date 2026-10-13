#include "../include/esp.hpp"
#include "../include/memory.hpp"
#include "../include/roblox.hpp"
#include "../include/math.hpp"
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

namespace esp {
    Config cfg;
    static Vec2 g_view_origin{ 0.f, 0.f };
    static std::vector<std::string> g_notifications;
    static auto g_notify_time = std::chrono::steady_clock::now();

    void notify(const std::string& msg) { g_notifications.push_back(msg); g_notify_time = std::chrono::steady_clock::now(); if (g_notifications.size() > 8) g_notifications.erase(g_notifications.begin()); }
    void set_view_origin(Vec2 origin) { g_view_origin = origin; }

    bool save_config(const char* path) {
        FILE* f = fopen(path, "wb"); if (!f) return false;
        fwrite(&cfg, sizeof(cfg), 1, f); fclose(f); return true;
    }
    bool load_config(const char* path) {
        FILE* f = fopen(path, "rb"); if (!f) return false;
        size_t sz = fread(&cfg, 1, sizeof(cfg), f); fclose(f); return sz == sizeof(cfg);
    }

    void render(ImDrawList* dl, const std::vector<PlayerInfo>& players, const Matrix4& vm, Vec2 screen) {
        if (cfg.draw_fov) { ImVec2 center(screen.x * 0.5f, screen.y * 0.5f); dl->AddCircle(center, cfg.fov_radius, ImColor(cfg.col_fov), 64, 1.5f); }
        if (cfg.draw_crosshair) { ImVec2 c(screen.x * 0.5f, screen.y * 0.5f); float s = cfg.crosshair_size; dl->AddLine({ c.x - s, c.y }, { c.x + s, c.y }, ImColor(cfg.col_crosshair), 1.5f); dl->AddLine({ c.x, c.y - s }, { c.x, c.y + s }, ImColor(cfg.col_crosshair), 1.5f); }
        for (auto& p : players) {
            if (!p.valid) continue;
            Vec2 pos = world_to_screen(vm, p.position, screen.x, screen.y); if (pos.x < 0.f && pos.y < 0.f) continue;
            Vec2 head = world_to_screen(vm, p.head_pos, screen.x, screen.y); if (head.x < 0.f && head.y < 0.f) continue;
            float h = pos.y - head.y; if (h < 10.f) continue; float w = h * 0.3f;
            ImU32 col = ImColor(cfg.col_box);
            if (cfg.box_style == 1) { dl->AddRect({ head.x - w * 0.5f, head.y }, { pos.x + w * 0.5f, pos.y }, col, 0.f, 0, cfg.box_thickness); }
            else if (cfg.box_style == 2) { dl->AddRectFilled({ head.x - w * 0.5f, head.y }, { pos.x + w * 0.5f, pos.y }, ImColor(cfg.col_box_fill), 0.f, 0); dl->AddRect({ head.x - w * 0.5f, head.y }, { pos.x + w * 0.5f, pos.y }, col, 0.f, 0, cfg.box_thickness); }
            if (cfg.draw_name) { ImVec2 ts = ImGui::CalcTextSize(p.name.c_str()); dl->AddText({ head.x - ts.x * 0.5f, head.y - ts.y - 4.f }, ImColor(cfg.col_name), p.name.c_str()); }
            if (cfg.draw_health) { float hp_pct = p.max_health > 0.f ? p.health / p.max_health : 0.f; float bar_h = h * hp_pct; dl->AddRectFilled({ head.x - w * 0.5f - 6.f, pos.y - bar_h }, { head.x - w * 0.5f - 2.f, pos.y }, ImColor(0.2f, 0.9f, 0.3f, 1.f)); }
            if (cfg.draw_distance) { char buf[32]; snprintf(buf, sizeof(buf), "%.0fm", (p.position - Vec3{}).length() * 0.02f); dl->AddText({ head.x + w * 0.5f + 4.f, head.y }, ImColor(1.f, 1.f, 1.f, 0.7f), buf); }
            if (cfg.draw_head_dot) { dl->AddCircleFilled({ head.x, head.y }, 3.f, ImColor(cfg.col_head)); }
            if (cfg.draw_skeleton) { dl->AddLine({ head.x, head.y }, { pos.x, pos.y - h * 0.4f }, ImColor(cfg.col_skeleton), 1.5f); }
            if (cfg.draw_line) { float ly = cfg.snapline_pos == 0 ? screen.y : (cfg.snapline_pos == 1 ? screen.y * 0.5f : 0.f); dl->AddLine({ screen.x * 0.5f, ly }, { pos.x, pos.y }, ImColor(cfg.col_line), 1.f); }
        }
        if (cfg.draw_watermark) { dl->AddText({ 10.f, 10.f }, ImColor(1.f, 1.f, 1.f, 0.8f), "RiadExternal"); }
        if (cfg.draw_hud) { char buf[128]; snprintf(buf, sizeof(buf), "Players: %zu", players.size()); dl->AddText({ 10.f, 30.f }, ImColor(1.f, 1.f, 1.f, 0.6f), buf); }
        if (cfg.radar_enabled) { float rs = cfg.radar_size; float rx = screen.x - rs - 20.f; float ry = 20.f; dl->AddRectFilled({ rx, ry }, { rx + rs, ry + rs }, ImColor(0.f, 0.f, 0.f, 0.5f), 4.f); dl->AddRect({ rx, ry }, { rx + rs, ry + rs }, ImColor(1.f, 1.f, 1.f, 0.3f), 4.f, 0, 1.f); for (auto& p : players) { if (!p.valid) continue; float dx = (p.position.x / cfg.radar_range) * rs * 0.5f; float dy = (p.position.z / cfg.radar_range) * rs * 0.5f; if (dx > rs * 0.5f) dx = rs * 0.5f; if (dx < -rs * 0.5f) dx = -rs * 0.5f; if (dy > rs * 0.5f) dy = rs * 0.5f; if (dy < -rs * 0.5f) dy = -rs * 0.5f; dl->AddCircleFilled({ rx + rs * 0.5f + dx, ry + rs * 0.5f + dy }, 3.f, ImColor(1.f, 0.3f, 0.3f)); } }
        auto now = std::chrono::steady_clock::now(); float elapsed = std::chrono::duration<float>(now - g_notify_time).count();
        for (size_t i = 0; i < g_notifications.size(); i++) { float alpha = 1.f - (elapsed - i * 0.5f) * 0.3f; if (alpha < 0.f) alpha = 0.f; dl->AddText({ 10.f, screen.y - 30.f - (float)(g_notifications.size() - i) * 20.f }, ImColor(1.f, 1.f, 1.f, alpha), g_notifications[i].c_str()); }
    }

    void render_console(const std::vector<PlayerInfo>& players, Vec2 screen) {
        // Console rendering handled by menu
    }
}
