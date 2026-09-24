## 2026-09-17 20:0x -- lane AUDIT1, a reader whose verdict was wider than its coverage

- 20:04 -- step 3 recorded the `.lodm` invariant as `23 files, 24 checks, 0 violations` and I wrote
  it into the report as the format's row. When step 6 pointed the same reader at every `.lodm` this
  lane had baked, it reported three FAILs -- `kind 'aggregate' is not one of source/card/array`, and
  `no textures object` against both VT sidecars -- and all three were the reader. Five kinds are
  written (`card`, `array`, `terrainVT`, `cardArray`, `aggregate`, plus `source` as the default),
  measured by grepping every `root.insert( "kind" )` in `src/`; my reader knew three, and it also
  required a texture table of a `terrainVT` sidecar, which legitimately has none because its sheets
  live in the `.lodt` container beside it. Fixed: the kind list names all six, the textures rule is
  per kind, `terrainVT` must carry a `terrain` object with three named fields, and three refuters
  were added and go red. After: 99 sidecars, 100 checks, 0 failures, the 23-card library still 24/0.
  **The rule: a reader's verdict covers the inputs it was given, not the format it names. Before a
  row goes in a report as a format's row, the reader is pointed at every family the WRITER can
  produce -- and the cheapest way to enumerate those is to grep the writer, not to remember them.**
