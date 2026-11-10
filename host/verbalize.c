/*
 * verbalize: play a score through the whole instrument and write a WAV file.
 *
 *   verbalize score.txt out.wav
 *
 * Score format, one command per line, '#' starts a comment:
 *
 *   mode words|tsr|formants   select the play mode
 *   pot <index> <value>       set one knob (index 0..15, value 0..255)
 *   pots <v0> <v1> ...        set knobs 0, 1, ... in order
 *   on <note>                 MIDI note on  (20..83)
 *   off <note>                MIDI note off
 *   wait <ms>                 let time pass
 *
 * When the score ends, all keys are released and the tail is rendered.
 * Output is 20 kHz, 16-bit mono.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hostlib.h"
#include "verbalizer.h"

static bool render(verbalizer *vb, samples *out, size_t n)
{
    int32_t *dst = samples_reserve(out, n);

    if (!dst)
        return false;
    vb_render(vb, dst, n);
    samples_commit(out, n);
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
    if (strcmp(cmd, "on") == 0)
        return vb_note_on(vb, (int)strtol(arg, NULL, 10));
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
    samples out = { 0 };
    char line[512];
    FILE *f;
    int lineno = 0;
    int guard;

    if (argc != 3) {
        fprintf(stderr, "usage: verbalize score.txt out.wav\n");
        return 2;
    }
    f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "%s: cannot open\n", argv[1]);
        return 1;
    }
    vb_init(&vb);
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

    if (!wav_write(argv[2], &out, VB_SAMPLE_RATE)) {
        fprintf(stderr, "%s: cannot write\n", argv[2]);
        return 1;
    }
    printf("%s: %.2f s\n", argv[2], (double)out.count / VB_SAMPLE_RATE);
    samples_free(&out);
    return 0;
}
