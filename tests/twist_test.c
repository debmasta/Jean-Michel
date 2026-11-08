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
    printf("TWISTS: %s\n", nbad ? "FAIL" : "ok");
    return nbad != 0;
}
