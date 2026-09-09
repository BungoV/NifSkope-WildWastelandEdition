#!/usr/bin/env python3
"""NOPROMPT 2026-09-09: headless runs never raise the Save Confirmation dialog.

Three edits, all in files this lane owns:

  src/nifskope.cpp  + <QProcessEnvironment> include
                    + NifSkope::wwHeadlessRun() -- the ONE headless predicate,
                      the same test saveUi() has used since 2026-07-27
                    + one guard at the top of closeEvent()
                    + generated .btd/.lodt documents start CLEAN
  src/nifskope.h    + the wwHeadlessRun() declaration

src/nifskope.cpp is MIXED (9234 CRLF lines of 10357); every region touched here
is CRLF, so every inserted line is CRLF and the script asserts the CR count grew
by exactly the number of lines added.  src/nifskope.h is LF-only.
"""
import sys

CPP = "src/nifskope.cpp"
H = "src/nifskope.h"


def read(p):
    with open(p, "rb") as f:
        return f.read()


def write(p, b):
    with open(p, "wb") as f:
        f.write(b)


def crlf(text):
    """A block written with plain \n, emitted CRLF."""
    return text.replace("\n", "\r\n").encode("utf-8")


def once(b, anchor, what):
    n = b.count(anchor)
    if n != 1:
        sys.exit("ANCHOR %s appears %d times, expected 1" % (what, n))
    return n


# ---------------------------------------------------------------- src/nifskope.cpp
cpp = read(CPP)
cpp_cr0 = cpp.count(b"\r")
cpp_lf0 = cpp.count(b"\n")

# 1. the include
a_inc = b"#include <QProgressBar>\r\n"
once(cpp, a_inc, "QProgressBar include")
cpp = cpp.replace(a_inc, b"#include <QProcessEnvironment>\r\n" + a_inc, 1)
added = 1

# 2. the predicate + the guard, both at closeEvent
a_close = (
    b"void NifSkope::closeEvent( QCloseEvent * e )\r\n"
    b"{\r\n"
    b"\tif ( closingWorkspaceGroup ) {\r\n"
    b"\t\te->accept();\r\n"
    b"\t\treturn;\r\n"
    b"\t}\r\n"
)
once(cpp, a_close, "closeEvent head")

predicate = crlf(
    """/*! True when this process is a WW harness or batch run rather than a person.
 *
 *  ONE predicate for every headless entry, and deliberately the SAME test
 *  saveUi() has made since 2026-07-27: any environment variable whose name
 *  starts with WW_.  Every headless route this application has is selected by
 *  exactly such a variable -- WW_RENDER_SHOT and its WW_RENDER_* switches,
 *  WW_LOD_CHANNEL, WW_IMPOSTOR_BAKE, WW_FIRSTFRAME_TEST and the WW_*_TEST
 *  harnesses -- so a new switch is covered the day it is written and there is
 *  no second list to keep in step.  `-no-gui` is answered too for completeness,
 *  although that path selects a QCoreApplication in main.cpp and never builds a
 *  window at all.
 *
 *  Cached: the environment and the command line do not change under us, and
 *  this is asked on a close path that must not walk the environment per window.
 */
bool NifSkope::wwHeadlessRun()
{
\tstatic const bool headless = []() {
\t\tconst QStringList envKeys = QProcessEnvironment::systemEnvironment().keys();
\t\tfor ( const QString & key : envKeys ) {
\t\t\tif ( key.startsWith( QLatin1String( "WW_" ) ) )
\t\t\t\treturn true;
\t\t}
\t\treturn QCoreApplication::arguments().contains( QLatin1String( "-no-gui" ) );
\t}();
\treturn headless;
}


"""
)

