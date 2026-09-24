# MISTAKES.md — entries owed by lane UINOTES2 (2026-09-12)

Splice these into the repository root `MISTAKES.md`, newest first, in the house
format. Nothing here is a build break; both are claims that outran their proof.

## 2026-09-12 — I wrote "Loop lights too" into the report before an exe existed to measure it

**What I did.** Step 1 gave the two gizmo toggles a lit (accent) icon, and I
extended the same treatment to the Loop button "so the Animation menu's own Loop
entry lights with the same ink". I wrote that into the report, into the doc
comment on `wwTransportToggleIcon()` and into the code, all before the build
that would have shown it.

**What was true.** The dock's loop button takes its icon from the SHARED
`aAnimLoop` action, and the render toolbar re-skins that action on every refresh
of its popup (`src/nifskope_ui.cpp`, `ui->aAnimLoop->setIcon( tlMakeIcon( "loop", ... ) )`)
with a single-state icon. Nothing the dock puts on that action survives. The
gate's own 2:1 picture, `transport_2x_on.png` 06:35, measures the checked loop
button's ink at `#4d5761` — plain ink over the checked blue — while pose and
auto-key measure `#61584b` and `#665847`, the accent.

**How it was caught.** By opening the picture the gate had just written and
measuring the ink inside each checked button's blue box, instead of reading the
gate's PASS and moving on. The gate passed: it only ever asserted the two
toggles bungo named. A green harness is not a build, and it is not a claim
either.

**The rule.** A sentence about what the user will SEE is owed a measurement of
the built exe, not of the source. If the picture does not exist yet, the
sentence does not get written yet.

**What was done about it.** `fix4_loop_claim.py` took the lit icon back off the
loop action, replaced the doc comment's claim with the measurement and the
`nifskope_ui.cpp` line where the decision actually lives, and
`fix5_loop_gate.py` made the harness PRINT loop's two inks beside the two it
gates — measured, deliberately not asserted, because "Loop must not light" is
the wrong thing to own the day someone decides it should.

## 2026-09-12 — the second place that ate the selection was found by reading, not by the gate

**What I did.** Step 2 fixed the Ctrl+A red at the place the previous lane had
measured: `setSequenceByName`'s `setCurrentItem`, which carries Qt's implicit
`ClearAndSelect`. That fix alone passes the check the gate had at the time,
because the gate asked the question once, straight after the key.

**What was true.** `rebuildList()` clears the list and restores only the CURRENT
row, so the 50 ms debounced refresh would have thrown the selection away a
moment later — and had been quietly reducing EVERY multi-selection to one row on
every refresh since it was written, so Delete / Copy / Cut acted on one clip
however many the user had picked. Nobody had named that.

**How it was caught.** By reading every path that writes the list's selection
before declaring the first fix sufficient, and then by making the gate ask the
same question at three moments — straight after the key, after one pump, and
after the list rebuilds — so the one-moment version could not pass a half fix.

**The rule.** When a fix is chosen for "smallest blast radius", the reading that
justifies the choice must cover every other writer of the same state. And a
check that asks once, straight after the input, cannot see anything a debounce
undoes; it asks again after the debounce has run.

## 2026-09-12 — I tested an exe the linker was still writing

**What I did.** The second build ran in the background. I watched the exe's
timestamp move, ran `make -q`, got 0, saw `cmp res/style.qss release/style.qss`
pass, and started the harness. It failed with "the harness wrote no log", twice,
on two different ports. Running the exe by hand said

```
./release/NifSkope.exe: cannot execute binary file: Exec format error
```

and its first two bytes were `00 00`, not `MZ`. Nothing was wrong with the build:
the linker had not finished. Three minutes later the same file began `MZ`, the
background task reported `BUILD-RC=0`, and the harness ran 224 / 0.

**The rule I broke.** `nifskope-ww-build-verify` gates on MAKE'S OWN EXIT CODE.
A timestamp is not an exit code, and `make -q` answers "is anything out of date",
which a half-written output satisfies perfectly. I had done this correctly for
the first build and skipped it for the second because the second felt small.

**What it cost.** Two harness runs and about four minutes — and it could have
cost much more: an exe that is 95% written can start, run, and give numbers that
are neither the old build's nor the new one's.

**What to do instead.** Wait for the build's own exit code, every time, however
small the change. If a run must start before that, the first thing it checks is
that the exe begins `MZ` and that `BUILD-RC=0` is in the log.

## 2026-09-12 — my first explanation of the lodl crash was a story, and it was wrong

**What I did.** `lodl_open` crashed 3 of 3 on the rung and passed 3 of 3 on my
exe, and within a minute I had an explanation: my Ctrl+A guard had broken a
selection re-entrancy loop, so the runaway recursion was gone. It was a good
story. Every piece of it was assumed.

**What was true.** The stack is 19 frames with no repetition — not recursion.
`syncing` is true across the whole of `rebuildList` and `setSequenceByName`, so
neither of my two changes alters what reaches `listRowChosen` at all. And the
build log says my exe is not "the rung plus my UI changes": `src/lodgen.h`
changed at 06:07, so the build recompiled `lodgen.o`, `lodgenchunkpass.o`,
`lodgenmanager.o`, `nativeemit.o` and `nifcli.o` as well.

**How it was caught.** By asking what else the build had compiled before
claiming a cause, and then by two cheap experiments that took four minutes:
the rung headless-renders a plain NIF, and the rung headless-renders a 2.65 MB
terrain NIF built from the same `.lodl` — both rc 0, both byte-identical to my
exe's picture. Only the `.lodl` document in the viewer dies.

**The rule.** Before a green row is credited to a change, read the build log for
what else was rebuilt. A fix nobody can name is a coincidence until an experiment
separates it, and "my change and the crash are in the same binary" is not an
experiment.


## 2026-09-12 — a new gate turned an old one into a SKIP, and the failure count did not move

**What happened.** Gate (k8b), added for ruling 08:2x, grabs the dope sheet
eleven times and pumps the event loop between grabs. On the next run gate (n)
stopped measuring its right-click menu and said *"the COM row is scrolled out of
the sheet"* instead. The run still reported **0 failures**. The only number that
moved was the skip count, 2 to 3, and a skip is never a pass.

**How it was caught.** By diffing the whole check list against the previous run,
not by reading the totals: `224 / 0 / 2` to `236 / 1 / 3` looks like "one new
failure" until the two lists are put side by side and a green line has turned
into a SKIP.

**The fix.** Not to make (k8b) tidier -- a gate must not be fragile enough to
care. Gate (n) now scrolls the COM row into view exactly as gate (k) already does
for the thigh row, says in the log that it had to, and then measures. The state a
gate needs is a state it forces.

**The rule.** After adding a check, compare the SKIP LIST with the previous run's
as carefully as the failure count. A silent conversion from measured to skipped
is a regression that both numbers you usually read will hide.
