# LANE CONTRACTS -- freeze the file contracts and write the FO4CS handoff package

Header: tree E:\Projects\NifskopeWildWastelandEdition, main, NO commits, DOCS ONLY:
you may write under docs/ and scratchpad/handoff_fo4cs/ and WW_CHANGES.md and
MISTAKES.md. No src/ edits (another lane owns them; if a contract needs a header
version field that the writer lacks, WRITE THE REQUIREMENT into
scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md, do not implement it). Read
CONSTITUTION.md, HANDOFF.md top block, then every docs/LODGEN_*.md, then
scratchpad/specs_20260906/spec_fo4cs_native.md (the .lodg/.lodi spec; bungo's
ruling today: the FO4CS far field IS .lodg + .lodi, no interim .lodo, .bto stays
the stock bake). Skills via the Skill tool: `nifskope-ww-lodgen`, `nif`. Read the
writers in src/ (lodgen.cpp, lodtfile.cpp) to confirm every byte you document
against the code -- a contract that disagrees with the writer is a mistake. The
FO4CS reader side for reference: E:\Projects\Fo4CommunityShaders\Codex\HANDOFF.md
(grep lodt / LODT1) and its .lodt loader source (find it; read-only). Model Opus 5.

## The work
1. One contract document per file type, versioned, written so a FO4CS engineer
   can write a reader without our source: `.lodt` (exists in LODGEN_BTD_FORMAT.md;
   check and complete: every plane, byte layout, pyramid, the section flags,
   water semantics), `.lodv` (LODGEN_TERRAIN_VT.md; complete), `.lodm` (material
   sidecar: every key, per source/card/array form, emissiveScale), the manifest
   (every line form incl. `A <block> -1 <lodm>` and `C ... <layer>`), the card
   sheets (`<ws>.LodgenCards.<family>.<WxH>` arrays, frame layout, OCT=8, the
   (N)^2 vs (N+1)^2 correction), the texture arrays, `.lodg` and `.lodi` (from
   the spec, marked SPEC / NOT YET WRITTEN, headers 256 B each, the 24-byte
   instance record, the u16 refusals). Each document: version, header, byte
   table, invariants a reader may assume, what is refused and how, a sample
   file path.
2. Correct the two wrong descriptor constants in docs/LODGEN_VERTEX_PACKING.md
   lines ~21-23 (the vertex desc the writer actually emits; the lodt lane
   measured 0x0041B00000650407 for terrain -- verify each from the code).
3. The handoff package: scratchpad/handoff_fo4cs/README.md -- the file family
   in one table (name, purpose, status: shipped/gated/spec, reader exists in
   FO4CS y/n), the read order, the draw description from the native spec's
   D3D11 section (8.3) rewritten as a reader's checklist, the sample-file set
   (list the paths of one generated set for the Commonwealth: .lodt, .lodv per
   level, a .lodm, the manifest, a card array -- point at existing generated
   files, do not generate; say which are missing and which lane makes them),
   and the open items (lattice/detail parked, .lodg/.lodi unwritten, the
   pending terrain gate, uncommitted tree).
4. WW_CHANGES.md entry: docs only, what was frozen at which version.

## Gates
- Every byte offset quoted is traced to a line in the writer (file:line in the
  document's provenance footer).
- No contradiction between a contract and the writer: state each check.

## Report
scratchpad/lane_contracts_report.md, incremental: 1. Documents written/updated.
2. Contradictions found between docs and code, and what you did. 3. Writer
changes needed (mirrors WRITER_CHANGES_NEEDED.md). 4. Mistakes (also
MISTAKES.md). 5. Finished-work skill review. Final message under 25 lines.
