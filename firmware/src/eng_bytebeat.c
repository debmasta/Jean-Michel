/* SPDX-License-Identifier: GPL-3.0-only */
/* BYTE: bytebeat. A formula of t gives an 8-bit sample for every step of t (METAL KAP 2's engine: the stack program of
 * tools/bytebeat/Bytebeat.h, compiled ahead of time into bb_bank.h, so nothing is parsed here and the formulas take no RAM).
 * EDIT 1  FORM  A  B  C  D      the formula; its variables a b c d (0..254), at noon the original formula; A B its own
 *                               numbers, C an xor ramp, D an offset stepped at the tempo (every formula)
 * EDIT 2  D  CUT  MORPH  BITS   a 12 dB low-pass; MORPH glides from state A to B; BITS drops the sample's low bits
 * Variables: t; a..d; T = t locked to the tempo (1024 a 16th: T>>10 a 16th, T>>12 a beat, T>>14 a bar; the transport's
 * when it plays, else its own clock from the note's start, at the tempo); n = the note.
 * Pitch: t runs 8000 steps a second at C4, in proportion for the other notes.
 * Integers wrap, shifts are & 31, x / 0 = 0: as in the METAL KAP 2 plugin, sample for sample.
 * State: ph[0..1] t (Q32), ph[2] + s[5] the free clock's T (Q16); s[0..1] filter, s[2] DC blocker, s[3] the held sample,
 * s[6] s[7] the t and the T it was computed for */
#include "bb_bank.h"
#include "bb_vm.h"
_Static_assert(BB_MAXDEPTH < BB_STACK, "a factory formula is deeper than the VM's stack");

/* T: worked out once a block (twist.c tw_block), not per voice: a 64-bit division is dear on this CPU */
static uint64_t bb_Tnow, bb_Tstep;
/* MORPH (EDIT 2, the BYTE MEM page, CC 24, a step's lock): up to four states of A B C D CUT (BYTE MEM; "set A" "set B" in the FX window);
 * the knob glides through those stored, 1 the first .. 127 the last. At 0, or with fewer than two stored, the knobs as they stand */
static int16_t bb_snap[4][5];
static uint8_t bb_have;
static volatile uint8_t bb_force;   /* CC 25 (the "BYTE MEM" list in Live): 0 none, 1..4 play that memory as it is, if it is stored */
static inline void bb_knobs(const int16_t *p, int32_t *var, int32_t *cut)
{
    int32_t m = clamp(p[P_E6], 0, 127), i, n = 0, span, seg, fr;
    uint8_t idx[4];
    for (i = 0; i < 4; i++)
        if ((bb_have >> i) & 1u)
            idx[n++] = (uint8_t)i;
    span = (m > 1 ? m - 1 : 0) * (n > 1 ? n - 1 : 0);
    seg = span / 126;
    fr = span % 126;
    if (seg >= n - 1)
        seg = n > 1 ? n - 2 : 0, fr = 126;
    for (i = 0; i < 5; i++) {
        int32_t v = clamp(p[P_E1 + i], 0, 127);
        if (bb_force && ((bb_have >> (bb_force - 1u)) & 1u))
            v = bb_snap[bb_force - 1u][i];
        else if (m > 0 && n >= 2)
            v = (bb_snap[idx[seg]][i] * (126 - fr) + bb_snap[idx[seg + 1]][i] * fr) / 126;
        if (i < 4)
            var[i] = v * 2;
        else
            *cut = v;
    }
}

/* Under load the formulas are worked out less often (the last sample is held: more lo-fi, no crackle): audio.c sets it from how
 * full the last half was. 1 = every step */
static volatile uint8_t bb_hold = 1;
#define BB_HOLD_MAX 8u

static void bb_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[5] = 0;
    v->s[3] = 128;
    v->s[6] = v->s[7] = -1;
    if (!v->env && !v->env_out)
        v->s[0] = v->s[1] = v->s[2] = 0;
}

