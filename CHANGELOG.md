# Changelog

All notable changes to AtlasPS2 are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Until 1.0.0 the minor number carries breaking changes: 0.x releases are
pre-release milestones, not a stable interface.

## [Unreleased]

### Added

- **Disc images are read and identified.** ISO and ZSO are presented to
  the rest of the program as a flat array of 2048-byte sectors;
  `SYSTEM.CNF` is parsed off that, giving the title, the game ID and the
  region. The format is decided by the file's magic and never by its
  extension, because a renamed file is an ordinary thing to find on
  someone's drive and the ISO path would read a ZSO's compressed bytes
  as a disc with no volume descriptor.
- **ZSO, and not CSO or CHD.** LZ4 decompression is a byte-copy loop
  with no entropy decoding and no window state, which is what a 37 MHz
  IOP can do while keeping a game's audio streaming fed. CSO is the same
  idea with DEFLATE, and a format that works until a cutscene needs
  audio is worse than one that is absent.
- **The LZ4 decoder is checked against the reference compressor**, not
  against itself. `tools/genlz4vec.py` emits the committed vectors; a
  decoder checked only against its own encoder can be uniformly wrong
  and still agree with itself, and this one runs over every sector a
  compressed image is read through.
- **The same disc, compared as an ISO and as a ZSO, sector by sector.**
  That pairing found two real bugs in the ZSO reader that no fixed
  expected value could have: a wrong index entry returns real data,
  correctly decompressed, from the wrong place.
- **A game ID that does not fit is refused, not truncated.** A truncated
  ID silently matches another game's compatibility entry, and nothing on
  screen would say so.
- **Per-game settings, in a file the user edits.**
  `ATLAS/CONFIG/COMPAT.INI` carries one section per game: the workaround
  flags a title needs and the video mode to hand it. AtlasPS2 ships no
  entries of its own - a table baked into the ELF is a compatibility
  list that can only be corrected by rebuilding, for a body of knowledge
  players collect and we do not.
- **Booleans are read the way people write them**, and matched whole.
  `1`, `yes`, `true`, `on` and their negatives, in any case; `perhaps`
  is not a boolean and `purple` is not a video mode, and both are
  logged and skipped rather than taken for their first letter.
- **One bad line does not cost the file.** A section header that is not
  a game ID is rejected and logged, an unknown key is skipped, and every
  good entry around them still loads: a list shared between launchers
  carries keys AtlasPS2 does not implement.
- **An unknown region stays AUTO rather than guessing NTSC.** A PAL
  title told it is NTSC runs a fifth too fast with the bottom of the
  picture cut off, which reads as a bad dump rather than as a setting
  the user can change.
- **`docs/DISC.md`** - what is implemented, and the exact contract the
  IOP-side drive emulation has to meet. The `cdvdman` replacement is not
  in this repository: it cannot be checked on a build machine, and a
  wrong one gives a black screen the user cannot tell from a bad dump.
- **Favorites and a recently-used list.** Square marks the highlighted
  application; the applications screen then grows a "Recently used" and
  a "Favorites" group above the full list. Both are stored in
  `ATLAS/CONFIG/FAVORITES.INI` on the first writable device, in the
  same Memory-Cards-before-USB order the rest of the configuration
  uses.
- **A favorite is a path, not a row.** The catalogue is rebuilt every
  time a device appears or disappears, so an index into it names a
  different program an hour later. A stored path that does not
  currently resolve is kept and simply not drawn: unplugging a USB
  stick must not silently empty someone's favorites.
- **A full favorites list refuses rather than evicting.** Dropping
  somebody else's entry to make room for a new one loses something the
  user never asked to lose, and they would find out by not finding it.
- **The card is written only when something actually changed.**
  Relaunching the program already at the top of the recent list is the
  single most common thing that list ever sees, and it now costs no
  write at all. The list is saved before the launch, not after: after a
  successful launch this program has been replaced and there is no
  "after" to write anything in.
- **The settings screen.** Eleven sections in one list: general,
  display, devices, applications, boot, theme, language, system
  information, advanced, recovery and about. Most rows are doors rather
  than controls - video, theme, devices and applications each already
  have a screen that owns their settings, and a second copy of the
  video clamps is one more place for them to disagree. The one that
  disagrees leaves a television showing nothing.
