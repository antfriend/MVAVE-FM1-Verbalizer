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
 */
#include "voice.h"

#include <string.h>

/* Spin assigns to byte variables by truncation; keep that explicit. */
#define BYTE(v) ((uint8_t)(v))

static void go(vb_voice *v, uint32_t time)
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

static void setformants(vb_voice *v, int s1, int s2, int s3, int s4)
{
    v->p.f1 = BYTE(s1 * 100 / 1953);
    v->p.f2 = BYTE(s2 * 100 / 1953);
    v->p.f3 = BYTE(s3 * 100 / 1953);
    v->p.f4 = BYTE(s4 * 100 / 1953);
}

static void set_gp_to_pitch(vb_voice *v, int key)
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
static void ga_wrapper(vb_voice *v, int value)
{
    int pitch = ((v->p.gp - v->bend) / 4 - 12) / 2;

    if (pitch < 0)
        pitch = 0;
    v->p.ga = BYTE(value > pitch ? value - pitch : 0);
}

/* ---- phonemes ---------------------------------------------------------- */

static void bEEt_to_silence(vb_voice *v)
{
    ga_wrapper(v, 0);
    v->p.aa = 0;
    v->p.fa = 0;
}

static void vowel(vb_voice *v, int s1, int s2, int s3, int s4, uint8_t aa)
{
    setformants(v, s1, s2, s3, s4);
    ga_wrapper(v, 30);
    v->p.aa = aa;
}

/*                                 ee    i     e     a     o     oh    foot  boot  r     l     uh  */
static const uint16_t f1s[11] = {  280,  450,  550,  700,  775,  575,  425,  275,  560,  560,  700 };
static const uint16_t f2s[11] = { 2040, 2060, 1950, 1800, 1100,  900, 1000,  850, 1200,  820, 1300 };
static const uint16_t f3s[11] = { 3040, 2700, 2600, 2550, 2500, 2450, 2400, 2400, 1500, 2700, 2600 };
static const uint16_t f4s[11] = { 3600, 3570, 3400, 3400, 3500, 3500, 3500, 3500, 3050, 3600, 3100 };

static void table_vowel(vb_voice *v, int i)
{
    setformants(v, f1s[i], f2s[i], f3s[i], f4s[i]);
    ga_wrapper(v, 30);
    v->p.aa = 0;
    v->p.fa = 0;
}

static void play_the_key(vb_voice *v)
{
    setformants(v, 400, 850, 2800, 3750);
    v->p.ff = 200;
    v->p.fa = 40;
}

static void go_blah(vb_voice *v, int key)
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

static void go_sustain_ah(vb_voice *v, int key)
{
    set_gp_to_pitch(v, key);
    setformants(v, 730, 1050, 2500, 3480);
    v->p.aa = 2;
    ga_wrapper(v, 30);
    v->p.na = 20;
    v->p.nf = 60;
}

static void Select_Allophone(vb_voice *v, uint8_t pot_value, int key)
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

/* ---- public ------------------------------------------------------------ */

void vb_voice_start(vb_voice *v, const uint8_t *pot)
{
    vt_init(&v->vt);
    memset(&v->p, 0, sizeof v->p);
    v->pot = pot;
    v->velocity = 127;
    v->bend = 0;
    v->dropped = 0;
    set_gp_to_pitch(v, 1);
    setformants(v, 500, 1500, 2500, 3500);
    bEEt_to_silence(v);
    go(v, 1);
}

bool vb_voice_done(const vb_voice *v)
{
    return vt_empty(&v->vt);
}

void vb_voice_trigger(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    set_gp_to_pitch(v, key);
    v->p.vp = pot[VB_POT_TRIG_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_TRIG_VIB_RATE] / 3;
    Select_Allophone(v, pot[VB_POT_TRIG_PHONEME], key);
    ga_wrapper(v, pot[VB_POT_TRIG_VOLUME] / 3);
    go(v, pot[VB_POT_TRIG_TIME] * 2u);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

void vb_voice_sustain(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    v->p.vp = pot[VB_POT_SUS_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_SUS_VIB_RATE] / 3;
    Select_Allophone(v, pot[VB_POT_SUS_PHONEME], key);
    ga_wrapper(v, pot[VB_POT_SUS_VOLUME] / 3);
    go(v, pot[VB_POT_SUS_TIME] * 2u);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

void vb_voice_release(vb_voice *v, int key)
{
    const uint8_t *pot = v->pot;
    int i;

    v->p.vp = pot[VB_POT_REL_VIB_DEPTH] / 3;
    v->p.vr = pot[VB_POT_REL_VIB_RATE] / 3;
    Select_Allophone(v, pot[VB_POT_REL_PHONEME], key);
    ga_wrapper(v, pot[VB_POT_REL_VOLUME] / 3);
    go(v, pot[VB_POT_REL_TIME] * 2u);

    /* now quickly go to silence */
    bEEt_to_silence(v);
    go(v, 10);
    setformants(v, 310, 2000, 3100, 3700);
    for (i = 0; i < 6; i++)
        go(v, 1);
}

void vb_voice_word(vb_voice *v, int key)
{
    go_blah(v, key);
    vb_voice_null(v);
}

void vb_voice_formants(vb_voice *v, int key)
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

void vb_voice_null(vb_voice *v)
{
    /* fade while falling to key 1's pitch, which gives a word its tail */
    set_gp_to_pitch(v, 1);
    ga_wrapper(v, 0);
    v->p.aa = 0;
    v->p.fa = 0;
    v->p.na = 0;
    go(v, 30);
}
