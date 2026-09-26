# ROADS1 (2026-09-26) -- text for the ledgers (the overseer splices it; nothing here is written into main)

## HANDOFF (top block entry)
ROADS1, branch roads1-20260926 (worktree E:\Projects\NifskopeWWE-roads1, from 6382a09a), commits aa8f096b, 273b704d
and the report commit. Not merged, not installed.
- Pavements are painted into the LOD terrain colour BY DEFAULT (bungo: "I approve of the roads"; "if that is their
  in game texture, it is their texture on our terrain too"). `--no-road-sidewalks` leaves them out;
  `--road-sidewalks` is kept as a no-op.
- The road stamp now applies the game's material swap (REFR swap, else the base's, else a SCOL part's own) --
  229 placements in the Boston box. Faithfulness gate: on the 3,169 swapped pavement texels the new sheet is
  2.25 levels from the in-game diffuse; 6382a09a is 50.26 off (it painted the unswapped texture).
- "Is there no roads here?" -- there is: the river road east of Diamond City (Landscape\Roads\River\RRoadCurveCustom01..11)
  was refused because its bases carry their own distant LOD, which the stamp read as "raised". Raised is now the
  HighwayOverpass and Bridge folders only; the river road, 45 park pavement placements and PlazaSwanPond01 are painted.
- The 2x2 tile lines on pavements are the texture's own slab joints (measured), kept.
- kLodgenGeneratorRevision 1 -> 2.
- OWED: the installed|new pictures (pics.py ready). lodgen_roads.sh R5 is red, but equally red at 6382a09a (0.3210;
  ROADS1 0.3193): a stale gate, not this lane. Pavements on/off confinement: 0 texels outside the footprints.

## WW_CHANGES (user-facing)
- LOD terrain: pavements and sidewalks now appear in the distant ground colour by default, wearing the same
  texture they wear up close, including the retextured variants the game swaps in.
- LOD terrain: the riverside road east of Diamond City and the park paths now show in the distant ground.
- Command line: `--no-road-sidewalks` turns pavements off; `--road-sidewalks` still parses and does nothing.

## MISTAKES (newest first)
- 2026-09-26 ROADS1: ran `bash turn.sh status` to look at FIX1's machine-wide NifSkope turn. The script has no
  status command -- every word but `release` acquires -- so it took the turn as "anon" and blocked every lane.
  Rule: read a lock script before calling it with a verb you have not seen in it; inspect a lock by `ls` of its
  directory, never by calling the script.
- 2026-09-26 ROADS1: declared nativeEffectiveSwap (anonymous namespace in nativeemit.cpp) in nativeemit.h for
  reuse; every call became "call of overloaded ... is ambiguous", one build lost. A function in an anonymous
  namespace is restated where it is needed (as nearlib.cpp does), never exported by a header declaration.
- 2026-09-26 ROADS1: three scripts through bash heredocs lost backslashes (a NUL byte in one, an anchor that did
  not match in flip.py, a Windows path read as a \N escape in a DONE.md filler). The skill's rule already says it:
  anything with a backslash goes through the Write tool.
- 2026-09-26 ROADS1: a Python rewrite with io.open(..., 'w') turned an LF script CRLF (a 493-line diff). Write
  with newline='' and check b.count(b'\r').
- 2026-09-26 ROADS1: the independent reader first took the BSTriShape UV and colour offsets from the wrong
  vertex-desc nibbles; the right ones are bits 8 and 24 (x4 bytes). Its first raster "disagreed" with the code
  for that reason alone.
