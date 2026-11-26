# Validation scope

The public Desktop r36 (S3) / r35 (C6/Nordic) source is a blank template. It has not been installed with a new
owner’s printer/account/network settings. No configured app bundle or captured
firmware image is published.

## Helper 2.15.0, screen orientation and device-purpose modules

- Both public ESP32-S3 purpose modules compile with Arduino ESP32 core 3.3.12;
  the native helper stages and signs locally. No configured binary is committed.
- Offline checks exercise independent device workers, name/profile controls,
  stale-selection isolation, application-only installation and recovery backups,
  ROM/USB identity and partition/checksum refusals, Gym title roundtrips, metric
  persistence, crank-event wrap/calibration, CPS packet bytes and stop timeout.
- A real originating Gym installation passed ten-turn cadence calibration and
  Watch cadence changes/stopping. The Watch subscribed to both CSC and power and
  displayed zero watts at rest. The owner physically confirmed metric tapping.
- Physical Desktop→Gym→Desktop switching is not yet qualified. Neither is the
  heuristic watt estimate calibrated. C6/Nordic purpose switching is unavailable.
- Existing public default/configuration, source opt-in, astronomy, renderer,
  transport, privacy and native menu checks remain part of the release suite.

## Screen-orientation coverage

- Shared firmware CRC parser, strict bounds and full-screen coordinate inverses
  run under ASan/UBSan. Calibration is checked at every rotation with all eight
  supported calibration transforms. Touch mapping remains separate from the
  display orientation.
- Worker tests cover per-device settings, idempotent absolute requests,
  reconnect restoration and refusing another device's settings. Native menu
  tests exercise the two actions, rapid left/right clicks and stale-target
  callbacks; older firmware keeps both actions disabled.
- Originating S3 and C6 firmware acknowledged right/left/wrap/restore requests
  through independent live helper workers. Their original orientations were
  restored after testing. Visual rotated-touch QC remains an owner check.
- S3 Desktop/Gym, C6 Desktop and Nordic Desktop public builds are compiled
  separately. Gym/Nordic rotation has no new physical-hardware claim.

## Firmware-update and ride-data coverage

- The actual OTA receiver runs under ASan/UBSan with real SHA256, fragmented
  input/output, failed writes, invalid images and profile/layout/hash refusals.
  Host tests cover pairing/identity checks, exact transfer, commit/readback and
  per-device maintenance cleanup. TLS pinning/authentication use local fixtures.
- Ride tests cover real crank intervals, no idle energy, pauses, persistence,
  reset/calibration behavior, averages/peaks and CSC/CPS wheel/crank timebases.
  Demo data remains separate from real rides and Watch notifications.
- Originating Gym r17/r18 completed actual Wi-Fi OTA and confirmed a different
  running slot and expected ELF. Desktop OTA transfer is still unverified; see
  [OTA.md](../device_profiles/OTA.md). C6/Nordic OTA is disabled.
- A separate C6 was updated to the public r34 build over USB. Firmware identity
  and helper reconnection were confirmed; Big Ben sustained approximately25FPS.
  That does not qualify C6 Wi-Fi/TLS or physical touch on every panel.
- Current public source is compiled separately for S3 Desktop/Gym, C6 Desktop
  and Nordic Desktop. No hardware is flashed as part of publication checks.

## Earlier Desktop update

- Offline privacy/default-config tests: no selected device, observer location,
  account, printer or enabled source; weather has no default city.
- Identification: S3/C6/Nordic and legacy firmware, unknown firmware, reconnect
  and boot changes, and same-device automatic connection scoping.
- Real localhost TLS tests: correct/wrong certificate pins, controller-token
  authentication, fragmented replies and receive limits. No internet used.
- USB setup parser under ASan/UBSan and finite CDC-ring overflow regression.
- Production clock recovery, schedules, brightness/session controls, carousel,
  scene/clock filters, Sun CRC coverage and S3 allocation/cache tests.
- Native helper builds/signs locally. Cached menu-open callback median was
  approximately0.003ms across500 off-screen opens; this excludes macOS popup
  animation and physical pointer movement. Views and checklists remain cached.
- Shared public sketch compiles with Arduino ESP32 core3.3.12 for S3/C6 and the
  documented nRF54 platform. Compilation does not establish C6 Wi-Fi runtime
  memory or a new owner’s panel/network behavior.

## Originating hardware observations

The S3 running the corresponding configured r33 firmware was positively
identified, joined its owner’s Wi-Fi and reconnected after a firmware update.
All ten scenes, menu controls, Weather12/24 mode, live Codex/printer/weather
packet delivery, and a complete Sun transfer passed over certificate-pinned
TLS. Full Sun CRC stayed identical across scene changes. Complete-row transfers
reduced a full integration check from133.80s to25.94s on that setup. These are
whole-test durations, not raw Wi-Fi throughput figures. The cached Sun image
was used for integrity checks when the upstream NASA fetch was unavailable.

Big Ben measured30.3FPS with Wi-Fi connected. The owner confirmed centre taps
and top double-taps. Helper2.7.0 identified the S3 in its actual menu, and the
owner confirmed the name/options. The neutral public r34 firmware itself was
compiled rather than flashed during this publication update.

C6 USB/Bluetooth scene controls were checked in earlier originating sessions;
C6 Wi-Fi/TLS heap and performance remain unqualified. S3 Bluetooth stack startup
was checked; new S3 pairing was not. Final public-package macOS permissions and
Bluetooth reconnect still need owner testing. Shared touch/RTC idle bus errors
remain a known separate investigation, despite working physical touch controls;
independent RTC reliability is not claimed. The helper synchronizes running time.

Run `python tools/check.py` for offline tests, `python tests/check_wifi.py` for
loopback TLS tests, and `python companion/service/manage.py stage` for the native
app build. None authorize querying an owner’s account, pairing a printer or
flashing a board. New owners should perform the explicit checks in SETUP.md,
HARDWARE.md and WIFI.md before relying on their own installation.

## Gym r19–r21 update

- ASan/UBSan checks cover wheel-counter reset/wrap, CSC/CPS packet consistency,
  malformed/unsupported control requests and complete fresh-ride reset.
- The real metric-cycle function is tested for Watts → RPM → Total Distance →
  Watts and saved selection; title/protocol checks remain included.
- Originating r20/r21 builds completed authenticated Wi-Fi installation with
  running ELF/slot verification. r21 live status showed all ride totals, averages
  and peaks at zero and retained the existing per-bike distance calibration.
- The new control point's live response remains unverified because the Mac probe
  discovered an obsolete cached service layout. Native Watch Indoor Cycle
  speed/distance summary saving remains unverified. No GymKit claim is made.
