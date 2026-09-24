"""AUDIT1 step 6: lodgen_roads.sh R1 compares a record of the RUN as if it
were output.

MEASURED (scratchpad/audit1_20260916/roadsdbg, two --no-roads bakes of the same
region): 1 of 10 files differs, and it is `obj/Commonwealth.lodb`, the BAKEREC1
record.  Its four differing lines are the bake time (`baked`), the run's own
--out-dir/--vt/--tex-dir paths echoed as `switch` lines, and the `census` line
carrying the peak working set in bytes and the layout path.  Every `chunk` and
`out` content hash in the two records is identical, and so is every other
output file -- the bake IS deterministic.  So the gate is stale (the bake
record landed after R1 was written), not the product.

The fix: compare the record on its content lines, not byte for byte, and keep
a floor -- the --roads bake's hashes must NOT match the --no-roads ones, so
the comparison is shown to be able to fail.
"""
import os
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_roads.sh'
s = open(P, encoding='utf-8', newline='').read()
orig = s
T = '\t'


def sub(old, new, count=1):
    global s
    n = s.count(old)
    assert n == count, 'anchor %r found %d times (want %d)' % (old[:70], n, count)
    s = s.replace(old, new)


old_r1 = (
    'echo "== R1 the OFF value is exact =="\n'
    'n=0; d=0\n'
    'for f in $(cd "$W/roadOff" && find . -type f | sort); do\n'
    + T + 'n=$((n + 1))\n'
    + T + 'cmp -s "$W/roadOff/$f" "$W/roadOff2/$f" || { d=$((d + 1)); say "differs: $f"; }\n'
    'done\n'
    '[ "$n" -ge 6 ] || bad "R1 the bake produced files to compare ($n)"\n'
    '[ "$n" -ge 6 ] && [ "$d" -eq 0 ] \\\n'
    + T + '&& ok "R1 two --no-roads runs are byte-identical, $n files" \\\n'
    + T + '|| bad "R1 two --no-roads runs are byte-identical ($d of $n differ)"\n'
)

new_r1 = (
    'echo "== R1 the OFF value is exact =="\n'
    '# The BAKEREC1 record (.lodb) is a record of the RUN, not an output of it:\n'
    '# it carries the bake time, this run\'s own --out-dir/--vt/--tex-dir paths\n'
    '# echoed as `switch` lines, and a census line with the peak working set in\n'
    '# bytes.  Two identical bakes differ there BY DESIGN, so it is compared on\n'
    '# its content lines below instead of byte for byte (measured 2026-09-17:\n'
    '# those four lines were the only difference, every chunk and out hash matched).\n'
    'n=0; d=0\n'
    'for f in $(cd "$W/roadOff" && find . -type f | sort); do\n'
    + T + 'case "$f" in *.lodb) continue;; esac\n'
    + T + 'n=$((n + 1))\n'
    + T + 'cmp -s "$W/roadOff/$f" "$W/roadOff2/$f" || { d=$((d + 1)); say "differs: $f"; }\n'
    'done\n'
    '[ "$n" -ge 6 ] || bad "R1 the bake produced files to compare ($n)"\n'
    '[ "$n" -ge 6 ] && [ "$d" -eq 0 ] \\\n'
    + T + '&& ok "R1 two --no-roads runs are byte-identical, $n files (the .lodb record apart)" \\\n'
    + T + '|| bad "R1 two --no-roads runs are byte-identical ($d of $n differ)"\n'
    '\n'
    '# the record\'s content: every chunk and output hash it names\n'
    'recHashes() { grep -aE "^(chunk|out)$(printf \'\\\\t\')" "$1" | sort; }\n'
    'rec1="$W/roadOff/obj/Commonwealth.lodb"; rec2="$W/roadOff2/obj/Commonwealth.lodb"; recOn="$W/roadOn/obj/Commonwealth.lodb"\n'
    'if [ -f "$rec1" ] && [ -f "$rec2" ] && [ -f "$recOn" ]; then\n'
    + T + 'rd=$(diff <(recHashes "$rec1") <(recHashes "$rec2") | grep -c "^[<>]")\n'
    + T + 'rn=$(diff <(recHashes "$rec1") <(recHashes "$recOn") | grep -c "^[<>]")\n'
    + T + '[ "$rd" -eq 0 ] \\\n'
    + T + T + '&& ok "R1 the two records name the same chunk and output hashes ($(recHashes "$rec1" | wc -l) lines)" \\\n'
    + T + T + '|| bad "R1 the two records name the same chunk and output hashes ($rd lines differ)"\n'
    + T + '# the floor: a DIFFERENT bake must move those hashes, or the check above\n'
    + T + '# compares nothing\n'
    + T + '[ "$rn" -gt 0 ] \\\n'
    + T + T + '&& ok "R1 the hash comparison can fail: --roads names other hashes ($rn lines differ)" \\\n'
    + T + T + '|| bad "R1 the hash comparison can fail: --roads names other hashes"\n'
    'else\n'
    + T + 'bad "R1 the bake records exist"\n'
    'fi\n'
)

sub(old_r1, new_r1)
assert s != orig
assert s.count('\r') == orig.count('\r'), 'line endings moved'
d = os.path.dirname(P)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(s)
f.close()
os.replace(f.name, P)
print('patched %s  %d -> %d bytes, CR %d' % (P, len(orig), len(s), s.count('\r')))
