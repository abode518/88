// esp.h — player ESP (boxes, skeleton, health), loot, radar
// Unity IL2CPP  |  Rob Run  |  Nov 2026

#pragma once
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include <vector>
#include <string>
#include <array>

// ── bone IDs (Unity HumanBodyBones) ─────────────────────────────────────────
namespace Bone {
    enum : int {
        Hips=0, LeftUpperLeg=1, RightUpperLeg=2,
        LeftLowerLeg=3, RightLowerLeg=4,
        LeftFoot=5, RightFoot=6,
        Spine=7, Chest=8,
        Neck=9, Head=10,
        LeftShoulder=11, RightShoulder=12,
        LeftUpperArm=13, RightUpperArm=14,
        LeftLowerArm=15, RightLowerArm=16,
        LeftHand=17, RightHand=18,
        COUNT=19
    };
}

// skeleton edge list — pairs of bone IDs to connect with a line
inline constexpr std::pair<int,int> SKELETON_EDGES[] = {
    {Bone::Head,       Bone::Neck},
    {Bone::Neck,       Bone::Chest},
    {Bone::Chest,      Bone::Spine},
    {Bone::Spine,      Bone::Hips},
    // arms
    {Bone::Chest,      Bone::LeftShoulder},
    {Bone::LeftShoulder, Bone::LeftUpperArm},
    {Bone::LeftUpperArm, Bone::LeftLowerArm},
    {Bone::LeftLowerArm, Bone::LeftHand},
    {Bone::Chest,       Bone::RightShoulder},
    {Bone::RightShoulder,Bone::RightUpperArm},
    {Bone::RightUpperArm,Bone::RightLowerArm},
    {Bone::RightLowerArm,Bone::RightHand},
    // legs
    {Bone::Hips,         Bone::LeftUpperLeg},
    {Bone::LeftUpperLeg, Bone::LeftLowerLeg},
    {Bone::LeftLowerLeg, Bone::LeftFoot},
    {Bone::Hips,          Bone::RightUpperLeg},
    {Bone::RightUpperLeg, Bone::RightLowerLeg},
    {Bone::RightLowerLeg, Bone::RightFoot},
};

struct PlayerData {
    uintptr_t base   = 0;
    Vec3      head;
    Vec3      feet;
    Vec2      scrHead;
    Vec2      scrFeet;
    // skeleton
    Vec3      boneWorld[Bone::COUNT]{};
    Vec2      boneScr[Bone::COUNT]{};
    bool      boneValid[Bone::COUNT]{};
    // stats
    float     health  = 0.f;
    float     maxHp   = 100.f;
    int       team    = -1;      // 0=police 1=criminal
    std::string name;
    bool      visible = false;
    float     dist    = 0.f;
};

struct LootData {
    Vec3      world;
    Vec2      scr;
    std::string name;
    float     dist = 0.f;
};

class ESP {
public:
    struct Config {
        bool playerBoxes    = true;
        bool playerSkeleton = true;
        bool playerHealth   = true;
        bool playerName     = true;
        bool playerDist     = true;
        bool lootESP        = true;
        bool radarESP       = true;
        bool teamCheck      = false;
        float maxDist       = 300.f;
        float radarRadius   = 90.f;
        float radarScale    = 0.6f;
        uint32_t colPolice   = 0xFF88DDFF;
        uint32_t colCriminal = 0xFF4466FF;
        uint32_t colLoot     = 0xFF44FF88;
        uint32_t colSkel     = 0xAAFFFFFF;
    } cfg;

    std::vector<PlayerData> players;
    std::vector<LootData>   loot;

    // ── read Animator bone transforms ─────────────────────────────────────────
    // Unity Animator bone chain:
    // PlayerController.animator (Animator*) → m_Avatar → HumanBone[i] → Transform*
    //
    // Simpler runtime approach: walk the Transform hierarchy by name hash
    // OR: use the Animator ptr + offset table from il2cpp dump
    //
    // We use the Animator direct approach:
    // Animator* → m_HumanSkeletonIndexArray (ptr) → index array
    //           → m_SkeletonPosesArray (ptr) → poses
    // This is Unity-internal and version-dependent.
    //
    // Practical fallback: approximate bones from head/feet + fixed ratios
    static void fillBonesFallback(PlayerData& pd) {
        Vec3 h = pd.head, f = pd.feet;
        float dy = h.y - f.y;
        // approximate skeleton from two anchor points
        pd.boneWorld[Bone::Head]         = h;
        pd.boneWorld[Bone::Neck]         = {h.x, h.y - dy*0.07f, h.z};
        pd.boneWorld[Bone::Chest]        = {h.x, h.y - dy*0.25f, h.z};
        pd.boneWorld[Bone::Spine]        = {h.x, h.y - dy*0.42f, h.z};
        pd.boneWorld[Bone::Hips]         = {h.x, h.y - dy*0.52f, h.z};
        // arms
        float aw = dy * 0.18f;
        pd.boneWorld[Bone::LeftShoulder]  = {h.x - aw*0.3f, h.y - dy*0.22f, h.z};
        pd.boneWorld[Bone::LeftUpperArm]  = {h.x - aw*0.6f, h.y - dy*0.28f, h.z};
        pd.boneWorld[Bone::LeftLowerArm]  = {h.x - aw*0.9f, h.y - dy*0.37f, h.z};
        pd.boneWorld[Bone::LeftHand]      = {h.x - aw,      h.y - dy*0.44f, h.z};
        pd.boneWorld[Bone::RightShoulder] = {h.x + aw*0.3f, h.y - dy*0.22f, h.z};
        pd.boneWorld[Bone::RightUpperArm] = {h.x + aw*0.6f, h.y - dy*0.28f, h.z};
        pd.boneWorld[Bone::RightLowerArm] = {h.x + aw*0.9f, h.y - dy*0.37f, h.z};
        pd.boneWorld[Bone::RightHand]     = {h.x + aw,      h.y - dy*0.44f, h.z};
        // legs
        float lw = dy * 0.08f;
        pd.boneWorld[Bone::LeftUpperLeg]  = {h.x - lw, h.y - dy*0.55f, h.z};
        pd.boneWorld[Bone::LeftLowerLeg]  = {h.x - lw, h.y - dy*0.75f, h.z};
        pd.boneWorld[Bone::LeftFoot]      = {h.x - lw, f.y,             h.z};
        pd.boneWorld[Bone::RightUpperLeg] = {h.x + lw, h.y - dy*0.55f, h.z};
        pd.boneWorld[Bone::RightLowerLeg] = {h.x + lw, h.y - dy*0.75f, h.z};
        pd.boneWorld[Bone::RightFoot]     = {h.x + lw, f.y,             h.z};
        for (int i = 0; i < Bone::COUNT; i++) pd.boneValid[i] = true;
    }

