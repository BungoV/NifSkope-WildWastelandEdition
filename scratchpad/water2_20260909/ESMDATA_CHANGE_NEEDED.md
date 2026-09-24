# A cross-lane change lane WATER2 did NOT make: a WATR accessor on `EsmWorld`

Lane WATER2 owns `src/lodtfile.*`, the `lodl` CLI, the plane list and its own
tests. `src/esmdata.h` / `src/esmdata.cpp` are not its files, so this is written
down instead of done (CONSTITUTION rule 1, one lane per file).

## What is missing

The water module's fallback floor for flow is vanilla's only flow signal, the
`WATR` record's `NAM0` **Linear Velocity** (a `vec3`, per FORM). `EsmWorld`
exposes the worldspace's default water type and a cell's `XCWT`, and nothing
else about a `WATR` record:

```
bool cellWater( int cx, int cy, float & height, quint32 * typeForm = nullptr ) const;
quint32 defaultWaterType() const;
```

There is no way to get from a form id to that form's fields.

## What WATER2 did instead, and why it is a stopgap

`src/lodtfile.cpp` opens the plugin a SECOND time, with its own `ESMFile`, only
when the water module is on and a path was given (`LodtWaterOptions::
velocityPlugin`, filled by `nifcli.cpp` from the `lodgen` command's own ESM
argument), and reads `NAM0` for the fifteen interned `WATR` forms plus the
worldspace default.

Costs, in order of how much they matter:

1. **A second full index of the plugin.** `ESMFile`'s constructor indexes every
   record; `EsmWorld` has already done that. For `Fallout4.esm` that is a few
   seconds and a few hundred MB, paid once, only under `--water-bodies`.
2. **The `.btd` path cannot use it at all** unless a caller passes a plugin,
   because a `.btd` has no plugin of its own.
3. **A second place that knows how to open a plugin**, which is what the shared
   -code rule exists to prevent (CONSTITUTION rule 10).

## The change that retires it

```cpp
//! One WATR form's Linear Velocity (NAM0), vanilla's only flow signal.
//! False when the form is not a WATR or carries no NAM0. Cached.
bool waterVelocity( quint32 watrForm, float & vx, float & vy, float & vz ) const;
```

Twenty-odd lines beside `EsmWorld::grass()`, which already does exactly this
shape of thing (`findRecord`, check the type, walk `ESMField`, cache). With it,
`lodtWrite( const EsmWorld & world, ... )` fills `LodtSource::waterVelocity`
from the world it was handed, `LodtWaterOptions::velocityPlugin` is deleted, and
`src/lodtfile.cpp` stops including `esmfile.hpp`.

## The fallback while it is not done

Named, never silent: with no plugin (or a plugin that cannot be read) the
velocity arm is simply absent, the census says so in words —

```
REFUSED ARM: N bodies have no WATR NAM0 to fall back on (no plugin was supplied
to read velocities from), so their flow is `none` rather than a made-up number
```

— and every affected body's `flow source` byte is 0. Nothing is invented.
