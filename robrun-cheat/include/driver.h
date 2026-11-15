// driver.h — kernel-driver memory read/write interface
// bypasses EAC usermode RPM monitoring by routing through a signed/vulnerable driver
//
// Two modes:
//   MODE_RPM   : fallback — standard ReadProcessMemory (detected by EAC kernel on some builds)
//   MODE_DRIVER: routes through ioctl to a loaded kernel driver (undetected)
//
// Driver implementation options (pick one, build separately):
//   A) gdrv-loader + CVE-2018-19320 (gigabyte driver, old but still works on W10)
//   B) ProcExp152.sys — signed MS driver, ioctl-abusable
//   C) custom kdmapper-loaded unsigned driver (best, see kdmapper repo)
//
// This header provides the interface; the .sys file and loader are separate.
// FILL: implement DriverComm::init() to load your chosen driver and set up ioctl

#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

#define MODE_RPM    0
#define MODE_DRIVER 1

#ifndef READ_MODE
#define READ_MODE MODE_RPM   // switch to MODE_DRIVER once driver is set up
#endif

namespace DriverComm {

// IOCTL codes — match your driver
constexpr DWORD IOCTL_READ_MEM  = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
constexpr DWORD IOCTL_WRITE_MEM = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);

struct ReadRequest {
    uintptr_t targetPid;
    uintptr_t address;
    uintptr_t buffer;
    size_t    size;
};

static HANDLE g_device  = INVALID_HANDLE_VALUE;
static DWORD  g_pid     = 0;

inline bool init(DWORD pid, const wchar_t* deviceName = L"\\\\.\\RobCheatDrv") {
    g_pid = pid;
#if READ_MODE == MODE_DRIVER
    g_device = CreateFileW(deviceName, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    return g_device != INVALID_HANDLE_VALUE;
#else
    return true; // RPM mode — no driver needed
#endif
}

template<typename T>
inline T read(HANDLE hProc, uintptr_t addr) {
    T val{};
#if READ_MODE == MODE_DRIVER
    ReadRequest req{ g_pid, addr, reinterpret_cast<uintptr_t>(&val), sizeof(T) };
    DWORD ret = 0;
    DeviceIoControl(g_device, IOCTL_READ_MEM, &req, sizeof(req), &req, sizeof(req), &ret, nullptr);
#else
    ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(addr), &val, sizeof(T), nullptr);
#endif
    return val;
}

template<typename T>
inline bool write(HANDLE hProc, uintptr_t addr, const T& val) {
#if READ_MODE == MODE_DRIVER
    ReadRequest req{ g_pid, addr, reinterpret_cast<uintptr_t>(const_cast<T*>(&val)), sizeof(T) };
    DWORD ret = 0;
    return DeviceIoControl(g_device, IOCTL_WRITE_MEM, &req, sizeof(req), &req, sizeof(req), &ret, nullptr) != 0;
#else
    return WriteProcessMemory(hProc, reinterpret_cast<LPVOID>(addr), &val, sizeof(T), nullptr) != 0;
#endif
}

} // namespace DriverComm
