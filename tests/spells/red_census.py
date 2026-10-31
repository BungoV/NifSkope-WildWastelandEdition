"""Census of every WW_*_RED switch in src/ and res/shaders/ (lane REDRETIRE1, CODE1030 item 3).

bungo 10-27: "Reds are scaffolding": a lane's red keeps the OLD path only until bungo himself confirms the lane live.
For each red name this prints
  name | owner lane | bungo's confirmation (quote + notes file) | retire yes/no (why)
and, with --tsv, the reader sites (file:line), the values, the first commit and the tests that name it.

  * reader sites: lines that READ the env/define (comments excluded); values: string literals on reader lines and
    values documented as NAME=a|b.
  * owner lane: the first non-merge commit whose -U0 patch ADDS a line naming the red (one git log pass).
  * confirmation: CONFIRM below, each quote checked verbatim in its notes file (a missing quote = unconfirmed).
    A lane judged only by a sub-agent's pictures or holds does not count.
  * a confirmed red whose reader sits in a busy file (--busy, one path per line) is "deferred (file busy)".
  * a red a committed test still uses as its red stays, unless its test red moved to a twin (src/wwredtwin.h).
  * RETIRED lists what REDRETIRE1 and REDRETIRE2 removed; each entry is checked GONE from src/res (no env reader left).

Usage: python -I tests/spells/red_census.py [--root <tree>] [--notes E:/Projects/Claude/notes] [--busy <file>]
       [--tsv <out>] [--names-only]
--names-only prints one name per line (the brief's "retired names gone" gate diffs the base tree's list and this).
"""
import argparse
import os
import re
import subprocess
import sys

NAME_RE = re.compile(r'\bWW_[A-Z0-9_]*_RED\b')
SCAN_DIRS = ('src', 'res/shaders')
SKIP_FILES = ('src/wwswitchstages.inc',)  # the generated stage table: a list, not a reader
TEXT_EXT = ('.cpp', '.h', '.inc', '.glsl', '.frag', '.vert', '.geom', '.comp', '.prog', '.hlsl', '.c', '.hpp')
READER_RE = re.compile(r'wwEnv|qEnvironmentVariable|qgetenv|getenv|qputenv|wwSwitch|WW_SWITCH|'
                       r'#\s*(if|ifdef|ifndef|elif|define)|defined\s*\(')

# Bungo's OWN confirmations, searched 10-30 in LANES.md, LANES_DONE.md, HANDOVER_1030.md, MORNING_1030.md, the
# merge*/ RESULTS + LANES backups, weatherlook1/APPROVALS.md and the git log. Lane -> [(quote, notes-relative file)].
# REDRETIRE2 searched again 10-31 (LANES.md, fix1030c HANDOFF/BRIEF_B, HANDOVER_1030.md): no new live confirmation.
CONFIRM = {
    # bungo ran the exe himself ("exe opened for bungo; merge after his OK") and OK'd the Cell dock panel.
    'OPENCELLFIX2': [('bungo OK on the panel 10-27', 'LANES.md'),
                     ('Rest is fine, but the presets', 'opencellfix2/NOTES.md')],
    'FLYFIX1': [('bungo 10-23 after FLYFIX1: "I can finally fly', 'LANES.md')],
}
# Quotes that look like a confirmation and are not (printed with --why-not, kept here so the next census agrees):
#   SSGI2FIX "bungo approved the look 10-22" (merge 23f19330): a picture pair of a branch NOT in main.
#   CLOUD4 'bungo 10-25 "weather works"': where cloud heights live (a design answer), not a live check.
#   WEATHERLOOK1 APPROVALS.md "snow_fine APPROVED": a texture asset, not the lane live.
#   GAPFIX-EMISSIVE "bungo accepted 1.575 as passed": a gate number; the lane owns no red.
#   SHOTPASS1 "5 confirmed reds": the lane's own pictures (sub-agent); its four owners already retired them.

# First commits whose subject names no lane: the lane from the commit body / WW_CHANGES.md.
LANE_FIX = {
    '5ba9486d': 'VOLAIR1', 'd3521fde': 'VOLAIR1', 'afa0b29a': 'PRTP-BAKEINCR', '4cea4120': 'BAKE-ALBEDO',
    'c7804a14': 'BAKE-ALBEDO', '4db1fd12': 'ALLON2', '46b0d575': 'SUNCELL1', '3acc3fee': 'GAPPERF-BINDLESS',
    'a6f20070': 'STARTUPGRID', '85c0b147': 'LANES-0910-0924',
}

