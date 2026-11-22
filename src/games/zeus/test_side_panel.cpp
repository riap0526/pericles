// Offline test of games/zeus/side_panel (not the game): the sidebar's names as the game's tooltips and the name bar
// that only draws the cost.
//
// The rules (games/zeus/side_panel_layout, no game memory):
//   names        every hover number's group and text: the 17 sidebar buttons (Poseidon's Science for Culture, the
//                two tabs), the build buttons (only while they can be used; Poseidon's two renamed buildings), the
//                report button (shown and open); other numbers none
//   report_rule  answering ahead of the game on the report screen: on every report (1 too) and while the screen is
//                redrawn, not with Balloon help off, only for our buttons
//   tip_memo     the group only in the same frame and for the same text; text 0 handed over as the stand-in
//   cost_trail   the same button for many frames: no redraw; another button or none: exactly one
//
// The hooks (the real detours on a copy of the game's address range with fake game functions, one child process per
// scenario as the game layer's test_restore; Zeus.exe 2.1.4.0):
//   sites        a site that does not match (each of them in turn): nothing installed, every detour the game's own
//   city         the city screen: names and groups by hover number, Population drawn with its real text 0 and the
//                stand-in back afterwards, a build button that cannot be used and other numbers the game's own;
//                the editors, no sidebar, other screens and [ui] side_panel = original the game's own
//   report       the report screen: answered ahead of balloon_help_update (report 1 and while redrawn too, not
//                with Balloon help off), the button text there too
//   cost         the cost drawn where the game draws it, screen_redraw only when the cost moves or goes away; no
//                name bar on any screen that draws the sidebar; the game's name bar when not active (the editors)
//   cost_fault   a fault in building_cost: off, the screen redrawn, the game's name bar this frame and every hook the
//                game's own afterwards
//   name_fault   a fault in science_names: off, no text left behind, the game's own afterwards
#include "core/diag/guard.h"
#include "core/log.h"
#include "games/zeus/side_panel.h"
#include "games/zeus/side_panel_layout.h"
#include "games/zeus/site.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

namespace lay = zeus::side_panel_layout;
namespace sp = zeus::side_panel;
using zeus::g_imageShift;

int g_failures = 0;

void Check(bool ok, const char *what)
{
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++g_failures;
    }
}

// ---- the rules ----

lay::Facts Facts(bool poseidon = false, bool reportOpen = false, bool enabled = true, int building = 0x20)
{
    return lay::Facts{poseidon, reportOpen, enabled, building};
}

void Names()
{
    lay::Name n{};
    for (int h = lay::kFirstSidebarButton; h <= lay::kLastSidebarButton; ++h) {
        Check(lay::NameFor(h, Facts(), &n) && n.group == 88 && n.text == h - 0x19, "sidebar button: group 88");
        Check(lay::NameFor(h, Facts(true), &n) && n.group == 88 && n.text == (h == 0x1f ? 0x18 : h - 0x19),
              "sidebar button on a Poseidon map: Culture as Science");
    }
    Check(lay::NameFor(0x28, Facts(), &n) && n.text == 15, "tab Show Info");
    Check(lay::NameFor(0x29, Facts(), &n) && n.text == 16, "tab Show Overview Map");
    for (int h = lay::kFirstBuildButton; h <= lay::kLastBuildButton; ++h) {
        Check(lay::NameFor(h, Facts(false, false, true, 0x30), &n) && n.group == 28 && n.text == 0x30,
              "build button: the building's name");
        Check(!lay::NameFor(h, Facts(false, false, false, 0x30), &n), "build button that cannot be used: none");
    }
    Check(lay::NameFor(0x32, Facts(true, false, true, 0x49), &n) && n.text == 0xd7, "Poseidon: 0x49 as 0xd7");
    Check(lay::NameFor(0x33, Facts(true, false, true, 0x88), &n) && n.text == 0xa4, "Poseidon: 0x88 as 0xa4");
    Check(lay::NameFor(0x33, Facts(false, false, true, 0x88), &n) && n.text == 0x88, "Zeus: 0x88 as it is");
    Check(lay::NameFor(200, Facts(), &n) && n.group == 51 && n.text == 79, "report button: See more information");
    Check(lay::NameFor(200, Facts(false, true), &n) && n.group == 51 && n.text == 80, "report open: Hide full report");
    const int others[] = {0, 10, 0x18, 0x2a, 0x31, 0x36, 100, 101, 199, 201, 1100};
    for (int h : others) {
        Check(!lay::NameFor(h, Facts(), &n) && !lay::Ours(h), "other numbers: none");
    }
}

