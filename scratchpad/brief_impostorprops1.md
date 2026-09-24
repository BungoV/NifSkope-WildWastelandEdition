# Lane IMPOSTORPROPS1 -- cards for car wrecks, lattice structures, signs and billboards

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED behind IMPOSTORSHRUB1 (one build lane at a time).
Folder: scratchpad/impostorprops1_<date>/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
Asked which other distant objects would benefit from impostors; offered: lattice structures (pylons, radio
masts, cranes, scaffolding, water towers), car wrecks and junk piles, dead trees/reeds/hedges, signs and
billboards on thin poles; poor fits named: buildings, terrain, roads, bridges. He said: "Car wrecks seem a good
candidate, lattice structures too, signs and billboards too".

## Standing rules that bind this lane
- Authored LODs only (2026-09-17): a vanilla LOD model is never replaced or decimated by this lane. A card
  fills ONLY where vanilla ships no far model. Replacing a vanilla LOD model with a card = his call, not
  this lane's; if the census says a vanilla LOD model is bad for a class (e.g. a lattice that breaks up),
  show it with pictures and numbers and ask.
- Crisp over smooth (2026-09-23): card transitions may be choppy; noise/stipple is the defect, tear too.

## bungo 2026-09-23: "Car wrecks however, some of them are physics objects that can move"
A distant card is baked at the reference's PLACED position; an object that can move (physics / havok-enabled,
movable static, or destructible: the explodable cars fly and burn) would leave its card standing where it
started. RULE for this lane: only objects that can NEVER move or change get cards. Exclude, and count
separately per class: MSTT and any base whose model carries havok motion (not fixed), references with the
havok/motion flags set, bases with destruction data (DEST) or an explosion, anything placed persistent that
a script moves. Name the fields/flags you used, from the xEdit FO4 definitions, and show two examples each way.

## Jobs
1. CENSUS, offline, whole vanilla corpus (E:\Tools\Fallout 4\DataUnpacked\Data + Fallout4.esm and the DLC
   masters): the three classes -- car wrecks (+ vehicle hulks: buses, trucks, boats if static), lattice
   structures (pylons, masts, cranes, scaffold, water towers, towers), signs and billboards (roadside
   billboards, pole signs). Per class: base records (EDID, model path), placed reference count in exterior
   worldspaces, how many are LOD bases today (distant-LOD flag / MNAM), how many have a vanilla far model per
   ring, and how many would draw NOTHING beyond the loaded cells today. Say how each class was identified
   (EDID/model-path patterns, keywords) and print the misses/false hits you checked.
2. PIPELINE: what the candidate listing does with these today (`--candidates missing|trees|all`,
   docs/LODGEN_IMPOSTOR_SPEC.md:195-213). If a base that is not a LOD base can never become a candidate, name
   the smallest change that lets a named class list opt in (a class list file, not a hardcoded name list in
   src/), and what the stock engine needs to draw it far (the BTO / crossed-quad `_fs` path). Prepare it; do
   not ship it -- a new candidate class is his call once he sees the pictures.
3. BAKE a sample: 3 of each class, the most-placed bases, at the size-laddered resolution, N8, on the current
   exe (the cut the IMPOSTOR16 N8 round named). Report empty/torn/muddy cards like any other.
4. PICTURES for bungo: per class, "3D model" | "Octahedral impostor" (labelled), elevation 0 and 20, plus
   what the game draws there today at the ring distance (vanilla far model or nothing). No rocks.

## Rules
CONSTITUTION.md first; skills nifskope-ww-lodgen, nifskope-ww-render-shot, nifskope-ww-build-verify,
ww-reference-card-diagnose, fo4-esp record reading as needed. Game down before any build. bungo's NifSkope
window: never kill; rename aside. One harness NifSkope at a time, second monitor, no focus steal. Build only
if job 2 needs it, with the rung taken first. Do not commit; do not edit HANDOFF/WW_CHANGES/MISTAKES -- text
into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING. Final report under 300 words, plain
words: census table, what draws nothing today, the opt-in change prepared, picture path.
