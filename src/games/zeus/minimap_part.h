// The map's own part of what minimap_draw 0x454980 (Zeus.exe 2.1.4.0) drew, without reading the game: the caller
// passes the drawing's origin and the map's range. Used by the minimap picture (games/zeus/city_picture: only the map
// is kept) and by the side panel's box (games/zeus/side_panel: the inset put back around a map smaller than the box).
//
// minimap_draw paints every cell of the view table: outside the map's range (map_col_min .. max, map_row_min ..
// (max - 1) & ~1) and on cells with no tile a filler picture (seen in testing, and in the decompiled non-editor loop
// of 0x454980). Its loops start 4 columns and 4 half rows before the origin (minimap_center's 0x7d3b34 / 30): the
// columns before it move x by 2 while they are off the table (below column 0), the rows before it move y by 1 while
// they are on it, and y starts at y0 - 4. Each cell is 2 px, odd rows (counted from the first loop row) 1 px further
// left.
#pragma once

#include <windows.h>

namespace zeus::minimap_part {

struct MapRange {
    int colMin, colMax, rowMin, rowMax; // map_col_min 0xfa05b4, map_col_max 0xfa05b0, map_row_min 0xfa05ac, max 0xfa05a8
};

// The map's rectangle (right and bottom exclusive) for a drawing at (x0, y0) whose origin was (originCol, originRow).
inline RECT MapPart(int x0, int y0, int originCol, int originRow, const MapRange &m)
{
    const int xLead = originCol < 4 ? 2 * (4 - originCol) : 0;
    const int yLead = originRow > 4 ? originRow - 4 : 0;
    const int rowLast = (m.rowMax - 1) & ~1;
    RECT a;
    a.left = x0 + xLead + 2 * (m.colMin - originCol) - 1;
    a.right = x0 + xLead + 2 * (m.colMax - originCol) + 2; // the last cell's two pixels
    a.top = y0 - 4 + m.rowMin - yLead;
    a.bottom = y0 - 4 + rowLast - yLead + 1;
    return a;
}

// minimap_center 0x4547c0 (w columns, h half rows): the origin minimap_draw uses, from the view and the minimap's
// bounds (set by 0x449ff0; 0x449f30 resets them). Columns: centred on the view, kept within [colLo, colHi - w];
// half rows likewise within [rowLo, rowHi - h], then made even. A map narrower or lower than w or h: the view table's
// middle instead.
struct CenterInputs {
    int colLo, colHi; // 0x7d3b84, 0x7d3b7c
    int rowLo, rowHi; // 0x7d3b80, 0x7d3b78
    int viewCols, viewRows; // viewport_cols 0x7d3bbc, viewport_rows 0x7d3bb8
    int viewX, viewY;       // view_drawn_x 0x7d3bd0, view_drawn_y 0x7d3bcc
};

inline int CenterCol(const CenterInputs &c, int w)
{
    if ((c.colHi - c.colLo) - w < 0) {
        return (0xe7 - w) / 2;
    }
    const int o = (c.viewCols / 2 - w / 2) + c.viewX;
    if (o < c.colLo) {
        return c.colLo;
    }
    if (c.colHi - w < o) {
        return c.colHi - w;
    }
    return o;
}

inline int CenterRow(const CenterInputs &c, int h)
{
    if ((c.rowHi - c.rowLo) - h < 0) {
        return ((0x1c9 - h) / 2) & 0xfffe;
    }
    const int o = (c.viewRows / 2 - h / 2) + c.viewY;
    if (o < c.rowLo) {
        return c.rowLo & 0xfffe;
    }
    if (c.rowHi - h < o) {
        return (c.rowHi - h) & 0xfffe;
    }
    return o & 0xfffe;
}

} // namespace zeus::minimap_part