void ReportRule()
{
    Check(lay::AnswerInReport({false, 2, 9, 0x19}), "report 9, Balloon help on");
    Check(lay::AnswerInReport({false, 1, 7, 200}), "Balloon help SOME");
    Check(lay::AnswerInReport({false, 2, 2, 0x33}), "report 2");
    Check(lay::AnswerInReport({true, 2, 9, 0x19}), "also while redrawn (reports that redraw on every frame)");
    Check(!lay::AnswerInReport({false, 0, 9, 0x19}), "not with Balloon help off");
    Check(lay::AnswerInReport({false, 2, 1, 0x19}), "also on report 1");
    Check(!lay::AnswerInReport({false, 2, 9, 0x12}), "not for the report's own buttons");
}

void TipMemoRule()
{
    lay::TipMemo m;
    int g = 0, t = 0;
    Check(!m.GroupFor(5, 3, &g), "nothing remembered");
    Check(m.Remember(5, {88, 3}) == 3, "text handed over as it is");
    Check(m.GroupFor(5, 3, &g) && g == 88, "same frame, same text: the group");
    Check(!m.GroupFor(6, 3, &g), "another frame: none");
    Check(!m.GroupFor(5, 4, &g), "another text: none");
    Check(!m.RealText(3, &t), "not the stand-in");
    Check(m.Remember(7, {88, 0}) == lay::kStandIn, "text 0 as the stand-in");
    Check(m.RealText(lay::kStandIn, &t) && t == 0, "the stand-in's real text");
    Check(m.GroupFor(7, 0, &g) && g == 88, "text 0's group while drawn");
    m.Forget();
    Check(!m.GroupFor(7, 0, &g) && !m.RealText(lay::kStandIn, &t), "forgotten");
}

void CostTrailRule()
{
    lay::CostTrail c;
    int redraws = 0;
    for (int i = 0; i < 20; ++i) {
        redraws += c.Frame(0x32);
    }
    Check(redraws == 0, "same button: no redraw");
    Check(c.Frame(0x33), "another button: one redraw");
    Check(!c.Frame(0x33), "then none");
    Check(c.Frame(0), "left: one redraw");
    Check(!c.Frame(0) && !c.Frame(0), "then none");
}

// ---- the hooks on a copy of the game's address range ----

constexpr DWORD kImageBase = 0x400000, kImageSize = 0x1211000;

template <typename T> volatile T &At(DWORD address)
{
    return *reinterpret_cast<volatile T *>(address + g_imageShift);
}

void Thunk(DWORD address, const void *target)
{
    BYTE *p = reinterpret_cast<BYTE *>(address + g_imageShift);
    p[0] = 0xe9;
    *reinterpret_cast<DWORD *>(p + 1) = reinterpret_cast<DWORD>(target) - reinterpret_cast<DWORD>(p + 5);
}

constexpr DWORD aGameScreen = 0xf2a6dd, aScreenRedraw = 0xf2a6ce, aEditorMode = 0x665e2c, aNoSidebar = 0x10dff60,
                aHover = 0x781834, aTipText = 0xef8194, aTipKind = 0xef8170, aTipSince = 0xef8178,
                aBalloonHelp = 0xf2a7b4, aDialog = 0x665e34, aReportOpen = 0xfa0428, aFrameCount = 0xf2aac0,
                aFrameTime = 0xf9c0f4, aSidebarX = 0x10dff34, aFontTable = 0x12afbec, aFontCount = 0x12bc820,
                aBuildButtons = 0x63e240, aBuildingOf = 0x5f2710, aNumberSuffix = 0x665e40, aImages = 0x12aebe8;

