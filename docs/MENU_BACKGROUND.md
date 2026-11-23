# Your own menu background

Back to the [README](../README.md).

Pericles shows the game's menu pictures (the main menu and its sub-menus, briefings, high scores, victory and defeat) in a frame, as large as the display scale makes them. Around them, by default, is a blurred and darkened picture of **the last city you played**: Pericles saves it as `menu_background_auto.png` in its folder (the game folder, or `Documents\My Games\Pericles`; see the [FAQ](FAQ.md#where-are-pericless-files)) whenever you leave a city (back to the menu, or Exit game). On the very first start, before any city has been seen, the game's own panel pattern is shown instead.

## Using a picture of your own

1. Put the picture in Pericles's folder or in the game folder (next to `Zeus.exe`) and name it **`menu_background.png`**, or **`menu_background.bmp`**.
2. Start the game. The picture is used instead of your last city.

- **Any size works.** The picture is scaled to cover the whole screen, keeping its proportions (cropped at the edges, never stretched), then blurred and darkened exactly like the city picture, so the menu stays readable.
- If both files exist, the `.png` is used.
- To go back to your last city, delete (or rename) the file.

## Settings

The menu frame is *Menu screens* in Options > Video (`[video] menu_screens` in `zeus_engine.ini`):

| Value | Around the menus |
|---|---|
| `city` (default) | a fret border; your picture or your last city, blurred and darkened |
| `panel` | a fret border, on the game's own panel pattern |
| `framed` | a fret border, black around |
| `original` | the game's own picture, black around (as the game itself on a wide screen) |

How much the picture is blurred and darkened: *City background: blur* and *City background: brightness* in Options > Video (`background_blur`, 0 to 8, default 2, 0 is not blurred; `background_brightness`, 30 to 100 percent, default 65).

Please only use pictures you have the right to use.
