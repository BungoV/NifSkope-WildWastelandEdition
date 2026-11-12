#ifndef LIVEPANEL_H
#define LIVEPANEL_H

/* lane MODULES1 (items 12 + 13, 11-09): ONE EDITOR FOR ONE WW_ SWITCH, AND THE LIVE PANEL BUILT FROM A JSON FILE.
 * bungo 11-09: "during live nifskope, I need a tool that you can create on the fly, with sliders or toggles or
 * dropdown selection that I need, of anything in nifskope".
 * A row shows the switch's value (or `fallback()` while it is unset), sets it on change and has a reset (unset). A
 * stage-0 switch applies at once; a reopen / shaders stage switch asks first, then runs `set` + `apply` through the
 * live port's runner; a relaunch-stage switch is shown greyed (a live set would be refused).
 * Panel file: {"title": "...", "rows": [ {"heading": "..."}, {"label", "switch": "WW_...", "type":
 * "slider|toggle|dropdown|number|color", "min", "max", "step", "options": [...]} ]} (a bare array works too). */

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

class QMainWindow;
class QWidget;

struct WwSwitchRowSpec
{
	QByteArray sw;
	QString type = QStringLiteral( "number" );	// slider | toggle | dropdown | number | color
	double min = 0.0, max = 1.0, step = 0.01;
	QStringList options;
	std::function<QString()> fallback;	// the value shown while unset (the weather's own value); empty = blank
};

//! The editor widget for one switch (fields + Reset)
QWidget * wwSwitchRowWidget( const WwSwitchRowSpec & spec, QWidget * parent );

//! Sets one switch from the UI, by its stage (see above); the reply as the live port gives it
QString wwSwitchRowSet( const QByteArray & sw, const QString & value, bool unset, QWidget * asker );

//! The live port's step runner (wwLiveCommand) and a repaint of the view; set once by the main window
void wwLivePanelSetHooks( std::function<QString( const QString & )> runner, std::function<void()> repaint );

//! Shows the Live Panel dock on `mw` built from `path` (empty: WW_LIVEPANEL); it rebuilds when the file changes
QString wwLivePanelOpen( QMainWindow * mw, const QString & path );

#endif
