---
name: ww-anchored-cut
description: Remove code from the NifSkope Wild Wasteland tree (E:\Projects\NifskopeWildWastelandEdition) with a refusing Python CUT script -- the subtractive twin of ww-anchored-hookup. Use whenever a lane deletes a feature, a format version, a switch, a channel or a whole route out of files another lane is also editing: the exact-once anchors, the trailing-newline trap that silently eats the line AFTER the block, the assert that catches it, the brace/paren balance check, and the render gate that is not optional afterwards. Lane HORIZONOUT (2026-09-19) re-derived this and shipped an exe that drew nothing.
---

# NifSkope WW: cut code with a refusing script

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane HORIZONOUT
(2026-09-19), which cut the baked-horizon far-shadow route out of seven source
files and, in doing so, lost one line that no count, no CRC and no census could
see.

`ww-anchored-hookup` is the ADDITIVE twin: it writes the few lines a new file
needs to join the build. This is the subtractive one. Same law -- a refusing
Python script through the **Write tool, never a heredoc** (a heredoc eats
backslashes; that mistake is in `MISTAKES.md` twice), `--check` first, nothing
interactive, no `sed -i`.

## 1. The shape of a cut script

One script per source file, named `cut_<file>.py`, living in the lane's
scratchpad so the director can read what was removed:

```python
CUTS = [
    ("<start anchor>", "<end anchor>"),   # the block, start..end INCLUSIVE
    ...
]
```

and a `cut(text, start, end)` that finds `start`, finds `end` **after** it, and
removes `text[i : j + len(end)]`. Every cut asserts:

* `text.count(start) == 1` and, in the slice after it, `text.count(end) >= 1`;
* the removed slice **contains** a word you expect (the switch name, the
  version constant) -- a cut that removes the wrong region usually removes the
  wrong words;
* byte and line deltas are printed per cut, not per file.

Run `--check` first. It prints and writes nothing. Only then run live.

## 2. THE TRAP: an `end` anchor that ends in a newline

This is the one that costs a build.

```python
("void horizonMarch(", "}\n")      # WRONG -- eats the line after the block
("void horizonMarch(", "}")        # right
```

If `end` ends with `\n`, the removal spans that newline, so the block's closing
line and the **line that follows it** are joined -- and if the following line
was a statement, it is gone. The file still compiles. The braces still balance.
Nothing in a diff summary looks odd.

HORIZONOUT audited its own cut scripts after the first symptom and found
**eighteen** such sites across four scripts -- and the line that actually broke
the build **was not one of the eighteen**. Two rules out of that:

* **Never let an `end` anchor end in a newline.** Grep your own scripts for
  `\\n")` before running any of them.
* **A helper known to be lossy condemns the WHOLE file it touched, not only the
  sites an audit can enumerate.** If you find one, re-read every region the
  script touched against `git diff`, line by line, not by count.

The assert that MISSED it was `assert removed_bytes == len(block)`. A swallowed
line satisfies it, because the swallowed bytes are inside the slice that was
asked for. The assert that catches it:

```python
before = text[:i].rsplit("\n", 1)[0]
after  = text[j + len(end):].split("\n", 1)[0]
# the first surviving line after the cut must be one you NAMED in the script
assert after.strip() == expect_next.strip(), (after, expect_next)
```

Name the next surviving line in every cut. It is one extra string per cut and
it is the only cheap check that sees this class.

## 3. After the cut, before the build

* **Brace/paren balance per file**, counted in Python over the whole file
  (`scratchpad/<lane>/bracescan.py` in HORIZONOUT). A cut that lands mid-block
  balances anyway if it removed a matched pair, so this is necessary, not
  sufficient.
* `git diff --stat` per file, and read the diff of every cut region. Not the
  summary -- the region.
* Line endings by **Python byte count** (`b.count(b'\r')`), never grep. Match
  the file's neighbours; never normalise.

## 4. The gate that is not optional

**A count is not a picture.** If the cut touched anything a renderer reads --
a builder, a vertex loop, a channel, a shader input -- then the bake's own
numbers CANNOT see the damage. HORIZONOUT's broken exe printed, to the vertex,
the same `2446 placements read, 2446 drawn ... 402 bases, 415 buckets` as the
day before, the same channel means to three decimals, and all three `.lodi` CRC
levels green, while every object was being drawn a chunk origin away from the
camera and nothing appeared on screen.

So, in this order, after any cut that touched a renderer:

1. a gate that COMPARES TWO RENDERS -- `tests/spells/lodi_v7.sh` G4,
   `tests/spells/lodl_channels.sh` leg (b), `tests/spells/native_open.sh` (c);
   these are the only legs that can see a wrong transform;
2. one picture through the render hook (`nifskope-ww-render-shot`), looked at;
3. only then the read-back, census and CRC checks.

The failure signature to recognise instantly: **"the render is byte-identical
to the default"** on many legs at once, or `covered 0.0000`. That is not a
channel bug. That is the scene drawing nothing.

## 5. What to keep in the report

The cut scripts themselves, the `--check` output, the per-cut byte/line deltas,
and -- if any anchor was reconstructed rather than copied from the file (a
comment opener retyped from memory, say) -- **say so by name**. HORIZONOUT had
two such lines in `src/lodgen.cpp` and `src/lodifile.cpp` and reported them
rather than letting a future reader assume they were originals.
