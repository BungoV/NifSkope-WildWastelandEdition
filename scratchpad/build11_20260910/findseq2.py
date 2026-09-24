import os, sys
BASE = r"E:\Tools\Fallout 4\DataUnpacked\Data\Meshes"
DIRS = ["Effects", "SetDressing", "Furniture", "Interiors", "Props", "Weapons", "Architecture"]
NEEDLE = b"NiControllerSequence"
hits = []
for d in DIRS:
    root = os.path.join(BASE, d)
    if not os.path.isdir(root):
        continue
    for dirpath, dirnames, filenames in os.walk(root):
        for fn in filenames:
            if not fn.lower().endswith(".nif"):
                continue
            p = os.path.join(dirpath, fn)
            try:
                with open(p, "rb") as f:
                    head = f.read(8192)
            except OSError:
                continue
            if NEEDLE in head:
                hits.append((os.path.getsize(p), p))
        if len(hits) >= 8:
            break
    if len(hits) >= 8:
        break
for sz, p in sorted(hits)[:8]:
    print(sz, p)
print("found", len(hits))
