// The side panel's rules that do not touch the game's memory (games/zeus/side_panel reads the game and calls these;
// the offline test calls them directly): which name a sidebar button shows as a tooltip, which text group goes with
// it and for how long, and when the cost drawn under a build button has to be cleared.
//
// The names are the ones the game's own name bar shows (sidebar_name_bar_draw 0x517210, Zeus.exe 2.1.4.0): the 17
// sidebar buttons (hover 0x19..0x29: the 11 categories, Build Road, Roadblock, Clear, Undo and the two tabs) the
// sidebar's group 88; the 4 build buttons (0x32..0x35) the building's name in group 28, only while the button can be
// used; the report button (200) group 51's "See more information". The report button's place is now the title's
// (hover 200 too): while its report is open the title has no tooltip (the game's own would show above the screen's
// top edge there; Emperor's title has none then either, only its lit font says it can be clicked).
#pragma once

#include "games/zeus/minimap_part.h"

namespace zeus::side_panel_layout {

// The hover numbers (hover_button 0x781834) of the buttons whose names the name bar shows.
constexpr int kFirstSidebarButton = 0x19, kLastSidebarButton = 0x29;
constexpr int kFirstBuildButton = 0x32, kLastBuildButton = 0x35;
constexpr int kReportButton = 200;

// The text groups and texts.
constexpr int kGroupSidebar = 88, kGroupBuildings = 28, kGroupReport = 51;
constexpr int kReportShow = 79;
// Category 6 is Culture; Poseidon's maps call it Science (text 0x18 of the same group), as the name bar does.
constexpr int kCultureCategory = 6, kScienceText = 0x18;
// Two buildings that Poseidon's maps name differently (their texts in group 28).
constexpr int kBuildingA = 0x49, kBuildingAPoseidon = 0xd7;
constexpr int kBuildingB = 0x88, kBuildingBPoseidon = 0xa4;

// balloon_help_update draws no tooltip while the text number is 0 (game_frame 0x5137ff; balloon_help_end 0x4f6af0
// takes only numbers above 0), and Population is text 0 of group 88. A name with text 0 is therefore handed over as
// this number and swapped for the real one only while the tooltip is drawn. Above every text of the game's groups.
constexpr int kStandIn = 0x7fff;

struct Name {
    int group;
    int text;
};

// What the game says about the button under the mouse (read by games/zeus/side_panel).
struct Facts {
    bool poseidon;     // science_names(0): a Poseidon map (its names for Culture and two buildings)
    bool reportOpen;   // sidebar_dialog_open: the sidebar's report is open
    bool buildEnabled; // the hovered build button's state is 1 (it can be used)
    int building;      // the hovered build button's building type
    bool boxMessages;  // the box at the bottom shows the messages (else the minimap)
};

// The box's toggle (ours, hover number 201): the game's own words for switching to the other one: "View messages"
// (group 68, the mouse help) while the box shows the minimap, "Show Overview Map" (group 88, the old tab's) while it
// shows the messages.
constexpr int kToggleButton = 201;
constexpr int kGroupMouseHelp = 68, kViewMessages = 33, kShowOverview = 16;

// One of the buttons whose name the name bar shows, or the box's toggle.
bool Ours(int hover);
// A build button (0x32..0x35).
bool BuildButton(int hover);
// The building's name text in group 28 (Poseidon's two renamed buildings).
int BuildingText(int building, bool poseidon);
// The name for hover, as the name bar would show it; false when it shows none (not ours, a build button that
// cannot be used, or the title while its report is open).
bool NameFor(int hover, const Facts &f, Name *out);

// The game's state as balloon_help_update 0x4f6110 sees it on the report screen (game_screen 7).
struct ReportFacts {
    bool screenRedraw; // screen_redraw is not 0: the game decides nothing this frame
    int balloonHelp;   // balloon_help: 0 off
    int dialog;        // sidebar_dialog
    int hover;
};
// Whether we answer for the sidebar's buttons ahead of balloon_help_update on the report screen, where the name bar
// showed their names on every report: the game asks balloon_help_button_text there only for two kinds of report,
// and several reports set screen_redraw on every frame they draw (the trade and the gods' reports), so that the
// game never decides a tooltip while they are open. Answered on every report and whether or not screen_redraw is
// set (the tooltip is drawn after the report in the same frame; balloon_help_end then leaves screen_redraw as it
// is); only Balloon help off hides the names.
bool AnswerInReport(const ReportFacts &f);

// The group for the name handed to the game in a frame: given only in the same frame and for the same text, so
// that every other tooltip keeps the game's own group.
class TipMemo {
public:
    // Remembers n for frame; returns the number to put into balloon_help_text (the text, or kStandIn for text 0).
    int Remember(int frame, const Name &n);
    void Forget()
    {
        on_ = false;
    }
    // The group for text in frame, when it is the one remembered.
    bool GroupFor(int frame, int text, int *group) const;
    // The number to put into balloon_help_text while the tooltip is drawn, when value is our stand-in.
    bool RealText(int value, int *text) const;

private:
    bool on_ = false;
    int frame_ = 0;
    Name name_{};
};

// The cost drawn under a build button: the game's name bar set screen_redraw on every frame it drew it (the
// background drawn again cleared it), which also kept balloon_help_update from deciding anything in that frame. Ours
// asks for one redraw when the cost moves to another button or goes away.
class CostTrail {
public:
    // drawn: the hover number whose cost was drawn this frame, 0 for none. True: set screen_redraw now.
    bool Frame(int drawn)
    {
        const bool clear = last_ != 0 && drawn != last_;
        last_ = drawn;
        return clear;
    }
    void Reset()
    {
        last_ = 0;
    }
    int Last() const
    {
        return last_;
    }

private:
    int last_ = 0;
};

// ---- The column moved up ----
//
// The sidebar's right column (from the tabs down to the build buttons) without the tabs: the game draws the column in
// its own place (sidebar_panel_draw 0x517480: the title at y 0x1e, the overlay buttons from y 0x32, the information
// panel and the category's own panel above y 300), and that drawing is moved up by the tabs' 25 rows. Every frame
// starts from a copy of the column as the backdrop left it (taken when the backdrop has just been drawn), so that
// what the game draws only on some frames (its buttons) or never again (the marble under the title) is not moved a
// second time. Pixels the game left as the backdrop had them are not moved: they become the backdrop's pixels at
// the new place, so that the marble stays where it is.

// The column, in sidebar columns (x - sidebar_x) and screen rows (the 1024 layout's).
constexpr int kColumnLeft = 50, kColumnRight = 186, kColumnW = kColumnRight - kColumnLeft;
// The backdrop as taken (rows 0..471: down to the old minimap's place and the four round buttons below it, which the
// game draws after the column), and the rows put back from it on every frame (0..453: the build buttons, their costs
// and the Overview category's longer panel; the round buttons from row 435 are drawn again by the game afterwards).
constexpr int kColumnRows = 472;
constexpr int kPutBackEnd = 454;
constexpr int kShift = 25; // the tabs' rows (0..24)
// The rows moved (the game's rows 25..332: the title's place down to the information panel's new bottom edge) and
// where they go (0..307); the rows below them (308..453) are the backdrop's, and the build buttons are drawn there.
constexpr int kMovedFirst = kShift, kMovedEnd = 333, kMovedRows = kMovedEnd - kMovedFirst;
// The Overview category has no build buttons and no minimap of its own: its information panel goes on down to the old
// minimap's last row (the game's row 453, 428 moved; the round buttons start at 435), and rows 25..453 are moved.
constexpr int kOverviewMovedEnd = 454;
// The rows moved for a category (kMovedEnd, or kOverviewMovedEnd for the Overview category).
int MovedEndFor(int category);
// The title: centered in 0x82 px from column 50, at y 0x1e in the game's place (5 moved); clicked and hovered over
// its whole row above the overlay buttons.
constexpr int kTitleX = kColumnLeft, kTitleW = 0x82, kTitleY = 0x1e - kShift;
constexpr int kTitleRight = 182, kTitleBottom = kShift;
// The left part of the tabs' rows (columns 0..49 of rows 0..21, above the category buttons): what tells whether the
// backdrop is the one whose army panel's picture (Zeus_Interface entry 1233) has the same rows without the tabs.
constexpr int kTopCheckW = kColumnLeft, kTopCheckRows = 22;
// The information panel: the game's height for n overlay buttons, two tiles (32 rows) longer at the bottom.
struct InfoPanel {
    int top, rows; // rows of 16 px
};
InfoPanel InfoPanelFor(int overlayButtons);
// The font of a font group as the game picks it: the group table's entry g (8 bytes each) while the table has more
// than g entries, else entry 0. Returns the byte offset of the entry.
int FontEntryOffset(int group, int count);
// The title's font groups: the game's (1) and the one while it can be clicked and the mouse is on it (15).
constexpr int kTitleFont = 1, kTitleHoverFont = 15;

// The report for the category under the title: the report button's rule (sidebar_report_button_draw 0x516fa0): a
// report kind, and category 9 only on Poseidon's maps.
constexpr int kCategoryCount = 11, kPoseidonOnlyReport = 9, kOverviewCategory = 10;
bool TitleClickable(int category, int reportKind, bool poseidon);
// A point in sidebar columns and screen rows: on the title.
bool OnTitle(int col, int y);
// Whether the mouse (sidebar column, screen row) is where the game's panels are seen moved: those inputs are given
// the game's row (y + kShift).
bool InMovedArea(int col, int y);

// 16-bit pixels of the column (kColumnW x kColumnRows) and of the tabs' rows of entry 1233 (kColumnW x kShift),
// row by row.
using Px = unsigned short;

// Whether the left part of the tabs' rows on the screen (kTopCheckW x kTopCheckRows, row by row) is entry 1233's
// (entry: its full width w): at most one step of each colour channel apart (the game's 16-bit drawing and ours),
// and nearly every pixel.
bool TopFits(const Px *screenLeft, const Px *entry, int entryW);

// The column on screen (`screen`: its first pixel at the column's left edge and row 0, `pitch` pixels a row):
// rows kMovedFirst..movedEnd - 1 moved up by kShift, the pixels that equal the backdrop at the same place replaced
// by the backdrop at the new place (the tabs' rows from `top`, or, without it, the backdrop's next 25 rows), and the
// rows below the moved ones down to kPutBackEnd put back from the backdrop.
void MoveColumn(Px *screen, int pitch, const Px *backdrop, const Px *top, int movedEnd = kMovedEnd);
// Whether the game drew anything below the moved rows (rows movedEnd..kPutBackEnd - 1 differ from the backdrop).
bool DrawnBelow(const Px *screen, int pitch, const Px *backdrop, int movedEnd = kMovedEnd);

// The column as the backdrop left it, and where it was taken: put back only on the same screen.
class Backdrop {
public:
    struct Where {
        int sidebarX, width, height, pitch;
        bool operator==(const Where &o) const
        {
            return sidebarX == o.sidebarX && width == o.width && height == o.height && pitch == o.pitch;
        }
    };
    // Copies the column from screen (as MoveColumn's screen): rows 0..kColumnRows - 1.
    void Take(const Px *screen, int pitch, const Where &w);
    bool Has(const Where &w) const
    {
        return has_ && where_ == w;
    }
    // Rows 0..kPutBackEnd - 1.
    void PutBack(Px *screen, int pitch) const;
    void Forget()
    {
        has_ = false;
    }
    const Px *Pixels() const
    {
        return px_;
    }

private:
    bool has_ = false;
    Where where_{};
    Px px_[kColumnW * kColumnRows];
};

// ---- The box at the bottom ----
//
// Below the Go to buttons and the message count (rows 478..511, as the game has them): one box for the minimap or
// the messages (an inset panel of the game's tiles where the message box, the button row and the picture were), the
// check / gem / globe row moved under it, and, on taller screens, an inset panel below 768 (the box and the buttons
// stay where they are on the 1024 x 768 layout). In sidebar columns (x - sidebar_x) and screen rows.
constexpr int kSidebarW = 186, kLayoutH = 768;
constexpr int kLowerTop = 512; // the first row laid out again (the old message box's)
constexpr int kBoxX = 9, kBoxY = 512, kBoxW = 172, kBoxH = 218; // the inset panel, rows 512..729
// The minimap: minimap_draw(sidebar_x + 12, 515, 83 columns of 2 px, 212 rows).
constexpr int kMinimapX = 12, kMinimapY = 515, kMinimapCols = 83, kMinimapRows = 212, kMinimapW = kMinimapCols * 2;
// The toggle: the report button's picture (and the next one under the mouse), the box's top right corner.
constexpr int kToggleX = 160, kToggleY = 515, kToggleSize = 18;
// The messages: the game's text drawing (its width, its lines), one after another from the top, as many as fit.
constexpr int kTextX = 13, kTextW = 148, kTextY = 517, kTextBottom = 726, kMessageGap = 6;
// The game's message box (sidebar_messages 0x7815a0: x sidebar_x + 8, width 0x9b): its arrows at x + width and its
// table's places (y + 2, y + 31), here at rows 535 and 708 below the toggle; clicked within its x and the box's rows.
constexpr int kListX = 8, kListW = 0x9b;
constexpr int kArrowUpY = 535, kArrowDownY = 708, kArrowUpOffset = 2, kArrowDownOffset = 31;
// The check / gem / globe row (rows 563..599 of the backdrop) at rows 731..767; their buttons' y in the game's tables
// moved by the same.
constexpr int kButtonRowTop = 563, kButtonRowEnd = 600, kButtonRowShift = 168;
// The edges carried down: the left one as below 768 elsewhere (its columns 0..10, rows 640..699 repeated), the right
// one's 5 columns (rows 512..561 repeated), and the plain row 562 between them.
constexpr int kEdgeW = 11, kEdgeBandTop = 640, kEdgeBandRows = 60;
constexpr int kRightX = 181, kRightBandTop = 512, kRightBandRows = 50, kPlainRow = 562;
// Below 768: an inset panel from column 11, 172 wide.
constexpr int kTallX = 11, kTallW = 172;

// The rows from kLowerTop down to height laid out from the backdrop on screen (`sidebar`: its first pixel at the
// sidebar's left edge and row 0, `pitch` pixels a row; height at least kLayoutH): the edges and the plain row, and the
// button row at its new place. The inset panels are drawn over it afterwards (the game's tiles).
void LowerHalf(Px *sidebar, int pitch, int height);

// A map smaller than the minimap's place: minimap_draw fills the rest with a filler picture that cannot be clicked
// (games/zeus/minimap_part). The box's inset (`backdrop`, kBoxW x kBoxH) is put back on screen (`box`: the box's
// first pixel, `pitch` pixels a row) everywhere in the box outside `part` (the map's rectangle in box coordinates,
// right and bottom exclusive), as Emperor's sidebar shows only the map. A map as large as the box: nothing changes.
// Returns the number of rows touched.
int PutBackAroundMap(Px *box, int pitch, const Px *backdrop, long partLeft, long partTop, long partRight,
                     long partBottom);

// At the map's edges minimap_center keeps its origin a little outside the map (2 half rows above it, 1 or 2 below,
// a column to the left: its bounds, not the map's range), so a map larger than the place leaves a gap of filler on
// that side. The drawing that closes it: the origin worked out as minimap_center will (games/zeus/minimap_part),
// for each direction the smallest move and growth, within the box's margins around the place, that puts the map's
// edges at or beyond the place's on both sides. A map smaller than the place in a direction, or no such drawing:
// that direction as placed. Returns false when the drawing is the place itself.
struct MinimapDrawing {
    int x, y, cols, rows;
};
bool FitMinimap(const MinimapDrawing &place, const zeus::minimap_part::CenterInputs &c,
                const zeus::minimap_part::MapRange &m, MinimapDrawing *out);

// Where a point (sidebar column, screen row) is.
bool OnToggle(int col, int y);
bool OnMinimap(int col, int y);
bool OnList(int col, int y); // the game's message box, as clicked: its columns, the box's rows

// The lines that the game's text_draw_wrapped 0x5090a0 makes of a text in `limit` px (its rules: words of
// 0x5091b0 -- leading spaces 4 px each, '$' a break without width, characters below 0x20 none -- added while the line
// stays narrower than limit; a word that does not fit starts the next line; at most 99 lines). width: the game's
// text_char_width for a character of 0x20 or above.
int WrapLines(const unsigned char *text, int limit, int (*width)(unsigned char c, void *ctx), void *ctx);
// Whether a message of `lines` lines (each 5 + lineHeight rows, as text_draw_wrapped advances) fits from row y.
bool MessageFits(int y, int lines, int lineHeight);

// ---- The army and ship panels' upper half ----
//
// As on Emperor's army panel: no minimap of its own at the top (the box at the bottom has one), the name box, the
// state and the command buttons moved up into its place, the inset panel below them as tall as the room left. The
// game draws that sidebar's backdrop on every frame (sidebar_draw_units 0x525280), and the whole of it is moved up
// right after that, before the command buttons, the inset and its text are drawn (their places are moved in the
// game's tables): rows 138..470 (from below the light bottom line of the minimap's recess) go to rows 0..332 with
// their marble, so nothing meets marble that was not moved; rows 333..471 are rows 469..331 mirrored (row 470 the
// axis: rows 471..477 hold the light line above the Go to row, drawn as they are), mostly covered by the taller
// inset; the left edge's top (columns 0..10 of rows 0..30: the marble that runs into the top bar and the edge's dark
// end) stays where it is.
constexpr int kUnitsShift = 138;
constexpr int kUnitsMovedEnd = 333;  // rows 0..332 from 138..470
constexpr int kUnitsMirrorAxis = 470;
constexpr int kUnitsEnd = 472;       // rows 333..471 mirrored; from 472 as drawn
constexpr int kUnitsCapW = kEdgeW, kUnitsCapRows = 31;
// The inset panel (the game's: (14, 360), 10 x 7 tiles), from below the command buttons down to row 471.
constexpr int kUnitsInsetX = 14, kUnitsInsetY = 222, kUnitsInsetW = 160, kUnitsInsetH = 250;
// The game's rows of the command buttons (two tables of 6, from y 173, 225 and 277 at the base 0x1e) and of the
// texts in the inset (0x168 and the clickable rows of 0x640480 at 335..435) move up by the same.

// The row that a pixel of the moved panel comes from (sidebar column, screen row); -1 for one that stays as drawn.
int UnitsSourceRow(int col, int y);
// The army panel on screen (`sidebar` as LowerHalf's) moved as above; rows below kUnitsEnd are not touched.
void MoveUnits(Px *sidebar, int pitch);

// ---- The category panels in the taller column ----
//
// Military (category 8, military_panel_draw 0x51b060 / military_panel_input 0x51b470): its lower block (three rows of
// icons and texts, three text buttons and Auto Defend, table 0x63ed68) is placed from one base row, n x 20 + 0xe0 for
// the drawing and + 0xe6 for the input (n: the overlay buttons); both bases are 32 rows lower in the game's code, so
// that the block sits against the information panel's bottom edge as the game has it (2 rows above it). Auto Defend
// (18 x 28 at (0x46 + 86, base + 6)) is drawn 48 rows high, as tall as the three buttons beside it: its pictures'
// rows 2 and 25 are plain, so each is repeated 10 more times (the helmet, rows 4..22, in the middle, not scaled), and
// its record's height is 48 (clicked and hovered over the same rows). Not while its state is 2 (the game draws
// nothing then).
constexpr int kMilitaryCategory = 8;
constexpr int kAutoDefendCol = 0x46 + 86, kAutoDefendW = 18, kAutoDefendH = 28, kAutoDefendTallH = 48;
constexpr int kAutoDefendRowA = 2, kAutoDefendRowB = 25, kAutoDefendRepeat = 10;
// Auto Defend's first row on screen (the game's place, before the column is moved up) for n overlay buttons.
int AutoDefendTop(int overlayButtons);
// The row of the game's 28 that row r (0..47) of the tall button shows.
int AutoDefendSourceRow(int r);
// The 18 x 28 drawn at `at` (pitch pixels a row) made 48 rows high in place.
void StretchAutoDefend(Px *at, int pitch);

// Overview (category 10): up to 8 requests (the game lists 3 there, 8 in the City Overview report, whose input also
// reads 8), each one clicked as the game's first 3 (its rows table 0x65b878 has 3 records with the report's table
// right after it, so ours has 8). The rows' records (0x1c bytes, as buttons_mouse_over 0x432300 and
// buttons_handle_click 0x4321e0 take them): x1, y1, x2, y2 (short), the actions at 8 and 0xc, the request's index at
// 0x14. Hovered request k (from 1) is hover number 0x63f + k; balloon_help_button_text has a text only for 0x640..0x642
// (0xc1 + request_tip_kind(k - 1)), ours the others up to the 8th.
constexpr int kMaxRequests = 8, kGameRequests = 3;
constexpr int kRequestRowSize = 0x1c, kRequestRowH = 20, kRequestRowIndex = 0x14;
constexpr int kFirstRequestHover = 0x640, kRequestHoverBase = 0x63f, kRequestTipText = 0xc1;
// The requests listed and clicked (the game's min(count, 3) with 8).
int RequestRows(int count);
// The request rows' table: 8 records like the game's first (`first`), each kRequestRowH lower with its own index.
void RequestTable(const unsigned char *first, unsigned char *out);
// A hover number of a request beyond the game's 3 (up to the 8th): true, and its index from 0.
bool ExtraRequest(int hover, int *index);

// The City Overview report's tooltip (balloon_help_draw 0x4f6ba0): on the report screen the game puts it at the
// mouse's y - 0x40 (the tooltip's mouse row 0xf2a72c), and from dialog_offset_y + 0x1b0 on at that row, so that it
// covers the title above the first request and the row under the mouse from the 6th. For the report's requests
// (sidebar_dialog 0xd, hover 0x6a4..0x6ab) it is put below the cursor as on the city screen: while the game draws it,
// the row it reads is the mouse's + kReportTipRow and dialog_offset_y kReportTipOffset more, so that its rule gives
// the mouse's y + 40.
constexpr int kCityOverviewReport = 0xd, kFirstReportRequest = 0x6a4, kLastReportRequest = 0x6ab;
constexpr int kReportTipRow = 0x68, kReportTipOffset = 0x200;
bool ReportRequestTip(int screen, int dialog, int hover);

// ---- The world map's sidebar ----
//
// The picture's frame is not at the same place on every sidebar (Zeus's city and world map sidebars: rows 604..761;
// Poseidon's city sidebar 610..755, Poseidon's world map sidebar 614..759; Poseidon's city picture 1 px right of the
// world map's). Which one it is: the frame's top row, found on the sidebar as the game drew it: the first dark row of
// column 93 from row 600 (the frame's dark line, blue <= 13 of 31; the plain blue above it is 16 or more); another
// row, or none by row 630: as Zeus's.
struct Picture {
    int frameTop, frameH, dx;
};
constexpr int kFrameColumn = 93, kFrameSearchTop = 600, kFrameSearchLast = 630;
int FrameTop(const Px *sidebar, int pitch, int height);
Picture PictureOf(int frameTop);

// The world and campaign map sidebar (Zeus_Interface entry 1600 or Poseidon's) laid out as the city's side panel:
// Back to City and the question mark (their table's y + 168, the game's code) on the row of the city sidebar's check,
// gem and globe, and the picture up above them. Done right after the game draws the sidebar's picture, before its
// buttons and texts: rows 563..566 stay (they continue the marble above); the frame (columns 11..180 of its rows)
// moves up to start at row 567, the gold beaded edge (columns 0..10) and the bevel right of it (181..185) as drawn;
// below it down to 767 (columns 11..185) the rows under the frame (its shadow, frameLast + 1..767) mirrored back and
// forth, the same darker marble all the way (the lighter rows that were under the buttons would show as a lighter
// patch). Every seam then has on its two sides rows that the game drew next to each other, or the frame's own edge.
//
// Poseidon's world map sidebar has the same rows as Zeus's above its picture, but its frame is 1 px right (columns
// 12..181) and the stone around and under the frame is darker, which shows against the lighter columns beside the
// beaded edge. There the frame is moved with its own columns, and everything else in rows 567..767 from column 11 on
// (column 11, the columns right of the frame, the rows under it) is the stone under Zeus's frame (Zeus_Interface entry
// 1600, rows 762..767) mirrored back and forth from row 567 (without it: Poseidon's own rows under its frame, as on
// Zeus's sidebar).
constexpr int kWorldKeepEnd = 567, kWorldEdgeW = 11, kWorldFrameW = 170;
constexpr int kWorldButtonShift = 168; // Back to City 570 -> 738, the question mark 572 -> 740
constexpr int kZeusWorldFrameTop = 604;
constexpr int kStoneTop = 762, kStoneRows = 6; // entry 1600's rows under its frame (186 wide)
// The frame's first column: Zeus's 11, Poseidon's world map's 12.
int WorldFrameLeft(const Picture &p);
// Where a pixel of the laid-out sidebar comes from (sidebar column, row), the frame found at frameTop (its last row
// frameTop + frameH - 1): the sidebar's own row, a row of the stone (0..kStoneRows - 1; only with `stone`), or as drawn.
struct WorldSource {
    enum Kind { Drawn, Own, Stone } kind;
    int row;
};
WorldSource WorldSourceOf(int col, int y, const Picture &p, bool stone);
// The sidebar on screen (`sidebar`: its first pixel at its left edge and row 0, `pitch` pixels a row, 768 rows or more)
// laid out as above; `stone`: entry 1600's rows 762..767 (kSidebarW x kStoneRows, row by row), or nullptr. Returns
// the frame's top row as found.
int LayOutWorldSidebar(Px *sidebar, int pitch, int height, const Px *stone);

// A 5-byte `call target` at `at` (both as the running image has them).
void CallBytes(unsigned long at, unsigned long target, unsigned char out[5]);

// The messages drawn in the box (their index in the game's list and their rows), for a click on one of them.
class MessageList {
public:
    static constexpr int kMax = 64;
    void Begin()
    {
        n_ = 0;
    }
    void Add(int index, int top, int bottom)
    {
        if (n_ < kMax) {
            s_[n_++] = Shown{index, top, bottom};
        }
    }
    // The message under row y, -1 for none.
    int At(int y) const;
    int Count() const
    {
        return n_;
    }

private:
    struct Shown {
        int index, top, bottom;
    };
    Shown s_[kMax];
    int n_ = 0;
};

} // namespace zeus::side_panel_layout
