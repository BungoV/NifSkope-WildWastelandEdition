p = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/shots.sh'
s = open(p, 'rb').read().decode('utf-8')

old = '''CEN="-73728,106496,0"
LCEN="8192,8192,0"          # the .BTR's own space: its shape sits at 0,0,0
ORT=8192'''
new = '''CEN="-73728,106496,0"
LCEN="8192,8192,0"          # the .BTR's own space: its shape sits at 0,0,0
# An OBLIQUE view needs the look-at at the land's own height or the picture is
# mostly empty sky: this chunk's terrain sits around z 8200 (the .BTR's
# vertices read 2052 through a shape scale of 4, and the .lodi's own placements
# read 8027..10032), so both obliques look at z 8500.
CENO="-73728,106496,8500"
LCENO="8192,8192,8500"
ORT=8192'''
assert s.count(old) == 1, 'centre anchor'
s = s.replace(old, new)

pairs = [
    ('\t[ "$v" = 1 ] && n=i_native_both_top || n=i_native_both_obl\n'
     '\tshot "$n" "$LODL" "$v" "$CEN" "$ORT" ',
     '\tif [ "$v" = 1 ]; then n=i_native_both_top; c="$CEN"; '
     'else n=i_native_both_obl; c="$CENO"; fi\n'
     '\tshot "$n" "$LODL" "$v" "$c" "$ORT" '),
    ('\t[ "$v" = 1 ] && n=ii_native_obj_top || n=ii_native_obj_obl\n'
     '\tshot "$n" "$LODI" "$v" "$CEN" "$ORT" ',
     '\tif [ "$v" = 1 ]; then n=ii_native_obj_top; c="$CEN"; '
     'else n=ii_native_obj_obl; c="$CENO"; fi\n'
     '\tshot "$n" "$LODI" "$v" "$c" "$ORT" '),
    ('\t[ "$v" = 1 ] && s=top || s=obl\n'
     '\tshot "legacy_btr_$s" "$OBJ/Commonwealth.4.-20.24.BTR" "$v" "$LCEN" "$ORT" ',
     '\tif [ "$v" = 1 ]; then s=top; lc="$LCEN"; c="$CEN"; '
     'else s=obl; lc="$LCENO"; c="$CENO"; fi\n'
     '\tshot "legacy_btr_$s" "$OBJ/Commonwealth.4.-20.24.BTR" "$v" "$lc" "$ORT" '),
    ('\tshot "legacy_bto_$s" "$OBJ/Commonwealth.4.-20.24.BTO" "$v" "$CEN" "$ORT" ',
     '\tshot "legacy_bto_$s" "$OBJ/Commonwealth.4.-20.24.BTO" "$v" "$c" "$ORT" '),
]
for old, new in pairs:
    assert s.count(old) == 1, 'anchor %r count %d' % (old[:40], s.count(old))
    s = s.replace(old, new)

open(p, 'wb').write(s.encode('utf-8'))
print('shots.sh patched')
