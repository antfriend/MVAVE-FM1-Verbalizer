/*
 * One Verbalizer voice. A port of Voice.spin by Dan Ray.
 *
 * This follows the Spin source closely. Function names keep the original's
 * where there is one. These things differ:
 *
 *  - The original's v.go() blocked while the tract's eight frame buffers were
 *    full. Here the tract has a longer queue and go() never waits.
 *  - Keys outside the original keyboard's 1..37 are tolerated: the pitch is
 *    clamped to what gp can hold, and ga_wrapper() clamps its pitch term at
 *    zero so low keys can still be silenced. Keys 1..37 are unchanged.
 *  - Three bugs are corrected. The table vowels (ee .. uh) are converted to
 *    the tract's 19.53 Hz units like every other phoneme; the original divided
 *    by 12 and let the result wrap in a byte. The closing fade no longer looks
 *    up a phoneme with the release time knob. And the volume back-off for
 *    higher keys applies to every volume; the original skipped it for volumes
 *    at or below the back-off, so turning a volume down could make it louder.
 *  - Every phoneme starts from frication and nasal off, and silence turns
 *    frication off. In the original a voice that had played the noise
 *    phoneme hissed until the mode changed, and the "ah" and "blah" phonemes
 *    left the nasal branch on for whatever came next.
 *  - A voice starts with its formants on a neutral vowel instead of 0 Hz, so
 *    its first note does not sweep up through an overload.
 *  - Velocity and pitch bend, which the original had no notion of.
 *  - Fourteen consonants take the places of repeats of the noise phoneme,
 *    and Words mode has sixteen more words. See "sounds added" below.
 */
#include "voice.h"

/* Spin assigns to byte variables by truncation; keep that explicit. */
#define BYTE(v) ((uint8_t)(v))

VB_COLD_CODE static void go(vb_voice *v, uint32_t time)
{
    vt_params p = v->p;

    if (v->velocity < 127) {
        p.ga = BYTE(p.ga * v->velocity / 127);
        p.aa = BYTE(p.aa * v->velocity / 127);
        p.fa = BYTE(p.fa * v->velocity / 127);
    }
    if (!vt_go(&v->vt, &p, time))
        v->dropped++;
}

VB_COLD_CODE static void setformants(vb_voice *v, int s1, int s2, int s3, int s4)
{
    v->p.f1 = BYTE(s1 * 100 / 1953);
    v->p.f2 = BYTE(s2 * 100 / 1953);
    v->p.f3 = BYTE(s3 * 100 / 1953);
    v->p.f4 = BYTE(s4 * 100 / 1953);
}

VB_COLD_CODE static void set_gp_to_pitch(vb_voice *v, int key)
{
    int gp = (key + 12) * 4 + v->bend;

    /* cannot happen on the original's keys 1..37 without pitch bend */
    if (gp < 0)
        gp = 0;
    if (gp > 255)
        gp = 255;
    v->p.gp = BYTE(gp);
}

/* Set the glottal amplitude, backing it off by key / 2 for higher keys.
 * Always set gp before calling this. */
VB_COLD_CODE static void ga_wrapper(vb_voice *v, int value)
{
    int pitch = ((v->p.gp - v->bend) / 4 - 12) / 2;

    if (pitch < 0)
        pitch = 0;
    v->p.ga = BYTE(value > pitch ? value - pitch : 0);
}

/* ---- phonemes ---------------------------------------------------------- */

VB_COLD_CODE static void bEEt_to_silence(vb_voice *v)
{
    ga_wrapper(v, 0);
    v->p.aa = 0;
    v->p.fa = 0;
}

VB_COLD_CODE static void vowel(vb_voice *v, int s1, int s2, int s3, int s4, uint8_t aa)
{
    setformants(v, s1, s2, s3, s4);
    ga_wrapper(v, 30);
    v->p.aa = aa;
}

