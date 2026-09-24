## 2026-09-17 19:0x -- lane AUDIT1, a PATH copied out of a brief into the wrong shell

- 19:02 -- my brief gave the build as `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH` then
  `mingw32-make -f Makefile.Release -j8`. I typed it verbatim from a Git Bash shell and got
  `mingw32-make: command not found`, `make rc=127` -- and for a moment read a missing toolchain into
  it. `/ucrt64/bin` is a path in the MSYS2 shell's own root; from Git Bash the same directory is
  `/c/msys64/ucrt64/bin`. Fixed by naming the absolute Windows-side path
  (`/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:/e/Tools/GIT/cmd:$PATH`) and the build ran clean.
  **The rule: a PATH is relative to the shell root it was written in. Before copying one between
  shells, `ls` the first entry -- rc 127 is a path question, not a toolchain question.**

## 2026-09-17 18:3x -- lane AUDIT1, a header offset remembered instead of read, and a FAIL blamed on the product

- 18:34 -- my new `.lodj` sweep (`scratchpad/audit1_20260916/lodj_sweep.py`) checked that the cache's
  placement count agrees with the `.lodi` beside it, and read `instanceCount` from a hard-coded offset
  0x80 that I had carried in a note. The field is at 0x58. Every tree came back
  "cache 3526 vs .lodi 98304" and I had four FAILs written down against the writer before I opened the
  decoder sitting in the same folder: `lodgen_native_decode.py:436` parses the header with
  `le('hhhhHHIIIII', b, 0x48)`, which puts `instanceCount` at 0x58. Reading it through
  `D.read_lodi(path)` turned 4 FAILs into 0. **The rule: an offset copied out of a note is a guess;
  the decoder beside it is the contract. A reader in an audit never re-implements a parse that the
  tree already has -- and a FAIL that is uniform across every fixture is a suspicion about the reader
  first.**

## 2026-09-17 18:2x -- lane AUDIT1, a refuter aimed at a field its reader never reads

- 18:22 -- refuter R2 was supposed to prove that `lodgen_bakerec_gate.py` catches a corrupted hash in a
  `.lodb` record. My mutation matched `\b([0-9a-f]{40})\b` and landed on the `switches` digest, which
  that gate never looks at -- it reads the five `hash <name> <16 hex>` rows. The gate stayed green, and
  I wrote NOT CAUGHT about a reader that works. Retargeted at
  `^hash\tobjectCorpusHash\t([0-9a-f]{16})$` and it went red at once. **The rule: a refuter is aimed at
  a field the reader it refutes actually reads. Before recording NOT CAUGHT, grep the reader for the
  field you broke -- a refuter that misses proves nothing about the product, only about the refuter.**

## 2026-09-17 18:1x -- lane AUDIT1, two backslashes through a heredoc in one session, against a written rule

- 18:11 -- twice in one hour I passed text carrying a backslash through a `python - <<PY` heredoc: a
  patch whose `old` anchor silently failed to match (`AssertionError: 0`), and an inline `python -c`
  with a path `.replace` that died with `SyntaxError: EOL while scanning string literal` twenty lines
  from the escape that caused it. The nifskope-ww-lodgen skill states the rule in as many words --
  no text carrying a backslash or an apostrophe goes through a heredoc at all -- and I had read it
  this session. Fixed by writing real script files (`mutate_lodb.py`, `mutate_lodt.py`,
  `mutate_lodo_v3.py`) and running them. **The rule stands and needs no amendment; what needs
  amending is reaching for a heredoc by habit. If the patch text contains either character, the
  first keystroke is a filename.**

## 2026-09-17 17:5x -- lane AUDIT1, a bare TypeError read as two failing checks

- 17:52 -- I ran the VT border and georef readers over `bake/everything/vt` and got
  `TypeError: NoneType object is not subscriptable`, 0 checks, rc=1, which I first tabled as two
  failures of the product. The cause was my own invocation: `--vt-height` is OFF by default, so the
  container carries no role-4 sheet and `Lodv.heights()` correctly returns None. The shipped gate
  `tests/spells/lodgen_terrain_vt.sh` spells `--vt-height` on its own bake (lines 130, 133) and check
  V23 asserts the flag is off by default, so the gate was never stale -- only my fixture was. Re-baked
  with the flag and the three rows went green. **The rule: a reader that raises before it counts has
  produced zero checks, not failing ones. A traceback with `0 checks` is a question about the input;
  read the gate that already exercises the same reader before writing a row against the product.**

