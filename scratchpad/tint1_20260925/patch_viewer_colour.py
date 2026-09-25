"""TINT1: the native far field drew a .lodo v5 mesh's colour into a vertex row that had no colour field.
src/lodinative.cpp gave the vertex descriptor VF_COLORS only for a CHANNEL colour (withColour), so the
library colour (libColour) was written through an invalid index and dropped: v4 and v5 rendered byte-identical.
Anchor count 1, CR count unchanged."""
p = 'src/lodinative.cpp'
b = open(p, 'rb').read(); cr0 = b.count(b'\r')
nl = b'\r\n' if b.count(b'\r\n') > b.count(b'\n') // 2 else b'\n'
old = (b'\tif ( b.withColour )' + nl + b'\t\tdesc.SetFlag( VertexFlags::VF_COLORS );' + nl +
       b'\tif ( b.layer >= 0 || b.withColour )' + nl + b'\t\tdesc.ResetAttributeOffsets( 130 );')
new = (b'\t// `.lodo` v5: the library colour needs the field too, or its values are written to nothing' + nl +
       b'\tif ( b.withColour || b.libColour )' + nl + b'\t\tdesc.SetFlag( VertexFlags::VF_COLORS );' + nl +
       b'\tif ( b.layer >= 0 || b.withColour || b.libColour )' + nl + b'\t\tdesc.ResetAttributeOffsets( 130 );')
assert b.count(old) == 1, b.count(old)
b = b.replace(old, new)
assert b.count(b'\r') == cr0 + (1 if nl == b'\r\n' else 0), (cr0, b.count(b'\r'))
open(p, 'wb').write(b); print('patched, CR', cr0, '->', b.count(b'\r'), 'newline', nl)