guard = crlf(
    """
\t/* HEADLESS RUNS DISCARD; THEY NEVER ASK (2026-09-09).
\t *
\t * `qApp->quit()` is not a bare exit(0) any more.  QCoreApplicationPrivate::quit()
\t * is virtual (Qt 6.11 QtCore/private/qcoreapplication_p.h:105) and
\t * QGuiApplicationPrivate overrides it (QtGui/private/qguiapplication_p.h:83) to
\t * CLOSE EVERY TOP-LEVEL WINDOW before the event loop exits.  So every WW_* hook
\t * that ends in qApp->quit() -- the render shot, the impostor card bake, every
\t * WW_*_TEST harness -- arrives HERE, asks saveConfirm(), and raises a modal
\t * question that nobody is there to answer.  The run then hangs until its own
\t * `timeout` kills it, and writes no picture.  Reported by bungo 2026-09-09,
\t * "agents keep always hanging on save confirmation", with the Save Confirmation
\t * box on TreeMapleForest3 -- the bake writes LOD1/LOD2 Size and the `_L*` shape
\t * flags into the loaded model (src/nifskope_ui.cpp, IMPOSTOR CARD BAKER), which
\t * is what made that document modified.
\t *
\t * A headless run has no user whose work could be lost.  Its document is scratch:
\t * the bake writes its own PNGs and .txt, a render writes its own framebuffer,
\t * and neither has anything to do with the file on disk.  So the honest answer to
\t * every one of those questions is No, and it is given here as STATE rather than
\t * as a branch -- one guard, and the whole close path below then takes the
\t * discard answer by itself: this window's saveConfirm(), every group member's,
\t * and the "unsaved and not on disk anywhere" background-document question.
\t * Nothing on the interactive path is touched: with no WW_ variable set this
\t * block does not run, and a real edit still prompts exactly as it did.
\t *
\t * The ~40 harnesses that call undoStack->setClean() by hand to dodge this
\t * dialog, and the two that keep a timer clicking "No" on it, no longer need to;
\t * they are left alone because doing nothing is still correct for them.
\t */
\tif ( wwHeadlessRun() ) {
\t\tcfg.suppressSaveConfirm = true;
\t\tif ( nif && nif->undoStack )
\t\t\tnif->undoStack->setClean();
\t\tsetWindowModified( false );
\t\tfor ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
\t\t\tif ( !document )
\t\t\t\tcontinue;
\t\t\tdocument->cfg.suppressSaveConfirm = true;
\t\t\tif ( document->nif && document->nif->undoStack )
\t\t\t\tdocument->nif->undoStack->setClean();
\t\t\tdocument->setWindowModified( false );
\t\t}
\t\tfor ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
\t\t\tif ( !document )
\t\t\t\tcontinue;
\t\t\tdocument->unsavedInMemory = false;
\t\t\tif ( document->nif && document->nif->undoStack )
\t\t\t\tdocument->nif->undoStack->setClean();
\t\t}
\t}
"""
)

cpp = cpp.replace(a_close, predicate + a_close + guard, 1)
added += predicate.count(b"\r\n") + guard.count(b"\r\n")

# 3. a generated document has nothing unsaved in it
a_perf = b"\tperfMark( \"loadFromFile (views detached)\" );\r\n"
once(cpp, a_perf, "perfMark after the load chain")

generated = crlf(
    """\t/* A GENERATED DOCUMENT IS NOT A MODIFIED ONE (2026-09-09).
\t *
\t * The .btd and .lodt routes above do not PARSE a document, they BUILD one, and
\t * building it fires NifModel::dataChanged, which is wired to setWindowModified
\t * (this file, the constructor).  So a terrain document was born dirty: closing
\t * it -- or quitting a headless render of it -- asked whether to save changes
\t * that nobody had made, to a file the save path refuses to write anyway
\t * (NifSkope::save() sends .btd and .lodt to Save As so a game file is never
\t * overwritten with foreign bytes).
\t *
\t * As generated IS as loaded here, exactly as it already is for the starter
\t * scene above, so the stack is cleared and marked clean before anyone is told
\t * the document is ready.  A later real edit dirties it again through the same
\t * signal, so the prompt still protects actual work.
\t */
\tif ( loaded && nif && nif->undoStack
\t\t&& ( f.suffix().compare( QLatin1String( "btd" ), Qt::CaseInsensitive ) == 0
\t\t\t|| f.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 ) ) {
\t\tnif->undoStack->clear();
\t\tnif->undoStack->setClean();
\t\tsetWindowModified( false );
\t}
"""
)
cpp = cpp.replace(a_perf, generated + a_perf, 1)
added += generated.count(b"\r\n")

cpp_cr1 = cpp.count(b"\r")
cpp_lf1 = cpp.count(b"\n")
if cpp_cr1 - cpp_cr0 != added or cpp_lf1 - cpp_lf0 != added:
    sys.exit("LINE ENDINGS: +%d CR, +%d LF, expected +%d of each"
             % (cpp_cr1 - cpp_cr0, cpp_lf1 - cpp_lf0, added))
if b"\r" in cpp.replace(b"\r\n", b""):
    sys.exit("a lone CR appeared in " + CPP)

# ---------------------------------------------------------------- src/nifskope.h
h = read(H)
h_cr0 = h.count(b"\r")
a_h = b"\t//! Save Confirm dialog\n\tbool saveConfirm();\n"
once(h, a_h, "saveConfirm declaration")
h_add = (
    b"\t//! True in a WW harness / batch run rather than an interactive one.\n"
    b"\t//! The one headless predicate; see nifskope.cpp for what it answers for.\n"
    b"\tstatic bool wwHeadlessRun();\n"
)
h = h.replace(a_h, h_add + a_h, 1)
if h.count(b"\r") != h_cr0:
    sys.exit("src/nifskope.h picked up a CR")

write(CPP, cpp)
write(H, h)
print("fix01: %s +%d lines (CR %d -> %d), %s +%d lines (CR %d)"
      % (CPP, added, cpp_cr0, cpp_cr1, H, h_add.count(b"\n"), h.count(b"\r")))
