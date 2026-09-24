LANE TERRAINFMT1 -- CLOSED 2026-09-12 13:45. Nothing pending.

This file held the mid-lane resume steps. Every item is finished or refused:

  item 1 / gate F1  measured   whole 6,120-sheet corpus
  item 2 / gate F2  landed     --sheet-format vanilla|legacy, off == rung bytes
  item 3            settled    and it refutes the brief: the cast is the .BTR's
                               vertex colours, not any sheet
  item 4 / gate F3  REFUSED    with three independent tests; no code, no flag
  item 5 / gate F4  passed     one build, nine harnesses at GROUND1's baselines
  item 6            done       cmp_render_msn.png, cmp_msn_detail.png,
                               cmp_msn_cache.png in images/
  item 7            done       docs/LODGEN_TERRAIN_VT.md section 7a + the four
                               deliverables in this directory
  item 8 / gate F5  landed     --msn-cache DIR, cache measured, sizes reported,
                               the 2K-vs-1K decision LEFT TO BUNGO

Report: scratchpad/lane_terrainfmt1_report.md (sections 0-9).
Deliverables here: WW_CHANGES_ENTRY.md, HANDOFF_BLOCK.md, MISTAKES_ENTRIES.md,
CHANGED_FILES.txt, images/.

Owed to the NEXT lane, not to this one:
  * docs/LODGEN_PARITY.md says the default carries no terrain identity;
    src/nifcli.cpp:6442 defaults it to true. A decision, not a defect.
  * lodgenBlendVanillaDetail (src/lodgen.cpp:7083) swaps which axis each detail
    delta is added to, on the non-default --land-detail-source vanilla-blend
    path. Found, not fixed; needs a build.
