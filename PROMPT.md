# MASTER PROMPT — NEOBOOT PS2

You are a senior low-level C/C++ engineer specialized in PlayStation 2 homebrew, MIPS/R5900, PS2SDK, gsKit, IOP modules, ELF loading, Memory Card filesystems and embedded UI development.

Your mission is to DESIGN AND IMPLEMENT a complete open-source PlayStation 2 boot environment called:

# NeoBoot PS2

NeoBoot must feel like a modern successor to FreeMcBoot: simpler, cleaner, safer, more attractive, easier to install and easier to configure.

It must run on REAL PlayStation 2 hardware.

This is not a PC mockup and not an emulator-only project.

The final project must contain real compilable PS2 homebrew code, an installer, documentation and release packaging.

---

# 1. MAIN OBJECTIVE

Create a complete PS2 boot/menu environment providing:

* attractive modern home screen
* fast startup
* application launcher
* automatic device detection
* Memory Card support
* USB support
* HDD support when available
* MX4SIO support when technically available
* network information when available
* configurable shortcuts
* application auto-discovery
* per-app settings
* video settings
* recovery mode
* safe installer/updater
* uninstall option
* backup/restore system
* simple French installation tutorial

The user should be able to turn on the PS2 and reach NeoBoot automatically through an existing compatible Memory Card boot method.

NeoBoot itself must be our own application and interface.

---

# 2. IMPORTANT ARCHITECTURE DECISION

DO NOT attempt to reinvent the PS2 Memory Card exploit during the first version.

Use mature existing boot mechanisms only as the bootstrap/entry point where appropriate, for example:

* PS2BBL
* OpenTuna
* an existing FMCB-compatible boot environment

NeoBoot then becomes the main `BOOT.ELF`.

Architecture:

PS2 POWER ON
↓
compatible bootstrap / exploit
↓
NeoBoot BOOT.ELF
↓
NeoBoot initialization
↓
NeoBoot Home UI
↓
applications / settings / games launcher

The exploit/bootstrap layer and NeoBoot must remain clearly separated.

Do not simply reskin FreeMcBoot.

NeoBoot must have its OWN:

* source tree
* launcher
* graphical interface
* configuration system
* application manager
* installer
* recovery system

Use upstream code only when licensing permits it and clearly document reused components and licenses.

---

# 3. LEGAL / SAFETY REQUIREMENTS

Never include:

* Sony BIOS files
* copyrighted Sony firmware
* copyrighted Sony graphical assets
* proprietary encryption keys
* copyrighted commercial game files
* pirated ISOs
* unauthorized proprietary modules

NeoBoot is a homebrew launcher.

Do not bundle commercial games.

Third-party homebrew applications such as OPL must remain separate components and respect their respective licenses.

Where appropriate, provide installation hooks or optional external downloads instead of embedding third-party binaries in our repository.

---

# 4. DEVELOPMENT STACK

Primary environment:

* C / C++
* PS2SDK
* ps2dev toolchain
* gsKit
* DMA Kit where appropriate
* standard PS2SDK libraries
* Make or CMake if fully compatible with PS2 toolchain

Target:

* real PS2 Emotion Engine
* MIPS R5900
* real PS2 IOP
* 32 MB main RAM
* limited GS VRAM

Memory usage must be carefully controlled.

Do NOT design the UI like a modern PC application requiring hundreds of MB.

Everything must remain lightweight.

---

# 5. REPOSITORY STRUCTURE

Create a clean repository similar to:

neoboot-ps2/
│
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── Makefile
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── BUILD.md
│   ├── INSTALL_FR.md
│   ├── INSTALL_EN.md
│   ├── RECOVERY.md
│   ├── VIDEO.md
│   └── COMPATIBILITY.md
│
├── src/
│   ├── main.c
│   ├── core/
│   ├── boot/
│   ├── config/
│   ├── devices/
│   ├── elf/
│   ├── input/
│   ├── ui/
│   ├── video/
│   ├── apps/
│   ├── recovery/
│   └── utils/
│
├── include/
│
├── assets/
│   ├── fonts/
│   ├── icons/
│   └── backgrounds/
│
├── installer/
│   ├── src/
│   ├── include/
│   └── Makefile
│
├── config/
│   └── NEOBOOT.INI
│
├── tools/
│
└── release/

