// Offline test of core/options/game_options (no DLL, no game): the game's own options as rows of the options window
// on a fake game -- the rows' kinds, ranges, defaults and texts; the speeds clamped to 10..100 in tens; autosave set
// only when it differs; the message kinds' "2 - value", their names, a fixed kind left out; the sound switches and
// volumes; Restore defaults and Apply writing only what changed; ReapplyVolumes on the streamed channels only; no
// rows before Attach. Then a new player's speeds for each combination of a player file and the shipped speeds, and
// PlayerFileIn on a folder made below this exe's (test_game_options_tmp\, removed at the end).
#include "core/options/game_options.h"
#include "core/options/window.h"
#include "core/platform/files.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace co = core::options;
namespace go = core::options::game_options;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
    std::printf("%-72s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) ++g_failures;
}

// The fake game.
struct Fake {
    int speed[2] = {90, 60};
    bool autosave = true;
    int autosaveCalls = 0;
    int kinds = 5;
    int messages[20] = {2, 1, 0, 2, 2};
    int defaults[20] = {2, 2, 1, 2, 0};
    bool on[4] = {true, true, true, false};
    int volume[4] = {50, 100, 80, 80};
    int setVolumeCalls[4] = {};
    int writes = 0; // every set of any value
    bool player = false;
} g;

int Speed(int which) { return g.speed[which]; }
void SetSpeed(int which, int v) { g.speed[which] = v; ++g.writes; }
bool Autosave() { return g.autosave; }
void SetAutosave(bool on) { g.autosave = on; ++g.autosaveCalls; ++g.writes; }
int MessageKinds() { return g.kinds; }
std::string MessageName(int k) { return "Kind " + std::to_string(k); }
std::string ModeName(int m)
{
    const char *n[] = {"Off", "Condensed", "Full"};
    return n[m];
}
int Message(int k) { return g.messages[k]; }
void SetMessage(int k, int v) { g.messages[k] = v; ++g.writes; }
int MessageDefault(int k) { return g.defaults[k]; }
bool SoundOn(int i) { return g.on[i]; }
void SetSoundOn(int i, bool on) { g.on[i] = on; ++g.writes; }
int Volume(int i) { return g.volume[i]; }
void SetVolume(int i, int v) { g.volume[i] = v; ++g.setVolumeCalls[i]; ++g.writes; }

const go::Channel kChannels[go::kChannelCount] = {
    {"music", 50, true}, {"speech", 100, true}, {"sound_effects", 80, false}, {"ambient_volume", 80, true}};

const go::GameOptions kGame(&Speed, &SetSpeed, &Autosave, &SetAutosave, &MessageKinds, &MessageName, &ModeName,
                            &Message, &SetMessage, &MessageDefault, -1, kChannels, &SoundOn, &SetSoundOn, &Volume,
                            &SetVolume);
const go::GameOptions kGameFixed(&Speed, &SetSpeed, &Autosave, &SetAutosave, &MessageKinds, &MessageName, &ModeName,
                                 &Message, &SetMessage, &MessageDefault, 3, kChannels, &SoundOn, &SetSoundOn, &Volume,
                                 &SetVolume);

int Get(const co::Value &v) { return v.get(v.ctx); }
void Set(const co::Value &v, int x) { v.set(v.ctx, x); }

