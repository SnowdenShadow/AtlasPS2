# Themes

A theme is a folder with a `theme.ini` in it. Fifteen colours, no code,
no images.

## Where it goes

```
mc0:/ATLAS/THEMES/Midnight/theme.ini
mass:/ATLAS/THEMES/Midnight/theme.ini
```

Memory Card slot 1, then slot 2, then USB — the first one carrying that
folder wins. That order is deliberate: a card stays in the console, so
a stick you borrowed does not quietly take over the console's look.

The **folder name is the theme name**. It is what the menu shows and
what goes into `ATLAS.INI`, so keep it short and keep it under 32
characters — anything longer is skipped rather than truncated, because
a shortened name would name a folder that does not exist.

## Choosing one

**Home → Theme.** The list is what is actually on the attached devices,
plus a first row for the built-in theme.

Highlighting a row applies it immediately, so you judge a theme by
looking at it. Nothing is written until you choose **Save**; pressing
Back puts the previous theme straight back.

`assets/themes/Midnight/` in this repository is a complete example.
Copy the folder and edit it.

## The file

```ini
[colors]
bg_top   = #0A0D18
accent   = #7C6BFF
text     = #EEF1F8
```

The `[colors]` header is optional and ignored — a theme file has one
section at most and its name is the folder it sits in. `#` and `;`
start a comment, and a comment may follow a value on the same line.
CRLF line endings are fine.

### Colours

`#RRGGBB`, or `#RRGGBBAA` to set transparency. The `#` is optional, and
hex digits may be upper or lower case.

**Alpha is on the PlayStation 2's own scale, where `80` is fully opaque.**
Above that the Graphics Synthesizer over-saturates rather than getting
brighter, turning a panel into a white block. A value over `80` is
clamped down to it, so writing the `FF` that every other system spells
"opaque" gives you an opaque colour — not a bug you would have had no
way to diagnose from looking at the number.

### The fifteen keys

| Key | What it paints |
|---|---|
| `bg_top` | Background gradient, top |
| `bg_bottom` | Background gradient, bottom |
| `panel` | The panel behind a menu row |
| `panel_selected` | The panel behind the highlighted row |
| `separator` | Hairlines, and unlit indicator dots |
| `text` | Primary text |
| `text_dim` | Secondary text: details, hints, paths |
| `text_on_accent` | Text drawn on top of the accent colour |
| `accent` | The interface's one strong colour |
| `accent_dim` | Its inactive companion |
| `warn` | Warnings, and reverted settings |
| `error` | Failures |
| `ok` | Success, and a device that is present |
| `bar` | The header and footer bars |
| `bar_text` | Text in those bars |

You do not have to set all fifteen. Anything you leave out keeps its
built-in value, so a file naming only `accent` is a theme with one
colour changed — not one with fourteen invisible ones.

## What happens to mistakes

Nothing that stops the console from starting.

- An unknown key is ignored. A theme written for a later version of
  AtlasPS2, with more colours in it, still loads on this one.
- A value that is not a colour leaves **that one colour** at its
  built-in default. `accent = blue` costs you the accent, not the file.
  Nothing is guessed: a half-parsed typo would give you a colour you
  never chose and could not account for.
- A `theme.ini` that is missing, empty or unreadable leaves the
  **built-in theme** active. The built-in theme is compiled into the
  ELF. It cannot be deleted or edited, which is what makes every
  statement above safe.
- Recovery mode (hold **L1 + R1** at boot) ignores themes entirely and
  draws in fixed colours.

## Designing for a television

Test on the hardware before you keep a theme. Colours picked on a
computer monitor routinely turn out unreadable across a room.

- **A CRT blooms.** Thin light text on a dark ground spreads. Keep the
  contrast between `text` and `bg_*` high, and do not make `text_dim`
  so dim it disappears.
- **Saturated reds bleed** into the pixels beside them, taking the
  legibility of anything sitting on them with it. The built-in `error`
  colour is desaturated for exactly this reason.
- **Keep `bg_top` and `bg_bottom` close.** A wide gradient bands
  visibly; there is no dithering to hide it.
- **`text_dim` earns its name.** The distance between it and `text` is
  what makes a secondary line read as secondary at three metres. Close
  that gap and the whole screen reads as one flat block.
- **`text_on_accent` is not decorative.** It is drawn on top of
  `accent`, so those two are the one pair that must contrast with each
  other rather than with the background.

## Not supported

`background.png` and per-theme icons are not read by this version. The
theme system is colours only; a file naming them is not an error, the
keys are simply ignored.
