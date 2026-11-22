#include "games/zeus/side_panel.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/side_panel_layout.h"

#include <cstring>

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

const Site *const kSites[] = {&kUpdate,  &kButtonText, &kGroup,  &kDraw,    &kNameBar,
                              &kScienceNames, &kCost,  &kNumber, &kImageId, &kImageDraw};
constexpr int kSiteCount = static_cast<int>(sizeof(kSites) / sizeof(kSites[0]));

Int_fn o_Update = nullptr;
ButtonText_fn o_ButtonText = nullptr;
Int_fn o_Group = nullptr;
Void_fn o_Draw = nullptr;
Void_fn o_NameBar = nullptr;

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

core::diag::Feature g_feature{"side panel (games/zeus/side_panel)"};
bool g_installed = false;
bool g_pericles = true;
bool g_fellBack = false;
lay::TipMemo g_memo;
lay::CostTrail g_trail;

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

// After a fault: the game's own side panel from now on (the feature is off), the screen drawn again.
void FallBack(const char *why)
{
    core::diag::Disable(g_feature, why);
    g_memo.Forget();
    g_trail.Reset();
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

const Detour kDetours[] = {
    {&kUpdate, reinterpret_cast<void *>(&Update_detour)},
    {&kButtonText, reinterpret_cast<void *>(&ButtonText_detour)},
    {&kGroup, reinterpret_cast<void *>(&Group_detour)},
    {&kDraw, reinterpret_cast<void *>(&Draw_detour)},
    {&kNameBar, reinterpret_cast<void *>(&NameBar_detour)},
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
    }
    core_log("side panel: %s", pericles ? "pericles (names as tooltips)" : "original (the game's own)");
    return true;
}

// The checks before anything is installed: every site (hooked or called) with the known bytes.
bool AllMatch(bool (*matches)(const Site &, int), int arg)
{
    for (int i = 0; i < kSiteCount; ++i) {
        if (!matches(*kSites[i], i == arg ? 1 : 0)) {
            core_log("side panel: %s at 0x%lx does not match; nothing installed, the game's own side panel",
                     kSites[i]->name, static_cast<unsigned long>(kSites[i]->address));
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
    if (!AllMatch([](const Site &s, int) { return Matches(s); }, -1)) {
        return false;
    }
    if (!Install(kUpdate, &Update_detour, o_Update) || !Install(kButtonText, &ButtonText_detour, o_ButtonText) ||
        !Install(kGroup, &Group_detour, o_Group) || !Install(kDraw, &Draw_detour, o_Draw) ||
        !Install(kNameBar, &NameBar_detour, o_NameBar)) {
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
    if (!AllMatch([](const Site &, int bad) { return bad == 0; }, mismatch)) {
        return 0;
    }
    g_installed = true;
    return kHookCount;
}

int SiteCountForTest()
{
    return kSiteCount;
}

void SetModeForTest(const char *value, bool initial)
{
    Apply(0, value, initial);
}

} // namespace zeus::side_panel
