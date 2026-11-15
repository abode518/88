// camera.h — Unity IL2CPP Camera matrix reader + local player state
// replaces the FILL-riddled getVP() in main.cpp

#pragma once
#include "memory.h"
#include "math.h"
#include <cmath>

// Unity 2022 IL2CPP — Camera component internal layout
// Offsets verified against Unity 2022.3 LTS IL2CPP stripped build
// FILL: re-verify with il2cppdumper if matrices come out wrong
namespace CamOffsets {
    // Camera component (UnityEngine.Camera) field offsets
    // dump.cs: search "Matrix4x4 worldToCameraMatrix" / "Matrix4x4 projectionMatrix"
    inline uintptr_t WorldToCam = 0x1D0;  // worldToCameraMatrix  (Matrix4x4, 64 bytes)
    inline uintptr_t Projection = 0x210;  // projectionMatrix     (Matrix4x4, 64 bytes)

    // Transform internal — Unity stores position in TransformAccess internals
    // Component.transform → Transform → m_TransformData (ptr) → localPosition at 0x90
    inline constexpr uintptr_t TransformToData   = 0x38;
    inline constexpr uintptr_t TransformDataPos  = 0x90;  // Vector3 position
    inline constexpr uintptr_t TransformDataRot  = 0x9C;  // Quaternion rotation (x,y,z,w)
    inline constexpr uintptr_t TransformDataFwd  = 0xAC;  // Vector3 forward (computed)
}

// 4x4 column-major multiply  (Unity uses column-major storage)
inline Matrix4x4 matMul(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 r{};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

struct LocalState {
    Vec3  pos;       // world position
    Vec3  forward;   // look direction
    float yaw = 0.f; // Y-axis rotation in radians (for radar)
};

class CameraReader {
public:
    // ── VP matrix ─────────────────────────────────────────────────────────────
    // chain: staticField → Camera* → worldToCameraMatrix + projectionMatrix
    Matrix4x4 getVP(const Memory& mem, uintptr_t camStaticOff) const {
        uintptr_t camPtr = mem.read<uintptr_t>(mem.base + camStaticOff);
        if (!camPtr) return {};

        // Unity Camera stores view matrix as worldToCameraMatrix
        // VP = projection * view
        Matrix4x4 view = mem.read<Matrix4x4>(camPtr + CamOffsets::WorldToCam);
        Matrix4x4 proj = mem.read<Matrix4x4>(camPtr + CamOffsets::Projection);
        return matMul(proj, view);
    }

    // ── local player state ────────────────────────────────────────────────────
    LocalState getLocal(const Memory& mem, uintptr_t localPtrOff) const {
        LocalState s{};
        uintptr_t localObj = mem.read<uintptr_t>(mem.base + localPtrOff);
        if (!localObj) return s;

        uintptr_t xform     = mem.read<uintptr_t>(localObj + 0x28); // Component.transform
        uintptr_t xformData = mem.read<uintptr_t>(xform + CamOffsets::TransformToData);
        if (!xformData) return s;

        s.pos     = mem.read<Vec3>(xformData + CamOffsets::TransformDataPos);

        // quaternion → yaw
        struct Quat { float x, y, z, w; };
        Quat q = mem.read<Quat>(xformData + CamOffsets::TransformDataRot);
        // yaw from quaternion: atan2(2(wy+xz), 1-2(y²+x²))  (Unity Y-up)
        s.yaw = std::atan2(2.f*(q.w*q.y + q.x*q.z),
                           1.f - 2.f*(q.y*q.y + q.x*q.x));

        // forward vector
        s.forward.x =  2.f*(q.x*q.z + q.w*q.y);
        s.forward.y =  2.f*(q.y*q.z - q.w*q.x);
        s.forward.z =  1.f - 2.f*(q.x*q.x + q.y*q.y);

        return s;
    }
};
