# Gym ride data — r21

The distance model starts at **5000 mm per complete crank revolution**. It is a
provisional odometer conversion, not real outdoor travel. Power remains the
prototype 28..46W-at-60RPM cadence-squared heuristic. All calculation runs on-board.
Tap the bottom page label to cycle Live, Ride, Averages and Peaks. The large Live
number cycles Watts → RPM → Total Km (Total Mi in imperial units). The selected
metric persists across restart; old watts/RPM preferences migrate automatically.
The power label is now simply Watts; its underlying calculation remains estimated.
Total distance is the current XIAO ride total, reset on every startup or by Start
New Ride, and is not automatically aligned to Watch workout start/stop.
Resistance controls stay available on
all pages. Pages show estimated speed/distance, elapsed/moving/paused time,
crank turns, mechanical work (kJ), moving averages for power/cadence/speed,
whole-session average power/speed and peaks for power/cadence/speed.

Averages and work use timed completed crank intervals. The first untimed pulse
contributes a distance increment, but no invented interval energy. Pauses add no
work or distance. Whole-session averages include powered-on pause time and use
timed distance for a consistent numerator. Elapsed continues until Start New Ride;
each restart begins a new session. These are not Watch
workout start/stop controls and there is no automatic cross-device ride sync.

A ride snapshot is saved every30seconds while dirty, when motion times out,
on explicit New Ride and immediately before OTA restart. From r21 onward, startup
explicitly starts/saves an empty ride instead of restoring the previous snapshot.
Power cycles, reset-button restarts and firmware-update restarts all begin fresh
distance, time, work, averages and peaks; stopping pedalling alone does not reset.
Calibration, resistance, brightness, title, units and metric selection persist.
Raw pedal logs retain their own flush policy and are never erased by Start New Ride.
Demo totals live in a
separate object and never feed real totals, saved ride snapshots or Bluetooth.

Helper: **Ride Data** selects kilometres/kmh or miles/mph, edits future metres per
pedal turn and offers an explicitly confirmed Start New Ride. Calibration changes
apply to future increments; they do not rewrite earlier estimated distance.
Mechanical kJ are distinct from metabolic calories. HR/calories require a separate
measurement source; no fabricated readings are displayed for them.

Bluetooth exposes CSC wheel+crank data and CPS wheel+crank+accumulated energy.
The wheel is virtual, with a fixed2100mm circumference. Set the Watch accessory's
wheel size to2100mm if offered, otherwise displayed speed/distance can differ.
Crank event times use1/1024s; CSC wheel times1/1024s; CPS wheel times1/2048s.
Ride resets do not rewind Bluetooth wheel/crank/energy counters. Those counters
restart at firmware boot as ordinary accessory counters. Indoor Cycle handling of
virtual speed/distance remains a Watch integration test, not a GymKit guarantee.

r19 adds CSC SC Control Point (0x2A55), Write + Indicate, and handles Set
Cumulative Value (0x01). Counter offsets apply to both CSC and CPS wheel data,
preserving wheel phase, event times, crank counts, calibration and ride totals.
Malformed supported requests return Invalid Parameter; unsupported opcodes return
Op Code Not Supported. Requests are applied on the sensor task, with asynchronous
response indications; disconnected peers cannot queue a later counter reset.
The Arduino callback cannot return service-specific ATT errors for writes while
unsubscribed or busy; those writes are ignored without changing counters. This
is not a claim of Bluetooth qualification or complete service conformance.

Originating-device validation: a native Indoor Cycle summary saved cadence and
estimated power but omitted distance and speed. A missing wheel-service control
point was found in r18; r19 adds it. The subsequent Mac BLE probe saw an obsolete
cached service layout, so live control-point responses remain unverified. Receiver
wheel-size configuration and native Watch speed/distance saving still require
confirmation. These are integration limits, not a certified GymKit claim.

r20 was installed over paired Wi-Fi with running ELF/slot verification. r21 was
also installed and live startup status confirmed every ride total/average/peak at
zero, while preserving the device's existing calibration. Public defaults remain
unconfigured and generic; no originating-device logs or settings are included.

Offline checks cover conversion, intervals, no idle energy, pause/restore/reset,
calibration changes, peak/average calculations and exact Bluetooth bytes/timebases;
helper command bounds/readback and native menus also pass. All four physical
framebuffers were inspected with zero and nonzero isolated demo values. Wi-Fi
installation confirmed the new ELF/slot; hardware reports healthy storage and
zero dropped sensor events. Real riding/Watch speed-distance confirmation remains
for the next bike test. No certified GymKit claim is made.
