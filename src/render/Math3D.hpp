#pragma once

#include "core/rw/DffTypes.hpp"

#include <array>
#include <cmath>

namespace imgstudio::render {

using Vec3 = core::rw::Vec3f;
using Mat4 = core::rw::Mat4f;

[[nodiscard]] inline Vec3 vec3_add(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] inline Vec3 vec3_sub(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] inline Vec3 vec3_scale(const Vec3& a, float s) noexcept {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

[[nodiscard]] inline float vec3_dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 vec3_cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

[[nodiscard]] inline Vec3 vec3_normalize(const Vec3& v) noexcept {
    const float len = std::sqrt(vec3_dot(v, v));
    if (len <= 1e-6f) {
        return Vec3{0.0f, 0.0f, 1.0f};
    }
    return vec3_scale(v, 1.0f / len);
}

[[nodiscard]] inline Mat4 mat4_identity() noexcept {
    return Mat4{};
}

[[nodiscard]] inline Mat4 mat4_mul(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[static_cast<std::size_t>(k * 4 + row)] *
                       b.m[static_cast<std::size_t>(c * 4 + k)];
            }
            r.m[static_cast<std::size_t>(c * 4 + row)] = sum;
        }
    }
    return r;
}

[[nodiscard]] inline Mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar) noexcept {
    const float f = 1.0f / std::tan(fovy_rad * 0.5f);
    Mat4 r{};
    r.m.fill(0.0f);
    r.m[0] = f / (aspect > 1e-4f ? aspect : 1.0f);
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

[[nodiscard]] inline Mat4 mat4_look_at(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
    const Vec3 f = vec3_normalize(vec3_sub(center, eye));
    const Vec3 s = vec3_normalize(vec3_cross(f, up));
    const Vec3 u = vec3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -vec3_dot(s, eye);
    r.m[13] = -vec3_dot(u, eye);
    r.m[14] = vec3_dot(f, eye);
    return r;
}

} // namespace imgstudio::render
