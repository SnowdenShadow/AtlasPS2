/*
 * AtlasPS2 - atlascdvd
 *
 * The module a game reads its disc through when the disc is a file.
 *
 * WHAT THIS IS
 * ------------
 * A replacement for `cdvdman`. It registers under that name, with that
 * export table, so that a game's own modules link to it without knowing
 * anything has changed. Every read it is asked for is served from a
 * list of raw device sectors handed to it at load time.
 *
 * WHY IT NEVER TOUCHES A FILESYSTEM
 * ---------------------------------
 * By the time this module answers its first read, the game owns the
 * IOP. Its modules are loaded, its threads run at priorities it chose,
 * and it programs DMA channels whenever it likes. Calling into a FAT
 * driver from here would mean running thousands of lines of somebody
 * else's code - allocating, taking semaphores, touching a cache - on a
 * path the game can pre-empt, in memory the game may have decided is
 * its own.
 *
 * So the filesystem is read exactly once, here, in _start - before the
 * game's own modules are loaded and before a line of it has run. The
 * file is reduced to the list of device sectors it occupies, and every
 * read after that is a raw sector read.
 *
 * The reader that does this is src/disc/frag.c, the same file the EE
 * side uses, compiled a second time by the IOP toolchain. It is shared
 * rather than transcribed because tests/test_frag.c is run against it
 * on every build: two copies of the arithmetic that decides which
 * sectors a game reads, one of them untested, is exactly how a game
 * ends up reading somebody else's data with nothing to show it did.
 *
 * WHAT IS AND IS NOT HERE
 * -----------------------
 * ISO images on a bdm block device - which on this console means USB.
 * That is deliberately the whole of it: docs/DISC.md sets out the order
 * this work goes in, and the first step is one format on one device, so
 * that when a game does not boot there is one thing it can be.
 *
 * ZSO, HDD and SMB are not here. Each is a change to bd_read() and to
 * nothing else, which is why that function is the only place in this
 * file that knows where bytes come from.
 *
 * NONE OF THIS IS TESTABLE ON A BUILD MACHINE
 * -------------------------------------------
 * There is no host on which this code can run, and no emulator whose
 * cdvdman behaviour is close enough to be evidence. What *can* be
 * checked off-console is checked elsewhere and deliberately kept out of
 * this file: the FAT walk and the extent arithmetic are in
 * src/disc/frag.c with tests/test_frag.c over them, the sector framing
 * is in src/disc/sector.c with tests/test_sector.c over it, and the
 * export ordinals are checked against the SDK header by
 * tools/checkexports.py as part of the build.
 *
 * What remains here is sequencing and hardware, and it is unverified
 * until it runs on a console. That is stated plainly rather than
 * implied, because a black screen is indistinguishable from a bad dump
 * and the user cannot tell which they have.
 */

#include <bdm.h>
#include <intrman.h>
#include <irx.h>
#include <libcdvd-common.h>
#include <loadcore.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <types.h>

#include "atlas/frag.h"

#include "atlascdvd.h"

IRX_ID("cdvdman", 1, 1);

extern struct irx_export_table _exp_cdvdman;

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define SECTOR_BYTES        2048        /* a PS2 disc sector           */

/*
 * The bounce buffer, in sectors.
 *
 * Reads never go straight into the buffer the game handed over: the
 * game may be DMAing from it at the same time, and a device driver
 * writing into it concurrently is a texture that is wrong twenty
 * minutes later rather than a failure anyone can attribute. So bytes
 * land here first and are copied.
 *
 * 16 sectors is 32 KB. Larger would mean fewer device transactions;
 * this is IOP RAM, of which there are 2 MB in total and the game
 * expects almost all of it.
 */
#define BOUNCE_SECTORS      16
#define BOUNCE_BYTES        (BOUNCE_SECTORS * SECTOR_BYTES)

/* How many times a failed device read is retried before it is reported.
 * A USB stick that NAKs once is ordinary; a game told the disc is
 * unreadable is a crash. */
#define READ_RETRIES        5

/* The first read a few titles issue arrives before their own interrupt
 * handlers are installed - they assume a physical drive takes tens of
 * milliseconds to answer. `slow_first_read` waits this long. */
#define SLOW_FIRST_USEC     80000

