#include "games/zeus/smoke.h"

#include "core/capture/city_picture.h"
#include "core/log.h"
#include "core/ui/notice.h"
#include "games/zeus/site.h"

#include <cstdio>
#include <cstring>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57; Ghidra and
// the exe's bytes).
namespace zeus::smoke {
namespace {

// cheat_run: one argument [esp+4] (the table index), returns al, plain ret: __cdecl. 0..99: the cheat count
// 0xfeaef8 + 1, then the handler of entry n of the table chosen by 0x661510 (1 in the exe, only ever read:
// cheat_table 0x5d8730, entry 0 Ambrosia = cheat_ambrosia 0x42c560), called with no arguments; returns 1.
// Checked before the call: cheat_run's bytes after its first 8 (cmp eax, 100; jge; mov edx, [0xfeaef8]), the table
// choice, entry 0's handler and cheat_ambrosia's bytes (mov al, 1; mov [0xf2a6db], al; mov [0xf2a6da], al). Read
// through At (the offline test's copy of the game; its fake jumps from cheat_run's start).
const Site kCheatRun{"cheat_run", 0x42c5a0, {}};
constexpr BYTE kCheatRunBody[8] = {0x83, 0xf8, 0x64, 0x7d, 0x3d, 0x8b, 0x15, 0xf8}; // at 0x42c5a8
constexpr DWORD kCheatRunBodyAt = 0x42c5a8;
using CheatRun_fn = char(__cdecl *)(int);
const Var<char> kCheatTableChoice{0x661510};
constexpr DWORD kCheatTableHandler0 = 0x5d8734; // cheat_table entry 0's handler
constexpr DWORD kCheatAmbrosia = 0x42c560;
constexpr BYTE kAmbrosiaCode[8] = {0xb0, 0x01, 0xa2, 0xdb, 0xa6, 0xf2, 0x00, 0xa2};

bool Same(DWORD address, const BYTE (&expect)[8])
{
    return std::memcmp(At<BYTE>(address), expect, sizeof(expect)) == 0;
}

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kContinueGoverning{0xfeaf28};  // continue_governing_on: mission_check returns at once while set
const Var<char> kCheatWin{0xf2a6db};
const Var<int> kLoseReason{0xfeb160};          // -1: not lost (4 bytes: mov dword)
const Var<short> kLoseMessage{0xfeb054};       // objectives_check: 0xb91 (2 bytes: 66 c7 05 / 66 a3)
const Var<short> kLoseMessage2{0xfeb056};      // objectives_check: 0xb90 (2 bytes)
const Var<int> kDialogX{0xf2a6ac}, kDialogY{0xf2a6a8};
const Var<short> kMouseX{0xf2a70e}, kMouseY{0xf2a70c};
const Var<unsigned char> kMouseButtons{0xf2a70b};
const Var<char> kLeftReleased{0xf2a708}, kRightReleased{0xf2a6fc};
const Var<int> kEdgeScrolling{0xf2a6d0};
constexpr int kCityScreen = 0x01;

bool InCity(const char *name, char *why, int size)
{
    if (*kGameScreen != kCityScreen) {
        std::snprintf(why, size, "%s: not on the city screen (screen 0x%02x)", name, *kGameScreen);
        return false;
    }
    if (*kContinueGoverning != 0) {
        std::snprintf(why, size, "%s: the city is governed on after a won mission (0xfeaf28 = %d): mission_check "
                                 "ignores wins and losses", name, *kContinueGoverning);
        return false;
    }
    return true;
}

bool Win(char *why, int size)
{
    if (!InCity("win", why, size)) {
        return false;
    }
    if (!Same(kCheatRunBodyAt, kCheatRunBody) || *kCheatTableChoice == 0 ||
        *At<DWORD>(kCheatTableHandler0) != kCheatAmbrosia || !Same(kCheatAmbrosia, kAmbrosiaCode)) {
        std::snprintf(why, size, "win: cheat_run or the cheat table is not as expected (not the tested exe?)");
        return false;
    }
    const char ok = Function<CheatRun_fn>(kCheatRun)(0);
    core_log("zeus: smoke: win: cheat_run(0) (Ambrosia) returned %d, cheat_win %d; mission_check takes it at the end "
             "of the month", static_cast<int>(ok), static_cast<int>(*kCheatWin));
    if (!ok || !*kCheatWin) {
        std::snprintf(why, size, "win: cheat_run(0) did not set cheat_win");
        return false;
    }
    return true;
}

bool Lose(char *why, int size)
{
    if (!InCity("lose", why, size)) {
        return false;
    }
    if (*kLoseReason >= 0) {
        std::snprintf(why, size, "lose: already lost (lose_reason 0x%x)", *kLoseReason);
        return false;
    }
    *kLoseReason = 0xb91;
    *kLoseMessage = 0xb91;
    *kLoseMessage2 = 0xb90;
    core_log("zeus: smoke: lose: lose_reason = 0xb91 (in debt too long), as objectives_check writes it; "
             "mission_check takes it on its next two monthly checks");
    return true;
}

} // namespace

const char kActions[] =
    "win lose notice notice_long notice_bracket notice_red zoom_50 zoom_75 zoom_125 picture_city picture_minimap";

bool Action(const char *name, char *why, int size)
{
    // The notice tests: texts for the smoke test only, never shown otherwise.
    static const struct {
        const char *name, *text;
        bool red;
    } kNotices[] = {
        {"notice", "Pericles test notice.", false},
        {"notice_long", "Minimap screenshot saved in the shots folder.", false},
        {"notice_bracket", "Bracket test: [city] and [unclosed", false},
        {"notice_red", "Red test: Zeus shows it in the plain font.", true},
    };
    for (const auto &n : kNotices) {
        if (std::strcmp(name, n.name) == 0) {
            core::ui::notice::Post(n.text, n.red);
            return true;
        }
    }
    static const struct {
        const char *name;
        int percent;
    } kZooms[] = {{"zoom_50", 50}, {"zoom_75", 75}, {"zoom_125", 125}};
    for (const auto &z : kZooms) {
        if (std::strcmp(name, z.name) == 0) {
            core::ui::notice::Zoom(z.percent);
            return true;
        }
    }
    // city_picture: as the keys F10 and Shift+F10 (the picture is drawn at
    // a later city_render; the log and shots\ show the result, e.g. a colony's name in the file name).
    if (std::strcmp(name, "picture_city") == 0) {
        core::capture::city_picture::RequestCity();
        return true;
    }
    if (std::strcmp(name, "picture_minimap") == 0) {
        core::capture::city_picture::RequestMinimap();
        return true;
    }
    if (std::strcmp(name, "win") == 0) {
        return Win(why, size);
    }
    if (std::strcmp(name, "lose") == 0) {
        return Lose(why, size);
    }
    std::snprintf(why, size, "no such action");
    return false;
}

void DialogOffset(int *x, int *y)
{
    *x = *kDialogX;
    *y = *kDialogY;
}

void Describe(char *out, int size)
{
    std::snprintf(out, size, "screen 0x%02x, game mouse (%d,%d) buttons %d released L%d R%d, edge scrolling %d, dialog "
                             "offset (%d,%d)",
                  *kGameScreen, *kMouseX, *kMouseY, *kMouseButtons, *kLeftReleased, *kRightReleased, *kEdgeScrolling,
                  *kDialogX, *kDialogY);
}

} // namespace zeus::smoke
