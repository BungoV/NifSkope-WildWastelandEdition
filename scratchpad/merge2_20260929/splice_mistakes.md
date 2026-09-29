## 2026-09-29 -- TERRLIVE1 lane text and MERGE2 (spliced by MERGE2)

### TERRLIVE1
- An ad-hoc preview run with a relative spec path failed, and its turn was not released: held idle 07:52-08:02,
  blocking IDENT2. Fix: every launch goes through a wrapper that releases on every exit (pv.sh); the preview now
  resolves spec paths against the spec.
- Build 1's HYBRID still wrote VT.2/VT.4 whenever the .btr chunk sheets were on, i.e. in the shipped recipe. It
  saved nothing, and that showed only in the byte table. Fix: stage those levels without writing them (build 2).
  Lesson: check an option's saving in the recipe that ships, not the bare one.
- Timestamps typed ahead of the clock three times in DONE.md (05:17/05:16, 06:01/05:59, 05:40/05:39, and
  12:10/12:04 in its header). Corrected each time. Rule: run `date` in the same command that writes the line.
- A Bash heredoc turned the Python regex `\b` into a backspace; the gate's seconds mask then matched nothing and
  the gate went red for the wrong reason. Fix: chr(92) plus `assert chr(8) not in source`.
- The preview mapped an empty .lodl base slot (0xFFFF) to grey instead of the engine default land set; the first
  live-splat pictures had big grey patches. Caught from the picture, then measured: 20-28 fell to 6-10/255 per bin.
- Read the ground outside our painted area as invented colour. It was vanilla's own dim-4 LOD diffuse
  (--vt-fill-vanilla, census line in every bake); bungo had to say it. Lesson: read the bake's own census line for
  what fills an area before describing it.
- `bash chain4.sh &` inside a foreground call kept running, and a second background run was launched; two
  whole-map bakes raced for one folder. The kill of the duplicate was refused. No harm: the duplicate failed at
  once and released the turn. Rule: background only via run_in_background, never `&` in a foreground call.
- The first "outline dip" metric read 0.13 / 0.44 on the old bake, i.e. it did not see the outline it was written
  for. Redefined (sink below both ends, net of vanilla's) and proven: old 10.55 / 9.53 FAIL, new 0.00 / 0.27 PASS.
- The rule patch anchored on `auto sampleLtex`, which matches two lambdas; the sed fallback then moved the wrong
  one (the chunk writer's) and printed nothing. Caught by the move check, reverted that one file, re-anchored at
  `static bool lodgenBakeVtTile(`. Rule: an anchor must be asserted unique, and a fallback edit must assert too.
- Again a Bash heredoc changed Python escapes (`\n` became real newlines), so the nifcli anchors did not match
  (failed loudly, no harm). Patch scripts now go through the Write tool with raw strings.
- A waiter grepped build6.log for "rc=", which the build script prints to its task output, not the log; it would
  never have ended. Stopped it. Wait on the line the log itself writes.

### MERGE2
@@MERGE2_MISTAKES@@
