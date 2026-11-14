"""lane WEATHERRECX3 gate: every shipped weather resolved twice and compared key by key.
  reference = origin/modules1-20261105's reader (dump_old.exe, -DWX3_OLD) on the ORIGINAL 1.4 files
  candidate = this branch's reader (dump_new.exe) on a candidate set (the migrated files; the reds)
Pass = 0 differences over every weather x hour x key (the branch's "stored." keys and "source" are not compared: spec
"stored for later" / the 1.8 echo). The Precip type files (no 1.4 reader) compare as JSON with only "version" moved.

usage: python -I gate.py <dump_old.exe> <dump_new.exe> <wthr_list.json> <orig Weathers> <flat Weathers> <work dir>
                         <name>=<candidate Weathers> [...]
  <flat Weathers>: the folder whose top-level .recx (the groundFog set, untouched by these lanes) both sides also read
Prints one census line per candidate: "census <name>: files F weathers W hours H keys K differences D -> PASS|FAIL"
"""
import json, os, shutil, subprocess, sys

HOURS = '0,2,3.5,5,8,11,14,17,20,23'
REL = 'Plugins/Fusion Shaders - Assets/Weathers'
NOCMP = ('stored.', 'source=', 'wiredlog.')  # WEATHERRECX4: the wired keys' telemetry text (branch only)

args = sys.argv[1:]
plan_dir = None	# --plan <Weathers>: the weathers to resolve come from this set (the control: a root with no files)
if '--plan' in args:
    i = args.index('--plan')
    plan_dir = args[i + 1]
    del args[i:i + 2]
old_exe, new_exe, list_json, orig, flat, work = args[:6]
cands = [a.split('=', 1) for a in args[6:]]
lst = {}
for w in json.load(open(list_json, encoding='utf-8')):
    lst[(w['owner'].lower(), int(w['lid'], 16))] = w


def build_root(name, weathers):
    r = os.path.join(work, 'roots', name)
    if os.path.isdir(r):
        shutil.rmtree(r)
    d = os.path.join(r, REL)
    os.makedirs(d)
    for f in os.listdir(flat):
        if f.endswith('.recx') and os.path.isfile(os.path.join(flat, f)):
            shutil.copy2(os.path.join(flat, f), d)
    for sub in os.listdir(weathers):
        if os.path.isdir(os.path.join(weathers, sub)):
            shutil.copytree(os.path.join(weathers, sub), os.path.join(d, sub))
    return r


def plan_of(weathers):
    rows, nfiles = [], 0
    for sub in sorted(os.listdir(weathers)):
        p = os.path.join(weathers, sub)
        if not os.path.isdir(p) or sub == 'Precip':
            continue
        for f in sorted(os.listdir(p)):
            if not f.endswith('.recx'):
                continue
            nfiles += 1
            j = json.load(open(os.path.join(p, f), encoding='utf-8'))
            for e in j.get('entries', []):
                owner, lid = e['weather'].split('|')
                w = lst[(owner.lower(), int(lid, 16))]
                d = w.get('data', {})
                pr = w.get('precip') or {}
                pt = {'rain': 0, 'snow': 1}.get(pr.get('type'), -1)
                rows.append([w['edid'], w['owner'], w['lid'], d.get('flags', 0), d.get('windSpeed', 0), d.get('windDir', 0),
                             d.get('windTurb') or 0, d.get('transDelta', 0), d.get('precipFadeIn', 0),
                             d.get('precipFadeOut', 0), d.get('thunderFadeIn', 0), d.get('thunderFadeOut', 0), pt,
                             pr.get('density', 7.0), int('thunder' in w.get('sounds', {}))])
    return rows, nfiles


