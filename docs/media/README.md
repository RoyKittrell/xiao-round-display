# Animated project preview

`xiao-display-showcase.gif` is a 26.2-second, 960 × 580 looping GIF. The 240 × 240
screen graphics are enlarged 2× inside a stylised bezel. This is a desktop
renderer preview, not a recording of a physical device or a hardware FPS claim.

The showcase uses six of the ten landmark clock faces, a night palette,
Analemma, Moon phases, planetary orbits, TYC 7037-89-1, Conway's Game of Life,
and the Codex/printer status renderers. Clock and orbital motion/calendar time
are accelerated for the short preview. Star systems and the Analemma remain
schematic; solar-system distances use the scene's compressed-distance display.

All data is generated offline. The fixed example dates, 68% usage allowance,
reset countdown, print progress and **Sample enclosure** print name are
illustrative. No account, printer, Bluetooth connection, network endpoint,
saved profile or live user data is read. The generator refuses a checkout with
a local firmware-settings override.

The clock, stars, Game of Life and status images call the production renderer
headers. Analemma, Moon and solar-system functions are extracted from the
current sketch at build time and rendered using the bundled Adafruit GFX code;
only hardware/serial dependencies are stubbed. The Moon uses the existing
NASA/LROC-derived texture; attribution remains in `MoonTexture.h` and
[THIRD_PARTY.md](../../THIRD_PARTY.md). System fonts are rasterized for the
surrounding captions; font files are not redistributed.

To regenerate from an unconfigured checkout, install the project requirements
and have Clang/C++17 available, then run:

```sh
.venv/bin/python -B tools/build_showcase.py
```

The output is written here. Temporary screen frames are removed automatically;
review contact sheets are written to the system temporary directory. Caption
fonts can differ between operating systems. Visually review any regenerated
GIF for layout and personal data before updating its fingerprint in
`tools/privacy_audit.py`. Only that specific reviewed GIF is permitted as a
binary by the audit; other unreviewed binaries are still rejected.
