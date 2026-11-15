// esp.h — player ESP, skeleton, loot, radar

#pragma once
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include <vector>
#include <string>

struct PlayerData {
    uintptr_t base   = 0;
    Vec3      head;
    Vec3      feet;
    Vec2      scrHead;
    Vec2      scrFeet;
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
        bool lootESP        = false;
        bool radarESP       = false;
        float maxDist       = 300.f; // meters
        float radarRadius   = 80.f;
        float radarScale    = 0.5f;  // px per meter
        bool teamCheck      = false; // skip same-team
        uint32_t colPolice    = 0xFF4488FF; // ABGR
        uint32_t colCriminal  = 0xFF4466FF;
        uint32_t colLoot      = 0xFF44FF44;
    } cfg;

    std::vector<PlayerData> players;
    std::vector<LootData>   loot;

    void update(const Memory& mem, const Matrix4x4& vp, float sw, float sh) {
        players.clear();
        loot.clear();

        // ── player list ───────────────────────────────────────────────────────
        uintptr_t listPtr = mem.read<uintptr_t>(mem.base + Offsets::PlayerList);
        if (!listPtr) return;
        int count = mem.read<int32_t>(listPtr + Offsets::PlayerListSize);
        uintptr_t items = mem.read<uintptr_t>(listPtr + Offsets::PlayerListItems);
        count = std::min(count, 64);

        for (int i = 0; i < count; i++) {
            uintptr_t obj = mem.read<uintptr_t>(items + 0x20 + i * 8);
            if (!obj) continue;

            bool isLocal = mem.read<bool>(obj + Offsets::PlayerIsLocal);
            if (isLocal) continue;

            PlayerData pd;
            pd.base  = obj;
            pd.team  = mem.read<int32_t>(obj + Offsets::PlayerTeam);
            pd.health = mem.read<float>(obj + Offsets::PlayerHealth);
            pd.maxHp  = mem.read<float>(obj + Offsets::PlayerHealthMax);
            if (pd.health <= 0.f) continue;

            // transform → position
            uintptr_t xform = mem.read<uintptr_t>(obj + Offsets::PlayerTransform);
            // Unity Transform position is at a fixed offset inside the internal TransformData
            // FILL: verify offset via pattern scan or CE on transform.position
            uintptr_t xformInternal = mem.read<uintptr_t>(xform + 0x38);
            pd.feet = mem.read<Vec3>(xformInternal + 0x90);
            pd.head = { pd.feet.x, pd.feet.y + 1.75f, pd.feet.z }; // approx head height

            if (!worldToScreen(pd.head, pd.scrHead, vp, sw, sh)) continue;
            if (!worldToScreen(pd.feet, pd.scrFeet, vp, sw, sh)) continue;

            pd.dist = (pd.feet - Vec3{}).len(); // FILL: subtract local player pos
            if (pd.dist > cfg.maxDist) continue;

            uintptr_t namePtr = mem.read<uintptr_t>(obj + Offsets::PlayerName);
            pd.name = namePtr ? mem.readString(namePtr) : "?";

            players.push_back(pd);
        }

        // ── loot ─────────────────────────────────────────────────────────────
        if (cfg.lootESP) {
            uintptr_t lootList = mem.read<uintptr_t>(mem.base + Offsets::LootList);
            if (!lootList) return;
            int lcount = std::min(mem.read<int32_t>(lootList + 0x18), 256);
            uintptr_t litems = mem.read<uintptr_t>(lootList + 0x10);
            for (int i = 0; i < lcount; i++) {
                uintptr_t obj = mem.read<uintptr_t>(litems + 0x20 + i*8);
                if (!obj) continue;
                LootData ld;
                uintptr_t xf = mem.read<uintptr_t>(obj + Offsets::LootTransform);
                uintptr_t xfi = mem.read<uintptr_t>(xf + 0x38);
                ld.world = mem.read<Vec3>(xfi + 0x90);
                if (!worldToScreen(ld.world, ld.scr, vp, sw, sh)) continue;
                uintptr_t namePtr = mem.read<uintptr_t>(obj + Offsets::LootItemName);
                ld.name = namePtr ? mem.readString(namePtr) : "item";
                loot.push_back(ld);
            }
        }
    }
};
