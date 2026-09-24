#!/usr/bin/env python3
"""Lane NIFPARSE1's FIXES, written as a refusing script and applied only after
the symbolised stack (gate N1) is in hand.

BAKEPERF1's third recorded mistake was "three fixes shipped on hypotheses, two
of them wrong". So these are WRITTEN from the inventory (report section 1.2),
CHECKED against the tree now, and APPLIED only once the stack and the stress
harness have said which of C1 / C2 / C3 is the cause. `--check` writes nothing.

    python fixes.py            # --check
    python fixes.py --apply

Two files here belong to no live lane but are not this lane's either
(src/message.cpp, src/gamemanager.{h,cpp}); keeping them in one refusing script
rather than editing them by hand is CONSTITUTION 1's "one lane per file" applied
honestly. src/model, src/xml and src/data are this lane's own and are in the
same script only so that one artefact is reviewable.

Every file is LF-only; the CR count is asserted on both sides.
"""

import sys, os

ROOT = r"E:\Projects\NifskopeWildWastelandEdition"
NL = chr(10)
TAB = chr(9)
# indent levels, built from chr(9) so no editor or heredoc can eat them
T1 = TAB
T2 = TAB * 2
T3 = TAB * 3
T4 = TAB * 4
T5 = TAB * 5
T6 = TAB * 6
T7 = TAB * 7
T8 = TAB * 8

