/*
 * AtlasPS2 - test_frag.c
 *
 * The extent list a file is reduced to before a game starts.
 *
 * WHY THIS CHECK IS BUILT THE WAY IT IS
 * -------------------------------------
 * An extent that points one cluster short does not fail. It returns
 * real bytes, from somewhere else on the card, and the game reads them
 * as its own data - which is exactly the failure mode the ISO/ZSO
 * comparison in test_image exists to catch, and it is caught the same
 * way here.
 *
 * So the volumes below are built byte by byte with a known pattern per
 * file, and every check reads the file *through the extent list* and
 * compares against the pattern. A list that is plausible but wrong
 * cannot pass that, whereas it would pass any assertion about the
 * numbers in it.
 *
 * The fragmented cases are the ones that matter: a file laid down in
 * one piece works with almost any chain walk. The interleaved case
 * below allocates two files alternately, so every cluster of each has a
 * different file's cluster after it on the device.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas/frag.h"

#define SECSZ 512

/* ------------------------------------------------------------------ */
/* A device made of memory                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *data;
    u32            sectors;
    u32            fail_at;     /* sector whose read fails, or ~0 */
    int            reads;
} dev_t;

static int dev_read(void *ctx, u32 sector, u32 count, void *buf)
{
    dev_t *d = (dev_t *)ctx;

    d->reads++;

    if (sector + count > d->sectors || sector + count < sector)
        return -1;

    if (d->fail_at != 0xFFFFFFFFU
        && sector <= d->fail_at && d->fail_at < sector + count)
        return -1;

    memcpy(buf, d->data + (size_t)sector * SECSZ, (size_t)count * SECSZ);
    return 0;
}

/* ------------------------------------------------------------------ */
/* A FAT volume, built here                                            */
/*                                                                     */
/* Small on purpose: a FAT16 volume needs fewer than 65525 clusters and */
/* a FAT32 one needs more, and both are built at the smallest size that */
/* lands on the correct side of that boundary, so the checks stay fast. */
/* ------------------------------------------------------------------ */

typedef struct {
    dev_t dev;

    int  fat32;
    u32  cluster_sectors;
    u32  reserved;
    u32  fat_sectors;
    u32  root_entries;      /* FAT16 */
    u32  root_cluster;      /* FAT32 */
    u32  data_start;
    u32  cluster_count;

    u32  next_free;         /* next cluster to hand out */
} vol_t;