struct Calls {
    int update, buttonText, group, draw, nameBar, cost, number, imageId, imageDraw;
    int drawnText; // balloon_help_text while the game's drawing ran
    int numberArgs[8];
    void *imageIdThis, *imageDrawThis;
    int imageDrawArgs[6];
};
Calls g_calls;
bool g_poseidon = false;
int g_cost = 0;
bool g_costFaults = false, g_scienceFaults = false;
int g_font[2] = {0, 0x1234};

int __cdecl FakeUpdate()
{
    ++g_calls.update;
    return 0;
}

int __cdecl FakeButtonText(int)
{
    ++g_calls.buttonText;
    At<int>(aTipText) = 0;
    return 0;
}

int __cdecl FakeGroup()
{
    ++g_calls.group;
    return 68;
}

void __cdecl FakeDraw()
{
    ++g_calls.draw;
    g_calls.drawnText = At<int>(aTipText);
}

void __cdecl FakeNameBar()
{
    ++g_calls.nameBar;
}

bool __cdecl FakeScience(int)
{
    if (g_scienceFaults) {
        *reinterpret_cast<volatile int *>(0x10) = 1;
    }
    return g_poseidon;
}

int __cdecl FakeCost(int, int)
{
    ++g_calls.cost;
    if (g_costFaults) {
        *reinterpret_cast<volatile int *>(0x10) = 1;
    }
    return g_cost;
}

void __cdecl FakeNumber(int a, int b, int c, int d, int e, int f, int g, int h)
{
    ++g_calls.number;
    const int args[8] = {a, b, c, d, e, f, g, h};
    std::memcpy(g_calls.numberArgs, args, sizeof(args));
}

int __fastcall FakeImageId(void *self, void *, int index)
{
    ++g_calls.imageId;
    g_calls.imageIdThis = self;
    return index == 0x804 ? 4321 : -1;
}

void __fastcall FakeImageDraw(void *self, void *, int id, int x, int y, int a, int b, int c)
{
    ++g_calls.imageDraw;
    g_calls.imageDrawThis = self;
    const int args[6] = {id, x, y, a, b, c};
    std::memcpy(g_calls.imageDrawArgs, args, sizeof(args));
}

// The detours by name.
template <typename F> F Hook(const char *name)
{
    int n = 0;
    const zeus::Detour *d = sp::Detours(&n);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(d[i].site->name, name) == 0) {
            return reinterpret_cast<F>(d[i].detour);
        }
    }
    std::printf("no detour %s\n", name);
    ExitProcess(3);
}

int Update()
{
    return Hook<int(__cdecl *)()>("balloon_help_update")();
}
int ButtonText(int hover)
{
    return Hook<int(__cdecl *)(int)>("balloon_help_button_text")(hover) & 0xff;
}
int Group()
{
    return Hook<int(__cdecl *)()>("balloon_help_group")();
}
void Draw()
{
    Hook<void(__cdecl *)()>("balloon_help_draw")();
}
void NameBar()
{
    Hook<void(__cdecl *)()>("sidebar_name_bar_draw")();
}

void SetBuildButton(int k, short x, short y, short w, short h, unsigned char state, int building)
{
    const DWORD b = aBuildButtons + static_cast<DWORD>(k * 0x28);
    At<short>(b + 0) = x;
    At<short>(b + 2) = y;
    At<short>(b + 4) = w;
    At<short>(b + 6) = h;
    At<unsigned char>(b + 0x18) = state;
    At<int>(aBuildingOf + static_cast<DWORD>((0x32 + k) * 4)) = building;
}

