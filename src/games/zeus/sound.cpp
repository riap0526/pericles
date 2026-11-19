#include "games/zeus/sound.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/sound/layer1.h"
#include "core/sound/master_volume.h"
#include "core/sound/sound.h"

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57):
// read with Ghidra and measured from outside the game.
//   sounds_3d_on 0xf2aa98 (int): the sound options' "3D sounds" (Zeus.inf's settings block 0xf2a760 + 0x338). Only
//     config_load (its default, then Zeus.inf) and the options' button (0x402110: 1 - value) write it; game_frame
//     (0x51397d) calls the 3D sound update (0x5a24f0) and the 3D sound start (0x5a2c30) only while it is not 0.
//   sound_init 0x5a3990 (__cdecl, no arguments, ret; game_main calls it once at 0x5c3a60, after config_load at
//     0x5c380f): opens the game's sound and its 3D provider; it does not read the switch.
// For the core's sound (calling conventions and layouts checked by
// disassembly):
//   ambient_sound_random 0x5a4790: __cdecl, no arguments, ret; game_frame's only call 0x513978, on the city screen
//     when rand() * [0xf42b2c] >> 15 == 1.
//   sound_play 0x5c80b0: __thiscall (ecx = sound_manager 0xf9bea0), one stack argument (the request 0xf9bef0: +4
//     sound, +8 volume, +0xc pan, +0x10 loops), ret 4; returns the voice or -1. Objects on the screen get pan
//     x * 128.0 / view width (0x5a6fe0), so 128 at the right edge.
//   sound_stop_all 0x5a1330: __cdecl, no arguments, ret; closes streams 0, 2, 3, 1 (0x5c85c0), ends every ordinary
//     and 3D voice (0x5c7dc0), writes -1 to 0x10e4220 (written only here and in sound_init, read nowhere: the pause
//     leaves it). Called by game_wndproc on WM_KILLFOCUS (returns to 0x5c4bc2) and on WM_ACTIVATE minimized (returns
//     to 0x5c4bec), and by game_main when the game ends (0x5c399f, returns to 0x5c39a4: left alone).
//   sound_manager 0xf9bea0: +0x2c the 4 stream handles, +0x3c the ordinary voices' count, +0xd8 a pointer to their
//     HSAMPLE array (0x5c7d40: mov ecx, [ecx+0xd8]; mov eax, [ecx+eax*4]), +0x84 a byte: the sound off.
//   music_state 0xf97000 (music_update 0x5a34c0, ecx = 0xf97000): +0x4e40 the mode; in mode 1 the start time +0x4e8c
//     (GetTickCount, 0x5a35e3) and the length +0x4e90: when GetTickCount passes start + length, mode 3 (0x5a35fb).
//   ambient_frequency_l2 0xf42b2c: FREQUENCY_L2 of Audio\sound_fxU.txt (300), written back at every city load
//     (0x5a1a56); game_screen 0xf2a6dd (1: the city).
// The layer-1 ambient sounds are ours (core/sound/layer1); the game's
// own way (its 3D voices on the RSX provider) stays off.
//   sound_started 0x10e4230 (int, sound_init writes it); the digital driver: sound_manager + 0xb8 (sound_init passes
//     its address to _AIL_waveOutOpen); ambient_sounds_on 0xf2a7c8 (byte) and ambient_volume 0xf2a7d8 (int, 0..100):
//     the game's own layer 1 starts only while the switch is on and plays at that volume (0x5139cd, 0x5a2c30).
//   sound_options_toggle 0x402110: __cdecl, no arguments, plain ret; called only through
//     the sound options dialog's button tables (.rdata 0x5ed5d8, 0x5ed5f4, 0x5ed610, 0x5ed62c, 0x5ed680; no direct
//     call). Its button 5 (0xf135f8 == 5) flips sounds_3d_on: put back to 0 at once after it.
namespace zeus::sound {
namespace {

using zeus::Install;
using zeus::Matches;
using zeus::Original;
using zeus::Site;
using zeus::Var;

const Site kSoundInit{"sound_init", 0x5a3990, {0xb9, 0xa0, 0xbe, 0xf9, 0x00, 0xe8, 0x86, 0x40}};
const Site kAmbient{"ambient_sound_random", 0x5a4790, {0xa1, 0x30, 0x42, 0x0e, 0x01, 0x83, 0xec, 0x28}};
const Site kPlay{"sound_play", 0x5c80b0, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x8b, 0xf1}};
const Site kStopAll{"sound_stop_all", 0x5a1330, {0x6a, 0x00, 0xb9, 0xa0, 0xbe, 0xf9, 0x00, 0xe8}};
const Site kButton{"sound_options_toggle", 0x402110, {0xa1, 0xf8, 0x35, 0xf1, 0x00, 0x53, 0xbb, 0x01}};
constexpr DWORD kFromKillFocus = 0x5c4bc2, kFromMinimized = 0x5c4bec;

const Var<int> kSounds3d{0xf2aa98}; // sounds_3d_on
const Var<int> kSoundStarted{0x10e4230};
const Var<char> kAmbientOn{0xf2a7c8};
const Var<int> kAmbientVolume{0xf2a7d8};
const Var<int> kAmbientN{0xf42b2c}; // ambient_frequency_l2
const Var<char> kGameScreen{0xf2a6dd};
constexpr DWORD kManager = 0xf9bea0;
const Var<int> kMusicMode{0xf97000 + 0x4e40}; // 1 = music for a while (start, length)
const Var<DWORD> kMusicStart{0xf97000 + 0x4e8c};
constexpr int kStreams = 4;

using SoundInit_fn = void(__cdecl *)();
using Void_fn = void(__cdecl *)();
using Play_fn = int(__fastcall *)(void *, void *, void *); // __thiscall: ecx = the manager, ret 4
SoundInit_fn o_SoundInit = nullptr;
Void_fn o_Ambient = nullptr, o_StopAll = nullptr, o_Button = nullptr;
Play_fn o_Play = nullptr;

core::diag::Feature g_feature{"3D sounds off (games/zeus/sound)"};

BYTE *Manager()
{
    return reinterpret_cast<BYTE *>(kManager + g_imageShift);
}

// The game's sound manager for core/sound. The game's 3D voices (its layer-1 ambience, looping) are not paused by the
// core: while the game's 3D sounds are on, switching away stops everything as the game does (they are kept off).
// Our layer-1 player is the core's: core/sound pauses and stops it itself.
const core::sound::SoundGame kGame(
    "zeus", [] { return *reinterpret_cast<volatile BYTE *>(Manager() + 0x84) != 0; },
    [](int i) { return reinterpret_cast<void **>(Manager() + 0x2c + 4 * i); }, kStreams,
    [] { return *reinterpret_cast<volatile int *>(Manager() + 0x3c); },
    [] { return *reinterpret_cast<void **volatile *>(Manager() + 0xd8); }, [] { return *kSounds3d != 0; },
    [] { return *kMusicMode == 1; }, [](unsigned long ms) { *kMusicStart = *kMusicStart + ms; },
    &core::sound::NoOwnPause, &core::sound::NoOwnStop, [] { return &*kAmbientN; }, [] { return *kGameScreen == 1; });

// The layer-1 ambient sounds as the game plays them: one starts on the city screen while its ambient sounds are on
// (0x5139c4..0x5139cd) and plays on over dialogs (sound_frame_update does not look at the screen), at their volume;
// all 21 of the file. The game may pick the same one twice in a row (rand() * count >> 15, 0x5139d5); ours does not,
// as Yongle's (it sounds more natural).
const core::sound::layer1::Layer1Game kLayer1(
    "zeus", [] { return *kSoundStarted != 0; }, [] { return *reinterpret_cast<void *volatile *>(Manager() + 0xb8); },
    [] { return *kGameScreen == 1 && *kAmbientOn != 0; }, [] { return *kAmbientOn != 0; },
    [] { return *kAmbientVolume; }, nullptr, 0, false);

void TurnOff(void *)
{
    const int was = *kSounds3d;
    *kSounds3d = 0;
    if (was != 0) {
        core_log("sound: the game's 3D sounds turned off for this run (Zeus.inf had them on): the 3D sound provider "
                 "of today's Windows (Intel RSX) waits 10 ms for each 3D sound stopped, which stopped the game about "
                 "220 ms every few seconds in the city; our nature sounds play them instead");
    }
    else {
        core_log("sound: the game's 3D sounds are off (as in Zeus.inf); our nature sounds play them instead");
    }
}

// Kept off: the sound options' "3D sounds" button turns the game's own way on again (and its stops), so it is put
// back to 0 at once after the button, and on every pass in case something else writes it.
void KeepOff(void *why)
{
    if (*kSounds3d != 0) {
        *kSounds3d = 0;
        core_log("sound: the game's 3D sounds turned on (%s): put back to off (our nature sounds play them)",
                 static_cast<const char *>(why));
    }
}

void __cdecl Button_detour()
{
    Original(o_Button, kButton)();
    core::diag::Guarded(g_feature, &KeepOff, const_cast<char *>("the sound options' button"));
}

void __cdecl SoundInit_detour()
{
    core::diag::Guarded(g_feature, &TurnOff, nullptr); // a fault: the switch as Zeus.inf has it
    Original(o_SoundInit, kSoundInit)();
}

void __cdecl StopAll_detour()
{
    const DWORD from = GameAddress(__builtin_return_address(0));
    if (core::sound::StopAll(from == kFromKillFocus || from == kFromMinimized)) {
        return;
    }
    Original(o_StopAll, kStopAll)();
}

void __cdecl Ambient_detour()
{
    if (!core::sound::AmbientBegin()) {
        return;
    }
    Original(o_Ambient, kAmbient)();
    core::sound::AmbientEnd();
}

int __fastcall Play_detour(void *self, void *, void *request)
{
    core::sound::BeforePlay(static_cast<int *>(request) + 3); // +0xc
    const int voice = Original(o_Play, kPlay)(self, nullptr, request);
    core::sound::AfterPlay(voice);
    return voice;
}

const Detour kDetours[] = {
    {&kSoundInit, reinterpret_cast<void *>(&SoundInit_detour)},
    {&kStopAll, reinterpret_cast<void *>(&StopAll_detour)},
    {&kAmbient, reinterpret_cast<void *>(&Ambient_detour)},
    {&kPlay, reinterpret_cast<void *>(&Play_detour)},
    {&kButton, reinterpret_cast<void *>(&Button_detour)},
};

void AttachSoundInit()
{
    if (!Matches(kSoundInit)) {
        core_log("sound: %s does not match: the game's 3D sounds as Zeus.inf has them", kSoundInit.name);
        return;
    }
    if (!Install(kSoundInit, &SoundInit_detour, o_SoundInit)) {
        core_log("sound: %s could not be hooked: the game's 3D sounds as Zeus.inf has them", kSoundInit.name);
        return;
    }
    core_log("sound: installed (the game's 3D sounds off at start, before its sound opens)");
}

void AttachButton()
{
    if (!Matches(kButton)) {
        core_log("sound: %s does not match: its 3D sounds button put back to off on the next pass", kButton.name);
        return;
    }
    if (!Install(kButton, &Button_detour, o_Button)) {
        core_log("sound: %s could not be hooked: its 3D sounds button put back to off on the next pass",
                 kButton.name);
    }
}

} // namespace

