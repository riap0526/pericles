# FAQ

Back to the [README](../README.md).

**[About Pericles](#about-pericles)**

- [Is there a macOS or Linux version?](#is-there-a-macos-or-linux-version)
- [Which versions of Zeus does Pericles work with?](#which-versions-of-zeus-does-pericles-work-with)
- [Does it work with the Steam release?](#does-it-work-with-the-steam-release)
- [Can I play on an older version of Windows?](#can-i-play-on-an-older-version-of-windows)
- [Are my saved games safe? Can I go back to the original game?](#are-my-saved-games-safe-can-i-go-back-to-the-original-game)
- [Does Pericles change how the game plays?](#does-pericles-change-how-the-game-plays)
- [Does the adventure editor work?](#does-the-adventure-editor-work)
- [Can I use Pericles together with cnc-ddraw, dgVoodoo or another ddraw.dll?](#can-i-use-pericles-together-with-cnc-ddraw-dgvoodoo-or-another-ddrawdll)
- [Is there a version for the other Impressions city builders?](#is-there-a-version-for-the-other-impressions-city-builders)

**[Display and settings](#display-and-settings)**

- [The text is too small on my screen. Can it be larger?](#the-text-is-too-small-on-my-screen-can-it-be-larger)
- [The screen looks wrong after I changed a setting. How do I undo it?](#the-screen-looks-wrong-after-i-changed-a-setting-how-do-i-undo-it)
- [Can I have the game's own side panel back?](#can-i-have-the-games-own-side-panel-back)
- [How do I skip the intro movie?](#how-do-i-skip-the-intro-movie)
- [Why Direct3D 11 by default?](#why-direct3d-11-by-default)
- [How do I switch the renderer?](#how-do-i-switch-the-renderer)

**[Files and the log](#files-and-the-log)**

- [Where are Pericles's files?](#where-are-pericless-files)
- [What is the log for, and what is in it?](#what-is-the-log-for-and-what-is-in-it)

## About Pericles

### Is there a macOS or Linux version?

Not for now. Pericles works inside the original Windows `Zeus.exe`, so it runs where that runs. Native macOS and Linux versions are a long-term goal, for after the reimplementation that no longer needs the original executable (subject to change: this is a goal, not a promise).

Tools that run Windows games on Linux or a Mac, such as **Proton** (Steam), **Wine** or **CrossOver**, may work, but nobody has reported on them yet. If you try one, please tell us how it went.

### Which versions of Zeus does Pericles work with?

Pericles is made and tested with the **English** *Zeus + Poseidon* as sold by **GOG** and **Steam**, `Zeus.exe` version 2.1.4.0 (both have the same `Zeus.exe`; its SHA256 is in the [README](../README.md#requirements)). They only sell the English edition. Other languages were only sold on the original retail CDs; those editions, and other releases, have not been tested: Pericles may work with them, but it may have problems, or some of its features may not work.

With any `Zeus.exe` other than the tested one, Pericles shows "Untested Zeus.exe" at the top of the screen for 20 seconds at the start, and the log says which exe it is. Before changing the game, Pericles also checks that the parts of `Zeus.exe` it changes are the ones it knows; if they are not, none of its game changes are made, and the game runs as the original at its own 1024×768, only shown in Pericles's window (the log says so at the start). If you have another version, please tell us how it went, with the log.

**Use an unmodified `Zeus.exe`.** An exe changed by another tool, such as a widescreen patch, may stop Pericles from working, or cause problems: Pericles only checks the parts it changes itself, not everything such a tool may have changed. If you have used one, restore the original exe (for example by reinstalling, or by verifying the game files in GOG Galaxy or Steam) before installing Pericles.

### Does it work with the Steam release?

Yes. The Steam release has the same `Zeus.exe` as the GOG release. Put the files in the game folder (in Steam: right-click the game > Manage > Browse local files) and start the game from Steam as usual. The Steam overlay (Shift+Tab) and Steam's FPS counter work with Pericles.

### Can I play on an older version of Windows?

Pericles is made for Windows 10 and 11 and tested on Windows 11. In theory it can also start on Windows 7 SP1 and 8.1, and on 32-bit Windows, but none of these have been tested:

- **Windows 7 and 8.1** need Microsoft's **Universal C Runtime** update (KB2999226) first; without it `ddraw.dll` cannot load and the game does not start (delete `ddraw.dll` to play without Pericles again).
- Some newer display features are only in Windows 10 and later; Pericles then uses the older way, or leaves them out.
- **32-bit Windows** gives the game less memory, so a full city screenshot of a very large city may not be possible (you get a message instead).

You are welcome to try. We may not be able to help if Pericles does not start on these versions; if it runs, problems in the game itself are welcome as usual (see [Reporting problems](../README.md#reporting-problems)).

### Are my saved games safe? Can I go back to the original game?

Yes. Pericles does not change the game's files or its formats: saved games and adventures (including custom ones) are the original game's, saves made with Pericles open in the game without it, and the other way round. To go back, delete `ddraw.dll`.

This may change in the future: options that change the game's rules or limits may need a save format of Pericles's own, as in Augustus. They would be optional, and the original saves and adventures would still load.

### Does Pericles change how the game plays?

Not so far: everything Pericles does today is about the display, the sound and the controls, and the city's simulation is the original game's. Later, guided by what players ask for, Pericles may add gameplay improvements and higher limits, closer to what Augustus does for Caesar 3 than to a strictly faithful remake. They would be options you choose, marked when they affect whether saves still work in the original game.

### Does the adventure editor work?

Yes. The editor is part of `Zeus.exe`, and Pericles works there too: its world map and the city you edit are at your resolution, and its main screen is at the game's own 1024×768, in a frame. The Options window (F6), the Pause menu (Esc) and drag scrolling work in the editor; city zoom does not.

### Can I use Pericles together with cnc-ddraw, dgVoodoo or another `ddraw.dll`?

No: they are all a `ddraw.dll` in the game folder, and only one can be used. Pericles already does what they are usually used for with this game (a borderless full screen or a window at any resolution, with sharp scaling). Keep a backup of the other `ddraw.dll` if you want to switch back.

### Is there a version for the other Impressions city builders?

Yes, for *Emperor: Rise of the Middle Kingdom*: [Yongle](https://github.com/riap0526/yongle) is Pericles's sister project, built on the same engine and with the same approach. For the other games, have a look at these community projects:

- [Julius](https://github.com/bvschaik/julius) and [Augustus](https://github.com/Keriew/augustus) for *Caesar III*.
- [Akhenaten](https://github.com/dalerank/Akhenaten) for *Pharaoh*.

## Display and settings

### The text is too small on my screen. Can it be larger?

Yes: raise the **Display scale** (Options > Video), or choose *Auto* (1080 pixels high on taller screens). The whole game is drawn larger, text included, and every screen follows it. Sharper, scalable fonts may be looked at later, together with localization.

### The screen looks wrong after I changed a setting. How do I undo it?

- **Resolution and display scale** ask "Keep this setting?" after a change: choose Revert, or wait 15 seconds and the old value comes back.
- For the other screen settings, open the **debug panel** (Insert), which has the settings for fixing the display, and change it back there.
- If the game cannot be used at all, close it and change the setting in `zeus_engine.ini` (the file explains each one). To start again with every setting at its default, delete both `zeus_engine.ini` and `zeus_engine.ini.bak`. The game's own options (game speed, volumes and so on) are kept either way.

### Can I have the game's own side panel back?

Yes: set **Side panel** to `original` in Options > General > Interface (or `side_panel = original` in the `[ui]` section of `zeus_engine.ini`). It switches at once.

### How do I skip the intro movie?

Set **Intro movie** to `skip` in Options > General, or `intro_movie = skip` in the `[ui]` section of `zeus_engine.ini`. It takes effect the next time the game starts. Pericles does not change the game's own files: without Pericles the movie plays as before.

### Why Direct3D 11 by default?

Both renderers draw practically the same picture, and on the computers we have tested they run at about the same speed. Direct3D 11 is the default because:

- It needs a little less CPU time per frame than Direct3D 9.
- The `sharp_bilinear` scaling option is done in one step in Direct3D 11 and in two in Direct3D 9 (the result looks almost the same).

Direct3D 9 stays available, and Pericles switches to it automatically if Direct3D 11 cannot start. If you see stutters, flickering or a picture that looks wrong, try Direct3D 9 (below) and please report it with the log.

### How do I switch the renderer?

Pericles uses Direct3D 11 by default (`renderer = auto`; Direct3D 9 only if Direct3D 11 cannot start). If you see stutters or a picture that looks wrong, try Direct3D 9: set **Renderer** to *Direct3D 9* in Options > Video > Display, and restart the game (the change takes effect at the next start). If you cannot change it in the game, set `renderer = direct3d` in the `[video]` section of `zeus_engine.ini` instead. *Direct3D 11* (`renderer = direct3d11`) forces Direct3D 11. If Direct3D 9 helps, please report it with the log (see [Reporting problems](../README.md#reporting-problems)).

## Files and the log

### Where are Pericles's files?

It depends on whether Windows lets a standard user (not an administrator) write to the game folder:

- **Yes** (most installs outside Program Files, e.g. `D:\Games\Zeus`): in the **game folder**, next to `ddraw.dll`. This includes games installed by GOG Galaxy in its default folder, `C:\Program Files (x86)\GOG Galaxy\Games` or `C:\Program Files\GOG Galaxy\Games`: GOG Galaxy lets standard users write there, even though it is under Program Files. The same goes for Steam's default library, `C:\Program Files (x86)\Steam\steamapps\common`.
- **No** (e.g. a game copied or installed elsewhere under `C:\Program Files (x86)`): in **`Documents\My Games\Pericles`** (your Documents folder, wherever Windows keeps it). If Pericles cannot write there either, for example because Windows Security's *Controlled folder access* blocks `Zeus.exe`, they are in `%LOCALAPPDATA%\Pericles` instead.

The answer is the same whether or not the game runs as administrator, so there is always one set of files. The debug panel (Insert) shows the folder at the bottom, with an **Open folder** button; the log's `files:` lines say why it was chosen. The game's own saved games stay where the game keeps them.

The files:

- `ddraw_proxy.log`: the log. Over 4 MB it is renamed to `ddraw_proxy.old.log` at the next start; it can be deleted at any time.
- `zeus_engine.ini`: the settings. When a new version of Pericles first starts, it brings the file's descriptions up to date (your values are kept) and keeps the file as it was in `zeus_engine.ini.bak`; the same backup is written when old settings are cleaned up. If the file is missing, it is made again, with the values of `zeus_engine.ini.bak` when there is one, otherwise at the defaults.
- `menu_background_auto.png`: the last city you played, for the menu backdrop.
- `shots\`: screenshots (F11), full city and minimap screenshots (F10, Shift+F10) and problem reports (F12).

To uninstall, delete `ddraw.dll`, and if you like `ddraw.syms` and these files (or the whole `Documents\My Games\Pericles` folder) too. Nothing else was changed.

### What is the log for, and what is in it?

`ddraw_proxy.log` is how we find and fix problems. Pericles changes how the game draws, scrolls and runs, and most problems only happen on some computers, so when you report one, the log tells us what Pericles did on yours: which renderer and resolution it used, which settings were in effect, how long each frame took, and the details of any error. Julius, Augustus and Akhenaten write a similar log file for the same reason.

- **Nothing is sent anywhere.** Pericles has no network code. The log stays on your computer, and only you decide whether to share it.
- **What it contains:** Pericles's settings, your screen resolution and refresh rate, the number of processors, the renderer and display driver, programs that load into the game (for example the Steam overlay), timings, and error details.
- **What it does not contain:** your name, email, accounts, computer name, IP address or saved games.
- **File paths:** the log shows where the game and Pericles's files are, for example `Documents\My Games\Pericles` inside your Windows user folder, whose name is usually your Windows user name, so the log can include it. If you'd rather not share it, replace it with something else before posting the log publicly; that does not affect what we can learn from it.
- You can delete the log at any time; Pericles starts a new one at the next start.
