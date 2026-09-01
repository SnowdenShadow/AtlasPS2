/*
 * AtlasPS2 - screen_files.c
 * The file manager.
 *
 * WHAT IT WILL AND WILL NOT DO
 * ----------------------------
 * Browse, launch an ELF, copy a file, move a file, rename, create a
 * folder, delete a file, delete an empty folder. That is the whole
 * list, and the omissions are the design:
 *
 *   - No recursive copy and no recursive delete. A D-pad, a television
 *     and no undo is the worst place in computing to offer "delete
 *     everything under here". A folder is emptied where the user can
 *     see each thing go.
 *   - No cross-device folder move. Within one device a move is a
 *     rename and is instant; across devices it is a recursive copy
 *     followed by a recursive delete, which is the operation above
 *     wearing a different hat.
 *
 * WHY DELETING ASKS TWICE FOR SOME FILES
 * --------------------------------------
 * `mc0:/BOOT.ELF` is what the console runs at power-on. Deleting it
 * from a file manager looks exactly like deleting anything else, and
 * the consequence - a card that no longer boots - arrives at the next
 * power cycle, long after the user has forgotten which row they were
 * on. So a protected path gets a second dialog with different words
 * that says what happens, rather than a louder copy of the first.
 *
 * There is no keyboard, so names are typed with the D-pad on a
 * character wheel. It is slow, and that is accepted: renaming here
 * happens once in a while, and the alternative - not offering it at
 * all - is worse than a slow one.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/device.h"
#include "atlas/file.h"
#include "atlas/fs.h"
#include "atlas/path.h"
#include "atlas/launch.h"
#include "atlas/i18n.h"
#include "atlas/log.h"

#define FM_PATH_MAX 256

/*
 * Rows are counted at draw time rather than fixed: see the note in
 * screen_apps.c.
 *
 * The file manager's reserve is the largest of the list screens: the
 * path sits above the list rather than below it, and the clipboard line
 * and the truncation warning both sit under it.
 */
static int fm_visible(void)
{
    return atlas_ui_rows_fit(atlas_ui_content_y_titled(),
                             atlas_ui_line_height() * 3.2f);
}

/* A Memory Card caps filenames at 32 bytes, so that is the ceiling
 * everywhere: a name that works on a stick and not on a card is a
 * failure the user only meets after the copy. */
#define FM_NAME_EDIT_MAX 32

typedef enum {
    FM_BROWSE = 0,   /* the listing                                  */
    FM_MENU,         /* the action list for the highlighted row      */
    FM_CONFIRM,      /* a yes/no question                            */
    FM_NAME,         /* the character wheel                          */
    FM_RESULT        /* one message, dismissed with any button       */
} fm_mode_t;

/* What a confirmed dialog goes on to do. Kept separate from the mode
 * so one confirmation path serves every dangerous action. */
typedef enum {
    FM_ACT_NONE = 0,
    FM_ACT_DELETE,
    FM_ACT_PASTE_OVERWRITE
} fm_action_t;

typedef struct {
    char path[FM_PATH_MAX];      /* folder being listed; "" = devices */

    atlas_fs_entry_t entries[ATLAS_FS_ENTRY_MAX];
    int              count;
    int              truncated;
    int              unreadable;

    int cursor;
    int top;

    fm_mode_t mode;
    int       menu_cursor;

    /* The confirmation in flight. `warned` is what makes the second
     * dialog a second dialog rather than the first one redrawn. */
    fm_action_t    pending;
    int            warned;
    atlas_str_id_t question;

    /* The clipboard. A path, not an index: the listing is rebuilt on
     * every folder change, and an index into the previous one names
     * whatever happens to occupy that row now. */
    char clip[FM_PATH_MAX];
    int  clip_is_move;

    /* The name editor. */
    char name[FM_NAME_EDIT_MAX];
    int  name_pos;
    int  name_for_rename;        /* 0 = new folder */

    atlas_str_id_t result;
} fm_state_t;

static fm_state_t s_state;

static void fm_draw(atlas_screen_t *self);

/* ------------------------------------------------------------------ */
/* Listing                                                             */
/* ------------------------------------------------------------------ */

