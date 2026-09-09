"""LENS 4, step 1: hunt this machine for any xLODGen / DynDOLOD / TexGen
install, config, log or OUTPUT. Report the negatives as carefully as the hits.

Two passes:
  A. NAME pass -- any path component matching a tool name pattern.
  B. OUTPUT pass -- any directory holding files that look like generated
     terrain LOD (Commonwealth.<n>.<x>.<y>.DDS / .BTR / .BTO), anywhere other
     than the known vanilla unpack.
Prints every directory it declined to descend into and why, so the negative is
auditable.
"""
import os, re, sys, time

ROOTS = [r'E:\\', r'X:\\', r'C:\Users\bungo', r'C:\Program Files',
         r'C:\Program Files (x86)', r'C:\ProgramData']

NAME_PAT = re.compile(
    r'(xlodgen|dyndolod|texgen|fo4lodgen|lodgenx?|xedit|fo4edit|tes5edit|sseedit|'
    r'modorganizer|mod organizer|vortex|nexus mod manager|mo2)', re.I)

FILE_PAT = re.compile(
    r'(dyndolod.*\.(txt|ini|log|esp|esm|bsa|ba2)|texgen.*\.(txt|ini|log)|'
    r'xlodgen.*\.(txt|ini|log)|lodgen.*\.(txt|ini|log)|.*\.lodsettings)$', re.I)

LOD_OUT = re.compile(r'^(\w+)\.(4|8|16|32|64|128)\.(-?\d+)\.(-?\d+)(_msn)?\.(dds|btr|bto)$', re.I)

SKIP_DIRS = {'$recycle.bin', 'system volume information', 'windows', 'winsxs',
             'node_modules', '.git', 'appdata\\local\\temp'}

VANILLA = os.path.normcase(r'E:\Tools\Fallout 4\DataUnpacked')

hits_name, hits_file, hits_out, skipped = [], [], {}, []
seen_dirs = 0
start = time.time()
BUDGET = 900.0


def walk(root):
    global seen_dirs
    for dirpath, dirnames, filenames in os.walk(root, topdown=True, onerror=lambda e: skipped.append(('err', str(e)))):
        if time.time() - start > BUDGET:
            skipped.append(('budget', dirpath))
            dirnames[:] = []
            return
        seen_dirs += 1
        low = os.path.normcase(dirpath)
        # prune noise
        pruned = []
        for d in list(dirnames):
            dl = d.lower()
            if dl in SKIP_DIRS or dl.startswith('$'):
                pruned.append(d)
                dirnames.remove(d)
        if pruned:
            skipped.append(('prune', dirpath + ' -> ' + ','.join(pruned)))
        base = os.path.basename(dirpath)
        if NAME_PAT.search(base):
            hits_name.append(dirpath)
        for f in filenames:
            if FILE_PAT.search(f):
                try:
                    sz = os.path.getsize(os.path.join(dirpath, f))
                except OSError:
                    sz = -1
                hits_file.append((os.path.join(dirpath, f), sz))
            m = LOD_OUT.match(f)
            if m and not low.startswith(VANILLA):
                hits_out.setdefault(dirpath, [0, set()])
                hits_out[dirpath][0] += 1
                hits_out[dirpath][1].add(m.group(6).lower())


for r in ROOTS:
    if not os.path.isdir(r):
        skipped.append(('missing', r))
        continue
    walk(r)

out = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hunt_result.txt'),
           'w', encoding='utf-8')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    out.write(s + '\n')


p('roots searched:', ROOTS)
p('directories visited:', seen_dirs, ' elapsed %.0fs' % (time.time() - start))
p()
p('=== A. directories whose NAME matches a tool pattern (%d) ===' % len(hits_name))
for h in sorted(set(hits_name)):
    p('   ', h)
p()
p('=== B. config/log files matching tool patterns (%d) ===' % len(hits_file))
for h, sz in sorted(set(hits_file)):
    p('    %10d  %s' % (sz, h))
p()
p('=== C. directories holding GENERATED-LOOKING terrain LOD, outside the vanilla unpack (%d) ===' % len(hits_out))
for d in sorted(hits_out):
    n, exts = hits_out[d]
    p('    %6d files  ext=%s  %s' % (n, ','.join(sorted(exts)), d))
p()
p('=== D. pruned / errored / budget (%d entries, first 60) ===' % len(skipped))
for kind, s in skipped[:60]:
    p('    [%s] %s' % (kind, s))
out.close()