void Setup()
{
    BYTE *block = static_cast<BYTE *>(VirtualAlloc(nullptr, kImageSize, MEM_COMMIT | MEM_RESERVE,
                                                   PAGE_EXECUTE_READWRITE));
    if (!block) {
        std::printf("cannot allocate the game's address range\n");
        ExitProcess(3);
    }
    g_imageShift = reinterpret_cast<DWORD>(block) - kImageBase;
    Thunk(0x4f6110, reinterpret_cast<void *>(&FakeUpdate));
    Thunk(0x4f5f00, reinterpret_cast<void *>(&FakeButtonText));
    Thunk(0x4f6b60, reinterpret_cast<void *>(&FakeGroup));
    Thunk(0x4f6ba0, reinterpret_cast<void *>(&FakeDraw));
    Thunk(0x517210, reinterpret_cast<void *>(&FakeNameBar));
    Thunk(0x4200f0, reinterpret_cast<void *>(&FakeScience));
    Thunk(0x433d50, reinterpret_cast<void *>(&FakeCost));
    Thunk(0x508f90, reinterpret_cast<void *>(&FakeNumber));
    Thunk(0x40fd10, reinterpret_cast<void *>(&FakeImageId));
    Thunk(0x5acf50, reinterpret_cast<void *>(&FakeImageDraw));
    At<unsigned char>(aGameScreen) = 1;
    At<int>(aBalloonHelp) = 2;
    At<int>(aSidebarX) = 838;
    At<int>(aFrameCount) = 100;
    At<int>(aFrameTime) = 5000;
    At<int>(aFontTable) = static_cast<int>(reinterpret_cast<DWORD>(&g_font[0])); // the font: *(table + 4)
    At<int>(aFontCount) = 0;
    SetBuildButton(0, 50, 294, 58, 42, 1, 0x30);
    SetBuildButton(1, 120, 294, 58, 42, 1, 0x88);
    SetBuildButton(2, 50, 349, 58, 42, 0, 0x31);
    SetBuildButton(3, 120, 349, 58, 42, 1, 0x49);
    sp::SetModeForTest("pericles", true);
}

// A frame where balloon_help_update asked for hover's text: what the game then draws.
struct Tip {
    int answered, stored, group, drawn;
};

Tip Frame(int hover)
{
    At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
    At<int>(aHover) = hover;
    Tip t{};
    t.answered = ButtonText(hover);
    t.stored = At<int>(aTipText);
    g_calls.drawnText = -1;
    if (t.stored != 0) {
        Draw();
        At<int>(aTipText) = g_calls.drawnText; // the drawing's group is asked with the text it draws
        t.group = Group();
        t.drawn = g_calls.drawnText;
        At<int>(aTipText) = 0; // balloon_help_end
    }
    return t;
}

void Sites()
{
    const int n = sp::SiteCountForTest();
    Check(n == 10, "ten sites (five hooked, five called)");
    for (int k = 0; k < n; ++k) {
        Check(sp::AttachForTest(k) == 0, "a site that does not match: nothing installed");
        Check(!sp::Active(), "not active");
        g_calls = {};
        Check(ButtonText(0x19) == 0 && g_calls.buttonText == 1, "button text: the game's own");
        At<int>(aTipText) = 0;
        Check(Group() == 68 && g_calls.group == 1, "group: the game's own");
        NameBar();
        Check(g_calls.nameBar == 1 && g_calls.cost == 0, "name bar: the game's own");
        Update();
        Check(g_calls.update == 1, "update: the game's own");
    }
    Check(sp::AttachForTest(-1) == 5, "all match: five hooks");
    Check(sp::Active(), "active");
}

