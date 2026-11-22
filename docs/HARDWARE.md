# Controllers and automatic settings

| Controller | USB | Bluetooth LE | Wi-Fi | Validation |
|---|---|---|---|---|
| XIAO ESP32-S3 (standard, 8 MB flash/8 MB PSRAM) | Yes | Implemented | TLS LAN control/data; same-profile OTA | S3 originating hardware: Wi-Fi, all ten scenes, full Sun CRC, touch and ~30 FPS clock verified |
| XIAO ESP32-C6 (4 MB flash, no PSRAM) | Yes | Yes | Implemented; experimental | USB/BLE controls previously checked; Wi-Fi/TLS memory/performance needs mounted C6 QC |
| XIAO nRF54LM20A Sense | Yes | Yes | No radio | Original scene/USB/BLE checks; final public Bluetooth reconnection still needs owner verification |

Plus/Sense variants are not automatically claimed equivalent to the qualified
standard S3 board. The helper reports the MCU family; it cannot infer a Plus
variant or flash capacity from a generic firmware tag. Intermittent shared
I²C/RTC idle diagnostic failures remain under investigation on the originating
hardware; successful taps do not establish independently verified RTC health.
The Mac synchronizes running time when connected.

The menu reports the model identified from the selected display’s FIRMWARE
response. Older supported firmware is recognized; an unknown tag stays unknown.
S3 and C6 share native USB identifiers, so those identifiers are never used to
guess their MCU. The probe runs in the single background worker on connection
and is invalidated by a new target or observed reboot. Menu navigation performs
no discovery or network I/O. Identifying firmware does not authorize flashing.

- Nordic: USB/Bluetooth options; no misleading Wi-Fi setup item.
- ESP32 with older non-Wi-Fi firmware: “Wi-Fi needs a firmware update”.
- ESP32 with Wi-Fi firmware: Wi-Fi plus one-time USB setup. Select USB to enable
  that setup action, then use Wi-Fi after provisioning.
- Legacy public r26: basic controls use v1 packets and the nine-scene mask;
  Weather/individual face filters are marked as requiring an update. Saved newer
  preferences are retained, and unsupported Weather packets are not sent.
- Unknown/offline: identify before offering MCU-specific setup. General scene
  and profile preferences remain saved and are not overwritten by detection.

Automatic connection prefers the selected USB serial, then its saved Wi-Fi,
then Bluetooth explicitly associated with that selected device. It does not
fall back to a previous controller’s Bluetooth UUID. Selecting a different USB
board clears ambiguous Bluetooth associations, and restores a Wi-Fi record only
when its full saved serial matches. OS-level bonds and source/profile settings
are not erased; explicitly choose Bluetooth again if needed.

## ESP32 build profiles

Use Arduino ESP32 core **3.3.12** and the bundled graphics libraries. From the
repo root, select exactly one FQBN below and compile to a separate build directory:

```sh
# Standard S3: OPI PSRAM is required, native hardware USB CDC, 8 MB flash.
arduino-cli compile --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default,PartitionScheme=default_8MB,LoopCore=1,EventsCore=0' --libraries ./libraries --build-path /tmp/xiao-s3-build XiaoPlayground

# C6: 4 MB flash with a large app / LittleFS partition layout.
arduino-cli compile --fqbn 'esp32:esp32:XIAO_ESP32C6:PartitionScheme=huge_app,CDCOnBoot=cdc' --libraries ./libraries --build-path /tmp/xiao-c6-build XiaoPlayground
```

After the owner chooses/verifies the intended USB device, stop the helper and
upload with the same FQBN and build directory, using `arduino-cli upload
--fqbn YOUR_MATCHING_FQBN --input-dir YOUR_BUILD_DIR --port YOUR_VERIFIED_PORT
XiaoPlayground`. Never auto-select one of several connected devices. Standard
nRF54 build/upload instructions are in SETUP.md.

Pins by XIAO alias: D1 CS, D3 DC, D4/D5 touch+RTC I²C, D6 backlight, D7 touch
IRQ, D8 SCK, D9 MISO, D10 MOSI. BOOT is S3 GPIO0 or C6 GPIO9. The S3 has four
RGB565 canvases in PSRAM; C6 uses internal canvases and LittleFS for the Sun
cache. ESP32 touch calibration is saved in NVS. Attach the S3 external antenna
for wireless use, align both header rows, USB-C outward, and keep the display
power switch ON. Turn KE switch2 ON for real backlight dimming.

ESP32 USB-host detection cannot reliably distinguish a wall charger from battery
power. Unknown power stays awake; do not promise charger-only sleep detection.
The Nordic PMIC power-detection path remains separate. First installation and purpose changes require USB. OTA-capable S3 builds can
install a newly compiled, locally bundled same-profile build over paired TLS
Wi-Fi; C6/Nordic firmware updates remain USB. See [OTA.md](../device_profiles/OTA.md).
