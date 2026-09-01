# Installing AtlasPS2 on a PS2

Written for someone who has not done this before. Every step says exactly
what to copy, which button to press, and what should appear on screen. If
what you see does not match what is written, stop and read
**If something goes wrong** at the end.

Allow twenty minutes the first time.

The French guide, [INSTALL_FR.md](INSTALL_FR.md), is the reference
version — it is checked first when either changes.

---

## What you need

* **A PlayStation 2**, any model, connected to a television.
* **An official Sony 8 MB Memory Card** — this is what AtlasPS2 will be
  installed on. Counterfeit "64 MB" and "128 MB" cards do not boot
  reliably and are not supported.
* **A USB stick**, formatted **FAT32**. 2 GB is plenty; very large sticks
  are often not recognised by the PS2.
* **A way to launch homebrew that you already have**: FreeMcBoot,
  uLaunchELF, Swap Magic, a modchip, or a card someone else prepared.
* **A controller** in port 1.

> **AtlasPS2 does not open your console.** It cannot turn a completely
> stock PS2 into one that runs homebrew — that has to be true already. If
> you have no way to launch an `.ELF` file, this guide does not apply to
> you yet: you first need a bootstrap (FreeMcBoot or equivalent), which is
> a separate operation with its own guides, and which depends on your
> console's exact model.

### What installing does not destroy

The installer **makes a backup before touching anything** and never
deletes the `BOOT`, `APPS` or `SYS-CONF` folders wholesale. Your game
saves are not touched: they live in other folders, and nothing here reads
or writes them.

That said, a Memory Card is a fragile twenty-year-old object. If it holds
saves you care about, copy them to a PC first, with uLaunchELF or an
adapter.

---

## Step 1 — Prepare the USB stick

1. Plug the stick into your computer.
2. If it is not FAT32, format it as FAT32. On Windows: right-click the
   drive → **Format** → file system **FAT32** → **Start**. This erases
   the stick.
3. Open `AtlasPS2-v0.1.0.zip`.
4. Copy these two files **to the root** of the stick — directly onto it,
   not into a folder:

   * `ATLAS_INSTALLER.ELF`
   * `ATLASPS2.ELF`

5. Also copy the archive's `ATLAS` folder to the root of the stick.

The stick should look like this:

```
E:\
├── ATLAS_INSTALLER.ELF
├── ATLASPS2.ELF
└── ATLAS\
    ├── LANG\
    └── THEMES\
```

`E:` is only an example; yours may be `D:` or `F:`.

6. Eject the stick properly. A stick pulled during a write can reach the
   PS2 with incomplete files.

> **Capitals matter.** The files must be named exactly
> `ATLAS_INSTALLER.ELF` and `ATLASPS2.ELF`, in capitals. If Windows hides
> extensions, check you do not have `ATLASPS2.ELF.txt`.

---

## Step 2 — Launch the installer

1. **Turn the PS2 off.**
2. Put the Memory Card you are installing to in **port 1** (the left one,
   facing the console).
3. Plug the USB stick into either front port.
4. Turn the PS2 on and start your usual homebrew environment (FreeMcBoot,
   uLaunchELF, and so on).
5. In it, open the USB device. Depending on the program it is called
   `mass:` or `USB`.
6. Highlight `ATLAS_INSTALLER.ELF` and press **✕** (cross).

The screen goes black for a second, then the installer appears: dark
background, **AtlasPS2 Installer** at the top, a list in the middle.

> **If the stick does not appear:** try the other USB port, then another
> stick. The PS2 is fussy about USB sticks, and this is by far the most
> common problem. It does not mean your console is faulty.

---

## Step 3 — Install AtlasPS2

The installer shows a summary at the top:

```
Console      0160EC20030325
Card         Memory Card (slot 1)
Free         6,128 KB free
Source       mass:/ATLASPS2.ELF
State        Not installed
```

Check two lines before continuing:

* **Card** must say *slot 1* (or *slot 2* if you used the right-hand
  port). That is the card that will be modified.
* **Source** must not say *None*. If it does, the installer did not find
  `ATLASPS2.ELF`: go back to step 1 — the file is in the wrong place or
  has the wrong name.

Then:

1. Move down to **Install AtlasPS2** with the stick or D-pad.
2. Press **✕**.
3. A confirmation appears naming the card. Read it. Press **✕** to
   confirm, or **○** to cancel.

The install runs, five lines ticking off one at a time:

```
Installing AtlasPS2

Checking                 OK
Backing up               OK
Copying program          OK
Configuration            OK
Verifying                OK

Installation complete.
```

It takes about a minute. **Do not remove the card or switch the console
off while it runs.** *Copying program* is the longest line: the program is
around 750 KB and a Memory Card writes slowly. A few seconds of an
apparently frozen screen are normal.

What each line does, if you want to know:

