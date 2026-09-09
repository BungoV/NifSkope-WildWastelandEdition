---
name: ww-contract-provenance
description: Write or update a document whose every claim is traced to a live, MOVING source tree — the five-step procedure (hash and line-count first, anchor text beside every line number, re-derive every number last from the anchors, re-read the version constant last of all, diff the source hash end to end), plus the scripted anchor pass and the two failures that make a page wrong rather than merely stale. Use for any format contract, handoff or spec written against src/ in the NifSkope Wild Wasteland tree while another lane is alive in those files, and whenever a lane changes a source a contract page already cites.
---

# Provenance for a contract written against a moving tree

Lane CONTRACTS worked this out from first principles twice on 2026-09-09, over
six documents, and lane RENAME needed it again the same afternoon. It exists
because a format contract is only worth its traceability: a page that says
"the magic is at 0x00" and cannot show you the writer line is a rumour, and a
page whose line numbers point at the wrong function is worse than one with no
line numbers at all.

**The failure this prevents.** `docs/LODGEN_BTD_FORMAT.md` was written against
`src/lodtfile.cpp` while another lane grew it from 1,534 to 1,682 lines and
added header **version 2**. The first draft of the contract did not mention
version 2 at all — not slightly stale, wrong, and wrong in the one field that
decides whether a consumer refuses or misparses.

## The five steps, in order

### 1. Hash and line-count every source BEFORE reading it, and record both

```bash
python - <<'EOF'
import hashlib
for f in ("src/lodtfile.cpp", "src/lodtfile.h"):
    b = open(f, "rb").read()
    print("%-28s %s  %d bytes  %d lines"
          % (f, hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n")))
EOF
```

Those three numbers go in the document, in a table above the claims:

```
| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodtfile.cpp` | `efaf56673b8e09d2` | 65,592 | 1,706 |
```

They are not decoration. They are what step 5 diffs against, and they are what
lets a later reader ask "was this page written against my file?" without
guessing.

### 2. Quote ANCHOR TEXT beside every line number, never a bare number

```
| claim | line | anchor |
|---|---|---|
| the reader's version refusal | 1417-1419 | `this reader knows %2..%3` |
```

The number is a convenience that rots in hours. The anchor is the claim's real
address, and it is the only thing that makes step 3 possible. Pick a fragment
that is **unique in the file** and that will survive a reformat: a string
literal, a full declaration, a distinctive comment. Never a bare `for (` or a
closing brace.

For a RANGE, anchor the START and mark the rest with `…`:
`` `h.u32( LODL_MAGIC );` … `const qsizetype offSizeAt = h.size();` ``.

### 3. Re-derive EVERY line number from its anchor, in one scripted pass, IMMEDIATELY before finishing

Not while writing — at the end, after the last edit, because a concurrent lane
moves the file under you while you write. A worked script is
`E:\Projects\NifskopeWildWastelandEdition\scratchpad\rename_20260909\p14_anchors.py`;
copy it rather than re-writing it. It parses the footer's markdown rows, pulls
the FIRST backticked span out of the anchor column, unescapes markdown (`\|`),
finds it in the current source, and rewrites the number.

**Three rules the script must enforce, each of which cost something:**

* **EXACT and UNIQUE, or no rewrite.** A softened prefix match once pointed a
  row at a different function whose opening tokens matched, and offered
  `1281-1282` for a claim whose code no longer existed at all. Print `MISSING`
  or `AMBIGUOUS` and leave the number alone: a stale number announces itself the
  moment a reader looks, a plausible wrong one never does
  (`MISTAKES.md`, 2026-09-09).
* **Multi-site rows are re-derived BY HAND.** A row like
  `` `lodvfile.cpp:422, 428, 533, 536, 542, 568` `` carries one anchor and six
  numbers; a script that rewrites only the first produces a row that is half
  right and reads as fully checked. Skip anything with a comma and list it for
  manual work.
* **An anchor that is MISSING is a content question, not a numbering one.** It
  means the code it named is gone. Do not delete the row; find what replaced it
  and re-anchor. On 2026-09-09 the height encoding row's anchor had been
  replaced by a helper, `lodtHeightWord`, and the honest repair was to point the
  row at the helper — which is a better claim than the one it replaced.

Report the counts: *"45 rows moved, 1 unchanged, 4 anchors not found"*. A pass
that reports nothing is indistinguishable from a pass that did nothing.

### 4. Re-read the VERSION CONSTANT last of all — after step 3, not with it

A version bump by a concurrent lane is the failure that makes a whole page
wrong rather than slightly stale, because every offset below it may have moved
and every consumer branches on it. Read it from the source, not from the page:

```bash
grep -n "VERSION\|kVersion\|headerVersion\|_MAGIC" src/lodtfile.{h,cpp} src/io/lodvfile.{h,cpp}
```

Then ask the second question, which is the one that actually bit: **does the
consumer know this version?** FO4CS pinned `kVersion = 1u` while this tree's
writer defaulted to 2, so every freshly generated file was refused by the
shipped reader. That belongs in the page, in bold, with the zero-effort
fallback beside it.

### 5. Diff the source's hash end to end

Recompute step 1's hash after the last edit. If it moved, the SEMANTICS were
re-checked, not only the numbers: re-read the regions the page describes, not
just the lines it cites. Then write the new hash into the table, so the stamp
describes the state the page was finished against rather than started against.

## When a lane CHANGES a source a contract already cites

Same procedure, run backwards, and it is owed in the same session as the code:

1. `grep -rln "src/thatfile" docs/` — every page that cites it.
2. Steps 1 and 5 give the new stamp; a small script rewrites the
   `| file | sha256 | bytes | lines |` rows and the prose form
   `` `src/x.cpp` sha256 `abc…`, N lines `` in one pass.
3. Step 3 re-derives the numbers.
4. Any row whose ANCHOR you renamed (a constant, a function) needs the anchor
   text updated too — the number alone is not the claim.

## What this is not

* It is not a substitute for reading the writer. The anchors prove a claim was
  read somewhere; only reading proves it is true. CONSTITUTION rule 4's third
  rule of 2026-09-04 21:33 — check our own tree before quoting a document —
  applies to the page you are writing as much as to the ones you cite.
* It is not a licence to cite a spec as a measurement. A page with no writer
  (`docs/LODGEN_NATIVE_LODO_LODI.md`) carries a **SPEC / NOT YET WRITTEN**
  banner and no provenance footer at all, and says so, rather than a footer that
  points at a design document dressed as a source.

## The line-ending rule that applies to every one of these edits

`docs/` is LF-only; `WW_CHANGES.md` is MIXED and stays so. Measure with Python
byte counts (`b.count(b"\r")`), splice mixed files in binary, and assert the CR
count is unchanged before writing. `grep` lies about this and heredocs arrive
CRLF (CONSTITUTION rule 8).
