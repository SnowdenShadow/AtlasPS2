/*
 * AtlasPS2 - device.c
 * The unified storage layer.
 */
#include <string.h>

#include <libmc.h>

/*
 * fileXio_rpc.h refuses to compile without this: the newlib port wants
 * POSIX calls instead. We need the raw interface anyway - opening
 * "mass:/" to test whether the volume mounted has no POSIX equivalent
 * that distinguishes "not mounted" from "empty".
 */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

#include "atlas/device.h"
#include "atlas/path.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static atlas_device_t s_dev[ATLAS_DEV_COUNT];
static int s_have_memcard;
static int s_have_usb;
static int s_cursor;        /* which device the next poll will probe */
static int s_ready;

/*
 * USB enumeration is not instant: the IOP has to see the device, read
 * its descriptors and mount the filesystem, which takes seconds on some
 * sticks. Probing it every poll while it is still coming up costs a
 * blocking open() per attempt, so back off between tries.
 */
#define MASS_RETRY_POLLS 30
static int s_mass_backoff;

/* ------------------------------------------------------------------ */
/* Memory Cards                                                        */
/* ------------------------------------------------------------------ */

/**
 * Probe one Memory Card slot.
 *
 * libmc is asynchronous: the mc* call queues the request and mcSync()
 * waits for it. The result that matters is mcSync's, not mcGetInfo's -
 * libmc.h is explicit that the `format` out-parameter is emulated and
 * that the sync result should be trusted instead.
 */
static void poll_memcard(atlas_device_t *d, int port)
{
    int type = 0, free_clusters = 0, format = 0;
    int cmd = 0, result = 0;

    if (!s_have_memcard) {
        d->state = ATLAS_DEV_ABSENT;
        return;
    }

    if (mcGetInfo(port, 0, &type, &free_clusters, &format) < 0) {
        d->state = ATLAS_DEV_ERROR;
        d->detail = "Memory Card interface not responding";
        d->free_kb = -1;
        return;
    }

    mcSync(MC_WAIT, &cmd, &result);

    /*
     * mcSync results, from libmc.h:
     *    0  same card as the last call
     *   -1  a formatted card was inserted since the last call
     *   -2  an unformatted card was inserted since the last call
     *  <-2  access error (a PS1 card reached this way, for instance)
     *
     * 0 and -1 both mean a usable card is present; the difference is
     * only whether it changed. -2 is a real card we must not treat as
     * absent: the user needs to be told to format it, not left
     * wondering why their card does not show up.
     */
    if (result == -2) {
        d->state = ATLAS_DEV_UNFORMATTED;
        d->detail = "Card is not formatted";
        d->free_kb = -1;
        return;
    }

    if (result < -2) {
        d->state = ATLAS_DEV_ERROR;
        d->detail = "Card unreadable (PS1 card, or damaged)";
        d->free_kb = -1;
        return;
    }

    if (type != sceMcTypePS2) {
        /*
         * A PS1 card or a PocketStation physically fits slot 1. It is
         * present, but nothing here can use it, so say so rather than
         * offering a path that every later operation would fail on.
         */
        if (type == sceMcTypeNoCard) {
            d->state = ATLAS_DEV_ABSENT;
            d->detail = NULL;
        } else {
            d->state = ATLAS_DEV_ERROR;
            d->detail = "Not a PS2 Memory Card";
        }
        d->free_kb = -1;
        return;
    }

    d->state = ATLAS_DEV_READY;
    d->detail = NULL;

    /* A PS2 card cluster is 1024 bytes, so clusters == kilobytes. */
    d->free_kb = (free_clusters >= 0) ? free_clusters : -1;
}

/* ------------------------------------------------------------------ */
/* USB mass storage                                                    */
/* ------------------------------------------------------------------ */

/**
 * Probe the first USB volume.
 *
 * There is no "is it mounted" call, so the test is whether the root
 * directory opens. That is one blocking RPC to the IOP; cheap when the
 * device is there, and the backoff above keeps it from being paid every
 * frame when it is not.
 */