Keep modules isolated.

Avoid one giant `main.c`.

---

# 6. BOOT PROCESS

NeoBoot must start with a robust initialization sequence.

Example:

1. initialize debug output
2. initialize GS
3. detect PAL / NTSC
4. initialize controller
5. initialize Memory Card
6. mount available devices
7. load NeoBoot configuration
8. detect applications
9. load UI
10. show Home screen

During boot display a minimal splash:

NEOBOOT
PlayStation 2 Home Environment

Then smoothly transition to Home.

Startup should be fast.

Do not display dozens of technical messages unless Debug Mode is enabled.

If an initialization step fails, continue whenever possible.

Example:

USB initialization failed

must NOT prevent NeoBoot from starting from Memory Card.

---

# 7. GRAPHICAL DESIGN

Create a clean, modern, premium interface inspired by modern console dashboards while remaining original.

DO NOT copy Sony's copyrighted PS2/PS3/PS5 UI.

Visual identity:

* very dark blue/black background
* subtle gradient
* electric blue accent
* white primary text
* grey secondary text
* large clean typography
* simple line icons
* rounded-looking panels where possible
* subtle animation
* no visual clutter

Target feeling:

2026 interface running on 2000 hardware.

Home example:

┌───────────────────────────────────────────────┐
│ NEOBOOT                         22:34    MC1  │
│                                               │
│ Welcome                                       │
│                                               │
│   [ Games ]                                   │
│   [ Applications ]                            │
│   [ File Manager ]                            │
│   [ Video ]                                   │
│   [ Settings ]                                │
│                                               │
│                         USB ●   HDD ●          │
└───────────────────────────────────────────────┘

Do not overload the screen.

Menus must remain usable in:

* PAL
* NTSC
* 4:3
* 16:9

Use safe margins to prevent CRT overscan problems.

---

# 8. CONTROLLER NAVIGATION

Default controls:

D-Pad:
navigate

Cross:
confirm

Circle:
back

Triangle:
context menu

Square:
optional secondary action

L1/R1:
switch categories

START:
quick settings

SELECT:
system information

Button configuration must be adaptable for console region where necessary.

Do not hardcode assumptions that make Japanese-region systems unusable.

---

# 9. HOME SCREEN

Home must contain:

Games

Applications

File Manager

Video

Settings

Power

At top/right show small useful status indicators:

* MC0
* MC1
* USB
* HDD
* Network

Use simple connected/disconnected icons.

Do not repeatedly poll devices in a way that causes freezes.

---

# 10. APPLICATION MANAGER

NeoBoot must automatically discover ELF applications.

Search configurable directories such as:

mc0:/NEOBOOT/APPS/
mc1:/NEOBOOT/APPS/
mass:/APPS/
mass0:/APPS/
hdd0:/...

Application folder example:

APPS/
└── OPL/
├── OPL.ELF
├── app.ini
└── icon.png

Example `app.ini`:

[app]
name=Open PS2 Loader
elf=OPL.ELF
category=Games
icon=icon.png

If no metadata file exists:

* detect ELF
* derive readable name from filename
* use generic icon

Applications should appear automatically.

No manual CNF editing should normally be required.

---

# 11. FAVORITES

Allow apps to be marked as favorites.

Home can then show:

Recently used

Favorites

Applications

Store information locally in:

mc0:/NEOBOOT/CONFIG/

Avoid unnecessary Memory Card writes.

Only save when values actually change.

---

# 12. ELF LAUNCHING

Implement a robust ELF loader.

Before launching an ELF:

