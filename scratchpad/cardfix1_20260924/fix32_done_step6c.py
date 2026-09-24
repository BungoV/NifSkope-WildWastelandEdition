# CARDFIX1 step 6c: DONE.md + DELIVERABLE_TEXT.md record the director's G4 decision (a) and run4. LF-only files.
R = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/'


def patch(name, pairs):
    p = R + name
    b = open(p, 'rb').read(); assert b.count(b'\r') == 0
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (name, o[:60], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8'); assert out.count(b'\r') == 0
    open(p, 'wb').write(out); print('patched', name)


patch('DONE.md', [
    ("PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps. Steps 1-5 landed; step 6 (sway A) built, committed and\n"
     "gated, RED on G4 only (the BC7 error bar, pre-registered from the synthetic; decision owed, section 3). NEXT:\n"
     "step 7, IMPOSTORPBRM1 (brief_impostorpbrm1.md), not started: the brief forbids a step on a red one.\n"
     "Step 6b (the director's relay of bungo's 2026-09-25 ruling, the N8 grid as the bake default) landed after 6.\n",
     "PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps. Steps 1-6 landed. Step 6 (sway A) went red on G4 only;\n"
     "the director decided (a) on 2026-09-25 (step 6c: G4's bar re-pinned to the sheet's measured codec floor),\n"
     "and the wind gate is now 28 / 0. Step 6b (the director's relay of bungo's 2026-09-25 ruling, the N8 grid as\n"
     "the bake default) landed after 6. NEXT / IN PROGRESS: step 7, IMPOSTORPBRM1 (brief_impostorpbrm1.md).\n"),
    ("# 3. Gates (numbers; red runs)\n",
     "## Step 6c -- G4 re-pinned: DIRECTOR DECISION (a), 2026-09-25 (not a ruling by bungo)\n"
     "- The director's decision, with its reasoning: re-pin G4's bar to the codec floor measured on the SAME\n"
     "  sheet (its normal R/G channels, 3.266 / 12 on the elm), plus the margin of the skill\n"
     "  ww-preregister-bar-from-the-subject, and state it in the gate as \"bar = codec floor of the sheet's other\n"
     "  channels, measured\". The BC7 weights are NOT changed: option (b) would move every card's normals for a\n"
     "  sway error of about 1.4 % of full scale, which nobody can see. A red must still fail.\n"
     "- tests/spells/impostor_wind.py/.sh (fix31_g4_repin.py): the bar is computed in the run from that run's\n"
     "  normal R/G error, x 1.25 on the mean and x 1.25 rounded up on the p95. If the floor was not measured\n"
     "  (empty, or a mean under 0.5), G4 fails by name; it never falls back to a constant. Two red controls must\n"
     "  fail the same bar: the next frame's picture, and the sway channel corrupted to 4 bits\n"
     "  (floor(A/16)*16+8, a codec about twice as coarse as BC7's own error here).\n"
     "- Which check the first bar skipped (skill check 3): 3.0 / 12 was copied from the synthetic input\n"
     "  (1.34 / 4) onto the real one. Red run kept: gates/impostor_wind.run3.out.\n"
     "\n"
     "# 3. Gates (numbers; red runs)\n"),
    ("# 4. Exe sha1 + commits\n",
     "## Step 6c (exe 309f3aa9, unchanged; gates/impostor_wind.run4.out = 28 checks, 0 failures, PASS)\n"
     "- G4 elm: sway error mean 3.573 p95 13 (max 88), 93533 covered texels. Floor (normal R/G) 3.266 / 12 ->\n"
     "  bar 4.082 / 15: ok. RED 1, the next frame: 51.099 > 4.082. RED 2, the 4-bit sway: mean 6.702 fails.\n"
     "  The synthetic Hero set for comparison: 1.281 / 4 against its own R/G floor 4.992 / 16.\n"
     "- G1-G3 unchanged from run 3 (same exe, same bake).\n"
     "\n"
     "# 4. Exe sha1 + commits\n"),
    ("step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 24e7835 (code, gate, DONE); step 6b = the commit carrying\n"
     "  this text.\n",
     "step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 24e7835 (code, gate, DONE); step 6b 6430dff; step 6c = the\n"
     "  commit carrying this text (G4 re-pin + run4).\n"),
])

patch('DELIVERABLE_TEXT.md', [
    ("**Lane CARDFIX1 (LOD-D), 2026-09-24/25: PARTIAL. Steps 1-5 landed; step 6 built and gated, RED on G4 only; step 7 not started.**\n",
     "**Lane CARDFIX1 (LOD-D), 2026-09-24/25: PARTIAL. Steps 1-6 landed (G4 decided by the director, (a)); step 7 in progress.**\n"),
    ("step 2 91ddd41, step 3 d8302c9, step 4 7896ad1, step 5 1303334 + 6c5f5f8, step 6 24e7835, step 6b = the branch\n"
     "head (the N8 default).",
     "step 2 91ddd41, step 3 d8302c9, step 4 7896ad1, step 5 1303334 + 6c5f5f8, step 6 24e7835, step 6b 6430dff\n"
     "(the N8 default), step 6c = the G4 re-pin commit."),
    ("  **Decision owed (director/bungo):** (a) accept at the measured level, or (b) raise the BC7 alpha weight for\n"
     "  model-sway sets (`kCardNormalBc7Weights {1,1,32,1}`, src/lodgen.cpp ~4764), which costs normal/height\n"
     "  precision and must be measured first.\n",
     "  **Director decision (a), 2026-09-25 (not bungo's ruling):** G4's bar is now the same sheet's measured codec\n"
     "  floor x 1.25 (4.082 / 15); the BC7 weights stay `{1,1,32,1}`, because (b) would move every card's normals\n"
     "  for a sway error of about 1.4 %. Red controls: the next frame (51.1) and the sway cut to 4 bits (6.702)\n"
     "  both fail. Wind gate 28 / 0 (gates/impostor_wind.run4.out).\n"),
    ("* **Step 7 (IMPOSTORPBRM1)** not started: the brief forbids starting a step on a red one, and it is a lane's\n"
     "  worth of work",
     "* **Step 7 (IMPOSTORPBRM1)** in progress after step 6c; it is a lane's\n"
     "  worth of work"),
])
