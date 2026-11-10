/*
 * Engine tests.
 *
 *   test_engine <path to tests/golden>
 *
 * The golden tests compare the C vocal tract, sample for sample, against
 * hashes produced by running the original Propeller assembly
 * (tools/pasm_ref.py). The rest check the queue, the voice logic and the
 * instrument's key handling.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hostlib.h"
#include "verbalizer.h"
#include "vt_tables.h"

static int failures;
static int checks;

#define CHECK(cond)                                                        \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                  \
    } while (0)

/* ---- helpers ------------------------------------------------------------ */

static int32_t peak(const int32_t *s, size_t n)
{
    int64_t best = 0;
    size_t i;

    for (i = 0; i < n; i++) {
        int64_t a = s[i] < 0 ? -(int64_t)s[i] : s[i];

        if (a > best)
            best = a;
    }
    return best > INT32_MAX ? INT32_MAX : (int32_t)best;
}

/* Render `n` samples in pieces of `chunk` and append them to `out`. */
static void render(verbalizer *vb, samples *out, size_t n, size_t chunk)
{
    while (n) {
        size_t c = n < chunk ? n : chunk;
        int32_t *dst = samples_reserve(out, c);

        if (!dst) {
            printf("out of memory\n");
            exit(1);
        }
        vb_render(vb, dst, c);
        samples_commit(out, c);
        n -= c;
    }
}

/* trigger "boot", sustain "a" with vibrato, release "oh" */
static const uint8_t tsr_patch[VB_NUM_POTS] = {
    115, 20, 90, 0, 0, 87, 60, 90, 12, 150, 101, 40, 0, 60, 0, 0
};

static void load_patch(verbalizer *vb, const uint8_t *patch)
{
    memcpy(vb->pot, patch, VB_NUM_POTS);
}

static unsigned total_dropped(const verbalizer *vb)
{
    unsigned n = 0;
    int i;

    for (i = 0; i < VB_VOICES; i++)
        n += vb->voice[i].dropped;
    return n;
}

static int voices_busy(const verbalizer *vb)
{
    int n = 0, i;

    for (i = 0; i < VB_VOICES; i++)
        n += !vb_voice_done(&vb->voice[i]);
    return n;
}

static bool some_voice_has_key(const verbalizer *vb, int key)
{
    int i;

    for (i = 0; i < VB_VOICES; i++) {
        if (vb->voice_key[i] == key)
            return true;
    }
    return false;
}

/* ---- tests -------------------------------------------------------------- */

static void test_tables(void)
{
    CHECK(vt_antilog_table[0] == 0);
    CHECK(vt_antilog_table[1] == 22);
    CHECK(vt_antilog_table[2047] == 65492);
    CHECK(vt_sine_table[0] == 0);
    CHECK(vt_sine_table[1] == 50);
    CHECK(vt_sine_table[2048] == 65535);
}

static void test_golden(const char *dir)
{
    char path[1024], line[256], err[256];
    FILE *f;
    int rows = 0;

    snprintf(path, sizeof path, "%s/hashes.txt", dir);
    f = fopen(path, "r");
    CHECK(f != NULL);
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char name[128];
        unsigned long count;
        uint64_t want, got;
        samples s = { 0 };

        if (line[0] == '#' || sscanf(line, "%127s %lu %" SCNx64, name, &count, &want) != 3)
            continue;
        rows++;
        snprintf(path, sizeof path, "%s/%s.vts", dir, name);
        if (!vts_play(path, &s, err, sizeof err)) {
            printf("FAIL %s\n", err);
            failures++;
            continue;
        }
        got = samples_hash(&s);
        if (s.count != count || got != want) {
            printf("FAIL golden %s: got %zu %016" PRIx64 ", want %lu %016" PRIx64 "\n",
                   name, s.count, got, count, want);
            failures++;
        } else {
            printf("  golden %-12s %6zu samples, bit-exact, peak %d\n",
                   name, s.count, peak(s.data, s.count));
        }
        checks++;
        CHECK(peak(s.data, s.count) > 0);
        samples_free(&s);
    }
    fclose(f);
    CHECK(rows >= 4);
}

