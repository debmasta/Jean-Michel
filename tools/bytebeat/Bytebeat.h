// Bytebeat: a formula of t (and the variables a-h) gives an 8-bit sample for every step of t.
// Our own compiler (C expressions on 32-bit signed integers, C precedence) into a small stack program.
// Where C is undefined we define it: overflow wraps, shift counts are & 31, x / 0 and x % 0 are 0.
// Also: "text"[i] reads a character code (i wraps around the text: melodies loop), 'A' is a character code,
// (int) casts are accepted and ignored, 96e3 is 96000. T is t locked to the tempo (T>>10 = a 16th, T>>12 = a beat, T>>14 = a bar), n is the note played.
#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace m2bb {

enum Op : uint8_t { K, T, V, NEG, INV, LNOT, IDX, MUL, DIV, MOD, ADD, SUB, SHL, SHR, LT, LE, GT, GE, EQ, NE, AND, XOR, OR, LAND, LOR, SEL };

struct Prog {
    static constexpr int MAXOP = 256, MAXLEN = 256, NV = 10;   // variables: a-h, T, n
    uint8_t op[MAXOP] {}; int32_t arg[MAXOP] {}; int n = 0; bool ok = false;
    int errPos = -1; const char* err = "";
    char str[MAXLEN] {}; int nstr = 0;   // the texts of "..."[i]

    int32_t run (int32_t t, const int32_t* var) const {
        int32_t s[64]; int sp = 0;
        for (int i = 0; i < n; ++i) {
            const uint8_t o = op[i];
            if (o == K) { s[sp++] = arg[i]; continue; }
            if (o == T) { s[sp++] = t; continue; }
            if (o == V) { s[sp++] = var[arg[i] < NV ? arg[i] : 0]; continue; }
            if (o == IDX) { const int off = arg[i] & 0xffff, len = arg[i] >> 16; int32_t k = s[sp - 1] % len; if (k < 0) k += len; s[sp - 1] = (unsigned char) str[off + k]; continue; }
            if (o == NEG) { s[sp - 1] = (int32_t) (0u - (uint32_t) s[sp - 1]); continue; }
            if (o == INV) { s[sp - 1] = ~s[sp - 1]; continue; }
            if (o == LNOT) { s[sp - 1] = ! s[sp - 1]; continue; }
            if (o == SEL) { const int32_t b = s[--sp], a = s[--sp], c = s[--sp]; s[sp++] = c ? a : b; continue; }
            const int32_t b = s[--sp], a = s[--sp]; int32_t r = 0;
            switch (o) {
                case MUL: r = (int32_t) ((uint32_t) a * (uint32_t) b); break;
                case DIV: r = b == 0 || (a == INT32_MIN && b == -1) ? 0 : a / b; break;
                case MOD: r = b == 0 || (a == INT32_MIN && b == -1) ? 0 : a % b; break;
                case ADD: r = (int32_t) ((uint32_t) a + (uint32_t) b); break;
                case SUB: r = (int32_t) ((uint32_t) a - (uint32_t) b); break;
                case SHL: r = (int32_t) ((uint32_t) a << (b & 31)); break;
                case SHR: r = a >> (b & 31); break;
                case LT: r = a < b; break; case LE: r = a <= b; break; case GT: r = a > b; break; case GE: r = a >= b; break;
                case EQ: r = a == b; break; case NE: r = a != b; break;
                case AND: r = a & b; break; case XOR: r = a ^ b; break; case OR: r = a | b; break;
                case LAND: r = a && b; break; case LOR: r = a || b; break;
                default: break;
            }
            s[sp++] = r;
        }
        return sp > 0 ? s[sp - 1] : 0;
    }
};

