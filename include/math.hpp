#pragma once
#include <cmath>
#include <initializer_list>

struct Vec2 { float x = 0.f, y = 0.f; };
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const { float l = length(); return l > 1e-6f ? Vec3{ x / l, y / l, z / l } : Vec3{}; }
};
struct Matrix4 { float m[16] = {}; };

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vec2 world_to_screen(const Matrix4& vm, const Vec3& pos, float w, float h) {
    float x = vm.m[0] * pos.x + vm.m[4] * pos.y + vm.m[8]  * pos.z + vm.m[12];
    float y = vm.m[1] * pos.x + vm.m[5] * pos.y + vm.m[9]  * pos.z + vm.m[13];
    float z = vm.m[2] * pos.x + vm.m[6] * pos.y + vm.m[10] * pos.z + vm.m[14];
    float ww = vm.m[3] * pos.x + vm.m[7] * pos.y + vm.m[11] * pos.z + vm.m[15];
    if (ww < 0.01f) return { -1.f, -1.f };
    return { (x / ww * 0.5f + 0.5f) * w, (0.5f - y / ww * 0.5f) * h };
}