| Line | What happens |
|---|---|
| Checking | The card is readable, there is room, the source file exists |
| Backing up | Whatever booted the console before is copied to `ATLAS/BACKUP` |
| Copying program | AtlasPS2 is written under the temporary name `BOOT.NEW` |
| Configuration | The `ATLAS/` folders and default settings are created |
| Verifying | The copy is read back and compared, and only then becomes `BOOT.ELF` |

The last line is the one that matters: **the working boot file is only
replaced after the copy has been read back and verified.** If anything
fails before that, your console still boots exactly as it did.

4. When **Installation complete** appears, press **✕**.
5. Choose **Exit**, then switch the console off at the back.

---

## Step 4 — Restart

1. Leave the Memory Card in its slot. You can remove the USB stick.
2. Turn the PS2 on **without holding any button**.
3. After the Sony logo, AtlasPS2 appears: dark background, **AtlasPS2**
   top left, the version top right, and a list:

```
Games
Applications
Files
Devices
Video
Theme
Settings
System information
Power
```

On the very first boot a short wizard appears first: three questions
(language, display, and whether to scan for your applications). Answer
with **left/right**, confirm with **✕**. It appears only once.

**That is it.** Move with the D-pad, confirm with **✕**, go back with
**○**.

### Adding your own homebrew

Copy your `.ELF` files into the `ATLAS/APPS` folder on the Memory Card or
the USB stick. They appear under **Applications**. You can also launch
them from **Files**, wherever they are.

---

## If something goes wrong

Both combinations below are held **as you switch the console on**. Hold
the buttons from the moment you hit the switch, until a picture appears.

### The picture is distorted, cropped, black and white, or missing

Hold **R1** while switching on.

AtlasPS2 starts in safe video mode: NTSC, 4:3, no offsets. That works on
practically every television. Then go to **Video** and fix the setting
that caused it. That screen has a confirmation countdown: if you do not
confirm within fifteen seconds it reverts on its own — so it cannot lock
you out.

### AtlasPS2 will not start, crashes, or the screen stays black

Hold **L1 + R1** while switching on.

This is **Recovery mode**. It draws with no theme, without reading your
settings, with minimal graphics — precisely so that a broken theme or a
damaged configuration file cannot lock you out. It offers:

| Entry | What it does |
|---|---|
| Continue | Boot normally anyway |
| Reset settings | Deletes `ATLAS.INI` and starts from factory values |
| Disable theme | Returns to the built-in theme, which cannot be missing |
| Roll back | Restores the AtlasPS2 build from before the last update |
| Install update | From `ATLAS_UPDATE` on the USB stick |
| Change card | Work on the card in slot 2 |
| Return to browser | The console's own menu |

**Roll back** is the one to pick if the problem started right after an
update.

### The console boots as it did before and AtlasPS2 never appears

Your console is probably not booting from the Memory Card. That means the
bootstrap (FreeMcBoot or equivalent) is not on this card, or is on the
other one. AtlasPS2 installs *alongside* a bootstrap, it does not install
one — that depends on the console's exact model, getting it wrong can
leave a card the console refuses to boot, so the installer does not guess.

### The installer says there is not enough space

It refuses to install with less than 1 MB free, so that it cannot fill the
card to the point of damaging your saves. Delete a few game saves you no
longer need (using the console's own browser) and try again.

### The installer cannot find the source

Go back to step 1. `ATLASPS2.ELF` must be at the root of the stick, in
capitals, with no extra extension.

---

## Uninstalling AtlasPS2

There are two different operations and **they must not be confused**:

* **Uninstall** puts back whatever booted the console *before* AtlasPS2
  was installed. That is the one you want.
* **Roll back** restores the AtlasPS2 build from before the last update.
  That is not an uninstall.

To uninstall:

1. Copy `ATLAS_INSTALLER.ELF` to a USB stick, as in step 1.
2. Launch the installer. You can launch it from AtlasPS2 itself
   (**Files** → the USB stick → `ATLAS_INSTALLER.ELF` → **✕**).
3. Check that the **Card** line names the right card.
4. Choose **Uninstall AtlasPS2**, press **✕**, confirm.
5. Switch off, switch on. The console boots as it did before.

The installer reads back the backup made at install time
(`ATLAS/BACKUP/BOOT.ELF`). It was written **once**, at the first install,
and is never rewritten — otherwise, after an update, "uninstall" would
reinstall AtlasPS2.

If that backup does not exist (because the card was empty to begin with,
say), the **Uninstall** entry is greyed out and says so. In that case
there is nothing to restore: delete `BOOT/BOOT.ELF` and the `ATLAS`
folder with uLaunchELF, which returns the card to how it was.

Your AtlasPS2 settings stay in `ATLAS/CONFIG` after an uninstall. They
harm nothing, and if you reinstall later you get them back.

---

## If none of that works

Write down exactly what you see — the console model (the `SCPH-xxxxx`
label underneath), the **Console** line the installer shows, which step it
stops at, and the message if there is one. Those four things are what
anyone needs to help you. See [COMPATIBILITY.md](COMPATIBILITY.md) for the
table of configurations that have actually been tested.
