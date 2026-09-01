/*
 * AtlasPS2 - layout.c
 * Where the content starts and how many rows fit under it.
 *
 * No gsKit and no device access, so `make check` compiles this on the
 * build machine. That is the whole reason it is a file of its own: this
 * arithmetic decides whether a list is drawn over the footer, it has
 * been wrong twice, and both times the only way to see it was to put a
 * television in front of a console. The screen dimensions and the font
 * metric come in as arguments rather than being read here, which is
 * what lets a check hand it PAL's field and NTSC's and compare.
 *
 * ui.c holds the thin wrappers that fetch the live values.
 */
#include "atlas/layout.h"

float atlas_ui_layout_content_y(void)
{
    return (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
}

float atlas_ui_layout_content_y_titled(float line_height)
{
    return atlas_ui_layout_content_y() + line_height * ATLAS_UI_TITLE_H;
}

int atlas_ui_layout_rows_fit(float top, float reserve, float safe_h)
{
    float bottom = safe_h - (float)ATLAS_UI_FOOTER_H - (float)ATLAS_UI_PAD
                 - reserve;
    int   n      = (int)((bottom - top)
                         / (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP));

    return n < 1 ? 1 : n;
}
