/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

See the accompanying LICENSE file for the full text.

***** END LICENSE BLOCK *****/

#ifndef WW_HARNESSWINDOW_H
#define WW_HARNESSWINDOW_H

#include <QSize>
#include <QString>

class QObject;
class QWidget;

/*! A harness run's window is FORCED, never inherited.
 *
 *  Lane HARNESSWIN1, 2026-09-19. The whole of the interface; the reasons are in
 *  src/harnesswindow.cpp.
 */

//! " <name>" when WW_SETTINGS_SCOPE names a usable scope, else empty. Appended
//! to applicationName in main.cpp so a harness's whole QSettings tree lands in
//! a key of its own and the user's can never be reached. Empty for an ordinary
//! session, so nothing about normal use moves.
QString wwHarnessSettingsSuffix();

//! True when this process is a harness run (NifSkope::wwHeadlessRun()).
bool wwHarnessRun();

//! True when restoreUi() must NOT replay the persisted window geometry.
//! Same predicate as the one saveUi() already uses to refuse to persist, so the
//! read side and the write side cannot disagree.
bool wwHarnessGeometryRestoreSuppressed();

//! The window size a harness asked for: WW_WINDOW_SIZE=WxH, else
//! WW_RENDER_SIZE=WxH, else wwHarnessDefaultWindowSize().
QSize wwHarnessAskedWindowSize();

//! True when WW_WINDOW_SIZE or WW_RENDER_SIZE named a size, so a refusal can
//! say "you asked" rather than "the default was".
bool wwHarnessWindowSizeWasAsked();

//! The largest window that sits WHOLLY on the screen the harness window is
//! placed on, measured from that screen and from \a w's own frame margins.
//! \a w may be null, in which case a conservative fallback is used.
QSize wwHarnessDefaultWindowSize( const QWidget * w = nullptr );

//! Clear every maximized/fullscreen/minimized bit BY HAND, show normal, and
//! resize to wwHarnessAskedWindowSize(). Does nothing outside a harness run.
void wwApplyHarnessWindow( QWidget * w );

//! Record what the window and the viewport actually came out at. Writes one
//! line to release/ww_harness_window.log and caches it for
//! wwHarnessWindowLine(). \a viewport may be null.
/* viewport is a QObject, not a QWidget, on purpose: the thing being measured
 * is NifSkope::getGLView(), and GLView is a QOpenGLWindow (src/glview.h:59) --
 * a QWindow, not a widget. Taking QObject lets the one function measure either
 * kind, and lets a caller pass it without a cast that would not compile. */
void wwHarnessRecordViewport( QWidget * window, QObject * viewport, const char * when );

//! The one line every harness log can quote, RECOMPUTED at the instant it is
//! asked rather than cached -- the WW_RENDER_SHOT path resizes the window on a
//! 2500 ms timer, so a line built at show time records the wrong moment.
//! \a viewport is optional; pass skope->getGLView() to have the viewport size
//! in the line. Empty outside a harness run.
QString wwHarnessWindowLine( QObject * viewport = nullptr );

//! Empty when \a got is exactly \a asked; otherwise the REFUSED sentence, with
//! BOTH numbers in it. A gate that gets a non-empty string writes it and
//! refuses instead of measuring.
QString wwHarnessSizeRefusal( const QSize & asked, const QSize & got );

//! The same refusal for the WINDOW size this run asked for against the window
//! size it obtained. Empty outside a harness run, and empty before the window
//! has been recorded.
QString wwHarnessSizeRefusal();

#endif // WW_HARNESSWINDOW_H