- **Saving touches only the keys this screen edits.** It reads
  `ATLAS.INI`, replaces six values and writes it back, so setting a
  video mode from the screen this one opens is not undone by pressing
  Save on the way out. The screen deliberately holds those six fields
  rather than a whole configuration struct, because holding the struct
  is what would make the stale write possible in the first place.
- **The language applies the moment it is chosen, and reverts if you
  leave without saving.** Otherwise the warning that says "leaving now
  puts back the previous settings" would be false for the one setting
  the user can see with their own eyes.
- **The auto-launch countdown is escapable by any button**, and the
  path is checked before the timer starts rather than after it runs
  out. A stored default application that no longer resolves - a USB
  stick left unplugged - now costs nothing instead of several seconds
  of watching a counter run down to a failure. It never runs on a
  recovery boot: the stored default is a plausible candidate for what
  is wrong, and the escape hatch must not escape into the same trap.
- **The startup setting now decides the first screen.** It was parsed,
  editable and ignored; `startup = apps` opens the application list.
- **System information reports what can actually be known.** Console
  region from the GS ROM, video mode and resolution, per-device free
  space, and the state of all five module groups. There is no PS2SDK
  version macro to print, so it prints the compiler and the build date
  instead of inventing one, and there is no network stack in this
  build, so the IP row says exactly that rather than showing a
  fabricated address. The console's ROM version string is deliberately
  absent: it is an identifier, and nothing here needs it.
- **"Restart AtlasPS2" appears only when we know where we came from.**
  `argv[0]` is kept at startup and checked once the filesystems are up;
  a launcher that passes nothing, or a bare `BOOT.ELF` with no device
  prefix, leaves the entry hidden. The alternative is a menu row that
  fails after the IOP has been reset and the GS released, at the one
  moment in the program where nothing is left that can draw an error.
- **There is no "Restart Console", because the PS2 has no software cold
  reset.** What the SDK offers is `ExecOSD()`, which is the row above it
  already described honestly as returning to the browser. A second row
  calling the same syscall under a name that promises a power cycle
  would be the same action with a false label.
- **A first-boot wizard: three questions on one screen.** Language,
  display and whether to scan for applications, then the menu. It runs
  only when no configuration file was found at all - a file that was
  damaged and recovered from its `.BAK` is a repaired install, not a
  new one, and asking that user to set their language up again would
  say otherwise. A failed write does not stop the console reaching its
  menu; it says so and continues, and asks again next boot, which is
  correct because nothing was recorded.
- **The wizard shows the display setting without offering to change
  it.** It is the one setting capable of making the wizard itself
  invisible, and unlike the video screen this has no confirmation
  countdown to escape with. Holding R1 at boot is the escape, and the
  row says so.
- **Clearing favorites now reaches the card.** `atlas_fav_reset()`
  clears the dirty flag by design - it is what a load starts from - so
  a reset followed by a save wrote nothing and the favorites came back
  at the next boot. Clearing on the user's behalf is a separate call
  that marks them changed.
- **Milestone 9 - the file manager.** Browse every mounted device from
  one tree, launch an ELF, copy, move, rename, create a folder, delete
  a file, delete an empty folder. File sizes are shown, and the
  clipboard says what is waiting and whether it is a copy or a move -
  the two look identical until one of them removes the original.
- **Dangerous operations ask, and the dialog names the file.** "Are
  you sure?" over a box that does not repeat the target is how the
  wrong thing gets deleted by somebody who was sure about a different
  one. Cross only opens folders; everything that changes a file is
  behind Square, so the button pressed on every other screen cannot
  delete anything.
- **System files ask a second time, with different words.** Deleting
  `mc0:/BOOT.ELF` looks exactly like deleting anything else, and the
  consequence arrives at the next power cycle, long after the user has
  forgotten which row they were on. The second dialog says what happens
  to the console rather than repeating the first one louder. The list
  of protected paths is deliberately generous, and anchored: `BOOT.ELF`
  at a card root is what the console starts, while one three folders
  down is somebody's homebrew - warning about the second would teach
  the user to press through the warning that matters.
- **There is no recursive delete, and no recursive copy.** A D-pad, a
  television and no undo is the worst place in computing to offer
  "delete everything under here". A folder is emptied where the user
  can see each thing go, and a non-empty folder is refused by
  AtlasPS2 itself rather than trusted to a driver - some Memory Card
  implementations remove one and leave its contents unreachable, which
  is deleting them without asking.
