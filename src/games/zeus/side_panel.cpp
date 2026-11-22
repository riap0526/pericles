#include "games/zeus/side_panel.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/side_panel_layout.h"

#include "core/assets/sg3.h"
#include "core/display/city_layout.h"

#include <cstring>
#include <cwchar>
#include <vector>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57):
// - sidebar_name_bar_draw 0x517210 (no arguments, plain ret; called by sidebar_panel_draw 0x517480 on every frame of
//   the city screen and of the report screen) draws an inset 8 x 1 at (sidebar_x + 0x32, 0x12e) and, by hover_button
//   0x781834, the name of the button under the mouse: 0x19..0x29 text hover - 0x19 of group 88 (Culture, 6, as 0x18
//   when science_names(0)); 0x32..0x35 the building's name (group 28, 0x5f2710[hover]; 0x49 and 0x88 renamed on
//   Poseidon's maps) while the button's state byte (+0x18 of its 0x28-byte record at 0x63e240) is 1, and then its cost
//   under the button: building_cost(building, 0), number_draw at (sidebar_x + x, y + h + 3 + 0x1e) and the coin
//   (image_id(0x804)) at (sidebar_x + x, y + h + 3 + 0x17), setting screen_redraw on every such frame; 200 text 79
//   of group 51, 80 while sidebar_dialog_open 0xfa0428 is set.
// - balloon_help_update 0x4f6110 (no arguments, plain ret; game_frame 0x5137e0) decides nothing while screen_redraw is
//   set; on the city screen it asks balloon_help_button_text 0x4f5f00 (__cdecl, the hover number; al: a text was
//   put into balloon_help_text 0xef8194) and shows its answer at once; on the report screen (7) it asks only for
//   sidebar_dialog 2 and 0xd (0x4f6467), setting 0xef8178 = frame_time 0xf9c0f4 first, and on an answer
//   0xef8170 = 0 and returns 1. 0x4f5f00 has no text for 0x19..0x29, 0x32..0x35 and 200.
// - game_frame draws the tooltip (balloon_help_draw 0x4f6ba0, no arguments) only while balloon_help_text is not 0
//   (0x5137ff); balloon_help_end 0x4f6af0 then clears it. The drawing takes its group from balloon_help_group
//   0x4f6b60 (no arguments; 68 on these screens). frame_count 0xf2aac0 counts up at game_frame's start.
// - The functions called: science_names 0x4200f0 (__cdecl flag, al), building_cost 0x433d50 (__cdecl building, 0;
//   eax), number_draw 0x508f90 (__cdecl, 8 arguments: value, 0x40, 0x665e40, x, y, width, font, 0), image_id
//   0x40fd10 (__thiscall on the image manager 0x12aebe8, 1 argument, ret 4), image_draw 0x5acf50 (__thiscall on
//   the image manager, 6 arguments, ret 0x18). The font: *(group table 0x12afbec (+ 0x68 when the count 0x12bc820
//   is above 12) + 4).
//
// The column moved up (no tabs):
// - sidebar_panel_draw 0x517480 (no arguments, plain ret, returns 1; called by sidebar_content_draw 0x517bf0 on every
//   frame of the city screen and inside sidebar_draw 0x517b10 whenever the backdrop is drawn): the title (text
//   0x508d40: __cdecl group 88, the category (6 as 0x18 on Poseidon's maps), x sidebar_x + 0x32, y 0x1e, width 0x82,
//   font *(table + 8 + 4), 0), overlay_menu_build 0x5170a0, the information panel (inner_panel_draw 0x59f2a0, __cdecl
//   x, y, columns, rows: sidebar_x + 0x32, 300 - h, 8, h / 16 for n = overlay_menu_count 0xf9fff8 buttons), the
//   category's own panel 0x517000, then for the Overview category (build_category 0xfa0004 = 10) the minimap at
//   (sidebar_x + 0x32, 0x13e), else the build buttons 0x5171f0; the report button 0x516fa0 and the name bar 0x517210.
//   All buttons are drawn only while sidebar_buttons_redraw 0xf2a6cd (or screen_redraw) is set; the title and the
//   marble under it only when the backdrop is drawn again.
// - sidebar_content_draw then draws the 17 sidebar buttons (table 0x63dc40, the last two the tabs at rows 0..24) with
//   `push 0x11` at 0x517ca3; the city's input passes the same count to buttons_hover and buttons_update (0x515138,
//   0x515154) and the build menu's (0x51535f): with 0x0f the tabs are neither drawn nor clicked.
// - sidebar_overview_toggle 0x516d30 (no arguments; Tab and the tabs) sets sidebar_overview 0xfa0000; the city's
//   input calls minimap_hover and minimap_click(0) while the category is 10 (cmp at 0x5150d7) or the tab is the
//   Overview map: they use the place where the minimap was last drawn, which is no longer the Overview category's.
// - sidebar_panel_input 0x5178e0 (no arguments, returns al): the build buttons 0x5178a0, overlay_menu_input
//   0x517620, the report button 0x517580, the category's panel 0x517650 (all no arguments, al). The report screen (7)
//   calls 0x517580 first. buttons_hover 0x431c00 (__cdecl x, y, table, count, first hover number; al) and
//   buttons_update 0x4319c0 (__cdecl x, y, table, count; eax) take the mouse at 0xf2a738 / 0xf2a734 and copy it to
//   0xf2a730 / 0xf2a72c (the tooltip's place); a button record is 0x28 bytes: x, y, w, h (short), the left and right
//   actions at 0x10 and 0x14, its state at 0x18 (0 and 2: not clickable), the values passed to the action at 0x1c and
//   0x20 (0xf135f8, 0xf135f4). The report button's record 0x63dee8, its value the category's report kind
//   (sidebar_report_kind 0x63e488, 11 ints); its action opens the report or, while one is open, closes it.
//
// The army and ship panels' upper half:
// - sidebar_draw_units 0x525280 (__cdecl, the screen) draws the whole backdrop on every frame (it sets screen_redraw):
//   the picture, units_minimap_draw(1) at 0x525359, the gem, the name box (image 0x61 at (sidebar_x + 0x17, 0x93)) with
//   the hovered button's name and the state written in it. units_minimap_draw 0x58ea90 (__cdecl, a flag: draw even
//   when not scrolling; ret) draws minimap_draw(sidebar_x + 0x19, 0x1c, 0x47, 0x6d) outside the editors and clears
//   0x10dffb8 (set at 0x483999 to have it drawn once); screen_draw also calls it with 0 on every frame of screen 0x48
//   (0x5140b6), after the sidebar's content. Its other callers (0x51e8e7, 0x51eb08) are the editors'.
// - The content, every frame (0x525630 for 0x48, 0x525c10 for 0x20, entered by jmp from 0x58ea29 / 0x58ea32 and the
//   help screen's 0x4f288a / 0x4f2894): buttons_draw(sidebar_x, 0x1e, 0x6402a0 or 0x640390, 6) (0x28-byte records,
//   y at +2: 173, 225, 277 twice; these panels' inputs at 0x514fad / 0x514a0f give the same tables to
//   buttons_hover and buttons_update), inner_panel_draw(sidebar_x + 0xe, 0x168, 10, 7) (calls at 0x5257e8 and
//   0x525e66; the caller pops every argument with one add esp, 0x3c), then the texts (0x524ca0 / 0x5258d0, __cdecl x,
//   y, width: push 0x168 at 0x5257fb / 0x525e78) whose clickable rows are the 5 records of 0x640480 (0x1c bytes:
//   x1, y1, x2, y2 as shorts; y1 335, 355, .. 415; drawn at the base 0x26, clicked at 0x1e by 0x514ab3 / 0x514f59),
//   and the report button at (sidebar_x, 0xcd) + (157, 250), which does not move. Nothing in the exe writes those
//   records' y (the content rewrites the buttons' pictures at +0xa and their state at +0x18).
namespace zeus::side_panel {
namespace {

namespace lay = zeus::side_panel_layout;

using Void_fn = void(__cdecl *)();
using Int_fn = int(__cdecl *)();
using ButtonText_fn = int(__cdecl *)(int hover);
using ScienceNames_fn = bool(__cdecl *)(int flag);
using Cost_fn = int(__cdecl *)(int building, int flag);
using Number_fn = void(__cdecl *)(int value, int a, const void *b, int x, int y, int w, int font, int c);
using ImageId_fn = int(__thiscall *)(void *images, int index);
using ImageDraw_fn = void(__thiscall *)(void *images, int id, int x, int y, int a, int b, int c);

// Hooked.
const Site kUpdate{"balloon_help_update", 0x4f6110, {0xa0, 0xce, 0xa6, 0xf2, 0x00, 0x53, 0x55, 0x56}};
const Site kButtonText{"balloon_help_button_text", 0x4f5f00, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x83, 0xf8, 0x0a}};
const Site kGroup{"balloon_help_group", 0x4f6b60, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0x3c, 0x14, 0x75}};
const Site kDraw{"balloon_help_draw", 0x4f6ba0, {0xa1, 0x70, 0x81, 0xef, 0x00, 0x48, 0x0f, 0x84}};
const Site kNameBar{"sidebar_name_bar_draw", 0x517210, {0xa1, 0x34, 0xff, 0x0d, 0x01, 0x55, 0x56, 0x57}};
// Called.
const Site kScienceNames{"science_names", 0x4200f0, {0x8b, 0x0d, 0xe0, 0xa4, 0x6a, 0x00, 0xb8, 0x01}};
const Site kCost{"building_cost", 0x433d50, {0x56, 0x8b, 0x74, 0x24, 0x0c, 0x83, 0xfe, 0x06}};
const Site kNumber{"number_draw", 0x508f90, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x33, 0xf6, 0x56}};
const Site kImageId{"image_id", 0x40fd10, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0xc6, 0x99}};
const Site kImageDraw{"image_draw", 0x5acf50, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x8b, 0x84}};

