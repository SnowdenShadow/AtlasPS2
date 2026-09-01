/*
 * AtlasPS2 - disc.c
 * Identifying a disc image: ISO9660, SYSTEM.CNF, and the game ID.
 *
 * No fileXio here, on purpose. Sectors arrive through a callback, so
 * `make check` feeds this a synthetic image on the build machine and
 * pins every answer. The alternative is discovering that a game ID was
 * parsed wrong when a console shows a black screen.
 */
#include <string.h>

#include "atlas/disc.h"
#include "atlas/ini.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* ISO9660 layout                                                      */
/*                                                                     */
/* Byte offsets from ECMA-119. Named rather than inlined because the   */
/* difference between offset 156 and 158 is a launcher that reads a    */
/* directory from the wrong place and reports every disc as empty.     */
/* ------------------------------------------------------------------ */

/* The volume descriptor set starts here, one descriptor per sector. */
#define VD_START_LBA        16
#define VD_SCAN_MAX         32   /* give up rather than walk a bad image */

#define VD_TYPE_PRIMARY     1
#define VD_TYPE_TERMINATOR  255

#define VD_OFF_TYPE         0
#define VD_OFF_MAGIC        1    /* "CD001"                            */
#define VD_OFF_VOLUME_ID    40   /* 32 bytes, space-padded             */
#define VD_OFF_VOLUME_SIZE  80   /* both-endian u32; LE half is here   */
#define VD_OFF_BLOCK_SIZE   128  /* both-endian u16; LE half is here   */
#define VD_OFF_ROOT_RECORD  156  /* a directory record, 34 bytes       */

/* Directory record fields, from the start of the record. */
#define DR_OFF_LENGTH       0
#define DR_OFF_EXTENT_LBA   2    /* both-endian u32; LE half is here   */
#define DR_OFF_DATA_LEN     10   /* both-endian u32; LE half is here   */
#define DR_OFF_FLAGS        25
#define DR_OFF_NAME_LEN     32
#define DR_OFF_NAME         33

#define DR_FLAG_DIRECTORY   0x02

/* A directory record is never smaller than its fixed part plus one
 * character of name, so anything shorter is padding or corruption. */
#define DR_MIN_LENGTH       34

/*
 * SYSTEM.CNF is a handful of lines - the largest seen in the wild is
 * well under 200 bytes. Reading one sector covers every real disc, and
 * capping it means a corrupt directory entry claiming a 700 MB
 * SYSTEM.CNF cannot ask this for 700 MB of stack.
 */
#define CNF_MAX_BYTES       ATLAS_DISC_SECTOR_SIZE

/* ------------------------------------------------------------------ */
/* Little-endian field readers                                         */
/*                                                                     */
/* The EE is little-endian and so is every field used here, but these  */
/* are read byte-wise anyway: the buffer is a raw sector with no       */
/* alignment guarantee, and a u32 load from an odd offset is an        */
/* address error on MIPS, not a slow read.                             */
/* ------------------------------------------------------------------ */

static u32 le32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u16 le16(const u8 *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

/* ------------------------------------------------------------------ */
/* Small character helpers                                             */
/*                                                                     */
/* Not <ctype.h>: those are locale-dependent and take an int that must */
/* not be a negative char. These see raw disc bytes, which are         */
/* whatever the mastering tool wrote.                                  */
/* ------------------------------------------------------------------ */

static int is_upper_alpha(int c) { return c >= 'A' && c <= 'Z'; }
static int is_lower_alpha(int c) { return c >= 'a' && c <= 'z'; }
static int is_digit(int c)       { return c >= '0' && c <= '9'; }

static char to_upper(char c)
{
    return is_lower_alpha((unsigned char)c) ? (char)(c - 'a' + 'A') : c;
}

/* ------------------------------------------------------------------ */
/* Game IDs                                                            */
/* ------------------------------------------------------------------ */

/*
 * Region is decided by the four-letter prefix of the game ID. It is not
 * cosmetic: it selects the video mode the emulated drive reports, and a
 * PAL game told it is on an NTSC console either refuses to start or
 * runs a fifth too fast with the bottom of the picture missing.
 *
 * Anything not listed is UNKNOWN, which is the honest answer for
 * homebrew and prototypes - and the caller treats UNKNOWN as "ask the
 * user" rather than guessing a mode on their behalf.
 */
static const struct {
    const char     *prefix;
    atlas_region_t  region;
} k_id_prefix[] = {
    { "SLUS", ATLAS_REGION_NTSC_U },
    { "SCUS", ATLAS_REGION_NTSC_U },

    { "SLES", ATLAS_REGION_PAL    },
    { "SCES", ATLAS_REGION_PAL    },
    { "SLED", ATLAS_REGION_PAL    },  /* demo discs                    */
    { "SCED", ATLAS_REGION_PAL    },

    { "SLPS", ATLAS_REGION_NTSC_J },
    { "SLPM", ATLAS_REGION_NTSC_J },
    { "SCPS", ATLAS_REGION_NTSC_J },
    { "SCPM", ATLAS_REGION_NTSC_J },
    { "SLKA", ATLAS_REGION_NTSC_J },  /* Korea, NTSC like Japan        */
    { "SCKA", ATLAS_REGION_NTSC_J },
    { "SLAJ", ATLAS_REGION_NTSC_J },  /* Asia                          */
    { "SCAJ", ATLAS_REGION_NTSC_J },
    { "PAPX", ATLAS_REGION_NTSC_J },  /* press / promotional           */
    { "PBPX", ATLAS_REGION_NTSC_J },
    { "TCPS", ATLAS_REGION_NTSC_J }   /* third-party Japanese label    */
};

atlas_region_t atlas_disc_region_of_id(const char *id)
{
    int i;

    if (!id)
        return ATLAS_REGION_UNKNOWN;

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_id_prefix); i++) {
        if (strncmp(id, k_id_prefix[i].prefix, 4) == 0)
            return k_id_prefix[i].region;
    }

    return ATLAS_REGION_UNKNOWN;
}

