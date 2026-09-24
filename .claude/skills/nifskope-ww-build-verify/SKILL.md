---
name: nifskope-ww-build-verify
description: Build the NifSkope Wild Wasteland Edition (E:\Projects\NifskopeWildWastelandEdition) and verify a change the only way that counts -- the gated build chain (make's own exit code, never grep's), the exe held by bungo's open window (rename aside, never kill), the stylesheet copied at link time, the harness run on an exe proven newer than the sources, the in-app grab, and what to tell bungo about his open window. Use for every NifSkope WW change before reporting it landed, whatever file it touched.
---

# NifSkope WW: build and verify a change

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. Nothing is committed without
bungo's word ("Not yet" stands until he lifts it). He runs `release\NifSkope.exe` directly and
keeps windows open for hours -- see the last section.

## The chain (one command, run in the background, poll the task file)
```bash
cd /e/Projects/NifskopeWildWastelandEdition && python "<scratch>/fixNN.py" \
 && LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r') \
 && if [ -n "$LOCKED" ]; then mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" && echo "running copy (pid $LOCKED) renamed aside"; else echo "exe not held by a window"; fi \
 && MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc' \
 && test release/NifSkope.exe -nt src/<the file you changed> \
 && cmp res/style.qss release/style.qss && echo "sheet in step" \
 && SHOT="<scratch>/panel.png" timeout 500 bash tests/spells/<harness>.sh 2>&1 | tail -12
```

`bash tools/ww_build.sh <sources>` is that chain, minus the patch script and the harness, as one
IN-TREE script -- use it when the session executes only under the repo, as an account-B lane does,
since such a lane may not invoke `/c/msys64/usr/bin/bash` directly.

Every `&&` is a gate. The rules each one encodes:

* **Patch with a script file, never a heredoc or `python -c`.** Heredocs halve backslashes and
  `python -c` cannot mix `\` into bytes literals (2026-09-06, twice). Write `fixNN.py` with the
  Write tool; assert `count(anchor) == 1` before every replace; assert the CR count is unchanged
  (src is LF-only, `WW_CHANGES.md` is mixed and stays so). Text with backslashes or `\n` escapes
  goes in a snippet file the script reads, not in a Python literal.
* **`make`'s own `$?` gates the chain.** `make | grep error | head` reports `head`'s status; the
  chain ran on and the harness tested the PREVIOUS exe (2026-09-06, docs/MISTAKES.md).
* **The exe under bungo's window is renamed aside, never killed.** Windows lets a running image be
  renamed; his process keeps the old file, the link writes a fresh `NifSkope.exe`. Delete the
  `NifSkope_inuse_<pid>.exe` only once `tasklist` shows no NifSkope.
* **`test exe -nt source`** -- the harness must run on an exe newer than what you changed.
* **`cmp res/style.qss release/style.qss`** -- the app reads `release/style.qss`, a copy
  `QMAKE_POST_LINK` makes at link time. A sheet edit without a relink needs `cp` by hand, and a
  style gate that scored 0 on a stale copy said so before the picture did.
* One build at a time (~4 min); the Bash tool's effective timeout is 600 s, so background the chain
  and read `tasks/<id>.output`. A failed chain leaves earlier scripts APPLIED: check which steps
  wrote before re-running.
* **A BACKGROUNDED build is finished when its exit code says so, and at no other moment**
  (2026-09-12, lane UINOTES2). Once the chain is in the background it is tempting to gate on
  what you can see from outside it: the exe's timestamp moving, `make -q` returning 0, `cmp` on
  the stylesheet passing. All three are true of a half-written exe. A link in progress had
  already stamped `release/NifSkope.exe` with a fresh mtime and 21,850,624 bytes while its first
  two bytes were still `00 00`; the harness reported "wrote no log" on two ports before the exe
  began `MZ` and the same harness gave 224 / 0. Wait for `BUILD-RC=` in the task output. If a
  run must start before that, its first two checks are `head -c 2 release/NifSkope.exe` = `MZ`
  and `BUILD-RC=0` present in the log.
* **Read the build log for what ELSE was rebuilt before crediting a change** (2026-09-12, lane
  UINOTES2). A header another lane edited in the same tree pulls its whole dependency fan into
  your build: one lane's two-file UI change compiled nine translation units, five of them
  lodgen's, so the new exe was not "the old exe plus my diff" and a crash that vanished could
  not be credited to the diff. `grep -oE "\-o GeneratedFiles/\.obj/[a-z_0-9]+\.o" <build log>`
  names them in one line.
* **`test exe -nt source` cannot see a STALE OBJECT, and one cost a lane a green gate on the
  wrong binary** (2026-09-18, lane LODIV7, root `MISTAKES.md`). That gate compares SOURCES to the
  exe and passes exactly when every source is older -- which is also true when an object file was
  never rebuilt. A lane inserted a value into the MIDDLE of `enum class LodlChannel`
  (`src/lodinative.h`), which renumbers every value after it; `GeneratedFiles/.obj/btdterrain.o`
  was not recompiled, so that translation unit compared against the OLD ordinals while the freshly
  built lookup returned the new ones, and the terrain viewer read every channel from `mask-r` on
  ONE LATE: `mask-r` drew the sheet's G, `emissive` drew the role-2 normal sheet, `normal` drew
  nothing. G1..G4 all passed, because the lane's own translation units WERE fresh.

  The check, ten seconds, after any edit to a header other files include:

  ```bash
  for f in $(grep -rl "<header>.h" src/ | grep '\.cpp$'); do
      b=$(basename "$f" .cpp)
      ls -l --time-style=+%H:%M "GeneratedFiles/.obj/$b.o" 2>/dev/null
  done
  ls -l --time-style=+%H:%M src/<header>.h
  ```

  Any `.o` older than the header is stale: `touch` its `.cpp` and rebuild. **Adding an enumerator
  at the END renumbers nothing and avoids the whole class.** And the thing that CAUGHT it was a
  neighbour harness with a standing count over code the lane never touched -- run those before
  believing a gate that only exercises the files you edited.

## Whose NifSkope is that? (2026-09-09, lane OFFSCREEN)

`tasklist` says `NifSkope.exe` and nothing else. The rule "his window is renamed aside, never
killed" needs the two told apart, and **the discriminator is the command line, not the pid, the
memory or the start time**: a harness or bake instance is always launched with `--port <n>`, an
interactive window never is.

```bash
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Select-Object ProcessId, CreationDate, CommandLine | Format-List"
```

* `--port` present -> a harness instance. A leftover from a stopped lane may be ended
  (`taskkill //PID <n> //T //F` from Git-Bash; note the doubled slashes). One belonging to a
  RUNNING lane is that lane's work: killing it is a director decision, and whatever is killed
  goes in the report and in `MISTAKES.md`.