- **A move copies, verifies, then deletes.** Within one device it is a
  rename and cannot half-succeed; across devices the copy is checksummed
  back before the original goes, and a failure leaves the original
  where it was and says so. A copy onto itself is refused: on most
  drivers the destination is opened for writing before the source is
  read, which truncates the file to nothing.
- **Long copies keep drawing.** A frozen picture for the seconds a
  700 KB write to a Memory Card takes reads as a hang, and the user's
  reflex is to pull the card during the one operation that must not be
  interrupted.
- The path rules are a host-side self-check (`test_fs_path`, 9 suites
  now). `atlas_fs_is_protected()` decides whether a user is warned at
  all, and the only other way to find out it was wrong is to wreck a
  Memory Card.
- **Milestone 9 - themes.** A theme is a folder holding a `theme.ini`
  with fifteen colours in it, read from `ATLAS/THEMES/<Name>/` on a
  Memory Card or a USB stick. Memory Cards are searched before USB, the
  same order the configuration uses: a card stays in the console, so a
  borrowed stick does not quietly take over how it looks.
- **The built-in theme is compiled in and cannot be removed**, which is
  what makes every other guarantee here safe. A theme file that is
  missing, empty, truncated or full of typos leaves a readable
  interface. Deleting the theme a console boots with costs colours, not
  the console.
- **A bad line costs one colour.** Unknown keys are ignored, so a theme
  written for a later version with more colours in it still loads; a
  value that is not a colour leaves that field at its built-in default
  rather than being half-parsed into a colour the author never chose.
  A file naming only `accent` is a theme with one colour changed, not
  one with fourteen invisible ones.
- **Alpha is clamped to the GS scale.** On the Graphics Synthesizer
  `0x80` is opaque and higher values over-saturate into a white block.
  A theme asking for the `0xFF` that every other system spells "opaque"
  gets an opaque colour, because an author has no way to know that from
  looking at the number.
- **Home > Theme** lists what is actually on the attached devices,
  plus the built-in theme, rather than offering a text field to type a
  folder name into on a console with no keyboard. A folder without a
  `theme.ini` in it is not listed, and the same theme on a card and a
  stick is one row.
- **Highlighting a theme applies it.** Colours are the one setting
  whose whole meaning is how they look, and a preview costing a press
  and a screen transition is a preview nobody uses. Leaving without
  saving puts back the theme that was live on entry - restored from the
  running module rather than by re-reading `ATLAS.INI`, so a boot where
  the configured theme failed to load does not retry that failure on
  the way out of the screen the user came to fix it from.
- **Save writes the theme that is on the screen**, not the row the
  cursor last passed over. A theme that failed to load leaves the
  built-in palette up, and writing its name anyway would save a setting
  that reproduces that failure on every boot from then on.
- **`docs/THEMES.md` and `assets/themes/Midnight/`** - the full key
  reference, the rules about mistakes, and a complete example to copy,
  with the CRT notes that matter: a bloom on thin light text, bleeding
  saturated reds, and banding across too wide a gradient.
- The theme parser is a host-side self-check (`test_theme`, 7 suites
  now). A theme file's colours are exactly what is wrong when a theme
  looks wrong, and a check that needs a television is not a check. It
  also parses the shipped example and asserts all fifteen fields land,
  since that file is what every theme author starts from.
- Recovery mode still ignores themes entirely and draws in fixed
  colours. That is unchanged and is the reason it can be trusted to
  repair a console whose theme is the thing that broke it.

- **Milestone 8 - the video screen.** The Home entry that opened a
  placeholder now opens the settings AtlasVideo has always had: display
  mode, aspect, screen position and margin, edited with Left and Right
  and applied to the picture immediately.
- **A change to the mode or the aspect reverts on its own** unless the
  user confirms it from inside the new mode. This is the one screen
  whose settings can destroy the means of using it - a television that
  cannot lock onto 480p shows nothing, and a user who cannot see the
  menu cannot press Back to leave it. The rule is that no press can
  leave a screen the user cannot read, and neither can the absence of
  one. Circle during the countdown reverts immediately rather than
  making a user who *can* see the screen sit out the timer.
- The fallback is read from the running video module, not from
  `ATLAS.INI`. On a boot where R1 skipped the stored settings the file
  names a mode that was deliberately never applied, and reverting to it
  would drop the user into the thing they held a button to escape. Two
  changes in a row both fall back to the last mode a user actually
  confirmed, not to the previous unconfirmed attempt.
