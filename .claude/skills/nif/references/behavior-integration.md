# NIF ↔ Behavior Graph Integration

NIF files can be linked to Havok behavior graphs (`.hkx`) that orchestrate animations at runtime. The behavior graph is the brain — it receives game events (attack, reload, equip) and tells the NIF which sequences to play and how to blend them.

## The Link: BSBehaviorGraphExtraData

The root node's `BSBehaviorGraphExtraData` (named "BGED") stores the behavior path:

```
BSBehaviorGraphExtraData "BGED"
  behavior_graph_file = "UniqueBehaviors\MinigunFX\MinigunFX.hkx"
  controls_base_skeleton = false
```

Path is relative to `Data/Meshes/`. File structure:
```
UniqueBehaviors/<Name>/
├── <Name>.hkx              ← project file (root)
├── Behaviors/Behavior.hkx  ← the actual behavior graph
└── Characters/Character.hkx ← character/skeleton binding
```

Plus `Meshes/AnimTextData/AnimationFileData/<Name>.txt` listing all HKX files.

## How Behaviors Drive NIF Animations

The behavior graph uses **BGSGamebryoSequenceGenerator** nodes to reference NIF controller sequences by name:

```
Behavior Graph                          NIF File
BGSGamebryoSequenceGenerator            NiControllerSequence
  pSequence = "x_ammoLoopOn"    ───►     name = "x_ammoLoopOn"
  fPercent = 1.0 (via variable)           controlled_blocks → interpolators → keyframes
  eBlendModeFunction = BMF_PERCENT
```

**BGSGamebryoSequenceGenerator properties:**

| Property | Description |
|----------|-------------|
| `pSequence` | NIF sequence name (must match `NiControllerSequence.name`) |
| `fPercent` | Blend weight (0.0-1.0), usually bound to a behavior variable |
| `eBlendModeFunction` | `BMF_NONE` (0), `BMF_PERCENT` (1), `BMF_ONE_MINUS_PERCENT` (2) |
| `eUseTimePercentage` | Time-based vs percentage-based playback |

## Variable Binding System

Behavior variables drive NIF animation parameters through **hkbVariableBindingSet**:

```
Variable: fToggleBlendDampened (float)
  │
  ├── binds to → partA.fPercent (BMF_ONE_MINUS_PERCENT)
  │              When dampened=0 → partA at 100%
  │
  └── binds to → partB.fPercent (BMF_PERCENT)
                 When dampened=0 → partB at 0%, dampened=1 → partB at 100%
```

This creates a crossfade between two NIF sequences controlled by a single behavior variable.

## Multi-NIF Weapon Architecture

FO4 weapons are assembled from multiple NIFs via connect points. Each NIF has its own controller sequences, but they share a single behavior graph (from the receiver's BGED):

```
Minigun_1.nif (receiver)          ← BGED → MinigunFX.hkx
├── x_ammoLoopOn sequence
├── x_ammoLoopOff sequence

MinigunBarrel_1.nif (barrel)      ← attached via P-Barrel
├── x_partA sequence               ← cold barrel look
├── x_partB sequence               ← hot barrel look

GunLgOverheatMPSLight.nif (light) ← attached via connect point
├── x_sequenceA sequence
└── SpecialIdle sequence
```

The behavior graph references ALL sequences across ALL attached NIFs.

## Key Behavior Patterns

| Pattern | Node Type | Purpose |
|---------|-----------|---------|
| **Set variables on state entry** | `BSAssignVariablesModifier` | Sets float/int values when entering a state |
| **Smooth transitions** | `hkbDampingModifier` | PID-style smoothing (kP controls speed) |
| **Crossfade sequences** | Two `BGSGamebryoSequenceGenerator` with BMF_PERCENT/ONE_MINUS | Blend via one variable |
| **Delayed events** | `BSTimerModifier` | Fire event after N seconds (variable-bound) |
| **No-animation state** | `hkbReferencePoseGenerator` | Placeholder when state only needs modifiers |
| **Conditional logic** | `hkbStateMachine` transitions | Route between states on game events |
| **Modifier + generator** | `hkbModifierGenerator` | Pairs logic with animation |
| **Multiple modifiers** | `hkbModifierList` | Apply several modifiers together |

## AnimationFileData Text File Format

**Do NOT manually create AnimationFileData files for weapon FX behaviors.** These are generated automatically by the Creation Kit when importing custom animations (HKT files). Weapon FX behaviors that only use NIF controller sequences (via BGSGamebryoSequenceGenerator) do NOT need AnimTextData — the engine finds the behavior via the BGED path alone.

AnimTextData is only needed when a behavior references standalone animation files (`.hkt`) that aren't embedded in the NIF. Vanilla weapon FX behaviors (CryolatorFX, MinigunFX, etc.) have AnimTextData because Bethesda created them through CK, but the files are not required for the behavior to function.

## Creating Custom Behavior-Driven Effects

**NIF side:**
1. Create NiControllerSequences for each visual state (e.g., `x_cold`, `x_hot`)
2. Each sequence drives shader controllers (float/color) on shapes
3. Add `BSBehaviorGraphExtraData` pointing to your behavior path
4. Set `BSXFlags` to include Animated (1)
5. For UI elements: BSEffectShaderProperty on quad meshes under OrderedRenderingNode

**Behavior side** (upstream: the `modkit data` CLI — **NOT AVAILABLE ON THIS MACHINE**; here,
search the unpacked corpus `E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\` for reference
behaviours and unpack them with HKXPACK — see the `behaivor-graph` skill):
- **Variable-driven** (smooth): hkbDampingModifier + BGSGamebryoSequenceGenerator crossfade
- **Event-driven** (discrete): Multi-stage state machine with transition+loop sequences
- Create variables, use BGSGamebryoSequenceGenerator to reference NIF sequences
- Build state machines, wire transitions to game events
