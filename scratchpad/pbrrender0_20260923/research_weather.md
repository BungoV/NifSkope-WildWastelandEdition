# PBRRENDER0 -- weather (WTHR) driven lookdev scene: research

Lane PBRRENDER0, research only, 2026-09-23. bungo: "Make it so you can load any weather from Fallout 4,
so from any .esm or .esp you load". Scope here: plugin reading + WTHR -> scene mapping.

## 1. What reads plugins today

**There is ONE reader: libfo76utils `ESMFile`** (`lib/libfo76utils/src/esmfile.cpp`, 597 lines), wrapped by
`EsmWorld` in `src/esmdata.cpp` (lodgen + Cell Editor). There is **no Plugin Editor / record editor in
src yet**: HANDOFF.md:536 (ruling 21:57 2026-09-19) lists "Plugin Editor (records, not built)"; HANDOFF.md:532
puts the "Plugin Editor shell (record tree, flat Name|Value)" in the CELLWORK2 queue. No `class *Plugin*`,
`*Record*`, `espeditor` in src. The Cell Editor passes a comma list to `EsmWorld::load` (src/cellview.cpp:495,
:630).

| Capability | Status | Where |
|---|---|---|
| Arbitrary record types (WTHR, IMGS, LGTM, CELL, WRLD, CLMT) | YES -- generic: every record indexed by FormID; `getRecord(formID)` + `ESMField` walks subrecords of any type; group tree via `children`/`next` (top-level GRUP label = record type) | esmfile.cpp:70-135 (index), :395 (getRecord), :403-479 (field walk, incl. XXXX big-field) |
| zlib-compressed records (flag 0x00040000) | YES -- `uncompressRecord` | esmfile.cpp:22-61; used by `ESMField(ESMFile&, const ESMRecord&)` :410 and `ESMField(ESMFile&, formID)` :429 |
| TRAP: the const `ESMField(const ESMRecord&, const ESMFile&)` ctor returns an EMPTY field list for a compressed record | silent | esmfile.cpp:438-448 -- the weather reader must use the non-const ctor |
| Several plugins in load order | YES -- one comma-separated list, order = load order (split on `.esm/.esp/.esl,`) | esmfile.cpp:163-184 |
| Master list (TES4 MAST) | YES -- read per file, basename match, case-folded | esmfile.cpp:216-276 |
| FormID remap (record header IDs) | YES -- top byte rewritten through the per-file master map | esmfile.cpp:63-68, :99 |
| FormID remap (FormIDs INSIDE subrecords) | YES but manual: caller must call `mapFormID(record, raw)` | esmfile.cpp:137-142; used e.g. esmdata.cpp:324, :407 |
| Override resolution | YES -- later file overwrites the record's data/flags/srcFile, i.e. last loaded wins (tree position stays the first file's) | esmfile.cpp:112-119 (`srcFile` = "input file index of the winning version", esmfile.hpp:19) |
| Missing master | NO REFUSAL -- absent master maps to identity (raw byte), silently; can alias another file's slot | esmfile.cpp:216-220 comment, :262-270 |
| ESL / light plugins | PARTIAL -- `.esl` name accepted (:174), ESL flag 0x200 never read; each file gets a full 8-bit slot (fine for reading, FE:xxx space not modelled); hard cap 256 input files (pluginMap 0x300/3) | esmfile.cpp:156, :174, :309 |
| Localized strings flag | read, but `esmFlags` is overwritten by each file (last file's flag wins) -- matters only for FULL names, not EDID | esmfile.cpp:195, esmfile.hpp:94 |
| Load-order hash (cache key) | YES | esmdata.cpp:933-957 |

Verdict: the weather picker needs NO new binary reader. It needs a thin `EsmWeather` layer on `ESMFile`:
walk the top-level `WTHR` group, list EDIDs, read one WTHR's fields with the non-const ESMField, map every
FormID subrecord through `mapFormID`, and add the missing-master refusal (section 5).

## 2. WTHR subrecords (xEdit + measured on Fallout4.esm)

Sources: `wbDefinitionsFO4.pas:13034-13108` (WTHR record) and the shared weather members in
`wbDefinitionsCommon.pas:9577-9951` (cached copy:
`C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\9c58247e-1b86-4aac-849d-0e519f2ccf55\scratchpad\wbDefinitionsCommon.pas`;
the FO4 file only calls `wbWeatherColors` etc., the layouts live in Common). Sizes MEASURED by
`scratchpad/pbrrender0_20260923/wthr_probe.py` (own struct+zlib decoder, not NifSkope's) over all 71
vanilla WTHRs: 0 of 71 compressed; form versions 40..131.

**Colour = `wbByteColors` = 4 bytes R,G,B,unused** (Common:6970). **Time-of-day (ToD) order in every 8-slot
array: 0 Sunrise, 1 Day, 2 Sunset, 3 Night, 4 EarlySunrise, 5 LateSunrise, 6 EarlySunset, 7 LateSunset**
(Common:8634-8643; slots 4-7 only from form version 111).

| Sub | Layout | Measured sizes | Stage |
|---|---|---|---|
| `EDID` | zstring | -- | 1 (picker label) |
| `NAM0` Weather Colors | 19 rows x 8 ToD x 4 B. Rows: 0 Sky-Upper, 1 Fog Near, 2 Unused, 3 **Ambient**, 4 **Sunlight**, 5 Sun (disc), 6 Stars, 7 Sky-Lower, 8 Horizon, 9 Effect Lighting, 10 Cloud LOD Diffuse, 11 Cloud LOD Ambient, 12 Fog Far, 13 Sky Statics, 14 Water Multiplier, 15 Sun Glare, 16 Moon Glare, 17 Fog Near High (fv>=119), 18 Fog Far High (fv>=119) (Common:9707-9757) | 608 (19x8x4) x65; **544 (17x8x4) x2; 272 (17x4x4) x4** | 1: rows 3,4 (+5 disc tint); 2: 0,7,8,6; 3: 1,12,17,18 |
| `DALC` x8 | one subrecord per ToD, SAME ToD order; each 32 B = 6 colours (X+,X-,Y+,Y-,Z+,Z-) + Specular colour (4 B) + float Fresnel Power (Common:6932-6952, 9759-9780) | 32 B always; **8 per record x67, 4 x4 (old form)** | 1 (directional ambient) |
| `IMSP` | 8 x FormID -> IMGS, ToD order (Common:9863-9884) | 32 x67, 16 x4 | 3 (exposure) |
| `FNAM` Fog Distance | floats: DayNear, DayFar, NightNear, NightFar, DayPower, NightPower, DayMax, NightMax, then (fv>=119) Day/Night NearHeightMid, NearHeightRange, HighDensityScale, (fv>=120) Day/Night FarHeightMid, FarHeightRange (Common:9795-9846) | 72 (18 floats) x63, 56 x2, 32 x6 | 3 |
| cloud textures | zstring per layer, 32 layers; signature = layer char + "0TX": `00TX`=#0 ... `90TX`=#9, `:0TX`..`@0TX`=#10..#16, `A0TX`..`O0TX`=#17..#31 (Common:9669-9701) | CommonwealthClear has 11 | 2 |
| `LNAM` | u32 max cloud layers (default 16) | 4 | 2 |
| `NAM1` | u32 disabled cloud layer bits | -- | 2 |
| `PNAM` Cloud Colors | 32 layers x 8 ToD x 4 B | 1024 x67, 512 (32x4x4) x4 | 2 |
| `JNAM` Cloud Alphas | 32 layers x 8 ToD x float | 1024 x67, 512 x4 | 2 |
| `RNAM` / `QNAM` | 32 x u8 Y / X cloud speeds (xEdit maps the byte through `wbWeatherCloudSpeedToStr`) | 32 / 32 | 2 |
| `DATA` | 20 B: WindSpeed u8, unused 2, TransDelta, **SunGlare u8 (/255)**, SunDamage, PrecipBeginFadeIn, PrecipEndFadeOut, ThunderBeginFadeIn, ThunderEndFadeOut, ThunderFrequency, Flags (Pleasant/Cloudy/Rainy/Snow/...), **Lightning colour R,G,B**, VisualEffectBegin, VisualEffectEnd, WindDirection (/255*360), WindDirRange, WindTurbulence (fv>=119) (FO4:13057-13087) | 20 x65, 19 x6 | reference / later |
| `MNAM` / `NNAM` | FormID precipitation SPGD / visual effect RFCT | -- | reference only |
| `WGDR` God Rays | 8 x FormID -> GDRY, ToD order (Common:9848-9861) | 32 x63 | later (volumetric) |
| `GNAM` | FormID -> LENS sun-glare lens flare | 4 x18 | later |
| `NAM4` | 32 floats, unknown | 128 | skip |
| `UNAM` magic, `SNAM` sounds, `TNAM` sky statics, aurora MODL, `VNAM`/`WNAM` volatility/visibility mult | -- | -- | skip |

Not in FO4 WTHR: **`HNAM` Volumetric Lighting** (Common:9933 is for FO76/Starfield; FO4's WTHR list does
not call it). FO4 god rays are `WGDR` -> GDRY.

**Sun texture and sun-glare texture live in the CLIMATE, not the weather:** `CLMT` FNAM = Sun Texture
path, GNAM = Sun Glare Texture path (FO4:6115-6126). DefaultClimate: `Sky\Sun_d.dds`, `Sky\SunGlare.dds`.
The weather carries only the glare STRENGTH (DATA byte), the colours (NAM0 rows 5 and 15) and the LENS form.

**IMGS** (FO4:7393-7464): `HNAM` HDR, 9 floats = EyeAdaptSpeed, TonemapE, BloomThreshold, BloomScale,
AutoExposureMax, AutoExposureMin, SunlightScale, SkyScale, MiddleGray; `CNAM` Cinematic = Saturation,
Brightness, Contrast; `TNAM` Tint = Amount + float RGB; `DNAM` depth of field; `TX00` LUT path (`ENAM` is
the old combined form).

**Measured gate values (Fallout4.esm, CommonwealthClear 0002B52A, form version 131):**
- NAM0 Sunlight Day = (225,225,225); Ambient Day = (93,93,93); Sun Day = (255,255,255);
  Sky-Upper Day = (38,64,99); Horizon Day = (130,145,162); Sunlight Night = (53,70,87)
- DALC[Day] X+ (97,113,130) X- (75,93,111) Y+ (82,96,111) Y- (93,113,132) Z+ (42,52,62) Z- (101,133,169),
  specular (161,176,180), fresnel 1.0
- FNAM day near/far 3000/250000, night 800/250000, power 0.35/0.30, max 0.85/0.85
- IMSP Day = 00216A9C `CW_ClearDAY_MAY19`: HNAM = 3.0, 0.02, 0.5, 0.2, 3.25, 1.6, 4.5, 2.4, 0.18; CNAM = 1,1,1
- DATA sun-glare byte 0x7F (= 0.5)

**Stage 1 takes:** NAM0 rows 3 Ambient and 4 Sunlight (row 5 Sun as the disc tint), all DALC (6 directions +
specular + fresnel), and CLMT TNAM for the clock. Everything else comes later (section 6).

Readers MUST decide the ToD count from the FORM VERSION (record header offset 20, u16), not from the size:
544 bytes is both 17x8x4 and 34x4x4. fv < 111 -> 4 ToD (map Early/Late slots to their neighbours);
fv 111..118 -> 17 NAM0 rows; fv >= 119 -> 19 rows.

## 3. Time of day

Already reverse-engineered by FO4CS: `E:\Projects\Fo4CommunityShaders\wt-spec1\docs\RE\solar-daynight.md`.

- **CLMT `TNAM`** = 6 bytes (Common:8709-8726): sunrise begin, sunrise end, sunset begin, sunset end,
  volatility, moons/phase. **Unit = 10 minutes, UNSIGNED** (hour = byte / 6; solar-daynight.md:62-80).
  DefaultClimate = [30, 54, 102, 126] -> sunrise 05:00-09:00, sunset 17:00-21:00. The 7 vanilla CLMTs:
  4 x (30,54,102,126), 2 x (33,48,96,123), 1 x (24,60,96,132) (measured).
- **Colour phases** (engine `GetTimes`, solar-daynight.md:169-192):
  - night: hour < sunriseBegin - 0.5 h, or hour >= sunsetEnd + 0.5 h (GMST `fDaytimeColorExtension` = 0.5,
    clamped to 0 / 23.99)
  - sunrise ramp [sunriseBegin - 0.5, sunriseEnd) cut into FOUR equal quarters, cross-fading
    Night -> EarlySunrise -> Sunrise -> LateSunrise -> Day (slots 3->4->0->5->1)
  - day [sunriseEnd, sunsetBegin] = pure slot 1
  - sunset ramp (sunsetBegin, sunsetEnd + 0.5) in four quarters: Day -> EarlySunset -> Sunset ->
    LateSunset -> Night (1->6->2->7->3)
- Fog (FNAM) has only Day/Night values; `Sky::UpdateFog` and `UpdateHDRValues` key on sunriseEnd/sunsetBegin.
- **The vanilla sun direction is not astronomy** (FO4CS `src/Sky/SolarPosition.h:6-24`):
  ramp = 1 - 2*(hour - sunriseBegin)/(sunsetEnd - sunriseBegin); pos = (ramp*400, 25, 400 - |ramp*400|)
  from GMSTs fSunXExtreme=400, fSunYExtreme=25, fSunZExtreme=-100 (a tent arc). The disc alpha fades over
  2 h (`fSunAlphaTransTime`) centred on the midpoint of each sunrise/sunset pair (solar-daynight.md:223-243).
  The light is floored at fSunShadowMinAngle (SolarPosition.h:756-760).

**Control design (hour slider):**
1. Slider 0.00..23.99 h (10-minute detents to match the record unit; free drag allowed), plus a climate
   picker defaulting to DefaultClimate 0000015F (or the worldspace's climate when one is known).
2. A pure function `todKeys(hour, tnam[4], ext = 0.5) -> (slotA, slotB, t)` implementing the table above,
   linear t within each quarter.
3. Every ToD quantity (NAM0 rows, DALC, cloud colours/alphas) = lerp(slotA, slotB, t) in the record's own
   units (byte/255, sRGB as stored; the lookdev linearises afterwards). IMSP: resolve both keys' IMGS and
   lerp their floats.
4. Sun direction from the vanilla tent arc above (a later toggle may offer real solar position, as FO4CS
   does). The summary line names the resolved keys, e.g. "LateSunrise 40% -> Day".
5. Four-ToD records (fv < 111): EarlySunrise/LateSunrise -> Sunrise, EarlySunset/LateSunset -> Sunset, so
   one function serves both.

## 4. Cubemaps -- not in WTHR

What FO4 actually does (FO4CS `docs/RE/envmap-technique-recon.md:10-19, 84-94`, measured in the shader
package): a BSLightingShader (.bgsm) material's own cubemap never reaches the deferred picture; the
G-buffer carries an env MASK and SCALE that modulate the engine's **live per-location probe cubemap array
(252 slices, t8)** in the ambient/IBL pass. The only place a material's own cubemap is composited is the
BSEffectShader (.bgem) envmap family. The live probes are captured from the world at runtime, so in game
the reflection environment IS the current weather's sky and surroundings.

Record fields that name an environment (xEdit FO4):
- material: BGSM `EnvmapTexture` string; TXST `TX05` Environment (FO4:6992)
- CELL `XWEM` Water Environment Map (FO4:6077), WRLD `XWEM` (FO4:13171) -- water only
- CELL `XCLL` (FO4:5994-6036) and LGTM `DATA` (FO4:8629-8663): ambient/directional/fog colours + a DALC
  block, NO cubemap; CELL `LTMP` -> LGTM, `XCIM` -> IMGS, `XGDR` -> GDRY. 66 LGTMs in Fallout4.esm, DATA 92/128/136 B.
- IMGS: no cubemap

Vanilla BGSM census (`bgsm_cubemap_census.py`, 6,616 BGSMs, 2,763 name a cubemap):
`shared\cubemaps\mipblur_defaultoutside1.dds` 1,309; `..._dielectric.dds` 244; metalbrushed01cube_e 137;
metalbrushed02cube_e 112; mipblur_outsidedesaturate_dielectric 102. 71 files in textures\shared\cubemaps.

**Recommendation:** in stage 1 the environment is a user choice with a default, never derived from the
WTHR: (a) the loaded material's own EnvmapTexture; else (b) `textures\shared\cubemaps\mipblur_defaultoutside1.dds`
(the corpus majority); (c) a picker over `textures\shared\cubemaps\*.dds`. The summary line names which of
(a)/(b)/(c) served. A later stage renders a sky cubemap FROM the weather (sky-upper/lower/horizon/sun colours,
then clouds) -- that is what the game's live probes approximate and the honest "weather drives reflections"
path; it waits for the sky-dome stage.

## 5. Mod weathers (Physical Weathers and any .esp)

Measured (`plugin_header_probe.py`): `FO4CSPhysicalWeathers.esp` flags 0x0 (plain ESP, not ESL, not
localized), masters = Fallout4.esm, DLCCoast.esm, DLCNukaWorld.esm; 69 WTHR, 0 compressed, 68 overrides
+ 1 new. DLCCoast.esm: ESM + localized, master Fallout4.esm, 16 new WTHR. So the target case needs a
multi-plugin load with masters; compression is not exercised by it but must still work (other mods ship
compressed records; ESMFile handles them, section 1).

What the picker needs, and which part ESMFile already gives:
1. **Load list.** User adds plugins (drop or browse, per the 2026-09-19 "drag and drop plugins into the
   editor" ruling, HANDOFF.md:532); the list is shown in load order and passed to `ESMFile` as one comma
   string, the same way the Cell Editor does (cellview.cpp:495, :630). Default helper: when the user picks
   one mod, offer "add its masters from the same Data folder" (read MAST first, cheap header read).
2. **Master resolution -- REFUSE, never guess.** Before constructing ESMFile, read every file's TES4 MAST
   list (a header-only pass, like the probe) and check each master appears EARLIER in the list
   (basename, case-folded, same rule as esmfile.cpp:253-269). If not: refuse with a named reason, e.g.
   `refused: FO4CSPhysicalWeathers.esp needs DLCNukaWorld.esm, which is not loaded (or is loaded after it)`.
   This is required because ESMFile silently maps an absent master to the raw byte (esmfile.cpp:216-220),
   which can alias another plugin's slot and show the WRONG record with no error.
3. **FormID remap.** Record IDs are remapped by ESMFile (esmfile.cpp:99). FormIDs inside WTHR fields
   (IMSP x8 -> IMGS, WGDR x8 -> GDRY, MNAM, NNAM, GNAM, UNAM, SNAM, TNAM) are RAW in the file and must go
   through `esm->mapFormID(record, raw)` (esmfile.cpp:137-142) against the WINNING record (its `srcFile`).
   Forgetting this is the classic bug: a mod's IMSP pointing into its own new IMGS resolves to a master's
   unrelated record.
4. **Override wins by load order.** ESMFile keeps the last file's version (esmfile.cpp:112-119). Listing:
   walk every top-level GRUP labelled `WTHR` (one per file; the chain is linked at esmfile.cpp:360-366).
   An override does NOT appear in the mod's own group chain (the record slot already has fileData, so
   `prv->next` is not relinked, esmfile.cpp:103-111); it appears once, under the master's group, carrying the
   winning data. So the walk yields each weather exactly once, already resolved. Show the winner:
   `EDID  [FormID]  from <basename of esmFiles[srcFile]>` and mark overrides ("Fallout4.esm, overridden by
   FO4CSPhysicalWeathers.esp") by comparing srcFile to the file that owns the FormID's load-order byte.
5. **ESL / light plugins.** Reading works (each input gets its own 8-bit slot). Differences that matter
   only for display: a light plugin's IDs should be shown as `FE xxx:yyy` to match xEdit, and the ESL flag
   (0x200) must be read from the header since ESMFile ignores it. Cap: 256 input files (esmfile.cpp:156);
   refuse above that with the count.
6. **Localized plugins** (Fallout4.esm, DLCs: flag 0x80): FULL names are string IDs; the picker uses EDID,
   so no .STRINGS reader is needed.
7. **Refusal lines** (one per failure, named): missing/out-of-order master; not a TES4 file; plugin count
   > 256; chosen form is not a WTHR; NAM0 size does not match the form-version rule (section 2); IMSP target
   missing or not IMGS; CLMT not found (fall back to DefaultClimate 0000015F and SAY so).

## 6. Staged plan, each stage with a gate that fails on broken code

**Stage W1 -- read + sun + ambient + cubemap (first stage).**
Scope: `EsmWeather` layer on ESMFile (list WTHRs, read one), master refusal, hour slider + climate TNAM,
the ToD blend function, sun direction (vanilla tent arc) + Sunlight colour (NAM0 row 4), ambient from DALC
(6-axis) with NAM0 row 3 as the flat fallback, the environment cubemap chosen per section 4.
Gates:
- G1 readback: load Fallout4.esm alone, read CommonwealthClear 0002B52A: NAM0 Sunlight Day == (225,225,225),
  Ambient Day == (93,93,93), Sunlight Night == (53,70,87); DALC[Day] Z- == (101,133,169). Values come from
  the independent decoder (`wthr_probe.py`), not the code under test. Fails on a wrong row/ToD stride
  (row 3 vs 4 swap gives (93,..) vs (225,..); ToD stride 4 vs 8 gives a different colour).
- G2 whole-corpus census: all 71 vanilla WTHRs parse with ZERO refusals and the NAM0 size histogram equals
  {608: 65, 544: 2, 272: 4} and DALC count {8: 67, 4: 4}. Fails if the form-version rule is wrong (the six
  old records refuse or misread).
- G3 blend known-answers (pure function, no build of the view needed): TNAM (30,54,102,126): 12:00 -> (Day,
  Day, 0); 02:00 -> (Night, Night, 0); 4:30 -> start of sunrise ramp (Night->EarlySunrise, t=0);
  the ramp is 4.5..9.0 h, quarter = 1.125 h, so 6.75 h -> (Sunrise->LateSunrise, t=0); 21:30 -> Night.
  A red control: feed a sabotaged function (slots in xEdit order 0..3 without the early/late insert) and
  show G3 fails.
- G4 mod override: load `Fallout4.esm,DLCCoast.esm,DLCNukaWorld.esm,FO4CSPhysicalWeathers.esp`; the
  CommonwealthClear winner's srcFile == the .esp and its NAM0 equals the .esp's bytes as read by the
  independent decoder (differs from vanilla, else the gate proves nothing -- check that first).
- G5 refusal: load the .esp without DLCNukaWorld.esm -> refusal line naming DLCNukaWorld.esm; zero records
  shown. Red control: with the check disabled, the load "succeeds" (proves the gate is load-bearing).
- G6 remap: a mod WTHR whose IMSP points at an IMGS inside the mod resolves to that IMGS (EDID match);
  construct a two-plugin fixture if no real one exists.
- G7 render: the Stage-1 picture differs between noon and midnight for the same WTHR (sun colour and
  ambient move), and the summary line names the keys + the cubemap source. (Picture checks follow the
  existing render-shot harness; the numeric gates above are the ones that fail on broken code.)

**Stage W2 -- sky dome + sun disc.** NAM0 Sky-Upper (0), Sky-Lower (7), Horizon (8), Sun (5), Stars (6);
CLMT FNAM sun texture + GNAM glare texture; DATA sun-glare strength; the sun-alpha fade (2 h at the
midpoints). Then render the weather cubemap from this dome and offer it as source (d) in section 4.
Gate: sample the dome at zenith/horizon at 12:00 CommonwealthClear == (38,64,99) / (130,145,162) (before
linearisation); a dome-to-cubemap readback of the +Z face centre equals the zenith colour.

**Stage W3 -- clouds.** 32 layers: `x0TX` textures, LNAM, NAM1 disabled bits, PNAM colours, JNAM alphas,
RNAM/QNAM speeds, Cloud LOD rows 10/11. Gate: layer count and texture list for CommonwealthClear == 11
names from the decoder; disabled-bit layers are not drawn (count drawn == 11 - popcount(NAM1 & mask)).

**Stage W4 -- fog.** FNAM near/far/power/max per day/night (+ height fog fields fv>=119), NAM0 Fog Near (1),
Fog Far (12), Fog Near/Far High (17/18). Day/night switch on sunriseEnd/sunsetBegin. Gate: fog factor at
distance = DayNear equals 0 and at DayFar equals DayMax (0.85 for CommonwealthClear).

**Stage W5 -- image space / exposure.** IMSP -> IMGS HNAM (eye adapt, bloom, exposure min/max, sunlight
scale, sky scale, middle grey), CNAM saturation/brightness/contrast, TNAM tint, TX00 LUT. Gate: IMGS Day of
CommonwealthClear read back == (3.0, 0.02, 0.5, 0.2, 3.25, 1.6, 4.5, 2.4, 0.18); the exposure control
reads SunlightScale 4.5 and changes the picture when the record changes.

**Later / reference only:** god rays (WGDR -> GDRY), lens flare (GNAM -> LENS), precipitation (MNAM SPGD,
NNAM RFCT), lightning colour (DATA), sounds, aurora, weather-to-weather transitions (DATA TransDelta),
CELL/LGTM interior lighting as an alternative light source for interior lookdev.

Open questions for bungo (not decided here): whether the stage-1 cubemap should be tinted by the weather
before the dome stage exists (recommend NO -- show it as authored and say so); whether the hour slider
defaults to noon or to the current system clock (recommend noon).