static void put16(unsigned char *p, u16 v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, u32 v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned char *sec(vol_t *v, u32 n)
{
    assert(n < v->dev.sectors);
    return v->dev.data + (size_t)n * SECSZ;
}

/* The FAT entry for a cluster, written into every copy of the FAT.
 * Only one copy is built here; num_fats is 1 in the boot sector. */
static void fat_set(vol_t *v, u32 cluster, u32 value)
{
    u32 off = v->fat32 ? cluster * 4 : cluster * 2;
    unsigned char *p;

    assert(off / SECSZ < v->fat_sectors);
    p = sec(v, v->reserved + off / SECSZ) + off % SECSZ;

    if (v->fat32)
        put32(p, value);
    else
        put16(p, (u16)value);
}

static u32 cluster_sector(vol_t *v, u32 cluster)
{
    return v->data_start + (cluster - 2) * v->cluster_sectors;
}

static void vol_init_n(vol_t *v, int fat32, u32 cluster_sectors,
                       u32 cluster_count)
{
    unsigned char *boot;
    u32 root_sectors, total;

    memset(v, 0, sizeof(*v));

    v->fat32           = fat32;
    v->cluster_sectors = cluster_sectors;
    v->reserved        = fat32 ? 32 : 1;
    v->cluster_count   = cluster_count;

    v->fat_sectors = ((v->cluster_count + 2) * (fat32 ? 4 : 2) + SECSZ - 1)
                     / SECSZ;

    if (fat32) {
        v->root_entries = 0;
        v->root_cluster = 2;
        root_sectors    = 0;
    } else {
        v->root_entries = 512;
        v->root_cluster = 0;
        root_sectors    = v->root_entries * 32 / SECSZ;
    }

    v->data_start = v->reserved + v->fat_sectors + root_sectors;
    total         = v->data_start + v->cluster_count * cluster_sectors;

    v->dev.sectors = total;
    v->dev.data    = (unsigned char *)calloc(total, SECSZ);
    v->dev.fail_at = 0xFFFFFFFFU;
    assert(v->dev.data != NULL);

    boot = sec(v, 0);
    put16(boot + 11, SECSZ);
    boot[13] = (unsigned char)cluster_sectors;
    put16(boot + 14, (u16)v->reserved);
    boot[16] = 1;                                   /* one FAT */
    put16(boot + 17, (u16)v->root_entries);

    if (total <= 0xFFFF)
        put16(boot + 19, (u16)total);
    else
        put32(boot + 32, total);

    if (fat32) {
        put32(boot + 36, v->fat_sectors);
        put32(boot + 44, v->root_cluster);
    } else {
        put16(boot + 22, (u16)v->fat_sectors);
    }

    /* Cluster 0 and 1 are reserved and never part of a chain. */
    fat_set(v, 0, v->fat32 ? 0x0FFFFFF8U : 0xFFF8U);
    fat_set(v, 1, v->fat32 ? 0x0FFFFFFFU : 0xFFFFU);

    v->next_free = 2;

    if (fat32) {
        /* The root directory is an ordinary chain on FAT32, one
         * cluster long here. */
        assert(v->next_free == v->root_cluster);
        v->next_free++;
        fat_set(v, v->root_cluster, 0x0FFFFFF8U);
    }
}

/*
 * The everyday volumes: comfortably on their own side of the line, so
 * the checks that are about files are not also about the boundary.
 * check_boundary() below is the one that is about the boundary.
 */
static void vol_init(vol_t *v, int fat32, u32 cluster_sectors)
{
    vol_init_n(v, fat32, cluster_sectors, fat32 ? 65600 : 4000);
}

static void vol_free(vol_t *v)
{
    free(v->dev.data);
    v->dev.data = NULL;
}

/* ------------------------------------------------------------------ */
/* Directory entries                                                   */
/* ------------------------------------------------------------------ */

/* Where the next entry goes, in a directory that is one cluster (or the
 * FAT16 fixed root). */
typedef struct {
    u32 cluster;        /* 0 = FAT16 root */
    int used;
} dir_t;

static unsigned char *dir_slot(vol_t *v, dir_t *d)
{
    u32 base, off = (u32)d->used * 32;

    if (d->cluster == 0 && !v->fat32)
        base = v->reserved + v->fat_sectors;
    else
        base = cluster_sector(v, d->cluster);

    d->used++;
    return sec(v, base + off / SECSZ) + off % SECSZ;
}

static void name_83(unsigned char *e, const char *base, const char *ext)
{
    int i;

    for (i = 0; i < 11; i++)
        e[i] = ' ';

    for (i = 0; base[i] && i < 8; i++)
        e[i] = (unsigned char)base[i];

    for (i = 0; ext[i] && i < 3; i++)
        e[8 + i] = (unsigned char)ext[i];
}

static void entry_set(unsigned char *e, u32 cluster, u32 size,
                      unsigned char attr)
{
    e[11] = attr;
    put16(e + 20, (u16)(cluster >> 16));
    put16(e + 26, (u16)(cluster & 0xFFFF));
    put32(e + 28, size);
}

/**
 * Write a long-name run followed by its 8.3 entry.
 *
 * The checksum byte is what a real driver uses to bind the two, and it
 * is written correctly here even though the reader does not check it:
 * a fixture that is wrong in a way the code under test happens to
 * ignore is a trap for whoever tightens the code later.
 */
static void add_lfn(vol_t *v, dir_t *d, const char *lname,
                    const char *b83, const char *e83,
                    u32 cluster, u32 size, unsigned char attr)
{
    static const int k_off[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24,
                                   28, 30 };
    unsigned char short_name[11];
    unsigned char sum = 0;
    int len = (int)strlen(lname);
    int parts = (len + 12) / 13;
    int i, p;

    {
        int k;
        for (k = 0; k < 11; k++)
            short_name[k] = ' ';
        for (k = 0; b83[k] && k < 8; k++)
            short_name[k] = (unsigned char)b83[k];
        for (k = 0; e83[k] && k < 3; k++)
            short_name[8 + k] = (unsigned char)e83[k];
    }

    for (i = 0; i < 11; i++)
        sum = (unsigned char)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i]);

    /* Written last-fragment-first, the way a real driver lays them
     * down, so the reader's prepending is actually exercised. */
    for (p = parts; p >= 1; p--) {
        unsigned char *e = dir_slot(v, d);
        int base = (p - 1) * 13;
        int k;

        memset(e, 0, 32);
        e[0] = (unsigned char)((p == parts) ? (0x40 | p) : p);
        e[11] = 0x0F;
        e[13] = sum;

        for (k = 0; k < 13; k++) {
            int idx = base + k;

            if (idx < len)
                put16(e + k_off[k], (u16)(unsigned char)lname[idx]);
            else if (idx == len)
                put16(e + k_off[k], 0x0000);
            else
                put16(e + k_off[k], 0xFFFF);
        }
    }

    {
        unsigned char *e = dir_slot(v, d);

        memset(e, 0, 32);
        memcpy(e, short_name, 11);
        entry_set(e, cluster, size, attr);
    }
}

