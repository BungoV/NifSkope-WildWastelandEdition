# CARDFIX1 step 6, after impostor_wind.sh run 1 (gates/impostor_wind.run1.out):
# (1) compress() said "$1" after `shift 2` -> set -u killed the script at the first compress.
# (2) G1a was pre-registered on the elm, which has NO shape without the tree-animation flag (one shape,
#     probed before the bars and not read): 0 mask-0 texels, so the row has no population there. It is now
#     a named N/A line (not counted as ok, not as a failure); maple and pine carry the row, and the red
#     control stands. MISTAKES text: a subject pre-registered for a row without checking it HAS the row's
#     population.
P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_wind.sh'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')


def rep(o, n):
    global s
    assert s.count(o) == 1, (o[:60], s.count(o))
    s = s.replace(o, n)


rep('\tO="$WORK/$1"; cx="$2"; shift 2\n', '\tct="$1"; O="$WORK/$1"; cx="$2"; shift 2\n')
rep('\tsay "compress $1: lodgen rc $?;', '\tsay "compress $ct: lodgen rc $?;')
rep('\tif ge "${z:--1}" 0.95; then ok "G1a $s: mask-0 texels carry A = 0 on $z (>= 0.95)"\n',
    '\tm0=$( echo "$v" | grep -oE "^mask0 [0-9]+" | awk \'{print $2}\' )\n'
    '\tif [ "${m0:-0}" = 0 ]; then say "  n/a   G1a $s: no mask-0 texels -- every shape of this model carries the tree-animation flag (not counted)"\n'
    '\telif ge "${z:--1}" 0.95; then ok "G1a $s: mask-0 texels carry A = 0 on $z (>= 0.95)"\n')
rep('#       a  mask-0 texels (the trunk: no tree-animation flag) carry A = 0 on >= 95 % (the AA edge allowance).\n',
    '#       a  mask-0 texels (the trunk: no tree-animation flag) carry A = 0 on >= 95 % (the AA edge allowance).\n'
    '#          A model with NO such shape (the elm: one shape) has no population: a named n/a line, not a pass\n'
    '#          (run 1 counted it as a failure; the bar was pre-registered without reading the elm\'s shape list).\n')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
