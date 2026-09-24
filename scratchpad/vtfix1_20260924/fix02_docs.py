"""VTFIX1 fix02: docs. (3) LODGEN_PARITY's terrain-identity line matches the code; the VT doc's stale
nifcli anchors and the maskRules census note; (4) the ledger doc's generator row. Anchor count == 1, CR count
unchanged per file."""
R = 'E:/Projects/NifskopeWWE-vtfix1/docs/'


def patch(name, subs):
    p = R + name
    with open(p, 'rb') as f:
        src = f.read()
    cr = src.count(b'\r')
    nl = b'\r\n' if cr and cr * 2 > src.count(b'\n') else b'\n'
    for old, new, label in subs:
        o = old.encode('utf-8').replace(b'\n', nl)
        n = new.encode('utf-8').replace(b'\n', nl)
        c = src.count(o)
        assert c == 1, '%s %s: anchor count %d' % (name, label, c)
        src = src.replace(o, n)
        print('applied', name, label)
    grow = src.count(b'\r') - cr
    assert grow == 0 or nl == b'\r\n', '%s: CR count moved in an LF file' % name
    data = src
    with open(p, 'wb') as f:
        f.write(data)
    print(name, 'CR', cr, '->', data.count(b'\r'))


patch('LODGEN_PARITY.md', [(
"""- **CS profiles are opt-in extras**: `--terrain-identity` adds
  COLORS+UV2 (desc `686095322853893`), identity/AO/sway on objects,
  EyeData geomorph, manifests. Default output carries none of it.
""",
"""- **CS profiles are opt-in extras, and there are two of them** (both OFF
  by default since bungo's ruling of 2026-09-12; corrected by lane VTFIX1,
  2026-09-24, against `nifcli.cpp`):
  - `--terrain-identity` (`bool lgTerrainIdentity = false;`, `nifcli.cpp:7666`;
    the panel row ships unticked) puts COLORS+UV2 on the terrain `.BTR`
    (desc `686095322853893`). Default `.BTR` = vanilla's `52776558133763`,
    neutral colours.
  - `--identity` (`bool lgIdentity = false;`, `nifcli.cpp:7549`) puts the
    object-index/AO/sway vertex colours, the UV2 layer and the EyeData
    geomorph on the `.BTO`. Default `.BTO` = the plain `474989027590661`.
  - The manifest sidecar is NOT part of either: it is written either way,
    byte for byte the same. This line used to fold all of it under
    `--terrain-identity` and say default output carried no manifest.
""", 'terrain identity')])

patch('LODGEN_TERRAIN_VT.md', [
("""| `nifcli.cpp:6474` | `bool lgTerrainIdentity = false;` |""",
 """| `nifcli.cpp:7666` (was 6474) | `bool lgTerrainIdentity = false;` |""", 'anchor 6474'),
("""| `nifcli.cpp:6412` | `bool lgIdentity = false;` |""",
 """| `nifcli.cpp:7549` (was 6412) | `bool lgIdentity = false;` |""", 'anchor 6412'),
("""| `nifcli.cpp:7013` | `else if""",
 """| `nifcli.cpp:8335` (was 7013) | `else if""", 'anchor 7013'),
("""measurement. The numbers above are the ones measured on the Sanctuary region
(cells −20..−17 x 24..27) on 2026-09-11.
""",
"""measurement. The numbers above are the ones measured on the Sanctuary region
(cells −20..−17 x 24..27) on 2026-09-11.

**Form 0, the null LTEX, is a layer too, and it counts under `noneDefault`**
(lane VTFIX1, 2026-09-24). A layer whose LTEX is 0, in a chunk with no dominant
base, is painted with the none-default mask constants, and the mask cache keeps
an entry for it. Until VTFIX1 that entry was stored and never counted, so on the
whole Commonwealth `distinctLtex` was 101 against a rule sum of 100 (lane VTBAKE1:
`pbrm 0 + legacyInverted 99 + noneDefault 1`). It now reads `noneDefault 2`, and
the census identity above holds. The gate is `tests/spells/lodgen_vtfix.sh` G1,
which checks the `.lodm` of a whole-map `--vt` bake. A region with no null layer,
such as Sanctuary's 14 = 14, does not move.
""", 'maskRules form 0'),
])