/*                                 ee    i     e     a     o     oh    foot  boot  r     l     uh  */
static const uint16_t f1s[11] VB_DATA = {  280,  450,  550,  700,  775,  575,  425,  275,  560,  560,  700 };
static const uint16_t f2s[11] VB_DATA = { 2040, 2060, 1950, 1800, 1100,  900, 1000,  850, 1200,  820, 1300 };
static const uint16_t f3s[11] VB_DATA = { 3040, 2700, 2600, 2550, 2500, 2450, 2400, 2400, 1500, 2700, 2600 };
static const uint16_t f4s[11] VB_DATA = { 3600, 3570, 3400, 3400, 3500, 3500, 3500, 3500, 3050, 3600, 3100 };

VB_COLD_CODE static void table_vowel(vb_voice *v, int i)
{
    setformants(v, f1s[i], f2s[i], f3s[i], f4s[i]);
    ga_wrapper(v, 30);
    v->p.aa = 0;
    v->p.fa = 0;
}

VB_COLD_CODE static void play_the_key(vb_voice *v)
{
    setformants(v, 400, 850, 2800, 3750);
    v->p.ff = 200;
    v->p.fa = 40;
}

VB_COLD_CODE static void go_blah(vb_voice *v, int key)
{
    unsigned t = v->pot[VB_POT_WORD_TIME];

    setformants(v, 400, 850, 2800, 3750);
    go(v, 1);
    set_gp_to_pitch(v, key);
    setformants(v, 100, 200, 2800, 3750);
    go(v, 50);

    setformants(v, 400, 850, 2800, 3750);
    v->p.aa = 10;
    ga_wrapper(v, 20);
    go(v, 40);

    go(v, t);

    setformants(v, 730, 1050, 2500, 3480);
    v->p.aa = 20;
    ga_wrapper(v, (int)(t / 3));
    v->p.na = 100;
    v->p.nf = 200;
    go(v, 4 * t);
}

VB_COLD_CODE static void go_sustain_ah(vb_voice *v, int key)
{
    set_gp_to_pitch(v, key);
    setformants(v, 730, 1050, 2500, 3480);
    v->p.aa = 2;
    ga_wrapper(v, 30);
    v->p.na = 20;
    v->p.nf = 60;
}

VB_COLD_CODE static void Select_Allophone(vb_voice *v, uint8_t pot_value, int key)
{
    set_gp_to_pitch(v, key);

    /* each phoneme says what it needs; nothing carries over from the last */
    v->p.fa = 0;
    v->p.na = 0;

    switch (pot_value / 7) {
    case 0:
    case 1:
    case 2:  bEEt_to_silence(v); break;
    case VB_PH_SOAP:   vowel(v, 530,  950, 2400, 3200, 0); break;
    case VB_PH_BEET:   vowel(v, 310, 2000, 3100, 3700, 3); break; /* breathy */
    case VB_PH_HAT:    vowel(v, 730, 1700, 2500, 3400, 0); break;
    case VB_PH_HOT:    vowel(v, 750, 1050, 2400, 3200, 0); break;
    case VB_PH_BORROW: vowel(v, 580, 1200, 1500, 4700, 0); break;
    case VB_PH_BALL:   vowel(v, 560,  850, 2600, 3600, 0); break;
    case VB_PH_EE:
    case VB_PH_I:
    case VB_PH_E:
    case VB_PH_A:
    case VB_PH_O:
    case VB_PH_OH:
    case VB_PH_FOOT:
    case VB_PH_BOOT:
    case VB_PH_R:
    case VB_PH_L:
    case VB_PH_UH:     table_vowel(v, pot_value / 7 - VB_PH_EE); break;
    case VB_PH_BLAH:       go_blah(v, key); break;
    case VB_PH_SUSTAIN_AH: go_sustain_ah(v, key); break;
    default:           play_the_key(v); break;
    }
}