/**
 * The device list, drawn as if it were a folder of folders.
 *
 * A file manager that starts inside one device needs a way to reach
 * the others, and "go up from mc0:/" has no answer the drivers agree
 * on. So the top of the tree is this, built by hand.
 */
static void list_devices(fm_state_t *st)
{
    int i;

    st->count = 0;
    st->truncated = 0;
    st->unreadable = 0;

    for (i = 0; i < ATLAS_DEV_COUNT; i++) {
        const atlas_device_t *d = atlas_device_get((atlas_device_id_t)i);

        /* Mounted devices only: a row for a card that is not in the
         * console leads to a listing that cannot be produced. */
        if (!d || d->state != ATLAS_DEV_READY || !d->path)
            continue;

        snprintf(st->entries[st->count].name,
                 sizeof(st->entries[st->count].name), "%s", d->path);
        st->entries[st->count].is_dir = 1;
        st->entries[st->count].size   = 0;
        st->count++;
    }
}

static void refresh(fm_state_t *st)
{
    if (st->path[0] == '\0') {
        list_devices(st);
    } else {
        int n = atlas_fs_list(st->path, st->entries,
                              ATLAS_FS_ENTRY_MAX, &st->truncated);

        st->unreadable = (n < 0);
        st->count = (n < 0) ? 0 : n;
    }

    if (st->cursor >= st->count)
        st->cursor = st->count > 0 ? st->count - 1 : 0;
    if (st->cursor < 0)
        st->cursor = 0;

    st->top = 0;
}

/** Full path of the highlighted row; 0 if there is not one. */
static int selected_path(const fm_state_t *st, char *out, int size)
{
    if (st->cursor < 0 || st->cursor >= st->count)
        return 0;

    /* At the device level the row IS the path. */
    if (st->path[0] == '\0')
        return snprintf(out, (size_t)size, "%s/",
                        st->entries[st->cursor].name) < size;

    return atlas_path_join(st->path, st->entries[st->cursor].name,
                           out, size) == ATLAS_OK;
}

static int selected_is_dir(const fm_state_t *st)
{
    if (st->cursor < 0 || st->cursor >= st->count)
        return 0;

    return st->entries[st->cursor].is_dir;
}

static int has_elf_suffix(const char *name)
{
    int n = (int)strlen(name);
    const char *e;

    if (n < 4)
        return 0;

    e = name + n - 4;

    return e[0] == '.'
        && (e[1] == 'e' || e[1] == 'E')
        && (e[2] == 'l' || e[2] == 'L')
        && (e[3] == 'f' || e[3] == 'F');
}

/* ------------------------------------------------------------------ */
/* The action menu                                                     */
/*                                                                     */
/* Built per row, because which actions apply depends on what is under */
/* the cursor. A fixed list with half its rows greyed out is a list    */
/* the user has to read every time to find the two entries that do     */
/* anything.                                                           */
/* ------------------------------------------------------------------ */

typedef enum {
    MI_OPEN = 0,
    MI_LAUNCH,
    MI_COPY,
    MI_MOVE,
    MI_PASTE,
    MI_RENAME,
    MI_DELETE,
    MI_MKDIR,
    MI_COUNT
} fm_menu_id_t;

typedef struct {
    fm_menu_id_t   id;
    atlas_str_id_t label;
} fm_menu_item_t;

static int build_menu(const fm_state_t *st, fm_menu_item_t *out)
{
    int n = 0;
    int have_row = (st->cursor >= 0 && st->cursor < st->count);
    int is_dir = selected_is_dir(st);
    int at_devices = (st->path[0] == '\0');

    if (have_row && is_dir) {
        out[n].id = MI_OPEN;
        out[n].label = ATLAS_STR_FM_OPEN;
        n++;
    }

    if (have_row && !is_dir && has_elf_suffix(st->entries[st->cursor].name)) {
        out[n].id = MI_LAUNCH;
        out[n].label = ATLAS_STR_FM_LAUNCH;
        n++;
    }

    /*
     * Copy and move are offered for files only. See the header: a
     * folder copy is a recursive walk, and this program does not have
     * one, for the same reason it has no recursive delete.
     */
    if (have_row && !is_dir && !at_devices) {
        out[n].id = MI_COPY;
        out[n].label = ATLAS_STR_FM_COPY;
        n++;
        out[n].id = MI_MOVE;
        out[n].label = ATLAS_STR_FM_MOVE;
        n++;
    }

    if (st->clip[0] && !at_devices) {
        out[n].id = MI_PASTE;
        out[n].label = ATLAS_STR_FM_PASTE;
        n++;
    }

    if (have_row && !at_devices) {
        out[n].id = MI_RENAME;
        out[n].label = ATLAS_STR_FM_RENAME;
        n++;
        out[n].id = MI_DELETE;
        out[n].label = ATLAS_STR_FM_DELETE;
        n++;
    }

    if (!at_devices) {
        out[n].id = MI_MKDIR;
        out[n].label = ATLAS_STR_FM_MKDIR;
        n++;
    }

    return n;
}

