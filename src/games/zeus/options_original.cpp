#include "games/zeus/options_original.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/options/game_options.h"
#include "core/platform/files.h"
#include "games/zeus/keys.h"
#include "games/zeus/resolution.h"
#include "games/zeus/site.h"

#include <algorithm>
#include <initializer_list>
#include <string>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): the game's own options dialogs read from the disassembly
// (the Sound options' switch 0x402110 and Cancel 0x402020, autosave_toggle 0x402660, the Messages dialog 0x59bbe0,
// config_load 0x5c44c0); calling conventions checked by disassembly.
namespace zeus::options_original {
namespace {

namespace go = core::options::game_options;

// The sound manager's methods (__thiscall, ecx = sound_manager 0xf9bea0): stream 0 music, 1 speech, 2 ambient.
// sound_channel_volume_set(stream, volume) pops 8 bytes; the others take one argument and pop 4.
const Site kChannelVolume{"sound_channel_volume_set", 0x5c8670, {0x8a, 0x81, 0x84, 0x00, 0x00, 0x00, 0x84, 0xc0}};
const Site kMusicVolume{"sound_music_volume_set", 0x5c8480, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}};
const Site kSpeechVolume{"sound_speech_volume_set", 0x5c8440, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}};
const Site kAmbientVolume{"sound_ambient_volume_set", 0x5c84c0, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}};
const Site kChannelStop{"sound_channel_stop", 0x5c85c0, {0x56, 0x57, 0x8b, 0xf9, 0x8a, 0x87, 0x84, 0x00}};
// sound_end_all_3d (__thiscall, ecx = the ambient layers 0xf3d210, no arguments): the Sound options' switch
// 0x402110 calls it when the ambient sounds go off while the 3D sounds are on.
const Site kEndAll3d{"sound_end_all_3d", 0x5a24a0, {0x53, 0x8b, 0xd9, 0x55, 0x57, 0x8b, 0x83, 0x10}};
// autosave_toggle (__cdecl, no arguments): flips autosave_off and the menu item's text.
const Site kAutosaveToggle{"autosave_toggle", 0x402660, {0xa1, 0xf4, 0xa7, 0xf2, 0x00, 0x85, 0xc0, 0x74}};
// message_kinds_count (__cdecl, no arguments): min([0xf284a4], 20), the number of kinds the Messages dialog shows.
const Site kMessageKindsCount{"message_kinds_count", 0x59bbd0, {0xa1, 0xa4, 0x84, 0xf2, 0x00, 0x83, 0xf8, 0x14}};
// config_load 0x5c44c0 (__cdecl, no arguments; eax; ret): writes the options' defaults, then reads Zeus.inf over them.
// Hooked by games/zeus/resolution when it is installed (it then calls us after the original); hooked here otherwise.
const Site kConfigLoad{"config_load", 0x5c44c0, {0x53, 0x55, 0x56, 0xbb, 0x01, 0x00, 0x00, 0x00}};

using Volume2_fn = void(__thiscall *)(void *, int, int);
using Volume1_fn = void(__thiscall *)(void *, int);
using Void_fn = void(__cdecl *)();
using This_fn = void(__thiscall *)(void *);
using Config_fn = int(__cdecl *)();

const DWORD kSoundManager = 0xf9bea0, kAmbientLayers = 0xf3d210;

const Var<int> kGameSpeed{0xf2a774};   // 10..100 in steps of 10 (game_speed_step 0x402430); no lock, no kept speed
const Var<int> kScrollSpeed{0xf2a778}; // 0..100 in steps of 10 (scroll_speed_step 0x4024a0)
const Var<int> kAutosaveOff{0xf2a7f4}; // not 0: no autosave (sim_after_tick)
const Var<int> kSounds3d{0xf2aa98};    // sounds_3d_on (our sound layer keeps it 0)
const Var<int> kMessageKinds{0xf284a4}; // read by message_kinds_count only; written at run time (17 in both games)
const DWORD kMessages = 0xf2a7fc;        // one byte per kind: 0 Off, 1 Condensed, 2 Full (20 of them)
const DWORD kMessageDefaults = 0x916ae4; // the kinds' defaults from the message categories file (0x466b80)
constexpr int kMaxMessageKinds = 20;
// message_kind_cycle 0x4026c0 always sets kind 19 to Full, and 0x474f60 treats it as Full: no row for it.
constexpr int kFixedMessageKind = 19;
constexpr int kTextMessageKinds = 174, kTextMessageModes = 183; // Zeus_Text.eng groups
// lang_get_string's table of groups (8 bytes each): +0 the group's offset in the text, +4 read here as its count of
// strings. lang_get_string does not check the index: past the group's strings it goes on into the next group.
constexpr DWORD kTextGroups = 0xf27f30;

