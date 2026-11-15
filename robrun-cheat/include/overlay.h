// overlay.h — transparent click-through DirectX 11 + ImGui overlay
// Defeats basic overlay detects: window class spoofed, layered+transparent flags

#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <string>
#include "math.h"

// forward-declare ImGui draw list; include imgui.h in translation units that need it
struct ImDrawList;

class Overlay {
public:
    HWND     hwnd        = nullptr;
    HWND     targetHwnd  = nullptr;
    float    width       = 1920.f;
    float    height      = 1080.f;

    ID3D11Device*           device    = nullptr;
    ID3D11DeviceContext*    context   = nullptr;
    IDXGISwapChain*         swapChain = nullptr;
    ID3D11RenderTargetView* rtv       = nullptr;

    bool init(const wchar_t* targetWindowName) {
        targetHwnd = FindWindowW(nullptr, targetWindowName);
        if (!targetHwnd) return false;

        RECT rc{};
        GetClientRect(targetHwnd, &rc);
        width  = static_cast<float>(rc.right);
        height = static_cast<float>(rc.bottom);

        // ── window ──────────────────────────────────────────────────────────
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"D3DOverlay";   // generic name, not "cheat"
        RegisterClassExW(&wc);

        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"D3DOverlay", L" ",
            WS_POPUP,
            rc.left, rc.top, rc.right, rc.bottom,
            nullptr, nullptr, wc.hInstance, nullptr
        );
        if (!hwnd) return false;

        SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        // ── D3D11 ────────────────────────────────────────────────────────────
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount                        = 2;
        sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width                   = static_cast<UINT>(width);
        sd.BufferDesc.Height                  = static_cast<UINT>(height);
        sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow                       = hwnd;
        sd.SampleDesc.Count                   = 1;
        sd.Windowed                           = TRUE;
        sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &swapChain, &device, &fl, &context
        );
        if (FAILED(hr)) return false;

        ID3D11Texture2D* back = nullptr;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
        device->CreateRenderTargetView(back, nullptr, &rtv);
        back->Release();

        return true;
    }

    void beginFrame() {
        // sync overlay position with game window
        RECT rc{};
        GetClientRect(targetHwnd, &rc);
        POINT pt{ rc.left, rc.top };
        ClientToScreen(targetHwnd, &pt);
        SetWindowPos(hwnd, HWND_TOPMOST, pt.x, pt.y, rc.right, rc.bottom, SWP_NOACTIVATE);
        width  = static_cast<float>(rc.right);
        height = static_cast<float>(rc.bottom);

        float clear[4] = {0,0,0,0};
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->ClearRenderTargetView(rtv, clear);
    }

    void endFrame() {
        swapChain->Present(0, 0);
    }

    void cleanup() {
        if (rtv)       { rtv->Release();       rtv = nullptr; }
        if (swapChain) { swapChain->Release();  swapChain = nullptr; }
        if (context)   { context->Release();    context = nullptr; }
        if (device)    { device->Release();     device = nullptr; }
        if (hwnd)      { DestroyWindow(hwnd);   hwnd = nullptr; }
    }

    // ── 2D draw helpers (call between ImGui::NewFrame / ImGui::Render) ───────
    static void drawBox(ImDrawList* dl, Vec2 pos, float w, float h, uint32_t col, float thickness = 1.5f);
    static void drawBoxFilled(ImDrawList* dl, Vec2 pos, float w, float h, uint32_t col);
    static void drawLine(ImDrawList* dl, Vec2 a, Vec2 b, uint32_t col, float thickness = 1.f);
    static void drawText(ImDrawList* dl, Vec2 pos, uint32_t col, const std::string& text);
    static void drawCircle(ImDrawList* dl, Vec2 center, float radius, uint32_t col, int segs = 48);
    static void drawHealthBar(ImDrawList* dl, Vec2 boxPos, float boxH, float hp, float maxHp);
};
