/*
 * AtlasPS2 - test_fav.c
 *
 * Favorites and the recently-used list.
 *
 * Order is the entire behaviour of a recently-used list, and a list
 * that reorders wrongly is not something a user can see is wrong - it
 * just quietly shows the second-most-recent thing first. So most of
 * what follows pins order, and the rest pins the two promises the
 * screens rely on: that a full list refuses rather than evicting, and
 * that nothing marks itself dirty without an actual change.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/fav.h"

/* ------------------------------------------------------------------ */

static void test_starts_empty(void)
{
    atlas_fav_reset();

    assert(atlas_fav_count() == 0);
    assert(atlas_recent_count() == 0);
    assert(atlas_fav_dirty() == 0);

    /* Out of range in both directions, and on an empty list. */
    assert(atlas_fav_get(0) == NULL);
    assert(atlas_fav_get(-1) == NULL);
    assert(atlas_recent_get(0) == NULL);
    assert(atlas_recent_get(-1) == NULL);

    assert(atlas_fav_is("mc0:/ATLAS/APPS/A.ELF") == 0);
}

static void test_toggle_is_a_toggle(void)
{
    const char *p = "mc0:/ATLAS/APPS/OPL.ELF";

    atlas_fav_reset();

    assert(atlas_fav_toggle(p) == 1);
    assert(atlas_fav_is(p) == 1);
    assert(atlas_fav_count() == 1);

    assert(atlas_fav_toggle(p) == 0);
    assert(atlas_fav_is(p) == 0);
    assert(atlas_fav_count() == 0);

    /* And back again: removing must not leave the slot poisoned. */
    assert(atlas_fav_toggle(p) == 1);
    assert(atlas_fav_count() == 1);
}

static void test_favorites_keep_their_order(void)
{
    atlas_fav_reset();

    atlas_fav_toggle("mc0:/A.ELF");
    atlas_fav_toggle("mc0:/B.ELF");
    atlas_fav_toggle("mc0:/C.ELF");

    /*
     * Removing from the middle must close the gap without disturbing
     * anything else. The screen draws these in array order, so a
     * reshuffle here is a list that rearranges itself when the user
     * unstars something unrelated.
     */
    atlas_fav_toggle("mc0:/B.ELF");

    assert(atlas_fav_count() == 2);
    assert(strcmp(atlas_fav_get(0), "mc0:/A.ELF") == 0);
    assert(strcmp(atlas_fav_get(1), "mc0:/C.ELF") == 0);
    assert(atlas_fav_get(2) == NULL);
}

static void test_favorites_are_bounded(void)
{
    char path[64];
    int i;

    atlas_fav_reset();

    for (i = 0; i < ATLAS_FAV_MAX; i++) {
        snprintf(path, sizeof(path), "mc0:/APP%03d.ELF", i);
        assert(atlas_fav_toggle(path) == 1);
    }

    assert(atlas_fav_count() == ATLAS_FAV_MAX);

    /*
     * One more must fail, and must fail by refusing - not by evicting
     * a favorite the user never asked to lose. The first entry is
     * still the first entry.
     */
    assert(atlas_fav_toggle("mc0:/ONE_TOO_MANY.ELF") == 0);
    assert(atlas_fav_count() == ATLAS_FAV_MAX);
    assert(atlas_fav_is("mc0:/ONE_TOO_MANY.ELF") == 0);
    assert(strcmp(atlas_fav_get(0), "mc0:/APP000.ELF") == 0);
}

static void test_paths_that_do_not_fit_are_refused(void)
{
    char path[ATLAS_APP_PATH_MAX + 32];
    size_t i;

    atlas_fav_reset();

    for (i = 0; i < sizeof(path) - 1; i++)
        path[i] = 'x';
    path[sizeof(path) - 1] = '\0';

    /*
     * Not truncated. A shortened path names a different file, or none,
     * and this string is later handed to the ELF loader.
     */
    assert(atlas_fav_toggle(path) == 0);
    assert(atlas_fav_count() == 0);

    /* And the degenerate arguments the screens can produce. */
    assert(atlas_fav_toggle(NULL) == 0);
    assert(atlas_fav_toggle("") == 0);
    assert(atlas_fav_is(NULL) == 0);
    assert(atlas_fav_is("") == 0);
    assert(atlas_fav_count() == 0);
}

/* ------------------------------------------------------------------ */

static void test_recent_is_most_recent_first(void)
{
    atlas_fav_reset();

    atlas_recent_note("mc0:/A.ELF");
    atlas_recent_note("mc0:/B.ELF");
    atlas_recent_note("mc0:/C.ELF");

    assert(atlas_recent_count() == 3);
    assert(strcmp(atlas_recent_get(0), "mc0:/C.ELF") == 0);
    assert(strcmp(atlas_recent_get(1), "mc0:/B.ELF") == 0);
    assert(strcmp(atlas_recent_get(2), "mc0:/A.ELF") == 0);
}

