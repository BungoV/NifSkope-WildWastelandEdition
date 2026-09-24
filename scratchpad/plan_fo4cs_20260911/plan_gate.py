#!/usr/bin/env python3
"""Gates G1, G2 and G3 for docs/FO4CS_IMPROVED_LOD_PLAN.md, each with a floor.

G1  every contract citation (`NATIVE 4.4`, `VT 2.2a`, `BTD Header`, ...) resolves
    to a section that exists in the page it names.
    FLOOR: a citation to a section that does not exist is appended to a COPY of
    the page in memory and must be reported MISSING.

G2  every rung heading (`### R0 ...` .. `### R5 ...`) is followed, before the
    next rung, by all NINE part labels.
    FLOOR: one label is deleted from a COPY and the rung must be reported short.

G3  every backticked lower-camelCase identifier in the page is either (a) a word
    that appears in one of the eight contract pages, or (b) a key this page
    itself declares in a key table with `[ImprovedLOD]`-style rows. Anything else
    is invented vocabulary.
    FLOOR: an invented token is appended to a COPY and must be reported.

Read-only on the tree; every floor works on a copy in memory.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
PAGE = os.path.join(REPO, 'docs', 'FO4CS_IMPROVED_LOD_PLAN.md')

PAGES = {
    'NATIVE':   'docs/LODGEN_NATIVE_LODO_LODI.md',
    'VT':       'docs/LODGEN_TERRAIN_VT.md',
    'BTD':      'docs/LODGEN_BTD_FORMAT.md',
    'CARDS':    'docs/LODGEN_CARD_SHEETS.md',
    'ARRAYS':   'docs/LODGEN_TEXTURE_ARRAYS.md',
    'MANIFEST': 'docs/LODGEN_MANIFEST_FORMAT.md',
    'LODM':     'docs/LODGEN_LODM_FORMAT.md',
    'CENSUS':   'docs/LODGEN_CENSUS.md',
}

# a numbered citation (NATIVE 4.4.1, VT 2.2a) or a NAMED one (BTD Header)
CITE_NUM = re.compile(r'\b(' + '|'.join(PAGES) + r')\s+([0-9]+[a-z]?(?:\.[0-9]+[a-z]?)*)\b')
CITE_NAME = re.compile(r'\b(BTD)\s+((?:[A-Z][a-z]+|[A-Z]{2,})(?:\s+[a-z]+)*)')

NINE = [
    '**READS.', '**DOES.', '**SWITCH AND KEYS.', '**FALLBACK, AND THE ARM WORD.',
    '**CENSUS.', '**GATES', '**FLIGHT.', '**MUST NOT.', '**RE CANDIDATES',
]


def sections_of(path):
    """Numbered sections AND named headings, both as lookup keys."""
    num, named = set(), set()
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            m = re.match(r'^#{2,6}\s+([0-9]+[a-z]?(?:\.[0-9]+[a-z]?)*)\.?[\s]', line)
            if m:
                num.add(m.group(1))
                continue
            m2 = re.match(r'^#{2,6}\s+(.+?)\s*$', line)
            if m2:
                named.add(m2.group(1).strip().lower())
    return num, named


def load_sections():
    out = {}
    for k, rel in PAGES.items():
        p = os.path.join(REPO, rel)
        out[k] = sections_of(p) if os.path.exists(p) else (set(), set())
    return out


def g1(text, have, label):
    missing = []
    seen = set()
    for m in CITE_NUM.finditer(text):
        key, sec = m.group(1), m.group(2)
        seen.add((key, sec))
        if sec not in have[key][0]:
            missing.append('%s %s -> %s has no such section' % (key, sec, PAGES[key]))
    named = set()
    for m in CITE_NAME.finditer(text):
        key, name = m.group(1), m.group(2).strip().lower()
        # try the longest prefix of the phrase that is a real heading
        words = name.split()
        hit = None
        for n in range(len(words), 0, -1):
            cand = ' '.join(words[:n])
            if any(h == cand or h.startswith(cand + ' ') or h.startswith(cand + ',') for h in have[key][1]):
                hit = cand
                break
        named.add((key, name))
        if hit is None:
            missing.append('%s "%s" -> %s has no such heading' % (key, name, PAGES[key]))
    print('%s: %d distinct numbered citations, %d named citations, %d unresolved'
          % (label, len(seen), len(named), len(missing)))
    for w in missing:
        print('  G1 MISSING %s' % w)
    return len(missing)


def rung_blocks(text):
    lines = text.split('\n')
    starts = [(n, l) for n, l in enumerate(lines) if re.match(r'^### R[0-9]', l)]
    out = []
    for i, (n, l) in enumerate(starts):
        end = starts[i + 1][0] if i + 1 < len(starts) else len(lines)
        out.append((l.strip(), '\n'.join(lines[n:end])))
    return out


def g2(text, label):
    short = []
    blocks = rung_blocks(text)
    for head, body in blocks:
        miss = [p for p in NINE if p not in body]
        if miss:
            short.append((head, miss))
    print('%s: %d rungs, %d short' % (label, len(blocks), len(short)))
    for head, miss in short:
        print('  G2 SHORT %s is missing %s' % (head[:40], ', '.join(x.strip('*') for x in miss)))
    return len(short)


TOKEN = re.compile(r'`([a-z][A-Za-z0-9_]*(?:\.[a-zA-Z][A-Za-z0-9_]*)?(?:\[[^\]]*\])?)`')

# Established ENGINE vocabulary: not ours, not in our contracts, and not invented.
# Each entry names where it is quoted, so the allowlist cannot grow silently.
ENGINE_VOCAB = {
    # the [TerrainManager] distance family, quoted with bungo's own live values in
    # E:\Projects\Fo4CommunityShaders\Codex\lod-fo4-vs-fo76-comparison.md
    'fBlockLevel0Distance',
}


def declared_keys(text):
    """Keys this page declares in its own key tables: | `key` | kind | ... |"""
    keys = set()
    for line in text.split('\n'):
        m = re.match(r'^\|\s*`(?:\[ImprovedLOD\]\s*)?([A-Za-z][A-Za-z0-9_]*)`\s*\|', line)
        if m:
            keys.add(m.group(1))
    return keys


def g3(text, corpus, label):
    keys = declared_keys(text)
    toks = {m.group(1) for m in TOKEN.finditer(text)}
    unknown = []
    for t in sorted(toks):
        base = t.split('[')[0].split('.')[-1]      # brackets FIRST: `rep[0..3]` -> `rep`
        if base in keys or t in keys:
            continue
        if base in corpus or t in corpus:
            continue
        if base in ENGINE_VOCAB or t in ENGINE_VOCAB:
            continue
        unknown.append(t)
    print('%s: %d backticked identifiers, %d declared keys, %d not found in any contract'
          % (label, len(toks), len(keys), len(unknown)))
    for t in unknown:
        print('  G3 INVENTED %s' % t)
    return len(unknown)


def main():
    text = open(PAGE, encoding='utf-8').read()
    have = load_sections()

    corpus = set()
    for rel in PAGES.values():
        p = os.path.join(REPO, rel)
        if os.path.exists(p):
            corpus |= set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', open(p, encoding='utf-8').read()))

    print('== the page ==')
    m = g1(text, have, 'G1')
    s = g2(text, 'G2')
    u = g3(text, corpus, 'G3')

    print('')
    print('== the floors ==')
    fm = g1(text + '\n(NATIVE 99.99) and (BTD Nonexistent heading)\n', have, 'G1 floor')
    blocks = rung_blocks(text)
    sabotaged = text.replace(blocks[0][1], blocks[0][1].replace('**MUST NOT.', '**Must not, unlabelled.'), 1)
    fs = g2(sabotaged, 'G2 floor')
    fu = g3(text + '\nA token nobody defined: `frobnicatorWidgetCount`.\n', corpus, 'G3 floor')

    print('')
    print('G1 %s (%d unresolved, floor caught %d)' % ('PASS' if m == 0 and fm > 0 else 'FAIL', m, fm))
    print('G2 %s (%d short, floor caught %d)' % ('PASS' if s == 0 and fs > 0 else 'FAIL', s, fs))
    print('G3 %s (%d invented, floor caught %d)' % ('PASS' if u == 0 and fu > u else 'FAIL', u, fu))
    ok = (m == 0 and fm > 0 and s == 0 and fs > 0 and u == 0 and fu > 0)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