const char *atlas_disc_region_str(atlas_region_t region)
{
    switch (region) {
    case ATLAS_REGION_NTSC_U: return "NTSC-U";
    case ATLAS_REGION_NTSC_J: return "NTSC-J";
    case ATLAS_REGION_PAL:    return "PAL";
    default:                  return "Unknown";
    }
}

atlas_err_t atlas_disc_id_normalize(const char *raw, char *out, int size)
{
    const char *name;
    const char *p;
    int n = 0;
    int alpha = 0, tail;

    if (!raw || !out || size <= 0)
        return ATLAS_EINVAL;

    /*
     * Strip any path in front. Discs write "cdrom0:\SLUS_209.02;1",
     * "cdrom0:SLUS_209.02", and both slash directions; taking whatever
     * follows the last separator handles all of them without caring
     * which spelling this particular mastering tool preferred.
     */
    name = raw;
    for (p = raw; *p; p++) {
        if (*p == '\\' || *p == '/' || *p == ':')
            name = p + 1;
    }

    for (p = name; *p; p++) {
        char c;

        /* ";1" is the ISO9660 file version, never part of the ID. */
        if (*p == ';')
            break;

        if (*p == '.')        /* "209.02" -> "20902"                   */
            continue;

        c = (*p == '_') ? '-' : to_upper(*p);

        /*
         * Anything else is not something a game ID contains. Stopping
         * rather than skipping matters: trailing junk means this was
         * not an ID, and silently cleaning it up would manufacture one
         * that a compatibility database could then match on.
         */
        if (c != '-' && !is_upper_alpha((unsigned char)c)
                     && !is_digit((unsigned char)c))
            break;

        if (n >= size - 1)
            return ATLAS_EINVAL;  /* refuse; a short ID names another game */

        out[n++] = c;
    }

    /*
     * Shape check: four letters, a separator, then at least two more
     * characters. Real IDs are "SLUS-20902"; without this, "BOOT" or an
     * empty string would be accepted as an ID and looked up as one.
     */
    while (alpha < n && is_upper_alpha((unsigned char)out[alpha]))
        alpha++;

    tail = n - alpha - 1;
    if (alpha != 4 || n < 5 || out[4] != '-' || tail < 2)
        return ATLAS_EINVAL;

    out[n] = '\0';
    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* SYSTEM.CNF                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char boot[ATLAS_DISC_BOOT_MAX];
    char vmode[8];
} cnf_t;

/*
 * SYSTEM.CNF is "KEY = VALUE" lines, which is what the INI reader
 * already handles - keys arrive lowercased, values verbatim. Writing a
 * second parser for the same grammar would double the ways a boot path
 * can be read wrong.
 */
static int cnf_key(void *user, const char *section, const char *key,
                   const char *value)
{
    cnf_t *c = (cnf_t *)user;

    ATLAS_UNUSED(section);

    if (strcmp(key, "boot2") == 0) {
        /*
         * Longer than we can hold means we would launch a different
         * path than the disc asked for, so the field stays empty and
         * the disc is reported as unlaunchable rather than launched
         * wrongly.
         */
        if (strlen(value) < sizeof(c->boot))
            strcpy(c->boot, value);
        else
            ATLAS_LOG("DISC", "BOOT2 too long (%d bytes)", (int)strlen(value));
    } else if (strcmp(key, "vmode") == 0) {
        if (strlen(value) < sizeof(c->vmode))
            strcpy(c->vmode, value);
    }

    return 0;
}

