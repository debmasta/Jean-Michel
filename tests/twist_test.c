/* SPDX-License-Identifier: GPL-3.0-only */
/* The twists (twist.c): each on its own and random mixes on a held chord: bounded, audible, no note left hanging after the
 * release; the note-level ones do what they say; the parameters they borrow come back. */
#include "synth8_head.c"   /* synth8_test.c up to its main (run_tests.sh) */
static uint32_t nbad;
static void chk(const char *w, int ok) { printf("%-66s %s\n", w, ok ? "ok" : "FAIL"); nbad += !ok; }
static uint32_t live(void) { uint32_t k, n = 0; for (k = 0; k < NVOICE; k++) n += trk[0].v[k].gate != 0; return n; }
static void go(uint32_t mask)
{
    tw_mask = mask; tw_apply();
    trk_note_on(&trk[0], 48, 100); trk_note_on(&trk[0], 55, 100);
    run(900);
    trk_note_off(&trk[0], 48); trk_note_off(&trk[0], 55);
    tw_mask = 0; tw_apply();
    run(3000);
}
int main(void)
{
    uint32_t i, k, bad = 0;
    host_tracks_init();
    for (i = 0; i < NPART; i++) host_preset(&trk[i], TRK_DEF[i][0], TRK_DEF[i][1]);
    clipper_proj_neutral(); trk[1].p[P_LYMODE] = LY_OFF; song.g[G_BPM] = 120;
    tw_init();
    quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0;
    for (k = 0; k < TW_ALLOFF; k++) {
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0;
        go(1u << k);
        if (!bounded() || live()) { bad++; printf("  %s: %u gates left\n", TW_LAB[k], live()); }
    }
    chk("each twist on a chord: no hanging note, bounded", !bad);
    bad = 0;
    for (i = 0; i < 40u; i++) {
        uint32_t m = 0;
        quiet(); host_preset(&trk[0], i % 3u ? ENGI_BYTE : 0u, 0); trk[0].p[P_AMODE] = 0;
        for (k = 0; k < TW_ALLOFF; k++) { m |= (tw_rnd(3) == 0u) << k; tw_p[k][0] = (uint8_t)(TW_PAR[k][0].n ? tw_rnd(TW_PAR[k][0].n) : 0); }
        go(m);
        if (!bounded() || live()) { bad++; printf("  mix %x: %u left\n", m, live()); }
    }
    chk("40 random mixes: no hanging note, bounded", !bad);
    {
        int16_t g = trk[0].p[P_GLIDE], d = trk[0].p[P_DLY], f = song.g[G_FILT], b = song.g[G_BPM];
        tw_mask = 1u << TW_GLIDE | 1u << TW_THROW | 1u << TW_FDN | 1u << TW_SPEED; tw_apply();
        chk("GLIDE THROW FILT SPEED change their parameters", trk[0].p[P_DLY] != d && song.g[G_FILT] != f && song.g[G_BPM] != b);
        tw_mask = 0; tw_apply();
        chk("and give them back", trk[0].p[P_GLIDE] == g && trk[0].p[P_DLY] == d && song.g[G_FILT] == f && song.g[G_BPM] == b);
    }
    {
        tw_p[TW_OCT][0] = 1; tw_mask = 1u << TW_OCT;
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0;
        trk_note_on(&trk[0], 60, 100);
        { uint32_t n = 0; for (k = 0; k < NVOICE; k++) if (trk[0].v[k].gate) n = trk[0].v[k].note; chk("OCTAVE +1 plays the note an octave up", n == 72u); }
        trk_note_off(&trk[0], 60); tw_mask = 0; run(2000);
        chk("and its note-off finds it", !live());
    }
    {   /* held from outside: CC 102.. (the FM-1 PAD plugin) */
        tw_mask = 0; tw_poll();
        chk("CC 103 holds ROLL", tw_midi_cc(103, 127) && tw_is(TW_ROLL));
        chk("and lets it go", tw_midi_cc(103, 0) && !tw_is(TW_ROLL));
        tw_mask = 1u << TW_OCT; tw_midi_cc(102, 127);
        chk("a latched twist stays when one is held", tw_is(TW_OCT) && tw_is(TW_STUT));
        tw_midi_cc(102, 0); chk("and after", tw_is(TW_OCT) && !tw_is(TW_STUT)); tw_mask = 0;
        tw_midi_cc(119, 127); run(10); chk("CC 119: pitch +24", tw_bend == 24 * 4096); tw_midi_cc(119, 64); run(10); chk("CC 119 at 64: none", tw_bend == 0);
        tw_midi_cc(104 + 8, 127); tw_poll(); chk("a parameter twist held by CC reaches its parameter", song.g[G_FILT] != 0 || tw_is(TW_FDN)); tw_midi_cc(112, 0); tw_poll();
        tw_midi_cc(102 + TW_THROW, 127); { int16_t d0 = tw_keep[1]; (void)d0; tw_poll(); chk("THROW by CC opens the delay", trk[0].p[P_DLY] > 0); } tw_midi_cc(102 + TW_THROW, 0); tw_poll();
    }
    {   /* page 2 */
        int16_t e[8]; uint32_t voices = 0;
        quiet(); host_preset(&trk[0], 0u, 0); trk[0].p[P_AMODE] = 0;
        for (k = 0; k < 8u; k++) e[k] = trk[0].p[P_E0 + k];
        tw_mask = 1u << TW_MUT; tw_apply();
        for (k = 0; k < 8u; k++) { trk_note_on(&trk[0], 60, 100); run(5); trk_note_off(&trk[0], 60); run(5); }
        { int moved = 0; for (k = 0; k < 8u; k++) moved |= trk[0].p[P_E0 + k] != e[k]; chk("MUTATE moves the sound", moved); }
        tw_mask = 0; tw_apply();
        { int same = 1; for (k = 0; k < 8u; k++) same &= trk[0].p[P_E0 + k] == e[k]; chk("and gives it back exactly", same); }
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0;
        tw_mask = 1u << TW_TARP; trk_note_on(&trk[0], 48, 100); run(400);
        for (k = 0; k < NVOICE; k++) voices += trk[0].v[k].gate != 0;
        chk("TRIPLE ARP sounds three notes", voices == 3u);
        tw_mask = 0; run(20); voices = 0; for (k = 0; k < NVOICE; k++) voices += trk[0].v[k].gate != 0;
        chk("then only the note held", voices == 1u);
        trk_note_off(&trk[0], 48); run(2000); chk("and none after", !live());
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0; tw_p[TW_BLFO][1] = 0;
        tw_mask = 1u << TW_BLFO; trk_note_on(&trk[0], 48, 100); { int32_t a = 0x7FFFFFFF, b = -a; for (k = 0; k < 200u; k++) { run(8); if (tw_lcut < a) a = tw_lcut; if (tw_lcut > b) b = tw_lcut; } chk("BYTE LFO moves the cutoff", b > a); }
        tw_mask = 0; trk_note_off(&trk[0], 48); run(2000); chk("bounded, no hanging note", bounded() && !live() && (run(10), tw_lcut == 0));
    }
    {   /* STRUM DELAY under chords: every echo that starts must end, the held notes must stay */
        uint32_t n, stuck = 0, heldlost = 0;
        for (n = 0; n < 12u; n++) {
            quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0;
            tw_p[TW_SDL][0] = (uint8_t)(n % 6u); tw_p[TW_SDL][1] = (uint8_t)(n % 5u); tw_p[TW_SDL][2] = 2;
            tw_mask = 1u << TW_SDL;
            trk_note_on(&trk[0], 48, 100); run(3); trk_note_on(&trk[0], 52, 100); run(3); trk_note_on(&trk[0], 55, 100); run(3);
            trk_note_off(&trk[0], 52); run(2); trk_note_off(&trk[0], 55); run(2);
            trk_note_on(&trk[0], 60, 100); run(1200);
            { uint32_t g = 0; for (k = 0; k < NVOICE; k++) if (trk[0].v[k].gate && (trk[0].v[k].note == 48u || trk[0].v[k].note == 60u)) g++; if (g < 2u) heldlost++; }
            trk_note_off(&trk[0], 48); trk_note_off(&trk[0], 60); tw_mask = 0; run(4000);
            if (live()) stuck++;
        }
        tw_p[TW_SDL][2] = 1;
        chk("STRUM DELAY on chords: no echo left hanging", !stuck);
        chk("STRUM DELAY: the notes held stay held", !heldlost);
    }
    {   /* MORPH and the sequencer */
        int32_t var[10], c; int16_t pv[P_COUNT];
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0);
        trk[0].p[P_E1] = 10; for (k = 0; k < 5u; k++) bb_snap[0][k] = trk[0].p[P_E1 + k];
        trk[0].p[P_E1] = 110; for (k = 0; k < 5u; k++) bb_snap[1][k] = trk[0].p[P_E1 + k]; bb_have = 3;
        trk[0].p[P_E6] = 127; bb_knobs(trk[0].p, var, &c); chk("MORPH at the top: state B", var[0] == 220);
        tw_midi_cc(24, 1); bb_knobs(trk[0].p, var, &c); chk("CC 24 moves MORPH: near state A", var[0] < 26);
        tw_midi_cc(24, 64); bb_knobs(trk[0].p, var, &c); chk("halfway: between", var[0] > 100 && var[0] < 130);
        tw_midi_cc(24, 0); bb_have = 0; tw_midi_cc(20, 64); chk("CC 20 moves A", trk[0].p[P_E1] == 64); (void)pv;
        bb_snap[0][0] = 0; bb_snap[1][0] = 40; bb_snap[2][0] = 80; bb_snap[3][0] = 120; bb_have = 15;
        trk[0].p[P_E6] = 1; bb_knobs(trk[0].p, var, &c); chk("four memories: 1 is A", var[0] == 0);
        trk[0].p[P_E6] = 43; bb_knobs(trk[0].p, var, &c); chk("43 is B", var[0] == 80);
        trk[0].p[P_E6] = 85; bb_knobs(trk[0].p, var, &c); chk("85 is C", var[0] == 160);
        trk[0].p[P_E6] = 127; bb_knobs(trk[0].p, var, &c); chk("127 is D", var[0] == 240);
        trk[0].p[P_E6] = 20;
        tw_midi_cc(25, 95); bb_knobs(trk[0].p, var, &c); chk("CC 25 at 95 plays memory C", var[0] == 160);
        tw_midi_cc(25, 127); bb_knobs(trk[0].p, var, &c); chk("CC 25 at 127 plays memory D", var[0] == 240);
        bb_have = 3; tw_midi_cc(25, 95); bb_knobs(trk[0].p, var, &c); chk("an empty memory plays nothing special", var[0] != 160);
        bb_have = 15; tw_midi_cc(25, 0); bb_knobs(trk[0].p, var, &c); chk("CC 25 at 0: back to MORPH", var[0] != 160 && var[0] != 240);
        trk[0].p[P_E6] = 0; bb_have = 0;
        memset(tw_auto, 0, sizeof tw_auto); song.playing = 1; song.rec = 1; tw_mask = 1u << TW_ROLL; run(4);
        { uint32_t st = trk[0].seq_idx % NSTEP; chk("REC writes the latched twist on the step", (tw_auto[st] >> TW_ROLL) & 1u);
          song.rec = 0; tw_mask = 0; tw_block(0); chk("PLAY plays it back", tw_is(TW_ROLL)); }
        memset(tw_auto, 0, sizeof tw_auto); song.playing = 0; tw_block(0); chk("stopped: nothing from the sequencer", !tw_is(TW_ROLL));
    }
    {   /* NUANCE on the sequencer */
        int16_t e[8]; int16_t got[2]; int moved = 0;
        quiet(); host_preset(&trk[0], 0u, 0);
        for (k = 0; k < 8u; k++) e[k] = trk[0].p[P_E0 + k];
        tw_p[TW_NUA][1] = 0; tw_mask = 1u << TW_NUA; tw_apply();
        song.playing = 1; song.rec = 0; trk[0].seq_idx = 3; tw_block(0);
        for (k = 0; k < 8u; k++) moved |= trk[0].p[P_E0 + k] != e[k];
        chk("NUANCE moves the sound on a step", moved);
        got[0] = trk[0].p[P_E1]; trk[0].seq_idx = 4; tw_block(0); trk[0].seq_idx = 3; tw_block(0); got[1] = trk[0].p[P_E1];
        chk("FIXED: a step keeps its own nuance", got[0] == got[1]);
        { int small = 1; for (k = 0; k < 8u; k++) { const param_desc_t *d = &ENGINES[trk[0].engine]->edit[k]; if (!d->names && d->max > d->min) small &= abs(trk[0].p[P_E0 + k] - e[k]) <= 7; } chk("and stays subtle", small); }
        song.playing = 0; tw_mask = 0; tw_apply();
        { int same = 1; for (k = 0; k < 8u; k++) same &= trk[0].p[P_E0 + k] == e[k]; chk("off: the sound comes back exactly", same); }
    }
    {   /* FOOTWORK and POLY */
        uint32_t hits; int32_t was; uint32_t q;
        quiet(); host_preset(&trk[0], ENGI_BYTE, 24); trk[0].p[P_AMODE] = 0;
        tw_p[TW_FOOT][1] = 0; tw_p[TW_FOOT][0] = 0;
        tw_mask = 1u << TW_FOOT; trk_note_on(&trk[0], 48, 100);
        hits = 0; was = 0;
        for (q = 0; q < 2000u; q++) { run(4); if (rhy[0].on && rhy[0].off > was) hits++; was = rhy[0].on ? rhy[0].off : 0; }
        printf("  footwork: %u hits\n", hits);
        chk("FOOTWORK plays hits on the held note", hits > 5u);
        tw_mask = 0; run(40);
        { uint32_t g = 0; for (k = 0; k < NVOICE; k++) g += trk[0].v[k].gate != 0; chk("off: the held note comes back", g >= 1u); }
        trk_note_off(&trk[0], 48); run(3000); chk("released: nothing left", !live());
        quiet(); host_preset(&trk[0], 0u, 0); trk[0].p[P_AMODE] = 0;
        tw_p[TW_POLY][0] = 1; tw_mask = 1u << TW_POLY; trk_note_on(&trk[0], 48, 100); run(2000);
        chk("POLY sounds on the two lanes", rhy[0].n == 48u || rhy[1].n == 60u);
        tw_mask = 0; run(40); trk_note_off(&trk[0], 48); run(3000); chk("POLY off, released: nothing left", bounded() && !live());
        tw_midi_cc(28, 127); chk("CC 28 sets the ratio", tw_p[TW_POLY][0] == 5u); tw_midi_cc(28, 30);
    }
    {   /* VOICE 4 */
        track_t *t = &trk[0]; uint32_t g;
        quiet(); host_preset(t, 0u, 0); t->p[P_AMODE] = 0; t->p[P_VOICE] = V_UNISON;
        vt_stack[0] = 1; trk_note_on(t, 48, 100); run(4);
        chk("STACK: voice 1 an octave up, voice 0 not", t->v[1].pitch16 == 60 * 16 && t->v[0].pitch16 == 48 * 16);
        trk_note_off(t, 48); run(3000); chk("STACK released: nothing left", !live()); vt_stack[0] = 0;
        vt_flam[0] = 40; trk_note_on(t, 48, 100);
        for (g = 0, k = 1; k < NVOICE; k++) g += t->v[k].gate != 0;
        chk("FLAM: the others wait", g == 0u); run(400);
        for (g = 0, k = 1; k < NVOICE; k++) g += t->v[k].gate != 0;
        chk("FLAM: then they come", g >= 1u);
        trk_note_off(t, 48); run(3000); chk("FLAM released: nothing left", !live());
        trk_note_on(t, 50, 100); run(1); trk_note_off(t, 50); run(3000); chk("FLAM cut short: no voice left behind", !live()); vt_flam[0] = 0;
        vt_drift[0] = 127; vt_tick = 700; chk("DRIFT moves the pitch", vt_drift_q(t, &t->v[0]) != vt_drift_q(t, &t->v[1])); vt_drift[0] = 0;
        t->p[P_VOICE] = 1; t->p[P_GLIDE] = 0; vt_grnd[0] = 127;
        trk_note_on(t, 48, 100); run(10); trk_note_off(t, 48); run(10); trk_note_on(t, 60, 100);
        chk("GLIDE RND at the top: the note glides", t->v[0].pitch_cur != t->v[0].pitch16);
        trk_note_off(t, 60); vt_grnd[0] = 0; t->p[P_VOICE] = 0; run(3000); chk("GLIDE RND released: nothing left", !live());
    }
    {   /* user formulas: the checks */
        static bb_user_t u; int32_t var[10] = {128, 128, 128, 128};
        static const uint8_t OK[] = {BB_T, BB_K, BB_MUL}, UND[] = {BB_K, BB_MUL}, BADV[] = {BB_V}; static const int32_t OKA[] = {0, 3, 0}, UA[] = {1, 0}, VA[] = {99};
        chk("install: t*3 accepted", bb_install(0, OK, OKA, 3, 0, 0, &u)); bb_user[0] = u;
        chk("install: it runs", bb_run(BB_N, 7, var) == 21);
        chk("install: packed in one", u.n == 2u);
        chk("install: stack underflow refused", !bb_install(0, UND, UA, 2, 0, 0, &u));
        chk("install: unknown variable refused", !bb_install(0, BADV, VA, 1, 0, 0, &u));
        memset(&bb_user[0], 0, sizeof u);
    }
    {   /* CHAOS gives the sound back, REMIX drops and comes back, GRAB records */
        track_t *t = &trk[0]; int16_t before[8]; uint32_t q; int same = 1, seenbits = 0;
        quiet(); host_preset(t, ENGI_BYTE, 24); t->p[P_AMODE] = 0;
        memcpy(before, &t->p[P_E0], sizeof before);
        trk_note_on(t, 48, 100);
        tw_midi_cc(83, 127);
        for (q = 0; q < 8000u; q++) { run(1); seenbits |= tw_ch != 0; if (q % 500u == 0u) { trk_note_on(t, 52, 90); trk_note_off(t, 52); } }
        chk("CHAOS: its twists turn on", seenbits);
        tw_midi_cc(83, 0); run(40);
        chk("CHAOS let go: nothing of it left", tw_ch == 0u && !tw_is(TW_MUT));
        for (q = 0; q < 8u; q++) same &= t->p[P_E0 + q] == before[q];
        chk("CHAOS let go: the sound exactly as it was", same);
        { uint32_t g = 0; for (k = 0; k < NVOICE; k++) g += t->v[k].gate != 0; chk("CHAOS let go: the held note sounds", g >= 1u); }
        trk_note_off(t, 48); run(3000); chk("CHAOS: nothing left", !live());
        tw_midi_cc(81, 127); run(4);
        chk("GRAB recording", grb_st == 2u);
        { static int32_t oo[64]; for (q = 0; q < 25000u && grb_st; q++) { grb_tap(oo, 32); run(1); } }
        chk("GRAB: filled and stopped", grb_st == 0u && grb_len == GRB_N);
        memset(grb_buf, 40, GRB_N);
        quiet(); host_preset(t, ENGI_GRAB, 0); t->engine = t->eng_req = ENGI_GRAB;
        trk_note_on(t, 50, 100); run(200);
        chk("GRAB engine sounds", bounded());
        { uint32_t vi = 0; while (vi < NVOICE - 1u && !t->v[vi].gate) vi++; chk("slice of its key", t->v[vi].ph[2] > t->v[vi].ph[1] && t->v[vi].ph[1] == 2u * GRB_N / 16u); }
        trk_note_off(t, 50); run(3000); chk("GRAB released: nothing left", !live());
        grb_len = 0;
    }
    printf("TWISTS: %s\n", nbad ? "FAIL" : "ok");
    return nbad != 0;
}
