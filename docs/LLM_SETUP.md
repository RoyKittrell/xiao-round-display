# Setup handoff for an LLM

The repository is a template, not a backup of a configured installation. Read
README.md, docs/SETUP.md, docs/PRIVACY.md and AGENTS.md before making changes.

## Ask the owner only for missing non-secret choices

1. Confirm exact microcontroller/display model and operating system; read docs/HARDWARE.md.
   The helper can identify a running firmware, but shared USB IDs alone cannot
   distinguish S3/C6 or identify an unflashed board. Never silently flash.
2. Discover serial devices locally; let the owner identify the intended one.
   Discover/verify the probe UID before any authorized upload.
3. Ask which sources they want: none, Sun/NOAA, their Codex usage, their Bambu
   status, or weather. Every source is disabled by default. Do not query accounts just to
   see whether they exist.
4. Ask for civil UTC offset and whether they want an observer location. They may
   leave UTC/no observer. Explain that fixed offsets do not implement DST.
5. Ask which scenes to include and whether to use carousel/hold. Use the menu
   and Profiles; do not delete other scenes to implement a preference.

## Configure with the provided tools

- Create a venv and install requirements; use configure.py to list/select USB.
- `configure.py --enable SOURCE` opts in to a source. Restart after changes.
- Coordinate optional location/offset through `--write-firmware-settings`, which
  creates a gitignored local override matching the helper's fixed-offset setting.
- For Codex, have the owner sign into their own CLI through its normal local UI.
  Reuse the read-only quota adapter. Never request passwords, tokens, session
  files or account API keys in chat. Treat RPC failures as unavailable.
- For Bambu, have the owner run the interactive pairing command themselves.
  Do not put the LAN access code in a command argument, chat, environment sample,
  screenshot or Git commit. The code is collected with getpass, stored only
  under local `private/`, and never sent to the display.
- Verify a printer certificate before trusting the first pin; do not silently
  disable TLS verification in the authenticated monitor.
- Enable Sun if desired; it uses public endpoints and needs no credentials.
- Build locally with manage.py stage before an owner-requested installation.
- Bluetooth is optional and experimental. Start with USB; after firmware is
  loaded, use the menu to discover and explicitly choose the owner's display.
  Never copy a pairing UUID or bond from another installation. Pairing uses the
  code shown on the display and macOS's own prompt; do not request it in chat.
- The installed helper copies its project environment into Application Support;
  retain the underlying Python installation. Local ad-hoc signing can cause
  macOS to ask for Bluetooth access again after rebuilding. Check the named
  helper in Privacy & Security → Bluetooth and retry discovery; keep USB usable.
- Scene selection, inclusion and data updates can use Bluetooth. Creating a
  renderer/animation or adding compiled assets still requires USB firmware upload.
- Distinguish offline coverage, originating-hardware observations and a new
  owner's actual checks. Final-package Bluetooth reconnect/permission testing
  and complete wireless Sun-image delivery are not yet verified.

## Keep the working shell honest

Codex/printer cards start with no data. Do not publish captured readings, print
filenames, profiles, certificates, screenshots, app bundles or ELF/HEX binaries
from a real installation. The Sun placeholder also contains no captured data.
Synthetic fixtures belong in tests and must be labelled as such.

Keep renderers separate from fetch adapters. New sources use the allowlisted
factory registry, bounded packet parsers and the single display dispatcher; no UI
opens a competing serial connection. Preserve CRC checking, stale timestamps,
unknown-value indicators, startup60% brightness after first-frame blanking and
idempotent controls. Enable KE switch2 physically for D6/backlight control; a
software acknowledgement alone cannot verify physical dimming. Keep true0%,
screen-off behavior and the no-cached-brightness-replay rule.

Before sharing changes: run offline tests and the privacy audit, inspect
`git diff --cached`, and verify local configuration/cache files remain ignored.
Only report hardware/account verification that was actually performed for this
owner. Do not claim the public template is already paired or signed in.

## ESP32 Wi-Fi and hardware-aware menus

Follow docs/HARDWARE.md for the correct build (S3 requires OPI PSRAM) and
WIFI.md for one-time USB provisioning. The helper identifies the selected
firmware in its worker and hides Wi-Fi on Nordic, or labels an older ESP32
build as needing an update. It preserves user scene/profile/appearance choices.
Unknown firmware is shown honestly. Choosing another USB serial clears ambiguous
wireless associations; automatic mode prefers that selected USB, then its saved
Wi-Fi, then a Bluetooth pair bound to the same selected device. Do not import
another owner’s pairing files or infer that a similarly named nearby display is
safe to control. Validate S3, C6 and Nordic compilation separately. C6 Wi-Fi/TLS
runtime memory is not yet hardware-qualified; do not claim otherwise.
