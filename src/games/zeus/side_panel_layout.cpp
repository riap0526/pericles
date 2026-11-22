#include "games/zeus/side_panel_layout.h"

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

void MoveColumn(Px *screen, int pitch, const Px *backdrop, const Px *top)
{
    // Top down: each row is read (kShift rows below) before anything is written over it.
    for (int dst = 0; dst < kMovedRows; ++dst) {
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
    for (int y = kMovedRows; y < kColumnRows; ++y) {
        const Px *b = backdrop + y * kColumnW;
        Px *to = screen + y * pitch;
        for (int x = 0; x < kColumnW; ++x) {
            to[x] = b[x];
        }
    }
}

bool DrawnBelow(const Px *screen, int pitch, const Px *backdrop)
{
    for (int y = kMovedEnd; y < kColumnRows; ++y) {
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
    for (int y = 0; y < kColumnRows; ++y) {
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

bool OnToggle(int col, int y)
{
    return col >= kToggleX && col < kToggleX + kToggleSize && y >= kToggleY && y < kToggleY + kToggleSize;
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