* No `--port` -> bungo's own window. Rename the exe aside; never touch the process.
* A bake driver (`tools/bake_impostor_cards.sh`) shows up as one or two `bash.exe` parents with
  the script in their command line. Killing only the child NifSkope makes the driver launch the
  next model a second later; kill the driver first, `//T` for its tree.
* The same filter belongs in any gate that asserts "nothing is left running", or the check fails
  because a person had the application open.

## When you CANNOT build: the game is up

`Fallout4.exe` up means the lane ends BUILD PENDING (CONSTITUTION 6) -- but
"cannot build" is not "cannot check the code". A syntax+semantics pass over the
changed translation units writes NOTHING, takes ~5-60 s each, needs no build
slot and does not touch `release/NifSkope.exe`. Run it before declaring code
finished, always when the lane cannot build. It caught a Qt keyword-macro
collision (`quint16 slots[6]`) that would have cost the director's build
(2026-09-09, lane LODTOPEN).

The flags are `CXXFLAGS`, `DEFINES` and `INCPATH` out of `Makefile.Release` --
read them from there rather than typing them, they move. Write them once into a
throwaway script IN THE REPO (an MSYS2 login shell cannot see `/tmp` written by
the Git-Bash parent) and delete it afterwards:

```bash
cat > sx_$LANE.sh <<'EOF'
g++ -fsyntax-only -march=nocona -msahf -mtune=generic -Wa,-mbig-obj  -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external  -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions -mthreads  -DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG  -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES  -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE  -DNIFSKOPE_VERSION='"x"' -DNIFSKOPE_REVISION='"x"' -DWW_EDITION_VERSION='"x"'  -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB  -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN  -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6  -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL  -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui  -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork  -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui  -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++ "$@"
EOF
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc  'cd /e/Projects/NifskopeWildWastelandEdition && for f in src/a.cpp src/b.cpp; do    echo "== $f"; bash sx_$LANE.sh $f 2>&1 | head -25; echo RC=${PIPESTATUS[0]}; done; rm -f sx_$LANE.sh'
```

* `${PIPESTATUS[0]}` gates, not the grep after it -- the same rule as the build.
* `GeneratedFiles/.moc` must already hold the moc output of a previous build for
  a file that includes a `moc_*.cpp` (`src/nifskope.cpp` does). If the class list
  changed, only a real build settles it.
* It proves the file COMPILES. It proves nothing about linking, about moc for a
  NEW `Q_OBJECT`, or about behaviour: say so, and still end BUILD PENDING.
* Known pre-existing noise: `qchar.h` `-Wsfinae-incomplete` from Qt + libstdc++
  16. Not yours.
* Check the changed identifiers against the Qt keyword macros first -- `emit`,
  `slots`, `signals`, `foreach` -- because that class of error reports twenty
  lines away from the cause.