* close unnecessary resources
* flush configuration changes
* clean relevant subsystems
* reset IOP only when required
* correctly transfer execution
* pass arguments where supported

Handle launch failure gracefully.

Never leave the user with an unexplained black screen if NeoBoot can recover.

Log errors in debug mode.

---

# 13. DEVICE MANAGER

Create a unified device layer.

Support when available:

mc0:
mc1:
mass:
mass0:
mass1:
HDD
MX4SIO
network-related device information

Device API should expose:

Device_Init()
Device_Mount()
Device_Unmount()
Device_IsAvailable()
Device_GetFreeSpace()

The UI must not directly contain device-specific implementation code.

---

# 14. FILE MANAGER

Include a SIMPLE file manager.

Functions:

* browse folders
* copy
* move
* delete
* create folder
* rename
* launch ELF
* show file size
* show source/destination

IMPORTANT:

Dangerous operations must require confirmation.

Example:

Delete BOOT.ELF?

[ Cancel ]
[ Delete ]

System folders should display an additional warning.

Do not silently delete user content.

---

# 15. CONFIGURATION

Use a human-readable configuration such as:

mc0:/NEOBOOT/CONFIG/NEOBOOT.INI

Example:

[system]
language=fr
theme=dark
startup=home

[video]
mode=auto
aspect=auto
overscan_x=0
overscan_y=0

[ui]
animations=true
sounds=true

[boot]
default_app=
timeout=0

Configuration parser must:

* tolerate missing values
* use safe defaults
* ignore unknown values
* automatically recover from a corrupt configuration

Keep backup:

NEOBOOT.INI.BAK

---

# 16. LANGUAGES

Architecture must support translations.

Initial languages:

French
English

Use translation files instead of hardcoding all text.

Example:

lang/fr.ini
lang/en.ini

French should be the polished reference translation.

---

# 17. VIDEO SYSTEM — NEOVIDEO

Create a video configuration module named:

NeoVideo

IMPORTANT:

Do NOT claim that forcing 1080 output makes PS2 games render internally at native 1920×1080.

Clearly distinguish:

* rendering resolution
* output timing
* progressive scan
* external scaling

Initial NeoVideo functionality:

* AUTO
* PAL
* NTSC
* safe 480i/576i
* supported progressive modes where appropriate
* screen position
* horizontal adjustment
* vertical adjustment
* overscan
* 4:3 / 16:9 UI
* saved profiles

Advanced/experimental options may later implement GSM-like video mode forcing.

Experimental modes must be clearly marked.

Never force risky/incompatible display timings by default.

If a display mode fails, provide a fallback mechanism.

Example:

Hold R1 during boot
→ restore safe AUTO video mode.

---

# 18. GAME PROFILES

Prepare architecture for per-title profiles.

Example:

profiles/
SLUS_209.46.ini

Potential fields:

video_mode
aspect_ratio
offset_x
offset_y
widescreen
launch_app

Do not implement questionable patches blindly.

Profiles must remain optional.

---

# 19. SETTINGS MENU

Settings sections:

General

Display

Devices

Applications

Boot

Theme

Language

System Information

Advanced

Recovery

About

System Information should display:

* NeoBoot version
* console region when safely detectable
* video mode
* free Memory Card space
* detected devices
* IP address when available
* PS2SDK build information

Do not display console-sensitive identifiers unnecessarily.

---

# 20. POWER MENU

Power menu:

Restart NeoBoot

Return to PS2 Browser

Restart Console

Power Off

Cancel

Only show functions that can be safely supported.

---

# 21. RECOVERY MODE

Recovery is CRITICAL.

Holding a specific button combination while NeoBoot starts must enter:

NeoBoot Recovery

Example:

Hold R1 + L1 during startup.

Recovery options:

* Boot NeoBoot normally
* Boot fallback ELF
* Reset configuration
* Disable custom theme
* Restore previous NeoBoot version
* File Manager
* Reinstall NeoBoot
* Return to PS2 Browser

