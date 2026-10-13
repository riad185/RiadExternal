#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "../include/menu.hpp"
#include "../include/esp.hpp"
#include "../include/executor.hpp"
#include "../include/memory.hpp"
#include "../include/offsets.hpp"
#include "../include/roblox.hpp"
#include "../include/overlay.hpp"
#include "../include/features.hpp"
#include "../include/scanner.hpp"
#include "../include/log.hpp"
#include "../include/luavm.hpp"
#include "../include/teleport.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {
    struct Theme {
        ImColor accent        { 138, 92, 255, 255 };
        ImColor accent_soft   { 138, 92, 255, 100 };
        ImColor accent_glow   { 138, 92, 255,  70 };
        ImColor winbg         {  12,  12,  16, 245 };
        ImColor sidebar       {  10,  10,  14, 240 };
        ImColor child         {  18,  18,  24, 255 };
        ImColor child_hover   {  26,  26,  34, 255 };
        ImColor stroke        {  35,  35,  45, 255 };
        ImColor stroke_soft   {  26,  26,  34, 255 };
        ImColor text          { 255, 255, 255, 255 };
        ImColor text_dim      { 180, 180, 190, 255 };
        ImColor text_dimmer   { 120, 120, 130, 255 };
        ImColor ok            { 100, 220, 140, 255 };
        ImColor warn          { 240, 200,  96, 255 };
        ImColor bad           { 240,  96, 108, 255 };
        ImColor ink           {  10,  10,  12, 255 };
        float win_w      = 1100.f;
        float win_h      = 680.f;
        float win_round  = 12.f;
        float sidebar_w  = 200.f;
        float item_h     = 36.f;
        float tab_h      = 34.f;
    } g_th;

    ImFont* g_font_regular = nullptr;
    ImFont* g_font_medium  = nullptr;
    ImFont* g_font_small   = nullptr;

    enum class Screen { Boot, Menu };
    std::atomic<Screen> g_screen{ Screen::Boot };
    std::atomic<bool>   g_visible{ true };
    std::atomic<bool>   g_scan_ok{ false };
    std::atomic<bool>   g_injected{ false };

    int g_active_tab = 0;
    int g_active_subtab = 0;

    struct AnimCache {
        struct E { float v; uint32_t last_frame; };
        std::unordered_map<ImGuiID, E> m;
        static constexpr size_t MAX = 1024;
        float step(ImGuiID id, float target, float speed) {
            auto& e = m[id]; e.last_frame = (uint32_t)ImGui::GetFrameCount();
            float dt = ImGui::GetIO().DeltaTime; if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
            float t = 1.f - std::exp(-speed * dt); e.v += (target - e.v) * t;
            if (std::fabs(e.v - target) < 0.001f) e.v = target;
            if (m.size() > MAX) evict(); return e.v;
        }
        void evict() { uint32_t f = (uint32_t)ImGui::GetFrameCount(); uint32_t cutoff = f > 240u ? f - 240u : 0u; for (auto it = m.begin(); it != m.end();) { if (it->second.last_frame < cutoff) it = m.erase(it); else ++it; } }
        void erase(ImGuiID id) { m.erase(id); }
    };
    AnimCache g_anim;
    inline float anim(ImGuiID id, float target, float speed = 16.f) { return g_anim.step(id, target, speed); }
    inline ImU32 blend(ImU32 a, ImU32 b, float t) { if (t <= 0.f) return a; if (t >= 1.f) return b; ImVec4 av = ImGui::ColorConvertU32ToFloat4(a); ImVec4 bv = ImGui::ColorConvertU32ToFloat4(b); return ImGui::ColorConvertFloat4ToU32({ av.x + (bv.x - av.x) * t, av.y + (bv.y - av.y) * t, av.z + (bv.z - av.z) * t, av.w + (bv.w - av.w) * t }); }
    inline ImU32 with_alpha(ImU32 c, float a) { ImVec4 v = ImGui::ColorConvertU32ToFloat4(c); v.w *= a; if (v.w < 0.f) v.w = 0.f; if (v.w > 1.f) v.w = 1.f; return ImGui::ColorConvertFloat4ToU32(v); }
    inline ImU32 col(const ImColor& c) { return (ImU32)c; }
    inline ImU32 col_a(const ImColor& c, float a) { return with_alpha((ImU32)c, a); }
    inline bool hit(const ImRect& r) { return ImGui::IsMouseHoveringRect(r.Min, r.Max); }

    // Script editor buffer
    static char g_script_buffer[8192] = "-- LunaExternal script\nprint('hello from luna')\n";
    static std::vector<std::string> g_script_history;
    static int g_current_script = 0;

    // Console log
    static std::vector<std::string> g_console_lines;
    static bool g_console_auto_scroll = true;

    // Keybind listening
    static int* g_listen_key = nullptr;
    static bool g_listen_pre[256] = {};
    static bool g_listen_last[256] = {};

    void add_console(const std::string& line) { g_console_lines.push_back(line); if (g_console_lines.size() > 256) g_console_lines.erase(g_console_lines.begin()); }

    const char* vk_name(int vk) {
        static char buf[16]; if (vk == 0) return "None";
        switch (vk) { case 0x01: return "LMB"; case 0x02: return "RMB"; case 0x04: return "MMB"; case 0x08: return "Back"; case 0x0D: return "Enter"; case 0x10: return "Shift"; case 0x11: return "Ctrl"; case 0x12: return "Alt"; case 0x1B: return "Esc"; case 0x20: return "Space"; case 0x25: return "Left"; case 0x26: return "Up"; case 0x27: return "Right"; case 0x28: return "Down"; case 0x2C: return "PrtSc"; case 0x2D: return "Ins"; case 0x2E: return "Del"; case 0xA0: return "LShift"; case 0xA1: return "RShift"; case 0xA2: return "LCtrl"; case 0xA3: return "RCtrl"; case 0xA4: return "LAlt"; case 0xA5: return "RAlt"; }
        if (vk >= 0x30 && vk <= 0x39) { snprintf(buf, sizeof(buf), "%c", vk); return buf; }
        if (vk >= 0x41 && vk <= 0x5A) { snprintf(buf, sizeof(buf), "%c", vk); return buf; }
        if (vk >= 0x70 && vk <= 0x7B) { snprintf(buf, sizeof(buf), "F%d", vk - 0x6F); return buf; }
        snprintf(buf, sizeof(buf), "%02X", vk); return buf;
    }

    void start_listen(int* pk) { g_listen_key = pk; for (int i = 0; i < 256; i++) { bool d = (GetAsyncKeyState(i) & 0x8000) != 0; g_listen_pre[i] = d; g_listen_last[i] = d; } }
    void poll_keybind_listen() {
        if (!g_listen_key) return;
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) && !g_listen_pre[VK_ESCAPE]) { g_listen_key = nullptr; return; }
        for (int vk = 0x01; vk <= 0xFE; vk++) { if (vk == VK_ESCAPE || vk == VK_LBUTTON) continue; bool down = (GetAsyncKeyState(vk) & 0x8000) != 0; bool prev = g_listen_last[vk]; g_listen_last[vk] = down; if (g_listen_pre[vk]) { if (!down) g_listen_pre[vk] = false; continue; } if (!prev && down) { *g_listen_key = vk; g_listen_key = nullptr; return; } }
    }

    // Custom widgets
    bool Tab(const char* label, int* state, int idx) {
        ImGuiWindow* win = ImGui::GetCurrentWindow(); if (win->SkipItems) return false;
        ImGui::PushID(idx);
        const float W = ImGui::GetContentRegionAvail().x; const float H = g_th.tab_h;
        ImVec2 pos = win->DC.CursorPos; ImRect bb(pos, { pos.x + W, pos.y + H });
        ImGui::ItemSize({ bb.Min, { bb.Max.x, bb.Max.y + 4.f } }, 0.f);
        ImGuiID id = win->GetID("tab"); if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
        bool active = (*state == idx); bool hovered = hit(bb); bool pressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (pressed) *state = idx;
        ImDrawList* dl = win->DrawList; float thov = anim(id, hovered ? 1.f : 0.f, 20.f); float tact = anim(id ^ 0xACAC, active ? 1.f : 0.f, 22.f);
        if (tact > 0.02f) { dl->AddRectFilled(bb.Min, bb.Max, col_a(g_th.child, 0.85f * tact), 6.f); dl->AddRectFilled({ bb.Min.x + 2.f, bb.Min.y + 6.f }, { bb.Min.x + 4.f, bb.Max.y - 6.f }, col_a(g_th.accent, tact), 1.f); }
        else if (thov > 0.02f) { dl->AddRectFilled(bb.Min, bb.Max, col_a(g_th.child_hover, 0.55f * thov), 6.f); }
        ImU32 lcol = active ? col(g_th.text) : blend(col(g_th.text_dimmer), col(g_th.text), thov);
        dl->AddText({ bb.Min.x + 16.f, bb.Min.y + (H - ImGui::GetFontSize()) * 0.5f - 1.f }, lcol, label);
        ImGui::PopID(); return pressed;
    }

    bool Checkbox(const char* label, bool* v) {
        ImGuiWindow* win = ImGui::GetCurrentWindow(); if (win->SkipItems) return false;
        ImGui::PushID(label);
        const float W = ImGui::GetContentRegionAvail().x; const float H = g_th.item_h;
        ImVec2 pos = win->DC.CursorPos; ImRect bb(pos, { pos.x + W, pos.y + H });
        ImGui::ItemSize({ bb.Min, { bb.Max.x, bb.Max.y + 2.f } }, 0.f);
        ImGuiID id = win->GetID("cb"); if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
        bool hovered = hit(bb); bool pressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (pressed) *v = !*v;
        ImDrawList* dl = win->DrawList; float thov = anim(id, hovered ? 1.f : 0.f, 22.f); float ton = anim(id ^ 0xFEED, *v ? 1.f : 0.f, 20.f);
        if (thov > 0.02f) dl->AddRectFilled(bb.Min, bb.Max, col_a(g_th.accent, 0.08f * thov), 5.f);
        dl->AddText({ bb.Min.x + 12.f, bb.Min.y + (H - ImGui::GetFontSize()) * 0.5f }, *v ? col(g_th.text) : blend(col(g_th.text_dimmer), col(g_th.text_dim), thov), label);
        const float pw = 30.f, ph = 16.f; ImVec2 pmn{ bb.Max.x - pw - 10.f, bb.Min.y + (H - ph) * 0.5f }; ImVec2 pmx{ pmn.x + pw, pmn.y + ph };
        ImU32 pill_bg = blend(col(g_th.stroke_soft), col(g_th.accent), ton); dl->AddRectFilled(pmn, pmx, pill_bg, ph * 0.5f);
        float pad = 2.f, dia = ph - pad * 2.f; float x0 = pmn.x + pad, x1 = pmx.x - pad - dia; float cx = x0 + (x1 - x0) * ton;
        dl->AddRectFilled({ cx, pmn.y + pad }, { cx + dia, pmx.y - pad }, blend(col(g_th.text_dim), col(g_th.text), ton), dia * 0.5f);
        ImGui::PopID(); return pressed;
    }

    bool SliderFloat(const char* label, float* v, float mn, float mx, const char* fmt = "%.2f") {
        ImGuiWindow* win = ImGui::GetCurrentWindow(); if (win->SkipItems) return false;
        ImGui::PushID(label);
        const float W = ImGui::GetContentRegionAvail().x; const float H = g_th.item_h + 10.f;
        ImVec2 pos = win->DC.CursorPos; ImRect bb(pos, { pos.x + W, pos.y + H });
        ImGui::ItemSize({ bb.Min, { bb.Max.x, bb.Max.y + 2.f } }, 0.f);
        ImGuiID id = win->GetID("sf"); if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
        ImDrawList* dl = win->DrawList;
        dl->AddText({ bb.Min.x + 12.f, bb.Min.y + 4.f }, col(g_th.text_dim), label);
        char buf[32]; snprintf(buf, sizeof(buf), fmt, *v); ImVec2 vsz = ImGui::CalcTextSize(buf);
        dl->AddText({ bb.Max.x - vsz.x - 12.f, bb.Min.y + 4.f }, col(g_th.text), buf);
        float track_y = bb.Min.y + 26.f; ImRect track({ bb.Min.x + 12.f, track_y }, { bb.Max.x - 12.f, track_y + 4.f });
        ImRect grab_zone({ track.Min.x - 6.f, track.Min.y - 8.f }, { track.Max.x + 6.f, track.Max.y + 8.f });
        bool hovered = hit(grab_zone) || hit(bb);
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ImGui::SetActiveID(id, win);
        if (ImGui::GetActiveID() == id && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) ImGui::ClearActiveID();
        bool changed = false;
        if (ImGui::GetActiveID() == id) { float t = (ImGui::GetIO().MousePos.x - track.Min.x) / track.GetWidth(); if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f; float nv = mn + t * (mx - mn); if (nv != *v) { *v = nv; changed = true; } }
        float pct = (mx > mn) ? ((*v - mn) / (mx - mn)) : 0.f; if (pct < 0.f) pct = 0.f; if (pct > 1.f) pct = 1.f;
        dl->AddRectFilled(track.Min, track.Max, col(g_th.stroke_soft), 2.f);
        if (pct > 0.001f) dl->AddRectFilled(track.Min, { track.Min.x + track.GetWidth() * pct, track.Max.y }, col(g_th.accent), 2.f);
        ImVec2 knob{ track.Min.x + track.GetWidth() * pct, track_y + 2.f }; float knob_h = anim(id ^ 0xB0F1, hovered ? 1.f : 0.f, 16.f);
        if (pct > 0.001f) dl->AddCircleFilled(knob, 6.f + 2.f * knob_h, col_a(g_th.accent, 0.5f));
        dl->AddCircleFilled(knob, 4.f, col(g_th.accent)); dl->AddCircle(knob, 4.f, col(g_th.ink), 0, 1.f);
        ImGui::PopID(); return changed;
    }

    bool Keybind(const char* id_str, int* pk) {
        ImGuiWindow* win = ImGui::GetCurrentWindow(); if (win->SkipItems) return false;
        ImGui::PushID(id_str);
        ImVec2 pos = win->DC.CursorPos; const float kw = 64.f, kh = 20.f;
        ImRect bb(pos, { pos.x + kw, pos.y + kh }); ImGui::ItemSize({ bb.Min, { bb.Max.x, bb.Max.y + 2.f } }, 0.f);
        ImGuiID id = win->GetID("kb"); if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
        bool listening = (g_listen_key == pk); bool hovered = hit(bb);
        bool pr_l = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left); bool pr_r = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if (pr_l) { start_listen(pk); listening = true; } if (pr_r && !listening) *pk = 0;
        ImDrawList* dl = win->DrawList; float thov = anim(id, (hovered || listening) ? 1.f : 0.f, 20.f);
        ImU32 bg = listening ? col(g_th.accent) : blend(col(g_th.stroke_soft), col(g_th.child_hover), thov);
        dl->AddRectFilled(bb.Min, bb.Max, bg, 4.f); dl->AddRect(bb.Min, bb.Max, col_a(g_th.accent, listening ? 1.f : 0.35f), 4.f, 0, 1.f);
        const char* t = listening ? "[..]" : vk_name(*pk); ImVec2 tz = ImGui::CalcTextSize(t);
        dl->AddText({ bb.Min.x + (kw - tz.x) * 0.5f, bb.Min.y + (kh - tz.y) * 0.5f }, listening ? col(g_th.ink) : col(g_th.text), t);
        ImGui::PopID(); return pr_l;
    }

    void Section(const char* label) { ImGui::Dummy({ 0, 4.f }); ImVec2 p = ImGui::GetCursorScreenPos(); ImGui::GetWindowDrawList()->AddText({ p.x + 12.f, p.y }, col(g_th.text_dimmer), label); ImGui::Dummy({ 0, ImGui::GetFontSize() + 4.f }); }

    bool BeginFrame(const char* id, ImVec2 size) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col(g_th.child)); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 10 }); ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f); ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 4 });
        return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    }
    void EndFrame() { ImGui::EndChild(); ImGui::PopStyleVar(3); ImGui::PopStyleColor(); }

    // Tab renderers
    void render_scripts_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##scripts_a", { fs.x * 0.6f, fs.y })) {
            Section("Script Editor");
            ImGui::InputTextMultiline("##editor", g_script_buffer, sizeof(g_script_buffer), { -1.f, fs.y - 120.f }, ImGuiInputTextFlags_AllowTabInput);
            ImGui::Dummy({ 0, 8.f });
            if (ImGui::Button("Execute", { 100.f, 28.f })) { executor::execute_source(g_script_buffer); add_console("Script executed"); }
            ImGui::SameLine();
            if (ImGui::Button("Clear", { 80.f, 28.f })) { g_script_buffer[0] = '\0'; }
            ImGui::SameLine();
            if (ImGui::Button("Save", { 80.f, 28.f })) { g_script_history.push_back(g_script_buffer); add_console("Script saved"); }
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##scripts_b", { -1.f, fs.y })) {
            Section("Script History");
            for (int i = (int)g_script_history.size() - 1; i >= 0; i--) {
                ImGui::PushID(i);
                if (ImGui::Button(g_script_history[i].substr(0, 30).c_str(), { -1.f, 24.f })) { strcpy_s(g_script_buffer, g_script_history[i].c_str()); }
                ImGui::PopID();
            }
            if (g_script_history.empty()) ImGui::TextDisabled("No saved scripts");
            EndFrame();
        }
    }

    void render_aimbot_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##aim_a", { fs.x * 0.5f, fs.y })) {
            Section("Aimbot");
            Checkbox("Enable aimbot", &esp::cfg.aim_enabled);
            Keybind("Aim key", &esp::cfg.key_aim);
            Checkbox("Team check", &esp::cfg.team_check);
            Checkbox("Visibility check", &esp::cfg.aim_visibility_check);
            Section("Tuning");
            SliderFloat("FOV radius", &esp::cfg.fov_radius, 10.f, 1000.f, "%.0f");
            SliderFloat("Strength", &esp::cfg.aim_strength, 1.f, 100.f, "%.0f");
            SliderFloat("Smoothing", &esp::cfg.aim_smooth, 0.1f, 1.f, "%.2f");
            SliderFloat("Max distance", &esp::cfg.aim_max_dist, 0.f, 5000.f, "%.0f");
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##aim_b", { -1.f, fs.y })) {
            Section("Target");
            const char* bones[] = { "Head", "Chest" };
            ImGui::Combo("Bone", &esp::cfg.aim_bone, bones, 2);
            const char* pri[] = { "Nearest crosshair", "Lowest HP" };
            ImGui::Combo("Priority", &esp::cfg.aim_priority, pri, 2);
            Section("Status");
            ImGui::Text("Target: %s", aimbot::g_target ? "Locked" : "None");
            ImGui::Text("Screen: %.0fx%.0f", aimbot::g_screen.x, aimbot::g_screen.y);
            EndFrame();
        }
    }

    void render_esp_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##esp_a", { fs.x * 0.5f, fs.y })) {
            Section("ESP");
            Checkbox("Enable ESP", &esp::cfg.enabled);
            const char* styles[] = { "Off", "Corner", "Full" };
            ImGui::Combo("Box style", &esp::cfg.box_style, styles, 3);
            Checkbox("Filled background", &esp::cfg.draw_filled);
            SliderFloat("Box thickness", &esp::cfg.box_thickness, 0.5f, 5.f, "%.1f");
            SliderFloat("Max distance", &esp::cfg.max_dist, 50.f, 10000.f, "%.0f");
            Section("Info");
            Checkbox("Names", &esp::cfg.draw_name);
            Checkbox("Health bar", &esp::cfg.draw_health);
            Checkbox("Distance", &esp::cfg.draw_distance);
            Checkbox("Head dot", &esp::cfg.draw_head_dot);
            Checkbox("Skeleton", &esp::cfg.draw_skeleton);
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##esp_b", { -1.f, fs.y })) {
            Section("Lines");
            Checkbox("Snaplines", &esp::cfg.draw_line);
            const char* snap[] = { "Bottom", "Center", "Top" };
            ImGui::Combo("Snap origin", &esp::cfg.snapline_pos, snap, 3);
            Section("Overlays");
            Checkbox("FOV circle", &esp::cfg.draw_fov);
            SliderFloat("FOV radius", &esp::cfg.fov_radius, 10.f, 1000.f, "%.0f");
            Checkbox("Crosshair", &esp::cfg.draw_crosshair);
            Section("Colors");
            ImGui::ColorEdit4("Box", &esp::cfg.col_box.x);
            ImGui::ColorEdit4("Name", &esp::cfg.col_name.x);
            ImGui::ColorEdit4("Line", &esp::cfg.col_line.x);
            EndFrame();
        }
    }

    void render_visuals_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##vis_a", { fs.x * 0.5f, fs.y })) {
            Section("World");
            Checkbox("Fullbright", &esp::cfg.fullbright);
            Checkbox("No skybox", &esp::cfg.no_skybox);
            Checkbox("No fog", &esp::cfg.world_nofog);
            Checkbox("Custom brightness", &esp::cfg.world_bright_enabled);
            SliderFloat("Brightness", &esp::cfg.world_bright, 0.f, 10.f, "%.1f");
            Section("Time");
            Checkbox("Time of day", &esp::cfg.world_time_enabled);
            SliderFloat("Clock", &esp::cfg.world_time, 0.f, 24.f, "%.1f");
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##vis_b", { -1.f, fs.y })) {
            Section("Camera");
            Checkbox("FOV changer", &esp::cfg.fov_changer_enabled);
            SliderFloat("Field of view", &esp::cfg.fov_value, 40.f, 120.f, "%.0f");
            Section("Effects");
            Checkbox("No blur", &esp::cfg.vis_no_blur);
            Checkbox("No depth of field", &esp::cfg.vis_no_dof);
            Checkbox("No sun rays", &esp::cfg.vis_no_sunrays);
            Checkbox("No atmosphere", &esp::cfg.vis_no_atmosphere);
            EndFrame();
        }
    }

    void render_misc_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##misc_a", { fs.x * 0.5f, fs.y })) {
            Section("Movement");
            Checkbox("Fly", &esp::cfg.fly_enabled);
            Keybind("Fly key", &esp::cfg.key_fly);
            SliderFloat("Fly speed", &esp::cfg.fly_speed, 10.f, 300.f, "%.0f");
            Checkbox("WalkSpeed", &esp::cfg.speed_enabled);
            SliderFloat("Speed", &esp::cfg.speed_value, 16.f, 200.f, "%.0f");
            Checkbox("JumpPower", &esp::cfg.jump_enabled);
            SliderFloat("Jump", &esp::cfg.jump_value, 50.f, 500.f, "%.0f");
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##misc_b", { -1.f, fs.y })) {
            Section("Character");
            Checkbox("No-clip", &esp::cfg.noclip_enabled);
            Keybind("No-clip key", &esp::cfg.key_noclip);
            Checkbox("Infinite jump", &esp::cfg.infinite_jump);
            Checkbox("Godmode", &esp::cfg.godmode);
            Checkbox("Hover", &esp::cfg.hover_enabled);
            SliderFloat("Hover height", &esp::cfg.hover_height, 2.f, 30.f, "%.1f");
            Section("Anti-Aim");
            Checkbox("Spinbot", &esp::cfg.antiaim_spin);
            SliderFloat("Spin speed", &esp::cfg.antiaim_spin_speed, 90.f, 1440.f, "%.0f");
            EndFrame();
        }
    }

    void render_config_tab() {
        ImVec2 fs = ImGui::GetContentRegionAvail();
        if (BeginFrame("##cfg_a", { fs.x * 0.5f, fs.y })) {
            Section("Config");
            if (ImGui::Button("Save config", { 120.f, 28.f })) { esp::save_config((mem::exe_dir() + "\\config.bin").c_str()); add_console("Config saved"); }
            ImGui::SameLine();
            if (ImGui::Button("Load config", { 120.f, 28.f })) { esp::load_config((mem::exe_dir() + "\\config.bin").c_str()); add_console("Config loaded"); }
            Section("Theme");
            ImGui::ColorEdit4("Accent", &g_th.accent.Value.x);
            Section("Keybinds");
            Keybind("Menu toggle", &esp::cfg.key_esp);
            Keybind("Panic", &esp::cfg.key_panic);
            EndFrame();
        }
        ImGui::SameLine(0.f, 10.f);
        if (BeginFrame("##cfg_b", { -1.f, fs.y })) {
            Section("Console");
            Checkbox("Show console", &esp::cfg.show_console);
            if (ImGui::Button("Clear console", { 100.f, 24.f })) g_console_lines.clear();
            ImGui::BeginChild("##console", { -1.f, -1.f }, true);
            for (auto& line : g_console_lines) ImGui::TextUnformatted(line.c_str());
            if (g_console_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
            ImGui::EndChild();
            EndFrame();
        }
    }

    void render_boot_screen() {
        ImVec2 ws = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({ 0, 0 }); ImGui::SetNextWindowSize(ws);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 1.f));
        ImGui::Begin("##boot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Spinner
        float t = (float)ImGui::GetTime();
        ImVec2 center(ws.x * 0.5f, ws.y * 0.4f);
        for (int i = 0; i < 12; i++) {
            float a = t * 3.f + i * 3.14159f / 6.f;
            float alpha = 1.f - (float)i / 12.f;
            dl->AddCircleFilled({ center.x + cosf(a) * 30.f, center.y + sinf(a) * 30.f }, 3.f, col_a(g_th.accent, alpha));
        }
        // Status text
        const char* phase = scanner::phase_name(scanner::g_phase.load());
        ImVec2 ts = ImGui::CalcTextSize(phase);
        dl->AddText({ center.x - ts.x * 0.5f, center.y + 50.f }, col(g_th.text_dim), phase);
        // Scan feed
        auto feed = scanner::feed_snapshot();
        float y = center.y + 80.f;
        for (auto it = feed.rbegin(); it != feed.rend() && y < ws.y - 40.f; ++it) {
            dl->AddText({ center.x - 200.f, y }, it->hit ? col(g_th.ok) : col(g_th.text_dimmer), it->text.c_str());
            y += 16.f;
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }
}

