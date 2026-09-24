---
name: ww-out-of-tree-hotfix-build
description: Build and ship a one-or-two-file NifSkope fix while a lane owns the tree's make. Compile the patched sources to scratch with the Makefile's own flags, link them with the tree's other objects, gate the side exe with the owning harness, then install it. Use when bungo says "now" and a lane is live.
---

# Out-of-tree hotfix build (director, lane live in the tree)

Used 2026-09-17 twice: ARCHLOCK1's two-line lock fix (gamemanager.cpp) and OPENHERE1's drop option
(nifskope.h + nifskope.cpp + nifskope_ui.cpp). Nothing of the tree's `GeneratedFiles/.obj/` is written.

## When
- bungo asks for a fix "now"/"asap" and one lane is live (one-lane rule: no second lane).
- The change is UI or a local fix in one to three `.cpp` files. A header change is fine only when it
  does not alter a struct/vtable layout (an added enum value is fine); otherwise wait for the lane's make.
- `tasklist | grep -i -E "Fallout4|NifSkope"` as its own command first, and no `make.exe` running in the
  tree (a link against objects make is rewriting is a coin toss).

## Steps
1. Patch the sources in the tree with a Python byte-replace script (`assert count == 1`, print CRLF
   counts before/after; `src/nifskope.cpp` is mixed CRLF/LF). Tell the live lane by SendMessage which
   files you touched and that they are yours, so its stale-source check and its next make count them.
2. Object list, EVERY time, from the current `Makefile.Release` (a lane's qmake adds objects; a stale
   list links "undefined reference"): parse the block that starts `OBJECTS<spaces>=` and its `\`
   continuations. A regex on `OBJECTS` alone matches `OBJECTS_DIR` first and yields one object.
3. Compile with the Makefile's own flags. Pull `DEFINES`, `CXXFLAGS` (strip the literal `$(DEFINES)`),
   `INCPATH`, `LIBS`, `LFLAGS` with `grep "^NAME *=" Makefile.Release | sed`. The compile line MUST be
   `eval g++ -c $CXXFLAGS $DEFINES $INCPATH -o $OUT/x.o src/x.cpp`: `DEFINES` carries
   `-DNIFSKOPE_VERSION=\"2.0.dev11\"` and without `eval` the backslashes reach the compiler
   ("expected primary-expression before ','"). Because of `eval`, `$OUT` must be a forward-slash path
   (`/c/Users/.../ns_fix`), never `$TMP` with backslashes (the assembler then "can't create C:Users...").
4. Link: `g++ $LFLAGS -o $OUT/NifSkope.exe <your .o files> <all other objects> $LIBS`. `LFLAGS` has
   `-Wl,-s`, so the result is stripped like the tree's; for a symbolised copy drop `-s`.
5. Gate on the SIDE exe first: copy it to `release/NifSkope.<tag>.exe` (Avast CyberCapture refuses to
   launch a new unsigned exe from TMP: "Access is denied"; `release/` launches), then run the harness
   that owns the area with `EXE=<side exe> PORT=<unused>` from Git Bash with `WW_WINDOW_AT=1960,40`
   (`external_nif_drop.sh`, `gamemanager_archlock.sh`, ...). A window count taken right after
   `->close()` is one too high: `settle()` first.
6. Install: game check again, `mv release/NifSkope.exe release/NifSkope.before_<tag>.exe`, copy the
   side exe in, delete the side copy, `sha1sum` both. Tell the lane the new mtime/size and that its rung
   may be taken from it. Tell bungo his window needs a restart.
7. Docs: WW_CHANGES entry (binary splice after the title line, CRLF/LF counts printed), HANDOFF line,
   MISTAKES for anything that bit. The lane's next make recompiles the same sources into the tree.

## Refuter
The side exe must pass the owning harness where `release/NifSkope.before_<tag>.exe` fails or lacks
the row (run the harness on the rung too when it is cheap: a PASS on both means the gate does not see
the change).
