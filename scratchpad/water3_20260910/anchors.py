# -*- coding: utf-8 -*-
"""anchors.py -- re-derive every line number and every stamp in
spec_water.md's provenance footer FROM ITS ANCHOR TEXT.

The procedure is `ww-contract-provenance` step 3, and its three rules are
enforced here: EXACT and UNIQUE or no rewrite (a stale number announces itself,
a plausible wrong one never does); a row with commas in its line column is left
for a human; a MISSING anchor is a CONTENT question and is reported, never
deleted.

Usage:  python anchors.py            # rewrite
        python anchors.py --check    # report only
"""
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
PAGE = os.path.join(ROOT, 'scratchpad', 'specs_20260909', 'spec_water.md')

# the short name a footer row uses -> the file it means
FILES = {
    'lodtfile.cpp': 'src/lodtfile.cpp',
    'lodtfile.h': 'src/lodtfile.h',
    'watermark.cpp': 'src/watermark.cpp',
    'watermark.h': 'src/watermark.h',
    'watermarkpanel.cpp': 'src/watermarkpanel.cpp',
    'watermarkpanel.h': 'src/watermarkpanel.h',
    'LODGEN_BTD_FORMAT.md': 'docs/LODGEN_BTD_FORMAT.md',
    'lodl_open_authority.py': 'tests/spells/lodl_open_authority.py',
}

# the placeholder token a stamp row uses -> the file it stamps
STAMPS = {
    'LODTFILE_CPP': 'src/lodtfile.cpp',
    'LODTFILE_H': 'src/lodtfile.h',
    'WATERMARK_CPP': 'src/watermark.cpp',
    'WATERMARK_H': 'src/watermark.h',
    'WATERMARKPANEL_CPP': 'src/watermarkpanel.cpp',
    'BTDFORMAT': 'docs/LODGEN_BTD_FORMAT.md',
    'AUTHORITY': 'tests/spells/lodl_open_authority.py',
}

check_only = '--check' in sys.argv
src_cache = {}


def source(rel):
    if rel not in src_cache:
        with open(os.path.join(ROOT, rel), 'rb') as f:
            src_cache[rel] = f.read().decode('utf-8', 'replace')
    return src_cache[rel]


def unmd(s):
    return s.replace('\\|', '|').replace('\\_', '_').replace('\\*', '*')


def find_line(rel, anchor):
    """Line number (1-based) of an anchor that occurs EXACTLY ONCE."""
    text = source(rel)
    n = text.count(anchor)
    if n == 0:
        return None, 'MISSING'
    if n > 1:
        return None, 'AMBIGUOUS(%d)' % n
    return text[:text.index(anchor)].count('\n') + 1, 'ok'


page = open(PAGE, 'rb').read().decode('utf-8')
assert page.count('\r') == 0, 'spec_water.md is LF-only'

# ---- 1. the stamps -------------------------------------------------------
for token, rel in STAMPS.items():
    with open(os.path.join(ROOT, rel), 'rb') as f:
        b = f.read()
    sha = hashlib.sha256(b).hexdigest()[:16]
    page = page.replace('SHA_' + token, sha)
    page = page.replace('BYTES_' + token, '{:,}'.format(len(b)))
    page = page.replace('LINES_' + token, '{:,}'.format(b.count(b'\n')))
    print('%-28s %s %8d bytes %6d lines  CR=%d'
          % (rel, sha, len(b), b.count(b'\n'), b.count(b'\r')))

# ---- 2. every claim row --------------------------------------------------
ROW = re.compile(r'^\| (?P<claim>[^|]+?) \| (?P<where>[^|]+?) \| (?P<anchor>.+?) \|\s*$')
out = []
moved = same = missing = manual = 0
for line in page.split('\n'):
    m = ROW.match(line)
    if not m:
        out.append(line)
        continue
    where = m.group('where').strip()
    hit = re.match(r'^(?P<file>[A-Za-z0-9_.]+)\s+(?P<num>\S+)$', where)
    if not hit or hit.group('file') not in FILES:
        out.append(line)
        continue
    if ',' in hit.group('num'):
        # a multi-site row carries one anchor and several numbers: by hand
        print('BY HAND   %s  (%s)' % (m.group('claim').strip(), where))
        manual += 1
        out.append(line)
        continue
    spans = re.findall(r'`([^`]+)`', m.group('anchor'))
    if not spans:
        out.append(line)
        continue
    rel = FILES[hit.group('file')]
    ln, why = find_line(rel, unmd(spans[0]))
    if ln is None:
        print('%-9s %s  ->  %s' % (why, m.group('claim').strip(), spans[0][:60]))
        missing += 1
        out.append(line)
        continue
    new_where = '%s %d' % (hit.group('file'), ln)
    if new_where == where:
        same += 1
    else:
        moved += 1
    out.append(line.replace('| ' + where + ' |', '| ' + new_where + ' |', 1))

page = '\n'.join(out)
print('rows: %d moved, %d unchanged, %d anchors not found, %d by hand'
      % (moved, same, missing, manual))
if check_only:
    sys.exit(1 if missing else 0)
open(PAGE, 'wb').write(page.encode('utf-8'))
nb = open(PAGE, 'rb').read()
print('spec_water.md: %d bytes, %d lines, CR=%d'
      % (len(nb), nb.count(b'\n'), nb.count(b'\r')))
sys.exit(1 if missing else 0)