/* ------------------------------------------------------------------ */
/* Doing the work                                                      */
/* ------------------------------------------------------------------ */

static void finish(fm_state_t *st, atlas_str_id_t msg)
{
    st->result = msg;
    st->mode = FM_RESULT;
    refresh(st);
}

/*
 * Copying a 700 KB ELF to a Memory Card takes seconds, and a frozen
 * picture for seconds reads as a hang - after which the user's reflex
 * is to pull the card, during the one operation on it that must not be
 * interrupted. So the copy draws frames from inside its own progress
 * callback, exactly as the installer's progress screen does.
 */
static int copy_tick(int done, int total, void *ctx)
{
    (void)done;
    (void)total;
    (void)ctx;

    atlas_video_frame_begin(atlas_theme()->bg_top);
    atlas_ui_background();
    fm_draw(NULL);
    atlas_video_frame_end();

    return 1;
}

static void do_delete(fm_state_t *st)
{
    char path[FM_PATH_MAX];
    atlas_err_t err;

    if (!selected_path(st, path, sizeof(path))) {
        finish(st, ATLAS_STR_FM_E_DELETE);
        return;
    }

    if (selected_is_dir(st)) {
        err = atlas_fs_rmdir(path);

        /* Named for what the user has to do about it, not for the
         * error code that produced it. */
        if (err == ATLAS_EBUSY) {
            finish(st, ATLAS_STR_FM_E_NOTEMPTY);
            return;
        }
    } else {
        err = atlas_file_remove(path);
    }

    finish(st, err == ATLAS_OK ? ATLAS_STR_FM_OK_DELETE
                               : ATLAS_STR_FM_E_DELETE);
}

/** Where the clipboard would land in the current folder. */
static int paste_target(const fm_state_t *st, char *out, int size)
{
    return atlas_path_join(st->path, atlas_fs_basename(st->clip),
                           out, size) == ATLAS_OK;
}

static void do_paste(fm_state_t *st)
{
    char dst[FM_PATH_MAX];
    atlas_err_t err;

    if (!paste_target(st, dst, sizeof(dst))) {
        finish(st, ATLAS_STR_FM_E_NAME);
        return;
    }

    /*
     * Copying a file onto itself truncates it to nothing on most
     * drivers: the destination is opened for writing before the source
     * is read. Caught here rather than trusted to the driver.
     */
    if (strcmp(dst, st->clip) == 0) {
        finish(st, ATLAS_STR_FM_E_SAME);
        return;
    }

    if (st->clip_is_move) {
        /*
         * A rename is instant and cannot half-succeed, so it is tried
         * first. It only works within one device; across devices the
         * driver refuses and the copy below runs instead.
         */
        if (atlas_file_rename(st->clip, dst) == ATLAS_OK) {
            st->clip[0] = '\0';
            finish(st, ATLAS_STR_FM_OK_MOVE);
            return;
        }

        err = atlas_file_copy_verified(st->clip, dst, copy_tick, NULL);

        if (err != ATLAS_OK) {
            /* The original is untouched - nothing has been removed
             * yet. That is what the message promises, so it has to
             * stay true. */
            finish(st, ATLAS_STR_FM_E_MOVE);
            return;
        }

        /*
         * Verified before the original goes. atlas_file_copy_verified()
         * reads the destination back and compares checksums, so by the
         * time this line runs the copy is known good - which is the
         * only thing that makes deleting the source safe.
         */
        atlas_file_remove(st->clip);
        st->clip[0] = '\0';
        finish(st, ATLAS_STR_FM_OK_MOVE);
        return;
    }

    err = atlas_file_copy_verified(st->clip, dst, copy_tick, NULL);

    if (err != ATLAS_OK) {
        finish(st, ATLAS_STR_FM_E_COPY);
        return;
    }

    st->clip[0] = '\0';
    finish(st, ATLAS_STR_FM_OK_COPY);
}

