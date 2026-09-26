import math, sys, pickle, re
from collections import Counter
sys.path.insert(0, r'E:\Projects\NifskopeWWE-cell1\tools\cellexport')
import esm_cells
S = r'C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\cell1_20260926'
p = esm_cells.load_cached(r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm', 'Commonwealth', S + r'\fo4.pkl')
pat = re.compile(sys.argv[1], re.I)
hits = Counter()
for r in p.refs:
    b = p.bases.get(r['base'])
    if not b:
        continue
    if pat.search(b['edid']) or pat.search(b['model']):
        cx = math.floor(r['pos'][0] / 4096); cy = math.floor(r['pos'][1] / 4096)
        if -8 <= cx <= 4 and -12 <= cy <= 0:
            hits[(cx, cy, b['edid'], b['model'])] += 1
for k, v in sorted(hits.items())[:60]:
    print(v, k)
