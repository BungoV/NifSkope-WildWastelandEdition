## 2026-09-16 13:0x -- lane VT1 (the pyramid sheet vs the direct bake)

1. **A comparison of two files that were not there, printed as "0 bytes
   differ".**
   To check that the fix generalises above dim 4, the lane baked the same
   region at `--dim 8` with both exes and compared the assembled dim-8 sheet
   with the direct one. It printed `rung assembled-vs-direct 0 bytes` and
   `new assembled-vs-direct 0 bytes`, which reads exactly like a clean pass.
   Both direct dim-8 bakes had written **no sheets at all** -- the line above
   the comparison said `sheets=0` and was not read -- so `cmp` was handed a
   missing file on one side and `wc -l` counted nothing. A check that cannot
   fail on its input is not a check (CONSTITUTION, measure do not eyeball), and
   this one could not even be handed an input.
   **How it was found:** the same script printed `sheets=0` next to `rc=0`, and
   the next bake in the lane printed `sheets=12`. The difference was the cause
   below.
   **The rule:** a comparison states how many files it compared, and refuses
   when that number is zero. Never `cmp` without asserting both sides exist.

2. **`--tex-dir` with a RELATIVE path writes no sheets and still exits 0.**
   `--out-dir scratchpad/.../obj` works relative and the log even echoes the
   relative path back, so the lane assumed `--tex-dir scratchpad/.../tex` did
   too. It does not: the bake runs, reports `0 empty, 0 failed`, prints its
   stage times and its census, and leaves the directory empty. Three control
   bakes were lost to it before the pattern was seen. This is a generator
   defect and is named under Owed in the lane report; the lane did not fix it
   because it is outside the brief.
   **How it was found:** the same command with an absolute `--tex-dir` wrote
   12 sheets; `find` proved the missing sheets had not been written anywhere
   else either.
   **The rule:** pass absolute paths to `lodgen`, and read the file COUNT a
   bake produced before comparing anything.

3. **Modelling the fix, two errors caught before they reached a build.**
   `ringfix_model.py` first computed the parent box with `hiX = (px0 + 2 * d -
   ry0 * 0 - rx0) * 32`, a half-finished edit that happened to give the right
   answer on the probe set; and it called `main()` at import, so importing it
   from `texelpredict.py` would have re-run the whole model silently. Both were
   found by reading the file back after writing it, before either was used.
   **The rule:** an offline model that a source change will be built on gets
   read back once, whole, before its numbers are quoted.

