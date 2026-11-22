# Gym ride data — r18

The distance model starts at **5000 mm per complete crank revolution**. It is a
provisional odometer conversion, not real outdoor travel. Power remains the
prototype 28..46W-at-60RPM cadence-squared heuristic. All calculation runs on-board.

Tap the bottom page label to cycle Live, Ride, Averages and Peaks. The large Live
number retains its watts/RPM tap toggle. Resistance controls stay available on
all pages. Pages show estimated speed/distance, elapsed/moving/paused time,
crank turns, mechanical work (kJ), moving averages for power/cadence/speed,
whole-session average power/speed and peaks for power/cadence/speed.

Averages and work use timed completed crank intervals. The first untimed pulse
contributes a distance increment, but no invented interval energy. Pauses add no
work or distance. Whole-session averages include powered-on pause time and use
timed distance for a consistent numerator. Elapsed continues until Start New Ride;
power-off time is excluded when restoring a saved session. These are not Watch
workout start/stop controls and there is no automatic cross-device ride sync.

A ride snapshot is saved every30seconds while dirty, when motion times out,
on explicit New Ride and immediately before OTA restart. A sudden loss of power
can lose up to30seconds of unsaved totals; raw pedal logs retain their own flush
policy. Old raw logs are never erased by Start New Ride. Demo totals live in a
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

Offline checks cover conversion, intervals, no idle energy, pause/restore/reset,
calibration changes, peak/average calculations and exact Bluetooth bytes/timebases;
helper command bounds/readback and native menus also pass. All four physical
framebuffers were inspected with zero and nonzero isolated demo values. Wi-Fi
installation confirmed the new ELF/slot; hardware reports healthy storage and
zero dropped sensor events. Real riding/Watch speed-distance confirmation remains
for the next bike test. No certified GymKit claim is made.
