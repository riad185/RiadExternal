#pragma once
#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>

namespace runtime {
    inline uintptr_t ve_addr = 0;
    inline uintptr_t dm_addr = 0;
}

namespace offsets {
    inline std::string roblox_version = "unknown";

    namespace FakeDataModel {
        inline uintptr_t Pointer       = 0;
        inline uintptr_t RealDataModel = 0;
    }

    namespace VisualEngine {
        inline uintptr_t Dimensions    = 0;
        inline uintptr_t FakeDataModel = 0xA90;
        inline uintptr_t Pointer       = 0;
        inline uintptr_t RenderView    = 0xBB8;
        inline uintptr_t ViewMatrix    = 0;
    }

    namespace Instance {
        inline uintptr_t ClassDescriptor = 0x18;
        inline uintptr_t Children        = 0;
        inline uintptr_t Name            = 0;
        inline uintptr_t Parent          = 0;
    }

    namespace Player {
        inline uintptr_t Character = 0x2A0;
        inline uintptr_t Team      = 0;
        inline uintptr_t MinZoomDistance = 0x36C;
        inline uintptr_t MaxZoomDistance = 0x368;
        inline uintptr_t CameraMode      = 0x370;
        inline uintptr_t UserId          = 0x300;
        inline uintptr_t AccountAge      = 0x35C;
    }

    namespace Players {
        inline uintptr_t LocalPlayer = 0;
    }

    namespace ClassDescriptor {
        inline constexpr uintptr_t Name = 0x08;
    }

    namespace BasePart {
        inline uintptr_t Primitive = 0;
    }

    namespace Primitive {
        inline uintptr_t CFrame = 0;
        inline uintptr_t Position = 0;
        inline uintptr_t Size     = 0;
        inline uintptr_t AssemblyLinearVelocity = 0;
        inline uintptr_t AssemblyAngularVelocity = 0;
        inline uintptr_t PrimitiveFlags = 0;
    }

    namespace PrimitiveFlags {
        inline uint8_t Anchored   = 0x2;
        inline uint8_t CanCollide = 0x8;
    }

    namespace Humanoid {
        inline uintptr_t Health         = 0x188;
        inline uintptr_t MaxHealth      = 0x1A8;
        inline uintptr_t WalkSpeed       = 0x1D0;
        inline uintptr_t WalkSpeedCheck  = 0x3BC;
        inline uintptr_t JumpPower       = 0x1A4;
        inline uintptr_t PlatformStand   = 0x1DC;
        inline uintptr_t FloorMaterial   = 0x184;
        inline uintptr_t Jump            = 0x1DA;
        inline uintptr_t HipHeight       = 0x194;
        inline uintptr_t JumpHeight      = 0x1A0;
        inline uintptr_t MaxSlopeAngle   = 0x1AC;
    }

    namespace Workspace {
        inline uintptr_t CurrentCamera = 0x488;
        inline uintptr_t World         = 0x3E0;
    }

    namespace World {
        inline uintptr_t Gravity = 0x210;
        inline uintptr_t FallenPartsDestroyHeight = 0x208;
    }

    namespace Lighting {
        inline uintptr_t ClockTime      = 0x1A8;
        inline uintptr_t Brightness     = 0x110;
        inline uintptr_t FogEnd         = 0x124;
        inline uintptr_t OutdoorAmbient = 0xF8;
    }

    namespace Camera {
        inline uintptr_t CFrame        = 0xD8;
        inline uintptr_t FieldOfView   = 0x140;
        inline uintptr_t CameraType    = 0x138;
        inline uintptr_t CameraSubject = 0xC8;
    }

    namespace ValueBase {
        inline uintptr_t Value = 0xB8;
    }

    namespace MouseService {
        inline uintptr_t InputObject   = 0xF0;
        inline uintptr_t MousePosition = 0xD4;
    }

