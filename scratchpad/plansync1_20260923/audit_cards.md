# PLANSYNC1 audit -- R4 cards vs today's impostor work (2026-09-23, clock 08:28)

Read-only audit. Plan = docs/FO4CS_IMPROVED_LOD_PLAN.md (1,605 lines, LF). Sources read: LODGEN_IMPOSTOR_SPEC.md (in full),
LODGEN_CARD_SHEETS.md (CARDS, in full), LODGEN_LODM_FORMAT.md (LODM 1-8), LODGEN_MANIFEST_FORMAT.md (MANIFEST 1, 4, 8, 9),
LODGEN_TEXTURE_ARRAYS.md (ARRAYS 3, 5), HANDOFF.md 2026-09-19..23 impostor lines, WW_CHANGES.md top entries, and src.

Every cited section number in the audited plan lines still exists (CARDS 1.1, 1.2, 2, 3.1, 3.7, 4, 7 inv 6, 9; LODM 3, 3.1, 4,
7 inv 8; MANIFEST 4; ARRAYS 3; NATIVE 3.2, 11 / Deviation 5, 12). No SECTION-MOVED rows.

## 1. Row table

| plan line | claim | current truth | source anchor | verdict |
|---|---|---|---|---|
| 92 (§1.2) | per-card sets at `Data\FO4CSLOD\Cards\<formid8hex>_oct*` | path right; the CONTRACT page still says `Data\Textures\Lodgen\Cards\` (contract stale, plan right) | src/lodgen.cpp:3106-3107 "`Data\Textures\Lodgen\Cards\` until today, `Data\FO4CSLOD\Cards\` now" (LAYOUT1 2026-09-16); CARDS 1.1 line 24 | OK |
| 92 (§1.2) | "four libraries of 19 Sanctuary trees exist (CARDS 9)" as usable sets | all four are 2026-09-09 bakes: before the ortho camera (2026-09-10), before the coverage contract (2026-09-10), before the 180-degree azimuth repair (2026-09-19). Every set before the repair must be re-baked | CARDS 9 lines 644-651 (exe 19:35 .. 23:41, 2026-09-09); CARDS 3.7 line 376 "Every card baked before 2026-09-10 was drawn through a 60-degree PERSPECTIVE frustum"; HANDOFF 2026-09-19 09:5x "EVERY IMPOSTOR SET BAKED BEFORE THAT EXE MUST BE RE-BAKED (only test bakes exist)" | STALE |
| 92 (§1.2) | no worldspace bake has written a card layer | still true: the native emitter hard-codes both | src/nativeemit.cpp:1676 `row.cardLayer = LODO_NO_CARD;`, :1538 `lib.cardCorpusHash = 0;` | OK |
| 93 (§1.2) | card arrays `<ws>.LodgenCards.<family>.<SW>x<SH>_*` + `.lodm`, CARDS 1.2 / LODM 4, "no" | unchanged; `SW x SH` is the whole sheet | CARDS 1.2 lines 55-67; src/nifcli.cpp:4641-4643 | OK |
| 719 | READS: card sets / arrays + `.lodm`, `cardLayer` bits, `C` lines | incomplete: two things a card reader must now read are not listed -- (a) `card.conv` / per-layer `conv` (the view-convention token, `spec1`; ABSENT = pre-2026-09-19 set with the azimuth turned 180 degrees); (b) `kind: "aggregate"` sets and their `.lodi` rows (they exist, see line 775). `conv` is not in the LODM or CARDS contract pages at all | SPEC lines 253-262 "An ABSENT token is not 'unknown': it means the set predates the repair"; src/lodgen.cpp:3322-3330 `oc.insert( QStringLiteral( "conv" ), card.octConv );`, :14213 per layer; LODM 3 table (no `conv` row) | STALE |
| 725-727 | N x N hemi-octahedral grid, `oct = N` frames per side, OCT=8 = 64 views (CARDS 2) | true today; bake accepts 2..16, panel offers 4/6/8, default 8. A ring layout for tree cards is RULED but HELD, not built (§5 below) | CARDS 2 lines 81-84; LODM 3 line 121; src/impostoroct.h:39-40 `kMinGrid = 2; kMaxGrid = 16;`; src/lodgenmanager.cpp:1055 default 8 | OK |
| 727 | blends the three nearest frame centres | true for today's grid and the viewer; the CARDS 2 phrase "the centre frame is the exact top" is FALSE for even N (4, 8, 16) and the triangle diagonal is unspecified -- the viewer names both as SPEC GAPs and fixes a rule a FO4CS shader must match | CARDS 2 lines 100-104; src/impostoroct.h SPEC GAP #2 (diagonal) and #3 "TRUE ONLY FOR ODD N" | OK (contract gap noted) |
| 728 | writes depth from the height channel | true: height = normal B. A swap of height and sway in `_n` is prepared and UNRULED | CARDS 4 line 426; WW_CHANGES 2026-09-22 "The `_n` height<->sway channel swap is PREPARED and not applied ... the .lodm files gain `"nlayout": 2`, and the loader refuses an older set by name" | OK (pending) |
| 729 | "sways it from the height" | WRONG channel: sway is its own weight in normal ALPHA, `h^2 x (0.35 + 0.65 r)` from the view's bottom row; height is normal blue. Correct text: "sways it by the sway weight in the normal sheet's alpha (CARDS 4)". IMPOSTORWIND1 may change the source of that weight | CARDS 4 line 427 "normal A (sway)"; LODM 2.1 line 80 "A = sway weight" | WRONG |
| 729 | lights from normal + height; mask sheet not mentioned | viewer today: lit from normal, baked AO into ambient, gloss/spec sheets debug-only; full-material (AO+gloss) exists behind a harness switch only, default is his call | WW_CHANGES line 307 "the material sheets as debug channels only"; HANDOFF 2026-09-23 05:5x IMPOSTOR16 "open: gloss/AO default = his call" | OK (pending) |
| 730 | emissive x `emissiveScale` at night | unchanged; emissive BC1, multiple in `.lodm` | CARDS 4 line 431; LODM 2.2 | OK |
| 733-747 | TRANSITION SMOOTHNESS SLIDER, 0 snap / middle / 1 stipple, default crisp side | matches the 06:0x rulings (HANDOFF). BUT the middle arm's open choice "strongest frame (or the average, whichever the IMPOSTOR16 N8 round measured as tearing less)" is now MEASURED: average (A = mean cut) tears less than strongest (F) at el 20 (N8 5.7% vs 7.1%, N16 3.3% vs 4.3%) and F pops 2.6-3x more; lane recommends A. Then a director FINDING: crisp A makes the trunk vanish between angles and the blend doubles the trunk; snap removes it; per-pixel depth search queued (IMPOSTORDEPTH1). Default value still his | HANDOFF 2026-09-23 07:2x IMPOSTOR16 N8 "A beats F on tear + pops 2.6-3x less; lane recommends A among crisp cuts"; FINDING 07:5x "crisp A makes the TRUNK VANISH mid-way between angles"; FINDING 07:4x "DOUBLED TRUNK"; scratchpad/impostor16_20260923/DELIVERABLE_TEXT.md lines 134-183 | STALE |
| 752-756 | point 1: quad at `pivot + center + frameOffset[2k] right + frameOffset[2k+1] up`, k = j*oct+i (LODM 3.1); 1,070 units low | unchanged | LODM 3.1 lines 170-177, 191-193; CARDS 3.6 line 332 | OK |
| 757-764 | point 2: alpha-test at `coverage.test` (128); set with no `coverage` at 16/255 (CARDS 4) | the contract text is unchanged, BUT the NifSkope viewer's ruled default since 2026-09-22 cuts the DECODED coverage fraction at 128/255 ("vanilla's LOD alpha test"), not the encoded `test` 128 (= fraction >= 16/255). Its own header says a 0.5 cut draws inside `half`. Which domain FO4CS tests in is NOT RULED | CARDS 4 lines 454-483; LODM 3 line 133; src/gl/impostordraw.h:18 `kImpostorVanillaCut = 128.0f / 255.0f`, :80-100; WW_CHANGES 2026-09-22 "The default coverage cut is now vanilla's LOD alpha test: 128/255 = 0.502 ... The old default was the set's floor, 16/255"; HANDOFF 2026-09-22 21:5x "transparency cut-off = VANILLA'S measured value, not 0.20" | STALE (conflict) |
| 765-770 | point 3: `projection` absent is not "unknown"; may refuse by name (LODM 7 inv 8, CARDS 3.7) | unchanged. Missing sibling rule: `conv` absent = pre-2026-09-19 azimuth-flipped set, owed a re-bake, "a consumer may draw such a set under the old reading as a diagnostic, and should say out loud" | LODM 7 inv 8 lines 422-428; CARDS 3.7 lines 409-412; CARDS 7 inv 1c; SPEC lines 253-262 | OK (add conv) |
| 771-772 | point 4: `half` spans the full frame, padding included (CARDS 3.1) | unchanged | CARDS 3.1 lines 200-202 "the quad is the frame"; CARDS 7 inv 1a | OK |
| 774-780 | AGGREGATE CARDS: "Lane CARDS-AGG ... they do not exist yet (NATIVE 12, CENSUS 5.1). R4 ships without them and `cardsAggregate` refuses `not_baked`" | CARDS-AGG LANDED 2026-09-11 (built 15:49:39, bake 15:53): `--aggregate` (OFF by default, off byte-identical), `kind: "aggregate"` `.lodm`, 3 sheets no emissive, 8 horizon azimuths, `.lodi` aggregate rows (v4; v5 with placement AO), `aggSwitchPx` 96, `aggBandRatio` 1.2. Forested-cell count delivered: 2,631 of 3,685 tree cells, 60,605 trees. Never flown. Built from pre-repair cards, so its sets are owed a re-bake too | CARDS 10 line 665 "SHIPPED, gated, never flown in a game"; LODM 3a; NATIVE 4.6 provenance "lane CARDS-AGG, 2026-09-11"; HANDOFF line 1612-1613 "CARDS-AGG launched ~15:08 ... built 15:49:39, bake 15:53"; WW_CHANGES "## 2026-09-11 — Aggregate ring-3 impostors" | WRONG |
| 786-788 | `bRefuseNonMetricCards` refuses a set with no `projection` | consistent with LODM 7 inv 8 | LODM 7 inv 8 | OK |
| 790-794 | FALLBACK arm `stock-fallback` = crossed quads in the `.bto`; `C` lines (MANIFEST 4) | `C` line layout unchanged. Under the FO4CS target the `.BTO` is scratch and removed (manifests kept in `FO4CSLOD/<ws>/`), so crossed quads exist only on a stock-target or `--keep-bto` bake | MANIFEST 4 lines 117-148; MANIFEST 1 "the `.BTO` itself is scaffolding under the FO4CS target"; plan §5 row 7 (closed) | STALE (minor) |
| 802-808 | GATE 1: every card field refuses today, `cardLayer` 0xFFFF, `cardCorpusHash` 0 | still true | src/nativeemit.cpp:1538, :1676; HANDOFF 2026-09-17 AUDIT1 "ROWS FOR BUNGO: cardCorpusHash is zero on every bake" | OK |
| 806-808 | GATE 1 fallback: gate on "the 19-tree Sanctuary libraries that exist on disk (CARDS 9)" | those libraries are non-metric (no `projection`), have no `coverage` key and no `conv` (back of the tree at the front). A gate needs a post-2026-09-19 bake carrying `projection ortho`, `coverage`, `conv spec1` (e.g. the IMPOSTORTEAR1 / IMPOSTOR16 test bakes in scratchpad/, not verified here as complete sets) | CARDS 9; HANDOFF 2026-09-19 09:5x (re-bake all); SPEC 253-262 | WRONG |
| 809-812 | GATE 2 controls: `center` zero and `frameOffset` ignored must FAIL (LODM 3.1) | unchanged | LODM 3.1 lines 201-203 | OK |
| 815 | GATE 4: `cardFrames` climbs toward N^2 | true for the grid layout; a ring layout would be `views` | CARDS 2; LODM 3a.2 | OK |
| 824-827 | MUST NOT: aux sizing on a mesh array (LODM 4); decimate a card (CARDS 7 inv 6); mesh normal-blue as height / mask-blue as AO (ARRAYS 3) | all three unchanged | LODM 4 lines 312-313 "A `kind: "array"` file carries no `aux*` keys"; CARDS 7 inv 6 lines 612-613; ARRAYS 3 "Height and sway are neutral on a mesh layer, and AO is neutral, on purpose" | OK |
| 1053 (§5 row 4) | aggregate cards + forested-cell count MISSING, blocks R4's aggregate half | DONE 2026-09-11, lane CARDS-AGG (see line 774 row) | as line 774 row | WRONG |
| 1057 (§5 row 8) | resource stack cannot see `.pbrm` / `.lodm` | DONE 2026-09-16, lane GENSMALL1: loose `.pbrm` / `.lodm` served, gate resource_ext.sh 12/0 | HANDOFF line 75 "GENSMALL1 LANDED 2026-09-16 14:4x ... resource_ext.sh NEW 12/0"; WW_CHANGES "### Loose-file whitelist: `.pbrm` and `.lodm`" ("`--resource <dir>` now serves `.pbrm` and `.lodm` from a loose folder") | STALE |
| 1060 (§5 row 11) | no bake writes a card layer | still PENDING | src/nativeemit.cpp:1676 | OK |
| 1250 (§8) | `.lodm` envelope 1, payload 1; refuse a third `family` word | unchanged. Since the plan: kind `aggregate` added; optional `card.conv` / layer `conv`; pending `nlayout` 2 if the `_n` swap is ruled | src/io/lodmfile.cpp:9 `static const quint32 LODM_VERSION = 1;`, :46 "payload is not a lodm 1 object", :51 "family must be legacy or pbr" | OK |
| 1251 (§8) | manifest `# lodgen manifest 2` | unchanged | src/lodgen.cpp:4534 | OK |
| 1252 (§8) | texture arrays sidecar `# lodgen texture arrays 5` | unchanged | src/lodgen.cpp:5271 | OK |
| 1364-1378 (§8.3) | "Today: NO" viewer preview; parked lane IMPOSTORVIEW1 awaits his signal | DONE under another name: lanes IMPOSTORSHOW + IMPOSTORFIX1 (2026-09-19) built the viewer card draw (view -> grid, three-frame blend, frameOffset, coverage decoded, height parallax + pixel depth, baked AO), gate tests/spells/impostor_draw.sh with an independent Python reference; then IMPOSTORLIGHT1, AA1, TEAR1, FIN1, IMPOSTOR16 (2026-09-22/23). "IMPOSTORVIEW1" does not appear in HANDOFF | WW_CHANGES "## Octahedral impostor preview: a card is drawn to the spec ... (2026-09-19, lanes IMPOSTORSHOW + IMPOSTORFIX1) -- PARTIAL"; src/impostoroct.h:20-24 "tests/spells/impostor_draw.sh ... compare ... against the Python reference" | STALE |