## A successful build is not a consistent one (2026-09-09)

`make` exiting 0 and `test exe -nt source` passing both held while the linked
exe carried a translation unit two hours stale, compiled against a class that
had since grown three members. Every `-no-gui lodt` run segfaulted; four gates
run before it were green because none of them reached that reader.

The cause is that **qmake's dependency lists are frozen when the Makefile is
generated.** `Makefile.Release` named `src/lodtfile.h` for `nifcli.o`,
`lodgenmanager.o` and `lodtfile.o` and not for `btdterrain.o`, because
`btdterrain.cpp` began including it after the last qmake run -- so make had no
reason to rebuild it, and said nothing.

After ANY change to a header, before running a gate:

```bash
H=src/<the header>.h
grep -rln "#include \"$(basename $H)\"" src/          # every TU that includes it
for f in <those>; do o=GeneratedFiles/.obj/$(basename $f .cpp).o; \
  [ "$o" -nt "$H" ] && echo "ok   $o" || echo "STALE $o"; done
```

A `.o` older than a header it includes is a stale build, whatever make says.
Delete that object and re-link (`rm GeneratedFiles/.obj/<name>.o && make -j2`).
If the include is NEW, re-run qmake as well, or add the dependency to
`Makefile.Release` by hand as a stopgap and say that the qmake run is owed --
the file is generated and the hand edit does not survive.

The class of symptom to expect: a segfault with no output, in a path whose own
source did not change, on inputs that worked yesterday -- including files
written in the format's OLD version, because the corruption is the caller's
stack object, not the data.

## The verdict is a number, then a picture
* A harness prints `N checks, M failures` then `PASS`/`FAIL`; read the numbers next to the exe
  timestamp, never `PASS` alone. Every new check gets a floor on the other side so an empty panel
  cannot pass, and a style or behaviour gate is run against the OLD state first to watch it fail.
* `SHOT=<png>` (WW_LODGEN_SHOT) grabs a dock from inside the app at 640 px; look at it after any
  layout change -- counts do not see a ragged column. Never screen-capture the desktop.
* GUI harnesses: `WW_WINDOW_AT=1960,40` (second monitor, set by `tests/spells/_harness.sh`), one
  NifSkope instance at a time, `--port <unused>` (it exits silently on a bound port), `release/ww_<name>_test.log`.

## Telling bungo
* His open window predates the change (title bar `build <rev>` stays at the last COMMIT, so the
  exe timestamp is the tell). Say so as a headline, not a footnote: the next launch of
  `release\NifSkope.exe` (`<HH:MM>`) has it.
* Report what was measured, with the number, and what was not.

**Syntax-check script name (lane SKELOVERLAY 2026-09-10):** the throwaway script is
`sx_$LANE.sh` with `LANE=<your lane name>` set first, never a fixed name -- a fixed
`sx_tmp.sh` at the repo root was overwritten by two live lanes at once. Delete your
own on exit; never delete another lane's.

## The exe-newer rule covers EVERY binary a gate runs (2026-09-10, lane BUILD8)

`test release/NifSkope.exe -nt <source>` says nothing about the STANDALONE gate
drivers, and most of the measurement in this tree happens in them. Lane BUILD8
changed `src/hkxanim.cpp`, built and swept the application correctly, and then
ran `tests/spells/hkxanim_gates.py` -- which executes
`release/hkxanim_dump.exe`, six hours old, linking the OLD file. Its
`134 checks, 3 failures` measured code that no longer existed. Three drivers
link that one source (`hkxanim_dump`, `hkxwrite_dump`, `gltfexport_dump`) and
all three had to be rebuilt.

**A gate driver is a binary too.** Before running a harness, name the binary it
executes and give that binary the same `-nt` test:

```bash
grep -n "release/[a-z_]*\.exe" tests/spells/<gate>.py tests/spells/<gate>.sh
for e in release/<each>.exe; do [ "$e" -nt src/<the file you changed> ]   && echo "ok    $e" || echo "STALE $e"; done
```

and re-run the driver's own build script (`scratchpad/<lane>/build_dump.sh`)
before the gate, not after the report.

## The process guard is a GATE, not a line of output (2026-09-10, lane BUILD8)

`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` at the top of a lane
answers a question about a moment that has passed. bungo opened
`release/NifSkope.exe` at 14:26:18 while a lane was running; the build a minute
later printed `proc-rc=0` into its own log and linked anyway, and `ld` died with
`cannot open output file release/NifSkope.exe: Permission denied` after four
minutes of compiling. That is what the `&&` chain at the top of this page is
for, and it is why `tools/ww_build.sh` does the rename INSIDE the chain. Use
one of the two; a guard whose answer is only echoed is not a guard, and the
check belongs immediately before the link, not at the start of the lane.

