# Privacy and local state

The public source contains no user configuration or recorded personal readings.
Default configuration selects no USB device and enables no data sources. Merely
starting a fresh helper does not query Codex, contact a printer or download images.

Local state is in `~/Library/Application Support/XIAO Display Helper/`:

| Path | Meaning |
|---|---|
| settings.json | Explicit device, source and civil-time/location settings |
| display-preferences.json | Scene inclusion, carousel, appearance, power and brightness requests |
| display-profiles.json | Named settings snapshots |
| device-catalog.json, device-connections.json, device-names.json | Remembered boards, per-device connection choices and local aliases |
| devices/ | Per-device preferences, telemetry, operational logs and private firmware recovery copies |
| gym-actions.json, gym-status.json | Targeted Gym settings requests and acknowledged values |
| display-connection.json | Locally chosen transport and macOS Bluetooth device UUID |
| bluetooth-status.json | Local discovery/connection phase |
| python-runtime/ | Installed Python dependencies; no source credentials are copied |
| private/bambu.json | User's printer address, serial and LAN access code |
| private/bambu.pem | Pinned printer certificate |
| assets/codex_usage, assets/bambu, assets/sun | Fetched data caches |
| logs/, helper-status.json, control-status.json | Local operational diagnostics |

These paths, local firmware overrides, build products and app bundles are
gitignored. Do not defeat the ignore rules with `git add -f`. Profiles can contain
user-chosen names and preferences, so treat them as local too.

Codex credentials remain with the user's normal CLI installation. The helper
does not open authentication files or copy tokens to firmware. It does not run
a model request to get quota. Quota percentages, reset timestamps and print
titles sent to the screen are personal runtime data, not repository examples.

Bambu credentials are entered interactively and stored with0600 permissions in
a0700 private directory. Nothing imports Bambu Studio's saved credentials.
Certificate inspection during pairing is unauthenticated and needs the owner's
explicit trust decision. Subsequent MQTT monitoring pins that certificate with
TLS verification enabled and subscribes only to the printer's status topic.

The repository includes third-party authors' required copyright/license notices
and landmark names/cities because those identify the code or subject of a clock,
not the current user's identity or location. No owner biography or personal
connection to those landmarks is retained.

If a secret is committed accidentally, removing it in a later commit is not
enough: revoke it and address repository history before wider distribution.

Bluetooth scanning happens only after an explicit menu request or when a saved
Bluetooth connection is selected. Connections target the locally chosen UUID;
no other user's device is built in. The firmware requires an authenticated,
encrypted link for control/data writes and can retain bonds on the board.
Gym Bluetooth is a separate read/notify-only cycling sensor interface using
normal encrypted bonding. It sends real crank events and estimated watts to a
Watch; no writable Mac control service is exposed. Desktop control/data links
retain their authenticated-write requirements.

Display packets contain the same scene/status data as USB, never printer LAN
codes or Codex authentication tokens. Bluetooth does not fetch internet data
independently and provides no firmware-update service.

Wi-Fi setup generates a certificate/key for each display and an independent
random controller token. The helper pins that exact certificate, validates its
hostname and sends the controller token inside TLS. No network credential is
compiled into the project. Wi-Fi passwords use a native secure input field and
stdin, never process arguments, chat, source, logs or saved Mac settings. Ordinary
device NVS holds the Wi-Fi credentials/private key; this does not imply physical
flash extraction protection. Pairing material under local private/wifi and
configured NVS/flash dumps must never be published. Hardware, Wi-Fi-setup and
weather-location state are ignored by Git and covered by the privacy audit.
