/* SPDX-License-Identifier: MIT */
/*
 * What the Verbalizer's screen shows and what its encoders do, without any
 * drawing: the firmware's screen code asks this for names and values and
 * tells it which encoder turned.
 *
 * There are seven pages, turned with the SELECT encoder. The page decides the
 * engine's mode: Words on the first, Trigger / Sustain / Release on the next
 * three, Formants on the last three. Each page has up to six controls, on
 * the other six encoders:
 *
 *   slot 0      ALGORITHM
 *   slot 1..4   KNOB 1..4
 *   slot 5      PRESETS
 *
 * Everything here runs in the firmware's panel task.
 */
#ifndef FM1_VERBALIZER_UI_H
#define FM1_VERBALIZER_UI_H

enum { FM1_VB_PAGES = 7, FM1_VB_SLOTS = 6, FM1_VB_TEXT = 8 };

/* Page title, at most 9 characters. */
const char *fm1_vb_page_name(unsigned page);

/* Make `page` the current one: puts the engine in that page's mode. */
void fm1_vb_page_open(unsigned page);

/* The control's name, at most 6 characters, or 0 if the slot is unused. */
const char *fm1_vb_label(unsigned page, unsigned slot);

/* The control's value as text: a phoneme's name, milliseconds, hertz, or the
 * number the engine uses. */
void fm1_vb_value(unsigned page, unsigned slot, char out[FM1_VB_TEXT]);

/* The control was turned by `detents` (negative is anticlockwise). */
void fm1_vb_turn(unsigned page, unsigned slot, int detents);

#endif