namespace menu {
    void init() { g_screen = Screen::Menu; }
    void set_visible(bool v) { g_visible = v; }
    bool is_visible() { return g_visible; }
    bool is_injected() { return g_injected; }
    void set_scan_ok(bool ok) { g_scan_ok = ok; }

    void render() {
        poll_keybind_listen();
        if (g_screen == Screen::Boot) {
            if (g_scan_ok.load()) { g_screen = Screen::Menu; g_injected = true; }
            render_boot_screen();
            return;
        }
        if (!g_visible.load()) return;
        ImVec2 ws = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({ 0, 0 }); ImGui::SetNextWindowSize(ws);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::Begin("##bg", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End(); ImGui::PopStyleColor();

        // Main window
        float wx = (ws.x - g_th.win_w) * 0.5f, wy = (ws.y - g_th.win_h) * 0.5f;
        ImGui::SetNextWindowPos({ wx, wy }); ImGui::SetNextWindowSize({ g_th.win_w, g_th.win_h });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, g_th.winbg.Value); ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, g_th.win_round); ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::Begin("RiadExternal", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Sidebar
        ImVec2 sp = ImGui::GetWindowPos(); ImVec2 ss = ImGui::GetWindowSize();
        dl->AddRectFilled({ sp.x, sp.y }, { sp.x + g_th.sidebar_w, sp.y + ss.y }, col(g_th.sidebar), g_th.win_round, ImDrawFlags_RoundCornersLeft);
        // Logo
        dl->AddText({ sp.x + 20.f, sp.y + 18.f }, col(g_th.accent), "RiadExternal");
        dl->AddText({ sp.x + 20.f, sp.y + 38.f }, col(g_th.text_dimmer), "v1.0");

        // Nav items
        const char* items[] = { "Scripts", "Aimbot", "ESP", "Visuals", "Misc", "Config" };
        float iy = sp.y + 70.f;
        for (int i = 0; i < 6; i++) {
            ImGui::SetCursorScreenPos({ sp.x + 10.f, iy });
            ImGui::PushID(i);
            ImRect bb({ sp.x + 10.f, iy }, { sp.x + g_th.sidebar_w - 10.f, iy + g_th.tab_h });
            bool hovered = hit(bb); bool active = (g_active_tab == i);
            float tact = anim(i, active ? 1.f : 0.f, 20.f); float thov = anim(i ^ 0x100, hovered ? 1.f : 0.f, 20.f);
            if (tact > 0.02f || thov > 0.02f) dl->AddRectFilled(bb.Min, bb.Max, col_a(active ? g_th.accent : g_th.child_hover, active ? 0.15f * tact : 0.3f * thov), 6.f);
            if (active) dl->AddRectFilled({ bb.Min.x, bb.Min.y + 6.f }, { bb.Min.x + 3.f, bb.Max.y - 6.f }, col_a(g_th.accent, tact), 1.f);
            dl->AddText({ bb.Min.x + 16.f, bb.Min.y + (g_th.tab_h - ImGui::GetFontSize()) * 0.5f }, active ? col(g_th.accent) : col(g_th.text_dim), items[i]);
            if (ImGui::Button("##nav", { g_th.sidebar_w - 20.f, g_th.tab_h })) g_active_tab = i;
            ImGui::PopID();
            iy += g_th.tab_h + 4.f;
        }

        // Content area
        ImGui::SetCursorScreenPos({ sp.x + g_th.sidebar_w + 10.f, sp.y + 10.f });
        ImGui::BeginChild("##content", { ss.x - g_th.sidebar_w - 20.f, ss.y - 20.f }, false);
        switch (g_active_tab) {
            case 0: render_scripts_tab(); break;
            case 1: render_aimbot_tab(); break;
            case 2: render_esp_tab(); break;
            case 3: render_visuals_tab(); break;
            case 4: render_misc_tab(); break;
            case 5: render_config_tab(); break;
        }
        ImGui::EndChild();

        // Status bar
        dl->AddRectFilled({ sp.x + g_th.sidebar_w, sp.y + ss.y - 28.f }, { sp.x + ss.x, sp.y + ss.y }, col(g_th.sidebar), g_th.win_round, ImDrawFlags_RoundCornersRight);
        char status[128]; snprintf(status, sizeof(status), "PID: %lu  |  %s  |  RSHIFT: toggle  END: exit", mem::g_pid, executor::ready() ? "Attached" : "Waiting...");
        dl->AddText({ sp.x + g_th.sidebar_w + 16.f, sp.y + ss.y - 22.f }, col(g_th.text_dim), status);

        ImGui::End();
        ImGui::PopStyleVar(2); ImGui::PopStyleColor();
    }
}