/* ------------------------------------------------------------------ */
/* Files                                                               */
/* ------------------------------------------------------------------ */

/**
 * Allocate `clusters` clusters, filling each with a byte derived from
 * the file's tag and the cluster's index within the file.
 *
 * `stride` is how many clusters to skip between allocations: 1 lays the
 * file down contiguously, 2 interleaves it with whatever else is being
 * allocated at the same time.
 */
static u32 file_write(vol_t *v, u32 clusters, unsigned char tag, u32 stride)
{
    u32 first = 0, prev = 0;
    u32 i;

    for (i = 0; i < clusters; i++) {
        u32 c = v->next_free;
        u32 s = cluster_sector(v, c);
        u32 k;

        v->next_free += stride;
        assert(v->next_free <= v->cluster_count + 2);

        for (k = 0; k < v->cluster_sectors; k++) {
            unsigned char *p = sec(v, s + k);
            int j;

            /* Every byte differs between files and between positions,
             * so a run taken from the wrong place cannot match. */
            for (j = 0; j < SECSZ; j++)
                p[j] = (unsigned char)(tag + i * 7 + k * 3 + j);
        }

        if (first == 0)
            first = c;
        else
            fat_set(v, prev, c);

        prev = c;
    }

    fat_set(v, prev, v->fat32 ? 0x0FFFFFFFU : 0xFFFFU);
    return first;
}

/** The byte the pattern above puts at `offset` of the file. */
static unsigned char pattern_at(vol_t *v, unsigned char tag, u32 offset)
{
    u32 cluster_bytes = v->cluster_sectors * SECSZ;
    u32 i = offset / cluster_bytes;
    u32 within = offset % cluster_bytes;
    u32 k = within / SECSZ;
    u32 j = within % SECSZ;

    return (unsigned char)(tag + i * 7 + k * 3 + j);
}

/* ------------------------------------------------------------------ */
/* Reading a file back through its extent list                         */
/* ------------------------------------------------------------------ */

/**
 * Compare every byte of the file against the pattern, reading only
 * through the extent list.
 *
 * This is the check that matters. It walks in irregular strides so that
 * the lookup is exercised at offsets that are not sector-aligned and
 * not run-aligned - a list that is right only at run boundaries fails
 * here and passes anything simpler.
 */
static void verify(vol_t *v, const atlas_fraglist_t *fl, unsigned char tag,
                   u32 size)
{
    static const u32 k_step[] = { 1, 511, 512, 513, 1024, 4096, 7919 };
    unsigned char buf[SECSZ];
    u32 s;

    assert(fl->size == size);
    assert(fl->sector_size == SECSZ);

    for (s = 0; s < sizeof(k_step) / sizeof(k_step[0]); s++) {
        u32 off;

        for (off = 0; off < size; off += k_step[s]) {
            u32 sector, skip, run;

            assert(atlas_frag_lookup(fl, off, &sector, &skip, &run)
                   == ATLAS_OK);

            assert(skip < SECSZ);
            assert(run > 0);
            assert(run <= size - off);

            assert(dev_read(&v->dev, sector, 1, buf) == 0);
            assert(buf[skip] == pattern_at(v, tag, off));
        }
    }

    /* The whole file, in the largest pieces the list allows - which is
     * how the drive emulation will actually read it. */
    {
        u32 off = 0;

        while (off < size) {
            u32 sector, skip, run, n;
            unsigned char big[65536];

            assert(atlas_frag_lookup(fl, off, &sector, &skip, &run)
                   == ATLAS_OK);

            n = run;
            if (n > sizeof(big) - SECSZ)
                n = sizeof(big) - SECSZ;

            {
                u32 sectors = (skip + n + SECSZ - 1) / SECSZ;
                u32 j;

                assert(dev_read(&v->dev, sector, sectors, big) == 0);

                for (j = 0; j < n; j++)
                    assert(big[skip + j] == pattern_at(v, tag, off + j));
            }

            off += n;
        }
    }

    /* Past the end is refused, never a silent zero: a game handed
     * zeroes reads them as a disc that is merely empty. */
    {
        u32 a, b, c;

        assert(atlas_frag_lookup(fl, size, &a, &b, &c) == ATLAS_EINVAL);
        assert(atlas_frag_lookup(fl, size + 1, &a, &b, &c) == ATLAS_EINVAL);
        assert(atlas_frag_lookup(fl, 0xFFFFFFFFU, &a, &b, &c) == ATLAS_EINVAL);
    }
}

