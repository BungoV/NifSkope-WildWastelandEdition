#!/usr/bin/env python
# Append lane IMPOSTORSHOW's entry to the root MISTAKES.md.
#
# MISTAKES.md is CRLF throughout (measured: 9354 CR, 9354 LF). A heredoc or a
# text-mode write would arrive LF and silently convert the file's ending class,
# so this is a BINARY splice with the CR count asserted on both sides.
import io, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, "MISTAKES.md")

ENTRY = """
## A cached resource pointer, and the function that frees what it points at (lane IMPOSTORSHOW, 2026-09-19)

`NifModel` keeps `gameResources`, a pointer to the `GameManager::GameResources`
that answers every `findResourceFile` and `getResourceFile` for that model. The
preview's loose-sheet registration called
`Game::GameManager::addNIFResourcePath( scene->nifModel, root )` and threw the
return value away, on the reading that "add" adds.

It does not. `addNIFResourcePath` calls `removeNIFResourcePath` first, that
decrements a refcount which was zero and `delete`s the object, and then returns
a NEW one. So the call did two things at once: it installed the root, and it
freed the thing the model still pointed at. Every texture lookup afterwards was
a use-after-free. The symptom was a SIGSEGV inside a Qt event callback with an
all-`?? ()` backtrace -- the exe is stripped -- and, on the run that did not
crash, a hang at `qApp->quit()` after the log had been written in full. Neither
one looks like a resource-registration bug, and two hours went on the GL draw
path that had never been reached.

Three rules out of it.

**A function that returns a pointer to the thing you already hold is replacing
it, not extending it.** `grep` the other call sites before believing otherwise:
all four in `nifmodel.cpp` assign the result. Mine was the only one that did
not, which was the tell and was there to be read at the start.

**A crash whose stack is `?? ()` is a crash you cannot locate by reading the
stack, so stop reading it.** The bracket trace that found this one
(`WW_IMPOSTOR_TRACE` in `src/gl/impostordraw.cpp`) took one build and said in
its first run that the draw path was not even reached -- it stopped at the
colour-sheet refusal every frame. Static analysis of the draw had already
"ruled out" six things that were never suspects.

**A loose resource root is named by its LAST data-shaped component, not by
where you put it.** `BA2File::findPrefixLen` (`lib/libfo76utils/src/ba2file.cpp:398`)
cuts a loose file's index name at the first path component that is one of its
own fourteen data-folder names -- "textures", "meshes", "materials" and so on --
and when the registered path contains none of them the prefix is ZERO and every
file is indexed under its whole absolute path. Registering `<fixture>` indexed
the sheets as `e:/.../fixture/textures/...dds` while every lookup asked for
`textures/...dds`; the files were plainly on disk and the log said "not found in
archives". Register the `textures` folder itself -- which is what
`GameManager::find_paths` does (`gamemanager.cpp:808`) and why it lists the
SUBFOLDERS of a data directory.

**`test exe -nt source` is not the staleness check when the build is long.** A
13-minute build started at 10:30 was still compiling when the source was edited
again at 10:39; it linked at 10:43, so the exe was NEWER than the file and the
chain's own gate said "ok". The OBJECT told the truth -- `impostordraw.o` was
stamped 10:38, a minute before its source. Compare the `.o` to the `.cpp`, not
the exe to the `.cpp`, whenever an edit could have landed mid-build, and do not
edit a source while its build is running unless you intend a second one.
"""


def main():
    data = open(PATH, "rb").read()
    cr, lf = data.count(b"\r"), data.count(b"\n")
    if cr != lf:
        sys.exit("REFUSED: MISTAKES.md is not uniformly CRLF (CR=%d LF=%d)" % (cr, lf))
    if b"lane IMPOSTORSHOW, 2026-09-19" in data:
        sys.exit("REFUSED: the entry is already there")
    if not data.endswith(b"\r\n"):
        sys.exit("REFUSED: the file does not end on a line break")

    add = ENTRY.replace("\n", "\r\n").encode("utf-8")
    out = data + add
    open(PATH, "wb").write(out)

    check = open(PATH, "rb").read()
    print("bytes %d -> %d  CR %d LF %d" % (len(data), len(check),
                                           check.count(b"\r"), check.count(b"\n")))
    if check.count(b"\r") != check.count(b"\n"):
        sys.exit("REFUSED AFTER WRITE: line endings no longer uniform")


main()
