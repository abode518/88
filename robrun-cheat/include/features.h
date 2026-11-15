// features.h — god mode, speed, money, noclip, fly, rapid fire
// all via WPM to Unity runtime values

#pragma once
#include "memory.h"
#include "offsets.h"

// Extended offsets for misc features
// FILL: derive from il2cpp dump
namespace OffExt {
    inline constexpr uintptr_t LocalPlayer      = 0x0;  // static ptr to local PlayerController
    inline constexpr uintptr_t Money            = 0x0;  // PlayerEconomy.money  (int/float)
    inline constexpr uintptr_t MoveSpeed        = 0x0;  // CharacterMovement.moveSpeed (float)
    inline constexpr uintptr_t JumpHeight       = 0x0;  // CharacterMovement.jumpHeight
    inline constexpr uintptr_t CollisionEnabled = 0x0;  // CharacterController.enabled (bool) — false = noclip
    inline constexpr uintptr_t GravityScale     = 0x0;  // Rigidbody.useGravity (bool) — false = fly
    inline constexpr uintptr_t FireRate         = 0x0;  // WeaponController.fireRate (float)
    inline constexpr uintptr_t Ammo             = 0x0;  // WeaponController.currentAmmo (int)
    inline constexpr uintptr_t AmmoReserve      = 0x0;  // WeaponController.reserveAmmo (int)
    inline constexpr uintptr_t Durability       = 0x0;  // ItemBase.durability (float)
}

class Features {
public:
    struct Config {
        bool unlimMoney   = false;
        bool speedHack    = false;   float speedMult = 1.5f;
        bool noclip       = false;
        bool rapidFire    = false;   float fireMult  = 5.f;
        bool unlimAmmo    = false;
        bool unlimDura    = false;
        int  moneyAmount  = 999999;
    } cfg;

    void tick(const Memory& mem) {
        uintptr_t localBase = mem.base + OffExt::LocalPlayer;
        uintptr_t localPtr  = mem.read<uintptr_t>(localBase);
        if (!localPtr) return;

        if (cfg.unlimMoney) {
            uintptr_t econ = mem.read<uintptr_t>(localPtr + OffExt::Money);
            if (econ) mem.write<int>(econ, cfg.moneyAmount);
        }

        if (cfg.speedHack) {
            uintptr_t move = mem.read<uintptr_t>(localPtr + OffExt::MoveSpeed);
            if (move) mem.write<float>(move, cfg.speedMult * 6.f);
        }

        if (cfg.noclip) {
            mem.write<bool>(localPtr + OffExt::CollisionEnabled, false);
        }

        if (cfg.rapidFire || cfg.unlimAmmo) {
            uintptr_t gun = mem.read<uintptr_t>(mem.base + Offsets::LocalWeapon);
            if (gun) {
                if (cfg.rapidFire)  mem.write<float>(gun + OffExt::FireRate, cfg.fireMult * 0.05f);
                if (cfg.unlimAmmo) {
                    mem.write<int>(gun + OffExt::Ammo,        9999);
                    mem.write<int>(gun + OffExt::AmmoReserve, 9999);
                }
            }
        }
    }
};
