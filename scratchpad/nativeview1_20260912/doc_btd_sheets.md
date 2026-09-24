
### Lit from the `.lodt` sheets, when they are beside the file

A `.lodl` carries heights and a per-vertex colour word; it carries no land
textures. The colour word is a *data* view -- it is what the file says, painted
so it can be read -- and it is not what the far field looks like in a game. The
pictures of the far field come from the `.lodt` sheet pyramid the same bake
writes (`docs/LODGEN_TERRAIN_VT.md`), which is where the composited land
textures actually live.

So the open route looks for that pyramid and, when it finds it, **lights the
terrain with it instead of painting the data view**:

  * **Where it looks.** `<worldspace>.VT.<dim>.lodt` in the `.lodl`'s own
    directory, or in `WW_LODL_SHEETS=<dir>`. With several levels present the
    **smallest `dim`** (the finest) is taken; `WW_LODL_SHEET_DIM=<n>` asks for
    one by name. Nothing found, nothing readable, or no colour sheet in the
    container -- the build says which in words and **draws the data view**, so
    the default behaviour of the route is unchanged.
  * **What it does with a tile.** The container's tiles are a packed payload,
    not files a texture loader can open, so each one is unpacked ONCE to a loose
    `.dds` pair (colour, `_msn`) in a cache directory -- the system temporary
    directory by default, `WW_LODL_SHEET_CACHE=<dir>` to put it where a lane can
    look at it afterwards. That directory is pushed onto the session's Fallout 4
    folder list exactly as `WW_LODGEN_RESOURCES` does it in `src/main.cpp`:
    prepended, archives closed so the next lookup re-scans, and never saved --
    `GameManager::save()` is what persists a folder list and only the Settings
    dialog calls it, so a user's own Resources page is untouched.
  * **What the shape gets.** A `BSLightingShaderProperty` with a real
    `BSShaderTextureSet`: the colour sheet in slot 0, the `_msn` in slot 1.
    Sheet row 0 is the NORTH row while the mesh is built row-0-SOUTH, so the
    sheet row is `tilesY - 1 - rowFromSouth`; the border texels are skipped by a
    UV bias and scale rather than by re-cutting the image.
  * **The region snaps outward to whole sheet tiles**, because half a tile has
    no texture of its own; the build prints the widened rectangle. A region that
    snaps entirely outside what the sheets cover falls back to the data view.
  * **The mesh tile size is forced to a divisor of the sheet tile size**, so no
    mesh tile ever straddles two sheets. At LOD 2 the vertex cap allows 7 cells a
    tile and the sheet tile is 4, so the build drops to 4 and says so.
  * **`WW_LODL_PLANE` is unchanged and wins.** Asking for a plane is asking for
    the data view; the sheets are not consulted and the document is the same
    bytes it has always been. This is the module-off identity the gate holds.
  * **Objects in the same document.** `WW_LODL_OBJECTS=<file.lodi>` appends the
    native object LOD under the same root, so one picture can carry the terrain
    and the objects of a region together (`docs/LODGEN_NATIVE_LODO_LODI.md`, the
    Viewer section). Unset, nothing of it runs.
