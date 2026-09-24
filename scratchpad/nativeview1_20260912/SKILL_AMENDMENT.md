# Amendment owed to `.claude/skills/nifskope-ww-render-shot/SKILL.md`

Lane NATIVEVIEW1, 2026-09-12. Two things the skill does not say and one number
it needs. Splice as a new section after the camera section.

---

## Photographing a BUILT document (`.lodl`, `.lodi`, `.btd`)

These files carry no triangles. The document is BUILT on open out of a region,
a level and (for `.lodi`) the library file beside it, so the picture depends on
environment the skill's recipe does not mention, and a wrong value produces a
perfectly convincing wrong picture.

```
WW_LODL_REGION="x0,y0,x1,y1,lod"      the landscape region and level
WW_LODL_PLANE=<key>                    a DATA view; unset = the lit/height view
WW_LODL_SHEETS=<dir>                   where the .lodt pyramid is
WW_LODL_OBJECTS=<file.lodi>            add the native objects to the same document
WW_LODI_REGION="x0,y0,x1,y1"           the object region, cells, inclusive
WW_LODI_LEVEL=n                        the cluster-ladder level, 0 = finest
WW_LODI_DUMP=<file>                    the instance census, one row per placement
```

**The notes are part of the picture's evidence.** Each of these routes prints
what it MEASURED — placements read and drawn, sheet tiles unpacked, the region
it actually used after snapping. Capture stdout with the shot and read it. A
`.lodl` that fell back to the data view and a `.lodl` lit from its sheets are
both handsome; only the notes say which one you have.

**Pin the camera off the region's own arithmetic, never off a remembered screen
position.** A chunk of `DIM` cells at cell `(CX, CY)` is world
`X = CX*4096 .. (CX+DIM)*4096`, so:

```
WW_RENDER_CENTER="$(( (2*CX+DIM)*2048 )),$(( (2*CY+DIM)*2048 )),0"
WW_RENDER_ORTHO=$(( DIM * 2048 ))          # half-width: the chunk fills the frame
WW_RENDER_VIEW=1                           # top
```

Two files photographed with that pair are comparable pixel for pixel, which is
what a coverage-mask IoU or a mean-colour difference needs.

---

## The number: `WW_RENDER_SIZE` is a WINDOW size and its WIDTH has a floor

`src/nifskope_ui.cpp` ~22060 does `skope->resize( rw, rh )`. The main window
will not go below its own minimum width, so a narrow request is silently
floored and nothing says so.

**Measured 2026-09-12:** `WW_RENDER_SIZE=480x480` produced a **1024x445** PNG.
The height obeyed (480 minus the window chrome); the width did not move at all.

So: ask for at least 1024 wide, and ALWAYS read the frame size back (PIL) and
`upp` back from `release/ww_camera_pin.log` instead of computing either from
what you asked for. The skill already says to read the frame size back; this is
the reason it is not optional.

---

## The other half of a native picture: the resource root

A `.BTO` of a region bake asks for
`data\Textures\Terrain\<Worldspace>\Objects\<Worldspace>.LodgenObjects.DDS`,
but a bake writes its atlas to `<out>/tex/Objects/`. Pointing
`WW_LODGEN_RESOURCES` at the bake directory is NOT enough and the render comes
back magenta with no warning that looks like a resource problem. Assemble a
shim root shaped like the archive instead:

```
<root>/Textures/Terrain/<Worldspace>/           <- out/tex/*.DDS      (the .BTR sheets)
<root>/Textures/Terrain/<Worldspace>/Objects/   <- out/tex/Objects/*  (the atlas)
<root>/Textures/LOD/                            <- out/obj/textures/LOD/*
```

`GameManager::get_full_path` searches for the archive folder (`textures/`,
`materials/`) at any `/` boundary and ERASES everything before it, which is why
this works and why prepending a folder to a path that already contains one
BREAKS it.
