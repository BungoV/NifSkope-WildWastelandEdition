
---

## Viewer -- opening a `.lodi` as a built document

`File > Open` on a `.lodi` BUILDS a Fallout 4 document, the way a `.btd` and a
`.lodl` do. The file stores no triangles: it is a worldspace's placement table,
and the geometry those placements point at is in the `.lodo` LIBRARY beside it.
So the route reads BOTH files and welds a scene out of them.

  * **The `.lodo` is found by worldspace stem**, `<stem>.lodo` in the `.lodi`'s
    own directory. Missing, and the open is refused in words naming the file it
    wanted -- never an empty document.
  * **The readers are `src/lodifile.cpp` and `src/lodofile.cpp`.** The builder
    (`src/lodinative.cpp`) contains no second parser of either format.
  * **The scene is shaped like a `.BTO`**, on purpose: one `NiNode` root, then
    one `BSTriShape` per **(base, material)** bucket, carrying every placement of
    that base welded into it in WORLD space, with the vertex layout the `.lodo`
    stores carried over -- position, normal, tangent/bitangent, UV and the UV2
    layer -- and the same `BSLightingShaderProperty` plumbing the chunk builder
    writes (`src/lodgen.cpp`). A node per placement would be thousands of nodes
    on one region and would answer no question a bucket does not.
  * **The material is the one the `.BTO` shape carries**, not a new one. The
    `.lodo` material strings are the bake machine's own paths
    (`...\Data\Materials\LOD\<name>.BGSM`); they are passed through with
    separators normalised and NOTHING prepended, because
    `Game::GameManager::get_full_path` finds the archive folder at any `/`
    boundary and erases everything before it -- a prepended `materials\` puts
    the folder at offset 0, the search stops, and the lookup MISSES. The BGSM
    goes in the shader block's **"Name"**, which is what
    `BSShaderLightingProperty::setMaterial` keys on, with the array sheets the
    bake wrote resolved through the usual resource roots.
  * **The vertex descriptor is the full-precision `0x0041B00000650407`**, not
    the `.BTO`'s half-precision object layout: a `.BTO` is chunk-local and its
    16-bit positions cannot hold region-space coordinates.
  * **Region** `WW_LODI_REGION="x0,y0,x1,y1"` in cells, inclusive; unset takes
    the file's whole extent. **Level** `WW_LODI_LEVEL=n` picks the cluster-ladder
    level, 0 (the default) being full detail; a mesh with fewer levels draws its
    own coarsest. Both are refused in words when they do not parse.
  * **The occluder boxes are drawn only under `WW_LODI_BOXES=1`**, and then as
    wire boxes -- they are not geometry the far field shows and a solid box would
    be read as a building.
  * **What it MEASURED is printed**, not just drawn: placements read, drawn,
    outside the region, with no mesh, with no geometry; bases, buckets, shapes,
    vertices, and the time. A picture is never the only evidence that the pair
    was understood.
  * **The instance census is written, not inferred.** `WW_LODI_DUMP=<file>` gets
    one row per placed instance -- `ref part base x y z scale mesh material
    level` -- because a gate cannot count placements by looking at merged
    geometry, and a screen coordinate is not carried between builds. That file
    is the left-hand side a harness compares against the `.lodi` reader and
    against the chunk manifest of the same bake.
  * **Read-only.** Nothing on this route writes a `.lodi`, a `.lodo` or any
    other bake output; `File > Save` on such a document goes to Save As, as it
    does for the other built documents.
  * **Together with the terrain.** `WW_LODL_OBJECTS=<file.lodi>` on a `.lodl`
    open appends these same objects under the terrain's root, defaulting to the
    terrain's own region, so one document carries both halves of the native bake
    (`docs/LODGEN_BTD_FORMAT.md`, "Lit from the `.lodt` sheets").
  * Gate: `tests/spells/native_open.sh`.
