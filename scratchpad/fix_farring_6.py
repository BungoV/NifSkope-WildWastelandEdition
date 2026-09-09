"""FARRING1 step 6: docs/LODGEN_IMPOSTOR_SPEC.md gains the Far rings paragraph,
and docs/LODGEN_VERTEX_PACKING.md says what a simplifier owes the channels."""

P = 'docs/LODGEN_IMPOSTOR_SPEC.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')

A = "## What is not in the textures\n"
n = s.count(A)
assert n == 1, 'anchor matched %d times' % n
snip = open('scratchpad/snip_spec.md', encoding='utf-8').read()
assert snip.count('\r') == 0
s = s.replace(A, snip + '\n' + A)
out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('%s: %d -> %d bytes' % (P, len(b), len(out)))

# ---------------------------------------------------------------- packing doc
P2 = 'docs/LODGEN_VERTEX_PACKING.md'
b2 = open(P2, 'rb').read()
assert b2.count(b'\r') == 0
s2 = b2.decode('utf-8')

B = "### Impostor cards and texture arrays: the LOD texture spec\n"
n = s2.count(B)
assert n == 1, 'anchor matched %d times' % n
ADD = """### What the far-ring simplifier owes this table

`lodgenSimplifyFarRings` decimates rings 2 and 3 after the merge, and every
slot above has to survive it. Two of them are INDICES and one is a layer, so
the pass groups triangles by (identity index, UV2.y layer) and simplifies each
group alone: no collapse crosses an object or a texture layer, and because
meshoptimizer creates no vertices the survivors are a SUBSET of the originals
— the identity index, the AO byte, the sway byte, the sky fraction and the
ground blend all arrive as they were written, never blended. The quantities
also ride as weighted attributes so the metric prefers to keep them. Every
group is asked for at least two triangles and restored whole if the simplifier
returns nothing, so **the set of identity indices in a chunk is invariant under
the pass** — a manifest row can never point at an object that is no longer
there. Alpha-tested shapes and impostor cards are not cut at all.

That invariant is the reason a consumer may key on the manifest across rings:
`(ref, part)` still resolves at ring 2 and ring 3 after the cut.

"""
s2 = s2.replace(B, ADD + B)
out2 = s2.encode('utf-8')
assert out2.count(b'\r') == 0
open(P2, 'wb').write(out2)
print('%s: %d -> %d bytes' % (P2, len(b2), len(out2)))
