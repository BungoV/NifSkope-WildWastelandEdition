"""AUDIT1: replace the refuter's sed with an awk that CANNOT be a no-op.

The sed set the first hex digit of a recorded .BTR digest to `f`, which doctors
nothing on a record whose digest already starts with `f` -- a refuter that is
silently vacuous one time in sixteen. The awk swaps that digit between 0 and 1,
so it always moves, and the `cmp` guard stays as the second belt."""
import io
import os

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native.sh'
s = io.open(p, encoding='utf-8', newline='').read()

old = """\tsed '0,/^out\\t.*\\.BTR\\t/s/\\(^out\\t.*\\.BTR\\t\\)\\([0-9a-f]\\)/\\1f/' "$NREC" > "$W/native_doctored.lodb"
"""
new = """\t# the first recorded .BTR digest, its leading hex digit swapped between 0 and
\t# 1 -- a flip to a FIXED digit is a no-op one record in sixteen, which is a
\t# refuter that quietly proves nothing.
\tawk -F'\\t' 'BEGIN { OFS = "\\t" }
\t\t!done && $1 == "out" && $4 ~ /\\.BTR$/ {
\t\t\t$5 = (substr($5, 1, 1) == "0" ? "1" : "0") substr($5, 2); done = 1 }
\t\t{ print }' "$NREC" > "$W/native_doctored.lodb"
"""
assert s.count(old) == 1, 'the sed line was not found verbatim'
s = s.replace(old, new)
tmp = p + '.tmp'
io.open(tmp, 'w', encoding='utf-8', newline='').write(s)
os.replace(tmp, p)
print('patched')
