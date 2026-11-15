# Public-template validation

Checked 15 November2026, using a clean Python virtual environment and blank
temporary application data. These checks did not query a real Codex account,
connect to a printer, install a login service or flash hardware.

- Firmware compiled with nrf54-arduino-core1.0.21 and the documented LM20A board
  alias:577,348 program bytes,21,936 static RAM bytes (r25).
- The Swift/AppKit menu application built and was locally signed successfully
  through `manage.py stage`; its executable reported version2.1.4.
- Offline checks passed for clock/RTC recovery, day/night and power schedules,
  empty/all scene masks, CRC parsing, carousel hold/resume, Codex and printer
  packet/render guards, source-worker isolation and Sun-transfer interleaving.
- Native profile persistence and All/None/mixed-state menu checks passed.
  Cached-menu opening work was measured off-screen; this is not a physical UI
  latency measurement or a claim of GUI automation coverage.
- Blank configuration creates no feeds or device selection. Tests verify that
  default source construction does not initialize the account/printer fetchers,
  that sources require explicit allowlisted opt-in, and that location/time
  overrides remain separate from tracked defaults.
- The publishable file list was audited for credential artifacts, runtime
  snapshots, local home paths, private network addresses and common token forms.
  A separate local comparison found no values from the originating installation's
  printer pairing or personal identifiers. No actual pairing values are included
  in this validation record or in the repository.

This verifies a buildable, unconfigured template. A new installation still needs
its owner's hardware, time/observer choices, sign-in/pairing and physical checks.

The r25 startup update sets both brightness defaults to60%, gates the backlight
until the first complete frame, and preserves Screen Off/scheduled-sleep0%.
Tests verify that cached0%/100% requests cannot replace the boot level without
a fresh explicit action. The setup guide identifies KE switch2 and its ON
direction; switch1 is optional battery sensing.