Counts: OK 20 (3 with a pending note), STALE 7, WRONG 4, SECTION-MOVED 0.

Side finding, sources disagreeing with each other: SPEC "The frame law" (lines 626-633) still prints the RETIRED five-rung
aspect ladder `1 3/4 1/2 3/8 1/4` and SPEC line 648 the old 46.4% half-aux saving; CARDS 3.2 / 3.5 and src say multiple-of-16
aspect and 42.9%. The contract page wins (SPEC line 14). The bake's own comment at src/nifskope_ui.cpp:23163-23166 ("ASPECT ...
at most two steps") is also stale beside the loop at :23251.

Contract-page staleness found on the way (not plan lines): CARDS 1.1 line 24 and the CARDS source-anchor table line 863
(`lodgen.cpp:2829`) still print `Data\Textures\Lodgen\Cards\`; the source moved it to `Data\FO4CSLOD\Cards\` (lodgen.cpp:3106-3107).
LODM 3 has no `conv` row.

## 2. Today's card law (as the sources state it)

- Grid: N x N hemi-octahedral, N^2 views, `oct` = N. Bake accepts 2..16 (impostoroct.h kMinGrid/kMaxGrid); panel 4/6/8,
  default 8 (lodgenmanager.cpp:1055); bake script OCT 8, TILE 128; panel card resolution 64/128/256/512, default 128.
- Frame size ladder: 1, 1/2, 1/4, 1/8 of the reference, floor 32 px, nearest in log against WW_IMPOSTOR_REF; no reference set
  = no ladder (nifskope_ui.cpp ~23174-23183, `qMax( 32, from >> qBound( 0, steps, 3 ) )`; CARDS 3.3).
- Aspect: short side = smallest multiple of 16 whose inner rect fits (nifskope_ui.cpp:23251; CARDS 3.2). The five-rung ladder
  is retired.
- Gap = max(2, side/16) rounded up to even; pad = gap/2; mips = log2(gap); `half` spans the whole frame (CARDS 3.1).
- Camera orthographic, `projection "ortho"`; absent = perspective vintage (CARDS 3.7). View convention `conv "spec1"` since the
  2026-09-19 azimuth repair; absent = pre-repair, re-bake owed (SPEC 235-262). bungo 2026-09-19: "Okay, fix the 180 issue".
- `--card-half-aux`: normal, mask and emissive sheets at half size, colour never; OFF by default (lodgenmanager.cpp:1103);
  4,480 -> 1,920 bytes, 42.9% (CARDS 3.5).
- Sheets: `_d`/`_bc` BC3 colour + coverage alpha (encoded, 160 and up inside); `_n` BC3 view normal RG, height B, sway A;
  `_gsaos`/`_rmaos` BC3 mask; `_g`/`_e` BC1 emissive (CARDS 4). Coverage contract {floor 16, test 128, base 160} encoded.
- Viewer cut default: decoded coverage fraction at 128/255 (impostordraw.h:18). bungo 2026-09-22 21:5x: "what vanilla game had
  worked pretty well". Default cut rule = stipple; mean and strong under WW_IMPOSTOR_CUT. Bake supersample 4x (IMPOSTORTEAR1).
- Crisp over smooth, bungo 2026-09-23 06:0x: "Fortnite impostors are a lil bit more choppy, but I think it's a fair tradeoff";
  "they're LOD objects ... it's fine if they're choppy, you won't be circling around them all the time".
- Slider, bungo 2026-09-23 06:0x: "fo4cs can implement a slider for how choppy the transition should be ... from instant
  rotation that's snappy, to something very smooth but possibly noisy".

## 3. Version words

- `.lodm`: envelope 1, payload `"lodm": 1` (lodmfile.cpp:9, :46); cap 4 MiB; family legacy|pbr only (:51). Kinds: source,
  card, array, cardArray, aggregate, terrainVT (LODM 2). New optional keys since the plan: `card.conv` / layer `conv`.
  Pending if ruled: `"nlayout": 2` with older sets refused by name.
- Manifest: `# lodgen manifest 2` (lodgen.cpp:4534). Texture arrays: `# lodgen texture arrays 5` (lodgen.cpp:5271).
- Card arrays: no text sidecar; the cardArray `.lodm` is the only descriptor.
- No version number changed since 2026-09-11.

