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
//
// The column moved up (no tabs):
//   column_rules       the information panel's height (its bottom at the last moved row), the fonts' entries, the
//                      title's rule (11 categories x a report x Poseidon), the title's and the moved area's
//                      rectangles, whether entry 1233 fits the backdrop
//   column_move        a column on its own: put back, drawn, moved, frame after frame the same (no drift, no trail),
//                      the marble in place (entry 1233's above, or the column's own), nothing drawn below
//   column             on a 1024 x 768 screen with the game's drawing faked in its place: the code changed, the
//                      backdrop taken when drawn and put back after, what is where (moved 25 rows, the build buttons
//                      and the marble where they are), the same picture over frames, the Overview category, the
//                      title's text and font, another screen size, the editors and original
//   column_no_backdrop the backdrop never seen drawn: off after a few frames, the code back
//   input              the title as the report button (its record, hover number 200, clicked or not, no report), the
//                      panels given the mouse 25 rows lower and the mouse put back, the report screen, the army panel
//   column_fault       a fault in the column's drawing: off, the code back, the game's own afterwards
//   code_sites         a site or a code change that does not match (each in turn): nothing installed
//
// The box at the bottom:
//   box_rules  the toggle's names, the box's places, the game's wrapping (the limit, a word that does not fit, $, a
//              word wider than the line), a message that fits, the message under a row, the lower half's pixels
//   box        on the 1024 x 768 screen: the buttons' y moved, the lower half laid out, the inset, the minimap and
//              the toggle (lit, its tooltips), the messages (which, where, as many as fit, scrolled, the arrows), the
//              messages' input (the one under the mouse, between two, the arrows first), the minimap's hover and
//              click, the toggle as a button, a Condensed message (only city_message_post's question), a city loaded
//              or started, original and the editors
//   box_tall   taller than 768: the inset panel below 768, the edge down to the bottom
//   army       an army or a ship panel: laid out after its backdrop, the gem again, the box after its content, the
//              panel's own minimap place kept and put back around a click on the box's, its messages clicked
//   army_no_backdrop  an army panel's backdrop never laid out: off after a few frames
//   box_fault  a fault in the box's drawing: off, the code and data back, the game's own afterwards
#include "core/diag/guard.h"
#include "core/log.h"
#include "games/zeus/side_panel.h"
#include "games/zeus/side_panel_layout.h"
#include "games/zeus/site.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

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
    Check(!lay::NameFor(200, Facts(false, true), &n), "report open: the title has no tooltip");
    const int others[] = {0, 10, 0x18, 0x2a, 0x31, 0x36, 100, 101, 199, 202, 1100};
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
    return index == 0x804 ? 4321 : index == 0x20c ? 9100 : -1; // the coin; the report button's picture
}

struct Drawn {
    int id, x, y;
};
std::vector<Drawn> g_drawn; // every image_draw

void __fastcall FakeImageDraw(void *self, void *, int id, int x, int y, int a, int b, int c)
{
    g_drawn.push_back(Drawn{id, x, y});
    ++g_calls.imageDraw;
    g_calls.imageDrawThis = self;
    const int args[6] = {id, x, y, a, b, c};
    std::memcpy(g_calls.imageDrawArgs, args, sizeof(args));
}

// ---- the column moved up: a 1024 x 768 screen and the game's drawing in its own place ----

constexpr DWORD aOverview = 0xfa0000, aCategory = 0xfa0004, aOverlayCount = 0xf9fff8, aButtonsRedraw = 0xf2a6cd,
                aMouseX = 0xf2a738, aMouseY = 0xf2a734, aHoverY = 0xf2a72c, aDrawBuffer = 0xf9c100,
                aPitch = 0xf2a698, aDdWidth = 0xf2a6a0, aDdHeight = 0xf2a69c, aReportKinds = 0x63e488,
                aReportRecord = 0x63dee8;
constexpr int kW = 1024, kH = 768, kSx = 838;
std::vector<unsigned short> g_screen(static_cast<size_t>(kW) * 1024); // up to 1024 rows (a taller screen)
constexpr unsigned short kOverlayPx = 0x1111, kPanelPx = 0x2222, kCategoryPx = 0x3333, kBuildPx = 0x4444;

unsigned short &Px(int x, int y)
{
    return g_screen[static_cast<size_t>(y) * kW + x];
}

// The sidebar's backdrop as the game draws it: every pixel different from its neighbours above and below.
unsigned short BackdropPx(int x, int y)
{
    return static_cast<unsigned short>(0x8000 | ((y * 131 + x * 7) & 0x7fff));
}
unsigned short TopPx(int col, int y) // entry 1233's rows 0..24
{
    return static_cast<unsigned short>(0x0100 + y * 3 + col);
}

void DrawBackdrop()
{
    for (int y = 0; y < kH; ++y) {
        for (int x = kSx; x < kW; ++x) {
            Px(x, y) = BackdropPx(x, y);
        }
    }
}

void Fill(int x, int y, int w, int h, unsigned short p)
{
    for (int yy = y; yy < y + h && yy < kH; ++yy) {
        for (int xx = x; xx < x + w && xx < kW; ++xx) {
            Px(xx, yy) = p;
        }
    }
}

struct ColumnCalls {
    int panelDraw, overlayDraw, inner, categoryDraw, buildDraw, text, overlayInput, categoryInput, buildInput,
        hover, update, panelInput, reportInput, toggle;
    int innerArgs[4], textArgs[7], hoverArgs[5], updateArgs[4];
    int overlayMouseY, categoryMouseY;
    BYTE record[0x28]; // the title's record as buttons_update saw it
};
ColumnCalls g_col;
int g_overlayButtons = 2;
bool g_onlyWhenRedrawn = true; // the overlay buttons as the game draws them: only with sidebar_buttons_redraw
bool g_drawBelow = false;
int g_updateResult = 0, g_buildInputResult = 0;
bool g_overlayFaults = false;
int *g_fontsForColumn = nullptr; // 16 font entries (the column's scenarios)

int __cdecl FakePanelDraw()
{
    ++g_col.panelDraw;
    return 1;
}
void __cdecl FakeOverlayDraw()
{
    ++g_col.overlayDraw;
    At<int>(aOverlayCount) = g_overlayButtons;
    if (g_overlayFaults) {
        *reinterpret_cast<volatile int *>(0x10) = 1;
    }
    if (!g_onlyWhenRedrawn || At<unsigned char>(aButtonsRedraw) != 0) {
        Fill(kSx + 50, 50, 130, 20, kOverlayPx); // the first overlay button
    }
}
void __cdecl FakeInner(int x, int y, int cols, int rows)
{
    ++g_col.inner;
    const int a[4] = {x, y, cols, rows};
    std::memcpy(g_col.innerArgs, a, sizeof(a));
    Fill(x, y, cols * 16, rows * 16, kPanelPx);
}
void __cdecl FakeCategoryDraw()
{
    ++g_col.categoryDraw;
    Fill(kSx + 60, 200, 40, 10, kCategoryPx);
    if (g_drawBelow) {
        Fill(kSx + 60, 340, 4, 4, kCategoryPx);
    }
}
void __cdecl FakeBuildDraw()
{
    ++g_col.buildDraw;
    Fill(kSx + 50, 324, 58, 42, kBuildPx);
}
int __cdecl FakeText(int group, int text, int x, int y, int w, int font, int flags)
{
    ++g_col.text;
    const int a[7] = {group, text, x, y, w, font, flags};
    std::memcpy(g_col.textArgs, a, sizeof(a));
    return 0;
}
int __cdecl FakeOverlayInput()
{
    ++g_col.overlayInput;
    g_col.overlayMouseY = At<int>(aMouseY);
    At<int>(aHoverY) = At<int>(aMouseY); // as buttons_hover copies the mouse
    return 0;
}
int __cdecl FakeCategoryInput()
{
    ++g_col.categoryInput;
    g_col.categoryMouseY = At<int>(aMouseY);
    return 0;
}
int __cdecl FakeBuildInput()
{
    ++g_col.buildInput;
    return g_buildInputResult;
}
int __cdecl FakeHover(int x, int y, void *table, int count, int first)
{
    ++g_col.hover;
    const int a[5] = {x, y, static_cast<int>(reinterpret_cast<DWORD>(table)), count, first};
    std::memcpy(g_col.hoverArgs, a, sizeof(a));
    const short *r = static_cast<const short *>(table);
    const int mx = At<int>(aMouseX), my = At<int>(aMouseY);
    if (mx >= x + r[0] && mx < x + r[0] + r[2] && my >= y + r[1] && my < y + r[1] + r[3]) {
        At<int>(aHover) = first;
        return 1;
    }
    return 0;
}
int __cdecl FakeUpdate4(int x, int y, void *table, int count)
{
    ++g_col.update;
    const int a[4] = {x, y, static_cast<int>(reinterpret_cast<DWORD>(table)), count};
    std::memcpy(g_col.updateArgs, a, sizeof(a));
    std::memcpy(g_col.record, table, sizeof(g_col.record));
    return g_updateResult;
}
int __cdecl FakePanelInput()
{
    ++g_col.panelInput;
    return 0;
}
int __cdecl FakeReportInput()
{
    ++g_col.reportInput;
    return 0;
}
void __cdecl FakeToggle()
{
    ++g_col.toggle;
}