/* ---- sounds added to the original's -------------------------------------
 *
 * The original has vowels, R, L and noise. These add the consonants the
 * tract can make with its nasal branch, frication and aspiration, and the
 * words built from them. Left out with VB_NO_EXTRAS.
 */
#ifndef VB_NO_EXTRAS

#define HZ(x)  ((uint8_t)((x) * 100 / 1953)) /* formants and the nasal */
#define HZ2(x) ((uint8_t)((x) * 100 / 3906)) /* frication */

/* A target for the tract. `voice`, `aa` and `fa` are for a volume of 30 and
 * scale with the volume. f1 = 0 leaves the formants where they are. */
typedef struct {
    uint8_t f1, f2, f3, f4;
    uint8_t voice, aa, fa, ff, na, nf;
} vb_sound;

enum {
    /* the fourteen phonemes, in knob order */
    S_M, S_N, S_NG, S_W, S_Y, S_H, S_F, S_S, S_SH, S_V, S_Z, S_B, S_D, S_G,
    /* only used by words */
    S_QUIET, S_T, S_EE, S_IH, S_EH, S_AE, S_AH, S_OH, S_OO, S_R, S_L, S_UH
};

static const vb_sound sounds[] VB_DATA = {
    /*             f1       f2        f3        f4       voice aa  fa  ff        na   nf */
    [S_M]     = { HZ(250), HZ(1100), HZ(2200), HZ(3300),  36,  0,  0, 0,         255, HZ(1000) },
    [S_N]     = { HZ(250), HZ(1500), HZ(2600), HZ(3400),  36,  0,  0, 0,         255, HZ(1500) },
    [S_NG]    = { HZ(250), HZ(2100), HZ(2700), HZ(3400),  28,  0,  0, 0,         255, HZ(3000) },
    [S_W]     = { HZ(290), HZ(610), HZ(2150), HZ(3300),  11,  0,  0, 0,           0, 0 },
    [S_Y]     = { HZ(260), HZ(2200), HZ(3100), HZ(3700),  40,  0,  0, 0,           0, 0 },
    [S_H]     = { HZ(500), HZ(1500), HZ(2500), HZ(3500),   0, 30,  0, 0,           0, 0 },
    [S_F]     = { HZ(400), HZ(1100), HZ(2400), HZ(3500),   0,  4,  7, HZ2(7500),   0, 0 },
    [S_S]     = { HZ(300), HZ(1700), HZ(2600), HZ(3500),   0,  0, 13, HZ2(6200),   0, 0 },
    [S_SH]    = { HZ(300), HZ(2000), HZ(2500), HZ(3400),   0,  0, 15, HZ2(2700),   0, 0 },
    [S_V]     = { HZ(300), HZ(1100), HZ(2400), HZ(3500),  12,  3,  5, HZ2(7500),   0, 0 },
    [S_Z]     = { HZ(300), HZ(1700), HZ(2600), HZ(3500),  17,  0,  9, HZ2(6200),   0, 0 },
    [S_B]     = { HZ(200), HZ(800), HZ(2200), HZ(3300),   8,  0,  0, 0,           0, 0 },
    [S_D]     = { HZ(200), HZ(1700), HZ(2700), HZ(3500),  13,  0,  0, 0,           0, 0 },
    [S_G]     = { HZ(200), HZ(2100), HZ(2400), HZ(3400),  12,  0,  0, 0,           0, 0 },
    [S_QUIET] = { 0, 0, 0, 0,                            0,  0,  0, 0,           0, 0 },
    [S_T]     = { 0, 0, 0, 0,                            0,  0, 20, HZ2(4500),   0, 0 },
    [S_EE]    = { HZ(280), HZ(2040), HZ(3040), HZ(3600),  43,  0,  0, 0,           0, 0 },
    [S_IH]    = { HZ(450), HZ(2060), HZ(2700), HZ(3570),  42,  0,  0, 0,           0, 0 },
    [S_EH]    = { HZ(550), HZ(1950), HZ(2600), HZ(3400),  39,  0,  0, 0,           0, 0 },
    [S_AE]    = { HZ(700), HZ(1800), HZ(2550), HZ(3400),  38,  0,  0, 0,           0, 0 },
    [S_AH]    = { HZ(750), HZ(1100), HZ(2500), HZ(3500),  25,  0,  0, 0,           0, 0 },
    [S_OH]    = { HZ(575), HZ(900), HZ(2450), HZ(3500),  22,  0,  0, 0,           0, 0 },
    [S_OO]    = { HZ(275), HZ(850), HZ(2400), HZ(3500),  23,  0,  0, 0,           0, 0 },
    [S_R]     = { HZ(450), HZ(1200), HZ(1500), HZ(3050),  11,  0,  0, 0,           0, 0 },
    [S_L]     = { HZ(380), HZ(950), HZ(2700), HZ(3600),  19,  0,  0, 0,           0, 0 },
    [S_UH]    = { HZ(650), HZ(1300), HZ(2600), HZ(3100),  30,  0,  0, 0,           0, 0 }
};

