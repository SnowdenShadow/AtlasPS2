/*
 * AtlasPS2 - input.c
 * libpad wrapper: polling, edge detection and key repeat.
 */
#include <string.h>

#include <kernel.h>
#include <libpad.h>

#include "atlas/input.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Pad buffers                                                         */
/*                                                                     */
/* libpad DMAs into these from the IOP side, so each one must be 64    */
/* byte aligned and at least 256 bytes (2 * sizeof(struct pad_data)).  */
/* ------------------------------------------------------------------ */

static char s_pad_buf[2][256] __attribute__((aligned(64)));

#define ATLAS_PAD_PORTS 2

typedef struct {
    int open;
    int connected;
} atlas_port_t;

static atlas_port_t s_port[ATLAS_PAD_PORTS];

static u32 s_held;
static u32 s_prev_held;
static u32 s_pressed;
static u32 s_released;
static u32 s_repeated;
static u16 s_raw;
static int s_stick_x;
static int s_stick_y;
static int s_any_connected;
static int s_initialised;

static atlas_pad_layout_t s_layout = ATLAS_LAYOUT_CROSS_CONFIRM;

/*
 * Key repeat, in frames. At 60 Hz (NTSC) that is ~0.4 s before the first
 * repeat and ~0.08 s between repeats after that; PAL runs at 50 Hz so it
 * feels marginally slower, which is fine and avoids a timer dependency.
 */
#define ATLAS_REPEAT_DELAY_FRAMES  24
#define ATLAS_REPEAT_PERIOD_FRAMES  5

static u32 s_repeat_mask;    /* buttons currently in the repeat cycle */
static int s_repeat_counter;

/* Directions are the only buttons that auto-repeat. */
#define ATLAS_REPEATABLE (ATLAS_BTN_UP | ATLAS_BTN_DOWN | \
                          ATLAS_BTN_LEFT | ATLAS_BTN_RIGHT)

/* ------------------------------------------------------------------ */
/* Mapping                                                             */
/* ------------------------------------------------------------------ */

/**
 * Translate a raw libpad mask (already inverted: 1 == pressed) into our
 * logical buttons.
 */
static u32 map_buttons(u16 raw)
{
    u32 out = 0;

    if (raw & PAD_UP)       out |= ATLAS_BTN_UP;
    if (raw & PAD_DOWN)     out |= ATLAS_BTN_DOWN;
    if (raw & PAD_LEFT)     out |= ATLAS_BTN_LEFT;
    if (raw & PAD_RIGHT)    out |= ATLAS_BTN_RIGHT;
    if (raw & PAD_TRIANGLE) out |= ATLAS_BTN_CONTEXT;
    if (raw & PAD_SQUARE)   out |= ATLAS_BTN_ACTION;
    if (raw & PAD_L1)       out |= ATLAS_BTN_PREV_TAB;
    if (raw & PAD_R1)       out |= ATLAS_BTN_NEXT_TAB;
    if (raw & PAD_L2)       out |= ATLAS_BTN_L2;
    if (raw & PAD_R2)       out |= ATLAS_BTN_R2;
    if (raw & PAD_START)    out |= ATLAS_BTN_START;
    if (raw & PAD_SELECT)   out |= ATLAS_BTN_SELECT;

    if (s_layout == ATLAS_LAYOUT_CIRCLE_CONFIRM) {
        if (raw & PAD_CIRCLE) out |= ATLAS_BTN_CONFIRM;
        if (raw & PAD_CROSS)  out |= ATLAS_BTN_BACK;
    } else {
        if (raw & PAD_CROSS)  out |= ATLAS_BTN_CONFIRM;
        if (raw & PAD_CIRCLE) out |= ATLAS_BTN_BACK;
    }

    return out;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_input_init(void)
{
    int port;
    int opened = 0;

    if (s_initialised)
        return ATLAS_OK;

    /*
     * The IOP side (SIO2MAN + PADMAN) is loaded by the boot module before
     * we get here; padInit() only sets up the EE half of libpad.
     */
    if (padInit(0) != 1) {
        ATLAS_LOG("PAD", "padInit failed");
        return ATLAS_ENODEV;
    }

    for (port = 0; port < ATLAS_PAD_PORTS; port++) {
        if (padPortOpen(port, 0, s_pad_buf[port]) != 0) {
            s_port[port].open = 1;
            opened++;
        } else {
            ATLAS_LOG("PAD", "port %d open failed", port);
        }
    }

    if (!opened) {
        ATLAS_LOG("PAD", "no port could be opened");
        return ATLAS_ENODEV;
    }

    s_initialised = 1;
    ATLAS_LOG("PAD", "initialised, %d port(s)", opened);
    return ATLAS_OK;
}

void atlas_input_shutdown(void)
{
    int port;

    if (!s_initialised)
        return;

    for (port = 0; port < ATLAS_PAD_PORTS; port++) {
        if (s_port[port].open) {
            padPortClose(port, 0);
            s_port[port].open = 0;
        }
    }

    padEnd();

    memset(s_port, 0, sizeof(s_port));
    s_held = s_prev_held = s_pressed = s_released = s_repeated = 0;
    s_raw = 0;
    s_any_connected = 0;
    s_initialised = 0;

    ATLAS_LOG("PAD", "shutdown");
}

void atlas_input_set_layout(atlas_pad_layout_t layout)
{
    s_layout = layout;
}

atlas_pad_layout_t atlas_input_layout(void)
{
    return s_layout;
}

/* ------------------------------------------------------------------ */
/* Polling                                                             */
/* ------------------------------------------------------------------ */

/**
 * Read one port. Returns the logical mask, or 0 when that port has no
 * usable pad this frame.
 */
static u32 poll_port(int port, u16 *raw_out, int *stick_x, int *stick_y)
{
    struct padButtonStatus buttons;
    int state;
    u16 raw;

    if (!s_port[port].open)
        return 0;

    state = padGetState(port, 0);

    /*
     * PAD_STATE_STABLE means the pad finished negotiating and its data
     * is meaningful. Anything else (FINDPAD, EXECCMD, ...) is a pad that
     * is still coming up - normal right after a hot-plug - so we report
     * "nothing pressed" for this frame instead of blocking.
     */
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        s_port[port].connected = 0;
        return 0;
    }

    s_port[port].connected = 1;

    if (padRead(port, 0, &buttons) == 0)
        return 0;

    /* libpad reports buttons ACTIVE LOW: a 0 bit means pressed. */
    raw = 0xFFFF ^ buttons.btns;

    if (raw_out)
        *raw_out = raw;

    /*
     * Analog sticks read 0..255 with 128 at rest on a DualShock; a
     * digital pad reports a constant 0 for these fields, which our
     * -128 bias turns into a hard corner deflection, so only trust them
     * when the pad actually reported analog mode.
     */
    if (stick_x && stick_y && buttons.mode >> 4 != 0x4 /* not digital */) {
        *stick_x = (int)buttons.ljoy_h - 128;
        *stick_y = (int)buttons.ljoy_v - 128;
    }

    return map_buttons(raw);
}

