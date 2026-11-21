// The game's own options as rows of the options window: game and scroll speed, autosave, the message kinds, and the
// four sound switches and volumes; and a new player's starting speeds. Both games keep these the same way (one block
// of their .inf file, written back by the game at exit), so the rows, their ranges, defaults and texts are here; the
// game layer gives where the values are and how the game's own dialogs change them (GameOptions). The values stay
// where the game keeps them, so the game without our DLL reads the same.
//
// Threads: Attach at start-up; the rows, ReapplyVolumes and CheckNewPlayer on the game thread.
#pragma once

#include <string>
#include <vector>

#include "core/options/window.h"

namespace core::options::game_options {

// Our starting speeds, also Restore defaults': 80% as Julius and Augustus (the walkers and animations at a natural
// pace); scroll 50%, where smooth scrolling (core/scroll/camera, Augustus's speeds) feels like the game's own 30%.
constexpr int kStartGameSpeed = 80, kStartScrollSpeed = 50;

constexpr int kChannelCount = 4; // music, speech, general sounds, ambient sounds (option_text "game" keys)

struct Channel {
    const char *key;     // "music", "speech", "sound_effects", "ambient_volume"
    int defaultVolume;   // the game's own default, 0..100
    bool stream;         // the game hands this volume to its sound manager (set again by ReapplyVolumes)
};

// What the game layer gives (a constant of the game layer). Every field must be filled.
struct GameOptions {
    GameOptions(int (*speed)(int which), void (*setSpeed)(int which, int value), bool (*autosave)(),
                void (*setAutosave)(bool on), int (*messageKinds)(), std::string (*messageName)(int kind),
                std::string (*messageModeName)(int mode), int (*message)(int kind),
                void (*setMessage)(int kind, int value), int (*messageDefault)(int kind), int fixedMessageKind,
                const Channel (&channels)[kChannelCount], bool (*soundOn)(int channel),
                void (*setSoundOn)(int channel, bool on), int (*volume)(int channel),
                void (*setVolume)(int channel, int value))
        : speed(speed), setSpeed(setSpeed), autosave(autosave), setAutosave(setAutosave), messageKinds(messageKinds),
          messageName(messageName), messageModeName(messageModeName), message(message), setMessage(setMessage),
          messageDefault(messageDefault), fixedMessageKind(fixedMessageKind), channels(channels), soundOn(soundOn),
          setSoundOn(setSoundOn), volume(volume), setVolume(setVolume)
    {
    }

    // which 0: the game speed, 1: the scroll speed. setSpeed gets a multiple of 10 in 10..100 and writes it as the
    // game's own speed buttons do.
    int (*speed)(int which);
    void (*setSpeed)(int which, int value);
    // Autosave on; setAutosave is called only when on differs from autosave() (the game's own switch flips it).
    bool (*autosave)();
    void (*setAutosave)(bool on);
    // The message kinds as the game's Messages dialog shows them: how many (a row each), the kind's name, a mode's
    // name (0 Off, 1 Condensed, 2 Full) and the game's value and default (0..2) of a kind.
    int (*messageKinds)();
    std::string (*messageName)(int kind);
    std::string (*messageModeName)(int mode);
    int (*message)(int kind);
    void (*setMessage)(int kind, int value);
    int (*messageDefault)(int kind);
    int fixedMessageKind; // a kind the game keeps at Full (no row); -1: none
    // The four sound channels: switch and volume as the game's Sound options set them (setSoundOn stops a channel
    // that goes off; setVolume 0..100 hands it to the sound manager).
    const Channel (&channels)[kChannelCount];
    bool (*soundOn)(int channel);
    void (*setSoundOn)(int channel, bool on);
    int (*volume)(int channel);
    void (*setVolume)(int channel, int value);
};

// Start-up, after the game layer checked the game's functions (g lives as long as the process). Before it the rows
// are left out.
void Attach(const GameOptions &g);
bool Attached();

// Rows for the window; nothing before Attach. Game thread (they read the game's values).
void SpeedRows(std::vector<Row> &rows, bool gameSpeed = true); // gameSpeed false: only the scroll speed
void AutosaveRows(std::vector<Row> &rows);
void MessageRows(std::vector<Row> &rows);
void SoundRows(std::vector<Row> &rows);

// The master volume changed: the stream volumes handed to the game's sound manager again, as the game does it.
void ReapplyVolumes();

// A new player's starting speeds. The game keeps its speeds in its .inf, which every install brings, so "no .inf"
// does not tell a new player. A new player is one with no player file yet (every game starts by making a player)
// whose speeds are still one of the shipped pairs: then they get ours.
struct NewPlayer {
    NewPlayer(const int (*shipped)[2], int shippedCount, bool (*hasPlayer)(), int (*speed)(int which),
              void (*writeSpeeds)(int game, int scroll))
        : shipped(shipped), shippedCount(shippedCount), hasPlayer(hasPlayer), speed(speed), writeSpeeds(writeSpeeds)
    {
    }

    const int (*shipped)[2]; // {game, scroll}: the install's .inf and the game's own defaults
    int shippedCount;
    bool (*hasPlayer)();     // a player file exists (PlayerFileIn)
    int (*speed)(int which); // 0: game speed, 1: scroll speed
    void (*writeSpeeds)(int game, int scroll); // as the game keeps them (it writes its .inf at exit)
};

// Game thread, once the game has read its .inf. true: ours written. Logged either way.
bool CheckNewPlayer(const NewPlayer &n);

// folder (with a trailing backslash) has a player file: Save\*.dat. Looked up from the game's own process, the same
// way the game finds its players.
bool PlayerFileIn(const std::wstring &folder);

// For the offline test: no game options again.
void DetachForTest();

} // namespace core::options::game_options
