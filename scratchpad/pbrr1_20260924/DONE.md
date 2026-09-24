DONE -- lane PBRR1 (PBR renderer stage R1: detect + load), 2026-09-24 03:30.

release/NifSkope.exe sha1 ae1013257a2fd8df051d3feec5fa9b91e7926756 (24268800 B, linked 03:04:22)
rung release/before_pbrr1/ = 8485154d (PBRR0), unchanged.

Gates (evidence under scratchpad/pbrr1_20260924/):
- R1 gates (gates/verdicts.txt): 48 checks, 0 failures -> PASS
  a=9/9 b=9/9 c=2/2 d=1/1 e=1/1 resolver=9/9 routeview=9/9 served=7/7 texfail=1/1
- reds (red_*/verdicts.txt): coverage, order, order_e, f0law, nifx -- all five BITE
- (f) pbr_shade_ab --old release/before_pbrr1 (ab/): 10 cases, 0 failures -> PASS
  (3 particle NIFs PASS-EMPTY as before; ShockHAndLeft + CryoJet01 draw and pass)
  red shader: 7 of 7 aimed cases FAIL -> BITES; judge red on the cross-stage census
  projection (ab_red_proj/): a route change FAILS, a reworded refusal passes.
- lodgen: tests/spells/lodgen_terrain_pbrm.sh output identical on the PBRR0 rung and on
  this exe (both 14 checks, 5 failures: a failure that predates this lane, not from R1).

Not committed. tests/fixtures/pbr_data holds copies of vanilla/FO76 files and is NOT
git-ignored: never add it (regenerate with tests/spells/pbr_r1_fixtures.py).
bungo's open NifSkope window, if any, predates this exe: it needs a restart to have R1.