static void do_rename(fm_state_t *st)
{
    char from[FM_PATH_MAX];
    char to[FM_PATH_MAX];

    if (!selected_path(st, from, sizeof(from))
        || atlas_path_join(st->path, st->name, to, sizeof(to)) != ATLAS_OK) {
        finish(st, ATLAS_STR_FM_E_NAME);
        return;
    }

    if (strcmp(from, to) == 0) {
        st->mode = FM_BROWSE;
        return;
    }

    /* Renaming onto an existing name would silently replace it on some
     * drivers and fail on others. Refused either way. */
    if (atlas_file_exists(to)) {
        finish(st, ATLAS_STR_FM_E_NAME);
        return;
    }

    finish(st, atlas_file_rename(from, to) == ATLAS_OK
               ? ATLAS_STR_FM_OK_MOVE : ATLAS_STR_FM_E_NAME);
}

static void do_mkdir(fm_state_t *st)
{
    char path[FM_PATH_MAX];

    if (atlas_path_join(st->path, st->name, path, sizeof(path))
        != ATLAS_OK) {
        finish(st, ATLAS_STR_FM_E_NAME);
        return;
    }

    finish(st, atlas_file_mkdir_p(path) == ATLAS_OK
               ? ATLAS_STR_FM_OK_MKDIR : ATLAS_STR_FM_E_MKDIR);
}

/* ------------------------------------------------------------------ */
/* The character wheel                                                 */
/*                                                                     */
/* Up and Down cycle the character under the caret; Left and Right     */
/* move along the name. Blanking the trailing characters is how a name */
/* is shortened, since there is no delete key to offer.                */
/* ------------------------------------------------------------------ */

