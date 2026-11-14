/* SPDX-License-Identifier: MIT */
/* The Verbalizer inside M-VAVE's open FM-1 firmware. See fm1_verbalizer.h. */
#include "fm1_verbalizer.h"

#include "resampler.h"

/* What keeps the audio interrupt out while a task writes the queue. The
 * browser preview and host tests run everything on one thread, and have no
 * board header. */
#ifdef FM1_HOST_TEST
#define queue_lock() 0U
#define queue_unlock(flags) ((void)(flags))
#else
#include "fm1_audio.h"
#define queue_lock() fm1_audio_irq_save()
#define queue_unlock(flags) fm1_audio_irq_restore(flags)
#endif

/* MIDI queue: written by tasks with interrupts masked, read by the audio
 * interrupt. Indexes only ever grow; the slot is index modulo the size. */
#define QUEUE_SIZE 64U /* power of two */

static volatile uint8_t queue[QUEUE_SIZE][3];
static volatile uint32_t queue_in;  /* next slot to write: tasks */
static volatile uint32_t queue_out; /* next slot to read: audio interrupt */
static volatile uint32_t dropped;

static verbalizer instrument;
static vb_output output_stage;
static uint8_t started;

/* Queue entries whose first byte is below 0x80 are not MIDI but the
 * firmware's own: set a knob to a full 0..255 value, or set the mode. */
enum { SET_KNOB = 1, SET_MODE = 2 };

/* The knobs of each mode, as the tasks see them. Start-up values, until the
 * firmware can store patches: Words at a medium length; trigger "boot",
 * sustain "a" with a little vibrato, release "oh"; and for Formants the
 * vowel "a". The audio interrupt reads this once, when it starts. */
static uint8_t patch[3][VB_NUM_POTS] = {
    {0, 40},
    {115, 20, 90, 0, 0, 87, 60, 90, 12, 150, 101, 40, 0, 60, 0, 0},
    {0, 174, 130, 35, 92, 128, 20, 90, 12, 150}
};
static uint8_t mode = VB_MODE_TSR;

/* Caller holds the lock. */
static int put(unsigned a, unsigned b, unsigned c)
{
    uint32_t slot = queue_in & (QUEUE_SIZE - 1U);

    if (queue_in - queue_out >= QUEUE_SIZE) {
        dropped = dropped + 1U;
        return -1;
    }
    queue[slot][0] = (uint8_t)a;
    queue[slot][1] = (uint8_t)b;
    queue[slot][2] = (uint8_t)c;
    queue_in = queue_in + 1U;
    return 0;
}

unsigned fm1_verbalizer_mode(void)
{
    return mode;
}

unsigned fm1_verbalizer_knob(unsigned knob)
{
    return patch[mode][knob & (VB_NUM_POTS - 1U)];
}

int fm1_verbalizer_set_knob(unsigned knob, unsigned value)
{
    uint32_t flags;
    int result;

    if (knob >= VB_NUM_POTS || value > 255)
        return -1;
    flags = queue_lock();
    result = put(SET_KNOB, knob, value);
    if (!result)
        patch[mode][knob] = (uint8_t)value;
    queue_unlock(flags);
    return result;
}

int fm1_verbalizer_set_mode(unsigned next)
{
    uint32_t flags;
    unsigned i;
    int result = -1;

    if (next > VB_MODE_FORMANTS)
        return -1;
    if (next == mode)
        return 0;
    flags = queue_lock();
    /* all or nothing: the mode and its sixteen knobs */
    if (QUEUE_SIZE - (queue_in - queue_out) > VB_NUM_POTS) {
        (void)put(SET_MODE, next, 0);
        for (i = 0; i < VB_NUM_POTS; i++)
            (void)put(SET_KNOB, i, patch[next][i]);
        mode = (uint8_t)next;
        result = 0;
    } else {
        dropped = dropped + 1U;
    }
    queue_unlock(flags);
    return result;
}

int fm1_verbalizer_midi(const uint8_t *message, size_t bytes)
{
    uint32_t flags;
    int result;

    if (!message || bytes < 2 || bytes > 3 || message[0] < 0x80 || message[0] >= 0xf0)
        return -1;
    /* The knob and mode controllers go the same way as the screen's edits,
     * so that the per-mode knob sets stay true. */
    if ((message[0] & 0xf0) == 0xb0 && bytes == 3) {
        unsigned controller = message[1], value = message[2] & 127U;

        if (controller >= VB_CC_KNOB_FIRST && controller < VB_CC_KNOB_FIRST + VB_NUM_POTS)
            return fm1_verbalizer_set_knob(controller - VB_CC_KNOB_FIRST, (value << 1) | (value >> 6));
        if (controller == VB_CC_MODE)
            return fm1_verbalizer_set_mode(value / 43U);
    }
    flags = queue_lock();
    result = put(message[0], message[1], bytes > 2 ? message[2] : 0);
    queue_unlock(flags);
    return result;
}

uint32_t fm1_verbalizer_dropped(void)
{
    return dropped;
}

FM1_AUDIO_CODE void fm1_verbalizer_render(int32_t *output, unsigned frames)
{
    unsigned i;

    if (!started) {
        vb_init(&instrument);
        for (i = 0; i < VB_NUM_POTS; i++)
            instrument.pot[i] = patch[VB_MODE_TSR][i];
        vb_output_init(&output_stage, &instrument);
        started = 1;
    }
    while (queue_out != queue_in) {
        uint32_t slot = queue_out & (QUEUE_SIZE - 1U);
        unsigned a = queue[slot][0], b = queue[slot][1], c = queue[slot][2];

        if (a == SET_KNOB)
            instrument.pot[b & (VB_NUM_POTS - 1U)] = (uint8_t)c;
        else if (a == SET_MODE)
            vb_set_mode(&instrument, (vb_mode)b);
        else
            vb_midi(&instrument, (uint8_t)a, (uint8_t)b, (uint8_t)c);
        queue_out = queue_out + 1U;
    }
    vb_output_render(&output_stage, output, frames);
}
