#!/usr/bin/env python3
"""NOPROMPT 2026-09-09, second half: the guard says what it discarded.

The dialog it replaces had no log line, so its absence could only ever be
measured as "the run did not hang" -- which passes just as well on a run whose
document was never dirty in the first place.  So the guard writes a READBACK:
one line per document it actually took a decision on, in
release/ww_headless_close.log, and nothing at all when there was nothing to
discard.  That is the floor the harness needs on the other side.

src/nifskope.cpp only, CRLF region, CR count asserted.
"""
import sys

CPP = "src/nifskope.cpp"

with open(CPP, "rb") as f:
    cpp = f.read()
cr0 = cpp.count(b"\r")


def crlf(t):
    return t.replace("\n", "\r\n").encode("utf-8")


old = crlf(
    """\tif ( wwHeadlessRun() ) {
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

new = crlf(
    """\tif ( wwHeadlessRun() ) {
\t\t/* WHAT WAS DISCARDED IS WRITTEN DOWN.
\t\t *
\t\t * The dialog this replaces left a trace on the screen; a discard leaves
\t\t * none, and "the harness did not hang" passes equally well on a run whose
\t\t * document was never dirty -- so it cannot tell a working guard from a
\t\t * case that never reached one.  Every document actually decided for is
\t\t * therefore named in release/ww_headless_close.log, and a run with nothing
\t\t * to discard writes no file at all.  That pair is what a harness can fail
\t\t * on in both directions.
\t\t *
\t\t * Collected BEFORE anything is cleared, or the answer is always "nothing".
\t\t */
\t\tQStringList discarded;
\t\tauto dirty = []( const NifModel * model, bool windowFlag ) {
\t\t\treturn windowFlag || ( model && model->undoStack && !model->undoStack->isClean() );
\t\t};
\t\tauto takeWindow = [&discarded, &dirty]( NifSkope * document ) {
\t\t\tif ( !document )
\t\t\t\treturn;
\t\t\tif ( dirty( document->nif, document->isWindowModified() ) )
\t\t\t\tdiscarded << ( document->nif
\t\t\t\t\t? document->nif->getFileInfo().completeBaseName() : QString() );
\t\t\tdocument->cfg.suppressSaveConfirm = true;
\t\t\tif ( document->nif && document->nif->undoStack )
\t\t\t\tdocument->nif->undoStack->setClean();
\t\t\tdocument->setWindowModified( false );
\t\t};
\t\ttakeWindow( this );
\t\tfor ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
\t\t\ttakeWindow( document );
\t\tfor ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
\t\t\tif ( !document )
\t\t\t\tcontinue;
\t\t\tif ( document->isModified() )
\t\t\t\tdiscarded << document->displayName();
\t\t\tdocument->unsavedInMemory = false;
\t\t\tif ( document->nif && document->nif->undoStack )
\t\t\t\tdocument->nif->undoStack->setClean();
\t\t}
\t\tif ( !discarded.isEmpty() ) {
\t\t\tqInfo().noquote() << "headless close: discarded unsaved changes to"
\t\t\t\t<< discarded.join( QStringLiteral( ", " ) );
\t\t\t/* A FILE, not only qInfo(): NifSkope links as a Windows GUI subsystem
\t\t\t * binary, so a harness that pipes it cannot count on the message
\t\t\t * arriving.  Same home and same append discipline as WW_GRID_PROBE. */
\t\t\tQFile probe( QApplication::applicationDirPath()
\t\t\t\t+ QStringLiteral( "/ww_headless_close.log" ) );
\t\t\tif ( probe.open( QIODevice::Append | QIODevice::Text ) ) {
\t\t\t\tfor ( const QString & name : std::as_const( discarded ) )
\t\t\t\t\tQTextStream( &probe ) << "discarded " << name << "\\n";
\t\t\t}
\t\t}
\t}
"""
)

n = cpp.count(old)
if n != 1:
    sys.exit("guard block appears %d times, expected 1" % n)
cpp = cpp.replace(old, new, 1)

added = new.count(b"\r\n") - old.count(b"\r\n")
cr1 = cpp.count(b"\r")
if cr1 - cr0 != added:
    sys.exit("CR moved by %d, expected %d" % (cr1 - cr0, added))
if b"\r" in cpp.replace(b"\r\n", b""):
    sys.exit("lone CR")

with open(CPP, "wb") as f:
    f.write(cpp)
print("fix02: %s +%d lines (CR %d -> %d)" % (CPP, added, cr0, cr1))
