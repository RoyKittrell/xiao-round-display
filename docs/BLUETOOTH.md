# Bluetooth scene controls (experimental)

The XIAO nRF54LM20A Sense has Bluetooth LE, not Wi-Fi. The Mac fetches network
data and must be awake and nearby to keep readings fresh. Helper **2.2.2** and
firmware **r26** carry the existing acknowledged scene protocol over a custom,
encrypted, authenticated GATT service. USB remains the default.

## Set up your own display

1. Follow [SETUP.md](SETUP.md) to build/upload the firmware and install the menu
   helper from your own virtual environment. No device, accounts, location or
   data sources are configured in this repository.
2. Verify USB scene selection first. Keep the screen powered on and visible so
   you can read a pairing code. Enable KE switch2 for physical brightness control.
3. Choose **Connection → Find Bluetooth Display…**. Allow Bluetooth access for
   **XIAO Display Helper** when macOS asks. Scanning is explicit; the helper
   never silently selects an arbitrary nearby display.
4. Select your display from the discovered list. If macOS requests a code,
   enter the six digits shown on the round screen into macOS's prompt.
5. Test scene selection and brightness with **Connection → Bluetooth** selected,
   even if USB still supplies power. Test reconnection on your own setup.
6. **Automatic · prefer USB** uses a configured, present USB port first and the
   explicitly paired Bluetooth display otherwise. It also supports a Bluetooth
   installation without a configured USB port. Pairing IDs stay in local app data.

The initial hardware tests passed passkey pairing, wireless scene/brightness
commands, clock animation and one bonded helper restart. The final installed
package's permission/reconnection check remains pending. Complete large Sun
image delivery and compatibility across other Macs/panels are not yet verified.
Treat Bluetooth as experimental and retain USB for setup and recovery.

## What can change wirelessly

Choose existing scenes; hide/re-enable carousel entries; select saved profiles;
change playback, appearance, brightness or power; synchronize time; and deliver
live-source packets. All scenes remain saved when excluded. New rendering code,
animations, clock faces or compiled assets require a USB firmware upload.

Large Sun images are slower over Bluetooth. Transfers yield to controls and
health updates after each acknowledged pixel packet; source timestamps retain
their original meaning. A completed full wireless image transfer is not yet a
hardware-verified claim. There is no wireless firmware updater or placeholder
DFU service. The Mac's own network connection is still required for live feeds.

## If discovery or pairing stalls

- Allow access in **System Settings → Privacy & Security → Bluetooth**, then
  retry **Find Bluetooth Display…**. Local ad-hoc app rebuilds can require access
  to be granted again. Discovery has a bounded timeout and can be retried.
- Ensure the display has r26 firmware, is powered on and is nearby. Briefly
  paused redraws during authentication leave radio time for discovery/pairing;
  the pairing card is drawn when its code changes.
- If macOS retains an obsolete pairing after firmware changes, remove that
  pairing in macOS Bluetooth settings and pair again.
- Select **Connection → USB** to return to the wired controls. Before uploading
  firmware, stop the helper and close Serial Monitor. Do not run competing data
  workers against the same display.

## Implementation and validation

The installer copies its dedicated project environment into Application Support
and runs it from there. Keep the underlying Python installation available. No
administrator daemon or Wi-Fi credentials are needed. BLE dependencies are
listed in `requirements.txt`; source/installed environments are local, ignored
files, not portable prebuilt apps.

The firmware stores900 tile fingerprints instead of a115,200-byte previous-frame
canvas, saving110,700 bytes for Bluetooth while retaining RGB565 image quality.
Only completed transfers advance the sent fingerprints; periodic LCD recovery
also repaints unchanged content. Tests reconstruct full frames across changed
pixels, interrupted transfers and forced repaints.

Offline tests cover bounded/fragmented input, authenticated RX, USB/Bluetooth
packet isolation, GATT write backpressure, empty-USB automatic fallback, native
search timeout/cleanup and source interleaving. These tests do not contact a
real account/printer, scan Bluetooth or assert universal hardware compatibility.
