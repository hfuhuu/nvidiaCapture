// =============================================================================
//  NVIDIA Scanout Capture for Anti-Cheat - Proof of Concept
// =============================================================================
//
//  Demonstrates that NvAPI_D3D11_WksReadScanout (undocumented, interface
//  0xBCB1C536) can read the GPU scanout buffer directly, bypassing any
//  screenshot-blocking techniques used by cheat software to hide overlays,
//  wallhacks, aimbots, or other visual modifications from anti-cheat systems.
//
//  Cheats commonly hook the graphics pipeline to block or return blank
//  screenshots so that anti-cheat screen capture sees a clean frame.
//  This tool reads the scanout buffer at the hardware level, capturing
//  exactly what the display controller sends to the monitor - including
//  any cheat overlays the software is trying to hide.
//
//  Requirements:
//    - NVIDIA GPU with compatible driver
//    - Workstation feature support in the driver
//    - Administrator privileges (or queueIndex 14 / Shadowplay path)
//
// Author: Manuel Herrera (TheCruZ)
// =============================================================================

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "user32.lib")

// ---- NVAPI types -----------------------------------------------------------

struct NV_RECT
{
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
};

struct NV_WKS_READ_SCANOUT_PARAMS
{
    uint32_t version;        // offset 0:  MAKE_NVAPI_VERSION(this, 1) = sizeof | (1<<16)
    uint32_t targetId;       // offset 4:  OS display target ID from QueryDisplayConfig()
    void* hPhysicalGpu;      // offset 8:  Physical GPU handle (for SLI GPU selection)
    NV_RECT rect;            // offset 16: IN: desired rect (0,0,0,0 = whole). OUT: actual captured rect
    struct {                  // offset 32: flags bitfield (8 bytes)
        uint64_t queueIndex            : 4;  // 0=OS render, 1=scanout, 14=Shadowplay (no admin)
        uint64_t mpoIndex              : 3;
        uint64_t rightStereoEye        : 1;
        uint64_t leftAndRightStereoEye : 1;
        uint64_t osRenderBuffer        : 1;  // OUT
        uint64_t scanoutBuffer         : 1;  // OUT
        uint64_t warping               : 1;  // OUT
        uint64_t perPixelIntensity     : 1;  // OUT
        uint64_t intensityRGTexture    : 1;  // OUT
        uint64_t intensityBATexture    : 1;  // OUT
        uint64_t grayscaleDome         : 1;  // OUT
        uint64_t grayscaleEizo         : 1;  // OUT
        uint64_t yuv420                : 1;  // OUT
        uint64_t yuv422                : 1;  // OUT
        uint64_t yuv444                : 1;  // OUT
        uint64_t hdr                   : 1;  // OUT
        uint64_t smoothScaling         : 1;  // OUT
        uint64_t eshift                : 1;  // OUT
        uint64_t virtualSplit          : 1;  // OUT
        uint64_t streamSource          : 1;  // OUT
        uint64_t reserved              : 39;
    } flags;
    uint32_t format;         // offset 40: OUT: NV_FORMAT (D3DDDIFORMAT)
    uint32_t _pad;           // offset 44: alignment padding
    void** ppData;           // offset 48: OUT: pixel data (VirtualAlloc'd, caller frees with VirtualFree)
};
// sizeof = 56 = 0x38

typedef void*    (*nvapi_QueryInterface_t)(uint32_t id);
typedef uint32_t (*nvapi_Initialize_t)();
typedef uint32_t (*nvapi_EnumPhysicalGPUs_t)(void* handles[64], uint32_t* count);
typedef uint32_t (*nvapi_WksReadScanout_t)(ID3D11Device* pDev, NV_WKS_READ_SCANOUT_PARAMS* params);

// ---- Warning banner --------------------------------------------------------

static void PrintCheatWarning(const std::string& reason, uint32_t nvapiStatus = 0)
{
    std::cerr << "\n";
    std::cerr << "  ##############################################################\n";
    std::cerr << "  ##                                                          ##\n";
    std::cerr << "  ##   !!  CAPTURE FAILED - POSSIBLE CHEAT INTERFERENCE  !!   ##\n";
    std::cerr << "  ##                                                          ##\n";
    std::cerr << "  ##############################################################\n";
    std::cerr << "\n";
    std::cerr << "  Reason : " << reason << "\n";
    if (nvapiStatus != 0)
        std::cerr << "  NVAPI  : 0x" << std::hex << nvapiStatus << std::dec << "\n";
    std::cerr << "\n";
    std::cerr << "  The scanout capture failed. This may indicate that cheat\n";
    std::cerr << "  software is intercepting GPU-level framebuffer reads to\n";
    std::cerr << "  hide its visual modifications from anti-cheat screenshots.\n";
    std::cerr << "\n";
}

