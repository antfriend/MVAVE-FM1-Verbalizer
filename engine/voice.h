/*
 * One Verbalizer voice: a vocal tract plus the phoneme and key-phase logic
 * that drives it. A port of Voice.spin by Dan Ray.
 *
 * A voice reads its settings from the instrument's knob array (`pot`, values
 * 0..255). The same sixteen knobs mean different things in each mode, exactly
 * as on the hardware; the VB_POT_* names below say which is which.
 */
#ifndef VOICE_H
#define VOICE_H

#include "vocal_tract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VB_NUM_POTS 16

/* Trigger / Sustain / Release mode. Each key phase has its own five knobs. */
enum {
    VB_POT_TRIG_PHONEME   = 0,  /* /7 selects a phoneme, see docs/phonemes.md */
    VB_POT_TRIG_TIME      = 1,  /* x2 glide steps of 700 us */
    VB_POT_TRIG_VOLUME    = 2,  /* /3 glottal amplitude */
    VB_POT_TRIG_VIB_DEPTH = 3,  /* /3 vibrato pitch */
    VB_POT_TRIG_VIB_RATE  = 4,  /* /3 vibrato rate */
    VB_POT_SUS_PHONEME    = 5,
    VB_POT_SUS_TIME       = 6,
    VB_POT_SUS_VOLUME     = 7,
    VB_POT_SUS_VIB_DEPTH  = 8,
    VB_POT_SUS_VIB_RATE   = 9,
    VB_POT_REL_PHONEME    = 10,
    VB_POT_REL_TIME       = 11,
    VB_POT_SPACE          = 12, /* echo/reverb amount; unused until the spatializer is ported */
    VB_POT_REL_VOLUME     = 13,
    VB_POT_REL_VIB_DEPTH  = 14,
    VB_POT_REL_VIB_RATE   = 15
};

/* Formants mode. The knobs set tract parameters directly. */
enum {
    VB_POT_FM_AA    = 0,
    VB_POT_FM_F4    = 1,
    VB_POT_FM_F3    = 2,
    VB_POT_FM_F1    = 3,
    VB_POT_FM_F2    = 4,
    VB_POT_FM_PITCH = 5,  /* /21 - 6: transpose by -6..+6 keys */
    VB_POT_FM_TIME  = 6,  /* x2 glide steps */
    VB_POT_FM_GA    = 7,  /* /3 */
    VB_POT_FM_VP    = 8,  /* /3 */
    VB_POT_FM_VR    = 9,  /* /3 */
    VB_POT_FM_FA    = 10,
    VB_POT_FM_FF    = 13,
    VB_POT_FM_NA    = 14,
    VB_POT_FM_NF    = 15
};

/* Words mode. */
enum {
    VB_POT_WORD_TIME = 1
};

/* Phoneme numbers (knob value / 7). 20..33 and 36 are the noise phoneme. */
enum {
    VB_PH_SILENCE    = 0,  /* 0, 1 and 2 */
    VB_PH_SOAP       = 3,
    VB_PH_BEET       = 4,
    VB_PH_HAT        = 5,
    VB_PH_HOT        = 6,
    VB_PH_BORROW     = 7,
    VB_PH_BALL       = 8,
    VB_PH_EE         = 9,
    VB_PH_I          = 10,
    VB_PH_E          = 11,
    VB_PH_A          = 12,
    VB_PH_O          = 13,
    VB_PH_OH         = 14,
    VB_PH_FOOT       = 15,
    VB_PH_BOOT       = 16,
    VB_PH_R          = 17,
    VB_PH_L          = 18,
    VB_PH_UH         = 19,
    VB_PH_NOISE      = 20,
    VB_PH_BLAH       = 34, /* the word "blah", queued as several frames */
    VB_PH_SUSTAIN_AH = 35
};

typedef struct {
    vocal_tract    vt;
    vt_params      p;        /* staged parameters; go() queues a copy */
    const uint8_t *pot;      /* the instrument's VB_NUM_POTS knobs */
    uint32_t       dropped;  /* frames lost to a full queue (should stay 0) */
} vb_voice;

/*
 * Keys are numbered as on the original's 37-key keyboard: key 1 is A1 (55 Hz)
 * and each key is a semitone, so key = MIDI note - 32. Usable range -12..51.
 */

/* Start the voice: resets the tract and queues a short silent frame. */
void vb_voice_start(vb_voice *v, const uint8_t *pot);

/* True when every queued frame has been played. */
bool vb_voice_done(const vb_voice *v);

/* Trigger / Sustain / Release mode: the three phases of a keystroke. */
void vb_voice_trigger(vb_voice *v, int key);
void vb_voice_sustain(vb_voice *v, int key);
void vb_voice_release(vb_voice *v, int key);

/* Words mode: say the word. */
void vb_voice_word(vb_voice *v, int key);

/* Formants mode: queue the knobs' current tract settings at this key's pitch. */
void vb_voice_formants(vb_voice *v, int key);

/* Fade to silence. */
void vb_voice_null(vb_voice *v);

#ifdef __cplusplus
}
#endif

#endif
