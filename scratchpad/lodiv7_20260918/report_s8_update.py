p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

ANCH = "**The fix is already in the script**, so the resumed lane does not repeat it:"
assert s.count(ANCH) == 1

NEW = r"""### 8a. The 18:0x run, and whether it counts as G4/G5. It does NOT.

The director's 18:0x message: PID 15088 was gone, but a run of `tests/spells/lodi_v7.sh` was LIVE, started
**18:04:59** by an orphaned bash (PID 40508) that neither the lane nor the director owns on record, having
launched **NifSkope PID 24828** on `--port 42947` through the fixed `shot()` path. Instruction: touch
neither, poll every 30 s for 15 minutes, then read what it left and say whether it counts.

Polled from **18:06:20 to 18:21:53**, 31 checks, `Fallout4` down throughout. **The cap expired with the exe
still up.** Nothing was killed.

What that run left, read at 18:22:43:

| | |
|---|---|
| `scratchpad/lodi_v7_gate/v7_placement.log` | 268 bytes, written **18:05:04**, four `QObject::connect` warnings and nothing else |
| everything else in that directory | `decode.log`, `refuters.log`, `v7_identity.log` -- all still **10:39**, from the earlier hung run. This run never re-ran G1, G2 or G3 |
| pictures | **none.** No `.png` anywhere under `scratchpad/lodi_v7_gate/` or `scratchpad/lodiv7_20260918/` newer than 18:00; `images/` is still empty, mtime 09:45:35 |
| NifSkope PID 24828 | still resident at 18:22:43, **17 minutes** after launch, 578,724 K |
| bash PID 40508 | still resident |

**Verdict: it does not count as G4 or G5, and the lane will re-run both.** One log of Qt warnings, no
picture, no refreshed G1-G3 output, and the process that was meant to produce them still running. There is
nothing in it to grade.

**And it produced a finding that corrects something this lane wrote earlier today.** The fixed `shot()`
carries `timeout 600`; the process outlived it by seven minutes and **no `timeout.exe` was left running**.
GNU `timeout` signals its child, and a native Windows GUI process started from an MSYS/Git-Bash shell does
not act on that signal -- `timeout` gives up, exits, and leaves the exe running and **orphaned from its own
guard**, which is worse than no guard because the driver believes it is protected. So the rule this lane put
in `MISTAKES.md` and in `.claude/skills/nifskope-ww-render-shot/SKILL.md` at 17:5x was incomplete, and both
have been corrected in place: wrap it in `timeout` **and** record the PID at launch **and** verify after the
wait that it is gone -- and if it is not, print the PID and the port and stop, because every later gate
refuses anyway. That correction is the one thing of value this run produced.

**The lane stops here, as the director instructed** (*"if the exe is still up after 15 minutes, write
PENDING.md and stop -- I will deal with it"*). `PENDING.md` names PID 24828 and bash PID 40508.

**The fix is already in the script**, so the resumed lane does not repeat it:"""

s = s.replace(ANCH, NEW)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('report 8a added')
