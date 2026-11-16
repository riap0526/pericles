#include "games/zeus/sound.h"

#include "core/diag/guard.h"
#include "core/log.h"

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57):
// read with Ghidra and measured from outside the game.
//   sounds_3d_on 0xf2aa98 (int): the sound options' "3D sounds" (Zeus.inf's settings block 0xf2a760 + 0x338). Only
//     config_load (its default, then Zeus.inf) and the options' button (0x402110: 1 - value) write it; game_frame
//     (0x51397d) calls the 3D sound update (0x5a24f0) and the 3D sound start (0x5a2c30) only while it is not 0.
//   sound_init 0x5a3990 (__cdecl, no arguments, ret; game_main calls it once at 0x5c3a60, after config_load at
//     0x5c380f): opens the game's sound and its 3D provider; it does not read the switch.
namespace zeus::sound {
namespace {

using zeus::Install;
using zeus::Matches;
using zeus::Original;
using zeus::Site;
using zeus::Var;

const Site kSoundInit{"sound_init", 0x5a3990, {0xb9, 0xa0, 0xbe, 0xf9, 0x00, 0xe8, 0x86, 0x40}};
const Var<int> kSounds3d{0xf2aa98}; // sounds_3d_on

using SoundInit_fn = void(__cdecl *)();
SoundInit_fn o_SoundInit = nullptr;

core::diag::Feature g_feature{"3D sounds off (games/zeus/sound)"};

void TurnOff(void *)
{
    const int was = *kSounds3d;
    *kSounds3d = 0;
    if (was != 0) {
        core_log("sound: the game's 3D sounds turned off for this run (Zeus.inf had them on): the 3D sound provider "
                 "of today's Windows (Intel RSX) waits 10 ms for each 3D sound stopped, which stopped the game about "
                 "220 ms every few seconds in the city; the game's sound options can turn them on again");
    }
    else {
        core_log("sound: the game's 3D sounds are off (as in Zeus.inf)");
    }
}

void __cdecl SoundInit_detour()
{
    core::diag::Guarded(g_feature, &TurnOff, nullptr); // a fault: the switch as Zeus.inf has it
    Original(o_SoundInit, kSoundInit)();
}

const Detour kDetours[] = {
    {&kSoundInit, reinterpret_cast<void *>(&SoundInit_detour)},
};

} // namespace

void Attach()
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

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

} // namespace zeus::sound