VB_COLD_CODE static uint8_t scaled(unsigned amount, int level)
{
    amount = amount * (unsigned)level / 30u;
    return BYTE(amount > 255u ? 255u : amount);
}

/* Stage a sound at `level` (30 is the sound as written). Always set gp
 * before calling this. */
VB_COLD_CODE static void set_sound(vb_voice *v, unsigned id, int level)
{
    const vb_sound *s = &sounds[id];

    /* The hiss and the nasal notch start at their own frequency instead of
     * sweeping to it: move the frequency first, while they are still off. */
    if ((s->fa && !v->p.fa) || (s->na && !v->p.na)) {
        if (s->fa)
            v->p.ff = s->ff;
        if (s->na)
            v->p.nf = s->nf;
        go(v, 1);
    }
    if (s->f1) {
        v->p.f1 = s->f1;
        v->p.f2 = s->f2;
        v->p.f3 = s->f3;
        v->p.f4 = s->f4;
    }
    ga_wrapper(v, s->voice * level / 30);
    v->p.aa = scaled(s->aa, level);
    v->p.fa = scaled(s->fa, level);
    v->p.na = s->na;
}

/*
 * The words. Each step is a sound, a time and a pitch: the tract glides to
 * the sound over the time, at the key's pitch plus so many semitones. A time
 * with LEN is in units that the LENGTH knob stretches (3.4 ms each at the
 * knob's usual 40, never less than 0.6 ms, so a glide is never a jump); a plain time is in the tract's 700 us steps and stays as
 * it is, which keeps consonants crisp however long the word is drawn out.
 */
#define LEN 0x80
#define END 0xff

