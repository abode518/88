// aimbot.h — silent aim + recoil control
// method: SendInput mouse delta (external, no WriteProcessMemory needed for aiming)
// recoil: write WeaponController.recoilPitch/Yaw back to 0 each frame

#pragma once
#include <Windows.h>
#include "esp.h"
#include "math.h"
#include "memory.h"
#include "offsets.h"
#include <optional>

class Aimbot {
public:
    struct Config {
        bool  enabled      = false;
        bool  holdKey      = true;
        int   key          = VK_RBUTTON;
        float fovPx        = 150.f;
        float smooth       = 6.f;
        int   targetBone   = Offsets::BoneHead; // 10 = Head
        bool  recoilComp   = true;
        bool  visCheck     = false;  // skip if no LOS (requires vis flag from ESP)
        bool  teamCheck    = true;
    } cfg;

    void run(const std::vector<PlayerData>& players, float sw, float sh,
             const Memory& mem, int localTeam = -1) {
        if (!cfg.enabled) return;
        bool keyDown = (GetAsyncKeyState(cfg.key) & 0x8000) != 0;
        if (cfg.holdKey && !keyDown) return;

        Vec2 center = screenCenter(sw, sh);
        const PlayerData* best = nullptr;
        float bestFov = cfg.fovPx;

        for (const auto& p : players) {
            // never aim at teammates
            if (cfg.teamCheck && localTeam != -1 && p.team == localTeam) continue;
            if (cfg.visCheck && !p.visible) continue;
            // use the configured bone if valid, fall back to scrHead
            Vec2 aimPt = (p.boneValid[cfg.targetBone])
                       ? p.boneScr[cfg.targetBone]
                       : p.scrHead;
            float fov = calcFov(center, aimPt);
            if (fov < bestFov) { bestFov = fov; best = &p; }
        }

        if (!best) return;

        Vec2 aimPt = (best->boneValid[cfg.targetBone])
                   ? best->boneScr[cfg.targetBone]
                   : best->scrHead;
        Vec2 delta = { aimPt.x - center.x, aimPt.y - center.y };
        // smooth
        delta.x /= cfg.smooth;
        delta.y /= cfg.smooth;

        moveMouse(static_cast<int>(delta.x), static_cast<int>(delta.y));
    }

    void recoilControl(const Memory& mem) {
        if (!cfg.recoilComp) return;
        uintptr_t gun = mem.read<uintptr_t>(mem.base + Offsets::LocalWeapon);
        if (!gun) return;
        // zero recoil & spread each frame
        // defeats server-side recoil compensation on pure client-authoritative servers
        // FILL: verify works with Rob Run's server auth model
        mem.write<float>(gun + Offsets::WeaponRecoilPitch, 0.f);
        mem.write<float>(gun + Offsets::WeaponRecoilYaw,   0.f);
        mem.write<float>(gun + Offsets::WeaponSpread,      0.f);
    }

private:
    static void moveMouse(int dx, int dy) {
        if (dx == 0 && dy == 0) return;
        INPUT input{};
        input.type           = INPUT_MOUSE;
        input.mi.dwFlags     = MOUSEEVENTF_MOVE;
        input.mi.dx          = dx;
        input.mi.dy          = dy;
        SendInput(1, &input, sizeof(INPUT));
    }
};
