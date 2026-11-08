// SPDX-License-Identifier: GPL-3.0-only
// BYTE: the firmware's stack machine (bb_vm.h) against METAL KAP 2's (Bytebeat.h), every factory formula, sample for sample.
#include <cstdint>
#include <cstdio>
#include "../tools/bytebeat/Bytebeat.h"
#define BB_WITH_SRC
#include "../firmware/src/bb_bank.h"
#include "../firmware/src/bb_vm.h"
int main() {
    uint32_t x = 0x9E3779B9u; int bad = 0; uint64_t n = 0;
    for (uint32_t f = 0; f < BB_N; f++) {
        m2bb::Prog p = m2bb::compile(BB_SRC[f]);
        if (!p.ok) { printf("%s: does not compile\n", BB_SRC[f]); return 1; }
        for (int i = 0; i < 200000; i++) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            int32_t v[10] = {(int32_t)(x & 255), (int32_t)(x >> 8 & 255), (int32_t)(x >> 16 & 255), (int32_t)(x >> 24), 0, 0, 0, 0, (int32_t)(x * 7u), (int32_t)(x & 127)};
            int32_t t = i < 100000 ? i : (int32_t)x;
            if (p.run(t, v) != bb_run(f, t, v) && bad++ < 5) printf("%s: t=%d differs\n", BB_SRC[f], t);
            n++;
        }
    }
    printf("bytebeat VM: %llu evaluations of %u formulas, %d differ\n", (unsigned long long)n, BB_N, bad);
    return bad != 0;
}
