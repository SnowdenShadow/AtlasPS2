/*
 * AtlasPS2 - log.c
 * Debug logging implementation (debug builds only).
 */
#include "atlas/log.h"

#ifdef ATLAS_DEBUG

#include <stdarg.h>
#include <stdio.h>

void atlas_log(const char *tag, const char *fmt, ...)
{
    va_list ap;

    printf("[%s] ", tag ? tag : "?");

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}

#endif /* ATLAS_DEBUG */
