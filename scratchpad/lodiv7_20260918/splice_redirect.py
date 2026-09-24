# Director REDIRECT, bungo 2026-09-18 17:4x, items (1) and (2).
p = 'docs/LODGEN_NATIVE_LODO_LODI.md'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


# ---- 4.1c: name the shadow key, and name what is NOT it ----------------------
rep("""themselves"* — the far-shadow pass keys on the identity index, so it must survive
into the native output.

There are two identities and the difference matters:""",
    """themselves"* — the far-shadow pass keys on an identity AND excludes
self-shadowing by it, so that word must survive into the native output.

**As of v7 the word it keys on is the GROUP (§4.9), and that is a correction to
what this section said before, made by the director on 2026-09-18 17:4x.** The
exclusion is what forces it. A kit-built house is not one placement: chunk
4.4.-12 holds one that is **205** separate wall, roof, floor and garage
placements. Give each its own caster identity and the pass excludes each piece
only from ITSELF, so the house's own front wall casts a far shadow across its
own roof — which is precisely the artifact the exclusion exists to prevent.
**One house, one SCOL, one tree = one caster.** A group shared by two DIFFERENT
objects is a wrong shadow; pieces of one object sharing a group is the point.

There are now three words, and the difference matters:

* **The caster identity is the GROUP**, `u16 group[i]` (§4.9), dense per chunk.
  It is the only one of the three the far-shadow pass may key on.""")

rep("""* **The format's own per-placement identity is the instance INDEX**, a u32,
  unique across the whole file by construction.""",
    """* **The per-placement identity is the instance INDEX**, a u32, unique across
  the whole file by construction — and it is **NOT the shadow key**. It is what
  a picker, a manifest join and `check_manifest`'s uniqueness gate need, and v7
  leaves it exactly as it was.""")

rep("""* **`cold[i].identity` is the STOCK bake's index** for that placement:""",
    """* **`cold[i].identity` is the STOCK bake's index** for that placement, and it
  is **NOT the shadow key either**:""")

# ---- 4.9: say the same thing where the table is defined ----------------------
rep("""v7 carries a SECOND word beside it.

**`identity` is not touched.**""",
    """v7 carries a SECOND word beside it.

**THE GROUP IS THE IDENTITY THE FAR-SHADOW PASS KEYS ON** (director, 2026-09-18
17:4x; §4.1c). That pass excludes self-shadowing by identity — bungo,
2026-09-11 08:4x: *"they can only occlude other objects and terrain, never
themselves"* — so the unit the identity names has to be the unit that must not
shadow itself. **One house, one SCOL, one tree = one caster.** A house split
into forty identities would shadow its own walls; that is the artifact the rule
below exists to prevent, and it is why a collision between two different groups
is a wrong shadow while forty pieces sharing one group is correct. The
per-placement instance index and `cold[i].identity` stay exactly as they were,
for the manifests and the stock join, and neither is the shadow key.

**`identity` is not touched.**""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('s4.1c + s4.9 shadow-key wording done')

# ---- the census word ---------------------------------------------------------
p2 = 'docs/LODGEN_CENSUS.md'
s = open(p2, encoding='utf-8', newline='').read()
rep("""| `shadowIdentityUnique` | 0/1 | whether every far caster drawn this frame carried a unique identity index. The far-shadow pass keys on identity and excludes self-shadowing (bungo 2026-09-11 08:4x), so a collision is a wrong shadow, not a slow one | NATIVE 4.1c | doctor a duplicate identity into the `.lodi`: it must read 0 | `unchecked` | `unchecked` |""",
    """| `shadowIdentityUnique` | 0/1 | whether every far caster drawn this frame carried a unique **GROUP** (`.lodi` v7, NATIVE 4.9) — **re-worded by the director 2026-09-18 17:4x, and the change is a change of MEANING, not of wording**. The far-shadow pass keys on the caster identity and excludes self-shadowing by it (bungo 2026-09-11 08:4x), so the unit counted here must be the unit that must not shadow itself: one house, one SCOL, one tree. Two different groups colliding on one id is a wrong shadow; the 205 pieces of one kit house sharing a group is the point, and under the old per-placement reading this word would have called that correct case a collision 205 times over. On a file with no group table (v3–6) it falls back to the per-placement instance index and **says so**, because a silent fallback would make a v6 file read like a v7 one. The instance index and `cold[i].identity` are NOT the shadow key (NATIVE 4.1c) | NATIVE 4.9, 4.1c | doctor two different groups onto one id in the `.lodi`: it must read 0 | `unchecked` | `unchecked` |""")
open(p2, 'w', encoding='utf-8', newline='').write(s)
print('shadowIdentityUnique re-worded')
