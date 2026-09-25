# CARDFIX1 step 6b: bungo RULED 2026-09-25 (verbatim, via the director): "Yes, 8x8 is the default choice for a
# bake". It replaces the 2026-09-23 ring default for tree bakes: the driver's tree run defaults to the N8 grid
# (RING=0); the 16-view ring stays an option (RING=16) and stays gated. The panel has no ring path (the only
# WW_IMPOSTOR_RING reader is the bake hook). New gate row R7 in impostor_ring.sh: the driver's own library.txt
# says `ring 0` for CANDIDATES=trees with no RING, `ring 16` with RING=16, and the step-5 driver (red) said
# `ring 16` by default. All files LF-only.
import re
R = 'E:/Projects/NifskopeWWE-cardfix1/'


def patch(rel, pairs):
    p = R + rel
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0, rel
    s = b.decode('utf-8')
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:70], s.count(old))
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(p, 'wb').write(out)
    print('patched', rel)


patch('tools/bake_impostor_cards.sh', [
    ('# THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1). bungo 2026-09-23 04:4x, RULED: "for fo4cs\n'
     '# use the convention was 22.5 degrees per take" -- TREE cards are 16 azimuths at elevation 0, one\n'
     '# row (the aggregate\'s ring layout), instead of the OCT x OCT hemi-octahedral grid. So the tree run\n'
     '# defaults to RING=16 and the empty-slot run keeps the grid; RING=0 forces the grid, RING=16 the ring.\n'
     '# With a ring, OCT is not used for the sheets (the library.txt says which one the set carries).\n'
     'if [ "$CANDIDATES" = trees ]; then RING="${RING:-16}"; else RING="${RING:-0}"; fi\n',
     '# THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1) is an OPTION: RING=16 photographs 16 azimuths\n'
     '# at elevation 0 in one row (the aggregate\'s ring layout) instead of the OCT x OCT hemi-octahedral grid.\n'
     '# THE DEFAULT IS THE N8 GRID (RING=0) for every run, trees included. bungo RULED 2026-09-25: "Yes, 8x8\n'
     '# is the default choice for a bake" -- replacing his 2026-09-23 ring default for tree runs, after step 5\n'
     '# measured N8 better than the ring at every elevation, the horizon included (docs/LODGEN_LODM_FORMAT.md\n'
     '# 3.2). With a ring, OCT is not used for the sheets (the library.txt says which one the set carries).\n'
     'RING="${RING:-0}"\n'),
    ('\t*) echo "RING must be 16 (the ruled 22.5-degree ring) or 0 (the grid); got \'${RING}\'" >&2; exit 2 ;;\n',
     '\t*) echo "RING must be 0 (the N8 grid, the default) or 16 (the 22.5-degree horizon ring); got \'${RING}\'" >&2; exit 2 ;;\n'),
])

patch('docs/LODGEN_LODM_FORMAT.md', [
    ('bungo, 2026-09-23 04:4x, RULED: *"for fo4cs use the convention was 22.5 degrees\n'
     'per take"*. **Tree** cards are photographed at 16 azimuths, 22.5 degrees apart, at\n'
     'elevation 0 -- not over the hemi-octahedral grid. The card bake driver makes the\n'
     'ring the default for `CANDIDATES=trees` (`RING=16`); the empty-slot run keeps the\n'
     'grid (`RING=0`), and either can be forced.\n',
     'bungo, 2026-09-23 04:4x, RULED: *"for fo4cs use the convention was 22.5 degrees\n'
     'per take"*. A ring card is photographed at 16 azimuths, 22.5 degrees apart, at\n'
     'elevation 0 -- not over the hemi-octahedral grid. **It is an option, not the\n'
     'default:** bungo RULED 2026-09-25, *"Yes, 8x8 is the default choice for a bake"*,\n'
     'after step 5 measured the N8 grid better than the ring at every elevation, the\n'
     'horizon included. The card bake driver defaults every run, trees included, to the\n'
     'grid (`RING=0`); `RING=16` bakes the ring (`tests/spells/impostor_ring.sh` R7).\n'),
])

patch('docs/LODGEN_CARD_SHEETS.md', [
    ('`WW_IMPOSTOR_OCT`); the driver: `RING=16`, the default for `CANDIDATES=trees`.',
     '`WW_IMPOSTOR_OCT`); the driver: `RING=16`, an option -- the default is the N8 grid\n'
     '(`RING=0`) for every run, trees included (bungo 2026-09-25: "Yes, 8x8 is the default choice for a bake").'),
])

R7 = open(R + 'scratchpad/cardfix1_20260924/fix29_r7.sh.txt', encoding='utf-8').read()
patch('tests/spells/impostor_ring.sh', [
    ('#   M   MEASUREMENT, not a gate: ring16 vs N8 IoU',
     '#   R7  THE DRIVER\'S DEFAULT (bungo RULED 2026-09-25: "Yes, 8x8 is the default choice for a bake"):\n'
     '#       tools/bake_impostor_cards.sh with CANDIDATES=trees and no RING writes `ring 0` in its library.txt;\n'
     '#       RING=16 writes `ring 16` (the option still reaches the bake); RING=5 is refused by name. RED: the\n'
     '#       step-5 driver (git 1303334) wrote `ring 16` with no RING. MAX=0, so nothing is photographed.\n'
     '#   M   MEASUREMENT, not a gate: ring16 vs N8 IoU'),
    ('say "$steps checks, $fails failures"\n', R7 + 'say "$steps checks, $fails failures"\n'),
])