// ---- the box at the bottom: the game's functions it calls or replaces ----

constexpr DWORD aMessageCount = 0xfd37f0, aMessages = 0xfd3848, aMessageCurrent = 0xfd37e4, aMessageText = 0x916af8,
                aWrapHeight = 0xf13a6c, aLineHeight = 0xf13960, aMinimapHovered = 0x78181c, aMousePressed = 0xf2a707,
                aList = 0x7815a0, aArrows = 0x5ed1a8, aMinimapArgs = 0x7d3b08, aMinimapRect = 0x7d3b20,
                aImageGroups = 0x12bc82c, aMessageOptions = 0xf2a760;
constexpr int kLineH = 11;          // text_line_height of the fake text drawing
constexpr int kInsetBase = 0x4000 + 7000; // the inset tiles' first id (the fake group entry's +4 is 7000)

struct BoxCalls {
    int listDraw, listInput, minimapHover, armyContent, shipContent, bottomInput, cityLoaded, messagesClear;
    int minimapDraw, minimapClick, textId, textGet, wrapped, imageButtons, repeat, open, rotate, imageGroup;
    int filter, filterThisBad;
    int minimapArgs[4];
    int clickLeft;        // the minimap's left edge (0x7d3b2c) when minimap_click ran
    int openIndex;
    int wrappedX[16], wrappedY[16], wrappedText[16];
    int arrowY[4], repeatY[4];
};
BoxCalls g_box;
int g_bottomResult = 0, g_clickResult = 0, g_repeatResult = 0;
bool g_minimapFaults = false;
int g_filterAnswer[16];  // message_filter's answer by message
int g_messageWords[16];  // each message's text: this many words of 5 letters (30 px each with a space)

int MessageIndex(const void *m)
{
    return static_cast<int>((reinterpret_cast<DWORD>(m) - g_imageShift - aMessages) / 0x3c);
}

void __fastcall FakeListDraw(void *, void *)
{
    ++g_box.listDraw;
}
void __fastcall FakeListInput(void *, void *)
{
    ++g_box.listInput;
}
void __cdecl FakeMinimapHover() // as the game's: the mouse in the place last drawn
{
    ++g_box.minimapHover;
    const int mx = At<int>(aMouseX), my = At<int>(aMouseY);
    At<int>(aMinimapHovered) = mx >= At<int>(aMinimapRect + 12) && mx < At<int>(aMinimapRect + 4) &&
                               my >= At<int>(aMinimapRect + 8) && my < At<int>(aMinimapRect + 0);
}
void __cdecl FakeArmyContent()
{
    ++g_box.armyContent;
}
void __cdecl FakeShipContent()
{
    ++g_box.shipContent;
}
int __cdecl FakeBottomInput()
{
    ++g_box.bottomInput;
    return g_bottomResult;
}
void __cdecl FakeCityLoaded()
{
    ++g_box.cityLoaded;
}
void __cdecl FakeMessagesClear()
{
    ++g_box.messagesClear;
}
void __cdecl FakeMinimapDraw(int x, int y, int cols, int rows) // its place written as the game does
{
    ++g_box.minimapDraw;
    if (g_minimapFaults) {
        *reinterpret_cast<volatile int *>(0x10) = 1;
    }
    const int a[4] = {x, y, cols, rows};
    std::memcpy(g_box.minimapArgs, a, sizeof(a));
    At<int>(aMinimapArgs + 12) = x;
    At<int>(aMinimapArgs + 8) = y;
    At<int>(aMinimapArgs + 4) = cols;
    At<int>(aMinimapArgs + 0) = rows;
    At<int>(aMinimapRect + 12) = x;            // left
    At<int>(aMinimapRect + 8) = y;             // top
    At<int>(aMinimapRect + 4) = x + cols * 2;  // right
    At<int>(aMinimapRect + 0) = y + rows;      // bottom
    At<int>(aMinimapRect + 16) = 77;           // its origin
}
int __cdecl FakeMinimapClick(int)
{
    ++g_box.minimapClick;
    g_box.clickLeft = At<int>(aMinimapRect + 12);
    return At<int>(aMinimapHovered) == 1 ? g_clickResult : 0;
}
int __fastcall FakeFilter(void *self, void *, void *m)
{
    ++g_box.filter;
    if (reinterpret_cast<DWORD>(self) != aMessageOptions + g_imageShift) {
        ++g_box.filterThisBad;
    }
    return g_filterAnswer[MessageIndex(m) & 15];
}
int __cdecl FakeTextId(int text)
{
    ++g_box.textId;
    return text == 99 ? -1 : text;
}
void __cdecl FakeTextGet(int text, char *to, int size)
{
    ++g_box.textGet;
    const int words = g_messageWords[text & 15];
    int n = 0;
    for (int w = 0; w < words && n + 6 < size; ++w) {
        if (w > 0) {
            to[n++] = ' ';
        }
        for (int k = 0; k < 5; ++k) {
            to[n++] = static_cast<char>('a' + text);
        }
    }
    to[n] = 0;
    // The message whose values are in the text: the one asked for.
    g_box.wrappedText[15] = At<int>(aMessageCurrent);
}
int FakeWidth(unsigned char, void *)
{
    return 6;
}
int __cdecl FakeCharWidth(int, int)
{
    return 6;
}
void __cdecl FakeWrapped(const char *text, int x, int y, int w, int, int)
{
    const int k = g_box.wrapped++ & 15;
    g_box.wrappedX[k] = x;
    g_box.wrappedY[k] = y;
    g_box.wrappedText[k] = text[0];
    At<int>(aLineHeight) = kLineH;
    const int lines = lay::WrapLines(reinterpret_cast<const unsigned char *>(text), w, &FakeWidth, nullptr);
    At<int>(aWrapHeight) = At<int>(aWrapHeight) + lines * (5 + kLineH);
}
void __cdecl FakeImageButtons(int, int y, void *table, int)
{
    const int k = g_box.imageButtons++ & 3;
    g_box.arrowY[k] = y + reinterpret_cast<short *>(table)[1]; // where the arrow is drawn (its record's y + y)
}
int __cdecl FakeRepeat(int, int y, void *table, int)
{
    const int k = g_box.repeat++ & 3;
    g_box.repeatY[k] = y + reinterpret_cast<short *>(table)[1];
    return g_repeatResult;
}
void __cdecl FakeOpen(int index)
{
    ++g_box.open;
    g_box.openIndex = index;
}
void __cdecl FakeRotate(int, int)
{
    ++g_box.rotate;
}
int g_insetEntry[2] = {0, 7000};
BYTE *__fastcall FakeImageGroup(void *, void *, int group)
{
    ++g_box.imageGroup;
    return group == 0x84 ? reinterpret_cast<BYTE *>(g_insetEntry) : nullptr;
}

