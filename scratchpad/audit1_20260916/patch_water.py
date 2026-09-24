import ast
import io

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_lodl_pyramid.py'
s = io.open(p, encoding='utf-8').read()

old = s[s.index("    # ---- C: the water table"):s.index("    # ---- D: the shore section")]
new = '''    # ---- C: the water fields -----------------------------------------------
    #
    # NOT "the table is inside the cell height range". That is not an invariant
    # of this format and must not be asserted: docs/LODGEN_BTD_FORMAT.md says
    # the height is RESOLVED (the cell XCLW, else the worldspace DNAM default)
    # and that whether a plane is DRAWN is a second question the file does not
    # answer. Measured on the whole Commonwealth, 2026-09-17: 20,340 cells sit
    # UNDER the default ocean plane and 14,586 sit above it, and both are the
    # file telling the truth. What the doc DOES bind is checked here.
    t.f.seek(0x98)
    defW, defT = struct.unpack('<fI', t.f.read(8)) if t.version >= 2 else (0.0, 0)
    SENTINELS = (0xFF7FFFFF, 0x7F7FFFFF, 0x4F7FFFC9)
    wet = dry = drybad = sent = badtype = above = below = 0
    for cy in range(t.minY, t.maxY + 1):
        for cx in range(t.minX, t.maxX + 1):
            lo, hi, wh, wt, fl = t.cell(cx, cy)
            if not (fl & 1):
                dry += 1
                if wh != 0.0 or wt != 0xFFFF:
                    drybad += 1
                continue
            wet += 1
            if struct.unpack('<I', struct.pack('<f', wh))[0] in SENTINELS:
                sent += 1
            if wt != 0xFFFF and wt >= t.nWatr:
                badtype += 1
            if wh > hi:
                above += 1
            elif wh < lo:
                below += 1
    ck.check('C1 every cell WITHOUT water writes height 0 and type 0xFFFF '
             '(%d dry cell(s), %d broke it)' % (dry, drybad), drybad == 0)
    ck.check('C2 no stored water height is one of the three no-water sentinels '
             '(%d water cell(s), %d sentinel)' % (wet, sent), sent == 0)
    ck.check('C3 every interned water type is inside the WATR table '
             '(%d type(s) in the table, %d row(s) past it)' % (t.nWatr, badtype),
             badtype == 0)
    print('    water heights, for the record: worldspace default %.1f (type %08X); '
          '%d cell(s) sit under their plane, %d over it -- both legal, and the reason '
          'the "table inside the cell range" rule is NOT asserted here'
          % (defW, defT, above, below))
    if wet == 0:
        print('    NOTE: not one cell carries the water flag, so C checked nothing. '
              'That is a fact about the region, not a pass.')

'''
assert old != new
s = s.replace(old, new)
s = s.replace('import os\nimport sys', 'import os\nimport struct\nimport sys')

doc_old = """  C  THE WATER TABLE.  For every cell the section flags say carries water,
     the stored water height is inside the cell's own [min, max] height band
     widened by nothing -- a water plane above every land sample in the cell,
     or below every one of them, is a cell where the table cannot be right.
     Cells WITHOUT water are counted and skipped, and the count is printed, so
     a worldspace whose water flag is vacuous reads as `0 checked` rather than
     as a pass."""
doc_new = """  C  THE WATER FIELDS, by the rule the format doc actually states.  A cell
     without water writes height 0 and type `0xFFFF`; no stored height is one
     of the three no-water sentinels; every interned type is inside the WATR
     table.  The audit brief's "water table inside the cell's height range" is
     NOT asserted, and must not be: the height is RESOLVED (the cell `XCLW`,
     else the worldspace `DNAM` default), and 20,340 Commonwealth cells sit
     under their own plane while 14,586 sit over it.  Both counts are printed
     instead, because they are a fact about the worldspace, not a verdict."""
assert s.count(doc_old) == 1
s = s.replace(doc_old, doc_new)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
ast.parse(s)
print('water section rewritten and parses')
