"""Recovery helper (2026-09-24): newest surviving copy of each emptied ledger. A copy qualifies when it contains 3 stable
lines taken from the middle of the committed (09-09) file. Read-only. Prints newest 4 per ledger + tonight probes."""
import os, time, subprocess
G = r"E:\Tools\GIT\bin\git.exe"
R = r"E:\Projects\NifskopeWildWastelandEdition"
roots = [os.path.expanduser(r"~\.claude\file-history"), R + r"\scratchpad", r"C:\Users\bungo\AppData\Local\Temp\claude"]
sigs = {}
for f in ("HANDOFF.md", "WW_CHANGES.md", "MISTAKES.md"):
    lines = [l for l in subprocess.run([G, "-C", R, "show", "HEAD:" + f], capture_output=True).stdout.split(b"\n")
             if len(l.strip()) > 60]
    n = len(lines); sigs[f] = [lines[n // 4].strip(), lines[n // 2].strip(), lines[3 * n // 4].strip()]
probes = [b"PBRR4", b"PBRR3", b"PBRR1", b"LIGHTANGLES1", b"TODDSTREAT1", b"Todd's treat", b"2026-09-2"]
hits = {f: [] for f in sigs}
for root in roots:
    for dp, dn, fs in os.walk(root):
        for nme in fs:
            p = os.path.join(dp, nme)
            try:
                s = os.path.getsize(p)
                if s < 60_000 or s > 20_000_000 or nme.endswith((".jsonl", ".png", ".bin", ".esm", ".exe", ".dll")): continue
                b = open(p, "rb").read()
            except OSError: continue
            for f, sg in sigs.items():
                if all(x in b for x in sg):
                    hits[f].append((os.path.getmtime(p), len(b), p, [x.decode() for x in probes if x in b]))
for f, L in hits.items():
    L.sort(reverse=True)
    print(f"== {f}: {len(L)} copies")
    for m, s, p, pr in L[:4]:
        print(" ", time.strftime("%m-%d %H:%M", time.localtime(m)), s, p, pr)