Recovery mode must use:

* minimal graphics
* safe video mode
* minimal drivers

It must not depend on optional theme files.

---

# 22. SAFE UPDATE SYSTEM

Design updates transactionally.

NEVER overwrite the current working `BOOT.ELF` directly.

Example process:

BOOT.ELF
BOOT.NEW
BOOT.BAK

Update:

1. validate new file
2. verify file size / checksum
3. rename current to BOOT.BAK
4. install BOOT.NEW
5. verify
6. activate
7. preserve rollback

If update fails:

restore previous version automatically.

No network updater is required for V1.

USB update is enough.

Example:

mass:/NEOBOOT_UPDATE/

---

# 23. INSTALLER

Create a separate executable:

NEOBOOT_INSTALLER.ELF

Installer interface:

NeoBoot Installer

Detected:
Console: PS2
Memory Card: Slot 1
Free space: xxxx KB

Options:

[ Install NeoBoot ]
[ Update NeoBoot ]
[ Repair installation ]
[ Backup existing setup ]
[ Restore backup ]
[ Uninstall NeoBoot ]
[ Exit ]

Before modifying a Memory Card:

CREATE A BACKUP WHEN POSSIBLE.

Do not blindly delete:

BOOT
APPS
SYS-CONF

Existing user files must be preserved whenever technically possible.

---

# 24. BOOTSTRAP INSTALLATION

The installer must detect the environment before installing any bootstrap.

Never assume one OpenTuna variant works on every PS2.

Detect relevant console/ROM information when technically reliable.

If automatic determination is unsafe:

STOP and display a clear compatibility message.

Never install a bootstrap variant by guessing.

Separate:

NeoBoot installation

from

bootstrap/exploit installation.

NeoBoot should also be installable on a Memory Card already configured with another compatible boot environment.

---

# 25. SIMPLE USER INSTALLATION EXPERIENCE

Goal:

A normal user should not edit configuration files manually.

Desired process:

1. Start existing homebrew environment.
2. Copy NeoBoot installer to USB.
3. Launch `NEOBOOT_INSTALLER.ELF`.
4. Select Memory Card.
5. Choose `Install NeoBoot`.
6. Installer performs compatibility checks.
7. Existing configuration is backed up.
8. NeoBoot files are copied.
9. Boot configuration is installed/configured.
10. Console restarts.
11. NeoBoot appears.

The installer must show progress clearly.

Example:

Installing NeoBoot

Preparing Memory Card         ✓
Creating backup              ✓
Installing NeoBoot           ✓
Installing configuration     ✓
Verifying files              ✓

Installation completed.

[ Restart PS2 ]

---

# 26. DOCUMENTATION

Generate an extremely simple:

docs/INSTALL_FR.md

It must be written for someone with very little technical knowledge.

Structure:

# Installer NeoBoot sur PS2

## Ce qu'il faut

* PS2
* Memory Card
* clé USB FAT32/exFAT only if technically supported by selected installer stack
* way to launch a homebrew ELF

## Étape 1 — Préparer la clé USB

Exact files and folders.

## Étape 2 — Lancer l'installateur

Exact controller actions.

## Étape 3 — Installer NeoBoot

Screens explained.

## Étape 4 — Redémarrer

Expected behavior.

## En cas de problème

Recovery button combination.

## Désinstaller NeoBoot

Exact procedure.

No vague instructions.

---

# 27. BUILD DOCUMENTATION

Create:

docs/BUILD.md

Give BOTH methods where practical.

### Option A — Docker

Use maintained ps2dev container images when compatible.

Example workflow:

docker run ...
make clean
make

### Option B — native PS2DEV toolchain

Document:

PS2DEV
PS2SDK
GSKIT
PATH

Commands must be real commands, not pseudocode.

Expected build outputs:

build/NEOBOOT.ELF
build/NEOBOOT_INSTALLER.ELF

---

