/*
 * Vocal tract synthesizer.
 *
 * A C port of "Vocal Tract v1.1" by Chip Gracey, Copyright (c) 2006 Parallax, Inc.
 * (VocalTract.spin, 28 October 2006), which carries these terms:
 *
 * TERMS OF USE: MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Porting notes
 * -------------
 * The Propeller code keeps everything in 32-bit registers that wrap on
 * overflow, and that wrap is audible (it is the Verbalizer's "crackle"), so
 * all state here is uint32_t and signed values are only formed for arithmetic
 * right shifts and the two multiplies. Comments quote the original's section
 * names so the two can be read side by side.
 */
#include "vocal_tract.h"

#include <string.h>

#include "vt_tables.h"

/* vt_params is copied in and out as 13 raw bytes. */
typedef char vt_params_is_13_bytes[sizeof(vt_params) == VT_NUM_PARAMS ? 1 : -1];
/* Arithmetic right shift of negative values is assumed (true of every
 * compiler this is likely to meet; the build fails here otherwise). */
typedef char vt_needs_arithmetic_shift[((-2) >> 1) == -1 ? 1 : -1];
typedef char vt_queue_is_power_of_two[(VT_QUEUE_FRAMES & (VT_QUEUE_FRAMES - 1)) == 0 ? 1 : -1];

#define SAR(v, n) ((uint32_t)((int32_t)(v) >> (n)))

enum { P_AA, P_GA, P_GP, P_VP, P_VR, P_F1, P_F2, P_F3, P_F4, P_NA, P_NF, P_FA, P_FF };

/* Frame handler states. The original is a coroutine that runs one short
 * segment per sample; each state below is one of its resume points. */
enum {
    ST_START,     /* first tick after reset only yields */
    ST_WAIT,      /* look for a queued frame */
    ST_FINAL,     /* no frame: bring one parameter per sample to its target */
    ST_SET,       /* new frame: compute one parameter's step per sample */
    ST_STEPFRAME, /* start of a glide step */
    ST_STEP       /* add one parameter's step per sample */
};

#define TUNE      0x66920000u /* scale tuned to 110.00 Hz at gp = 100 */
#define LFSR_TAPS 0x80061000u

static const uint32_t cordic_delta[12] = {
    0x4B901476u, 0x27ECE16Du, 0x14444750u, 0x0A2C350Cu, 0x05175F85u, 0x028BD879u,
    0x0145F154u, 0x00A2F94Du, 0x00517CBBu, 0x0028BE60u, 0x00145F30u, 0x000A2F98u,
};

static uint32_t parity32(uint32_t v)
{
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    return (0x6996u >> (v & 15)) & 1;
}

static uint32_t lfsr_next(uint32_t lfsr)
{
    return (lfsr << 1) | parity32(lfsr & LFSR_TAPS);
}

/* Antilog: top 4 bits of `log` are the whole part, the next 11 the fraction.
 * Returns 0x00010000..0xFFEA0000. */
static uint32_t antilog(uint32_t log)
{
    return (0x10000u | vt_antilog_table[(log >> 17) & 0x7FF]) << (log >> 28);
}

/* Multiply: 15 top bits of unsigned `scale` by 17 top bits of signed `value`.
 * The original does this with 15 shift-and-add steps, which come to the same
 * 32-bit product. */
static uint32_t mult(uint32_t scale, uint32_t value)
{
    return (uint32_t)((int32_t)(scale >> 17) * ((int32_t)value >> 15));
}

/* Scaled sine: 15 top bits of `scale` times the sine of the 13 top bits of
 * `angle` (a full turn is 2^32). */
static uint32_t sine(uint32_t scale, uint32_t angle)
{
    uint32_t a = angle >> 19;
    uint32_t i = a & 0x7FF;
    int32_t s;

    if (a & 0x800) /* quadrants 2 and 4 read the table backwards */
        i = 0x800 - i;
    s = vt_sine_table[i];
    if (a & 0x1000) /* quadrants 3 and 4 are negative */
        s = -s;
    return (uint32_t)((int32_t)(scale >> 17) * s);
}

/* Cordic rotation: scale (x, y) by 0.60725 * 0.984 (the cordic gain correction
 * plus slight damping), then rotate by `a`, where 2^32 is 90 degrees. */
