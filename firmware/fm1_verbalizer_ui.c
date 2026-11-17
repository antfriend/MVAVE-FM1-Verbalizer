/* SPDX-License-Identifier: MIT */
/* The Verbalizer's pages and controls. See fm1_verbalizer_ui.h. */
#include "fm1_verbalizer_ui.h"

#include "fm1_verbalizer.h"
#include "verbalizer.h"

/* The firmware's program flash is nearly full and none of this runs in the
 * audio interrupt, so on the device it is compiled for size. */
#ifdef __GCC_PI32V2__
#define UI_CODE __attribute__((minsize))
#else
#define UI_CODE
#endif

/* How a knob's 0..255 is stepped and shown. Shown value = knob * mul / div
 * + offset. */
enum { RAW, THIRD, TIME, HZ, HZ2, PITCH, PHONEME, WORD_NAME, KINDS };

static const struct {
    uint8_t step;      /* knob units per detent */
    uint16_t mul, div;
    int8_t offset;
} kinds[KINDS] = {
    [RAW]     = {1, 1, 1, 0},
    [THIRD]   = {3, 1, 3, 0},        /* the engine divides these by 3 */
    [TIME]    = {1, 7, 5, 0},        /* x2 steps of 700 us, in ms */
    [HZ]      = {1, 1953, 100, 0},   /* 19.53 Hz units */
    [HZ2]     = {1, 3906, 100, 0},   /* 39.06 Hz units */
    [PITCH]   = {21, 1, 21, -6},     /* keys up or down */
    [PHONEME] = {7, 1, 7, 0},
    [WORD_NAME] = {15, 1, 15, 0}
};

enum { L_PHONE, L_TIME, L_VOLUME, L_VIB, L_RATE, L_VOICE, L_F1, L_F2, L_F3, L_F4, L_GLIDE,
       L_BREATH, L_NASAL, L_NFREQ, L_FRIC, L_FFREQ, L_PITCH, L_LENGTH, L_WORD };

static const char labels[][7] = {
    "PHONE", "TIME", "VOLUME", "VIB", "RATE", "VOICE", "F1", "F2", "F3", "F4", "GLIDE",
    "BREATH", "NASAL", "N FREQ", "FRIC", "F FREQ", "PITCH", "LENGTH", "WORD"
};

#define NONE {255, 0, 0}
#ifndef VB_NO_EXTRAS
#define WORD_SLOT {VB_POT_WORD, WORD_NAME, L_WORD}
#else
#define WORD_SLOT NONE /* the one word needs no choosing */
#endif

static const struct {
    char name[10];
    uint8_t mode;
    struct { uint8_t knob, kind, label; } slot[FM1_VB_SLOTS];
} pages[FM1_VB_PAGES] = {
    {"WORDS", VB_MODE_WORDS,
        {WORD_SLOT, {VB_POT_WORD_TIME, TIME, L_LENGTH}, NONE, NONE, NONE, NONE}},
    {"TRIGGER", VB_MODE_TSR,
        {{VB_POT_TRIG_PHONEME, PHONEME, L_PHONE}, {VB_POT_TRIG_TIME, TIME, L_TIME},
         {VB_POT_TRIG_VOLUME, THIRD, L_VOLUME}, {VB_POT_TRIG_VIB_DEPTH, THIRD, L_VIB},
         {VB_POT_TRIG_VIB_RATE, THIRD, L_RATE}, NONE}},
    {"SUSTAIN", VB_MODE_TSR,
        {{VB_POT_SUS_PHONEME, PHONEME, L_PHONE}, {VB_POT_SUS_TIME, TIME, L_TIME},
         {VB_POT_SUS_VOLUME, THIRD, L_VOLUME}, {VB_POT_SUS_VIB_DEPTH, THIRD, L_VIB},
         {VB_POT_SUS_VIB_RATE, THIRD, L_RATE}, NONE}},
    {"RELEASE", VB_MODE_TSR,
        {{VB_POT_REL_PHONEME, PHONEME, L_PHONE}, {VB_POT_REL_TIME, TIME, L_TIME},
         {VB_POT_REL_VOLUME, THIRD, L_VOLUME}, {VB_POT_REL_VIB_DEPTH, THIRD, L_VIB},
         {VB_POT_REL_VIB_RATE, THIRD, L_RATE}, NONE}},
    {"FORMANT 1", VB_MODE_FORMANTS,
        {{VB_POT_FM_GA, THIRD, L_VOICE}, {VB_POT_FM_F1, HZ, L_F1}, {VB_POT_FM_F2, HZ, L_F2},
         {VB_POT_FM_F3, HZ, L_F3}, {VB_POT_FM_F4, HZ, L_F4}, {VB_POT_FM_TIME, TIME, L_GLIDE}}},
    {"FORMANT 2", VB_MODE_FORMANTS,
        {{VB_POT_FM_AA, RAW, L_BREATH}, {VB_POT_FM_NA, RAW, L_NASAL}, {VB_POT_FM_NF, HZ, L_NFREQ},
         {VB_POT_FM_FA, RAW, L_FRIC}, {VB_POT_FM_FF, HZ2, L_FFREQ}, {VB_POT_FM_PITCH, PITCH, L_PITCH}}},
    {"FORMANT 3", VB_MODE_FORMANTS,
        {NONE, {VB_POT_FM_VP, THIRD, L_VIB}, {VB_POT_FM_VR, THIRD, L_RATE}, NONE, NONE, NONE}}
};

