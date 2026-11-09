/* SPDX-License-Identifier: GPL-3.0-only */
/* The bytebeat stack machine: METAL KAP 2's m2bb::Prog::run in C (tests/bytebeat_test.cpp compares them) */
#pragma once
enum { BB_K, BB_T, BB_V, BB_NEG, BB_INV, BB_LNOT, BB_IDX, BB_MUL, BB_DIV, BB_MOD, BB_ADD, BB_SUB, BB_SHL, BB_SHR, BB_LT, BB_LE,
       BB_GT, BB_GE, BB_EQ, BB_NE, BB_AND, BB_XOR, BB_OR, BB_LAND, BB_LOR, BB_SEL };
#define BB_STACK 32
#define BB_MIN ((int32_t)0x80000000u)

static int32_t bb_run(uint32_t prog, int32_t t, const int32_t *var)
{
    const uint8_t *op = BB_OP + BB_PROG[prog].off;
    const int32_t *arg = BB_ARG + BB_PROG[prog].off;
    uint32_t i, n = BB_PROG[prog].n;
    int32_t s[BB_STACK];
    int sp = 0;
    for (i = 0; i < n; i++) {
        uint32_t o = op[i];
        int32_t a, b, r = 0;
        if (o >= 64u) {                                  /* a binary op with its right operand built in (gen_bytebeat.cpp): one dispatch instead of two */
            b = o < 128u ? arg[i] : o < 192u ? t : var[arg[i]];
            o &= 63u;
            a = s[--sp];
            goto bin;
        }
        switch (o) {
        case BB_K: s[sp++] = arg[i]; continue;
        case BB_T: s[sp++] = t; continue;
        case BB_V: s[sp++] = var[arg[i]]; continue;
        case BB_IDX: {
            int32_t off = arg[i] & 0xFFFF, len = arg[i] >> 16, k = s[sp - 1] % len;
            if (k < 0) k += len;
            s[sp - 1] = BB_STR[off + k];
            continue;
        }
        case BB_NEG: s[sp - 1] = (int32_t)(0u - (uint32_t)s[sp - 1]); continue;
        case BB_INV: s[sp - 1] = ~s[sp - 1]; continue;
        case BB_LNOT: s[sp - 1] = !s[sp - 1]; continue;
        case BB_SEL: b = s[--sp]; a = s[--sp]; sp--; s[sp] = s[sp] ? a : b; sp++; continue;
        default: break;
        }
        b = s[--sp];
        a = s[--sp];
    bin:
        switch (o) {
        case BB_MUL: r = (int32_t)((uint32_t)a * (uint32_t)b); break;
        case BB_DIV: r = b == 0 || (a == BB_MIN && b == -1) ? 0 : a / b; break;
        case BB_MOD: r = b == 0 || (a == BB_MIN && b == -1) ? 0 : a % b; break;
        case BB_ADD: r = (int32_t)((uint32_t)a + (uint32_t)b); break;
        case BB_SUB: r = (int32_t)((uint32_t)a - (uint32_t)b); break;
        case BB_SHL: r = (int32_t)((uint32_t)a << (b & 31)); break;
        case BB_SHR: r = a >> (b & 31); break;
        case BB_LT: r = a < b; break;
        case BB_LE: r = a <= b; break;
        case BB_GT: r = a > b; break;
        case BB_GE: r = a >= b; break;
        case BB_EQ: r = a == b; break;
        case BB_NE: r = a != b; break;
        case BB_AND: r = a & b; break;
        case BB_XOR: r = a ^ b; break;
        case BB_OR: r = a | b; break;
        case BB_LAND: r = a && b; break;
        case BB_LOR: r = a || b; break;
        default: break;
        }
        s[sp++] = r;
    }
    return sp > 0 ? s[sp - 1] : 0;
}
/* the formula, then the two knobs common to all: C an xor ramp, D an offset stepped at the tempo; at noon, nothing to do */
static int32_t bb_eval(uint32_t prog, int32_t t, const int32_t *var)
{
    int32_t r = bb_run(prog, t, var);
    if (var[2] != 128 || var[3] != 128)
        r ^= ((int32_t)((uint32_t)(var[2] - 128) * (uint32_t)t) >> 8) + (var[3] - 128) * ((var[8] >> 10) & 7);
    return r;
}
