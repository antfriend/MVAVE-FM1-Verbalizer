/*
 * Where the engine's code and tables are placed, and the few memory helpers
 * it uses.
 *
 * On a desktop none of this does anything. On the FM-1 the audio interrupt
 * keeps running while flash is being written, when code and data in flash
 * cannot be reached, so everything the engine runs or reads has to sit in
 * RAM, and it cannot call the C library (which is in flash). VB_CODE marks
 * every engine function and VB_DATA every constant table; the helpers below
 * stand in for memcpy and memset.
 */
#ifndef VB_PLATFORM_H
#define VB_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GCC_PI32V2__) && !defined(VB_CODE)
/* JieLi PI32v2 (the FM-1): the SDK's RAM sections for audio code and data. */
#define VB_CODE __attribute__((section(".volatile_ram_code")))
#define VB_DATA __attribute__((section(".volatile_ram.fm1"), aligned(4)))
/* The program image is nearly full: build the big tables in RAM at start-up. */
#define VT_TABLES_IN_RAM
#endif

#ifndef VB_CODE
#define VB_CODE
#endif

/* VB_COLD_CODE is VB_CODE for functions that run rarely (key events, the
 * 2 ms scan, start-up). On the FM-1 they are compiled for size, because the
 * program image has almost no room; the per-sample code stays optimised for
 * speed. */
#ifndef VB_COLD_CODE
#ifdef __GCC_PI32V2__
#define VB_COLD_CODE VB_CODE __attribute__((minsize))
#else
#define VB_COLD_CODE VB_CODE
#endif
#endif
#ifndef VB_DATA
#define VB_DATA
#endif

#if defined(__GNUC__) || defined(__clang__)
#define VB_INLINE static inline __attribute__((always_inline))
#else
#define VB_INLINE static
#endif

/* Byte copy, lowest address first, so it is also safe for moving a block
 * down over itself. The volatile keeps the compiler from turning the loop
 * back into a library call. */
VB_INLINE void vb_copy(void *to, const void *from, size_t bytes)
{
    volatile uint8_t *d = (volatile uint8_t *)to;
    const uint8_t *s = (const uint8_t *)from;

    while (bytes--)
        *d++ = *s++;
}

VB_INLINE void vb_fill(void *to, uint8_t value, size_t bytes)
{
    volatile uint8_t *d = (volatile uint8_t *)to;

    while (bytes--)
        *d++ = value;
}

#endif
