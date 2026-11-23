# Known issues

Back to the [README](../README.md). If you run into something not listed here, please see [Reporting problems](../README.md#reporting-problems).

## Performance

- **Laptops and slower computers: lower the resolution first.** Raise the display scale (Options > Video), or choose *Auto* (1080 pixels high on taller screens). The game then draws fewer pixels; this helps more than any other setting.
- **City zoom above 100 % is slower**: the game draws more of the city (at 150 %, more than twice as much). If the game slows down, zoom in a little.
- **Laptops on battery are slower**, most noticeably when zoomed out. For the smoothest game, plug the laptop in, or set Windows' power mode to *Best performance* (Settings > System > Power & battery).

## Troubleshooting

- **Stutters or a picture that looks wrong:** try Direct3D 9: set **Renderer** to *Direct3D 9* in Options > Video > Display, and restart the game (the change takes effect at the next start). If you cannot change it in the game, set `renderer = direct3d` in the `[video]` section of `zeus_engine.ini` instead. Please also report it, with the log.
- **Crashes or sound problems:** Pericles's own work (showing the picture, saving screenshots) and the graphics driver run on the other processor cores, while the game itself stays where Windows puts it. To check whether this is the cause, set `cpu_affinity = original` in the `[compat]` section of `zeus_engine.ini` and restart the game. Please also report it, with the log.

## Display

- The adventure editor's main screen runs at the game's own 1024×768 (in a frame), not at your resolution. The city and the world map you edit are at your resolution.
- The editor does not zoom: the mouse wheel works there as in the original.
- Small cosmetic quirks of the original are kept for now (e.g. a side panel button can stay lit after the cursor moves quickly into the city).

## Installation

- **Antivirus or SmartScreen warnings:** `ddraw.dll` is not code-signed. You can check the zip against the SHA256 given on the release page.
- **Windows Security's Controlled folder access:** when it protects Documents and has not allowed `Zeus.exe`, Pericles cannot write to `Documents\My Games\Pericles` (Windows may show a "blocked" notification). Pericles then keeps its files in `%LOCALAPPDATA%\Pericles` instead; the log's `files:` lines say so. To use Documents, allow `Zeus.exe` in Controlled folder access (*Allow an app through Controlled folder access*). See also [Where are Pericles's files?](FAQ.md#where-are-pericless-files)
- **Documents synced with OneDrive:** `Documents\My Games\Pericles` is synced too, log and screenshots included. Nothing is done about this yet; tell us if it is a problem.

## Tested so far

- Two machines: AMD graphics (2560×1440, 200 Hz) and a laptop with Intel integrated graphics (2560×1600, 120 Hz), Windows 11; the GOG release, and the Steam release started from Steam.
- **Not tested yet:** NVIDIA graphics, real 4K monitors, and moving the window between several monitors. Reports are welcome.

## The original game

- While an advisor's chart is open (for example population), the city's animations run on and a little faster; the original does this too.