static void cordic(uint32_t *px, uint32_t *py, uint32_t a)
{
    uint32_t x = *px, y = *py, t;
    uint32_t c;
    int i;

    x = SAR(x, 1);
    x += SAR(x, 3);
    x += SAR(x, 4);
    y = SAR(y, 1);
    y += SAR(y, 3);
    y += SAR(y, 4);

    /* first step: 45 degrees */
    t = x;
    x -= y;
    y += t;
    a -= 0x80000000u;
    c = a >> 31; /* set when the remaining angle is negative */

    for (i = 1; i <= 13; i++) {
        uint32_t dx = SAR(y, i);
        uint32_t dy = SAR(x, i);

        if (c) {
            x += dx;
            y -= dy;
            if (i <= 12)
                a += cordic_delta[i - 1];
        } else {
            x -= dx;
            y += dy;
            if (i <= 12)
                a -= cordic_delta[i - 1];
        }
        c = a >> 31;
    }
    *px = x;
    *py = y;
}

/* ---- saturating variants (vt_set_saturate) ------------------------------ */

static uint32_t clamp32(int64_t v)
{
    if (v > INT32_MAX)
        v = INT32_MAX;
    if (v < INT32_MIN)
        v = INT32_MIN;
    return (uint32_t)(int32_t)v;
}

static uint32_t sat_add(uint32_t a, uint32_t b)
{
    return clamp32((int64_t)(int32_t)a + (int32_t)b);
}

/* The same rotation, but it cannot wrap. A vector no longer than 2^31 cannot
 * overflow inside the rotation (its gain is 0.984), so that case uses the
 * 32-bit routine. Longer vectors, which only arise once an input has already
 * been clipped, are rotated in 64 bits and the result is clipped. */
static void cordic_sat(uint32_t *px, uint32_t *py, uint32_t a)
{
    const int32_t safe = 0x5A827999; /* 2^31 / sqrt(2) */
    int32_t sx = (int32_t)*px, sy = (int32_t)*py;
    int64_t x, y, t;
    uint32_t c;
    int i;

    if (sx > -safe && sx < safe && sy > -safe && sy < safe) {
        cordic(px, py, a);
        return;
    }

    x = sx >> 1;
    x += x >> 3;
    x += x >> 4;
    y = sy >> 1;
    y += y >> 3;
    y += y >> 4;

    t = x;
    x -= y;
    y += t;
    a -= 0x80000000u;
    c = a >> 31;

    for (i = 1; i <= 13; i++) {
        int64_t dx = y >> i;
        int64_t dy = x >> i;

        if (c) {
            x += dx;
            y -= dy;
            if (i <= 12)
                a += cordic_delta[i - 1];
        } else {
            x -= dx;
            y += dy;
            if (i <= 12)
                a -= cordic_delta[i - 1];
        }
        c = a >> 31;
    }
    *px = clamp32(x);
    *py = clamp32(y);
}

/* Run one segment of the frame handler. */
static void frame_handler(vocal_tract *vt)
{
    unsigned i;

    switch (vt->state) {
    case ST_START:
        vt->state = ST_WAIT;
        break;

    case ST_WAIT:
        vt->frame_cnt = 0;
        if (vt->count) {
            /* next frame ready; the +1 insures accurate accumulation */
            vt->step_size = vt->queue[vt->head].step + 1;
            vt->step_acc = vt->step_size;
            vt->state = ST_SET;
        } else {
            vt->state = ST_FINAL;
        }
        break;

    case ST_FINAL:
        i = vt->frame_cnt;
        vt->cur[i] = vt->next[i];
        if (++vt->frame_cnt == VT_NUM_PARAMS)
            vt->state = ST_WAIT;
        break;

    case ST_SET: {
        uint32_t target, from;

        i = vt->frame_cnt;
        target = (uint32_t)vt->queue[vt->head].par[i] << 24;
        from = vt->next[i];
        vt->cur[i] = from;
        vt->next[i] = target;
        /* signed step = (target - from, as a byte) * step size */
        if (target < from)
            vt->delta[i] = 0u - ((from - target) >> 24) * vt->step_size;
        else
            vt->delta[i] = ((target - from) >> 24) * vt->step_size;
        if (++vt->frame_cnt == VT_NUM_PARAMS)
            vt->state = ST_STEPFRAME;
        break;
    }

    case ST_STEPFRAME:
        vt->frame_cnt = 0;
        vt->state = ST_STEP;
        break;

    case ST_STEP:
        i = vt->frame_cnt;
        vt->cur[i] += vt->delta[i];
        if (++vt->frame_cnt == VT_NUM_PARAMS) {
            vt->step_acc += vt->step_size;
            if (vt->step_acc & 0x01000000u) {
                /* frame done */
                vt->head = (uint8_t)((vt->head + 1) & (VT_QUEUE_FRAMES - 1));
                vt->count--;
                vt->state = ST_WAIT;
            } else {
                vt->state = ST_STEPFRAME;
            }
        }
        break;
    }
}

