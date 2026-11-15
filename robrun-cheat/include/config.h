// config.h — save/load settings to encrypted binary file
// file: %APPDATA%\Microsoft\config.bin  (hidden in MS folder, looks legit)

#pragma once
#include "esp.h"
#include "aimbot.h"
#include "features.h"
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <string>

#pragma comment(lib, "Shell32.lib")

struct FullConfig {
    ESP::Config     esp;
    Aimbot::Config  aim;
    Features::Config feat;
    uint32_t        magic = 0xDEADC0DE; // validate on load
};

class ConfigMgr {
    std::string path_;

public:
    ConfigMgr() {
        wchar_t appdata[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
        // hide in Microsoft folder
        std::wstring p = std::wstring(appdata) + L"\\Microsoft\\config.bin";
        path_ = std::string(p.begin(), p.end());
    }

    void save(const FullConfig& cfg) {
        std::ofstream f(path_, std::ios::binary | std::ios::trunc);
        if (!f) return;
        // simple XOR obfuscation so it's not plaintext
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&cfg);
        for (size_t i = 0; i < sizeof(FullConfig); i++) {
            uint8_t b = raw[i] ^ (0xA5 + static_cast<uint8_t>(i));
            f.write(reinterpret_cast<char*>(&b), 1);
        }
    }

    bool load(FullConfig& cfg) {
        std::ifstream f(path_, std::ios::binary);
        if (!f) return false;
        uint8_t* raw = reinterpret_cast<uint8_t*>(&cfg);
        for (size_t i = 0; i < sizeof(FullConfig); i++) {
            uint8_t b = 0;
            f.read(reinterpret_cast<char*>(&b), 1);
            raw[i] = b ^ (0xA5 + static_cast<uint8_t>(i));
        }
        return cfg.magic == 0xDEADC0DE;
    }
};
