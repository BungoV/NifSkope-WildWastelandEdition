import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
import sheetlib as S
from reencode import bc3_alpha_roundtrip

CARDS = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/cardortho_20260910/cards"
TREES = ["0003a28b", "0004a074", "00038599"]
FLOOR, TEST = 16, 128

def enc(a, base):
    a = a.astype(np.int32); o = np.zeros_like(a); m = a >= FLOOR
    o[m] = base + ((a[m] - FLOOR) * (255 - base) + (255 - FLOOR)//2) // (255 - FLOOR)
    return np.clip(o, 0, 255).astype(np.uint8)

print("| base | tree | BC3 disagreeing texels | fraction levels kept |")
print("|---|---|---|---|")
for base in (128, 144, 160, 176, 192):
    for ident in TREES:
        a = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        ap = enc(a, base)
        bc = bc3_alpha_roundtrip(ap)
        s0 = a >= FLOOR
        print("| %d | %s | %d | %d |" % (base, ident, int((s0 != (bc >= TEST)).sum()), 256 - base))