// The column moved up: hooked.
const Site kPanelDraw{"sidebar_panel_draw", 0x517480, {0x56, 0x8b, 0x35, 0x04, 0x00, 0xfa, 0x00, 0x83}};
const Site kPanelInput{"sidebar_panel_input", 0x5178e0, {0xe8, 0xbb, 0xff, 0xff, 0xff, 0x84, 0xc0, 0x74}};
const Site kReportInput{"sidebar_report_button_input", 0x517580, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0x56, 0x33, 0xf6}};
const Site kOverviewToggle{"sidebar_overview_toggle", 0x516d30, {0xa1, 0xf8, 0x35, 0xf1, 0x00, 0xa3, 0x00, 0x00}};
// Called.
const Site kOverlayDraw{"overlay_menu_build", 0x5170a0, {0x8b, 0x0d, 0x04, 0x00, 0xfa, 0x00, 0x83, 0xec}};
const Site kInnerPanel{"inner_panel_draw", 0x59f2a0, {0x83, 0xec, 0x08, 0x8b, 0x4c, 0x24, 0x18, 0x55}};
const Site kCategoryDraw{"sidebar_category_panel_draw", 0x517000, {0xa1, 0x00, 0x00, 0xfa, 0x00, 0x83, 0xe8, 0x00}};
const Site kBuildDraw{"sidebar_build_buttons_draw", 0x5171f0, {0xa1, 0x34, 0xff, 0x0d, 0x01, 0x6a, 0x04, 0x68}};
const Site kText{"text_draw_centered", 0x508d40, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x4c, 0x24, 0x04}};
const Site kOverlayInput{"overlay_menu_input", 0x517620, {0xa1, 0xf8, 0xff, 0xf9, 0x00, 0x8b, 0x0d, 0x34}};
const Site kCategoryInput{"sidebar_category_panel_input", 0x517650, {0xa1, 0xf8, 0xff, 0xf9, 0x00, 0x8b, 0x15, 0x04}};
const Site kBuildInput{"sidebar_build_buttons_input", 0x5178a0, {0xa1, 0x34, 0xff, 0x0d, 0x01, 0x6a, 0x32, 0x6a}};
const Site kButtonsHover{"buttons_hover", 0x431c00, {0x8b, 0x44, 0x24, 0x10, 0x53, 0x55, 0x56, 0x57}};
const Site kButtonsUpdate{"buttons_update", 0x4319c0, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x1c, 0x56}};

// The box at the bottom: hooked.
const Site kListDraw{"sidebar_messages_draw", 0x430d80, {0x83, 0xec, 0x18, 0x56, 0x8b, 0xd1, 0x8b, 0x0d}};
const Site kListInput{"sidebar_messages_input", 0x430ed0, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0xbb, 0x01, 0x00}};
const Site kMinimapHover{"minimap_hover", 0x405d40, {0xa1, 0x38, 0xa7, 0xf2, 0x00, 0x8b, 0x0d, 0x2c}};
const Site kArmyContent{"army_sidebar_content_draw", 0x525630, {0xa1, 0x34, 0x18, 0x78, 0x00, 0x83, 0xec, 0x08}};
const Site kShipContent{"ship_sidebar_content_draw", 0x525c10, {0xa1, 0xf4, 0x57, 0xef, 0x00, 0x53, 0x55, 0x56}};
const Site kBottomInput{"sidebar_bottom_buttons_input", 0x517de0, {0xa1, 0x34, 0xff, 0x0d, 0x01, 0x6a, 0x0a, 0x6a}};
const Site kFilter{"message_filter", 0x521970, {0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x85}};
const Site kCityLoaded{"city_loaded_setup", 0x510ed0, {0xa1, 0x3c, 0xb8, 0xdb, 0x00, 0x83, 0xec, 0x0c}};
const Site kMessagesClear{"messages_clear", 0x522230, {0xb8, 0x48, 0x38, 0xfd, 0x00, 0x33, 0xd2, 0x66}};
// The army panels' upper half: hooked.
const Site kUnitsMinimap{"units_minimap_draw", 0x58ea90, {0x8b, 0x0d, 0x60, 0xff, 0x0d, 0x01, 0x33, 0xc0}};
// Called.
const Site kMinimapDraw{"minimap_draw", 0x454980, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x6c, 0xa1, 0xe4}};
const Site kMinimapClick{"minimap_click", 0x405d90, {0x83, 0x3d, 0x1c, 0x18, 0x78, 0x00, 0x01, 0x74}};
const Site kTextId{"message_text_id", 0x477d60, {0x8b, 0x44, 0x24, 0x04, 0x3d, 0x45, 0x10, 0x00}};
const Site kTextGet{"message_text_get", 0x466ad0, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0xc6, 0x05}};
const Site kWrapped{"text_draw_wrapped", 0x5090a0, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x55, 0x8b}};
const Site kCharWidth{"text_char_width", 0x507f20, {0x8b, 0x44, 0x24, 0x04, 0x84, 0xc0, 0x75, 0x03}};
const Site kImageButtons{"image_buttons_draw", 0x431530, {0x8b, 0x44, 0x24, 0x10, 0x85, 0xc0, 0x0f, 0x8e}};
const Site kRepeat{"buttons_repeat_update", 0x431c80, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x18, 0x33, 0xdb}};
const Site kOpen{"message_open", 0x522bd0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x0d, 0xf0, 0x37}};
const Site kRotateDraw{"sidebar_rotate_draw", 0x517a20, {0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x44, 0x24, 0x08}};
const Site kImageGroup{"image_group_get", 0x40fcf0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0x38, 0xcc}};

const Site *const kSites[] = {&kUpdate,     &kButtonText,   &kGroup,        &kDraw,         &kNameBar,
                              &kPanelDraw,  &kPanelInput,   &kReportInput,  &kOverviewToggle,
                              &kListDraw,   &kListInput,    &kMinimapHover, &kArmyContent,  &kShipContent,
                              &kBottomInput, &kFilter,      &kCityLoaded,   &kMessagesClear, &kUnitsMinimap,
                              &kScienceNames, &kCost,       &kNumber,       &kImageId,      &kImageDraw,
                              &kOverlayDraw, &kInnerPanel,  &kCategoryDraw, &kBuildDraw,    &kText,
                              &kOverlayInput, &kCategoryInput, &kBuildInput, &kButtonsHover, &kButtonsUpdate,
                              &kMinimapDraw, &kMinimapClick, &kTextId,      &kTextGet,      &kWrapped,
                              &kCharWidth,  &kImageButtons, &kRepeat,       &kOpen,         &kRotateDraw,
                              &kImageGroup};
constexpr int kSiteCount = static_cast<int>(sizeof(kSites) / sizeof(kSites[0]));

// Bytes changed in the game's code while the side panel is active (put back otherwise).
struct Patch {
    const char *name;
    DWORD address;
    int size;
    BYTE game[9], ours[9];
};
// `push 0x11; push 0x63dc40` -> `push 0x0f`: the 17 sidebar buttons without the two tabs (the table's last two).
#define TABS_PUSH(name, at)                                                                                        \
    {name, at, 7, {0x6a, 0x11, 0x68, 0x40, 0xdc, 0x63, 0x00}, {0x6a, 0x0f, 0x68, 0x40, 0xdc, 0x63, 0x00}}
// A short, little-endian.
#define SHORT_BYTES(v) static_cast<BYTE>((v) & 0xff), static_cast<BYTE>(((v) >> 8) & 0xff)
// A command button's y: record k of a table.
#define UNITS_BUTTON_Y(name, table, k, y)                                                                          \
    {name, (table) + (k) * 0x28 + 2, 2, {SHORT_BYTES(y)}, {SHORT_BYTES((y) - lay::kUnitsShift)}}
// A clickable row's y1, x2 (160) and y2: record k of 0x640480 (rows 20 high from 335).
#define UNITS_ROW(name, k)                                                                                         \
    {name,                                                                                                         \
     0x640480 + (k) * 0x1c + 2,                                                                                    \
     6,                                                                                                            \
     {SHORT_BYTES(335 + 20 * (k)), SHORT_BYTES(160), SHORT_BYTES(355 + 20 * (k))},                                                         \
     {SHORT_BYTES(335 + 20 * (k) - lay::kUnitsShift), SHORT_BYTES(160), SHORT_BYTES(355 + 20 * (k) - lay::kUnitsShift)}}
