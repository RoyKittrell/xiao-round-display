# Astronomy and schematic systems

Planet/Moon calculations use the included Astronomy Engine C implementation.
Civil-time conversion defaults to UTC and uses the configured fixed offset.
Optional observer latitude/longitude orient the Moon; with no observer the
renderer uses a neutral orientation and an unset-location label.

The analemma is deliberately schematic. It is not a to-scale image of a local
sky path. Solstice/equinox calendar dates use Astronomy Engine events converted
to configured civil time.

The star systems preserve adopted masses and orbital shapes where practical,
but expand close separations and use independent orbital clocks for readability.
They are not current-epoch astrometric/eclipsing-binary predictions:

- TYC7037-89-1 / TIC168789840: [Powell et al.2021](https://arxiv.org/abs/2101.03433),
  Tables2,7,8 and section6.3. All three binary pairs are shown.
- TIC290061484: [Kostov et al.2024](https://ntrs.nasa.gov/citations/20240013277),
  Table3 and sections5–6. The compact triple is shown, omitting the distant fourth component.
- HR6819: [Klement et al.2025](https://arxiv.org/abs/2501.04103), Table3.
  The Be star's present disc is a decretion disc. The optional historical gas
  stream illustrates a past transfer episode; it is not observed present-day
  flow. Playback does not grow/shrink stars, age them or force a merger.

Moon texture: [NASA CGI Moon Kit / LROC WAC](https://svs.gsfc.nasa.gov/4720/).
Live Sun imagery and NOAA measurements are fetched only when enabled; generated
data is kept outside the repository. Display source timestamps and stale states
are retained rather than presenting cached observations as current.