// The code the side panel changes, as the game has it.
struct GameBytes {
    DWORD at;
    BYTE b[9];
    int n;
    BYTE ours1; // the byte that becomes ours (offset 1 for the pushes, 6 for the compare)
    int oursAt;
};
const GameBytes kGameBytes[] = {
    {0x515138, {0x6a, 0x11, 0x68, 0x40, 0xdc, 0x63, 0x00}, 7, 0x0f, 1},
    {0x515154, {0x6a, 0x11, 0x68, 0x40, 0xdc, 0x63, 0x00}, 7, 0x0f, 1},
    {0x51535f, {0x6a, 0x11, 0x68, 0x40, 0xdc, 0x63, 0x00}, 7, 0x0f, 1},
    {0x517ca3, {0x6a, 0x11, 0x68, 0x40, 0xdc, 0x63, 0x00}, 7, 0x0f, 1},
    {0x5150d7, {0x83, 0x3d, 0x04, 0x00, 0xfa, 0x00, 0x0a, 0x74, 0x10}, 9, 0x7f, 6},
    {0x63e04a, {0x1f, 0x02}, 2, 0xc7, 0}, // the check, globe and gem buttons' y: 543 -> 711
    {0x63e072, {0x1f, 0x02}, 2, 0xc7, 0},
    {0x63e09a, {0x1f, 0x02}, 2, 0xc7, 0},
};

// Every changed byte is ours (true) or every byte the game's (false).
bool CodeIs(bool ours)
{
    for (const GameBytes &g : kGameBytes) {
        for (int i = 0; i < g.n; ++i) {
            const BYTE want = ours && i == g.oursAt ? g.ours1 : g.b[i];
            if (At<BYTE>(g.at + static_cast<DWORD>(i)) != want) {
                return false;
            }
        }
    }
    return true;
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
    Thunk(0x517480, reinterpret_cast<void *>(&FakePanelDraw));
    Thunk(0x5178e0, reinterpret_cast<void *>(&FakePanelInput));
    Thunk(0x517580, reinterpret_cast<void *>(&FakeReportInput));
    Thunk(0x516d30, reinterpret_cast<void *>(&FakeToggle));
    Thunk(0x5170a0, reinterpret_cast<void *>(&FakeOverlayDraw));
    Thunk(0x59f2a0, reinterpret_cast<void *>(&FakeInner));
    Thunk(0x517000, reinterpret_cast<void *>(&FakeCategoryDraw));
    Thunk(0x5171f0, reinterpret_cast<void *>(&FakeBuildDraw));
    Thunk(0x508d40, reinterpret_cast<void *>(&FakeText));
    Thunk(0x517620, reinterpret_cast<void *>(&FakeOverlayInput));
    Thunk(0x517650, reinterpret_cast<void *>(&FakeCategoryInput));
    Thunk(0x5178a0, reinterpret_cast<void *>(&FakeBuildInput));
    Thunk(0x431c00, reinterpret_cast<void *>(&FakeHover));
    Thunk(0x4319c0, reinterpret_cast<void *>(&FakeUpdate4));
    Thunk(0x430d80, reinterpret_cast<void *>(&FakeListDraw));
    Thunk(0x430ed0, reinterpret_cast<void *>(&FakeListInput));
    Thunk(0x405d40, reinterpret_cast<void *>(&FakeMinimapHover));
    Thunk(0x525630, reinterpret_cast<void *>(&FakeArmyContent));
    Thunk(0x525c10, reinterpret_cast<void *>(&FakeShipContent));
    Thunk(0x517de0, reinterpret_cast<void *>(&FakeBottomInput));
    Thunk(0x521970, reinterpret_cast<void *>(&FakeFilter));
    Thunk(0x510ed0, reinterpret_cast<void *>(&FakeCityLoaded));
    Thunk(0x522230, reinterpret_cast<void *>(&FakeMessagesClear));
    Thunk(0x454980, reinterpret_cast<void *>(&FakeMinimapDraw));
    Thunk(0x405d90, reinterpret_cast<void *>(&FakeMinimapClick));
    Thunk(0x477d60, reinterpret_cast<void *>(&FakeTextId));
    Thunk(0x466ad0, reinterpret_cast<void *>(&FakeTextGet));
    Thunk(0x5090a0, reinterpret_cast<void *>(&FakeWrapped));
    Thunk(0x507f20, reinterpret_cast<void *>(&FakeCharWidth));
    Thunk(0x431530, reinterpret_cast<void *>(&FakeImageButtons));
    Thunk(0x431c80, reinterpret_cast<void *>(&FakeRepeat));
    Thunk(0x522bd0, reinterpret_cast<void *>(&FakeOpen));
    Thunk(0x517a20, reinterpret_cast<void *>(&FakeRotate));
    Thunk(0x40fcf0, reinterpret_cast<void *>(&FakeImageGroup));
    // The message box's arrows as the game has them (x -1; y 2 and 31 below the box's y).
    At<short>(aArrows + 0) = -1;
    At<short>(aArrows + 2) = 2;
    At<short>(aArrows + 0x1c) = -1;
    At<short>(aArrows + 0x1e) = 31;
    for (const GameBytes &g : kGameBytes) {
        std::memcpy(reinterpret_cast<BYTE *>(g.at + g_imageShift), g.b, static_cast<size_t>(g.n));
    }
    At<int>(aDrawBuffer) = static_cast<int>(reinterpret_cast<DWORD>(g_screen.data()));
    At<int>(aPitch) = kW * 2;
    At<int>(aDdWidth) = kW;
    At<int>(aDdHeight) = kH;
    const int kinds[11] = {7, 9, 1, 6, 8, 12, 10, 11, 0, 15, 13}; // category 8 without a report here
    for (int c = 0; c < 11; ++c) {
        At<int>(aReportKinds + static_cast<DWORD>(c * 4)) = kinds[c];
    }
    for (int i = 0; i < 0x28; ++i) {
        At<BYTE>(aReportRecord + static_cast<DWORD>(i)) = static_cast<BYTE>(0xa0 + i);
    }
    // The fonts: entries 0..15 of 8 bytes, the font at +4 (entry 1 0x1001, entry 15 0x100f).
    static int fonts[16 * 2];
    for (int g = 0; g < 16; ++g) {
        fonts[g * 2] = 0;
        fonts[g * 2 + 1] = 0x1000 + g;
    }
    g_font[1] = 0x1234;
    At<int>(aFontTable) = static_cast<int>(reinterpret_cast<DWORD>(&g_font[0]));
    static unsigned short top[136 * 25], left[50 * 22];
    for (int y = 0; y < 25; ++y) {
        for (int c = 0; c < 136; ++c) {
            top[y * 136 + c] = TopPx(c, y);
        }
    }
    for (int y = 0; y < 22; ++y) {
        for (int x = 0; x < 50; ++x) {
            left[y * 50 + x] = BackdropPx(kSx + x, y); // the backdrop is the one entry 1233 belongs to
        }
    }
    sp::SetTopForTest(top, left);
    g_fontsForColumn = fonts;
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
    Check(n == 52, "52 checks (18 hooked, 26 called, 8 code and data changes)");
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
    Check(sp::AttachForTest(-1) == 18, "all match: 18 hooks");
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
    g_calls = {};
    t = Frame(200);
    Check(t.answered == 0 && t.stored == 0 && g_calls.buttonText == 1, "report open: no tooltip on the title");
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

// ---- the column moved up: the rules ----

void ColumnRules()
{
    for (int n = 0; n <= 11; ++n) {
        const lay::InfoPanel p = lay::InfoPanelFor(n);
        const unsigned spare = static_cast<unsigned>(11 - n) * 20u;
        const int h = static_cast<int>(spare + 32u - (spare & 15u));
        Check(p.top == 300 - h && p.rows == h / 16 + 2, "information panel: the game's height, two tiles longer");
        Check(p.top + p.rows * 16 == lay::kMovedEnd - 1, "information panel: its bottom at the last moved row");
    }
    Check(lay::FontEntryOffset(1, 1) == 0 && lay::FontEntryOffset(1, 2) == 8, "font group 1: entry 1 when there");
    Check(lay::FontEntryOffset(15, 15) == 0 && lay::FontEntryOffset(15, 16) == 0x78, "font group 15");
    Check(lay::FontEntryOffset(13, 13) == 0 && lay::FontEntryOffset(13, 14) == 0x68,
          "group 13 as the cost's font (+0x68 above 12, as the game)");
    for (int c = 0; c < 11; ++c) {
        Check(!lay::TitleClickable(c, 0, false) && !lay::TitleClickable(c, 0, true), "no report: not clickable");
        Check(lay::TitleClickable(c, 5, true), "a report on a Poseidon map: clickable");
        Check(lay::TitleClickable(c, 5, false) == (c != 9), "category 9 only on Poseidon's maps");
    }
    Check(!lay::TitleClickable(-1, 5, true) && !lay::TitleClickable(11, 5, true), "no such category");
    Check(lay::OnTitle(50, 0) && lay::OnTitle(181, 24) && !lay::OnTitle(49, 10) && !lay::OnTitle(182, 10) &&
              !lay::OnTitle(100, 25) && !lay::OnTitle(100, -1),
          "the title's rectangle: columns 50..181, rows 0..24");
    Check(lay::InMovedArea(50, 0) && lay::InMovedArea(185, 307) && !lay::InMovedArea(49, 100) &&
              !lay::InMovedArea(186, 100) && !lay::InMovedArea(100, 308),
          "the moved area: columns 50..185, rows 0..307");
    std::vector<lay::Px> a(50 * 22), b(50 * 22);
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<lay::Px>(0x8410 + (i % 7));
        b[i] = a[i];
    }
    Check(lay::TopFits(a.data(), b.data(), 50), "the same pixels fit");
    for (size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<lay::Px>(a[i] + 0x0821); // one step of each channel
    }
    Check(lay::TopFits(a.data(), b.data(), 50), "one step of each channel fits");
    for (size_t i = 0; i < 30; ++i) {
        b[i] = 0;
    }
    Check(!lay::TopFits(a.data(), b.data(), 50), "more than 2% different: another backdrop");
}