# 28. DEVELOPMENT BUILD

Include debug logging.

Possible output through:

* ps2link / ps2client where appropriate
* console/debug output

Compile modes:

make release

make debug

Debug mode may display:

[BOOT] GS initialized
[BOOT] pad initialized
[MC] mc0 mounted
[USB] mass mounted
[CONFIG] loaded NEOBOOT.INI
[APP] 7 applications found
[UI] home screen ready

Release build must not spam debug output.

---

# 29. ERROR HANDLING

Every major subsystem must return meaningful error codes.

Do not use:

if(failed)
freeze_forever();

Prefer:

* retry
* fallback
* error message
* continue with reduced functionality

Example:

USB unavailable.

NeoBoot will continue without USB.

[ OK ]

---

# 30. PERFORMANCE

Target fast boot.

Avoid:

* huge textures
* unnecessary alpha layers
* giant images
* constant filesystem rescans
* allocating/freeing every frame
* excessive Memory Card writes
* blocking I/O in rendering loop

Cache application metadata.

Use small optimized textures.

Aim for stable frame pacing.

---

# 31. UI ASSET RULES

All NeoBoot visual assets must be original.

Use:

* simple icons
* open-source fonts compatible with redistribution
* lightweight PNG or PS2-appropriate texture formats

Do not use:

* PlayStation logos without authorization
* copied PS2 BIOS assets
* Sony menu graphics

NeoBoot branding must be independent.

---

# 32. THEMES

Prepare theme support.

Example:

mc0:/NEOBOOT/THEMES/Default/

theme.ini
background.png
icons/

But the built-in default theme must always be available internally so deleting the external theme cannot make NeoBoot unusable.

---

# 33. FIRST BOOT EXPERIENCE

On first boot:

Welcome to NeoBoot

Language:
[ Français ]
[ English ]

Display:
[ Automatic ]

Scan applications:
[ Yes ]

Then Home.

Do not require 15 configuration screens.

---

# 34. COMPATIBILITY DATABASE

Create documentation/table recording actual test status:

Console model
ROM
Region
Boot method
NeoBoot
USB
HDD
MX4SIO
Video
Notes

Never claim compatibility until verified.

Use:

UNTESTED
WORKS
PARTIAL
BROKEN

---

# 35. TESTING

Test in:

PCSX2 for rapid development

AND

real PlayStation 2 hardware before declaring hardware compatibility.

Emulator success alone does not prove hardware compatibility.

Create a test checklist covering:

* cold boot
* controller disconnect/reconnect
* no USB
* corrupted config
* empty Memory Card
* full Memory Card
* missing theme
* missing application
* PAL
* NTSC
* 4:3
* 16:9
* MC slot 1
* MC slot 2
* failed update
* recovery boot

---

# 36. RELEASE STRUCTURE

Generate release package:

NeoBoot-vX.Y.Z/
│
├── NEOBOOT_INSTALLER.ELF
├── NEOBOOT.ELF
├── USB/
│   └── NEOBOOT/
│
├── README.txt
├── INSTALL_FR.txt
├── INSTALL_EN.txt
├── LICENSES/
└── CHECKSUMS.txt

Release archive must be understandable without opening the source repository.

---

# 37. VERSIONING

Use semantic versioning:

0.1.0
0.2.0
1.0.0

Display version in:

Settings → About

and

installer.

---

# 38. IMPLEMENTATION ROADMAP

Do not attempt every feature at once.

Implement sequentially.

## MILESTONE 1

Real ELF boots on PS2.

Display:

NeoBoot
Controller detected
Press X

Deliver a compilable ELF.

## MILESTONE 2

Basic graphical UI.

Home
Applications
Settings
Power

## MILESTONE 3

Device detection.

MC0
MC1
USB

## MILESTONE 4

ELF application scanner and launcher.

## MILESTONE 5

Configuration and translations.

## MILESTONE 6

Installer.

## MILESTONE 7

Recovery system and backups.

