"""Lane PRTP1 light gate: NifSkope's WW_CELL_LIGHTS dump for one interior vs light_census.py's
independent walk (fo4_lights.tsv). Every row must match: ref, base, base EDID, position, radius, XRDS,
XLIG present, initially disabled. Prints one verdict line.
usage: python light_gate.py <census.tsv> <cellEdid> <dump.tsv> [--drop-one]
  --drop-one  the refuter: drop the dump's first row; the gate must then FAIL."""
import sys


def rows(path, key=None):
    out = {}
    with open(path, encoding='utf8') as fh:
        head = fh.readline().rstrip('\n').split('\t')
        for line in fh:
            c = dict(zip(head, line.rstrip('\n').split('\t')))
            if key is not None and c.get('cell') != key:
                continue
            out[c['ref']] = c
    return out


def same(a, b, col):
    if col in ('x', 'y', 'z', 'xrds'):
        if a == '' or b == '':
            return a == b
        # both sides print one decimal and round a tie (-833.25) differently: one printed digit apart is equal
        return abs(float(a) - float(b)) <= 0.1001
    return a == b


census_path, edid, dump_path = sys.argv[1:4]
want = rows(census_path, 'int:' + edid)
got = rows(dump_path)
if '--drop-one' in sys.argv and got:
    got.pop(sorted(got)[0])
cols = ['base', 'baseEdid', 'x', 'y', 'z', 'radius', 'xrds', 'xlig', 'initDisabled']
missing = sorted(set(want) - set(got))
extra = sorted(set(got) - set(want))
bad = [(r, c, want[r][c], got[r][c]) for r in sorted(set(want) & set(got)) for c in cols if not same(want[r][c], got[r][c], c)]
ok = not missing and not extra and not bad and len(want) > 0
print(f'{edid}: census {len(want)} dump {len(got)} missing {len(missing)} extra {len(extra)} mismatched {len(bad)} '
      f'{"PASS" if ok else "FAIL"}' + (f'  first: {(missing or extra or bad)[0]}' if not ok and (missing or extra or bad) else ''))
sys.exit(0 if ok else 1)
