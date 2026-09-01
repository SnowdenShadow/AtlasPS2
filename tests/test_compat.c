/*
 * AtlasPS2 - test_compat.c
 *
 * The per-game settings, and the video mode they decide.
 *
 * Both halves are invisible until a television is in front of you. A
 * workaround that silently failed to parse looks exactly like a game
 * that needed a different workaround, and a video mode chosen wrongly
 * looks exactly like a bad dump - the game runs a fifth too fast with
 * the bottom of the picture missing, and nothing on screen says why.
 *
 * So the checks below are mostly about the ways a hand-edited file goes
 * wrong: a boolean written four different ways, a section header that
 * is not a game ID, a key from some other launcher's format, a value
 * that is simply a typo. Each must either work or be skipped in a way
 * that leaves the rest of the file standing.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/compat.h"

static void parse(const char *text, int expect_count)
{
    int n = -1;

    assert(atlas_compat_parse(text, (int)strlen(text), &n) == ATLAS_OK);
    assert(n == expect_count);
    assert(atlas_compat_count() == expect_count);
}

/* ------------------------------------------------------------------ */

static void check_basic(void)
{
    const atlas_compat_t *e;

    parse("[SLUS-20902]\n"
          "force_dvd = 1\n"
          "vmode = pal\n"
          "\n"
          "[SLES-50490]\n"
          "hide_tray = yes\n"
          "slow_first_read = true\n", 2);

    e = atlas_compat_find("SLUS-20902");
    assert(e != NULL);
    assert(e->flags == ATLAS_COMPAT_FORCE_DVD);
    assert(e->vmode == ATLAS_VMODE_PAL);

    e = atlas_compat_find("SLES-50490");
    assert(e != NULL);
    assert(e->flags == (ATLAS_COMPAT_HIDE_TRAY
                        | ATLAS_COMPAT_SLOW_FIRST_READ));
    assert(e->vmode == ATLAS_VMODE_AUTO);

    /* A game with no entry is the normal case, and means "defaults",
     * not "cannot launch". */
    assert(atlas_compat_find("SCUS-97472") == NULL);
    assert(atlas_compat_find("") == NULL);
    assert(atlas_compat_find(NULL) == NULL);
}

static void check_boolean_spellings(void)
{
    const atlas_compat_t *e;

    /*
     * Four spellings, all of which a person editing this on a PC will
     * write. A parser accepting only "1" fails silently on the other
     * three: the setting is not applied, the game still hangs, and
     * nothing distinguishes that from a game needing something else.
     */
    parse("[SLUS-20001]\nforce_dvd=1\n"
          "[SLUS-20002]\nforce_dvd=yes\n"
          "[SLUS-20003]\nforce_dvd=true\n"
          "[SLUS-20004]\nforce_dvd=on\n", 4);

    e = atlas_compat_find("SLUS-20001"); assert(e->flags);
    e = atlas_compat_find("SLUS-20002"); assert(e->flags);
    e = atlas_compat_find("SLUS-20003"); assert(e->flags);
    e = atlas_compat_find("SLUS-20004"); assert(e->flags);

    /* And the negatives, including "off" - which shares its first
     * letter with "on" and is the one a sloppy parser gets backwards. */
    parse("[SLUS-20001]\nforce_dvd=0\n"
          "[SLUS-20002]\nforce_dvd=no\n"
          "[SLUS-20003]\nforce_dvd=false\n"
          "[SLUS-20004]\nforce_dvd=off\n", 4);

    e = atlas_compat_find("SLUS-20001"); assert(e->flags == 0);
    e = atlas_compat_find("SLUS-20002"); assert(e->flags == 0);
    e = atlas_compat_find("SLUS-20003"); assert(e->flags == 0);
    e = atlas_compat_find("SLUS-20004"); assert(e->flags == 0);

    /* Case is not meaningful here, and the INI reader hands values back
     * exactly as typed - so the folding has to happen at this end. */
    parse("[SLUS-20001]\nforce_dvd = YES\n"
          "[SLUS-20002]\nforce_dvd = True\n"
          "[SLUS-20003]\nforce_dvd = OFF\nhide_tray = On\n", 3);

    e = atlas_compat_find("SLUS-20001"); assert(e->flags);
    e = atlas_compat_find("SLUS-20002"); assert(e->flags);
    e = atlas_compat_find("SLUS-20003");
    assert(e->flags == ATLAS_COMPAT_HIDE_TRAY);

    /*
     * The near misses, which are the reason these are matched as whole
     * words. Each shares a first letter with a real spelling, and a
     * parser keying on that letter takes "nearly" for no and "onward"
     * for on - applying a setting the user never asked for, or dropping
     * one they did, with nothing on screen either way.
     */
    parse("[SLUS-20001]\nforce_dvd = 1\nforce_dvd = nearly\n"
          "[SLUS-20002]\nforce_dvd = onward\n"
          "[SLUS-20003]\nforce_dvd = t\n"
          "[SLUS-20004]\nforce_dvd = 10\n", 4);

    /* The bad value left the good line before it standing. */
    e = atlas_compat_find("SLUS-20001");
    assert(e->flags == ATLAS_COMPAT_FORCE_DVD);

    e = atlas_compat_find("SLUS-20002"); assert(e->flags == 0);
    e = atlas_compat_find("SLUS-20003"); assert(e->flags == 0);
    e = atlas_compat_find("SLUS-20004"); assert(e->flags == 0);

    /* An empty value is not a boolean either, and must not read as
     * false - a line the user half-wrote should be reported, not
     * quietly turn the setting off. */
    parse("[SLUS-20001]\nforce_dvd = 1\nforce_dvd =\n", 1);
    assert(atlas_compat_find("SLUS-20001")->flags == ATLAS_COMPAT_FORCE_DVD);
}

