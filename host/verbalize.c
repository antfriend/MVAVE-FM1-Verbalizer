/*
 * verbalize: play a score through the whole instrument and write a WAV file.
 *
 *   verbalize score.txt out.wav [44100]
 *
 * With 44100 the audio goes through the FM-1 output stage (the resampler and
 * 24-bit conversion the firmware will use) and the file is written at
 * 44.1 kHz. Otherwise it is the engine's own 20 kHz.
 *
 * Score format, one command per line, '#' starts a comment:
 *
 *   mode words|tsr|formants   select the play mode
 *   crackle on|off            wrap on overflow like the original (default off)
 *   pot <index> <value>       set one knob (index 0..15, value 0..255)
 *   pots <v0> <v1> ...        set knobs 0, 1, ... in order
 *   on <note> [velocity]      MIDI note on  (20..83), velocity 1..127
 *   off <note>                MIDI note off
 *   pedal on|off              sustain pedal
 *   bend <value>              pitch bend, -8192..8191 for -2..+2 semitones
 *   wait <ms>                 let time pass
 *
 * When the score ends, all keys are released and the tail is rendered.
 * Output is 16-bit mono.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hostlib.h"
#include "resampler.h"
#include "verbalizer.h"

/* set when writing at the FM-1's rate */
static vb_output *fm1_output;

/* Render `n` samples of engine time (20 kHz). */
static bool render(verbalizer *vb, samples *out, size_t n)
{
    int32_t *dst;

    if (!fm1_output) {
        dst = samples_reserve(out, n);
        if (!dst)
            return false;
        vb_render(vb, dst, n);
        samples_commit(out, n);
        return true;
    }
    /* the same stretch of time in FM-1 blocks: 441 output frames per 200 */
    n = n * RS_UP / RS_DOWN;
    while (n) {
        int32_t pcm[2 * VB_OUTPUT_MAX_FRAMES];
        size_t frames = n < VB_OUTPUT_MAX_FRAMES ? n : VB_OUTPUT_MAX_FRAMES;
        size_t i;

        dst = samples_reserve(out, frames);
        if (!dst)
            return false;
        vb_output_render(fm1_output, pcm, frames);
        for (i = 0; i < frames; i++)
            dst[i] = (int32_t)((uint32_t)pcm[2 * i] << 8); /* left channel, Q23 back to 32 bits */
        samples_commit(out, frames);
        n -= frames;
    }
    return true;
}

static bool play_line(verbalizer *vb, samples *out, char *line)
{
    char *cmd, *arg;

    line[strcspn(line, "#\r\n")] = '\0';
    cmd = strtok(line, " \t");
    if (!cmd)
        return true;
    arg = strtok(NULL, " \t");
    if (!arg)
        return false;

    if (strcmp(cmd, "mode") == 0) {
        if (strcmp(arg, "words") == 0)
            vb_set_mode(vb, VB_MODE_WORDS);
        else if (strcmp(arg, "tsr") == 0)
            vb_set_mode(vb, VB_MODE_TSR);
        else if (strcmp(arg, "formants") == 0)
            vb_set_mode(vb, VB_MODE_FORMANTS);
        else
            return false;
        return true;
    }
    if (strcmp(cmd, "crackle") == 0) {
        if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0)
            return false;
        vb_set_crackle(vb, strcmp(arg, "on") == 0);
        return true;
    }
    if (strcmp(cmd, "pot") == 0) {
        long index = strtol(arg, NULL, 10);
        char *value = strtok(NULL, " \t");

        if (!value || index < 0 || index >= VB_NUM_POTS)
            return false;
        vb->pot[index] = (uint8_t)strtol(value, NULL, 10);
        return true;
    }
    if (strcmp(cmd, "pots") == 0) {
        int i;

        for (i = 0; arg; i++, arg = strtok(NULL, " \t")) {
            if (i >= VB_NUM_POTS)
                return false;
            vb->pot[i] = (uint8_t)strtol(arg, NULL, 10);
        }
        return true;
    }
    if (strcmp(cmd, "on") == 0) {
        char *velocity = strtok(NULL, " \t");

        return vb_note_on_velocity(vb, (int)strtol(arg, NULL, 10),
                                   velocity ? (int)strtol(velocity, NULL, 10) : 127);
    }
    if (strcmp(cmd, "pedal") == 0) {
        if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0)
            return false;
        vb_set_pedal(vb, strcmp(arg, "on") == 0);
        return true;
    }
    if (strcmp(cmd, "bend") == 0) {
        vb_set_pitch_bend(vb, (int)strtol(arg, NULL, 10));
        return true;
    }
    if (strcmp(cmd, "off") == 0)
        return vb_note_off(vb, (int)strtol(arg, NULL, 10));
    if (strcmp(cmd, "wait") == 0) {
        long ms = strtol(arg, NULL, 10);

        if (ms < 0 || ms > 600000)
            return false;
        return render(vb, out, (size_t)ms * VB_SAMPLE_RATE / 1000);
    }
    return false;
}

int main(int argc, char **argv)
{
    static verbalizer vb;
    static vb_output output;
    unsigned rate = VB_SAMPLE_RATE;
    samples out = { 0 };
    char line[512];
    FILE *f;
    int lineno = 0;
    int guard;

    if (argc != 3 && !(argc == 4 && strcmp(argv[3], "44100") == 0)) {
        fprintf(stderr, "usage: verbalize score.txt out.wav [44100]\n");
        return 2;
    }
    f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "%s: cannot open\n", argv[1]);
        return 1;
    }
    vb_init(&vb);
    if (argc == 4) {
        vb_output_init(&output, &vb);
        fm1_output = &output;
        rate = RS_OUT_RATE;
    }
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (!play_line(&vb, &out, line)) {
            fprintf(stderr, "%s:%d: bad command\n", argv[1], lineno);
            fclose(f);
            return 1;
        }
    }
    fclose(f);

    /* release everything, let the voices finish (10 s at most), then ring out */
    vb_all_keys_up(&vb);
    for (guard = 0; guard < 1000; guard++) {
        if (!render(&vb, &out, VB_SAMPLE_RATE / 100))
            return 1;
        if (vb_idle(&vb))
            break;
    }
    if (!render(&vb, &out, VB_SAMPLE_RATE / 4))
        return 1;

    if (!wav_write(argv[2], &out, rate)) {
        fprintf(stderr, "%s: cannot write\n", argv[2]);
        return 1;
    }
    printf("%s: %.2f s at %u Hz\n", argv[2], (double)out.count / rate, rate);
    samples_free(&out);
    return 0;
}