/* Phonemes by name (docs/phonemes.md), five characters each. Below 3 is
 * silence. Without the extras 20..33 and 36 are all the same noise, so only
 * 3..20, the word (34) and the nasal "ah" (35) are named and offered. */
#ifndef VB_NO_EXTRAS
enum { FIRST_NAMED = 3, LAST_PHONEME = 36, OFF = 2 };
#else
enum { FIRST_NAMED = 3, NOISE = 20, WORD = 34, LAST_PHONEME = 35, OFF = 2 };
#endif
static const char phonemes[] =
    "SOAP\0BEET\0HAT\0\0HOT\0\0RR\0\0\0BALL\0EE\0\0\0I\0\0\0\0E\0\0\0\0A\0\0\0\0"
    "O\0\0\0\0OH\0\0\0FOOT\0BOOT\0R\0\0\0\0L\0\0\0\0UH\0\0\0"
    "NOISE"
#ifndef VB_NO_EXTRAS
    "M\0\0\0\0N\0\0\0\0NG\0\0\0W\0\0\0\0Y\0\0\0\0H\0\0\0\0F\0\0\0\0"
    "S\0\0\0\0SH\0\0\0V\0\0\0\0Z\0\0\0\0B\0\0\0\0D\0\0\0\0"
    "BLAH\0AH\0\0\0G\0\0\0";

/* The words of Words mode, seven characters each. */
static const char words[VB_WORDS][7] = {
    "BLAH", "YEAH", "WOW", "WHY", "YOU", "OH", "LA", "AH-OO", "REALLY",
    "MAMA", "NO", "NOW", "ONE", "AMEN", "YES", "HELLO", "ROBOT"
};
#else
    "BLAH\0AH\0\0\0";
#endif

UI_CODE static void copy(char *out, const char *from, unsigned most)
{
    while (most-- && *from)
        *out++ = *from++;
    *out = 0;
}

UI_CODE const char *fm1_vb_page_name(unsigned page)
{
    return page < FM1_VB_PAGES ? pages[page].name : "";
}

UI_CODE void fm1_vb_page_open(unsigned page)
{
    if (page < FM1_VB_PAGES)
        (void)fm1_verbalizer_set_mode(pages[page].mode);
}

UI_CODE const char *fm1_vb_label(unsigned page, unsigned slot)
{
    if (page >= FM1_VB_PAGES || slot >= FM1_VB_SLOTS || pages[page].slot[slot].knob == 255)
        return 0;
    return labels[pages[page].slot[slot].label];
}

UI_CODE void fm1_vb_value(unsigned page, unsigned slot, char out[FM1_VB_TEXT])
{
    char digits[FM1_VB_TEXT];
    unsigned kind, n = 0, magnitude;
    int value;

    out[0] = 0;
    if (!fm1_vb_label(page, slot))
        return;
    kind = pages[page].slot[slot].kind;
    value = (int)fm1_verbalizer_knob(pages[page].slot[slot].knob);
    value = (int)((unsigned)value * kinds[kind].mul / kinds[kind].div) + kinds[kind].offset;
    if (kind == PHONEME) {
        if (value < FIRST_NAMED)
            copy(out, "OFF", 3);
        else {
#ifdef VB_NO_EXTRAS
            if (value > NOISE)
                value = value == WORD || value == LAST_PHONEME ? value - WORD + NOISE + 1 : NOISE;
#endif
            copy(out, phonemes + (value - FIRST_NAMED) * 5, 5);
        }
        return;
    }
#ifndef VB_NO_EXTRAS
    if (kind == WORD_NAME) {
        copy(out, words[value < VB_WORDS ? value : VB_WORDS - 1], 7);
        return;
    }
#endif
    if (value < 0) {
        *out++ = '-';
        value = -value;
    } else if (kind == PITCH && value) {
        *out++ = '+';
    }
    magnitude = (unsigned)value;
    do
        digits[n++] = (char)('0' + magnitude % 10U);
    while (magnitude /= 10U);
    while (n)
        *out++ = digits[--n];
    *out = 0;
}

UI_CODE void fm1_vb_turn(unsigned page, unsigned slot, int detents)
{
    unsigned kind, knob;
    int step, value, most;

    if (!detents || !fm1_vb_label(page, slot))
        return;
    kind = pages[page].slot[slot].kind;
    knob = pages[page].slot[slot].knob;
    step = kinds[kind].step;
    value = (int)fm1_verbalizer_knob(knob);
    if (step == 1) {
        /* one detent is one unit; a faster turn covers the range sooner */
        value += detents == 1 || detents == -1 ? detents : detents * 2;
        most = 255;
    } else {
        /* stepped controls move one step at a time, and sit mid-step so
         * that the engine's division lands on the step meant */
        value = value / step + (detents > 0 ? 1 : -1);
        most = 255 / step;
        if (kind == PHONEME) {
#ifdef VB_NO_EXTRAS
            /* skip the run of identical noise phonemes */
            if (value > NOISE && value < WORD)
                value = detents > 0 ? WORD : NOISE;
#endif
            if (value < OFF)
                value = OFF;
            most = LAST_PHONEME;
        }
#ifndef VB_NO_EXTRAS
        if (kind == WORD_NAME)
            most = VB_WORDS - 1;
#endif
        if (value > most)
            value = most;
        value = value < 0 ? 0 : value * step + step / 2;
        most = 255;
    }
    if (value < 0)
        value = 0;
    if (value > most)
        value = most;
    (void)fm1_verbalizer_set_knob(knob, (unsigned)value);
}