EDITS = [

    # ---------------------------------------------------------------- F1
    # A MESSAGE BOX IS A WIDGET. Message::append / message build QMessageBoxes
    # and parent them to a main-thread widget, from whatever thread calls them,
    # and append to an unguarded static vector. A bake reaches them by six
    # paths, the commonest being qWarning() on every missing texture in
    # GameResources::get_file. Constructing a QWidget off the GUI thread is
    # undefined in Qt and is a live candidate for the heap corruption.
    ("src/message.cpp", "after",
     "#include <QScreen>" + NL,
     "#include <QThread>" + NL
     + "#include <cstdio>" + NL
     + NL
     + "/* A MESSAGE BOX IS A WIDGET, AND A WIDGET OFF THE GUI THREAD IS UNDEFINED." + NL
     + " * (lane NIFPARSE1, 2026-09-11.)" + NL
     + " *" + NL
     + " * Every entry point below builds a QMessageBox and parents it to" + NL
     + " * qApp->activeWindow(), which splices it into a main-thread widget's child" + NL
     + " * list, and appends it to the unguarded static `messageBoxes`. The LOD" + NL
     + " * generator's chunk pass reaches here from a worker by six paths -- the" + NL
     + " * commonest being qWarning() on EVERY missing texture in" + NL
     + " * GameResources::get_file, through the installed message handler." + NL
     + " *" + NL
     + " * The message is not dropped: it goes where a headless run's diagnostics" + NL
     + " * already go. What is dropped is the window, which a worker had no business" + NL
     + " * creating and which -- on the main thread -- is unchanged." + NL
     + " */" + NL
     + "static bool wwMessageOffGuiThread( const QString & str, const QString & err )" + NL
     + "{" + NL
     + TAB + "if ( !qApp || QThread::currentThread() == qApp->thread() )" + NL
     + TAB + TAB + "return false;" + NL
     + TAB + "const QByteArray s = str.toLocal8Bit();" + NL
     + TAB + "const QByteArray e = err.toLocal8Bit();" + NL
     + TAB + "fprintf( stderr, \"%s%s%s\\n\", s.constData()," + NL
     + TAB + TAB + "e.isEmpty() ? \"\" : \": \", e.constData() );" + NL
     + TAB + "fflush( stderr );" + NL
     + TAB + "return true;" + NL
     + "}" + NL),

    ("src/message.cpp", "after",
     "QMessageBox* Message::message( QWidget * parent, const QString & str, QMessageBox::Icon icon )" + NL
     + "{" + NL,
     TAB + "if ( wwMessageOffGuiThread( str, QString() ) )" + NL
     + TAB + TAB + "return nullptr;" + NL),

    ("src/message.cpp", "after",
     "QMessageBox* Message::message( QWidget * parent, const QString & str, const QString & err, QMessageBox::Icon icon )" + NL
     + "{" + NL,
     TAB + "if ( wwMessageOffGuiThread( str, err ) )" + NL
     + TAB + TAB + "return nullptr;" + NL),

    ("src/message.cpp", "after",
     "void Message::message( QWidget * parent, const QString & str, const QMessageLogContext * context, QMessageBox::Icon icon )" + NL
     + "{" + NL,
     TAB + "if ( wwMessageOffGuiThread( str, QString() ) )" + NL
     + TAB + TAB + "return;" + NL),

    ("src/message.cpp", "after",
     "void Message::append( QWidget * parent, const QString & str, const QString & err, QMessageBox::Icon icon )" + NL
     + "{" + NL,
     TAB + "if ( wwMessageOffGuiThread( str, err ) )" + NL
     + TAB + TAB + "return;" + NL),

    # ---------------------------------------------------------------- F2
    # The shared BA2File. init_archives() and close_archives() delete and
    # rebuild archives[game].ba2File with no lock, and the self-healing retry
    # in get_file calls close_archives() while other workers are inside
    # findFile()/extractFile() -- which also hands out interior string_views.
    # A read/write lock, not a mutex: extractFile is where the texture stage's
    # time is, and serialising it would cost the bake what the fan-out buys.
    ("src/gamemanager.h", "after",
     TAB + "static QRecursiveMutex & nifResourceMutex();" + NL,
     TAB + "/*! THE SHARED ARCHIVE INDEX'S LOCK (lane NIFPARSE1, 2026-09-11)." + NL
     + TAB + " *" + NL
     + TAB + " *  Every NIF-local GameResources has `parent = &archives[game]`, so all" + NL
     + TAB + " *  sixteen chunk workers funnel into ONE object, and `init_archives()` /" + NL
     + TAB + " *  `close_archives()` delete and rebuild its `BA2File` with nothing held." + NL
     + TAB + " *  `lodgenWarmSharedIndices()` covers FIRST use and only first use; the" + NL
     + TAB + " *  self-healing retry in `get_file` (a loose file whose size changed under" + NL
     + TAB + " *  a running bake) calls `close_archives()` at any moment, and `findFile`" + NL
     + TAB + " *  hands out `std::string_view`s into buffers that `delete` frees." + NL
     + TAB + " *" + NL
     + TAB + " *  READ/WRITE, not a mutex: `extractFile` is where the texture stage's" + NL
     + TAB + " *  time is and serialising it would cost the bake exactly what the" + NL
     + TAB + " *  fan-out buys. Readers share; only the build and the teardown exclude." + NL
     + TAB + " */" + NL
     + TAB + "static QReadWriteLock & archiveLock();" + NL),

    ("src/gamemanager.h", "after",
     "#include <QMutexLocker>" + NL,
     "#include <QReadWriteLock>" + NL
     + "#include <QReadLocker>" + NL
     + "#include <QWriteLocker>" + NL),

    ("src/gamemanager.cpp", "after",
     "QRecursiveMutex & GameManager::nifResourceMutex()" + NL
     + "{" + NL
     + TAB + "static QRecursiveMutex" + TAB + "m;" + NL
     + TAB + "return m;" + NL
     + "}" + NL,
     NL
     + "//! See gamemanager.h. One lock for every GameResources' archive state." + NL
     + "QReadWriteLock & GameManager::archiveLock()" + NL
     + "{" + NL
     + TAB + "static QReadWriteLock" + TAB + "l( QReadWriteLock::Recursive );" + NL
     + TAB + "return l;" + NL
     + "}" + NL),

    # the two builders/teardowns take the WRITE lock; recursive, because
    # init_archives recurses into parent->init_archives()
    ("src/gamemanager.cpp", "after",
     "void GameManager::GameResources::init_archives()" + NL
     + "{" + NL,
     TAB + "QWriteLocker" + TAB + "archiveWriteLock( &archiveLock() );" + NL),

    ("src/gamemanager.cpp", "after",
     "void GameManager::GameResources::close_archives()" + NL
     + "{" + NL,
     TAB + "QWriteLocker" + TAB + "archiveWriteLock( &archiveLock() );" + NL),

    # close_materials additionally walks nifResourceMap, which BAKEPERF1's lock
    # missed (one of its two unlocked iterations)
    ("src/gamemanager.cpp", "after",
     "void GameManager::GameResources::close_materials()" + NL
     + "{" + NL,
     TAB + "QWriteLocker" + TAB + "archiveWriteLock( &archiveLock() );" + NL
     + TAB + "QMutexLocker" + TAB + "resourceLock( &nifResourceMutex() );" + NL),

    # the readers. find_file/get_file first check under the read lock and only
    # drop to the builder when the index is genuinely absent.
    ("src/gamemanager.cpp", "replace",
     "QString GameManager::GameResources::find_file( const std::string_view & fullPath )" + NL
     + "{" + NL
     + TAB + "if ( !ba2File && !dataPaths.isEmpty() )" + NL
     + TAB + TAB + "init_archives();" + NL,
     "QString GameManager::GameResources::find_file( const std::string_view & fullPath )" + NL
     + "{" + NL
     + TAB + "if ( !ba2File && !dataPaths.isEmpty() )" + NL
     + TAB + TAB + "init_archives();" + NL
     + TAB + "// the index may not be torn down under us while it is read (NIFPARSE1)" + NL
     + TAB + "QReadLocker" + TAB + "archiveReadLock( &archiveLock() );" + NL),

    ("src/gamemanager.cpp", "replace",
     "bool GameManager::GameResources::get_file( QByteArray & data, const std::string_view & fullPath )" + NL
     + "{" + NL
     + TAB + "if ( !ba2File && !dataPaths.isEmpty() )" + NL
     + TAB + TAB + "init_archives();" + NL,
     "bool GameManager::GameResources::get_file( QByteArray & data, const std::string_view & fullPath )" + NL
     + "{" + NL
     + TAB + "if ( !ba2File && !dataPaths.isEmpty() )" + NL
     + TAB + TAB + "init_archives();" + NL
     + TAB + "/* The read lock covers findFile AND extractFile AND the interior" + NL
     + TAB + " * string_views of the FileInfo between them. It is released before the" + NL
     + TAB + " * retry below, which takes the WRITE lock through close_archives(). */" + NL
     + TAB + "QReadLocker" + TAB + "archiveReadLock( &archiveLock() );" + NL),

    ("src/gamemanager.cpp", "replace",
     TAB + TAB + "if ( std::string_view(e.what()).starts_with( \"BA2File: unexpected change to size of loose file\" ) ) {" + NL
     + TAB + TAB + TAB + "close_archives();" + NL
     + TAB + TAB + TAB + "return get_file( data, fullPath );" + NL,
     TAB + TAB + "if ( std::string_view(e.what()).starts_with( \"BA2File: unexpected change to size of loose file\" ) ) {" + NL
     + TAB + TAB + TAB + "// drop the READ lock before close_archives() takes the write one" + NL
     + TAB + TAB + TAB + "archiveReadLock.unlock();" + NL
     + TAB + TAB + TAB + "close_archives();" + NL
     + TAB + TAB + TAB + "return get_file( data, fullPath );" + NL),

    # ---------------------------------------------------------------- F3
    # NifValue::type() calls initialize(), which CLEARS all four static tables,
    # whenever typeMap is empty. It cannot fire after loadXML() -- but it is
    # reachable from insertType and updateArraySizeImpl on every parse, and a
    # lazy DESTRUCTIVE re-init on a path sixteen threads take is not something
    # to leave resting on call order.
    ("src/data/nifvalue.cpp", "replace",
     "NifValue::Type NifValue::type( const QString & id )" + NL
     + "{" + NL
     + TAB + "if ( typeMap.isEmpty() )" + NL
     + TAB + TAB + "initialize();" + NL,
     "NifValue::Type NifValue::type( const QString & id )" + NL
     + "{" + NL
     + TAB + "/* initialize() CLEARS all four tables, and this is on the parse path" + NL
     + TAB + " * (insertType, updateArraySizeImpl) that sixteen chunk workers take." + NL
     + TAB + " * call_once, so a cold first call cannot be entered twice and a warm" + NL
     + TAB + " * one costs an acquire load. (lane NIFPARSE1, 2026-09-11) */" + NL
     + TAB + "if ( typeMap.isEmpty() ) {" + NL
     + TAB + TAB + "static std::once_flag" + TAB + "initOnce;" + NL
     + TAB + TAB + "std::call_once( initOnce, []() { NifValue::initialize(); } );" + NL
     + TAB + "}" + NL),

    ("src/data/nifvalue.cpp", "replace",
     "bool NifValue::registerAlias( const QString & alias, const QString & original )" + NL
     + "{" + NL
     + TAB + "if ( typeMap.isEmpty() )" + NL
     + TAB + TAB + "initialize();" + NL,
     "bool NifValue::registerAlias( const QString & alias, const QString & original )" + NL
     + "{" + NL
     + TAB + "// XML parse only, under XMLlock -- guarded for the same reason as type()" + NL
     + TAB + "if ( typeMap.isEmpty() ) {" + NL
     + TAB + TAB + "static std::once_flag" + TAB + "aliasInitOnce;" + NL
     + TAB + TAB + "std::call_once( aliasInitOnce, []() { NifValue::initialize(); } );" + NL
     + TAB + "}" + NL),

    # ---------------------------------------------------------------- F4
    # The non-const QMap::operator[] on a process-wide table inside load().
    ("src/model/nifmodel.cpp", "replace",
     TAB + TAB + TAB + TAB + TAB + TAB + TAB + "if ( blockHashes.contains( hash ) )" + NL
     + TAB + TAB + TAB + TAB + TAB + TAB + TAB + TAB + "blktyp = blockHashes[hash]->id;" + NL,
     TAB + TAB + TAB + TAB + TAB + TAB + TAB + "// .value(), not operator[]: the non-const one detaches a" + NL
     + TAB + TAB + TAB + TAB + TAB + TAB + TAB + "// process-wide QMap from inside a per-file load (NIFPARSE1)" + NL
     + TAB + TAB + TAB + TAB + TAB + TAB + TAB + "if ( const NifBlockPtr blkForHash = blockHashes.value( hash ) )" + NL
     + TAB + TAB + TAB + TAB + TAB + TAB + TAB + TAB + "blktyp = blkForHash->id;" + NL),

    # ---------------------------------------------------------------- F5
    # R3, found by BAKEPERF1 and deliberately left unfixed there: the reference
    # BINDS to the global and the two assignments then COPY-ASSIGN INTO IT, so
    # the first multi-array row ever displayed permanently replaces the
    # application's singular-name table for the rest of the session. A pointer
    # re-points; a reference cannot. Editor path, not the bake -- but it is a
    # process-wide WRITE from a display call, so it belongs in this lane's
    # table and in this lane's fix. Its gate is loaded_nifs / the editor
    # harnesses, not the bake.
    ("src/model/nifmodel.cpp", "replace",
     T5 + "if ( p && p->isArray() && !p->isBinary() ) {" + NL
     + T6 + "QHash<QString, QString> & pseudonymMap = arrayPseudonyms;" + NL
     + T6 + "// Is it a 2nd level array of a multi-array?" + NL
     + T6 + "if ( p->isMultiArray() )" + NL
     + T7 + "pseudonymMap = multiArrayPseudonyms1;" + NL
     + T6 + "else {" + NL
     + T7 + "// Is it an item (3rd level) of a multi-array?" + NL
     + T7 + "auto pp = p->parent();" + NL
     + T7 + "if ( pp && pp->isMultiArray() )" + NL
     + T8 + "pseudonymMap = multiArrayPseudonyms2;" + NL
     + T6 + "}" + NL
     + NL
     + T6 + "return QString( namePrefix % pinMark % pseudonymMap.value( item->name(), item->name() ) % SPACE_QSTRING % QString::number( item->row() ) );" + NL,
     T5 + "if ( p && p->isArray() && !p->isBinary() ) {" + NL
     + T6 + "/* A POINTER, NOT A REFERENCE (lane NIFPARSE1, 2026-09-11; found by" + NL
     + T6 + " * BAKEPERF1 as its red R3 and left). `QHash & m = arrayPseudonyms;`" + NL
     + T6 + " * followed by `m = multiArrayPseudonyms1;` does not re-point m -- it" + NL
     + T6 + " * COPY-ASSIGNS into the global, so the first multi-array row ever" + NL
     + T6 + " * displayed replaced the application's singular-name table for the" + NL
     + T6 + " * rest of the session, and did it as a process-wide write from a" + NL
     + T6 + " * display call. */" + NL
     + T6 + "const QHash<QString, QString> * pseudonymMap = &arrayPseudonyms;" + NL
     + T6 + "// Is it a 2nd level array of a multi-array?" + NL
     + T6 + "if ( p->isMultiArray() )" + NL
     + T7 + "pseudonymMap = &multiArrayPseudonyms1;" + NL
     + T6 + "else {" + NL
     + T7 + "// Is it an item (3rd level) of a multi-array?" + NL
     + T7 + "auto pp = p->parent();" + NL
     + T7 + "if ( pp && pp->isMultiArray() )" + NL
     + T8 + "pseudonymMap = &multiArrayPseudonyms2;" + NL
     + T6 + "}" + NL
     + NL
     + T6 + "return QString( namePrefix % pinMark % pseudonymMap->value( item->name(), item->name() ) % SPACE_QSTRING % QString::number( item->row() ) );" + NL),

    # ---------------------------------------------------------------- F6
    # "Believed safe" is exactly what the slab pool's comment said. These four
    # are shared QRegularExpression instances on the array-resize path, which
    # every worker takes thousands of times. Qt compiles a pattern lazily inside
    # the shared instance behind its own mutex, so this is belt and braces --
    # and it costs one TLS slot per thread.
    ("src/xml/nifexpr.cpp", "replace",
     T3 + "static QRegularExpression reInt( \"\\\\A(?:[-+]?[0-9]+)\\\\z\" );" + NL,
     T3 + "/* thread_local, not static: shared instances on a path sixteen chunk" + NL
     + T3 + " * workers take (every array resize builds NifExprs). Qt guards its own" + NL
     + T3 + " * lazy compile; this removes the question. (lane NIFPARSE1) */" + NL
     + T3 + "thread_local QRegularExpression reInt( \"\\\\A(?:[-+]?[0-9]+)\\\\z\" );" + NL),

    ("src/xml/nifexpr.cpp", "replace",
     T3 + "static QRegularExpression reUInt( \"\\\\A(?:0[xX][0-9a-fA-F]+)\\\\z\" );" + NL,
     T3 + "thread_local QRegularExpression reUInt( \"\\\\A(?:0[xX][0-9a-fA-F]+)\\\\z\" );" + NL),

    ("src/xml/nifexpr.cpp", "replace",
     T3 + "static QRegularExpression reFloat( \"^[-+]?[0-9]*\\\\.?[0-9]+([eE][-+]?[0-9]+)?$\" );" + NL,
     T3 + "thread_local QRegularExpression reFloat( \"^[-+]?[0-9]*\\\\.?[0-9]+([eE][-+]?[0-9]+)?$\" );" + NL),

    ("src/xml/nifexpr.cpp", "replace",
     T3 + "static QRegularExpression reVersion( \"\\\\A(?:[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\\.[0-9]+)\\\\z\" );" + NL,
     T3 + "thread_local QRegularExpression reVersion( \"\\\\A(?:[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\\.[0-9]+)\\\\z\" );" + NL),

    # ---------------------------------------------------------------- F7
    # THE MEMORY CAP ON THE CHUNK FAN-OUT'S DEFAULT (brief item 4).
    # Each chunk worker owns its own EsmWorld and its own texture cache, so N
    # workers is N x (world + budget) of FIXED overhead before a single chunk's
    # own state. BAKEPERF1 measured 21.9 GB at sixteen workers on twenty-five
    # chunks -- 70 percent of this machine. "The machine's core count" is
    # therefore the wrong default even once the gate is green; the default is
    # the smaller of the cores and what free memory can hold, with a reserve.
    # The per-worker figure is a MEASUREMENT, not a guess: it is set from this
    # lane's own 100-chunk run before the default is flipped off 1.
    ("src/lodgenparallel.h", "after",
     "void lodgenSetChunkThreadCount( int n );" + NL
     + "int lodgenChunkThreadCount();" + NL,
     NL
     + "/*! WHAT THE MACHINE'S MEMORY CAN HOLD, in chunk workers." + NL
     + " *" + NL
     + " *  Each chunk worker owns its OWN plugin reader and its OWN texture cache," + NL
     + " *  so N workers is N x (world + budget) of fixed overhead before one chunk" + NL
     + " *  of state. `lodgenChunkThreadCount()` never returns more than this." + NL
     + " *" + NL
     + " *  `texBudgetBytes` is the per-worker texture budget the pass will use." + NL
     + " *  The world figure is a measured constant, not an estimate; the census" + NL
     + " *  line prints the cap and what bound it, so a run that was held back by" + NL
     + " *  memory rather than by cores says so in words." + NL
     + " */" + NL
     + "int lodgenChunkThreadMemoryCap( qint64 texBudgetBytes );" + NL
     + NL
     + "//! Which bound decided the worker count: \"cores\", \"memory\" or \"asked\"." + NL
     + "QString lodgenChunkThreadBoundBy();" + NL),

    # F7's definition, so the declaration never lands without it.
    ("src/lodgenparallel.cpp", "replace",
     "int lodgenChunkThreadCount()" + NL
     + "{" + NL
     + T1 + "int n = g_chunkThreads;" + NL
     + T1 + "if ( n <= 0 )" + NL
     + T2 + "n = QThread::idealThreadCount();" + NL
     + T1 + "return qMax( 1, n );" + NL
     + "}" + NL,
     "//! The per-worker plugin reader, MEASURED: a load-plus-one-chunk run peaks" + NL
     + "//! at 336 MB and the marginal cost of a second world is ~250 MB (BAKEPERF1," + NL
     + "//! 2026-09-11 section 3.10). Not an estimate, and not a round number chosen" + NL
     + "//! to make the arithmetic come out." + NL
     + "static const qint64 kWorkerWorldBytes = qint64( 250 ) << 20;" + NL
     + NL
     + "//! Which bound decided the count, for the census line." + NL
     + "static QString g_chunkBoundBy = QStringLiteral( \"cores\" );" + NL
     + NL
     + "int lodgenChunkThreadMemoryCap( qint64 texBudgetBytes )" + NL
     + "{" + NL
     + T1 + "const qint64 perWorker = kWorkerWorldBytes + qMax( qint64( 64 ) << 20, texBudgetBytes );" + NL
     + "#ifdef Q_OS_WIN32" + NL
     + T1 + "MEMORYSTATUSEX" + T1 + "ms;" + NL
     + T1 + "memset( &ms, 0, sizeof( ms ) );" + NL
     + T1 + "ms.dwLength = sizeof( ms );" + NL
     + T1 + "if ( GlobalMemoryStatusEx( &ms ) ) {" + NL
     + T2 + "/* 60 percent of what is FREE, not of what is installed: the machine has" + NL
     + T2 + " * a desktop and a browser on it, and the post-queue passes (the atlas," + NL
     + T2 + " * the arrays, the merge) still have to fit after the workers retire. */" + NL
     + T2 + "const qint64 usable = qint64( double( ms.ullAvailPhys ) * 0.60 );" + NL
     + T2 + "return qMax( 1, int( usable / perWorker ) );" + NL
     + T1 + "}" + NL
     + "#else" + NL
     + T1 + "Q_UNUSED( perWorker )" + NL
     + "#endif" + NL
     + T1 + "// no answer from the platform: do not invent one, do not cap" + NL
     + T1 + "return QThread::idealThreadCount();" + NL
     + "}" + NL
     + NL
     + "QString lodgenChunkThreadBoundBy()" + NL
     + "{" + NL
     + T1 + "return g_chunkBoundBy;" + NL
     + "}" + NL
     + NL
     + "int lodgenChunkThreadCount()" + NL
     + "{" + NL
     + T1 + "int n = g_chunkThreads;" + NL
     + T1 + "if ( n > 0 ) {" + NL
     + T2 + "g_chunkBoundBy = QStringLiteral( \"asked\" );" + NL
     + T2 + "return n;" + NL
     + T1 + "}" + NL
     + T1 + "/* 0 or negative used to mean \"the machine\". It cannot: each chunk worker" + NL
     + T1 + " * owns its own plugin reader AND its own texture cache, so sixteen of" + NL
     + T1 + " * them is ~12 GB of FIXED overhead before one chunk of state -- 21.9 GB" + NL
     + T1 + " * measured on twenty-five chunks, 70 percent of this machine. The count" + NL
     + T1 + " * is the smaller of the cores and what free memory can hold, and the" + NL
     + T1 + " * census says WHICH bound it was, so a run held back by memory reads as" + NL
     + T1 + " * held back by memory rather than as mysteriously slow." + NL
     + T1 + " * (lane NIFPARSE1, 2026-09-11.) */" + NL
     + T1 + "const int cores = QThread::idealThreadCount();" + NL
     + T1 + "const int cap = lodgenChunkThreadMemoryCap( g_chunkTexBudgetBytes );" + NL
     + T1 + "g_chunkBoundBy = ( cap < cores ) ? QStringLiteral( \"memory\" ) : QStringLiteral( \"cores\" );" + NL
     + T1 + "return qMax( 1, qMin( cores, cap ) );" + NL
     + "}" + NL),

    # the budget the cap is computed against, set by the chunk pass
    ("src/lodgenparallel.cpp", "replace",
     "int g_chunkThreads = 1;                     //!< ONE until the parser is safe" + NL,
     "int g_chunkThreads = 1;                     //!< ONE until the parser is safe" + NL
     + "qint64 g_chunkTexBudgetBytes = qint64( 512 ) << 20;   //!< what the cap is computed against" + NL),
]


