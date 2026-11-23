#include "games/zeus/side_panel_layout.h"

#include <algorithm>
#include <cstring>

namespace zeus::side_panel_layout {

bool BuildButton(int hover)
{
    return hover >= kFirstBuildButton && hover <= kLastBuildButton;
}

bool Ours(int hover)
{
    return (hover >= kFirstSidebarButton && hover <= kLastSidebarButton) || BuildButton(hover) ||
           hover == kReportButton || hover == kToggleButton;
}

int BuildingText(int building, bool poseidon)
{
    if (poseidon) {
        if (building == kBuildingA) {
            return kBuildingAPoseidon;
        }
        if (building == kBuildingB) {
            return kBuildingBPoseidon;
        }
    }
    return building;
}

bool NameFor(int hover, const Facts &f, Name *out)
{
    if (hover == kToggleButton) {
        *out = f.boxMessages ? Name{kGroupSidebar, kShowOverview} : Name{kGroupMouseHelp, kViewMessages};
        return true;
    }
    if (hover == kReportButton) {
        if (f.reportOpen) {
            return false;
        }
        *out = Name{kGroupReport, kReportShow};
        return true;
    }
    if (hover >= kFirstSidebarButton && hover <= kLastSidebarButton) {
        int text = hover - kFirstSidebarButton;
        if (text == kCultureCategory && f.poseidon) {
            text = kScienceText;
        }
        *out = Name{kGroupSidebar, text};
        return true;
    }
    if (BuildButton(hover) && f.buildEnabled) {
        *out = Name{kGroupBuildings, BuildingText(f.building, f.poseidon)};
        return true;
    }
    return false;
}

bool AnswerInReport(const ReportFacts &f)
{
    return f.balloonHelp != 0 && Ours(f.hover);
}

int TipMemo::Remember(int frame, const Name &n)
{
    on_ = true;
    frame_ = frame;
    name_ = n;
    return n.text != 0 ? n.text : kStandIn;
}

bool TipMemo::GroupFor(int frame, int text, int *group) const
{
    if (!on_ || frame != frame_ || text != name_.text) {
        return false;
    }
    *group = name_.group;
    return true;
}

bool TipMemo::RealText(int value, int *text) const
{
    if (!on_ || value != kStandIn || name_.text != 0) {
        return false;
    }
    *text = name_.text;
    return true;
}

InfoPanel InfoPanelFor(int overlayButtons)
{
    // sidebar_panel_draw 0x517480: (11 - n) x 20 rounded up to whole tiles plus two (at least one more tile), from
    // row 300 upwards.
    const unsigned spare = static_cast<unsigned>(11 - overlayButtons) * 20u;
    const int h = static_cast<int>(spare + (32u - (spare & 15u)));
    return InfoPanel{300 - h, h / 16 + 2};
}

int FontEntryOffset(int group, int count)
{
    return count > group ? group * 8 : 0;
}

bool TitleClickable(int category, int reportKind, bool poseidon)
{
    if (category < 0 || category >= kCategoryCount || reportKind == 0) {
        return false;
    }
    return category != kPoseidonOnlyReport || poseidon;
}

bool OnTitle(int col, int y)
{
    return col >= kTitleX && col < kTitleRight && y >= 0 && y < kTitleBottom;
}

bool InMovedArea(int col, int y)
{
    return col >= kColumnLeft && col < kColumnRight && y >= 0 && y < kMovedEnd - kShift;
}

namespace {

bool Near(Px a, Px b)
{
    const int dr = (a >> 11) - (b >> 11), dg = ((a >> 5) & 0x3f) - ((b >> 5) & 0x3f), db = (a & 0x1f) - (b & 0x1f);
    return dr >= -1 && dr <= 1 && dg >= -1 && dg <= 1 && db >= -1 && db <= 1;
}

} // namespace

bool TopFits(const Px *screenLeft, const Px *entry, int entryW)
{
    int off = 0;
    for (int y = 0; y < kTopCheckRows; ++y) {
        for (int x = 0; x < kTopCheckW; ++x) {
            off += !Near(screenLeft[y * kTopCheckW + x], entry[y * entryW + x]);
        }
    }
    return off * 50 <= kTopCheckRows * kTopCheckW; // 98% or more
}

int MovedEndFor(int category)
{
    return category == kOverviewCategory ? kOverviewMovedEnd : kMovedEnd;
}

void MoveColumn(Px *screen, int pitch, const Px *backdrop, const Px *top, int movedEnd)
{
    const int movedRows = movedEnd - kMovedFirst;
    // Top down: each row is read (kShift rows below) before anything is written over it.
    for (int dst = 0; dst < movedRows; ++dst) {
        const int src = dst + kShift;
        Px *to = screen + dst * pitch;
        const Px *from = screen + src * pitch;
        const Px *was = backdrop + src * kColumnW;
        const Px *under = dst < kShift ? (top ? top + dst * kColumnW : backdrop + src * kColumnW)
                                       : backdrop + dst * kColumnW;
        for (int x = 0; x < kColumnW; ++x) {
            const Px p = from[x];
            to[x] = p == was[x] ? under[x] : p;
        }
    }
    for (int y = movedRows; y < kPutBackEnd; ++y) {
        const Px *b = backdrop + y * kColumnW;
        Px *to = screen + y * pitch;
        for (int x = 0; x < kColumnW; ++x) {
            to[x] = b[x];
        }
    }
}

bool DrawnBelow(const Px *screen, int pitch, const Px *backdrop, int movedEnd)
{
    for (int y = movedEnd; y < kPutBackEnd; ++y) {
        for (int x = 0; x < kColumnW; ++x) {
            if (screen[y * pitch + x] != backdrop[y * kColumnW + x]) {
                return true;
            }
        }
    }
    return false;
}

void Backdrop::Take(const Px *screen, int pitch, const Where &w)
{
    for (int y = 0; y < kColumnRows; ++y) {
        for (int x = 0; x < kColumnW; ++x) {
            px_[y * kColumnW + x] = screen[y * pitch + x];
        }
    }
    where_ = w;
    has_ = true;
}

void Backdrop::PutBack(Px *screen, int pitch) const
{
    for (int y = 0; y < kPutBackEnd; ++y) {
        for (int x = 0; x < kColumnW; ++x) {
            screen[y * pitch + x] = px_[y * kColumnW + x];
        }
    }
}

void LowerHalf(Px *sidebar, int pitch, int height)
{
    // The rows read lie in the rows written: copied first.
    constexpr int kRowRows = kButtonRowEnd - kButtonRowTop;
    Px edge[kEdgeBandRows][kEdgeW], right[kRightBandRows][kSidebarW - kRightX], plain[kRightX - kEdgeW];
    Px row[kRowRows][kSidebarW];
    for (int y = 0; y < kEdgeBandRows; ++y) {
        for (int x = 0; x < kEdgeW; ++x) {
            edge[y][x] = sidebar[(kEdgeBandTop + y) * pitch + x];
        }
    }
    for (int y = 0; y < kRightBandRows; ++y) {
        for (int x = kRightX; x < kSidebarW; ++x) {
            right[y][x - kRightX] = sidebar[(kRightBandTop + y) * pitch + x];
        }
    }
    for (int x = kEdgeW; x < kRightX; ++x) {
        plain[x - kEdgeW] = sidebar[kPlainRow * pitch + x];
    }
    for (int y = 0; y < kRowRows; ++y) {
        for (int x = 0; x < kSidebarW; ++x) {
            row[y][x] = sidebar[(kButtonRowTop + y) * pitch + x];
        }
    }
    for (int y = kLowerTop; y < height; ++y) {
        Px *to = sidebar + y * pitch;
        const Px *e = edge[(y - kLowerTop) % kEdgeBandRows];
        const Px *r = right[(y - kLowerTop) % kRightBandRows];
        for (int x = 0; x < kEdgeW; ++x) {
            to[x] = e[x];
        }
        for (int x = kEdgeW; x < kRightX; ++x) {
            to[x] = plain[x - kEdgeW];
        }
        for (int x = kRightX; x < kSidebarW; ++x) {
            to[x] = r[x - kRightX];
        }
    }
    for (int y = 0; y < kRowRows; ++y) {
        Px *to = sidebar + (kButtonRowTop + kButtonRowShift + y) * pitch;
        for (int x = 0; x < kSidebarW; ++x) {
            to[x] = row[y][x];
        }
    }
}

int UnitsSourceRow(int col, int y)
{
    if (y < 0 || y >= kUnitsEnd || col < 0 || col >= kSidebarW || (col < kUnitsCapW && y < kUnitsCapRows)) {
        return -1;
    }
    const int from = y + kUnitsShift;
    return from <= kUnitsMirrorAxis ? from : 2 * kUnitsMirrorAxis - from;
}

void MoveUnits(Px *sidebar, int pitch)
{
    // The rows read (331..470) overlap the rows written (0..471): copied first.
    constexpr int kFirst = kUnitsShift, kRows = kUnitsMirrorAxis + 1 - kUnitsShift;
    static Px from[kRows][kSidebarW];
    for (int y = 0; y < kRows; ++y) {
        const Px *s = sidebar + (kFirst + y) * pitch;
        for (int x = 0; x < kSidebarW; ++x) {
            from[y][x] = s[x];
        }
    }
    for (int y = 0; y < kUnitsEnd; ++y) {
        Px *to = sidebar + y * pitch;
        for (int x = 0; x < kSidebarW; ++x) {
            const int src = UnitsSourceRow(x, y);
            if (src >= 0) {
                to[x] = from[src - kFirst][x];
            }
        }
    }
}

bool OnToggle(int col, int y)
{
    return col >= kToggleX && col < kToggleX + kToggleSize && y >= kToggleY && y < kToggleY + kToggleSize;
}

int PutBackAroundMap(Px *box, int pitch, const Px *backdrop, long partLeft, long partTop, long partRight,
                     long partBottom)
{
    const long left = std::clamp<long>(partLeft, 0, kBoxW), right = std::clamp<long>(partRight, left, kBoxW);
    const long top = std::clamp<long>(partTop, 0, kBoxH), bottom = std::clamp<long>(partBottom, top, kBoxH);
    int touched = 0;
    for (int y = 0; y < kBoxH; ++y) {
        Px *row = box + y * pitch;
        const Px *from = backdrop + y * kBoxW;
        if (y < top || y >= bottom || left >= right) {
            std::memcpy(row, from, kBoxW * sizeof(Px));
            ++touched;
            continue;
        }
        if (left > 0 || right < kBoxW) {
            std::memcpy(row, from, static_cast<size_t>(left) * sizeof(Px));
            std::memcpy(row + right, from + right, static_cast<size_t>(kBoxW - right) * sizeof(Px));
            ++touched;
        }
    }
    return touched;
}

bool FitMinimap(const MinimapDrawing &place, const zeus::minimap_part::CenterInputs &c,
                const zeus::minimap_part::MapRange &m, MinimapDrawing *out)
{
    namespace mp = zeus::minimap_part;
    // The box's margins around the place: a larger drawing stays inside the box (its inset is put back there).
    constexpr int kLeftMargin = kMinimapX - kBoxX, kTopMargin = kMinimapY - kBoxY;
    constexpr int kRightMargin = kBoxX + kBoxW - (kMinimapX + kMinimapW);
    constexpr int kBottomMargin = kBoxY + kBoxH - (kMinimapY + kMinimapRows);
    const long left = place.x, right = place.x + 2L * place.cols, top = place.y, bottom = place.y + place.rows;
    MinimapDrawing d = place;
    // Columns: x moved left by dx, dw columns more; the drawing within [left - kLeftMargin, right + kRightMargin].
    auto PartCols = [&](int x, int w) {
        return mp::MapPart(x, place.y, mp::CenterCol(c, w), mp::CenterRow(c, place.rows), m);
    };
    const RECT asPlaced = PartCols(place.x, place.cols);
    if (asPlaced.right - asPlaced.left >= right - left && (asPlaced.left > left || asPlaced.right < right)) {
        bool found = false;
        for (int cost = 1; cost <= kLeftMargin + kRightMargin && !found; ++cost) {
            for (int dx = 0; dx <= std::min(cost, kLeftMargin) && !found; ++dx) {
                const int dw = cost - dx; // columns: 2 px each; the drawing still covers the place
                if (2 * dw - dx > kRightMargin || 2 * dw < dx) {
                    continue;
                }
                const RECT a = PartCols(place.x - dx, place.cols + dw);
                if (a.left <= left && a.right >= right) {
                    d.x = place.x - dx;
                    d.cols = place.cols + dw;
                    found = true;
                }
            }
        }
    }
    // Half rows likewise: y moved up by dy, dh rows more.
    auto PartRows = [&](int y, int h) {
        return mp::MapPart(place.x, y, mp::CenterCol(c, place.cols), mp::CenterRow(c, h), m);
    };
    const RECT asPlacedRows = PartRows(place.y, place.rows);
    if (asPlacedRows.bottom - asPlacedRows.top >= bottom - top &&
        (asPlacedRows.top > top || asPlacedRows.bottom < bottom)) {
        bool found = false;
        for (int cost = 1; cost <= 2 * kTopMargin + kBottomMargin && !found; ++cost) {
            for (int dy = 0; dy <= std::min(cost, kTopMargin) && !found; ++dy) {
                const int dh = cost - dy;
                if (dh - dy > kBottomMargin || dh < dy) {
                    continue;
                }
                const RECT a = PartRows(place.y - dy, place.rows + dh);
                if (a.top <= top && a.bottom >= bottom) {
                    d.y = place.y - dy;
                    d.rows = place.rows + dh;
                    found = true;
                }
            }
        }
    }
    *out = d;
    return d.x != place.x || d.y != place.y || d.cols != place.cols || d.rows != place.rows;
}

bool OnMinimap(int col, int y)
{
    return col >= kMinimapX && col < kMinimapX + kMinimapW && y >= kMinimapY && y < kMinimapY + kMinimapRows;
}

bool OnList(int col, int y)
{
    // As the game's sidebar_messages_input: x .. x + width and y .. y + height, both ends in.
    return col >= kListX && col <= kListX + kListW && y >= kMinimapY && y <= kMinimapY + kMinimapRows;
}

namespace {

// A word as 0x5091b0 measures it from p: its width, and in *count the characters it takes (not the one it stops at).
int Word(const unsigned char *p, int *count, int (*width)(unsigned char, void *), void *ctx)
{
    bool seen = false;
    int w = 0;
    *count = 0;
    for (int k = 1; k < 2000; ++k) {
        const unsigned char c = *p++;
        if (c == 0) {
            return w;
        }
        if (c == 0x20) {
            if (seen) {
                return w;
            }
            w += 4;
        }
        else if (c == 0x24) {
            if (seen) {
                return w;
            }
        }
        else if (c >= 0x20) {
            w += width(c, ctx);
            seen = true;
        }
        ++*count;
    }
    return w;
}

} // namespace

int WrapLines(const unsigned char *text, int limit, int (*width)(unsigned char c, void *ctx), void *ctx)
{
    const unsigned char *p = text;
    bool more = true;
    int lines = 0;
    for (int n = 1; n < 100; ++n) {
        int lineW = 0;
        if (more) {
            for (;;) {
                if (lineW >= limit) {
                    break;
                }
                int count = 0;
                lineW += Word(p, &count, width, ctx);
                if (lineW >= limit) {
                    if (lineW != 0) {
                        continue; // the line ends before this word
                    }
                    more = false;
                    break;
                }
                p += count;
                if (*p == 0) {
                    more = false;
                    break;
                }
            }
        }
        ++lines;
        if (!more) {
            break;
        }
    }
    return lines;
}

bool MessageFits(int y, int lines, int lineHeight)
{
    return y + lines * (5 + lineHeight) <= kTextBottom;
}

int AutoDefendTop(int overlayButtons)
{
    return overlayButtons * 20 + 0x106; // the drawing's base (0x100 in the changed code) + 6, Auto Defend's y 0
}

int AutoDefendSourceRow(int r)
{
    // Rows 0..2, row 2 ten more times, rows 3..25, row 25 ten more times, rows 26..27.
    constexpr int a = kAutoDefendRowA, b = kAutoDefendRowB, n = kAutoDefendRepeat;
    if (r <= a) {
        return r;
    }
    if (r <= a + n) {
        return a;
    }
    if (r <= b + n) {
        return r - n;
    }
    if (r <= b + 2 * n) {
        return b;
    }
    return r - 2 * n;
}

void StretchAutoDefend(Px *at, int pitch)
{
    Px drawn[kAutoDefendH][kAutoDefendW];
    for (int y = 0; y < kAutoDefendH; ++y) {
        for (int x = 0; x < kAutoDefendW; ++x) {
            drawn[y][x] = at[y * pitch + x];
        }
    }
    for (int y = 0; y < kAutoDefendTallH; ++y) {
        const Px *from = drawn[AutoDefendSourceRow(y)];
        for (int x = 0; x < kAutoDefendW; ++x) {
            at[y * pitch + x] = from[x];
        }
    }
}

int RequestRows(int count)
{
    return count < kMaxRequests ? count : kMaxRequests;
}

void RequestTable(const unsigned char *first, unsigned char *out)
{
    for (int k = 0; k < kMaxRequests; ++k) {
        unsigned char *r = out + k * kRequestRowSize;
        for (int i = 0; i < kRequestRowSize; ++i) {
            r[i] = first[i];
        }
        const short y1 = static_cast<short>(k * kRequestRowH), y2 = static_cast<short>((k + 1) * kRequestRowH);
        r[2] = static_cast<unsigned char>(y1 & 0xff);
        r[3] = static_cast<unsigned char>((y1 >> 8) & 0xff);
        r[6] = static_cast<unsigned char>(y2 & 0xff);
        r[7] = static_cast<unsigned char>((y2 >> 8) & 0xff);
        for (int i = 0; i < 4; ++i) {
            r[kRequestRowIndex + i] = static_cast<unsigned char>((k >> (8 * i)) & 0xff);
        }
    }
}

bool ExtraRequest(int hover, int *index)
{
    const int k = hover - kFirstRequestHover;
    if (k < kGameRequests || k >= kMaxRequests) {
        return false;
    }
    *index = k;
    return true;
}

bool ReportRequestTip(int screen, int dialog, int hover)
{
    return screen == 7 && dialog == kCityOverviewReport && hover >= kFirstReportRequest &&
           hover <= kLastReportRequest;
}

namespace {

constexpr Picture kPictures[] = {{604, 158, 0}, {610, 146, 1}, {614, 146, 0}};

} // namespace

int FrameTop(const Px *sidebar, int pitch, int height)
{
    for (int y = kFrameSearchTop; y <= kFrameSearchLast && y < height; ++y) {
        if ((sidebar[y * pitch + kFrameColumn] & 0x1f) <= 13) {
            return y;
        }
    }
    return kPictures[0].frameTop;
}

Picture PictureOf(int frameTop)
{
    for (const Picture &p : kPictures) {
        if (p.frameTop == frameTop) {
            return p;
        }
    }
    return kPictures[0];
}

int WorldFrameLeft(const Picture &p)
{
    return p.frameTop == kZeusWorldFrameTop ? kWorldEdgeW : kWorldEdgeW + 1;
}

namespace {

// Back and forth over n rows from row `first`: first .. first + n - 1, then the same back up, and again.
int Mirrored(int first, int n, int i)
{
    const int j = i % (2 * n);
    return first + (j < n ? j : 2 * n - 1 - j);
}

} // namespace

WorldSource WorldSourceOf(int col, int y, const Picture &p, bool stone)
{
    if (col < kWorldEdgeW || col >= kSidebarW || y < kWorldKeepEnd || y >= kLayoutH) {
        return WorldSource{WorldSource::Drawn, -1};
    }
    const int left = WorldFrameLeft(p), frameEnd = kWorldKeepEnd + p.frameH; // the frame's rows end here
    const bool beside = y < frameEnd;
    if (beside && col >= left && col < left + kWorldFrameW) {
        return WorldSource{WorldSource::Own, p.frameTop + (y - kWorldKeepEnd)};
    }
    if (stone && p.frameTop != kZeusWorldFrameTop) {
        return WorldSource{WorldSource::Stone, Mirrored(0, kStoneRows, y - kWorldKeepEnd)};
    }
    const int shadowTop = p.frameTop + p.frameH, shadowRows = kLayoutH - shadowTop;
    if (beside || shadowRows <= 0) {
        return WorldSource{WorldSource::Drawn, -1};
    }
    return WorldSource{WorldSource::Own, Mirrored(shadowTop, shadowRows, y - frameEnd)};
}

int LayOutWorldSidebar(Px *sidebar, int pitch, int height, const Px *stone)
{
    const int top = FrameTop(sidebar, pitch, height);
    const Picture p = PictureOf(top);
    // The rows read (from the frame's top on) overlap the rows written (567..767): copied first.
    constexpr int kFirst = kFrameSearchTop, kRows = kLayoutH - kFirst;
    static Px from[kRows][kSidebarW];
    for (int y = 0; y < kRows; ++y) {
        const Px *s = sidebar + (kFirst + y) * pitch;
        for (int x = 0; x < kSidebarW; ++x) {
            from[y][x] = s[x];
        }
    }
    for (int y = kWorldKeepEnd; y < kLayoutH; ++y) {
        Px *to = sidebar + y * pitch;
        for (int x = 0; x < kSidebarW; ++x) {
            const WorldSource s = WorldSourceOf(x, y, p, stone != nullptr);
            if (s.kind == WorldSource::Own && s.row >= kFirst) {
                to[x] = from[s.row - kFirst][x];
            }
            else if (s.kind == WorldSource::Stone) {
                to[x] = stone[s.row * kSidebarW + x];
            }
        }
    }
    return top;
}

void CallBytes(unsigned long at, unsigned long target, unsigned char out[5])
{
    const unsigned long rel = target - (at + 5);
    out[0] = 0xe8;
    for (int i = 0; i < 4; ++i) {
        out[1 + i] = static_cast<unsigned char>((rel >> (8 * i)) & 0xff);
    }
}

int MessageList::At(int y) const
{
    for (int i = 0; i < n_; ++i) {
        if (y >= s_[i].top && y < s_[i].bottom) {
            return s_[i].index;
        }
    }
    return -1;
}

} // namespace zeus::side_panel_layout
