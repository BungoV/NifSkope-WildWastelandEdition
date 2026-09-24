p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

# ---- 0b: the exe as it ships, read at 19:1x --------------------------------
A = "## 1. The v7 layout: what it adds, where it had to go, and every refusal by name"
assert s.count(A) == 1

S0B = """### 0b. The exe as it ships, read at 19:14 (`date` in the same command)

| what | value |
|---|---|
| `release/NifSkope.exe` mtime | 2026-09-18 **19:05:11**.012337400 +0200 |
| size | **22,764,544** B |
| sha1 | `28ac412c6da96e0707e492c175822d2076016dc5` |
| sources newer than it | **none.** `find src res lib tests -newer release/NifSkope.exe -name '*.cpp' -o -name '*.h' -o -name '*.frag'` is empty |
| objects against the headers they include | `btdterrain.o` 19:05, `lodinative.o` 10:16, `nifskope.o` 10:16, `lodifile.o` 10:09, `nativeemit.o` 10:38, against `lodinative.h` 10:16, `lodifile.h` 10:01, `nativeemit.h` 10:08 -- **every object now newer than every header it includes.** That second row is the check that did not exist this morning; s8b is why it does now |
| the exe before the rebuild | kept as `release/NifSkope.before_btdterrain_rebuild.exe`, sha1 `d7261c9a7b3f9491c9ec47e50e87d4fe25d55e4e`, 22,764,544 B. Nothing named `NifSkope.before_*.exe`, `NifSkope.archlock1_rung.exe`, `NifSkope.at_0117.exe` or `NifSkope_inuse_*.exe` was deleted by this lane |

**Two builds, both with the game/NifSkope check run as its own command first**: 10:38:44 (the lane's C++)
and 19:05:11 (the stale object, s8b). No NifSkope was up at either link, so no `NifSkope_inuse_<pid>.exe`
rename was needed and nothing was killed at any point in this lane.

"""
s = s[:s.index(A)] + S0B + s[s.index(A):]

# ---- the guard I tried to amend and was refused ----------------------------
B = """So `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe, with `lodgen_native.sh` and
`render_shot.sh` inside it, is the one thing this lane hands over unfinished. **Game up during run.**"""
assert s.count(B) == 1
N = """So `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe, with `lodgen_native.sh` and
`render_shot.sh` inside it, is the one thing this lane hands over unfinished. **Game up during run.**

**And I did not work around the guard.** The director's word for this resume was that a running `Fallout4`
is not a stop condition, so I tried to give my own gate an `ALLOW_GAME=1` opt-in that would run anyway and
print `GAME UP DURING RUN` on its first line so any number under it could be attributed. **The session's
permission classifier refused that edit** (`[Security Weaken]`), and I left it refused rather than reaching
for another route to the same effect. The guard stands as written: `SKIP: Fallout4 is up -- no exe runs
while the game holds the files`. The run waits for the game to be down, which costs one command."""
s = s.replace(B, N)

# ---- s10: the skill that gained a rule -------------------------------------
C = "## 11. Rows for bungo"
S10 = """**A third skill gained a rule, and it is the one that would have caught s8b.**
`.claude/skills/nifskope-ww-build-verify/SKILL.md` -- the chain whose `test exe -nt <source>` gate this lane
proved insufficient -- now carries the object-against-header check, the ten-second loop that runs it, the
reason an enumerator added at the END renumbers nothing, and the standing instruction to run the neighbour
harnesses before believing a gate that only exercises the files you edited.

"""
s = s[:s.index(C)] + S10 + s[s.index(C):]

# ---- s14 sentence 5 must end on his window ---------------------------------
D = """5. The pictures are in `scratchpad/lodiv7_20260918/images/` -- one of whole houses in single colours, one
   of the old per-placement view beside it, the new sky against the old flat one, and the 205-piece house on
   its own -- and one honest thing left over: the exe I first tested was missing a rebuild of one file, which
   made the terrain colour views read one channel late, so I rebuilt it, re-proved the viewer, and the one
   run still owed is the whole gate on the rebuilt exe, which the harness refused to start while your game
   was up."""
assert s.count(D) == 1
N14 = """5. The pictures are in `scratchpad/lodiv7_20260918/images/` -- whole houses in single colours, the old
   per-placement view beside it, the new sky against the old flat one, and the 205-piece house on its own --
   and one honest thing left over: the exe I first tested was missing a rebuild of one file, which made the
   terrain colour views read one channel late, so I rebuilt it and re-proved the viewer, and the one run
   still owed is the whole gate on that rebuilt exe, which its own guard refused to start while your game
   was up; **his open window needs a restart.**"""
s = s.replace(D, N14)

open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('report closed; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
