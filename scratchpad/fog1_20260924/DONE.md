DONE -- lane FOG1, 2026-09-24 19:16
release/NifSkope.exe 18:45:55 sha1 629a3911d8a1c33c1879d9762fcfc0ced86c2ebf (rung release/before_fog1 45108d83)

Gates (tests/spells/pbr_fog1_gates.sh): 64 checks, 0 failures on this exe. The full run on 629a3911 was
63/64; the one failure was the legacy far framing (model 6 px at 20000 u), which was reframed to FOV 8.
That section alone then went 9/9 (5 legacy checks, 260 px) -> leg/, final2/.
- CLI 117/117 probed fragments; alpha 6/6 +-1; colour 4/4 +-2 (mid-key, 07:00 between keys, 19:30 dusk);
  height 2/2; distance scale; sky identical on/off; OFF = before_fog1 byte for byte (6 shots + legacy);
  legacy path: fo4_fog.prog serves with Fog on, alpha = judge, off = fo4_default.prog and byte-identical;
  live leg 14/0.
- Reds: 14/14 FAIL (reds/summary.txt + reds/fognoswap2, reds/fogleak on the new exe).
- Regression: zero set 10/0 (zero2/); R1 48/0, R2a, R2b (live 14/0), R3, R4 PASS, WX1 71/0 (regress2.log).
- Fix during the lane: the fog code sitting switched off in fo4_default.frag moved the zero set (783 px);
  it is now compiled only into fo4_fog.prog (commit 8bfb374).
Commits (local, not pushed): 5928bb5 031c521 4a4d24b 96bfe94 52f6fad 8bfb374 9c754f3 85f052a
Pictures: fog1_pics_sheet.png (view 8, dawn/noon/night on/off), fog1_far_sheet.png (20000 u), fog1_pics_diff_x16.png
Not flown. Any open NifSkope window of bungo's predates this build -- restart it.