## MILESTONE 8

NeoVideo.

## MILESTONE 9

Themes and polishing.

## MILESTONE 10

1.0 release candidate and real-hardware compatibility tests.

EACH milestone must leave the repository compilable.

---

# 39. AI WORKING RULES

You are not only producing architecture documentation.

YOU MUST IMPLEMENT THE SOFTWARE.

When producing code:

* give complete files
* do not use fake APIs
* verify PS2SDK functions actually exist
* inspect current PS2SDK headers/source when unsure
* do not invent library names
* do not leave core functions as TODO
* explain any hardware limitation
* compile frequently
* fix compiler errors before continuing
* preserve a working build after every milestone

If an upstream API changed, use the CURRENT upstream implementation.

Do not blindly copy outdated 2008 PS2 code when current PS2SDK offers a cleaner solution.

---

# 40. UPSTREAM PROJECTS TO STUDY

Study current source code and documentation of:

* ps2dev/ps2sdk
* ps2dev/ps2dev
* ps2dev/gsKit
* PlayStation2-Basic-BootLoader / PS2BBL
* OpenTuna installer
* FreeMcBoot
* wLaunchELF
* Open PS2 Loader

Use them to understand:

* hardware initialization
* pad handling
* ELF loading
* filesystem access
* IOP modules
* Memory Card behavior
* GS rendering
* boot methods

DO NOT blindly merge their source trees.

Respect licenses.

Document anything reused.

---

# 41. CODE QUALITY

Use consistent naming.

Example:

nb_system_init()
nb_video_init()
nb_pad_init()
nb_config_load()
nb_device_scan()
nb_apps_scan()
nb_elf_launch()
nb_ui_render()

Prefix public NeoBoot symbols with:

nb_

Use headers properly.

Keep platform-specific code separated from UI logic.

Add comments explaining PS2-specific behavior.

---

# 42. MAIN UX GOAL

FreeMcBoot often feels like a configuration utility.

NeoBoot should feel like an actual console home environment.

A user should be able to:

Turn PS2 on
↓
NeoBoot appears
↓
select Games
↓
select launcher/game
↓
play

with almost zero configuration.

The interface must be:

FAST
CLEAN
BEAUTIFUL
RELIABLE
SAFE
SIMPLE

Reliability always has priority over animation or visual effects.

---

# 43. DEFINITION OF DONE FOR V1

NeoBoot 1.0 is complete when:

✓ boots reliably on supported real PS2 models

✓ has its own graphical interface

✓ controller navigation works

✓ MC0/MC1 detection works

✓ USB detection works

✓ ELF applications are automatically discovered

✓ ELF applications launch correctly

✓ configuration persists

✓ French and English work

✓ safe video configuration exists

✓ Recovery Mode works

✓ installer works

✓ installer creates backups

✓ update can roll back

✓ uninstall works

✓ missing USB does not crash NeoBoot

✓ corrupt configuration does not brick boot

✓ installation tutorial is understandable by a beginner

✓ entire source builds from documented commands

✓ release package can be generated automatically

---

# 44. START NOW

Begin with Milestone 1.

Before writing application code:

1. inspect the current PS2SDK / ps2dev toolchain
2. determine the exact libraries required
3. create the repository structure
4. create the Makefile
5. create the smallest real NeoBoot ELF
6. initialize video safely
7. initialize controller
8. render a simple NeoBoot screen
9. provide exact compilation commands
10. ensure it compiles before moving forward

Do NOT jump directly to the entire final UI.

Do NOT provide only theoretical instructions.

Produce working code milestone by milestone.

When a technical uncertainty exists, inspect upstream PS2SDK/source documentation rather than inventing an answer.

The first deliverable must be:

NEOBOOT.ELF

showing on a real/emulated PS2:

NEOBOOT

Welcome to NeoBoot

Press X to continue

After this first executable works, proceed to Milestone 2 while keeping Milestone 1 functional.
