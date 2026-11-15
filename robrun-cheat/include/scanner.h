// scanner.h — runtime pattern scanner + offset resolver
// scans GameAssembly.dll in target process, writes results into offsets.h / features.h
// survives patches as long as surrounding byte patterns stay stable

#pragma once
#include "memory.h"
#include "offsets.h"
#include "features.h"
#include <vector>
#include <cstdint>
#include <Windows.h>
#include <TlHelp32.h>

// ── pattern parser ────────────────────────────────────────────────────────────
inline std::vector<std::pair<uint8_t,bool>> parsePattern(const char* pat) {
    std::vector<std::pair<uint8_t,bool>> r;
    while (*pat) {
        while (*pat == ' ') pat++;
        if (!*pat) break;
        if (*pat == '?') { r.push_back({0,false}); pat++; if (*pat=='?') pat++; }
        else             { r.push_back({(uint8_t)strtol(pat,nullptr,16),true}); pat+=2; }
    }
    return r;
}

inline uintptr_t scanBuf(const uint8_t* buf, size_t sz,
                          const std::vector<std::pair<uint8_t,bool>>& pat,
                          uintptr_t base)
{
    size_t n = pat.size();
    for (size_t i = 0; i + n <= sz; i++) {
        bool ok = true;
        for (size_t j = 0; j < n; j++)
            if (pat[j].second && buf[i+j] != pat[j].first) { ok=false; break; }
        if (ok) return base + i;
    }
    return 0;
}

class Scanner {
public:
    static uintptr_t find(const Memory& mem, const char* pattern,
                           const wchar_t* modName = L"GameAssembly.dll")
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, mem.pid);
        MODULEENTRY32W me{ sizeof(me) };
        uintptr_t modBase = 0; size_t modSize = 0;
        if (Module32FirstW(snap, &me)) do {
            if (!_wcsicmp(me.szModule, modName)) {
                modBase = (uintptr_t)me.modBaseAddr;
                modSize = me.modBaseSize;
                break;
            }
        } while (Module32NextW(snap, &me));
        CloseHandle(snap);
        if (!modBase) return 0;

        auto pat = parsePattern(pattern);
        constexpr size_t CHUNK = 4*1024*1024;
        std::vector<uint8_t> buf(CHUNK);
        for (size_t off = 0; off < modSize; off += CHUNK) {
            size_t toRead = std::min(CHUNK, modSize - off);
            SIZE_T got = 0;
            ReadProcessMemory(mem.hProc, (LPCVOID)(modBase+off), buf.data(), toRead, &got);
            uintptr_t hit = scanBuf(buf.data(), got, pat, modBase+off);
            if (hit) return hit;
        }
        return 0;
    }
};

// ── signatures (Unity 2022 IL2CPP, Rob Run Nov 2026 build) ───────────────────
// FILL: re-verify after major patches using x64dbg + il2cppdumper
namespace Patterns {
    inline constexpr const char* CameraMain  = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 80 ? ? ? ?";
    inline constexpr const char* PlayerList  = "48 8B 0D ? ? ? ? 48 85 C9 74 ? 8B 51 18";
    inline constexpr const char* LocalPlayer = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 ? C3";
    inline constexpr const char* HealthWrite = "F3 0F 11 81 ? ? ? ? F3 0F 10 05";
    inline constexpr const char* LootList    = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 40 18 85 C0 74";
}

// reads a RIP-relative 4-byte operand from the target process and resolves the address
inline uintptr_t ripResolve(const Memory& mem, uintptr_t insnAddr,
                             int relOff, int insnLen)
{
    int32_t rel = mem.read<int32_t>(insnAddr + relOff);
    return insnAddr + insnLen + rel;
}

// ── auto-resolve — call once after attach ─────────────────────────────────────
namespace OffsetResolver {
    inline bool resolve(const Memory& mem) {
        bool allOk = true;

        // Camera static field
        uintptr_t h = Scanner::find(mem, Patterns::CameraMain);
        if (h) Offsets::CameraStaticField = ripResolve(mem, h, 3, 7) - mem.base;
        else   allOk = false;

        // PlayerList static
        h = Scanner::find(mem, Patterns::PlayerList);
        if (h) Offsets::PlayerList = ripResolve(mem, h, 3, 7) - mem.base;
        else   allOk = false;

        // LocalPlayer static
        h = Scanner::find(mem, Patterns::LocalPlayer);
        if (h) OffExt::LocalPlayer = ripResolve(mem, h, 3, 7) - mem.base;
        else   allOk = false;

        // health field offset — extracted from mov [rcx+??], xmm0
        h = Scanner::find(mem, Patterns::HealthWrite);
        if (h) Offsets::PlayerHealth = mem.read<uint32_t>(h + 4);
        else   allOk = false;

        // loot list
        h = Scanner::find(mem, Patterns::LootList);
        if (h) Offsets::LootList = ripResolve(mem, h, 3, 7) - mem.base;
        // loot missing is non-fatal

        return allOk;
    }
}
