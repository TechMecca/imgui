// Win32 + Direct3D 9 host: a borderless window that the ImGui UI (app.cpp) fills completely.

#include "app.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "theme.h"
#include "window.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <dwmapi.h>
#include <math.h>
#include <wchar.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows 11 DWM attributes (declared here so older SDKs build too).
static const DWORD kDwmWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
static const DWORD kDwmBorderColor            = 34; // DWMWA_BORDER_COLOR
static const DWORD kDwmCornerRound            = 2;  // DWMWCP_ROUND

static const wchar_t* kWindowClass = L"ImGuiAppWindow";

static LPDIRECT3D9           g_pD3D = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice = nullptr;
static D3DPRESENT_PARAMETERS g_d3dpp = {};
static bool                  g_DeviceLost = false;

static HWND  g_hWnd = nullptr;
static float g_DpiScale = 1.0f;
static bool  g_DpiChanged = false;
static bool  g_RoundedCorners = false;

// Window requests from the UI, applied between frames.
static bool  g_ResizePending = false;
static float g_ResizeWidth = 0.0f, g_ResizeHeight = 0.0f;
static bool  g_MinimizePending = false;
static bool  g_Dragging = false;
static POINT g_DragCursorStart = {}, g_DragWindowStart = {};

//-----------------------------------------------------------------------------
// Direct3D 9
//-----------------------------------------------------------------------------

static bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    RECT rc;
    ::GetClientRect(hWnd, &rc);
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.BackBufferWidth = (UINT)(rc.right - rc.left);
    g_d3dpp.BackBufferHeight = (UINT)(rc.bottom - rc.top);
    g_d3dpp.EnableAutoDepthStencil = FALSE;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // vsync

    // Prefer hardware vertex processing, fall back to software (old GPUs / some VMs).
    const DWORD flags = D3DCREATE_FPU_PRESERVE;
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, flags | D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0 &&
        g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, flags | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;
    return true;
}

static void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

static void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_DEVICELOST)
    {
        g_DeviceLost = true; // retried from the main loop
        return;
    }
    IM_ASSERT(hr != D3DERR_INVALIDCALL);
    g_DeviceLost = false;
    ImGui_ImplDX9_CreateDeviceObjects();
}

//-----------------------------------------------------------------------------
// Window services used by the UI (window.h)
//-----------------------------------------------------------------------------

namespace Window
{
    void Resize(float width, float height)
    {
        g_ResizePending = true;
        g_ResizeWidth = width;
        g_ResizeHeight = height;
    }

    void StartDrag()
    {
        RECT rc;
        if (!::GetCursorPos(&g_DragCursorStart) || !::GetWindowRect(g_hWnd, &rc))
            return;
        g_DragWindowStart = { rc.left, rc.top };
        g_Dragging = true;
    }

    void Minimize()          { g_MinimizePending = true; }
    void Close()             { ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0); }
    bool HasRoundedCorners() { return g_RoundedCorners; }
}

