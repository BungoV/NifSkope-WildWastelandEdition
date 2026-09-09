"""Targeted look at the places a rebake output would land, while the broad
hunt runs. Prints directory listings the shell tool will not."""
import os, sys, re

TARGETS = [
    r'X:\Programs\Steam\steamapps\common\Fallout 4',
    r'X:\Programs\Steam\steamapps\common\Fallout 4\Data',
    r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Textures',
    r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Meshes',
    r'E:\Tools',
    r'E:\Tools\Fallout 4',
    r'E:\Projects',
    r'C:\Users\bungo\AppData\Local',
    r'C:\Users\bungo\AppData\Roaming',
    r'C:\Users\bungo\Documents\My Games',
]
for t in sys.argv[1:] or TARGETS:
    print('===', t)
    if not os.path.isdir(t):
        print('    (missing)')
        continue
    try:
        ents = sorted(os.listdir(t))
    except OSError as e:
        print('    ERROR', e)
        continue
    for e in ents:
        p = os.path.join(t, e)
        try:
            if os.path.isdir(p):
                print('    [dir ] ' + e)
            else:
                print('    %10d %s' % (os.path.getsize(p), e))
        except OSError:
            print('    [?   ] ' + e)
