/*
 * AtlasPS2 - layout.h
 *
 * The list-geometry arithmetic, taking its inputs as arguments so it can
 * be checked on the build machine.
 *
 * Screens do not call these: they call the atlas_ui_* wrappers in ui.h,
 * which supply the live field height and font metric. This header exists
 * so `make check` can ask the same functions what happens on a PAL field
 * and on an NTSC one without a console in the room.
 */
#ifndef ATLAS_LAYOUT_H
#define ATLAS_LAYOUT_H

#include "atlas/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The y just below the header bar. */
float atlas_ui_layout_content_y(void);

/** The y below a screen's own title, given the UI font's line height. */
float atlas_ui_layout_content_y_titled(float line_height);

/**
 * How many ATLAS_UI_ROW_H rows fit between `top` and the footer of a
 * safe area `safe_h` tall, keeping `reserve` free below the list.
 *
 * @return at least 1, so a caller can always draw the cursor's row.
 */
int atlas_ui_layout_rows_fit(float top, float reserve, float safe_h);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_LAYOUT_H */
