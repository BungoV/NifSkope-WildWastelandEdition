# Lane BTOFREE1, 2026-09-16 -- patch 8: the plan closes row 7 and rules (k).
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
    print('%-40s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


D = []

D.append((
'''| 7 | **`.BTO` chunk files are still written under the FO4CS target** \u2014 not as an offered output, but because the texture arrays, the card arrays, the shape merge and the far-ring cut all read them back | nothing; but the bake is larger than the plan says it is | LODUI1's call, open in \u00a76 | nothing |''',
'''| 7 | ~~**`.BTO` chunk files are still written under the FO4CS target**~~ \u2014 **CLOSED 2026-09-16, lane BTOFREE1**: the five read-back passes still get their chunk, in `<mod folder>/lodgen_bto_scratch`, and the teardown moves the manifest sidecars into `meshes/terrain/<ws>/` and removes the chunks and the folder. The mod folder receives `.lodl .lodt .lodo .lodi .lodm`, the arrays, the heightmap and the sidecars. Way back: `--keep-bto` / panel row *Keep legacy .BTO chunks* (OFF), byte-identical to a pre-2026-09-16 bake. The stock target is untouched. Gate `tests/spells/lodgen_btofree.sh` | nothing left | \u2014 | nothing |''',
))

D.append((
'''**(k) `.BTO` under the FO4CS target** \u2014 stop writing them, or keep them until the
runtime reads the `.lodo`/`.lodi` pair? (The director's standing recommendation:
keep until FO4CS reads the pair, then drop.)''',
'''**(k) `.BTO` under the FO4CS target** \u2014 **RULED 2026-09-16** (bungo, 2026-09-12
18:3x: "essentially, no legacy vanilla file types are now used by us or baked in
the FO4CS lod bake" \u2014 "Except the data we're reading from for the bakes"). They
are DROPPED. The bake still builds one per chunk, because five passes read it
back, but it builds it in `<mod folder>/lodgen_bto_scratch` and removes it once
they have; the `.BTO.manifest.txt` sidecar moves into the mod folder and stays.
The exact way back is `--keep-bto` on the command line and the panel row *Keep
legacy .BTO chunks* (default OFF, FO4CS only), and it is byte-identical to a bake
from before that date. The stock target does not change at all. Lane BTOFREE1;
gate `tests/spells/lodgen_btofree.sh`.''',
))

patch('docs/FO4CS_IMPROVED_LOD_PLAN.md', D)
print('patch8 ok')
