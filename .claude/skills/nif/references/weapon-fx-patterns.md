# Weapon FX Patterns & Case Studies

Reusable visual effect patterns from FO4/FO76 weapons, with full NIF + behavior breakdowns.

## Pattern Catalog

| Pattern | Behavior Structure | Variables | NIF Requirements | Used By |
|---------|-------------------|-----------|-----------------|---------|
| **Heat/Cool Blend** | DampingModifier + 2x AssignVariables + Timer + Blender | fToggleBlend, fToggleBlendDampened, fDampRate, fCoolTimer | `partA`/`partB` sequences with shader controllers | Minigun, Shishkebab, Flamer, MG42 |
| **Multi-Stage** | Linear StateMachine with loop states | None (event-driven) | `stage1`–`stage4` + loop sequences | Meltdown, Sentry Bot Overheat |
| **Charged Weapon** | Parallel StateMachines per mesh part | None (event-driven) | Per-part sequence sets (`x_Muzzle*`, `x_Dish*`) | Gamma Gun |
| **Melee FX Overlay** | Dual sequences (base + x_FX) | None (event-driven) | `autoLoop`/`x_autoLoopFX` pairs | Auto Axe, Chainsaw |
| **Material Toggle** | StateMachine with matOn/matOff events | None (event-driven) | Simple on/off sequences | Ripper, Drill |
| **Generic VFX Lifecycle** | Standardized state cycle (Off→Startup→Idle→Attack→Shutdown) | None (event-driven) | `VFXOff`→`VFX_Startup`→`VFX_Idle`→`VFX_Attack`→`VFX_Shutdown` + loops | SharedFX template |

---

## Case Study: Minigun Barrel Heat

The barrel heat-up system demonstrates the full NIF↔behavior pipeline with smooth crossfading.

**Variables:**

| Variable | Purpose | Default |
|----------|---------|---------|
| `fToggleBlend` | Raw heat target (0=cold, 1=hot) | 0.0 |
| `fToggleBlendDampened` | Smoothed output driving the visual blend | 0.0 |
| `fDampRate` | PID kP rate (higher = faster transition) | 0.03 |
| `fCoolTimer` | Seconds before auto-cooldown fires | 0.5 |

**Events:** `attackStartAuto`, `attackStateExit`, `triggerEnd`, `CoolDown00`, `WeaponFire`, `reloadState`

**Behavior structure:**

```
hkbBehaviorGraph "Behavior.hkb"
└── hkbStateMachine "Behavior00"
    └── hkbBlenderGenerator "Blend01" (4 children)
        │
        ├── [child 0] BarrelHeatBlend (hkbModifierGenerator)
        │   ├── modifier: hkbDampingModifier
        │   │   kP ← fDampRate, rawValue ← fToggleBlend, dampedValue → fToggleBlendDampened
        │   └── generator: hkbBlenderGenerator "Blend00"
        │       ├── partA (x_partA, BMF_ONE_MINUS_PERCENT, fPercent ← fToggleBlendDampened)
        │       └── partB (x_partB, BMF_PERCENT, fPercent ← fToggleBlendDampened)
        │
        ├── [child 1] BenAlongSequenceLightControl (state machine, light effects)
        │
        ├── [child 2] AmmoChainStateMachine
        │   ├── ammoLoopOffState → x_ammoLoopOff
        │   └── ammoLoopOnState → x_ammoLoopOn
        │
        └── [child 3] BarrelHeatControl (state machine)
            ├── collValuesState (cold): fToggleBlend=0.0, fDampRate=0.0025
            │   → on attackStartAuto → hotValuesStage
            ├── hotValuesStage (hot): fToggleBlend=1.0, fDampRate=0.005
            │   + BSTimerModifier → fires CoolDown00
            │   → on triggerEnd/attackStateExit → collValuesState
            └── InstantCooldown: fToggleBlend=0.0, fDampRate=1.0 (fast reset)
```