static void test_relaunching_moves_to_the_front(void)
{
    atlas_fav_reset();

    atlas_recent_note("mc0:/A.ELF");
    atlas_recent_note("mc0:/B.ELF");
    atlas_recent_note("mc0:/C.ELF");

    /* A is launched again: it moves, it is not duplicated. */
    atlas_recent_note("mc0:/A.ELF");

    assert(atlas_recent_count() == 3);
    assert(strcmp(atlas_recent_get(0), "mc0:/A.ELF") == 0);
    assert(strcmp(atlas_recent_get(1), "mc0:/C.ELF") == 0);
    assert(strcmp(atlas_recent_get(2), "mc0:/B.ELF") == 0);
}

static void test_relaunching_the_top_entry_costs_nothing(void)
{
    atlas_fav_reset();

    atlas_recent_note("mc0:/A.ELF");
    atlas_fav_reset();          /* clears the dirty flag too */
    atlas_recent_note("mc0:/A.ELF");
    assert(atlas_fav_dirty() == 1);

    /*
     * The common case: the same program launched twice in a row. The
     * list is already exactly what it would be written as, so a second
     * note must not mark it dirty and spend a Memory Card write on
     * storing what is already stored.
     */
    atlas_fav_parse("[recent]\npath=mc0:/A.ELF\n", 25);
    assert(atlas_fav_dirty() == 0);
    atlas_recent_note("mc0:/A.ELF");
    assert(atlas_fav_dirty() == 0);
    assert(atlas_recent_count() == 1);
}

static void test_recent_drops_the_oldest(void)
{
    char path[64];
    int i;

    atlas_fav_reset();

    for (i = 0; i < ATLAS_RECENT_MAX + 3; i++) {
        snprintf(path, sizeof(path), "mc0:/APP%03d.ELF", i);
        atlas_recent_note(path);
    }

    /*
     * Unlike favorites, this list evicts - and it must, because it is
     * a record of what happened rather than a choice the user made.
     * The oldest three are gone and the newest is first.
     */
    assert(atlas_recent_count() == ATLAS_RECENT_MAX);
    assert(strcmp(atlas_recent_get(0), "mc0:/APP010.ELF") == 0);
    assert(strcmp(atlas_recent_get(ATLAS_RECENT_MAX - 1),
                  "mc0:/APP003.ELF") == 0);
    assert(atlas_recent_get(ATLAS_RECENT_MAX) == NULL);
}

static void test_recent_refuses_bad_paths(void)
{
    char path[ATLAS_APP_PATH_MAX + 32];
    size_t i;

    atlas_fav_reset();

    for (i = 0; i < sizeof(path) - 1; i++)
        path[i] = 'x';
    path[sizeof(path) - 1] = '\0';

    atlas_recent_note(path);
    atlas_recent_note(NULL);
    atlas_recent_note("");

    assert(atlas_recent_count() == 0);
    assert(atlas_fav_dirty() == 0);
}

/* ------------------------------------------------------------------ */

static void test_dirty_tracks_real_changes(void)
{
    atlas_fav_reset();
    assert(atlas_fav_dirty() == 0);

    /* Merely looking at the list is not a change. */
    assert(atlas_fav_is("mc0:/A.ELF") == 0);
    atlas_fav_count();
    atlas_recent_count();
    assert(atlas_fav_dirty() == 0);

    /* A refused add is not a change either: nothing was stored. */
    atlas_fav_toggle("");
    assert(atlas_fav_dirty() == 0);

    atlas_fav_toggle("mc0:/A.ELF");
    assert(atlas_fav_dirty() == 1);
}

/* ------------------------------------------------------------------ */

static void test_round_trip(void)
{
    char text[ATLAS_FAV_FILE_MAX];
    int len;

    atlas_fav_reset();
    atlas_fav_toggle("mc0:/ATLAS/APPS/OPL.ELF");
    atlas_fav_toggle("mass:/APPS/uLaunchELF.ELF");
    atlas_recent_note("mc0:/ATLAS/APPS/OPL.ELF");
    atlas_recent_note("mass:/APPS/wLaunchELF.ELF");

    len = atlas_fav_format(text, (int)sizeof(text));
    assert(len > 0);

    /* Reading back what was written must give the same two lists in
     * the same order - that is the only property the file has. */
    assert(atlas_fav_parse(text, len) == ATLAS_OK);

    assert(atlas_fav_count() == 2);
    assert(strcmp(atlas_fav_get(0), "mc0:/ATLAS/APPS/OPL.ELF") == 0);
    assert(strcmp(atlas_fav_get(1), "mass:/APPS/uLaunchELF.ELF") == 0);

    assert(atlas_recent_count() == 2);
    assert(strcmp(atlas_recent_get(0), "mass:/APPS/wLaunchELF.ELF") == 0);
    assert(strcmp(atlas_recent_get(1), "mc0:/ATLAS/APPS/OPL.ELF") == 0);

    /* A load is not a change. */
    assert(atlas_fav_dirty() == 0);
}