// A column on its own (not the screen): the moves without the game.
void ColumnMove()
{
    constexpr int W = lay::kColumnW, R = lay::kColumnRows;
    std::vector<lay::Px> back(W * R), top(W * lay::kShift), screen(W * R);
    for (int y = 0; y < R; ++y) {
        for (int x = 0; x < W; ++x) {
            back[y * W + x] = static_cast<lay::Px>(0x8000 | ((y * 131 + x * 7) & 0x7fff));
        }
    }
    for (int y = 0; y < lay::kShift; ++y) {
        for (int x = 0; x < W; ++x) {
            top[y * W + x] = static_cast<lay::Px>(0x0100 + y * 3 + x);
        }
    }
    lay::Backdrop bd;
    const lay::Backdrop::Where where{838, 1024, 768, 2048};
    bd.Take(back.data(), W, where);
    Check(bd.Has(where) && !bd.Has({838, 1024, 1024, 2048}) && !bd.Has({1094, 1280, 768, 2560}),
          "the backdrop only for the same screen");
    std::vector<lay::Px> first;
    for (int frame = 0; frame < 6; ++frame) {
        bd.PutBack(screen.data(), W);
        Check(std::memcmp(screen.data(), back.data(), back.size() * 2) == 0, "put back: the backdrop");
        for (int y = 50; y < 70; ++y) { // something drawn in the game's place
            for (int x = 10; x < 40; ++x) {
                screen[y * W + x] = 0x1111;
            }
        }
        lay::MoveColumn(screen.data(), W, bd.Pixels(), frame % 2 ? nullptr : top.data());
        if (frame == 0) {
            first = screen;
        }
        else if (frame == 2 || frame == 4) {
            Check(screen == first, "the same frame after frame: no drift, no trail");
        }
        for (int y = 0; y < R; ++y) {
            for (int x = 0; x < W; ++x) {
                const lay::Px p = screen[y * W + x];
                lay::Px want;
                if (y >= 25 && y < 45 && x >= 10 && x < 40) {
                    want = 0x1111;
                }
                else if (y < lay::kShift) {
                    want = frame % 2 ? back[(y + lay::kShift) * W + x] : top[y * W + x];
                }
                else {
                    want = back[y * W + x];
                }
                if (p != want) {
                    Check(false, "moved 25 rows up, the marble in place (entry 1233's or the column's own above)");
                    y = R;
                    break;
                }
            }
        }
    }
    Check(!lay::DrawnBelow(screen.data(), W, bd.Pixels()), "nothing below the moved rows");
    screen[400 * W + 3] = 1;
    Check(lay::DrawnBelow(screen.data(), W, bd.Pixels()), "drawn below the moved rows: seen");
}

// ---- the column moved up: the hooks ----

int PanelDraw()
{
    return Hook<int(__cdecl *)()>("sidebar_panel_draw")();
}
int PanelInput()
{
    return Hook<int(__cdecl *)()>("sidebar_panel_input")() & 0xff;
}
int ReportInput()
{
    return Hook<int(__cdecl *)()>("sidebar_report_button_input")() & 0xff;
}
void Toggle()
{
    Hook<void(__cdecl *)()>("sidebar_overview_toggle")();
}

// A frame of the city screen: the backdrop drawn (fresh) or kept from the frame before.
void ColumnFrame(bool fresh)
{
    if (fresh) {
        DrawBackdrop();
        sp::BackdropDrawing(true);
        PanelDraw();
        sp::BackdropDrawing(false);
    }
    At<unsigned char>(aButtonsRedraw) = 0;
    PanelDraw();
}

void ColumnSetup()
{
    sp::AttachForTest(-1);
    At<int>(aFontTable) = static_cast<int>(reinterpret_cast<DWORD>(g_fontsForColumn));
    At<int>(aFontCount) = 16;
    At<int>(aCategory) = 0;
    At<int>(aHover) = 0;
    At<int>(aMouseX) = 100;
    At<int>(aMouseY) = 400; // in the city
    DrawBackdrop();
}

