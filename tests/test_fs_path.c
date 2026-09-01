/*
 * AtlasPS2 - test_fs_path.c
 *
 * The file manager's path rules.
 *
 * atlas_fs_is_protected() is the function that decides whether a user
 * is warned before deleting the file their console boots from. There
 * is no way to check it on hardware without wrecking a Memory Card to
 * find out it was wrong, so it is checked here, on the build machine,
 * every time.
 *
 * atlas_fs_parent() gets the same attention for a quieter reason: a
 * parent walk that can step above a device root ends up asking the
 * driver to list "mc0:" or "", and what comes back for those differs
 * per driver and per filesystem.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/fs.h"

/* ------------------------------------------------------------------ */
/* Roots                                                               */
/* ------------------------------------------------------------------ */

static void test_root_recognition(void)
{
    assert(atlas_fs_is_root("mc0:/"));
    assert(atlas_fs_is_root("mc1:/"));
    assert(atlas_fs_is_root("mass:/"));

    /* Both spellings: some drivers hand back the colon form. */
    assert(atlas_fs_is_root("mc0:"));
    assert(atlas_fs_is_root("mass:"));

    assert(!atlas_fs_is_root("mc0:/ATLAS"));
    assert(!atlas_fs_is_root("mass:/GAMES/A"));

    /* Nothing usable is not a root. */
    assert(!atlas_fs_is_root(""));
    assert(!atlas_fs_is_root(NULL));
    assert(!atlas_fs_is_root("no-colon-here"));
}

/* ------------------------------------------------------------------ */
/* Walking up                                                          */
/* ------------------------------------------------------------------ */

static void up(char *p)
{
    atlas_fs_parent(p);
}

static void test_parent_walks_down_to_the_root(void)
{
    char p[64];

    strcpy(p, "mass:/GAMES/RPG/SAVE");

    up(p);
    assert(strcmp(p, "mass:/GAMES/RPG") == 0);

    up(p);
    assert(strcmp(p, "mass:/GAMES") == 0);

    up(p);
    assert(strcmp(p, "mass:/") == 0);
}

static void test_parent_stops_at_the_root(void)
{
    char p[64];

    /*
     * The one that matters. A file manager that walks off the top of a
     * device asks the driver for a path it has no answer for, and the
     * screen the user gets is empty for reasons nothing can explain.
     */
    strcpy(p, "mass:/");
    assert(atlas_fs_parent(p) == 0);
    assert(strcmp(p, "mass:/") == 0);

    strcpy(p, "mc0:");
    assert(atlas_fs_parent(p) == 0);
    assert(strcmp(p, "mc0:") == 0);
}

static void test_parent_drops_a_trailing_slash(void)
{
    char a[64], b[64];

    /* "mc0:/A/B/" and "mc0:/A/B" are one directory, and must give one
     * parent - two spellings is two rows in a listing that should have
     * shown one. */
    strcpy(a, "mc0:/A/B/");
    strcpy(b, "mc0:/A/B");

    up(a);
    up(b);

    assert(strcmp(a, "mc0:/A") == 0);
    assert(strcmp(b, "mc0:/A") == 0);
}

static void test_parent_refuses_nonsense(void)
{
    char p[64];

    strcpy(p, "no-colon");
    assert(atlas_fs_parent(p) == 0);
    assert(strcmp(p, "no-colon") == 0);

    strcpy(p, "");
    assert(atlas_fs_parent(p) == 0);
}

/* ------------------------------------------------------------------ */
/* Basenames                                                           */
/* ------------------------------------------------------------------ */

static void test_basename(void)
{
    assert(strcmp(atlas_fs_basename("mc0:/ATLAS/BOOT.ELF"), "BOOT.ELF") == 0);
    assert(strcmp(atlas_fs_basename("mass:/A"), "A") == 0);

    /* No slash after the device prefix. */
    assert(strcmp(atlas_fs_basename("mc0:BOOT.ELF"), "BOOT.ELF") == 0);

    /* A root has no last component; what comes back must still be a
     * string a caller can hand to snprintf without checking. */
    assert(atlas_fs_basename("mc0:/") != NULL);
    assert(atlas_fs_basename(NULL) != NULL);
    assert(strcmp(atlas_fs_basename(NULL), "") == 0);

    assert(strcmp(atlas_fs_basename("plain"), "plain") == 0);
}

/* ------------------------------------------------------------------ */
/* What must not be deleted without a second look                      */
/* ------------------------------------------------------------------ */

