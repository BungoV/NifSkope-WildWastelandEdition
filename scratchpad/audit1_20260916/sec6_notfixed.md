### 6.7 The confirmed defects this lane did NOT fix, and why

Four of the eight CONFIRMED findings in section 4 are not in the exe. Each is a
row for the director, with the reason it is a ruling rather than an audit fix:

| id | why not fixed here | what it needs |
|---|---|---|
| **C3** `src/lodgen.cpp:7902` | the patch is three lines copied verbatim from the sibling guard at `:8367`, but the input that reaches it is a **malformed road `.nif`** and no file in any shipped corpus is malformed. I could not build a gate that goes deterministically red BEFORE the fix -- an out-of-range `QVector` subscript in a release build is undefined, not reliably a crash -- and this lane's rule is that a fix without a red-before gate does not go in | a hostile-input fixture and a second build. The patch text is already written, in section 4.1 |
| **C5** `src/lodgenchunkpass.cpp:156` | the one-line fix (`failed++`) changes an **exit code**: a bake that today exits 0 with a stderr line would start exiting 1, and every harness that reads that exit code changes verdict with it | bungo's ruling, not an audit fix |
| **C6** `src/lodgenchunkpass.cpp:300` | a two-line fix, but it changes what the **census clause** reports (`B bytes freed`), and the census is what `lodgen_btofree.sh` leg (d) reads. Changing a number a gate asserts on, in the same lane that re-runs the gate, buries the evidence | a row for whoever owns the census wording; the mechanism and the repro are in section 4.1 |
| **C7** `src/lodinative.cpp:649` | this is the **viewer**, not the bake. It needs a bucket that rolls over rather than a bounds test, roughly six lines, and the region that proves it is a large tree population in the native view -- a picture, not a gate | a rung of the native-view lane |

**C3 is wider than the one line section 4 found, and this lane measured how
wide.** `src/lodgen.cpp` has ten loops over a `Triangle` array. Six of them
subscript an array with the triangle's own index -- `buried[t.v1()]` (`:3935`),
`remap[t.v1()]` (`:4030`), `s.pos[t.v1()]` (`:4118`), `pos[t.v1()]` (`:4129`),
`sh.pos[t[k]]` (`:7902`, the site section 4 names) and `verts[t.v1()]`
(`:13133`) -- and exactly ONE loop in the whole file validates the index first,
the sibling at `:8367`. The arrays come off the model unchecked: `:2149` is
`s.tris = src.getArray<Triangle>( iTris );` and nothing between there and the
subscripts compares an index against `pos.size()`.

That changes the shape of the fix, which is why it is a row and not a patch
here: the cheap correct place is the LOAD, one test beside `:2149`, not six
guards. A load-time filter is also the one that can move a baked byte -- if any
shipped `.nif` carries such a triangle today, dropping it changes that model's
LOD -- so it needs a corpus measurement first, and a measurement I did not take
is not one I will patch around.

**One SUSPECT closed by measurement while step 6 ran.** S2 said the writer
accepts a placement scale down to 0 while the reader refuses `scale == 0` by
name, and that whether the Commonwealth contains such a placement was where it
would be decided. It does not, in any region this lane baked. The quantised
word (`lround( scale * 8192 )`) read straight out of the four `.lodi` files
with the independent decoder:

| tree | placements | min | max |
|---|---|---|---|
| `sanctuary_fo4cs` | 3,526 | 2048 (0.25) | 16056 (1.96) |
| `coast_fo4cs` | 3,303 | 3277 (0.40) | 19661 (2.40) |
| `urban_fo4cs` | 33,123 | 2130 (0.26) | 16384 (2.00) |
| `aggreal` | 3,526 | 2048 (0.25) | 16056 (1.96) |

43,478 placements, not one zero and nothing within four orders of magnitude of
the 6.1e-5 quantum. S2 stays SUSPECT rather than CONFIRMED, and it stays there
on a number rather than on a hunch: the path exists in the code and no input in
this worldspace reaches it.
