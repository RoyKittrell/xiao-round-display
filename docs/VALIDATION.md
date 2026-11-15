# Validation and remaining checks

Checked 15 November 2026. The public source is an **unconfigured template** for
XIAO nRF54LM20A Sense plus Seeed Round Display; it is not a configured backup.
Helper **2.2.2** includes the menu responsiveness fixes and experimental
Bluetooth controls. Firmware is **public-template-r26**.

## Build and offline verification

- Firmware compiled with nrf54-arduino-core **1.0.21** and the documented board
  alias: **630,112 program bytes**, **79,692 static RAM bytes**. Dynamic canvases
  are additional RAM allocations.900 tile fingerprints replace the old
  previous-frame canvas, saving110,700 bytes while retaining RGB565 images.
- The Swift/AppKit helper built and was locally signed through `manage.py stage`.
  Staging does not install a service or start a data worker.
- The declared dependencies were installed in a dedicated environment. A real
  temporary runtime relocation check imported dependencies from the relocated
  environment and verified blank configuration: no device and no enabled feeds.
- Offline checks cover scene controls, CRC parsing, brightness boot isolation,
  clock/bus recovery, power scheduling, all/empty carousel masks, profiles,
  astronomy and bounded renderers. No account, printer or Bluetooth radio is
  contacted by these checks.
- Bluetooth harnesses cover fragmented/bounded receive data, authenticated RX,
  USB/Bluetooth packet isolation, write backpressure, overflow reconnection,
  cancellation cleanup and automatic fallback with no configured USB port.
- Native search tests check decoded results, failed child processes and bounded
  timeout/cleanup using synthetic scripts. The native menu harness checks cached
  view reuse, All/None/mixed states and menu-activity cleanup. These are not
  measurements of physical pointer/menu animation latency.
- Sun-transfer mocks reconstruct57,600 pixels with CRCs and complete coverage.
  They verify1,920 wireless control/health opportunities and120 USB opportunities.
- Publishable files are audited for personal paths/addresses, credentials,
  device selection, profiles, caches and generated binaries. Pairings and live
  readings stay local; optional sources remain disabled by default.

## Observations on the originating hardware

The owner confirmed that the menu responsiveness fix improved opening and
navigation. Initial Bluetooth integration passed passkey pairing, acknowledged
wireless scene selection and brightness changes, and one bonded helper restart.
The owner confirmed scene changes and clock animation. Startup brightness60%,
physical dimming with KE switch2 ON and USB controls were also checked.

These observations do not establish compatibility for every Mac, panel or
printer. The final installed package's Bluetooth permission/reconnection check
remains pending after the latest rebuild. Full wireless Sun-image completion,
long-duration wireless operation and cold-boot reconnection are not yet verified.
Use USB as the initial/recovery path and treat Bluetooth as experimental.

Intermittent shared touch/RTC I²C stalls occurred on the originating hardware.
Automatic recovery is included, but power/connection checks and a full power
cycle can still be needed. There is no wireless firmware updater. Additional
XIAO controllers require their own pin, RAM, driver and upload validation.
