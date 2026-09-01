AtlasPS2 v@VERSION@
==================

A boot and menu environment for the PlayStation 2. It is what loads when
the console powers on: it brings up the IOP, finds your Memory Cards and
USB sticks, lists the homebrew it discovers on them, and launches it.


READ THIS FIRST
---------------

This is a PRE-RELEASE, and it has not been tested on a console.

Not "lightly tested" - not tested. Every hardware row in the project's
compatibility table says UNTESTED, and that is a statement of fact
rather than a placeholder awaiting someone's optimism.

What that means for you:

  * Back up anything on the Memory Card you install to. The installer
    does make its own backup, and you should not rely on it alone.
  * Booting a game from a disc image is the least verified part of the
    whole program. The Games screen warns you before it starts one.
  * A result you did not expect is worth reporting, including the ones
    that merely look untidy.

The installer is built so that the ways it can fail are survivable: it
never overwrites the working BOOT.ELF in place, it keeps what booted your
console before AtlasPS2 existed, and a failed step rolls back. That is
design, not evidence. Only a console can supply evidence.


WHAT IS IN THIS ARCHIVE
-----------------------

  ATLAS_INSTALLER.ELF   Run once, from a USB stick, to install AtlasPS2
                        onto a Memory Card.

  ATLASPS2.ELF          AtlasPS2 itself. The installer copies this to
                        the card. You can also just run it directly from
                        a USB stick without installing anything - a good
                        way to see it before committing to a card.

  USB/                  Copy the CONTENTS of this folder to the root of
                        a FAT32 USB stick. It is already laid out the
                        way the stick should look, which removes the
                        step people most often get wrong.

  INSTALL_FR.txt        Step-by-step installation guide, in French.
  INSTALL_EN.txt        The same in English.

  CHANGELOG.txt         What changed, and what is known not to work.

  LICENSES/             Licence text for AtlasPS2 and everything it
                        embeds.

  CHECKSUMS.txt         SHA-256 of every file above.


INSTALLING, IN ONE PARAGRAPH
----------------------------

Copy the contents of USB/ to the root of a FAT32 stick. Plug it into the
PS2 and start ATLAS_INSTALLER.ELF from whatever homebrew launcher your
console already has (uLaunchELF, for instance). Choose Install, pick the
Memory Card, and confirm. Then power off, remove the stick, and power on.

If that paragraph left you with questions, the answers are in
INSTALL_EN.txt - it assumes nothing and is written for someone who has
never installed PS2 homebrew before.


WHAT THIS DOES NOT DO
---------------------

It does not install a bootstrap or an exploit onto your console. Which
one a console needs depends on its ROM version and region, getting it
wrong can leave a Memory Card the console refuses to boot from, and no
software method determines it reliably on every model. AtlasPS2 needs
your console to ALREADY run homebrew, and installing it is a separate,
safe operation from whatever made that true.

It contains no Sony code, BIOS files, firmware, keys or artwork, and no
games. It does not read rom0:FONTM or any other ROM resource: the
interface font is baked from DejaVu Sans at build time.

Third-party homebrew - Open PS2 Loader and the like - stays a separate
component under its own licence. AtlasPS2 launches it; it does not
contain it.


IF SOMETHING GOES WRONG
-----------------------

Two hotkeys, both held while the console powers on:

  R1          Safe video. Forces NTSC 4:3 with no offsets. This is for
              when a video setting you chose produced a picture your
              television cannot display.

  L1 + R1     Recovery. Draws with no theme, reads no configuration and
              uses minimal graphics, so a broken theme or a corrupt
              settings file cannot lock you out. From there you can
              reset settings, disable the theme, roll back to the
              previous version, or return to the console's own browser.

Recovery is the answer to almost everything. If the console powers on to
a black screen, try L1+R1 before assuming the card is lost.

Your settings live in ATLAS/CONFIG/ATLAS.INI on the Memory Card, as
plain text. If you can read the card on a PC, you can fix them in
Notepad - which is precisely why it is a text file and not a binary one.


SOURCE
------

https://github.com/SnowdenShadow/AtlasPS2

AtlasPS2 is MIT licensed. The full text, and the licences of everything
it embeds, are in LICENSES/.
