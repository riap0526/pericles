# Changelog

All notable changes to Pericles. Downloads, with the SHA256 of each zip, are on the [Releases](https://github.com/riap0526/pericles/releases) page. To upgrade, replace `ddraw.dll` and `ddraw.syms` in the game folder and keep your `zeus_engine.ini`: new settings take their defaults when they are missing, and the file's descriptions are brought up to date at the first start, with your values kept.

## 0.1.0 (2026-11-23)

The first release, for the English *Zeus + Poseidon* from GOG and Steam. More about each feature is in [docs/FEATURES.md](docs/FEATURES.md).

### Highlights

- **No display mode changes.** The game runs in a borderless full-screen window at your desktop resolution and is scaled up sharply. Sharp, nearest-pixel and smooth scaling switch at once in Options > Video.
- **Any resolution.** The game runs at your screen's resolution: the side panel and top bar reach the edges of the screen, the city fills the rest, and dialogs are centered.
- **Display scale for every screen.** 100–250 % or *Auto* makes the city, menus, briefings and loading screens bigger together. Menus are shown in a frame, over a blurred picture of your last city, a picture of your own, or black.
- **Smooth frame rate.** The picture updates at your monitor's refresh rate instead of about 20 times a second. The game's own timing, for the simulation and the animations, stays the same.
- **Animations at their designed speed.** On today's computers, the gods, heroes and urchin collectors moved slowly or unevenly, and some buildings far too fast. Pericles fixes the cause in the game's timing, for every building and person.
- **Windowed mode.** F5 or Alt+Enter switches between full screen and a window you can move, resize and maximize. Its size and position are remembered.
- **World map** at its own size or larger, centered over a blurred copy of itself or over your city (Options > Video > World map).
- **A new side panel, modeled on Emperor: Rise of the Middle Kingdom's.** The minimap is always shown, in a larger box that can also show several messages at once (Tab switches). Without the tabs, the information area is taller; names appear in the game's own tooltips. Information windows open next to the cursor. The original panel is one setting away.
- **Options window.** F6 opens a settings window in the game's own style, with the game's settings and Pericles's together on General, Video and Audio pages. It is also in the Options menu, and replaces Activision Website on the main menu.
- **Key rebinding** in Options > Hotkeys. Almost every key can be changed, with two keys per action; Ctrl, Shift, Alt and the mouse side buttons work too. The defaults are the original's, and "Game paused" shows your pause key.
- **Overlay keys.** Each overlay (supplies, water, hygiene, unrest, taxes, appeal and the others) has a key of its own, which the original does not have. They can be changed in Options > Hotkeys.
- **Esc closes windows**, like a right click, and answers No in confirmation boxes. In the city, it first drops the building tool or the selected troops, then opens the Pause menu. On the main menu, it asks before quitting.
- **Pause menu.** Continue, Replay map, Load, Save and Delete adventure, Options, Exit to main menu and Exit game, with the game paused while it is open. The editor has one too, with its own File menu items.
- **Difficulty and auto defend in Options.** While you play a city, Options > General > Difficulty has both, besides the game's own Options menu items. Difficulty changes one level at a time, as the game does it.
- **New players start at 80 % game speed and 50 % scroll speed**, which suit smooth scrolling better than the original's 90 % and 60 %. If you have played before, your settings stay the same.
- **Skip the intro movie** in Options > General, from the next start. The game's own files are not changed.
- **Messages in the game's message boxes.** "Screenshot saved", the zoom level and Pericles's other notices appear at the top of the city, like the game's own.
- **Smooth scrolling.** The city scrolls pixel by pixel and eases in and out. Each scroll speed setting has its own steady speed, the same on every computer.
- **City zoom with the mouse wheel**, from 50 % to 150 %, toward the cursor. A "Zoom" message shows the level, and the middle mouse button goes back to 100 %. The starting zoom is in Options > Video.
- **Drag scrolling.** Hold the middle mouse button over the city and move the mouse: the city follows it. A click without moving goes back to 100 % zoom. It works in the editor too.
- **Edge scrolling width.** How close to the edge of the screen the cursor has to be to scroll the city: 1 pixel as in the original, up to 20, or 0 for none (Options).
- **Steady game speed.** A big city at a high resolution runs at the speed you set, because Pericles catches up on missed simulation steps, as Augustus does. Time in a window or the Pause menu is not caught up.
- **A steady 100 % speed**, one step above 90 %, instead of as fast as the computer can draw. The city also keeps running while you scroll. Both can be switched back to the original.
- **Master volume and mute** for all music and sounds (Options > Audio). The game's own volume settings are kept.
- **Music and sounds pause when you switch away** (Alt+Tab, another window, minimizing) and go on where they were when you come back. The original stops them and starts another song.
- **Ambient sounds at a steady rate**, about one every two seconds on any computer and at any resolution. In the original, the rate depends on how fast the game runs.
- **Nature sounds without stutters.** The game's 3D sounds of wind and bells made the city stop for about a fifth of a second every ten seconds or so. Pericles plays them itself, in stereo, the way the game designed them.
- **Full city and minimap screenshots.** F10 saves the whole city as one PNG at the game's normal size, without the interface; Shift+F10 saves the minimap. Both are also in the File menu.
- **Screenshots and problem reports.** F11 saves a screenshot to `shots\` without slowing the game down. F12 marks the log and saves pictures to attach to a bug report.
- **Debug panel** (Insert), with the settings for testing and for fixing the display if something goes wrong, and the folder where Pericles keeps its files.
- **The adventure editor at your resolution.** Its world map and the city you edit run at your resolution, the city with the side panel down to the bottom of the screen. The editor's main screen is framed at 1024×768.
- **A notice for other versions of the game.** With any `Zeus.exe` other than the English GOG and Steam release 2.1.4.0, "Untested Zeus.exe" shows for 20 seconds at the start.