- Position and margin are not guarded that way, and they do not re-open
  the screen either: `atlas_video_set_trim()` moves the picture without
  a mode switch, because a black frame and a TV re-syncing between every
  press makes an adjustment done by eye impossible to perform.
- Nothing is written until Save is chosen, and Save rewrites only the
  `[video]` block of an `ATLAS.INI` that is read first - a video setting
  should not cost the user their language. Reset is the screen's own
  copy only, so a reset nobody meant costs a trip back through the menu
  rather than their settings.
- AUTO shows what it resolved to (`auto (pal)`), since "auto" alone does
  not tell someone whose picture is rolling what the console decided -
  which is the first thing they need to know. The mode values are not
  translated: they are the same tokens that go into `ATLAS.INI`, and a
  French screen naming them differently would send a user looking for
  something the file does not contain.
- The screen says, in both languages, that the mode sets the output
  signal only and the interface is always drawn at 640x448 or 640x512.
  A user selecting 480p is exactly the user about to assume otherwise.
- The clamp limits moved to `config.h`. A screen that let a user pick an
  offset the parser clamps would write a file that reads back different,
  and the setting would appear to change on its own at the next boot.
- **A language file can no longer break a format string.** Four strings
  are `printf` formats and the caller passes the arguments the built-in
  text asks for, so an override writing `%s` where the original had `%d`
  reads an int as a pointer - on a console with nowhere to report it.
  Overrides whose conversions differ are refused and the built-in text
  stands; translators may still reorder the words around them freely.
  `make check` also sweeps all 136 strings for English and French
  agreeing on their conversions, using its own comparison rather than
  the module's, so a bug shared between the two cannot hide the case.

- **Milestone 7 - recovery mode.** Holding L1+R1 while the console
  powers on comes up in Recovery instead of the normal interface: no
  configuration is read, no theme is loaded, and video is forced to the
  safe NTSC 4:3 defaults. It is the screen for the case where AtlasPS2
  itself is what is broken, so it assumes nothing survived.
- Recovery is its own **root**, not a screen pushed over Home. Home is
  drawn from a configuration this boot deliberately did not read, and a
  Back that fell through to it would land the user in the thing they
  held two buttons to escape. The way out is the "Start AtlasPS2
  normally" entry, or the one that returns to the PS2 Browser - both
  things a user can read before choosing.
- Its seven options are ordered safest first: start normally, reset the
  configuration, disable a custom theme, restore the previous version,
  install an update from USB, change card, return to the Browser. The
  two that change what the console boots ask a second time; the two that
  only touch a settings file do not, because a confirmation on every
  entry trains a user to press through the one that matters.
- Reset writes the defaults through the same atomic path everything else
  uses, so the previous `ATLAS.INI` survives as `ATLAS.INI.BAK` - the
  fix is reversible with a card reader, which is what the on-screen text
  promises. Disabling a theme rewrites only the theme name and leaves
  the rest of the user's settings alone: clearing their video mode to
  fix their colours would be a repair that breaks something else. On a
  console with no readable configuration at all it writes nothing and
  says so, rather than quietly doing what Reset does.
- **The install engine now lives in the shared tree** (`src/core/`) and
  both ELFs link it. Recovery needs reinstall and rollback, which is
  exactly what the installer already does; a second transactional
  `BOOT.ELF` swap would have been two chances to get the rollback wrong,
  with only one of them exercised on any given run. The progress screen
  moved with it, so the "do not remove the card" line has one home.
- A new operation, `ATLAS_OP_ROLLBACK`: swap `BOOT/BOOT.ELF` with
  `BOOT/BOOT.BAK` to return to the AtlasPS2 build that was live before
  the last update. It is **not** Restore, which reaches past every
  AtlasPS2 build to whatever booted the console originally. Because it
  is a swap rather than a copy, doing it twice returns to where it
  started - and it is offered only on a card AtlasPS2 actually boots,
  so a `BOOT.BAK` left by someone else's installer is never swapped in.
  The `INSTALLED.TXT` marker is left untouched by a rollback: the live
  build is now the previous one, whose version this program has no way
  to know, and a marker that lies is worse than one that is stale.