const Patch kPatches[] = {
    TABS_PUSH("city input: sidebar buttons hovered", 0x515138),
    TABS_PUSH("city input: sidebar buttons clicked", 0x515154),
    TABS_PUSH("build menu input: sidebar buttons", 0x51535f),
    TABS_PUSH("sidebar_content_draw: sidebar buttons", 0x517ca3),
    // `cmp dword ptr [build_category], 10; je` -> 0x7f (no category): the Overview category draws no minimap here,
    // so its input must not click the place where one was last drawn.
    {"city input: the Overview category's minimap",
     0x5150d7,
     9,
     {0x83, 0x3d, 0x04, 0x00, 0xfa, 0x00, 0x0a, 0x74, 0x10},
     {0x83, 0x3d, 0x04, 0x00, 0xfa, 0x00, 0x7f, 0x74, 0x10}},
    // The check, globe and gem buttons' y (543) in the bottom buttons' table and the gem's own record (data): 711,
    // the row under the box. Their drawing and input (the city's and the army panels') follow the tables.
    {"the check button's y", 0x63e04a, 2, {0x1f, 0x02}, {0xc7, 0x02}},
    {"the globe button's y", 0x63e072, 2, {0x1f, 0x02}, {0xc7, 0x02}},
    {"the gem's y", 0x63e09a, 2, {0x1f, 0x02}, {0xc7, 0x02}},
    // The army and ship panels' command buttons (y at +2 of 0x28-byte records), 138 rows up.
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 0, 173),
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 1, 225),
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 2, 277),
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 3, 173),
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 4, 225),
    UNITS_BUTTON_Y("an army command button's y", 0x6402a0, 5, 277),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 0, 173),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 1, 225),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 2, 277),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 3, 173),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 4, 225),
    UNITS_BUTTON_Y("a ship command button's y", 0x640390, 5, 277),
    // Their inset's clickable rows (y1, x2, y2 of 0x1c-byte records from +2), 138 rows up.
    UNITS_ROW("an army or ship panel's clickable row", 0),
    UNITS_ROW("an army or ship panel's clickable row", 1),
    UNITS_ROW("an army or ship panel's clickable row", 2),
    UNITS_ROW("an army or ship panel's clickable row", 3),
    UNITS_ROW("an army or ship panel's clickable row", 4),
    // Their texts' y (`push 0x168`), 138 rows up.
    {"army panel: its text's y", 0x5257fb, 5, {0x68, 0x68, 0x01, 0x00, 0x00}, {0x68, SHORT_BYTES(0x168 - 138), 0x00, 0x00}},
    {"ship panel: its text's y", 0x525e78, 5, {0x68, 0x68, 0x01, 0x00, 0x00}, {0x68, SHORT_BYTES(0x168 - 138), 0x00, 0x00}},
    // Their inset (`call inner_panel_draw`; the caller pops the arguments): a 5-byte nop. Ours is drawn, taller, with
    // the backdrop before the content.
    {"army panel: its inset", 0x5257e8, 5, {0xe8, 0xb3, 0x9a, 0x07, 0x00}, {0x0f, 0x1f, 0x44, 0x00, 0x00}},
    {"ship panel: its inset", 0x525e66, 5, {0xe8, 0x35, 0x94, 0x07, 0x00}, {0x0f, 0x1f, 0x44, 0x00, 0x00}},
};
#undef TABS_PUSH
#undef UNITS_BUTTON_Y
#undef UNITS_ROW
#undef SHORT_BYTES
constexpr int kPatchCount = static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0]));

Int_fn o_Update = nullptr;
ButtonText_fn o_ButtonText = nullptr;
Int_fn o_Group = nullptr;
Void_fn o_Draw = nullptr;
Void_fn o_NameBar = nullptr;
Int_fn o_PanelDraw = nullptr;
Int_fn o_PanelInput = nullptr;
Int_fn o_ReportInput = nullptr;
Void_fn o_OverviewToggle = nullptr;

// The box: the message box's functions take the message box (0x7815a0) in ecx, nothing on the stack (plain ret);
// message_filter is __thiscall on the message options (0xf2a760) with the message (ret 4), its answer in eax.
using List_fn = void(__fastcall *)(void *list, void *);
using Filter_fn = int(__fastcall *)(void *self, void *, void *message);
using Units_fn = void(__cdecl *)();
List_fn o_ListDraw = nullptr;
List_fn o_ListInput = nullptr;
Void_fn o_MinimapHover = nullptr;
Void_fn o_ArmyContent = nullptr;
Void_fn o_ShipContent = nullptr;
Int_fn o_BottomInput = nullptr;
Filter_fn o_Filter = nullptr;
Void_fn o_CityLoaded = nullptr;
Void_fn o_MessagesClear = nullptr;
using UnitsMinimap_fn = void(__cdecl *)(int force);
UnitsMinimap_fn o_UnitsMinimap = nullptr;
using MinimapDraw_fn = void(__cdecl *)(int x, int y, int cols, int rows);
using MinimapClick_fn = int(__cdecl *)(int unused);
using TextId_fn = int(__cdecl *)(int text);
using TextGet_fn = void(__cdecl *)(int text, char *to, int size);
using Wrapped_fn = void(__cdecl *)(const char *text, int x, int y, int w, int font, int flags);
using CharWidth_fn = int(__cdecl *)(int c, int font);
using ImageButtons_fn = void(__cdecl *)(int x, int y, void *table, int count);
using Repeat_fn = int(__cdecl *)(int x, int y, void *table, int count);
using Open_fn = void(__cdecl *)(int index);
using RotateDraw_fn = void(__cdecl *)(int x, int y);
using ImageGroup_fn = BYTE *(__thiscall *)(void *groups, int group);

using Inner_fn = void(__cdecl *)(int x, int y, int cols, int rows);
using Text_fn = int(__cdecl *)(int group, int text, int x, int y, int w, int font, int flags);
using Hover_fn = int(__cdecl *)(int x, int y, void *table, int count, int first);
using Update_fn = int(__cdecl *)(int x, int y, void *table, int count);

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<unsigned char> kScreenRedraw{0xf2a6ce};
const Var<int> kEditorMode{0x665e2c};
const Var<int> kNoSidebar{0x10dff60};
const Var<int> kHover{0x781834};
const Var<int> kTipText{0xef8194};
const Var<int> kTipKind{0xef8170};
const Var<int> kTipSince{0xef8178};
const Var<int> kBalloonHelp{0xf2a7b4};
const Var<int> kSidebarDialog{0x665e34};
const Var<unsigned char> kReportOpen{0xfa0428};
const Var<int> kFrameCount{0xf2aac0};
const Var<int> kFrameTime{0xf9c0f4};
const Var<int> kSidebarX{0x10dff34};
const Var<int> kFontTable{0x12afbec};
const Var<int> kFontCount{0x12bc820};
constexpr DWORD kBuildButtons = 0x63e240; // 4 records of 0x28 bytes: x, y, w, h (short) at 0..6, the state at 0x18
constexpr int kBuildButtonSize = 0x28, kBuildButtonState = 0x18;
constexpr DWORD kBuildingOf = 0x5f2710; // int per hover number: the build button's building
constexpr DWORD kNumberSuffix = 0x665e40;
constexpr DWORD kImages = 0x12aebe8;
constexpr int kCoinImage = 0x804, kNumberFlags = 0x40;

constexpr int kScreenCity = 1, kScreenReport = 7;

// The column moved up.
const Var<int> kOverview{0xfa0000};
const Var<int> kCategory{0xfa0004};
const Var<int> kOverlayCount{0xf9fff8};
const Var<unsigned char> kButtonsRedraw{0xf2a6cd};
const Var<int> kMouseX{0xf2a738}, kMouseY{0xf2a734};
const Var<int> kHoverY{0xf2a72c};
const Var<unsigned char *> kDrawBuffer{0xf9c100};
const Var<int> kPitch{0xf2a698};
const Var<int> kDdWidth{0xf2a6a0}, kDdHeight{0xf2a69c};
constexpr DWORD kReportKinds = 0x63e488;
constexpr DWORD kReportRecord = 0x63dee8;
constexpr int kRecordSize = 0x28, kRecordAction = 0x10, kRecordState = 0x18, kRecordValue = 0x1c;
constexpr int kGroupSidebar = 88, kScienceText = 0x18, kPanelX = 0x32, kPanelCols = 8;
constexpr int kTopEntry = 1233; // Zeus_Interface: the army panel's picture, 186 wide; its rows 0..24 are the
                                // city sidebar's without the tabs
constexpr wchar_t kInterfacePack[] = L"DATA\\Zeus_Interface.sg3"; // next to Zeus.exe

// The box at the bottom.
constexpr unsigned char kScreenArmy = 0x48, kScreenShip = 0x20;
constexpr DWORD kList = 0x7815a0;      // sidebar_messages: +0x10 the messages it can show, +0x18 the first one shown,
constexpr int kListCount = 0x10, kListFirst = 0x18, kListHovered = 0x1c; // +0x1c the mouse on it (the hand cursor)
constexpr DWORD kArrows = 0x5ed1a8;    // its two arrows (up, down), 0x1c bytes each
constexpr int kArrowSize = 0x1c;
const Var<int> kMessageCount{0xfd37f0};
constexpr DWORD kMessages = 0xfd3848;  // 0x3c bytes each, the text at +0 (short)
constexpr int kMessageSize = 0x3c;
const Var<int> kMessageCurrent{0xfd37e4}; // the message whose text is fetched (its values in the text)
constexpr DWORD kMessageText = 0x916af8;  // city_warning_text, 200 bytes, as the game's message box uses it
constexpr int kMessageTextSize = 200;
constexpr DWORD kMessageOptions = 0xf2a760;
constexpr DWORD kPostAsks = 0x521bc8; // city_message_post's second message_filter call returns here: 1 = Condensed
const Var<int> kWrapHeight{0xf13a6c};  // text_draw_wrapped adds the rows it took
const Var<int> kLineHeight{0xf13960};  // text_line_height: text_draw's, for its font
const Var<int> kMinimapHovered{0x78181c};
const Var<unsigned char> kMousePressed{0xf2a707};
const Var<int> kUnitsMinimapOnce{0x10dffb8}; // units_minimap_draw: draw it once (cleared when it looks)
constexpr DWORD kImageGroups = 0x12bc82c; // the interface pak's image groups (image_group_get's this)
constexpr int kGroupInset = 0x84;          // inner_panel_draw's 16 px tiles
constexpr int kToggleImage = 0x20c;        // the report button's picture (Zeus_Interface entry 1295), +1 hovered
// minimap_draw's place (0x7d3b08..14: its arguments; 0x7d3b20..34: its rectangle and origin), as minimap_hover and
// minimap_click read it: the army panel's own minimap at the top and the box's are both drawn there.
constexpr DWORD kMinimapArgs = 0x7d3b08, kMinimapRect = 0x7d3b20;
constexpr int kMinimapArgsSize = 16, kMinimapRectSize = 24;

