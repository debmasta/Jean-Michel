/* SPDX-License-Identifier: GPL-3.0-only */
/* TWISTS (from METAL KAP 2): FX held, the 16 white keys latch the twists on and off and pick the one KNOBS 1..3 edit; let go
 * and every note played (keys, sequencer, arp, MIDI) goes through them. They act on notes, not on the audio: nothing here
 * takes RAM from the synth. Key 15 lets everything go, key 16 is the screen flashes (ui_draw.c).
 * Runs in the audio ISR (tw_on, tw_off, tw_block) except the parts that change a parameter (tw_apply, the UI). */
#if CLIPPER_SYNTH
static volatile uint8_t fl_note;                  /* counts the notes: a lightning cue for the screen (ui_draw.c) */
static uint8_t tw_last_on, tw_last_n, tw_last_v;   /* last note played: what FOOTWORK and POLY cut up while the sequencer runs */
static void tw_cc_out(void);
enum { TW_STUT, TW_ROLL, TW_ARP, TW_OCT, TW_GLIDE, TW_SDL, TW_SCALE, TW_STAIR, TW_PDN, TW_PUP, TW_FDN, TW_FUP, TW_THROW, TW_SPEED,
       TW_ALLOFF, TW_CATS,
       /* page 2 (the first black keys pick the page) */
       TW_BLFO, TW_TARP, TW_MUT, TW_SETA, TW_SETB, TW_RECCLR, TW_NUA, TW_FOOT, TW_POLY, TW_N };
#define TW_NSC 19u
static const int8_t TW_SC[TW_NSC][13] = {
    {0, 1, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 7, 8, -1}, {0, 2, 4, 6, 8, 10, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 3, 5, 7, 10, -1},
    {0, 2, 4, 5, 7, 9, 11, -1}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1}, {0, 2, 3, 5, 7, 8, 11, -1}, {0, 2, 3, 5, 7, 9, 11, -1},
    {0, 2, 3, 5, 7, 9, 10, -1}, {0, 2, 4, 6, 7, 9, 11, -1}, {0, 2, 4, 5, 7, 9, 10, -1}, {0, 1, 3, 5, 6, 8, 10, -1}, {0, 1, 4, 5, 7, 8, 10, -1},
    {0, 2, 3, 6, 7, 8, 11, -1}, {0, 3, 5, 6, 7, 10, -1}, {0, 2, 4, 7, 9, -1}, {0, 1, 5, 7, 10, -1}, {0, 1, 4, 5, 7, 8, 11, -1}};
static const char *const TW_SCN[TW_NSC] = {"PHRYG", "HIRAJ", "WHOLE", "MINOR", "MPENT", "MAJOR", "CHROM", "HMIN", "MMIN", "DORI", "LYDI",
                                           "MIXO", "LOCR", "PDOM", "HUNG", "BLUES", "PENT", "INSEN", "DHARM"};
#define TW_NP 4u
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
static const char *const TN_DEST[] = {"CUT", "PITCH", "SHAPE"};
static const char *const TN_DEPTH[] = {"10", "25", "50", "75", "100"};
static const char *const TN_LRATE[] = {"1/4", "1/8", "1/16", "1/32", "1/64", "FAST"};
static const char *const TN_LAB[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};
static const char *const TN_TSET[] = {"OCTS", "5THS", "DARK", "WIDE"};
static const char *const TN_MUT[] = {"SOFT", "MID", "WILD"};
static const char *const TN_NUA[] = {"HAIR", "SOFT", "MID"};
static const char *const TN_NUM[] = {"FIXED", "EVOLVE", "RND"};
static const char *const TN_FST[] = {"JUKE", "GHETTO", "ROLLS", "CHOP"};
static const char *const TN_FVR[] = {"0", "25", "50", "75", "100"};
static const char *const TN_PRT[] = {"3:2", "3:4", "5:4", "7:4", "5:3", "7:5"};
static const char *const TN_PSP[] = {"SAME", "OCT", "5TH", "WIDE"};
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
    [TW_BLFO] = {{"form", 16, 0, TN_LAB}, {"dest", NN(TN_DEST), 0, TN_DEST}, {"depth", NN(TN_DEPTH), 2, TN_DEPTH}, {"speed", NN(TN_LRATE), 2, TN_LRATE}},
    [TW_TARP] = {{"set", NN(TN_TSET), 0, TN_TSET}, {"rate", NN(TN_ARATE), 2, TN_ARATE}},
    [TW_MUT] = {{"amount", NN(TN_MUT), 1, TN_MUT}},
    [TW_FOOT] = {{"style", NN(TN_FST), 0, TN_FST}, {"vary", NN(TN_FVR), 2, TN_FVR}},
    [TW_POLY] = {{"ratio", NN(TN_PRT), 1, TN_PRT}, {"spread", NN(TN_PSP), 1, TN_PSP}},
    [TW_NUA] = {{"amount", NN(TN_NUA), 1, TN_NUA}, {"mode", NN(TN_NUM), 0, TN_NUM}},
};
static const char *const TW_LAB[TW_N] = {"stut", "roll", "arp", "oct", "glide", "strdly", "scale", "stair", "pitch-", "pitch+",
                                         "filt-", "filt+", "throw", "speed", "ALL OFF", "flash", "bytlfo", "3arp", "mutate", "set A", "set B", "clr rec", "nuance", "footwk", "poly"};