- Updates from USB read `mass:/ATLAS_UPDATE/ATLASPS2.ELF` first, ahead
  of every other search location - a stick holding both a fresh download
  and last month's copy in its root should not install last month's. A
  missing stick is reported beside the entry the user pressed rather
  than behind a screen transition that looks like the copy started.
- The per-operation source path is now derived in one place
  (`source_for()`), used by check, copy and verify alike. Copy and
  verify must agree on it or verification compares the staged file
  against something it was never made from; check uses it to fail
  immediately rather than after three steps have reported success.

- **Milestone 6 - the installer.** `ATLAS_INSTALLER.ELF`, a second
  program built from the same tree, that puts AtlasPS2 onto a Memory
  Card and can take it off again. It is separate rather than a screen
  inside the launcher because repairing a broken installation would
  otherwise mean running the broken installation - and a launcher that
  will not start is exactly when the installer is needed.
- Six operations: install, update, repair, backup, restore, uninstall.
  Each one is five steps - check, backup, copy, config, verify - and the
  engine runs exactly one step per call so the screen can draw between
  them. The steps an operation does not need are marked skipped rather
  than hidden, so the same five lines appear every time and the list
  reads the same way on the run that goes wrong.
- **The working `BOOT.ELF` is never written over.** The copy lands as
  `BOOT/BOOT.NEW`, is checksummed after the card is synced, and only
  then does the live file rotate to `BOOT/BOOT.BAK` and the staged file
  take its place. If the rename fails the old file is put straight back.
  Every outcome except the successful one leaves the card booting what
  it booted before.
- Two different backups, deliberately not the same file.
  `BOOT/BOOT.BAK` is one transaction's rollback slot and is overwritten
  every update. `ATLAS/BACKUP/BOOT.ELF` is whatever booted the console
  *before* AtlasPS2 was ever installed: written once, never refreshed,
  and never taken from a `BOOT.ELF` that is already AtlasPS2. Conflating
  them would make uninstall reinstall AtlasPS2 after the first update.
  A plain-text `ORIGINAL.TXT` beside it says what the file is, so it is
  not deleted as junk.
- Nothing is deleted to make room. Install creates `ATLAS/CONFIG`,
  `ATLAS/APPS`, `ATLAS/LANG` and `ATLAS/THEMES` and writes a default
  `ATLAS.INI` only when one is not already there; update and repair skip
  the config step entirely, because an update that rewrote `ATLAS.INI`
  would silently discard the video mode a user needs to see the screen
  at all. `SYS-CONF` and the rest of the card are never touched.
- **It refuses to install a bootstrap or exploit**, and says so on a
  full screen rather than in a footnote. Which exploit a console needs
  depends on its model and ROM version; guessing is what costs people
  their Memory Card. The console's ROM name is read and displayed for
  reference, and acted on by nothing.
- Progress is live during the copy, not only between steps: a verified
  copy of a 700 KB ELF is three passes over the file and long enough
  that a still picture reads as a hang - and the reflex then is to pull
  the card, during the one operation on it that must not be interrupted.
  The engine calls back into the screen mid-step and that callback
  renders a whole frame. There is no cancel button for the same reason.
- The source ELF is looked for on USB before the Memory Cards, since
  that is where a release archive gets unzipped and a stale copy on a
  card should not win over the one just plugged in. Install, update,
  restore and uninstall each ask a second time before running; backup
  and repair do not, because a confirmation on every entry trains a user
  to press through the one that matters.
- The installer reads no configuration, no theme and no language file.
  It runs before any of those exist on the card, and its own text is
  compiled in - the same table the launcher uses, now 116 strings in both
  languages.
- `make installer` builds it; `make all-elf` builds both. Objects live
  in their own tree: the two programs compile the same sources with the
  same flags, but they have different entry points, and a shared tree
  would make "which ELF is stale" unanswerable.
- `atlas_file_rename()` in the file layer, so the installer's activation
  swap goes through the same place as every other file operation instead
  of calling fileXio behind its back.
- `make check` covers CRC-32 against the published IEEE 802.3 check
  value (`0xCBF43926` for "123456789") rather than against itself: a
  typo in the table would otherwise produce a stable, wrong hash that
  the installer would verify every copy against forever. It also pins
  chunked chaining - if that broke, verification would fail on every
  file larger than one chunk.

- **Milestone 5 - configuration and translations.** Settings survive a
  power cycle, and every word on screen comes from a table that can be
  replaced from a Memory Card.
