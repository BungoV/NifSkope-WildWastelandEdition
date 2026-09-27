# Lane IDENT1 -- one id per building across the whole map, and occluder boxes per building

Worktree: E:\Projects\NifskopeWWE-ident1, branch ident1-20260927 from night-20260927 @ 8f58e7db.
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\ident1_20260927\DONE.md in the worktree.

## His words
"04, is this the identity map? 05 is unusable, object id needs to be per building, not a texture map it is"

## What is wrong (audit ranks 3 and 6; sections 2.2-2.4; pictures 04, 05, 10, 25)
- The building identity (group table, .lodi 4.9, picture 04) is per building in intent and the far shadows key on
  it, but ids restart in every chunk, so a building crossing a chunk line is cut (Diamond City in 4 groups); and the
  64-unit join welds neighbours (the ballpark joins its neighbours into 420 pieces; row houses weld into a
  12,700-unit block).
- Occluder boxes (.lodi 4.5, picture 25): 340 in the whole Commonwealth, median 5 units thick -- wall slabs; no hills.
  They hide almost nothing at LOD range.

## The work
1. Identity: one id space for the whole worldspace file (not per chunk), join rule = pieces that touch (a real
   surface-contact/overlap test, not a 64-unit bounding gap) OR share a SCOL / a precombine parent, with a size cap so
   a street never becomes one building. Measure and set the cap from the data (distribution of joined sizes; name the
   buildings that sit near the cap). Do NOT bump the .lodi version (lane GROUND1 owns tonight's bump): if the group
   table cannot hold file-wide ids in its current layout, stop that step and report exactly what field is too small.
2. Occluders: one box per building group (oriented to the group, filled only where the building is solid enough --
   pick the fill test by measurement), plus terrain hill boxes (the ridge lines that hide the city behind them).
   Same format; count, sizes and a coverage number (what fraction of the Boston skyline pixels would be hidden from
   three street-level eye points).
3. Viewer: the `identityraw` view (05) is dropped from the list or clearly renamed as the low byte of the piece id;
   the `placement` view's caption says "one colour per placed kit piece".
4. Gates:
   - off (env/command-line, gate only) = night-20260927's Boston bake byte for byte;
   - named buildings: the two towers, Trinity Church, the ballpark (Diamond City) each exactly ONE id, including
     across chunk lines; three row houses on one street = three ids (name them by base form and position);
   - no group larger than the cap; histogram of group sizes before/after;
   - every placement has an id; ids unique file-wide (count);
   - occluders: no box pokes out of its building by more than 1% of volume (sample test); count and median thickness.
5. Pictures, each full size with its title bar, maps1 Boston camera: identity before (renderer, night bake), identity
   after; occluder boxes before and after (offline top-down as maps1 O_occluders, and over the city).
