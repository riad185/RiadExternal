#include "../include/memory.hpp"
#include "../include/offsets.hpp"
#include "../include/roblox.hpp"
#include "../include/overlay.hpp"
#include "../include/esp.hpp"
#include "../include/math.hpp"
#include "../include/aimbot.hpp"
#include "../include/features.hpp"
#include "../include/scanner.hpp"
#include "../include/log.hpp"
#include "../include/luavm.hpp"
#include "../include/executor.hpp"
#include "../include/menu.hpp"
#include "../include/teleport.hpp"

#include <Windows.h>
#include <imgui.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <algorithm>

std::vector<PlayerInfo> g_players;
Matrix4                 g_view_matrix{};
Vec2                    g_screen{};
uintptr_t               g_render_view = 0;
std::atomic<bool>       g_menu_open{ true };
std::atomic<uintptr_t>  g_local_team{ 0 };
std::atomic<uintptr_t>  g_aim_target{ 0 };
std::atomic<bool>       g_scan_done{ false };
std::atomic<bool>       g_scan_ok{ false };

std::vector<PlayerInfo> g_players_temp;
std::mutex             g_players_mtx;

static HWND find_roblox_window(DWORD pid) {
    struct FD { DWORD pid; HWND hwnd; } fd{ pid, nullptr };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* fd = (FD*)lp; DWORD wpid = 0; GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid == fd->pid && IsWindowVisible(hwnd)) { char title[64]{}; GetWindowTextA(hwnd, title, sizeof(title)); RECT rc{}; GetClientRect(hwnd, &rc); if (rc.right > 200 && rc.bottom > 200) { fd->hwnd = hwnd; return FALSE; } }
        return TRUE;
    }, (LPARAM)&fd);
    return fd.hwnd;
}

