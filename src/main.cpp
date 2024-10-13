#include "../include/RpcMonitor.h"
#include "../include/RpcServersConfig.h"
#include "../include/FileCrawler.h"
#include "../externals/json/single_include/nlohmann/json.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <imgui_impl_win32.cpp>

#include <iostream>
#include <fstream>
#include <string>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <commdlg.h>
#include <shlobj.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>


static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using json = nlohmann::json;

RpcMonitor* monitor;

std::string SelectDirectory(HWND owner)
{
    char path[MAX_PATH];
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = "Select a folder";

    LPITEMIDLIST idList = SHBrowseForFolder(&bi);
    if (idList != nullptr)
    {
        SHGetPathFromIDListA(idList, path);
        CoTaskMemFree(idList);
        return std::string(path);
    }
    return "";
}

void RunGuiAndMonitor(HWND hwnd)
{
    std::string rpcServersFile;
    std::string outputFilename;
    std::string startDir;
    static int selectedFileIndex = -1;

    std::vector<std::string> foundFiles;
    std::atomic<bool> isCrawling(false);
    std::mutex foundFilesMutex;

    // ImGui Setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    const ImVec2 fixedWindowSize(800, 600);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // GUI Elements
        ImGui::SetNextWindowSize(fixedWindowSize);
        ImGui::Begin("RPC Monitor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);

        if (ImGui::Button("Select Starting Directory"))
        {
            startDir = SelectDirectory(hwnd);
            std::cout << "Selected Starting Directory: " << startDir << std::endl;
        }

        ImGui::Text("Starting Directory: %s", startDir.c_str());

        if (!startDir.empty())
        {
            if (ImGui::Button("Find RPC Files"))
            {
                if (!isCrawling)
                {
                    isCrawling = true;
                    std::thread([&, startDir]() {
                        FileCrawler crawler(startDir);
                        std::vector<std::string> extensions = { ".json", ".xml" };
                        std::vector<std::string> tempFiles = crawler.findFiles(extensions);
                        {
                            std::lock_guard<std::mutex> lock(foundFilesMutex);
                            foundFiles = std::move(tempFiles);
                        }
                        isCrawling = false;
                        }).detach();
                }
            }

            if (isCrawling)
            {
                ImGui::Text("Crawling for RPC files...");
            }
            else
            {
                if (!foundFiles.empty())
                {
                    ImGui::Text("Select RPC Server File:");
                    if (ImGui::BeginCombo("Files", selectedFileIndex == -1 ? "Select a file" : foundFiles[selectedFileIndex].c_str()))
                    {
                        std::lock_guard<std::mutex> lock(foundFilesMutex);
                        for (int i = 0; i < foundFiles.size(); i++)
                        {
                            bool isSelected = (selectedFileIndex == i);
                            if (ImGui::Selectable(foundFiles[i].c_str(), isSelected))
                            {
                                selectedFileIndex = i;
                                rpcServersFile = foundFiles[selectedFileIndex];
                                std::cout << "Selected RPC Server File: " << rpcServersFile << std::endl;
                            }
                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::Text("No RPC files found.");
                }
            }
        }
        else 
        {
            ImGui::Text("No directory selected.");
        }

        if (ImGui::Button("Select Output Filename"))
        {
            OPENFILENAME ofn;
            char szFile[260];

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            ofn.lpstrTitle = "Select Output Filename";
            ofn.Flags = OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn))
            {
                outputFilename = szFile;
                std::cout << "Selected Output Filename: " << outputFilename << std::endl;
            }
        }

        if (ImGui::Button("Start Monitor") && selectedFileIndex != -1 && !outputFilename.empty())
        {
            try
            {
                RpcServersConfig rpcConfig = RpcServersConfig::load(rpcServersFile);
                std::cout << "Loaded RPC server configurations from " << rpcServersFile << std::endl;

                monitor = new RpcMonitor(rpcConfig);
                std::cout << "Starting RPC session..." << std::endl;
                monitor->start();
            }
            catch (const std::exception& e)
            {
                std::cerr << "An error occurred while loading RPC servers: " << e.what() << std::endl;
            }
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // cleanup
    if (monitor)
    {
        monitor->stop();
        delete monitor;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

int main(int argc, char* argv[])
{
    bool guiMode = (argc == 2 && std::string(argv[1]) == "--gui");
    if (!guiMode)
    {
        std::cerr << "Usage: " << argv[0] << " --gui" << std::endl;
        return 1;
    }

    // Window Setup
    const int windowWidth = 800;
    const int windowHeight = 600;
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("RPC Monitor"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("RPC Monitor"), WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 100, 100, windowWidth, windowHeight, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    RunGuiAndMonitor(hwnd);

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pd3dDeviceContext);
    if (FAILED(hr))
    {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)))
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
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
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}