# Per-value owners of the confirmed lanes' reds (git log -S'red( "<v>" )' / -S'QLatin1String( "<v>" )', 10-30).
# Value -> (owner lane, where it went). 'twin' = moved to src/wwredtwin.h (the test's red build only).
VALUES = {
    'WW_CELLGROUP_RED': {'<group>': ('OPENCELLFIX2', 'twin'), 'ssr': ('OPENCELLFIX2', 'twin')},
    'WW_DROPDOWN_RED': {'fit': ('OPENCELLFIX2', 'twin'),
                        '1': ('OPENCELLFIX2', 'kept: the in-app gate src/cellworkspacetest.cpp names it')},
    'WW_CELLSHOW_RED': dict(
        [(v, ('OPENCELLFIX2', 'twin')) for v in ('ticks', 'dropdown', 'presets', 'nest', 'livebind', 'foldrows',
                                               'inline', 'arrowframe', 'dropright', 'override', 'builtin',
                                               'presetbar')]
        + [('groups', ('OPENCELLFIX2', 'kept: the in-app gate src/cellworkspacetest.cpp names it')),
           ('showbutton', ('OPENCELLFIX2', 'twin')),
           ('menu', ('OPENCELLFIX1', 'kept: lane not confirmed')), ('sync', ('OPENCELLFIX1', 'kept: lane not confirmed'))]),
    'WW_OPENCELL_RED': {'lang': ('OPENCELLFIX2', 'twin'), 'dblclick': ('OPENCELLFIX2', 'twin'),
                        'writer': ('OPENCELL1', 'kept: lane not confirmed'),
                        'strings': ('LEFTOVERS1', 'kept: lane not confirmed'),
                        'masters': ('OPENCELLFIX1', 'kept: lane not confirmed'),
                        'restore': ('OPENCELLFIX1', 'kept: lane not confirmed'),
                        'list': ('CELLSPAWN1', 'kept: lane not confirmed')},
    # WW_FLYFIX_RED (FLYFIX1, value 1): the whole name retired by REDRETIRE2 (RETIRED below).
}

# What REDRETIRE1 + REDRETIRE2 retired: (name, value or None for the whole name, the test whose red build keeps the twin).
RETIRED = [('WW_CELLGROUP_RED', None, 'tests/cellgroups (REDTWIN=CELLGROUP=<group>|ssr)'),
           ('WW_FLYFIX_RED', None, 'tests/flyfix (REDTWIN=FLYFIX=tick|refocus)')] + \
    [('WW_CELLSHOW_RED', v, 'tests/cellshow (REDTWIN=CELLSHOW=%s)' % v)
     for v, (o, w) in VALUES['WW_CELLSHOW_RED'].items() if w == 'twin'] + \
    [('WW_DROPDOWN_RED', 'fit', 'tests/cellshow (REDTWIN=DROPDOWN=fit)'),
     ('WW_OPENCELL_RED', 'lang', 'tests/opencell (REDTWIN=OPENCELL=lang)'),
     ('WW_OPENCELL_RED', 'dblclick', 'tests/opencell (REDTWIN=OPENCELL=dblclick)')]


def read_lines(p):
    with open(p, 'rb') as fh:
        return fh.read().decode('utf-8', 'replace').split('\n')


def walk_text(root):
    for d in SCAN_DIRS:
        for dp, dn, fn in os.walk(os.path.join(root, d)):
            for f in fn:
                if f.endswith(TEXT_EXT):
                    p = os.path.join(dp, f)
                    rel = os.path.relpath(p, root).replace('\\', '/')
                    if rel not in SKIP_FILES:
                        yield p, rel


