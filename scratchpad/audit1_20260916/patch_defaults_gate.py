"""AUDIT1 step 6: a gate for the confirmed bug at src/nifcli.cpp:7610 -- a
misspelt `--land-guide` value prints that OFF stands, and then leaves the
DEFAULT (flatwarp:1.0, lane DEFAULTS1) standing.

The message is the bug: telemetry echoes truth, and this line states an outcome
the bake did not have.  The bake itself must not change (that would be a default
change, which this lane is forbidden), so the check is that the warning names
what actually stands, proved against two bakes: the default one and a real
`--land-guide off` one.

New phase (f) of tests/spells/lodgen_defaults.sh; run it alone with
`PHASES=f bash tests/spells/lodgen_defaults.sh`.
"""
import os
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_defaults.sh'
s = open(P, encoding='utf-8', newline='').read()
orig = s
T = '\t'

# the phase list in the usage block and the default value
s = s.replace('PHASES="${PHASES:-abcde}"', 'PHASES="${PHASES:-abcdef}"', 1)
assert 'PHASES="${PHASES:-abcdef}"' in s

anchor = '# ---- (e) the native data does not thin out -----------------------------------'
assert s.count(anchor) == 1

# phase (f) goes AFTER (e): find the end of the file's phase section by
# appending before the summary line
tail_anchor = 'echo "$checks checks, $fails failures"'
n = s.count(tail_anchor)
assert n == 1, 'summary anchor found %d times' % n

phase = '''# ---- (f) a REFUSED switch value says what actually stands --------------------
if [ "${PHASES#*f}" != "$PHASES" ]; then
''' + T + '''echo "== (f) a --land-guide value that is not one of the six =="
''' + T + '''# src/nifcli.cpp:7610 prints "... is not one of off|drag|aspect|aspecthex|
''' + T + '''# slopewarp|flatwarp; off stands" and then calls NOTHING, so what stands is
''' + T + '''# the ruled DEFAULT (flatwarp:1.0), not off. The bake is right and must not
''' + T + '''# move -- changing it would be a default change -- so what is gated is that
''' + T + '''# the sentence names the value that is really in force.
''' + T + '''bake "$NS" f_bad 4 -20 24 -20 24 -- --land-guide flatwrap
''' + T + '''[ -d "$W/f_def" ] || bake "$NS" f_def 4 -20 24 -20 24 --
''' + T + '''bake "$NS" f_off 4 -20 24 -20 24 -- --land-guide off
''' + T + '''# the floor: --land-guide off is a DIFFERENT bake, or nothing below means
''' + T + '''# anything
''' + T + '''if treecmp "$W/f_off" "$W/f_def" "[.]lodb" >/dev/null 2>&1; then
''' + T + T + '''bad "(f) --land-guide off bakes the same bytes as the default -- the floor is gone"
''' + T + '''else
''' + T + T + '''ok "(f) --land-guide off is a different bake from the default (the floor)"
''' + T + '''fi
''' + T + '''WARN="$(grep -a -m1 "is not one of" "$W/f_bad.log")"
''' + T + '''say "the warning: ${WARN:-<none>}"
''' + T + '''if [ -z "$WARN" ]; then
''' + T + T + '''bad "(f) a bad --land-guide value printed no warning at all"
''' + T + '''elif treecmp "$W/f_bad" "$W/f_def" "[.]lodb" >/dev/null 2>&1; then
''' + T + T + '''ok "(f) the bad value left the DEFAULT bake standing, byte for byte"
''' + T + T + '''case "$WARN" in
''' + T + T + '''*"off stands"*)
''' + T + T + T + '''bad "(f) the warning says OFF stands, and the bake is the DEFAULT one, not the off one" ;;
''' + T + T + '''*)
''' + T + T + T + '''ok "(f) the warning does not claim an outcome the bake did not have" ;;
''' + T + T + '''esac
''' + T + '''else
''' + T + T + '''# it changed the bake: then it must be the one the message names
''' + T + T + '''if treecmp "$W/f_bad" "$W/f_off" "[.]lodb" >/dev/null 2>&1; then
''' + T + T + T + '''ok "(f) the bad value really did fall back to off, as the warning says"
''' + T + T + '''else
''' + T + T + T + '''bad "(f) the bad value baked neither the default nor the off bake"
''' + T + T + '''fi
''' + T + '''fi
fi

'''

s = s.replace(tail_anchor, phase + tail_anchor, 1)
assert s != orig
assert s.count('\r') == orig.count('\r'), 'line endings moved'
d = os.path.dirname(P)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(s)
f.close()
os.replace(f.name, P)
print('patched %s  %d -> %d bytes, CR %d' % (P, len(orig), len(s), s.count('\r')))