void TestRows()
{
    std::vector<co::Row> rows;
    go::SpeedRows(rows);
    go::AutosaveRows(rows);
    go::MessageRows(rows);
    go::SoundRows(rows);
    Check(rows.empty() && !go::Attached(), "before Attach: no rows");

    go::Attach(kGame);
    Check(go::Attached(), "attached");

    // Speeds.
    rows.clear();
    go::SpeedRows(rows);
    Check(rows.size() == 2, "two speed rows");
    Check(rows[0].kind == co::Kind::Slider && rows[0].id == "game_speed" && rows[0].label == "Game speed",
          "game speed: slider, id and label");
    Check(rows[1].id == "scroll_speed" && rows[1].label == "Scroll speed" && !rows[1].help.empty(),
          "scroll speed: id, label, description");
    Check(rows[0].min == 10 && rows[0].max == 100 && rows[0].step == 10 && rows[0].unit == "%" &&
              rows[1].min == 10 && rows[1].max == 100 && rows[1].step == 10,
          "speeds: 10..100 in tens, %");
    Check(rows[0].defaultValue == 80 && rows[1].defaultValue == 50, "speeds' defaults 80 / 50");
    Check(Get(rows[0].value) == 90 && Get(rows[1].value) == 60, "speeds read the game's");
    Set(rows[0].value, 37);
    Check(g.speed[0] == 30, "a game speed of 37 is written as 30");
    Set(rows[0].value, 5);
    Check(g.speed[0] == 10, "below 10: 10");
    Set(rows[1].value, 140);
    Check(g.speed[1] == 100, "above 100: 100");
    g.speed[0] = 90;
    g.speed[1] = 60;
    rows.clear();
    go::SpeedRows(rows, false);
    Check(rows.size() == 1 && rows[0].id == "scroll_speed", "without the game speed: only the scroll speed");

    // Autosave.
    rows.clear();
    go::AutosaveRows(rows);
    Check(rows.size() == 1 && rows[0].kind == co::Kind::Check && rows[0].label == "Autosave" &&
              rows[0].defaultValue == 1,
          "autosave: one checkbox, default on");
    Set(rows[0].value, 1);
    Check(g.autosaveCalls == 0, "autosave set to what it is: the game's switch not called");
    Set(rows[0].value, 0);
    Check(g.autosaveCalls == 1 && !g.autosave && Get(rows[0].value) == 0, "autosave off: switched once");
    Set(rows[0].value, 1);
    Check(g.autosaveCalls == 2 && g.autosave, "autosave on again");

    // Messages.
    rows.clear();
    go::MessageRows(rows);
    Check(rows.size() == 5, "a row per message kind");
    Check(rows[0].kind == co::Kind::Choice && rows[0].label == "Kind 0" && rows[4].label == "Kind 4",
          "message rows: choices named by the game");
    Check(rows[0].choices.size() == 3 && rows[0].choices[0] == "Full" && rows[0].choices[2] == "Off",
          "choices Full, Condensed, Off");
    Check(rows[0].choiceHelp.size() == 3 && !rows[0].choiceHelp[0].empty() && rows[0].help.empty(),
          "each choice has a tooltip; the name has none");
    Check(Get(rows[0].value) == 0 && Get(rows[1].value) == 1 && Get(rows[2].value) == 2,
          "shown as 2 - the game's value");
    Check(rows[0].defaultValue == 0 && rows[2].defaultValue == 1 && rows[4].defaultValue == 2,
          "defaults as 2 - the game's default");
    Set(rows[0].value, 2);
    Check(g.messages[0] == 0, "Off written as the game's 0");
    Set(rows[3].value, 1);
    Check(g.messages[3] == 1, "Condensed written as the game's 1");
    g.messages[1] = 7;
    Check(Get(rows[1].value) == 0, "a value past Full is shown as Full");

    go::Attach(kGameFixed);
    rows.clear();
    go::MessageRows(rows);
    bool fixedShown = false;
    for (const co::Row &r : rows) {
        fixedShown = fixedShown || r.label == "Kind 3";
    }
    Check(rows.size() == 4 && !fixedShown && rows[3].label == "Kind 4", "the fixed kind has no row");
    Set(rows[3].value, 2);
    Check(g.messages[4] == 0, "the row after the fixed kind writes its own kind");
    go::Attach(kGame);
    g.kinds = 0;
    rows.clear();
    go::MessageRows(rows);
    Check(rows.empty(), "no kinds: no rows");
    g.kinds = 5;

    // Sound.
    rows.clear();
    go::SoundRows(rows);
    Check(rows.size() == 4, "four sound rows");
    Check(rows[0].kind == co::Kind::CheckSlider && rows[0].label == "Music" && rows[2].label == "General sounds" &&
              rows[3].label == "Ambient sounds",
          "sound rows: check + slider, named");
    Check(rows[0].min == 0 && rows[0].max == 100 && rows[0].step == 5 && rows[1].defaultValue == 100 &&
              rows[0].defaultToggle == 1,
          "volumes 0..100 in fives; defaults the game's; on by default");
    Check(Get(rows[3].toggle) == 0 && Get(rows[0].toggle) == 1 && Get(rows[1].value) == 100,
          "switches and volumes read the game's");
    Set(rows[1].value, 120);
    Check(g.volume[1] == 100, "a volume above 100: 100");
    Set(rows[1].value, -3);
    Check(g.volume[1] == 0, "a volume below 0: 0");
    Set(rows[3].toggle, 1);
    Check(g.on[3], "a switch on");

    // ReapplyVolumes: the streamed channels, each once, with their own volume.
    std::memset(g.setVolumeCalls, 0, sizeof(g.setVolumeCalls));
    g.volume[0] = 45;
    go::ReapplyVolumes();
    Check(g.setVolumeCalls[0] == 1 && g.setVolumeCalls[1] == 1 && g.setVolumeCalls[2] == 0 &&
              g.setVolumeCalls[3] == 1 && g.volume[0] == 45,
          "ReapplyVolumes: music, speech, ambient once each; general sounds not");
}