**Flow:**
1. Player fires → `attackStartAuto` event
2. `BarrelHeatControl` transitions cold→hot, sets `fToggleBlend=1.0`
3. `hkbDampingModifier` smoothly ramps `fToggleBlendDampened` 0→1
4. `Blend00` crossfades `x_partA`→`x_partB` (cold→hot shader)
5. Player stops → `triggerEnd` → `fToggleBlend=0.0`
6. Damping ramps back down (rate=0.0025, slower cooldown)

---

## Case Study: Shishkebab Fire Effect

Demonstrates fire effects with **dual particle systems, dynamic lighting, and billboard meshes**.

**NIF:** `Shishkebab_1.nif` (261 blocks)

**Fire node structure:**
```
WEAPON (root)
├── TritiumDot01 (NiNode)           ← fire glow billboard #1
│   └── TritiumDot01:0 (BSTriShape + BSEffectShaderProperty)
├── TritiumDot (NiNode)             ← fire glow billboard #2
├── Omni001 (NiNode)                ← dynamic point light
│   └── NiPointLight                ← animated radius/dimmer/color
├── PCloud003-Emitter (NiNode)      ← fire particle emitter position
│   └── NiParticleSystem            ← main fire particles
├── PCloudSparks-Emitter (NiNode)   ← spark emitter position
│   └── NiParticleSystem            ← spark/ember particles
├── Drag001 (NiNode)                ← drag reference
└── WindNoiseSparks (NiNode)        ← wind reference
```

**Key differences from Minigun pattern:**
- **Asymmetric damping**: heat-up kP=0.1 (fast ignition), cool-down kP=0.03 (slow fade)
- **Dual particles**: Fire (flipbook + color gradient, no gravity) + Sparks (gravity, speed variation)
- **Dynamic light**: NiPointLight with radius/dimmer/color controllers
- **BSTimerModifier** auto-cools after 10 seconds of continuous flame

**Additional block types:**

| Block Type | Purpose |
|------------|---------|
| `NiPointLight` | Dynamic light source |
| `NiLightRadiusController` | Animates light radius |
| `NiLightDimmerController` | Animates light brightness |
| `NiLightColorController` | Animates light color |
| `NiPSysEmitterSpeedCtlr` | Animates emission speed |
| `NiPSysEmitterInitialRadiusCtlr` | Animates particle initial size |
| `NiPSysModifierActiveCtlr` | Toggles modifiers on/off |

---

## Case Study: Meltdown Overheat UI Bar (FO76)

Demonstrates **physical UI meshes on the weapon** — a progress bar that fills as the weapon overheats.

**UI mesh structure:**
```
WEAPON (root)
└── OrderedRenderingNode (NiNode)    ← ensures correct render order
    ├── UIBase:0 (BSTriShape)        ← background frame
    ├── UIProgressBar:0 (BSTriShape) ← fill bar (simple quad)
    ├── TriangleOrange:0             ← warning indicator
    ├── Triangle:0                   ← warning indicator base
    ├── ScanLines:0                  ← CRT scan line overlay
    └── Glass:0                      ← glass overlay
```

All UI shapes use BSEffectShaderProperty + NiAlphaProperty.

**Progress bar fill mechanism:** `BSEffectShaderPropertyFloatController` targeting `SourceTexVOffset` (variable 11). Scrolls the fill texture vertically to make the bar appear to fill bottom-to-top.

**4-stage event-driven behavior:**
```
stage1 → stage1Loop → [stage2] → stage2 → stage2Loop → [stage3]
→ stage3 → stage3Loop → [stage4] → stage4 → stage4Loop
                    [reset from any state] → stage1
```

**Three parallel blender channels:**
1. **CoreStages** — heat stage progression
2. **TexScroll** — continuous CRT scan line effect
3. **LightGlow** — light toggle

**Key patterns for reuse:**
- **OrderedRenderingNode** — back-to-front compositing for layered UI
- **UV offset scrolling** — animate SourceTexVOffset for fill/scroll effects
- **Multi-stage event-driven** — discrete stages instead of continuous blend
- **Parallel blender channels** — independent state machines via hkbBlenderGenerator children
- **Physical UI on weapon** — BSTriShape quads with effect shaders as in-world UI
