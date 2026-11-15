// antidbg.h — anti-debug + anti-VM + timing checks
// defeats: x64dbg attach, OllyDbg, Cheat Engine debugger, VM sandboxes

#pragma once
#include <Windows.h>
#include <intrin.h>
#include <cstdint>
#include <thread>
#include <chrono>

namespace AntiDbg {

inline bool isDebuggerPresent() {
    // IsDebuggerPresent (basic, most AV-aware debuggers patch this)
    if (IsDebuggerPresent()) return true;

    // NtGlobalFlag — set to 0x70 when process created by debugger
    // peb->NtGlobalFlag not in public SDK; read by offset (0xBC on x64)
    uintptr_t peb = __readgsqword(0x60);
    if (*reinterpret_cast<ULONG*>(peb + 0xBC) & 0x70) return true;

    // heap flags — x64 offsets: Flags=0x70, ForceFlags=0x74 (NOT 0x40/0x44 which are x86)
    PVOID heap = GetProcessHeap();
    DWORD flags      = *reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(heap) + 0x70);
    DWORD forceFlags = *reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(heap) + 0x74);
    if (flags & 0x40 || forceFlags != 0) return true;

    // RDTSC timing — debugger single-step causes large delta
    uint64_t t1 = __rdtsc();
    __nop(); __nop(); __nop();
    uint64_t t2 = __rdtsc();
    if ((t2 - t1) > 2000) return true; // too slow → being stepped

    return false;
}

inline bool isVM() {
    // CPUID hypervisor bit
    int info[4]{};
    __cpuid(info, 1);
    if (info[2] & (1 << 31)) return true; // hypervisor present

    // CPUID vendor string check
    __cpuid(info, 0x40000000);
    char vendor[13]{};
    memcpy(vendor,     &info[1], 4);
    memcpy(vendor + 4, &info[2], 4);
    memcpy(vendor + 8, &info[3], 4);
    // VMware="VMwareVMware" VBox="VBoxVBoxVBox" QEMU="TCGTCGTCGTCG"
    if (strstr(vendor, "VMware") || strstr(vendor, "VBox") || strstr(vendor, "TCGTCG")) return true;

    // Check for known VM artifacts
    HANDLE h = CreateFileA("\\\\.\\VBoxMiniRdrDN", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return true; }

    return false;
}

// call in a background thread — exits process if debugger detected mid-run
inline void watchdog() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500 + (rand() % 500)));
        if (isDebuggerPresent()) {
            // subtle exit — crash with access violation instead of ExitProcess
            // makes it look like a natural crash in debugger logs
            *reinterpret_cast<volatile int*>(0) = 0;
        }
    }
}

} // namespace AntiDbg
