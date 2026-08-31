/*
 * AtlasPS2 - ins_screen.h
 *
 * The installer's screens. Three of them, and that is the whole
 * program: a menu, a progress list, and the refusal to install a
 * bootstrap.
 *
 * They use the launcher's screen stack, UI widgets and theme rather
 * than a second set. The installer is what a user sees first, so the
 * two programs looking like each other is the point - and the built-in
 * theme is a compiled constant, so nothing here can fail to load.
 */
#ifndef ATLAS_INS_SCREEN_H
#define ATLAS_INS_SCREEN_H

#include "atlas/screen.h"
#include "install.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The main menu: what was detected, and what can be done to it. */
atlas_screen_t *atlas_ins_screen_menu(void);

/**
 * The progress list, pushed once an operation is confirmed.
 *
 * Owns the job for its lifetime; the menu hands it one already set up
 * so that a job which cannot start shows its reason on this screen
 * rather than flashing past.
 */
atlas_screen_t *atlas_ins_screen_run(const atlas_install_job_t *job);

/**
 * Why no bootstrap is installed here.
 *
 * A full screen rather than a line in a menu: a user arriving with a
 * brand-new console needs to understand that this program is not the
 * one that will get their card booting, and a message they can dismiss
 * without reading would send them away thinking it failed.
 */
atlas_screen_t *atlas_ins_screen_bootstrap(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_INS_SCREEN_H */