- `atlas_i18n_*`: English and French, with French as the reference
  translation. Both languages are compiled into the ELF and a file only
  ever *overrides* a string, never supplies one - Recovery has to draw a
  readable screen when the Memory Card is exactly what failed, and a
  launcher whose error messages live on the device that broke shows a
  blank screen at the one moment it matters. Keys are an enum, so a
  lookup is an array index rather than a hash of a string, and the
  X-macro table carries the key, its file name and both translations on
  one row: a key cannot be added without a French string, because there
  is nowhere to put it the compiler will accept.
- `make lang` writes `lang/en.ini` and `lang/fr.ini` from those same
  tables, so the shipped files and the built-in text cannot drift. Drop
  one at `mc0:/ATLAS/LANG/fr.ini` to change wording without rebuilding.
  A key that is deleted, blank or misspelled falls back to the built-in
  text, so a half-finished translation is safe to leave on the card.
- `ATLAS.INI`, read from `ATLAS/CONFIG/` on the first device that has
  one, Memory Cards before USB - a stick that happens to be plugged in
  should not quietly take over the configuration of the console that
  borrowed it. It is a plain text file because the fix for a bad setting
  has to be something a user can do with a card reader and Notepad.
- Loading never fails: parsing starts from the defaults, so a partial
  file still produces a whole configuration, and an out-of-range number
  is clamped rather than rejected. Numbers are validated character by
  character before conversion, because `atoi()` reads "left" as 0 - a
  legal offset - and a typo would silently become a setting the user
  never chose. A file with more than eight unreadable lines is treated
  as damaged and `ATLAS.INI.BAK` is tried instead: a previous version of
  the user's own settings beats defaults by a long way.
- `atlas_file_write_atomic()` writes `<file>.NEW`, rotates the old file
  to `.BAK`, then renames. The window between truncating a file and
  finishing the write is not theoretical - it is a card holding a
  zero-byte `ATLAS.INI`. This is the shape the transactional `BOOT.ELF`
  update will reuse.
- Recovery reads no configuration at all - a recovery mode that loads
  the file it is meant to repair is no recovery at all - and safe video
  (R1) still loads the language but ignores the stored `[video]` block.
- Every screen now draws through `atlas_str()`. The Home menu no longer
  dispatches by comparing its own labels, which would have stopped
  working the moment they were translated, and would have stopped
  working silently: the French build would have opened the placeholder
  from rows that name a real screen.
- `make check` covers both new modules: the string tables (every key
  non-empty in both languages, key names unique, overrides, and a UTF-8
  sweep asserting every byte stays inside the range the font atlas
  covers) and the configuration parser (clamping, tolerant booleans, and
  a format-then-parse round trip comparing all thirteen fields
  individually, since a `memcmp` would pass on padding and hide a field
  the formatter forgot to write).

- **Milestone 4 - applications.** Homebrew is discovered on the attached
  devices and launched from a list. Nothing has to be registered: the
  user copies a folder or an ELF onto a card or a stick and it appears,
  because a list they have to maintain is one more thing that goes stale
  when a card moves between consoles.
- `atlas_app_scan()` walks `ATLAS/APPS` on each Memory Card and both
  `APPS` and `ATLAS/APPS` on USB, one level deep. A root holds either
  loose ELFs or one folder per application - that is the whole
  convention, and a deeper walk would let a stick holding a PC backup
  stall the scan for minutes. Scanning happens on entering the screen or
  on an explicit Triangle, never per frame.
- `app.ini` supplies `name`, `elf` and `category`. When it names an ELF
  that is not there - a typo, or a partial copy - the folder is scanned
  instead of the application being dropped, because a folder the user
  deliberately created should appear even when its metadata is wrong.
  Without metadata the name is derived from the filename, keeping the
  author's capitalisation: title-casing would render "uLaunchELF" as
  "Ulaunchelf".
- `atlas_ini_parse()`: a tolerant reader for files a user edits on a PC.
  CRLF, comments, indentation and spacing are all accepted; a line it
  cannot parse is skipped and counted rather than aborting the file, so
  one bad line costs one setting. An oversized value is dropped, never
  truncated - a shortened `elf=` names a different file. Parses a buffer
  rather than a path, so it is covered by `make check` and will be
  reused for `ATLAS.INI` and its `.BAK` recovery.