static void parse_cnf(char *text, int len, cnf_t *out)
{
    int i;

    memset(out, 0, sizeof(*out));

    /*
     * Some discs end SYSTEM.CNF lines with a bare CR and no LF. The INI
     * reader splits on LF, so such a file would arrive as one enormous
     * line and BOOT2 would swallow every following key as part of its
     * value - a boot path that does not exist. Translating CR to LF up
     * front costs one pass and makes all three line-ending conventions
     * the same file. CRLF is unaffected: the CR becomes a second
     * newline, and blank lines are already skipped.
     */
    for (i = 0; i < len; i++) {
        if (text[i] == '\r')
            text[i] = '\n';
    }

    atlas_ini_parse(text, len, cnf_key, out, NULL);
}

/* ------------------------------------------------------------------ */
/* Directory walking                                                   */
/* ------------------------------------------------------------------ */

/**
 * Does this directory record name `want` (an upper-case name with no
 * version suffix)?
 *
 * ISO9660 names carry ";1" and may be recorded in either case despite
 * the standard's insistence on upper. Both are normalised here rather
 * than at the call site, because every future caller looking for a file
 * on a disc needs exactly the same two rules.
 */
static int record_is(const u8 *rec, const char *want)
{
    int name_len = rec[DR_OFF_NAME_LEN];
    const char *name = (const char *)(rec + DR_OFF_NAME);
    int i;

    /* Drop the version suffix before comparing. */
    for (i = 0; i < name_len; i++) {
        if (name[i] == ';') {
            name_len = i;
            break;
        }
    }

    if (name_len != (int)strlen(want))
        return 0;

    for (i = 0; i < name_len; i++) {
        if (to_upper(name[i]) != want[i])
            return 0;
    }

    return 1;
}

/**
 * Find a file in a directory extent.
 *
 * @return 1 and fills `lba`/`size` if found, 0 if not, negative on a
 *         read failure - three outcomes the caller must tell apart: a
 *         disc with no SYSTEM.CNF is a disc, and a USB stick that
 *         stopped responding is a fault.
 */
