/*
 * AtlasPS2 - compat.c
 * Per-game settings, read from a file the user can edit.
 *
 * No fileXio here: the parsing is checkable on the build machine, and
 * what it decides - which workarounds a game gets, which video mode it
 * is told about - is invisible until a television is in front of you.
 * The read itself is in compat_io.c.
 */
#include <string.h>

#include "atlas/compat.h"
#include "atlas/ini.h"
#include "atlas/log.h"

/*
 * The list lives here rather than in a caller-owned struct. There is
 * one of it, it is consulted from the browser and from the launcher,
 * and passing it through both would be a pointer that can only ever
 * have one value.
 */
static atlas_compat_t s_entry[ATLAS_COMPAT_MAX];
static int s_count;

/* ------------------------------------------------------------------ */
/* Values                                                              */
/* ------------------------------------------------------------------ */

/**
 * Compare two strings ignoring case, ASCII only.
 *
 * The INI reader hands values back verbatim, because a value can be a
 * path or a name where case matters. Here it does not, so the settings
 * below do their own folding rather than the file being lowercased for
 * everyone.
 */
static int ieq(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] && b[i]; i++) {
        char ca = a[i], cb = b[i];

        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return 0;
    }

    return a[i] == b[i];
}

/**
 * Read a boolean the way a person writes one.
 *
 * A user editing this file on a PC writes "1", "true", "yes" or "on"
 * depending on habit, and a parser that only accepts one of them fails
 * silently: the setting is simply not applied, and the game still hangs.
 * There is nothing to tell them apart from a game that needed a
 * different workaround.
 *
 * The spellings are matched whole rather than by first letter. Matching
 * on the letter is shorter and would accept every one of these, but it
 * also accepts "nearly" as no and "perhaps" as nothing recognisable at
 * all only by luck - and a value this does not understand must be said
 * out loud, since a setting that quietly did not apply is exactly the
 * thing the user cannot see.
 */
static int parse_bool(const char *v, int *out)
{
    static const char *const k_true[]  = { "1", "yes", "true", "on" };
    static const char *const k_false[] = { "0", "no", "false", "off" };
    int i;

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_true); i++) {
        if (ieq(v, k_true[i])) {
            *out = 1;
            return 0;
        }
    }

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_false); i++) {
        if (ieq(v, k_false[i])) {
            *out = 0;
            return 0;
        }
    }

    return -1;
}

static const struct {
    const char *key;
    u32         bit;
} k_flag[] = {
    { "force_dvd",       ATLAS_COMPAT_FORCE_DVD       },
    { "hide_tray",       ATLAS_COMPAT_HIDE_TRAY       },
    { "slow_first_read", ATLAS_COMPAT_SLOW_FIRST_READ },
    { "low_modules",     ATLAS_COMPAT_LOW_MODULES     },
    { "no_disc_patch",   ATLAS_COMPAT_NO_DISC_PATCH   }
};

/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    int  current;       /**< index of the entry being filled, or -1 */
    char section[ATLAS_INI_SECTION_MAX];
} parse_state_t;

/**
 * Find or create the entry for a section name.
 *
 * @return its index, or -1 if the name is not a game ID or the table
 *         is full.
 */
static int entry_for(const char *section)
{
    char id[ATLAS_DISC_ID_MAX];
    int i;

    /*
     * The section name has to be a real game ID. A typo would otherwise
     * become an entry that never matches anything, and the user would
     * see their setting having no effect with nothing to explain why -
     * so it is normalised through the same function the disc goes
     * through, and a name that is not an ID is rejected here where it
     * can be logged.
     */
    if (atlas_disc_id_normalize(section, id, sizeof(id)) != ATLAS_OK) {
        ATLAS_LOG("COMPAT", "'%s' is not a game ID", section);
        return -1;
    }

    for (i = 0; i < s_count; i++) {
        if (strcmp(s_entry[i].id, id) == 0)
            return i;
    }

    if (s_count >= ATLAS_COMPAT_MAX) {
        ATLAS_LOG("COMPAT", "list full at %d entries", ATLAS_COMPAT_MAX);
        return -1;
    }

    memset(&s_entry[s_count], 0, sizeof(s_entry[0]));
    strcpy(s_entry[s_count].id, id);

    return s_count++;
}