void City()
{
    sp::AttachForTest(-1);
    g_calls = {};
    Tip t = Frame(0x19); // Population: text 0
    Check(t.answered == 1 && t.stored == lay::kStandIn, "Population: the stand-in handed over");
    Check(t.drawn == 0 && t.group == 88, "Population: drawn as text 0 of group 88");
    Check(g_calls.draw == 1 && g_calls.buttonText == 0, "the game's drawing once, its button text not asked");
    t = Frame(0x1a);
    Check(t.answered && t.stored == 1 && t.drawn == 1 && t.group == 88, "Husbandry");
    t = Frame(0x1f);
    Check(t.stored == 6 && t.group == 88, "Culture");
    g_poseidon = true;
    t = Frame(0x1f);
    Check(t.stored == 0x18 && t.group == 88, "Poseidon: Science");
    t = Frame(0x33);
    Check(t.stored == 0xa4 && t.group == 28, "Poseidon: building 0x88 as 0xa4");
    t = Frame(0x35);
    Check(t.stored == 0xd7 && t.group == 28, "Poseidon: building 0x49 as 0xd7");
    g_poseidon = false;
    t = Frame(0x32);
    Check(t.stored == 0x30 && t.group == 28, "building name");
    t = Frame(0x28);
    Check(t.stored == 15 && t.group == 88, "tab Show Info");
    g_calls = {};
    t = Frame(0x34);
    Check(t.answered == 0 && t.stored == 0 && g_calls.buttonText == 1, "build button that cannot be used: the game's");
    t = Frame(200);
    Check(t.stored == 79 && t.group == 51, "report button");
    At<unsigned char>(aReportOpen) = 1;
    t = Frame(200);
    Check(t.stored == 80 && t.group == 51, "report open: Hide full report");
    At<unsigned char>(aReportOpen) = 0;
    g_calls = {};
    t = Frame(0x12);
    Check(t.answered == 0 && g_calls.buttonText == 1, "another number: the game's own");

    // The group only in the frame and for the text it was given.
    Frame(0x1a);
    At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
    At<int>(aTipText) = 1;
    Check(Group() == 68, "the next frame: the game's group");
    ButtonText(0x1a);
    At<int>(aTipText) = 2;
    Check(Group() == 68, "another text: the game's group");
    At<int>(aTipText) = 0;

    // Not active: the game's own.
    struct Case {
        DWORD address;
        int value;
        const char *what;
    };
    const Case cases[] = {{aEditorMode, 1, "editor: the game's own"},
                          {aNoSidebar, 1, "no sidebar: the game's own"}};
    for (const Case &c : cases) {
        At<int>(c.address) = c.value;
        g_calls = {};
        t = Frame(0x1a);
        Check(t.answered == 0 && g_calls.buttonText == 1, c.what);
        At<int>(c.address) = 0;
    }
    At<unsigned char>(aGameScreen) = 9;
    g_calls = {};
    t = Frame(0x1a);
    Check(t.answered == 0 && g_calls.buttonText == 1, "another screen: the game's own");
    At<unsigned char>(aGameScreen) = 1;
    At<unsigned char>(aScreenRedraw) = 0;
    sp::SetModeForTest("original", false);
    Check(At<unsigned char>(aScreenRedraw) == 1, "switched to original: the screen drawn again");
    g_calls = {};
    t = Frame(0x1a);
    Check(t.answered == 0 && g_calls.buttonText == 1, "original: the game's own");
    NameBar();
    Check(g_calls.nameBar == 1, "original: the game's name bar");
    sp::SetModeForTest("pericles", false);
    t = Frame(0x1a);
    Check(t.answered == 1, "back to pericles");
}

void Report()
{
    sp::AttachForTest(-1);
    At<unsigned char>(aGameScreen) = 7;
    At<int>(aDialog) = 9;
    At<int>(aHover) = 0x19;
    At<int>(aTipKind) = 4;
    At<int>(aTipSince) = 0;
    g_calls = {};
    Check(Update() == 1 && g_calls.update == 0, "report 9: answered ahead of the game");
    Check(At<int>(aTipText) == lay::kStandIn && At<int>(aTipKind) == 0 && At<int>(aTipSince) == 5000,
          "text, kind 0 and the time as the game's answer");
    At<int>(aTipText) = 0;
    At<int>(aDialog) = 1;
    Check(Update() == 1 && g_calls.update == 0 && At<int>(aTipText) == lay::kStandIn, "report 1: answered too");
    At<int>(aTipText) = 0;
    At<int>(aDialog) = 6;
    At<unsigned char>(aScreenRedraw) = 1; // the trade report sets it on every frame it draws
    Check(Update() == 1 && g_calls.update == 0 && At<int>(aTipText) == lay::kStandIn, "while redrawn: answered too");
    Check(At<unsigned char>(aScreenRedraw) == 1, "screen_redraw left as it was");
    At<int>(aTipText) = 0;
    At<unsigned char>(aScreenRedraw) = 0;
    At<int>(aBalloonHelp) = 0;
    Check(Update() == 0 && g_calls.update == 1, "Balloon help off: the game's own");
    At<int>(aBalloonHelp) = 1;
    At<int>(aHover) = 0x34; // cannot be used
    Check(Update() == 0 && g_calls.update == 2, "a build button that cannot be used: the game's own");
    At<int>(aHover) = 0x12;
    Check(Update() == 0 && g_calls.update == 3, "the report's own button: the game's own");
    Tip t = Frame(200);
    Check(t.stored == 79 && t.group == 51, "the button text on the report screen");
    At<unsigned char>(aGameScreen) = 1;
    g_calls = {};
    Update();
    Check(g_calls.update == 1, "the city screen: the game's update");
}