static void check_all_flags(void)
{
    const atlas_compat_t *e;

    parse("[SLUS-20902]\n"
          "force_dvd = 1\n"
          "hide_tray = 1\n"
          "slow_first_read = 1\n"
          "low_modules = 1\n"
          "no_disc_patch = 1\n", 1);

    e = atlas_compat_find("SLUS-20902");
    assert(e->flags == (ATLAS_COMPAT_FORCE_DVD
                        | ATLAS_COMPAT_HIDE_TRAY
                        | ATLAS_COMPAT_SLOW_FIRST_READ
                        | ATLAS_COMPAT_LOW_MODULES
                        | ATLAS_COMPAT_NO_DISC_PATCH));

    /* Turning one off must leave the others alone: a user adding a
     * line to an entry that already worked must not lose the rest. */
    parse("[SLUS-20902]\n"
          "force_dvd = 1\n"
          "hide_tray = 1\n"
          "force_dvd = 0\n", 1);

    e = atlas_compat_find("SLUS-20902");
    assert(e->flags == ATLAS_COMPAT_HIDE_TRAY);
}

static void check_malformed_survives(void)
{
    const atlas_compat_t *e;

    /*
     * A file with one bad line in it must keep every good line. A list
     * shared between launchers carries keys this does not implement,
     * and dropping the file over one of them loses the entries that
     * would have worked.
     */
    parse("[SLUS-20001]\n"
          "force_dvd = 1\n"
          "some_other_launchers_key = 42\n"
          "hide_tray = perhaps\n"          /* not a boolean            */
          "vmode = purple\n"               /* not a video mode         */
          "\n"
          "[NOT-A-GAME-ID]\n"              /* skipped whole            */
          "force_dvd = 1\n"
          "\n"
          "[SLUS-20002]\n"
          "low_modules = 1\n", 2);

    e = atlas_compat_find("SLUS-20001");
    assert(e != NULL);
    assert(e->flags == ATLAS_COMPAT_FORCE_DVD);   /* the good line held */
    assert(e->vmode == ATLAS_VMODE_AUTO);         /* the bad one did not */

    /* The entry after the bad section still loaded. */
    e = atlas_compat_find("SLUS-20002");
    assert(e != NULL);
    assert(e->flags == ATLAS_COMPAT_LOW_MODULES);

    assert(atlas_compat_find("NOT-A-GAME-ID") == NULL);
}

