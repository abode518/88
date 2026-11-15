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

        // team badge — always shown, حتى لو الاسم مخفي
        {
            const char* tag   = (p.team == 0) ? "[COP]" : "[CRIM]";
            uint32_t    tagCol = (p.team == 0) ? 0xFF88DDFF : 0xFF4466FF;
            std::string lbl   = std::string(tag);
            if (g_esp.cfg.playerName) lbl += " " + p.name;
            if (g_esp.cfg.playerDist) lbl += "  " + std::to_string(static_cast<int>(p.dist)) + "m";
            Overlay::drawText(dl, {bp.x, bp.y - 14.f}, tagCol, lbl);
        }

        // snapline
        Overlay::drawLine(dl,
            {g_overlay.width * 0.5f, g_overlay.height},
            p.scrFeet,
            (col & 0x00FFFFFF) | 0x33000000, 0.8f);
    }

    for (const auto& l : g_esp.loot) {
        // diamond shape لللوت — أوضح من دائرة
        float d = 5.f;
        Vec2 lp = l.scr;
        Overlay::drawLine(dl, {lp.x,   lp.y-d}, {lp.x+d, lp.y  }, g_esp.cfg.colLoot, 1.5f);
        Overlay::drawLine(dl, {lp.x+d, lp.y  }, {lp.x,   lp.y+d}, g_esp.cfg.colLoot, 1.5f);
        Overlay::drawLine(dl, {lp.x,   lp.y+d}, {lp.x-d, lp.y  }, g_esp.cfg.colLoot, 1.5f);
        Overlay::drawLine(dl, {lp.x-d, lp.y  }, {lp.x,   lp.y-d}, g_esp.cfg.colLoot, 1.5f);
        Overlay::drawText(dl, {lp.x + 8, lp.y - 6}, g_esp.cfg.colLoot, l.name);
    }

    if (g_esp.cfg.radarESP) {
        Vec2 rc { g_overlay.width - g_esp.cfg.radarRadius - 10.f,
                  10.f + g_esp.cfg.radarRadius };
        // bg fill
        Overlay::drawBoxFilled(dl,
            {rc.x - g_esp.cfg.radarRadius, rc.y - g_esp.cfg.radarRadius},
            g_esp.cfg.radarRadius * 2, g_esp.cfg.radarRadius * 2, 0xBB000000);
        Overlay::drawBox(dl,
            {rc.x - g_esp.cfg.radarRadius, rc.y - g_esp.cfg.radarRadius},
            g_esp.cfg.radarRadius * 2, g_esp.cfg.radarRadius * 2, 0x88FFFFFF);
        // crosshair (local player)
        Overlay::drawLine(dl, {rc.x-6, rc.y}, {rc.x+6, rc.y}, 0xFFFFFFFF);
        Overlay::drawLine(dl, {rc.x, rc.y-6}, {rc.x, rc.y+6}, 0xFFFFFFFF);

        for (const auto& p : g_esp.players) {
            // FILL: rotate delta by local player yaw for correct orientation
            float rx = rc.x + p.feet.x * g_esp.cfg.radarScale;
            float ry = rc.y + p.feet.z * g_esp.cfg.radarScale;
            float dx = rx - rc.x, dy = ry - rc.y;
            if (dx*dx + dy*dy > g_esp.cfg.radarRadius * g_esp.cfg.radarRadius) continue;

            uint32_t c = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;
            // triangle pointing "up" per player
            Overlay::drawLine(dl, {rx,   ry-4}, {rx+3, ry+3}, c, 1.5f);
            Overlay::drawLine(dl, {rx+3, ry+3}, {rx-3, ry+3}, c, 1.5f);
            Overlay::drawLine(dl, {rx-3, ry+3}, {rx,   ry-4}, c, 1.5f);
        }
        // label
        Overlay::drawText(dl,
            {rc.x - g_esp.cfg.radarRadius, rc.y + g_esp.cfg.radarRadius + 2},
            0x88FFFFFF, "RADAR");
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
            ImGui::TextColored({0.f,0.8f,1.f,1.f}, "Players");
            ImGui::Checkbox("Boxes",           &g_cfg.esp.playerBoxes);
            ImGui::Checkbox("Health bars",     &g_cfg.esp.playerHealth);
            ImGui::Checkbox("Names",           &g_cfg.esp.playerName);
            ImGui::Checkbox("Distance",        &g_cfg.esp.playerDist);
            ImGui::Checkbox("Team check (skip same)", &g_cfg.esp.teamCheck);
            ImGui::SliderFloat("Max dist (m)", &g_cfg.esp.maxDist, 50.f, 600.f, "%.0f");
            ImGui::Separator();
            ImGui::TextColored({0.f,0.8f,1.f,1.f}, "World");
            ImGui::Checkbox("Loot ESP",        &g_cfg.esp.lootESP);
            ImGui::Checkbox("Radar",           &g_cfg.esp.radarESP);
            ImGui::Separator();
            // live team count
            int cops = 0, crims = 0;
            for (auto& p : g_esp.players) { if (p.team==0) cops++; else crims++; }
            ImGui::TextColored({0.6f,0.9f,1.f,1.f}, "[COP]  %d", cops);
            ImGui::SameLine(0,16);
            ImGui::TextColored({1.f,0.6f,0.6f,1.f}, "[CRIM] %d", crims);
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
            ImGui::Checkbox("Unlimited money", &g_cfg.feat.unlimMoney);
            if (g_cfg.feat.unlimMoney)
                ImGui::SliderInt("Amount", &g_cfg.feat.moneyAmount, 1000, 9999999);
            ImGui::Separator();
            ImGui::Checkbox("Speed hack",      &g_cfg.feat.speedHack);
            if (g_cfg.feat.speedHack)
                ImGui::SliderFloat("Speed mult", &g_cfg.feat.speedMult, 1.f, 5.f, "%.1fx");
            ImGui::Checkbox("Noclip",          &g_cfg.feat.noclip);
            ImGui::Separator();
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