static const char s_charset[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-";

static int charset_index(char c)
{
    const char *p = strchr(s_charset, c);

    /* A character the wheel cannot produce - one already in the name
     * that is not on the wheel - starts from the blank. strchr also
     * matches the terminator, hence the *p test. */
    return (p && *p) ? (int)(p - s_charset) : 0;
}

static void name_cycle(fm_state_t *st, int delta)
{
    int n = (int)sizeof(s_charset) - 1;   /* without the terminator */
    int i;

    if (st->name_pos < 0 || st->name_pos >= FM_NAME_EDIT_MAX - 1)
        return;

    /* Typing past the end extends the name by one. */
    if (st->name[st->name_pos] == '\0') {
        st->name[st->name_pos] = ' ';
        st->name[st->name_pos + 1] = '\0';
    }

    i = charset_index(st->name[st->name_pos]);
    i = (i + delta + n) % n;

    st->name[st->name_pos] = s_charset[i];
}

/** Strip the blanks the wheel leaves at either end. */
static void name_trim(char *s)
{
    int i, n = (int)strlen(s);

    while (n > 0 && s[n - 1] == ' ')
        s[--n] = '\0';

    for (i = 0; s[i] == ' '; i++)
        ;

    if (i > 0)
        memmove(s, s + i, strlen(s + i) + 1);
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void enter_confirm(fm_state_t *st, fm_action_t act,
                          atlas_str_id_t question)
{
    st->pending  = act;
    st->question = question;
    st->warned   = 0;
    st->mode     = FM_CONFIRM;
}

static void menu_choose(fm_state_t *st, fm_menu_id_t id)
{
    char path[FM_PATH_MAX];

    switch (id) {
    case MI_OPEN:
        if (selected_path(st, path, sizeof(path))) {
            snprintf(st->path, sizeof(st->path), "%s", path);
            st->cursor = 0;
            refresh(st);
        }
        st->mode = FM_BROWSE;
        break;

    case MI_LAUNCH:
        if (selected_path(st, path, sizeof(path))) {
            if (atlas_launch_check(path) != ATLAS_OK) {
                finish(st, ATLAS_STR_APPS_FAIL_FORMAT);
                return;
            }
            atlas_launch_elf(path, 0, NULL);
            /* Only reached if the loader gave up after teardown, at
             * which point there is no interface left to return to. */
            atlas_screen_request_exit();
        }
        st->mode = FM_BROWSE;
        break;

    case MI_COPY:
    case MI_MOVE:
        if (selected_path(st, path, sizeof(path))) {
            snprintf(st->clip, sizeof(st->clip), "%s", path);
            st->clip_is_move = (id == MI_MOVE);
        }
        st->mode = FM_BROWSE;
        break;

    case MI_PASTE:
        if (!paste_target(st, path, sizeof(path))) {
            finish(st, ATLAS_STR_FM_E_NAME);
            return;
        }
        /* Overwriting is the one paste outcome that cannot be undone,
         * so it is the one that asks. */
        if (atlas_file_exists(path)) {
            enter_confirm(st, FM_ACT_PASTE_OVERWRITE,
                          ATLAS_STR_FM_Q_OVERWRITE);
            return;
        }
        do_paste(st);
        break;

    case MI_RENAME:
        if (st->cursor >= 0 && st->cursor < st->count) {
            snprintf(st->name, sizeof(st->name), "%s",
                     st->entries[st->cursor].name);
            st->name_pos = 0;
            st->name_for_rename = 1;
            st->mode = FM_NAME;
        }
        break;

    case MI_DELETE:
        enter_confirm(st, FM_ACT_DELETE,
                      selected_is_dir(st) ? ATLAS_STR_FM_Q_DELETE_DIR
                                          : ATLAS_STR_FM_Q_DELETE);
        break;

    case MI_MKDIR:
        snprintf(st->name, sizeof(st->name), "%s",
                 atlas_str(ATLAS_STR_FM_NEWDIR_NAME));
        st->name_pos = 0;
        st->name_for_rename = 0;
        st->mode = FM_NAME;
        break;

    default:
        st->mode = FM_BROWSE;
        break;
    }
}

static void update_browse(fm_state_t *st)
{
    u32 rep = atlas_input_repeated();

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        if (st->path[0] == '\0') {
            atlas_screen_pop();
            return;
        }

        /* Up from a device root goes to the device list, not to a
         * parent no driver can produce. */
        if (atlas_fs_is_root(st->path))
            st->path[0] = '\0';
        else
            atlas_fs_parent(st->path);

        st->cursor = 0;
        refresh(st);
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONTEXT)) {
        refresh(st);
        return;
    }

    if (st->count > 0) {
        if (rep & ATLAS_BTN_UP)
            st->cursor = (st->cursor + st->count - 1) % st->count;
        if (rep & ATLAS_BTN_DOWN)
            st->cursor = (st->cursor + 1) % st->count;

        if (st->cursor < st->top)
            st->top = st->cursor;
        if (st->cursor >= st->top + fm_visible())
            st->top = st->cursor - fm_visible() + 1;
    }

    /*
     * Cross opens a folder and does nothing else. Every action that
     * changes a file is behind Square, so a user pressing the button
     * they press on every other screen cannot delete anything.
     */
    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM) && st->count > 0) {
        if (selected_is_dir(st)) {
            char path[FM_PATH_MAX];

            if (selected_path(st, path, sizeof(path))) {
                snprintf(st->path, sizeof(st->path), "%s", path);
                st->cursor = 0;
                refresh(st);
            }
        }
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_ACTION)) {
        st->menu_cursor = 0;
        st->mode = FM_MENU;
    }
}

static void update_menu(fm_state_t *st)
{
    fm_menu_item_t items[MI_COUNT];
    int n = build_menu(st, items);
    u32 rep = atlas_input_repeated();

    if (n == 0 || atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        st->mode = FM_BROWSE;
        return;
    }

    if (st->menu_cursor >= n)
        st->menu_cursor = 0;

    if (rep & ATLAS_BTN_UP)
        st->menu_cursor = (st->menu_cursor + n - 1) % n;
    if (rep & ATLAS_BTN_DOWN)
        st->menu_cursor = (st->menu_cursor + 1) % n;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        menu_choose(st, items[st->menu_cursor].id);
}

