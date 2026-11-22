#include "games/zeus/side_panel.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/side_panel_layout.h"

#include "core/assets/sg3.h"

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

const Site *const kSites[] = {&kUpdate,     &kButtonText,   &kGroup,        &kDraw,         &kNameBar,
                              &kPanelDraw,  &kPanelInput,   &kReportInput,  &kOverviewToggle,
                              &kScienceNames, &kCost,       &kNumber,       &kImageId,      &kImageDraw,
                              &kOverlayDraw, &kInnerPanel,  &kCategoryDraw, &kBuildDraw,    &kText,
                              &kOverlayInput, &kCategoryInput, &kBuildInput, &kButtonsHover, &kButtonsUpdate};
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
};
#undef TABS_PUSH
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
constexpr int kRecordSize = 0x28, kRecordState = 0x18, kRecordValue = 0x1c;
constexpr int kGroupSidebar = 88, kScienceText = 0x18, kPanelX = 0x32, kPanelCols = 8;
constexpr int kTopEntry = 1233; // Zeus_Interface: the army panel's picture, 186 wide; its rows 0..24 are the
                                // city sidebar's without the tabs
constexpr wchar_t kInterfacePack[] = L"DATA\\Zeus_Interface.sg3"; // next to Zeus.exe

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
    *kScreenRedraw = 1;
    core_log("side panel: %s", want ? "active (no tabs, the column moved up)" : "the game's own side panel here");
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

struct ColumnCall {
    bool game;   // the game's own drawing this frame (no backdrop yet, or not on screen)
    bool giveUp; // the backdrop never comes (sidebar_draw is not hooked): the game's own side panel
};
// Frames in a row that asked for the backdrop and did not get it: one redraw brings it, so a few more mean that
// sidebar_draw is not seen (its hook is not installed).
int g_noBackdrop = 0;
constexpr int kNoBackdropLimit = 8;

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

// Tab (and the tabs, no longer shown): there is only the Info tab.
void __cdecl OverviewToggle_detour()
{
    if (ActiveNow()) {
        return;
    }
    Original(o_OverviewToggle, kOverviewToggle)();
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
        "How the side panel looks. pericles: Pericles's style, modeled on the side panel of Emperor: Rise of the "
        "Middle Kingdom: no tabs and no picture; the minimap and the messages share a box at the bottom (switch with "
        "the button on the box or with Tab); category, building and report names show as tooltips, which follow "
        "Help > Balloon help; click the title to open its report; the army panel has no minimap of its own. "
        "original: Zeus's own side panel. Default: pericles.",
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
        !Install(kOverviewToggle, &OverviewToggle_detour, o_OverviewToggle)) {
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

void SetModeForTest(const char *value, bool initial)
{
    Apply(0, value, initial);
}

} // namespace zeus::side_panel
