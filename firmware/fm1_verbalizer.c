/* SPDX-License-Identifier: MIT */
/* The Verbalizer inside M-VAVE's open FM-1 firmware. See fm1_verbalizer.h. */
#include "fm1_verbalizer.h"

#include "fm1_audio.h"
#include "resampler.h"

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

/* A patch to start with until the firmware has a way to edit and store them:
 * trigger "boot", sustain "a" with a little vibrato, release "oh". */
static const uint8_t first_patch[VB_NUM_POTS] FM1_AUDIO_DATA = {
    115, 20, 90, 0, 0, 87, 60, 90, 12, 150, 101, 40, 0, 60, 0, 0
};

int fm1_verbalizer_midi(const uint8_t *message, size_t bytes)
{
    uint32_t flags, slot;
    int result = -1;

    if (!message || bytes < 2 || bytes > 3 || message[0] < 0x80 || message[0] >= 0xf0)
        return -1;
    flags = fm1_audio_irq_save();
    if (queue_in - queue_out < QUEUE_SIZE) {
        slot = queue_in & (QUEUE_SIZE - 1U);
        queue[slot][0] = message[0];
        queue[slot][1] = message[1];
        queue[slot][2] = bytes > 2 ? message[2] : 0;
        queue_in = queue_in + 1U;
        result = 0;
    } else {
        dropped = dropped + 1U;
    }
    fm1_audio_irq_restore(flags);
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
            instrument.pot[i] = first_patch[i];
        vb_output_init(&output_stage, &instrument);
        started = 1;
    }
    while (queue_out != queue_in) {
        uint32_t slot = queue_out & (QUEUE_SIZE - 1U);

        vb_midi(&instrument, queue[slot][0], queue[slot][1], queue[slot][2]);
        queue_out = queue_out + 1U;
    }
    vb_output_render(&output_stage, output, frames);
}
