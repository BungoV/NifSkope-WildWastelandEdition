---
name: ww-knob-owning-stage
description: Find which stage of a split pipeline actually owns a parameter before reporting that it cannot be reached. Covers the two-process shape this tree keeps producing (a GUI hook driven by environment variables that PHOTOGRAPHS, and a `-no-gui` command line that COMPRESSES), why grepping the argument parser answers a question about one half only, the sidecar that carries a decided value forward so the later stage cannot choose it, and how to turn "prepare a change and ask for a ruling" back into "run the experiment today". Use before writing the words "there is no switch for it", or whenever a measurement's decisive discriminator looks unreachable from a script.
---

# WW: which stage owns the knob?

## When this fires

You have measured something, you know which number would settle it, and a grep
says the number is not settable. You are about to write "PREPARED, needs a
ruling" into a report.

Stop. In this tree that sentence has a specific failure behind it often enough
to be worth a page.

## The shape

The WW pipelines are split across two PROCESSES, and they do not look alike:

| stage | how it runs | how it is configured |
|---|---|---|
| the photography / capture half | the GUI, opened on one file, `--port <n>` | **environment variables** read in `src/nifskope_ui.cpp` |
| the compression / assembly half | `NifSkope.exe -no-gui <verb>` | **command-line arguments** parsed in `src/nifcli.cpp` |

and the two are stitched by a **sidecar** the first writes and the second reads
(for impostor cards, `<formid>.txt`, whose `oct N tileW tileH ... base` line
carries the frame size). Once a value is in the sidecar it is a FACT to the
later stage: `lodgen --impostors` cannot choose the frame size because by the
time it runs, the frames exist.

So `grep -- "--card-res" src/nifcli.cpp` returning nothing is a true answer to
the wrong question. The knob was `WW_IMPOSTOR_TILE` (plus `WW_IMPOSTOR_REF`,
whose ABSENCE switches the whole size ladder off) in `src/nifskope_ui.cpp`, and
`tools/bake_impostor_cards.sh` exposed both as `TILE=` and `REF=` all along.
Lane IMPOSTORFIX5, 2026-09-19, root `MISTAKES.md`.

## The procedure

1. **Name every process that could set it**, before grepping anything. Write the
   list down: the GUI hook, the CLI verb, the driver script, the QSettings the
   panel writes. Four places, not one.
2. **Grep each one in its own idiom.** `--flag` in `src/nifcli.cpp`;
   `qEnvironmentVariable*( "WW_` in `src/nifskope_ui.cpp` and `src/lodgen*.cpp`;
   `settings.value( QStringLiteral( "` in `src/lodgenmanager.cpp`; and plain
   `UPPERCASE=` assignments in `tools/*.sh`. A knob exposed only to the driver
   script looks like a hard-coded constant from every other angle.
3. **Read the driver script even when you are not using it.** `tools/` is where
   this project's knobs are documented, in comments, with the validated values
   (`TILE must be 64, 128, 256 or 512`) and the cost law (`OCT^2 * TILE^2`). It
   is faster than reading the source that consumes them.
4. **Follow the sidecar backwards.** If the later stage reads a value it did not
   choose, open the sidecar, find the line, and grep for the code that WRITES
   that line. That writer is the owning stage, by construction.
5. **Then re-ask the question.** "Can the experiment run today?" is usually yes,
   and a run beats a prepared script and an owed ruling in every direction:
   it produces a number, it can refute your own reading, and it does not put a
   decision on the director's desk that measurement would have made.
6. **If it really is unreachable, say which stage would own it**, so the next
   lane's change lands in the right file. "No CLI switch" is not that sentence;
   "the bake hook reads it from the environment and the driver does not pass it
   through" is.

## The control you still owe

Running the experiment does not excuse the usual floors. Re-bake a subject that
is ALREADY at the setting you are moving to, through the same new route, and
check it reproduces its own published number. If it does not, the experiment is
measuring the route and not the knob. On the card ladder that control is a
full-size subject (`base 128` in its sidecar): change nothing for it, and its
known-answer IoU must come back where it was.

## What this is not

Not a licence to flip a shipped default. The experiment settles the MECHANISM;
what ships is still bungo's, and an owed ruling never ships as a default
(`feedback_authored_lods_only`). The report says: here is the number at the
other setting, here is what it costs, here is the refuter that would show I am
wrong.