/* ------------------------------------------------------------------ */
/* The checks                                                          */
/* ------------------------------------------------------------------ */

static void check_contiguous(int fat32)
{
    vol_t v;
    dir_t root;
    atlas_fraglist_t fl;
    u32 cluster, size;

    vol_init(&v, fat32, 4);

    root.cluster = fat32 ? v.root_cluster : 0;
    root.used    = 0;

    size    = 40 * 4 * SECSZ;           /* 40 clusters exactly */
    cluster = file_write(&v, 40, 0x11, 1);

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "DISC", "ISO");
        entry_set(e, cluster, size, 0);
    }

    assert(atlas_frag_build(dev_read, &v.dev, "/DISC.ISO", &fl) == ATLAS_OK);

    /* Laid down in one piece, so it must be exactly one run - not a
     * cosmetic point: the run count is what bounds the table, and a
     * reader that failed to coalesce would overflow on a real image. */
    assert(fl.count == 1);
    verify(&v, &fl, 0x11, size);

    /* Case and separators are both things a caller gets wrong. */
    assert(atlas_frag_build(dev_read, &v.dev, "/disc.iso", &fl) == ATLAS_OK);
    assert(fl.count == 1);
    assert(atlas_frag_build(dev_read, &v.dev, "\\DISC.ISO", &fl) == ATLAS_OK);
    assert(atlas_frag_build(dev_read, &v.dev, "DISC.ISO", &fl) == ATLAS_OK);

    assert(atlas_frag_build(dev_read, &v.dev, "/NOPE.ISO", &fl) == ATLAS_ENOENT);
    assert(atlas_frag_build(dev_read, &v.dev, "/", &fl) == ATLAS_ENOENT);
    assert(atlas_frag_build(dev_read, &v.dev, "", &fl) == ATLAS_ENOENT);

    /* A path that walks through a file is not a path to anything. */
    assert(atlas_frag_build(dev_read, &v.dev, "/DISC.ISO/X", &fl)
           == ATLAS_ENOENT);

    vol_free(&v);
}

static void check_fragmented(int fat32)
{
    vol_t v;
    dir_t root;
    atlas_fraglist_t fl;
    u32 c_a, c_b, size;

    vol_init(&v, fat32, 2);

    root.cluster = fat32 ? v.root_cluster : 0;
    root.used    = 0;

    /*
     * Two files allocated with stride 2 from adjacent starting points,
     * so their clusters alternate on the device: every cluster of A is
     * followed by a cluster of B. A chain walk that is off by one
     * returns B's data for A and passes anything that does not compare
     * the bytes.
     */
    size = 30 * 2 * SECSZ;

    c_a = file_write(&v, 30, 0x40, 2);
    v.next_free = c_a + 1;
    c_b = file_write(&v, 30, 0x90, 2);

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "A", "ISO");
        entry_set(e, c_a, size, 0);

        e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "B", "ISO");
        entry_set(e, c_b, size, 0);
    }

    assert(atlas_frag_build(dev_read, &v.dev, "/A.ISO", &fl) == ATLAS_OK);
    assert(fl.count == 30);         /* nothing consecutive to coalesce */
    verify(&v, &fl, 0x40, size);

    assert(atlas_frag_build(dev_read, &v.dev, "/B.ISO", &fl) == ATLAS_OK);
    assert(fl.count == 30);
    verify(&v, &fl, 0x90, size);

    vol_free(&v);
}

