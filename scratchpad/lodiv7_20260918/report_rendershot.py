p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

# ---- the neighbour table row ------------------------------------------------
A = """| `render_shot.sh` | 82/0 | **2 failures**, cause NOT established | both are luminance-range checks -- "the pixel sampler CAN see a window's pixels", 0.250 against a bar of 15, and the black/white matte, 5.000 against 30. It ran at 18:53 on the **pre-rebuild** exe, whose terrain channel path was the one s8b describes. **I have not re-run it and I am not going to guess** |"""
N = """| `render_shot.sh` | 82/0 | 2 failures at 18:53, then **82 checks, 0 failures** re-run at 19:19:24..19:21:45 | **the stale binary again.** Both failures were luminance-range checks -- "the pixel sampler CAN see a window's pixels", 0.250 against a bar of 15, and the black/white matte, 5.000 against 30 -- and both are gone on the rebuilt exe at the harness's exact standing count. **Game up during run** (`Fallout4` PID 4464), which is worth stating: a PASS at the standing count is hard to get by accident, so this one counts, where a FAIL under the same conditions would not have been attributable |"""
assert s.count(A) == 1, 'render_shot row'
s = s.replace(A, N)

# ---- the "three PASS, three FAILED" summary cell ----------------------------
B = "| **G5** the neighbours | six owner harnesses, standing counts theirs | **three PASS, three FAILED and the failures are the interesting part** -- next table |"
N2 = "| **G5** the neighbours | six owner harnesses, standing counts theirs | **three PASS first time, three FAILED -- and the failures are the interesting part.** Five now stand at or above their standing counts; one, the bake, is owed -- next table |"
assert s.count(B) == 1, 'G5 row'
s = s.replace(B, N2)

# ---- what is owed -----------------------------------------------------------
C = """So `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe, with `lodgen_native.sh` and
`render_shot.sh` inside it, is the one thing this lane hands over unfinished. **Game up during run.**"""
N3 = """`render_shot.sh` has since been re-run on the rebuilt exe directly and returns its standing **82/0**, so
five of the six neighbours now stand where their owners left them. What is left is
`NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe -- which re-runs G1, G2 and G3 and carries
`lodgen_native.sh` inside it. **That one is a BAKE**, it reads the archives the running game holds, and it
is the reason the guard exists; it waits for the game to be down and costs one command."""
assert s.count(C) == 1, 'owed'
s = s.replace(C, N3)

# ---- the handoff block ------------------------------------------------------
D = """> counts, and **the seven pictures** in `scratchpad/lodiv7_20260918/images/` with `captions.md`."""
N4 = """> counts, **`render_shot.sh` 82/0 re-run on the shipped exe**, and **the seven pictures** in
> `scratchpad/lodiv7_20260918/images/` with `captions.md`."""
assert s.count(D) == 1, 'handoff 1'
s = s.replace(D, N4)

E = """> not yet re-run (`lodgen_native.sh`, repaired but unrun; `render_shot.sh`, 2 luminance failures whose cause
> is not established and which last ran on the stale binary). The gate refuses to start while `Fallout4` is
> up, by its own rule."""
N5 = """> not yet re-run: `lodgen_native.sh`, whose two failures are repaired but whose bake has not been taken
> again. It reads the archives the running game holds, so the gate refuses to start while `Fallout4` is up,
> by its own rule."""
assert s.count(E) == 1, 'handoff 2'
s = s.replace(E, N5)

open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('render_shot result folded in; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
