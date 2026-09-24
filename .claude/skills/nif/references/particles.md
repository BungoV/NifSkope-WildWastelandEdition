# NIF Particle Systems

Particle effects in FO4 use the NiPSys (NetImmerse Particle System) framework. Particles are emitted, simulated, and rendered entirely within the NIF.

## Particle System Architecture

A complete particle effect consists of:

```
NiNode "smoke_parent"                    ← positioning node
├── NiNode "emitterRefNode"              ← emitter position reference
│   └── NiNode "gravityRefNode"          ← gravity direction reference
└── (sibling or nearby)
NiParticleSystem "SmokeL"                ← the particle system itself
├── NiPSysData                           ← particle pool (max particles, vertex data)
├── BSEffectShaderProperty               ← how particles look (texture, blending)
├── NiAlphaProperty                      ← alpha blend mode
├── NiPSysAgeDeathModifier               ← kills particles when they expire
├── NiPSysSpawnModifier                  ← respawns particles on death
├── BSPSysLODModifier                    ← distance-based LOD
├── NiPSysBoxEmitter                     ← WHERE particles spawn
├── BSPSysSimpleColorModifier            ← color over lifetime
├── NiPSysRotationModifier               ← spin particles
├── BSPSysSubTexModifier                 ← animated sprite sheet (flipbook)
├── BSPSysScaleModifier                  ← size over lifetime
├── NiPSysGravityModifier                ← gravity force
├── NiPSysPositionModifier               ← position update
├── NiPSysBoundUpdateModifier            ← bounding box update
└── NiPSysDragFieldModifier              ← drag/friction force
```

Plus a companion **billboard mesh**:
```
BSTriShape "smoke_mesh_l"                ← pre-built quad mesh for particle rendering
├── BSEffectShaderProperty               ← effect shader with smoke texture
└── NiAlphaProperty
```

## Emitter Types

| Emitter Type | Emission Shape |
|-------------|---------------|
| `NiPSysBoxEmitter` | Rectangular volume (width x height x depth) |
| `NiPSysSphereEmitter` | Sphere volume |
| `NiPSysCylinderEmitter` | Cylinder volume |
| `NiPSysMeshEmitter` | Emit from mesh surface |

Emitter properties: birth rate, speed, speed variation, declination/planar angles, lifespan, lifespan variation, initial size, initial color, and a reference to the emitter node.

## Modifier Types

Modifiers update particle properties each frame, in processing order.

| Modifier Type | Effect |
|--------------|--------|
| `NiPSysAgeDeathModifier` | Kills expired particles, triggers spawn |
| `NiPSysSpawnModifier` | Respawns new particles from dying ones |
| `BSPSysLODModifier` | Reduces particle count at distance |
| `BSPSysSimpleColorModifier` | Color gradient over lifetime (birth -> peak -> death) |
| `NiPSysRotationModifier` | Spin speed and axis |
| `BSPSysSubTexModifier` | Animated sprite sheet (flipbook) |
| `BSPSysScaleModifier` | Size curve over lifetime |
| `NiPSysGravityModifier` | Gravity force (references a gravity node) |
| `NiPSysPositionModifier` | Updates particle positions |
| `NiPSysBoundUpdateModifier` | Recalculates bounding box |
| `NiPSysDragFieldModifier` | Air resistance / drag |
| `NiPSysAirFieldModifier` | Directional air current |
| `BSWindModifier` | Wind effect matching game weather |
| `NiPSysBombModifier` | Explosive radial force |
| `NiPSysColliderManager` | Particle-world collision |

## Controlling Particles with Animations

Particle systems are driven by NiControllerSequences via:

**`NiPSysEmitterCtlr`** — Controls the emitter. Needs TWO controlled blocks per sequence:
1. **Float interpolator** → birth rate over time (e.g. ramp from 0→6→10 particles/sec)
2. **Bool interpolator** → emitter active state (true=emitting, false=stopped)

**`NiPSysUpdateCtlr`** — Ticks the particle simulation forward. Usually simple on/off.

**`NiVisController`** — Controls visibility of the companion billboard mesh.

**`BSEffectShaderPropertyFloatController`** — Animates the billboard mesh's shader (alpha fade, emissive intensity). Property type = "BSEffectShaderProperty".

## Complete Smoke Effect Example (SawedOff)

**Sequences:** `LSmoking` (hard smoke), `LSmokingSoft` (soft smoke), `LWaiting` (idle), and R variants.

**LSmoking sequence** (5 seconds, CLAMP):
```
ControlledBlock 1: SmokeL / NiPSysEmitterCtlr → NiFloatInterpolator
  Birth rate: 0→6→10 over 4 seconds

ControlledBlock 2: SmokeL / NiPSysEmitterCtlr → NiBoolInterpolator
  Active: true at t=0, false at t=4

ControlledBlock 3: smoke_mesh_l / NiVisController → NiBoolInterpolator
  Visible: true at t=0

ControlledBlock 4: smoke_mesh_l / BSEffectShaderPropertyFloatController
  Alpha: 0→0.5→0 (fade billboard in and out)

ControlledBlock 5: smoke_mesh_l / BSEffectShaderPropertyFloatController
  Emissive: 0→0.3→0 (glow intensity curve)
```

**Key insight:** Each `NiPSysEmitterCtlr` needs TWO controlled blocks — one for the birth rate float and one for the active bool. Unlike other controllers which need only one.

## NiPSysData

The particle data block holds the particle pool:
- Maximum active particles
- Vertex positions/colors/sizes for active particles (runtime state)
- Has vertices, normals, colors — but these are PARTICLE vertices, not mesh geometry
- The BSEffectShaderProperty on the NiParticleSystem determines the visual

## Particle Rendering

FO4 particles render in two complementary ways:
1. **NiParticleSystem** — hardware point sprites / generated quads
2. **BSTriShape billboard mesh** — pre-built mesh with effect shader, animated to fade in/out

Both are needed for the full effect. The billboard mesh provides volumetric look; the particle system provides individual particle motion.

## Common Particle Textures

| Texture | Use |
|---------|-----|
| `textures/effects/smokecloudbrighttile_d.dds` | Bright smoke clouds |
| `textures/effects/smokecloud01_d.dds` | Standard smoke |
| `textures/effects/fire*.dds` | Fire/flame effects |
| `textures/effects/sparks*.dds` | Spark effects |
| `textures/effects/muzzleflash*.dds` | Muzzle flash |
| `textures/effects/dust*.dds` | Dust/debris |