static void test_format_refuses_a_short_buffer(void)
{
    char small[16];

    atlas_fav_reset();
    atlas_fav_toggle("mc0:/A.ELF");

    /*
     * -1, not a half-written file. A truncated favorites file whose
     * last line is a partial path would name a file that does not
     * exist, and it would be written over the good one.
     */
    assert(atlas_fav_format(small, (int)sizeof(small)) == -1);
    assert(atlas_fav_format(NULL, 64) == -1);
    assert(atlas_fav_format(small, 0) == -1);
}

static void test_file_tolerance(void)
{
    static const char text[] =
        "# a comment\r\n"
        "\r\n"
        "  [favorites]  \r\n"
        "  path = mc0:/A.ELF  \r\n"
        "path=mc0:/A.ELF\n"           /* a duplicate is not two stars */
        "nonsense-with-no-equals\n"   /* skipped, file continues */
        "colour=blue\n"               /* unknown key, ignored */
        "[future]\n"                  /* unknown section, ignored */
        "path=mc0:/IGNORED.ELF\n"
        "[recent]\n"
        "path=mc0:/B.ELF\n";

    atlas_fav_reset();
    assert(atlas_fav_parse(text, (int)strlen(text)) == ATLAS_OK);

    assert(atlas_fav_count() == 1);
    assert(strcmp(atlas_fav_get(0), "mc0:/A.ELF") == 0);
    assert(atlas_fav_is("mc0:/IGNORED.ELF") == 0);

    assert(atlas_recent_count() == 1);
    assert(strcmp(atlas_recent_get(0), "mc0:/B.ELF") == 0);
}

static void test_a_file_longer_than_the_lists_is_clipped(void)
{
    char text[ATLAS_FAV_FILE_MAX];
    char line[64];
    int n = 0, i;

    n += snprintf(text + n, sizeof(text) - (size_t)n, "[favorites]\n");

    for (i = 0; i < ATLAS_FAV_MAX + 10; i++) {
        snprintf(line, sizeof(line), "path=mc0:/APP%03d.ELF\n", i);
        n += snprintf(text + n, sizeof(text) - (size_t)n, "%s", line);
    }

    atlas_fav_reset();
    assert(atlas_fav_parse(text, n) == ATLAS_OK);

    /*
     * A file with more entries than the list holds keeps the first
     * ones and drops the rest, rather than overrunning. Which ones
     * survive matters less than that the program does.
     */
    assert(atlas_fav_count() == ATLAS_FAV_MAX);
    assert(strcmp(atlas_fav_get(0), "mc0:/APP000.ELF") == 0);
}

static void test_parse_of_nothing(void)
{
    atlas_fav_reset();
    atlas_fav_toggle("mc0:/A.ELF");

    /* An empty or absent file empties the lists rather than leaving
     * whatever was there before. */
    assert(atlas_fav_parse(NULL, 0) == ATLAS_EINVAL);
    assert(atlas_fav_count() == 0);

    atlas_fav_toggle("mc0:/A.ELF");
    assert(atlas_fav_parse("", 0) == ATLAS_EINVAL);
    assert(atlas_fav_count() == 0);
}

static void test_set_reports_what_it_knows(void)
{
    atlas_fav_reset();

    assert(atlas_fav_set("favorites", "path", "mc0:/A.ELF") == 1);
    assert(atlas_fav_set("recent", "path", "mc0:/B.ELF") == 1);

    assert(atlas_fav_set("favorites", "colour", "blue") == 0);
    assert(atlas_fav_set("future", "path", "mc0:/C.ELF") == 0);
    assert(atlas_fav_set(NULL, "path", "mc0:/C.ELF") == 0);
    assert(atlas_fav_set("favorites", NULL, "mc0:/C.ELF") == 0);
    assert(atlas_fav_set("favorites", "path", NULL) == 0);

    assert(atlas_fav_count() == 1);
    assert(atlas_recent_count() == 1);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    test_starts_empty();
    test_toggle_is_a_toggle();
    test_favorites_keep_their_order();
    test_favorites_are_bounded();
    test_paths_that_do_not_fit_are_refused();

    test_recent_is_most_recent_first();
    test_relaunching_moves_to_the_front();
    test_relaunching_the_top_entry_costs_nothing();
    test_recent_drops_the_oldest();
    test_recent_refuses_bad_paths();

    test_dirty_tracks_real_changes();

    test_round_trip();
    test_format_refuses_a_short_buffer();
    test_file_tolerance();
    test_a_file_longer_than_the_lists_is_clipped();
    test_parse_of_nothing();
    test_set_reports_what_it_knows();

    printf("test_fav: all checks passed (%d favorites, %d recent)\n",
           ATLAS_FAV_MAX, ATLAS_RECENT_MAX);
    return 0;
}
