# Device purpose modules — local review build

One helper, one device catalog, independent workers for every remembered XIAO.
The catalog in `catalog.json` is the list of purposes shown by Device Settings.
This differs from saved Desktop scene presets, which remain per-device settings.

| Purpose | Source | Function |
| --- | --- | --- |
| Desktop | `../XiaoPlayground/` | All existing clocks, astronomy, scenes, and optional feeds |
| Gym Display | `gym/BikeDashboard/` | Real pedal cadence, estimated watts, display animations, Watch CSC/CPS |

The catalog links both firmware modules in this repository. Generated builds
and personal installation state are excluded from Git.

## Device Settings

Choose a device in the helper, then **Device Settings…**. Device Name is a local
alias that survives disconnection. Profile is Desktop or Gym Display. Saving an
unchanged profile only saves settings; it does not flash firmware. Editing the
Gym Display Title sends an acknowledged USB/TLS command and saves it on the board.
Titles currently accept 1–24 printable ASCII characters (the display's font).
Tap the large Gym number to cycle Watts, RPM and Total Km (or Mi); this preference is saved on-board. Ride totals reset on every startup.
Demo remains display-only and never generates Watch workout data.

Changing purpose installs a different application over USB. The Mac checks the
live firmware identity, USB serial, ROM MAC, package checksums, and exact partition
layout. It backs up the old application before an application-only write. It
never writes NVS, bootloader, partition table, OTA metadata or pedal logs. Existing
per-device Desktop preferences and Gym NVS settings remain separate. A new Gym
filesystem is initialized only if its entire partition is erased; existing or
unrecognized data is never formatted automatically.

The helper only reports success after the display identifies its new purpose.
It then reprobes that device and changes its menus/feed workers. Other devices
continue updating. A failed preflight restores normal boot where possible and
leaves the old firmware in place; a failed write retains a private recovery image.
Disconnect the bike sensor before repurposing a board for another wiring setup.
Same-profile S3 firmware updates can use paired Wi-Fi after one USB bootstrap;
see [OTA.md](OTA.md). Purpose changes still require USB.

## Build and package

From the project root, with the Arduino ESP32 toolchain/libraries installed:

```sh
python tools/build_device_profiles.py
.venv/bin/python -B companion/service/manage.py install
```

The build tool compiles only; it never connects to or flashes hardware. Generated
applications, partition files and checksum manifests live under ignored `build/`.
The helper package includes those locally built payloads and the ESP32 USB tool. The documented macOS installer packages the tool from
the locally installed Arduino ESP32 core.
No generated binaries or configured runtime state should be committed publicly.

This iteration supports in-helper purpose installation on positively identified
ESP32-S3 controllers with the existing default_8MB partition layout. Desktop
source support for C6 and nRF54 is unchanged. Gym is not qualified on those boards,
and their in-helper installer is not enabled. Initial unflashed-board setup still
requires choosing the board/toolchain and flashing the selected module over USB;
shared USB IDs alone are not hardware identification.

## Validation

Both S3 modules compile. Offline tests cover native Device Settings widgets,
per-device aliases, switching isolation, checksum/MAC/partition refusals, recovery
and application-only writes, Gym title encoding/acknowledgement, toggle persistence,
and preserving existing filesystem data. Helper 2.14.1 and Gym r18 were tested on the originating installation. The originating installation confirmed the metric tap toggle physically. Two live workers remain
independent. Physical Desktop→Gym→Desktop installation is intentionally awaiting
a user-initiated test; no board has been repurposed as part of these automated checks.
