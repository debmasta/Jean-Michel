# The biggest symbols of the image (objdump -t on stdin): what takes the room in flash
import re, sys
rows = []
for l in sys.stdin:
    m = re.match(r'^([0-9a-f]+)\s.*?\s(\.\S+)\s+([0-9a-f]+)\s+(\S+)\s*$', l)
    if m and m.group(2) in ('.text', '.rodata', '.data', '.ram_text', '.c1_text') or (m and m.group(2).startswith(('.text', '.rodata'))):
        n = int(m.group(3), 16)
        if n:
            rows.append((n, m.group(4), m.group(2)))
rows.sort(reverse=True)
print('SYMTOTAL', sum(r[0] for r in rows))
for n, name, sec in rows[:400]:
    print('SYM', n, name, sec)
