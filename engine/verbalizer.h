/*
 * The Verbalizer instrument: four voices, the three play modes and the key
 * handling. A port of VerbalizeIt.spin and the main loop of Verbalizer.spin
 * by Dan Ray.
 *
 * Feed it key (or MIDI note) events and knob values, and pull mono audio at
 * 20 kHz. Nothing here touches hardware.
 */
#ifndef VERBALIZER_H
#define VERBALIZER_H

#include "voice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VB_VOICES       4
#define VB_SAMPLE_RATE  VT_SAMPLE_RATE

/* Keys are the original's key numbers: key 1 = A1, key = MIDI note - 32. */
#define VB_KEY_MIN      (-12)   /* MIDI 20 */
#define VB_KEY_MAX      51      /* MIDI 83 */
#define VB_NUM_KEYS     (VB_KEY_MAX - VB_KEY_MIN + 1)
#define VB_MIDI_OFFSET  32

/* The original scanned its keyboard about every 2 ms; so does this. */
#define VB_SCAN_SAMPLES 40

typedef enum {
    VB_MODE_WORDS,    /* each key says a word */
    VB_MODE_TSR,      /* trigger / sustain / release: the main mode */
    VB_MODE_FORMANTS  /* knobs drive the tract directly; one voice */
} vb_mode;

/* The voices point at `pot`, so do not copy or move an initialised verbalizer. */
typedef struct {
    uint8_t  pot[VB_NUM_POTS];   /* knobs, 0..255; write these directly */
    vb_mode  mode;

    vb_voice voice[VB_VOICES];
    int      voice_key[VB_VOICES];        /* last key each voice played */

    uint8_t  key_state[VB_NUM_KEYS];
    int      formants_cursor;             /* round-robin over held keys */
    unsigned formants_fade_pending;       /* releases still owed a fade */
    unsigned scan_countdown;
} verbalizer;

/* Reset everything: knobs at 0, Trigger/Sustain/Release mode, all silent,
 * crackle off. */
void vb_init(verbalizer *vb);

/*
 * The original's arithmetic wraps when a voice is driven too hard, which is
 * heard as bursts of loud noise. By default the voices clip instead. Pass
 * true to get the original's crackle back.
 */
void vb_set_crackle(verbalizer *vb, bool on);

void vb_set_mode(verbalizer *vb, vb_mode mode);

/* Key events. Out-of-range keys are ignored and return false. */
bool vb_key_down(verbalizer *vb, int key);
bool vb_key_up(verbalizer *vb, int key);

/* The same, by MIDI note number (20..83). */
bool vb_note_on(verbalizer *vb, int note);
bool vb_note_off(verbalizer *vb, int note);

/* Release every key. */
void vb_all_keys_up(verbalizer *vb);

/* True when no key is held and every voice has finished speaking. */
bool vb_idle(const verbalizer *vb);

/*
 * Render `count` mono samples at 20 kHz, full-scale signed 32-bit. The four
 * voices are summed, halved and clipped. The result does not depend on how
 * the calls are split up.
 */
void vb_render(verbalizer *vb, int32_t *out, size_t count);

#ifdef __cplusplus
}
#endif

#endif