static void check_subdirectory(int fat32)
{
    vol_t v;
    dir_t root, sub;
    atlas_fraglist_t fl;
    u32 dir_cluster, file_cluster, size;

    vol_init(&v, fat32, 4);

    root.cluster = fat32 ? v.root_cluster : 0;
    root.used    = 0;

    /* One cluster for the subdirectory itself. */
    dir_cluster = v.next_free++;
    fat_set(&v, dir_cluster, v.fat32 ? 0x0FFFFFFFU : 0xFFFFU);

    size         = 17 * 4 * SECSZ;
    file_cluster = file_write(&v, 17, 0x55, 1);

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "GAMES", "");
        entry_set(e, dir_cluster, 0, 0x10);
    }

    sub.cluster = dir_cluster;
    sub.used    = 0;

    /* A long name, because that is what a user's file actually has, and
     * the 8.3 alias next to it is what a reader falls back to. */
    add_lfn(&v, &sub, "My Favourite Game.iso", "MYFAVO~1", "ISO",
            file_cluster, size, 0);

    assert(atlas_frag_build(dev_read, &v.dev,
                            "/GAMES/My Favourite Game.iso", &fl) == ATLAS_OK);
    assert(fl.count == 1);
    verify(&v, &fl, 0x55, size);

    /* The same file by its alias, and by a differently-cased long
     * name. Both are how it arrives from a caller. */
    assert(atlas_frag_build(dev_read, &v.dev, "/GAMES/MYFAVO~1.ISO", &fl)
           == ATLAS_OK);
    assert(fl.count == 1);

    assert(atlas_frag_build(dev_read, &v.dev,
                            "/games/MY FAVOURITE GAME.ISO", &fl) == ATLAS_OK);
    assert(fl.count == 1);

    /* A directory is not a file, even when the path is otherwise
     * right - a caller handed an extent list for a directory would
     * read directory entries as disc sectors. */
    assert(atlas_frag_build(dev_read, &v.dev, "/GAMES", &fl) == ATLAS_ENOENT);

    vol_free(&v);
}

static void check_partial_cluster(void)
{
    vol_t v;
    dir_t root;
    atlas_fraglist_t fl;
    u32 cluster, size;

    vol_init(&v, 0, 8);

    root.cluster = 0;
    root.used    = 0;

    /*
     * A size that stops partway through the last cluster, which is the
     * normal case for any real file. The slack after it belongs to
     * nobody, and a lookup must not offer it: a caller reading the run
     * it was promised would hand a game whatever the format left there.
     */
    cluster = file_write(&v, 5, 0x77, 1);
    size    = 4 * 8 * SECSZ + 300;

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "PARTIAL", "BIN");
        entry_set(e, cluster, size, 0);
    }

    assert(atlas_frag_build(dev_read, &v.dev, "/PARTIAL.BIN", &fl)
           == ATLAS_OK);
    assert(fl.size == size);
    verify(&v, &fl, 0x77, size);

    /* The last byte is inside the file and the one after it is not. */
    {
        u32 sector, skip, run;

        assert(atlas_frag_lookup(&fl, size - 1, &sector, &skip, &run)
               == ATLAS_OK);
        assert(run == 1);
        assert(atlas_frag_lookup(&fl, size, &sector, &skip, &run)
               == ATLAS_EINVAL);
    }

    vol_free(&v);
}