// ---- GPU detection ---------------------------------------------------------

static bool HasNvidiaGpu()
{
    IDXGIFactory* factory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory))))
        return false;

    bool found = false;
    IDXGIAdapter* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
    {
        DXGI_ADAPTER_DESC desc{};
        adapter->GetDesc(&desc);
        adapter->Release();

        // NVIDIA vendor ID = 0x10DE
        if (desc.VendorId == 0x10DE)
        {
            std::wcout << L"[+] NVIDIA GPU found: " << desc.Description << std::endl;
            found = true;
            break;
        }
    }

    factory->Release();
    return found;
}

// ---- Display target --------------------------------------------------------

static uint32_t GetPrimaryDisplayTargetId()
{
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
        return 0;

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
        return 0;

    if (pathCount > 0)
        return paths[0].targetInfo.id;

    return 0;
}

// ---- PNG save --------------------------------------------------------------

static HRESULT SaveBufferToPng(const void* data, uint32_t width, uint32_t height, uint32_t stride, const wchar_t* filename)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != S_FALSE && hr != RPC_E_CHANGED_MODE)
        return hr;

    IWICImagingFactory* factory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;

    IWICStream* stream = nullptr;
    factory->CreateStream(&stream);
    hr = stream->InitializeFromFilename(filename, GENERIC_WRITE);
    if (FAILED(hr))
    {
        stream->Release();
        factory->Release();
        return hr;
    }

    IWICBitmapEncoder* encoder = nullptr;
    factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    encoder->Initialize(stream, WICBitmapEncoderNoCache);

    IWICBitmapFrameEncode* frame = nullptr;
    encoder->CreateNewFrame(&frame, nullptr);
    frame->Initialize(nullptr);
    frame->SetSize(width, height);

    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    frame->SetPixelFormat(&pixelFormat);

    for (uint32_t y = 0; y < height; y++)
    {
        const BYTE* row = static_cast<const BYTE*>(data) + y * stride;
        hr = frame->WritePixels(1, width * 4, width * 4, const_cast<BYTE*>(row));
        if (FAILED(hr))
            break;
    }

    frame->Commit();
    encoder->Commit();

    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    CoUninitialize();

    return hr;
}

// ---- Main ------------------------------------------------------------------