    inline bool load_txt(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::string line;
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(0, 1);
        };
        while (std::getline(f, line)) {
            auto eq = line.find('='); if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq); std::string val = line.substr(eq + 1);
            trim(key); trim(val); if (key.empty() || val.empty()) continue;
            uintptr_t v = 0; try { v = std::stoull(val, nullptr, 0); } catch (...) { continue; }
            if (v == 0) continue;
            if (key == "VisualEngine::Pointer")            VisualEngine::Pointer = v;
            else if (key == "VisualEngine::Dimensions")    VisualEngine::Dimensions = v;
            else if (key == "VisualEngine::ViewMatrix")    VisualEngine::ViewMatrix = v;
            else if (key == "VisualEngine::FakeDataModel") VisualEngine::FakeDataModel = v;
            else if (key == "FakeDataModel::Pointer")      FakeDataModel::Pointer = v;
            else if (key == "FakeDataModel::RealDataModel")FakeDataModel::RealDataModel = v;
            else if (key == "Instance::Name")              Instance::Name = v;
            else if (key == "Instance::Parent")            Instance::Parent = v;
            else if (key == "Instance::Children")          Instance::Children = v;
            else if (key == "Player::Character")           Player::Character = v;
            else if (key == "Players::LocalPlayer")        Players::LocalPlayer = v;
            else if (key == "Player::Team")                Player::Team = v;
            else if (key == "BasePart::Primitive")         BasePart::Primitive = v;
            else if (key == "Primitive::CFrame")           Primitive::CFrame = v;
            else if (key == "Primitive::Position")         Primitive::Position = v;
            else if (key == "Humanoid::Health")            Humanoid::Health = v;
            else if (key == "Humanoid::MaxHealth")         Humanoid::MaxHealth = v;
            else if (key == "Humanoid::WalkSpeed")         Humanoid::WalkSpeed = v;
            else if (key == "Humanoid::JumpPower")         Humanoid::JumpPower = v;
            else if (key == "Workspace::CurrentCamera")   Workspace::CurrentCamera = v;
            else if (key == "Workspace::World")            Workspace::World = v;
            else if (key == "World::Gravity")              World::Gravity = v;
            else if (key == "Lighting::ClockTime")         Lighting::ClockTime = v;
            else if (key == "Lighting::Brightness")        Lighting::Brightness = v;
            else if (key == "Lighting::FogEnd")            Lighting::FogEnd = v;
            else if (key == "Camera::CFrame")              Camera::CFrame = v;
            else if (key == "Camera::FieldOfView")         Camera::FieldOfView = v;
        }
        return VisualEngine::Pointer != 0 && Instance::Children != 0;
    }

    inline bool load_txt_content(const std::string& content) {
        std::istringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            auto eq = line.find('='); if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq); std::string val = line.substr(eq + 1);
            auto trim = [](std::string& s) {
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
                while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(0, 1);
            };
            trim(key); trim(val); if (key.empty() || val.empty()) continue;
            uintptr_t v = 0; try { v = std::stoull(val, nullptr, 0); } catch (...) { continue; }
            if (v == 0) continue;
            if (key == "VisualEngine::Pointer")            VisualEngine::Pointer = v;
            else if (key == "VisualEngine::Dimensions")    VisualEngine::Dimensions = v;
            else if (key == "VisualEngine::ViewMatrix")    VisualEngine::ViewMatrix = v;
            else if (key == "VisualEngine::FakeDataModel") VisualEngine::FakeDataModel = v;
            else if (key == "FakeDataModel::Pointer")      FakeDataModel::Pointer = v;
            else if (key == "FakeDataModel::RealDataModel")FakeDataModel::RealDataModel = v;
            else if (key == "Instance::Name")              Instance::Name = v;
            else if (key == "Instance::Parent")            Instance::Parent = v;
            else if (key == "Instance::Children")          Instance::Children = v;
            else if (key == "Player::Character")           Player::Character = v;
            else if (key == "Players::LocalPlayer")        Players::LocalPlayer = v;
            else if (key == "Player::Team")                Player::Team = v;
            else if (key == "BasePart::Primitive")         BasePart::Primitive = v;
            else if (key == "Primitive::CFrame")           Primitive::CFrame = v;
            else if (key == "Primitive::Position")         Primitive::Position = v;
            else if (key == "Humanoid::Health")            Humanoid::Health = v;
            else if (key == "Humanoid::MaxHealth")         Humanoid::MaxHealth = v;
            else if (key == "Humanoid::WalkSpeed")         Humanoid::WalkSpeed = v;
            else if (key == "Humanoid::JumpPower")         Humanoid::JumpPower = v;
            else if (key == "Workspace::CurrentCamera")   Workspace::CurrentCamera = v;
            else if (key == "Workspace::World")            Workspace::World = v;
            else if (key == "World::Gravity")              World::Gravity = v;
            else if (key == "Lighting::ClockTime")         Lighting::ClockTime = v;
            else if (key == "Lighting::Brightness")        Lighting::Brightness = v;
            else if (key == "Lighting::FogEnd")            Lighting::FogEnd = v;
            else if (key == "Camera::CFrame")              Camera::CFrame = v;
            else if (key == "Camera::FieldOfView")         Camera::FieldOfView = v;
        }
        return VisualEngine::Pointer != 0 && Instance::Children != 0;
    }
}
