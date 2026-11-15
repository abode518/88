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
#include <algorithm>

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
#include "scanner.h"
#include "camera.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// ── globals ───────────────────────────────────────────────────────────────────
static Memory       g_mem;
static Overlay      g_overlay;
static ESP          g_esp;
static Aimbot       g_aim;
static Features     g_feat;
static ConfigMgr    g_cfgMgr;
static FullConfig   g_cfg;
static CameraReader g_cam;
static LocalState   g_local;
static bool         g_streamProof = true;

static std::atomic<bool> g_running = true;
static bool  g_menuOpen    = false;
static bool  g_offsetsOk   = false;
static char  g_statusMsg[128] = "waiting for game...";

// ── obfuscated sleep — randomize timing to defeat rhythm-based detection ──────
static void rsleep(int baseMs) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(baseMs + (rand() % (baseMs / 4 + 1))));
}

// ── camera VP matrix + local state ───────────────────────────────────────────
static Matrix4x4 getVP() {
    return g_cam.getVP(g_mem, Offsets::CameraStaticField);
}

static void updateLocalState() {
    g_local = g_cam.getLocal(g_mem, OffExt::LocalPlayer);
}

// ── ESP draw ──────────────────────────────────────────────────────────────────
static void drawESP(ImDrawList* dl) {
    for (const auto& p : g_esp.players) {
        float bH = p.scrFeet.y - p.scrHead.y;
        if (bH < 4.f) continue;  // too small / too far
        float bW = bH * 0.4f;
        Vec2  bp { p.scrHead.x - bW * 0.5f, p.scrHead.y };
        uint32_t col = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;

        // ── skeleton ──────────────────────────────────────────────────────────
        if (g_esp.cfg.playerSkeleton) {
            for (auto [a, b] : SKELETON_EDGES) {
                if (p.boneValid[a] && p.boneValid[b]) {
                    Overlay::drawLine(dl, p.boneScr[a], p.boneScr[b],
                                      g_esp.cfg.colSkel, 1.2f);
                }
            }
            // head dot
            if (p.boneValid[Bone::Head])
                Overlay::drawCircle(dl, p.boneScr[Bone::Head], 3.f, col, 8);
        }

        // ── corner box ────────────────────────────────────────────────────────
        if (g_esp.cfg.playerBoxes) {
            float c = bW * 0.25f;
            auto L = [&](Vec2 a, Vec2 b) { Overlay::drawLine(dl, a, b, col, 1.5f); };
            L(bp,                 {bp.x+c,    bp.y     });
            L(bp,                 {bp.x,      bp.y+c   });
            L({bp.x+bW, bp.y},    {bp.x+bW-c, bp.y     });
            L({bp.x+bW, bp.y},    {bp.x+bW,   bp.y+c   });
            L({bp.x,  bp.y+bH},   {bp.x+c,    bp.y+bH  });
            L({bp.x,  bp.y+bH},   {bp.x,      bp.y+bH-c});
            L({bp.x+bW,bp.y+bH},  {bp.x+bW-c, bp.y+bH  });
            L({bp.x+bW,bp.y+bH},  {bp.x+bW,   bp.y+bH-c});
        }

        // ── health bar ────────────────────────────────────────────────────────
        if (g_esp.cfg.playerHealth)
            Overlay::drawHealthBar(dl, bp, bH, p.health, p.maxHp);

        // ── label [COP]/[CRIM] + name + dist ─────────────────────────────────
        {
            const char* tag    = (p.team == 0) ? "[COP]" : "[CRIM]";
            uint32_t    tagCol = (p.team == 0) ? 0xFF88DDFF : 0xFF6677FF;
            std::string lbl    = tag;
            if (g_esp.cfg.playerName) lbl += " " + p.name;
            if (g_esp.cfg.playerDist) lbl += "  " + std::to_string(static_cast<int>(p.dist)) + "m";
            Overlay::drawText(dl, {bp.x, bp.y - 14.f}, tagCol, lbl);
        }

        // ── snapline ─────────────────────────────────────────────────────────
        Overlay::drawLine(dl,
            {g_overlay.width * 0.5f, g_overlay.height},
            p.scrFeet,
            (col & 0x00FFFFFF) | 0x33000000, 0.7f);
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

        // rotate each player position by -localYaw so radar always
        // points "forward" at the top (like a compass)
        float sinY = std::sin(-g_local.yaw);
        float cosY = std::cos(-g_local.yaw);

        for (const auto& p : g_esp.players) {
            // delta from local player
            float dx = p.feet.x - g_local.pos.x;
            float dz = p.feet.z - g_local.pos.z;
            // rotate by -yaw
            float rx = rc.x + (dx * cosY - dz * sinY) * g_esp.cfg.radarScale;
            float ry = rc.y + (dx * sinY + dz * cosY) * g_esp.cfg.radarScale;
            float ex = rx - rc.x, ey = ry - rc.y;
            if (ex*ex + ey*ey > g_esp.cfg.radarRadius * g_esp.cfg.radarRadius) continue;

            uint32_t c = (p.team == 0) ? g_esp.cfg.colPolice : g_esp.cfg.colCriminal;
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

// ── helpers ───────────────────────────────────────────────────────────────────
static void Badge(const char* label, bool on) {
    if (on) ImGui::TextColored({0.f,0.9f,0.4f,1.f}, "[ON]  %s", label);
    else    ImGui::TextColored({0.5f,0.5f,0.5f,1.f}, "[--]  %s", label);
}
static void SectionHeader(const char* title) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.f,0.8f,1.f,0.6f});
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4{0.f,0.6f,0.8f,0.3f});
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ── menu ──────────────────────────────────────────────────────────────────────
static void drawMenu() {
    if (!g_menuOpen) return;

    // ── style ─────────────────────────────────────────────────────────────────
    auto& st = ImGui::GetStyle();
    st.WindowRounding   = 6.f;
    st.FrameRounding    = 3.f;
    st.ItemSpacing      = {8.f, 6.f};
    st.WindowPadding    = {14.f, 12.f};
    st.FramePadding     = {6.f, 3.f};
    st.TabRounding      = 4.f;
    st.ScrollbarRounding= 3.f;

    ImGui::PushStyleColor(ImGuiCol_WindowBg,         {0.05f,0.05f,0.09f,0.96f});
    ImGui::PushStyleColor(ImGuiCol_Border,            {0.0f, 0.5f, 0.7f, 0.4f});
    ImGui::PushStyleColor(ImGuiCol_TitleBg,           {0.04f,0.04f,0.08f,1.f});
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,     {0.04f,0.04f,0.08f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Tab,               {0.08f,0.08f,0.14f,1.f});
    ImGui::PushStyleColor(ImGuiCol_TabActive,         {0.0f, 0.45f,0.65f,1.f});
    ImGui::PushStyleColor(ImGuiCol_TabHovered,        {0.0f, 0.35f,0.5f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Header,            {0.0f, 0.3f, 0.45f,0.5f});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,     {0.0f, 0.4f, 0.6f, 0.6f});
    ImGui::PushStyleColor(ImGuiCol_CheckMark,         {0.0f, 0.9f, 0.5f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,        {0.0f, 0.7f, 1.0f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  {0.0f, 0.9f, 1.0f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_FrameBg,           {0.09f,0.09f,0.15f,1.f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,    {0.1f, 0.1f, 0.2f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Button,            {0.09f,0.09f,0.16f,1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,     {0.0f, 0.4f, 0.6f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,      {0.0f, 0.6f, 0.8f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_PopupBg,           {0.06f,0.06f,0.10f,0.98f});
    constexpr int STYLE_COLORS = 18;

    // ── window ────────────────────────────────────────────────────────────────
    ImGui::SetNextWindowSize({370.f, 490.f}, ImGuiCond_Once);
    ImGui::Begin("##rr", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar);

    // ── header bar ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.0f,0.3f,0.45f,0.15f});
    ImGui::BeginChild("##hdr", {0, 44}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({10, 6});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.f,0.85f,1.f,1.f});
    ImGui::Text("ROB RUN CHEAT");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 14);
    // status dot
    if (g_offsetsOk) ImGui::TextColored({0.f,0.9f,0.4f,1.f}, "●");
    else             ImGui::TextColored({1.f,0.4f,0.2f,1.f}, "●");
    ImGui::SameLine(0, 4);
    ImGui::TextColored({0.5f,0.5f,0.5f,1.f}, "%s", g_statusMsg);

    // live COP / CRIM count on right side
    int cops=0, crims=0;
    for (auto& p : g_esp.players) { if(p.team==0) cops++; else crims++; }
    char buf[48]; snprintf(buf,sizeof(buf),"COP %d  CRIM %d  |  INSERT  END",cops,crims);
    float tw = ImGui::CalcTextSize(buf).x;
    ImGui::SetCursorPos({ImGui::GetWindowWidth()-tw-10, 6});
    ImGui::TextColored({0.4f,0.4f,0.4f,1.f}, "COP ");
    ImGui::SameLine(0,0); ImGui::TextColored({0.5f,0.8f,1.f,1.f},"%d",cops);
    ImGui::SameLine(0,6); ImGui::TextColored({0.4f,0.4f,0.4f,1.f},"CRIM ");
    ImGui::SameLine(0,0); ImGui::TextColored({1.f,0.5f,0.5f,1.f},"%d",crims);
    ImGui::SameLine(0,6); ImGui::TextColored({0.3f,0.3f,0.3f,1.f},"| HOME  END");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (!g_offsetsOk)
        ImGui::TextColored({1.f,0.4f,0.2f,1.f},
            "  ⚠  patterns not resolved — update scanner.h after this patch");
    ImGui::Spacing();

    // ── tabs ──────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f,0.85f,0.85f,1.f});
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {

        // ════════════════ ESP ════════════════════════════════════════════════
        if (ImGui::BeginTabItem("  ESP  ")) {
            SectionHeader("PLAYERS");
            ImGui::Checkbox("Boxes",     &g_cfg.esp.playerBoxes);
            ImGui::Checkbox("Skeleton",  &g_cfg.esp.playerSkeleton);
            ImGui::Checkbox("Health",    &g_cfg.esp.playerHealth);
            ImGui::Checkbox("Names",     &g_cfg.esp.playerName);
            ImGui::Checkbox("Distance",  &g_cfg.esp.playerDist);
            ImGui::Checkbox("Skip teammates", &g_cfg.esp.teamCheck);
            ImGui::Spacing();
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("##dist", &g_cfg.esp.maxDist, 50.f, 600.f, "Max dist  %.0f m");
            ImGui::PopItemWidth();

            SectionHeader("WORLD");
            ImGui::Checkbox("Loot ESP",  &g_cfg.esp.lootESP);  Key("[F5]");
            ImGui::Checkbox("Radar",     &g_cfg.esp.radarESP); Key("[F4]");
            if (g_cfg.esp.radarESP) {
                ImGui::PushItemWidth(-1);
                ImGui::SliderFloat("##rscl", &g_cfg.esp.radarScale, 0.1f, 2.f, "Radar scale  %.1f");
                ImGui::PopItemWidth();
            }

            SectionHeader("STATUS");
            Badge("Boxes",    g_cfg.esp.playerBoxes);  ImGui::SameLine(130);
            Badge("Skeleton", g_cfg.esp.playerSkeleton);
            Badge("Loot",     g_cfg.esp.lootESP);      ImGui::SameLine(130);
            Badge("Radar",    g_cfg.esp.radarESP);

            ImGui::EndTabItem();
        }

        // ════════════════ AIMBOT ═════════════════════════════════════════════
        if (ImGui::BeginTabItem("  AIM  ")) {
            SectionHeader("AIMBOT");
            ImGui::Checkbox("Enable  (hold RMB)", &g_cfg.aim.enabled);
            ImGui::Spacing();
            ImGui::BeginDisabled(!g_cfg.aim.enabled);
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("##fov",  &g_cfg.aim.fovPx,  20.f, 500.f, "FOV radius  %.0f px");
            ImGui::SliderFloat("##smth", &g_cfg.aim.smooth,  1.f,  20.f, "Smooth  %.1f");
            ImGui::PopItemWidth();
            const char* bones[] = {"Hips","Chest","Head"};
            int boneIdx = (g_cfg.aim.targetBone == Offsets::BoneHead)  ? 2 :
                          (g_cfg.aim.targetBone == Offsets::BoneChest) ? 1 : 0;
            ImGui::PushItemWidth(120);
            if (ImGui::Combo("Target bone", &boneIdx, bones, 3))
                g_cfg.aim.targetBone = (boneIdx==2) ? Offsets::BoneHead  :
                                       (boneIdx==1) ? Offsets::BoneChest : Offsets::BoneHips;
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            SectionHeader("WEAPON");
            ImGui::Checkbox("No recoil / no spread", &g_cfg.aim.recoilComp);
            ImGui::Checkbox("Visibility check",      &g_cfg.aim.visCheck);

            SectionHeader("STATUS");
            Badge("Aimbot",    g_cfg.aim.enabled);    ImGui::SameLine(130);
            Badge("No recoil", g_cfg.aim.recoilComp);
            Badge("Vis check", g_cfg.aim.visCheck);

            ImGui::EndTabItem();
        }

        // ════════════════ MISC ═══════════════════════════════════════════════
        if (ImGui::BeginTabItem("  MISC  ")) {
            SectionHeader("ECONOMY");
            ImGui::Checkbox("Unlimited money", &g_cfg.feat.unlimMoney);
            if (g_cfg.feat.unlimMoney) {
                ImGui::PushItemWidth(-1);
                ImGui::SliderInt("##money", &g_cfg.feat.moneyAmount, 10000, 9999999, "Amount  $%d");
                ImGui::PopItemWidth();
            }

            SectionHeader("MOVEMENT");
            ImGui::Checkbox("Speed hack", &g_cfg.feat.speedHack);
            if (g_cfg.feat.speedHack) {
                ImGui::PushItemWidth(-1);
                ImGui::SliderFloat("##spd", &g_cfg.feat.speedMult, 1.f, 5.f, "Mult  x%.1f");
                ImGui::PopItemWidth();
            }
            ImGui::Checkbox("Noclip", &g_cfg.feat.noclip);

            SectionHeader("COMBAT");
            ImGui::Checkbox("Rapid fire",        &g_cfg.feat.rapidFire);
            ImGui::Checkbox("Unlimited ammo",    &g_cfg.feat.unlimAmmo);
            ImGui::Checkbox("Unlimited durabil", &g_cfg.feat.unlimDura);

            SectionHeader("STATUS");
            Badge("Money",   g_cfg.feat.unlimMoney); ImGui::SameLine(130);
            Badge("Speed",   g_cfg.feat.speedHack);
            Badge("Noclip",  g_cfg.feat.noclip);     ImGui::SameLine(130);
            Badge("R.Fire",  g_cfg.feat.rapidFire);

            ImGui::EndTabItem();
        }

        // ════════════════ SETTINGS ═══════════════════════════════════════════
        if (ImGui::BeginTabItem("  SET  ")) {
            SectionHeader("PROTECTION");
            if (ImGui::Checkbox("Stream proof  (OBS / Game Bar / ShadowPlay)", &g_streamProof))
                g_overlay.applyStreamProof(g_streamProof);
            ImGui::TextColored({0.4f,0.4f,0.4f,1.f},
                "   invisible to any screen capture");

            SectionHeader("CONFIG");
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.0f,0.3f,0.45f,0.7f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.0f,0.5f,0.7f, 0.9f});
            if (ImGui::Button("  Save config  ", {120,0})) {
                g_cfg.magic = 0xDEADC0DE; g_cfgMgr.save(g_cfg);
            }
            ImGui::SameLine(0,8);
            if (ImGui::Button("  Load config  ", {120,0})) {
                g_cfgMgr.load(g_cfg);
            }
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
            ImGui::TextColored({0.3f,0.3f,0.3f,1.f},
                "%%APPDATA%%\\Microsoft\\config.bin");
            ImGui::Spacing();
            ImGui::TextColored({0.3f,0.3f,0.3f,1.f},
                "offsets: %s", g_offsetsOk ? "resolved" : "MISSING — patterns need update");

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(); // tab text

    ImGui::End();
    ImGui::PopStyleColor(STYLE_COLORS);
}

// ── hotkeys ───────────────────────────────────────────────────────────────────
// HOME = menu open/close
// END  = exit
static void hotkeyThread() {
    while (g_running) {
        if (GetAsyncKeyState(VK_HOME) & 1) g_menuOpen = !g_menuOpen;
        if (GetAsyncKeyState(VK_END)  & 1) g_running  = false;
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

    // wait for game
    const std::wstring procName = L"RobRun.exe"; // FILL: verify exact process name
    snprintf(g_statusMsg, sizeof(g_statusMsg), "waiting for RobRun.exe...");
    while (!g_mem.attach(procName.c_str())) rsleep(2000);

    // auto-resolve offsets via pattern scanning — survives patches
    snprintf(g_statusMsg, sizeof(g_statusMsg), "scanning offsets...");
    OffsetResolver::resolve(g_mem);
    g_offsetsOk = true;
    snprintf(g_statusMsg, sizeof(g_statusMsg), "ready");

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

        updateLocalState();
        Matrix4x4 vp = getVP();
        g_esp.update(g_mem, vp, g_overlay.width, g_overlay.height, g_local.pos);
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
