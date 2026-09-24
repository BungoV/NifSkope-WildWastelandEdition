"""DOCFIX1: move the stale Data\\Textures\\Lodgen\\{Cards,Aggregate} game paths to the
FO4CSLOD layout (src/lodgenlayout.cpp lodgenFo4csGameCardPath, src/lodgenaggregate.cpp:184-186).
Exact-count replacements; refuses if any count is off. Byte-level, LF preserved."""
import sys

D = 'E:/Projects/NifskopeWildWastelandEdition/docs/'
edits = {
    'LODGEN_CARD_SHEETS.md': [
        # §1.1 block: re-align the column (old prefix 27 chars, new 23)
        (b'Data\\Textures\\Lodgen\\Cards\\<formid8hex>_oct_d.DDS      legacy   BC3\n',
         b'Data\\FO4CSLOD\\Cards\\<formid8hex>_oct_d.DDS      legacy   BC3\n', 1),
        (b'\n                           <formid8hex>_oct', b'\n                    <formid8hex>_oct', 9),
        # §10.1 aggregate block
        (b'Data\\Textures\\Lodgen\\Aggregate\\<ws>\\<cellX>_<cellY>_agg_d.DDS      legacy  BC3\n',
         b'Data\\FO4CSLOD\\<ws>\\Aggregate\\<cellX>_<cellY>_agg_d.DDS      legacy  BC3\n', 1),
        (b'\n                                     <cellX>_<cellY>_agg', b'\n                             <cellX>_<cellY>_agg', 6),
    ],
    'LODGEN_LODM_FORMAT.md': [
        (b'`Data\\Textures\\Lodgen\\Cards\\<formid8hex>_oct.lodm`', b'`Data\\FO4CSLOD\\Cards\\<formid8hex>_oct.lodm`', 1),
        (b'`Data\\Textures\\Lodgen\\Aggregate\\<worldspace>\\<cellX>_<cellY>_agg.lodm`',
         b'`Data\\FO4CSLOD\\<worldspace>\\Aggregate\\<cellX>_<cellY>_agg.lodm`', 1),
    ],
    'LODGEN_IMPOSTOR_SPEC.md': [
        (b'Files `Data\\Textures\\Lodgen\\Cards\\<formid8hex>_oct_d.DDS`', b'Files `Data\\FO4CSLOD\\Cards\\<formid8hex>_oct_d.DDS`', 1),
    ],
}
check = '--check' in sys.argv
bad = False
out = {}
for f, reps in edits.items():
    b = open(D + f, 'rb').read()
    crlf0 = b.count(b'\r\n')
    for old, new, n in reps:
        c = b.count(old)
        if c != n:
            print(f'REFUSE {f}: {old[:50]!r} count {c} != {n}'); bad = True; continue
        b = b.replace(old, new)
    if b.count(b'\r\n') != crlf0:
        print(f'REFUSE {f}: CR count moved'); bad = True
    out[f] = b
if bad:
    sys.exit(1)
if check:
    print('check OK'); sys.exit(0)
for f, b in out.items():
    open(D + f, 'wb').write(b)
    print(f, 'written', len(b), 'CRLF', b.count(b'\r\n'))