// Resizes the window around its current center, kept inside the monitor's work area.
static void ApplyResize(float width, float height)
{
    const int w = (int)lroundf(width * g_DpiScale);
    const int h = (int)lroundf(height * g_DpiScale);

    RECT rc;
    ::GetWindowRect(g_hWnd, &rc);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfoW(::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
    const RECT& work = mi.rcWork;

    int x = (rc.left + rc.right) / 2 - w / 2;
    int y = (rc.top + rc.bottom) / 2 - h / 2;
    x = (x + w > work.right) ? work.right - w : x;
    y = (y + h > work.bottom) ? work.bottom - h : y;
    x = (x < work.left) ? work.left : x;
    y = (y < work.top) ? work.top : y;
    ::SetWindowPos(g_hWnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void UpdateDrag()
{
    if (!g_Dragging)
        return;
    const int button = ::GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    if ((::GetAsyncKeyState(button) & 0x8000) == 0)
    {
        g_Dragging = false;
        return;
    }
    POINT p;
    if (::GetCursorPos(&p))
        ::SetWindowPos(g_hWnd, nullptr, g_DragWindowStart.x + p.x - g_DragCursorStart.x, g_DragWindowStart.y + p.y - g_DragCursorStart.y,
                       0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void ApplyWindowStyle(HWND hwnd)
{
    // Windows 11: rounded corners and a border that matches the theme. Fails harmlessly on Windows 10.
    const DWORD corner = kDwmCornerRound;
    g_RoundedCorners = SUCCEEDED(::DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner)));
    if (g_RoundedCorners)
    {
        const ImVec4& c = Theme::Colors.Border;
        const COLORREF border = RGB((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f));
        ::DwmSetWindowAttribute(hwnd, kDwmBorderColor, &border, sizeof(border));
    }
}

//-----------------------------------------------------------------------------
// Window procedure
//-----------------------------------------------------------------------------

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MAXIMIZED) // e.g. started from a "Run: Maximized" shortcut; the layout is fixed-size
        {
            ::PostMessageW(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
            return 0;
        }
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            const UINT w = (UINT)LOWORD(lParam), h = (UINT)HIWORD(lParam);
            if (w > 0 && h > 0 && (w != g_d3dpp.BackBufferWidth || h != g_d3dpp.BackBufferHeight))
            {
                g_d3dpp.BackBufferWidth = w;
                g_d3dpp.BackBufferHeight = h;
                ResetDevice();
            }
        }
        return 0;
    case WM_DPICHANGED:
    {
        // Moved to a monitor with a different scale: rebuild fonts/style and take the suggested size.
        g_DpiScale = (float)LOWORD(wParam) / 96.0f;
        g_DpiChanged = true;
        const RECT* r = (const RECT*)lParam;
        ::SetWindowPos(hWnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// Entry point
//-----------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    const bool demo_failure = wcsstr(::GetCommandLineW(), L"--demo-fail") != nullptr;

    // Start on the monitor under the mouse, at that monitor's scale.
    POINT cursor = {};
    ::GetCursorPos(&cursor);
    HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    g_DpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfoW(monitor, &mi);
    const int w = (int)lroundf(Theme::ConnectWindowWidth * g_DpiScale);
    const int h = (int)lroundf(Theme::ConnectWindowHeight * g_DpiScale);
    const int x = (mi.rcWork.left + mi.rcWork.right - w) / 2;
    const int y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;

    wchar_t title[64] = {};
    ::MultiByteToWideChar(CP_UTF8, 0, Theme::AppName, -1, title, IM_ARRAYSIZE(title) - 1);
    HBRUSH background = ::CreateSolidBrush(RGB(10, 11, 15)); // matches Theme::Colors.Bg, avoids a white flash

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = ::LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = ::LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = background;
    wc.lpszClassName = kWindowClass;
    ::RegisterClassExW(&wc);

    // Borderless popup that still behaves like an app window (taskbar button, minimize, Alt+Space menu).
    g_hWnd = ::CreateWindowExW(WS_EX_APPWINDOW, kWindowClass, title, WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                               x, y, w, h, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd || !CreateDeviceD3D(g_hWnd))
    {
        CleanupDeviceD3D();
        if (g_hWnd)
            ::DestroyWindow(g_hWnd);
        ::UnregisterClassW(kWindowClass, hInstance);
        ::DeleteObject(background);
        ::MessageBoxW(nullptr, L"Could not initialize Direct3D 9.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // nothing to persist: the layout is fixed

    Theme::Init(g_DpiScale);
    ApplyWindowStyle(g_hWnd);
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    App app;
    app.Init(demo_failure);

    ::ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_hWnd);

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Nothing to draw while minimized or while the device is lost (e.g. after a lock screen).
        if (::IsIconic(g_hWnd))
        {
            ::Sleep(16);
            continue;
        }
        if (g_DeviceLost)
        {
            const HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            if (g_DeviceLost)
                continue;
        }

        UpdateDrag();
        if (g_DpiChanged)
        {
            g_DpiChanged = false;
            ImGui_ImplDX9_InvalidateDeviceObjects(); // font texture is rebuilt on the next NewFrame
            Theme::SetDpiScale(g_DpiScale);
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.Frame();
        ImGui::Render();

        const ImVec4 c = app.ClearColor();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f), 255), 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        if (g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST)
            g_DeviceLost = true;

        // Window changes requested by the UI during the frame.
        if (g_ResizePending)
        {
            g_ResizePending = false;
            ApplyResize(g_ResizeWidth, g_ResizeHeight);
        }
        if (g_MinimizePending)
        {
            g_MinimizePending = false;
            g_Dragging = false;
            ::ShowWindow(g_hWnd, SW_MINIMIZE);
        }
    }

    app.Shutdown();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    if (::IsWindow(g_hWnd))
        ::DestroyWindow(g_hWnd);
    ::UnregisterClassW(kWindowClass, hInstance);
    ::DeleteObject(background);
    return 0;
}