static void test_tract_queue(void)
{
    static vocal_tract vt;
    vt_params p;
    int i;

    memset(&p, 0, sizeof p);
    vt_init(&vt);
    CHECK(vt_empty(&vt));
    CHECK(!vt_full(&vt));

    /* a silent tract stays at exactly zero */
    for (i = 0; i < 1000; i++)
        CHECK(vt_tick(&vt) == 0);

    /* the queue reports "full" at the original's 8 and refuses past 32 */
    for (i = 0; i < VT_QUEUE_FRAMES; i++) {
        CHECK(vt_full(&vt) == (i >= VT_ORIGINAL_FRAMES));
        CHECK(vt_go(&vt, &p, 2));
    }
    CHECK(!vt_go(&vt, &p, 2));
    CHECK(vt_queued(&vt) == VT_QUEUE_FRAMES);
}

static void test_tract_timing(void)
{
    static vocal_tract vt;
    vt_params p;
    int i;

    /* A frame of `time` steps lasts time * 14 samples, after the one sample
     * the tract spends starting up. */
    memset(&p, 0, sizeof p);
    p.f1 = 200;
    p.gp = 100;
    vt_init(&vt);
    CHECK(vt_go(&vt, &p, 10));
    for (i = 0; i < 10 * VT_SAMPLES_PER_STEP; i++)
        vt_tick(&vt);
    CHECK(!vt_empty(&vt));
    vt_tick(&vt);
    CHECK(vt_empty(&vt));

    /* once the queue runs dry, parameters land exactly on the frame's values */
    for (i = 0; i < 2 * VT_SAMPLES_PER_STEP; i++)
        vt_tick(&vt);
    CHECK(vt.cur[5] == (uint32_t)200 << 24); /* f1 */
    CHECK(vt.cur[2] == (uint32_t)100 << 24); /* gp */

    /* gp = 100 is tuned to 110.00 Hz */
    {
        uint32_t before = vt.gphase;
        double hz;

        vt_tick(&vt);
        hz = (double)(uint32_t)(vt.gphase - before) * VT_SAMPLE_RATE / 4294967296.0;
        CHECK(hz > 109.9 && hz < 110.1);
    }

    /* time is clamped to at least 2 steps, and pace scales it */
    vt_init(&vt);
    vt_go(&vt, &p, 0);
    CHECK(vt.queue[0].step == 0x01000000u / 2);
    vt_set_pace(&vt, 50);
    vt_go(&vt, &p, 10);
    CHECK(vt.queue[1].step == 0x01000000u / 20);
    vt_set_pace(&vt, 0); /* ignored */
    CHECK(vt.pace == 50);
}

static void test_voice(void)
{
    static vb_voice v;
    uint8_t pot[VB_NUM_POTS];

    memset(pot, 0, sizeof pot);
    vb_voice_start(&v, pot);
    CHECK(v.p.gp == 52); /* key 1 */
    CHECK(v.p.ga == 0);
    CHECK(vt_queued(&v.vt) == 1);

    /* key 13 is A2, the tract's gp = 100 */
    pot[VB_POT_TRIG_PHONEME] = VB_PH_EE * 7;
    pot[VB_POT_TRIG_VOLUME] = 90;
    pot[VB_POT_TRIG_VIB_DEPTH] = 30;
    pot[VB_POT_TRIG_VIB_RATE] = 156;
    vb_voice_trigger(&v, 13);
    CHECK(v.p.gp == 100);
    CHECK(v.p.vp == 10);
    CHECK(v.p.vr == 52);
    CHECK(v.p.f1 == 23);     /* (280 + 6) / 12 */
    CHECK(v.p.f2 == 170);
    CHECK(v.p.f3 == 253);
    CHECK(v.p.f4 == 44);     /* (3600 + 6) / 12 = 300, wrapped to a byte */
    CHECK(v.p.ga == 30 - 6); /* volume 30, backed off by key / 2 */
    CHECK(vt_queued(&v.vt) == 1 + 7);

    /* the formula phonemes use 19.53 Hz units */
    pot[VB_POT_SUS_PHONEME] = VB_PH_BEET * 7;
    pot[VB_POT_SUS_VOLUME] = 9;
    vb_voice_sustain(&v, 13);
    CHECK(v.p.f1 == 15 && v.p.f2 == 102 && v.p.f3 == 158 && v.p.f4 == 189);
    CHECK(v.p.aa == 3);
    CHECK(v.p.ga == 3); /* a volume at or below key / 2 is not backed off */

    /* release ends silent */
    vb_voice_release(&v, 13);
    CHECK(v.p.ga == 0 && v.p.aa == 0);
    CHECK(vt_queued(&v.vt) == 1 + 7 + 7 + 8);
    CHECK(v.dropped == 0);

    /* keys beyond the original keyboard clamp instead of wrapping */
    vb_voice_start(&v, pot);
    vb_voice_trigger(&v, 60);
    CHECK(v.p.gp == 255);
    vb_voice_trigger(&v, -20);
    CHECK(v.p.gp == 0);
    vb_voice_null(&v);
    CHECK(v.p.ga == 0);
}

