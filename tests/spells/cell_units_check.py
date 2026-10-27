#!/usr/bin/env python3
"""The cell view's texture units: one table, no overlap (lane VOLAIR1, 2026-10-15).

Every sampler the cell surface programs and the air pass declare has its unit in src/gl/cellunits.h. This gate:
  T  the table: every unit distinct, none under 10 (0..9 are TexCache's), every name in kAll;
  L  no literal unit elsewhere in src/gl: `constexpr int k...Unit... = <number>` or a `units > <number>` limit,
     except the passes with programs of their own (ALLOW below), which bind low units no cell sampler sits beside;
  R  two reds run on copies in memory and must FAIL: `dup` (AirVol put on FarRec's unit, the clash before the lane)
     and `literal` (the AO decal's old `units > 20 ? 20` limit put back in celllights.cpp).
Usage: python tests/spells/cell_units_check.py [repo root]
Verdict line: CELL UNITS VERDICT PASS|FAIL
"""
import os
import re
import sys

ALLOW = {
    ('cellbloom.cpp', 'kUnitA'), ('cellbloom.cpp', 'kUnitB'),
    ('cellhdr.cpp', 'kSrcUnit'), ('cellhdr.cpp', 'kFogBloomUnit'), ('cellfogbloom.cpp', 'kDepthUnit'),
    ('cellpost.cpp', 'kUnitCur'), ('cellpost.cpp', 'kUnitBlur'), ('cellpost.cpp', 'kUnitDepth'), ('cellpost.cpp', 'kUnitMv'),
    ('gametaa.cpp', 'kUnitCur'), ('gametaa.cpp', 'kUnitHist'), ('gametaa.cpp', 'kUnitMv'), ('gametaa.cpp', 'kUnitDepth'),
}
ENTRY = re.compile(r'^\s*constexpr int (\w+) = (\d+);', re.M)
KALL = re.compile(r'kAll\[\] = \{([^}]*)\}', re.S)
LIT_CONST = re.compile(r'constexpr int ((?:\w+ = \d+,\s*)*\w+ = \d+);')
LIT_LIMIT = re.compile(r'\bunits\s*>\s*\d+')


def judge_table(text):
    bad = []
    units = [(n, int(v)) for n, v in ENTRY.findall(text)]
    m = KALL.search(text)
    listed = [s.strip() for s in m.group(1).split(',') if s.strip()] if m else []
    names = [n for n, _ in units]
    for n in names:
        if n not in listed:
            bad.append(f'{n} is not in kAll')
    # lane WATERREBASE1: a run of units (LandWaterInfo 79-81) is listed as `Name + k`; each joins the clash check
    base = dict(units)
    for n in listed:
        run = re.fullmatch(r'(\w+)\s*\+\s*(\d+)', n)
        if run and run.group(1) in base:
            units.append((run.group(1) + '+' + run.group(2), base[run.group(1)] + int(run.group(2))))
        elif n not in names:
            bad.append(f'kAll names {n}, which the table does not define')
    seen = {}
    for n, v in units:
        if v < 10:
            bad.append(f'{n} = {v} is a TexCache unit (0..9)')
        if v in seen:
            bad.append(f'{n} and {seen[v]} share unit {v}')
        seen.setdefault(v, n)
    return units, bad


def judge_sources(files):
    bad = []
    for name, text in files.items():
        for m in LIT_CONST.finditer(text):
            for part in m.group(1).split(','):
                k = part.split('=')[0].strip()
                if 'Unit' in k and not k.endswith('Bias') and (name, k) not in ALLOW:
                    line = text.count('\n', 0, m.start()) + 1
                    bad.append(f'{name}:{line}: {k} is a literal unit (take it from cellunits.h)')
        for m in LIT_LIMIT.finditer(text):
            line = text.count('\n', 0, m.start()) + 1
            bad.append(f'{name}:{line}: "{m.group(0)}" is a literal unit limit')
    return bad


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    gl = os.path.join(root, 'src', 'gl')
    table = open(os.path.join(gl, 'cellunits.h'), encoding='utf-8', errors='replace').read()
    files = {}
    for f in sorted(os.listdir(gl)):
        if f.endswith(('.cpp', '.h')) and f != 'cellunits.h':
            files[f] = open(os.path.join(gl, f), encoding='utf-8', errors='replace').read()

    units, tbad = judge_table(table)
    print('  T  ' + ('PASS' if not tbad else 'FAIL') + '  ' + ', '.join(f'{n} {v}' for n, v in units))
    for b in tbad:
        print('     ' + b)
    lbad = judge_sources(files)
    print('  L  ' + ('PASS' if not lbad else 'FAIL') + f'  {len(files)} files in src/gl, {len(lbad)} literal units')
    for b in lbad[:30]:
        print('     ' + b)

    # reds, on copies in memory
    red_dup = re.sub(r'AirVol = \d+;', 'AirVol = 21;', table)
    _, rb = judge_table(red_dup)
    dup_fails = bool(rb)
    print(f'  red dup (AirVol on 21) -> T {"FAIL" if dup_fails else "PASS"} (must FAIL)' + (f': {rb[0]}' if rb else ''))
    # lane WATERREBASE1: a unit inside a `Name + k` run (UnderDepthMs on LandWaterInfo + 1) must clash too
    if re.search(r'UnderDepthMs = \d+;', table) and 'LandWaterInfo + 1' in table:
        _, rr = judge_table(re.sub(r'UnderDepthMs = \d+;', 'UnderDepthMs = 80;', table))
        dup_fails = dup_fails and bool(rr)
        print(f'  red run (UnderDepthMs on LandWaterInfo + 1) -> T {"FAIL" if rr else "PASS"} (must FAIL)' + (f': {rr[0]}' if rr else ''))
    red_lit = dict(files)
    cl = red_lit.get('celllights.cpp', '')
    red_lit['celllights.cpp'] = cl + '\nstatic int wwRedDecal( int units ) { return units > 20 ? 20 : -1; }\n'
    lb = judge_sources(red_lit)
    lit_fails = len(lb) > len(lbad)
    print(f'  red literal (units > 20 ? 20) -> L {"FAIL" if lit_fails else "PASS"} (must FAIL)')

    ok = not tbad and not lbad and dup_fails and lit_fails
    print('CELL UNITS VERDICT ' + ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