static void update_confirm(fm_state_t *st)
{
    char path[FM_PATH_MAX];

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        st->pending = FM_ACT_NONE;
        st->mode = FM_BROWSE;
        return;
    }

    if (!atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        return;

    /*
     * A protected path earns a second dialog with different words -
     * not a louder version of the first. The point is to break the
     * rhythm of "X, X" and say what actually becomes of a console
     * whose BOOT.ELF is gone.
     */
    if (st->pending == FM_ACT_DELETE && !st->warned
        && selected_path(st, path, sizeof(path))
        && atlas_fs_is_protected(path)) {
        st->warned = 1;
        return;
    }

    switch (st->pending) {
    case FM_ACT_DELETE:
        st->pending = FM_ACT_NONE;
        do_delete(st);
        break;

    case FM_ACT_PASTE_OVERWRITE:
        st->pending = FM_ACT_NONE;
        do_paste(st);
        break;

    default:
        st->mode = FM_BROWSE;
        break;
    }
}

static void update_name(fm_state_t *st)
{
    u32 rep = atlas_input_repeated();
    int len;

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        st->mode = FM_BROWSE;
        return;
    }

    if (rep & ATLAS_BTN_UP)
        name_cycle(st, 1);
    if (rep & ATLAS_BTN_DOWN)
        name_cycle(st, -1);

    len = (int)strlen(st->name);

    if ((rep & ATLAS_BTN_LEFT) && st->name_pos > 0)
        st->name_pos--;

    /* One past the end is a legal caret position: that is where the
     * next Up starts a new character. */
    if ((rep & ATLAS_BTN_RIGHT)
        && st->name_pos < len && st->name_pos < FM_NAME_EDIT_MAX - 2)
        st->name_pos++;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        name_trim(st->name);

        /*
         * An empty name, or one carrying a separator, would build a
         * path pointing somewhere else entirely. Refused before it
         * reaches the driver.
         */
        if (st->name[0] == '\0' || strchr(st->name, '/')
            || strchr(st->name, ':')) {
            finish(st, ATLAS_STR_FM_E_NAME);
            return;
        }

        if (st->name_for_rename)
            do_rename(st);
        else
            do_mkdir(st);
    }
}

static void fm_update(atlas_screen_t *self)
{
    fm_state_t *st = (fm_state_t *)self->data;

    switch (st->mode) {
    case FM_MENU:    update_menu(st);    break;
    case FM_CONFIRM: update_confirm(st); break;
    case FM_NAME:    update_name(st);    break;

    case FM_RESULT:
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)
            || atlas_input_is_pressed(ATLAS_BTN_BACK))
            st->mode = FM_BROWSE;
        break;

    default:
        update_browse(st);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

/**
 * A size a person can read at three metres.
 *
 * Two significant figures is all a television resolves, and the exact
 * byte count of a file the user is about to copy tells them nothing
 * the rounded one does not.
 */
static void size_text(int bytes, char *out, int size)
{
    if (bytes < 0)
        out[0] = '\0';
    else if (bytes < 1024)
        snprintf(out, (size_t)size, "%d B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(out, (size_t)size, "%d KB", bytes / 1024);
    else
        snprintf(out, (size_t)size, "%d.%d MB",
                 bytes / (1024 * 1024),
                 (bytes % (1024 * 1024)) / (1024 * 1024 / 10));
}

/*
 * A dialog of up to a few lines.
 *
 * atlas_ui_message_box() draws a single-line body, and the font has no
 * newline handling - a '\n' in a string is just a glyph it does not
 * have. The warnings here are two full sentences plus the name of the
 * file, and the name is exactly the part that must not be ellipsised
 * away, so this lays the lines out itself.
 */
static void draw_dialog(const char *title, const char **lines, int n,
                        const char *hint)
{
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float sh = (float)atlas_video_safe_h();
    float lh = atlas_ui_line_height();

    float w = sw * 0.8f;
    float h = lh * (4.5f + (float)n * 1.3f);
    float x = (sw - w) * 0.5f;
    float y = (sh - h) * 0.5f;
    float ty;
    int i;

    /* Dim what is behind, so the listing stays legible as context
     * without competing with the question. */
    atlas_ui_rect(0.0f, 0.0f, sw, sh, ATLAS_RGBA(0x00, 0x00, 0x00, 0x50));

    atlas_ui_panel(x, y, w, h, t->panel);
    atlas_ui_rect(x, y, w, 3.0f, t->accent);

    ty = y + (float)ATLAS_UI_PAD;

    if (title) {
        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, ty, t->text, title,
                              w - (float)ATLAS_UI_PAD * 2.0f);
        ty += lh * 1.6f;
    }

    for (i = 0; i < n; i++) {
        if (lines[i])
            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, ty, t->text_dim,
                                  lines[i], w - (float)ATLAS_UI_PAD * 2.0f);
        ty += lh * 1.3f;
    }

    if (hint)
        atlas_ui_text(x + w * 0.5f, y + h - lh - (float)ATLAS_UI_PAD,
                      ATLAS_ALIGN_CENTER, t->text_dim, hint);
}