def main():
    apply = "--apply" in sys.argv
    files = {}
    ok = True
    for path, mode, anchor, text in EDITS:
        full = os.path.join(ROOT, path)
        if path not in files:
            with open(full, "rb") as f:
                files[path] = f.read()
        b = files[path]
        a = anchor.encode("utf-8")
        n = b.count(a)
        first = anchor.strip().split(NL)[0][:62]
        print("%-24s %-7s count=%d  %s" % (path, mode, n, first))
        if n != 1:
            ok = False
            print("   REFUSE: anchor must match exactly once, matched %d" % n)
            continue
        t = text.encode("utf-8")
        files[path] = b.replace(a, (a + t) if mode == "after" else t, 1)

    for path in files:
        with open(os.path.join(ROOT, path), "rb") as f:
            before = f.read()
        cr_b, cr_a = before.count(b"\r"), files[path].count(b"\r")
        print("%-24s CR before=%d after=%d" % (path, cr_b, cr_a))
        if cr_b != cr_a:
            ok = False
            print("   REFUSE: line endings changed")

    if not ok:
        print(NL + "RESULT REFUSED - nothing written")
        return 2
    if not apply:
        print(NL + "RESULT CHECK OK - %d edits, all anchors matched once, nothing written" % len(EDITS))
        return 0
    for path, data in files.items():
        with open(os.path.join(ROOT, path), "wb") as f:
            f.write(data)
        print("wrote " + path)
    print(NL + "RESULT APPLIED - %d edits" % len(EDITS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
