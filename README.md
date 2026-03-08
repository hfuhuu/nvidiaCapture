# NVIDIA Scanout Capture for Anti-Cheat

> **Proof of concept** showing how anti-cheat systems can use an undocumented NVIDIA driver function to capture the real screen content, bypassing screenshot-blocking techniques used by cheat software.

---

## The Problem

Cheat software (wallhacks, aimbots, ESP overlays, etc.) commonly hooks into the graphics pipeline to **block or fake screenshots** taken by anti-cheat systems. When an anti-cheat tries to capture the screen to look for visual evidence of cheating, the cheat intercepts the call and returns a clean frame — hiding its overlays, modified textures, and visual modifications.

Conventional capture methods (`BitBlt`, `DXGI OutputDuplication`, `PrintWindow`) all operate at the application or compositor level, where cheat hooks can easily intercept and sanitize the output.

## The Solution

The NVIDIA driver exposes an undocumented workstation function — `NvAPI_D3D11_WksReadScanout` (interface `0xBCB1C536`) — that reads directly from the **GPU scanout buffer**: the final composited image that the display controller sends to the monitor.

This happens at the hardware level, **below** any user-mode hooks that cheat software installs:

```
Application framebuffer
        |
        v
+-----------------------+
|   DWM / Compositor    |<-- Most block/flags/hooks happens at this point or before
+-----------------------+
        |
        v
+-----------------------+
|  GPU Scanout Buffer   |<-- This tool reads HERE (hardware level)
+-----------------------+
        |
        v
      Monitor
```

Since cheats hook the rendering pipeline or capture APIs, they **cannot intercept** a kernel-mode scanout read issued through the NVIDIA driver. The captured image shows exactly what the monitor displays — including any cheat overlays the software is trying to hide.

> **Note on DKOM:** In theory, a cheat running at kernel level could use Direct Kernel Object Manipulation (DKOM) to intercept or tamper with the driver's scanout read path — for example, by patching the NVIDIA kernel-mode dispatch table or manipulating the returned buffer. However, DKOM-based bypass of scanout captures is still uncommon in the wild due to the complexity involved, the risk of triggering PatchGuard / KPP, and the additional anti-cheat kernel drivers (e.g. EAC, BattlEye, Vanguard) that actively monitor for such modifications.

## Requirements

| Requirement | Details |
|---|---|
| **GPU** | NVIDIA GPU (detected via DXGI vendor ID `0x10DE`) |
| **Driver** | NVIDIA driver with workstation feature support |
| **OS** | Windows 10 or later (x64) |
| **Privileges** | Administrator |
| **Build tools** | Visual Studio 2022 (v143 toolset) or compatible MSVC compiler |

## Building

### Visual Studio (recommended)

1. Open `nvidiaCapture.sln` in Visual Studio 2022
2. Select **Release | x64** configuration
3. Build -> Build Solution (`Ctrl+Shift+B`)
4. Output: `x64/Release/nvidiaCapture.exe`

## Usage

```bat
:: Run as Administrator
nvidiaCapture.exe
```

On success, `capture.png` is saved in the working directory containing the real scanout buffer contents — including any cheat overlays that would be hidden from normal screenshots.

### Blocked capture (cheat interference detected)

When cheat software intercepts the capture:

```
  ##############################################################
  ##                                                          ##
  ##   !!  CAPTURE FAILED - POSSIBLE CHEAT INTERFERENCE  !!   ##
  ##                                                          ##
  ##############################################################

  Reason : NvAPI_D3D11_WksReadScanout failed: NVAPI_NO_IMPLEMENTATION ...
  NVAPI  : 0xfffffffd

  The scanout capture failed. This may indicate that cheat
  software is intercepting GPU-level framebuffer reads to
  hide its visual modifications from anti-cheat screenshots.
```

## NVAPI Error Reference

| Code | Value | Meaning |
|---|---|---|
| `0xfffffffd` | `-3` | `NVAPI_NO_IMPLEMENTATION` — driver lacks workstation capture support (common on GeForce consumer drivers) |
| `0xfffffffc` | `-4` | `NVAPI_API_NOT_INITIALIZED` — `NvAPI_Initialize` was not called |
| `0xfffffffb` | `-5` | `NVAPI_INVALID_ARGUMENT` — struct layout or version mismatch |
| `0xfffffffa` | `-6` | `NVAPI_INVALID_USER_PRIVILEGE` — not running as admin |
| `0xfffffff7` | `-9` | `NVAPI_INCOMPATIBLE_STRUCT_VERSION` — wrong version field in params |
| `0xffffff9b` | `-101` | `NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE` — invalid GPU handle in params |
| `0xffffff9a` | `-102` | `NVAPI_EXPECTED_DISPLAY_HANDLE` — `targetId` is zero or invalid |
| `0xffffff77` | `-137` | `NVAPI_INVALID_USER_PRIVILEDGE` — not running as admin |

## Queue Index Options

The `flags.queueIndex` field selects which stage of the scanout composition pipeline to capture:

Based on Nvidia leaks:
0: os render portion, 1 scanout portion, 2..15 : invisible intermediate portions.

## Technical Details

### Struct: `NV_WKS_READ_SCANOUT_PARAMS` (56 bytes)

```
Offset  Size  Field            Description
--------------------------------------------------------------
0x00    4     version          sizeof(struct) | (1 << 16) = 0x00010038
0x04    4     targetId         OS display target (QueryDisplayConfig)
0x08    8     hPhysicalGpu     NvPhysicalGpuHandle
0x10    16    rect             NV_RECT {left, top, right, bottom} (IN/OUT)
0x20    8     flags            Bitfield: queueIndex(4), mpoIndex(3), ...
0x28    4     format           OUT: NV_FORMAT / D3DDDIFORMAT
0x2C    4     _pad             Alignment padding
0x30    8     ppData           OUT: Pointer to VirtualAlloc'd pixel data
```

### Function signature

```cpp
// Interface ID: 0xBCB1C536
// Resolved via: nvapi_QueryInterface(0xBCB1C536)
NvAPI_Status NvAPI_D3D11_WksReadScanout(
    ID3D11Device*                pDev,
    NV_WKS_READ_SCANOUT_PARAMS*  pParams
);
```

### Output data

- `ppData` points to raw pixel data allocated by the driver via `VirtualAlloc()`
- Format is reported in `params.format` (typically `21` = `D3DFMT_A8R8G8B8` / BGRA8)
- The caller **must** free the buffer with `VirtualFree(data, 0, MEM_RELEASE)`

## Disclaimer

This tool is published for **educational and security research purposes**. It demonstrates a technique for anti-cheat systems to capture tamper-proof screenshots on NVIDIA hardware. Use responsibly and only on systems you own or have explicit authorization to test.

## Author

Manuel Herrera (TheCruZ)
