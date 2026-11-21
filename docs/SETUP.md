# Setup

## 1. Verify the hardware

Choose the exact XIAO model before compiling. The shared sketch supports the
standard ESP32-S3, ESP32-C6 and nRF54LM20A Sense with the GC9A01A Round Display.
The helper identifies a board once compatible firmware is running; an empty
controller still needs its first correct USB upload. See [HARDWARE.md](HARDWARE.md)
for the support matrix and ESP32 Arduino profiles. Never use an S3 binary on a
C6 or Nordic chip. Initial upload is explicit. Later, Device Settings can install a different
device-purpose profile on a positively identified ESP32-S3 over USB.

The Nordic instructions below apply only to the original nRF54 board. Its
built-in CMSIS-DAP bridge means no external debugger is needed.

The tested Arduino platform is the community
[nrf54-arduino-core](https://github.com/lolren/nrf54-arduino-core), **1.0.21**.
Use its [Boards Manager installation instructions](https://github.com/lolren/nrf54-arduino-core#quick-install).
For that exact installed platform, the historical LM20A-compatible board label
and FQBN used by this project are:

```
nrf54l15clean:nrf54l15clean:xiao_nrf54lm20b:cpu_freq=128m
```

The physical board is LM20A; this is a naming quirk of the tested package.
Newer packages have changed board naming. Do not silently replace the FQBN or
upgrade the core while assuming that pins, timers and upload targets remain identical.
Check the installed board definitions and the chip before selecting an alias.

## 2. Enable hardware brightness control: KE switch 2

Before closing the display's case:

1. Unplug USB and disconnect any main rechargeable battery.
2. Find the tiny two-position **KE** switch between the microSD slot and the
   XIAO sockets. It is separate from the large display power switch.
3. Move **switch 2 toward ON**. With `ON KE` above the sliders and `1 2` below
   them, move slider **2 upward**. This connects D6 to the backlight driver.
4. Switch1 is optional: it enables battery-voltage sensing and is not required
   for brightness. Leave the large power switch ON for normal use.
5. Reconnect USB and test the menu brightness slider before replacing the case.

[Seeed's close-up](https://files.seeedstudio.com/wiki/round_display_for_xiao/90.jpg)
and [switch schematic](https://files.seeedstudio.com/wiki/round_display_for_xiao/91.png)
show the location and D6/backlight connection. A successful software percentage
or PWM acknowledgement does not prove the backlight is connected when KE2 is OFF.

Startup brightness is **60%** in both day/night profiles. The backlight stays
dark through initialization and fades up after the first frame. Existing manual
brightness choices and explicitly selected profiles can override the level;
cached host settings are not automatically replayed after restart/reconnection.
Scheduled screen-off and the menu's Screen Off option still blank the backlight.

## 3. Configure civil time and optional observer

Defaults are UTC, no observer and no branding. For another fixed offset, provide
your own numbers through the local setup tool. Both coordinates are optional;
without them the Moon uses a neutral orientation and does not claim a location.

```sh
python companion/configure.py --utc-offset-minutes YOUR_OFFSET \
  --location-label "YOUR_LOCATION" --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE --write-firmware-settings
```

This writes ignored `XiaoPlayground/UserSettings.local.h` and local helper
`settings.json`. Recompile/reflash after changing the firmware override and
restart the helper after changing helper settings. Keep their fixed offsets
matched. DST changes need another explicit update. Location and branding are
optional; do not commit the local header if it identifies the owner.

## 4. Select a device purpose and compile

Desktop is the existing `XiaoPlayground/` sketch. Gym Display is the separate
`device_profiles/gym/BikeDashboard/` sketch and currently supports ESP32-S3 only.
Both are modules of this repository; the helper changes its controls based on
the firmware that is actually running.

To build both installable S3 packages without touching hardware:

```sh
python tools/build_device_profiles.py --arduino-cli arduino-cli
```

This uses the `default_8MB` layout and writes ignored checksum manifests and
binaries under `device_profiles/build/`. Build these before staging/installing
the helper if you want its USB profile installer to include both packages. The
ESP32 Arduino toolchain supplies the USB installer executable. C6/Nordic Desktop
builds still follow their own hardware instructions; the purpose installer does
not support those controllers yet.

After an initial supported firmware upload, use Device → Device Settings… to
name the device and select Desktop or Gym Display. Applying another profile
requires a USB data cable, matching existing partitions and verified board identity.
It writes only the application, retaining device preferences and logs. See
[profile installation](../device_profiles/README.md) and [Gym setup](GYM.md).

### Initial Desktop upload

The required graphics libraries are included in `libraries/`, including the
project's GC9A01A driver modifications. Retain their notices when distributing.

```sh
arduino-cli compile \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54lm20b:cpu_freq=128m \
  --libraries ./libraries --build-path /tmp/xiao-display-build XiaoPlayground
```

Before uploading, stop the display helper and close Arduino Serial Monitor.
Discover the **new user's** serial port and CMSIS-DAP probe UID with the board
package's tools. If serial-to-probe discovery is ambiguous, supply the verified
UID; do not select an arbitrary connected board.

```sh
arduino-cli upload \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54lm20b:cpu_freq=128m \
  --port /dev/cu.usbmodemYOUR_DEVICE \
  --upload-property upload.uid=YOUR_PROBE_UID \
  --input-dir /tmp/xiao-display-build XiaoPlayground
```

The community package may require its host-tool setup and Python HID dependency;
follow that package's documentation for the chosen version. Upload success is
not a substitute for checking the actual screen and touch coordinates.

Until a valid RTC reading or first host clock sync, the compile-time clock is
only provisional. Configure USB and start the helper to synchronize real time;
accurate astronomy depends on that clock. An optional CR927 preserves RTC time
while main power is absent.

## 5. Build the menu app

Install Xcode Command Line Tools (`xcode-select --install`) and Python 3.10 or
newer. Create a dedicated virtual environment for this project. Installation
copies its dependencies into the helper's Application Support folder, so login
startup does not read a virtual environment in Documents. Keep the underlying
Python installation available; this is not a standalone Python distribution.

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python companion/configure.py --list-devices
python companion/configure.py --device /dev/cu.usbmodemYOUR_DEVICE
python companion/service/manage.py stage   # compile/sign only; no installation
python companion/service/manage.py install
```

The app is `~/Applications/XIAO Display Helper.app`, bundle ID
`org.xiaodisplay.helper`. Its per-user LaunchAgent is
`~/Library/LaunchAgents/org.xiaodisplay.helper.plist`. It uses no administrator
daemon. Stage/install output is generated locally and ignored by Git.

```sh
python companion/service/manage.py status
python companion/service/manage.py stop
python companion/service/manage.py start
python companion/service/manage.py uninstall
```

Installation/reinstallation preserves user settings, profiles and pairings.
Uninstall removes the application/service, leaving user data for explicit backup
or deletion by its owner. The installer manages one menu helper per Mac.
Manual backend test instances can use separate data directories and serial
devices; do not run two workers against one USB port.

## 6. Optional Bluetooth connection (experimental)

Start with USB and verify scene selection and brightness first. Then follow
[BLUETOOTH.md](BLUETOOTH.md) to discover and explicitly pair your own display.
No Bluetooth device is preselected, and a fresh helper does not scan until
requested. The nRF54 controller has Bluetooth LE and no Wi-Fi. ESP32 boards also have
Wi-Fi hardware; follow [WIFI.md](WIFI.md) after the matching firmware upload.

Bluetooth controls were exercised on the originating hardware; the final
package's permission/reconnection check remains pending. Keep USB available
while setting up. Changing rendering code or adding a new animation still
requires a USB firmware upload.

## 7. Connect optional sources

### Codex

Install/sign into the user's own Codex CLI, then enable the source. The adapter
opens an isolated stdio `codex app-server`, calls initialization and
`account/rateLimits/read`, and closes that process. RPC availability depends on
the CLI version/account. If it is unavailable, keep an honest unavailable state;
do not scrape credentials, invent remaining percentages or use API spend as a
substitute. Set `XIAO_CODEX_BIN` if the executable is outside normal discovery.
Configure that environment in the user's LaunchAgent if it must survive login.

### Bambu

With the helper stopped, use `pair_printer.py` on the user's trusted local
network. Enter the serial and hidden LAN code **in the local terminal**, not in
an LLM chat. Confirm the displayed certificate fingerprint belongs to that
printer before trusting it. Pairing does not establish independent certificate
identity automatically; the explicit pinning step is trust-on-first-use.

The implementation was developed for a P2S. Other models/firmware/network modes
need verification; this project claims read-only status monitoring, not universal
printer compatibility. No cloud account is required by the local MQTT adapter.

### Sun

Enable `sun` to download NASA He II 304 Å imagery and NOAA observations. These
requests are public and do not need user credentials. Missing/stale fields are
shown as unavailable/old. No captured readings are included in this repository.

## 8. Offline checks

Install development dependencies, then run:

```sh
python -m pip install -r requirements-dev.txt
python tools/check.py
```

Clang/C++17 is required for firmware parser/renderer harnesses. On macOS, Swift
profile checks and the off-screen native-menu benchmark also run. This validates
code and packet behavior without querying real accounts, contacting a printer,
flashing hardware or installing a login service.

## 9. Touch check

Check the top double-tap, centre tap and brightness swipe on your own panel.
If coordinates are wrong, stop the helper, open Serial Monitor at115200 baud
and send `c`. Hold each of the four gold dots until captured, lifting your
finger between dots. The calibration result is printed to serial; calibration
persists in ESP32 NVS, while the Nordic build keeps it in memory only. If a nonzero orientation is
needed, keep that result in your local `TouchCoordinates.orientation` default
in `XiaoPlayground/TouchCalibration.h`, then rebuild. Close Serial Monitor before
restarting the helper.

Touch and RTC share I²C. Automatic bus recovery is included, but intermittent
SDA-low stalls have occurred on the originating hardware. Check power and
connections and try a full power cycle if both stop responding; this is not a
claim that every panel fault is recoverable in software.

## 10. Local weather (opt-in)

Enable the source explicitly with `python companion/configure.py --enable weather`,
then restart the helper. Choose Scenes → Weather → Select Location and grant
macOS location permission. One location is saved locally; no continuous tracking
or example city is configured. Missing location shows unavailable data rather
than silently fetching a developer’s home forecast. Open-Meteo weather data is
attributed under CC BY 4.0 in the app. Centre taps toggle the 12/24-hour rain ring.
The weather location does not silently change the fixed civil clock offset;
configure that separately when moving between timezones.