void Column()
{
    ColumnSetup();
    At<int>(aOverview) = 1;
    g_col = {};
    At<unsigned char>(aScreenRedraw) = 0;
    PanelDraw();
    Check(CodeIs(true), "active: the game's code changed (no tabs, no Overview minimap input)");
    Check(g_col.panelDraw == 1 && At<unsigned char>(aScreenRedraw) == 1,
          "no backdrop yet: the game's drawing this frame, the screen drawn again");
    Check(At<int>(aOverview) == 0, "the Info tab");
    g_col = {};
    ColumnFrame(true);
    Check(g_col.panelDraw == 0 && g_col.overlayDraw == 2 && g_col.inner == 2 && g_col.categoryDraw == 2 &&
              g_col.buildDraw == 2 && g_col.text == 2,
          "the backdrop taken, then put back: ours on both calls");
    const lay::InfoPanel info = lay::InfoPanelFor(g_overlayButtons);
    Check(g_col.innerArgs[0] == kSx + 0x32 && g_col.innerArgs[1] == info.top && g_col.innerArgs[2] == 8 &&
              g_col.innerArgs[3] == info.rows,
          "the information panel in the game's place, two tiles longer");
    const int *t = g_col.textArgs;
    Check(t[0] == 88 && t[1] == 0 && t[2] == kSx + 50 && t[3] == 5 && t[4] == 0x82 && t[5] == 0x1001 && t[6] == 0,
          "the title: group 88, moved up, the game's font");
    Check(At<unsigned char>(aButtonsRedraw) == 1, "the buttons drawn on every frame");

    // What is on screen.
    Check(Px(kSx + 60, 25) == kOverlayPx && Px(kSx + 60, 44) == kOverlayPx && Px(kSx + 60, 45) != kOverlayPx,
          "the overlay button 25 rows up");
    Check(Px(kSx + 60, 175) == kCategoryPx && Px(kSx + 60, 200) != kCategoryPx, "the category's panel 25 rows up");
    Check(Px(kSx + 55, info.top - 25) == kPanelPx && Px(kSx + 55, 306) == kPanelPx &&
              Px(kSx + 55, 307) == BackdropPx(kSx + 55, 307) && Px(kSx + 55, 308) == BackdropPx(kSx + 55, 308),
          "the information panel moved, its last row 306 (the game's 331)");
    Check(Px(kSx + 60, 330) == kBuildPx, "the build buttons where they are");
    Check(Px(kSx + 183, 10) == TopPx(183 - 50, 10) && Px(kSx + 183, 100) == BackdropPx(kSx + 183, 100),
          "the marble: entry 1233's above the title, the backdrop's where it was");
    Check(Px(kSx + 20, 10) == BackdropPx(kSx + 20, 10), "left of the column: as drawn");

    // Frames after frames: the same picture (the buttons drawn again on each, nothing moved twice).
    std::vector<unsigned short> before = g_screen;
    for (int i = 0; i < 5; ++i) {
        ColumnFrame(false);
    }
    Check(g_screen == before, "five more frames: the same picture");
    g_onlyWhenRedrawn = false;
    ColumnFrame(false);
    Check(g_screen == before, "a drawing that does not wait for the flag: the same picture");
    g_onlyWhenRedrawn = true;

    // The Overview category: no build buttons, the title's category.
    At<int>(aCategory) = 10;
    g_col = {};
    ColumnFrame(false);
    Check(g_col.buildDraw == 0 && g_col.textArgs[1] == 10, "Overview: no build buttons, its title");
    Check(Px(kSx + 60, 330) == BackdropPx(kSx + 60, 330), "Overview: the build buttons' place is the backdrop");
    g_poseidon = true;
    At<int>(aCategory) = 6;
    ColumnFrame(false);
    Check(g_col.textArgs[1] == 0x18, "Poseidon: Culture's title as Science");
    g_poseidon = false;

    // The title's font: lit only when it can be clicked and the mouse is on it.
    At<int>(aCategory) = 1;
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 10;
    ColumnFrame(false);
    Check(g_col.textArgs[5] == 0x100f, "on the title with a report: font group 15");
    At<int>(aCategory) = 8; // no report here
    ColumnFrame(false);
    Check(g_col.textArgs[5] == 0x1001, "no report: the game's font");
    At<int>(aCategory) = 9;
    ColumnFrame(false);
    Check(g_col.textArgs[5] == 0x1001, "category 9 on a Zeus map: the game's font");
    At<int>(aMouseY) = 30;
    At<int>(aCategory) = 1;
    ColumnFrame(false);
    Check(g_col.textArgs[5] == 0x1001, "below the title: the game's font");

    // Another screen size: taken again.
    At<int>(aDdHeight) = 1024;
    g_col = {};
    At<unsigned char>(aScreenRedraw) = 0;
    PanelDraw();
    Check(g_col.panelDraw == 1 && At<unsigned char>(aScreenRedraw) == 1, "another size: the backdrop taken again");
    At<int>(aDdHeight) = kH;

    // Not active: the game's own, the code put back.
    At<int>(aEditorMode) = 1;
    g_col = {};
    PanelDraw();
    Check(g_col.panelDraw == 1 && CodeIs(false), "the editors: the game's drawing and code");
    At<int>(aEditorMode) = 0;
    PanelDraw();
    Check(CodeIs(true), "back in the city: ours again");
    sp::SetModeForTest("original", false);
    Check(CodeIs(false), "original: the game's code at once");
    g_col = {};
    PanelDraw();
    Toggle();
    Check(g_col.panelDraw == 1 && g_col.toggle == 1, "original: the game's drawing and tabs");
    sp::SetModeForTest("pericles", false);
    Toggle();
    Check(g_col.toggle == 1 && sp::BoxMessagesForTest(), "pericles: Tab switches the box, not the game's tabs");
    Toggle();
    Check(!sp::BoxMessagesForTest(), "Tab again: the minimap");
}

// The backdrop never seen being drawn (sidebar_draw not hooked): the game's own side panel after a few frames.
void ColumnNoBackdrop()
{
    ColumnSetup();
    for (int i = 0; i < 20 && sp::Active(); ++i) {
        PanelDraw();
    }
    Check(!sp::Active() && CodeIs(false), "no backdrop: off, the game's code back");
}

void Input()
{
    ColumnSetup();
    PanelDraw(); // active: the code changed
    At<int>(aCategory) = 1;

    // The title: as the report button.
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 10;
    g_col = {};
    g_updateResult = 1;
    Check(PanelInput() == 1, "clicked on the title: taken");
    Check(g_col.hover == 1 && g_col.hoverArgs[0] == kSx && g_col.hoverArgs[1] == 0 && g_col.hoverArgs[3] == 1 &&
              g_col.hoverArgs[4] == 200 && At<int>(aHover) == 200,
          "hovered as the report button (200)");
    const BYTE *r = g_col.record;
    short place[4];
    std::memcpy(place, r, sizeof(place));
    int value = 0;
    std::memcpy(&value, r + 0x1c, 4);
    Check(place[0] == 50 && place[1] == 0 && place[2] == 132 && place[3] == 25 && r[0x18] == 1 && value == 9,
          "the title's record: its place, clickable, the category's report");
    Check(r[0x10] == 0xb0 && r[0x14] == 0xb4 && r[0x20] == 0xc0, "the report button's actions");
    Check(g_col.update == 1 && g_col.updateArgs[0] == kSx && g_col.updateArgs[1] == 0 &&
              g_col.updateArgs[2] == g_col.hoverArgs[2],
          "updated with the same record");
    Check(g_col.overlayInput == 0 && g_col.categoryInput == 0, "on the title: not the panels");
    g_updateResult = 0;
    g_col = {};
    Check(PanelInput() == 0 && g_col.update == 1, "hovered, not clicked: not taken");
    At<int>(aCategory) = 8;
    At<int>(aHover) = 0;
    g_col = {};
    Check(PanelInput() == 0 && g_col.hover == 0 && At<int>(aHover) == 0 && g_col.overlayInput == 0,
          "no report: no hover number, nothing clicked");

    // The panels: the mouse in the game's place.
    At<int>(aCategory) = 1;
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 30;
    g_col = {};
    PanelInput();
    Check(g_col.overlayInput == 1 && g_col.overlayMouseY == 55 && g_col.categoryInput == 1 &&
              g_col.categoryMouseY == 55,
          "the panels see the mouse 25 rows lower");
    Check(At<int>(aMouseY) == 30 && At<int>(aHoverY) == 30, "afterwards the mouse and the tooltip's place as they are");
    At<int>(aMouseX) = 100;
    At<int>(aMouseY) = 400;
    At<int>(aHoverY) = 7;
    g_col = {};
    PanelInput();
    Check(g_col.overlayMouseY == 425 && At<int>(aMouseY) == 400 && At<int>(aHoverY) == 400,
          "elsewhere too (their lists see the mouse leave)");
    g_buildInputResult = 1;
    g_col = {};
    Check(PanelInput() == 1 && g_col.overlayInput == 0, "a build button clicked: nothing else");
    g_buildInputResult = 0;

    // The report screen: the title closes the report.
    At<unsigned char>(aGameScreen) = 7;
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 10;
    At<unsigned char>(aScreenRedraw) = 0;
    g_updateResult = 1;
    g_col = {};
    Check(ReportInput() == 1 && g_col.reportInput == 0 && g_col.update == 1, "report screen: the title clicked");
    Check(At<unsigned char>(aScreenRedraw) == 1, "the title's font changes: the screen drawn again");
    At<int>(aMouseY) = 300;
    g_col = {};
    Check(ReportInput() == 0 && g_col.reportInput == 0 && g_col.update == 0, "report screen: the report button gone");
    At<unsigned char>(aGameScreen) = 0x48;
    g_col = {};
    ReportInput();
    Check(g_col.reportInput == 1, "the army panel: the game's report button");
    At<unsigned char>(aGameScreen) = 1;
    g_updateResult = 0;
    sp::SetModeForTest("original", false);
    g_col = {};
    PanelInput();
    Check(g_col.panelInput == 1, "original: the game's input");
}

