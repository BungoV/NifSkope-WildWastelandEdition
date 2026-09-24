# PBRRENDER0 research: how FO4CS decides a mesh is PBR (worktree wt-spec1)

All paths relative to E:\Projects\Fo4CommunityShaders\wt-spec1 unless stated. Read-only research, 2026-09-23.

## 1. How a .pbrm is found for a mesh

Master gate: `[PBRMaterials] bPBRMMaterials` (field `pbrmMaterials`, ships true) -- src\Materials\TruePBRShim.h:98-102.
Every seat below returns early when it is off (e.g. TruePBRShimRuntime.cpp:4267, 3529, 3575, 3452).

### Rule A -- same-name sibling of the NIF's BGSM/BGEM (the main route)
- The name read is the SHADER PROPERTY NAME (`property->GetName()`), which for a FO4 NIF is the BGSM/BGEM path
  (`Materials\...\X.bgsm`, root sometimes implied) -- TruePBRShimRuntime.cpp:3344-3345, used at :4284-4285 (lighting)
  and :3533-3534 (effect).
- `pbrm::NormalizeMaterialPath` (src\Materials\PBRM.cpp:1339-1384):
  - trim; refuse empty, leading `/` or `\`, or any `:` (drive/UNC) -- :1342-1347
  - `/` -> `\`, collapse doubled `\` -- :1349-1353; strip leading `.\` -- :1354
  - refuse any `..` segment -- :1356-1366
  - a name ending `.pbrm` is REFUSED ("direct .pbrm NIF references are reserved for future runtime support") -- :1368-1371
  - must end `.bgsm` or `.bgem` (case-insensitive), else refused -- :1372-1375
  - prefix `Materials\` when the name does not already start with `materials\` (case-insensitive) -- :1380-1382
  - case is otherwise PRESERVED (not lower-cased).
- `pbrm::ResolveMaterialPath` = normalise, then replace the last 5 chars (`.bgsm`/`.bgem`) with `.pbrm` -- PBRM.cpp:1386-1394.
  So `Weapons\M2\M2Barrel.bgsm` -> `Materials\Weapons\M2\M2Barrel.pbrm`, same folder, same stem. No other folder remap.
- Lighting seat: vfunc 0x07 ReceiveValuesFromRootMaterial hook runs the engine first, then
  `ApplyPbrmForRootProperty` for a BSLightingShaderProperty -- TruePBRShimRuntime.cpp:4393-4403 (comment :4300-4301
  "let Fallout initialize its authored BGSM/BGEM first, then derive and apply an optional same-name PBRM replacement").
- Effect seat (BSEffectShaderProperty / .bgem): `EffectReceiveValuesFromRootMaterialHook` -> `ApplyEffectPbrmForRootMaterial`
  -- TruePBRShimRuntime.cpp:4330-4340, 3525-3570. Same path rule.

### Rule B -- texture-swap (material swap / OMOD skin) from the final diffuse name
- Runs in OnLoadTextureSet2 (vfunc 0x10), NOT during root receive, and is asked FIRST there -- TruePBRShimRuntime.cpp:4489-4492.
- `ResolveTextureSwapMaterialPaths(diffuse)` -- PBRM.cpp:1396-1448:
  - normalise the diffuse texture path (Rule in item 4); it must end `_d.dds` -- :1406-1410
  - stem = file name minus `_d.dds`; directory = path under `Textures\`
  - candidate 1: `Materials\<same dir>\<stem>.pbrm`; candidate 2: `Materials\<parent dir>\<stem>.pbrm` -- :1425-1446
  - e.g. `Textures\Armor\X01\Tesla\foo_d.dds` -> `Materials\Armor\X01\Tesla\foo.pbrm`, then `Materials\Armor\X01\foo.pbrm`.
- First candidate that parses AND binds wins -- TruePBRShimRuntime.cpp:3582-3591.

### Rule C -- Fallout 76 BGSM read directly (no .pbrm file)
- After Rule A declines, `ApplyFo76PbrmForRootProperty` re-reads the BGSM named by the property -- TruePBRShimRuntime.cpp:4404-4412, 3447-3494.
- Accepted when the file parses as a FO76 BGSM (version word at bytes 4-7 in 20..22; vanilla FO4 = 2) AND its PBR flag is set
  AND it has a base colour -- :3383-3404, :3406-3413. Converted to the same runtime material (fo76::ToPbrmMaterial).

### Precedence
1. Texture-swap (Rule B) beats the root-BGSM sibling: `ApplyPbrmForRootProperty` keeps an existing swap binding and returns
   -- TruePBRShimRuntime.cpp:4270-4281; OnLoadTextureSet2 asks the swap first -- :4464-4492.
2. Rule A sibling `.pbrm`.
3. Rule C FO76 BGSM, only when A declined (A's probe tears down bindings first; FO76 is applied LAST) -- :4404-4411, 3442-3446.
- A material+path pair is probed once (per-material path memo) -- ShouldProbePbrm :1565-1576; results cached incl. misses
  (`g_pbrmCache.FindOrLoad`) -- :3276.

### Failure behaviour (all fall back to the untouched engine BGSM/BGEM)
- Missing file: silent, negative-cached -- :3277-3283.
- Parse error: logged once `[PBRM] <path>: <error>` and no binding -- :3280-3282.
- Parsed but not runtime-supported: logged, no binding -- :3285-3294. Supported (lighting) means
  shader `Standard` or `Advanced Subsurface`, runtime pass deferred surface (or forward surface + alpha blend with alpha
  from base colour/constant for the blend variant), known composition and alpha source, NO secondary UV, all
  `requirements` capability versions satisfied -- PBRM.cpp:1257-1269, requirement check :1236-1255. Unknown shader
  names are kept as a valid-but-unsupported material (fail-closed) -- :1232-1234, 1283-1285.
  Effect seat support: forward surface or effect pass, no secondary UV, known composition/alpha -- TruePBRShimRuntime.cpp:1883-1890.
- ANY authored texture that fails to load aborts the whole binding (base colour, normal, RMAOS, emissive, subsurface,
  specular colour/IOR map) -- TruePBRShimRuntime.cpp:1847-1879, logged at :3298-3302.
- The engine's own BGSM textures are left intact under a bound PBRM ("safe engine-owned base") -- :80-84, :3313-3316.
- PBR is tagged on the engine material by writing sentinel bits into `specularColorScale` -- :3317; `IsPbrmSentinel` :3257-3260.

## 2. "A .json file linked to our NIF that points to our shader node"

Plain answer: the FO4CS runtime in wt-spec1 has NO JSON that maps a NIF or a NIF shape/shader node to a PBR material.
The runtime never reads a .pbrm name from a NIF either: a direct `.pbrm` name in a shader property is refused
(PBRM.cpp:1368-1371), and TruePBRShimRuntime.cpp:83-84 says direct .pbrm NIF references are "reserved for a future bridge".
Material identity comes ONLY from the shader property's BGSM/BGEM name (item 1) or the diffuse texture name (Rule B).

What DOES exist, closest first:

a) `.pbrmset` -- PBR Material Editor, editor-side only (PBRMaterialEditorQt\docs\PBRM-v6.md:156-161):
   `{"schema": "FO4.PBRM.MaterialSet", "version": 1, "assignments": {"<submesh name>": "<.pbrm path>"}}`,
   stored next to a preview mesh, paths relative to the mesh directory when possible. Keyed by SUBMESH NAME (a string, not a
   block index). The doc states runtimes do not use it: "Runtimes that want per-shape material binding read material paths
   from the NIF itself; the sidecar only drives the editor's multi-material preview." No reference to `pbrmset` or
   `MaterialSet` anywhere in wt-spec1\src. This is the most likely thing the owner means. File NAME convention (stem vs
   anything) is not stated in the docs; the editor's src\ is outside this task's scope -> unknown here.

b) `.nifx` -- FO4CS per-NIF JSON sidecar (src\NifSidecar\NifSidecar.h:11-57, 61-74):
   - same directory as the mesh, named for it: `meshes\foo\bar.nif` -> `meshes/foo/bar.nifx` (separators normalised to
     `/`, lower-cased) -- NifSidecar.h:26-29, 89-100. No path field inside; location is the binding.
   - top level = JSON object of namespaced sections, REQUIRED `"version"` (=1, higher refused) -- :30-34, :69-74, NifSidecar.cpp:573.
   - keys inside a section are BARE NODE NAMES -- NifSidecar.h:39.
   - unknown sections ignored with a log note -- :35-38.
   - ONLY section today: `fakeVolume` (per-particle-emitter-node `bulgeStrength`/`wrapContrast`/`powderContrast` in [0,1])
     -- NifSidecar.h:110, 116-122, 147-169; NifSidecar.cpp:423, 437, 498-505. There is NO material/PBR section.
   - It is not live: `RegisterLoadedModel` has no caller in src (only its own definition NifSidecar.cpp:837), the header
     says the NIF-load seam is still owed -- NifSidecar.h:385-398. So even fakeVolume never reaches the game today.
   Example shape: `{"version":1,"fakeVolume":{"<NodeName>":{"bulgeStrength":0.5}}}`.

c) Skyrim Community Shaders' TruePBR convention (PBRNifPatcher-style JSON that rewrites NIFs offline, plus a NIF shader
   flag) is NOT present in wt-spec1: no match for PBRNifPatcher / NifPatcher / SLSF / shader-flag detection in src\Materials.
   The only JSON-ish per-mesh ecosystem idea in the runtime is .nifx above.

d) `pbrm-conversion-manifest.json` (FO4-Conversion.md:902, 944) and `Data\PBRM\SurfaceStates.json`
   (SurfaceStates-Architecture.md:39, location "a #38 campaign decision") -- converter/record files, not NIF bindings.

Precedence vs a direct .pbrm link: moot in the runtime -- neither a JSON binding nor a direct .pbrm link is read.
Unknown: whether the owner's JSON is the .pbrmset (editor) or a planned material section of .nifx (candidate list lives
in FO4CS ROADMAP.md "SECTION CANDIDATE REGISTRY" per NifSidecar.h:112-115, not read here).

## 3. NIF-side markers the runtime reads to flag a shape as PBR

None. The runtime reads no shader flag bit, Shader Type, texture-slot convention or special property name to decide PBR.
- The only NIF input is the BSLightingShaderProperty / BSEffectShaderProperty NAME (= BGSM/BGEM path) -- item 1.
- Rule B reads the ENGINE-LOADED diffuse texture name (ending `_d.dds`), i.e. whatever texture set the material swap produced.
- The old by-name `_RMAOS.dds` SmoothSpec-slot classifier and the cb2 sentinel limb were DELETED (RETIRE2/RETIRE4) --
  TruePBRShim.h:16-20, 81-95, 691-699; TruePBRShimRuntime.cpp:63-66, 3251-3255.
- `bForceAll` (ships 0) stamps every draw as PBR, diagnostic only -- TruePBRShim.h:103-112, 679-683.
- Seats: BSLightingShaderProperty::LoadBinary (vfunc 0x1B, NIF load) and material ReceiveValuesFromRootMaterial (0x07)
  run the sibling probe; OnLoadTextureSet2 (0x10) runs the swap probe -- TruePBRShimRuntime.cpp:4576-4604, 4393-4412, 4417-4492.
- Inverse direction (what PBR WRITES onto the NIF property flags when bound, useful for a viewer to mimic):
  sets Specular, CastShadows, ZBufferTest; ZBufferWrite unless alpha-blend; TwoSided from the .pbrm; AlphaTest from the
  .pbrm; clears PremultAlpha and OwnEmit -- TruePBRShimRuntime.cpp:3807-3828. Restored on unbind -- :3831-3862.

## 4. Texture path normalisation and archive lookup for .pbrm texture slots

- Slots live under root `primaryUv` as objects keyed `primaryBaseColor`, `primaryNormal`, `primaryRmaos`(name per doc),
  `primaryEmissive`, `primarySpecularColor`, `primarySubsurface`, ... each `{enabled, path, values}` -- PBRM.cpp:794, 518-557.
  A slot's texture is used only when `enabled` is true AND the matching `override*` value is false (e.g. base colour
  :828-843, normal :845-867). Otherwise the constant in `values` is used.
- `NormalizeTexturePath` (PBRM.cpp:1291-1337), applied to every slot path (:835, 858, 897, 947, 988, 1060):
  - trim; refuse empty; refuse leading `/` or `\` or any `:` (absolute, drive, UNC) -- :1293-1302
  - split on `/` or `\`, drop empty and `.` segments, REFUSE `..` -- :1304-1322
  - drop ONE leading `textures` segment (case-insensitive) -- :1323-1325
  - rebuild as `Textures\` + segments joined with `\` -- :1330-1336. Case preserved; no lower-casing.
  So `textures/Foo/bar_d.dds`, `Foo\bar_d.dds`, `.\Foo\bar_d.dds` all become `Textures\Foo\bar_d.dds`.
- A path that fails normalisation is NOT fatal: diagnostic "...; constants used" and the slot falls back to its constant
  -- e.g. :840-842, :864-866.
- A path that normalises but cannot be READ or DECODED IS fatal for the whole material: the binding returns nullopt and
  the draw keeps the vanilla BGSM -- TruePBRShimRuntime.cpp:1847-1879; log "[PBRM] texture could not be read through
  Fallout resources" :1687-1691, "DDS upload failed" :1706-1709.
- Lookup: `ReadTextureResource` -> `CubemapResourcePath` (adds `Textures\` only if missing, `/`->`\`) -> the engine's
  `RE::BSResourceNiBinaryStream` -- TruePBRShimRuntime.cpp:1618-1630, 1653-1672. That is Fallout's own resource system:
  loose Data\ files and BA2 archives with the engine's normal override order; no custom search. The .pbrm itself is read
  the same way (`ReadPbrmResource`, :1596-1616). Hot-reload watches loose files only (`Data\` + path timestamp; "BA2
  resources have no loose-file timestamp") -- :3643-3647.
- Size limits: texture 128 B .. 256 MiB (:1655, 1664); .pbrm payload <= 64 MiB (PBRM.h:34, TruePBRShimRuntime.cpp:1605).
- Colour space at load: base colour and emissive forced sRGB; normal, RMAOS, subsurface linear; specular-colour map sRGB
  (alpha linear) -- TruePBRShimRuntime.cpp:1848, 1852, 1856, 1862, 1867-1868, 1876-1877; loader flag :1703.
- Texture identity keys elsewhere use `CanonicalTexturePathKey` (case + slash folded) -- TruePBRShimRuntime.cpp:548.

## 5. PBRM envelope and v6 (runtime parser in wt-spec1 + PBRM-v6.md)

Runtime envelope (PBRM.cpp:1143-1206, constants PBRM.h:18-35):
- bytes 0-3 ASCII `PBRM`; u32 LE version at 4; u32 LE payload size at 8; UTF-8 JSON payload from 12 that must consume the
  file EXACTLY; payload <= 64 MiB.
- accepted versions: 4, 5, 6 (kLegacyEnvelopeVersion / kSpecularV5EnvelopeVersion / kEnvelopeVersion) -- anything else
  "unsupported PBRM envelope version" -- PBRM.cpp:1157-1162.
- JSON root must be an object with `schema` == "FO4.PBRM.Material", `schemaVersion` == the envelope version, non-empty
  `shader` -- :1187-1206. Optional objects `primaryUv`, `secondaryUv`, `tertiaryUv`, `resources`, `settings`,
  `requirements` -- :1208-1214; booleans `secondaryUvEnabled`, `tertiaryUvEnabled` -- :1216-1228.
- `requirements`: {capability: positive integer version}; an unmet one makes the material valid-but-unrenderable -- :1236-1255.

From the format docs (PBRMaterialEditorQt\docs):
- Envelope table PBRM-v6.md:19-28 (its row at :22 still says "`5`; `4` accepted on read" -- stale; :422 says 4, 5 or 6).
- JSON root example with `schemaVersion: 6`, required `schema`/`schemaVersion`/`shader` -- PBRM-v6.md:30-61. Unknown
  shader name = valid but unsupported, never treated as Standard -- :55-57. Unknown keys are additive and ignored -- :216-219.
- Slot object `{enabled, expanded, path, values}`; `expanded` is editor-only -- :196-214.
- A texture is sampled only if slot `enabled`, valid non-empty path, and the matching `override*` is false -- :358-366.

v6 fields (PBRM-v6-Specular.md:9-25, PBRM-v6.md:163-187, 325-356):
- `primaryRmaos` alpha: v5 = dielectric F0 (clamped 0.16); v6 = OpenPBR specular WEIGHT 0..1. Constant `f0` (0.04) renamed
  `specularWeight` (1.0). `alphaCarries` "Specular Weight"/"Porosity"; sampled only while "Specular Weight" and
  `overrideSpecularWeight=false`. Runtime: PBRM.cpp:876-894.
- Pattern B = feature parameters live on the slot's `values` (PBRM-v6.md:163-187). For specular: `primarySpecularColor.values`
  holds `overrideColor` (default true), `color` (#FFFFFF sRGB), `ior` (1.5), `iorMax` (4.25), `overrideIor` (default true).
  `ior`/`iorMax` are read whether or not the slot is enabled; a disabled/absent slot = white tint + constant IOR -- PBRM-v6.md:336;
  runtime PBRM.cpp:963-1008.
- Maths: F0_ior = ((ior-1)/(ior+1))^2; ior = overrideIor ? ior : specA x iorMax; tint = sRGB-decoded (overrideColor ? color :
  map RGB); F0_diel = specularWeight x tint x F0_ior; F0 = lerp(F0_diel, baseColor, metallic). No 0.16 clamp in v6 --
  PBRM-v6-Specular.md:27-46, PBRM-v6.md:340-356. Defaults give F0 0.04 = v5 default exactly (:348-349).
- overrideIor=false with no map: runtime clamps ior to min(ior, iorMax) to mirror the editor preview -- PBRM.cpp:1001-1007.
- `primaryNormal.curvatureInAlpha` retired -> `alphaCarries` "None"/"Curvature"/"Cone Map" (Cone reserved, nothing samples it)
  -- PBRM-v6-Specular.md:23-24, 117-124; runtime PBRM.cpp:850-863.
- Emission: v6 editor writes `luminance` in nits; runtime intensity = luminance/100, falls back to old `intensity` key -- PBRM.cpp:916-943.
- Feature-bit mismatch to note (derived, not serialized, so harmless on disk): doc says SpecularIorTexture = bit 26 and
  NormalConeMap = 27 (PBRM-v6.md:384-385); the wt-spec1 runtime uses bit 30 and 31 (PBRM.h:119-120). Doc s5 says bit 26 needs
  bit 25 too (PBRM-v6-Specular.md:109-110), but the runtime sets the IOR bit independently of overrideColor (PBRM.cpp:989-991).

What a v5 reader must do with a v6 file:
- REFUSE it: "A v6 file is refused by a v5 reader (schemaVersion gate, as before)" -- PBRM-v6-Specular.md:95; "no best-effort
  read of future versions" -- PBRM-v6.md:434-435. In FO4CS a refused/unsupported PBRM leaves the vanilla BGSM rendering
  (item 1 failure behaviour). A v6 reader reads v4/v5 with the old F0 law byte for byte (PBRM.h:19-23; PBRM.cpp:891-894),
  and the editor migrates constants w = min(f0/0.04,1), ior = (1+sqrt f0)/(1-sqrt f0) when f0 > 0.04 --
  PBRM-v6-Specular.md:73-95.

CONTRADICTION between format doc and runtime (important for NifSkope):
- PBRM-v6.md:440-442: "PBRM is standalone. A NIF directly references its `.pbrm`; no same-name BGSM or BGEM is consulted."
- The wt-spec1 runtime does the OPPOSITE: it refuses a direct `.pbrm` name (PBRM.cpp:1368-1371) and only finds the .pbrm as
  the same-name sibling of the NIF's BGSM/BGEM, layered over the engine-loaded BGSM (TruePBRShimRuntime.cpp:80-84).
  To match the game TODAY, NifSkope should follow the runtime (sibling rule + swap rule + FO76 BGSM rule), and may accept a
  direct `.pbrm` name as the doc's intended future without the game honouring it yet.

## Unknowns
- Which JSON the owner means: `.pbrmset` (editor, per-submesh-name -> .pbrm path) vs a not-yet-built material section of
  `.nifx`. Neither is read by the runtime today.
- `.pbrmset` file-name convention (stem.pbrmset beside the NIF?) -- not in the docs; the editor src was out of scope.
- Rule B's diffuse name comes from the engine's live texture set after a material swap; NifSkope only has the NIF's own
  BGSM, so it can reproduce the swap rule only if it knows the swapped diffuse (OMOD/MSWP data from the ESP).

## ADDENDUM (bungo ruling 23:0x: the owner's ".json linked to our nif" = the .nifx sidecar)

### (a) .nifx exact current schema (wt-spec1\src\NifSidecar\)
- File name/location: same folder as the mesh, `<nifstem>.nifx`. Derivation NifSidecar.cpp:376-415: lower-case, `\` -> `/`,
  leading `/` dropped (:376-390); must end `.nif` with a non-empty stem, else refused (:399-411); result = path + "x" (:412-413).
  E.g. `Meshes\Foo\Bar.nif` -> `meshes/foo/bar.nifx`. No path field inside; location is the binding (NifSidecar.h:26-29).
- Top level: JSON object (NotObject refused, NifSidecar.cpp:564-567); <= 1 MiB, <= 64 sections, <= 4096 nodes (NifSidecar.h:79-81,
  NifSidecar.cpp:568-571, 604-607).
- `"version"`: REQUIRED number, must equal 1 exactly; missing -> MissingVersion, any other value -> UnsupportedVersion
  (NifSidecar.cpp:573-585; kSidecarVersion NifSidecar.h:74).
- Sections: every other top-level key is a section name. Only `fakeVolume` is known (exact, case-sensitive compare,
  NifSidecar.cpp:437); unknown sections are kept by name and ignored with a note (:595-600).
- `fakeVolume` = object of `{ "<node name>": { "bulgeStrength": f, "wrapContrast": f, "powderContrast": f } }`, each optional,
  finite, clamped to [0,1] (NifSidecar.cpp:470-521); an entry that sets none is dropped (:523-531).
- Node matching: the key is a BARE NODE NAME (NifSidecar.h:39), compared ASCII case-insensitively against the draw's node name
  (FindFakeVolumeNode, NifSidecar.cpp:613-625). No block index, no shape path.
- There is NO material/PBR section and no key naming a .pbrm or shader property.
  Example: `{"version":1,"fakeVolume":{"SmokeEmitter01":{"bulgeStrength":0.5}}}`.

### "Nothing loads it" -- confirmed
- Scoped grep of wt-spec1\src for `nifsidecar::` / `NifSidecar.h`: only NifSidecar.cpp itself and two COMMENTS in
  src\Lights\LightSources.h:686, :702. No caller of RegisterLoadedModel, RegisterNode, SidecarStore, SidecarPathForModel,
  FindFakeVolumeNode or ResolveFakeVolumeDialsForDraw outside NifSidecar\ (grep over src, *.h/*.cpp).
- The header says so: the NIF-load seam is owed, "With nothing registered every lookup answers 'no model'" (NifSidecar.h:385-398).

### (b) Existing PBR precedence (restated, file:line)
1. Material swap: OnLoadTextureSet2 asks `ApplyPbrmForTextureSwap` first (TruePBRShimRuntime.cpp:4489-4492); candidates
   `Materials\<dir>\<stem>.pbrm` then parent dir, from a `_d.dds` diffuse (PBRM.cpp:1396-1448); an existing swap binding is kept by
   the root seat (TruePBRShimRuntime.cpp:4270-4281).
2. Same-name sibling: shader property name -> NormalizeMaterialPath -> `.bgsm`/`.bgem` swapped for `.pbrm`
   (PBRM.cpp:1339-1394; seats TruePBRShimRuntime.cpp:4284-4295, 4393-4403, 4588-4596, effect :3525-3570).
3. FO76 BGSM: only after the sibling declined, applied last (TruePBRShimRuntime.cpp:4404-4412, 4597-4602, 3447-3494).

Direct `.pbrm` in the NIF -- runtime REFUSES it, PBRM.cpp:1368-1371:
    if (EndsWithInsensitive(normalized, ".pbrm")) {
        if (diagnostic) *diagnostic = "direct .pbrm NIF references are reserved for future runtime support";
        return std::nullopt;
    }
The format doc says the opposite, PBRMaterialEditorQt\docs\PBRM-v6.md:440-441:
    "PBRM is standalone. A NIF directly references its `.pbrm`; no same-name BGSM or
    BGEM is consulted."
