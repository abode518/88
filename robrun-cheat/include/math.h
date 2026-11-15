// math.h — Vec2/Vec3/Matrix + world-to-screen

#pragma once
#include <cmath>
#include <array>

struct Vec2 {
    float x = 0, y = 0;
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    float len() const { return std::sqrt(x*x + y*y); }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    float len() const { return std::sqrt(x*x + y*y + z*z); }
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
};

// Unity column-major 4x4
struct Matrix4x4 {
    float m[4][4]{};
};

// Unity Camera.WorldToScreenPoint equivalent
// Unity uses a left-handed coordinate system; camera looks down -Z
inline bool worldToScreen(const Vec3& world, Vec2& screen,
                           const Matrix4x4& vp,
                           float screenW, float screenH)
{
    // clip space
    float cx = vp.m[0][0]*world.x + vp.m[0][1]*world.y + vp.m[0][2]*world.z + vp.m[0][3];
    float cy = vp.m[1][0]*world.x + vp.m[1][1]*world.y + vp.m[1][2]*world.z + vp.m[1][3];
    float cw = vp.m[3][0]*world.x + vp.m[3][1]*world.y + vp.m[3][2]*world.z + vp.m[3][3];
    if (cw < 0.1f) return false;  // behind camera
    float ndcX =  cx / cw;
    float ndcY = -cy / cw;
    screen.x = (ndcX + 1.0f) * 0.5f * screenW;
    screen.y = (ndcY + 1.0f) * 0.5f * screenH;
    return true;
}

inline float dist2D(Vec2 a, Vec2 b) {
    return (a - b).len();
}

inline Vec2 screenCenter(float w, float h) { return {w * 0.5f, h * 0.5f}; }

// angle between screen center and target for aimbot FOV check
inline float calcFov(Vec2 center, Vec2 target) {
    return dist2D(center, target);
}
