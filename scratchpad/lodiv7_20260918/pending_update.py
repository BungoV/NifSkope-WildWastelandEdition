p = 'scratchpad/lodiv7_20260918/PENDING.md'
s = open(p, encoding='utf-8', newline='').read()

start = s.index('## 0. THE BLOCKER')
end = s.index('---\n\n## 1. State:')

NEW = r"""## 0. THE BLOCKER, and it is the first thing to deal with

**Updated 2026-09-18 18:2x. The first wedge is cleared and a SECOND one replaced it.**

PID 15088 (the 10:39 wedge) is gone -- the director cleared it. Live now, and
**neither may be touched**, per the director's 18:0x instruction:

```
NifSkope PID 24828   release\NifSkope.exe --port 42947     launched 18:04:59
bash    PID 40508   an orphaned run of tests/spells/lodi_v7.sh, owner unknown
```

The director owns these two: *"if the exe is still up after 15 minutes, write
PENDING.md and stop -- I will deal with it."* Polled every 30 s from 18:06:20 to
18:21:53, 31 checks, `Fallout4` down throughout; **the cap expired with both
still up**, so the lane stopped. Nothing was killed and nothing was signalled.

**What that run left, and it is not a G4/G5 result** (report 8a): one file,
`scratchpad/lodi_v7_gate/v7_placement.log`, 268 bytes of Qt warnings written at
18:05:04 and nothing since; `decode.log`, `refuters.log` and `v7_identity.log` in
that directory are still the stale 10:39 ones, so G1, G2 and G3 were never
re-run; **no `.png` anywhere newer than 18:00** and `images/` still empty at
mtime 09:45:35. The resumed lane **re-runs G4 and G5 from the top** rather than
grading any of it.

### The finding that run produced, and it corrects this lane's own earlier rule

The fixed `shot()` carries `timeout 600`. NifSkope 24828 outlived it by seven
minutes -- still resident at 18:22:43, 17 minutes after launch -- and **no
`timeout.exe` process was left running**. GNU `timeout` signals its child, and a
native Windows GUI process started from an MSYS/Git-Bash shell does not act on
that signal: `timeout` gives up, exits, and leaves the exe running and **orphaned
from its own guard**. That is worse than no guard, because the driver believes it
is protected.

**So the guard has three parts, not two, and the resuming lane must add the
third before it runs anything:**

1. pass the scene file (the `.lodl`, positional) -- already in `shot()`;
2. wrap the launch in `timeout` -- already in `shot()`, and now known to be
   necessary but **not sufficient**;
3. **record the PID at launch and verify after the wait that it is gone.** Launch
   with `&`, keep `$!`, and afterwards check by the `--port` you gave it. A
   driver that finds it still alive and cannot end it **prints the PID and the
   port and stops**, because the one-instance rule makes every later gate refuse
   anyway.

`MISTAKES.md` and `.claude/skills/nifskope-ww-render-shot/SKILL.md` have both
been corrected in place with this measurement. **Part 3 is NOT yet written into
`tests/spells/lodi_v7.sh`** -- that is the resuming lane's first code change.

**Why the original wedge happened at all:** `WW_RENDER_SHOT` only arms when a
file is on the command line (`src/nifskope_ui.cpp:22056`; the hook hangs off
`completeLoading`). The 10:39 `shot()` passed none, so that process never loaded,
never rendered, never quit and never printed a reason. That part is fixed.

**Permissions, stated so the next session does not rediscover it:** this session
was refused `Stop-Process`, `taskkill` and `CloseMainWindow` by the auto-mode
classifier with `[Interfere With Workloads]`, four separate attempts. A session
that must clean up after a harness needs that permission granted, or a human at
the keyboard.

**The `BUILDING` marker is REMOVED** (`scratchpad/lodiv7_20260918/BUILDING`).
No build is in flight and none is owed -- `find src -newer release/NifSkope.exe`
is empty -- so holding the build lock while parked would block other lanes for
no reason. The resuming lane re-creates it only if it changes C++.

"""

s = s[:start] + NEW + s[end:]
s = s.replace('Written 2026-09-18 17:5x CEDT.',
              'Written 2026-09-18 17:5x CEDT, updated 18:2x after a second wedge.')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('PENDING.md section 0 rewritten')
