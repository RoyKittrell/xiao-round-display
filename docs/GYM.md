# Gym Display profile

Gym currently targets the standard XIAO ESP32-S3 with OPI PSRAM and the Seeed
Round Display. Use the FQBN in `device_profiles/catalog.json`. Connect its external
antenna for Wi-Fi/Bluetooth. USB or a suitable external USB power source can run
it; the optional RTC coin cell is not required for operation.

## Sensor and wiring

The input expects a verified passive contact closure between **D2 (GPIO3)** and
**GND**, using the internal pull-up and 5 ms stable debounce. The original bike
console must be disconnected. Establish that the bike jack really exposes a
passive switch before wiring; an active sensor or powered console signal requires
appropriate electrical interfacing. Do not connect an unknown output to a GPIO.
Cable colours are not standard: map the socket contacts with a meter. A mono plug
can short a stereo socket's ring and sleeve together. No SD card is initialised,
because the Round Display's SD chip select also uses D2.

Pedal logging runs separately from rendering and Bluetooth. LittleFS appends
`/pedals.csv` across resets, up to 1 MiB, with no automatic log erasure. Stop and
wait at least two seconds before removing power. The user LED shows recording
readiness and accepted closures; a readiness heartbeat alone does not prove the
sensor is connected. Completely erased storage can be initialized for first use;
existing or unrecognized filesystem data is never automatically formatted.

Count ten complete forward pedal turns and compare accepted closures to establish
pulses per revolution. The originating bike produced one closure per full turn;
other bikes require their own calibration. `PPR n` accepts 1–16 and saves it.

## Estimated power

The current user-selected prototype model is:

```
watts = (28 + (resistance - 1) * 18 / 7) * (crank_RPM / 60)^2
```

Resistance is manually selected from 1–8. The result is rounded to whole watts.
It gives 28–46 W at 60 RPM and 63–104 W at 90 RPM. This is a heuristic informed
by a slow force test and comparison with home exercise bikes, not a measured
power curve for arbitrary bikes. Adjust `EstimatedPower.h` for another setup.
Do not interpret it as a calibrated power meter or a precise calorie estimate.
The Watch power field does not convey the display's estimated-power label.

All calculation runs on the XIAO. Standard CSC service 0x1816 and Cycling Power
service 0x1818 transmit accepted real crank events and estimated watts. They do
not implement certified GymKit or NFC pairing. Start an Indoor Cycle workout and
select the sensor in Watch Bluetooth settings. Depending on cached services,
forgetting and pairing again may be needed after a firmware service change.

Only real sensor events feed the Watch. Demo Mode animates the display without
fabricating workout events. Power returns to zero after no event for the greater
of four seconds or three previous crank intervals. Two crank events are needed
to establish cadence after boot/calibration.

## Controls and connections

Tap the large number to toggle watts/RPM. The other metric remains above it.
Use the on-screen +/- buttons to match the bike's actual resistance setting.
A right-edge downward drag dims the backlight. The helper's Pulses submenu
controls the glow separately. Device Settings exposes a saved 1–24 character
printable ASCII title as well as the Mac-local device name.

The Mac prefers USB, then the paired certificate-pinned Wi-Fi connection.
Bluetooth on the Gym profile is reserved for the Watch and offers no Mac control
writes. Wi-Fi settings are provisioned over USB. Neither the laptop nor Wi-Fi
is needed for local sensing, display, calculation or Watch notifications.
Same-profile updates can use paired Wi-Fi after one USB bootstrap; see
[OTA.md](../device_profiles/OTA.md). No automatic sleep/wake feature is included.

## Validation limits

On the originating S3 installation, a counted ten-turn test yielded ten accepted
closures, Watch cadence followed faster pedalling and returned to zero, and the
Watch subscribed to the new power characteristic and displayed zero at rest.
The metric toggle was physically confirmed. The current r18 build also adds
[ride totals and virtual wheel data](../device_profiles/gym/RIDE_METRICS.md).
Distance defaults to a provisional 5 metres per full crank revolution and must
be calibrated for each bike. Bluetooth wheel circumference is fixed at2100mm;
set that value on the receiving device if offered. Virtual travel and estimated
mechanical work are not outdoor distance or metabolic calories.

Offline tests exercise packet bytes,
the power model, stop timeout, calibration, title encoding, persistence and storage
protection. These observations do not establish watt accuracy or qualify another
bike/controller. Physical Desktop/Gym purpose-swap testing remains outstanding.
