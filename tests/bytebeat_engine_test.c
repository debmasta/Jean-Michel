/* SPDX-License-Identifier: GPL-3.0-only */
/* BYTE (eng_bytebeat.c): every preset bounded and audible, t follows the pitch, T follows the tempo with the transport stopped, voices free. argv[1]: a WAV of the presets */
#include "synth8_head.c"   /* synth8_test.c up to its main (run_tests.sh) */
static uint32_t nbad;
static void chk(const char *w, int ok) { printf("%-66s %s\n", w, ok ? "ok" : "FAIL"); nbad += !ok; }
int main(int argc, char **argv)
{
    const engine_t *en = ENGINES[ENGI_BYTE];
    uint32_t pi, b = 1, l = 1;
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0;
    uint32_t total = 0;
    uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x44,0xAC,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',0,0,0,0};
    host_tracks_init();
    for (pi = 0; pi < NPART; pi++)
        host_preset(&trk[pi], TRK_DEF[pi][0], TRK_DEF[pi][1]);
    clipper_proj_neutral();
    trk[1].p[P_LYMODE] = LY_OFF;
    if (f) fwrite(h, 1, 44, f);
    chk("BYTE is engine 23, named", !strcmp(en->name, "BYTE") && !strcmp(N_ENGNAME[ENGI_BYTE], "BYTE"));
    for (pi = 0; pi < en->npresets; pi++) {
        uint32_t i; double r = 0;
        quiet();
        host_preset(&trk[0], ENGI_BYTE, pi);
        trk[0].p[P_AMODE] = 0;
        trk_note_on(&trk[0], 60, 110);
        run(400);
        r = rms_l(0, ncap);
        printf("  %-16s rms %7.0f\n", en->presets[pi].name, r);
        if (!bounded()) b = 0;
        if (r < (en->presets[pi].name[1] != 'B' ? 150.0 : 300.0)) l = 0;    /* (BP: short hits, the measure is mostly silence) */
        if (f) {
            uint32_t bl;
            for (bl = 0; bl < 6u; bl++) {
                for (i = 0; i < ncap; i++) { int16_t v = (int16_t)(cl[i] > 32767 ? 32767 : cl[i] < -32768 ? -32768 : cl[i]); fwrite(&v, 2, 1, f); total++; }
                run(400);
            }
        }
        trk_note_off(&trk[0], 60);
    }
    chk("every preset bounded", b);
    chk("every preset audible", l);
    {
        voice_t *v; uint32_t a, c, T0; double Te;
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0; trk[0].p[P_VINT] = 0; song.playing = 0;
        trk_note_on(&trk[0], 60, 100); run(100); { uint32_t k; v = &trk[0].v[0]; for (k = 0; k < NVOICE; k++) if (trk[0].v[k].active && trk[0].v[k].note == 60) v = &trk[0].v[k]; } a = v->ph[1];
        Te = (double)song.g[G_BPM] / 60.0 * 4096.0 * 100.0 * CTL / 44100.0; T0 = v->ph[2]; printf("  C4: t = %u, T = %u (expected about %.0f) after 100 blocks\n", a, T0, Te);
        quiet(); host_preset(&trk[0], ENGI_BYTE, 0); trk[0].p[P_AMODE] = 0; trk[0].p[P_VINT] = 0;
        trk_note_on(&trk[0], 72, 100); run(100); { uint32_t k; c = 0; for (k = 0; k < NVOICE; k++) if (trk[0].v[k].active && trk[0].v[k].note == 72) c = trk[0].v[k].ph[1]; }
        chk("t runs near 8000 / s at C4", a > 540u && a < 620u);
        chk("an octave up doubles t", c > a * 19u / 10u && c < a * 21u / 10u);
        chk("T runs at the tempo, transport stopped", T0 > Te * 0.9 && T0 < Te * 1.1);
        quiet();
    }
    if (f) { uint32_t n = total * 2u, r = n + 36u; fseek(f, 4, SEEK_SET); fwrite(&r, 4, 1, f); fseek(f, 40, SEEK_SET); fwrite(&n, 4, 1, f); fclose(f); }
    printf("BYTE: %s\n", nbad ? "FAIL" : "ok");
    return nbad != 0;
}
