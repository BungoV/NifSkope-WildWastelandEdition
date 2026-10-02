#!/usr/bin/env python3
"""Lane EFX2's checker (tests/spells/cell_fx.sh): the cell-lit effects, judged from four shots of one camera.

  hide.png    WW_CELL_FX_RED=hide    the cell-lit effects draw nothing (the reference)
  on.png      the run under test     (a red control's run when the gate is red)
  legacy.png  WW_CELL_FX_RED=legacy  the viewer's effect shader
  nosoft.png  WW_CELL_FX_RED=nosoft  the soft and near fades held at 1

An effect's reach R = the pixels where legacy or nosoft part from hide by more than 6/255 (either one draws there).
  N  nothing else moves: where nosoft equals hide exactly, 3 px clear of any pixel it does not (no effect draws
     there even unfaded; the margin, lane FXLIT1, keeps the rims of the dimmer lit effects out), on.png parts
     from hide.png by <= 3/255 on >= 99.9% of them; skipped when fewer than 1000 such pixels are in the frame
  S  the Soft fades only take away: over R, |on - hide| <= |nosoft - hide| + 3 on >= 99% of the pixels,
     and it is smaller by more than 6/255 on >= 1000 pixels
  H  the haze drops: over R, the mean |on - hide| <= HMAX x the mean |legacy - hide| (argv 2, set per camera
     by cell_fx.sh; the legacy red is 1 by construction)
Prints one line per stage, then "effects PASS" or "effects FAIL".
"""
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int32)


def main():
    run = sys.argv[1]
    hmax = float(sys.argv[2])
    hide, on, legacy, nosoft = (load(f"{run}/{t}.png") for t in ("hide", "on", "legacy", "nosoft"))
    d_on = np.abs(on - hide).max(axis=2)
    d_leg = np.abs(legacy - hide).max(axis=2)
    d_ns = np.abs(nosoft - hide).max(axis=2)
    reach = ( d_leg > 6 ) | ( d_ns > 6 )
    n_reach = int(reach.sum())
    print(f"reach {n_reach} px ({100.0 * reach.mean():.2f}%)")
    ok = True
    if n_reach < 2000:
        print("R FAIL  the effects reach under 2000 pixels: nothing to judge")
        ok = False

    # lane FXLIT1: the bare pixels stand 3 px clear of any pixel nosoft touches. Lit effects are dimmer, so a
    # card's faint rim rounds to nothing in nosoft while on (faded against the depth, not clipped by it) still
    # darkens it (INFERRED cause): measured 2026-10-03 at camera 1, all 94 failing pixels within 3 px of an effect. Without the
    # margin the exe before the lane left 102 bare pixels there (N skipped); with it the lane leaves 99
    touched = d_ns > 0
    near = touched.copy()
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            near |= np.roll(touched, (dy, dx), (0, 1))
    bare = ~near
    n_bare = int(bare.sum())
    if n_bare < 1000:
        print(f"N skip  only {n_bare} pixels with no effect at all")
    else:
        still = float(( d_on[bare] <= 3 ).mean())
        n_ok = still >= 0.999
        print(f"N {'PASS' if n_ok else 'FAIL'}  {100.0 * still:.3f}% of {n_bare} effect-free pixels equal hide (>= 99.9%)")
        ok &= n_ok

    if n_reach:
        within = float(( d_on[reach] <= d_ns[reach] + 3 ).mean())
        faded = int(( d_on[reach] < d_ns[reach] - 6 ).sum())
        s_ok = within >= 0.99 and faded >= 1000
        print(f"S {'PASS' if s_ok else 'FAIL'}  {100.0 * within:.2f}% no stronger than nosoft (>= 99%), "
              f"{faded} px faded (>= 1000)")
        ok &= s_ok

        m_on = float(d_on[reach].mean())
        m_leg = float(d_leg[reach].mean())
        ratio = m_on / m_leg if m_leg > 0 else 1.0
        h_ok = ratio <= hmax
        print(f"H {'PASS' if h_ok else 'FAIL'}  mean change over the reach {m_on:.2f} vs legacy {m_leg:.2f}, "
              f"ratio {ratio:.3f} (<= {hmax})")
        ok &= h_ok

    print("effects PASS" if ok else "effects FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