void ColumnFault()
{
    ColumnSetup();
    ColumnFrame(true);
    Check(CodeIs(true), "active");
    g_overlayFaults = true;
    g_col = {};
    At<unsigned char>(aScreenRedraw) = 0;
    PanelDraw();
    g_overlayFaults = false;
    Check(!sp::Active() && CodeIs(false) && At<unsigned char>(aScreenRedraw) == 1,
          "a fault in the column's drawing: off, the game's code back, the screen drawn again");
    Check(g_col.panelDraw == 1, "the game's drawing in the frame of the fault");
    g_col = {};
    PanelDraw();
    PanelInput();
    Toggle();
    ReportInput();
    Check(g_col.panelDraw == 1 && g_col.panelInput == 1 && g_col.toggle == 1 && g_col.reportInput == 1,
          "afterwards every hook the game's own");
}

void CodeSites()
{
    const int n = sp::SiteCountForTest(), patches = sp::PatchCountForTest();
    Check(patches == 8 && n == 44 + 8, "44 sites and 8 code and data changes");
    ColumnSetup();
    for (int k = 0; k < n; ++k) {
        Check(sp::AttachForTest(k) == 0, "one that does not match: nothing installed");
        g_col = {};
        PanelDraw();
        PanelInput();
        Toggle();
        Check(g_col.panelDraw == 1 && g_col.panelInput == 1 && g_col.toggle == 1 && CodeIs(false),
              "not installed: the game's own, its code as it is");
    }
}

// ---- the box at the bottom: the rules ----

int Lines(const char *t, int limit)
{
    return lay::WrapLines(reinterpret_cast<const unsigned char *>(t), limit, &FakeWidth, nullptr);
}

void BoxRules()
{
    lay::Name n{};
    lay::Facts f = Facts();
    Check(lay::Ours(201) && lay::NameFor(201, f, &n) && n.group == 68 && n.text == 33,
          "the toggle over the minimap: View messages (group 68, text 33)");
    f.boxMessages = true;
    Check(lay::NameFor(201, f, &n) && n.group == 88 && n.text == 16,
          "over the messages: Show Overview Map (group 88, text 16)");
    Check(lay::OnToggle(160, 515) && lay::OnToggle(177, 532) && !lay::OnToggle(178, 515) && !lay::OnToggle(160, 533),
          "the toggle: 18 x 18 at (160, 515)");
    Check(lay::OnMinimap(12, 515) && lay::OnMinimap(177, 726) && !lay::OnMinimap(11, 600) &&
              !lay::OnMinimap(178, 600) && !lay::OnMinimap(100, 727),
          "the minimap: 166 x 212 at (12, 515)");
    Check(lay::OnList(8, 515) && lay::OnList(163, 727) && !lay::OnList(164, 600) && !lay::OnList(7, 600) &&
              !lay::OnList(50, 514),
          "the message box as clicked: its columns 8..163, the box's rows (both ends in)");

    // The game's wrapping, 6 px a letter: "aaaaa bbbbb" is 30 + 4 + 30 px.
    Check(Lines("", 148) == 1 && Lines("aaaaa", 148) == 1, "an empty text or one word: one line");
    Check(Lines("aaaaa bbbbb", 64) == 2 && Lines("aaaaa bbbbb", 65) == 1,
          "a line stays narrower than the limit (64 px at 64: two lines)");
    Check(Lines("aaaaa bbbbb ccccc", 64) == 3 && Lines("aaaaa bbbbb ccccc", 70) == 2,
          "a word that does not fit starts the next line (with its space)");
    Check(Lines("aaaaa$bbbbb", 31) == 2 && Lines("aaaaa$bbbbb", 61) == 1, "$: a break without width");
    Check(Lines("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 100) == 99, "a word wider than the limit: 99 lines, as the game");
    Check(lay::MessageFits(517, 1, 11) && lay::MessageFits(710, 1, 11) && !lay::MessageFits(711, 1, 11) &&
              !lay::MessageFits(517, 14, 11),
          "a message fits down to row 726 (each line 5 + the line height)");
    lay::MessageList l;
    l.Begin();
    l.Add(3, 517, 549);
    l.Add(7, 555, 571);
    Check(l.Count() == 2 && l.At(517) == 3 && l.At(548) == 3 && l.At(549) == -1 && l.At(552) == -1 &&
              l.At(555) == 7 && l.At(571) == -1,
          "the message under a row (none between two)");

    // The lower half on a sidebar of its own (186 wide), 768 and 900 rows high.
    for (int h : {768, 900}) {
        std::vector<unsigned short> sb(static_cast<size_t>(186) * h);
        auto px = [&](int x, int y) -> unsigned short & { return sb[static_cast<size_t>(y) * 186 + x]; };
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < 186; ++x) {
                px(x, y) = BackdropPx(x, y);
            }
        }
        lay::LowerHalf(sb.data(), 186, h);
        bool above = true, edge = true, plain = true, right = true, row = true;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < 186; ++x) {
                const unsigned short p = px(x, y);
                if (y < 512) {
                    above = above && p == BackdropPx(x, y);
                }
                else if (y >= 731 && y < 768) {
                    row = row && p == BackdropPx(x, 563 + y - 731);
                }
                else if (x < 11) {
                    edge = edge && p == BackdropPx(x, 640 + (y - 512) % 60);
                }
                else if (x < 181) {
                    plain = plain && p == BackdropPx(x, 562);
                }
                else {
                    right = right && p == BackdropPx(x, 512 + (y - 512) % 50);
                }
            }
        }
        Check(above, "lower half: the rows above 512 as they were");
        Check(edge && plain && right, "lower half: the left edge (rows 640..699), the plain row 562, the right edge "
                                      "(rows 512..561) carried down");
        Check(row, "lower half: the check / gem / globe row (563..599) at 731..767");
    }
}

// ---- the box at the bottom: the hooks ----

template <typename F> F BoxHook(const char *name)
{
    return Hook<F>(name);
}
void *Addr(DWORD a)
{
    return reinterpret_cast<void *>(a + g_imageShift);
}
void ListDraw()
{
    BoxHook<void(__fastcall *)(void *, void *)>("sidebar_messages_draw")(Addr(aList), nullptr);
}
void ListInput()
{
    BoxHook<void(__fastcall *)(void *, void *)>("sidebar_messages_input")(Addr(aList), nullptr);
}
void MinimapHover()
{
    BoxHook<void(__cdecl *)()>("minimap_hover")();
}
int BottomInput()
{
    return BoxHook<int(__cdecl *)()>("sidebar_bottom_buttons_input")() & 0xff;
}

bool DrawnAt(int id, int x, int y)
{
    for (const Drawn &d : g_drawn) {
        if (d.id == id && d.x == x && d.y == y) {
            return true;
        }
    }
    return false;
}
bool TileAt(int x, int y)
{
    for (const Drawn &d : g_drawn) {
        if (d.id >= kInsetBase && d.id < kInsetBase + 49 && d.x == x && d.y == y) {
            return true;
        }
    }
    return false;
}

// Four messages: 0 (3 words, one line), 1 (not for the sidebar), 2 (10 words: three lines), 3 (40 words: ten lines,
// too many for what is left of the box); the game's box can show three.
void SetMessages()
{
    At<int>(aMessageCount) = 4;
    const int answers[4] = {1, 0, 2, 1}, words[4] = {3, 3, 10, 40};
    for (int i = 0; i < 4; ++i) {
        At<short>(aMessages + static_cast<DWORD>(i * 0x3c)) = static_cast<short>(i);
        g_filterAnswer[i] = answers[i];
        g_messageWords[i] = words[i];
    }
    At<int>(aList + 0x10) = 3;
    At<int>(aList + 0x18) = 0;
    At<int>(aMessageCurrent) = 42;
}

void BoxSetup()
{
    ColumnSetup();
    SetMessages();
    g_box = {};
    g_drawn.clear();
    At<unsigned char>(aMousePressed) = 0;
    g_clickResult = g_repeatResult = g_bottomResult = g_updateResult = 0;
}