void Cost()
{
    sp::AttachForTest(-1);
    g_cost = 30;
    At<unsigned char>(aScreenRedraw) = 0;
    At<int>(aHover) = 0x32;
    g_calls = {};
    for (int i = 0; i < 10; ++i) {
        NameBar();
    }
    Check(g_calls.nameBar == 0 && g_calls.cost == 10 && g_calls.number == 10 && g_calls.imageDraw == 10,
          "the cost drawn on every frame, no name bar");
    Check(At<unsigned char>(aScreenRedraw) == 0, "no redraw while on the same button");
    const int *a = g_calls.numberArgs;
    Check(a[0] == 30 && a[1] == 0x40 && a[2] == static_cast<int>(aNumberSuffix + g_imageShift) && a[3] == 838 + 50 &&
              a[4] == 294 + 42 + 3 + 0x1e && a[5] == 58 && a[6] == 0x1234 && a[7] == 0,
          "number_draw's arguments as the game's");
    const int *d = g_calls.imageDrawArgs;
    Check(d[0] == 4321 && d[1] == 838 + 50 && d[2] == 294 + 42 + 3 + 0x17 && d[3] == 0 && d[4] == 0 && d[5] == 0,
          "the coin where the game draws it");
    Check(g_calls.imageIdThis == reinterpret_cast<void *>(aImages + g_imageShift) &&
              g_calls.imageDrawThis == reinterpret_cast<void *>(aImages + g_imageShift),
          "the image manager as this");
    At<int>(aHover) = 0x33;
    NameBar();
    Check(At<unsigned char>(aScreenRedraw) == 1, "another button: one redraw");
    At<unsigned char>(aScreenRedraw) = 0;
    NameBar();
    NameBar();
    Check(At<unsigned char>(aScreenRedraw) == 0, "then none");
    At<int>(aHover) = 0x19;
    NameBar();
    Check(At<unsigned char>(aScreenRedraw) == 1, "left: one redraw");
    At<unsigned char>(aScreenRedraw) = 0;
    NameBar();
    Check(At<unsigned char>(aScreenRedraw) == 0, "then none");
    g_cost = 0;
    At<int>(aHover) = 0x32;
    g_calls = {};
    NameBar();
    Check(g_calls.number == 0 && At<unsigned char>(aScreenRedraw) == 0, "no cost: nothing drawn, no redraw");
    g_cost = 30;
    At<int>(aHover) = 0x34;
    NameBar();
    Check(g_calls.cost == 1 && g_calls.number == 0, "cannot be used: no cost");
    const unsigned char others[] = {3, 5, 6, 9, 0x11}; // drop-down menu, help, build menu, info window, messages
    for (unsigned char screen : others) {
        At<unsigned char>(aGameScreen) = screen;
        At<int>(aHover) = 0x12;
        g_calls = {};
        NameBar();
        Check(g_calls.nameBar == 0 && g_calls.number == 0, "another screen with the sidebar: no name bar");
        At<int>(aHover) = 0x32;
        NameBar();
        Check(g_calls.nameBar == 0 && g_calls.number == 1, "another screen: the cost as the game draws it");
        At<int>(aHover) = 0;
        NameBar();
    }
    At<unsigned char>(aScreenRedraw) = 0;
    At<int>(aEditorMode) = 1;
    g_calls = {};
    NameBar();
    Check(g_calls.nameBar == 1 && g_calls.cost == 0, "the editors: the game's name bar");
    At<int>(aEditorMode) = 0;
    At<unsigned char>(aGameScreen) = 7;
    At<int>(aHover) = 0x32;
    g_calls = {};
    NameBar();
    Check(g_calls.nameBar == 0 && g_calls.number == 1, "the report screen: the cost");
}