static void draw_menu(const fm_state_t *st, float x, float w)
{
    const atlas_theme_t *t = atlas_theme();
    fm_menu_item_t items[MI_COUNT];
    int n = build_menu(st, items);
    float mw = 220.0f;
    float mh = (float)(n * (ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP))
             + (float)ATLAS_UI_PAD * 2.0f;
    float mx = x + w - mw;
    float my = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD * 3.0f;
    float y;
    int i;

    if (n <= 0)
        return;

    atlas_ui_panel(mx, my, mw, mh, t->panel);

    y = my + (float)ATLAS_UI_PAD;

    for (i = 0; i < n; i++) {
        atlas_ui_menu_row(mx + (float)ATLAS_UI_ROW_GAP, y,
                          mw - (float)ATLAS_UI_ROW_GAP * 2.0f,
                          i == st->menu_cursor,
                          atlas_str(items[i].label), NULL);

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }
}

static void draw_confirm(const fm_state_t *st)
{
    const char *lines[3];
    char hint[80];
    char path[FM_PATH_MAX];
    int n = 0;

    if (!selected_path(st, path, sizeof(path)))
        path[0] = '\0';

    /*
     * The name is repeated inside the dialog. "Are you sure?" over a
     * box that does not say what it is about is how the wrong file
     * gets deleted by somebody who was sure about a different one.
     */
    if (st->warned) {
        lines[n++] = atlas_str(ATLAS_STR_FM_W_SYSTEM);
        lines[n++] = atlas_fs_basename(path);
        lines[n++] = atlas_str(ATLAS_STR_FM_W_AGAIN);
    } else {
        lines[n++] = atlas_str(st->question);
        lines[n++] = atlas_fs_basename(path);
    }

    snprintf(hint, sizeof(hint), "X  %s     O  %s",
             atlas_str(ATLAS_STR_CONFIRM), atlas_str(ATLAS_STR_CANCEL));

    draw_dialog(atlas_str(st->warned ? ATLAS_STR_FM_W_AGAIN : st->question),
                lines, n, hint);
}

static void draw_name(const fm_state_t *st)
{
    char caret[FM_NAME_EDIT_MAX + 2];
    const char *lines[2];
    char hint[96];
    int i, len = (int)strlen(st->name);

    /*
     * The caret goes on a line of its own under the name rather than
     * being inserted into it: the interface font is proportional, so
     * a marker inside the string would shift every character after it
     * on every press.
     */
    for (i = 0; i < len && i < FM_NAME_EDIT_MAX; i++)
        caret[i] = (i == st->name_pos) ? '^' : ' ';

    if (st->name_pos >= len && i < FM_NAME_EDIT_MAX + 1)
        caret[i++] = '^';

    caret[i] = '\0';

    lines[0] = st->name;
    lines[1] = caret;

    snprintf(hint, sizeof(hint), "Up/Down  A-Z     X  %s     O  %s",
             atlas_str(ATLAS_STR_OK), atlas_str(ATLAS_STR_CANCEL));

    draw_dialog(atlas_str(st->name_for_rename ? ATLAS_STR_FM_RENAME
                                              : ATLAS_STR_FM_MKDIR),
                lines, 2, hint);
}

