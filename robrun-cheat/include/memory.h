// memory.h — RPM/WPM wrappers + pattern scan
// target: RobRun.exe (x64)

#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <optional>

class Memory {
public:
    HANDLE  hProc  = nullptr;
    DWORD   pid    = 0;
    uintptr_t base = 0;

    bool attach(const wchar_t* procName) {
        pid = getPid(procName);
        if (!pid) return false;
        hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProc || hProc == INVALID_HANDLE_VALUE) return false;
        base = getModuleBase(procName);
        return base != 0;
    }

    bool alive() const {
        DWORD code = 0;
        return GetExitCodeProcess(hProc, &code) && code == STILL_ACTIVE;
    }

    template<typename T>
    T read(uintptr_t addr) const {
        T val{};
        ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(addr), &val, sizeof(T), nullptr);
        return val;
    }

    template<typename T>
    bool write(uintptr_t addr, const T& val) const {
        return WriteProcessMemory(hProc, reinterpret_cast<LPVOID>(addr), &val, sizeof(T), nullptr) != 0;
    }

    std::string readString(uintptr_t addr, size_t maxLen = 64) const {
        // Unity managed string: [obj header 16][length int32][chars UTF-16]
        int len = read<int32_t>(addr + 0x10);
        if (len <= 0 || len > static_cast<int>(maxLen)) return "";
        std::wstring ws(len, L'\0');
        ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(addr + 0x14), ws.data(), len * 2, nullptr);
        return wideToUtf8(ws);
    }

    uintptr_t chain(uintptr_t base_, std::initializer_list<uintptr_t> offsets) const {
        uintptr_t cur = read<uintptr_t>(base_);
        for (auto off : offsets) {
            if (!cur) return 0;
            cur = read<uintptr_t>(cur + off);
        }
        return cur;
    }

    // IDA-style pattern scan in target process module
    std::optional<uintptr_t> patternScan(const char* pattern, const char* mask,
                                          uintptr_t start, size_t size) const {
        std::vector<uint8_t> buf(size);
        SIZE_T read_ = 0;
        if (!ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(start), buf.data(), size, &read_)) return {};
        size_t patLen = strlen(mask);
        for (size_t i = 0; i + patLen <= read_; i++) {
            bool found = true;
            for (size_t j = 0; j < patLen; j++) {
                if (mask[j] == 'x' && buf[i+j] != static_cast<uint8_t>(pattern[j])) { found = false; break; }
            }
            if (found) return start + i;
        }
        return {};
    }

private:
    static DWORD getPid(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe{ sizeof(pe) };
        DWORD result = 0;
        if (Process32FirstW(snap, &pe)) {
            do { if (!_wcsicmp(pe.szExeFile, name)) { result = pe.th32ProcessID; break; } }
            while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return result;
    }

    uintptr_t getModuleBase(const wchar_t* modName) const {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        MODULEENTRY32W me{ sizeof(me) };
        uintptr_t result = 0;
        if (Module32FirstW(snap, &me)) {
            do { if (!_wcsicmp(me.szModule, modName)) { result = reinterpret_cast<uintptr_t>(me.modBaseAddr); break; } }
            while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return result;
    }

    static std::string wideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return "";
        int sz = WideCharToMultiByte(CP_UTF8, 0, ws.data(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(sz - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), -1, s.data(), sz, nullptr, nullptr);
        return s;
    }
};