void AfterFault(const char *what)
{
    Check(!sp::Active(), what);
    Check(At<unsigned char>(aScreenRedraw) == 1, "the screen drawn again");
    g_calls = {};
    Tip t = Frame(0x1a);
    Check(t.answered == 0 && g_calls.buttonText == 1, "afterwards the button text is the game's");
    At<int>(aTipText) = lay::kStandIn;
    Check(Group() == 68, "afterwards the group is the game's");
    At<int>(aTipText) = 0;
    NameBar();
    Check(g_calls.nameBar == 1, "afterwards the name bar is the game's");
    At<unsigned char>(aGameScreen) = 7;
    At<int>(aDialog) = 9;
    At<int>(aHover) = 0x19;
    Update();
    Check(g_calls.update == 1, "afterwards the update is the game's");
}

void CostFault()
{
    sp::AttachForTest(-1);
    g_cost = 30;
    g_costFaults = true;
    At<unsigned char>(aScreenRedraw) = 0;
    At<int>(aHover) = 0x32;
    g_calls = {};
    NameBar();
    Check(g_calls.nameBar == 1, "the game's name bar in the frame of the fault");
    g_costFaults = false;
    AfterFault("a fault in building_cost: off");
}

void NameFault()
{
    sp::AttachForTest(-1);
    g_scienceFaults = true;
    At<unsigned char>(aScreenRedraw) = 0;
    g_calls = {};
    At<int>(aHover) = 0x19;
    const int r = ButtonText(0x19);
    Check(r == 0 && g_calls.buttonText == 1 && At<int>(aTipText) == 0, "the game's answer, no text of ours left");
    g_scienceFaults = false;
    AfterFault("a fault in science_names: off");
}

struct Scenario {
    const char *name;
    void (*run)();
};

const Scenario kScenarios[] = {
    {"names", &Names},       {"report_rule", &ReportRule}, {"tip_memo", &TipMemoRule}, {"cost_trail", &CostTrailRule},
    {"sites", &Sites},       {"city", &City},              {"report", &Report},        {"cost", &Cost},
    {"cost_fault", &CostFault}, {"name_fault", &NameFault},
};

} // namespace

int main(int argc, char **argv)
{
    if (argc >= 2) {
        for (const Scenario &s : kScenarios) {
            if (std::strcmp(argv[1], s.name) == 0) {
                DeleteFileA("test_side_panel.log");
                core_log_open(GetModuleHandleW(nullptr), "test_side_panel.log");
                core::diag::InstallExceptionLog();
                Setup();
                s.run();
                return g_failures ? 1 : 0;
            }
        }
        std::printf("unknown scenario %s\n", argv[1]);
        return 2;
    }
    char self[MAX_PATH];
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    int failed = 0;
    for (const Scenario &s : kScenarios) {
        std::printf("%s\n", s.name);
        std::fflush(stdout);
        char cmd[MAX_PATH + 64];
        std::snprintf(cmd, sizeof(cmd), "\"%s\" %s", self, s.name);
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        DWORD code = 99;
        if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        if (code != 0) {
            std::printf("  scenario %s FAILED (exit code %lu)\n", s.name, code);
            ++failed;
        }
    }
    std::printf("RESULT: %s (%d scenario(s) failed)\n", failed ? "FAIL" : "PASS", failed);
    return failed ? 1 : 0;
}
