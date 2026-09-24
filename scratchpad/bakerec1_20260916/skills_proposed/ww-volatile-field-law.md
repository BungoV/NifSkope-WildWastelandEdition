---
name: ww-volatile-field-law
description: Make a written artefact deterministic when it must record wall clocks and absolute paths - name each volatile thing, mask never drop, implement the mask twice, and gate by repeating the identical command. Use when writing any file two runs are supposed to agree on.
---

# The volatile field law

## When this fires

You are writing a file that two runs of the same job must agree on byte for
byte, AND the file has to record things that move anyway: when it ran, where the
inputs live, how long a stage took.

The wrong answers are both common. Dropping the volatile facts makes the file
deterministic and useless to the person reading it. Keeping them makes the file
useful and kills every whole-directory comparison that would have included it.

## The procedure

1. **Put each volatile thing on a named line or a named field**, never inline in
   the middle of a line with facts that do not move. A comparison can then mask
   precisely it, and a reader can see it.
2. **MASK, never drop.** Replace the value with a literal marker (`<volatile>`).
   A dropped line also hides a file that LOST that line; a masked one does not.
   Keep the parts that do not move: a resource line's KIND and ORDER stay, so a
   reordered stack still shows.
3. **Write the mask twice, in two languages** -- once beside the writer, once in
   the independent reader the gates use. Two implementations of one rule is the
   only way a wrong mask is caught.
4. **Count them in the doc, the header comment and both implementations**, and
   move all four together. "The three volatile things" in a header comment above
   a function that masks four is the bug you will ship next.
5. **Gate it by repeating the IDENTICAL command.** Same switches, same output
   directory, same everything -- then compare the normalised files, and assert
   BOTH halves:
   * normalised, the two are byte-equal;
   * RAW, the two DIFFER (or the mask is doing nothing and the gate is asleep).
6. **Name which kinds of line are allowed to differ**, and make the allowance
   exact. "A census line may differ" is too loose; "a census line may differ only
   when it begins `stage times:`" is the rule.

## What goes wrong

* **The repeat is not identical.** Running the two into different output folders
  changes every path the file records and the prose that names the root, so the
  gate measures the folder name. It will be red for a reason that is not the one
  you are looking for, or -- worse -- green because you then loosened it.
* **The leak hides behind passing legs.** Lane BAKEREC1 had seven green legs
  while the record leaked `stage times: ... meshes 61.4 s`, because no other leg
  ran the same bake twice. If nothing in the suite repeats a command, nothing in
  the suite tests determinism, however many legs there are.
* **The masking gets applied to the artefact itself.** Normalisation is for
  COMPARISON. The file on disk keeps the real value, or you have dropped it with
  extra steps.