core::diag::Feature g_feature{"side panel (games/zeus/side_panel)"};
bool g_installed = false;
bool g_pericles = true;
bool g_fellBack = false;
lay::TipMemo g_memo;
lay::CostTrail g_trail;

bool g_patched = false;      // kPatches are ours
bool g_backdropNow = false;  // inside sidebar_draw: the backdrop has just been drawn
lay::Backdrop g_backdrop;
bool g_titleHover = false;   // the title as last drawn: in its hovered font
bool g_loggedBelow = false;
// Entry 1233's rows 0..24, columns 50..185 (the tabs' rows without the tabs), and whether the backdrop is the one it
// belongs to (the Zeus pack's city sidebar).
struct Top {
    bool tried = false, ok = false;
    std::vector<lay::Px> px;   // kColumnW x kShift
    std::vector<lay::Px> left; // kTopCheckW x kTopCheckRows: columns 0..49 of the same rows
    int fits = -1;             // the backdrop taken last: 1 fits, 0 does not, -1 not known
} g_top;
// The title as a button: the report button's record, at the title's place (a fixed address: the game remembers the
// pressed button by its address).
alignas(4) BYTE g_titleRecord[kRecordSize];
bool g_titleRecordSet = false;

// The box at the bottom: what it shows (the minimap first, again for every city started or loaded), the box's inset
// as laid out (put back before its content on every frame: the messages are text over it), the messages drawn in it,
// the toggle as a button and the place where the box's minimap was drawn.
bool g_boxMessages = false;
struct BoxBackdrop {
    bool has = false;
    lay::Backdrop::Where where{};
    lay::Px px[lay::kBoxW * lay::kBoxH];
} g_boxBackdrop;
lay::MessageList g_list;
int g_lineHeight = 0; // text_line_height after the last message drawn (the box's font)
bool g_loggedMeasure = false;
alignas(4) BYTE g_toggleRecord[kRecordSize];
bool g_toggleRecordSet = false;
struct MinimapPlace {
    BYTE args[kMinimapArgsSize], rect[kMinimapRectSize];
};
MinimapPlace g_boxMinimap;
bool g_boxMinimapSet = false;

const char *const kChoices[] = {"pericles", "original"};
const char *const kChoiceLabels[] = {"pericles (Pericles style)", "original (the game's own side panel)"};

bool ActiveNow()
{
    return g_installed && g_pericles && core::diag::Enabled(g_feature) && *kEditorMode == 0 && *kNoSidebar == 0;
}

// Active on a screen with the side panel's names: the city and a report.
bool Ready()
{
    if (!ActiveNow()) {
        return false;
    }
    const unsigned char s = *kGameScreen;
    return s == kScreenCity || s == kScreenReport;
}

bool WritePatches(bool ours)
{
    bool ok = true;
    for (const Patch &p : kPatches) {
        ok = core::site::PatchCode(p.address + g_imageShift, ours ? p.ours : p.game, static_cast<size_t>(p.size)) &&
             ok;
    }
    return ok;
}

void FallBack(const char *why);

// The game's code as the side panel needs it: ours while active, the game's otherwise (the editors, no sidebar,
// original, after a fault). Checked on every frame and at once when the setting changes or something fails.
void SyncPatches()
{
    const bool want = ActiveNow();
    if (want == g_patched) {
        return;
    }
    if (!WritePatches(want)) {
        if (want) {
            WritePatches(false);
            g_patched = false;
            FallBack("the game's code could not be changed");
            return;
        }
        core_log("side panel: the game's code could not be put back");
    }
    g_patched = want;
    if (want) {
        *kOverview = 0; // the Info tab: the only one there is now (the game clears it too on a new game and a load)
    }
    g_backdrop.Forget();
    g_boxBackdrop.has = false;
    g_boxMinimapSet = false;
    *kScreenRedraw = 1;
    core_log("side panel: %s", want ? "active (no tabs, the column moved up, the box at the bottom)"
                                    : "the game's own side panel here");
}

// After a fault: the game's own side panel from now on (the feature is off), the screen drawn again.
void FallBack(const char *why)
{
    core::diag::Disable(g_feature, why);
    g_memo.Forget();
    g_trail.Reset();
    g_backdrop.Forget();
    SyncPatches();
    if (!g_fellBack) {
        g_fellBack = true;
        *kScreenRedraw = 1;
        core_log("side panel: off after an error (%s); the game's own side panel from now on", why);
    }
}

unsigned char *BuildButton(int hover)
{
    return At<unsigned char>(kBuildButtons + static_cast<DWORD>((hover - lay::kFirstBuildButton) * kBuildButtonSize));
}

bool BuildEnabled(int hover)
{
    return BuildButton(hover)[kBuildButtonState] == 1;
}

int BuildingOf(int hover)
{
    return At<int>(kBuildingOf)[hover];
}

lay::Facts ReadFacts(int hover)
{
    lay::Facts f{};
    f.poseidon = Function<ScienceNames_fn>(kScienceNames)(0);
    f.reportOpen = *kReportOpen != 0;
    f.boxMessages = g_boxMessages;
    if (lay::BuildButton(hover)) {
        f.buildEnabled = BuildEnabled(hover);
        f.building = BuildingOf(hover);
    }
    return f;
}

struct TextCall {
    int hover;
    bool answered;
};

// The name for the button as the tooltip's text (balloon_help_text), its group remembered for this frame.
void NameNow(void *p)
{
    TextCall &c = *static_cast<TextCall *>(p);
    lay::Name n{};
    if (!lay::NameFor(c.hover, ReadFacts(c.hover), &n)) {
        return;
    }
    *kTipText = g_memo.Remember(*kFrameCount, n);
    c.answered = true;
}

bool Answer(TextCall &c)
{
    g_memo.Forget();
    if (core::diag::Guarded(g_feature, &NameNow, &c)) {
        return c.answered;
    }
    FallBack("a sidebar button's name");
    *kTipText = 0;
    return false;
}

int __cdecl Update_detour()
{
    SyncPatches(); // game_frame: once a frame on every screen, after the input and before the next frame's drawing
    if (Ready() && *kGameScreen == kScreenReport) {
        const lay::ReportFacts f{*kScreenRedraw != 0, *kBalloonHelp, *kSidebarDialog, *kHover};
        if (lay::AnswerInReport(f)) {
            TextCall c{f.hover, false};
            if (Answer(c)) {
                // As the game's own answer from balloon_help_button_text on this screen (0x4f6452, 0x4f6477).
                *kTipSince = *kFrameTime;
                *kTipKind = 0;
                return 1;
            }
        }
    }
    return Original(o_Update, kUpdate)();
}

int __cdecl ButtonText_detour(int hover)
{
    if (Ready() && lay::Ours(hover)) {
        TextCall c{hover, false};
        if (Answer(c)) {
            return 1;
        }
    }
    else {
        g_memo.Forget();
    }
    return Original(o_ButtonText, kButtonText)(hover);
}

struct GroupCall {
    int group;
    bool ours;
};

int __cdecl Group_detour()
{
    if (g_installed && core::diag::Enabled(g_feature)) {
        GroupCall c{0, false};
        if (core::diag::Guarded(
                g_feature,
                [](void *p) {
                    GroupCall &c = *static_cast<GroupCall *>(p);
                    c.ours = g_memo.GroupFor(*kFrameCount, *kTipText, &c.group);
                },
                &c)) {
            if (c.ours) {
                return c.group;
            }
        }
        else {
            FallBack("the tooltip's text group");
        }
    }
    return Original(o_Group, kGroup)();
}

struct DrawCall {
    int text;
    core::diag::Restore r;
};

void __cdecl Draw_detour()
{
    int real = 0;
    if (g_installed && core::diag::Enabled(g_feature) && g_memo.RealText(*kTipText, &real)) {
        // Text 0 (Population): the real number only while the tooltip is drawn; balloon_help_end then finds our
        // stand-in (above 0) and clears it as it clears any text.
        DrawCall c{real, {}};
        if (core::diag::Guarded(
                g_feature,
                [](void *p) {
                    DrawCall &c = *static_cast<DrawCall *>(p);
                    c.r.Set(kTipText.ptr(), c.text);
                    Original(o_Draw, kDraw)();
                    c.r.Undo();
                },
                &c, &c.r)) {
            return;
        }
        FallBack("drawing a sidebar button's name");
        return; // not drawn this frame: the stand-in is not a text of the game's
    }
    Original(o_Draw, kDraw)();
}

// The cost under the hovered build button, as the name bar draws it; the hover number when drawn, else 0.
int CostNow()
{
    const int hover = *kHover;
    if (!lay::BuildButton(hover) || !BuildEnabled(hover)) {
        return 0;
    }
    const int cost = Function<Cost_fn>(kCost)(BuildingOf(hover), 0);
    if (cost == 0) {
        return 0;
    }
    const unsigned char *b = BuildButton(hover);
    short x, y, w, h;
    std::memcpy(&x, b + 0, 2);
    std::memcpy(&y, b + 2, 2);
    std::memcpy(&w, b + 4, 2);
    std::memcpy(&h, b + 6, 2);
    const int below = y + h + 3;
    const int table = *kFontTable + (*kFontCount > 12 ? 0x68 : 0);
    const int font = *reinterpret_cast<const int *>(static_cast<DWORD>(table) + 4);
    void *images = At<void>(kImages);
    Function<Number_fn>(kNumber)(cost, kNumberFlags, At<void>(kNumberSuffix), *kSidebarX + x, below + 0x1e, w, font,
                                 0);
    const int coin = Function<ImageId_fn>(kImageId)(images, kCoinImage);
    Function<ImageDraw_fn>(kImageDraw)(images, coin, *kSidebarX + x, below + 0x17, 0, 0, 0);
    return hover;
}

