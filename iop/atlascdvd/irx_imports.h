/*
 * AtlasPS2 - atlascdvd irx_imports.h
 *
 * The headers imports.lst is expanded against.
 *
 * The SDK does not ship this file: each module writes its own, listing
 * exactly the libraries it imports. That is not ceremony - the
 * IMPORTS_start/IMPORTS_end macros come from these headers, so a
 * library missing here is a build error rather than a module that
 * loads and jumps into nothing.
 *
 * Keep it in step with imports.lst. Nothing else should be added: an
 * import is a library that must be resident before this module loads
 * and must survive the game loading its own.
 */
#ifndef IRX_IMPORTS_H
#define IRX_IMPORTS_H

#include "irx.h"

#include <atad.h>
#include <bdm.h>
#include <intrman.h>
#include <loadcore.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>

#endif /* IRX_IMPORTS_H */