static void test_boot_files_at_a_card_root_are_protected(void)
{
    assert(atlas_fs_is_protected("mc0:/BOOT.ELF"));
    assert(atlas_fs_is_protected("mc1:/BOOT.ELF"));

    /* The staged update and its rollback copy: deleting either one
     * mid-transaction is how an interrupted update becomes an
     * unbootable card. */
    assert(atlas_fs_is_protected("mc0:/BOOT.NEW"));
    assert(atlas_fs_is_protected("mc0:/BOOT.BAK"));

    assert(atlas_fs_is_protected("mc0:/BADISK.ELF"));
    assert(atlas_fs_is_protected("mc0:/BAEXEC-SYSTEM"));
    assert(atlas_fs_is_protected("mc0:/BREXEC-SYSTEM"));
    assert(atlas_fs_is_protected("mc0:/BIEXEC-SYSTEM"));

    /* Our own installation. */
    assert(atlas_fs_is_protected("mc0:/ATLAS"));
    assert(atlas_fs_is_protected("mc0:/ATLAS/CONFIG/ATLAS.INI"));
}

static void test_case_does_not_matter(void)
{
    /* FAT and Memory Cards disagree about case, and a user who copied
     * a file from a PC may well have `boot.elf`. */
    assert(atlas_fs_is_protected("mc0:/boot.elf"));
    assert(atlas_fs_is_protected("mc0:/Boot.Elf"));
    assert(atlas_fs_is_protected("mass:/sys-conf"));
}

static void test_protection_is_anchored_at_the_root(void)
{
    /*
     * The whole reason the first component is compared whole: BOOT.ELF
     * at the root of a card is what the console starts, and a BOOT.ELF
     * three folders down is somebody's homebrew. Warning about the
     * second one would teach the user to press through the warning
     * that matters.
     */
    assert(!atlas_fs_is_protected("mass:/HOMEBREW/GAME/BOOT.ELF"));
    assert(!atlas_fs_is_protected("mc0:/MYSTUFF/BOOT.ELF"));
}

static void test_a_prefix_is_not_a_match(void)
{
    /* "BOOT" is protected; "BOOTLEG" is not, and matching on a prefix
     * would protect it. */
    assert(atlas_fs_is_protected("mc0:/BOOT"));
    assert(!atlas_fs_is_protected("mc0:/BOOTLEG"));
    assert(!atlas_fs_is_protected("mc0:/BOOT.ELF.BAK"));
    assert(!atlas_fs_is_protected("mc0:/APPSTORE"));
}

static void test_console_folders_are_protected_anywhere(void)
{
    /* These hold the console's own settings, and they matter wherever
     * they sit rather than only at a root. */
    assert(atlas_fs_is_protected("mc0:/SYS-CONF"));
    assert(atlas_fs_is_protected("mc0:/A/B/SYS-CONF"));
    assert(atlas_fs_is_protected("mass:/BACKUP/BADATA-SYSTEM/X"));
    assert(atlas_fs_is_protected("mc1:/X/BWNETCNF"));
}

static void test_the_device_root_is_protected(void)
{
    /*
     * Nothing offers to delete a root, but the copy and move targets
     * get checked too, and "overwrite the root" must never look like
     * an ordinary destination.
     */
    assert(atlas_fs_is_protected("mc0:/"));
    assert(atlas_fs_is_protected("mass:"));
}

static void test_ordinary_files_are_not_protected(void)
{
    /*
     * The other half of the promise. A list that says yes to
     * everything is a list nobody reads, and the second dialog only
     * works while it is rare.
     */
    assert(!atlas_fs_is_protected("mass:/VIDEO.MP4"));
    assert(!atlas_fs_is_protected("mc0:/SAVES/BASLUS-12345"));
    assert(!atlas_fs_is_protected("mass:/ATLAS-THEMES/DARK.INI"));

    /* Not a path this program builds; must not crash or claim. */
    assert(!atlas_fs_is_protected(""));
    assert(!atlas_fs_is_protected(NULL));
    assert(!atlas_fs_is_protected("relative/path"));
}

/* ------------------------------------------------------------------ */

int main(void)
{
    test_root_recognition();
    test_parent_walks_down_to_the_root();
    test_parent_stops_at_the_root();
    test_parent_drops_a_trailing_slash();
    test_parent_refuses_nonsense();
    test_basename();

    test_boot_files_at_a_card_root_are_protected();
    test_case_does_not_matter();
    test_protection_is_anchored_at_the_root();
    test_a_prefix_is_not_a_match();
    test_console_folders_are_protected_anywhere();
    test_the_device_root_is_protected();
    test_ordinary_files_are_not_protected();

    printf("test_fs_path: all checks passed\n");

    return 0;
}
