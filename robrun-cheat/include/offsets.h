// Rob Run — offsets
// Engine: Unity IL2CPP (Kefir)
// FILL: dump fresh offsets with il2cppdumper each patch
//   1. attach x64dbg to RobRun.exe
//   2. run il2cppdumper on GameAssembly.dll → dump.cs
//   3. grep for PlayerController, NetworkPlayer, CharacterStats

#pragma once
#include <cstdint>

namespace Offsets {

// ── Unity IL2CPP runtime ──────────────────────────────────────────────────────
// FILL: get from il2cpp.h pattern scan or hardcode from dump
inline constexpr uintptr_t Il2CppBase        = 0x0;    // GameAssembly.dll base, resolve at runtime

// ── Camera ───────────────────────────────────────────────────────────────────
// Camera.main → ViewMatrix / ProjectionMatrix
// FILL: find Camera static instance via il2cpp Class::GetStaticFieldData
inline constexpr uintptr_t CameraStaticField = 0x0;    // Camera$$main static ptr
inline constexpr uintptr_t ViewMatrix        = 0xEC;   // offset inside Camera component
inline constexpr uintptr_t ProjMatrix        = 0xFC;

// ── PlayerController ─────────────────────────────────────────────────────────
// FILL: grep dump.cs for "class PlayerController"
inline constexpr uintptr_t PlayerList        = 0x0;    // static List<PlayerController>
inline constexpr uintptr_t PlayerListItems   = 0x10;   // List._items (T[])
inline constexpr uintptr_t PlayerListSize    = 0x18;   // List._size

// per-player
inline constexpr uintptr_t PlayerTransform   = 0x28;   // PlayerController.transform
inline constexpr uintptr_t PlayerHealth      = 0x60;   // CharacterStats.health  (float)
inline constexpr uintptr_t PlayerHealthMax   = 0x64;   // CharacterStats.maxHealth
inline constexpr uintptr_t PlayerIsLocal     = 0x9C;   // bool isLocalPlayer
inline constexpr uintptr_t PlayerTeam        = 0x78;   // int team (0=police 1=criminal)
inline constexpr uintptr_t PlayerName        = 0x80;   // string displayName
inline constexpr uintptr_t PlayerVelocity    = 0x44;   // Rigidbody.velocity (Vector3)

// ── Skeleton / bones ─────────────────────────────────────────────────────────
// FILL: Unity HumanBodyBones enum → Animator.GetBoneTransform
inline constexpr int BoneHead    = 7;
inline constexpr int BoneNeck    = 6;
inline constexpr int BoneChest   = 2;
inline constexpr int BoneHips    = 0;
inline constexpr int BoneLFoot   = 16;
inline constexpr int BoneRFoot   = 19;

// ── Weapons ──────────────────────────────────────────────────────────────────
// FILL: grep "class WeaponController" or "class GunBase"
inline constexpr uintptr_t LocalWeapon       = 0x0;
inline constexpr uintptr_t WeaponRecoilPitch = 0x0;    // float recoilPitch
inline constexpr uintptr_t WeaponRecoilYaw   = 0x0;    // float recoilYaw
inline constexpr uintptr_t WeaponSpread      = 0x0;    // float currentSpread

// ── Loot / items ──────────────────────────────────────────────────────────────
// FILL: grep "class LootItem" or "class PickupBase"
inline constexpr uintptr_t LootList          = 0x0;
inline constexpr uintptr_t LootTransform     = 0x18;
inline constexpr uintptr_t LootItemName      = 0x28;

} // namespace Offsets
