# BRMesh Cockpit Light – DCS-driven room lighting

An ESP32 controls BRMesh / FastCon Bluetooth lamps to match the **sun position in DCS World**. Mission time, position and altitude come from [DCS-BIOS](https://github.com/DCS-Skunkworks) over Wi-Fi. From these the ESP32 calculates the sun position and sets lamp colour and brightness: deep blue at night, orange at sunrise and sunset, white during the day. The light scenes are configured through a web interface served directly by the ESP32.

> **Note:** The lamps are addressed purely via BLE advertising. They never send anything back, so the ESP32 cannot tell whether a command was received.

## AI disclosure

This project was developed with substantial help from AI (Claude by Anthropic). This applies to the code as well as to this documentation. The code has been tested by the author on real hardware, but it may still contain mistakes. Please review it before relying on it.


## Credits and origin

The lamp control is **strongly influenced by [millskyle/BRMesh_homeassistant](https://github.com/millskyle/BRMesh_homeassistant)**. That project shows how to control BRMesh lamps with an ESP32 and explains how to obtain the network key. This project would not exist without that groundwork.

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
- BRMesh / FastCon compatible BLE lamps https://brmeshlighting.com/brmesh-flood-light-guide/
- An Android device with the BRMesh app, **only once**, to read out the network key
- A computer with `adb` (Android Debug Bridge)

**Software**

- ESP32 Arduino core with the libraries `BLEDevice`, `WebServer`, `ESPmDNS`, `ArduinoOTA` and `Preferences` (all included in the core)
- The [DCS-BIOS Arduino library, Wi-Fi fork by SCUBA82](https://github.com/SCUBA82/dcs-bios-arduino-library)
- DCS World with DCS-BIOS installed

The source is laid out as a PlatformIO / Arduino project . In `config.h` you can choose the USB version of the DCS-BIOS library (`DCSBIOS_DEFAULT`) instead of the Wi-Fi fork. In that case the Wi-Fi credentials for DCS-BIOS are not needed, but the web interface and OTA still require an existing Wi-Fi network.

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

Your personal settings live in **`config.h`**. 

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



** !!TODO!! Mission date:** For the time beeing, DCS-BIOS delivers only the time of day not a date. So the date is currently hard-coded in `main.cpp` and determines the sun position (season): 
There is an open [Feature Request](https://github.com/DCS-Skunkworks/dcs-bios/issues/1924) which is still pending.

```cpp
const int MISSION_YEAR  = 2026;
const int MISSION_MONTH = 7;
const int MISSION_DAY   = 1;
```

## Building and flashing

1. Open the project (PlatformIO or Arduino IDE) and install the DCS-BIOS library (Wi-Fi fork by SCUBA82).
2. Create and fill in `config.h` as described above.
3. Flash via USB the **first time**.
4. The serial monitor (115200 baud) shows the hostname, IP address and web interface address after boot. Later updates can be uploaded **over Wi-Fi (OTA)** using the name from `OTA_HOSTNAME`.

## Using the web interface

The web interface is reachable at `http://<hostname>.local/` 
<img width="782" height="890" alt="Setup" src="https://github.com/user-attachments/assets/5e488fb9-017c-4ad8-a258-e9232e72eab9" />
<img width="722" height="843" alt="LiveView" src="https://github.com/user-attachments/assets/174210a7-bc94-4d41-833e-cd483e89d8d9" />