void Box()
{
    BoxSetup();
    ColumnFrame(true);
    Check(CodeIs(true), "active: the code changed and the check, globe and gem buttons' y moved (711)");
    Check(Px(kSx + 100, 731) == BackdropPx(kSx + 100, 563) && Px(kSx + 100, 767) == BackdropPx(kSx + 100, 599),
          "the check / gem / globe row at 731..767");
    Check(Px(kSx + 100, 600) == BackdropPx(kSx + 100, 562) && Px(kSx + 3, 600) == BackdropPx(kSx + 3, 640 + 88 % 60),
          "the picture's place: the plain row and the left edge carried down");
    Check(TileAt(kSx + 9, 512) && TileAt(kSx + 9 + 172 - 16, 512 + 218 - 16) && g_box.imageGroup >= 1,
          "the box's inset panel (172 x 218 at (9, 512)), the game's tiles");
    Check(g_box.minimapDraw == 2 && g_box.minimapArgs[0] == kSx + 12 && g_box.minimapArgs[1] == 515 &&
              g_box.minimapArgs[2] == 83 && g_box.minimapArgs[3] == 212,
          "the minimap in the box on both calls: (sidebar_x + 12, 515), 83 x 212");
    Check(DrawnAt(9100, kSx + 160, 515), "the toggle: the report button's picture at (160, 515)");
    Check(g_box.wrapped == 0, "no messages while the box shows the minimap");
    g_box = {};
    ListDraw();
    Check(g_box.listDraw == 0 && g_box.wrapped == 0, "the game's message box draws nothing (the box does)");

    // The toggle under the mouse: lit; its tooltip.
    At<int>(aMouseX) = kSx + 165;
    At<int>(aMouseY) = 520;
    g_drawn.clear();
    ColumnFrame(false);
    Check(DrawnAt(9101, kSx + 160, 515), "the toggle under the mouse: the next picture");
    Tip t = Frame(201);
    Check(t.answered && t.group == 68 && t.drawn == 33, "its tooltip over the minimap: View messages");

    // Messages.
    Toggle();
    Check(sp::BoxMessagesForTest() && At<unsigned char>(aScreenRedraw) == 1, "the toggle's action: the messages");
    t = Frame(201);
    Check(t.answered && t.group == 88 && t.drawn == 16, "its tooltip over the messages: Show Overview Map");
    At<int>(aMouseX) = 100;
    At<int>(aMouseY) = 400;
    g_box = {};
    ColumnFrame(false);
    Check(g_box.minimapDraw == 0 && g_box.wrapped == 2, "messages: no minimap; two messages fit");
    Check(g_box.wrappedY[0] == 517 && g_box.wrappedY[1] == 517 + 16 + 6 && g_box.wrappedX[0] == kSx + 13 &&
              g_box.wrappedText[0] == 'a' && g_box.wrappedText[1] == 'c',
          "messages 0 and 2 (1 is not for the sidebar), one under the other, 6 rows apart");
    Check(g_box.textGet == 3 && g_box.wrappedText[15] == 3 && At<int>(aMessageCurrent) == 42,
          "message 3 fetched (as the message to fetch) but not drawn: it does not fit; the current message back");
    Check(g_box.filterThisBad == 0 && g_box.filter == 4, "message_filter asked once for each, on the message options");
    Check(g_box.imageButtons == 2 && g_box.arrowY[0] == 535 && g_box.arrowY[1] == 708, "the arrows at rows 535, 708");
    At<int>(aList + 0x18) = 1;
    g_box = {};
    ColumnFrame(false);
    Check(g_box.wrapped == 2 && g_box.wrappedText[0] == 'c' && g_box.wrappedY[0] == 517 &&
              g_box.wrappedText[1] == 'd' && g_box.wrappedY[1] == 517 + 48 + 6,
          "scrolled by one: message 2 at the top, 3 (nine lines) fits after it");
    At<int>(aList + 0x18) = 0;
    At<int>(aList + 0x10) = 1;
    g_box = {};
    ColumnFrame(false);
    Check(g_box.imageButtons == 0, "one message the box can show: no arrows");
    At<int>(aList + 0x10) = 3;
    ColumnFrame(false);

    // The messages' input: the message under the mouse; the arrows first; the hand cursor.
    At<int>(aMouseX) = kSx + 50;
    At<int>(aMouseY) = 545;
    At<unsigned char>(aMousePressed) = 1;
    g_box = {};
    ListInput();
    Check(g_box.listInput == 0 && g_box.open == 1 && g_box.openIndex == 2 && At<BYTE>(aList + 0x1c) == 1,
          "a click on the second message shown: message 2 opened, the hand cursor");
    Check(g_box.repeat == 2 && g_box.repeatY[0] == 535 && g_box.repeatY[1] == 708, "the arrows asked where drawn");
    At<int>(aMouseY) = 535; // between messages 0 (517..532) and 2 (539..)
    g_box = {};
    ListInput();
    Check(g_box.open == 0, "between two messages: nothing opened");
    g_repeatResult = 1;
    At<int>(aMouseY) = 520;
    g_box = {};
    ListInput();
    Check(g_box.open == 0, "an arrow held: nothing else");
    g_repeatResult = 0;
    At<unsigned char>(aMousePressed) = 0;
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 600;
    g_box = {};
    MinimapHover();
    Check(g_box.minimapHover == 0 && At<int>(aMinimapHovered) == 0, "messages: the minimap's hover not asked");
    g_box = {};
    PanelInput();
    Check(g_box.minimapHover == 0 && g_box.minimapClick == 0, "messages: the box's minimap not clicked");

    // Back to the minimap: its input.
    Toggle();
    ColumnFrame(false);
    At<BYTE>(aList + 0x1c) = 1;
    g_box = {};
    ListInput();
    Check(At<BYTE>(aList + 0x1c) == 0 && g_box.repeat == 0 && g_box.open == 0, "the minimap: no message input");
    At<unsigned char>(aMousePressed) = 1;
    g_clickResult = 1;
    g_box = {};
    Check(PanelInput() == 1 && g_box.minimapHover == 1 && g_box.minimapClick == 1 && g_box.clickLeft == kSx + 12,
          "a click on the box's minimap: hovered and clicked where it is drawn");
    g_clickResult = 0;
    At<int>(aMouseX) = kSx + 165;
    At<int>(aMouseY) = 520;
    g_box = {};
    MinimapHover();
    Check(g_box.minimapHover == 0 && At<int>(aMinimapHovered) == 0, "on the toggle: not the minimap");
    g_updateResult = 1;
    g_col = {};
    g_box = {};
    Check(PanelInput() == 1 && g_col.hover == 1 && g_col.hoverArgs[4] == 201 && g_box.minimapClick == 0,
          "the toggle clicked: as a button, hover number 201");
    short place[4];
    std::memcpy(place, g_col.record, sizeof(place));
    DWORD action = 0;
    std::memcpy(&action, g_col.record + 0x10, 4);
    Check(place[0] == 160 && place[1] == 515 && place[2] == 18 && place[3] == 18 && g_col.record[0x18] == 1 &&
              action == 0x516d30 + g_imageShift,
          "its record: its place, clickable, the Tab key's action");
    g_updateResult = 0;
    At<unsigned char>(aMousePressed) = 0;
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 600;
    g_box = {};
    MinimapHover();
    Check(g_box.minimapHover == 1, "the minimap elsewhere: the game's hover");

    // A Condensed message posted: the box shows the messages; only city_message_post's question counts.
    using Filter_fn = int(__fastcall *)(void *, void *, void *);
    void *filter = reinterpret_cast<void *>(BoxHook<Filter_fn>("message_filter"));
    BYTE *post = reinterpret_cast<BYTE *>(0x521bb9 + g_imageShift); // mov ecx; push message; call; (0x521bc8) ret
    post[0] = 0xb9;
    *reinterpret_cast<DWORD *>(post + 1) = aMessageOptions + g_imageShift;
    post[5] = 0x68;
    *reinterpret_cast<DWORD *>(post + 6) = aMessages + g_imageShift;
    post[10] = 0xe8;
    *reinterpret_cast<DWORD *>(post + 11) = reinterpret_cast<DWORD>(filter) - reinterpret_cast<DWORD>(post + 15);
    post[15] = 0xc3;
    auto posted = reinterpret_cast<void(__cdecl *)()>(post);
    g_filterAnswer[0] = 2;
    posted();
    Check(!sp::BoxMessagesForTest(), "a Full message: the minimap stays");
    g_filterAnswer[0] = 1;
    BoxHook<Filter_fn>("message_filter")(Addr(aMessageOptions), nullptr, Addr(aMessages));
    Check(!sp::BoxMessagesForTest(), "message_filter asked elsewhere (the box's drawing): the minimap stays");
    At<unsigned char>(aScreenRedraw) = 0;
    posted();
    Check(sp::BoxMessagesForTest() && At<unsigned char>(aScreenRedraw) == 1,
          "a Condensed message posted: the messages, the screen drawn again");

    // A city loaded or started: the minimap.
    g_box = {};
    BoxHook<void(__cdecl *)()>("city_loaded_setup")();
    Check(g_box.cityLoaded == 1 && !sp::BoxMessagesForTest(), "a city loaded: the game's setup, then the minimap");
    Toggle();
    BoxHook<void(__cdecl *)()>("messages_clear")();
    Check(g_box.messagesClear == 1 && !sp::BoxMessagesForTest(), "the messages cleared (a new city): the minimap");

    // Not active: the game's own message box, the buttons' y back.
    sp::SetModeForTest("original", false);
    Check(CodeIs(false), "original: the code and the buttons' y back");
    g_box = {};
    ListDraw();
    ListInput();
    MinimapHover();
    Check(g_box.listDraw == 1 && g_box.listInput == 1 && g_box.minimapHover == 1, "original: the game's own");
    posted();
    Check(!sp::BoxMessagesForTest(), "original: a Condensed message changes nothing");
    sp::SetModeForTest("pericles", false);
    At<int>(aEditorMode) = 1;
    PanelDraw();
    Check(CodeIs(false), "the editors: the buttons' y back");
    At<int>(aEditorMode) = 0;
}

