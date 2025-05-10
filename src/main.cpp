#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <tchar.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "InputHandler.h"
#include "TrainerLogic.h"
#include <memory>

// Forward declaration for RenderDebugOverlay
void RenderDebugOverlay();

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// Data
static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RenderDebugOverlay();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Delete ImGui settings file so widgets always start at default position/size
    DeleteFileA("imgui.ini");

    // Register window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("SuperglideTrainer"), NULL };
    RegisterClassEx(&wc);

    // Create a borderless, always-on-top, transparent window
    HWND hwnd = CreateWindow(
        wc.lpszClassName, _T("Superglide Trainer"),
        WS_POPUP, // Borderless
        100, 100, 900, 500,
        NULL, NULL, wc.hInstance, NULL);

    // Make window always on top and layered (for transparency)
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOPMOST | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY); // Black is transparent

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 purple = ImVec4(0.45f, 0.25f, 0.75f, 1.0f);
    ImVec4 purpleAccent = ImVec4(0.60f, 0.35f, 0.90f, 1.0f);
    ImVec4 bgDark = ImVec4(0.10f, 0.08f, 0.15f, 0.0f);
    ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_WindowBg] = bgDark;
    style.Colors[ImGuiCol_ChildBg] = bgDark;
    style.Colors[ImGuiCol_TitleBg] = purple;
    style.Colors[ImGuiCol_TitleBgActive] = purpleAccent;
    style.Colors[ImGuiCol_Border] = purpleAccent;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.13f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = purple;
    style.Colors[ImGuiCol_FrameBgActive] = purpleAccent;
    style.Colors[ImGuiCol_Button] = purple;
    style.Colors[ImGuiCol_ButtonHovered] = purpleAccent;
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.45f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Header] = purple;
    style.Colors[ImGuiCol_HeaderHovered] = purpleAccent;
    style.Colors[ImGuiCol_HeaderActive] = purpleAccent;
    style.Colors[ImGuiCol_Separator] = purpleAccent;
    style.Colors[ImGuiCol_SliderGrab] = purpleAccent;
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.75f, 0.45f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = purpleAccent;
    // style.Colors[ImGuiCol_ProgressBarBg] = ImVec4(0.18f, 0.13f, 0.25f, 1.0f);
    // style.Colors[ImGuiCol_ProgressBar] = purpleAccent;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(18, 14);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Modular components
    std::unique_ptr<InputHandler> inputHandler = std::make_unique<InputHandler>(hwnd);
    std::unique_ptr<TrainerLogic> trainerLogic = std::make_unique<TrainerLogic>();

    // State
    bool showSettings = true;
    bool running = false;
    enum class BindMode { None, Jump, Crouch };
    BindMode bindMode = BindMode::None;
    WPARAM jumpKey = 0, crouchKey = 0;
    char jumpKeyName[32] = "", crouchKeyName[32] = "";
    bool keysChanged = false;

    // Main loop
    bool done = false;
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (!done)
    {
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Update input
        inputHandler->Update();
        // Only update trainer logic if running and both keys are set
        if (running && jumpKey && crouchKey) {
            trainerLogic->Update(*inputHandler);
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // DockSpace host window for overlay, drag, and docking
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, transparent);
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        dockspace_flags &= ~ImGuiDockNodeFlags_AutoHideTabBar; // Always show tab bar
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        // Main menu bar (optional, can be in DockSpace host or as a window)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Settings")) {
                showSettings = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Settings window (top-level)
        if (showSettings) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
            ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_NoCollapse);
            ImGui::TextColored(purpleAccent, "Key Bindings:");
            ImGui::Separator();
            ImGui::Text("Jump: %s", jumpKey ? jumpKeyName : "Not set");
            ImGui::SameLine();
            ImGui::BeginDisabled(running || bindMode != BindMode::None);
            if (ImGui::Button("Bind Jump")) {
                inputHandler->BeginKeyBinding();
                bindMode = BindMode::Jump;
            }
            ImGui::EndDisabled();
            ImGui::Text("Crouch: %s", crouchKey ? crouchKeyName : "Not set");
            ImGui::SameLine();
            ImGui::BeginDisabled(running || bindMode != BindMode::None);
            if (ImGui::Button("Bind Crouch")) {
                inputHandler->BeginKeyBinding();
                bindMode = BindMode::Crouch;
            }
            ImGui::EndDisabled();
            if (bindMode != BindMode::None) {
                ImGui::TextColored(ImVec4(1,1,0,1), "Press a key...");
                if (!inputHandler->IsBinding()) {
                    jumpKey = inputHandler->GetJumpKey();
                    crouchKey = inputHandler->GetCrouchKey();
                    GetKeyNameTextA(MapVirtualKeyA((UINT)jumpKey, 0) << 16, jumpKeyName, sizeof(jumpKeyName));
                    GetKeyNameTextA(MapVirtualKeyA((UINT)crouchKey, 0) << 16, crouchKeyName, sizeof(crouchKeyName));
                    keysChanged = true;
                    bindMode = BindMode::None;
                }
            }
            ImGui::Separator();
            bool canStart = jumpKey && crouchKey;
            ImGui::BeginDisabled(!canStart && !running);
            if (ImGui::Button(running ? "Stop Trainer" : "Start Trainer")) {
                if (running) {
                    running = false;
                    trainerLogic = std::make_unique<TrainerLogic>();
                } else if (canStart) {
                    running = true;
                    trainerLogic = std::make_unique<TrainerLogic>();
                }
            }
            ImGui::EndDisabled();
            if (ImGui::Button("Reset Window Size/Pos")) {
                HWND hwnd = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
                if (hwnd) {
                    MoveWindow(hwnd, 100, 100, 900, 500, TRUE);
                }
            }
            if (ImGui::Button("Exit")) {
                HWND hwnd = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
                if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            ImGui::PopStyleVar(2);
            ImGui::End();
        }

        // Superglide Trainer window (top-level)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 18));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::Begin("Superglide Trainer", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::PushFont(NULL);
        ImGui::TextColored(purpleAccent, "Status: %s", running ? "Running" : "Stopped");
        ImGui::Text("Jump Key: %s", jumpKey ? jumpKeyName : "Not set");
        ImGui::Text("Crouch Key: %s", crouchKey ? crouchKeyName : "Not set");
        ImGui::Separator();
        // FPS input logic
        if (running && jumpKey && crouchKey && trainerLogic->IsAwaitingFPS()) {
            static char fpsBuf[16] = "";
            ImGui::Text("Enter your target FPS (e.g. 144):");
            ImGui::InputText("##fps", fpsBuf, sizeof(fpsBuf), ImGuiInputTextFlags_CharsDecimal);
            if (ImGui::Button("Set FPS") && atof(fpsBuf) > 0.0) {
                trainerLogic->SetTargetFPS(atof(fpsBuf));
            }
        } else if (running && jumpKey && crouchKey) {
            trainerLogic->Update(*inputHandler);
            ImGui::Text("Attempt: %d", trainerLogic->GetAttempt());
            ImGui::Text("Frame time: %.3f ms (%.2f FPS)", trainerLogic->GetFrameTime() * 1000.0, trainerLogic->GetTargetFPS());
            ImGui::Text("Last delay: %.6f ms (%.6f frames)", trainerLogic->GetLastDeltaMs(), trainerLogic->GetLastDeltaFrames());
            if (trainerLogic->GetLastChance() > 0.0)
                ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Chance to hit: %.6f%%", trainerLogic->GetLastChance());
            else
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Chance to hit: 0%%");
            ImGui::TextWrapped("%s", trainerLogic->GetFeedback().c_str());
            if (trainerLogic->GetAttempt() > 0)
                ImGui::Text("Average: %.1f%%", trainerLogic->GetCumulativePercent() / trainerLogic->GetAttempt());
            ImGui::ProgressBar((float)(trainerLogic->GetLastDeltaFrames() / 2.0), ImVec2(240, 24), "");
        } else {
            ImGui::Text("Press Start in Settings to begin training.");
        }
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        // Allow resizing from all edges/corners (8px border)
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hWnd, &pt);

        RECT rc;
        GetClientRect(hWnd, &rc);
        const int border = 8;

        // Left
        if (pt.x >= rc.left && pt.x < rc.left + border) {
            if (pt.y >= rc.top && pt.y < rc.top + border) return HTTOPLEFT;
            if (pt.y >= rc.bottom - border && pt.y < rc.bottom) return HTBOTTOMLEFT;
            return HTLEFT;
        }
        // Right
        if (pt.x >= rc.right - border && pt.x < rc.right) {
            if (pt.y >= rc.top && pt.y < rc.top + border) return HTTOPRIGHT;
            if (pt.y >= rc.bottom - border && pt.y < rc.bottom) return HTBOTTOMRIGHT;
            return HTRIGHT;
        }
        // Top
        if (pt.y >= rc.top && pt.y < rc.top + border) return HTTOP;
        // Bottom
        if (pt.y >= rc.bottom - border && pt.y < rc.bottom) return HTBOTTOM;
        break;
    }
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
} 