# Working on this template

This repository is the public, unconfigured variant. Keep it free of personal
identity, local computer paths, USB/probe IDs, printer addresses/serials/codes,
account data, profiles, logs and screenshots from a real user installation.
Do not import app authentication files. Configuration examples must stay blank.
Required third-party author/license notices and landmark names are preserved.

Read docs/LLM_SETUP.md when configuring a new user's installation. Keep optional
feeds disabled until that owner chooses them, and enter credentials through the
local interactive tool instead of chat or command arguments. Runtime state lives
in application support, not tracked source. Local firmware overrides are ignored.

Preserve all scenes/assets when a menu preference changes. Maintain native-menu
caching and the persistent checklist; avoid rebuilding menus or doing network
work on the UI thread. One independent Python worker owns each remembered device and its selected
transport (USB, explicitly paired Bluetooth, or USB-provisioned certificate-pinned
Wi-Fi). Menu selection changes the view; it must not stop other devices.
Device-purpose modules are declared in device_profiles/catalog.json. Changing
purpose is an explicit USB application-only installation, never a menu-selection
side effect. Preserve device-scoped settings, recovery backups and positive
hardware/partition checks. New integrations use the
allowlisted source registry and bounded CRC packets. Do not issue printer-control
commands from the status monitor. Startup brightness is60%, with first-frame
blanking; preserve true0% and screen-off support, and do not replay cached levels.

Use tools/check.py for offline checks, then compile the intended MCU target and
stage the native app where relevant. Never silently test by querying someone's
account, altering a printer or flashing a connected board. Report build, mock
and physical validation separately. Inspect the complete staged Git content
before publishing. Do not commit generated local configuration or app bundles.

Bluetooth remains experimental. Preserve USB fallback, encrypted/authenticated
controls and bounded discovery. Do not claim final-package reconnection or
full wireless image delivery was verified without new physical evidence.

Hardware identification runs in the worker, never in a menu callback. Unknown
firmware stays unidentified; do not infer ESP32-S3/C6 from their shared USB IDs.
Preserve endpoint/serial scoping so auto mode cannot select another display’s
old wireless pairing. Wi-Fi credentials are entered only through the native
secure field over USB; keep generated device keys, controller tokens, pins and
location/network diagnostic files out of Git. No firmware OTA service exists.
See docs/HARDWARE.md and docs/WIFI.md for build and verification limits.
