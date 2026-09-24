#!/usr/bin/env python3
"""anchors.py -- ww-contract-provenance step 3 for spec_water.md.

Parses the provenance footer's rows, pulls the FIRST backticked span out of the
anchor column, finds it in the current source, and rewrites the line number.
EXACT and UNIQUE or no rewrite: MISSING / AMBIGUOUS are printed and the number
is left alone, because a stale number announces itself and a plausible wrong one
never does.  Rows carrying a comma in the line column are listed for hand work.
"""

import os
import re
import sys

REPO = r'E:\Projects\NifskopeWildWastelandEdition'
DOC = os.path.join(REPO, 'scratchpad', 'specs_20260909', 'spec_water.md')
FILES = {
    'lodtfile.cpp': 'src/lodtfile.cpp',
    'lodtfile.h': 'src/lodtfile.h',
    'lodl_open_authority.py': 'tests/spells/lodl_open_authority.py',
    'LODGEN_BTD_FORMAT.md': 'docs/LODGEN_BTD_FORMAT.md',
}

src = {k: open(os.path.join(REPO, v), 'r', encoding='utf-8', errors='replace').read().splitlines()
       for k, v in FILES.items()}


def find(stem, anchor):
    hits = [i + 1 for i, ln in enumerate(src[stem]) if anchor in ln]
    if not hits:
        return None, 'MISSING'
    if len(hits) > 1:
        return None, 'AMBIGUOUS(%s)' % ','.join(map(str, hits[:5]))
    return hits[0], 'ok'


text = open(DOC, 'r', encoding='utf-8', newline='').read()
rows = re.findall(r'^\| (.+?) \| ([^|]+?) \| (.+?) \|$', text, re.M)
moved = same = bad = manual = 0
out = text
for claim, loc, anchor in rows:
    m = re.match(r'^\s*(\S+?)\s+([\d,\- ]+)\s*$', loc)
    if not m:
        continue
    stem, nums = m.group(1), m.group(2).strip()
    if stem not in src:
        continue
    if ',' in nums:
        print('MANUAL  %-60s %s %s' % (claim[:60], stem, nums))
        manual += 1
        continue
    a = re.findall(r'`([^`]+)`', anchor)
    if not a:
        print('NOANCHOR %s' % claim[:60])
        bad += 1
        continue
    key = a[0].replace('\\|', '|')
    line, status = find(stem, key)
    if line is None:
        print('%-10s %-58s %s' % (status, claim[:58], key[:50]))
        bad += 1
        continue
    span = nums.split('-')
    new = str(line) if len(span) == 1 else '%d-%d' % (line, line + int(span[1]) - int(span[0]))
    if new == nums:
        same += 1
        continue
    old_row = '| %s | %s %s | %s |' % (claim, stem, nums, anchor)
    new_row = '| %s | %s %s | %s |' % (claim, stem, new, anchor)
    if old_row not in out:
        print('ROWMISS %s' % claim[:60])
        bad += 1
        continue
    out = out.replace(old_row, new_row)
    print('moved   %-58s %s -> %s' % (claim[:58], nums, new))
    moved += 1

if out != text:
    with open(DOC, 'w', encoding='utf-8', newline='') as f:
        f.write(out)
print('\n%d rows moved, %d unchanged, %d anchors not found, %d for hand work'
      % (moved, same, bad, manual))
