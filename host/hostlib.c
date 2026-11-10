#include "hostlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vocal_tract.h"

int32_t *samples_reserve(samples *s, size_t extra)
{
    if (s->count + extra > s->capacity) {
        size_t cap = s->capacity ? s->capacity * 2 : 4096;
        int32_t *data;

        while (cap < s->count + extra)
            cap *= 2;
        data = (int32_t *)realloc(s->data, cap * sizeof *data);
        if (!data)
            return NULL;
        s->data = data;
        s->capacity = cap;
    }
    return s->data + s->count;
}

void samples_commit(samples *s, size_t written)
{
    s->count += written;
}

void samples_free(samples *s)
{
    free(s->data);
    memset(s, 0, sizeof *s);
}

uint64_t samples_hash(const samples *s)
{
    uint64_t h = 0xCBF29CE484222325ull;
    size_t i;
    int b;

    for (i = 0; i < s->count; i++) {
        uint32_t v = (uint32_t)s->data[i];

        for (b = 0; b < 4; b++) {
            h = (h ^ (v & 0xFF)) * 0x100000001B3ull;
            v >>= 8;
        }
    }
    return h;
}

static void put_le(FILE *f, uint32_t v, int bytes)
{
    while (bytes--) {
        fputc((int)(v & 0xFF), f);
        v >>= 8;
    }
}

bool wav_write(const char *path, const samples *s, unsigned sample_rate)
{
    FILE *f = fopen(path, "wb");
    uint32_t data_bytes = (uint32_t)(s->count * 2);
    size_t i;
    bool ok;

    if (!f)
        return false;
    fputs("RIFF", f);
    put_le(f, 36 + data_bytes, 4);
    fputs("WAVEfmt ", f);
    put_le(f, 16, 4);              /* fmt chunk size */
    put_le(f, 1, 2);               /* PCM */
    put_le(f, 1, 2);               /* mono */
    put_le(f, sample_rate, 4);
    put_le(f, sample_rate * 2, 4); /* bytes per second */
    put_le(f, 2, 2);               /* block align */
    put_le(f, 16, 2);              /* bits per sample */
    fputs("data", f);
    put_le(f, data_bytes, 4);
    for (i = 0; i < s->count; i++)
        put_le(f, (uint32_t)s->data[i] >> 16, 2);
    ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

/* ---- .vts scripts ------------------------------------------------------- */

static bool render(vocal_tract *vt, samples *out, size_t n)
{
    int32_t *dst = samples_reserve(out, n);

    if (!dst)
        return false;
    vt_render(vt, dst, n);
    samples_commit(out, n);
    return true;
}

static bool set_param(vt_params *p, const char *name, long value)
{
    static const char *const names[VT_NUM_PARAMS] = {
        "aa", "ga", "gp", "vp", "vr", "f1", "f2", "f3", "f4", "na", "nf", "fa", "ff"
    };
    int i;

    for (i = 0; i < VT_NUM_PARAMS; i++) {
        if (strcmp(name, names[i]) == 0) {
            ((uint8_t *)p)[i] = (uint8_t)value;
            return true;
        }
    }
    return false;
}

bool vts_play(const char *path, samples *out, char *err, size_t err_size)
{
    FILE *f = fopen(path, "r");
    vocal_tract vt;
    vt_params p;
    char line[512];
    int lineno = 0;
    bool ok = true;

    if (!f) {
        snprintf(err, err_size, "%s: cannot open", path);
        return false;
    }
    vt_init(&vt);
    memset(&p, 0, sizeof p);

    while (ok && fgets(line, sizeof line, f)) {
        char *cmd, *arg;

        lineno++;
        line[strcspn(line, "#\r\n")] = '\0';
        cmd = strtok(line, " \t");
        if (!cmd)
            continue;
        arg = strtok(NULL, " \t");

        if (strcmp(cmd, "set") == 0) {
            for (; ok && arg; arg = strtok(NULL, " \t")) {
                char *eq = strchr(arg, '=');

                if (!eq) {
                    ok = false;
                    break;
                }
                *eq = '\0';
                ok = set_param(&p, arg, strtol(eq + 1, NULL, 10));
            }
        } else if (!arg) {
            ok = false;
        } else if (strcmp(cmd, "go") == 0) {
            /* the original's go() blocked while all eight buffers were in use */
            while (ok && vt_full(&vt))
                ok = render(&vt, out, 1);
            if (ok)
                vt_go(&vt, &p, (uint32_t)strtoul(arg, NULL, 10));
        } else if (strcmp(cmd, "run") == 0) {
            ok = render(&vt, out, (size_t)strtoul(arg, NULL, 10));
        } else if (strcmp(cmd, "pace") == 0) {
            vt_set_pace(&vt, (unsigned)strtoul(arg, NULL, 10));
        } else if (strcmp(cmd, "atten") == 0) {
            vt_set_attenuation(&vt, (unsigned)strtoul(arg, NULL, 10));
        } else {
            ok = false;
        }
    }
    fclose(f);
    if (!ok)
        snprintf(err, err_size, "%s:%d: bad command", path, lineno);
    return ok;
}
