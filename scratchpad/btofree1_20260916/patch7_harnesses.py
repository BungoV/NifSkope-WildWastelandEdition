# Lane BTOFREE1, 2026-09-16 -- patch 7: the two harnesses that compared a
# `.BTO` produced by a `--native` bake.
#
#   tests/spells/lodgen_native.sh check 5 asks whether the STOCK outputs are the
#   same bytes with and without `--native`. That question is worth keeping
#   exactly as it is, and the way to keep it is to spell the way back on the
#   `--native` side: check 4 now bakes with `--keep-bto`, and a new check reads
#   the chunks it left, so the way back is exercised by the mainline gate rather
#   than only by the new one.
#
#   tests/spells/lodgen_byte_gate.sh phase (c) compares the panel's tree with the
#   command line's file by file. BOTH sides are FO4CS bakes, so both now drop the
#   chunk: the row becomes "neither of them left one, and neither left a scratch
#   folder behind", which is a stronger statement than the old comparison was.
#
# lodgen_defaults.sh and lodgen_ladder.sh were READ and need nothing: defaults
# phases (b) and (c) bake without `--native` (the .BTO is still on disk there),
# phase (e) compares only files under the native directory, and ladder's only
# chunk reference is a `.BTO.manifest.txt`, which is kept.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:90])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-34s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


# ===== lodgen_native.sh =====================================================
NAT = []
NAT.append((
'''if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \\
\t--data-root "$DATA" --out-dir "$WA/native" --native "$WA/native/Native" \\
\t--native-mesh-report "$WA/native/mesh_report.txt"; then''',
'''# `--keep-bto` (lane BTOFREE1, 2026-09-16). From today a `--native` bake builds
# its `.BTO` chunks in a scratch folder and removes them, and check 5 below is
# the question "are the STOCK outputs the same bytes with and without
# `--native`" -- a question worth asking about the chunk files themselves. So
# this arm spells the way back, check 5 keeps comparing chunk for chunk, and the
# DEFAULT (dropping) bake is what tests/spells/lodgen_btofree.sh measures.
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \\
\t--data-root "$DATA" --out-dir "$WA/native" --native "$WA/native/Native" \\
\t--keep-bto \\
\t--native-mesh-report "$WA/native/mesh_report.txt"; then''',
))
NAT.append((
'''[ -s "$W/native/mesh_report.txt" ] && note "the per-mesh report was written" || bad "the per-mesh report was written"''',
'''[ -s "$W/native/mesh_report.txt" ] && note "the per-mesh report was written" || bad "the per-mesh report was written"
# the way back, read off the disk rather than assumed: --keep-bto has to leave
# the chunks in the output folder and leave no scratch folder behind it
KB="$(find "$W/native" -maxdepth 1 -name "*.BTO" | wc -l)"
echo "    --keep-bto left $KB .BTO chunk(s) in the output folder"
[ "$KB" -gt 0 ] && note "--keep-bto leaves the .BTO chunks in the output folder ($KB)" \\
\t|| bad "--keep-bto leaves the .BTO chunks in the output folder (found $KB)"
[ ! -d "$W/native/lodgen_bto_scratch" ] && note "and no .BTO scratch folder is left behind" \\
\t|| bad "and no .BTO scratch folder is left behind"''',
))
patch('tests/spells/lodgen_native.sh', NAT)

# ===== lodgen_byte_gate.sh ==================================================
BG = []
BG.append((
'''\t\tfor f in Commonwealth.4.-20.24.BTR Commonwealth.4.-20.24.BTO \\
\t\t\tCommonwealth.4.-20.24.BTO.manifest.txt; do
\t\t\tcmpone "$BASE/meshes/terrain/Commonwealth/$f" "$CLIS/$f"
\t\tdone''',
'''\t\tfor f in Commonwealth.4.-20.24.BTR Commonwealth.4.-20.24.BTO.manifest.txt; do
\t\t\tcmpone "$BASE/meshes/terrain/Commonwealth/$f" "$CLIS/$f"
\t\tdone
\t\t# THE .BTO IS NOT AN OUTPUT OF EITHER SIDE ANY MORE (lane BTOFREE1,
\t\t# 2026-09-16). Both trees above are FO4CS bakes, so both build their chunks
\t\t# in a scratch folder and remove them. Comparing two files that should not
\t\t# exist would read as MISSING on both sides and count as a failure, so the
\t\t# row asks the question that is actually left: did BOTH front ends drop the
\t\t# chunk, and did BOTH tidy up after themselves? The manifest above is still
\t\t# compared byte for byte, which is what proves the sidecar survived the move.
\t\tpb="$BASE/meshes/terrain/Commonwealth/Commonwealth.4.-20.24.BTO"
\t\tcb="$CLIS/Commonwealth.4.-20.24.BTO"
\t\tif [ ! -e "$pb" ] && [ ! -e "$cb" ]; then
\t\t\techo "  dropped by both: Commonwealth.4.-20.24.BTO"
\t\t\tsame=$(( same + 1 ))
\t\telse
\t\t\techo "  STILL THERE: Commonwealth.4.-20.24.BTO (panel $([ -e "$pb" ] && echo yes || echo no), cli $([ -e "$cb" ] && echo yes || echo no))"
\t\t\tdiff=$(( diff + 1 ))
\t\tfi
\t\tfor d in "$BASE/lodgen_bto_scratch" "$CLIS/lodgen_bto_scratch"; do
\t\t\tif [ -d "$d" ]; then
\t\t\t\techo "  STILL THERE: the scratch folder $d"
\t\t\t\tdiff=$(( diff + 1 ))
\t\t\telse
\t\t\t\tsame=$(( same + 1 ))
\t\t\tfi
\t\tdone''',
))
patch('tests/spells/lodgen_byte_gate.sh', BG)
print('patch7 ok')
