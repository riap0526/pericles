#include "games/zeus/scroll_arrows.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/ui/scroll_repeat.h"
#include "games/zeus/site.h"

#include <windows.h>
#include <mmsystem.h>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): buttons_update 0x4319c0
// (__cdecl x, y, table, count; plain ret, the callers pop 0x10) acts on a type-6 button at every call while the left
// button is held on it (0x431b63..0x431b76), with no timer; its press path (0xf2a707) only remembers the button and
// its release path (0xf2a708) acts once. 0x431bc0 treats types 4 and 6 alike (the press effect's countdown).
// input_update_mouse 0x50b470: mouse_left_held 0xf2a704 = bit 0 of the buttons (0x50b618); mouse_left_pressed 0xf2a707
// = 1 when it went from 0 to 1 (0x50b66d).
namespace zeus::scroll_arrows {
namespace {

namespace sr = core::ui::scroll_repeat;

const Site kButtonsUpdate{"buttons_update", 0x4319c0, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x1c, 0x56}};
using ButtonsUpdate_fn = int(__cdecl *)(int x, int y, void *table, int count);
ButtonsUpdate_fn o_ButtonsUpdate = nullptr;

const Var<char> kLeftHeld{0xf2a704};
const Var<char> kLeftPressed{0xf2a707};
const Var<int> kMouseX{0xf2a738}, kMouseY{0xf2a734};

// The image button: 0x28 bytes; x, y, w, h words at 0..6, the type word at 8, the state byte at 0x18 (0 and 2 take no
// left click: 0x431a99..0x431aa3).
const sr::Layout kLayout(0x28, 0, 2, 4, 6, 8, 0x18);

core::diag::Feature g_feature{"held scroll arrows (games/zeus/scroll_arrows)"};
sr::Gate g_gate;

struct Call {
    int x, y;
    void *table;
    int count;
    sr::Changed changed;
};

void PrepareNow(void *p)
{
    Call &c = *static_cast<Call *>(p);
    const sr::Input in{*kLeftPressed != 0, *kLeftHeld != 0, *kMouseX, *kMouseY};
    sr::Prepare(kLayout, c.table, c.count, c.x, c.y, in, g_gate, timeGetTime(), c.changed);
}

int __cdecl ButtonsUpdate_detour(int x, int y, void *table, int count)
{
    Call c{x, y, table, count, {}};
    if (core::diag::Enabled(g_feature) && !core::diag::Guarded(g_feature, &PrepareNow, &c)) {
        sr::Restore(c.changed); // what it changed before the fault (types only); then the game's own way
        core_log("scroll arrows: off after an error; held scroll arrows act as the game has them");
    }
    const int r = Original(o_ButtonsUpdate, kButtonsUpdate)(x, y, table, count);
    sr::Restore(c.changed);
    return r;
}

} // namespace

void Attach()
{
    if (!Matches(kButtonsUpdate)) {
        core_log("scroll arrows: buttons_update at 0x%lx does not match; held scroll arrows act as the game has them",
                 kButtonsUpdate.address);
        return;
    }
    if (!Install(kButtonsUpdate, &ButtonsUpdate_detour, o_ButtonsUpdate)) {
        core_log("scroll arrows: buttons_update could not be hooked; held scroll arrows act as the game has them");
        return;
    }
    core_log("scroll arrows: held scroll arrows scroll a line every %lu ms (from the press)", sr::kIntervalMs);
}

} // namespace zeus::scroll_arrows
