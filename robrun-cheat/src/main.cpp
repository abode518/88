// Rob Run external cheat — main.cpp  v2
// target:  RobRun.exe  x64  EAC kernel-mode  (Steam App 4262460)
// engine:  Unity IL2CPP (Kefir)
// build:   MSVC 2022  /std:c++20  /O2  /MT  x64  Release
//          link: d3d11.lib dxgi.lib user32.lib Shell32.lib
//          add: imgui/*.cpp imgui_impl_win32.cpp imgui_impl_dx11.cpp
// evasion: external (no DLL injection) → EAC usermode scan miss
//          optional: set READ_MODE=MODE_DRIVER + load kernel driver for RPM bypass
//          anti-debug watchdog thread
//          compile-time string encryption (ENC macro)
//          config hidden in %APPDATA%\Microsoft\

#include <Windows.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

#include "memory.h"
#include "driver.h"
#include "overlay.h"
#include "esp.h"
#include "aimbot.h"
#include "features.h"
#include "config.h"
#include "antidbg.h"
#include "encrypt.h"
#include "math.h"
#include "offsets.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// ── globals ───────────────────────────────────────────────────────────────────
static Memory    g_mem;
static Overlay   g_overlay;
static ESP       g_esp;
static Aimbot    g_aim;
static Features  g_feat;
static ConfigMgr g_cfgMgr;
static FullConfig g_cfg;

static std::atomic<bool> g_running = true;
static bool g_menuOpen = false;

// ── obfuscated sleep — randomize timing to defeat rhythm-based detection ──────
static void rsleep(int baseMs) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(baseMs + (rand() % (baseMs / 4 + 1))));
}

// ── camera VP matrix ──────────────────────────────────────────────────────────
static Matrix4x4 getVP() {
    // FILL: exact ptr chain from your dump
    // example Unity chain: GameAssembly.dll + Camera_static → Camera obj → ViewMatrix
    return g_mem.read<Matrix4x4>(g_mem.base + Offsets::CameraStaticField + Offsets::ViewMatrix);
}

// ── ESP draw ──────────────────────────────────────────────────────────────────
static void drawESP(ImDrawList* dl) {
    for (const auto& p : g_esp.players) {
        float bH = p.scrFeet.y - p.scrHead.y;
        float bW = bH * 0.4f;
        Vec2  bp { p.scrHead.x - bW * 0.5f, p.scrHead.y };
        uint32_t col = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;

        if (g_esp.cfg.playerBoxes) {
            // corner-box (less screenshot-detectable than full rect)
            float c = bW * 0.25f;
            auto L = [&](Vec2 a, Vec2 b) { Overlay::drawLine(dl, a, b, col, 1.5f); };
            L(bp,                    {bp.x+c,    bp.y     });
            L(bp,                    {bp.x,      bp.y+c   });
            L({bp.x+bW, bp.y},       {bp.x+bW-c, bp.y     });
            L({bp.x+bW, bp.y},       {bp.x+bW,   bp.y+c   });
            L({bp.x,    bp.y+bH},    {bp.x+c,    bp.y+bH  });
            L({bp.x,    bp.y+bH},    {bp.x,      bp.y+bH-c});
            L({bp.x+bW, bp.y+bH},    {bp.x+bW-c, bp.y+bH  });
            L({bp.x+bW, bp.y+bH},    {bp.x+bW,   bp.y+bH-c});
        }

        if (g_esp.cfg.playerHealth)
            Overlay::drawHealthBar(dl, bp, bH, p.health, p.maxHp);

        if (g_esp.cfg.playerName || g_esp.cfg.playerDist) {
            std::string lbl = p.name;
            if (g_esp.cfg.playerDist) lbl += "  " + std::to_string(static_cast<int>(p.dist)) + "m";
            Overlay::drawText(dl, {bp.x, bp.y - 14.f}, col, lbl);
        }

        // snapline from bottom center to target feet
        Overlay::drawLine(dl,
            {g_overlay.width * 0.5f, g_overlay.height},
            p.scrFeet,
            (col & 0x00FFFFFF) | 0x33000000, 0.8f);
    }

    for (const auto& l : g_esp.loot) {
        Overlay::drawCircle(dl, l.scr, 3.f, g_esp.cfg.colLoot, 6);
        Overlay::drawText(dl, {l.scr.x + 5, l.scr.y - 6}, g_esp.cfg.colLoot, l.name);
    }

    if (g_esp.cfg.radarESP) {
        Vec2 rc { g_overlay.width - g_esp.cfg.radarRadius - 10.f,
                  10.f + g_esp.cfg.radarRadius };
        Overlay::drawCircle(dl, rc, g_esp.cfg.radarRadius, 0xAA000000);
        Overlay::drawCircle(dl, rc, g_esp.cfg.radarRadius, 0x55FFFFFF);
        // crosshair
        Overlay::drawLine(dl, {rc.x - 4, rc.y}, {rc.x + 4, rc.y}, 0x88FFFFFF);
        Overlay::drawLine(dl, {rc.x, rc.y - 4}, {rc.x, rc.y + 4}, 0x88FFFFFF);
        for (const auto& p : g_esp.players) {
            float rx = rc.x + p.feet.x * g_esp.cfg.radarScale;
            float ry = rc.y + p.feet.z * g_esp.cfg.radarScale;
            if (std::abs(rx - rc.x) < g_esp.cfg.radarRadius &&
                std::abs(ry - rc.y) < g_esp.cfg.radarRadius) {
                uint32_t c = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;
                Overlay::drawCircle(dl, {rx, ry}, 3.f, c, 6);
            }
        }
    }

    if (g_aim.cfg.enabled)
        Overlay::drawCircle(dl,
            screenCenter(g_overlay.width, g_overlay.height),
            g_aim.cfg.fovPx, 0x33FFFFFF, 64);
}

