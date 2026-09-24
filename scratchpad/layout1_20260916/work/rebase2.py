"""Lane LAYOUT1 (2026-09-16): the harnesses whose OUTPUT paths moved.

Only paths a bake by THIS exe writes are re-based. A fixture baked months ago
(showcase1/water2 trees, the shipped mod folder) is a READ path and stays where
it is; so does every stock-target output, which did not move.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
FO = 'FO4CSLOD/Commonwealth'


def patch(rel, subs):
    p = ROOT + rel
    s = open(p, encoding='utf-8', newline='').read()
    for a, b, n_exp in subs:
        n = s.count(a)
        if n != n_exp:
            print('MISS %d (want %d) in %s: %r' % (n, n_exp, rel, a[:60]))
            sys.exit(1)
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='').write(s)
    d = open(p, 'rb').read()
    print('ok %-32s CR %d LF %d' % (rel, d.count(b'\r'), d.count(b'\n')))


# ---- lodl_write.sh: every .lodl this harness writes ------------------------
patch('tests/spells/lodl_write.sh', [
    ('F="$W/Terrain/Commonwealth.lodl"',
     '# THE .lodl MOVED under one root inside the mod folder (lane LAYOUT1,\n'
     '# 2026-09-16, bungo 19:3x): <out>/FO4CSLOD/<ws>/<ws>.lodl.\n'
     'F="$W/%s/Commonwealth.lodl"' % FO, 1),
    ('F1="$W/v1/Terrain/Commonwealth.lodl"',
     'F1="$W/v1/%s/Commonwealth.lodl"' % FO, 1),
    ('L2="$W/nwa/Terrain/NukaWorldAmphitheater.lodl"',
     'L2="$W/nwa/FO4CSLOD/NukaWorldAmphitheater/NukaWorldAmphitheater.lodl"', 1),
    ('mkdir -p "$W/verify/Terrain"\ncp "$F" "$W/verify/Terrain/Commonwealth.lodl"',
     'mkdir -p "$W/verify/%s"\ncp "$F" "$W/verify/%s/Commonwealth.lodl"' % (FO, FO), 1),
])

# ---- lodl_water.sh --------------------------------------------------------
patch('tests/spells/lodl_water.sh', [
    ('OFF="$W/off/Terrain/Commonwealth.lodl"',
     '# the .lodl moved under FO4CSLOD/<ws>/ (lane LAYOUT1, 2026-09-16)\n'
     'OFF="$W/off/%s/Commonwealth.lodl"' % FO, 1),
    ('FB="$W/fb/Terrain/Commonwealth.lodl"', 'FB="$W/fb/%s/Commonwealth.lodl"' % FO, 1),
    ('V3="$W/v3/Terrain/Commonwealth.lodl"', 'V3="$W/v3/%s/Commonwealth.lodl"' % FO, 1),
    ('V3B="$W/v3b/Terrain/Commonwealth.lodl"', 'V3B="$W/v3b/%s/Commonwealth.lodl"' % FO, 1),
])

# ---- lodl_btd.sh: the worldspace is the .btd's own name --------------------
patch('tests/spells/lodl_btd.sh', [
    ('F="$(ls "$W"/Terrain/*.lodl 2>/dev/null | head -1)"',
     '# the .lodl moved under FO4CSLOD/<ws>/ (lane LAYOUT1, 2026-09-16); the\n'
     '# worldspace folder is named from the .btd, so the glob asks rather than\n'
     '# spells it.\n'
     'F="$(ls "$W"/FO4CSLOD/*/*.lodl 2>/dev/null | head -1)"', 1),
])

# ---- lodgen_terrain_vt.sh: the containers and the index --------------------
patch('tests/spells/lodgen_terrain_vt.sh', [
    ('DIR="$W/run1/mod/Terrain"',
     '# THE CONTAINERS MOVED with every other FO4CS-target file (lane LAYOUT1,\n'
     '# 2026-09-16): <out>/FO4CSLOD/<ws>/<ws>.VT.<dim>.lodt and the index\n'
     '# beside them.\n'
     'DIR="$W/run1/mod/%s"' % FO, 1),
    ('"$W/run2/mod/Terrain/$(basename "$f")"', '"$W/run2/mod/%s/$(basename "$f")"' % FO, 1),
    ('ok "the containers are written under Terrain/, one per level"',
     'ok "the containers are written under FO4CSLOD/<ws>/, one per level"', 1),
    ('bad "the containers are written under Terrain/, one per level"',
     'bad "the containers are written under FO4CSLOD/<ws>/, one per level"', 1),
    ('say "the index is at Terrain\\, which lodmSourceCandidate() cannot produce: it"',
     'say "the index is at FO4CSLOD\\, which lodmSourceCandidate() cannot produce: it"', 1),
    ('say "material, so no source lookup can reach Terrain\\*.VT.lodm."',
     'say "material, so no source lookup can reach FO4CSLOD\\*.VT.lodm."', 1),
])

# ---- lodgen_terrain_pbrm.sh ------------------------------------------------
patch('tests/spells/lodgen_terrain_pbrm.sh', [
    ('"$W/$v/Terrain/Commonwealth.VT.2.lodt"',
     '"$W/$v/%s/Commonwealth.VT.2.lodt"' % FO, 1),
])

print('done')
