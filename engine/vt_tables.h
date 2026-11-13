/*
 * Propeller ROM lookup tables used by the vocal tract.
 *
 *   antilog: round((2^(i/2048) - 1) * 65536), i = 0..2047   (ROM $D000)
 *   sine:    round(sin(i/2048 * 90 degrees) * 65535), i = 0..2048   (ROM $E000)
 *
 * Normally they are constants in vt_tables.c (see tools/gen_tables.py). With
 * VT_TABLES_IN_RAM defined they are ordinary arrays that vt_tables_init()
 * fills in, which must then be called before the first sample. The FM-1
 * build does this to keep 8 KB out of its nearly full program image.
 */
#ifndef VT_TABLES_H
#define VT_TABLES_H

#include <stdint.h>

#include "vb_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef VT_TABLES_IN_RAM
extern uint16_t vt_antilog_table[2048];
extern uint16_t vt_sine_table[2049];
#else
extern const uint16_t vt_antilog_table[2048];
extern const uint16_t vt_sine_table[2049];
#endif

/* Make the tables ready. Does nothing when they are constants; safe to call
 * more than once. vt_init() calls it. */
void vt_tables_init(void);

/* Compute both tables into the caller's arrays. */
void vt_tables_generate(uint16_t antilog[2048], uint16_t sine[2049]);

#ifdef __cplusplus
}
#endif

#endif