static int compat_key(void *user, const char *section, const char *key,
                      const char *value)
{
    parse_state_t *st = (parse_state_t *)user;
    atlas_compat_t *e;
    int i;

    /* Keys before the first [SLUS-...] have no game to belong to. */
    if (!section[0])
        return 0;

    /*
     * Sections arrive in order, so the entry only has to be resolved
     * when the name changes. Doing it per key would re-normalise and
     * re-search the table for every line of the file.
     */
    if (strcmp(section, st->section) != 0) {
        strcpy(st->section, section);
        st->current = entry_for(section);
    }

    if (st->current < 0)
        return 0;   /* a section that is not a game ID; skip its keys */

    e = &s_entry[st->current];

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_flag); i++) {
        if (strcmp(key, k_flag[i].key) == 0) {
            int on;

            if (parse_bool(value, &on) != 0) {
                ATLAS_LOG("COMPAT", "%s: %s='%s' is not yes or no",
                          e->id, key, value);
                return 0;
            }

            if (on)
                e->flags |= k_flag[i].bit;
            else
                e->flags &= ~k_flag[i].bit;

            return 0;
        }
    }

    if (strcmp(key, "vmode") == 0) {
        /* Whole words, for the same reason the booleans are: "purple"
         * is not a video mode, and accepting it as PAL would give the
         * user a picture running a fifth too slow with no way to tell
         * that from a bad dump. */
        if (ieq(value, "pal"))
            e->vmode = ATLAS_VMODE_PAL;
        else if (ieq(value, "ntsc"))
            e->vmode = ATLAS_VMODE_NTSC;
        else if (ieq(value, "auto"))
            e->vmode = ATLAS_VMODE_AUTO;
        else
            ATLAS_LOG("COMPAT", "%s: vmode='%s' unknown", e->id, value);

        return 0;
    }

    /*
     * An unknown key is skipped and logged rather than failing the
     * file. A list shared between launchers will carry keys this does
     * not implement, and dropping the whole file over one of them
     * would lose the entries that do work.
     */
    ATLAS_LOG("COMPAT", "%s: unknown setting '%s'", e->id, key);
    return 0;
}

atlas_err_t atlas_compat_parse(const char *text, int len, int *out_count)
{
    parse_state_t st;

    if (out_count)
        *out_count = 0;

    if (!text || len < 0)
        return ATLAS_EINVAL;

    atlas_compat_clear();

    st.current = -1;
    st.section[0] = '\0';

    atlas_ini_parse(text, len, compat_key, &st, NULL);

    if (out_count)
        *out_count = s_count;

    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Lookup                                                              */
/* ------------------------------------------------------------------ */

const atlas_compat_t *atlas_compat_find(const char *id)
{
    int i;

    if (!id || !id[0])
        return NULL;

    for (i = 0; i < s_count; i++) {
        if (strcmp(s_entry[i].id, id) == 0)
            return &s_entry[i];
    }

    return NULL;
}

int atlas_compat_count(void)
{
    return s_count;
}

void atlas_compat_clear(void)
{
    s_count = 0;
}

atlas_compat_vmode_t atlas_compat_vmode_for(const atlas_compat_t *entry,
                                            atlas_region_t region)
{
    /* An explicit per-game setting is somebody's tested answer, and it
     * outranks anything derived from the ID prefix. */
    if (entry && entry->vmode != ATLAS_VMODE_AUTO)
        return entry->vmode;

    switch (region) {
    case ATLAS_REGION_PAL:
        return ATLAS_VMODE_PAL;

    case ATLAS_REGION_NTSC_U:
    case ATLAS_REGION_NTSC_J:
        return ATLAS_VMODE_NTSC;

    default:
        /*
         * An unknown region stays AUTO rather than guessing NTSC. The
         * caller shows the user a choice; guessing here would send a
         * PAL homebrew to an NTSC television and look like a bad dump.
         */
        return ATLAS_VMODE_AUTO;
    }
}