static void poll_mass(atlas_device_t *d)
{
    int fd;

    if (!s_have_usb) {
        d->state = ATLAS_DEV_ABSENT;
        return;
    }

    if (d->state != ATLAS_DEV_READY && s_mass_backoff > 0) {
        s_mass_backoff--;
        return;
    }

    fd = fileXioDopen("mass:/");

    if (fd < 0) {
        d->state = ATLAS_DEV_ABSENT;
        d->detail = NULL;
        d->free_kb = -1;
        s_mass_backoff = MASS_RETRY_POLLS;
        return;
    }

    fileXioDclose(fd);

    d->state = ATLAS_DEV_READY;
    d->detail = NULL;

    /*
     * Left unknown deliberately. Free space on a FAT volume means
     * walking the allocation table, which on a large stick takes long
     * enough to drop frames - and no screen needs the number badly
     * enough to justify that.
     */
    d->free_kb = -1;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_device_init(int have_memcard, int have_usb)
{
    int i;

    static const char *names[ATLAS_DEV_COUNT] = {
        "Memory Card 1", "Memory Card 2", "USB", "HDD"
    };
    static const char *paths[ATLAS_DEV_COUNT] = {
        "mc0:", "mc1:", "mass:", "hdd0:"
    };

    memset(s_dev, 0, sizeof(s_dev));

    for (i = 0; i < ATLAS_DEV_COUNT; i++) {
        s_dev[i].id      = (atlas_device_id_t)i;
        s_dev[i].state   = ATLAS_DEV_ABSENT;
        s_dev[i].name    = names[i];
        s_dev[i].path    = paths[i];
        s_dev[i].free_kb = -1;
        s_dev[i].detail  = NULL;
    }

    s_have_memcard = have_memcard;
    s_have_usb     = have_usb;
    s_cursor       = 0;
    s_mass_backoff = 0;

    if (s_have_memcard) {
        /*
         * MC_TYPE_MC selects mcserv, the module boot.c loads. A failure
         * here is not fatal: the cards simply stay absent and every
         * other device still works.
         */
        if (mcInit(MC_TYPE_MC) < 0) {
            ATLAS_LOG("DEV", "mcInit failed; Memory Cards unavailable");
            s_have_memcard = 0;
        }
    }

    s_ready = 1;

    ATLAS_LOG("DEV", "init mc=%d usb=%d", s_have_memcard, s_have_usb);

    return ATLAS_OK;
}

int atlas_device_poll(void)
{
    atlas_device_t *d;
    atlas_device_state_t before;

    if (!s_ready)
        return 0;

    d = &s_dev[s_cursor];
    before = d->state;

    switch (d->id) {
    case ATLAS_DEV_MC0:
        poll_memcard(d, 0);
        break;
    case ATLAS_DEV_MC1:
        poll_memcard(d, 1);
        break;
    case ATLAS_DEV_MASS:
        poll_mass(d);
        break;
    case ATLAS_DEV_HDD:
        /*
         * The HDD needs its own module set (dev9, atad, hdd, pfs) that
         * is not loaded yet, and mounting a partition is a separate
         * step. Until then it is honestly absent rather than shown as
         * an entry that fails when opened.
         */
        d->state = ATLAS_DEV_ABSENT;
        break;
    default:
        break;
    }

    /* Advance regardless, so one failing device cannot starve the rest. */
    s_cursor = (s_cursor + 1) % ATLAS_DEV_COUNT;

    if (d->state != before) {
        ATLAS_LOG("DEV", "%s %d -> %d", d->name, before, d->state);
        return 1;
    }

    return 0;
}

const atlas_device_t *atlas_device_get(atlas_device_id_t id)
{
    if (id < 0 || id >= ATLAS_DEV_COUNT)
        return NULL;

    return &s_dev[id];
}

int atlas_device_is_ready(atlas_device_id_t id)
{
    const atlas_device_t *d = atlas_device_get(id);

    return d && d->state == ATLAS_DEV_READY;
}

int atlas_device_ready_count(void)
{
    int i, n = 0;

    for (i = 0; i < ATLAS_DEV_COUNT; i++) {
        if (s_dev[i].state == ATLAS_DEV_READY)
            n++;
    }

    return n;
}

atlas_err_t atlas_device_path(atlas_device_id_t id, const char *rel,
                              char *out, int size)
{
    const atlas_device_t *d = atlas_device_get(id);

    if (!d)
        return ATLAS_EINVAL;

    /*
     * Not-ready is refused here rather than in atlas_path_join: a path
     * on an absent device is well-formed but useless, and returning one
     * would let a caller open, create or delete against a device that
     * is not there.
     */
    if (d->state != ATLAS_DEV_READY)
        return ATLAS_ENODEV;

    return atlas_path_join(d->path, rel, out, size);
}

void atlas_device_shutdown(void)
{
    int i;

    for (i = 0; i < ATLAS_DEV_COUNT; i++)
        s_dev[i].state = ATLAS_DEV_ABSENT;

    s_ready = 0;
}
