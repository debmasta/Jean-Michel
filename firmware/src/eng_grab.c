/* SPDX-License-Identifier: GPL-3.0-only */
/* GRAB: the FM-1 resampling itself. The "grab" key of the FX window (CC 81) records what comes out, about two seconds: a bar when the transport
 * plays (from the next beat), else the whole buffer. 11 kHz, 8 bit: crunchy, on purpose, and all the RAM there is. RAM only: gone at power-off.
 * SLICE: the keys play its slices, from C3 up; PITCH: the whole of it, chromatic.
 * EDIT 1  MODE  DIV  START  LEN   EDIT 2  CUT  SPEED  REV  BITS */
#define GRB_N 24576u
#define GRB_RATE_Q16 16384u                               /* 11025 / 44100 */
static int8_t grb_buf[GRB_N] __attribute__((section(".pool")));
static volatile uint32_t grb_len, grb_w;
static volatile uint8_t grb_st, grb_done;                 /* grb_st: 0 idle, 1 waiting for its beat, 2 recording */
static uint32_t grb_acc, grb_c;
static inline void grb_tap(const int32_t *o, uint32_t n)
{
    uint32_t i;
    if (grb_st != 2u)
        return;
    for (i = 0; i < n; i++) {
        grb_acc += (uint32_t)(o[2u * i] + o[2u * i + 1u]);
        if (++grb_c == 4u) {
            int32_t v = clamp((int32_t)grb_acc >> 11, -127, 127);
            grb_c = 0, grb_acc = 0;
            if (grb_w < GRB_N)
                grb_buf[grb_w++] = (int8_t)v;
        }
    }
}
static void grab_note_on(track_t *t, voice_t *v)
{
    static const uint8_t DV[4] = {4, 8, 16, 32};
    const int16_t *p = t->p;
    uint32_t len = grb_len, a, b, d = DV[clamp(p[P_E1], 0, 3)], sh = (uint32_t)clamp(p[P_E2], 0, 127) * len / 128u;
    if (p[P_E0]) {                                        /* PITCH: all of it, from START */
        a = sh, b = len;
    } else {
        uint32_t idx = (uint32_t)((int32_t)v->note + 1200 - 48) % d;
        a = idx * len / d + sh;
        b = a + (len / d) * (uint32_t)clamp(p[P_E3] + 1, 1, 128) / 128u;
        if (a > len)
            a = len;
        if (b > len)
            b = len;
    }
    v->ph[0] = p[P_E6] ? (b ? b - 1u : 0u) << 16 : a << 16;
    v->ph[1] = a, v->ph[2] = b;
    v->s[6] = b <= a + 2u;
    if (!v->env && !v->env_out)
        v->s[0] = v->s[1] = 0;
}
static void grab_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t pos = v->ph[0], a = v->ph[1], b = v->ph[2], i, st, rev = p[P_E6] != 0, mask = 0xFFFFFFFFu << ((uint32_t)clamp(p[P_E7], 0, 127) * 12u / 127u);
    int32_t ic1 = v->s[0], ic2 = v->s[1], cut = clamp((clamp(p[P_E4], 0, 127) << 8) + m->cutoff, 0, 127 << 8);
    tsvf_t f;
    int filt = cut < (127 << 8);
    if (v->s[6])
        return;
    st = p[P_E0] ? (uint32_t)(((uint64_t)m->inc * 172608u) >> 28) : GRB_RATE_Q16;   /* (at C4: the natural speed) */
    st = st * (uint32_t)(clamp(p[P_E5], 0, 127) + 64) >> 7;
    if (st > (8u << 16))
        st = 8u << 16;
    if (filt)
        tsvf_coef(&f, cut, 0);
    for (i = 0; i < n; i++) {
        uint32_t k = pos >> 16, fr = pos & 0xFFFFu, fade;
        int32_t y, s0, s1;
        if (k + 1u >= b || k < a) {
            v->s[6] = 1;
            break;
        }
        s0 = grb_buf[k], s1 = grb_buf[k + 1u];
        y = ((s0 * (int32_t)(65536u - fr) + s1 * (int32_t)fr) >> 8);
        fade = k - a < b - k ? k - a : b - k;
        if (fade < 24u)
            y = y * (int32_t)fade / 24;
        y = (int32_t)((uint32_t)y & mask);
        if (filt)
            y = tsvf_lp(&f, y, &ic1, &ic2);
        out[i] += voice_amp(y, m, i);
        if (rev) {
            if (pos < st) {
                v->s[6] = 1;
                break;
            }
            pos -= st;
        } else {
            pos += st;
        }
    }
    v->ph[0] = pos;
    v->s[0] = ic1;
    v->s[1] = ic2;
}
static const char *const N_GRM[] = {"SLICE", "PITCH"}, *const N_GRD[] = {"4", "8", "16", "32"}, *const N_GRR[] = {"OFF", "ON"};
static const preset_t GRB_PRESETS[] = {
    {"GRAB SLICE", {0, 2, 0, 127, 127, 64, 0, 0}, {0, 60, 100, 14}, 0, 0, FX(0, 0, 0, 0)},
    {"GRAB PITCH", {1, 2, 0, 127, 127, 64, 0, 0}, {0, 60, 100, 20}, 0, 0, FX(0, 0, 0, 0)},
};
static const engine_t ENG_GRAB = {
    .name = "GRAB",
    .page_title = {"SLICE", "TONE"},
    .edit = {
        {"MODE", F_ENUM, 0, 1, 0, N_GRM, 0},
        {"DIV", F_ENUM, 0, 3, 2, N_GRD, 0},
        {"START", F_INT, 0, 127, 0, 0, 0},
        {"LEN", F_INT, 0, 127, 127, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 127, 0, 0},
        {"SPEED", F_INT, 0, 127, 64, 0, 0},
        {"REV", F_ENUM, 0, 1, 0, N_GRR, 0},
        {"BITS", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = GRB_PRESETS,
    .npresets = NELEM(GRB_PRESETS),
    .note_on = grab_note_on,
    .render = grab_render,
    .fil_page = 1,
    .color = 0x07FF,
    .macro = {P_E2, P_E3, P_E4, P_E5},
    .poly = 4,
};
