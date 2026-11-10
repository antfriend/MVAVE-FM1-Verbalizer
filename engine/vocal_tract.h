/*
 * Vocal tract synthesizer: a C port of "Vocal Tract v1.1" by Chip Gracey,
 * Copyright (c) 2006 Parallax, Inc., released under the MIT License.
 * The full notice is at the top of vocal_tract.c.
 *
 * The tract produces one signed 32-bit sample per tick at a fixed 20 kHz and
 * is bit-exact with the Propeller original (see tests/golden and
 * tools/pasm_ref.py). It uses integer arithmetic only and has no hardware
 * dependencies.
 *
 * Usage: fill in a vt_params, call vt_go() to queue a glide towards those
 * values over a number of 700 us steps, and pull samples with vt_tick() or
 * vt_render(). Frames play back to back; when the queue runs dry the tract
 * holds the last frame.
 */
#ifndef VOCAL_TRACT_H
#define VOCAL_TRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VT_SAMPLE_RATE     20000 /* Hz, fixed: every unit below depends on it */
#define VT_NUM_PARAMS      13
#define VT_SAMPLES_PER_STEP 14   /* one glide step = 14 samples = 700 us */

/* Frames the original could hold. vt_full() reports against this number. */
#define VT_ORIGINAL_FRAMES 8
/* Frames this port can hold (power of two). The extra room replaces the
 * original's habit of blocking the caller inside go() until a slot is free. */
#define VT_QUEUE_FRAMES    32

/* The 13 tract parameters, in the original's order. */
typedef struct {
    uint8_t aa; /* aspiration amplitude  0..255, linear                    */
    uint8_t ga; /* glottal amplitude     0..255, linear                    */
    uint8_t gp; /* glottal pitch         1/48 octave, 100 = 110 Hz (A2)    */
    uint8_t vp; /* vibrato pitch         1/48 octave, 48 = +-1/2 octave    */
    uint8_t vr; /* vibrato rate          0.0763 Hz,   52 = 4 Hz            */
    uint8_t f1; /* formant 1 frequency   19.53 Hz,    40 = 781 Hz          */
    uint8_t f2; /* formant 2 frequency   19.53 Hz,    56 = 1094 Hz         */
    uint8_t f3; /* formant 3 frequency   19.53 Hz,   128 = 2500 Hz         */
    uint8_t f4; /* formant 4 frequency   19.53 Hz,   179 = 3496 Hz         */
    uint8_t na; /* nasal amplitude       0..255, linear                    */
    uint8_t nf; /* nasal frequency       19.53 Hz,   102 = 1992 Hz         */
    uint8_t fa; /* frication amplitude   0..255, linear                    */
    uint8_t ff; /* frication frequency   39.06 Hz,    60 = 2344 Hz ("sh")  */
} vt_params;

typedef struct {
    uint32_t step;                 /* 0x01000000 / time; never 0 when queued */
    uint8_t  par[VT_NUM_PARAMS];
} vt_frame;

typedef struct {
    /* frame queue */
    vt_frame queue[VT_QUEUE_FRAMES];
    uint8_t  head;                 /* frame being played / next to play */
    uint8_t  count;                /* frames queued, including the one playing */

    /* settings */
    uint8_t  attenuation;          /* 0..7, 6 dB per step (an arithmetic shift) */
    uint32_t pace;                 /* percent, 100 = normal */

    /* frame handler */
    uint8_t  state;
    uint8_t  frame_cnt;
    uint32_t step_size;
    uint32_t step_acc;
    uint32_t cur[VT_NUM_PARAMS];   /* parameter in the top byte, fraction below */
    uint32_t next[VT_NUM_PARAMS];
    uint32_t delta[VT_NUM_PARAMS];

    /* oscillators and filters */
    uint32_t lfsr;
    uint32_t vphase, gphase, fphase;
    uint32_t fx[4], fy[4];         /* formant resonator states */
    uint32_t nx;                   /* nasal state */
    uint32_t x;                    /* sample computed last tick */
} vocal_tract;

/* Reset to the power-on state: silent, queue empty, pace 100, attenuation 0. */
void vt_init(vocal_tract *vt);

/* Master attenuation, 0..7. Takes effect on the next sample. */
void vt_set_attenuation(vocal_tract *vt, unsigned level);

/* Pace in percent; scales the time of frames queued afterwards. 0 is ignored. */
void vt_set_pace(vocal_tract *vt, unsigned percent);

/*
 * Queue a glide from wherever the tract is to `params`, taking `time` steps of
 * 700 us (scaled by pace, minimum 2 steps). Returns false and queues nothing
 * if the queue is full.
 */
bool vt_go(vocal_tract *vt, const vt_params *params, uint32_t time);

/* Frames queued, including the one in progress. */
unsigned vt_queued(const vocal_tract *vt);

/* True once every queued frame has finished (the original's `empty`). */
bool vt_empty(const vocal_tract *vt);

/* True where the original's go() would have blocked: 8 or more frames queued. */
bool vt_full(const vocal_tract *vt);

/*
 * Step the noise generator `samples` sample periods without producing audio.
 * Tracts started together otherwise make identical noise; on the Propeller
 * each ran in its own cog and they were never aligned.
 */
void vt_advance_noise(vocal_tract *vt, unsigned samples);

/* Produce one 20 kHz sample. */
int32_t vt_tick(vocal_tract *vt);

/* Produce `count` samples. */
void vt_render(vocal_tract *vt, int32_t *out, size_t count);

#ifdef __cplusplus
}
#endif

#endif