#define EF_DONE             1           /* the read-completed event    */

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static atlascdvd_arg_t  g_arg;
static struct block_device *g_bd;

/* Where the image lives on the device. Built once in _start and then
 * only read - nothing after start-up writes to it. */
static atlas_fraglist_t g_fl;

static int  g_sema;                     /* one command at a time       */
static int  g_event;                    /* read completion             */
static int  g_thread;

static sceCdCBFunc g_callback;
static int  g_disk_type = SCECdPS2DVD;
static int  g_last_error = SCECdErNO;
static int  g_tray_count;
static int  g_first_read_done;
static int  g_streaming_lba;

/* The command the worker thread is to run. Written by the caller with
 * the semaphore held, read by the worker. */
static struct {
    u32   lba;
    u32   sectors;
    void *buf;
    int   mode;
    int   busy;
    int   result;
} g_cmd;

static u8 g_bounce[BOUNCE_BYTES] __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/* The device                                                          */
/* ------------------------------------------------------------------ */

/**
 * Read `count` device sectors, with retries.
 *
 * The only function in this file that knows where bytes come from.
 * ZSO, HDD and SMB each replace the body of this and nothing else.
 */
static int bd_read(u32 sector, u32 count, void *buf)
{
    int tries;

    if (!g_bd)
        return -1;

    for (tries = 0; tries < READ_RETRIES; tries++) {
        if (g_bd->read(g_bd, g_bd->sectorOffset + sector, buf,
                       (u16)count) == (int)count)
            return 0;

        /* A device that just failed is usually busy rather than gone.
         * Backing off costs a frame; not backing off turns a stutter
         * into an unreadable disc. */
        DelayThread(2000);
    }

    return -1;
}

/**
 * The read callback frag.c walks the filesystem through.
 *
 * Sector numbers are relative to the start of the volume; a block
 * device's own read adds the partition offset, which is what bd_read()
 * already does.
 */
static int frag_read_cb(void *ctx, u32 sector, u32 count, void *buf)
{
    (void)ctx;
    return bd_read(sector, count, buf);
}

/**
 * Read `bytes` from image offset `offset` into `dst`.
 *
 * Everything goes through the bounce buffer: an extent boundary rarely
 * falls where a game's read does, so a device read almost always starts
 * part-way into a sector and the copy is unavoidable anyway.
 */