// Restore defaults and Apply in the window: only the rows whose value changes are written.
void TestWindow()
{
    g = Fake{};
    g.speed[0] = 80; // already the default
    g.speed[1] = 60;
    co::Page page{"General", {{"Speed", {}}}};
    go::SpeedRows(page.categories[0].rows);
    go::AutosaveRows(page.categories[0].rows);
    co::Window w;
    w.SetPages({page});
    w.Open(0, 0);
    w.RestoreDefaults();
    g.writes = 0;
    w.Apply();
    Check(g.speed[0] == 80 && g.speed[1] == 50, "Restore defaults + Apply: 80 / 50");
    Check(g.writes == 1 && g.autosaveCalls == 0, "Apply wrote only the scroll speed (the rest was at its default)");
}

std::vector<std::pair<int, int>> g_written;
int NpSpeed(int which) { return g.speed[which]; }
void NpWrite(int game, int scroll)
{
    g_written.push_back({game, scroll});
    g.speed[0] = game;
    g.speed[1] = scroll;
}
bool NpPlayer() { return g.player; }

void TestNewPlayer()
{
    const int shipped[][2] = {{90, 60}, {60, 10}};
    const go::NewPlayer n(shipped, 2, &NpPlayer, &NpSpeed, &NpWrite);
    struct Case {
        bool player;
        int game, scroll;
        bool written;
        const char *what;
    } cases[] = {
        {false, 90, 60, true, "new player, the game's defaults 90 / 60: 80 / 50"},
        {false, 60, 10, true, "new player, the install's 60 / 10: 80 / 50"},
        {false, 70, 60, false, "no player file but other speeds: kept"},
        {false, 90, 10, false, "no player file, one speed shipped, the other not: kept"},
        {true, 90, 60, false, "a player file and the defaults: kept"},
        {true, 30, 20, false, "a player file and other speeds: kept"},
    };
    for (const Case &c : cases) {
        g.player = c.player;
        g.speed[0] = c.game;
        g.speed[1] = c.scroll;
        g_written.clear();
        const bool r = go::CheckNewPlayer(n);
        const bool ok = r == c.written &&
                        (c.written ? g_written.size() == 1 && g.speed[0] == 80 && g.speed[1] == 50
                                   : g_written.empty() && g.speed[0] == c.game && g.speed[1] == c.scroll);
        Check(ok, c.what);
    }
}

void TestPlayerFile()
{
    const std::wstring root = std::wstring(core_files_module_folder()) + L"test_game_options_tmp\\";
    const std::wstring save = root + L"Save\\";
    CreateDirectoryW(root.c_str(), nullptr);
    Check(!go::PlayerFileIn(root), "no Save folder: no player");
    CreateDirectoryW(save.c_str(), nullptr);
    Check(!go::PlayerFileIn(root), "an empty Save folder: no player");
    CreateDirectoryW((save + L"Folder.dat").c_str(), nullptr);
    HANDLE f = CreateFileW((save + L"dummy.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    CloseHandle(f);
    Check(!go::PlayerFileIn(root), "a folder named *.dat and another file: no player");
    f = CreateFileW((save + L"Someone.dat").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    CloseHandle(f);
    Check(go::PlayerFileIn(root), "Save\\Someone.dat: a player");
    DeleteFileW((save + L"Someone.dat").c_str());
    DeleteFileW((save + L"dummy.txt").c_str());
    RemoveDirectoryW((save + L"Folder.dat").c_str());
    RemoveDirectoryW(save.c_str());
    RemoveDirectoryW(root.c_str());
    Check(GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES, "the test folder removed");
}

} // namespace

int main()
{
    TestRows();
    TestWindow();
    TestNewPlayer();
    TestPlayerFile();
    go::DetachForTest();
    std::vector<co::Row> rows;
    go::SoundRows(rows);
    Check(rows.empty(), "after DetachForTest: no rows");
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}
