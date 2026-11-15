// Rob Run external cheat — main.cpp
// target:  RobRun.exe  x64  EAC kernel-mode
// method:  external overlay (no injection), RPM for reads, SendInput for aim
// build:   MSVC 2022  /std:c++20  /O2  x64
//          link: d3d11.lib dxgi.lib user32.lib
//          add imgui/*.cpp to project (docking or master branch)
// evasion: no injected DLL → EAC usermode driver scans miss us
//          overlay window spoofed as calculator, click-through so no focus

#include <Windows.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

#include "memory.h"
#include "overlay.h"
#include "esp.h"
#include "aimbot.h"
#include "math.h"
#include "offsets.h"

// ImGui (add to project)
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

static Memory  g_mem;
static Overlay g_overlay;
static ESP     g_esp;
static Aimbot  g_aim;

static std::atomic<bool> g_running = true;

// menu visibility — Insert key to toggle
static bool g_menuOpen = false;

// ── camera VP matrix reader (Unity) ──────────────────────────────────────────
static Matrix4x4 getVPMatrix() {
    // Unity Camera.main → component array → Camera component → projMatrix * viewMatrix
    // FILL: derive exact chain from your dump, e.g.:
    //   cameraObj = read(staticField + 0x10)   // Camera static instance
    //   viewMat   = read(cameraObj + 0xEC)
    //   projMat   = read(cameraObj + 0xFC)
    //   return proj * view
    return g_mem.read<Matrix4x4>(g_mem.base + Offsets::CameraStaticField + Offsets::ViewMatrix);
}

// ── render one frame of ESP ───────────────────────────────────────────────────
static void renderESP(ImDrawList* dl) {
    for (const auto& p : g_esp.players) {
        float boxH = p.scrFeet.y - p.scrHead.y;
        float boxW = boxH * 0.4f;
        Vec2  boxPos{ p.scrHead.x - boxW * 0.5f, p.scrHead.y };

        uint32_t col = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;

        if (g_esp.cfg.playerBoxes) {
            // corner box style (less detectable than full rect to screenshot-based detects)
            float cLen = boxW * 0.25f;
            uint32_t outline = 0xCC000000;
            // top-left
            Overlay::drawLine(dl, boxPos, {boxPos.x + cLen, boxPos.y}, outline, 2.5f);
            Overlay::drawLine(dl, boxPos, {boxPos.x, boxPos.y + cLen}, outline, 2.5f);
            Overlay::drawLine(dl, boxPos, {boxPos.x + cLen, boxPos.y}, col, 1.5f);
            Overlay::drawLine(dl, boxPos, {boxPos.x, boxPos.y + cLen}, col, 1.5f);
            // top-right
            Vec2 tr{boxPos.x + boxW, boxPos.y};
            Overlay::drawLine(dl, tr, {tr.x - cLen, tr.y}, col, 1.5f);
            Overlay::drawLine(dl, tr, {tr.x, tr.y + cLen}, col, 1.5f);
            // bottom-left
            Vec2 bl{boxPos.x, boxPos.y + boxH};
            Overlay::drawLine(dl, bl, {bl.x + cLen, bl.y}, col, 1.5f);
            Overlay::drawLine(dl, bl, {bl.x, bl.y - cLen}, col, 1.5f);
            // bottom-right
            Vec2 br{boxPos.x + boxW, boxPos.y + boxH};
            Overlay::drawLine(dl, br, {br.x - cLen, br.y}, col, 1.5f);
            Overlay::drawLine(dl, br, {br.x, br.y - cLen}, col, 1.5f);
        }

        if (g_esp.cfg.playerHealth)
            Overlay::drawHealthBar(dl, boxPos, boxH, p.health, p.maxHp);

        if (g_esp.cfg.playerName || g_esp.cfg.playerDist) {
            std::string label = p.name;
            if (g_esp.cfg.playerDist) label += " [" + std::to_string(static_cast<int>(p.dist)) + "m]";
            Overlay::drawText(dl, {boxPos.x, boxPos.y - 14.f}, col, label);
        }
    }

    for (const auto& l : g_esp.loot) {
        Overlay::drawText(dl, l.scr, g_esp.cfg.colLoot, l.name);
    }

    // radar
    if (g_esp.cfg.radarESP) {
        Vec2 radarCenter{ g_overlay.width - g_esp.cfg.radarRadius - 10.f, 10.f + g_esp.cfg.radarRadius };
        Overlay::drawCircle(dl, radarCenter, g_esp.cfg.radarRadius, 0x88000000);
        Overlay::drawCircle(dl, radarCenter, g_esp.cfg.radarRadius, 0x88FFFFFF, 48);
        for (const auto& p : g_esp.players) {
            // FILL: rotate by local player yaw
            float rx = radarCenter.x + (p.feet.x) * g_esp.cfg.radarScale;
            float ry = radarCenter.y + (p.feet.z) * g_esp.cfg.radarScale;
            if (std::abs(rx - radarCenter.x) < g_esp.cfg.radarRadius &&
                std::abs(ry - radarCenter.y) < g_esp.cfg.radarRadius) {
                uint32_t col = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;
                Overlay::drawCircle(dl, {rx, ry}, 3.f, col, 8);
            }
        }
    }

    // aimbot FOV circle
    if (g_aim.cfg.enabled) {
        Overlay::drawCircle(dl,
            screenCenter(g_overlay.width, g_overlay.height),
            g_aim.cfg.fovPx,
            0x44FFFFFF, 64);
    }
}