// Taller than 768: the box where it is, an inset panel below 768.
void BoxTall()
{
    BoxSetup();
    At<int>(aDdHeight) = 1024;
    ColumnFrame(true);
    Check(TileAt(kSx + 9, 512) && TileAt(kSx + 11, 768) && TileAt(kSx + 11 + 172 - 16, 1024 - 16),
          "the box at 512, an inset panel from 768 to the bottom (172 wide from column 11)");
    Check(Px(kSx + 100, 731) == BackdropPx(kSx + 100, 563) && Px(kSx + 3, 900) == BackdropPx(kSx + 3, 640 + 388 % 60),
          "the button row at 731; the left edge down to the bottom");
    At<int>(aDdHeight) = kH;
}

// An army (and a ship) panel: the lower half laid out after its backdrop, the box after its content, the game's own
// minimap at the top the one its input sees.
void Army()
{
    BoxSetup();
    PanelDraw(); // active
    At<unsigned char>(aGameScreen) = 0x48;
    DrawBackdrop();
    FakeMinimapDraw(kSx + 25, 28, 71, 109); // the army panel's own, drawn with its backdrop
    g_box = {};
    sp::ArmyBackdropDrawn();
    Check(g_box.rotate == 1 && Px(kSx + 100, 731) == BackdropPx(kSx + 100, 563),
          "after its backdrop: the lower half laid out, the gem drawn again");
    BoxHook<void(__cdecl *)()>("army_sidebar_content_draw")();
    Check(g_box.armyContent == 1 && g_box.minimapDraw == 1 && g_box.minimapArgs[0] == kSx + 12 &&
              At<int>(aMinimapRect + 12) == kSx + 25 && At<int>(aMinimapArgs + 12) == kSx + 25,
          "its content, then the box's minimap; the place of the panel's own minimap kept");
    At<int>(aMouseX) = kSx + 100;
    At<int>(aMouseY) = 600;
    At<unsigned char>(aMousePressed) = 1;
    g_clickResult = 1;
    g_box = {};
    Check(BottomInput() == 1 && g_box.bottomInput == 1 && g_box.minimapClick == 1 && g_box.clickLeft == kSx + 12 &&
              At<int>(aMinimapRect + 12) == kSx + 25,
          "a click on the box's minimap: clicked with the box's place, the panel's own put back");
    g_bottomResult = 1;
    g_box = {};
    Check(BottomInput() == 1 && g_box.minimapClick == 0, "a bottom button clicked: nothing else");
    g_bottomResult = 0;
    g_clickResult = 0;
    // The messages there: drawn after the content, clicked through the bottom buttons' input.
    Toggle();
    g_box = {};
    BoxHook<void(__cdecl *)()>("ship_sidebar_content_draw")();
    Check(g_box.shipContent == 1 && g_box.wrapped == 2 && g_box.minimapDraw == 0, "a ship panel: the messages");
    At<int>(aMouseX) = kSx + 50;
    At<int>(aMouseY) = 520;
    g_box = {};
    BottomInput();
    Check(g_box.open == 1 && g_box.openIndex == 0, "a click on a message there: opened");
    At<unsigned char>(aGameScreen) = 1;
    g_box = {};
    BottomInput();
    Check(g_box.bottomInput == 1 && g_box.open == 0, "the city screen: the bottom buttons only (the box is asked "
                                                     "with the panel's input)");
    At<unsigned char>(aMousePressed) = 0;
}

// An army panel's backdrop never laid out (sidebar_draw_units not hooked): the game's own side panel after a few
// frames, not the buttons moved over its picture.
void ArmyNoBackdrop()
{
    BoxSetup();
    PanelDraw();
    At<unsigned char>(aGameScreen) = 0x48;
    for (int i = 0; i < 4; ++i) {
        sp::ArmyBackdropDrawn();
        BoxHook<void(__cdecl *)()>("army_sidebar_content_draw")();
    }
    Check(sp::Active(), "laid out on every frame: active");
    for (int i = 0; i < 20 && sp::Active(); ++i) {
        BoxHook<void(__cdecl *)()>("army_sidebar_content_draw")();
    }
    Check(!sp::Active() && CodeIs(false), "never laid out: off, the buttons' y back");
    At<unsigned char>(aGameScreen) = 1;
}

// A fault in the box's drawing: off, the code and the buttons' y back, the game's own afterwards.
void BoxFault()
{
    BoxSetup();
    ColumnFrame(true);
    Check(CodeIs(true), "active");
    g_minimapFaults = true;
    At<unsigned char>(aScreenRedraw) = 0;
    ColumnFrame(false);
    g_minimapFaults = false;
    Check(!sp::Active() && CodeIs(false) && At<unsigned char>(aScreenRedraw) == 1,
          "a fault in the box: off, the game's code and data back, the screen drawn again");
    g_box = {};
    ListDraw();
    ListInput();
    MinimapHover();
    Check(g_box.listDraw == 1 && g_box.listInput == 1 && g_box.minimapHover == 1, "afterwards the game's own");
}

struct Scenario {
    const char *name;
    void (*run)();
};

const Scenario kScenarios[] = {
    {"names", &Names},       {"report_rule", &ReportRule}, {"tip_memo", &TipMemoRule}, {"cost_trail", &CostTrailRule},
    {"sites", &Sites},       {"city", &City},              {"report", &Report},        {"cost", &Cost},
    {"cost_fault", &CostFault}, {"name_fault", &NameFault}, {"column_rules", &ColumnRules},
    {"column_move", &ColumnMove}, {"column", &Column},     {"column_no_backdrop", &ColumnNoBackdrop},
    {"input", &Input},          {"column_fault", &ColumnFault}, {"code_sites", &CodeSites},
    {"box_rules", &BoxRules},   {"box", &Box},             {"box_tall", &BoxTall},     {"army", &Army},
    {"army_no_backdrop", &ArmyNoBackdrop}, {"box_fault", &BoxFault},
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
