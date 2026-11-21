# XIAO Round Display

A modular display platform for **Seeed Studio XIAO ESP32-S3, ESP32-C6 and
nRF54LM20A Sense** controllers and
**240 × 240 Seeed Round Display**, with a native **Swift/AppKit macOS menu bar app**.

![Animated preview of XIAO Round Display clocks, astronomy, Game of Life and status scenes](docs/media/xiao-display-showcase.gif)

*26-second firmware-renderer preview in a stylised bezel. Usage and printer data
are illustrative; animation time is accelerated. [How this GIF is made](docs/media/README.md).*

This is a blank, user-configurable template. It contains the working scene and
control code, with **no configured device, accounts, printer, location, saved
profiles, live readings or enabled network feeds**. Integrations are opt-in.

Choose a **Desktop** or **Gym Display** device profile. The native macOS helper
remembers multiple displays, gives each a local name, and updates them independently.
Selecting a device in the menu changes which controls you see.

**Device → Device Settings…** provides Device Name and a Profile dropdown.
Changing purpose is an explicit USB firmware installation with identity checks,
matching partition layout, and a recovery copy of the previous application.
In-helper purpose switching currently supports ESP32-S3; Desktop source support
for ESP32-C6 and nRF54LM20A Sense is retained. No firmware OTA updater is included.
See [device modules and installation](device_profiles/README.md),
[hardware support](docs/HARDWARE.md), [Wi-Fi](docs/WIFI.md), and
[Bluetooth](docs/BLUETOOTH.md).

## Gym Display

- Wired contact-switch bike sensor on D2/GPIO3 and GND, with configurable pulses per turn.
- Cadence and estimated watts calculated on the ESP32, independent of the laptop.
- Manual resistance levels 1–8, smooth green pulse background and brightness gesture.
- Tap the large number to switch watts/RPM; customise the title in Device Settings.
- Standard Bluetooth cycling cadence and power services for Apple Watch workouts.
- Display-only Demo Mode, persistent pedal logs and saved on-device settings.

**Power is an estimate**, using an adjustable-in-source heuristic rather than a
calibrated torque measurement. This is standard Bluetooth sensor support, not
certified Apple GymKit. Check [Gym wiring, model and validation](docs/GYM.md)
before connecting an existing bike sensor. Never assume a cut cable's colours
identify its contacts.

## Scenes

- Ten analogue clock faces, inspired by landmark clocks, with day/night palettes.
- Schematic analemma, Moon phase, current planetary positions and a Sun/NOAA card.
- Three nested star-system animations, including a clearly labelled historical
  mass-transfer illustration for HR 6819, available separately from its present view.
- Conway's Game of Life on a grid extending beyond the circular viewport.
- Empty Codex-usage and Bambu-printer scenes, ready for the new user's own feeds.
- Local weather with a 12/24-hour rain ring, larger weather icon, temperature and UV.

The menu bar app provides scene selection, a persistent carousel checklist with
All/None/mixed states, carousel/hold/interval controls, named Profiles, appearance,
brightness and screen-power controls. Profiles contain settings, not credentials.
All scenes remain in source when excluded from the carousel. Retired modules
are preserved separately in `optional_assets/`.

## Hardware

| Part | Quantity |
|---|---:|
| One supported XIAO controller (S3, C6 or nRF54LM20A Sense), with headers | 1 |
| Seeed Round Display for XIAO, 240 × 240 GC9A01A | 1 |
| USB data cable to the Mac | 1 |
| Optional CR927 clock-backup cell | 1 |

The XIAO's USB socket faces outward. The display connects through its XIAO
sockets. This firmware uses the existing Seeed pinout: D1 LCD CS, D3 LCD DC,
D8 clock, D10 MOSI, D4/D5 touch/RTC I²C, D7 touch interrupt and D6 backlight.
The nRF54 board has no Wi-Fi; ESP32 variants do. The Mac fetches the live data
and sends it over the selected USB/Bluetooth/Wi-Fi connection. Wi-Fi support
does not make Codex/printer/weather feeds independent of the Mac helper.

## Start here

Read [SETUP.md](docs/SETUP.md) for compiling firmware and building/installing the
Mac app. Give [LLM_SETUP.md](docs/LLM_SETUP.md) to an LLM helping configure a new
user's installation. [AGENTS.md](AGENTS.md) describes the project boundaries.

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python companion/configure.py --list-devices
# Select your own device from that list; no port is preselected.
python companion/configure.py --device /dev/cu.usbmodemYOUR_DEVICE
python companion/service/manage.py install
```

Compile and flash the firmware using the separate instructions before expecting
the scene controls to work. Installing the helper does not flash a board. Applying a different device profile
in Device Settings is a separate, explicit USB installation.
The app builds locally with `swiftc`, uses one worker per remembered device, starts at login
through a per-user LaunchAgent and has no Dock icon. There is no prebuilt app
containing someone else's Python path or system registration.

## Empty by default

The helper's own application-data directory is
`~/Library/Application Support/XIAO Display Helper/`. It is distinct from other
installations. `XIAO_DISPLAY_DATA_DIR` can override it for tests or a separate
instance. Copying the example configuration is optional; the defaults are already
empty. Select only the sources the owner actually wants:

```sh
python companion/configure.py --enable sun
python companion/configure.py --enable codex
# Pair a printer locally before enabling its feed:
python companion/pair_printer.py --host YOUR_PRINTER_IP
python companion/configure.py --enable bambu
```

Restart the helper after configuration changes. Codex uses the new user's
already-signed-in local CLI; it does not require an API key in this repository.
Printer pairing prompts for the new user's serial and hidden LAN access code,
requires an explicit certificate-trust decision and stores them locally with
restricted permissions. It never imports another application's credentials.
The printer connection subscribes to status reports and issues no print-control
commands. See [PRIVACY.md](docs/PRIVACY.md).

## Controls and scheduling

- Double-tap the glass near 12 o'clock to advance through included scenes.
- Tap the centre of a clock to change watch face; scene-specific taps reveal information.
- Right-edge swipes adjust brightness. **Turn KE switch2 ON** to connect D6 to
  the backlight; with `ON KE` above `1 2`, slide2 upward. See the illustrated
  [hardware brightness setup](docs/SETUP.md#2-enable-hardware-brightness-control-ke-switch-2).
- The carousel is configurable from the menu. Selecting no scenes pauses it on
  the current scene. Manual scene selection remains available.
- Night palette: 17:00–07:30; USB screen sleep: 01:00–07:00, in configured civil time.
  A clock advances to the next watch face at half past the hour while visible.
- Startup brightness is **60%**, after the first frame is ready.
  Old saved brightness is never replayed automatically on boot/reconnect.

UTC is the neutral time default. A single local configuration command can write
matching fixed-offset firmware settings for another location. Daylight-saving
transitions require updating the fixed offset; automatic DST is not implemented.

## Validation and dependencies

See [VALIDATION.md](docs/VALIDATION.md) for build, mock and originating-hardware
evidence, including the remaining C6 Wi-Fi and RTC limitations. This public template's blank
configuration and build are checked separately; no new user's printer/account
connection is claimed to have been tested. Run the offline checks described in
[SETUP.md](docs/SETUP.md) before changing code.

Third-party notices remain with their code and fonts; see
[THIRD_PARTY.md](THIRD_PARTY.md). Public publication does not change those terms.
No project-wide license for the original application code has been selected yet.
