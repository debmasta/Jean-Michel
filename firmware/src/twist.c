/* SPDX-License-Identifier: GPL-3.0-only */
/* TWISTS (from METAL KAP 2): FX held, the 16 white keys latch the twists on and off and pick the one KNOBS 1..3 edit; let go
 * and every note played (keys, sequencer, arp, MIDI) goes through them. They act on notes, not on the audio: nothing here
 * takes RAM from the synth. Key 15 lets everything go, key 16 is the cats (ui_draw.c).
 * Runs in the audio ISR (tw_on, tw_off, tw_block) except the parts that change a parameter (tw_apply, the UI). */
#if CLIPPER_SYNTH
enum { TW_STUT, TW_ROLL, TW_ARP, TW_OCT, TW_GLIDE, TW_SDL, TW_SCALE, TW_STAIR, TW_PDN, TW_PUP, TW_FDN, TW_FUP, TW_THROW, TW_SPEED,
       TW_ALLOFF, TW_CATS, TW_N };
#define TW_NSC 19u
static const int8_t TW_SC[TW_NSC][13] = {
    {0, 1, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 7, 8, -1}, {0, 2, 4, 6, 8, 10, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 3, 5, 7, 10, -1},
    {0, 2, 4, 5, 7, 9, 11, -1}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1}, {0, 2, 3, 5, 7, 8, 11, -1}, {0, 2, 3, 5, 7, 9, 11, -1},
    {0, 2, 3, 5, 7, 9, 10, -1}, {0, 2, 4, 6, 7, 9, 11, -1}, {0, 2, 4, 5, 7, 9, 10, -1}, {0, 1, 3, 5, 6, 8, 10, -1}, {0, 1, 4, 5, 7, 8, 10, -1},
    {0, 2, 3, 6, 7, 8, 11, -1}, {0, 3, 5, 6, 7, 10, -1}, {0, 2, 4, 7, 9, -1}, {0, 1, 5, 7, 10, -1}, {0, 1, 4, 5, 7, 8, 11, -1}};
static const char *const TW_SCN[TW_NSC] = {"PHRYG", "HIRAJ", "WHOLE", "MINOR", "MPENT", "MAJOR", "CHROM", "HMIN", "MMIN", "DORI", "LYDI",
                                           "MIXO", "LOCR", "PDOM", "HUNG", "BLUES", "PENT", "INSEN", "DHARM"};