int main()
{
    std::cout << "=============================================================\n";
    std::cout << "  NVIDIA Scanout Capture for Anti-Cheat - Proof of Concept\n";
    std::cout << "=============================================================\n\n";

    // Step 1: Check for NVIDIA GPU via DXGI
    std::cout << "[*] Checking for NVIDIA GPU...\n";
    if (!HasNvidiaGpu())
    {
        std::cout << "No NVIDIA GPU detected via DXGI (vendor 0x10DE)";
        return 1;
    }

    // Step 2: Create D3D11 device
    std::cout << "[*] Creating D3D11 device...\n";
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &level, 1, D3D11_SDK_VERSION,
        &device, nullptr, &ctx);

    if (FAILED(hr))
    {
        PrintCheatWarning("D3D11 device creation failed (hr=0x" +
            ([&]{ char b[16]; snprintf(b,sizeof(b),"%08X",hr); return std::string(b); })() + ")");
        return 1;
    }
    std::cout << "[+] D3D11 device created\n";

    // Step 3: Load NVAPI
    std::cout << "[*] Loading nvapi64.dll...\n";
    HMODULE nvapiModule = LoadLibraryA("nvapi64.dll");
    if (!nvapiModule)
    {
        PrintCheatWarning("Cannot load nvapi64.dll - NVIDIA driver missing or blocked");
        ctx->Release(); device->Release();
        return 1;
    }

    auto QueryInterface = reinterpret_cast<nvapi_QueryInterface_t>(
        GetProcAddress(nvapiModule, "nvapi_QueryInterface"));
    if (!QueryInterface)
    {
        PrintCheatWarning("nvapi_QueryInterface export not found - driver tampered?");
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    std::cout << "[+] nvapi64.dll loaded\n";

    // Step 4: Initialize NVAPI
    std::cout << "[*] Initializing NVAPI...\n";
    auto nvInit = reinterpret_cast<nvapi_Initialize_t>(QueryInterface(0x0150E828));
    if (!nvInit)
    {
        PrintCheatWarning("NvAPI_Initialize (0x0150E828) not found in driver");
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    uint32_t status = nvInit();
    if (status != 0)
    {
        PrintCheatWarning("NvAPI_Initialize failed", status);
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    std::cout << "[+] NVAPI initialized\n";

    // Step 5: Enumerate physical GPUs
    std::cout << "[*] Enumerating physical GPUs...\n";
    auto nvEnumGPUs = reinterpret_cast<nvapi_EnumPhysicalGPUs_t>(QueryInterface(0xE5AC921F));
    void* gpuHandles[64] = {};
    uint32_t gpuCount = 0;
    if (!nvEnumGPUs || (status = nvEnumGPUs(gpuHandles, &gpuCount)) != 0 || gpuCount == 0)
    {
        PrintCheatWarning("NvAPI_EnumPhysicalGPUs failed or returned 0 GPUs", status);
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    std::cout << "[+] Found " << gpuCount << " physical GPU(s)\n";

    // Step 6: Get display target ID
    std::cout << "[*] Querying display target ID...\n";
    uint32_t targetId = GetPrimaryDisplayTargetId();
    if (targetId == 0)
    {
        PrintCheatWarning("QueryDisplayConfig returned no active display target");
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    std::cout << "[+] Primary display target ID: " << targetId << "\n";

    // Step 7: Resolve the undocumented scanout read function
    std::cout << "[*] Resolving NvAPI_D3D11_WksReadScanout (0xBCB1C536)...\n";
    auto nvReadScanout = reinterpret_cast<nvapi_WksReadScanout_t>(QueryInterface(0xBCB1C536));
    if (!nvReadScanout)
    {
        PrintCheatWarning("NvAPI_D3D11_WksReadScanout (0xBCB1C536) not found - "
                          "driver may lack workstation capture support");
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }
    std::cout << "[+] Function resolved\n";

    // Step 8: Perform scanout capture
    std::cout << "[*] Performing scanout capture...\n";

    void* capturedData = nullptr;
    NV_WKS_READ_SCANOUT_PARAMS params{};
    params.version      = sizeof(NV_WKS_READ_SCANOUT_PARAMS) | (1 << 16);
    params.targetId     = targetId;
    params.hPhysicalGpu = gpuHandles[0];
    params.ppData       = reinterpret_cast<void**>(&capturedData);
    // rect {0,0,0,0} = capture the entire scanout surface
    // flags.queueIndex = 0 = OS render buffer (requires admin)

    status = nvReadScanout(device, &params);
    if (status != 0)
    {
        std::string detail;
        switch (static_cast<int32_t>(status))
        {
        case -3:   detail = "NVAPI_NO_IMPLEMENTATION - driver does not support this function"; break;
        case -4:   detail = "NVAPI_API_NOT_INITIALIZED"; break;
        case -5:   detail = "NVAPI_INVALID_ARGUMENT - struct layout or version mismatch"; break;
        case -6:   detail = "NVAPI_INVALID_USER_PRIVILEGE - run as Administrator or use queueIndex=14"; break;
        case -9:   detail = "NVAPI_INCOMPATIBLE_STRUCT_VERSION"; break;
        case -101: detail = "NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE"; break;
        case -102: detail = "NVAPI_EXPECTED_DISPLAY_HANDLE - targetId invalid"; break;
        default:   detail = "Unknown NVAPI error"; break;
        }
        PrintCheatWarning("NvAPI_D3D11_WksReadScanout failed: " + detail, status);
        FreeLibrary(nvapiModule); ctx->Release(); device->Release();
        return 1;
    }

    uint32_t width  = params.rect.right - params.rect.left;
    uint32_t height = params.rect.bottom - params.rect.top;

    std::cout << "[+] Capture succeeded!\n";
    std::cout << "    Region : " << params.rect.left << "," << params.rect.top
              << " -> " << params.rect.right << "," << params.rect.bottom
              << " (" << width << "x" << height << ")\n";
    std::cout << "    Format : " << params.format << "\n";
    std::cout << "    Data   : " << capturedData << "\n";

    // Step 9: Save to PNG
    if (width > 0 && height > 0 && capturedData)
    {
        std::cout << "[*] Saving capture.png...\n";
        hr = SaveBufferToPng(capturedData, width, height, width * 4, L"capture.png");
        if (FAILED(hr))
        {
            PrintCheatWarning("Failed to encode PNG (hr=0x" +
                ([&]{ char b[16]; snprintf(b,sizeof(b),"%08X",hr); return std::string(b); })() + ")");
        }
        else
        {
            std::cout << "[+] Saved capture.png (" << width << "x" << height << ")\n";
        }
        VirtualFree(capturedData, 0, MEM_RELEASE);
    }
    else
    {
        PrintCheatWarning("Capture returned empty data (width=" + std::to_string(width) +
                          " height=" + std::to_string(height) + ")");
    }

    // Cleanup
    ctx->Release();
    device->Release();
    FreeLibrary(nvapiModule);

    std::cout << "\n[*] Done.\n";
    return 0;
}