static void bb_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t prog = (uint32_t)clamp(p[P_E0], 0, (int32_t)BB_N - 1), i, hold = bb_hold, wait = 0;
    int needT;
    uint32_t mask = 0xFFu << (((uint32_t)clamp(p[P_E7], 0, 127) * 7u) / 127u);
    uint64_t tq = (uint64_t)v->ph[1] << 32 | v->ph[0];
    uint64_t step = ((uint64_t)m->inc * 125248u) >> 12;    /* 8000 steps a second at C4: 8000 / 261.6256 (Q12) */
    uint64_t Tq, Tstep = bb_Tstep;
    int playing = song.playing;
    int32_t var[10], r = v->s[3], lt = v->s[6], lT = v->s[7], ic1 = v->s[0], ic2 = v->s[1], cut, dc = v->s[2];
    tsvf_t f;
    int filt;
    bb_knobs(p, var, &cut);                            /* (noon, 64: 128, the original formula) */
    var[4] = var[5] = var[6] = var[7] = 0;
    var[9] = v->note;
    needT = BB_PROG[prog].uses_T || var[3] != 128;     /* a formula without T only changes with t: at 120 BPM T alone would double the work */
    if (playing)
        Tq = bb_Tnow;
    else
        Tq = (uint64_t)v->ph[2] << 16 | (uint16_t)v->s[5];
    cut = clamp((cut << 8) + m->cutoff, 0, 127 << 8);
    filt = cut < (127 << 8);
    if (filt)
        tsvf_coef(&f, cut, 0);
    for (i = 0; i < n; i++) {
        int32_t ti = (int32_t)(tq >> 32), Ti = (int32_t)(Tq >> 16), y;
        if (wait)
            wait--;
        else if (ti != lt || (needT && Ti != lT)) {
            wait = hold - 1u;
            lt = ti;
            lT = Ti;
            var[8] = Ti;
            r = (int32_t)((uint32_t)bb_eval(prog, ti, var) & mask & 255u);
        }
        tq += step;
        Tq += Tstep;
        y = (2 * r - 255) * 100;
        if (filt)
            y = tsvf_lp(&f, y, &ic1, &ic2);
        dc += ((y << 8) - dc) >> 9;
        y -= dc >> 8;
        out[i] += voice_amp(soft_knee(y, 24000), m, i);
    }
    v->ph[0] = (uint32_t)tq;
    v->ph[1] = (uint32_t)(tq >> 32);
    if (!playing) {
        v->ph[2] = (uint32_t)(Tq >> 16);
        v->s[5] = (int32_t)(Tq & 0xFFFFu);
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = dc;
    v->s[3] = r;
    v->s[6] = lt;
    v->s[7] = lT;
}

/* {FORM, A, B, C, D, CUT, RES, BITS} */
static const preset_t BB_PRESETS[] = {
#define BB_P(i, name, dec, sus) {name, {i, 64, 64, 64, 64, 127, 0, 0}, {0, dec, sus, sus ? 30 : dec > 60 ? dec : 16}, 0, 0, FX(0, 0, sus ? 15 : 4, sus ? 15 : 8)},
    BB_PRESET_LIST(BB_P)
#undef BB_P
};

static const engine_t ENG_BYTE = {
    .name = "BYTE",
    .page_title = {"FORMULA", "TONE"},
    .edit = {
        {"FORM", F_ENUM, 0, BB_N - 1, 0, N_BB, 0},
        {"A", F_INT, 0, 127, 64, 0, 0},
        {"B", F_INT, 0, 127, 64, 0, 0},
        {"C", F_INT, 0, 127, 64, 0, 0},
        {"D", F_INT, 0, 127, 64, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 127, 0, 0},
        {"MORPH", F_INT, 0, 127, 0, 0, 0},
        {"BITS", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = BB_PRESETS,
    .npresets = NELEM(BB_PRESETS),
    .note_on = bb_note_on,
    .render = bb_render,
    .fil_page = 1,
    .color = 0xFD20,
    .macro = {P_E1, P_E2, P_E3, P_E6},
    .poly = 4,
};
