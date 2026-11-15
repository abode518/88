// scanner.h — runtime pattern scan + auto-offset resolver
// بدل hardcoded offsets، يمسح GameAssembly.dll كل شغلة ويلقي الـ offsets بنفسه
// يبقى يشتغل بعد معظم الباتشات لأن الـ byte patterns ما تتغير كثير

#pragma once
#include "memory.h"
#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include <Windows.h>
#include <TlHelp32.h>

// ── pattern format: "48 8B ? ? ? ? 48 85 C0" — ? = wildcard ─────────────────
inline std::vector<std::pair<uint8_t,bool>> parsePattern(const char* pat) {
    std::vector<std::pair<uint8_t,bool>> result;
    const char* p = pat;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?') {
            result.push_back({0, false}); // wildcard
            p++; if (*p == '?') p++;
        } else {
            uint8_t b = static_cast<uint8_t>(strtol(p, nullptr, 16));
            result.push_back({b, true});
            p += 2;
        }
    }
    return result;
}

// scan a memory region (already dumped into buf) for a pattern
inline uintptr_t scanBuf(const uint8_t* buf, size_t sz,
                          const std::vector<std::pair<uint8_t,bool>>& pat,
                          uintptr_t baseAddr)
{
    size_t pLen = pat.size();
    for (size_t i = 0; i + pLen <= sz; i++) {
        bool ok = true;
        for (size_t j = 0; j < pLen; j++) {
            if (pat[j].second && buf[i+j] != pat[j].first) { ok = false; break; }
        }
        if (ok) return baseAddr + i;
    }
    return 0;
}

class Scanner {
public:
    // scan GameAssembly.dll for a pattern, return matched address in target process
    static uintptr_t find(const Memory& mem, const char* pattern,
                           const wchar_t* modName = L"GameAssembly.dll")
    {
        // get module info
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, mem.pid);
        MODULEENTRY32W me{ sizeof(me) };
        uintptr_t modBase = 0; size_t modSize = 0;
        if (Module32FirstW(snap, &me)) {
            do {
                if (!_wcsicmp(me.szModule, modName)) {
                    modBase = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    modSize = me.modBaseSize;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        if (!modBase) return 0;

        // read module into local buffer in 4MB chunks (avoid huge single RPM)
        auto pat = parsePattern(pattern);
        constexpr size_t CHUNK = 4 * 1024 * 1024;
        std::vector<uint8_t> buf(CHUNK);
        for (size_t off = 0; off < modSize; off += CHUNK) {
            size_t toRead = std::min(CHUNK, modSize - off);
            SIZE_T got = 0;
            ReadProcessMemory(mem.hProc,
                reinterpret_cast<LPCVOID>(modBase + off),
                buf.data(), toRead, &got);
            uintptr_t hit = scanBuf(buf.data(), got, pat, modBase + off);
            if (hit) return hit;
        }
        return 0;
    }

    // resolve a RIP-relative instruction: pattern points to the MOV/LEA,
    // offset_to_rel = bytes from match start to the 4-byte relative operand
    static uintptr_t resolveRIP(uintptr_t insnAddr, int offsetToRel, int insnLen) {
        // read the 4-byte relative offset
        int32_t rel = 0;
        memcpy(&rel, reinterpret_cast<void*>(insnAddr + offsetToRel), 4); // local addr won't work
        // must read from target process — use mem.read in call sites
        return insnAddr + insnLen + rel;
    }
};

// ── known patterns for Rob Run (Unity 2022 IL2CPP, Nov 2026 build) ────────────
// VERIFY / UPDATE after each patch — run update_offsets() on startup
// format: IDA-style byte pattern
namespace Patterns {
    // Camera.main static field ptr
    // lea rcx, [rip+????] ; mov rax, [rcx] ; test rax,rax ; ...
    inline constexpr const char* CameraMain =
        "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 80 ? ? ? ?";

    // NetworkPlayer list (static List<NetworkPlayer>)
    // mov rax, [rip+????] ; mov rcx, [rax+10] ; ...  (List._items)
    inline constexpr const char* PlayerList =
        "48 8B 0D ? ? ? ? 48 85 C9 74 ? 8B 51 18";

    // LocalPlayer static instance
    inline constexpr const char* LocalPlayer =
        "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 ? C3";

    // WeaponController current weapon (from LocalPlayer component)
    inline constexpr const char* WeaponCtrl =
        "48 8B 8F ? ? ? ? 48 85 C9 74 ? 48 8B 01 FF 50";

    // CharacterStats health field — find "health" property setter
    // mov [rcx+??], xmm0 ; vmovss xmm0 ...
    inline constexpr const char* HealthWrite =
        "F3 0F 11 81 ? ? ? ? F3 0F 10 05";

    // LootItem list
    inline constexpr const char* LootList =
        "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 40 18 85 C0 74";
}

// ── auto-resolve all offsets on startup ───────────────────────────────────────
#include "offsets.h"
namespace OffsetResolver {
    // call once after attach; populates Offsets:: values at runtime
    inline void resolve(const Memory& mem) {
        // CameraStaticField
        uintptr_t camHit = Scanner::find(mem, Patterns::CameraMain);
        if (camHit) {
            // read RIP-relative operand (bytes 3-6) + instruction len 7
            int32_t rel = mem.read<int32_t>(camHit + 3);
            const_cast<uintptr_t&>(Offsets::CameraStaticField) = (camHit + 7 + rel) - mem.base;
        }

        // PlayerList
        uintptr_t plHit = Scanner::find(mem, Patterns::PlayerList);
        if (plHit) {
            int32_t rel = mem.read<int32_t>(plHit + 3);
            const_cast<uintptr_t&>(Offsets::PlayerList) = (plHit + 7 + rel) - mem.base;
        }

        // LocalPlayer
        uintptr_t lpHit = Scanner::find(mem, Patterns::LocalPlayer);
        if (lpHit) {
            int32_t rel = mem.read<int32_t>(lpHit + 3);
            const_cast<uintptr_t&>(OffExt::LocalPlayer) = (lpHit + 7 + rel) - mem.base;
        }

        // health offset — extract field offset from the mov instruction
        uintptr_t hpHit = Scanner::find(mem, Patterns::HealthWrite);
        if (hpHit) {
            uint32_t fieldOff = mem.read<uint32_t>(hpHit + 4);
            const_cast<uintptr_t&>(Offsets::PlayerHealth) = fieldOff;
        }
    }
}