    void update(const Memory& mem, const Matrix4x4& vp,
                float sw, float sh, const Vec3& localPos, int localTeam = -1)
    {
        players.clear();
        loot.clear();

        uintptr_t listPtr = mem.read<uintptr_t>(mem.base + Offsets::PlayerList);
        if (!listPtr) return;
        int   count = std::min(mem.read<int32_t>(listPtr + Offsets::PlayerListSize), 64);
        uintptr_t items = mem.read<uintptr_t>(listPtr + Offsets::PlayerListItems);

        for (int i = 0; i < count; i++) {
            uintptr_t obj = mem.read<uintptr_t>(items + 0x20 + i*8);
            if (!obj) continue;
            if (mem.read<bool>(obj + Offsets::PlayerIsLocal)) continue;

            PlayerData pd;
            pd.base   = obj;
            pd.team   = mem.read<int32_t>(obj + Offsets::PlayerTeam);

            // skip teammates
            if (cfg.teamCheck && localTeam != -1 && pd.team == localTeam) continue;

            pd.health = mem.read<float>(obj + Offsets::PlayerHealth);
            pd.maxHp  = mem.read<float>(obj + Offsets::PlayerHealthMax);
            if (pd.health <= 0.f) continue;

            // position
            uintptr_t xform  = mem.read<uintptr_t>(obj + Offsets::PlayerTransform);
            uintptr_t xformD = mem.read<uintptr_t>(xform + 0x38);
            pd.feet = mem.read<Vec3>(xformD + 0x90);
            pd.head = { pd.feet.x, pd.feet.y + 1.75f, pd.feet.z };

            if (!worldToScreen(pd.head, pd.scrHead, vp, sw, sh)) continue;
            if (!worldToScreen(pd.feet, pd.scrFeet, vp, sw, sh)) continue;

            // distance from local player
            Vec3 delta{ pd.feet.x - localPos.x, 0.f, pd.feet.z - localPos.z };
            pd.dist = delta.len();
            if (pd.dist > cfg.maxDist) continue;

            // skeleton — fill from approximation; upgrade to Animator read
            // once offsets are verified
            fillBonesFallback(pd);
            for (int b = 0; b < Bone::COUNT; b++) {
                pd.boneValid[b] = worldToScreen(pd.boneWorld[b], pd.boneScr[b], vp, sw, sh);
            }

            uintptr_t namePtr = mem.read<uintptr_t>(obj + Offsets::PlayerName);
            pd.name = namePtr ? mem.readString(namePtr) : "?";

            players.push_back(pd);
        }

        // loot
        if (cfg.lootESP) {
            uintptr_t ll = mem.read<uintptr_t>(mem.base + Offsets::LootList);
            if (!ll) goto done_loot;
            {
            int lc = std::min(mem.read<int32_t>(ll + 0x18), 256);
            uintptr_t li = mem.read<uintptr_t>(ll + 0x10);
            for (int i = 0; i < lc; i++) {
                uintptr_t obj = mem.read<uintptr_t>(li + 0x20 + i*8);
                if (!obj) continue;
                LootData ld;
                uintptr_t xf  = mem.read<uintptr_t>(obj + Offsets::LootTransform);
                uintptr_t xfd = mem.read<uintptr_t>(xf + 0x38);
                ld.world = mem.read<Vec3>(xfd + 0x90);
                if (!worldToScreen(ld.world, ld.scr, vp, sw, sh)) continue;
                Vec3 d{ld.world.x-localPos.x, 0.f, ld.world.z-localPos.z};
                ld.dist = d.len();
                if (ld.dist > cfg.maxDist) continue;
                uintptr_t np = mem.read<uintptr_t>(obj + Offsets::LootItemName);
                ld.name = np ? mem.readString(np) : "item";
                loot.push_back(ld);
            }
            } // end inner block
        }
        done_loot:;
    }
};
