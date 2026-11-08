/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP boot splash: the logo (tools/gen_logo.py -> sloop_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#include "sloop_logo.h"
static void sloop_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < SLOOP_SPLASH_H; r0 += 120u) {
        uint32_t rows = SLOOP_SPLASH_H - r0 > 120u ? 120u : SLOOP_SPLASH_H - r0;
        uint32_t lo = r0 * SLOOP_SPLASH_W, hi = (r0 + rows) * SLOOP_SPLASH_W, pos = 0, i;
        cv_begin(SLOOP_SPLASH_W, rows, C_BLACK);
        for (i = 0; i < sizeof SLOOP_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (SLOOP_SPLASH_RLE[i] >> 4) + 1u, c = SLOOP_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(SLOOP_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - SLOOP_SPLASH_W) / 2u, y0 + r0);
    }
}

#if CLIPPER_SYNTH
/* the boot screen: the cat of the notes, large, its fur t*(42&t>>10) sliding through the colours */
static void jm_cat(uint32_t frame)
{
    enum { S = 12, OX = (240 - 16 * S) / 2, OY = 18 };
    uint32_t band;
    for (band = 0; band < 2u; band++) {
        uint32_t y0 = band ? 10u : 0u, y1 = band ? CAT_H : 10u, x, y;
        cv_begin(240, (y1 - y0) * S, C_BLACK);
        for (y = y0; y < y1; y++)
            for (x = 0; x < CAT_W; x++) {
                char c = CAT_ART[y][x];
                uint16_t col;
                uint32_t t = 9000u + y * 256u + x * 16u;
                if (c == '.')
                    continue;
                col = c == 'o' ? C_WHITE : c == 'p' ? C_BLACK : bb_hue((t * (42u & (t >> 10))) + y * 3u + frame * 9u, 255u);
                cv_rect(OX + (int32_t)x * S, (int32_t)(y - y0) * S, S, S, col);
            }
        cv_blit(0, OY + y0 * S);
    }
}
static void sloop_splash(void)
{
    uint32_t f;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    jm_cat(0);
    cv_begin(240, 66, C_BLACK);
    cv_text(120 - text_w(&FONT_L, "JEAN-MICHEL") / 2, 0, &FONT_L, "JEAN-MICHEL", C_WHITE);
    cv_text(120 - text_w(&FONT_S, "t*(42&t>>10)") / 2, 30, &FONT_S, "t*(42&t>>10)", RGB(255, 60, 60));
    cv_text(120 - text_w(&FONT_S, "from clipper, felucca") / 2, 48, &FONT_S, "from clipper, felucca", RGB(96, 96, 104));
    cv_blit(0, 170);
    for (f = 1; f < 20u; f++) {
        fm1_delay_ms(40);
        jm_cat(f);
    }
}
#else
/* the boot screen: the logo, the version, where it comes from */
static void sloop_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    sloop_logo_draw(10);
    const char *v = FELUCCA_VERSION;
    char vb[24];
    if (v[0] == 'C' && v[1] == 'L' && v[7] == ' ')
        v += 8;                                         /* "CLIPPER 0.1" -> "0.1" under the wordmark */
    {   /* (1.0) "1.0 EXP" -> "1.0 EXPERIMENTAL": the boot screen says it whole */
        uint32_t n = 0;
        while (v[n] && n < 12u)
            vb[n] = v[n], n++;
        vb[n] = 0;
        if (n >= 4u && vb[n - 4] == ' ' && vb[n - 3] == 'E' && vb[n - 2] == 'X' && vb[n - 1] == 'P' && !v[n])
            str_cpy(vb + n, "ERIMENTAL", 10);
        v = vb;
    }
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, v) / 2, 0, &FONT_S, v, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "based on felucca") / 2, 18, &FONT_S, "based on felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
#endif