## 4. Owed generator items

- Card layer in a worldspace bake (plan §5 row 11): PENDING (nativeemit.cpp:1676, :1538).
- Aggregate cards + forested-cell count (§5 row 4): DONE 2026-09-11, CARDS-AGG; source cards pre-repair, re-bake owed.
- `.pbrm`/`.lodm` resource whitelist (§5 row 8): DONE 2026-09-16, GENSMALL1.
- Re-bake of every pre-2026-09-19 set: owed (HANDOFF 2026-09-19 09:5x); no re-baked library found in CARDS 9.
- IMPOSTORPBRM1: QUEUED; scope ruled 2026-09-23 07:5x (specular weight + colour + IOR, TintMask in the bake).
- IMPOSTORWIND1: QUEUED 2026-09-23 (bungo: "sway from the tree's model's own wind weights would be neat").
- IMPOSTORDEPTH1 (per-pixel depth search, doubled trunk): QUEUED. IMPOSTORSHRUB1 (empty cards for Sapling01,
  TreeElmUndergrowth01, ShrubGroupLarge05): LIVE 07:2x. IMPOSTORPROPS1: queued. IMPOSTORRING1: HELD.
- Viewer preview (§8.3): DONE (IMPOSTORSHOW 2026-09-19 onward). "IMPOSTORVIEW1": not found in HANDOFF.