static volatile uint32_t tw_mask;                /* the twists on */
static uint8_t tw_sel;                           /* the one the knobs edit */
static uint8_t tw_page;                          /* which 16 the white keys show (black keys 1, 2) */
static uint8_t tw_p[TW_N][TW_NP];
static uint8_t tw_ready;

static volatile int32_t tw_bend;                 /* 1/4096 semitone, read by voice.c */
static volatile int32_t tw_lcut, tw_lshape;      /* BYTE LFO: added to each voice's cutoff, shape (0..127 << 8 scale) */
static volatile int32_t tw_lfo_a = 128, tw_lfo_b = 128;   /* its a and b (MIDI CC 87, 88: the sticks) */
#define TW_NH 8u
static struct { uint8_t in, out, cur, on, vel; uint8_t ai; } tw_h[TW_NH];
#define TW_NE 24u
/* an echo of STRUM DELAY, whole: its start and its end together, so one never goes without the other. stage 0 free, 1 waiting
 * to start, 2 sounding */
static struct { uint8_t stage, note, vel; int32_t on, off; } tw_ev[TW_NE];
static uint32_t tw_acc, tw_st, tw_ticks, tw_oalt;
static int32_t tw_deg;
static int16_t tw_mut_base[8];
static uint8_t tw_mut_have;
static uint32_t tw_rand = 0x9E3779B9u;
static uint32_t tw_rnd(uint32_t n) { tw_rand ^= tw_rand << 13; tw_rand ^= tw_rand >> 17; tw_rand ^= tw_rand << 5; return (tw_rand >> 8) % n; }
/* MIDI, held from outside (the FM-1 PAD plugin, the game controller): CC 102..115 hold twists 0..13 while >= 64, on top of the
 * latched ones (the FX window is untouched); CC 116 the stutter's rate, 117 the master filter (64 = open), 119 pitch +-24 */
static volatile uint32_t tw_cc;
static uint32_t tw_cc_seen;
static volatile int32_t tw_cc_semi;
/* the twists written into the sequencer: REC + PLAY stores what is latched on each step, PLAY plays it back ("clr rec" empties it).
 * RAM only: not saved with the project */
static uint32_t tw_auto[NSTEP];
static volatile uint32_t tw_auto_now;
static inline int tw_is(uint32_t k) { return ((tw_mask | tw_cc | tw_auto_now) >> k) & 1u; }
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
static int tw_sounding(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < TW_NH; i++)
        if (tw_h[i].on && tw_h[i].cur == note)
            return 1;
    return 0;
}
/* every voice of the sound in use: an echo would take one from a note you hold (and load the CPU to the limit) */
static int tw_full(void)
{
    const track_t *t = &trk[0];
    const engine_t *e = ENGINES[t->engine % NENGINES];
    uint32_t k, n = 0, cap = e->poly && e->poly < NVOICE ? e->poly : NVOICE;
    for (k = 0; k < NVOICE; k++)
        n += t->v[k].active != 0;
    return n >= cap;
}
static void tw_echo(uint32_t note, uint32_t vel, int32_t on, int32_t off)
{
    uint32_t i;
    for (i = 0; i < TW_NE; i++)
        if (!tw_ev[i].stage) {
            tw_ev[i].stage = 1, tw_ev[i].note = (uint8_t)note, tw_ev[i].vel = (uint8_t)vel, tw_ev[i].on = on, tw_ev[i].off = off;
            return;
        }
}
static int32_t tw_units(uint32_t num, uint32_t den) { return (int32_t)(BEAT_U / den * num); }