// Per channel: the switch (a byte), the volume (an int 0..100), the sound manager's stream (-1: none, general sounds
// are read when they play) and its own volume method.
struct ChannelSite {
    DWORD on, volume;
    int stream;
    const Site *setter;
};
const ChannelSite kChannelSites[go::kChannelCount] = {
    {0xf2a76c, 0xf2a7d0, 0, &kMusicVolume},
    {0xf2a76d, 0xf2a7d4, 1, &kSpeechVolume},
    {0xf2a76b, 0xf2a7cc, -1, nullptr},
    {0xf2a7c8, 0xf2a7d8, 2, &kAmbientVolume},
};
// Defaults: config_load (the same as Emperor's).
const go::Channel kChannels[go::kChannelCount] = {
    {"music", 50, true},
    {"speech", 100, true},
    {"sound_effects", 80, false},
    {"ambient_volume", 80, true},
};

bool g_ok = false;

volatile BYTE &Byte(DWORD address)
{
    return *Var<BYTE>{address};
}
volatile int &Int(DWORD address)
{
    return *Var<int>{address};
}

const char *Text(int group, int index)
{
    const char *s = zeus::keys::GameText(group, index);
    return s ? s : "(no text)";
}

int Speed(int which)
{
    return which == 0 ? *kGameSpeed : *kScrollSpeed;
}

void SetSpeed(int which, int v)
{
    if (which == 0) {
        *kGameSpeed = v; // game_speed_step 0x402430 writes only this
    }
    else {
        *kScrollSpeed = v;
    }
}

bool Autosave()
{
    return *kAutosaveOff == 0;
}

void SetAutosave(bool)
{
    Function<Void_fn>(kAutosaveToggle)(); // the core calls it only when the switch differs
}

int MessageKinds()
{
    return std::clamp(static_cast<int>(*kMessageKinds), 0, kMaxMessageKinds); // as message_kinds_count
}

std::string MessageName(int kind)
{
    const char *s = zeus::keys::GameText(kTextMessageKinds, kind);
    return s ? s : "";
}

std::string MessageModeName(int mode)
{
    const char *s = zeus::keys::GameText(kTextMessageModes, mode);
    return s ? s : "";
}

int Message(int kind)
{
    return Byte(kMessages + kind);
}

void SetMessage(int kind, int value)
{
    Byte(kMessages + kind) = static_cast<BYTE>(value);
}

int MessageDefault(int kind)
{
    return Byte(kMessageDefaults + kind);
}

bool SoundOn(int i)
{
    return Byte(kChannelSites[i].on) != 0;
}

void SetSoundOn(int i, bool on)
{
    const ChannelSite &ch = kChannelSites[i];
    Byte(ch.on) = on ? 1 : 0;
    if (on || ch.stream < 0) { // the Sound options' switch 0x402110: switching on only sets it
        return;
    }
    Function<Volume1_fn>(kChannelStop)(reinterpret_cast<void *>(kSoundManager), ch.stream);
    if (ch.stream == 2 && *kSounds3d != 0) {
        Function<This_fn>(kEndAll3d)(reinterpret_cast<void *>(kAmbientLayers));
    }
}

int Volume(int i)
{
    return Int(kChannelSites[i].volume);
}

void SetVolume(int i, int v)
{
    const ChannelSite &ch = kChannelSites[i];
    Int(ch.volume) = v;
    if (ch.stream >= 0) { // as the Sound options' Cancel 0x402020 hands the volumes on
        Function<Volume2_fn>(kChannelVolume)(reinterpret_cast<void *>(kSoundManager), ch.stream, v);
        Function<Volume1_fn>(*ch.setter)(reinterpret_cast<void *>(kSoundManager), v);
    }
}