def scan(root):
    names = {}
    for p, rel in walk_text(root):
        for i, ln in enumerate(read_lines(p), 1):
            for m in NAME_RE.finditer(ln):
                n = m.group(0)
                e = names.setdefault(n, {'sites': [], 'readers': [], 'values': set()})
                e['sites'].append('%s:%d' % (rel, i))
                code = ln.split('//')[0]
                stripped = code.strip()
                is_comment = (not stripped) or stripped.startswith(('*', '/*', '#!')) or n not in code
                if rel.endswith('.prog') and stripped.startswith('#') and \
                        not re.match(r'#\s*(if|ifdef|ifndef|elif|define)', stripped):
                    is_comment = True
                if not is_comment and READER_RE.search(code):
                    e['readers'].append('%s:%d' % (rel, i))
                    for v in re.findall(r'"([A-Za-z0-9_.+-]{1,32})"', code):
                        if v != n:
                            e['values'].add(v)
                for v in re.findall(re.escape(n) + r'=([A-Za-z0-9_]+(?:\|[A-Za-z0-9_]+)*)', ln):
                    e['values'].update(v.split('|'))
    return names


def owners(root, names):
    """First non-merge commit (oldest first) whose -U0 patch adds a line naming each red."""
    out, want = {}, set(names)
    proc = subprocess.Popen(['git', 'log', '--reverse', '--no-merges', '-p', '-U0', '--format=@@C %h %ad %s',
                             '--date=short', '--', *SCAN_DIRS], cwd=root, stdout=subprocess.PIPE)
    cur = None
    for raw in proc.stdout:
        ln = raw.decode('utf-8', 'replace')
        if ln.startswith('@@C '):
            cur = ln[4:].rstrip('\n')
        elif ln.startswith('+') and not ln.startswith('+++') and '_RED' in ln:
            for m in NAME_RE.finditer(ln):
                if m.group(0) in want and m.group(0) not in out:
                    out[m.group(0)] = cur
    proc.wait()
    return out


def lane_of(subject):
    if not subject:
        return '?'
    h = subject.split(' ', 1)[0]
    if h in LANE_FIX:
        return LANE_FIX[h]
    s = subject.split(' ', 2)[2] if subject.count(' ') >= 2 else subject
    m = re.search(r'\b[Ll]ane ([A-Z][A-Z0-9_-]*[0-9A-Z])\b', s) or re.search(r'\(([A-Z][A-Z0-9_-]*[0-9])\)\s*$', s)
    if m:
        return m.group(1)
    m = re.match(r'(?:Merge\s+)?([A-Z][A-Z0-9_-]*[0-9A-Z])(?:\s+(?:M\d+|round \w+|[A-Z]\d?))?\s*[:(]', s)
    if m:
        return m.group(1)
    m = re.match(r'([A-Z][A-Z0-9_-]{2,})\b', s)
    return m.group(1) if m else '?'


def tests_using(root, names):
    users = {n: [] for n in names}
    for dp, dn, fn in os.walk(os.path.join(root, 'tests')):
        if os.sep + 'fixtures' in dp or os.sep + 'baselines' in dp or os.sep + 'out' in dp:
            continue
        for f in fn:
            if not f.endswith(('.sh', '.py', '.cpp', '.h', '.pro', '.txt', '.ps1')):
                continue
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, root).replace('\\', '/')
            if rel == 'tests/spells/red_census.py':
                continue
            t = open(p, 'rb').read().decode('utf-8', 'replace')
            for m in set(NAME_RE.findall(t)):
                if m in users:
                    users[m].append(rel)
    return users


def confirmation(notes, lane):
    hits = []
    for quote, f in CONFIRM.get(lane, []):
        try:
            t = open(os.path.join(notes, f), 'rb').read().decode('utf-8', 'replace')
        except OSError:
            return None, 'confirmation file missing: ' + f
        if quote not in t:
            return None, 'confirmation quote not found in ' + f
        hits.append('"%s" (%s)' % (quote, f))
    return ('; '.join(hits) or None), None


def decide(n, lane, conf, why, e, busy, tests):
    if n in VALUES:
        parts = []
        for v, (owner, where) in sorted(VALUES[n].items()):
            parts.append('%s=%s [%s]: %s' % (n.replace('WW_', '').replace('_RED', ''), v, owner,
                                             'retired -> test twin' if where == 'twin' else where))
        if any(w == 'twin' for o, w in VALUES[n].values()):
            return 'partly', '; '.join(parts)
        if any(w.startswith('deferred') for o, w in VALUES[n].values()):
            return 'deferred', '; '.join(parts)
        return 'no', '; '.join(parts)
    if not conf:
        return 'no', why or 'no bungo confirmation of %s on record' % lane
    bf = sorted({r.split(':')[0] for r in e['readers'] if r.split(':')[0] in busy})
    if bf:
        return 'deferred', 'file busy: ' + ' '.join(bf)
    if tests:
        return 'yes*', 'a committed test names it: move its red to a twin first'
    return 'yes', 'bungo confirmed ' + lane


