#pragma once
#include <stdint.h>
#include <math.h>
// User-approved heuristic, not a measured/calibrated power meter:
// 28..46 W at 60 RPM, resistance 1..8, squared cadence scaling.
inline int16_t estimatedWatts(float rpm, int resistance) {
  if (!isfinite(rpm) || rpm <= 0) return 0;
  if (resistance < 1) resistance = 1;
  if (resistance > 8) resistance = 8;
  float watts = (28.0f + (resistance - 1) * (18.0f / 7.0f)) * (rpm / 60.0f) * (rpm / 60.0f);
  return watts >= 32767 ? 32767 : int16_t(lroundf(watts));
}
