/*
 * AtlasPS2 - test_layout.c
 *
 * The list geometry, checked against the two fields the program
 * actually runs on.
 *
 * This check exists because the bug it covers shipped twice. Each list
 * screen used to carry a hand-counted row constant - eight here, nine
 * there - chosen against one video mode and never revisited when the
 * row height grew or PAL gave the screen sixty more lines. A list that
 * asks for more rows than fit does not fail: it draws the extra ones
 * over the footer and off the bottom of the screen, which looks like a
 * broken interface rather than like a number being wrong. Nobody could
 * see it without a television.
 *
 * So the property under test is not "the answer is eight". It is that
 * the last row drawn always ends above the footer, on every field, for
 * every reserve any screen asks for.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/layout.h"

static int s_fail;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            s_fail++;                                                     \
        }                                                                 \
    } while (0)

/*
 * The safe area is 88% of the field, six per cent inset top and bottom
 * (see ATLAS_SAFE_INSET_Y_PCT in video.c). These are what that comes to
 * for the two modes AtlasPS2 sets up.
 */
#define SAFE_H_NTSC 396.0f   /* 448 lines, less 6% each end */
#define SAFE_H_PAL  460.0f   /* 512 lines, likewise         */

/* The UI font's line height at the size the atlases are baked at. */
#define LINE_H      24.0f

/* Every reserve any screen passes, largest last. */
static const float k_reserves[] = {
    2.6f * LINE_H,   /* apps, games: path + position counter   */
    3.0f * LINE_H,   /* settings: description + status         */
    3.2f * LINE_H,   /* files: clipboard + truncation warning  */
    3.4f * LINE_H    /* theme: description + counter + note    */
};

#define RESERVE_COUNT ((int)(sizeof(k_reserves) / sizeof(k_reserves[0])))

/**
 * The invariant: the bottom of the last row stays above the footer.
 *
 * This is the whole point of the module. If it holds for both fields
 * and every reserve, no screen can draw over its own footer.
 */
static void check_fits(float safe_h, float top, float reserve,
                       const char *what)
{
    int   n      = atlas_ui_layout_rows_fit(top, reserve, safe_h);
    float used   = top + (float)n * (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    float footer = safe_h - (float)ATLAS_UI_FOOTER_H;

    if (used + reserve > footer) {
        printf("  FAIL %s: %d rows end at %.0f, footer at %.0f "
               "(reserve %.0f)\n", what, n, used + reserve, footer, reserve);
        s_fail++;
    }

    /* A screen must always be able to draw the row the cursor is on,
     * however cramped the field: rows_fit promises at least one. */
    CHECK(n >= 1);
}

static void test_fits_both_fields(void)
{
    int i;

    printf("layout: rows never overflow the footer\n");

    for (i = 0; i < RESERVE_COUNT; i++) {
        check_fits(SAFE_H_NTSC, atlas_ui_layout_content_y(),
                   k_reserves[i], "NTSC, no title");
        check_fits(SAFE_H_PAL, atlas_ui_layout_content_y(),
                   k_reserves[i], "PAL, no title");

        check_fits(SAFE_H_NTSC, atlas_ui_layout_content_y_titled(LINE_H),
                   k_reserves[i], "NTSC, titled");
        check_fits(SAFE_H_PAL, atlas_ui_layout_content_y_titled(LINE_H),
                   k_reserves[i], "PAL, titled");
    }
}

/*
 * A titled screen starts lower, so it can never fit MORE rows than the
 * same screen without a title. This is the specific mistake the `top`
 * argument was added to prevent: rows_fit once assumed every list began
 * just under the header, which handed the file manager and the theme
 * picker one row more than they had room for.
 */
static void test_title_costs_rows(void)
{
    float plain  = atlas_ui_layout_content_y();
    float titled = atlas_ui_layout_content_y_titled(LINE_H);
    int   i;

    printf("layout: a title never buys rows\n");

    CHECK(titled > plain);

    for (i = 0; i < RESERVE_COUNT; i++) {
        CHECK(atlas_ui_layout_rows_fit(titled, k_reserves[i], SAFE_H_NTSC)
              <= atlas_ui_layout_rows_fit(plain, k_reserves[i], SAFE_H_NTSC));
        CHECK(atlas_ui_layout_rows_fit(titled, k_reserves[i], SAFE_H_PAL)
              <= atlas_ui_layout_rows_fit(plain, k_reserves[i], SAFE_H_PAL));
    }
}

/*
 * PAL has sixty-four more lines than NTSC and must use them. The old
 * constants were decided on an NTSC field and applied to both, so PAL
 * users saw the same short list with a band of empty screen under it.
 */
static void test_pal_shows_more(void)
{
    float top = atlas_ui_layout_content_y();

    printf("layout: PAL uses its extra lines\n");

    CHECK(atlas_ui_layout_rows_fit(top, 0.0f, SAFE_H_PAL)
          > atlas_ui_layout_rows_fit(top, 0.0f, SAFE_H_NTSC));
}

/*
 * A larger reserve asks for more room below the list, so it can only
 * ever cost rows - never add them.
 */
static void test_reserve_is_monotonic(void)
{
    float top = atlas_ui_layout_content_y();
    int   i;

    printf("layout: a bigger reserve never adds rows\n");

    for (i = 1; i < RESERVE_COUNT; i++)
        CHECK(atlas_ui_layout_rows_fit(top, k_reserves[i], SAFE_H_NTSC)
              <= atlas_ui_layout_rows_fit(top, k_reserves[i - 1],
                                          SAFE_H_NTSC));
}

/*
 * A field too small to hold anything still yields one row rather than
 * zero or a negative count. A screen that got zero would draw no rows
 * at all, including the one the cursor is on, and look frozen.
 */
static void test_degenerate_field(void)
{
    printf("layout: an impossible field still gives one row\n");

    CHECK(atlas_ui_layout_rows_fit(atlas_ui_layout_content_y(), 0.0f, 0.0f)
          == 1);
    CHECK(atlas_ui_layout_rows_fit(atlas_ui_layout_content_y(),
                                   10000.0f, SAFE_H_NTSC) == 1);
}

int main(void)
{
    test_fits_both_fields();
    test_title_costs_rows();
    test_pal_shows_more();
    test_reserve_is_monotonic();
    test_degenerate_field();

    if (s_fail) {
        printf("test_layout: %d failure(s)\n", s_fail);
        return 1;
    }

    printf("test_layout: OK\n");
    return 0;
}