class Compiler {
public:
    static Prog compile (const char* src) { Compiler c (src); Prog p; c.P = &p; c.skip(); if (! *c.s) { c.fail ("formule vide"); return p; }
        c.ternary (0); c.skip(); if (! c.bad && *c.s) c.fail ("en trop ici");
        if (! c.bad && c.depth > 60) c.fail ("trop imbriquée");
        p.ok = ! c.bad && p.n > 0; return p; }
private:
    explicit Compiler (const char* src) : s0 (src), s (src) {}
    const char* s0; const char* s; Prog* P = nullptr; bool bad = false; int depth = 0, cur = 0;
    void fail (const char* m) { if (bad) return; bad = true; P->err = m; P->errPos = (int) (s - s0); P->n = 0; }
    void emit (uint8_t o, int32_t a = 0) { if (bad) return; if (P->n >= Prog::MAXOP) { fail ("trop longue"); return; } P->op[P->n] = o; P->arg[P->n] = a; P->n++;
        // stack depth: pushes +1, binaries -1, select -2
        cur += (o <= V) ? 1 : (o <= IDX) ? 0 : (o == SEL) ? -2 : -1; if (cur > depth) depth = cur; }
    // (int), (unsigned char), (int32_t)…: casts, ignored (everything is already an integer)
    bool cast() { const char* b = s; bool any = false;
        static const char* W[] = { "unsigned", "signed", "char", "short", "long", "int", "uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t" };
        while (true) { skip(); bool hit = false; for (auto* w : W) { const size_t n = std::strlen (w); if (std::strncmp (s, w, n) == 0 && ! ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_')) { s += n; hit = any = true; break; } } if (! hit) break; }
        skip(); if (any && *s == ')') { ++s; return true; } s = b; return false; }
    void skip() { while (*s == ' ' || *s == '\t') ++s; }
    bool eat (const char* t) { skip(); const size_t n = std::strlen (t); if (std::strncmp (s, t, n) == 0) { s += n; return true; } return false; }
    // binary operators by precedence (lowest first); longer spellings checked before their prefixes
    struct B { const char* t; uint8_t o; int p; };
    const B* binop (int minP) {
        static const B ops[] = { { "||", LOR, 1 }, { "&&", LAND, 2 }, { "|", OR, 3 }, { "^", XOR, 4 }, { "&", AND, 5 }, { "==", EQ, 6 }, { "!=", NE, 6 },
            { "<<", SHL, 8 }, { ">>", SHR, 8 }, { "<=", LE, 7 }, { ">=", GE, 7 }, { "<", LT, 7 }, { ">", GT, 7 }, { "+", ADD, 9 }, { "-", SUB, 9 },
            { "*", MUL, 10 }, { "/", DIV, 10 }, { "%", MOD, 10 } };
        skip();
        for (const auto& b : ops) { const size_t n = std::strlen (b.t); if (std::strncmp (s, b.t, n) != 0) continue;
            if (n == 1 && (b.t[0] == '|' || b.t[0] == '&') && s[1] == b.t[0]) continue;   // | is not ||, & is not &&
            if (n == 1 && (b.t[0] == '<' || b.t[0] == '>') && (s[1] == b.t[0])) continue; // < is not <<
            if (b.p < minP) return nullptr; return &b; }
        return nullptr;
    }
    void ternary (int lvl) {
        if (lvl > 40) { fail ("trop imbriquée"); return; }
        binary (1, lvl); skip();
        if (*s == '?') { ++s; ternary (lvl + 1); if (! eat (":")) { fail ("il manque « : »"); return; } ternary (lvl + 1); emit (SEL); }
    }
    void binary (int minP, int lvl) {
        unary (lvl);
        while (! bad) { const B* b = binop (minP); if (! b) break; s += std::strlen (b->t); binary (b->p + 1, lvl + 1); emit (b->o); }
    }
    void unary (int lvl) {
        if (bad) return; if (lvl > 40) { fail ("trop imbriquée"); return; }
        skip(); const char c = *s;
        if (c == '-') { ++s; unary (lvl + 1); emit (NEG); return; }
        if (c == '+') { ++s; unary (lvl + 1); return; }
        if (c == '~') { ++s; unary (lvl + 1); emit (INV); return; }
        if (c == '!') { ++s; unary (lvl + 1); emit (LNOT); return; }
        if (c == '(') { ++s; if (cast()) { unary (lvl + 1); return; } ternary (lvl + 1); if (! eat (")")) fail ("il manque « ) »"); return; }
        if (c == '"') { ++s; const int off = P->nstr; while (*s && *s != '"') { char ch = *s++; if (ch == '\\' && *s) { ch = *s++; if (ch == 'n') ch = '\n'; else if (ch == 't') ch = '\t'; else if (ch == '0') ch = 0; }
                if (P->nstr >= Prog::MAXLEN) { fail ("texte trop long"); return; } P->str[P->nstr++] = ch; }
            if (*s != '"') { fail ("il manque le \" de fin"); return; } ++s; const int len = P->nstr - off; if (len == 0) { fail ("texte vide"); return; }
            if (! eat ("[")) { fail ("un texte doit être suivi de [ ]"); return; } ternary (lvl + 1); if (! eat ("]")) { fail ("il manque « ] »"); return; }
            emit (IDX, off | (len << 16)); return; }
        if (c == '\'') { ++s; char ch = *s ? *s++ : 0; if (ch == '\\' && *s) ch = *s++; if (*s != '\'') { fail ("il manque le ' de fin"); return; } ++s; emit (K, (unsigned char) ch); return; }
        if (c >= '0' && c <= '9') {
            uint32_t v = 0;
            if (c == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; const char* st = s; while (true) { const char h = *s; int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1; if (d < 0) break; v = v * 16 + (uint32_t) d; ++s; } if (s == st) { fail ("nombre hexa vide"); return; } }
            else { while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t) (*s - '0'); ++s; }
                if (*s == '.') { fail ("pas de nombres à virgule"); return; }
                if ((*s == 'e' || *s == 'E') && s[1] >= '0' && s[1] <= '9') { ++s; int e = 0; while (*s >= '0' && *s <= '9') { e = std::min (12, e * 10 + (*s - '0')); ++s; } while (e-- > 0) v *= 10; } }
            emit (K, (int32_t) v); return; }
        if (c == 't') { ++s; emit (T); return; }
        if (c == 'T') { ++s; emit (V, 8); return; }
        if (c == 'n') { ++s; emit (V, 9); return; }
        if (c >= 'a' && c <= 'h') { ++s; emit (V, c - 'a'); return; }
        fail (c ? "caractère inattendu" : "la formule s'arrête trop tôt");
    }
};

inline Prog compile (const char* src) { return Compiler::compile (src); }

} // namespace m2bb