// On every screen that draws the sidebar (the city, a report, the build menu, the information and message windows,
// the help, the drop-down menus): the name bar's inset is drawn there whatever the mouse is on.
void __cdecl NameBar_detour()
{
    if (!ActiveNow()) {
        g_trail.Reset();
        Original(o_NameBar, kNameBar)();
        return;
    }
    if (core::diag::Guarded(
            g_feature,
            [](void *) {
                if (g_trail.Frame(CostNow())) {
                    *kScreenRedraw = 1; // the cost drawn under another button (or none now): cleared by the background
                }
            },
            nullptr)) {
        return;
    }
    FallBack("the cost under a build button");
    Original(o_NameBar, kNameBar)();
}

// ---- The column moved up ----

bool LoadTop()
{
    if (g_top.tried) {
        return g_top.ok;
    }
    g_top.tried = true;
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    while (len > 0 && path[len - 1] != L'\\') {
        --len;
    }
    path[len] = L'\0';
    if (len + sizeof(kInterfacePack) / sizeof(wchar_t) >= MAX_PATH) {
        return false;
    }
    std::wcscat(path, kInterfacePack);
    core::assets::Sg3Pack pack;
    std::vector<WORD> whole;
    int w = 0, h = 0;
    if (!pack.Open(path) || !pack.Size(kTopEntry, &w, &h) || w != lay::kColumnRight || h < lay::kShift ||
        !pack.Decode565(kTopEntry, 0, &whole)) {
        core_log("side panel: Zeus_Interface entry %d (%dx%d) not read: the marble above the title is the column's "
                 "own, moved up",
                 kTopEntry, w, h);
        return false;
    }
    g_top.px.resize(static_cast<size_t>(lay::kColumnW) * lay::kShift);
    for (int y = 0; y < lay::kShift; ++y) {
        std::memcpy(&g_top.px[static_cast<size_t>(y) * lay::kColumnW], &whole[static_cast<size_t>(y) * w + lay::kColumnLeft],
                    lay::kColumnW * sizeof(lay::Px));
    }
    g_top.left.resize(static_cast<size_t>(lay::kTopCheckW) * lay::kTopCheckRows);
    for (int y = 0; y < lay::kTopCheckRows; ++y) {
        std::memcpy(&g_top.left[static_cast<size_t>(y) * lay::kTopCheckW], &whole[static_cast<size_t>(y) * w],
                    lay::kTopCheckW * sizeof(lay::Px));
    }
    g_top.ok = true;
    return true;
}

struct Screen16 {
    lay::Px *column; // the column's first pixel (sidebar column 50, row 0)
    int pitch;       // pixels a row
    lay::Backdrop::Where where;
};

// The game's drawing buffer at the column, when the whole column is on it.
bool ColumnOnScreen(Screen16 *out)
{
    BYTE *px = *kDrawBuffer;
    const int pitch = *kPitch, w = *kDdWidth, h = *kDdHeight, x = *kSidebarX;
    if (!px || pitch <= 0 || (pitch & 1) || w <= 0 || h < lay::kColumnRows || pitch < w * 2 || x < 0 ||
        x + lay::kColumnRight > w) {
        return false;
    }
    out->column = reinterpret_cast<lay::Px *>(px) + x + lay::kColumnLeft;
    out->pitch = pitch / 2;
    out->where = lay::Backdrop::Where{x, w, h, pitch};
    return true;
}

// Whether entry 1233's rows fit the backdrop just taken (the left part of the tabs' rows, not moved).
void CheckTop(const Screen16 &s)
{
    int fits = 0;
    if (LoadTop()) {
        std::vector<lay::Px> left(static_cast<size_t>(lay::kTopCheckW) * lay::kTopCheckRows);
        const lay::Px *row0 = s.column - lay::kColumnLeft;
        for (int y = 0; y < lay::kTopCheckRows; ++y) {
            std::memcpy(&left[static_cast<size_t>(y) * lay::kTopCheckW], row0 + y * s.pitch,
                        lay::kTopCheckW * sizeof(lay::Px));
        }
        fits = lay::TopFits(left.data(), g_top.left.data(), lay::kTopCheckW) ? 1 : 0;
    }
    if (fits != g_top.fits) {
        if (fits) {
            core_log("side panel: above the title: Zeus_Interface entry %d's marble", kTopEntry);
        }
        else {
            core_log("side panel: above the title: the column's own marble, moved up (another backdrop)");
        }
        g_top.fits = fits;
    }
}

bool Poseidon()
{
    return Function<ScienceNames_fn>(kScienceNames)(0);
}

int ReportKind(int category)
{
    return category >= 0 && category < lay::kCategoryCount ? At<int>(kReportKinds)[category] : 0;
}

bool TitleClickableNow()
{
    const int category = *kCategory;
    return lay::TitleClickable(category, ReportKind(category), Poseidon());
}

bool MouseOnTitle()
{
    return lay::OnTitle(*kMouseX - *kSidebarX, *kMouseY);
}

int Font(int group)
{
    const int table = *kFontTable + lay::FontEntryOffset(group, *kFontCount);
    return *reinterpret_cast<const int *>(static_cast<DWORD>(table) + 4);
}

void DrawTitle()
{
    const int category = *kCategory;
    int text = category;
    const bool poseidon = Poseidon();
    if (category == lay::kCultureCategory && poseidon) {
        text = kScienceText;
    }
    g_titleHover = lay::TitleClickable(category, ReportKind(category), poseidon) && MouseOnTitle();
    Function<Text_fn>(kText)(kGroupSidebar, text, *kSidebarX + lay::kTitleX, lay::kTitleY, lay::kTitleW,
                             Font(g_titleHover ? lay::kTitleHoverFont : lay::kTitleFont), 0);
}

// ---- The box at the bottom ----

// The whole sidebar on the game's drawing buffer (at least the 1024 x 768 layout's rows).
struct Sidebar16 {
    lay::Px *px; // the sidebar's first pixel (column 0, row 0)
    int pitch;   // pixels a row
    int height;
    lay::Backdrop::Where where;
};

bool SidebarOnScreen(Sidebar16 *out)
{
    BYTE *px = *kDrawBuffer;
    const int pitch = *kPitch, w = *kDdWidth, h = *kDdHeight, x = *kSidebarX;
    if (!px || pitch <= 0 || (pitch & 1) || w <= 0 || h < lay::kLayoutH || pitch < w * 2 || x < 0 ||
        x + lay::kSidebarW > w) {
        return false;
    }
    out->px = reinterpret_cast<lay::Px *>(px) + x;
    out->pitch = pitch / 2;
    out->height = h;
    out->where = lay::Backdrop::Where{x, w, h, pitch};
    return true;
}

void *Images()
{
    return At<void>(kImages);
}

void DrawInsetTile(int k, int x, int y, void *base)
{
    Function<ImageDraw_fn>(kImageDraw)(Images(), *static_cast<int *>(base) + k, x, y, 0, 0, 0);
}

// The game's inset panel of any size (its 16 px tiles, group 0x84 of the interface pak, as inner_panel_draw has them).
void Inset(const RECT &r)
{
    BYTE *entry = Function<ImageGroup_fn>(kImageGroup)(At<void>(kImageGroups), kGroupInset);
    if (!entry) {
        return;
    }
    int base = *reinterpret_cast<int *>(entry + 4) + 0x4000;
    core::display::InsetPanel(r, &DrawInsetTile, &base);
}

// The backdrop has just been drawn (the city's sidebar, or an army panel's): the rows from 512 down laid out again,
// the box's inset kept for its content's frames.
void LayOutLowerHalf()
{
    Sidebar16 s{};
    if (!SidebarOnScreen(&s)) {
        g_boxBackdrop.has = false;
        return;
    }
    lay::LowerHalf(s.px, s.pitch, s.height);
    const int x = *kSidebarX;
    Inset(RECT{x + lay::kBoxX, lay::kBoxY, x + lay::kBoxX + lay::kBoxW, lay::kBoxY + lay::kBoxH});
    if (s.height - lay::kLayoutH >= 32) { // lower than its two edges: the plain rows as laid out
        Inset(RECT{x + lay::kTallX, lay::kLayoutH, x + lay::kTallX + lay::kTallW, s.height});
    }
    for (int y = 0; y < lay::kBoxH; ++y) {
        std::memcpy(&g_boxBackdrop.px[y * lay::kBoxW], s.px + (lay::kBoxY + y) * s.pitch + lay::kBoxX,
                    lay::kBoxW * sizeof(lay::Px));
    }
    g_boxBackdrop.where = s.where;
    g_boxBackdrop.has = true;
}

void SavePlace(MinimapPlace *p)
{
    std::memcpy(p->args, At<BYTE>(kMinimapArgs), kMinimapArgsSize);
    std::memcpy(p->rect, At<BYTE>(kMinimapRect), kMinimapRectSize);
}