static void check_id_forms(void)
{
    /*
     * The section header goes through the same normaliser the disc
     * does, so a user who wrote the ID the way it appears on the disc -
     * with an underscore and a dot - gets the entry they meant rather
     * than one that never matches.
     */
    parse("[SLUS_209.02]\nforce_dvd = 1\n", 1);
    assert(atlas_compat_find("SLUS-20902") != NULL);

    parse("[slus-20902]\nforce_dvd = 1\n", 1);
    assert(atlas_compat_find("SLUS-20902") != NULL);

    /* The same game twice merges rather than duplicating: a lookup
     * returns the first match, so a second entry would be dead text
     * the user could edit forever with no effect. */
    parse("[SLUS-20902]\nforce_dvd = 1\n"
          "[SLUS-20902]\nhide_tray = 1\n", 1);
    {
        const atlas_compat_t *e = atlas_compat_find("SLUS-20902");
        assert(e->flags == (ATLAS_COMPAT_FORCE_DVD | ATLAS_COMPAT_HIDE_TRAY));
    }

    /* Two spellings of the same game are the same game, and must merge
     * for the same reason: whichever the user edits has to be the one
     * that takes effect. */
    parse("[SLUS_209.02]\nforce_dvd = 1\n"
          "[slus-20902]\nhide_tray = 1\n", 1);
    {
        const atlas_compat_t *e = atlas_compat_find("SLUS-20902");
        assert(e->flags == (ATLAS_COMPAT_FORCE_DVD | ATLAS_COMPAT_HIDE_TRAY));
    }

    /* The stored ID is the normalised one, so what the browser shows
     * and what the file says line up. */
    parse("[slus_209.02]\nforce_dvd = 1\n", 1);
    assert(strcmp(atlas_compat_find("SLUS-20902")->id, "SLUS-20902") == 0);

    /* A lookup is on the normalised form only. The caller has a game ID
     * from the disc, which has already been through the same function;
     * accepting raw forms here would be a second normaliser to keep in
     * step with the first. */
    assert(atlas_compat_find("SLUS_209.02") == NULL);
}

static void check_vmode_spellings(void)
{
    /* Case-insensitive, whole words. "purple" is checked with the other
     * malformed values; here it is the ones that must work. */
    parse("[SLUS-20001]\nvmode = PAL\n"
          "[SLUS-20002]\nvmode = ntsc\n"
          "[SLUS-20003]\nvmode = Auto\n"
          "[SLUS-20004]\nvmode = pal\nvmode = ntsc\n", 4);

    assert(atlas_compat_find("SLUS-20001")->vmode == ATLAS_VMODE_PAL);
    assert(atlas_compat_find("SLUS-20002")->vmode == ATLAS_VMODE_NTSC);
    assert(atlas_compat_find("SLUS-20003")->vmode == ATLAS_VMODE_AUTO);

    /* The last line wins, so a user correcting the file above an old
     * line does not have to find and delete it. */
    assert(atlas_compat_find("SLUS-20004")->vmode == ATLAS_VMODE_NTSC);

    /* A near miss is not a mode. "p" is not "pal", and a first-letter
     * parser would take it. */
    parse("[SLUS-20001]\nvmode = pal\nvmode = p\n"
          "[SLUS-20002]\nvmode = never\n", 2);

    assert(atlas_compat_find("SLUS-20001")->vmode == ATLAS_VMODE_PAL);
    assert(atlas_compat_find("SLUS-20002")->vmode == ATLAS_VMODE_AUTO);
}

