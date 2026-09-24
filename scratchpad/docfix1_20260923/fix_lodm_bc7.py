"""DOCFIX1 LODM 2: card `_n` sheet is BC7 (bungo's ruling 2026-09-23, lane IMPOSTORDEPTH2).
Source: src/lodgen.cpp:3238-3248 (sheets[] {_n, bc7=true}, mask bc7=false); colour 3237 BC3 (bc3=true);
emissive 3250-3253 BC1; writer src/lodgen.cpp:4804-4815 (DX10 header, BC7_UNORM)."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_LODM_FORMAT.md'
OLD = b"| normal | `normal` | `normal` | `_n` / `_n` | BC3 | R = normal X, G = normal Y, B = height, A = sway weight |"
NEW = b"| normal | `normal` | `normal` | `_n` / `_n` | **BC7** (a `DX10` header, `BC7_UNORM`; since 2026-09-23, lane IMPOSTORDEPTH2: BC3 carried the height in the 5:6:5 colour block) | R = normal X, G = normal Y, B = height, A = sway weight |"
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n')
if b.count(OLD) != 1: print('REFUSE', b.count(OLD)); sys.exit(1)
b = b.replace(OLD, NEW)
if b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
