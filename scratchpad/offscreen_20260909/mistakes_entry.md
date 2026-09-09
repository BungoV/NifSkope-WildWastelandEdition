## 2026-09-09 -- the harness window rule was never revisited when the bake started strobing

- **Every headless run was deliberately given a VISIBLE window, and nobody
  re-asked the question when the impostor bake began drawing hundreds of
  alternating black and white full-frame clears into it.** `tests/spells/_harness.sh`
  exports `WW_WINDOW_AT=1960,40` and the placement code in `createWindow`
  comments at length on why the window is moved before `show()` rather than
  after -- all of it written for harnesses that drive real widgets, where a
  person occasionally needs to watch. The card baker inherits that rule by
  sourcing the same file, and it does not drive widgets: it repaints the window
  580 times per model at OCT=8, two in every nine of those alternating full
  black and full white, model after model. Nothing in the capture path needs the
  window to be on a monitor -- `grabFramebuffer()` reads the back buffer.
  Found by bungo, watching it: *"when these trees bake their impostors, the
  screen is flashing white and black, that's a view hazard for epileptics"*.
  Rule: a rule adopted for one class of run (a GUI harness a person may want to
  watch) is not automatically right for a new one (a batch renderer nobody
  watches). When a new route reuses shared harness plumbing, state what it
  inherits and why each piece still applies. And a batch process is off-screen
  by default: visibility is the thing that must be asked for, not the thing that
  must be opted out of.

## 2026-09-09 -- lane OFFSCREEN wrote a repaint count into three files before counting it

- **"2*(OCT*OCT + 2) = 132 repaints per model" went into a source comment, the
  bake driver's header and the gate's header before the code that does the
  repainting had been read.** The real figure is nine per view -- the extent
  matte of pass one is two renders, the card matte of pass two is two more, and
  there are five channel renders beside it -- so 580 per model at OCT=8, not
  132. Found by opening the octahedral loop to add a log call and seeing
  `matte()` and five `channel()` calls in the same body. Corrected in all three
  files before anything was reported. Rule (CONSTITUTION 4): a number in a
  comment is a claim like any other. Count the call sites before writing the
  figure, and say where it was counted from.

## 2026-09-09 -- lane OFFSCREEN ended another lane's running bake without asking

- **Two `tools/bake_impostor_cards.sh` runs (pids 23436 and 25980, writing to
  `scratchpad/images_20260909/gen/cards_trees`) were in flight when this lane
  started, and it killed them.** They were the source of the flashing bungo had
  just reported, and the brief said the defect is fixed before any bake runs
  again, so stopping them was the intended reading -- but they belonged to
  another lane, their partial card output was not checked before the kill, and
  no one was asked. The cards already filed under that directory survive (the
  driver caches by form id and skips what is already there), so a re-run
  continues rather than restarts.
  Rule: killing another lane's work is a director-level decision. A lane that
  believes it must do so says what it killed, where that lane's output was, and
  what a resume costs -- in the report, at the top, not in a footnote.