void LoadPlace(const MinimapPlace &p)
{
    std::memcpy(At<BYTE>(kMinimapArgs), p.args, kMinimapArgsSize);
    std::memcpy(At<BYTE>(kMinimapRect), p.rect, kMinimapRectSize);
}

int MouseCol()
{
    return *kMouseX - *kSidebarX;
}

// The box's minimap: the last one drawn (the army panels have none of their own at the top any more), so the
// game's own input on those panels asks it where it is.
void DrawBoxMinimap()
{
    Function<MinimapDraw_fn>(kMinimapDraw)(*kSidebarX + lay::kMinimapX, lay::kMinimapY, lay::kMinimapCols,
                                           lay::kMinimapRows);
    SavePlace(&g_boxMinimap);
    g_boxMinimapSet = true;
}

int CharWidth(unsigned char c, void *font)
{
    return Function<CharWidth_fn>(kCharWidth)(c, *static_cast<int *>(font));
}

// The font of the game's message box: the group table's entry 16 when the table has 16 entries or more.
int MessageFont()
{
    const int table = *kFontTable + (*kFontCount >= 16 ? 0x80 : 0);
    return *reinterpret_cast<const int *>(static_cast<DWORD>(table) + 4);
}

int Filter(void *message)
{
    return Original(o_Filter, kFilter)(At<void>(kMessageOptions), nullptr, message);
}

// The messages as sidebar_messages_draw 0x430d80 picks them (message_filter, a text, from the first one shown on),
// one after another from the top of the box while they fit, and its arrows while it has more than one.
void DrawMessages()
{
    g_list.Begin();
    const int count = *kMessageCount;
    if (count <= 0) {
        return;
    }
    const int font = MessageFont();
    *kButtonsRedraw = 1;
    BYTE *list = At<BYTE>(kList);
    int first = 0, listCount = 0;
    std::memcpy(&first, list + kListFirst, sizeof(first));
    std::memcpy(&listCount, list + kListCount, sizeof(listCount));
    const int x = *kSidebarX;
    char *text = At<char>(kMessageText);
    *kWrapHeight = 0;
    int y = lay::kTextY, counted = 0;
    for (int i = 0; i < count; ++i) {
        void *m = At<void>(kMessages + static_cast<DWORD>(i * kMessageSize));
        if (Filter(m) == 0) {
            continue;
        }
        const int id = *static_cast<const short *>(m);
        if (id < 0 || Function<TextId_fn>(kTextId)(id) < 0 || ++counted <= first) {
            continue;
        }
        const int was = *kMessageCurrent;
        *kMessageCurrent = i; // the message's own values in its text
        Function<TextGet_fn>(kTextGet)(id, text, kMessageTextSize);
        *kMessageCurrent = was;
        int f = font;
        const int lines = lay::WrapLines(reinterpret_cast<const unsigned char *>(text), lay::kTextW, &CharWidth, &f);
        if (g_list.Count() > 0 && !lay::MessageFits(y, lines, g_lineHeight)) {
            break;
        }
        const int before = *kWrapHeight;
        Function<Wrapped_fn>(kWrapped)(text, x + lay::kTextX, y, lay::kTextW, font, 0);
        const int rows = *kWrapHeight - before;
        g_lineHeight = *kLineHeight;
        if (!g_loggedMeasure && rows != lines * (5 + g_lineHeight)) {
            g_loggedMeasure = true;
            core_log("side panel: a message measured %d lines but drew %d rows (line height %d)", lines, rows,
                     g_lineHeight);
        }
        g_list.Add(i, y, y + rows);
        y += rows + lay::kMessageGap;
        if (y >= lay::kTextBottom) {
            break;
        }
    }
    if (listCount > 1) {
        const int ax = x + lay::kListX + lay::kListW;
        Function<ImageButtons_fn>(kImageButtons)(ax, lay::kArrowUpY - lay::kArrowUpOffset, At<void>(kArrows), 1);
        Function<ImageButtons_fn>(kImageButtons)(ax, lay::kArrowDownY - lay::kArrowDownOffset,
                                                 At<void>(kArrows + kArrowSize), 1);
    }
}

void DrawToggle()
{
    const bool hover = lay::OnToggle(MouseCol(), *kMouseY);
    const int id = Function<ImageId_fn>(kImageId)(Images(), kToggleImage) + (hover ? 1 : 0);
    Function<ImageDraw_fn>(kImageDraw)(Images(), id, *kSidebarX + lay::kToggleX, lay::kToggleY, 0, 0, 0);
}

// The box's content over its inset put back: the minimap or the messages, and the toggle.
void DrawBox()
{
    Sidebar16 s{};
    if (!SidebarOnScreen(&s) || !g_boxBackdrop.has || !(g_boxBackdrop.where == s.where)) {
        return; // laid out with the next backdrop
    }
    for (int y = 0; y < lay::kBoxH; ++y) {
        std::memcpy(s.px + (lay::kBoxY + y) * s.pitch + lay::kBoxX, &g_boxBackdrop.px[y * lay::kBoxW],
                    lay::kBoxW * sizeof(lay::Px));
    }
    if (g_boxMessages) {
        DrawMessages();
    }
    else {
        g_list.Begin();
        DrawBoxMinimap();
    }
    DrawToggle();
}

// The message box's input as sidebar_messages_input 0x430ed0 has it (its arrows, the hand cursor over it), but a
// click opens the message under the mouse (the game shows one at a time and opens the first one shown).
void MessagesInput(BYTE *list)
{
    list[kListHovered] = 0;
    if (!g_boxMessages) {
        return;
    }
    const int x = *kSidebarX;
    int listCount = 0;
    std::memcpy(&listCount, list + kListCount, sizeof(listCount));
    if (listCount > 1) {
        const int ax = x + lay::kListX + lay::kListW;
        const int up = Function<Repeat_fn>(kRepeat)(ax, lay::kArrowUpY - lay::kArrowUpOffset, At<void>(kArrows), 1);
        const int down = Function<Repeat_fn>(kRepeat)(ax, lay::kArrowDownY - lay::kArrowDownOffset,
                                                      At<void>(kArrows + kArrowSize), 1);
        if (up != 0 || down != 0) {
            return;
        }
    }
    if (!lay::OnList(MouseCol(), *kMouseY)) {
        return;
    }
    list[kListHovered] = 1;
    if (*kMousePressed == 0) {
        return;
    }
    const int k = g_list.At(*kMouseY);
    if (k >= 0) {
        Function<Open_fn>(kOpen)(k);
    }
}

// The toggle as a button (the report button's record at the toggle's place, its action the toggle's own: the Tab
// key's sidebar_overview_toggle): hovered (hover number 201) and clicked as the game's buttons are.
bool ToggleInput()
{
    if (!g_toggleRecordSet) {
        std::memcpy(g_toggleRecord, At<BYTE>(kReportRecord), kRecordSize);
        g_toggleRecordSet = true;
    }
    const short place[4] = {static_cast<short>(lay::kToggleX), static_cast<short>(lay::kToggleY),
                            static_cast<short>(lay::kToggleSize), static_cast<short>(lay::kToggleSize)};
    std::memcpy(g_toggleRecord, place, sizeof(place));
    g_toggleRecord[kRecordState] = 1;
    const DWORD action = kOverviewToggle.address + g_imageShift;
    std::memcpy(g_toggleRecord + kRecordAction, &action, sizeof(action));
    const int x = *kSidebarX;
    if ((Function<Hover_fn>(kButtonsHover)(x, 0, g_toggleRecord, 1, lay::kToggleButton) & 0xff) != 0 &&
        Function<Update_fn>(kButtonsUpdate)(x, 0, g_toggleRecord, 1) != 0) {
        *kButtonsRedraw = 1;
        return true;
    }
    return false;
}

// The box's input: the toggle; the minimap (with the box's place put in while asked); on an army panel the messages
// too (the game has no message box input there).
bool BoxInput(bool army)
{
    const int col = MouseCol(), y = *kMouseY;
    if (lay::OnToggle(col, y)) {
        return ToggleInput();
    }
    if (g_boxMessages) {
        if (army) {
            MessagesInput(At<BYTE>(kList));
        }
        return false;
    }
    if (!lay::OnMinimap(col, y) || !g_boxMinimapSet) {
        return false;
    }
    MinimapPlace now{};
    SavePlace(&now);
    LoadPlace(g_boxMinimap);
    Original(o_MinimapHover, kMinimapHover)();
    const bool hit = Function<MinimapClick_fn>(kMinimapClick)(0) != 0;
    LoadPlace(now);
    return hit;
}

struct ColumnCall {
    bool game;   // the game's own drawing this frame (no backdrop yet, or not on screen)
    bool giveUp; // the backdrop never comes (sidebar_draw is not hooked): the game's own side panel
};
// Frames in a row that asked for the backdrop and did not get it: one redraw brings it, so a few more mean that
// sidebar_draw is not seen (its hook is not installed).
int g_noBackdrop = 0;
constexpr int kNoBackdropLimit = 8;
bool g_armyLaidOut = false; // an army panel's backdrop laid out since its content was last drawn
int g_noArmyBackdrop = 0;

