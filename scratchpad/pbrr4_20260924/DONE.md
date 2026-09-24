DONE b6d37f73365514ca75cae8af8d71faf1158d563f

Lane PBRR4, 2026-09-24 13:21. release/NifSkope.exe built 12:43:47 (make RC 0), shaders in step. Not committed.

pbr_r4 tint -> PASS (Normalize, Add, Priority RGBA; every region |d| <= 0.5/255)
pbr_r4 emission -> PASS (100 nits -> linear 0.9989 pre-exposure; replace, override, mask)
pbr_r4 comp -> PASS (Opaque, Alpha Test lo/hi, Blend, Premultiplied, Additive, Multiply)
pbr_r4 s1b -> PASS (weight 0.5/1 head-on 0.4996; grazing >= 80 deg 0.9822)
pbr_r4 d1 -> PASS (normal incidence 1.0000 / 1.0000; 75 deg ring 95.5 vs 95.4)
R4 reds 7/7 fail their target: nodiv notintmask emitraw emitmul nocomp f90scaled lambert
R3: furnace twins s1 s2 s3 q9 PASS; reds 6/6 (noms nosplit f0law fo4csweight notint q9legacy)
R1: 48 checks, 0 failures -> PASS; red f0law fails (c)
R2A GATES: PASS; R2B GATES: PASS
zero set vs before_pbrr4: 10 cases, 0 failures -> PASS; red shader BITES 7/7

Evidence: scratchpad/pbrr4_20260924/ (r4_a, red_*, r3*, r1*, r2a, r2b, zero*, *.log, progress.md)
