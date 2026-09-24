# NIF Animation System

Animations in NIF files use a controller/interpolator/keyframe architecture. The behavior graph (`.hkx`) triggers named sequences; the NIF stores the actual keyframe data.

## Architecture

```
NiControllerManager (on root node)
├── NiControllerSequence "FireGun"
│   ├── ControlledBlock: node=WeaponBolt, ctrl=NiTransformController
│   │   └── NiTransformInterpolator → NiTransformData (translation/rotation/scale keys)
│   ├── ControlledBlock: node=MuzzleFlash, ctrl=NiVisController
│   │   └── NiBoolInterpolator → NiBoolData (visibility on/off keys)
│   └── ControlledBlock: node=GlowMesh, ctrl=BSEffectShaderPropertyFloatController
│       └── NiFloatInterpolator → NiFloatData (alpha/emissive keys)
├── NiControllerSequence "Idle"
│   └── ...
├── NiMultiTargetTransformController (bone transforms)
└── NiDefaultAVObjectPalette (name→block mapping)
```

## NiControllerSequence

A named animation clip with timing and a list of controlled blocks.

| Property | Description |
|----------|-------------|
| `name` | Sequence name (matched by behavior graph) |
| `start_time` / `stop_time` | Time range in seconds |
| `frequency` | Playback speed multiplier (usually 1.0) |
| `cycle_type` | 0=LOOP, 1=REVERSE, 2=CLAMP |
| `weight` | Blend weight (usually 1.0) |
| `accum_root_name` | Root bone for motion accumulation |
| `text_key_data` | → NiTextKeyExtraData with "start"/"end" markers |
| `controlled_blocks` | List of ControllerLink entries |

### Naming Conventions

| Prefix | Meaning | Example |
|--------|---------|---------|
| (none) | Standard animation sequence | `partA`, `stage1`, `Reload` |
| `x_` | Controller-based sequence driving shader/emitter properties | `x_ammoLoopOn`, `x_texScroll` |

## ControllerLink (Controlled Block)

Each controlled block maps an interpolator to a target node+controller.

| Field | Description |
|-------|-------------|
| `node_name` | Target node/shape name (e.g. "WeaponBolt") |
| `controller_type` | Controller class name (e.g. "NiTransformController") |
| `property_type` | If targeting a shader property (e.g. "BSEffectShaderProperty") |
| `interpolator` | → NiInterpolator subclass with keyframe data |
| `controller` | → Controller block (may be shared via NiBlend*Interpolator) |

## Controller Types

| Controller | What It Drives | Interpolator |
|------------|----------------|--------------|
| `NiTransformController` | Node position/rotation/scale | `NiTransformInterpolator` → `NiTransformData` |
| `NiMultiTargetTransformController` | Multiple bones (skeleton) | N/A (managed by behavior) |
| `NiVisController` | Node visibility on/off | `NiBoolInterpolator` → `NiBoolData` |
| `BSEffectShaderPropertyFloatController` | Effect shader float (emissive, alpha, UV offset) | `NiFloatInterpolator` → `NiFloatData` |
| `BSEffectShaderPropertyColorController` | Effect shader color (emissive RGB) | `NiPoint3Interpolator` → `NiPosData` |
| `BSLightingShaderPropertyFloatController` | Lighting shader float | `NiFloatInterpolator` → `NiFloatData` |
| `BSLightingShaderPropertyColorController` | Lighting shader color | `NiPoint3Interpolator` → `NiPosData` |
| `NiLightDimmerController` | Point/spot light intensity | `NiFloatInterpolator` → `NiFloatData` |
| `NiLightRadiusController` | Light radius | `NiFloatInterpolator` → `NiFloatData` |
| `NiLightColorController` | Light color | `NiPoint3Interpolator` → `NiPosData` |
| `NiAlphaController` | Material alpha | `NiFloatInterpolator` → `NiFloatData` |
| `NiPSysEmitterCtlr` | Particle emitter birth rate + active state | See particles reference |
| `NiPSysUpdateCtlr` | Particle system update tick | N/A |

## Interpolator Types

| Interpolator | Data Block | Key Format |
|-------------|------------|------------|
| `NiTransformInterpolator` | `NiTransformData` | Translation + quaternion rotation + scale keys |
| `NiFloatInterpolator` | `NiFloatData` | `(time, float_value)` keys |
| `NiBoolInterpolator` | `NiBoolData` | `(time, bool_value)` keys |
| `NiPoint3Interpolator` | `NiPosData` | `(time, (x,y,z))` keys |
| `NiBlend*Interpolator` | None | Blends child interpolators from different sequences |

**NiBlend*Interpolator** — Placed on controllers targeted by multiple sequences. Manages transitioning between per-sequence interpolators. Doesn't hold keyframe data.

## Keyframe Data Example

From SawedOff smoke — `LSmoking` drives effect shader alpha 0→0.3→0 over 5 seconds:

```
NiFloatData for smoke_mesh_l BSEffectShaderPropertyFloatController:
  t=0.000  v=0.0000   ← invisible
  t=0.500  v=0.3000   ← fade in
  t=5.000  v=0.0000   ← fade out
```

## Text Key Markers

Every sequence should have `NiTextKeyExtraData` with at minimum:
```
t=0.000  "start"
t=5.000  "end"
```

These tell the behavior graph when the sequence begins and ends. Additional markers like `"hit"` or `"sound"` can trigger gameplay events.
