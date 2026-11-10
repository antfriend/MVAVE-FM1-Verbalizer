/*
 * The Verbalizer instrument: four voices, the three play modes and the key
 * handling. A port of VerbalizeIt.spin and the main loop of Verbalizer.spin
 * by Dan Ray.
 *
 * Feed it key or MIDI events and knob values, and pull mono audio at 20 kHz.
 * Nothing here touches hardware.
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

/* A key released before it got a voice still plays if a voice comes free
 * within this many scans (100 ms); after that it is forgotten. */
#define VB_TAP_WAIT_SCANS 50

/* MIDI controllers understood by vb_midi(). See docs/midi.md. */
#define VB_CC_PEDAL         64
#define VB_CC_KNOB_FIRST    102  /* 102..117 set knobs 0..15 */
#define VB_CC_MODE          118  /* 0..42 words, 43..85 trigger/sustain/release, 86..127 formants */
#define VB_CC_CRACKLE       119  /* 64 and up turns the crackle on */
#define VB_CC_ALL_SOUND_OFF 120
#define VB_CC_ALL_NOTES_OFF 123

/* Pitch bend range, in semitones each way. */
#define VB_BEND_SEMITONES   2

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
    uint8_t  key_velocity[VB_NUM_KEYS];
    uint8_t  key_pedalled[VB_NUM_KEYS];   /* released while the pedal was down */
    uint8_t  key_tap_wait[VB_NUM_KEYS];   /* scans a tapped key will still wait */
    bool     pedal;
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

/*
 * Key events. Out-of-range keys are ignored and return false. `velocity` is
 * 1..127 and scales the loudness of the note; 127 plays the knobs' volumes
 * as they are, which is what vb_key_down() does.
 *
 * A key released before the instrument has started it still plays, as a note
 * of the shortest possible length, provided a voice comes free within 100 ms.
 */
bool vb_key_down(verbalizer *vb, int key);
bool vb_key_down_velocity(verbalizer *vb, int key, int velocity);
bool vb_key_up(verbalizer *vb, int key);

/* The same, by MIDI note number (20..83). */
bool vb_note_on(verbalizer *vb, int note);
bool vb_note_on_velocity(verbalizer *vb, int note, int velocity);
bool vb_note_off(verbalizer *vb, int note);

/* Sustain pedal: while it is down, released keys keep sounding. */
void vb_set_pedal(verbalizer *vb, bool down);

/*
 * Pitch bend, -8192..8191 for -2..+2 semitones, in steps of a quarter
 * semitone. It applies to frames queued from now on, so a held note follows
 * it each time its sustain phase repeats.
 */
void vb_set_pitch_bend(verbalizer *vb, int bend);

/*
 * Feed one MIDI channel message (any channel). Handles note on/off with
 * velocity, pitch bend, the sustain pedal, all-notes-off, and the VB_CC_*
 * controllers. Returns false for messages it does not use.
 */
bool vb_midi(verbalizer *vb, uint8_t status, uint8_t data1, uint8_t data2);

/* Release every key, pedal or not. */
void vb_all_keys_up(verbalizer *vb);

/* True when no key is held and every voice has finished speaking. */
bool vb_idle(const verbalizer *vb);

/*
 * Render `count` mono samples at 20 kHz, full-scale signed 32-bit. The four
 * voices are summed. Up to half of full scale the sum passes through
 * unchanged; above that it is squeezed smoothly so that it can never clip
 * hard. The result does not depend on how the calls are split up.
 */
void vb_render(verbalizer *vb, int32_t *out, size_t count);

#ifdef __cplusplus
}
#endif

#endif
