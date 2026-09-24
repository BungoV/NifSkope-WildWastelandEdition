"""DEFAULTS1: re-base the harnesses on the new defaults by SPELLING the switch.

Each harness below measures something that used to come for free and now has to
be asked for. Nothing about what they measure changes -- only the command line.

Refusing: every anchor must be found exactly once, the replacement must not be
present already, and the CR count must stay 0.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/'

# (file, anchor, replacement)
EDITS = [
    # --- lodgen_terrain: the wide land descriptor, and the object identity ----
    ('lodgen_terrain.sh',
     '--terrain -20 24 --dim 4 -o "$GEN"',
     '--terrain -20 24 --dim 4 --terrain-identity -o "$GEN"'),
    ('lodgen_terrain.sh',
     '--objects -20 24 --dim 4 -o "$OBJ"',
     '--objects -20 24 --dim 4 --identity -o "$OBJ"'),
    ('lodgen_terrain.sh',
     '# The CS terrain profile is ON by default now, so the Land descriptor is',
     '# The CS terrain profile is OFF by default since 2026-09-12 (bungo\'s\n'
     '# ruling), so the bake above ASKS for it with --terrain-identity and the\n'
     '# Land descriptor is then DELIBERATELY wider than vanilla\'s -- it carries'),
    ('lodgen_terrain.sh',
     '# DELIBERATELY wider than vanilla\'s -- it carries material class, wetness, AO,',
     '# material class, wetness, AO,'),

    # --- tree sway lives in the vertex ALPHA, which is identity data ---------
    ('lodgen_tree_sway.sh',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 \\',
     '# Sway rides in the vertex ALPHA, which is identity data: OFF by default\n'
     '# since 2026-09-12, so this harness spells --identity.\n'
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 --identity \\'),

    # --- the far-ring cut groups by (identity index, UV2 layer) --------------
    ('lodgen_farring.sh',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim "$dim" \\',
     '# The ring cut groups by (identity index, UV2.y layer) and this harness\n'
     '\t# reads both back, so it spells --identity (off by default since\n'
     '\t# 2026-09-12).\n'
     '\t"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim "$dim" --identity \\'),

    # --- the texture arrays check the layer PER VERTEX in UV2.y -------------
    ('lodgen_texture_arrays.sh',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao \\\n'
     '\t--out-dir "$W/obj" ',
     '# Every A line is checked against the layer carried PER VERTEX in UV2.y,\n'
     '# which is identity data and off by default since 2026-09-12.\n'
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao --identity \\\n'
     '\t--out-dir "$W/obj" '),
    ('lodgen_texture_arrays.sh',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao \\\n'
     '\t--out-dir "$W/obj2" ',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao --identity \\\n'
     '\t--out-dir "$W/obj2" '),

    # --- the VT harness is pre-registered on the OLD land look --------------
    # MEASURED (scratchpad/defaults1_20260912/v9a_probe.sh): with the ruled land
    # default the pyramid-assembled colour sheet and the direct bake stop being
    # byte-identical on 2 of the 4 dim-4 chunks -- 4 bytes of 174,888 on
    # (-24,28) and 27 on (-20,28). --land-warp 0 alone restores it, and so does
    # --land-guide off: the warp offset is what reaches past a tile's own
    # footprint. Every one of this file's 41 checks was pre-registered against
    # the old look, so the whole harness is spelled back onto it rather than
    # one bar being loosened. The shipped default is gated instead by
    # tests/spells/lodgen_defaults.sh phase (a), byte for byte against the rung.
    ('lodgen_terrain_vt.sh',
     '\t\t--out-dir "$W/$name/obj" --data-root "$DATA" "$@" > "$W/$name.log" 2>&1',
     '\t\t--out-dir "$W/$name/obj" --data-root "$DATA" \\\n'
     '\t\t--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off \\\n'
     '\t\t"$@" > "$W/$name.log" 2>&1'),

    # --- the road-presence bars were calibrated with the verge painted ------
    ('lodgen_roads.sh',
     '--vt "$WA/$name/mod" --tex-dir "$WA/$name/tex" --cover "$@" \\',
     '--vt "$WA/$name/mod" --tex-dir "$WA/$name/tex" --cover \\\n'
     '\t\t--road-ground-paint 1 "$@" \\'),
]

COUNTS = {}
for fn, anchor, repl in EDITS:
    p = ROOT + fn
    raw = open(p, 'rb').read()
    text = raw.decode('utf-8')
    if fn not in COUNTS:
        COUNTS[fn] = (len(raw), raw.count(b'\n'), raw.count(b'\r'))
    n = text.count(anchor)
    if n != 1:
        print('REFUSED %s: anchor %r found %d times' % (fn, anchor[:60], n))
        sys.exit(2)
    text = text.replace(anchor, repl)
    out = text.encode('utf-8')
    if out.count(b'\r') != COUNTS[fn][2]:
        print('REFUSED %s: CR count moved' % fn)
        sys.exit(2)
    open(p, 'wb').write(out)

for fn, before in COUNTS.items():
    raw = open(ROOT + fn, 'rb').read()
    print('%-28s %6d -> %6d B   LF %4d -> %4d   CR %d' %
          (fn, before[0], len(raw), before[1], raw.count(b'\n'), raw.count(b'\r')))
print('ok')