// sidebar_panel_draw, the column moved up: the backdrop put back, the game's drawing in its own place (without the
// title, the Overview category's minimap, the report button and the name bar), moved up, then the title, the build
// buttons and the cost in the new place.
void DrawColumn(void *p)
{
    ColumnCall &c = *static_cast<ColumnCall *>(p);
    Screen16 s{};
    if (!ColumnOnScreen(&s)) {
        c.game = true;
        return;
    }
    if (g_backdropNow) {
        g_backdrop.Take(s.column, s.pitch, s.where);
        CheckTop(s);
        g_noBackdrop = 0;
        // Before anything is drawn there: the game's buttons (the bottom row at its new place, the gem) are drawn
        // over it afterwards in the same sidebar_draw.
        LayOutLowerHalf();
    }
    else if (g_backdrop.Has(s.where)) {
        g_backdrop.PutBack(s.column, s.pitch);
    }
    else {
        *kScreenRedraw = 1; // taken when the backdrop is drawn, from the next frame
        c.game = true;
        c.giveUp = ++g_noBackdrop > kNoBackdropLimit;
        return;
    }
    *kOverview = 0;
    *kButtonsRedraw = 1; // the buttons drawn on every frame, over the backdrop put back
    Function<Void_fn>(kOverlayDraw)();
    const lay::InfoPanel info = lay::InfoPanelFor(*kOverlayCount);
    Function<Inner_fn>(kInnerPanel)(*kSidebarX + kPanelX, info.top, kPanelCols, info.rows);
    Function<Void_fn>(kCategoryDraw)();
    if (!g_loggedBelow && lay::DrawnBelow(s.column, s.pitch, g_backdrop.Pixels())) {
        g_loggedBelow = true;
        core_log("side panel: category %d drew below the moved rows (rows %d..%d are the backdrop's)", *kCategory,
                 lay::kMovedEnd, lay::kColumnRows - 1);
    }
    lay::MoveColumn(s.column, s.pitch, g_backdrop.Pixels(), g_top.fits == 1 ? g_top.px.data() : nullptr);
    DrawTitle();
    if (*kCategory != lay::kOverviewCategory) {
        Function<Void_fn>(kBuildDraw)();
    }
    DrawBox();
}

int __cdecl PanelDraw_detour()
{
    SyncPatches();
    if (ActiveNow()) {
        ColumnCall c{false, false};
        if (core::diag::Guarded(g_feature, &DrawColumn, &c)) {
            if (!c.game) {
                NameBar_detour(); // the cost under the hovered build button
                return 1;
            }
            if (c.giveUp) {
                FallBack("the sidebar's backdrop is not seen being drawn");
            }
        }
        else {
            FallBack("drawing the side panel's column");
        }
    }
    return Original(o_PanelDraw, kPanelDraw)();
}

// The title as the report button: hovered (hover number 200, the report button's name) and clicked as it is.
bool TitleInput()
{
    if (!TitleClickableNow()) {
        return false;
    }
    if (!g_titleRecordSet) {
        std::memcpy(g_titleRecord, At<BYTE>(kReportRecord), kRecordSize);
        g_titleRecordSet = true;
    }
    const short place[4] = {static_cast<short>(lay::kTitleX), 0, static_cast<short>(lay::kTitleRight - lay::kTitleX),
                            static_cast<short>(lay::kTitleBottom)};
    std::memcpy(g_titleRecord, place, sizeof(place));
    g_titleRecord[kRecordState] = 1;
    const int kind = ReportKind(*kCategory);
    std::memcpy(g_titleRecord + kRecordValue, &kind, sizeof(kind));
    const int x = *kSidebarX;
    if ((Function<Hover_fn>(kButtonsHover)(x, 0, g_titleRecord, 1, lay::kReportButton) & 0xff) != 0 &&
        Function<Update_fn>(kButtonsUpdate)(x, 0, g_titleRecord, 1) != 0) {
        *kButtonsRedraw = 1;
        return true;
    }
    return false;
}

struct InputCall {
    bool hit;
    core::diag::Restore r;
};

// sidebar_panel_input: the build buttons where they are; the title; the overlay buttons and the category's panel
// with the mouse in the game's place (25 rows lower). The panels are given the moved mouse wherever it is (as the
// game gives them the mouse on every frame: their lists see it leave), and nothing of theirs is drawn below the
// moved rows.
void PanelInputNow(void *p)
{
    InputCall &c = *static_cast<InputCall *>(p);
    // The box: on the city screen, where the game's input asked its minimap (the message box is the game's own
    // call, at the end of its input).
    if (*kGameScreen == kScreenCity && BoxInput(false)) {
        c.hit = true;
        return;
    }
    if (Function<Int_fn>(kBuildInput)() & 0xff) {
        c.hit = true;
        return;
    }
    if (MouseOnTitle()) {
        c.hit = TitleInput();
        return;
    }
    const int y = *kMouseY, moved = y + lay::kShift;
    c.r.Set(kMouseY.ptr(), moved);
    c.hit = (Function<Int_fn>(kOverlayInput)() & 0xff) != 0 || (Function<Int_fn>(kCategoryInput)() & 0xff) != 0;
    c.r.Undo();
    if (*kHoverY == moved) {
        *kHoverY = y; // the tooltip beside the mouse as it is
    }
}

int __cdecl PanelInput_detour()
{
    if (ActiveNow()) {
        InputCall c{false, {}};
        if (core::diag::Guarded(g_feature, &PanelInputNow, &c, &c.r)) {
            return c.hit ? 1 : 0;
        }
        FallBack("the side panel's input");
    }
    return Original(o_PanelInput, kPanelInput)();
}

// The report screen asks the report button first: there the title is it (clicked to close the report). The column
// is drawn there only with the background, so a change of the title's font asks for one.
void ReportTitleNow(void *p)
{
    bool &hit = *static_cast<bool *>(p);
    const bool hover = TitleClickableNow() && MouseOnTitle();
    if (hover != g_titleHover) {
        *kScreenRedraw = 1;
    }
    hit = MouseOnTitle() && TitleInput();
}

int __cdecl ReportInput_detour()
{
    if (ActiveNow() && *kGameScreen == kScreenReport) {
        bool hit = false;
        if (core::diag::Guarded(g_feature, &ReportTitleNow, &hit)) {
            return hit ? 1 : 0;
        }
        FallBack("the title on the report screen");
    }
    return Original(o_ReportInput, kReportInput)();
}

// Tab and the box's toggle (the tabs are no longer shown): the box shows the other one.
void __cdecl OverviewToggle_detour()
{
    if (ActiveNow()) {
        g_boxMessages = !g_boxMessages;
        *kScreenRedraw = 1;
        *kButtonsRedraw = 1;
        return;
    }
    Original(o_OverviewToggle, kOverviewToggle)();
}

// The game's message box is drawn in the box (DrawBox), over its inset put back.
void __fastcall ListDraw_detour(void *list, void *)
{
    if (ActiveNow()) {
        return;
    }
    Original(o_ListDraw, kListDraw)(list, nullptr);
}

void __fastcall ListInput_detour(void *list, void *)
{
    if (ActiveNow()) {
        if (core::diag::Guarded(g_feature, [](void *p) { MessagesInput(static_cast<BYTE *>(p)); }, list)) {
            return;
        }
        FallBack("the messages' input");
    }
    Original(o_ListInput, kListInput)(list, nullptr);
}

// The game asks whether the mouse is on the minimap where it was drawn last: not on the box's toggle, and not while
// the box shows the messages (the last minimap drawn is the box's, on the city screen and on the army panels, whose
// input asks it too).
void __cdecl MinimapHover_detour()
{
    if (ActiveNow()) {
        bool block = false;
        if (core::diag::Guarded(
                g_feature,
                [](void *p) { *static_cast<bool *>(p) = lay::OnToggle(MouseCol(), *kMouseY) || g_boxMessages; },
                &block)) {
            if (block) {
                *kMinimapHovered = 0;
                return;
            }
        }
        else {
            FallBack("the minimap's hover");
        }
    }
    Original(o_MinimapHover, kMinimapHover)();
}

// An army or a ship panel's content (on every frame, after its backdrop): then the box's. Its backdrop is drawn on
// every frame there, so a few frames without it mean that sidebar_draw_units is not seen (its hook is not
// installed): the game's own side panel then, not the buttons moved over the picture.
void ArmyBox()
{
    if (!ActiveNow()) {
        return;
    }
    const unsigned char s = *kGameScreen;
    if (s == kScreenArmy || s == kScreenShip) {
        if (!g_armyLaidOut && ++g_noArmyBackdrop > kNoBackdropLimit) {
            FallBack("the army panel's backdrop is not seen being drawn");
            return;
        }
        g_armyLaidOut = false;
    }
    if (!core::diag::Guarded(g_feature, [](void *) { DrawBox(); }, nullptr)) {
        FallBack("the box on an army panel");
    }
}

void __cdecl ArmyContent_detour()
{
    Original(o_ArmyContent, kArmyContent)();
    ArmyBox();
}

void __cdecl ShipContent_detour()
{
    Original(o_ShipContent, kShipContent)();
    ArmyBox();
}

// The army panels' input asks the bottom buttons after the gem: then the box (the game's own message box input is
// not there).
int __cdecl BottomInput_detour()
{
    const int r = Original(o_BottomInput, kBottomInput)();
    const unsigned char s = *kGameScreen;
    if ((r & 0xff) != 0 || !ActiveNow() || (s != kScreenArmy && s != kScreenShip)) {
        return r;
    }
    bool hit = false;
    if (core::diag::Guarded(g_feature, [](void *p) { *static_cast<bool *>(p) = BoxInput(true); }, &hit)) {
        return hit ? 1 : r;
    }
    FallBack("the box's input on an army panel");
    return r;
}

// city_message_post 0x521a70 asks message_filter a second time for the message just posted; 1 (Condensed: shown in
// the sidebar, not as a window) brings the box to the messages, as Emperor's side panel does.
int __fastcall Filter_detour(void *self, void *, void *message)
{
    const int r = Original(o_Filter, kFilter)(self, nullptr, message);
    if (r == 1 && !g_boxMessages &&
        reinterpret_cast<DWORD>(__builtin_return_address(0)) == kPostAsks + g_imageShift && ActiveNow()) {
        g_boxMessages = true;
        *kScreenRedraw = 1;
        core_log("side panel: a Condensed message: the box shows the messages");
    }
    return r;
}