static void list_reader_thread() {
    while (overlay::is_running()) {
        if (!mem::alive()) break;
        RbxDataModel dm = RbxDataModel::get();
        if (dm.valid()) {
            auto players = collect_player_instances(dm);
            uintptr_t lteam = 0;
            if (offsets::Player::Team) { RbxInstance psvc = dm.get_service("Players"); if (psvc.valid()) { uintptr_t lp = rpm<uintptr_t>(psvc.ptr + offsets::Players::LocalPlayer); if (lp) lteam = rpm<uintptr_t>(lp + offsets::Player::Team); } }
            g_local_team = lteam;
            std::lock_guard<std::mutex> lock(g_players_mtx);
            std::unordered_map<uintptr_t, size_t> ix; ix.reserve(g_players_temp.size() * 2 + 1);
            for (size_t i = 0; i < g_players_temp.size(); i++) ix.emplace(g_players_temp[i].player_ptr, i);
            const size_t orig_end = g_players_temp.size(); std::vector<uint8_t> keep(orig_end, 0);
            for (auto& fresh : players) { auto it = ix.find(fresh.player_ptr); if (it != ix.end()) { g_players_temp[it->second].name = std::move(fresh.name); keep[it->second] = 1; } else { g_players_temp.push_back(std::move(fresh)); } }
            size_t write = 0; for (size_t read = 0; read < orig_end; read++) { if (!keep[read]) continue; if (write != read) g_players_temp[write] = std::move(g_players_temp[read]); write++; }
            if (write != orig_end) { const size_t appended = g_players_temp.size() - orig_end; for (size_t k = 0; k < appended; k++) g_players_temp[write + k] = std::move(g_players_temp[orig_end + k]); g_players_temp.resize(write + appended); }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

static void position_reader_thread() {
    while (overlay::is_running()) {
        if (!mem::alive()) break;
        RbxVisualEngine ve = RbxVisualEngine::get();
        if (ve.valid()) { ve.read_view_matrix(g_view_matrix); g_screen = ve.dimensions(); g_render_view = ve.render_view(); }
        { std::lock_guard<std::mutex> lock(g_players_mtx);
            for (auto& p : g_players_temp) {
                if (p.player_ptr) {
                    p.team_ptr = offsets::Player::Team ? rpm<uintptr_t>(p.player_ptr + offsets::Player::Team) : 0;
                    if (offsets::Player::UserId) p.user_id = rpm<long long>(p.player_ptr + offsets::Player::UserId);
                    uintptr_t char_ptr = rpm<uintptr_t>(p.player_ptr + offsets::Player::Character);
                    if (!char_ptr) { RbxDataModel dm = RbxDataModel::get(); if (dm.valid()) { RbxInstance ws = dm.get_service("Workspace"); if (ws.valid()) { RbxInstance char_root = ws.find_child_by_name("CharacterRoot"); RbxInstance candidate; if (char_root.valid()) { candidate = char_root.find_child_by_name("Player"); if (!candidate.valid()) candidate = char_root.find_child_by_name(p.name); } if (!candidate.valid()) candidate = ws.find_child_by_name("Player"); if (!candidate.valid()) candidate = ws.find_child_by_name(p.name); if (candidate.valid() && candidate.get_class() == "Model") char_ptr = candidate.ptr; } } }
                    if (char_ptr) {
                        if (char_ptr != p.cached_char_ptr) { p.cached_char_ptr = char_ptr; p.hrp_ptr = 0; p.head_ptr = 0; p.humanoid_ptr = 0; p.valid = false; p.joint_valid_mask = 0; for (int i = 0; i < J_COUNT; i++) p.joint_prim[i] = 0; p.seen_dead = false; }
                        if (!p.joint_valid_mask) { RbxInstance character{ char_ptr }; struct J { const char* name; int idx; }; static const J MAP[] = { {"Head", J_Head}, {"UpperTorso", J_UpperTorso}, {"LowerTorso", J_LowerTorso}, {"Torso", J_UpperTorso}, {"HumanoidRootPart", J_HRP}, {"LeftUpperArm", J_LUArm}, {"LeftLowerArm", J_LLArm}, {"LeftHand", J_LHand}, {"RightUpperArm", J_RUArm}, {"RightLowerArm", J_RLArm}, {"RightHand", J_RHand}, {"LeftUpperLeg", J_LULeg}, {"LeftLowerLeg", J_LLLeg}, {"LeftFoot", J_LFoot}, {"RightUpperLeg", J_RULeg}, {"RightLowerLeg", J_RLLeg}, {"RightFoot", J_RFoot}, {"Left Arm", J_LLArm}, {"Right Arm", J_RLArm}, {"Left Leg", J_LLLeg}, {"Right Leg", J_RLLeg} }; for (auto& c : character.get_children()) { std::string nm = c.get_name(); for (auto& m : MAP) { if (nm == m.name) { uintptr_t prim = rpm<uintptr_t>(c.ptr + offsets::BasePart::Primitive); if (prim) { p.joint_prim[m.idx] = prim; p.joint_valid_mask |= (1u << m.idx); } break; } } } }
                        for (int i = 0; i < J_COUNT; i++) { if (!p.joint_prim[i]) continue; p.joint_pos[i] = rpm<Vec3>(p.joint_prim[i] + offsets::Primitive::Position); }
                        if (!p.hrp_ptr) { RbxInstance character{ char_ptr }; p.hrp_ptr = character.find_child_by_name("HumanoidRootPart").ptr; }
                        if (p.hrp_ptr) { uintptr_t primitive = rpm<uintptr_t>(p.hrp_ptr + offsets::BasePart::Primitive); if (primitive) { p.position = rpm<Vec3>(primitive + offsets::Primitive::Position); if (offsets::Primitive::AssemblyLinearVelocity) p.velocity = rpm<Vec3>(primitive + offsets::Primitive::AssemblyLinearVelocity); p.valid = true; } else p.valid = false; } else p.valid = false;
                        if (!p.head_ptr) { RbxInstance character{ char_ptr }; p.head_ptr = character.find_child_by_name("Head").ptr; } if (p.head_ptr) { uintptr_t hprim = rpm<uintptr_t>(p.head_ptr + offsets::BasePart::Primitive); if (hprim) p.head_pos = rpm<Vec3>(hprim + offsets::Primitive::Position); else p.head_ptr = 0; }
                        if (!p.humanoid_ptr) { RbxInstance character{ char_ptr }; p.humanoid_ptr = character.find_child_by_class("Humanoid").ptr; } if (p.humanoid_ptr) { float new_hp = rpm<float>(p.humanoid_ptr + offsets::Humanoid::Health); float new_max = rpm<float>(p.humanoid_ptr + offsets::Humanoid::MaxHealth); if (p.prev_health > 0.f && new_hp + 0.5f < p.prev_health) p.last_hit_time = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); p.prev_health = new_hp; p.health = new_hp; p.max_health = new_max; }
                        { uintptr_t head_or_hrp = p.head_ptr ? p.head_ptr : p.hrp_ptr; if (head_or_hrp && offsets::Primitive::CFrame) { uintptr_t prim = rpm<uintptr_t>(head_or_hrp + offsets::BasePart::Primitive); if (prim) { float cf[9]{}; mem::vm_read(mem::g_proc, (LPCVOID)(prim + offsets::Primitive::CFrame), cf, sizeof(cf), nullptr); Vec3 look = { -cf[2], -cf[5], -cf[8] }; float l = look.length(); if (l > 1e-4f) p.char_look = { look.x / l, look.y / l, look.z / l }; } } }
                        p.role = 0; RbxInstance player{ p.player_ptr }; RbxInstance backpack = player.find_child_by_class("Backpack"); if (backpack.valid()) { for (auto& child : backpack.get_children()) { if (child.get_class() == "Tool") { std::string t_name = child.get_name(); if (t_name == "Knife") { p.role = 1; break; } else if (t_name == "Gun") { p.role = 2; break; } } } }
                        p.weapon.clear(); { RbxInstance character{ char_ptr }; for (auto& child : character.get_children()) { if (child.get_class() != "Tool") continue; std::string t_name = child.get_name(); if (p.weapon.empty()) p.weapon = t_name; if (p.role == 0) { if (t_name == "Knife") p.role = 1; else if (t_name == "Gun") p.role = 2; } } }
                    } else { p.valid = false; p.cached_char_ptr = 0; p.hrp_ptr = 0; p.humanoid_ptr = 0; p.role = 0; }
                }
            }
            g_players = g_players_temp;
        }
        feat::apply_self_mods();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    { char buf[MAX_PATH]{}; GetModuleFileNameA(nullptr, buf, MAX_PATH); std::string p(buf); auto slash = p.find_last_of("\\/"); if (slash != std::string::npos) SetCurrentDirectoryA(p.substr(0, slash).c_str()); }
    printf("[RiadExternal] Waiting for RobloxPlayerBeta.exe...\n");
    while (!mem::attach("RobloxPlayerBeta.exe")) Sleep(1000);
    mem::g_base = mem::get_module_base();
    printf("[RiadExternal] Attached. PID=%lu  Base=0x%llX\n", mem::g_pid, (unsigned long long)mem::g_base);
    elog::ok("attached to RobloxPlayerBeta.exe pid=%lu base=0x%llX", mem::g_pid, (unsigned long long)mem::g_base);
    if (mem::kdrv_ping()) { if (mem::kdrv_attach(mem::g_pid)) elog::ok("kernel: lunadrv attached to pid=%lu", mem::g_pid); else elog::warn("kernel: driver present but attach IOCTL failed, falling back to usermode"); }
    HWND roblox_hwnd = nullptr; for (int i = 0; i < 20; i++) { roblox_hwnd = find_roblox_window(mem::g_pid); if (roblox_hwnd) break; Sleep(500); } if (!roblox_hwnd) { MessageBoxA(nullptr, "Could not find Roblox window.", "LunaExternal", MB_OK | MB_ICONERROR); return 1; }
    if (!overlay::init(roblox_hwnd, hInst)) { MessageBoxA(nullptr, "Failed to create overlay.", "RiadExternal", MB_OK | MB_ICONERROR); return 1; }

    // Load embedded offsets
    offsets::load_txt_content(R"EMBED(
VisualEngine::Pointer = 0x8136228
FakeDataModel::Pointer = 0x8b79b58
FakeDataModel::RealDataModel = 0x1d8
VisualEngine::Dimensions = 0xae0
VisualEngine::ViewMatrix = 0x180
VisualEngine::FakeDataModel = 0xac0
Instance::Name = 0x40
Instance::Parent = 0x68
Instance::Children = 0x78
Player::Character = 0x298
Players::LocalPlayer = 0x130
Player::Team = 0x2d8
BasePart::Primitive = 0x188
Primitive::CFrame = 0xec
Primitive::Position = 0xec
Humanoid::Health = 0x190
Humanoid::MaxHealth = 0x1a8
Humanoid::WalkSpeed = 0x1d0
Humanoid::JumpPower = 0x1a4
Workspace::CurrentCamera = 0x498
Workspace::World = 0x3f0
World::Gravity = 0x210
Lighting::ClockTime = 0xc8
Lighting::Brightness = 0x118
Lighting::FogEnd = 0x12c
Camera::CFrame = 0xd8
Camera::FieldOfView = 0x140
)EMBED");

    luavm::load_content(R"EMBED(
luau_execute = 0x7FE910
luaO_nilobject = 0x5E47E60
luaH_dummynode = 0x5E47D00
luaD_throw = 0x7ED3D0
luaC_step = 0x7EE130
OpCodeLookupTable = 0x61987D0
luau_load = 0x7FFC00
LuaVM_Load = 0x1E63CD0
Print = 0x8273F0
ScriptContextResume = 0x1ECC890
ResumeFacet = 0x7E8
GetGlobalState = 0x1E456B0
GetLuaStateForInstance = 0x1F3C3B0
)EMBED", "embedded");

    // Start scanner
    std::thread([roblox_hwnd]() { for (int i = 0; i < 30; i++) { if (scanner::run(roblox_hwnd, true)) { g_scan_ok = true; break; } Sleep(1000); } g_scan_done = true; }).detach();

    std::string config_path = mem::exe_dir() + "\\config.bin";
    if (esp::load_config(config_path.c_str())) printf("[RiadExternal] Loaded saved config.\n");

    overlay::set_passthrough(false);
    std::thread(list_reader_thread).detach();
    std::thread(position_reader_thread).detach();
    std::thread([]() { while (overlay::is_running()) { if (!mem::alive()) break; feat::apply_movement(); std::this_thread::sleep_for(std::chrono::microseconds(500)); } }).detach();
    aimbot::start_thread();

    std::thread([]() { for (;;) { Sleep(750); if (!mem::alive()) { elog::info("watchdog: Roblox is gone, exiting."); ExitProcess(0); } } }).detach();

    menu::init();

    while (overlay::is_running()) {
        overlay::poll_messages();
        overlay::sync_to_target(roblox_hwnd);
        { HWND fg_now = GetForegroundWindow(); bool rob_focus = fg_now == roblox_hwnd || fg_now == overlay::g_hwnd; if (menu::is_injected() && rob_focus && (GetAsyncKeyState(VK_RSHIFT) & 1)) { bool open = !g_menu_open.load(); g_menu_open.store(open); overlay::set_passthrough(!open); } }
        if (GetAsyncKeyState(VK_END) & 1) { esp::save_config((mem::exe_dir() + "\\config.bin").c_str()); mem::kdrv_detach(); mem::kdrv_close(); overlay::quit(); break; }
        if (GetAsyncKeyState(esp::cfg.key_fly) & 1) { esp::cfg.fly_enabled = !esp::cfg.fly_enabled; esp::notify(esp::cfg.fly_enabled ? "Fly: ON" : "Fly: OFF"); }
        if (GetAsyncKeyState(esp::cfg.key_noclip) & 1) { esp::cfg.noclip_enabled = !esp::cfg.noclip_enabled; esp::notify(esp::cfg.noclip_enabled ? "No-Clip: ON" : "No-Clip: OFF"); }
        if (GetAsyncKeyState(esp::cfg.key_esp) & 1) { esp::cfg.enabled = !esp::cfg.enabled; esp::notify(esp::cfg.enabled ? "ESP: ON" : "ESP: OFF"); }
        if (esp::cfg.key_aim && (GetAsyncKeyState(esp::cfg.key_aim) & 1)) { esp::cfg.aim_enabled = !esp::cfg.aim_enabled; esp::notify(esp::cfg.aim_enabled ? "Aimbot: ON" : "Aimbot: OFF"); }
        static bool prev_sp = false; if (esp::cfg.streamproof != prev_sp) { overlay::set_streamproof(esp::cfg.streamproof); prev_sp = esp::cfg.streamproof; }
        overlay::begin_frame();
        HWND fg = GetForegroundWindow(); bool roblox_focused = !menu::is_injected() || fg == roblox_hwnd || fg == overlay::g_hwnd; if (!roblox_focused) { overlay::end_frame(); continue; }
        if (menu::is_injected()) {
            ImGuiIO& io = ImGui::GetIO(); ImGui::SetNextWindowPos({ 0.f, 0.f }); ImGui::SetNextWindowSize(io.DisplaySize); ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##esp", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            { RbxVisualEngine ve = RbxVisualEngine::get(); RbxDataModel dm = RbxDataModel::get(); int valid = 0; for (auto& p : g_players) if (p.valid) valid++; char line1[160]; snprintf(line1, sizeof(line1), "RiadExternal | ve:%s dm:%s | players:%d | RSHIFT=menu END=exit", ve.valid() ? "OK" : "FAIL", dm.valid() ? "OK" : "FAIL", valid); dl->AddText({ 11.f, 11.f }, IM_COL32(0, 0, 0, 220), line1); dl->AddText({ 10.f, 10.f }, IM_COL32(255, 255, 255, 255), line1); }
            { RbxVisualEngine ve_now = RbxVisualEngine::get(); if (ve_now.valid()) { ve_now.read_view_matrix(g_view_matrix); Vec2 dims_now = ve_now.dimensions(); if (dims_now.x > 0.f && dims_now.y > 0.f) g_screen = dims_now; } }
            { POINT client_origin{ 0, 0 }; if (roblox_hwnd && ClientToScreen(roblox_hwnd, &client_origin)) { RECT wr{}; if (GetWindowRect(roblox_hwnd, &wr)) esp::set_view_origin({ (float)(client_origin.x - wr.left), (float)(client_origin.y - wr.top) }); } }
            if (g_screen.x > 0.f && g_screen.y > 0.f) esp::render(dl, g_players, g_view_matrix, g_screen);
            ImGui::End();
        }
        bool need_cursor = g_menu_open.load() || esp::cfg.show_console || !menu::is_injected(); ImGui::GetIO().MouseDrawCursor = need_cursor;
        menu::set_scan_ok(g_scan_ok.load()); menu::set_visible(g_menu_open.load()); menu::render();
        if (menu::is_injected() && !menu::is_visible() && g_menu_open.load()) { g_menu_open.store(false); overlay::set_passthrough(true); }
        overlay::end_frame();
    }
    mem::detach();
    return 0;
}