void atlas_input_update(void)
{
    u32 held = 0;
    u16 raw = 0;
    int port;
    int connected = 0;

    s_stick_x = 0;
    s_stick_y = 0;

    for (port = 0; port < ATLAS_PAD_PORTS; port++) {
        u16 port_raw = 0;
        /* Both ports are merged: a user with a pad in slot 2 can drive
         * the menu without being told to move it. */
        held |= poll_port(port, &port_raw, &s_stick_x, &s_stick_y);
        if (port == 0)
            raw = port_raw;
        connected |= s_port[port].connected;
    }

    s_prev_held = s_held;
    s_held      = held;
    s_pressed   = held & ~s_prev_held;
    s_released  = ~held & s_prev_held;
    s_raw       = raw;
    s_any_connected = connected;

    /* --- key repeat ------------------------------------------------ */

    s_repeated = s_pressed;

    {
        u32 repeatable_held = held & ATLAS_REPEATABLE;

        if (repeatable_held != s_repeat_mask) {
            /* Direction changed (or released): restart the delay. */
            s_repeat_mask    = repeatable_held;
            s_repeat_counter = 0;
        } else if (repeatable_held) {
            s_repeat_counter++;

            if (s_repeat_counter >= ATLAS_REPEAT_DELAY_FRAMES) {
                int since = s_repeat_counter - ATLAS_REPEAT_DELAY_FRAMES;

                if (since % ATLAS_REPEAT_PERIOD_FRAMES == 0)
                    s_repeated |= repeatable_held;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Queries                                                             */
/* ------------------------------------------------------------------ */

u32 atlas_input_held(void)     { return s_held; }
u32 atlas_input_pressed(void)  { return s_pressed; }
u32 atlas_input_released(void) { return s_released; }
u32 atlas_input_repeated(void) { return s_repeated; }

int atlas_input_is_held(atlas_btn_t btn)
{
    return (s_held & (u32)btn) != 0;
}

int atlas_input_is_pressed(atlas_btn_t btn)
{
    return (s_pressed & (u32)btn) != 0;
}

int atlas_input_connected(void)
{
    return s_any_connected;
}

u16 atlas_input_raw(void)
{
    return s_raw;
}

int atlas_input_stick_x(void) { return s_stick_x; }
int atlas_input_stick_y(void) { return s_stick_y; }