void ResetBox()
{
    g_boxMessages = false;
    g_list.Begin();
}

// A city loaded (a saved game or the next mission) or started (its messages cleared): the box shows the minimap.
void __cdecl CityLoaded_detour()
{
    Original(o_CityLoaded, kCityLoaded)();
    ResetBox();
}

void __cdecl MessagesClear_detour()
{
    Original(o_MessagesClear, kMessagesClear)();
    ResetBox();
}

// The army panels' own minimap at the top: not drawn (their upper half is moved into its place; the box at the
// bottom has the minimap). Its flag to be drawn once is cleared as the game clears it when it looks. The editors'
// (their own place) as the game draws it.
void __cdecl UnitsMinimap_detour(int force)
{
    if (ActiveNow()) {
        *kUnitsMinimapOnce = 0;
        return;
    }
    Original(o_UnitsMinimap, kUnitsMinimap)(force);
}

const Detour kDetours[] = {
    {&kUpdate, reinterpret_cast<void *>(&Update_detour)},
    {&kButtonText, reinterpret_cast<void *>(&ButtonText_detour)},
    {&kGroup, reinterpret_cast<void *>(&Group_detour)},
    {&kDraw, reinterpret_cast<void *>(&Draw_detour)},
    {&kNameBar, reinterpret_cast<void *>(&NameBar_detour)},
    {&kPanelDraw, reinterpret_cast<void *>(&PanelDraw_detour)},
    {&kPanelInput, reinterpret_cast<void *>(&PanelInput_detour)},
    {&kReportInput, reinterpret_cast<void *>(&ReportInput_detour)},
    {&kOverviewToggle, reinterpret_cast<void *>(&OverviewToggle_detour)},
    {&kListDraw, reinterpret_cast<void *>(&ListDraw_detour)},
    {&kListInput, reinterpret_cast<void *>(&ListInput_detour)},
    {&kMinimapHover, reinterpret_cast<void *>(&MinimapHover_detour)},
    {&kArmyContent, reinterpret_cast<void *>(&ArmyContent_detour)},
    {&kShipContent, reinterpret_cast<void *>(&ShipContent_detour)},
    {&kBottomInput, reinterpret_cast<void *>(&BottomInput_detour)},
    {&kFilter, reinterpret_cast<void *>(&Filter_detour)},
    {&kCityLoaded, reinterpret_cast<void *>(&CityLoaded_detour)},
    {&kMessagesClear, reinterpret_cast<void *>(&MessagesClear_detour)},
    {&kUnitsMinimap, reinterpret_cast<void *>(&UnitsMinimap_detour)},
};
constexpr int kHookCount = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));

bool Apply(int, const char *value, bool initial)
{
    const bool pericles = std::strcmp(value, kChoices[0]) == 0;
    const bool changed = pericles != g_pericles;
    g_pericles = pericles;
    g_memo.Forget();
    g_trail.Reset();
    if (changed && !initial && g_installed) {
        *kScreenRedraw = 1; // drawn the new way from the next frame
        SyncPatches();
    }
    core_log("side panel: %s", pericles ? "pericles (no tabs, names as tooltips)" : "original (the game's own)");
    return true;
}

bool PatchMatches(const Patch &p)
{
    return std::memcmp(At<const BYTE>(p.address), p.game, static_cast<size_t>(p.size)) == 0;
}

// The checks before anything is installed: every site (hooked or called) and every code change with the known
// bytes. arg: the index (sites, then code changes) that the test makes fail.
bool AllMatch(bool (*matches)(const Site &, int), bool (*patchMatches)(const Patch &, int), int arg)
{
    for (int i = 0; i < kSiteCount; ++i) {
        if (!matches(*kSites[i], i == arg ? 1 : 0)) {
            core_log("side panel: %s at 0x%lx does not match; nothing installed, the game's own side panel",
                     kSites[i]->name, static_cast<unsigned long>(kSites[i]->address));
            return false;
        }
    }
    for (int i = 0; i < kPatchCount; ++i) {
        if (!patchMatches(kPatches[i], kSiteCount + i == arg ? 1 : 0)) {
            core_log("side panel: %s at 0x%lx does not match; nothing installed, the game's own side panel",
                     kPatches[i].name, static_cast<unsigned long>(kPatches[i].address));
            return false;
        }
    }
    return true;
}

} // namespace

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    const int i = core::settings::Register(Item{
        "ui", "side_panel", "Side panel",
        "How the side panel looks. pericles: modeled on the side panel of Emperor: Rise of the Middle Kingdom; the "
        "minimap stays on the side panel, in a box shared with the messages (Tab switches them). original: Zeus's "
        "own side panel. Default: pericles.",
        Type::Choice, "pericles", Effect::Live, true, true, 0, 0, 0, nullptr, kChoices, kChoiceLabels, 2});
    core::settings::OnChange(i, &Apply);
}

bool Attach()
{
    if (!AllMatch([](const Site &s, int) { return Matches(s); }, [](const Patch &p, int) { return PatchMatches(p); },
                  -1)) {
        return false;
    }
    if (!Install(kUpdate, &Update_detour, o_Update) || !Install(kButtonText, &ButtonText_detour, o_ButtonText) ||
        !Install(kGroup, &Group_detour, o_Group) || !Install(kDraw, &Draw_detour, o_Draw) ||
        !Install(kNameBar, &NameBar_detour, o_NameBar) || !Install(kPanelDraw, &PanelDraw_detour, o_PanelDraw) ||
        !Install(kPanelInput, &PanelInput_detour, o_PanelInput) ||
        !Install(kReportInput, &ReportInput_detour, o_ReportInput) ||
        !Install(kOverviewToggle, &OverviewToggle_detour, o_OverviewToggle) ||
        !Install(kListDraw, &ListDraw_detour, o_ListDraw) || !Install(kListInput, &ListInput_detour, o_ListInput) ||
        !Install(kMinimapHover, &MinimapHover_detour, o_MinimapHover) ||
        !Install(kArmyContent, &ArmyContent_detour, o_ArmyContent) ||
        !Install(kShipContent, &ShipContent_detour, o_ShipContent) ||
        !Install(kBottomInput, &BottomInput_detour, o_BottomInput) || !Install(kFilter, &Filter_detour, o_Filter) ||
        !Install(kCityLoaded, &CityLoaded_detour, o_CityLoaded) ||
        !Install(kMessagesClear, &MessagesClear_detour, o_MessagesClear) ||
        !Install(kUnitsMinimap, &UnitsMinimap_detour, o_UnitsMinimap)) {
        // The hooks already in stay, but with the feature off every one of them calls the game's own.
        core::diag::Disable(g_feature, "a hook could not be installed");
        core_log("side panel: a hook could not be installed; the game's own side panel");
        return false;
    }
    g_installed = true;
    core_log("side panel: installed (%d hooks); [ui] side_panel %s", kHookCount, g_pericles ? "pericles" : "original");
    return true;
}

bool Active()
{
    return ActiveNow();
}

const Detour *Detours(int *count)
{
    *count = kHookCount;
    return kDetours;
}

int AttachForTest(int mismatch)
{
    g_installed = false;
    if (!AllMatch([](const Site &, int bad) { return bad == 0; }, [](const Patch &, int bad) { return bad == 0; },
                  mismatch)) {
        return 0;
    }
    g_installed = true;
    return kHookCount;
}

int SiteCountForTest()
{
    return kSiteCount + kPatchCount;
}

int PatchCountForTest()
{
    return kPatchCount;
}

void SetTopForTest(const unsigned short *top, const unsigned short *left)
{
    g_top.tried = true;
    g_top.ok = top != nullptr;
    g_top.px.assign(top ? top : nullptr, top ? top + lay::kColumnW * lay::kShift : nullptr);
    g_top.left.assign(left ? left : nullptr, left ? left + lay::kTopCheckW * lay::kTopCheckRows : nullptr);
    g_top.fits = -1;
}

void BackdropDrawing(bool drawing)
{
    g_backdropNow = drawing;
}

void ArmyBackdropDrawn()
{
    SyncPatches(); // the command buttons' and texts' places as the moved panel needs them, before they are drawn
    if (!ActiveNow()) {
        return;
    }
    g_armyLaidOut = true;
    g_noArmyBackdrop = 0;
    if (!core::diag::Guarded(
            g_feature,
            [](void *) {
                Sidebar16 s{};
                if (SidebarOnScreen(&s)) {
                    // The upper half moved up, then its taller inset (the game's own is not drawn); the command
                    // buttons and the texts are drawn over them in the content, in their moved places.
                    lay::MoveUnits(s.px, s.pitch);
                    const int x = *kSidebarX;
                    Inset(RECT{x + lay::kUnitsInsetX, lay::kUnitsInsetY, x + lay::kUnitsInsetX + lay::kUnitsInsetW,
                               lay::kUnitsInsetY + lay::kUnitsInsetH});
                }
                LayOutLowerHalf();
                Function<RotateDraw_fn>(kRotateDraw)(0, 0); // drawn by the game before the rows were laid out
            },
            nullptr)) {
        FallBack("laying out an army panel's lower half");
    }
}

void SetModeForTest(const char *value, bool initial)
{
    Apply(0, value, initial);
}

bool BoxMessagesForTest()
{
    return g_boxMessages;
}

} // namespace zeus::side_panel