static void test_instrument_silence(void)
{
    static verbalizer vb;
    samples s = { 0 };

    vb_init(&vb);
    CHECK(vb.mode == VB_MODE_TSR);
    render(&vb, &s, 4000, 256);
    CHECK(peak(s.data, s.count) == 0);
    CHECK(vb_idle(&vb));
    CHECK(!vb_note_on(&vb, 19));
    CHECK(!vb_note_on(&vb, 84));
    CHECK(!vb_note_off(&vb, 200));
    samples_free(&s);
}

static void test_instrument_tsr(void)
{
    static verbalizer vb;
    samples s = { 0 };
    int32_t loud, quiet;

    vb_init(&vb);
    load_patch(&vb, tsr_patch);

    CHECK(vb_note_on(&vb, 45));
    render(&vb, &s, 10000, 256);
    loud = peak(s.data + 6000, 4000);
    CHECK(loud > (1 << 24)); /* sustaining */
    CHECK(!vb_idle(&vb));
    CHECK(vb.voice_key[0] == 45 - VB_MIDI_OFFSET);
    CHECK(vb.voice[0].p.gp == 100);

    CHECK(vb_note_off(&vb, 45));
    render(&vb, &s, 20000, 256);
    CHECK(vb_idle(&vb));
    quiet = peak(s.data + s.count - 2000, 2000);
    CHECK(quiet < loud / 1000);
    CHECK(total_dropped(&vb) == 0);
    samples_free(&s);
}

static void test_instrument_polyphony(void)
{
    static verbalizer vb;
    samples s = { 0 };
    int note;

    vb_init(&vb);
    load_patch(&vb, tsr_patch);

    /* five keys, four voices: the fifth waits */
    for (note = 40; note < 45; note++)
        vb_note_on(&vb, note);
    render(&vb, &s, 4000, 256);
    for (note = 40; note < 44; note++)
        CHECK(some_voice_has_key(&vb, note - VB_MIDI_OFFSET));
    CHECK(!some_voice_has_key(&vb, 44 - VB_MIDI_OFFSET));

    /* ...and takes over the first voice to come free */
    vb_note_off(&vb, 41);
    render(&vb, &s, 20000, 256);
    CHECK(some_voice_has_key(&vb, 44 - VB_MIDI_OFFSET));
    CHECK(!some_voice_has_key(&vb, 41 - VB_MIDI_OFFSET));

    vb_all_keys_up(&vb);
    render(&vb, &s, 20000, 256);
    CHECK(vb_idle(&vb));
    CHECK(total_dropped(&vb) == 0);
    samples_free(&s);
}

static void test_instrument_words(void)
{
    static verbalizer vb;
    samples s = { 0 };

    vb_init(&vb);
    vb_set_mode(&vb, VB_MODE_WORDS);
    vb.pot[VB_POT_WORD_TIME] = 60;
    vb_note_on(&vb, 45);
    render(&vb, &s, 12000, 256);
    CHECK(peak(s.data, 8000) > (1 << 24));
    /* the word ends by itself while the key is still down */
    CHECK(voices_busy(&vb) == 0);
    CHECK(!vb_idle(&vb));
    CHECK(peak(s.data + 11000, 1000) < (1 << 16));
    vb_note_off(&vb, 45);
    render(&vb, &s, 200, 256);
    CHECK(vb_idle(&vb));
    CHECK(total_dropped(&vb) == 0);
    samples_free(&s);
}