// ── ImGui menu ────────────────────────────────────────────────────────────────
static void renderMenu() {
    if (!g_menuOpen) return;
    ImGui::SetNextWindowSize({320, 420}, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("Rob Run", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Boxes",     &g_esp.cfg.playerBoxes);
        ImGui::Checkbox("Skeleton",  &g_esp.cfg.playerSkeleton);
        ImGui::Checkbox("Health",    &g_esp.cfg.playerHealth);
        ImGui::Checkbox("Name",      &g_esp.cfg.playerName);
        ImGui::Checkbox("Distance",  &g_esp.cfg.playerDist);
        ImGui::Checkbox("Loot ESP",  &g_esp.cfg.lootESP);
        ImGui::Checkbox("Radar",     &g_esp.cfg.radarESP);
        ImGui::SliderFloat("Max Dist", &g_esp.cfg.maxDist, 50.f, 600.f, "%.0fm");
    }

    if (ImGui::CollapsingHeader("Aimbot")) {
        ImGui::Checkbox("Enabled",       &g_aim.cfg.enabled);
        ImGui::Checkbox("Hold Key",      &g_aim.cfg.holdKey);
        ImGui::Checkbox("Recoil Comp",   &g_aim.cfg.recoilComp);
        ImGui::Checkbox("Vis Check",     &g_aim.cfg.visCheck);
        ImGui::SliderFloat("FOV px",     &g_aim.cfg.fovPx,   20.f, 400.f);
        ImGui::SliderFloat("Smoothness", &g_aim.cfg.smooth,   1.f,  20.f);
    }

    ImGui::Separator();
    ImGui::TextColored({0.4f,0.4f,0.4f,1.f}, "INSERT = toggle menu");
    ImGui::TextColored({0.4f,0.4f,0.4f,1.f}, "players: %d", (int)g_esp.players.size());
    ImGui::End();
}

// ── hotkey thread ─────────────────────────────────────────────────────────────
static void hotkeyThread() {
    while (g_running) {
        if (GetAsyncKeyState(VK_INSERT) & 1) g_menuOpen = !g_menuOpen;
        if (GetAsyncKeyState(VK_END)    & 1) g_running = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ── entry ─────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // wait for game
    while (!g_mem.attach(L"RobRun.exe")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    if (!g_overlay.init(L"Rob Run")) return 1; // FILL: exact window title

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // no imgui.ini
    ImGui_ImplWin32_Init(g_overlay.hwnd);
    ImGui_ImplDX11_Init(g_overlay.device, g_overlay.context);
    ImGui::StyleColorsDark();

    std::thread hk(hotkeyThread);

    MSG msg{};
    while (g_running && g_mem.alive()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }

        Matrix4x4 vp = getVPMatrix();
        g_esp.update(g_mem, vp, g_overlay.width, g_overlay.height);
        g_aim.run(g_esp.players, g_overlay.width, g_overlay.height, g_mem);
        g_aim.recoilControl(g_mem);

        g_overlay.beginFrame();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        renderESP(dl);
        renderMenu();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_overlay.endFrame();
    }

    g_running = false;
    hk.join();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_overlay.cleanup();
    return 0;
}