## 5. Unruled (each with its HANDOFF / WW_CHANGES line)

- Grid and cut default: HANDOFF 2026-09-23 07:2x "open: his pick of grid + cut default, slider lane owed".
- Slider default value: ruled "crisp side", number not given (06:0x).
- Gloss/AO default: HANDOFF 2026-09-23 05:5x "open: gloss/AO default = his call"; IMPOSTORLIGHT1 00:0x "OWED: gloss ruling".
- `_n` height<->sway swap: HANDOFF 2026-09-22 21:5x "still UNRULED, prepared only"; WW_CHANGES line 215.
- Keep the lit card: WW_CHANGES 2026-09-22 "A ruling is owed on whether to keep it" (brightness later fixed to 0.95-0.98).
- Minimum frame size for fine-twig trees: HANDOFF IMPOSTORFIX5 2026-09-19 20:06 "NEW RULING OWED".
- Ring layout second row: IMPOSTORRING1 "a raised second row = his call".
- Coverage as opacity: IMPOSTORLOOK1 2026-09-19 "RULING, not applied".
- FO4CS cut domain (encoded `coverage.test` 128 vs vanilla fraction 128/255): not found as a ruling for FO4CS.
- cardCorpusHash zero on every bake: HANDOFF 2026-09-17 AUDIT1 "ROWS FOR BUNGO".

## R0-IMPACT

- `conv` (card and per layer): a loader must read it; absent = pre-repair, azimuth reversed; refuse or diagnose by name.
  Not in LODM 3.
- `nlayout` 2 if the `_n` swap is ruled: sway/height channels move; older sets refused by name.
- `kind: "aggregate"` exists (LODM 3a); `cardsAggregate` "not_baked" refusal is no longer the only arm.
- IMPOSTORRING1 (held): tree cards in a ring layout (`views`, 2-frame blend) instead of `oct`.
- Coverage cut domain conflict (plan point 2 vs viewer default).
- Versions unchanged: `.lodm` 1/1, manifest 2, arrays 5.
