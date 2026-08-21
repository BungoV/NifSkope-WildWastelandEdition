"""Hold every rebuilt compound's AABB against VANILLA's, file by file.

    python tools/hkcompound_sweep.py <manifest.tsv> [<rebuilt-root> <vanilla-root>]

The manifest is the rebuild script's: status in column 1, relative path in
column 5. Exit 1 if any compound disagrees with vanilla.

hkcompound.py --quiet only checks a file against ITSELF: pointer present, tree
consistent, AABB equal to its own root box. All of that passed on the nine
meshes whose bound was short by a whole capsule, because the short bound was
short in both places. The number that cannot be argued with is Bethesda's.
"""
import os, subprocess, sys
BS = chr(92)
MOD = sys.argv[2] if len(sys.argv) > 2 else 'E:/Projects/Fallout 4 Mods/mods/WW Concord Collision Test/Meshes/'
VAN = sys.argv[3] if len(sys.argv) > 3 else 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'
TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hkcompound.py')

def aabbs(path):
    r = subprocess.run([sys.executable, TOOL, path, '--aabb'],
                       capture_output=True, text=True)
    return [l.strip() for l in r.stdout.splitlines() if '|' in l]


def flags(path):
    """Every shape's +0x10 header word: flags, key bits, dispatch type."""
    r = subprocess.run([sys.executable, TOOL, path, '--flags'],
                       capture_output=True, text=True)
    return [l.strip() for l in r.stdout.splitlines() if 'dispatch' in l]

same = diff = none = hdr = 0
for line in open(sys.argv[1]):
    parts = line.rstrip('\n').split('\t')
    if len(parts) < 5 or parts[0] != 'ok':
        continue
    rel = parts[4].replace(BS, '/')
    a, b = MOD + rel, VAN + rel
    if not (os.path.exists(a) and os.path.exists(b)):
        continue
    fa, fb = flags(a), flags(b)
    if fa != fb:
        hdr += 1
        print('HEADER %s' % rel)
        for o, t in zip(fa + [''] * len(fb), fb + [''] * len(fa)):
            if o != t:
                print('   ours    %s' % o)
                print('   vanilla %s' % t)
    ours, theirs = aabbs(a), aabbs(b)
    if not theirs:
        none += 1
        continue
    if ours == theirs:
        same += 1
    else:
        diff += 1
        print('DIFF %s' % rel)
        for o, t in zip(ours + [''] * len(theirs), theirs + [''] * len(ours)):
            if o != t:
                print('   ours    %s' % o)
                print('   vanilla %s' % t)
print('%d compounds match vanilla exactly, %d differ, %d files hold none'
      % (same, diff, none))
print('%d files whose shape HEADER words differ from vanilla' % hdr)
sys.exit(1 if (diff or hdr) else 0)