static const uint8_t words[] VB_DATA = {
    /* yeah   */ S_Y, 25, 0,  S_AE, LEN | 50, 0,  S_AE, LEN | 40, 0,  S_UH, LEN | 40, (uint8_t)-2,  END,
    /* wow    */ S_W, 30, 0,  S_AH, LEN | 50, 0,  S_AH, LEN | 30, 0,  S_OO, LEN | 60, (uint8_t)-2,  END,
    /* why    */ S_W, 30, 0,  S_AH, LEN | 50, 0,  S_AH, LEN | 40, 0,  S_IH, LEN | 60, (uint8_t)-1,  END,
    /* you    */ S_Y, 30, 0,  S_OO, LEN | 50, 0,  S_OO, LEN | 70, (uint8_t)-2,  END,
    /* oh     */ S_OH, 20, 0,  S_OH, LEN | 60, 0,  S_OO, LEN | 50, (uint8_t)-3,  END,
    /* la     */ S_L, 35, 0,  S_AH, LEN | 35, 0,  S_AH, LEN | 90, 0,  END,
    /* ah-oo  */ S_AH, 20, 0,  S_AH, LEN | 60, 0,  S_QUIET, 25, 0,  S_OO, 20, 5,  S_OO, LEN | 90, 5,  END,
    /* really */ S_R, 35, 0,  S_EE, LEN | 40, 0,  S_EE, LEN | 30, 0,  S_L, LEN | 30, 0,
                 S_EE, LEN | 30, 2,  S_EE, LEN | 50, 3,  END,
    /* mama   */ S_M, 40, 0,  S_M, 40, 0,  S_AH, LEN | 25, 0,  S_AH, LEN | 40, 0,
                 S_M, 45, 0,  S_AH, LEN | 25, 0,  S_AH, LEN | 60, (uint8_t)-2,  END,
    /* no     */ S_N, 45, 0,  S_N, 30, 0,  S_OH, LEN | 30, 0,  S_OH, LEN | 50, 0,  S_OO, LEN | 40, (uint8_t)-3,  END,
    /* now    */ S_N, 45, 0,  S_N, 30, 0,  S_AH, LEN | 35, 0,  S_AH, LEN | 40, 0,  S_OO, LEN | 50, (uint8_t)-2,  END,
    /* one    */ S_W, 30, 0,  S_UH, LEN | 40, 0,  S_UH, LEN | 30, 0,  S_N, 40, 0,  S_N, 60, (uint8_t)-2,  END,
    /* amen   */ S_AH, 20, 0,  S_AH, LEN | 50, 0,  S_M, 40, 0,  S_M, 35, 0,  S_EH, LEN | 25, 0,
                 S_EH, LEN | 30, 0,  S_N, 40, (uint8_t)-3,  S_N, 60, (uint8_t)-3,  END,
    /* yes    */ S_Y, 25, 0,  S_EH, LEN | 40, 0,  S_EH, LEN | 40, 0,  S_S, 25, 0,  S_S, 110, 0,  END,
    /* hello  */ S_H, 30, 0,  S_H, 40, 0,  S_EH, LEN | 20, 0,  S_EH, LEN | 20, 0,  S_L, LEN | 25, 0,
                 S_L, 30, 0,  S_OH, LEN | 30, 2,  S_OH, LEN | 50, 0,  S_OO, LEN | 40, (uint8_t)-3,  END,
    /* robot  */ S_R, 35, 0,  S_OH, LEN | 30, 0,  S_OH, LEN | 30, 0,  S_B, 30, 0,  S_B, 25, 0,
                 S_AH, LEN | 20, (uint8_t)-2,  S_AH, LEN | 40, (uint8_t)-2,  S_QUIET, 35, (uint8_t)-2,
                 S_T, 8, (uint8_t)-2,  S_QUIET, 20, (uint8_t)-2,  END
};

/* `word` is 1..VB_WORDS - 1. */
VB_COLD_CODE static void go_word(vb_voice *v, unsigned word, int key)
{
    const uint8_t *step = words;
    unsigned t = v->pot[VB_POT_WORD_TIME];

    /* a pitch byte can equal END, so walk whole steps */
    while (--word) {
        while (step[0] != END)
            step += 3;
        step++;
    }
    for (; step[0] != END; step += 3) {
        unsigned time = step[1] & LEN ? (step[1] & 0x7fu) * (t + 8u) / 10u : step[1];

        set_gp_to_pitch(v, key + (int8_t)step[2]);
        set_sound(v, step[0], 30);
        go(v, time);
    }
}

#endif /* VB_NO_EXTRAS */

/* A phase of a keystroke: the phoneme on its knob at the volume on its knob. */
VB_COLD_CODE static void phoneme(vb_voice *v, uint8_t pot_value, uint8_t volume, int key)
{
#ifndef VB_NO_EXTRAS
    unsigned number = pot_value / 7u;

    if ((number >= VB_PH_M && number <= VB_PH_D) || number == VB_PH_G) {
        set_gp_to_pitch(v, key);
        set_sound(v, number == VB_PH_G ? S_G : number - VB_PH_M, volume / 3);
        return;
    }
#endif
    Select_Allophone(v, pot_value, key);
    ga_wrapper(v, volume / 3);
}

