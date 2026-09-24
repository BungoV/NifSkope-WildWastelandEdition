# PBRRENDER0 -- FO4 lookdev test scene (research, read-only)

2026-09-23. Corpus = E:\Tools\Fallout 4\DataUnpacked\Data (listed with depth limits, headers read by
dds_headers.py in this folder). Fallout4.esm is NOT in E:\Tools\Fallout 4\Data (that folder does not exist);
it is X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm (330,776,576 bytes), the path
src\cellview.h:39 already cites.

## 1. Assets (every file confirmed to exist by listing)

Paths are relative to Data\.

### Sky dome
- meshes\sky\AtmosphereDome002.nif -- the sky dome
- meshes\sky\Atmosphere.nif -- the atmosphere shell
- meshes\sky\Clouds.nif -- the cloud layer shell (WTHR cloud textures are painted onto this)
- meshes\sky\Stars.nif (night only; not needed for a noon stage)
- Distant/shape cloud cards also ship (CloudDistant01..06, CloudShape01..06, each with _25/_50/_O and INV_
  variants) -- not needed for the minimal stage.

### Cloud layer (textures\sky\, 71 files)
- textures\sky\CloudsLower01_d.DDS 1024x1024 DXT5 11 mips (lower layer)
- textures\sky\CloudsUpper01_d.DDS 512x512 DXT5 10 mips (upper layer)
- textures\sky\CloudsHorizon01_d.DDS 1024x1024 DXT5 11 mips (horizon band)
- also CloudsLower02..04, CloudsUpper02..04, CloudsHorizon02/03, CloudsLowerLight01, CloudyTop01/02,
  CloudSheet01, CloudsFill_d (1.5 KB, flat fill). Correction after item 2: CommonwealthClear itself uses SkyrimCloudsUpper01/02, SkyrimCloudsHorizon01 and SkyrimCloudsFill, so the Skyrim* cloud files are LIVE; only Masser/Secunda are Skyrim leftovers.
- Which layer a weather uses is set per WTHR record (cloud texture subrecords, 00TX..L0TX in xEdit terms);
  pick them from the chosen weather, do not hard-code (see item 2 -- the WTHR reader belongs to another agent).

### Sun
- textures\sky\Sun.DDS 256x256 uncompressed 32bpp 9 mips (the disc)
- textures\sky\SunGlare.DDS 512x512 DXT5 10 mips (glare sprite)
- textures\sky\Sun_d.DDS 1024x1024 DXT5 11 mips (alt disc)
  (CLMT is where the game names the sun/glare textures; these are the vanilla files it points at.)

### Ground plane
- No landscape mesh needed: a generated quad (e.g. 8192 x 8192 units, UV tiled ~ every 512 units)
  with the vanilla landscape default set:
  - textures\landscape\Ground\CommonwealthDefault01_d.DDS 2048x2048 DXT5 12 mips
  - textures\landscape\Ground\CommonwealthDefault01_N.DDS 1024x1024 BC5 11 mips
  - textures\landscape\Ground\CommonwealthDefault01_s.DDS 1024x1024 BC5 11 mips
  - material: materials\landscape\Ground\CommonwealthDefault01.bgsm
  Plain Commonwealth dirt: neutral enough not to tint the material under test.

### Legacy cubemaps (textures\shared\cubemaps\, 71 files)
- NifSkope's default FO4 cube: src\gl\renderer.cpp:69
  `static const QString cube_fo4 = "textures/shared/cubemaps/mipblur_defaultoutside1.dds";`
  (FO76 setting defaults to the same file, renderer.cpp:99.)
- Common vanilla ones (all 128x128, 8 mips, uncompressed BGRA8, six faces -- file size 524,408 = 128 + 6 faces
  x full mip chain):
  mipblur_DefaultOutside1.dds, mipblur_DefaultOutside1_dielectric.dds, mipblur_DefaultOutside1_bronze.dds,
  mipblur_DefaultOutside1_Copper.dds, mipblur_OutsideDesaturate_dielectric.dds, mipblur_OutsideBronze.dds,
  mipblur_InstInterior.dds (+_dialectric), mipblur_CGPlayerHouseCube.dds, mipblur_Eye1.dds, OutsideDay01.dds,
  ShinyDull_e.dds, CityBuildingsCube.dds, QuickSky_e.dds (32x32 DXT1, 6 mips).
