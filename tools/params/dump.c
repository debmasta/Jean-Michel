/* the FM-1's parameters as the editor sees them (DESC, JM_DESC, the FX window), for FM-1 PAD's host parameters: tools/params/gen.py */
#define main editor_fuzz_host_main
#include "editor_host.c"
#undef main
static void q(const char *s) { putchar('"'); for (; s && *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); putchar(*s); } putchar('"'); }
static void d(const char *k, int s, unsigned id, const param_desc_t *x)
{
    int i;
    printf("{\"k\":\"%s\",\"s\":%d,\"id\":%u,\"fmt\":%u,\"min\":%d,\"max\":%d,\"def\":%d,\"label\":", k, s, id, x->fmt, x->min, x->max, x->def);
    q(x->label); printf(",\"names\":[");
    if (x->fmt == F_ENUM && x->names) for (i = 0; i <= x->max - x->min && i < 64; i++) { if (i) putchar(','); q(x->names[i]); }
    printf("]}\n");
}
int main(void)
{
    unsigned id, n, k;
    int16_t *vp;
    harness_init();
    printf("{\"k\":\"pe0\",\"v\":%d}\n", P_E0);
    for (id = 0; id < P_COUNT; id++) { const param_desc_t *x = ed_desc(0, id, &vp); if (x && x->label) d("t", 0, id, x); }
    for (id = 0; id < G_COUNT; id++) { const param_desc_t *x = ed_desc(1, id, &vp); if (x && x->label) d("t", 1, id, x); }
    for (id = 0; id < 128; id++) { const param_desc_t *x = syn_desc(id, &vp); if (x && x->label && x->label[0]) d("x", 0, id, x); }
    for (n = 0; n < TW_N; n++) {
        printf("{\"k\":\"tw\",\"n\":%u,\"label\":", n); q(TW_LAB[n]); printf(",\"p\":[");
        for (k = 0; k < TW_NP; k++) { int j; const tw_par_t *p = &TW_PAR[n][k]; if (k) putchar(',');
            printf("{\"label\":"); q(p->n ? p->lab : ""); printf(",\"names\":[");
            for (j = 0; j < p->n; j++) { if (j) putchar(','); q(p->names[j]); } printf("]}"); }
        printf("]}\n");
    }
    return 0;
}