static int find_in_dir(atlas_disc_read_fn read, void *ctx,
                       u32 dir_lba, u32 dir_size,
                       const char *want, u32 *out_lba, u32 *out_size)
{
    u8 sector[ATLAS_DISC_SECTOR_SIZE];
    u32 sectors = (dir_size + ATLAS_DISC_SECTOR_SIZE - 1)
                / ATLAS_DISC_SECTOR_SIZE;
    u32 s;

    /* A directory claiming more than this is corrupt, and walking it
     * would be a long silent hang on a console with no way to cancel. */
    if (sectors > 64)
        sectors = 64;

    for (s = 0; s < sectors; s++) {
        int off = 0;

        if (read(ctx, dir_lba + s, 1, sector) != 0)
            return -1;

        /*
         * Records never cross a sector boundary; the remainder of a
         * sector is zero padding, and a zero length byte is how the
         * standard says "no more records here".
         */
        while (off < ATLAS_DISC_SECTOR_SIZE) {
            const u8 *rec = sector + off;
            int rec_len = rec[DR_OFF_LENGTH];

            if (rec_len == 0)
                break;

            /*
             * A record that runs past the sector, or is too short to
             * hold its own fixed fields, means the image is damaged.
             * Stop reading this sector rather than interpreting bytes
             * from beyond the record as a name length.
             */
            if (rec_len < DR_MIN_LENGTH
                || off + rec_len > ATLAS_DISC_SECTOR_SIZE)
                break;

            /* Guard the name too: name_len is disc-supplied, and a
             * damaged one would read past the end of the record. */
            if (DR_OFF_NAME + rec[DR_OFF_NAME_LEN] <= rec_len
                && !(rec[DR_OFF_FLAGS] & DR_FLAG_DIRECTORY)
                && record_is(rec, want)) {
                *out_lba  = le32(rec + DR_OFF_EXTENT_LBA);
                *out_size = le32(rec + DR_OFF_DATA_LEN);
                return 1;
            }

            off += rec_len;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Volume descriptor                                                   */
/* ------------------------------------------------------------------ */

/** Copy an ISO9660 space-padded field, trimming the padding. */
static void copy_padded(const u8 *src, int len, char *out, int size)
{
    int n = len;

    if (n > size - 1)
        n = size - 1;

    while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\0'))
        n--;

    memcpy(out, src, (size_t)n);
    out[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_disc_probe(atlas_disc_read_fn read, void *ctx,
                             atlas_disc_info_t *out)
{
    u8 sector[ATLAS_DISC_SECTOR_SIZE];
    atlas_disc_info_t info;
    cnf_t cnf;
    char cnf_text[CNF_MAX_BYTES + 1];
    u32 root_lba, root_size;
    u32 cnf_lba = 0, cnf_size = 0;
    u32 lba;
    int found_pvd = 0;
    int hit;

    if (!read || !out)
        return ATLAS_EINVAL;

    memset(&info, 0, sizeof(info));

    /*
     * Scan the descriptor set rather than assuming the PVD sits at 16.
     * It usually does, but an image carrying a boot record or a Joliet
     * supplementary descriptor puts it further along the set, and those
     * are ordinary discs, not broken ones.
     */
    for (lba = VD_START_LBA; lba < VD_SCAN_MAX; lba++) {
        if (read(ctx, lba, 1, sector) != 0)
            return ATLAS_EIO;

        if (memcmp(sector + VD_OFF_MAGIC, "CD001", 5) != 0)
            return ATLAS_EFORMAT;   /* not ISO9660 at all */

        if (sector[VD_OFF_TYPE] == VD_TYPE_TERMINATOR)
            break;

        if (sector[VD_OFF_TYPE] == VD_TYPE_PRIMARY) {
            found_pvd = 1;
            break;
        }
    }

    if (!found_pvd)
        return ATLAS_EFORMAT;

    /*
     * The PS2 drive reports 2048-byte logical blocks and its file layer
     * assumes them. An image built with any other block size is not
     * something this console boots, and pretending otherwise would put
     * every subsequent LBA in the wrong place.
     */
    if (le16(sector + VD_OFF_BLOCK_SIZE) != ATLAS_DISC_SECTOR_SIZE)
        return ATLAS_EFORMAT;

    info.sectors = le32(sector + VD_OFF_VOLUME_SIZE);
    copy_padded(sector + VD_OFF_VOLUME_ID, 32,
                info.volume, sizeof(info.volume));

    root_lba  = le32(sector + VD_OFF_ROOT_RECORD + DR_OFF_EXTENT_LBA);
    root_size = le32(sector + VD_OFF_ROOT_RECORD + DR_OFF_DATA_LEN);

    if (root_size == 0)
        return ATLAS_EFORMAT;

    /*
     * From here, failing to find SYSTEM.CNF is not an error. Data discs
     * and a few legitimate titles have none; the caller shows them with
     * an empty ID and refuses to launch them, which is more useful than
     * hiding the disc entirely.
     */
    hit = find_in_dir(read, ctx, root_lba, root_size,
                      "SYSTEM.CNF", &cnf_lba, &cnf_size);
    if (hit < 0)
        return ATLAS_EIO;

    if (hit == 1 && cnf_size > 0) {
        u32 want = cnf_size;

        if (want > CNF_MAX_BYTES)
            want = CNF_MAX_BYTES;

        if (read(ctx, cnf_lba, 1, sector) != 0)
            return ATLAS_EIO;

        memcpy(cnf_text, sector, want);
        cnf_text[want] = '\0';

        parse_cnf(cnf_text, (int)want, &cnf);

        if (cnf.boot[0]) {
            strcpy(info.boot, cnf.boot);

            /*
             * An unparsable BOOT2 leaves the ID empty rather than
             * inventing one: the ID keys the compatibility database,
             * and a wrong key applies another game's patches.
             */
            if (atlas_disc_id_normalize(cnf.boot, info.id,
                                        sizeof(info.id)) != ATLAS_OK) {
                info.id[0] = '\0';
                ATLAS_LOG("DISC", "no game ID in BOOT2 '%s'", cnf.boot);
            }
        }

        info.region = atlas_disc_region_of_id(info.id);

        /*
         * VMODE, when present, is the disc telling us directly, and it
         * outranks the ID prefix: the discs that carry it are largely
         * the ones whose prefix does not match how they were actually
         * mastered, which is the case the prefix table gets wrong.
         */
        if (cnf.vmode[0]) {
            if (to_upper(cnf.vmode[0]) == 'P')
                info.region = ATLAS_REGION_PAL;
            else if (to_upper(cnf.vmode[0]) == 'N'
                     && info.region == ATLAS_REGION_UNKNOWN)
                info.region = ATLAS_REGION_NTSC_U;
        }
    }

    *out = info;
    return ATLAS_OK;
}
