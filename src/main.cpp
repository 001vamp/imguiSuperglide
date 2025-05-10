#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "InputHandler.h"
#include "TrainerLogic.h"
#include <memory>

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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Register window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("SuperglideTrainer"), NULL };
    RegisterClassEx(&wc);

    // Create a normal window (not transparent)
    HWND hwnd = CreateWindow(
        wc.lpszClassName, _T("Superglide Trainer"),
        WS_OVERLAPPEDWINDOW,
        100, 100, 900, 500,
        NULL, NULL, wc.hInstance, NULL);

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

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

        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Settings")) {
                showSettings = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Settings window
        if (showSettings) {
            ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Key Bindings:");
            ImGui::Separator();
            // Always show the current keys from InputHandler
            jumpKey = inputHandler->GetJumpKey();
            crouchKey = inputHandler->GetCrouchKey();
            GetKeyNameTextA(MapVirtualKeyA((UINT)jumpKey, 0) << 16, jumpKeyName, sizeof(jumpKeyName));
            GetKeyNameTextA(MapVirtualKeyA((UINT)crouchKey, 0) << 16, crouchKeyName, sizeof(crouchKeyName));
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
                // Wait for binding to finish
                if (!inputHandler->IsBinding()) {
                    // Update the correct key and name
                    jumpKey = inputHandler->GetJumpKey();
                    crouchKey = inputHandler->GetCrouchKey();
                    GetKeyNameTextA(MapVirtualKeyA((UINT)jumpKey, 0) << 16, jumpKeyName, sizeof(jumpKeyName));
                    GetKeyNameTextA(MapVirtualKeyA((UINT)crouchKey, 0) << 16, crouchKeyName, sizeof(crouchKeyName));
                    keysChanged = true;
                    bindMode = BindMode::None;
                }
            }
            ImGui::Separator();
            // Only enable Start Trainer if both keys are set
            bool canStart = jumpKey && crouchKey;
            ImGui::BeginDisabled(!canStart && !running);
            if (ImGui::Button(running ? "Stop Trainer" : "Start Trainer")) {
                if (running) {
                    running = false;
                    trainerLogic = std::make_unique<TrainerLogic>(); // Reset logic
                } else if (canStart) {
                    running = true;
                    trainerLogic = std::make_unique<TrainerLogic>(); // Reset logic
                }
            }
            ImGui::EndDisabled();
            ImGui::End();
        }

        // If keys were changed, reset trainer logic and stop trainer
        if (keysChanged) {
            running = false;
            trainerLogic = std::make_unique<TrainerLogic>();
            keysChanged = false;
        }

        // Main trainer window (moveable)
        ImGui::Begin("Superglide Trainer", nullptr, ImGuiWindowFlags_None);
        ImGui::Text("Status: %s", running ? "Running" : "Stopped");
        ImGui::Text("Jump Key: %s", jumpKey ? jumpKeyName : "Not set");
        ImGui::Text("Crouch Key: %s", crouchKey ? crouchKeyName : "Not set");
        if (running && jumpKey && crouchKey) {
            trainerLogic->RenderUI();
        } else {
            ImGui::Text("Press Start in Settings to begin training.");
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
} 