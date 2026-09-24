"""AUDIT1 step 6: a gate for the confirmed bug `--incremental` spelled without
its directory.

MEASURED on the audited exe: `... --native <dir> --incremental` (the flag last)
exits 0 and FULL-bakes, because src/nifcli.cpp:7618 does
`gLgIncremental = next()` and `next()` (src/nifcli.cpp:7424) returns an empty
QString at the end of the argument vector; `gLgIncremental.isEmpty()` then skips
the whole incremental block, refusals and all.  The census says
`native-library-build: rebuilt (not offered: this is not an incremental bake)`
while the operator believes the run was incremental -- the exact lie the code's
own comment at src/nifcli.cpp:3985 forbids ("An --incremental that silently
promoted itself to a full bake would have lied to an operator who is watching a
clock").

The arm goes red on the audited exe and green once the parser refuses.
"""
import os
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_incremental.sh'
s = open(P, encoding='utf-8', newline='').read()
orig = s
T = '\t'

anchor = '''echo
echo "FAILURES: $FAIL"'''
assert s.count(anchor) == 1

arm = '''# =========================== (g) --incremental spelled without its directory
echo
echo "(g) --incremental with no value: it must refuse, not full-bake"
# `--incremental` TAKES a directory. Spelled as the last token it used to parse
# to an empty string, and an empty gLgIncremental skips the whole incremental
# block -- every refusal with it -- so the run full-baked and exited 0 while the
# operator was watching the clock for a cached one. The census said
# `native-library-build: rebuilt (not offered: this is not an incremental bake)`
# and nothing else did.
GD="$W/novalue"
mkdir -p "$GD/tex"
GDW="$(win "$GD")"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \\
''' + T + '''--data-root "$DATA" --out-dir "$GDW" --tex-dir "$GDW/tex" --native "$GDW" \\
''' + T + '''--incremental > "$W/novalue.log" 2>&1
RCG=$?
NG="$(find "$GD" -type f -not -path "$GD/tex/*" | wc -l)"
echo "      rc=$RCG, $NG file(s) written under the out-dir"
if [ "$RCG" -eq 0 ]; then
''' + T + '''bad "(g) --incremental with no value did NOT refuse (rc=0) and wrote $NG file(s)"
elif ! grep -qa -- "--incremental" "$W/novalue.log"; then
''' + T + '''bad "(g) it exits non-zero but the message never names --incremental"
''' + T + '''head -3 "$W/novalue.log" | sed 's/^/        /'
elif [ "$NG" -gt 0 ]; then
''' + T + '''# the floor: refusing AFTER baking is not refusing
''' + T + '''bad "(g) it refused but still wrote $NG file(s) -- the refusal must come before the bake"
else
''' + T + '''note "(g) it refuses (rc=$RCG), names the flag, and writes nothing"
fi

'''

s = s.replace(anchor, arm + anchor, 1)
assert s != orig
assert s.count('\r') == orig.count('\r'), 'line endings moved'
d = os.path.dirname(P)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(s)
f.close()
os.replace(f.name, P)
print('patched %s  %d -> %d bytes, CR %d' % (P, len(orig), len(s), s.count('\r')))
