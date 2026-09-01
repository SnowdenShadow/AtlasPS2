# Recovery mode

Hold **L1 + R1** while switching the console on.

Recovery is the answer to one question: *what happens when AtlasPS2's own
configuration is what broke it?* A launcher that can be locked out by its
own settings is a launcher that can take the console with it, and on a
Memory Card boot there is no second copy to fall back to.

So Recovery is built to a rule: **it depends on nothing the user can
change.**

## What it deliberately does not use

| Not used | Why |
|---|---|
| The theme | A theme is a file on a card. A bad one must not be able to make the escape hatch invisible — so Recovery draws with fixed colours compiled into the ELF. |
| `ATLAS.INI` | The configuration is not read at all. Not "read and defaulted on failure" — not read. A parser is code, and code that runs is code that can fail. |
| The font atlas beyond plain text | Minimal drawing: rectangles and text. No images, no theme assets. |
| Language overrides | Translation files are read from the card too. Recovery uses the compiled-in strings. |
| The video settings | Recovery implies safe video: NTSC 4:3, no offsets. It must come up on any television. |

The consequence is that Recovery is plainer than the rest of AtlasPS2,
and that is the point. Everything it could look nicer with is a thing
that could be missing.

## Getting there

Hold both shoulder buttons from the moment you hit the power switch,
until a picture appears. The buttons are sampled during startup, before
the configuration is read.

If it does not appear:

* The controller must be in **port 1**.
* Some third-party controllers report shoulder buttons late. Try an
  official one.
* If the console does not reach AtlasPS2 at all — it boots straight to
  the Sony browser — then the problem is before this: the card is not
  what the console boots from, and Recovery is not involved. See the
  install guide's "The console boots as it did before".

## What it offers

| Entry | What it does | When you want it |
|---|---|---|
| **Continue** | Boot normally anyway | You held the buttons by accident, or you want to see whether the fault is still there |
| **Reset settings** | Delete `ATLAS.INI` and start from factory defaults | A setting is wrong and you do not know which |
| **Disable theme** | Return to the built-in theme | The interface is unreadable, or a theme folder went missing |
| **Roll back** | Restore the AtlasPS2 build from before the last update | It broke immediately after an update |
| **Install update** | Install from `ATLAS_UPDATE/` on USB | You have a fixed build to apply |
| **Change card** | Work on the card in the other slot | You installed to slot 2 |
| **Return to browser** | The console's own menu | Nothing here helped and you want out |

### Reset settings

Deletes the configuration file. The next normal boot finds no file at
all, which is what triggers the first-boot wizard — so you will be asked
for your language again. That is correct: nothing was recovered, the
settings genuinely are gone.

Note that this is different from what happens when a config file is
*damaged*. In that case AtlasPS2 recovers from `ATLAS.INI.BAK` on its own
during a normal boot, says so, and does not need Recovery at all.

### Roll back is not uninstall

These two are easy to confuse and confusing them loses something:

* **Roll back** swaps `BOOT/BOOT.ELF` with `BOOT/BOOT.BAK` — the
  AtlasPS2 build from before the last update. Because it is a swap rather
  than a copy, doing it twice returns you to where you started.
* **Uninstall** (in the installer, not here) restores
  `ATLAS/BACKUP/BOOT.ELF` — whatever booted the console *before AtlasPS2
  was ever installed*. That backup is written once and never rewritten,
  precisely so that an uninstall after several updates does not
  "uninstall" by reinstalling one of our own builds.

Recovery offers rollback and not uninstall, because rollback is what a
user in Recovery actually needs: something worked yesterday and does not
today.

### Install update

Reads `ATLAS_UPDATE/ATLASPS2.ELF` from a USB stick. With no stick
present, or no file on it, it says so and does nothing.

The update goes through the same transaction as any other install: the
new program is staged as `BOOT.NEW`, read back and verified, and only
then does it become `BOOT.ELF` with the previous one moved to `BOOT.BAK`.
**The working `BOOT.ELF` is never opened for writing.** A card pulled
mid-update leaves the console booting exactly as it did.

## Why Recovery and the installer share one engine

The installer is a separate ELF and has to be: it writes the file the
console boots, so repairing a broken installation must not require
running the broken installation.

But Recovery needs the same operations — a rollback and an update are
what people reach Recovery to do. Two implementations of a transaction
that swaps the boot file would be two chances to get the rollback wrong,
and only one of them would be exercised on any given run.

So the engine lives in `src/core/install.c`, both ELFs link it, and there
is exactly one implementation of the swap. What differs between the two
programs is where the new ELF comes from, not what happens to it.

## If Recovery itself will not come up

Then the problem is not AtlasPS2's configuration, and the remaining
options are outside it:

1. **Try the other Memory Card slot**, if you have a second card.
2. **Boot your original homebrew environment** (uLaunchELF, FreeMcBoot's
   own menu, Swap Magic) and run `ATLAS_INSTALLER.ELF` from USB. The
   installer can repair, roll back or uninstall without AtlasPS2 running
   at all — which is the reason it is a separate program.
3. **Delete `BOOT/BOOT.ELF`** from the card with uLaunchELF. The console
   returns to whatever it did before, and you can reinstall later.

Step 2 is the one to reach for. It is the case the installer's
separateness was designed around.