/* ---- public ------------------------------------------------------------ */

VB_COLD_CODE void vb_voice_start(vb_voice *v, const uint8_t *pot)
{
    vt_init(&v->vt);
    vb_fill(&v->p, 0, sizeof v->p);
    v->pot = pot;
    v->velocity = 127;
    v->bend = 0;
    v->dropped = 0;
    set_gp_to_pitch(v, 1);
    setformants(v, 500, 1500, 2500, 3500);
    bEEt_to_silence(v);
    go(v, 1);
}

VB_CODE bool vb_voice_done(const vb_voice *v)
{
    return vt_empty(&v->vt);
}

VB_COLD_CODE void vb_voice_trigger(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    set_gp_to_pitch(v, key);
    v->p.vp = pot[VB_POT_TRIG_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_TRIG_VIB_RATE] / 3;
    phoneme(v, pot[VB_POT_TRIG_PHONEME], pot[VB_POT_TRIG_VOLUME], key);
    go(v, pot[VB_POT_TRIG_TIME] * 2u);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

VB_COLD_CODE void vb_voice_sustain(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    v->p.vp = pot[VB_POT_SUS_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_SUS_VIB_RATE] / 3;
    phoneme(v, pot[VB_POT_SUS_PHONEME], pot[VB_POT_SUS_VOLUME], key);
    go(v, pot[VB_POT_SUS_TIME] * 2u);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

VB_COLD_CODE void vb_voice_release(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    v->p.vp = pot[VB_POT_REL_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_REL_VIB_RATE] / 3;
    phoneme(v, pot[VB_POT_REL_PHONEME], pot[VB_POT_REL_VOLUME], key);
    go(v, pot[VB_POT_REL_TIME] * 2u);

    /* now quickly go to silence */
    bEEt_to_silence(v);
    go(v, 10);
    setformants(v, 310, 2000, 3100, 3700);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

VB_COLD_CODE void vb_voice_word(vb_voice *v, int key)
{
#ifndef VB_NO_EXTRAS
    unsigned word = v->pot[VB_POT_WORD] / 15u;

    if (word) {
        go_word(v, word < VB_WORDS ? word : VB_WORDS - 1u, key);
        vb_voice_null(v);
        return;
    }
#endif
    go_blah(v, key);
    vb_voice_null(v);
}

VB_COLD_CODE void vb_voice_formants(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    v->p.f1 = pot[VB_POT_FM_F1];
    v->p.f2 = pot[VB_POT_FM_F2];
    v->p.f3 = pot[VB_POT_FM_F3];
    v->p.f4 = pot[VB_POT_FM_F4];
    v->p.aa = pot[VB_POT_FM_AA];

    set_gp_to_pitch(v, key + pot[VB_POT_FM_PITCH] / 21 - 6);
    ga_wrapper(v, pot[VB_POT_FM_GA] / 3);

    v->p.vp = pot[VB_POT_FM_VP] / 3;
    v->p.vr = pot[VB_POT_FM_VR] / 3;
    v->p.na = pot[VB_POT_FM_NA];
    v->p.nf = pot[VB_POT_FM_NF];
    v->p.fa = pot[VB_POT_FM_FA];
    v->p.ff = pot[VB_POT_FM_FF];

    go(v, pot[VB_POT_FM_TIME] * 2u);
    for (i = 0; i < 7; i++)
        go(v, 1);
}

VB_COLD_CODE void vb_voice_null(vb_voice *v)
{
    /* fade while falling to key 1's pitch, which gives a word its tail */
    set_gp_to_pitch(v, 1);
    ga_wrapper(v, 0);
    v->p.aa = 0;
    v->p.fa = 0;
    v->p.na = 0;
    go(v, 30);
}
