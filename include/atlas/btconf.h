/*
 * AtlasPS2 - btconf.h
 *
 * The IOP boot module list, with the drive modules taken out.
 *
 * WHY THIS EXISTS
 * ---------------
 * A module cannot register a library name that is already registered.
 * The stock cdvdman is in the IOP's boot list, so on an ordinary reset
 * it takes the name and our drive emulation loads and is never called.
 * The only way to win is for the real one not to be there, and the only
 * place that is decided is the module list the IOP boots with.
 *
 * WHY IT IS READ FROM THE CONSOLE AND NOT WRITTEN DOWN HERE
 * ---------------------------------------------------------
 * That list differs between console revisions. A list baked into this
 * program would be a guess about somebody else's machine, and a wrong
 * one is not a message - it is an IOP that boots without a module it
 * needed, on a console we do not own, with no screen left to say so.
 *
 * So the list is the console's own, read from rom0:IOPBTCONF, and this
 * file only ever removes lines from it. Everything it does not
 * recognise it copies through byte for byte.
 *
 * WHY THE MODULES COME BACK AFTERWARDS
 * ------------------------------------
 * cdvdfsv is what the EE's own disc calls reach: the game calls
 * sceCdRead on the EE, cdvdfsv receives it on the IOP, and cdvdfsv
 * calls cdvdman. It imports cdvdman, so it cannot load while cdvdman is
 * missing - which is exactly the window we are creating. It is
 * therefore removed with cdvdman and loaded again from rom0: once our
 * module holds the name, which is why `removed` is a list and not a
 * flag: it says what to put back, in the order it was taken out.
 */
#ifndef ATLAS_BTCONF_H
#define ATLAS_BTCONF_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The stock list is a few hundred bytes on every console this runs on.
 * A list that does not fit is refused rather than truncated: half a
 * boot list is an IOP missing whichever modules came after the cut. */
#define ATLAS_BTCONF_MAX          2048

/* CDVDMAN, CDVDFSV, CDVDSTM - and room for one more should a revision
 * carry a fourth. */
#define ATLAS_BTCONF_REMOVED_MAX  4
#define ATLAS_BTCONF_NAME_MAX     16

typedef struct {
    /** The filtered list, ready to be handed to the IOP. Not a C
     *  string: the file is not one, and `len` is the length. */
    char text[ATLAS_BTCONF_MAX];
    int  len;

    /** The modules taken out, in the order the list named them. These
     *  are loaded again from rom0: after the drive emulation has the
     *  cdvdman name. */
    char removed[ATLAS_BTCONF_REMOVED_MAX][ATLAS_BTCONF_NAME_MAX];
    int  removed_count;
} atlas_btconf_t;

/**
 * Copy a boot list, dropping the lines that name a drive module.
 *
 * A line is dropped when its first token, ignoring leading blanks and
 * case, is exactly one of the drive module names. Everything else -
 * comments, blank lines, control lines, line endings, a file that does
 * not end in a newline - is copied through unchanged, because this
 * knows what it is removing and nothing else about the format.
 *
 * @param src the file as read, not necessarily NUL-terminated
 * @param len its length in bytes
 * @return ATLAS_OK;
 *         ATLAS_EINVAL on a null argument or a negative length;
 *         ATLAS_ENOMEM if the list does not fit in ATLAS_BTCONF_MAX;
 *         ATLAS_EFORMAT if the list does not name cdvdman at all,
 *                      which means this is not a list we understand -
 *                      and acting on a list we do not understand is
 *                      the one thing worse than refusing.
 */
atlas_err_t atlas_btconf_filter(const char *src, int len, atlas_btconf_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_BTCONF_H */
