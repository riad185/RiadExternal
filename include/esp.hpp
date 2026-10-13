#pragma once
#include <d3d11.h>
#include <imgui.h>
#include "math.hpp"
#include "roblox.hpp"

namespace esp {
    struct Config {
        bool  enabled       = false;
        int   box_style     = 1;
        bool  draw_filled   = false;
        bool  draw_name     = true;
        bool  draw_health   = true;
        bool  draw_distance = true;
        bool  draw_line     = false;
        int   snapline_pos  = 0;
        bool  draw_head_dot = false;
        bool  text_shadow   = true;
        float box_thickness = 1.5f;
        float max_dist      = 10000.f;
        ImVec4 col_box      = { 0.00f, 1.00f, 0.35f, 1.f };
        ImVec4 col_box_fill = { 0.00f, 1.00f, 0.35f, 0.2f };
        ImVec4 col_name     = { 1.00f, 1.00f, 1.00f, 1.f };
        ImVec4 col_line     = { 1.00f, 0.25f, 0.25f, 1.f };
        bool  draw_fov      = false;
        float fov_radius    = 100.f;
        ImVec4 col_fov      = { 1.00f, 1.00f, 1.00f, 0.30f };
        bool  draw_skeleton = false;
        ImVec4 col_skeleton = { 1.00f, 1.00f, 1.00f, 1.00f };
        bool  aim_enabled   = false;
        int   aim_key       = 0x02;
        float aim_strength  = 100.f;
        float aim_smooth    = 1.0f;
        bool  aim_lock      = true;
        int   aim_bone      = 0;
        bool  team_check    = false;
        bool  aim_visibility_check = false;
        int   aim_priority  = 0;
        float aim_max_dist  = 0.f;
        bool  speed_enabled = false;
        float speed_value   = 16.f;
        bool  jump_enabled  = false;
        float jump_value    = 50.f;
        bool  noclip_enabled = false;
        bool  infinite_jump  = false;
        bool  fly_enabled    = false;
        float fly_speed      = 60.f;
        bool  fov_changer_enabled = false;
        float fov_value     = 70.f;
        bool  antiaim_spin   = false;
        float antiaim_spin_speed = 720.f;
        bool  godmode        = false;
        bool  hover_enabled  = false;
        float hover_height   = 5.f;
        bool  jump_height_enabled = false;
        float jump_height    = 15.f;
        bool  radar_enabled  = false;
        float radar_size     = 110.f;
        float radar_range    = 150.f;
        bool  draw_hud       = false;
        bool  draw_watermark = false;
        bool  draw_crosshair = false;
        int   key_fly    = 0x70;
        int   key_noclip = 0x71;
        int   key_esp    = 0x73;
        int   key_aim    = 0;
        bool  streamproof = false;
        bool  show_console = false;
        float aim_deadzone = 0.f;
    };
    extern Config cfg;
    bool save_config(const char* path);
    bool load_config(const char* path);
    void notify(const std::string& msg);
    void set_view_origin(Vec2 origin);
    void render(ImDrawList* dl, const std::vector<PlayerInfo>& players, const Matrix4& vm, Vec2 screen);
    void render_console(const std::vector<PlayerInfo>& players, Vec2 screen);
}