void vt_init(vocal_tract *vt)
{
    memset(vt, 0, sizeof *vt);
    vt->pace = 100;
    vt->lfsr = 1;
    vt->state = ST_START;
}

void vt_set_saturate(vocal_tract *vt, bool on)
{
    vt->saturate = on;
}

void vt_set_attenuation(vocal_tract *vt, unsigned level)
{
    vt->attenuation = (uint8_t)(level > 31 ? 31 : level);
}

void vt_set_pace(vocal_tract *vt, unsigned percent)
{
    if (percent)
        vt->pace = percent;
}

bool vt_go(vocal_tract *vt, const vt_params *params, uint32_t time)
{
    vt_frame *f;
    uint64_t steps;
    uint32_t step;

    if (vt->count >= VT_QUEUE_FRAMES)
        return false;

    steps = (uint64_t)time * 100 / vt->pace;
    if (steps < 2)
        steps = 2;
    /* A step of 0 marks an empty slot in the original and would stall it;
     * that needs a time above 16.7 million steps, so just keep it at 1. */
    step = steps >= 0x01000000u ? 1 : (uint32_t)(0x01000000u / steps);

    f = &vt->queue[(vt->head + vt->count) & (VT_QUEUE_FRAMES - 1)];
    f->step = step;
    memcpy(f->par, params, VT_NUM_PARAMS);
    vt->count++;
    return true;
}

unsigned vt_queued(const vocal_tract *vt)
{
    return vt->count;
}

bool vt_empty(const vocal_tract *vt)
{
    return vt->count == 0;
}

bool vt_full(const vocal_tract *vt)
{
    return vt->count >= VT_ORIGINAL_FRAMES;
}

void vt_advance_noise(vocal_tract *vt, unsigned samples)
{
    while (samples--)
        vt->lfsr = lfsr_next(lfsr_next(lfsr_next(vt->lfsr)));
}

int32_t vt_tick(vocal_tract *vt)
{
    const uint32_t *p = vt->cur;
    const bool sat = vt->saturate;
    uint32_t x, y, t1, t2;
    int32_t out;
    int i;

    /* output the sample computed last tick, after master attenuation */
    out = (int32_t)SAR(vt->x, vt->attenuation);

    /* White noise source */
    vt->lfsr = lfsr_next(lfsr_next(lfsr_next(vt->lfsr)));

    /* Aspiration */
    x = SAR(mult(p[P_AA], vt->lfsr), 8);

    /* Vibrato */
    vt->vphase += p[P_VR] >> 10;
    t1 = sine(p[P_VP], vt->vphase) + p[P_GP];

    /* Glottal pulse: divide the pitch by 3 to mesh with the 12 notes/octave
     * musical scale (multiply by %0.0101010101010101) */
    t1 >>= 2;
    t1 += t1 >> 2;
    t1 += t1 >> 4;
    t1 += t1 >> 8;
    t1 += TUNE;
    vt->gphase += antilog(t1);
    t2 = antilog(vt->gphase) - 0x40000000u;
    x += SAR(sine(p[P_GA], t2), 6);

    /* Vocal tract formants: four resonators in series */
    y = 0;
    for (i = 0; i < 4; i++) {
        if (sat) {
            x = sat_add(x, vt->fx[i]);
            y = sat_add(y, vt->fy[i]);
            cordic_sat(&x, &y, p[P_F1 + i]);
        } else {
            x += vt->fx[i];
            y += vt->fy[i];
            cordic(&x, &y, p[P_F1 + i]);
        }
        vt->fx[i] = x;
        vt->fy[i] = y;
    }

    /* Nasal anti-formant */
    if (sat) {
        vt->nx = sat_add(vt->nx, x);
        cordic_sat(&x, &y, p[P_NF]);
    } else {
        vt->nx += x;
        cordic(&x, &y, p[P_NF]);
    }
    t1 = mult(p[P_NA], x);
    x = vt->nx;
    vt->nx = 0u - t1;

    /* Frication */
    t1 = SAR(vt->lfsr, 3);
    vt->fphase += t1;
    vt->fphase += SAR(t1, 1);
    vt->fphase += p[P_FF] >> 1;
    t1 = sine(p[P_FA], vt->fphase);
    x = sat ? sat_add(x, t1) : x + t1;

    vt->x = x;

    /* Handle frame (after the sample, so `p` was stable throughout) */
    frame_handler(vt);

    return out;
}

void vt_render(vocal_tract *vt, int32_t *out, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        out[i] = vt_tick(vt);
}
