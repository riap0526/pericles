#include "games/zeus/notice.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/ui/notice.h"
#include "games/zeus/site.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): the
// notice boxes read with Ghidra; calling conventions checked by disassembly.
// The same code as Emperor's (games/emperor/notice.cpp) at other addresses, except for three things: no function
// takes a text as it is (the game's id 1000 shows the text in city_warning_text, which is also a block of the save
// file); one notice font (no red); the game replaces [names] in the text.
namespace zeus::notice {
namespace {

using zeus::Function;
using zeus::Install;
using zeus::Matches;
using zeus::Original;
using zeus::Site;
using zeus::Var;

// city_warnings_draw 0x522ec0 (__cdecl, no arguments; game_frame 0x5137fa calls it every frame): draws the boxes and
// clears those shown more than 15000 ms ago.
const Site kDraw{"city_warnings_draw", 0x522ec0, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0x83, 0xec, 0x18}};
// city_warning_show(id, arg, 0) 0x5231e0 (__cdecl, 3 arguments, ret; the callers add esp, 0xc): returns at once when
// the warnings are off (warnings_on), on screen 0 or for id 0x12 in the editor; otherwise takes the first slot not in
// use and, for id 1000, the text in city_warning_text (copied through a 500-byte stack buffer by 0x466b20, which
// replaces [name] with the game's values), measures it, picks the box's width and copies the text into the slot.
const Site kShow{"city_warning_show", 0x5231e0, {0xa0, 0xc9, 0xa7, 0xf2, 0x00, 0x81, 0xec, 0xf4}};
// What city_warning_show does with a slot, for rewriting the zoom box in place (the same calls, all __cdecl):
// text_get_width(text, font) 0x507da0, text_length(text) 0x507f00 (characters, at most 10000), text_copy(text, to,
// count) 0x507d10.
const Site kTextWidth{"text_get_width", 0x507da0, {0x83, 0xec, 0x08, 0x8b, 0x15, 0x20, 0xc8, 0x2b}};
const Site kTextLength{"text_length", 0x507f00, {0x56, 0x8b, 0x74, 0x24, 0x08, 0xba, 0x10, 0x27}};
const Site kTextCopy{"text_copy", 0x507d10, {0x56, 0x8b, 0x74, 0x24, 0x10, 0x85, 0xf6, 0x7e}};

using Draw_fn = void(__cdecl *)();
using Show_fn = void(__cdecl *)(int id, int arg, int unused);
using TextWidth_fn = int(__cdecl *)(const char *text, int font);
using TextLength_fn = short(__cdecl *)(const char *text);
using TextCopy_fn = void(__cdecl *)(const char *text, char *to, int count);
Draw_fn o_Draw = nullptr;

constexpr int kTextId = 1000; // city_warning_show: the text in city_warning_text (the game's own "Sound Frags %d")
const Var<BYTE> kWarningsOn{0xf2a7c9}; // warnings_on (city_warning_show's first check; config_load, 0x402620 write it)
const Var<int> kFrameTime{0xf9c0f4};   // frame_time (ms): when a box was shown
// city_warning_text 0x916af8, 200 bytes: block 125 of the save file (the save table 0x50d180), cleared for a new game
// (0x511e30), also used by the city message windows (0x4f2d10, 0x430d80). Ours is put back right after the call:
// city_warning_show has copied the text by then and does not read it again.
constexpr DWORD kWarningText = 0x916af8;
constexpr int kWarningTextSize = 200;
// The notice font, as city_warning_show takes it: image_paks' group table (0x12afbec), group 0 -- group 1 when the
// entry count 0x12bc820 is above 0 -- and the group's first image (+4).
const Var<DWORD> kGroupTable{0x12afbec};
const Var<int> kGroupCount{0x12bc820};
constexpr DWORD kSlots = 0xfd3438; // city_warnings: 5 boxes of 0x78 bytes
constexpr int kSlotCount = 5, kSlotSize = 0x78, kTextSize = core::ui::notice::kTextSize;

// One box (the game's layout, city_warning_show and city_warnings_draw; the same as Emperor's).
#pragma pack(push, 1)
struct Slot {
    short id;    // +0: the text's number; 1000 for a text in city_warning_text (ours, and the game's Sound Frags)
    short x, y;  // +2, +4
    short inUse; // +6
    short lines; // +8: the text's length in characters (text_length)
    short width; // +0xA: the box's width (200, 300, 400 or 460)
    int shown;   // +0xC: frame_time when shown
    short pad;   // +0x10
    char text[kTextSize]; // +0x12
};
#pragma pack(pop)
static_assert(sizeof(Slot) == 0x76 && sizeof(Slot) <= kSlotSize, "the game's notice slot");

Slot *SlotAt(int k)
{
    return At<Slot>(kSlots + k * kSlotSize);
}

int g_zoomSlot = -1;        // the box we made for the zoom (game thread)
char g_zoomText[kTextSize]; // what that box holds (to tell it is still ours)

// Our text as the game may take it: '[' starts a name the game replaces (0x466b20), and where it finds no ']' or no
// such name it leaves its buffer without an end, so the length counted next runs into the stack. Ours never names
// anything: '[' becomes '('.
void GameText(const char *text, char *out)
{
    int n = 0;
    for (; text[n] != 0 && n < kTextSize - 1; ++n) {
        out[n] = text[n] == '[' ? '(' : text[n];
    }
    out[n] = 0;
}

// The call itself runs under a guard of its own, so that the 200 bytes and warnings_on are put back also when it
// faults (the core's guard around the whole frame would leave them as we set them: a save would then keep our text).
core::diag::Feature g_showFeature{"notice text through city_warning_text (games/zeus/notice)"};

void ShowCall(void *gameText)
{
    BYTE *buffer = At<BYTE>(kWarningText);
    std::memset(buffer, 0, kWarningTextSize);
    std::memcpy(buffer, gameText, std::strlen(static_cast<const char *>(gameText)) + 1); // at most kTextSize bytes
    *kWarningsOn = 1; // ours answer the player: shown also with the game's warnings off
    Function<Show_fn>(kShow)(kTextId, 0, 0);
}

// city_warning_show with our text in city_warning_text and warnings_on 1 for the call; both put back right after,
// whether the call completed or faulted. False after a fault: our notices are then switched off (the caption line).
bool ShowText(const char *gameText)
{
    BYTE *buffer = At<BYTE>(kWarningText);
    BYTE kept[kWarningTextSize];
    std::memcpy(kept, buffer, sizeof(kept));
    const BYTE on = *kWarningsOn;
    static int s_loggedOn = -1; // the switch as last logged (a line when it changes, not one per notice)
    if (on != s_loggedOn) {
        s_loggedOn = on;
        core_log("zeus: notice: the game's warnings are %s%s", on ? "on" : "off",
                 on ? "" : "; ours are shown all the same");
    }
    const bool ok = core::diag::Guarded(g_showFeature, &ShowCall, const_cast<char *>(gameText));
    *kWarningsOn = on;
    std::memcpy(buffer, kept, sizeof(kept));
    if (!ok) {
        core::ui::notice::SwitchOff("city_warning_show faulted (city_warning_text and warnings_on put back)");
    }
    return ok;
}

int FirstFree()
{
    for (int k = 0; k < kSlotCount; ++k) {
        if (SlotAt(k)->inUse == 0) {
            return k;
        }
    }
    return -1;
}

int TextWidth(const char *text)
{
    // The table holds a pointer (not a game address to shift).
    const DWORD group = *kGroupTable + (*kGroupCount > 0 ? 8 : 0);
    return Function<TextWidth_fn>(kTextWidth)(text, *reinterpret_cast<const int *>(group + 4));
}

// Our box's width for a text this many pixels wide: the game's rule gives 460 above 300 pixels, without the side
// ornaments; ours keep the 400-pixel box up to 320, as in Emperor (the same boxes and ornaments).
short BoxWidth(int px)
{
    return px <= 100 ? 200 : px <= 200 ? 300 : px <= 320 ? 400 : 460;
}

// A new box with text; its slot index, or -1 when there was no room (five boxes shown, as for the game's own) or the
// game showed none (screen 0). Game thread (core/ui/notice's frame).
int NewBox(const char *text)
{
    const int k = FirstFree();
    if (k < 0) {
        return -1;
    }
    char gameText[kTextSize];
    GameText(text, gameText);
    if (!ShowText(gameText)) {
        return -1;
    }
    Slot *s = SlotAt(k);
    if (s->inUse == 0 || s->id != kTextId) {
        return -1;
    }
    const int px = TextWidth(s->text);
    const short game = s->width;
    s->width = BoxWidth(px);
    // The first 20 boxes only (the zoom box is opened again and again).
    static int logged = 0;
    constexpr int kWidthLogs = 20;
    if (logged < kWidthLogs) {
        core_log("zeus: notice: %d px wide: the %d box (the game's %d)", px, s->width, game);
    }
    else if (logged == kWidthLogs) {
        core_log("zeus: notice: the box widths are not logged any more");
    }
    if (logged <= kWidthLogs) {
        ++logged;
    }
    return k;
}

// core/ui/notice's newBox. red: Zeus has one notice font (city_warnings_draw 0x522ec0 takes it once for every box and
// never reads the id), so an error is shown in it as the others (Game::Red::kNone).
void NewBoxForCore(const char *text, bool)
{
    NewBox(text);
}

void ShowZoom(int percent)
{
    char text[32];
    std::snprintf(text, sizeof(text), core::ui::notice::kZoomFormat, percent);
    if (g_zoomSlot >= 0) {
        Slot *s = SlotAt(g_zoomSlot);
        // id 1000 is also the game's Sound Frags; the text tells ours.
        if (s->inUse != 0 && s->id == kTextId && std::memcmp(s->text, g_zoomText, kTextSize) == 0) {
            // Rewritten in place: the box keeps its place among the others and does not open again.
            s->width = BoxWidth(TextWidth(text));
            s->lines = Function<TextLength_fn>(kTextLength)(text);
            std::memset(s->text, 0, kTextSize);
            Function<TextCopy_fn>(kTextCopy)(text, s->text, s->lines);
            s->shown = *kFrameTime;
            std::memcpy(g_zoomText, s->text, kTextSize);
            return;
        }
        g_zoomSlot = -1; // gone (its time was up) or taken by another notice
    }
    g_zoomSlot = NewBox(text);
    if (g_zoomSlot >= 0) {
        std::memcpy(g_zoomText, SlotAt(g_zoomSlot)->text, kTextSize);
    }
}

void __cdecl Draw_detour()
{
    core::ui::notice::OnFrame(); // guarded by the core
    Original(o_Draw, kDraw)();
}

core::ui::notice::Game ZeusGame()
{
    return core::ui::notice::Game{"zeus: notice", core::ui::notice::Game::Red::kNone, &NewBoxForCore, &ShowZoom};
}

} // namespace

bool Attach()
{
    core::ui::notice::SetLogPrefix("zeus: notice"); // also on the caption line, when the boxes are not hooked
    const Site *sites[] = {&kDraw, &kShow, &kTextWidth, &kTextLength, &kTextCopy};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: notice: %s at 0x%lx does not match; our messages stay on our own caption line", s->name,
                     static_cast<unsigned long>(s->address));
            return false;
        }
    }
    if (!Install(kDraw, &Draw_detour, o_Draw)) {
        core_log("zeus: notice: city_warnings_draw could not be hooked; our messages stay on our own caption line");
        return false;
    }
    core::ui::notice::Install(ZeusGame());
    return true;
}

void InstallForTest()
{
    core::ui::notice::Install(ZeusGame());
}

} // namespace zeus::notice
