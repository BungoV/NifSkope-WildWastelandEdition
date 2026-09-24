---
name: ww-retire-a-bake-route
description: Remove a whole BAKED route from the NifSkope Wild Wasteland generator (E:\Projects\NifskopeWildWastelandEdition) after bungo rules against it -- a format version and its stream, its terrain sheet role, its switches, its census words, its viewer channels, its gate and its docs. The order that makes it provable: inventory, cut, byte-identity against the rung exe, a reader left TOLERANT of files already in the wild, the gate swapped not deleted, and ONE history paragraph in the docs instead of a hole. Lane HORIZONOUT (2026-09-19) retired the baked far-shadow horizon this way.
---

# NifSkope WW: retire a baked route

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane HORIZONOUT
(2026-09-19), which removed the `.lodi` v8 per-vertex horizon stream, the
`.lodt` role-7 terrain sheets, the march that made them, their switches, their
census words and their two viewer channels, after bungo ruled *"horizon goes
bye bye now, we're back to identity"*.

This is about a route the BAKE writes. For a dock or a panel use
`ww-retire-a-surface`; for the mechanics of the deletion itself use
`ww-anchored-cut`; for the byte-identity proof use `ww-module-off-is-identical`.

## 1. Inventory before a line is cut

`git status`, `git log -5`, and a table of every artefact of the route: file,
lines, **which lane added it**, committed or uncommitted. Include `res/`,
`docs/`, `tests/spells/` and **both skill trees** (`.claude/skills` here and
`E:\Projects\Claude\.claude\skills`).

Split any UNCOMMITTED work of a half-finished neighbouring lane into **KEEP**
and **DROP** explicitly, in the report, before touching it. HORIZONOUT
inherited HORIZON3's uncommitted hunks and kept exactly one thing out of them
(a scrappable instance bit), dropping three others by name.

## 2. What must survive the cut

* **Anything the route SHARED.** A horizon march and a sky-occlusion cast can
  share a lattice, a sampler, a cast helper. Deleting the route must not delete
  the shared part -- and *"I did not delete it"* is not proof.
* **The proof is byte-identity.** Bake the same chunk with the rung exe and
  with yours, and diff every payload file. The streams the route did not own
  (sky, AO, the terrain sheets, the DDS) must be **byte-identical**, and the
  report lists the only differing bytes and says which are the version word.
  Use the SAME `--library` on both sides, and say which it was: the group and
  mesh counts differ between `mnam` and `near`, and a reader who does not know
  which was used will think two of your numbers disagree.

## 3. The reader stays TOLERANT

A version you retire is still on somebody's disk. The reader must **open** a
file of the retired version, name the retired stream in its note line, skip it
by its own length, and carry on -- never refuse, never crash. Gate that with a
fixture of the retired version kept in the lane's folder.

State plainly in the docs which exe still WRITES the retired version
(`release/NifSkope.before_<lane>.exe`), so a regeneration is possible.

## 4. The default version, decided and written down

Say which version a default bake now stamps and WHY, and put it in the docs'
version table, which is the authority. HORIZONOUT: default `v7` (the layout
before the retired v8), `--scrappable` writes `v9` (= v7 plus one instance
flag), `.lodo` unchanged at v4. The ways back (`--lodi-v6`, `--lodi-v7`) keep
working and are gated.

## 5. Gates are SWAPPED, not deleted

Retire the route's gate (`tests/spells/lodgen_<route>.sh`) and write the small
gate that replaces it for whatever outlived the route. Remove its row from any
board script. Then run the NAMED neighbours at their standing counts, **before
and after, on your own exes**, and put both passes in the report -- the before
pass is what catches a cut that broke something (see `ww-anchored-cut` s4).

If the brief allows re-basing baseline images, re-base **only** the rows that
belonged to the route, and list them. If the list is empty, say the list is
empty and show the grep that proves it.

## 6. The docs get a history paragraph, not a hole

Every section the route owned is replaced by ONE short paragraph: what was
tried, the two numbers that killed it, why it was dropped, and which exe still
bakes it. A deleted section leaves the next lane free to re-invent the route;
a paragraph does not. Cross out nothing in the consumer plan
(`docs/FO4CS_IMPROVED_LOD_PLAN.md`) without replacing it with the route that
won.

Write the skill text into **both** skill trees and hash both files in the
report (`sha256sum`), because the two trees drift silently.