const go::GameOptions kGame(&Speed, &SetSpeed, &Autosave, &SetAutosave, &MessageKinds, &MessageName,
                            &MessageModeName, &Message, &SetMessage, &MessageDefault, kFixedMessageKind, kChannels,
                            &SoundOn, &SetSoundOn, &Volume, &SetVolume);

// A new player: config_load's defaults and the GOG and Steam installs' Zeus.inf are both 90 / 60.
const int kShipped[][2] = {{90, 60}};

bool HasPlayer()
{
    return go::PlayerFileIn(core_files_module_folder());
}

void WriteSpeeds(int game, int scroll)
{
    *kGameSpeed = game;
    *kScrollSpeed = scroll;
}

const go::NewPlayer kNewPlayer(kShipped, 1, &HasPlayer, &Speed, &WriteSpeeds);

// --- Difficulty and auto defend (core/options/game_options Difficulty).
// difficulty_step 0x401ee0 (__cdecl, no arguments; ret): button_param 0 one level up, else one down, clamped to 0..4,
// then the game's data for the level read again and screen_redraw set. It is the difficulty dialog's arrows' handler;
// the dialog itself (screen 0x45) lowers difficulty_lowest to the difficulty when it closes (0x515530..0x51555a).
const Site kDifficultyStep{"difficulty_step", 0x401ee0, {0xa1, 0xf8, 0x35, 0xf1, 0x00, 0x85, 0xc0, 0xa1}};
// auto_defend_toggle 0x5692d0 (__cdecl, no arguments; ret): flips auto_defend unless enemies are attacking and it is
// off, writes the Options menu's ON / OFF text number and keeps the drop-down open (0x781888).
const Site kAutoDefendToggle{"auto_defend_toggle", 0x5692d0, {0xa1, 0x84, 0xb0, 0xfe, 0x00, 0x85, 0xc0, 0xa0}};

const Var<int> kDifficulty{0xf2a988};       // 0..4; Zeus.inf and the saved games keep it
const Var<int> kDifficultyLowest{0xfeb1f8}; // the lowest difficulty of this mission (its result reads it)
const Var<int> kButtonParam{0xf135f8};      // the pressed button's parameter (difficulty_step reads it)
const Var<BYTE> kAutoDefend{0xf2a990};      // 0 / 1; Zeus.inf and the saved games keep it
const Var<int> kInvasionA{0xfeb084}, kInvasionB{0xfeb3ec}; // not 0: enemies attacking (auto_defend_toggle's test)
const Var<BYTE> kGameScreen{0xf2a6dd};
const Var<int> kEditorMode{0x665e2c}; // 1 in the editors (their city map is screen 0x01 too)
constexpr int kTextDifficulty = 153;  // Zeus_Text.eng: 1..5 Beginner .. Olympian

int Difficulty()
{
    return *kDifficulty;
}
int DifficultyLowest()
{
    return *kDifficultyLowest;
}
void SetDifficultyLowest(int v)
{
    *kDifficultyLowest = v;
}
int ButtonParam()
{
    return *kButtonParam;
}
void SetButtonParam(int v)
{
    *kButtonParam = v;
}
void __cdecl DifficultyStep()
{
    Function<Void_fn>(kDifficultyStep)();
}
std::string LevelName(int level)
{
    return Text(kTextDifficulty, level + 1);
}
bool AutoDefend()
{
    return *kAutoDefend != 0;
}
void ToggleAutoDefend()
{
    Function<Void_fn>(kAutoDefendToggle)(); // the core calls it only with no enemies attacking: it always flips
}
bool UnderAttack()
{
    return *kInvasionA != 0 || *kInvasionB != 0;
}
// The city being played: the city screen, or with a ship (0x20) or a company (0x48) selected -- the screens where Esc
// opens the pause menu (games/zeus/game_menu); the Options menu puts the city screen back before calling its items.
bool InCity()
{
    const int s = *kGameScreen;
    return *kEditorMode != 1 && (s == 0x01 || s == 0x20 || s == 0x48);
}
// Zeus has no multiplayer game.
bool AlwaysSinglePlayer()
{
    return true;
}

