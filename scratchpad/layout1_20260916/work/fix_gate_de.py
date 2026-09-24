"""Lane LAYOUT1 (2026-09-16): legs (d) and (e) of the layout gate expected the
wrong thing. Written to a file, with every backslash built from chr(92),
because a heredoc halves them (the trap this lane recorded twice already).
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_layout.sh'
B = chr(92)
C = B + '\n'          # a shell line continuation

s = open(P, encoding='utf-8', newline='').read()

a = ('KM="$(find "$W/keep/$FO4" -name \'*.BTO.manifest.txt\' 2>/dev/null | wc -l)"\n'
     '[ "$KM" -gt 0 ] && note "(d) the manifest sidecar is beside the files it describes '
     '($KM under $FO4/)" ' + C +
     '\t|| bad "(d) the manifest sidecar is beside the files it describes '
     '(found $KM under $FO4/)"\n')

b = ('# THE SIDECAR FOLLOWS ITS CHUNK, which is the whole point of calling it a\n'
     '# sidecar. With the scratch folder the `.BTO` does not survive, so the\n'
     '# sidecar lands under the one root beside the files that DID survive; with\n'
     '# --keep-bto the chunk stays at the out-dir root and its sidecar stays next\n'
     '# to it, because --keep-bto is the way back to the old tree BYTE FOR BYTE\n'
     '# (lane BTOFREE1) and a sidecar in another folder is not that tree. BOTH\n'
     '# sides are counted, so a build that moved it under the root anyway fails.\n'
     'KMR="$(find "$W/keep" -maxdepth 1 -name \'*.BTO.manifest.txt\' 2>/dev/null | wc -l)"\n'
     'KMF="$(find "$W/keep/$FO4" -name \'*.BTO.manifest.txt\' 2>/dev/null | wc -l)"\n'
     '[ "$KMR" = "$KB" ] && [ "$KMF" = "0" ] ' + C +
     '\t&& note "(d) each kept chunk still has its sidecar beside it '
     '($KMR at the out-dir root, 0 under $FO4/)" ' + C +
     '\t|| bad "(d) each kept chunk still has its sidecar beside it '
     '($KMR beside $KB chunk(s), $KMF under $FO4/)"\n')

if s.count(a) != 1:
    print('MISS (d): %d' % s.count(a))
    sys.exit(1)
s = s.replace(a, b)

a2 = ("\t| grep -v 'lodgen" + B + ".cpp:[0-9]*:[^:]*" + B + "* ' " + C)
b2 = ('# Each exclusion is a REASON, not a sweep, and the list printed above is\n'
      '# what a reviewer reads:\n'
      '#   * a comment -- the line is prose about a path and writes nothing\n'
      '#   * the HeightMap -- the far heightmap DDS did NOT move, by the ruling,\n'
      '#     and both its writer and the panel row that reports it name the path\n'
      "\t| grep -v ':[0-9]*:[[:space:]]*[*/]' " + C +
      "\t| grep -v 'HeightMap' " + C)
if s.count(a2) != 1:
    print('MISS (e): %d' % s.count(a2))
    sys.exit(1)
s = s.replace(a2, b2)

open(P, 'w', encoding='utf-8', newline='').write(s)
d = open(P, 'rb').read()
print('ok CR %d LF %d' % (d.count(b'\r'), d.count(b'\n')))
