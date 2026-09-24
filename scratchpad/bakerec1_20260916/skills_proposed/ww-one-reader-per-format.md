---
name: ww-one-reader-per-format
description: One reader per file format, imported by every consumer, with a grep gate that fails when anyone hand-parses it again. Use when a format is about to change, or when a second place starts parsing it.
---

# One reader per format

## When this fires

You are about to change a file format, and a grep shows more than one place that
parses it. Or you are adding a second consumer and are about to copy the first
one's parsing code.

Lane BAKEREC1 (2026-09-17) met this with the `.lodb` bake record: four harnesses
each carried their own parser of the binary container, every one of them spelled
slightly differently (`b[b.index(b'{'):]`, `json.loads(raw[16:])`, and two more).
Four parsers is four places a format change has to be found, and the fourth one
would have been discovered by watching an unrelated harness fail on a Tuesday.

## The procedure

1. **Count the parsers before you touch the format.** Grep for the container's
   tells -- a magic string, a fixed header offset, a slice of the raw bytes --
   not for the format's name. A parser rarely mentions the format.
2. **Write ONE reader**, deliberately as a SECOND implementation of the writer:
   an independent reader is the only thing that can catch a writer that agrees
   with itself. Give it the writer's field names, not prettier ones.
3. **Keep the old spelling of the returned keys** where consumers already use
   them, so each consumer changes by one line (`json.loads(...)` becomes
   `reader.read(path)`) instead of being rewritten.
4. **Refuse an unknown version BY NAME**, in the reader, with the words a user
   can act on ("re-bake once and every bake writes it"). Ignore an unknown line
   KIND, so a later lane can add one. Refusing the version and ignoring the kind
   are not the same rule and both are needed.
5. **Keep unknown lines rather than dropping them**, so a consumer can say "this
   file has a line I do not understand" instead of quietly measuring half a file.
6. **Gate it with a grep**, in the harness, over the tests tree:
   * build the pattern from quoted pieces so the gate's own line is not a hit;
   * `--exclude` the reader itself (its header comment will list the parsers it
     replaced) and the harness itself, by NAME, not by a broad filter;
   * `--exclude-dir=__pycache__`;
   * and add the positive half: count the files that DO import the one reader,
     and print the number.

## What goes wrong

* The gate matches its own pattern line, and you "fix" the tree for an hour.
* You exclude too broadly (`--exclude-dir=tests/spells`) and the gate can never
  fail again.
* You drop the old key spellings and every consumer becomes a rewrite, which
  means the change stops being reviewable.
