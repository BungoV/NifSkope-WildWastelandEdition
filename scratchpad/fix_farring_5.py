"""FARRING1 step 5: src/nifskope_ui.cpp -- WW_LODGEN_TEST counts the far-ring
rows, and the number-field floor rises with them."""

P = 'src/nifskope_ui.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'nifskope_ui.cpp must be LF-only'
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:240])


# ---- 1. the object-name roll call ------------------------------------------
A = ("\t\t\t\t\t\t\t\t\"LodgenCullCheck\", \"LodgenCullMarginSpin\", \"LodgenSlotFallbackCheck\",\n")
once(s, A)
s = s.replace(A, A +
    "\t\t\t\t\t\t\t\t\"LodgenSimplifyCheck\", \"LodgenSimplify8Spin\", \"LodgenSimplify16Spin\",\n"
    "\t\t\t\t\t\t\t\t\"LodgenSimplify32Spin\", \"LodgenSimplifyErrorSpin\",\n")

# ---- 2. the floor rises with the four new numbers --------------------------
B = ("\t\t\t\t\t\tcheck( \"the numbers are scrub fields, like every other number\", numbers >= 8 && plain == 0 );\n")
once(s, B)
s = s.replace(B,
    "\t\t\t\t\t\t/* The floor rises with every number added, or the count stops\n"
    "\t\t\t\t\t\t * being a floor: 8 before the far-ring rows, 12 with their four. */\n"
    "\t\t\t\t\t\tcheck( \"the numbers are scrub fields, like every other number\", numbers >= 12 && plain == 0 );\n")

# ---- 3. the far-ring behaviour checks, after the target block --------------
C = ("\t\t\t\t\t\t} else {\n"
     "\t\t\t\t\t\t\tcheck( \"the target switches the outputs\", false );\n"
     "\t\t\t\t\t\t}\n")
once(s, C)
snip = open('scratchpad/snip_selftest.cpp', encoding='utf-8').read()
assert snip.count('\r') == 0
s = s.replace(C, C + snip)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('nifskope_ui.cpp: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