static void test_instrument_formants(void)
{
    static verbalizer vb;
    samples s = { 0 };

    vb_init(&vb);
    vb_set_mode(&vb, VB_MODE_FORMANTS);
    vb.pot[VB_POT_FM_F1] = 37;
    vb.pot[VB_POT_FM_F2] = 54;
    vb.pot[VB_POT_FM_F3] = 128;
    vb.pot[VB_POT_FM_F4] = 179;
    vb.pot[VB_POT_FM_PITCH] = 128; /* no transpose */
    vb.pot[VB_POT_FM_TIME] = 10;
    vb.pot[VB_POT_FM_GA] = 90;

    vb_note_on(&vb, 45);
    render(&vb, &s, 8000, 256);
    CHECK(peak(s.data + 4000, 4000) > (1 << 24));
    CHECK(vb.voice[0].p.gp == 100);
    CHECK(vb.voice[0].p.f1 == 37);
    /* only voice 0 plays in this mode */
    CHECK(vb_voice_done(&vb.voice[1]) && vb_voice_done(&vb.voice[2]) && vb_voice_done(&vb.voice[3]));

    /* knob changes are picked up while the key is held */
    vb.pot[VB_POT_FM_F1] = 14;
    render(&vb, &s, 2000, 256);
    CHECK(vb.voice[0].p.f1 == 14);

    vb_note_off(&vb, 45);
    render(&vb, &s, 8000, 256);
    CHECK(vb_idle(&vb));
    CHECK(peak(s.data + s.count - 2000, 2000) < (1 << 16));

    /* a release that lands just after the queue was refilled still fades */
    vb_note_on(&vb, 45);
    do {
        render(&vb, &s, 1, 1);
    } while (vt_queued(&vb.voice[0].vt) < VT_ORIGINAL_FRAMES);
    vb_note_off(&vb, 45);
    render(&vb, &s, 8000, 256);
    CHECK(vb_idle(&vb));
    CHECK(vb.voice[0].p.ga == 0);
    CHECK(peak(s.data + s.count - 2000, 2000) < (1 << 16));

    CHECK(total_dropped(&vb) == 0);
    samples_free(&s);
}

/* Play a fixed pseudo-random performance; events land every `step` samples. */
static uint64_t performance(size_t chunk, unsigned *dropped)
{
    static verbalizer vb;
    samples s = { 0 };
    uint32_t rng = 12345;
    uint64_t hash;
    int i;

    vb_init(&vb);
    load_patch(&vb, tsr_patch);
    for (i = 0; i < 400; i++) {
        int note, r;

        rng = rng * 1664525u + 1013904223u;
        r = (int)(rng >> 16);
        note = 33 + r % 37;
        if (r & 0x1000)
            vb_note_on(&vb, note);
        else
            vb_note_off(&vb, note);
        if (i == 150)
            vb_set_mode(&vb, VB_MODE_WORDS);
        if (i == 250)
            vb_set_mode(&vb, VB_MODE_FORMANTS);
        if (i == 320)
            vb_set_mode(&vb, VB_MODE_TSR);
        vb.pot[(r >> 4) & 15] = (uint8_t)(rng >> 8);
        render(&vb, &s, 500, chunk);
    }
    vb_all_keys_up(&vb);
    render(&vb, &s, 40000, chunk);
    CHECK(vb_idle(&vb));
    *dropped = total_dropped(&vb);
    hash = samples_hash(&s);
    samples_free(&s);
    return hash;
}

static void test_instrument_determinism(void)
{
    unsigned dropped;
    uint64_t a = performance(1, &dropped);

    /* knobs are random here, so word phonemes pile up frames; still no loss */
    CHECK(dropped == 0);
    /* the audio must not depend on how the caller slices its render calls */
    CHECK(performance(7, &dropped) == a);
    CHECK(performance(64, &dropped) == a);
    CHECK(performance(4096, &dropped) == a);
    printf("  performance hash %016" PRIx64 "\n", a);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_engine <path to tests/golden>\n");
        return 2;
    }
    test_tables();
    test_golden(argv[1]);
    test_tract_queue();
    test_tract_timing();
    test_voice();
    test_instrument_silence();
    test_instrument_tsr();
    test_instrument_polyphony();
    test_instrument_words();
    test_instrument_formants();
    test_instrument_determinism();

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
