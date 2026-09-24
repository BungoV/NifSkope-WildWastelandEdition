## 2026-09-11 -- lane NATIVE1b: a gate's own path line ran BOTH halves of a `||`, and nine checks failed as if the writer were broken

**What was done.** `tests/spells/lodgen_native.sh` has carried this line since
lane NATIVE1a wrote it:

```
case "$W" in /*) WA="$(cd "$W" && pwd -W 2>/dev/null || cd "$W" && pwd)";; *) WA="$W";; esac
```

The first run of the suite on the v3 exe came back **16 checks, 15 failures**,
with `1 stock files compared, 1 differ`, `DIFFER *`, the decoder refusing the
real pair, `--native-verify` saying `cannot open`, and the field gate dying in
`open()`.

**What was true instead.** Nothing was wrong with the writer. The shell parses
`a && b || c && d` as `(((a && b) || c) && d)`, so when `pwd -W` SUCCEEDS the
`||` short-circuits past the second `cd` and then **`pwd` runs anyway**: `$WA`
comes back as two lines, `E:/...` and `/e/...`, separated by a newline. Every
path built from it carried that newline, so the bake wrote into one directory
and every checker looked in another. The give-away is in the traceback, which
prints the path with a literal `\n` in the middle of it.

**How it was found.** By reading the exception text rather than the verdict:
`OSError: [Errno 22] Invalid argument: 'E:/...gate/after\n/e/...Commonwealth.lodo'`.
The `\n` is the whole diagnosis, and it is two characters wide.

**Why NATIVE1a never saw it.** It ran the suite with no `OUT=`, into a
`mktemp -d` directory. This lane ran it with `OUT=` so the bake could be kept
and photographed -- the same code path, a different argument, and the defect
had been sitting in the gate for a day.

**Cost.** One suite run, about five minutes. No build, no relink: the spell is
a script and the fix does not touch the exe.

**The rule that prevents it.** Two:

1. **Group the fallback.** `x="$(cd "$d" && { pwd -W 2>/dev/null || pwd; })"` --
   braces, every time. A bare `a && b || c && d` in a path expression is a bug
   waiting for the day `b` starts succeeding.
2. **A gate run with a new argument is a NEW gate.** `OUT=` had never been
   exercised on this suite. Before believing a suite's baseline, run it the way
   the lane will actually run it, and treat the first such run as a gate on the
   gate -- `nifskope-ww-lodgen` already says a suite whose ok COUNT dropped is a
   broken block until proved otherwise; this extends it to a suite whose count
   dropped because its own arguments changed.

## 2026-09-11 -- lane NATIVE1b: a hand-derived fixture answer that no hand can derive

**What was done.** The `.lodo` fixture's known answers included
`lodo.clusterCount 3`, `lodo.vertexCount 8+18+6` and `lodo.triangles 32`. With
the v3 ladder switched on in the same fixture, all three describe the ladder's
output as well as level 0 -- and the ladder's output is a SIMPLIFIER'S, which
no hand derives.

**What was true instead.** Keeping the three keys would have meant either
turning the ladder off in the fixture (so the new code is never exercised by
the known-answer control at all) or copying whatever the writer produced into
the expectations, which is the exact circularity
`ww-standalone-writer-gate` exists to prevent.

**How it was caught.** Before the numbers were measured, while rewriting the
fixture: the question "what is the right answer here" had no answer that did
not start with "whatever the simplifier does".

**The repair, and it is the general form.** Split the claims: the LEVEL-0
counts stay hand-derived (`lodo.level0.clusterCount`, `.vertices`,
`.triangles`), the ladder is checked by INVARIANTS that hold whatever the
simplifier does (errors monotone up every chain, the roots' subtree counts
summing to the level-0 triangle count, every triangle inside its sphere, every
normal inside its cone or the cone flagged open), and one number is stated as a
PREDICTION made before the run (`lodo.levelMaxAtLeast 1`) rather than as a
derivation. A fixture may predict; it may never copy.

## 2026-09-11 -- lane NATIVE1b: a gate I wrote myself compared a FRAGMENT against a WHOLE and called the difference a hole

**What was done.** The far-shadow rule says a decimation may never open a
silhouette, so the lane's new field-gate check h9 read the mesh report's
`boundaryCoarsest` (the coarsest level's boundary-edge count) against
`boundaryEmitted` (level 0's) and failed any mesh where it rose. It failed on
fourteen meshes, two of which went from a WATERTIGHT level 0 to four and eight
boundary edges. I wrote a contract deviation saying those fourteen were reported
rather than refused.

**What was true instead.** Two separate things, and the deviation had them both
wrong.

* The watertight-to-holed pair WAS a real defect and should never have been
  written down as acceptable. A per-STEP refusal now catches it in the writer --
  a simplified group whose boundary count exceeds that of **the surface it
  replaces** is refused and its clusters stay roots. It fires **57 times** on
  the nine-chunk region.
* h9 itself was not measuring what it claimed. **A ladder is PARTIAL wherever a
  group refuses**, so the coarsest level is a FRAGMENT of the mesh, and a
  fragment has its own outline. After the per-step refusal landed, 18 meshes
  still showed a rise; every one of them has at least one refused group, and
  their coarsest level covers as little as **6.6 percent** of their own surface
  (`RockCliffGSCrater02_LOD_0.nif`, 45 of 685 full-detail triangles). The
  comparison was apples to oranges from the first line.

**How it was found.** By printing the TABLE rather than the count
(`ww-spec-gate-audit`): the eighteen rows with their refusal counters and their
coverage fractions beside them. The count alone had looked like a small residual
defect for two runs.

**The repair.** h9 became two checks that are true: the per-step rule is LIVE
(57 refusals, so it is not dead code), and every mesh whose coarsest count rose
is EXPLAINED by a refusal rather than left unexplained -- a mesh with a rise and
no refusal would still be red. `boundaryCoarsest` is now documented as a
reporting column, not a gate (contract Deviation 11).

**The rule that prevents it.** `ww-spec-gate-audit` applies to the gates a lane
writes for ITSELF, not only to numbers inherited from another lane. Before a
check compares two quantities, ask whether they are over the same DOMAIN -- and
when a check fails on real data, print the rows and their domains before
believing either the check or the data.

## 2026-09-11 -- lane NATIVE1b: a backslash through a Bash heredoc, exactly the trap the skill names

**What was done.** A patch script written inline through a `<<'PYEOF'` heredoc
carried a Python string `'...model\n";'` meant to match the C literal
`model\n";` in `src/nativeemit.cpp`. The anchor counted 0 and the script refused.

**What was true instead.** The file was fine. The heredoc halved the backslash,
so the pattern the script actually searched for was one character different from
the text in the file. `nifskope-ww-lodgen` names this trap in so many words --
*"no text carrying a backslash or an apostrophe goes through a heredoc at all"*
-- and the lane had already read it.

**How it was found.** By `cat -A` on the target line, which showed the file's
bytes were exactly what the anchor claimed to want.

**Cost.** Two minutes, no build. The same trap then cost a second round in the
lane's `anchors.py`, where apostrophes inside single-quoted claim strings
arrived escaped and would not compile.

**The rule that prevents it.** Unchanged, and now paid for twice in one lane:
anything carrying a backslash or an apostrophe goes through the Write or Edit
tool, never a heredoc -- INCLUDING a one-off anchor count and INCLUDING prose in
a script's own data table.
