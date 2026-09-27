# WATER1 -- ledger text for the overseer to splice (lane writes no ledger itself)

## HANDOFF (NifSkope WW, top block line)
WATER1 (2026-09-27, branch water1-20260927): the .lodl is written with water bodies (version 3) by default
from the CLI and the panel; `--no-water-bodies` is the way back (version 2, byte for byte). In v3 the per-cell
has-water bit is set only where water is over ground. The far-LOD viewer draws every water body as a flat
surface at its body height (plain water, alpha 0.60, in the default view; the water plane views paint on the
water, with a legend line). NOT FLOWN; FO4CS reader changes listed below are owed.

## WW_CHANGES.md
- LOD landscape file: water bodies are on by default (version 3). `--no-water-bodies` writes the old version 2.
  A default run falls back to version 2 by itself when the body step refuses a worldspace, and says so.
- In a version-3 file the cell "has water" flag now means water above the ground (it was set on every cell).
- Far-LOD viewer: water is drawn as flat water at its body's height, over the terrain, with bridges and dry
  ground left out; the water views (height, type, body, flow, shore, cell flags) colour the water surface and
  leave the ground as it is. Flow is a colour wheel. Each view prints its legend.

## MISTAKES.md (root, newest on top)
- WATER1 05:10: a bare `turn.sh` call (no arguments) queued an `acquire anon` for the machine-wide NifSkope
  turn; it holds the lock with no owner once it gets it. Rule: turn.sh has no "status" form -- read
  `.ns_turn/who` instead.
- WATER1: two #include lines went into src/btdterrain.cpp through a python heredoc, against the night rule
  (Write/Edit only). Result correct; route wrong.

## FO4CS reader change list (owed; FO4CS is built last by standing order)
1. Expect version 3 by default. The reader already knows 1 and 2 only (LODT1): it must accept 3, or the
   default bake will be refused. The v3 header is 0xF8 bytes; the water sections are appended after the v2
   payload and their offsets live in the v3 header words (docs/LODGEN_BTD_FORMAT.md, "Water bodies").
2. Cell flag bit 0 in a v3 file = "water over ground in this cell" (Deviation W1). A reader that placed a
   water plane for every bit-0 cell now places one only where there is water to see. In v1/v2 the bit keeps
   its old meaning (set on all cells) -- branch on the version.
3. The water plane of a v3 file is per body: body-ID plane (nearest, never filtered) -> body table height.
   The per-cell height stays in the file as the v2 fallback.
4. A default bake can still be version 2 (the fallback when the body step refuses); the reader must take
   both.