- Header quirk: these files carry the cube-face bits (0xFE00) in dwCaps (reads 0x40FE08) and dwCaps2 = 0.
  A strict DDS reader that tests dwCaps2 & 0x200 will call them 2D. NifSkope already loads them, so its
  loader tolerates this; any NEW loader (IBL prefilter) must reuse that path, not a fresh parser.

## 2. The sun and ambient (one clear-day weather, noon)

Scope note: a separate agent owns the WTHR subrecord map and the any-plugin weather picker. This item is only
the values of ONE vanilla weather, read by wthr_noon.py (this folder) straight from Fallout4.esm.

Weather: **CommonwealthClear, FormID 0002B52A** (Fallout4.esm has 71 WTHR records; DefaultWeather 0000015E and
CommonwealthClearestSkies 001D670E also exist). Subrecord sizes as read: NAM0 608 bytes = 19 colour types x
8 times of day x RGBA8; FNAM 72 bytes = 18 floats; 8 x DALC(32). Column/row names below follow the FO4 xEdit
convention (times: Sunrise, Day, Sunset, Night, then EarlySunrise, LateSunrise, EarlySunset, LateSunset; the
second entry is Day). The sizes match that convention; the names are the other agent's to confirm.

NAM0, Day column (0-255, as stored):
| type | R G B | | type | R G B |
|---|---|---|---|---|
| Sunlight (direct) | 225 225 225 | | Sky upper | 38 64 99 |
| Sun (disc) | 255 255 255 | | Sky lower | 92 122 157 |
| Ambient | 93 93 93 | | Horizon | 130 145 162 |
| Fog near | 78 124 177 | | Fog far | 166 192 221 |
| Fog near high | 57 74 93 | | Fog far high | 114 146 179 |
| Sun glare | 255 255 255 | | Effect lighting | 150 150 150 |
| Sky statics | 227 238 240 | | Water mult | 225 225 225 |

Directional ambient, DALC #2 (Day), per axis, 0-255:
X+ 97 113 130 | X- 75 93 111 | Y+ 82 96 111 | Y- 93 113 132 | Z+ 42 52 62 | Z- 101 133 169 |
specular 161 176 180 | scale 1.0.
The bright blue axis (Z-, 101 133 169) is plainly the sky term and the dim one (Z+, 42 52 62) the ground
bounce, so it has to land on UP-facing normals; which sign convention the game uses is the other agent's
reader to confirm -- do not guess it in shader code.

Fog, FNAM (Day): near 3000, far 250000, power 0.35, max 0.85 (night 800 / 250000 / 0.3 / 0.85); the other
ten floats are the height-fog pairs (64 / 25000 etc.). At lookdev distances (a few hundred units) this fog
is effectively zero, so the stage can skip fog.

Also in the record: 11 cloud layers (00TX..?0TX), e.g. layer 0 Sky\SkyrimCloudsUpper01.dds, layer 2
Sky\CloudsLower04_d.dds, layer 3 Sky\SkyrimCloudsHorizon01.dds; IMSP = 8 image spaces (tone/exposure live
there, NOT in these bytes -- so these colours are relative, not absolute brightness).

How NifSkope maps them today: its renderer has one directional light (lightSourceDiffuse[0]) and one flat
ambient (lightSourceAmbient). Stage 1 = Sunlight -> diffuse, Ambient -> ambient, Sun colour -> the skybox
highlight. The six-axis DALC is the upgrade (a 6-colour ambient cube is trivial in a shader).

