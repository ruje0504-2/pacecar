# Pacecar Switch / Windows 11 ARM64

## Current monitor update (2026-11-15)

| Field / behavior | Source and status |
|---|---|
| CPU / GPU / RAM | Fresh SwitchFan hardware readbacks; RAM uses the cached EMC IOCTL, never the old UEFI clock |
| Battery power | Battery14 cached MAX17050 current × voltage; positive charging, negative discharge; displayed as battery net watts |
| Power unavailable | Old driver, invalid calibration, failed read or sample older than6s renders `--`; real zero remains0.0W |
| Panel background | Default50% opacity (50% transparent), per latest user selection; text is unaffected |
| Startup | Per-user interactive scheduled task,60s after logon; existing single-instance behavior retained |
| Native acceptance | Battery14 fresh0xAC/0x7fff gauge sample verified:3.966875V, -57.5mA, -0.228095W. With Fan33 both apps were responding in session1 after boot16:11:46UTC; owner manual testing subsequently passed |

On AC power the battery-power column is not total system or charger input power.
CPU/GPU wattage has no verified sensor source and is not estimated from clocks.
The telemetry query reads a cached structure; the overlay does not issue I2C
transactions per frame. Battery charge limits are controlled independently.


Native C++20/Win32/Direct2D port of [binbuf/pacecar](https://github.com/binbuf/pacecar/tree/ab52b8e8ca860535dae6ee639dd7c9858ab6a509).
The upstream source and attribution are retained. The upstream revision has no
LICENSE file; no broader license is asserted for its code. The Switch changes
are maintained in [the user fork](https://github.com/ruje0504-2/pacecar).

## Display and controls

The default is a horizontal strip, 640 x 58 DIPs. A three-column compact panel
is also available. Click the notification-area icon once to show/hide the
overlay; closing the window hides it, and the tray's Exit command stops it.
Explorer restarting recreates the icon. Normal SoC polling needs no elevated
helper. The settings retain position, size, visibility, font, hotkeys and an
optional per-user start-at-login entry.

| Field | Actual source / meaning |
|---|---|
| CPU MHz | CAR PLLX or DFLL monitor, not Windows' nominal 1.00 GHz |
| GPU MHz | Fresh KMD GPCCLK sample through SwitchFan power ABI v2 |
| RAM MHz | Fresh runtime BPMP readback through the cached EMC IOCTL; dynamic clock, stale/error shown as unavailable |
| Fan PWM | Fan duty percentage; no tachometer/RPM feedback exists |
| SoC / board temperature | TMP451 remote / local sensor respectively |
| Battery % | Windows battery driver; charging/external power also indicated |
| Application FPS | Target process's kernel Present submission cadence |
| Graphics API | DXVK(DX9), DXVK(DX10), DXVK(DX11), Vulkan or Zink |

Missing/stale sensor values show `--`. Old SwitchFan 13 supports only fresh
registry CPU/PWM/temperature data and static firmware EMC data. Its boot GPU
reading is never advertised as live. ABI v2 GPU data must be no older than two
seconds and carry the new KMD live-counter handshake. GPU sampling happens at
idle boundaries; continuous activity can make the sample unavailable. TMP451
readings expire after six seconds. SoC temperature is not an independently
measured CPU/GPU junction temperature.

Whole-system watts are omitted: Battery14 exposes validated battery net current,
but no validated whole-system or AC-input power measurement. Screen refresh rate, PC vendor
sensor SDKs, NIC/disk/ping sampling, and the PC history/specification pages are
not exposed in this Switch build. No invented power or FPS value is substituted.

## FPS and API identification

Enable FPS capture from the tray menu/hotkey. This starts the native ARM64
self-contained .NET 8 helper through Windows UAC because ETW capture needs
the corresponding privileges. Rejecting UAC leaves FPS unavailable and does
not affect SoC telemetry. The helper does not contain LibreHardwareMonitor or
PawnIO and is not installed as a service.

Only `Microsoft-Windows-DxgKrnl` `Present_Info` (event 184) is counted, filtered
to the foreground process. DXGI/runtime/flip events are not counted a second
time. These are submitted Presents, not proof of displayed frames or panel
refresh. A path that does not emit this event stays unavailable. Actual NVK,
DXVK and Zink ETW coverage still needs Windows validation.

API identification follows the foreground application even with FPS disabled.
Opening Pacecar itself preserves the previous target. DXVK needs matching DLL
metadata or DXVK initialization/config strings; a filename such as `d3d11.dll`
alone is not evidence. The DX10 wrapper is classified ahead of its D3D11
implementation DLL. Mixed DXVK9/11 or Zink/DXVK evidence remains unconfirmed.
Vulkan and in-process Zink use the matching Mesa patch's per-process live
device/screen markers. Old unmarked Mesa libraries display `未确认` rather than
guessing from `vulkan-1.dll`. The marker proves an active backend object, not
that every draw in a mixed-backend process used that API. Standalone D3D and
OpenGL labels are intentionally absent.

## Installation

The reviewed installer places `Pacecar.Switch.exe` in
`%LOCALAPPDATA%\SwitchWOA\Pacecar` and registers an interactive per-user logon
task with60-second delay. Switch Power has its own10-second task. The current
device has no tool desktop shortcuts; `打开硬件工具.cmd` reopens both apps without
resetting modes or changing autostart. Its source and the shared update installer
are in [WindowsDrivers](https://github.com/ruje0504-2/Windows-for-Switch-Drivers/tree/hdh001-winarm-bounded-write/app).
The optional FPS helper remains separate and disabled for this acceptance round.
Settings live in `%APPDATA%\PacecarSwitch\config.json`.

## Build

The verified cross-build uses LLVM-MinGW 20261104, Windows SDK/WDK 10.0.26100.0
and nlohmann/json 3.11.3. Put its single header in `<deps>/nlohmann/json.hpp`
(SHA256 `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`).
The script reads the native Visual Studio project source lists rather than
building an unrelated replacement UI:

```sh
python3 scripts/build-switch.py --out <output> --deps <deps> \
  --toolchain <llvm-mingw-bin> --wdk-include <wdk-version-include> \
  --sdk-include <sdk-version-include>
dotnet publish helper/Pacecar.Sensors/Pacecar.SwitchCapture.csproj \
  -c Release -r win-arm64 -o <helper-output>
```

Copy the helper output alongside the native EXE, retaining its `arm64` folder.
The native EXE has no x64 runtime dependency. ARM64 Visual Studio project
configurations are also supplied; the verified build here is LLVM-MinGW.

## Verification and remaining acceptance

Passed: ARM64 native link and ARM64 helper publish; x64 controls for SoC value
expiry/reset, nominal-clock replacement, API classification and live-marker
reference lifetime; synthetic FPS interval/PID-reset controls; C# single-event
selector checks; headless unavailable-driver behavior. Source controls live in
`scripts/test-switch.cpp`, `scripts/test-fps-math.cpp` and `scripts/fps-controls`.
`--switch-snapshot` prints the current read-only JSON measurement and returns
0 for the native bridge, 3 when unavailable; this is not GPU acceptance.

The Wine GUI preview could not create its host D3D/WGL render target. No visual
pass is claimed. Main-thread review must check the strip at Windows scaling,
tray show/hide/recreation, device readback and FPS/API against known apps on
the Switch. No new driver or firmware was installed by this porting session.

References: [PresentMon descriptor at the pinned revision](https://github.com/GameTechDev/PresentMon/blob/f50c407ad9492051f4b834d77c31c49f38f0fef0/PresentData/ETW/Microsoft_Windows_DxgKrnl.h),
[TaskbarCreated contract](https://learn.microsoft.com/en-us/windows/win32/shell/taskbar#taskbar-creation-notification).
