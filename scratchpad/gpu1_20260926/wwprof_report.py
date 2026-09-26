"""Symbolise and summarise a wwprof.exe trace (lane GPU1, 2026-09-26).

python wwprof_report.py <trace.txt> <unstripped exe> [--top N] [--phases <bake.log with epoch prefixes> <t0 epoch>]
                        [--window a b]   (only samples with a <= t_ms/1000 <= b)
                        [--under FUNCSUBSTR]  (only stacks containing that function; inclusive tree under it)

Prints, CPU-weighted (each sample carries the CPU its thread burned in the interval):
  * total CPU seconds, wall seconds, mean busy threads (occupancy)
  * SELF: leaf function (first frame inside the exe; DLL frames named by module)
  * INCLUSIVE: every distinct function on the stack
  * MAIN THREAD timeline: what the main thread's stack names, per 10 s bucket, with busy threads
"""
import sys, subprocess, bisect, collections, re, os

args = sys.argv[1:]
trace, exe = args[0], args[1]
top = 40; window = None; under = None; bucket = 10.0
i = 2
while i < len(args):
    if args[i] == '--top': top = int(args[i + 1]); i += 2
    elif args[i] == '--window': window = (float(args[i + 1]), float(args[i + 2])); i += 3
    elif args[i] == '--under': under = args[i + 1]; i += 2
    elif args[i] == '--bucket': bucket = float(args[i + 1]); i += 2
    else: raise SystemExit('bad arg ' + args[i])

NM = r'C:/msys64/ucrt64/bin/nm.exe'
out = subprocess.run([NM, '-C', '--defined-only', exe], capture_output=True, text=True, errors='replace').stdout
syms = []
for l in out.splitlines():
    p = l.split(' ', 2)
    if len(p) == 3 and p[1] in ('T', 't'):
        try: syms.append((int(p[0], 16), p[2]))
        except ValueError: pass
syms.sort()
addrs = [a for a, _ in syms]
IMAGE_BASE = 0x140000000

def short(n):
    n = n.replace('(anonymous namespace)::', '~')
    n = re.sub(r'\(.*$', '', n) or '?paren'
    return n[:110]

mods = []; S = []; T = []
for l in open(trace, errors='replace'):
    if l.startswith('M '):
        _, b, sz, path = l.rstrip('\n').split(' ', 3); mods.append((int(b, 16), int(sz, 16), os.path.basename(path)))
    elif l.startswith('S '):
        p = l.split(); S.append((int(p[1]), int(p[2]), int(p[3]), int(p[4]), [int(x, 16) for x in p[5:]]))
    elif l.startswith('T '):
        p = l.split(); T.append((int(p[1]), int(p[2]), int(p[3]), int(p[4])))
exeMod = [m for m in mods if m[2].lower().startswith('nifskope')]
exeBase = exeMod[0][0] if exeMod else IMAGE_BASE
cache = {}
def name(a):
    if a in cache: return cache[a]
    r = None
    for b, sz, n in mods:
        if b <= a < b + sz:
            if n.lower().startswith('nifskope'):
                va = a - b + IMAGE_BASE
                k = bisect.bisect_right(addrs, va) - 1
                r = short(syms[k][1]) if k >= 0 else '?exe'
            else:
                r = '[' + n + ']'
            break
    if r is None: r = '?'
    cache[a] = r
    return r

if window:
    S = [s for s in S if window[0] <= s[0] / 1000 <= window[1]]
    T = [t for t in T if window[0] <= t[0] / 1000 <= window[1]]
selfc = collections.Counter(); incl = collections.Counter(); tot = 0
for t, tid, d, ismain, fr in S:
    if d <= 0: continue
    names = [name(a) for a in fr]
    if under:
        if not any(under in n for n in names): continue
        k = max(j for j, n in enumerate(names) if under in n)
        names = names[:k + 1]
    tot += d
    leaf = next((n for n in names if not n.startswith('[')), names[0] if names else '?')
    # a leaf in a DLL is reported as the DLL plus its first exe caller
    if names and names[0].startswith('['):
        leaf = names[0] + ' <- ' + leaf
    selfc[leaf] += d
    for n in set(names): incl[n] += d
wall = (T[-1][0] - T[0][0]) / 1000 if len(T) > 1 else 0
cpu = sum(x[1] for x in T) / 1e7
print('trace %s: wall %.1f s, process CPU %.1f s, mean busy cores %.2f, profiled CPU %.1f s%s' % (
    os.path.basename(trace), wall, cpu, cpu / wall if wall else 0, tot / 1e7, (' under ' + under) if under else ''))
print('\n== SELF (leaf) CPU s, share ==')
for n, d in selfc.most_common(top): print('%8.1f %5.1f%%  %s' % (d / 1e7, 100 * d / max(tot, 1), n))
print('\n== INCLUSIVE CPU s, share ==')
for n, d in incl.most_common(top): print('%8.1f %5.1f%%  %s' % (d / 1e7, 100 * d / max(tot, 1), n))

# main-thread timeline: the deepest exe frame that is a lodgen*/native*/Lodt* function names the stage
STAGEPAT = re.compile(r'(lodgen|native|Native|Lodt|lodt|Lodgen|nifcli|Vt|vt)')
print('\n== MAIN THREAD per %.0f s: busy cores, top stack frame names (outermost lodgen-ish frames) ==' % bucket)
mb = collections.defaultdict(collections.Counter); busy = collections.defaultdict(list)
for t, s, c, b in T: busy[int(t / 1000 / bucket)].append(s / 1e7 / (0.1 if False else 1))
for t, tid, d, ismain, fr in S:
    if not ismain: continue
    names = [name(a) for a in fr]
    st = [n for n in names if STAGEPAT.search(n) and not n.startswith('[')]
    key = ' < '.join(st[:3]) if st else (names[0] if names else '?')
    mb[int(t / 1000 / bucket)][key] += 1
# CPU per bucket from T lines: sum dcpu / bucket seconds
cpub = collections.Counter()
for t, s, c, b in T: cpub[int(t / 1000 / bucket)] += s
for k in sorted(mb):
    n, c = mb[k].most_common(1)[0]
    print('%6.0f s  cores %5.2f  %s' % (k * bucket, cpub[k] / 1e7 / bucket, n[:200]))