static void fm_draw(atlas_screen_t *self)
{
    /* Called with NULL from the copy progress callback, which is
     * mid-operation and has only the one state to draw. */
    fm_state_t *st = self ? (fm_state_t *)self->data : &s_state;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i, last;
    char hints[192];

    atlas_ui_header(atlas_str(ATLAS_STR_FM_TITLE));

    y = atlas_ui_content_y();

    /* The path, not the screen's name: where they are matters far more
     * to the user than what the screen is called. */
    atlas_ui_text_clipped(x, y, t->text,
                          st->path[0] ? st->path
                                      : atlas_str(ATLAS_STR_HOME_DEVICES),
                          w);
    y = atlas_ui_content_y_titled();

    if (st->unreadable)
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->warn,
                      atlas_str(ATLAS_STR_FM_UNREADABLE));
    else if (st->count == 0)
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                      atlas_str(ATLAS_STR_FM_EMPTY));

    last = st->top + fm_visible();
    if (last > st->count)
        last = st->count;

    for (i = st->top; i < last; i++) {
        const atlas_fs_entry_t *e = &st->entries[i];
        char label[ATLAS_FS_NAME_MAX + 4];
        char size[24];

        /* A trailing slash marks a folder. A separate icon column
         * would cost horizontal room the names need more. */
        snprintf(label, sizeof(label), "%s%s", e->name, e->is_dir ? "/" : "");

        if (e->is_dir)
            size[0] = '\0';
        else
            size_text(e->size, size, sizeof(size));

        atlas_ui_menu_row(x, y, w, i == st->cursor, label,
                          size[0] ? size : NULL);

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    if (st->truncated)
        atlas_ui_text(x, y + (float)ATLAS_UI_ROW_GAP, ATLAS_ALIGN_LEFT,
                      t->warn, atlas_str(ATLAS_STR_FM_TRUNCATED));

    /*
     * What is waiting to be pasted, and where it came from. Without
     * this the clipboard is invisible state the user has to carry in
     * their head across however many folders they walk through, and a
     * copy and a move look identical until one of them removes the
     * original.
     */
    if (st->clip[0]) {
        char line[FM_PATH_MAX + 64];

        snprintf(line, sizeof(line), "%s  %s  (%s)",
                 atlas_str(ATLAS_STR_FM_CLIPBOARD), st->clip,
                 atlas_str(st->clip_is_move ? ATLAS_STR_FM_MOVE
                                            : ATLAS_STR_FM_COPY));

        atlas_ui_text_clipped(
            x,
            (float)atlas_video_safe_h() - (float)ATLAS_UI_FOOTER_H
                - lh * 1.4f,
            st->clip_is_move ? t->warn : t->text_dim, line, w);
    }

    snprintf(hints, sizeof(hints),
             "X  %s   Square  %s   Triangle  %s   O  %s",
             atlas_str(ATLAS_STR_FM_OPEN), atlas_str(ATLAS_STR_FM_MENU),
             atlas_str(ATLAS_STR_RESCAN), atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    switch (st->mode) {
    case FM_MENU:    draw_menu(st, x, w); break;
    case FM_CONFIRM: draw_confirm(st);    break;
    case FM_NAME:    draw_name(st);       break;

    case FM_RESULT: {
        const char *lines[1];
        char ok[40];

        lines[0] = atlas_str(st->result);
        snprintf(ok, sizeof(ok), "X  %s", atlas_str(ATLAS_STR_OK));
        draw_dialog(atlas_str(ATLAS_STR_FM_TITLE), lines, 1, ok);
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */

static void fm_enter(atlas_screen_t *self)
{
    fm_state_t *st = (fm_state_t *)self->data;

    st->mode = FM_BROWSE;
    st->pending = FM_ACT_NONE;
    st->warned = 0;

    /*
     * The clipboard deliberately survives leaving the screen: copying
     * from a stick, stepping out to Devices to check a card is
     * mounted, and coming back is an ordinary thing to do, and losing
     * the pending copy for it would be baffling.
     */
    refresh(st);
}

static atlas_screen_t s_screen = {
    "Files",
    fm_enter,
    NULL,
    fm_update,
    fm_draw,
    &s_state
};

atlas_screen_t *atlas_screen_files(void)
{
    return &s_screen;
}