void RegisterSettings()
{
    core::sound::master_volume::RegisterSettings(); // first in [audio]: the volume over all
    core::sound::RegisterSettings(); // [audio] background_sound, ambient_sounds; [debug] sound_probe
    core::sound::layer1::RegisterSettings("The original game calls them 3D sounds.");
}

void OnLoop()
{
    if (core::diag::Enabled(g_feature)) {
        core::diag::Guarded(g_feature, &KeepOff, const_cast<char *>("found on a pass"));
    }
    core::sound::OnLoop();
}

void Attach()
{
    AttachSoundInit();
    AttachButton();
    core::sound::layer1::Attach(kLayer1);
    bool probe = false, pause = false;
    if (!Matches(kAmbient) || !Matches(kPlay)) {
        core_log("sound: %s does not match: no counts, no pan limit, ambient sounds also under our windows",
                 Matches(kAmbient) ? kPlay.name : kAmbient.name);
    }
    else if (!Install(kAmbient, &Ambient_detour, o_Ambient) || !Install(kPlay, &Play_detour, o_Play)) {
        core_log("sound: ambient_sound_random or sound_play could not be hooked: no counts, no pan limit");
    }
    else {
        probe = true;
    }
    if (!Matches(kStopAll)) {
        core_log("sound: %s does not match: sounds stop when switched away, as in the game", kStopAll.name);
    }
    else if (!Install(kStopAll, &StopAll_detour, o_StopAll)) {
        core_log("sound: %s could not be hooked: sounds stop when switched away, as in the game", kStopAll.name);
    }
    else {
        pause = true;
    }
    core::sound::Attach(kGame, pause, probe);
    // The master volume: before game_main, so every volume the game sets goes through it. Zeus sets its stream
    // volumes through _AIL_set_stream_volume on live handles only, all of them in the master volume's list, so the
    // game needs to set nothing again when ours changes [inferred: checked by ear in the test].
    static const core::sound::master_volume::VolumeGame kVolume(&core::sound::master_volume::NoGameVolumes);
    core::sound::master_volume::Attach(kVolume);
}

void AttachForTest()
{
    core::sound::Attach(kGame, true, true);
    core::sound::layer1::Attach(kLayer1);
}

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

} // namespace zeus::sound