static void check_empty_and_limits(void)
{
    char big[16384];
    int i, n;

    /* An empty file, and a file of nothing but comments, are both a
     * valid "no game needs a workaround". */
    parse("", 0);
    parse("# nothing here\n; nor here\n", 0);

    /* Keys before any section have no game and are dropped rather than
     * attached to whichever entry happens to come first. */
    parse("force_dvd = 1\n[SLUS-20902]\nhide_tray = 1\n", 1);
    assert(atlas_compat_find("SLUS-20902")->flags == ATLAS_COMPAT_HIDE_TRAY);

    /*
     * More entries than the table holds. The list must fill and stop,
     * not overrun - and the entries it did take must be intact, since
     * a user pasting a large shared list still wants the first part of
     * it to work.
     */
    big[0] = '\0';
    for (i = 0; i < ATLAS_COMPAT_MAX + 50; i++) {
        char line[64];
        sprintf(line, "[SLUS-2%04d]\nforce_dvd=1\n", i);
        strcat(big, line);
    }

    assert(atlas_compat_parse(big, (int)strlen(big), &n) == ATLAS_OK);
    assert(n == ATLAS_COMPAT_MAX);
    assert(atlas_compat_count() == ATLAS_COMPAT_MAX);
    assert(atlas_compat_find("SLUS-20000") != NULL);
    assert(atlas_compat_find("SLUS-20000")->flags == ATLAS_COMPAT_FORCE_DVD);

    assert(atlas_compat_parse(NULL, 10, &n) == ATLAS_EINVAL);
    assert(atlas_compat_parse("x", -1, &n) == ATLAS_EINVAL);

    /* A rejected call must not leave the previous list half-replaced:
     * the caller was told nothing happened, and the entries that were
     * already loaded are still the ones in force. */
    parse("[SLUS-20902]\nforce_dvd = 1\n", 1);
    assert(atlas_compat_parse(NULL, 10, NULL) == ATLAS_EINVAL);
    assert(atlas_compat_count() == 1);
    assert(atlas_compat_find("SLUS-20902") != NULL);

    /* Loading replaces rather than adds. A user who fixes a wrong entry
     * and reloads must not still be running the old one. */
    parse("[SLUS-20001]\nforce_dvd = 1\n", 1);
    assert(atlas_compat_find("SLUS-20902") == NULL);

    atlas_compat_clear();
    assert(atlas_compat_count() == 0);
    assert(atlas_compat_find("SLUS-20001") == NULL);

    /* out_count is optional; the count is still readable afterwards. */
    assert(atlas_compat_parse("[SLUS-20902]\nforce_dvd=1\n", 25, NULL)
           == ATLAS_OK);
    assert(atlas_compat_count() == 1);

    /*
     * A length shorter than the text stops there. atlas_compat_load()
     * passes what it actually read, which for an oversized file is a
     * cut somewhere in the middle - the entries before the cut must
     * still load.
     */
    {
        const char *text = "[SLUS-20001]\nforce_dvd=1\n[SLUS-20002]\n";

        assert(atlas_compat_parse(text, 25, &n) == ATLAS_OK);
        assert(n == 1);
        assert(atlas_compat_find("SLUS-20001")->flags
               == ATLAS_COMPAT_FORCE_DVD);
        assert(atlas_compat_find("SLUS-20002") == NULL);
    }
}

/* ------------------------------------------------------------------ */
/* Video mode                                                          */
/* ------------------------------------------------------------------ */

static void check_vmode(void)
{
    atlas_compat_t e;

    memset(&e, 0, sizeof(e));

    /* With no entry, the region decides. */
    assert(atlas_compat_vmode_for(NULL, ATLAS_REGION_PAL)
           == ATLAS_VMODE_PAL);
    assert(atlas_compat_vmode_for(NULL, ATLAS_REGION_NTSC_U)
           == ATLAS_VMODE_NTSC);
    assert(atlas_compat_vmode_for(NULL, ATLAS_REGION_NTSC_J)
           == ATLAS_VMODE_NTSC);

    /*
     * An unknown region stays AUTO. Guessing NTSC here would send PAL
     * homebrew to an NTSC television, where it looks like a bad dump
     * rather than like a setting the user can change.
     */
    assert(atlas_compat_vmode_for(NULL, ATLAS_REGION_UNKNOWN)
           == ATLAS_VMODE_AUTO);

    /* An explicit per-game setting is somebody's tested answer and
     * outranks the region every time. */
    e.vmode = ATLAS_VMODE_NTSC;
    assert(atlas_compat_vmode_for(&e, ATLAS_REGION_PAL) == ATLAS_VMODE_NTSC);

    e.vmode = ATLAS_VMODE_PAL;
    assert(atlas_compat_vmode_for(&e, ATLAS_REGION_NTSC_U) == ATLAS_VMODE_PAL);

    /* AUTO in an entry means "no opinion", so the region decides
     * again - it must not be treated as an override of its own. */
    e.vmode = ATLAS_VMODE_AUTO;
    assert(atlas_compat_vmode_for(&e, ATLAS_REGION_PAL) == ATLAS_VMODE_PAL);
    assert(atlas_compat_vmode_for(&e, ATLAS_REGION_UNKNOWN)
           == ATLAS_VMODE_AUTO);
}

int main(void)
{
    check_basic();
    check_boolean_spellings();
    check_all_flags();
    check_malformed_survives();
    check_id_forms();
    check_vmode_spellings();
    check_empty_and_limits();
    check_vmode();

    printf("test_compat: OK\n");
    return 0;
}