- `atlas_launch_elf()` checks the ELF header before handing over the
  console. The SDK's loader validates the magic with a trap instruction,
  so giving it a file that is not a PS2 ELF does not return an error -
  it raises an exception, and that is a black screen with no way back
  but the power switch. The file is rejected while there is still a
  screen to explain it on. Video, pad and the device layer are shut down
  in the reverse of the order they came up, so the incoming program
  finds a quiet machine rather than one with our DMA chains and
  interrupt handlers still live.

- **Milestone 3 - storage devices.** A unified layer over the two Memory
  Card slots, USB mass storage and (reserved) the internal HDD, plus a
  Devices screen.
- `atlas_device_*`: one numbered slot per device with a mount path a
  caller can hand straight to fileXio, so nothing above this layer knows
  that a Memory Card is polled through libmc while a USB stick is a
  mounted FAT volume. State is cached and refreshed one device per frame
  from the update half of the loop - probing an empty slot blocks for
  milliseconds, and a draw path that blocks turns 60 Hz into a stutter.
  USB backs off between attempts while it is still enumerating.
- A card that is present but unusable reports why: unformatted, a PS1
  card, or unreadable. A missing row tells the user nothing they can act
  on. The Devices screen shows those reasons, and the Home indicators
  now read the real cache instead of inferring from which IOP modules
  loaded.
- `atlas_path_join()`: refuses rather than truncates, and leaves the
  output buffer untouched when it does. A shortened path names a
  different file that may well exist, and the file manager and installer
  delete and overwrite through this. Covered by `make check`.
- Release and debug builds no longer share object files. An object
  records nothing about the flags that built it, so `make` after
  `make debug` used to report a release ELF that still carried every log
  string.
- **Milestone 2 - graphical interface.** A dark theme, drawing primitives,
  a screen stack, and the Home, System Info, Power and placeholder
  screens.
- `atlas_theme_*`: the default dark palette is compiled into the ELF and
  cannot be removed, so a corrupt or missing theme file still leaves a
  readable interface rather than an invisible one.
- `atlas_ui_*`: rectangles, gradients, notched panels, separators, header
  and footer bars, menu rows, and a modal message box. Every coordinate
  is safe-area-relative and the 16:9 narrowing is applied in one place,
  so no screen has to know about overscan or aspect.
- `atlas_screen_*`: a fixed-depth stack of statically allocated screens.
  Push, pop and replace take effect between frames, so a screen's
  `enter()` always runs before it first draws.
- Home, System Info and Power screens; Power builds its entry list from
  what is actually available and asks for confirmation before anything
  that ends the session. Unfinished entries open an honest placeholder
  rather than a black screen.
- `atlas_utf8_*`: the UTF-8 decoder, split out of the font renderer so it
  can be checked on the host. `make check` builds and runs the self-check
  in `tests/`, which covers the French accented characters, replacement
  of anything outside Latin-1, and the guarantee that malformed input
  always advances - a decoder that could stall would hang the render loop
  with no way out.
- **Milestone 1 - bootable ELF.** IOP reset, embedded IRX module loading,
  GS bring-up, controller input, bitmap font rendering, and a splash screen
  reporting which module groups came up.
- `atlas_boot_*`: IOP reset with bounded retries, SBV patches, and the
  module set (iomanX, fileXio, sio2man, padman, mcman, mcserv, usbd, bdm,
  bdmfs_fatfs, usbmass_bd, poweroff) embedded in the ELF. Every group is
  optional except the file layer; a failure degrades features instead of
  aborting the boot.
- `atlas_video_*` (AtlasVideo): AUTO/NTSC/PAL/480p modes with region
  detection via `gsKit_check_rom()`, 4:3 and 16:9, screen offsets, an
  overscan-aware safe area, and a CT24 framebuffer.
- `atlas_input_*`: both controller ports merged, logical buttons so no UI
  code hardcodes cross-as-confirm, edge detection, and direction repeat.
- `atlas_font_*`: T8 atlas plus greyscale CLUT uploaded to VRAM, so one
  atlas draws in any colour; UTF-8 decoding covering Latin-1 for the
  French translation; width measurement and ellipsis clipping.
- `tools/genfont.py`: build-time TTF-to-atlas baker. Avoids gsKit's FONTM
  helper, which reads Sony ROM data.
- Boot hotkeys latched before any configuration is parsed: L1+R1 for
  Recovery, R1 alone for safe video.