Sun direction: **fixed elevation 45 deg, azimuth 135 deg (front-left, from the camera's default view)**.
Why: a stage for comparisons must not move; 45 deg lights both the top and the front of an object and leaves
a readable shadow side, where a zenith sun hides normal-map detail on walls and a low sun turns every
comparison into a grazing-specular test. It is also near the real thing: Boston is at 42.4 N, so the
equinox noon sun sits at about 47.6 deg. It maps onto NifSkope's existing light controls
(declination / planarAngle, glview.cpp:3768).

## 3. IBL source for .pbrm materials

- Use **textures\shared\cubemaps\mipblur_DefaultOutside1.dds** -- the same file as cube_fo4, so legacy and
  .pbrm materials see the same sky. 128x128 per face, 6 faces, 8 mips (128..1), uncompressed BGRA8
  (8 bits per channel, LDR, no HDR sun in it), 524,408 bytes. "mipblur" = Bethesda already blurred each mip;
  that blur is not a GGX lobe.
- Today pbrm_default.frag:209 samples `textureLod(CubeMap, R, rough * 8.0)` (mip 8 does not exist; it clamps
  at 7) and multiplies by f0 instead of a BRDF LUT (comment at :210). Ambient diffuse is the flat A term;
  the header comment (:14-17) says the editor's 32-sample irradiance convolution was not ported.
- Must NifSkope prefilter? For a comparison harness against the PBRM editor: yes, both.
  - GGX specular prefilter at load: 6 faces x 128^2 x 8 mips is tiny (a one-shot GL pass or a CPU loop in
    milliseconds). Map mip = roughness * (mipCount-1), not rough * 8.
  - Irradiance: SH9 (9 RGB coefficients, one uniform array) projected from the 16^2 or 8^2 mip. Cheaper and
    smoother than an irradiance cube; replaces the editor's per-pixel 32-sample loop with the same answer.
  - Plus the split-sum BRDF LUT (128^2 RG16F, generated once), which the shader comment already names as
    missing.
  - Cache by file path + mtime; the vanilla cube never changes.
- Scale: the cube is LDR and unitless; multiply IBL by the weather's ambient level so the sun/ambient ratio
  from item 2 holds.

## 4. What NifSkope cannot draw today (scoped greps: src\gl, res\shaders)

- **BSSkyShaderProperty is not rendered.** AtmosphereDome002.nif, Atmosphere.nif, Clouds.nif and Stars.nif
  all use BSSkyShaderProperty on a BSTriShape (read from the files); the only src hits for that name are
  lib\importex\obj.cpp and spells\texture.cpp -- nothing in src\gl. In the game the dome's colour comes from
  the weather's Sky upper / Sky lower / Horizon colours, not from a texture, so loading the dome NIF alone
  gives nothing useful.
- **No cloud layer blending.** The game stacks up to 32 cloud layers on Clouds.nif with per-layer
  colour/alpha/speed from the weather (PNAM/JNAM/QNAM/RNAM); nothing like it exists.
- **No sun disc/glare sprite.** skybox.frag draws a GGX highlight in the light direction over the cube
  (skybox.frag:60-63) -- acceptable as the sun for now.
- **No shadows** (no shadow map anywhere in src\gl), so the ground will not receive the object's shadow.
- **No ground-plane generator**; a quad with CommonwealthDefault01.bgsm through the existing fo4_default
  path is all it needs.
- **No fog** (FNAM) -- not needed at stage distances.
- What exists: Renderer::drawSkyBox (renderer.cpp:1760) draws the cube map as the background with the sun
  highlight (skybox.prog), and every material already samples the same cube.
- Minimal first stage: cubemap-only background (existing drawSkyBox) + sun/ambient from item 2 + a ground
  quad. Stage 2: a small sky shader fed by the three weather sky colours instead of the cube background.
  Stage 3 (optional): clouds; then render that sky into the IBL cube so reflections match the backdrop.

## 5. Loading once, for three users

One `LookdevStage` object (cube path, sun dir, sun/ambient/DALC colours, ground on/off, weather id) with a
single `apply(Renderer&)`; the three users only choose whether to call it.
- Harness: `WW_LOOKDEV=1` (optional `WW_LOOKDEV_WEATHER=<EditorID or FormID>`), in the same family as the
  WW_RENDER_* switches read in nifskope_ui.cpp:22097+; default OFF so every existing shot stays
  byte-identical.
- Viewport: one row, "Lookdev stage" (on/off) in the render settings, following the panel style skill;
  the weather picker comes from the other agent's WTHR work.
- Card bake: **bake under a neutral stage, never the lookdev one.** docs\LODGEN_IMPOSTOR_SPEC.md:44 defines
  the card colour sheet as "colour (diffuse / albedo), unlit", with normal/height/AO in their own sheets;
  the game lights the card with its own sun and ambient, so a sky baked in would light it twice. The lookdev
  stage is still useful for the card PREVIEW (mesh vs card side by side under the same light).