static void check_limits(void)
{
    vol_t v;
    dir_t root;
    atlas_fraglist_t fl;
    u32 cluster, size;

    /*
     * More runs than the table holds. This must be refused: a list cut
     * short reads whatever is at the sectors it does have and presents
     * it as the file, which is another file's data with nothing to say
     * so.
     */
    vol_init(&v, 1, 1);

    root.cluster = v.root_cluster;
    root.used    = 0;

    cluster = file_write(&v, ATLAS_FRAG_MAX + 10, 0x33, 2);
    size    = (ATLAS_FRAG_MAX + 10) * SECSZ;

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "HUGE", "ISO");
        entry_set(e, cluster, size, 0);
    }

    assert(atlas_frag_build(dev_read, &v.dev, "/HUGE.ISO", &fl)
           == ATLAS_ENOMEM);

    /* A file whose chain ends before its size accounts for. The card is
     * damaged; saying so beats a list that is quietly short. */
    {
        unsigned char *e = dir_slot(&v, &root);
        u32 c = file_write(&v, 4, 0x44, 1);

        memset(e, 0, 32);
        name_83(e, "SHORT", "ISO");
        entry_set(e, c, 100 * SECSZ, 0);

        assert(atlas_frag_build(dev_read, &v.dev, "/SHORT.ISO", &fl)
               == ATLAS_EFORMAT);
    }

    /* A chain pointing outside the volume. */
    {
        unsigned char *e = dir_slot(&v, &root);
        u32 c = file_write(&v, 2, 0x66, 1);

        fat_set(&v, c, v.cluster_count + 500);

        memset(e, 0, 32);
        name_83(e, "WILD", "ISO");
        entry_set(e, c, 4 * SECSZ, 0);

        assert(atlas_frag_build(dev_read, &v.dev, "/WILD.ISO", &fl)
               == ATLAS_EFORMAT);
    }

    /* A zero-length file has no data to point at. */
    {
        unsigned char *e = dir_slot(&v, &root);

        memset(e, 0, 32);
        name_83(e, "EMPTY", "BIN");
        entry_set(e, 0, 0, 0);

        assert(atlas_frag_build(dev_read, &v.dev, "/EMPTY.BIN", &fl)
               == ATLAS_EFORMAT);
    }

    /* Arguments. */
    assert(atlas_frag_build(NULL, &v.dev, "/X", &fl) == ATLAS_EINVAL);
    assert(atlas_frag_build(dev_read, &v.dev, NULL, &fl) == ATLAS_EINVAL);
    assert(atlas_frag_build(dev_read, &v.dev, "/X", NULL) == ATLAS_EINVAL);

    {
        u32 a, b, c;

        assert(atlas_frag_lookup(NULL, 0, &a, &b, &c) == ATLAS_EINVAL);
        assert(atlas_frag_lookup(&fl, 0, NULL, &b, &c) == ATLAS_EINVAL);
        assert(atlas_frag_lookup(&fl, 0, &a, NULL, &c) == ATLAS_EINVAL);
        assert(atlas_frag_lookup(&fl, 0, &a, &b, NULL) == ATLAS_EINVAL);
    }

    /* An empty list holds nothing, and offset 0 is already past its
     * end - the caller must not get sector 0 of the device. */
    {
        atlas_fraglist_t empty;
        u32 a, b, c;

        memset(&empty, 0, sizeof(empty));
        assert(atlas_frag_lookup(&empty, 0, &a, &b, &c) == ATLAS_EINVAL);
    }

    vol_free(&v);
}

/**
 * The two volumes either side of the FAT16/FAT32 line.
 *
 * A volume's width is not written down anywhere: it follows from the
 * cluster count, and fewer than 65525 clusters means FAT16. A reader
 * that puts the line one cluster out reads 32-bit entries from a 16-bit
 * FAT (or the reverse) and walks a chain of numbers that are half of
 * two other clusters - which still returns data, from the wrong place.
 * Only a volume sitting exactly on the line catches that, so both sides
 * of it are built here.
 *
 * One sector per cluster keeps these at ~34 MB each rather than the
 * hundreds a larger cluster would need.
 */
static void check_boundary(void)
{
    struct { int fat32; u32 clusters; } k_case[2];
    int c;

    k_case[0].fat32 = 0; k_case[0].clusters = 65524;   /* last FAT16 */
    k_case[1].fat32 = 1; k_case[1].clusters = 65525;   /* first FAT32 */

    for (c = 0; c < 2; c++) {
        vol_t v;
        dir_t root;
        atlas_fraglist_t fl;
        u32 cluster, size;

        vol_init_n(&v, k_case[c].fat32, 1, k_case[c].clusters);

        root.cluster = k_case[c].fat32 ? v.root_cluster : 0;
        root.used    = 0;

        /* Fragmented, so the chain is actually walked through the FAT
         * rather than run straight off the starting cluster. */
        size    = 24 * SECSZ;
        cluster = file_write(&v, 24, 0x88, 3);

        {
            unsigned char *e = dir_slot(&v, &root);
            memset(e, 0, 32);
            name_83(e, "EDGE", "ISO");
            entry_set(e, cluster, size, 0);
        }

        assert(atlas_frag_build(dev_read, &v.dev, "/EDGE.ISO", &fl)
               == ATLAS_OK);
        assert(fl.count == 24);
        verify(&v, &fl, 0x88, size);

        vol_free(&v);
    }
}

