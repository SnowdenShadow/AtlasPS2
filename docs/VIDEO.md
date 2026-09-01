# AtlasVideo

The video module: what it can actually do, what it cannot, and why the
settings screen behaves the way it does.

## The claim this document exists to refuse

**Selecting a progressive or high-definition output mode does not make
anything render at that resolution.**

The PS2 renders into a framebuffer in the Graphics Synthesizer's 4 MB of
VRAM. The GS then scans that buffer out using the video timings of the
selected mode. These are two separate things:

| | What it is | What changes it |
|---|---|---|
| **Rendering resolution** | How many pixels are actually drawn | The program doing the drawing |
| **Output timing** | The signal sent to the television | The video mode |

AtlasPS2's interface renders at 640×448 (NTSC) or 640×512 (PAL) and that
does not change when you pick 480p. The 480p setting changes the signal
the television receives, not the number of pixels behind it.

The same is true of the games AtlasPS2 launches, and more strongly: a
game's rendering resolution is compiled into the game. Forcing a
different output mode cannot add detail that was never drawn. Anything
claiming a PS2 "runs at 1080p" is describing the cable, not the picture.

This is stated here, in the header of `include/atlas/video.h`, and marked
in the settings screen, because it is the most common false claim made
about PS2 video tools and users deserve to know what they are choosing.

## Modes

| Setting | Framebuffer | Signal | Notes |
|---|---|---|---|
| `AUTO` | follows the console | NTSC or PAL | The safe default |
| `NTSC` | 640×448 | interlaced, 59.94 Hz | |
| `PAL` | 640×512 | interlaced, 50 Hz | |
| `480p` | 640×448 | progressive | **Experimental.** Needs component or VGA |

### AUTO

AUTO reads the console's own region with `gsKit_check_rom()` and picks
NTSC or PAL to match. That is the only console detection this module
does, and it is the one that is reliable.

AUTO is a real choice, not the absence of one — it means "whatever this
console is", which is a different instruction from "PAL". That
distinction matters in per-title profiles, where a profile saying `auto`
must be able to override a global setting of PAL.

### 480p is marked experimental and is never the default

480p output requires a component (YPbPr) or VGA connection. Over the
composite or RGB SCART cable most consoles are used with, selecting it
produces **no picture at all** — not a degraded one, no signal.

It is therefore offered, marked, and never selected on your behalf. The
spec's rule is the right one: experimental modes must be clearly marked
and never forced by default.

Two things make picking it recoverable:

* The video screen has a **fifteen-second confirmation countdown**. Apply
  a mode, and if you do not confirm, it reverts to what was there before.
  A television showing nothing cannot show you the confirmation prompt —
  which is exactly the case the countdown is for.
* Holding **R1** at boot forces NTSC 4:3 with no offsets, regardless of
  what is stored.

Nothing is written to the Memory Card until you confirm. Power-cycling
during the countdown leaves the old setting in place.

## Aspect

| Setting | Effect |
|---|---|
| `AUTO` | 4:3 unless the console itself is configured for 16:9 |
| `4:3` | |
| `16:9` | UI x-coordinates are scaled so the layout keeps its proportions |

16:9 here is anamorphic: the same framebuffer, horizontally scaled, which
is how the PS2 does widescreen. The interface is laid out for 4:3 and
`atlas_video_x_scale()` compensates, so a 16:9 selection does not stretch
the text.

This setting affects **AtlasPS2's own interface only**. A game's internal
rendering aspect is a property of the game; AtlasPS2 patches nothing.
That is also why a per-title profile's `widescreen` field is recorded and
not acted on — it carries what the user knows about the title, for the
launcher AtlasPS2 hands over to.

## The safe area

Every UI coordinate in AtlasPS2 is relative to the **safe area**, never
to the framebuffer.

A CRT does not show the whole picture. How much it hides varies by set,
by decade, and by how the tube has aged; ten percent lost on each edge is
normal and twenty is not unusual. A menu laid out to the framebuffer's
edges is a menu with its title cropped on a real television — and the
person it happens to has no way to know that the software was drawing it.

So the module computes an inset rectangle and hands out four numbers:

```c
int atlas_video_safe_x(void);
int atlas_video_safe_y(void);
int atlas_video_safe_w(void);
int atlas_video_safe_h(void);
```

The layout constants (`ATLAS_UI_HEADER_H`, `ATLAS_UI_PAD`, and the rest)
are all measured inside that rectangle. The result is one layout that
survives both a 448-line NTSC frame and a 512-line PAL one without a
second set of numbers.

### Overscan

`overscan_x` and `overscan_y` (0–64 pixels) shrink the safe area further,
for a television that hides more than the default assumes.

Note the direction: overscan makes the interface *smaller*, moving it
away from the edges. If your television is cutting the title off, this is
the setting.

### Offsets

`offset_x` and `offset_y` (−32 to 32 pixels) move the whole picture. This
is for a set whose picture is not centred — a very common CRT fault.

Offsets and overscan are applied by `atlas_video_set_trim()`, which
deliberately does **not** re-open the screen. These are the four settings
a user adjusts by eye, one press at a time, watching the picture move.
Routing them through a full mode switch would cost a black frame and a
television re-sync per press, which makes the thing being adjusted
impossible to see.

## Safe video mode

Hold **R1** while switching the console on.

That forces NTSC, 4:3, no offsets, no overscan, and skips the stored
`[video]` settings entirely. It exists for the case where a video setting
produced a picture the television cannot show — where the normal way to
fix the setting is behind the broken setting.

Recovery mode (**L1 + R1**) implies safe video, for the same reason and
more strongly: it must come up on any television, on a console whose
configuration is what broke.

## Per-title video

A profile at `mc0:/ATLAS/PROFILES/SLUS_20946.INI` can set `video_mode`,
`aspect_ratio`, `offset_x` and `offset_y` for one game. Fields it does
not mention are left alone — every field has an unset state distinct from
its zero value, so a profile naming only the aspect does not silently
reset the screen trim.

`overscan_x` and `overscan_y` are deliberately **not** profile fields.
Overscan trims the safe area AtlasPS2 draws inside, and the program
AtlasPS2 launches draws its own — so a per-game overscan would be a
setting with no effect on the thing the user was looking at when they set
it.

## What the module does not do

* **No GSM-style mode forcing for games.** AtlasPS2 sets its own output
  mode and releases the GS before handing over. What the game then does
  is the game's business.
* **No scanline, filter or upscaling effects.**
* **No 1080i.** It would be an output timing over a 640×448 framebuffer,
  which is the claim at the top of this document.

## Reading the code

| File | Contents |
|---|---|
| `src/video/video.c` | The GS: gsKit setup, mode switching, the frame cycle |
| `src/video/video_cfg.c` | Modes, labels, limits and parsing — no gsKit |

The split is why `test_config` can run on the build machine: what a mode
*means* and what its limits are is pure data, and a check for it should
not need a television.