const go::Difficulty kDifficultyRows(&Difficulty, &DifficultyLowest, &SetDifficultyLowest, &ButtonParam,
                                     &SetButtonParam, &DifficultyStep, &LevelName,
                                     1, // Mortal: config_load's default
                                     &AutoDefend, &ToggleAutoDefend, &UnderAttack, &InCity, &AlwaysSinglePlayer);

void AttachDifficultyRows()
{
    for (const Site *s : {&kDifficultyStep, &kAutoDefendToggle}) {
        if (!Matches(*s)) {
            core_log("options window: %s at 0x%lx does not match; no Difficulty and Auto defend rows (the Options "
                     "menu keeps the game's own)",
                     s->name, static_cast<unsigned long>(s->address));
            return;
        }
    }
    go::AttachDifficulty(kDifficultyRows);
    core_log("options window: the game's difficulty and auto defend found");
}

core::diag::Feature g_newPlayer{"new player's speeds (games/zeus/options_original)"};
bool g_checked = false;

Config_fn o_ConfigLoad;

int __cdecl ConfigLoad_detour()
{
    const int r = Original(o_ConfigLoad, kConfigLoad)();
    AfterInfRead();
    return r;
}

} // namespace

void AfterInfRead()
{
    if (g_checked || !core::diag::Enabled(g_newPlayer)) {
        return;
    }
    g_checked = true;
    core::diag::Guarded(g_newPlayer, [](void *) { go::CheckNewPlayer(kNewPlayer); }, nullptr);
}

bool Attach()
{
    // A new player's speeds, once Zeus.inf has been read (whether or not the rows can be).
    if (zeus::resolution::ConfigLoadHooked()) {
        zeus::resolution::SetAfterConfigLoad(&AfterInfRead);
    }
    else if (Matches(kConfigLoad)) {
        Install(kConfigLoad, &ConfigLoad_detour, o_ConfigLoad);
    }
    else {
        core_log("options window: %s at 0x%lx does not match: a new player starts at the game's own speeds",
                 kConfigLoad.name, static_cast<unsigned long>(kConfigLoad.address));
    }
    const Site *sites[] = {&kChannelVolume, &kMusicVolume,    &kSpeechVolume,     &kAmbientVolume,
                           &kChannelStop,   &kEndAll3d,       &kAutosaveToggle,   &kMessageKindsCount};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("options window: %s at 0x%lx does not match; the game's own options are left out", s->name,
                     static_cast<unsigned long>(s->address));
            return false;
        }
    }
    if (!zeus::keys::LangHooked()) {
        core_log("options window: lang_get_string is not hooked; the game's own options are left out");
        return false;
    }
    g_ok = true;
    go::Attach(kGame);
    core_log("options window: the game's own options found");
    AttachDifficultyRows();
    return true;
}

void AttachForTest()
{
    g_ok = true;
    go::Attach(kGame);
    go::AttachDifficulty(kDifficultyRows);
}

void ReapplyVolumes()
{
    go::ReapplyVolumes();
}

void LogMessageKinds()
{
    if (!g_ok) {
        core_log("options window: message kinds: not attached");
        return;
    }
    const int raw = *kMessageKinds;
    const int shown = std::min(raw, kMaxMessageKinds);
    const int names = *Var<int>{kTextGroups + 8 * kTextMessageKinds + 4};
    core_log("options window: message kinds: 0x%lx = %d (the Messages dialog shows %d); group %d has %d texts",
             static_cast<unsigned long>(kMessageKinds.address), raw, shown, kTextMessageKinds, names);
    const volatile BYTE *values = Var<BYTE>{kMessages}.ptr();
    const volatile BYTE *defaults = Var<BYTE>{kMessageDefaults}.ptr();
    for (int i = 0; i < kMaxMessageKinds; ++i) {
        core_log("options window: message kind %d: value %d, default %d, \"%s\"", i, values[i], defaults[i],
                 Text(kTextMessageKinds, i));
    }
    for (int k = 0; k < 3; ++k) {
        core_log("options window: message mode %d: \"%s\"", k, Text(kTextMessageModes, k));
    }
}

} // namespace zeus::options_original
