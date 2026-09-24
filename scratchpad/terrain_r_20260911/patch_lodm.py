p = 'docs/LODGEN_LODM_FORMAT.md'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


rep("""Two collisions are resolved here rather than discovered:

* **`family` is vestigial.** A tile pyramid is neither legacy nor PBR. It writes
  `"legacy"` only because §1.1 rule 8 hard-refuses a third family word.
  **`kind` is the discriminator.**
* **Its path is deliberately unreachable from a source lookup.** The index lives
  at `Data\\Terrain\\<EDID>.VT.lodm`, and `lodmSourceCandidate()` always prepends
  `materials\\`, so no shape can ever resolve to it by accident.""",
    """**`family` IS NO LONGER VESTIGIAL HERE** (2026-09-11, bungo: *"you can mirror
how it's set up for the .lodm"*, *"we just add the coverage for whatever's
missing in terrain textures that lod objects have in the texture department"*).

Until container version 2 the pyramid's third sheet was terrain's own invention
— AO, wetness, shore proximity and ground cover — and `family` wrote `"legacy"`
because a tile pyramid was neither family and §1.1 rule 8 hard-refuses a third
word. Version 2 gave the pyramid **this page's own texture family**:

| §2.1 slot | what the pyramid stores | note |
|---|---|---|
| colour | the `color` sheet, RGB albedo with the grass tint folded in | its alpha is FREE, and is the other candidate for the ground cover — see the terrain page §2.2a |
| normal | the `msn` sheet | MODEL-space, not tangent-space: terrain has one basis and needs no tangents. Z IS stored, so the `sqrt(1 - x^2 - y^2)` rebuild does **not** apply |
| mask | the `mask` sheet, **`rmaos` exactly**: R roughness, G metallic, B AO, **A ground cover in place of subsurface** | the one substitution, and the index names it in the sheet's `channels` string |
| emissive | the `emissive` sheet, RGB, BC1 | written only when a layer supplies one; **absent** otherwise, and `terrain.emissive` says `"none"` in words |

Two sheets have no slot on this page and are terrain's own: **height** (R16, the
shadow heightmap's encoding) and nothing else.

So `family` is **`"pbr"`** and it describes the bytes: a legacy landscape
material is CONVERTED at bake — its gloss inverted into roughness, its metallic
0, never guessed from its specular colour — and `terrain.maskRules` counts how
many landscape textures came by which road, so a reader can audit the word
instead of trusting it. `kind` is still the discriminator for WHICH payload
object to read.

* **Its path is deliberately unreachable from a source lookup.** The index lives
  at `Data\\Terrain\\<EDID>.VT.lodm`, and `lodmSourceCandidate()` always prepends
  `materials\\`, so no shape can ever resolve to it by accident.""")

rep("""8. `family` is neither `"legacy"` nor `"pbr"` — **a third family word is a hard
   refusal, not a fallback.** This is why the terrain-VT index says `legacy`
   although it is neither (see §5).""",
    """8. `family` is neither `"legacy"` nor `"pbr"` — **a third family word is a hard
   refusal, not a fallback.** The terrain-VT index used to say `legacy` for that
   reason alone; since 2026-09-11 it says `pbr` and means it (see §5).""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