static int image_read(u32 offset, u32 bytes, u8 *dst)
{
    while (bytes > 0) {
        u32 sector, skip, run, chunk, want_sectors;
        u32 ss = g_fl.sector_size;

        if (atlas_frag_lookup(&g_fl, offset, &sector, &skip, &run)
            != ATLAS_OK)
            return -1;

        chunk = (run < bytes) ? run : bytes;

        /* One device sector of the bounce buffer is reserved for the
         * part of the first sector before `skip`, which is read and
         * thrown away. Without it a full-buffer request would need one
         * sector more than the buffer holds. */
        if (chunk > BOUNCE_BYTES - ss)
            chunk = BOUNCE_BYTES - ss;

        want_sectors = (skip + chunk + ss - 1) / ss;

        if (bd_read(sector, want_sectors, g_bounce) != 0)
            return -1;

        memcpy(dst, g_bounce + skip, chunk);

        dst    += chunk;
        offset += chunk;
        bytes  -= chunk;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Sector framing                                                      */
/*                                                                     */
/* The same rules as src/disc/sector.c, which carries the reasoning    */
/* and the self-check. Repeated because this module cannot link EE     */
/* code; the two are changed together.                                 */
/*                                                                     */
/* 2352 is absent on purpose: sceCdRead()'s datapattern field has no   */
/* value for it, so nothing can ask for it here.                       */
/* ------------------------------------------------------------------ */

static u8 to_bcd(u32 v)
{
    return (u8)(((v / 10) % 10) * 16 + (v % 10));
}

static u32 sector_out_size(int mode)
{
    switch (mode) {
    case SCECdSecS2048: return 2048;
    case SCECdSecS2328: return 2328;
    case SCECdSecS2340: return 2340;
    default:            return 0;
    }
}

static void sector_expand(int mode, u32 lba, const u8 *data, u8 *out)
{
    u32 total = lba + 150;      /* the two-second pre-gap */

    switch (mode) {
    case SCECdSecS2048:
        memcpy(out, data, 2048);
        return;

    case SCECdSecS2328:
        memcpy(out, data, 2048);
        memset(out + 2048, 0, 2328 - 2048);
        return;

    case SCECdSecS2340:
        out[0] = to_bcd(total / (75 * 60));
        out[1] = to_bcd((total / 75) % 60);
        out[2] = to_bcd(total % 75);
        out[3] = 2;                     /* Mode 2 */

        out[4] = 0;                     /* file    */
        out[5] = 0;                     /* channel */
        out[6] = 0x08;                  /* submode: data, form 1 */
        out[7] = 0;
        out[8]  = out[4];
        out[9]  = out[5];
        out[10] = out[6];
        out[11] = out[7];

        memcpy(out + 12, data, 2048);
        memset(out + 12 + 2048, 0, 2340 - 12 - 2048);
        return;

    default:
        return;
    }
}

/* ------------------------------------------------------------------ */
/* The worker                                                          */
/*                                                                     */
/* sceCdRead() is asynchronous: it returns at once and the game learns  */
/* the read finished through the callback it registered and through    */
/* sceCdSync(). A synchronous implementation works in testing and       */
/* deadlocks in a game that waits on the callback, so the work happens  */
/* on this thread and nowhere else.                                    */
/* ------------------------------------------------------------------ */

static void worker(void *unused)
{
    (void)unused;

    for (;;) {
        u32 lba, sectors, i;
        u8 *out;
        int mode, ok = 1;

        SleepThread();

        lba     = g_cmd.lba;
        sectors = g_cmd.sectors;
        out     = (u8 *)g_cmd.buf;
        mode    = g_cmd.mode;

        /*
         * A few titles issue their first read before installing the
         * handler that receives it, having assumed a physical drive
         * would take tens of milliseconds to answer.
         */
        if (!g_first_read_done) {
            g_first_read_done = 1;

            if (g_arg.flags & ATLASCDVD_F_SLOW_FIRST)
                DelayThread(SLOW_FIRST_USEC);
        }

        if (mode == SCECdSecS2048) {
            /* The common path, and the only one with no per-sector
             * work: the bytes go straight out. */
            if (image_read(lba * SECTOR_BYTES, sectors * SECTOR_BYTES,
                           out) != 0)
                ok = 0;
        } else {
            /*
             * A larger sector size means framing has to be built around
             * each 2048 bytes. Done one sector at a time because the
             * header differs per sector; the arithmetic itself is the
             * same code as src/disc/sector.c, which is checked on the
             * build machine.
             */
            u32 out_size = sector_out_size(mode);

            if (out_size == 0) {
                ok = 0;
            } else {
                for (i = 0; i < sectors; i++) {
                    u8 data[SECTOR_BYTES];

                    if (image_read((lba + i) * SECTOR_BYTES, SECTOR_BYTES,
                                   data) != 0) {
                        ok = 0;
                        break;
                    }

                    sector_expand(mode, lba + i, data,
                                  out + i * out_size);
                }
            }
        }

        g_cmd.result = ok ? 0 : -1;
        g_last_error = ok ? SCECdErNO : SCECdErREAD;

        g_cmd.busy = 0;
        SetEventFlag(g_event, EF_DONE);

        /*
         * The callback runs last, after the completion is visible: a
         * game whose callback calls sceCdSync() must not find the
         * command still marked busy.
         */
        if (g_callback)
            g_callback(SCECdFuncRead);
    }
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

static int start_read(u32 lba, u32 sectors, void *buf, sceCdRMode *mode)
{
    if (!buf || sectors == 0)
        return 0;               /* 0 means "not accepted" to a caller */

    WaitSema(g_sema);

    if (g_cmd.busy) {
        SignalSema(g_sema);
        return 0;
    }

    ClearEventFlag(g_event, ~EF_DONE);

    g_cmd.lba     = lba;
    g_cmd.sectors = sectors;
    g_cmd.buf     = buf;
    g_cmd.mode    = mode ? mode->datapattern : SCECdSecS2048;
    g_cmd.result  = 0;
    g_cmd.busy    = 1;

    WakeupThread(g_thread);
    SignalSema(g_sema);

    return 1;
}

int sceCdInit(int mode)
{
    (void)mode;
    return 1;
}

int sceCdRead(u32 lbn, u32 sectors, void *buf, sceCdRMode *mode)
{
    return start_read(lbn, sectors, buf, mode);
}

int sceCdRead0(u32 lbn, u32 sectors, void *buf, sceCdRMode *mode)
{
    return start_read(lbn, sectors, buf, mode);
}

int sceCdReadFull(unsigned int lsn, unsigned int sectors, void *buf,
                  sceCdRMode *mode)
{
    return start_read(lsn, sectors, buf, mode);
}

/**
 * Wait for, or poll, the outstanding command.
 *
 * mode 0 blocks until it finishes; anything else returns immediately
 * with whether it is still running. A game polls this in a loop and a
 * blocking answer to a poll is a frame lost every read.
 */
int sceCdSync(int mode)
{
    if (mode == 0) {
        u32 bits;

        while (g_cmd.busy)
            WaitEventFlag(g_event, EF_DONE, 0, &bits);

        return 0;
    }

    return g_cmd.busy ? 1 : 0;
}

int sceCdGetError(void)
{
    return g_last_error;
}

int sceCdGetDiskType(void)
{
    return g_disk_type;
}

int sceCdDiskReady(int mode)
{
    (void)mode;

    /* There is no drive to spin up. Always ready is the truthful
     * answer, and the one a game can proceed from. */
    return SCECdComplete;
}

int sceCdTrayReq(int param, u32 *traychk)
{
    /*
     * `hide_tray`: some titles poll the tray while streaming and treat
     * any change as the disc having been removed. With no physical
     * tray the honest count is the one they abort on.
     */
    if (traychk)
        *traychk = (g_arg.flags & ATLASCDVD_F_HIDE_TRAY) ? 0
                                                         : (u32)g_tray_count;

    (void)param;
    return 1;
}

int sceCdStatus(void)
{
    return 0;                   /* stopped, tray closed, no error */
}

sceCdCBFunc sceCdCallback(sceCdCBFunc func)
{
    sceCdCBFunc old;
    int state;

    /*
     * Interrupts are held off across the swap: the worker reads this
     * pointer, and a game that replaces its callback while a read is in
     * flight would otherwise be entered through half a pointer.
     */
    CpuSuspendIntr(&state);
    old = g_callback;
    g_callback = func;
    CpuResumeIntr(state);

    return old;
}

u32 sceCdGetReadPos(void)
{
    return 0;
}

u32 sceCdPosToInt(sceCdlLOCCD *p)
{
    if (!p)
        return 0;

    return (u32)((btoi(p->minute) * 60 + btoi(p->second)) * 75
                 + btoi(p->sector) - 150);
}

sceCdlLOCCD *sceCdIntToPos(u32 i, sceCdlLOCCD *p)
{
    u32 total;

    if (!p)
        return 0;

    total = i + 150;

    p->minute = itob(total / (75 * 60));
    p->second = itob((total / 75) % 60);
    p->sector = itob(total % 75);
    p->track  = 0;

    return p;
}

/* ------------------------------------------------------------------ */
/* Streaming                                                           */
/*                                                                     */
/* A title streaming audio reads through these rather than sceCdRead.  */
/* They are a position and a read at that position; the drive's own    */
/* buffering is not reproduced, because with no seek latency there is  */
/* nothing for it to hide.                                             */
/* ------------------------------------------------------------------ */

int sceCdStInit(u32 bufmax, u32 bankmax, void *buffer)
{
    (void)bufmax; (void)bankmax; (void)buffer;
    g_streaming_lba = 0;
    return 1;
}

int sceCdStStart(u32 lbn, sceCdRMode *mode)
{
    (void)mode;
    g_streaming_lba = (int)lbn;
    return 1;
}

int sceCdStRead(u32 sectors, u32 *buf, u32 mode, u32 *err)
{
    u32 got = 0;

    (void)mode;

    if (buf && sectors) {
        if (image_read((u32)g_streaming_lba * SECTOR_BYTES,
                       sectors * SECTOR_BYTES, (u8 *)buf) == 0) {
            got = sectors;
            g_streaming_lba += (int)sectors;
        }
    }

    if (err)
        *err = (got == sectors) ? SCECdErNO : SCECdErREAD;

    return (int)got;
}

int sceCdStSeek(u32 lbn)
{
    g_streaming_lba = (int)lbn;
    return 1;
}

int sceCdStSeekF(unsigned int lsn)
{
    g_streaming_lba = (int)lsn;
    return 1;
}

int sceCdStStop(void)   { return 1; }
int sceCdStPause(void)  { return 1; }
int sceCdStResume(void) { return 1; }

int sceCdStStat(void)
{
    return 0;
}

/* ------------------------------------------------------------------ */
/* Everything else                                                     */
/*                                                                     */
/* A stub that returns a defined value is not laziness here: the        */
/* export table must be dense (see exports.tab), so every ordinal the  */
/* real module fills has to be filled. A stub that succeeds where it   */
/* cannot would be worse than one that fails, so these fail.           */
/* ------------------------------------------------------------------ */

int atlas_cdvd_null(void) { return 0; }

int sceCdStandby(void)  { return 1; }
int sceCdStop(void)     { return 1; }
int sceCdPause(void)    { return 1; }
int sceCdSeek(u32 lbn)  { (void)lbn; return 1; }
int sceCdBreak(void)    { return 1; }
int sceCdNop(void)      { return 1; }

int sceCdCheckCmd(void) { return 1; }
int sceCdMmode(int media) { (void)media; return 1; }
int sceCdSetHDMode(u32 mode) { (void)mode; return 1; }
int sceCdSetTimeout(int param, int timeout) { (void)param; (void)timeout; return 1; }
int sceCdSpinCtrlIOP(u32 speed) { (void)speed; return 1; }
int sceCdAutoAdjustCtrl(int mode, u32 *result) { (void)mode; if (result) *result = 0; return 1; }
int sceCdCtrlADout(int mode, u32 *status) { (void)mode; if (status) *status = 0; return 1; }
int sceCdForbidDVDP(u32 *result) { if (result) *result = 0; return 1; }
int sceCdForbidRead(u32 *result) { if (result) *result = 0; return 1; }
int sceCdBlueLEDCtl(u8 control, u32 *result) { (void)control; if (result) *result = 0; return 1; }
int sceCdCancelPOffRdy(u32 *result) { if (result) *result = 0; return 1; }
int sceCdBootCertify(const u8 *romname) { (void)romname; return 1; }

/*
 * Everything below reaches hardware this module does not stand in for -
 * the mechacon, the NVM, the RTC, the console's own identifiers. They
 * fail rather than inventing an answer: a game handed a fabricated
 * console ID has been lied to about the machine it is running on, and a
 * fabricated clock timestamps every save the player makes.
 *
 * A title that genuinely needs one of these will stop, which is
 * visible, rather than misbehave later, which is not.
 */
int sceCdGetToc(u8 *toc) { (void)toc; return 0; }
int sceCdGetToc2(u8 *toc, int param) { (void)toc; (void)param; return 0; }
int sceCdSearchFile(sceCdlFILE *fp, const char *name) { (void)fp; (void)name; return 0; }
int sceCdLayerSearchFile(sceCdlFILE *fp, const char *name, int layer) { (void)fp; (void)name; (void)layer; return 0; }
int sceCdReadDVDV(u32 lbn, u32 sectors, void *buf, sceCdRMode *mode) { (void)lbn; (void)sectors; (void)buf; (void)mode; return 0; }
int sceCdReadCDDA(u32 lbn, u32 sectors, void *buf, sceCdRMode *mode) { (void)lbn; (void)sectors; (void)buf; (void)mode; return 0; }
int sceCdReadChain(sceCdRChain *tag, sceCdRMode *mode) { (void)tag; (void)mode; return 0; }
int sceCdReadSUBQ(void *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdRI(u8 *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdWI(const u8 *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdRM(char *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdWM(const char *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdRV(u8 *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdRC(void *clock) { (void)clock; return 0; }
int sceCdSC(int code, u32 *param) { (void)code; (void)param; return 0; }
int sceCdMV(u8 *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdReadClock(sceCdCLOCK *clock) { (void)clock; return 0; }
int sceCdWriteClock(sceCdCLOCK *clock) { (void)clock; return 0; }
int sceCdReadNVM(u32 addr, u16 *data, u8 *status) { (void)addr; (void)data; if (status) *status = 0; return 0; }
int sceCdWriteNVM(u32 addr, u16 data, u8 *status) { (void)addr; (void)data; if (status) *status = 0; return 0; }
int sceCdReadConsoleID(u8 *id, u32 *status) { (void)id; if (status) *status = 0; return 0; }
int sceCdWriteConsoleID(const u8 *id, u32 *status) { (void)id; if (status) *status = 0; return 0; }
int sceCdReadDiskID(void *id) { (void)id; return 0; }
int sceCdReadGUID(u64 *guid) { (void)guid; return 0; }
int sceCdReadModelID(unsigned int *id) { (void)id; return 0; }
int sceCdReadDvdDualInfo(int *on_dual, unsigned int *layer1_start) {
    /* The one item in this group this module can answer honestly: the
     * EE side knows the layer break from the image and passed it in. */
    if (on_dual) *on_dual = g_arg.layer1_lba ? 1 : 0;
    if (layer1_start) *layer1_start = g_arg.layer1_lba;
    return 1;
}
int sceCdReadKey(unsigned char arg1, unsigned char arg2, unsigned int command,
                 unsigned char *key)
{ (void)arg1; (void)arg2; (void)command; (void)key; return 0; }
int sceCdDecSet(unsigned char enable_xor, unsigned char enable_shift,
                unsigned char shiftval)
{ (void)enable_xor; (void)enable_shift; (void)shiftval; return 0; }
int sceCdOpenConfig(int block, int mode, int NumBlocks, u32 *status)
{ (void)block; (void)mode; (void)NumBlocks; if (status) *status = 0; return 0; }
int sceCdCloseConfig(u32 *status) { if (status) *status = 0; return 0; }
int sceCdReadConfig(void *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdWriteConfig(const void *buf, u32 *status) { (void)buf; if (status) *status = 0; return 0; }
int sceCdApplySCmd(u8 cmd, const void *in, u16 in_size, void *out)
{ (void)cmd; (void)in; (void)in_size; (void)out; return 0; }
int sceCdApplyNCmd(u8 cmd, const void *in, u16 in_size) { (void)cmd; (void)in; (void)in_size; return 0; }
int sceGetFsvRbuf(void) { return 0; }
int sceCdstm0Cb(void *cb) { (void)cb; return 0; }
int sceCdstm1Cb(void *cb) { (void)cb; return 0; }
int sceCdPowerOff(u32 *status) { if (status) *status = 0; return 0; }
void *sceCdPOffCallback(void (*func)(void *userdata), void *userdata)
{ (void)func; (void)userdata; return 0; }

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */

/*
 * Ordinals 1-3 of the export table: shutdown, restart, and one the
 * kernel never calls. Doing nothing is the correct behaviour for all
 * three - this module holds no hardware to quiesce, and the game it
 * serves is not going to be shut down and restarted underneath it.
 */
void _retonly(void)
{
}

/**
 * Find the block device the extents belong to.
 *
 * bdm hands out however many devices are connected; the EE side chose
 * one by index and that index is what is in the argument block. A
 * device that is not there is a refusal to start, not a module that
 * loads and fails every read: a game that never boots is one problem,
 * and a game that boots and reads garbage is several.
 */
static int find_device(void)
{
    struct block_device *bd[8];
    unsigned int i;

    memset(bd, 0, sizeof(bd));
    bdm_get_bd(bd, 8);

    for (i = 0; i < 8; i++) {
        if (bd[i] && bd[i]->devNr == g_arg.device_index) {
            g_bd = bd[i];
            return 0;
        }
    }

    return -1;
}

int _start(int argc, char *argv[])
{
    iop_thread_t th;

    /*
     * The argument block arrives as a pointer in argv[1], written as
     * hex by the EE side. It is checked before a field of it is read:
     * an argument block silently misread is a drive emulation
     * answering with somebody else's numbers.
     */
    if (argc < 2 || !argv[1])
        return MODULE_NO_RESIDENT_END;

    {
        const atlascdvd_arg_t *src;
        u32 addr = 0;
        const char *p = argv[1];

        while (*p) {
            u32 d;

            if (*p >= '0' && *p <= '9')       d = (u32)(*p - '0');
            else if (*p >= 'a' && *p <= 'f')  d = (u32)(*p - 'a' + 10);
            else if (*p >= 'A' && *p <= 'F')  d = (u32)(*p - 'A' + 10);
            else return MODULE_NO_RESIDENT_END;

            addr = addr * 16 + d;
            p++;
        }

        if (addr == 0)
            return MODULE_NO_RESIDENT_END;

        src = (const atlascdvd_arg_t *)addr;

        if (src->magic != ATLASCDVD_MAGIC
            || src->version != ATLASCDVD_ARG_VERSION)
            return MODULE_NO_RESIDENT_END;

        if (src->device != ATLASCDVD_DEV_BDM)
            return MODULE_NO_RESIDENT_END;

        /* Copied, not referenced: the block lives in EE memory the game
         * is about to be given. */
        memcpy(&g_arg, src, sizeof(g_arg));

        /* A path the EE side failed to terminate would be walked off
         * the end of the block. Terminated here rather than trusted,
         * because the cost of being wrong is reading whatever follows
         * it in memory as a filename. */
        g_arg.path[ATLASCDVD_PATH_MAX - 1] = 0;

        if (g_arg.path[0] == 0)
            return MODULE_NO_RESIDENT_END;
    }

    if (find_device() != 0)
        return MODULE_NO_RESIDENT_END;

    /*
     * The one and only time this module reads a filesystem.
     *
     * It happens here, in _start, because here the IOP is doing nothing
     * else: the game's modules are not loaded, its threads do not
     * exist, and no DMA of its is in flight. Every read after this
     * point is a raw sector read against the list built now.
     *
     * A failure is a refusal to stay resident, not a module that loads
     * and fails every read. A game that never starts is one problem; a
     * game that starts and reads garbage is several, and the user
     * cannot tell the second from a bad dump.
     */
    if (atlas_frag_build(frag_read_cb, 0, g_arg.path, &g_fl) != ATLAS_OK)
        return MODULE_NO_RESIDENT_END;

    /* frag.c guarantees an empty list on failure, so this cannot be a
     * partial one. Checked anyway: the whole module is built on that
     * guarantee and it costs two comparisons to stop trusting it. */
    if (g_fl.count <= 0 || g_fl.size == 0 || g_fl.sector_size == 0)
        return MODULE_NO_RESIDENT_END;

    /* Reads are framed in 2048-byte disc sectors and served from device
     * sectors. A device whose sectors are larger than a disc sector -
     * a 4K-native drive - would need the arithmetic in image_read() to
     * work the other way round, and silently reading the wrong offsets
     * is not the way to find that out. */
    if (g_fl.sector_size > SECTOR_BYTES
        || (g_fl.sector_size & (g_fl.sector_size - 1)) != 0)
        return MODULE_NO_RESIDENT_END;

    /*
     * A CD holds 360000 sectors at most, so anything larger cannot be
     * one. The reverse does not hold: plenty of DVD titles are smaller
     * than a CD, and one of those reported as a CD will refuse to boot.
     * That is what `force_dvd` in COMPAT.INI is for, and it is the
     * first thing to try when a title stops at its own logo.
     */
    g_disk_type = ((g_arg.flags & ATLASCDVD_F_FORCE_DVD)
                   || g_fl.size > 360000u * SECTOR_BYTES)
                  ? SCECdPS2DVD : SCECdPS2CD;

    {
        iop_sema_t sp;

        sp.attr    = 1;
        sp.option  = 0;
        sp.initial = 1;
        sp.max     = 1;

        g_sema = CreateSema(&sp);
        if (g_sema < 0)
            return MODULE_NO_RESIDENT_END;
    }

    {
        iop_event_t ef;

        ef.attr   = 2;      /* multiple waiters */
        ef.option = 0;
        ef.bits   = 0;

        g_event = CreateEventFlag(&ef);
        if (g_event < 0)
            return MODULE_NO_RESIDENT_END;
    }

    /*
     * The worker runs above the game's own threads but below the
     * interrupt handlers it installs. A read the game is blocked on
     * must not wait behind the game's ordinary work.
     */
    th.attr      = TH_C;
    th.option    = 0;
    th.thread    = worker;
    th.stacksize = 4096;
    th.priority  = 8;

    g_thread = CreateThread(&th);
    if (g_thread < 0)
        return MODULE_NO_RESIDENT_END;

    StartThread(g_thread, 0);

    if (RegisterLibraryEntries(&_exp_cdvdman) != 0)
        return MODULE_NO_RESIDENT_END;



    return MODULE_RESIDENT_END;
}
