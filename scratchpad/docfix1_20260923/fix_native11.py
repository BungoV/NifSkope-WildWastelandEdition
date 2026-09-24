"""DOCFIX1 NATIVE 11: the three src/nifcli.cpp anchors in 'Source, as built', re-derived
2026-09-23 (grep of the current tree): 8101 --native, 8107 --native-no-ladder, 3859 lodgenNativeBegin."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_NATIVE_LODO_LODI.md'
reps = [
(b"| `src/nifcli.cpp:5791` | `else if ( t == QLatin1String( \"--native\" ) ) lgNativeDir = next();` |",
 b"| `src/nifcli.cpp:8101` | `else if ( t == QLatin1String( \"--native\" ) ) lgNativeDir = next();` |", 1),
(b"| the CLI: the two ways back | `src/nifcli.cpp:5796` | `else if ( t == QLatin1String( \"--native-no-ladder\" ) ) lgNativeLadder = false;` |",
 b"| the CLI: the ladder switch (OFF by default since 2026-09-17; `--native-ladder` at 8108 turns it on) and the occluders' way back (8109) | `src/nifcli.cpp:8107` | `else if ( t == QLatin1String( \"--native-no-ladder\" ) ) lgNativeLadder = false;` |", 1),
(b"| `src/nifcli.cpp:3430` | `lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel,` |",
 b"| `src/nifcli.cpp:3859` | `lodgenNativeBegin( &world, lodgenFo4csWorldDir( nativeDir, world.worldspaceEdid() ),` |", 1),
]
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n'); bad = False
for old, new, n in reps:
    c = b.count(old)
    if c != n: print('REFUSE', old[:70], c); bad = True; continue
    b = b.replace(old, new)
if bad or b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
