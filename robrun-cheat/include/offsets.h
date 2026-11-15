// Rob Run — offsets  (Unity IL2CPP, Kefir, Steam App 4262460)
// All uintptr_t values are mutable — scanner.h writes them at runtime
// FILL: verify with il2cppdumper on GameAssembly.dll after each patch

#pragma once
#include <cstdint>

namespace Offsets {

// ── Camera ────────────────────────────────────────────────────────────────────
inline uintptr_t CameraStaticField = 0x0;   // Camera$$main static ptr (resolved by scanner)
inline uintptr_t ViewMatrix        = 0xEC;  // offset inside Camera component
inline uintptr_t ProjMatrix        = 0xFC;

// ── PlayerController ──────────────────────────────────────────────────────────
inline uintptr_t PlayerList      = 0x0;   // static List<PlayerController> (resolved)
inline uintptr_t PlayerListItems = 0x10;  // List._items (T[])
inline uintptr_t PlayerListSize  = 0x18;  // List._size

inline uintptr_t PlayerTransform  = 0x28; // Component.transform
inline uintptr_t PlayerHealth     = 0x60; // CharacterStats.health  (float)
inline uintptr_t PlayerHealthMax  = 0x64; // CharacterStats.maxHealth
inline uintptr_t PlayerIsLocal    = 0x9C; // bool isLocalPlayer
inline uintptr_t PlayerTeam       = 0x78; // int  team (0=police, 1=criminal)
inline uintptr_t PlayerName       = 0x80; // string displayName
inline uintptr_t PlayerVelocity   = 0x44; // Rigidbody.velocity (Vector3)

// ── Bone IDs (Unity HumanBodyBones) ───────────────────────────────────────────
// must stay constexpr int — used as array indices in esp.h Bone::enum
inline constexpr int BoneHead  = 10;
inline constexpr int BoneNeck  =  9;
inline constexpr int BoneChest =  8;
inline constexpr int BoneHips  =  0;
inline constexpr int BoneLFoot =  5;
inline constexpr int BoneRFoot =  6;

// ── Weapons ───────────────────────────────────────────────────────────────────
inline uintptr_t LocalWeapon       = 0x0;
inline uintptr_t WeaponRecoilPitch = 0x0;
inline uintptr_t WeaponRecoilYaw   = 0x0;
inline uintptr_t WeaponSpread      = 0x0;

// ── Loot ──────────────────────────────────────────────────────────────────────
inline uintptr_t LootList      = 0x0;
inline uintptr_t LootTransform = 0x18;
inline uintptr_t LootItemName  = 0x28;

} // namespace Offsets
