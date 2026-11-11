/* The BYTE memories and VOICE 4 are kept with a project: in RAM, in flash (the side record), and a project without them starts empty */
#define main editor_fuzz_host_main
#include "editor_host.c"
#undef main
static int fails;
static void chk(int ok, const char *s) { printf("%-60s %s\n", s, ok ? "ok" : "FAIL"); fails += !ok; }
static void setv(void) { bb_have = 5; bb_snap[2][1] = 99; vt_stack[0] = 2; vt_drift[1] = 44; vt_flam[2] = 7; }
static void wipe(void) { bb_have = 0; bb_snap[2][1] = 1; vt_stack[0] = 0; vt_drift[1] = 0; vt_flam[2] = 0; }
static int same(void) { return bb_have == 5u && bb_snap[2][1] == 99 && vt_stack[0] == 2u && vt_drift[1] == 44u && vt_flam[2] == 7u; }
int main(void)
{
    harness_init(); flash_ok = 1;
    setv(); project_save(1); wipe(); transport_req = 0; project_load(1);
    chk(same(), "from RAM: memories and VOICE 4 back");
    wipe(); transport_req = 0;
    proj_slot[1].magic ^= 0xFFu; memset(&proj_jms[1], 0, sizeof proj_jms[1]);
    project_load(1);
    chk(same(), "from flash: back too");
    proj_jms[1].magic = 0; transport_req = 0; project_load(1);
    chk(bb_have == 0u && vt_stack[0] == 0u, "a project without them: empty");
    {   /* a formula over SysEx (t*3): installed, in flash, back after a restart */
        static uint8_t f[64] = {0x7D, 0x4A, 0x4D, 0x42, 2, 3, 0};
        uint8_t *q = f + 7; int32_t var[10] = {128, 128, 128, 128};
        q[0] = BB_T; q += 6; q[0] = BB_K; q[1] = 3; q += 6; q[0] = BB_MUL;
        song.playing = 0; transport_req = 0;
        chk(bbu_rx(f, 7u + 18u), "SysEx: accepted");
        chk(bb_run(BB_N + 2u, 5, var) == 15, "SysEx: it plays");
        bbu_save(); memset(bb_user, 0, sizeof bb_user); bbu_loaded = 0; bbu_load();
        chk(bb_run(BB_N + 2u, 5, var) == 15, "flash: back after a restart");
        tw_init(); tw_p[TW_FOOT][0] = 3; tw_p[TW_POLY][0] = 4; bbu_save();
        tw_p[TW_FOOT][0] = 0; tw_p[TW_POLY][0] = 0; bbu_loaded = 0; bbu_load();
        chk(tw_p[TW_FOOT][0] == 3u && tw_p[TW_POLY][0] == 4u, "flash: the FX settings back");
        chk(bb_run(BB_N + 2u, 5, var) == 15, "flash: the formula still there");
        vt_acc[0] = 77; project_save(1); vt_acc[0] = 0; transport_req = 0; project_load(1); chk(vt_acc[0] == 77u, "project: VOICE 5 back");
        f[5] = 80; chk(!bbu_rx(f, 25u), "SysEx: wrong length refused");
    }
    {   /* the editor's JM commands: pages, a layer sound set by number, an FX tile on and off */
        static uint8_t f[8] = {0x7D, 0x46, 0x4C, 100};
        uint32_t tot, before;
        ed_handle(f, 4);
        chk(ed_out[4] == 100u && ed_out[5] >= 8u, "JM_PAGES: our pages listed");
        { uint32_t q = 6, k; for (k = 0; k < ed_out[5]; k++) { printf("  [%s]", (const char *)&ed_out[q]); q += (uint32_t)strlen((const char *)&ed_out[q]) + 5u; } printf("\n"); }
        trk[1].p[P_LYMODE] = LY_LAYER;
        before = layer_pos(&tot);
        f[3] = 103; f[4] = X_LYSOUND; f[5] = (uint8_t)((before + 5u + 8192u) & 127u); f[6] = (uint8_t)((before + 5u + 8192u) >> 7);
        ed_handle(f, 7);
        chk(layer_pos(&tot) == before + 5u, "JM_SET: the layer's sound changes");
        printf("  layer sound now: %s\n", (const char *)&ed_out[8]);
        f[3] = 104; f[4] = 1; f[5] = TW_POLY; ed_handle(f, 6);
        chk(tw_is(TW_POLY), "JM_FX: POLY on");
        f[4] = 2; ed_handle(f, 6);
        chk(!tw_is(TW_POLY), "JM_FX: POLY off");
        f[3] = 101; f[4] = X_VSTACK; ed_handle(f, 5);
        chk(!strcmp((const char *)&ed_out[13], "STACK"), "JM_DESC: VOICE 4 STACK");
    }
    printf("JM PROJECT: %s\n", fails ? "FAILED" : "ok");
    return fails != 0;
}