# A red read through a local helper (the value is on the caller's line, not the name's): file -> call form.
HELPERS = {'WW_CELLSHOW_RED': ('src/ui/cellshowpanel.cpp', 'red( "%s" )')}


def retired_check(root):
    """Every RETIRED entry must have no env reader left in src/res: (entry, ok, evidence)."""
    texts = [(rel, read_lines(p)) for p, rel in walk_text(root)]
    out = []
    for name, value, test in RETIRED:
        bad = []
        for rel, lines in texts:
            for i, ln in enumerate(lines, 1):
                if value is not None and name in HELPERS and rel == HELPERS[name][0] and \
                        (HELPERS[name][1] % value) in ln:
                    bad.append('%s:%d' % (rel, i))
                    continue
                if name not in ln:
                    continue
                if value is None or ('"%s"' % value) in ln or re.search(re.escape(name) + r'=(\w+\|)*' +
                                                                        re.escape(value) + r'\b', ln):
                    bad.append('%s:%d' % (rel, i))
        out.append(('%s%s' % (name, '' if value is None else '=' + value), not bad, test, bad))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument('--notes', default='E:/Projects/Claude/notes')
    ap.add_argument('--busy')
    ap.add_argument('--tsv')
    ap.add_argument('--names-only', action='store_true')
    ap.add_argument('--no-git', action='store_true')
    a = ap.parse_args()
    names = scan(a.root)
    if a.names_only:
        for n in sorted(names):
            print(n)
        return 0
    busy = set()
    if a.busy:
        busy = {l.strip().replace('\\', '/') for l in open(a.busy, encoding='utf-8') if l.strip()}
    own = {} if a.no_git else owners(a.root, names)
    tu = tests_using(a.root, names)
    rows = []
    for n in sorted(names):
        e = names[n]
        subj = own.get(n)
        lane = lane_of(subj)
        conf, why = confirmation(a.notes, lane)
        if n in VALUES:  # a name whose values belong to a confirmed lane: the confirmation of that lane
            lanes = sorted({o for o, w in VALUES[n].values()})
            conf = '; '.join(c for c in (confirmation(a.notes, l)[0] for l in lanes) if c) or conf
        retire, reason = decide(n, lane, conf, why, e, busy, tu[n])
        rows.append((n, lane, subj or '?', conf or '-', retire, reason, ','.join(sorted(e['values'])) or '-',
                     ' '.join(e['readers']) or '-', len(e['sites']), ' '.join(tu[n]) or '-'))
    hdr = ('name', 'lane', 'first commit', 'confirmation', 'retire', 'why', 'values', 'readers', 'sites', 'tests')
    if a.tsv:
        with open(a.tsv, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('\t'.join(hdr) + '\n')
            for r in rows:
                fh.write('\t'.join(str(x) for x in r) + '\n')
    for r in rows:
        print('%s | %s | %s | %s (%s)' % (r[0], r[1], r[3], r[4], r[5]))
    rc = 0
    print('RETIRED (REDRETIRE1 + REDRETIRE2; the env reader is gone, the old path lives only in the named test\'s red build):')
    for what, ok, test, bad in retired_check(a.root):
        print('  %s %s -> %s%s' % ('GONE' if ok else 'STILL READ', what, test, '' if ok else ' at ' + ' '.join(bad)))
        rc |= 0 if ok else 1
    cnt = {}
    for r in rows:
        cnt[r[4]] = cnt.get(r[4], 0) + 1
    print('RED CENSUS: %d names (%d without a reader line), %s; confirmed lanes: %s' % (
        len(rows), sum(1 for r in rows if r[7] == '-'), ', '.join('%s %d' % kv for kv in sorted(cnt.items())),
        ', '.join(sorted(CONFIRM))))
    return rc


if __name__ == '__main__':
    sys.exit(main())
