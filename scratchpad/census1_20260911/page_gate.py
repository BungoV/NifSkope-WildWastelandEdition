#!/usr/bin/env python3
"""Gates C1 and C3 for docs/LODGEN_CENSUS.md, with their floors.

C1  every field row has all SIX gate columns filled -- name / unit / read-from /
    moves / refusal / default. Blanks = 0.
    FLOOR: a deliberately blank row is appended to a COPY of the page and the
    blank count must rise above 0.

C3  every "read from" citation resolves to a section that exists in the page it
    names. The literal `runtime` is allowed and counted separately: it means the
    field is not read from any baked file.
    FLOOR: a citation to a section number that does not exist is added to a COPY
    and must be reported MISSING.

Read-only on the tree; the floors work on temporary copies in this directory.
"""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
PAGE = os.path.join(REPO, 'docs', 'LODGEN_CENSUS.md')

PAGES = {
    'NATIVE':   'docs/LODGEN_NATIVE_LODO_LODI.md',
    'VT':       'docs/LODGEN_TERRAIN_VT.md',
    'CARDS':    'docs/LODGEN_CARD_SHEETS.md',
    'ARRAYS':   'docs/LODGEN_TEXTURE_ARRAYS.md',
    'MANIFEST': 'docs/LODGEN_MANIFEST_FORMAT.md',
    'LODM':     'docs/LODGEN_LODM_FORMAT.md',
}

# the field tables are the ones whose header row starts with `| field |`
HEADER = ('field', 'unit', 'what it counts', 'read from', 'how it moves',
          'refusal words', 'default')
GATE_COLS = (0, 1, 3, 4, 5, 6)          # name, unit, read-from, moves, refusal, default


def cells(line):
    s = line.strip()
    if not s.startswith('|'):
        return None
    parts = s.split('|')
    return [p.strip() for p in parts[1:-1]] if len(parts) >= 3 else None


def field_rows(text):
    """-> [(lineno, cells)] for every data row of every field table."""
    rows, in_table = [], False
    for n, line in enumerate(text.split('\n'), 1):
        c = cells(line)
        if c is None:
            in_table = False
            continue
        low = [x.lower() for x in c]
        if len(c) == 7 and tuple(low) == HEADER:
            in_table = True
            continue
        if in_table:
            if all(set(x) <= set('-: ') for x in c):     # the |---|---| rule
                continue
            rows.append((n, c))
    return rows


def sections_of(path):
    out = set()
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            m = re.match(r'^#{2,6}\s+([0-9]+[a-z]?(?:\.[0-9]+[a-z]?)*)\.?[\s]', line)
            if m:
                out.add(m.group(1))
            m2 = re.match(r'^#{2,6}\s+(Deviations?|Provenance|Appendix)\b', line, re.I)
            if m2:
                out.add(m2.group(1).capitalize())
    return out


CITE = re.compile(r'\b(NATIVE|VT|CARDS|ARRAYS|MANIFEST|LODM)\s+([0-9]+[a-z]?(?:\.[0-9]+[a-z]?)*)\b')


def gate(text, label):
    rows = field_rows(text)
    blank, dash = [], 0
    for n, c in rows:
        if len(c) != 7:
            blank.append((n, 'row has %d cells, not 7' % len(c)))
            continue
        for i in GATE_COLS:
            v = c[i]
            if not v:
                blank.append((n, 'column %d (%s) is blank' % (i + 1, HEADER[i])))
            elif v in ('—', '-', 'n/a'):
                dash += 1

    cites, missing, runtime = set(), [], 0
    for n, c in rows:
        if len(c) == 7:
            col = c[3]
            if col.strip() == 'runtime':
                runtime += 1
            for m in CITE.finditer(col):
                cites.add((m.group(1), m.group(2), n))
    for m in CITE.finditer(text):                         # prose citations too
        cites.add((m.group(1), m.group(2), 0))

    have = {}
    for k, rel in PAGES.items():
        p = os.path.join(REPO, rel)
        have[k] = sections_of(p) if os.path.exists(p) else set()
    for k, sec, n in sorted(cites):
        if sec not in have[k]:
            missing.append((n, '%s %s -> %s has no such section' % (k, sec, PAGES[k])))

    print('%s: %d field rows, %d citations (%d distinct), %d `runtime` cells, '
          '%d cells that state "no refusal"'
          % (label, len(rows), len(cites), len({(a, b) for a, b, _ in cites}), runtime, dash))
    for n, why in blank:
        print('  C1 BLANK  line %s: %s' % (n, why))
    for n, why in missing:
        print('  C3 MISSING line %s: %s' % (n, why))
    print('%s: C1 blanks %d, C3 missing %d' % (label, len(blank), len(missing)))
    return len(blank), len(missing)


def main():
    text = open(PAGE, encoding='utf-8').read()
    b, m = gate(text, 'C1/C3 on the page')

    print('')
    print('-- the floors --')
    blankrow = ('\n| field | unit | what it counts | read from | how it MOVES | refusal words | default |\n'
                '|---|---|---|---|---|---|---|\n'
                '| deliberatelyBlank |  | the C1 floor |  |  |  |  |\n')
    fb, _ = gate(text + blankrow, 'C1 floor (a blank row appended)')
    badcite = ('\n| field | unit | what it counts | read from | how it MOVES | refusal words | default |\n'
               '|---|---|---|---|---|---|---|\n'
               '| deliberatelyWrongCite | count | the C3 floor | NATIVE 99.99 | never | none | unread |\n')
    _, fm = gate(text + badcite, 'C3 floor (a citation to a section that does not exist)')

    ok = (b == 0 and m == 0 and fb > 0 and fm > 0)
    print('')
    print('C1 %s (%d blanks, floor caught %d)' % ('PASS' if b == 0 and fb > 0 else 'FAIL', b, fb))
    print('C3 %s (%d missing, floor caught %d)' % ('PASS' if m == 0 and fm > 0 else 'FAIL', m, fm))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
