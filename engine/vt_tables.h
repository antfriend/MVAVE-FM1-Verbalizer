/* Propeller ROM lookup tables used by the vocal tract. See tools/gen_tables.py. */
#ifndef VT_TABLES_H
#define VT_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* round((2^(i/2048) - 1) * 65536), i = 0..2047 */
extern const uint16_t vt_antilog_table[2048];

/* round(sin(i/2048 * 90 degrees) * 65535), i = 0..2048 */
extern const uint16_t vt_sine_table[2049];

#ifdef __cplusplus
}
#endif

#endif
