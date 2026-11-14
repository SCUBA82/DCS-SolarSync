# BRMesh Cockpit Light – DCS-driven room lighting

An ESP32 controls BRMesh / FastCon Bluetooth lamps to match the **sun position in DCS World**. Mission time, position and altitude come from [DCS-BIOS](https://github.com/DCS-Skunkworks) over Wi-Fi. From these the ESP32 calculates the sun position and sets lamp colour and brightness: deep blue at night, orange at sunrise and sunset, white during the day. The light scenes are configured through a web interface served directly by the ESP32.

> **Note:** The lamps are addressed purely via BLE advertising. They never send anything back, so the ESP32 cannot tell whether a command was received.

## AI disclosure

This project was developed with substantial help from AI (Claude by Anthropic). This applies to the code as well as to this documentation. The code has been tested by the author on real hardware, but it may still contain mistakes. Please review it before relying on it.

## Contents

- [Credits and origin](#credits-and-origin)
- [Features](#features)
- [Hardware and software](#hardware-and-software)
- [Getting the BRMesh key](#getting-the-brmesh-key)
- [Configuration before compiling](#configuration-before-compiling)
- [Building and flashing](#building-and-flashing)
- [Using the web interface](#using-the-web-interface)
- [Mission date from DCS](#mission-date-from-dcs-optional)
- [How the automatic mode works](#how-the-automatic-mode-works)
- [HTTP API](#http-api)
- [Project structure](#project-structure)
- [Known limitations](#known-limitations)
- [License](#license)

## Credits and origin

The lamp control (packet construction, encryption, whitening, CRC and sending via BLE advertising) is **strongly influenced by [millskyle/BRMesh_homeassistant](https://github.com/millskyle/BRMesh_homeassistant)**. That project shows how to control BRMesh lamps with an ESP32 and explains how to obtain the network key. This project would not exist without that groundwork.

The send queue with its state machine (advertising duration, gap, interval) follows the approach of [esphome-fastcon](https://github.com/dennispg/esphome-fastcon). Unlike millskyle's project, there is no Home Assistant or MQTT integration here. Control is done through DCS-BIOS and a built-in web interface.

More background on the BRMesh protocol and on extracting the key can be found in the Home Assistant forum thread [BRmesh app bluetooth lights](https://community.home-assistant.io/t/brmesh-app-bluetooth-lights/473486).

## Features

- Automatic light scenes by sun elevation, separate tables for **colour** and **white**, with linear interpolation between freely editable keyframes (up to 10 per table)
- Switching between colour and white at an adjustable elevation, with **hysteresis**
- **Altitude-aware horizon:** the horizon dips lower at height, so the sun "sets later" when you fly high
- Web interface with two tabs: **Setup** (manual control and scenes) and **Live** (DCS status, mission data, sun position, output values)
- Scenes are stored in flash (NVS) and survive a reboot
- Fallback to a fixed default scene when DCS stops delivering data
- Over-the-air updates (ArduinoOTA) and mDNS (`http://<hostname>.local/`)
- A simple serial console for testing

<!-- Space for a title image or GIF -->
<!-- ![Overview](docs/screenshots/overview.png) -->

## Hardware and software

**Hardware**

- ESP32 with Bluetooth LE (tested with classic ESP32 boards; other variants such as ESP32-S3 or -C3 have not been verified)
- BRMesh / FastCon compatible BLE lamps (up to 4, see `MAX_LAMPS` in `BRMesh.h`)
- An Android device with the BRMesh app, **only once**, to read out the network key
- A computer with `adb` (Android Debug Bridge)

**Software**

- ESP32 Arduino core with the libraries `BLEDevice`, `WebServer`, `ESPmDNS`, `ArduinoOTA` and `Preferences` (all included in the core)
- The [DCS-BIOS Arduino library, Wi-Fi fork by SCUBA82](https://github.com/SCUBA82/dcs-bios-arduino-library)
- DCS World with DCS-BIOS installed

The source is laid out as a PlatformIO / Arduino project (`main.cpp` with `setup()` and `loop()`). In `config.h` you can choose the USB version of the DCS-BIOS library (`DCSBIOS_DEFAULT`) instead of the Wi-Fi fork. In that case the Wi-Fi credentials for DCS-BIOS are not needed, but the web interface and OTA still require an existing Wi-Fi network.

## Getting the BRMesh key

All lamps in a BRMesh network share one **4-byte network key**, which the app generates during setup. Without it the lamps will not react to the ESP32. The steps below follow the approach described in [millskyle/BRMesh_homeassistant](https://github.com/millskyle/BRMesh_homeassistant):

1. **Set up your lamps in the BRMesh app.** You must be able to control them with the app.
2. **Enable USB debugging** on the Android device (developer options) and connect it to your computer via USB.
3. **Install `adb`** if you do not have it yet, and check that the device is detected:
   ```bash
   adb devices
   ```
4. **Follow the log** and switch a lamp or change its colour in the BRMesh app:
   ```bash
   adb logcat | grep jyq_helper
   ```
   On Windows without `grep`:
   ```
   adb logcat | findstr jyq_helper
   ```
5. The `getPayloadWithInnerRetry` lines end with `key:` followed by eight hex digits. Example:
   ```
   jyq_helper: getPayloadWithInnerRetry---> payload:7203ff00ff00000000000000, key: 31323334
   ```
6. These eight digits are four bytes. `31323334` becomes:
   ```cpp
   const uint8_t my_key[] = {0x31, 0x32, 0x33, 0x34};
   ```

**Tips**

- A key like `5e367bc4` shows up when you **add a new** lamp. That is the default key for unpaired lamps and **not** your network key. Your own key appears in the log lines *after* setup.
- **iPhone users:** the iOS app does not print the key to a log. You can, however, share your home to an Android device via QR code ("share home") and read the key there. The lamps do not need to be re-paired.
- If you re-pair the lamps in the app, the key changes. Read it out again in that case.
- The key is only 4 bytes long and offers no real security. Still, treat it as private and do not publish it.

## Configuration before compiling

Your personal settings live in **`config.h`**. This file must **not** be committed to the repository, because it contains your Wi-Fi password and your key. Create it from the template:

```bash
cp config_h.example config.h
```

Add `config.h` to your `.gitignore`:

```
config.h
```

Then edit `config.h`:

```cpp
// Which DCS-BIOS library do you use?
//#define DCSBIOS_DEFAULT      // USB
#define DCSBIOS_WIFI_FORK      // Wi-Fi (SCUBA82 fork)

// Hostname of the ESP32: used for OTA, Wi-Fi and mDNS (http://<hostname>.local/)
#define OTA_HOSTNAME "BRMeshCtrl"

// Wi-Fi credentials
#ifdef DCSBIOS_WIFI_FORK
  char ssid[]   = "MyWiFi";
  char passwd[] = "MyPassword";
#endif

// BRMesh key from the section above
const uint8_t my_key[] = {0x31, 0x32, 0x33, 0x34};
```

| Setting | Meaning |
|---|---|
| `OTA_HOSTNAME` | Used as Wi-Fi hostname, OTA name and mDNS name. The web interface is then reachable at `http://<hostname>.local/`. |
| `ssid`, `passwd` | Wi-Fi credentials. The same network is used for DCS-BIOS. |
| `my_key` | The 4-byte BRMesh key (see above). |

**Number of lamps and send repeats** are set in `main.cpp`:

```cpp
BRMesh mesh(my_key, 2, 2);   // key, number of lamps (1–4), repeats per command
```

Lamps are numbered from 1, in the order in which you added them in the BRMesh app.

**Mission date:** DCS-BIOS delivers only the time of day and the mission duration, not the date. The date matters because it determines the season and therefore the sun position. Until a date arrives from DCS, the fallback date in `main.cpp` is used:

```cpp
const int MISSION_YEAR  = 2026;
const int MISSION_MONTH = 7;
const int MISSION_DAY   = 1;
```

To take the real mission date from DCS instead, see [Mission date from DCS](#mission-date-from-dcs-optional).

## Building and flashing

1. Open the project (PlatformIO or Arduino IDE) and install the DCS-BIOS library (Wi-Fi fork by SCUBA82).
2. Create and fill in `config.h` as described above.
3. Flash via USB the **first time**.
4. The serial monitor (115200 baud) shows the hostname, IP address and web interface address after boot. Later updates can be uploaded **over Wi-Fi (OTA)** using the name from `OTA_HOSTNAME`.

## Using the web interface

The web interface is reachable at `http://<hostname>.local/` or at the printed IP address. It runs entirely on the ESP32 and needs no internet access. It has two tabs: **Setup** and **Live**. The link `…/#live` opens the Live tab directly.

### "Setup" tab

<!-- ![Setup tab](docs/screenshots/setup.png) -->

**Manual control**

- **Lamp:** selects a single lamp or "All".
- **Automatic mode by sun position:** as soon as you change a value manually, the automatic mode switches off (manual mode). Tick this box to switch it on again; the current scene is sent immediately.
- **Mode:**
  - *Colour* sends the white values as 0.
  - *White* sends the colour values as 0.
  - *Both* sends everything unchanged.
  Many lamps cannot show colour and white at the same time.
- **Colour wheel and RGB fields:** set the lamp colour. The preview swatch above is only an approximation.
- **Warm white and cold white:** on the tested lamps, white only reacts when **both** values are 255 and red, green and blue are 0. Dimming is done with the brightness.
- **Brightness:** 0 to 255.
- **Values for the scene table:** shows the current values as text, ready to copy (R, G, B, brightness).

Changes are sent throttled. If the lamps' send queue is full, the page automatically resends the latest value.

**Light scenes by sun elevation**

<!-- ![Scene tables](docs/screenshots/scenes.png) -->

- **Switch colour → white at:** sun elevation in degrees at which the output changes from the colour scene to the white scene (−20 to 60°).
- **Hysteresis:** prevents flicker around the switch elevation (0 to 5°).
- **Colour table:** each row holds sun elevation (°), R, G, B and brightness. Values are interpolated linearly in between.
- **White table:** each row holds sun elevation (°) and brightness. Warm and cold white are fixed at 255.
- 2 to 10 rows per table. Elevations must be unique and between −30 and 90°.

Row icons:

| Icon | Function |
|---|---|
| Flask | **Test:** show this row on the lamp (switches the automatic mode off) |
| Check mark | **Apply:** write the current colour and brightness from the wheel and sliders into this row |
| Trash can | **Remove:** delete the row |

**Typical workflow for tuning a scene:** set colour and brightness on the lamp with the wheel and sliders, copy them into the desired row with the check mark, and enter that row's sun elevation. **Save** sorts the rows by elevation, validates them and stores them in flash. **Defaults** resets all scenes. With automatic mode on, a changed scene is sent immediately.

### "Live" tab

<!-- ![Live tab](docs/screenshots/live.png) -->

The tab refreshes every second while it is open.

| Section | Content |
|---|---|
| **DCS-BIOS status** | *connected*, *waiting for data* or *no updates*. Also shows the operating mode (automatic, manual or default scene). |
| **Mission** | Date, time, altitude above sea level (ft and m), latitude and longitude from DCS-BIOS |
| **Calculated sun position** | Sun elevation, horizon dip from altitude, effective elevation used for the scene, and azimuth, plus a scale with a marker |
| **Output** | The scene values calculated from the sun position, and the values most recently sent to each lamp (R, G, B, brightness, W6, W7), each with a colour swatch |

The DCS-BIOS connection is detected through the mission time: if it does not change for more than 10 seconds, the tab shows "no updates". This happens both when the connection is lost and when DCS is paused. The swatches keep the colour recognisable even at low brightness; the numbers next to them are the actual values.

## Mission date from DCS (optional)

The mission date is not available in the DCS export environment that DCS-BIOS runs in. It can only be read from a DCS *hook*. The files in `dcs/` bridge the gap:

1. The hook `MissionDateHook.lua` reads the date from the mission when it loads and writes it to `Saved Games\DCS\Scripts\DCS-BIOS-Extensions\MissionDate.txt`.
2. `MissionDate.lua` adds three controls to the existing **CommonData** module of DCS-BIOS: `MISSION_YEAR`, `MISSION_MONTH` and `MISSION_DAY`. They read that file. Because they belong to CommonData, they are named `CommonData_MISSION_YEAR` etc. on the Arduino side, and no extra base address is needed.
3. The ESP32 listens to the three controls and uses the date instead of the fallback date.

**1. Install the DCS side.** Run the installer from the `dcs/` folder in PowerShell (use `-DcsSavedGamesPath` for the open beta or a different Saved Games folder):

```powershell
.\Install-MissionDate.ps1
```

It copies the hook and the extension, and adds one guarded call to `Scripts\DCS-BIOS\lib\modules\common_modules\CommonData.lua`, directly before the final `return`. A backup of that file is made first. The call is wrapped in `pcall`, so an error in the extension cannot break CommonData. `.\Install-MissionDate.ps1 -Uninstall` removes everything again.

> A DCS-BIOS update overwrites `CommonData.lua`. Run the installer again afterwards. It can be run repeatedly.

**2. Find the addresses.** Restart DCS and load a mission with an aircraft. Open the DCS-BIOS Reference Tool (`Scripts\DCS-BIOS\doc\control-reference.html`, Bort or BIOSBuddy) and look up **CommonData → MISSION_YEAR / MISSION_MONTH / MISSION_DAY**. Note address, mask and shift of each.

**3. Enter them in the firmware.** If your DCS-BIOS Arduino library has a CommonData header with the other `CommonData_*` entries (for example `CommonData_LAT_DEG`), add the three new entries there in the same format. Otherwise uncomment the three `#define` lines in `main.cpp` and fill them in the same format:

```cpp
#define CommonData_MISSION_YEAR  <address, mask, shift>
#define CommonData_MISSION_MONTH <address, mask, shift>
#define CommonData_MISSION_DAY   <address, mask, shift>
```

Without these entries the fallback date is used. After flashing, the serial monitor prints `Missionsdatum aus DCS: YYYY-MM-DD` when the date arrives.

If DCS-BIOS logs a buffer overflow for `CommonData`, the module has no free space for the three values. Check `DCS-BIOS.log` next to `dcs.log` in that case.

The hook only sees the mission of the local DCS instance. This works for single player and for hosting. It has not been verified as a multiplayer client. Mission rollover past midnight is handled by the ESP32, which adds days to the start date.

## How the automatic mode works

1. DCS-BIOS provides mission start time, elapsed mission seconds, latitude, longitude and altitude.
2. From date, time and position the ESP32 calculates sun elevation and azimuth (using NOAA formulas).
3. The **horizon dip** derived from the flight altitude is added to the sun elevation. The result is the effective elevation.
4. The effective elevation selects colour and brightness from the scene tables. Below the switch elevation the colour table applies, above it the white table (with hysteresis).
5. The result is only sent when it differs from the last value. All lamps receive the same value.

**Fallback:** if the mission time does not change for more than 10 seconds, the ESP32 sets all lamps to a default scene (white, brightness 180). When data returns, the automatic mode takes over again. In manual mode neither the automatic mode nor the fallback runs; the Live tab keeps calculating anyway.

## HTTP API

The web interface uses these endpoints. You can also use them for your own scripts:

| Method | Path | Function |
|---|---|---|
| GET | `/` | Web interface |
| GET | `/api/state?lamp=N` | Stored values of a lamp (`0` and `1` both show lamp 1) |
| GET | `/api/set?lamp=N&r=&g=&b=&br=&w6=&w7=` | Set values (`lamp=0` = all); switches to manual mode. Replies with HTTP 429 when the queue is full. |
| GET | `/api/auto?on=1` | Switch automatic mode on, or off with `on=0` |
| GET | `/api/scene` | Scene configuration as JSON |
| POST | `/api/scene` | Save scenes (`switch`, `hyst`, `rgb`, `white`) |
| POST | `/api/scene/reset` | Restore and save the default scenes |
| GET | `/api/live` | Live data for the Live tab |

## Project structure

| File | Content |
|---|---|
| `main.cpp` | Entry point, DCS-BIOS binding, sun position calculation, automatic mode, serial console, OTA |
| `BRMesh.h` / `BRMesh.cpp` | BRMesh / FastCon protocol and send queue (BLE advertising) |
| `SceneConfig.h` / `SceneConfig.cpp` | Light scenes, interpolation, hysteresis, storage in flash |
| `WebControl.h` / `WebControl.cpp` | Web server and HTTP API |
| `WebUI.h` | Web interface (HTML, CSS and JavaScript in one piece, no external files) |
| `config_h.example` | Template for `config.h` |
| `dcs/Hooks/MissionDateHook.lua` | DCS hook that writes the mission date to a file |
| `dcs/MissionDate.lua` | DCS-BIOS extension: adds `MISSION_YEAR/MONTH/DAY` to CommonData |
| `dcs/Install-MissionDate.ps1` | Installer (and uninstaller) for the DCS side |

## Known limitations

- The lamps do not acknowledge commands. A lost command goes unnoticed.
- In automatic mode all lamps receive the same value.
- The mission date needs the optional hook setup. Without it, a fixed fallback date from `main.cpp` is used.
- The white channels (bytes 6 and 7) have only been verified with the tested lamps. Other lamps may behave differently.
- The ESP32 web server handles one request at a time. Several browser tabs open at once may slow it down.

## License

Not yet specified. Please add a license before publishing, and check the licenses of the projects used and referenced ([BRMesh_homeassistant](https://github.com/millskyle/BRMesh_homeassistant), [esphome-fastcon](https://github.com/dennispg/esphome-fastcon), [dcs-bios-arduino-library](https://github.com/DCS-Skunkworks/dcs-bios-arduino-library)).