## A changed FLAG makes an object stale, and no mtime says so (2026-09-10, lane BUILD9)

The section above covers a changed HEADER. It does not cover a changed
`DEFINES` / `CXXFLAGS` line, and that is the same failure with no warning at all.

Lane HKX3's hook-up adds `DEFINES += WW_HKXANIM_UI`, and every clip-shaped line
of `src/ui/widgets/timeline.cpp` is behind `#ifdef WW_HKXANIM_UI`. qmake
regenerated the Makefile with the new flag; `make` compares MTIMES, not flags,
found `timeline.o` newer than its source and kept it -- compiled WITHOUT the
define, with `TimelineWidget::setGLView` compiled out -- while the freshly built
`nifskope_ui.o` called it. The link died on one undefined reference and DELETED
`release/NifSkope.exe` on the way out, so there was no application on disk for
four minutes.

**After any change to `DEFINES` or `CXXFLAGS` in the `.pro`, delete the objects
of every translation unit that reads the macro, before running make:**

```bash
grep -rln "WW_MY_FLAG" src/                     # the sources that read it
# ...then every .cpp that includes any header on that list
rm -f GeneratedFiles/.obj/{timeline,timelineedit,timelineviews,...}.o
```

and prove the flag actually reached the compiler with
`grep -c "WW_MY_FLAG" Makefile.Release`. Do NOT try to prove it by grepping the
exe for a string the new code introduces: `QStringLiteral` compiles to UTF-16,
so an ASCII `grep -c "TimelineSeqBox" release/NifSkope.exe` returns 0 on an exe
that contains it twice. In Python, `b.count("TimelineSeqBox".encode("utf-16-le"))`.

## Compile now, link later: bungo's window blocks the LINK, not the build (2026-09-11, lane UI5)

His own window holds `release/NifSkope.exe` -- no `--port` on its command line,
so it is his and it is never touched -- and the lane's brief says the process
check must print `rc=1` immediately before the link. That reads as "wait", and
the lane then spends thirty minutes idle and resumes with a four-minute build
still to run.

Only the LINK opens the exe. Everything before it writes into
`GeneratedFiles/.obj` and cannot touch his window at all, so the build splits at
exactly the line the rule draws:

```bash
# the long half, safe while his window is up: build $(OBJECTS) and stop
OBJ=$(make -f Makefile.Release -p -n 2>/dev/null | grep -m1 '^OBJECTS = ' | sed 's/^OBJECTS = //')
make -j2 -f Makefile.Release $OBJ
```

* Take the object list out of the Makefile (`make -p -n`), never type it. The
  link target is `release/NifSkope.exe: ... $(OBJECTS)`, so naming `$(OBJECTS)`
  as the goal builds everything and stops one step short.
* Do **not** get the same effect by running `make` and letting `ld` fail on a
  locked output. It works -- the objects survive and the old exe survives,
  because the link fails before it can replace it -- but the chain then reports
  `BUILD-RC=1` and every later reader of the log has to be told it is the good
  kind of failure.
* This matters most when a shared HEADER changed: lane UI5 touched
  `src/wwskin.h`, which `Makefile.Release` names as a dependency of **30**
  objects, so the compile was almost the whole build and the link took seconds.
* Afterwards, the resume is `make` with the game check and the "whose NifSkope
  is that" check immediately before it, in the same shell -- unchanged. Check
  the objects are newer than every source before believing the split
  (`find src res tests -newer GeneratedFiles/.obj/<the big one>.o`) rather than
  assuming make's dependency lists were complete.
* A lane that ends BUILD PENDING after doing this says so in its PENDING resume:
  "objects built, COMPILE-RC=0, only the link is owed" is a different resume
  from "nothing has been compiled".

## Added by lane HORIZON2 (2026-09-18)

**A header that NO object lists at all is the silent version, and `make` exits
0 on it.** `Makefile.Release` is generated by qmake and its dependency lists
are frozen at generation time, so a header added to `src/` after the last qmake
run appears in no `.o`'s prerequisites. Editing it produces `Nothing to be done
for 'first'.` and **rc=0**, and `find src tests res -newer release/NifSkope.exe`
stays empty because the exe is newer than everything -- it is the OLD exe.
The tell is the one thing neither check looks at: **the exe's mtime did not
move.** `stat -c %y release/NifSkope.exe` before and after, every build, and
`grep -c <newheader> Makefile.Release` after adding any header (0 = the
Makefile has never heard of it; delete the includers' objects by hand and note
that a qmake run is owed, because the hand repair does not survive one).
Lane HORIZON2, 2026-09-18, root `MISTAKES.md`.

A brief that hands over a `PATH` is handing over a shell assumption: `/ucrt64/bin` is an MSYS2-shell path and resolves to nothing under Git-Bash, where MSYS2 is at `/c/msys64`. The working line for this tree's Bash tool is `export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`.
