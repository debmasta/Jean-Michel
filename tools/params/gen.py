#!/usr/bin/env python3
"""FM-1 PAD's host parameters (automation in the DAW, the controller window): from the FM-1's own descriptors
(tools/params/dump.c, run on the host build) and the editor's page layout. Writes web/params.json, a list in a fixed
order: never reorder or remove entries (a DAW's automation points at their index); add at the end.
usage: gen.py dump.jsonl > web/params.json"""
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
T = {(r["s"], r["id"]): r for r in rows if r["k"] == "t"}
X = {r["id"]: r for r in rows if r["k"] == "x"}
TW = {r["n"]: r for r in rows if r["k"] == "tw"}
pe0 = next(r["v"] for r in rows if r["k"] == "pe0")
S0, S1 = 0, 1
PAGES = [("ENV", S0, [1, 2, 3, 4]), ("ENV DEST", S0, [5, 6, 7, 8]), ("ENV2", S0, [56, 57, 58, 59]), ("ENV2", S0, [95]),
         ("LFO", S0, [9, 10, 11, 12]), ("LFO DEST", S0, [13, 14, 15, 16]), ("MOD", S0, [45, 47, 48]), ("LFO2", S0, [53, 54, 55]), ("LFO3", S0, [92, 93, 94]),
         ("MODSEQ", S0, [83, 84, 85, 86]), ("EDIT", S0, list(range(pe0, pe0 + 8))), ("MACRO", S0, [96, 97, 98, 99]),
         ("LEVEL", S0, [0]), ("VOICE", S0, [37, 38, 41, 42, 43, 44, 39, 89, 91]),
         ("FX", S0, [33, 34, 35, 36]), ("FILTER", S0, [50, 46]), ("INSERT", S0, [78, 79, 80, 81, 82]),
         ("DLY", S1, [4, 5, 6, 7, 33]), ("REVERB", S1, [28, 8, 9, 30]), ("CHORUS", S1, [10, 11]),
         ("SEL", S0, [25, 26, 27, 49, 28, 51, 52]), ("ARP", S0, [17, 18, 19, 20, 21, 22, 23, 24]), ("SPICE", S0, [87, 88, 90])]
out = []
def add(name, kind, d, **k):
    e = {"name": name[:24], "kind": kind, "min": d["min"], "max": d["max"], "def": d["def"]}
    if d.get("names"): e["names"] = d["names"]
    e.update(k); out.append(e)
for title, s, ids in PAGES:
    for i, pid in enumerate(ids):
        d = T.get((s, pid))
        if not d or d["max"] <= d["min"] or d["label"] in ("-", ""): continue
        lab = f"{i + 1}" if title == "EDIT" else d["label"]
        add(f"S1 {title} {lab}" if title != "LEVEL" else "S1 LEVEL", "t", d, s=s, id=pid)
# the layer and the FM-1's own pages (JM_SET): ids from ui_synth.c
for xid, nm in [(0, "MODE"), (1, "SOUND"), (2, "SPLIT"), (3, "LEVEL"), (4, "OCT"), (5, "PAN"), (6, "FILT")]:
    add(f"S2 {nm}", "x", X[xid], id=xid)
for xid, pre in [(75, "S2 FX"), (76, "S2 FX"), (77, "S2 FX"), (78, "S2 FX"), (79, "S2 INS"), (80, "S2 INS"), (81, "S2 INS"), (82, "S2 INS"), (83, "S2 INS"),
                 (65, "VOICE4"), (66, "VOICE4"), (67, "VOICE4"), (68, "VOICE4"), (69, "VOICE5"), (70, "VOICE5"), (71, "VOICE5"),
                 (61, "RHYTHM"), (62, "RHYTHM"), (63, "RHYTHM"), (64, "RHYTHM"), (72, "RHYTHM"), (73, "RHYTHM"), (74, "RHYTHM"), (57, "BYTE")]:
    if xid in X and X[xid]["max"] > X[xid]["min"]: add(f"{pre} {X[xid]['label']}", "x", X[xid], id=xid)
# the FX: on / off, then their settings (the FX window)
HIDE = {14, 15, 19, 20, 21}
for n in sorted(TW):
    if n in HIDE: continue
    add(f"FX {TW[n]['label'].upper()}", "twon", {"min": 0, "max": 1, "def": 0, "names": ["OFF", "ON"]}, n=n)
for n in sorted(TW):
    if n in HIDE: continue
    for k, p in enumerate(TW[n]["p"]):
        if p["names"]: add(f"FX {TW[n]['label'].upper()} {p['label'].upper()}", "twp", {"min": 0, "max": len(p["names"]) - 1, "def": 0, "names": p["names"]}, n=n, k=k)
json.dump(out, sys.stdout, ensure_ascii=False, separators=(",", ":"))
print(f"{len(out)} parameters", file=sys.stderr)
