---
name: ww-reuse-or-rebuild-chain
description: Use when an expensive artefact (the FO4CS .lodo library) may be KEPT instead of rebuilt: a chain of refusals, cheapest first, first refusal wins, kept only at the end of the chain, and a census line that says reused or rebuilt and why.
---

# ww-reuse-or-rebuild-chain

Written by lane PERF1, 2026-09-17; placed by the director at landing.

*How to let an expensive artefact be KEPT instead of rebuilt, without ever
shipping a stale one -- and how to make the bake say which it did.*

## The shape

A chain of refusals, first refusal wins, one sentence each, evaluated cheapest
first. The artefact is kept only if the chain reaches the end. PERF1's chain for
the FO4CS object library (`.lodo`), in order:

1. not offered -- this is not an incremental run;
2. a switch the artefact depends on is on (occluders: the per-model box it needs
   lives in neither file, so a kept library has nothing to offer);
3. the load order moved (FNV over each plugin's lower-cased NAME and byte SIZE);
4. the plugin corpus moved (FNV over every LAND's VHGT bytes);
5. the object census moved (the object walk's own hash);
6. there is no previous artefact beside the one this run would write;
7. it did not read back -- and the read is PAYLOAD-CHECKED, not header-only;
8. its own header disagrees with the record that offered it.

## The rules

* **Every input the artefact was built from must be in the chain, or fenced and
  named.** PERF1's four inputs all come out of the PLUGINS; a `.nif` edited on
  disk moves none of them, and a reused run opens no model to notice. That gap
  is fenced (reuse only under `--incremental`; the ruled pipeline has occluders
  ON and therefore always rebuilds) and written down as a divergence row, not
  quietly left out.
* **The decision goes in the census, in words, every run.** `reused (...)` or
  `rebuilt (<the one test that refused>)`. Never a boolean, never silence. Give
  it its OWN keyword; do not hang it off a keyword that already carries a
  different sentence (MISTAKES.md 2026-09-17, entry 5).
* **A kept artefact must be read back the way a fresh one is written.** PERF1
  found `lodoRead` had never restored `loadOrderHash`; nothing noticed for two
  format versions, because nobody had ever read a library back and then WRITTEN
  from it. The reused `.lodi` came out 12 bytes apart. Reuse is what turns a
  reader's omission into a wrong file.
* **The saving must be measurable in a shipped line**, not in a scratch build:
  put the stage split in the census (see the stage-time split) so the next lane
  reads what reuse bought without instrumenting anything.

## The refuter this skill owes

Touch ONE byte of an input between two runs -- and touch it in a way the parser
still accepts. PERF1's first attempt appended a NUL to an ESM copy, the run died
with `end of input file` and never reached the decision at all; the refuter that
worked wrote a minimal valid second plugin and edited its author string.
A refuter that kills the program has refuted nothing.

## The proof this skill owes

The kept-artefact run and the full run must produce BYTE-IDENTICAL outputs
(see `ww-prove-tree-identity`), on the null arm AND on a mixed arm where some
work is replayed from cache and some is redone.