static void tw_on(uint32_t note, uint32_t vel)
{
    track_t *t = &trk[0];
    int32_t m = (int32_t)note;
    uint32_t i;
    tw_last_on = 1, tw_last_n = (uint8_t)note, tw_last_v = (uint8_t)vel;
    fl_note++;
    if (tw_is(TW_MUT)) {                         /* MUTATE: this note, the sound's numbers thrown around where they are */
        static const uint8_t R[3] = {10, 28, 64};
        const engine_t *e = ENGINES[t->engine % NENGINES];
        uint32_t k, a = R[tw_v(TW_MUT, 0)];
        for (k = 0; k < 8u; k++) {
            const param_desc_t *d = &e->edit[k];
            if (!d->names && d->max > d->min && (tw_mut_have >> k & 1u))
                t->p[P_E0 + k] = (int16_t)clamp(tw_mut_base[k] + (int32_t)tw_rnd(2u * a + 1u) - (int32_t)a, d->min, d->max);
        }
    }
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
            tw_echo((uint32_t)o, gv ? gv : 1u, iv * (int32_t)e, iv * (int32_t)e + iv * 85 / 100);
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
        tw_ev[i].stage = 0;
    tw_last_on = 0;
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
/* TRIPLE ARP: three arps at once on the first held note, 3, 4 and 5 steps long: they drift apart and meet again every 60 */
static uint8_t tp_on, tp_cur[3], tp_step;
static uint32_t tp_acc, tw_free_T;
static void tp_stop(void)
{
    uint32_t j;
    if (!tp_on)
        return;
    tp_on = 0;
    for (j = 0; j < 3u; j++)
        trk_note_off0(&trk[0], tp_cur[j]);
}
static void tw_triple(uint32_t adv)
{
    static const int8_t L[4][3][5] = {
        {{0, 12, 24}, {7, 12, 19, 31}, {-12, 0, 12, 24, 36}},
        {{0, 7, 14}, {5, 12, 19, 24}, {-5, 2, 7, 14, 19}},
        {{0, 3, 7}, {-12, -9, -5, 1}, {12, 15, 13, 19, 22}},
        {{0, 19, 31}, {-12, 24, 7, 36}, {12, -5, 31, 2, 24}}};
    static const uint8_t LEN[3] = {3, 4, 5}, D[4] = {24, 8, 4, 2};
    uint32_t i, j;
    for (i = 0; i < TW_NH && !tw_h[i].on; i++)
        ;
    if (!tw_is(TW_TARP) || i == TW_NH) {
        if (tp_on) {
            tp_stop();
            if (i < TW_NH) {                         /* back to the note you hold */
                tw_h[i].cur = tw_h[i].out;
                trk_note_on0(&trk[0], tw_h[i].out, tw_h[i].vel);
            }
        }
        tp_acc = 0, tp_step = 0;
        return;
    }
    tp_acc += adv;
    if (tp_on && tp_acc < (uint32_t)tw_units(1, D[tw_v(TW_TARP, 1)]))
        return;
    tp_acc = 0;
    if (!tp_on)
        trk_note_off0(&trk[0], tw_h[i].cur);         /* the held note makes room */
    else
        tp_stop();
    for (j = 0; j < 3u; j++) {
        tp_cur[j] = (uint8_t)clamp((int32_t)tw_h[i].out + L[tw_v(TW_TARP, 0)][j][tp_step % LEN[j]], 0, 127);
        trk_note_on0(&trk[0], tp_cur[j], tw_h[i].vel * (3u - j) / 3u + 1u);
    }
    tp_on = 1;
    tp_step = (uint8_t)((tp_step + 1u) % 60u);
}
/* FOOTWORK and POLY: the note held (playing: the last note of the sequencer too) cut up by a rhythm generator, on the clock.
 * FOOTWORK: a bar of 16 cells (rest, hit, roll of 2 3 4) that varies by itself; POLY: two lanes, A against B hits per two beats */
static struct { uint8_t on, n; int32_t off; } rhy[2];
static uint8_t rhy_mute;
static uint32_t rhy_free, rhy_ftick = 0xFFFFFFFFu, rhy_bar, rhy_pi[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
static void rhy_hit(uint32_t l, uint32_t note, uint32_t vel, int32_t gate)
{
    if (rhy[l].on)
        trk_note_off0(&trk[0], rhy[l].n);
    trk_note_on0(&trk[0], note, vel);
    rhy[l].on = 1, rhy[l].n = (uint8_t)note, rhy[l].off = gate;
}
static void rhy_block(uint32_t adv)
{
    static const uint8_t PAT[4][16] = {{1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 2, 0},
                                       {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 3, 0, 1, 1, 0, 3},
                                       {1, 0, 3, 0, 1, 0, 3, 3, 1, 2, 0, 3, 1, 0, 4, 4},
                                       {1, 0, 0, 0, 2, 0, 0, 3, 0, 0, 1, 0, 2, 2, 3, 0}};
    static const uint8_t RT[5][6] = {{0, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0}, {1, 0, 1, 0, 1, 0}, {1, 1, 1, 1, 1, 1}};
    static const uint8_t PA[6] = {3, 3, 5, 7, 5, 7}, PB[6] = {2, 4, 4, 4, 3, 5};
    static const int8_t SP[4] = {0, 12, 7, 19};
    uint32_t i, note = 0, vel = 100, have = 0, u;
    for (i = 0; i < 2u; i++)
        if (rhy[i].on && (rhy[i].off -= (int32_t)adv) <= 0) {
            trk_note_off0(&trk[0], rhy[i].n);
            rhy[i].on = 0;
        }
    for (i = 0; i < TW_NH && !tw_h[i].on; i++)
        ;
    if (i < TW_NH)
        note = tw_h[i].out, vel = tw_h[i].vel, have = 1;
    else if (song.playing && tw_last_on)
        note = tw_last_n, vel = tw_last_v, have = 1;
    if ((!tw_is(TW_FOOT) && !tw_is(TW_POLY)) || !have) {
        if (rhy_mute) {
            rhy_mute = 0;
            if (i < TW_NH) {                         /* back to the note you hold */
                tw_h[i].cur = tw_h[i].out;
                trk_note_on0(&trk[0], tw_h[i].out, tw_h[i].vel);
            }
        }
        rhy_pi[0] = rhy_pi[1] = rhy_ftick = 0xFFFFFFFFu;
        return;
    }
    if (!rhy_mute) {
        rhy_mute = 1;
        if (i < TW_NH)
            trk_note_off0(&trk[0], tw_h[i].cur);     /* the held note makes room: the rhythm plays it */
    }
    if (song.playing)
        u = (uint32_t)(bb_Tnow >> 8);                /* T << 8 (Q28 beats >> 16 = T, 4096 a beat) */
    else {
        rhy_free += adv;
        u = (uint32_t)(((uint64_t)rhy_free << 8 << 12) / BEAT_U);   /* (T's scale, 2^12 a beat, << 8) */
    }
    if (tw_is(TW_FOOT)) {
        uint32_t tk = (u >> 8) * 24u / 4096u, sty = tw_v(TW_FOOT, 0), bar, st, sub, c, h;
        if (tk != rhy_ftick) {
            rhy_ftick = tk;
            bar = tk / 96u, st = (tk % 96u) / 6u, sub = tk % 6u;
            c = PAT[sty][st];
            h = (st + 1u) * 2654435761u ^ (bar * 40503u + sty);
            h ^= h >> 15;
            if ((h * 2654435761u >> 24) % 100u < tw_v(TW_FOOT, 1) * 25u / 2u)   /* VARY: this cell goes elsewhere */
                c = (h >> 7) % 5u;
            if (RT[c][sub])
                rhy_hit(0, note, sub ? vel * 3u / 4u + 1u : vel, tw_units(c > 1u ? 1 : 1, c > 1u ? 12 : 5));
        }
    }
    if (tw_is(TW_POLY)) {
        uint32_t r = tw_v(TW_POLY, 0), l, cyc = 2u * 4096u;
        for (l = 0; l < 2u; l++) {
            uint32_t a = l ? PB[r] : PA[r], k = (u >> 8) * a / cyc;
            if (k != rhy_pi[l]) {
                rhy_pi[l] = k;
                if (!tw_is(TW_FOOT) || l)
                    rhy_hit(l, (uint32_t)clamp((int32_t)note + (l ? SP[tw_v(TW_POLY, 1)] : 0), 0, 127), l ? vel * 4u / 5u + 1u : vel,
                            tw_units(1, a + 1u));
            }
        }
    }
}
/* once per control block, adv = its length in clock units */
static void tw_block(uint32_t adv)
{
    static uint8_t was_arp;
    uint32_t i;
    int32_t semis = 0;
    bb_Tnow = ((uint64_t)clk_beat << 28) + (((uint64_t)clk_pos << 28) / BEAT_U);
    bb_Tstep = ((uint64_t)(uint32_t)song.g[G_BPM] << 28) / BEAT_U;
    if (song.playing && tw_ready && tw_is(TW_NUA) && tw_mut_have) {    /* NUANCE: each step of the sequencer, the sound a little elsewhere */
        static uint32_t last = 0xFFFFFFFFu, loops;
        uint32_t st = trk[0].seq_idx % NSTEP;
        if (st != last) {
            static const uint8_t R[3] = {3, 7, 14};
            const engine_t *e = ENGINES[trk[0].engine % NENGINES];
            uint32_t k, a = R[tw_v(TW_NUA, 0)], md = tw_v(TW_NUA, 1);
            if (st < last)
                loops++;
            last = st;
            for (k = 0; k < 8u; k++) {
                const param_desc_t *d = &e->edit[k];
                uint32_t h;
                if (d->names || d->max <= d->min)
                    continue;
                h = (st + 1u) * 2654435761u ^ (k + 1u) * 40503u;
                if (md == 1u)
                    h ^= (loops / 2u) * 2246822519u;      /* EVOLVE: another set every two loops */
                else if (md == 2u)
                    h = tw_rnd(0xFFFFFFFEu);
                h ^= h >> 13;
                trk[0].p[P_E0 + k] = (int16_t)clamp(tw_mut_base[k] + (int32_t)((h * 2654435761u >> 16) % (2u * a + 1u)) - (int32_t)a, d->min, d->max);
            }
        }
    }
    if (song.playing) {
        uint32_t st = trk[0].seq_idx % NSTEP;
        if (song.rec & 1u)
            tw_auto[st] = tw_mask, tw_auto_now = 0;
        else
            tw_auto_now = tw_auto[st];
    } else {
        tw_auto_now = 0;
    }
    if (!tw_ready)
        return;
    for (i = 0; i < TW_NE; i++)
        if (tw_ev[i].stage) {
            tw_ev[i].on -= (int32_t)adv;
            tw_ev[i].off -= (int32_t)adv;
            if (tw_ev[i].stage == 1u && tw_ev[i].on <= 0) {
                if (tw_sounding(tw_ev[i].note) || tw_full())   /* that pitch is held, or no voice free: no echo (never steal) */
                    tw_ev[i].stage = 0;
                else {
                    trk_note_on0(&trk[0], tw_ev[i].note, tw_ev[i].vel);
                    tw_ev[i].stage = 2;
                }
            }
            if (tw_ev[i].stage == 2u && tw_ev[i].off <= 0) {
                if (!tw_sounding(tw_ev[i].note))     /* (held since it started: not ours to end) */
                    trk_note_off0(&trk[0], tw_ev[i].note);
                tw_ev[i].stage = 0;
            }
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
    tw_bend = (semis + tw_cc_semi) * 4096;
    tw_triple(adv);
    rhy_block(adv);
    {   /* BYTE LFO: a formula's bytes as a modulation, locked to the tempo */
        static uint32_t freeq;
        tw_lcut = tw_lshape = 0;
        if (tw_is(TW_BLFO)) {
            static const uint8_t SH[6] = {12, 11, 10, 9, 8, 6}, DP[5] = {10, 25, 50, 75, 100};
            uint32_t Tn;
            int32_t var[10] = {tw_lfo_a, tw_lfo_b, 128, 128, 0, 0, 0, 0, 0, 60}, v, depth = DP[tw_v(TW_BLFO, 2)], m;
            if (song.playing)
                Tn = (uint32_t)(((uint64_t)clk_beat << 12) + (((uint64_t)clk_pos << 12) / BEAT_U));
            else {
                freeq += adv;
                while (freeq >= BEAT_U >> 12)
                    freeq -= BEAT_U >> 12, tw_free_T++;
                Tn = tw_free_T;
            }
            var[8] = (int32_t)Tn;
            v = bb_run(tw_v(TW_BLFO, 0), (int32_t)((Tn >> SH[tw_v(TW_BLFO, 3)]) * 257u), var) & 255;   /* (a step of the grid = 257 of t: its bytes and its high bits both move) */
            m = (v - 128) * depth / 100;                  /* -128 .. 127 */
            switch (tw_v(TW_BLFO, 1)) {
            case 0: tw_lcut = m * 96; break;              /* up to +-48 steps of the cutoff */
            case 1: tw_bend += m * 12 * 4096 / 128; break;   /* up to +-12 semitones */
            default: tw_lshape = m * 96; break;
            }
        }
    }
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
    if ((tw_is(TW_MUT) || tw_is(TW_NUA)) && !tw_mut_have) {         /* MUTATE: the sound as it is, given back when it ends */
        uint32_t k;
        for (k = 0; k < 8u; k++)
            tw_mut_base[k] = t->p[P_E0 + k];
        tw_mut_have = 0xFFu;
    } else if (!tw_is(TW_MUT) && !tw_is(TW_NUA) && tw_mut_have) {
        uint32_t k;
        tw_mut_have = 0;
        for (k = 0; k < 8u; k++)
            t->p[P_E0 + k] = tw_mut_base[k];
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
/* a CC from outside (seq.c midi_cc, the audio ISR); 1 = it was one of these */
static int tw_midi_cc(uint32_t cc, uint32_t v)
{
    if (cc == 26u || cc == 27u || cc == 28u || cc == 29u) {   /* FOOTWORK style, vary; POLY ratio, spread */
        uint32_t k = cc < 28u ? TW_FOOT : TW_POLY, p = cc & 1u ? 1u : 0u;
        tw_init();
        tw_p[k][p] = (uint8_t)(v * TW_PAR[k][p].n / 128u);
        return 1;
    }
    if (cc == 30u || cc == 31u) {
        uint32_t b = 1u << (cc == 30u ? TW_FOOT : TW_POLY);
        tw_init();
        if (v >= 64u) tw_cc |= b; else tw_cc &= ~b;
        return 1;
    }
    if (cc == 85u || cc == 86u || cc == 118u || cc == 84u) {   /* the page 2 twists held: 118 BYTE LFO, 85 TRIPLE ARP, 86 MUTATE */
        uint32_t b = 1u << (cc == 118u ? TW_BLFO : cc == 85u ? TW_TARP : cc == 84u ? TW_NUA : TW_MUT);
        tw_init();
        if (v >= 64u) tw_cc |= b; else tw_cc &= ~b;
        return 1;
    }
    if (cc == 87u || cc == 88u) {
        if (cc == 87u) tw_lfo_a = (int32_t)v * 2; else tw_lfo_b = (int32_t)v * 2;
        return 1;
    }
    if (cc >= 102u && cc <= 115u) {
        tw_init();
        if (v >= 64u)
            tw_cc |= 1u << (cc - 102u);
        else
            tw_cc &= ~(1u << (cc - 102u));
        return 1;
    }
    if (cc >= 20u && cc <= 24u) {                 /* BYTE: A B C D MORPH, one slider each in Live */
        if (trk[0].eng_req != ENGI_BYTE)
            return 0;
        trk[0].p[P_E1 + (cc == 24u ? 5u : cc - 20u)] = (int16_t)v;
        return 1;
    }
    if (cc == 25u) {                              /* BYTE: which memory plays: 0 none, then A B C D */
        bb_force = (uint8_t)(v * 5u / 128u);
        return 1;
    }
    if (cc == 116u) {
        tw_init();
        tw_p[TW_STUT][0] = (uint8_t)(v * 9u / 128u);
        return 1;
    }
    if (cc == 117u) {
        song.g[G_FILT] = (int16_t)((int32_t)v - 64);
        return 1;
    }
    if (cc == 119u) {
        tw_cc_semi = ((int32_t)v - 64) * 24 / 63;
        return 1;
    }
    return 0;
}
/* (UI, every frame) a held twist that is a parameter comes on or goes off */
static void tw_poll(void)
{
    if ((tw_cc | tw_auto_now) != tw_cc_seen) {
        tw_cc_seen = tw_cc | tw_auto_now;
        tw_apply();
    }
}
#endif
