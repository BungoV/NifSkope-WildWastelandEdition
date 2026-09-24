
### 6.8 Pictures

Four, all rendered by this lane from files on disk and none of them a
screenshot of a window someone arranged by hand. The three native views are
`release/NifSkope.exe`'s own native renderer opening the `.lodi` of each
fixture region, pinned by arithmetic -- the camera centre and ortho half-width
come from the region's cell footprint (4,096 units a cell), never from a
remembered screen coordinate -- by
`scratchpad/audit1_20260916/make_images.sh`. The `.lodt` sheet is decoded in
Python by the tree's own independent reader
(`tests/spells/lodgen_vt_check.py`, `Lodv` + `decode_bc1`) and laid out at the
tile grid the container's header declares, by
`scratchpad/audit1_20260916/make_lodt_sheet.py`, so a container that decoded to
garbage would show as garbage rather than as the renderer's idea of the file.

PICTURES-TABLE
