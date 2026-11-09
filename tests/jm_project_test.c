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
    printf("JM PROJECT: %s\n", fails ? "FAILED" : "ok");
    return fails != 0;
}