static void check_not_fat(void)
{
    dev_t d;
    atlas_fraglist_t fl;
    unsigned char *data;

    data = (unsigned char *)calloc(64, SECSZ);
    assert(data != NULL);

    d.data    = data;
    d.sectors = 64;
    d.fail_at = 0xFFFFFFFFU;
    d.reads   = 0;

    /* All zeroes: every field a FAT volume needs is absent. */
    assert(atlas_frag_build(dev_read, &d, "/X.ISO", &fl) == ATLAS_EFORMAT);

    /* A sector size that is not a power of two, and one too large for
     * the buffers - both are refused rather than used to compute
     * offsets nothing can hold. */
    put16(data + 11, 700);
    assert(atlas_frag_build(dev_read, &d, "/X.ISO", &fl) == ATLAS_EFORMAT);

    put16(data + 11, 8192);
    assert(atlas_frag_build(dev_read, &d, "/X.ISO", &fl) == ATLAS_EFORMAT);

    /* A plausible sector size with a zero cluster size: the division
     * that follows would be by zero. */
    put16(data + 11, 512);
    data[13] = 0;
    assert(atlas_frag_build(dev_read, &d, "/X.ISO", &fl) == ATLAS_EFORMAT);

    /* A cluster size that is not a power of two is not FAT either. */
    data[13] = 3;
    put16(data + 14, 1);
    data[16] = 1;
    put16(data + 17, 512);
    put16(data + 19, 64);
    put16(data + 22, 1);
    assert(atlas_frag_build(dev_read, &d, "/X.ISO", &fl) == ATLAS_EFORMAT);

    free(data);
}

static void check_io_errors(void)
{
    vol_t v;
    dir_t root;
    atlas_fraglist_t fl;
    u32 cluster, size;
    u32 stage;

    vol_init(&v, 0, 4);

    root.cluster = 0;
    root.used    = 0;

    size    = 20 * 4 * SECSZ;
    cluster = file_write(&v, 20, 0x22, 2);   /* fragmented, so the FAT
                                              * is read repeatedly */

    {
        unsigned char *e = dir_slot(&v, &root);
        memset(e, 0, 32);
        name_83(e, "IO", "ISO");
        entry_set(e, cluster, size, 0);
    }

    assert(atlas_frag_build(dev_read, &v.dev, "/IO.ISO", &fl) == ATLAS_OK);

    /*
     * A read failing at each of the three stages - boot sector, root
     * directory, FAT - must be reported, never treated as an absent
     * file or an empty chain. A card going away mid-read is the
     * ordinary way this happens, and "not found" would send the user
     * looking for the wrong problem.
     */
    v.dev.fail_at = 0;
    assert(atlas_frag_build(dev_read, &v.dev, "/IO.ISO", &fl) == ATLAS_EIO);

    v.dev.fail_at = v.reserved + v.fat_sectors;     /* the root */
    assert(atlas_frag_build(dev_read, &v.dev, "/IO.ISO", &fl) == ATLAS_EIO);

    v.dev.fail_at = v.reserved;                     /* the FAT */
    assert(atlas_frag_build(dev_read, &v.dev, "/IO.ISO", &fl) == ATLAS_EIO);

    /*
     * Whichever sector fails, the answer is either the right one or an
     * error - never a short list the caller would read through. Most of
     * these sectors are never touched (a mostly-empty root, the tail of
     * the FAT), so success is a legitimate outcome; what is checked is
     * that failure never produces a list.
     */
    for (stage = 0; stage < v.data_start; stage++) {
        atlas_err_t err;

        v.dev.fail_at = stage;
        memset(&fl, 0xAB, sizeof(fl));

        err = atlas_frag_build(dev_read, &v.dev, "/IO.ISO", &fl);

        if (err == ATLAS_OK) {
            v.dev.fail_at = 0xFFFFFFFFU;
            verify(&v, &fl, 0x22, size);
            v.dev.fail_at = stage;
        } else {
            assert(err == ATLAS_EIO || err == ATLAS_EFORMAT
                   || err == ATLAS_ENOENT);
            assert(fl.count == 0);
        }
    }

    v.dev.fail_at = 0xFFFFFFFFU;

    vol_free(&v);
}

int main(void)
{
    check_contiguous(0);
    check_contiguous(1);
    check_fragmented(0);
    check_fragmented(1);
    check_subdirectory(0);
    check_subdirectory(1);
    check_partial_cluster();
    check_limits();
    check_boundary();
    check_not_fat();
    check_io_errors();

    printf("test_frag: OK\n");
    return 0;
}