// ── ImGui menu ────────────────────────────────────────────────────────────────
static void drawMenu() {
    if (!g_menuOpen) return;

    // dark style
    ImGui::PushStyleColor(ImGuiCol_WindowBg,     {0.08f,0.08f,0.12f,0.92f});
    ImGui::PushStyleColor(ImGuiCol_Header,       {0.15f,0.15f,0.25f,1.f});
    ImGui::PushStyleColor(ImGuiCol_CheckMark,    {0.0f, 0.8f, 1.0f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,   {0.0f, 0.8f, 1.0f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_FrameBg,      {0.1f, 0.1f, 0.18f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Button,       {0.1f, 0.1f, 0.18f,1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.0f, 0.4f, 0.5f, 1.f});

    ImGui::SetNextWindowSize({360, 480}, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.0f); // handled by PushStyleColor above
    ImGui::Begin("##robrun", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored({0.f,0.8f,1.f,1.f}, "  ROB RUN");
    ImGui::SameLine(0, 20);
    ImGui::TextColored({0.4f,0.4f,0.4f,1.f},
        "players:%d  INSERT=hide  END=exit", (int)g_esp.players.size());
    ImGui::Separator();

    if (ImGui::BeginTabBar("tabs")) {

        // ── ESP tab ────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("Boxes",           &g_cfg.esp.playerBoxes);
            ImGui::Checkbox("Health bars",     &g_cfg.esp.playerHealth);
            ImGui::Checkbox("Names",           &g_cfg.esp.playerName);
            ImGui::Checkbox("Distance",        &g_cfg.esp.playerDist);
            ImGui::Checkbox("Loot ESP",        &g_cfg.esp.lootESP);
            ImGui::Checkbox("Radar",           &g_cfg.esp.radarESP);
            ImGui::SliderFloat("Max dist (m)", &g_cfg.esp.maxDist, 50.f, 600.f, "%.0f");
            ImGui::EndTabItem();
        }

        // ── Aimbot tab ─────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Checkbox("Enable (RMB hold)", &g_cfg.aim.enabled);
            ImGui::Checkbox("Recoil control",    &g_cfg.aim.recoilComp);
            ImGui::Checkbox("Vis check",         &g_cfg.aim.visCheck);
            ImGui::SliderFloat("FOV (px)",        &g_cfg.aim.fovPx,  20.f, 500.f, "%.0f");
            ImGui::SliderFloat("Smooth",          &g_cfg.aim.smooth,  1.f,  20.f, "%.1f");
            const char* bones[] = {"Hips","Chest","Head"};
            int boneIdx = (g_cfg.aim.targetBone == Offsets::BoneHead) ? 2 :
                          (g_cfg.aim.targetBone == Offsets::BoneChest) ? 1 : 0;
            if (ImGui::Combo("Bone", &boneIdx, bones, 3)) {
                g_cfg.aim.targetBone = (boneIdx==2) ? Offsets::BoneHead :
                                       (boneIdx==1) ? Offsets::BoneChest : Offsets::BoneHips;
            }
            ImGui::EndTabItem();
        }

        // ── Misc tab ───────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Misc")) {
            ImGui::Checkbox("God mode",        &g_cfg.feat.godMode);
            ImGui::Checkbox("Unlimited money", &g_cfg.feat.unlimMoney);
            if (g_cfg.feat.unlimMoney)
                ImGui::SliderInt("Amount", &g_cfg.feat.moneyAmount, 1000, 9999999);
            ImGui::Checkbox("Speed hack",      &g_cfg.feat.speedHack);
            if (g_cfg.feat.speedHack)
                ImGui::SliderFloat("Speed mult", &g_cfg.feat.speedMult, 1.f, 5.f, "%.1fx");
            ImGui::Checkbox("Fly mode",        &g_cfg.feat.flyMode);
            ImGui::Checkbox("Noclip",          &g_cfg.feat.noclip);
            ImGui::Checkbox("Rapid fire",      &g_cfg.feat.rapidFire);
            ImGui::Checkbox("Unlimited ammo",  &g_cfg.feat.unlimAmmo);
            ImGui::Checkbox("Unlimited dura",  &g_cfg.feat.unlimDura);
            ImGui::EndTabItem();
        }

        // ── Config tab ─────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Config")) {
            if (ImGui::Button("Save config")) {
                g_cfg.magic = 0xDEADC0DE;
                g_cfgMgr.save(g_cfg);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load config")) g_cfgMgr.load(g_cfg);
            ImGui::Separator();
            ImGui::TextColored({0.4f,0.4f,0.4f,1.f}, "Config: %%APPDATA%%\\Microsoft\\config.bin");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleColor(7);
}

// ── hotkeys ───────────────────────────────────────────────────────────────────
static void hotkeyThread() {
    while (g_running) {
        if (GetAsyncKeyState(VK_INSERT) & 1) g_menuOpen = !g_menuOpen;
        if (GetAsyncKeyState(VK_END)    & 1) g_running  = false;
        rsleep(10);
    }
}

// ── entry ─────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // anti-debug — exit silently if debugger or VM detected
    if (AntiDbg::isDebuggerPresent() || AntiDbg::isVM()) return 0;
    std::thread(AntiDbg::watchdog).detach();

    // load saved config
    g_cfgMgr.load(g_cfg);

    // wait for game — encrypted proc name
    const std::wstring procName = L"RobRun.exe"; // FILL: verify exact process name
    while (!g_mem.attach(procName.c_str())) rsleep(2000);

    // init overlay — exact window title
    // FILL: verify window title with Spy++ or Process Hacker
    if (!g_overlay.init(L"Rob Run")) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(g_overlay.hwnd);
    ImGui_ImplDX11_Init(g_overlay.device, g_overlay.context);

    std::thread hk(hotkeyThread);

    MSG msg{};
    while (g_running && g_mem.alive()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }

        // sync config structs
        g_esp.cfg  = g_cfg.esp;
        g_aim.cfg  = g_cfg.aim;
        g_feat.cfg = g_cfg.feat;

        Matrix4x4 vp = getVP();
        g_esp.update(g_mem, vp, g_overlay.width, g_overlay.height);
        g_aim.run(g_esp.players, g_overlay.width, g_overlay.height, g_mem);
        g_aim.recoilControl(g_mem);
        g_feat.tick(g_mem);

        g_overlay.beginFrame();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawESP(ImGui::GetBackgroundDrawList());
        drawMenu();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_overlay.endFrame();

        rsleep(1); // slight randomized sleep — smooths CPU + defeats timing analysis
    }

    // save on exit
    g_cfg.magic = 0xDEADC0DE;
    g_cfgMgr.save(g_cfg);

    g_running = false;
    hk.join();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_overlay.cleanup();
    return 0;
}
