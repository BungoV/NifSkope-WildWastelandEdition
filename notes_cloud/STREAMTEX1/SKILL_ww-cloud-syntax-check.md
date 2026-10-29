---
name: ww-cloud-syntax-check
description: Syntax-check NifSkope Wild Wasteland C++ changes inside a Linux CLOUD session (no Windows, no MSYS2, no GPU) before reporting code that nobody could build -- installing Qt 6 headers with apt, the g++ -fsyntax-only flag set that matches NifSkope.pro, generating the ui_*.h headers with uic, telling this lane's errors from the older-Qt ones already on main, and the Python text-mode trap that silently turns a mixed CRLF file into LF. Use in every cloud lane that edits src/ and cannot run release/NifSkope.exe.
---

# WW: syntax-check a change in a cloud session

A cloud lane cannot build the exe, but it CAN compile each changed translation unit for syntax against real Qt
headers. That catches the usual misses (an include not added, a name not declared, a signature that does not
match its call, a lambda that will not convert) before the main session spends a build on it.

## 1. Qt headers (about a minute)

    apt-get install -y -q qt6-base-dev || { apt-get update -q; apt-get install -y -q qt6-base-dev; }

That gives Qt 6.4 under /usr/include/x86_64-linux-gnu/qt6 and uic at /usr/lib/qt6/libexec/uic.

## 2. The flags (from NifSkope.pro: DEFINES, INCLUDEPATH)

    Q=/usr/include/x86_64-linux-gnu/qt6
    FL="-std=c++20 -fsyntax-only -fPIC -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING \
     -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DWW_HKXANIM_UI -DWW_HKXCLIP_CANON -DWW_ANIMWS_HKXMODEL \
     -DNIFSKOPE_VERSION=\"x\" -DWW_EDITION_VERSION=\"x\" -Isrc -Ilib -Ilib/qhull/src -Ilib/gli/gli -Ilib/gli/external \
     -Ilib/libfo76utils/src -I$Q -I$Q/QtCore -I$Q/QtGui -I$Q/QtWidgets -I$Q/QtOpenGL -I$Q/QtOpenGLWidgets -I$Q/QtXml \
     -I$Q/QtNetwork -I$Q/QtConcurrent"
    g++ $FL src/cellstream.cpp; echo rc=$?

Judge by the exit code and `grep -c error`, never by an empty grep for one word: a clean run prints nothing.

## 3. nifskope_ui.cpp needs the generated ui headers

    mkdir -p $S/ui; for u in $(find src -name '*.ui'); do /usr/lib/qt6/libexec/uic $u -o $S/ui/ui_$(basename $u .ui).h; done
    g++ $FL -I$S/ui src/nifskope_ui.cpp

## 4. Errors that are NOT yours (Qt 6.4 is older than the Windows build's Qt)

As of 2026-10-29 nifskope_ui.cpp has two `QApplication::processEvents(ProcessEventsFlag, QDeadlineTimer)` errors
(that overload is newer than 6.4). Prove an error is old by compiling the same file at the base
(`git stash; g++ ...; git stash pop`) and compare: only NEW errors are the lane's.

## 5. What it does not prove

No link, no moc (signals/slots added to a Q_OBJECT class are not checked), no GLSL, no Windows-only branches
(`#ifdef Q_OS_WIN` code is skipped on Linux; read it twice). Say "syntax-checked against Qt 6.4", never "builds".

## 6. The line-ending trap when editing with Python

`open(p).read()` in TEXT mode turns every CRLF into LF, and writing it back converts the whole file: WW_CHANGES.md
(43 CRLF lines) went 118 changed lines for a 32-line insert. Read and write mixed files in BINARY (`'rb'`/`'wb'`),
or `open(p, newline='')`, and give each new line the ending of the line it is anchored to. The gate after every
edit: `git diff --stat` must equal `git diff --stat --ignore-cr-at-eol`.