#define TW_NP 3u
typedef struct { const char *lab; uint8_t n, def; const char *const *names; } tw_par_t;
static const char *const TN_STUT[] = {"1/4", "1/8.", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64", "1/128"};
static const char *const TN_ROLL[] = {"TICK", "1/32T", "1/32", "1/16T", "1/16", "ACCEL"};
static const char *const TN_CHORD[] = {"0-7-12", "MIN", "MAJ", "0-12-24", "0-5-10", "0-7-19", "MAJ7", "M7", "7E", "SUS4"};
static const char *const TN_ARATE[] = {"TICK", "1/32", "1/16", "1/8"};
static const char *const TN_DIR[] = {"UP", "DOWN", "U-D", "RND"};
static const char *const TN_OCT[] = {"RND", "+1", "-1", "+2", "+1/0", "-1/+1"};
static const char *const TN_GLIDE[] = {"SHORT", "MID", "LONG", "XLONG"};
static const char *const TN_SDC[] = {"MAJ", "MIN", "5TH", "OCT", "SUS4", "FALL"};
static const char *const TN_SDR[] = {"1/16", "1/8", "1/8.", "1/32", "1/16T"};
static const char *const TN_SDN[] = {"3", "5", "8"};
static const char *const TN_STR[] = {"1/32", "1/16", "1/8", "1/4"};
static const char *const TN_SKP[] = {"1", "2", "3"};
static const char *const TN_PIT[] = {"2", "5", "12", "24"};
static const char *const TN_FLT[] = {"SOFT", "MID", "FULL"};
static const char *const TN_THR[] = {"HALF", "FULL"};
static const char *const TN_SPD[] = {"/4", "/2", "x2", "x4", "x8"};
#define NN(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
static const tw_par_t TW_PAR[TW_N][TW_NP] = {
    [TW_STUT] = {{"rate", NN(TN_STUT), 4, TN_STUT}},
    [TW_ROLL] = {{"rate", NN(TN_ROLL), 1, TN_ROLL}},
    [TW_ARP] = {{"chord", NN(TN_CHORD), 0, TN_CHORD}, {"rate", NN(TN_ARATE), 2, TN_ARATE}, {"dir", NN(TN_DIR), 0, TN_DIR}},
    [TW_OCT] = {{"mode", NN(TN_OCT), 1, TN_OCT}},
    [TW_GLIDE] = {{"time", NN(TN_GLIDE), 1, TN_GLIDE}},
    [TW_SDL] = {{"chord", NN(TN_SDC), 1, TN_SDC}, {"rate", NN(TN_SDR), 0, TN_SDR}, {"echoes", NN(TN_SDN), 1, TN_SDN}},
    [TW_SCALE] = {{"scale", TW_NSC, 0, TW_SCN}},
    [TW_STAIR] = {{"rate", NN(TN_STR), 1, TN_STR}, {"skip", NN(TN_SKP), 0, TN_SKP}, {"scale", TW_NSC, 3, TW_SCN}},
    [TW_PDN] = {{"amount", NN(TN_PIT), 2, TN_PIT}},
    [TW_PUP] = {{"amount", NN(TN_PIT), 2, TN_PIT}},
    [TW_FDN] = {{"amount", NN(TN_FLT), 2, TN_FLT}},
    [TW_FUP] = {{"amount", NN(TN_FLT), 1, TN_FLT}},
    [TW_THROW] = {{"amount", NN(TN_THR), 1, TN_THR}},
    [TW_SPEED] = {{"mult", NN(TN_SPD), 1, TN_SPD}},
};
static const char *const TW_LAB[TW_N] = {"stut", "roll", "arp", "oct", "glide", "strdly", "scale", "stair", "pitch-", "pitch+",
                                         "filt-", "filt+", "throw", "speed", "ALL OFF", "cats"};
static volatile uint32_t tw_mask;                /* the twists on */
static uint8_t tw_sel;                           /* the one the knobs edit */
static uint8_t tw_p[TW_N][TW_NP];
static uint8_t tw_ready;
/* the cats: ON, and their four knobs */
static volatile uint8_t cat_w;                   /* notes for the UI's cats (written here, read by ui_draw.c) */
static volatile uint8_t cat_q[8];
static uint8_t cat_size = 2, cat_form, cat_life = 3, cat_pal = 4;   /* (cat_pal 4: a palette per cat) */

static volatile int32_t tw_bend;                 /* 1/4096 semitone, read by voice.c */
#define TW_NH 8u
static struct { uint8_t in, out, cur, on, vel; uint8_t ai; } tw_h[TW_NH];
#define TW_NE 12u
static struct { uint8_t kind, note, vel; int32_t left; } tw_ev[TW_NE];   /* kind 0 none, 1 note on, 2 note off */
static uint32_t tw_acc, tw_st, tw_ticks, tw_oalt;
static int32_t tw_deg;
static uint32_t tw_rand = 0x9E3779B9u;
static uint32_t tw_rnd(uint32_t n) { tw_rand ^= tw_rand << 13; tw_rand ^= tw_rand >> 17; tw_rand ^= tw_rand << 5; return (tw_rand >> 8) % n; }
static inline int tw_is(uint32_t k) { return (tw_mask >> k) & 1u; }
static inline uint32_t tw_v(uint32_t k, uint32_t p) { return tw_p[k][p]; }

static void tw_init(void)
{
    uint32_t k, p;
    if (tw_ready)
        return;
    tw_ready = 1;
    for (k = 0; k < TW_N; k++)
        for (p = 0; p < TW_NP; p++)
            tw_p[k][p] = TW_PAR[k][p].def;
}
static int32_t tw_snap(int32_t m, uint32_t sc, int32_t root)
{
    const int8_t *d = TW_SC[sc % TW_NSC];
    int32_t rel = ((m - root) % 12 + 12) % 12, best = 0, bd = 99;
    uint32_t i;
    for (i = 0; d[i] >= 0; i++) {
        int32_t dd = d[i] > rel ? d[i] - rel : rel - d[i];
        if (dd < bd)
            bd = dd, best = d[i];
    }
    if (12 - rel < bd)
        best = 12;
    return m - rel + best;
}
static int32_t tw_degsemi(int32_t deg, uint32_t sc)
{
    const int8_t *d = TW_SC[sc % TW_NSC];
    int32_t n = 0, o, i;
    while (d[n] >= 0)
        n++;
    o = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
    i = deg - o * n;
    return d[i] + o * 12;
}
static void tw_event(uint32_t kind, uint32_t note, uint32_t vel, int32_t left)
{
    uint32_t i;
    for (i = 0; i < TW_NE; i++)
        if (!tw_ev[i].kind) {
            tw_ev[i].kind = (uint8_t)kind, tw_ev[i].note = (uint8_t)note, tw_ev[i].vel = (uint8_t)vel, tw_ev[i].left = left;
            return;
        }
}
static int32_t tw_units(uint32_t num, uint32_t den) { return (int32_t)(BEAT_U / den * num); }

static void tw_on(uint32_t note, uint32_t vel)
{
    track_t *t = &trk[0];
    int32_t m = (int32_t)note;
    uint32_t i;
    cat_q[cat_w & 7u] = (uint8_t)note;
    RING_PUBLISH();
    cat_w++;
    if (tw_is(TW_OCT)) {
        static const int8_t J[5] = {0, 12, -12, 24, 12};
        uint32_t md = tw_v(TW_OCT, 0), a = tw_oalt++;
        m += md == 1u ? 12 : md == 2u ? -12 : md == 3u ? 24 : md == 4u ? ((a & 1u) ? 12 : 0) : md == 5u ? ((a & 1u) ? 12 : -12) : J[tw_rnd(5)];
    }
    if (tw_is(TW_SCALE))
        m = tw_snap(m, tw_v(TW_SCALE, 0), t->p[P_ROOT]);
    m = clamp(m, 0, 127);
    for (i = 0; i < TW_NH && tw_h[i].on; i++)
        ;
    if (i < TW_NH) {
        tw_h[i].in = (uint8_t)note, tw_h[i].out = tw_h[i].cur = (uint8_t)m, tw_h[i].on = 1, tw_h[i].vel = (uint8_t)vel, tw_h[i].ai = 0;
    }
    trk_note_on0(t, (uint32_t)m, vel);
    if (tw_is(TW_SDL)) {                         /* each note echoes up a chord, quieter each time */
        static const int8_t CH[6][3] = {{0, 4, 7}, {0, 3, 7}, {0, 7, 0}, {0, 0, 0}, {0, 5, 7}, {0, -5, 0}};
        static const uint8_t CN[6] = {3, 3, 2, 1, 3, 2}, NE[3] = {3, 5, 8};
        static const uint8_t IVN[5] = {1, 2, 3, 1, 1}, IVD[5] = {4, 4, 4, 8, 6};
        uint32_t c = tw_v(TW_SDL, 0), r = tw_v(TW_SDL, 1), n = NE[tw_v(TW_SDL, 2)], e, gv = vel;
        int32_t iv = tw_units(IVN[r], IVD[r]);
        for (e = 1; e <= n; e++) {
            int32_t semi = CH[c][e % CN[c]] + (c == 5u ? -12 : 12) * (int32_t)((e / CN[c]) % 3u), o = clamp(m + semi, 0, 127);
            gv = gv * 74u / 100u;
            tw_event(1, (uint32_t)o, gv ? gv : 1u, iv * (int32_t)e);
            tw_event(2, (uint32_t)o, 0, iv * (int32_t)e + iv * 85 / 100);
        }
    }
}
static void tw_off(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < TW_NH; i++)
        if (tw_h[i].on && tw_h[i].in == note) {
            tw_h[i].on = 0;
            trk_note_off0(&trk[0], tw_h[i].cur);
            return;
        }
    trk_note_off0(&trk[0], note);
}
static void tw_clear(void)
{
    uint32_t i;
    for (i = 0; i < TW_NH; i++)
        tw_h[i].on = 0;
    for (i = 0; i < TW_NE; i++)
        tw_ev[i].kind = 0;
}
/* every held note again: at its own pitch (STUT ROLL), or the next note of the chord (ARP) */
static void tw_retrig(void)
{
    uint32_t i;
    track_t *t = &trk[0];
    for (i = 0; i < TW_NH; i++) {
        uint32_t nt;
        if (!tw_h[i].on)
            continue;
        nt = tw_h[i].out;
        if (tw_is(TW_ARP)) {
            static const int8_t A[10][4] = {{7, 12, -99, 0}, {3, 7, -99, 0}, {4, 7, -99, 0}, {12, 24, -99, 0}, {5, 10, -99, 0}, {7, 19, -99, 0},
                                            {4, 7, 11, 0}, {3, 7, 10, 0}, {4, 7, 10, 0}, {5, 7, 12, 0}};
            const int8_t *c = A[tw_v(TW_ARP, 0)];
            int32_t N[4] = {0, c[0], c[1], c[2]}, cnt = c[2] > -99 ? 4 : 3, q = tw_h[i].ai % cnt, dir = (int32_t)tw_v(TW_ARP, 2);
            if (dir == 1)
                q = cnt - 1 - q;
            else if (dir == 2) {
                int32_t P = 2 * cnt - 2, u = tw_h[i].ai % P;
                q = u < cnt ? u : P - u;
            } else if (dir == 3)
                q = (int32_t)tw_rnd((uint32_t)cnt);
            tw_h[i].ai++;
            nt = (uint32_t)clamp((int32_t)tw_h[i].out + N[q], 0, 127);
        }
        trk_note_off0(t, tw_h[i].cur);
        tw_h[i].cur = (uint8_t)nt;
        trk_note_on0(t, nt, tw_h[i].vel);
    }
}
/* no ARP or roll any more: the held notes back on their own pitch */
static void tw_home(void)
{
    uint32_t i;
    for (i = 0; i < TW_NH; i++)
        if (tw_h[i].on && tw_h[i].cur != tw_h[i].out) {
            trk_note_off0(&trk[0], tw_h[i].cur);
            tw_h[i].cur = tw_h[i].out;
            trk_note_on0(&trk[0], tw_h[i].out, tw_h[i].vel);
        }
}
/* once per control block, adv = its length in clock units */
static void tw_block(uint32_t adv)
{
    static uint8_t was_arp;
    uint32_t i;
    int32_t semis = 0;
    if (!tw_ready)
        return;
    for (i = 0; i < TW_NE; i++)
        if (tw_ev[i].kind && (tw_ev[i].left -= (int32_t)adv) <= 0) {
            if (tw_ev[i].kind == 1u)
                trk_note_on0(&trk[0], tw_ev[i].note, tw_ev[i].vel);
            else
                trk_note_off0(&trk[0], tw_ev[i].note);
            tw_ev[i].kind = 0;
        }
    {
        int32_t iv = 0;
        if (tw_is(TW_STUT)) {
            static const uint8_t N[9] = {1, 3, 1, 1, 1, 1, 1, 1, 1}, D[9] = {1, 4, 2, 3, 4, 6, 8, 16, 32};
            iv = tw_units(N[tw_v(TW_STUT, 0)], D[tw_v(TW_STUT, 0)]);
        } else if (tw_is(TW_ROLL)) {
            static const uint8_t T[6] = {1, 2, 3, 4, 6, 6}, AC[5] = {6, 4, 3, 2, 1};
            uint32_t r = tw_v(TW_ROLL, 0);
            iv = tw_units(r == 5u ? AC[tw_ticks / 12u > 4u ? 4u : tw_ticks / 12u] : T[r], 24);
        } else if (tw_is(TW_ARP)) {
            static const uint8_t D[4] = {24, 8, 4, 2};
            iv = tw_units(1, D[tw_v(TW_ARP, 1)]);
        }
        if (iv) {
            tw_acc += adv;
            while (tw_acc >= (uint32_t)iv) {
                tw_acc -= (uint32_t)iv;
                tw_ticks++;
                tw_retrig();
            }
        } else {
            tw_acc = 0, tw_ticks = 0;
        }
        if (was_arp && !tw_is(TW_ARP))
            tw_home();
        was_arp = (uint8_t)tw_is(TW_ARP);
    }
    if (tw_is(TW_STAIR)) {
        static const uint8_t D[4] = {8, 4, 2, 1};
        uint32_t len = BEAT_U / D[tw_v(TW_STAIR, 0)], sc = tw_v(TW_STAIR, 2);
        int32_t n = 0;
        while (TW_SC[sc][n] >= 0)
            n++;
        tw_st += adv;
        while (tw_st >= len) {
            tw_st -= len;
            if (tw_deg > -n)
                tw_deg -= (int32_t)tw_v(TW_STAIR, 1) + 1;
        }
        semis = tw_degsemi(tw_deg, sc);
    } else {
        tw_deg = 0, tw_st = 0;
    }
    {
        static const uint8_t P[4] = {2, 5, 12, 24};
        if (tw_is(TW_PDN))
            semis -= P[tw_v(TW_PDN, 0)];
        if (tw_is(TW_PUP))
            semis += P[tw_v(TW_PUP, 0)];
    }
    tw_bend = semis * 4096;
}
/* (UI) the twists that are a parameter while they are on: GLIDE THROW FILT SPEED */
static int16_t tw_keep[4];
static uint8_t tw_kept;
static void tw_apply(void)
{
    static const uint8_t GL[4] = {30, 60, 90, 120};
    static const int8_t FD[3] = {-30, -45, -60}, FU[3] = {20, 35, 55};
    static const uint8_t SN[5] = {1, 1, 2, 4, 8}, SD[5] = {4, 2, 1, 1, 1};
    track_t *t = &trk[0];
    if (!(tw_kept & 1u) && tw_is(TW_GLIDE))
        tw_keep[0] = t->p[P_GLIDE], tw_kept |= 1u;
    if (tw_kept & 1u) {
        t->p[P_GLIDE] = tw_is(TW_GLIDE) ? GL[tw_v(TW_GLIDE, 0)] : tw_keep[0];
        if (!tw_is(TW_GLIDE))
            tw_kept &= (uint8_t)~1u;
    }
    if (!(tw_kept & 2u) && tw_is(TW_THROW))
        tw_keep[1] = t->p[P_DLY], tw_kept |= 2u;
    if (tw_kept & 2u) {
        t->p[P_DLY] = tw_is(TW_THROW) ? (tw_v(TW_THROW, 0) ? 127 : 64) : tw_keep[1];
        if (!tw_is(TW_THROW))
            tw_kept &= (uint8_t)~2u;
    }
    if (!(tw_kept & 4u) && (tw_is(TW_FDN) || tw_is(TW_FUP)))
        tw_keep[2] = song.g[G_FILT], tw_kept |= 4u;
    if (tw_kept & 4u) {
        song.g[G_FILT] = tw_is(TW_FDN) ? FD[tw_v(TW_FDN, 0)] : tw_is(TW_FUP) ? FU[tw_v(TW_FUP, 0)] : tw_keep[2];
        if (!tw_is(TW_FDN) && !tw_is(TW_FUP))
            tw_kept &= (uint8_t)~4u;
    }
    if (!(tw_kept & 8u) && tw_is(TW_SPEED))
        tw_keep[3] = song.g[G_BPM], tw_kept |= 8u;
    if (tw_kept & 8u) {
        uint32_t s = tw_v(TW_SPEED, 0);
        song.g[G_BPM] = tw_is(TW_SPEED) ? (int16_t)clamp(tw_keep[3] * SN[s] / SD[s], GP[G_BPM].min, GP[G_BPM].max) : tw_keep[3];
        if (!tw_is(TW_SPEED))
            tw_kept &= (uint8_t)~8u;
    }
}
#else
static inline void tw_block(uint32_t adv) { (void)adv; }
#define tw_bend 0
#endif