def run(exe, root, plan, tag):
    env = dict(os.environ)
    env['PATH'] = 'C:\\msys64\\ucrt64\\bin;' + env['PATH']
    for k in list(env):
        if k.startswith('WW_'):
            env.pop(k)
    env['WW_RECX_ROOT'] = root
    pf = os.path.join(work, 'plan.tsv')
    with open(pf, 'w', encoding='utf-8', newline='\n') as f:
        for row in plan:
            f.write('\t'.join(str(x) for x in row) + '\n')
    r = subprocess.run([exe, pf, HOURS], env=env, stdin=subprocess.DEVNULL, capture_output=True)
    open(os.path.join(work, 'dump_%s.out' % tag), 'wb').write(r.stdout)
    open(os.path.join(work, 'dump_%s.err' % tag), 'wb').write(r.stderr)
    if r.returncode:
        raise SystemExit('%s exit %d' % (tag, r.returncode))
    res = {}
    for line in r.stdout.decode('utf-8', 'replace').splitlines():
        c = line.rstrip('\r').split('\t')
        res[c[0]] = dict(kv.split('=', 1) for kv in c[1:] if not kv.startswith(NOCMP))
        res[c[0]]['#stored'] = ';'.join(kv for kv in c[1:] if kv.startswith('stored.'))
    return res


def precip_diff(a, b):
    out = []
    for f in sorted(os.listdir(a)):
        ja = json.load(open(os.path.join(a, f), encoding='utf-8'))
        pb = os.path.join(b, f)
        if not os.path.isfile(pb):
            out.append('Precip/%s missing' % f)
            continue
        jb = json.load(open(pb, encoding='utf-8'))
        ja.pop('version', None)
        if jb.pop('version', None) != '1.8':
            out.append('Precip/%s version not 1.8' % f)
        if ja != jb:
            out.append('Precip/%s values differ' % f)
    return out


os.makedirs(work, exist_ok=True)
plan, nfiles = plan_of(plan_dir or orig)
ref = run(old_exe, build_root('orig', orig), plan, 'orig_old')
worst = 0
for name, cdir in cands:
    cplan, cfiles = plan_of(cdir)
    got = run(new_exe, build_root(name, cdir), plan, name + '_new')
    keys = diffs = 0
    lines = []
    stored = set()
    for case, rv in ref.items():
        gv = got.get(case)
        if gv is None:
            diffs += 1
            lines.append('%s\tMISSING' % case)
            continue
        if gv['#stored']:
            stored.update(s.split('=')[0] for s in gv['#stored'].split(';'))
        for k in sorted(set(rv) | set(gv)):
            if k == '#stored':
                continue
            keys += 1
            if rv.get(k) != gv.get(k):
                diffs += 1
                lines.append('%s\t%s\t%s\t%s' % (case, k, rv.get(k), gv.get(k)))
    pd = precip_diff(os.path.join(orig, 'Precip'), os.path.join(cdir, 'Precip')) if os.path.isdir(os.path.join(orig, 'Precip')) else []
    diffs += len(pd)
    lines += pd
    with open(os.path.join(work, 'diff_%s.tsv' % name), 'w', encoding='utf-8', newline='\n') as f:
        f.write('case\tkey\treference(1.4 reader, original)\tcandidate(1.8 reader)\n' + '\n'.join(lines) + '\n')
    by = {}
    for ln in lines:
        c = ln.split('\t')
        if len(c) > 1:
            by[c[1]] = by.get(c[1], 0) + 1
    top = ', '.join('%s %d' % kv for kv in sorted(by.items(), key=lambda kv: -kv[1])[:8])
    print('census %s: files %d (+%d Precip) weathers %d hours %d keys %d differences %d -> %s%s' % (
        name, cfiles, len(os.listdir(os.path.join(cdir, 'Precip'))) if os.path.isdir(os.path.join(cdir, 'Precip')) else 0, len(plan), len(HOURS.split(',')), keys, diffs,
        'PASS' if diffs == 0 else 'FAIL', ('  [' + top + ']') if top else ''))
    print('  stored-for-later keys (not compared): %s' % (', '.join(sorted(stored)) or 'none'))
