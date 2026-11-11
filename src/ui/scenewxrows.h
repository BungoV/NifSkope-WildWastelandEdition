#ifndef SCENEWXROWS_H
#define SCENEWXROWS_H

/* lane MODULES1 (items 4 + 12, 11-09): the Scene window's Weather Overrides rows and its row layout. */

#include <QSize>
#include <QString>

#include <functional>

class QLabel;
class QWidget;

//! One row per weather override (WW_WX_*), grouped under headings, plus Reset all and Save as .recx
void wwSceneAddWeatherOverrides( const std::function<void( const QString & )> & heading,
	const std::function<void( const QString &, QWidget * )> & row, QWidget * tree );

/* MODULES1 R6-11: the Scene window's rows on the panel standard (skill nifskope-ww-panel-style): a scroll area
 * ("SceneRows") holding one label | field grid. Returns the rows' body, the parent of every field. */
QWidget * wwSceneRowsCreate( QWidget * window );
//! the scroll area of a rows body (what the window lays out)
QWidget * wwSceneRowsArea( QWidget * body );
//! a group heading (wwHeading) across both columns
void wwSceneRowsHeading( QWidget * body, const QString & text );
//! one setting: its name (greys with the field, buddy = the field, takes the field's tooltip) and its field
QLabel * wwSceneRowsAdd( QWidget * body, const QString & name, QWidget * field, bool enabled );
//! a one-line text that is elided to its width; the whole text is its tooltip (set it with wwSceneElideSet)
QLabel * wwSceneElideLabel( QWidget * parent );
void wwSceneElideSet( QLabel * label, const QString & full );
/* One row height, selectors never narrower than their longest entry, the name column as wide as the longest name.
 * Red WW_SCENE_LAYOUT_RED=1: no tidy pass. Prints "scene window: panel standard rows: ...". */
void wwSceneTidyRows( QWidget * body );
/* The geometry probe's stderr line "scene window rows: ..." (five checks: rows inside the body, names cut,
 * selectors cut, rows overlapping, one row height). WW_SCENE_ROWS_PROBE=1 runs it at 400, 800 and 1152 px. */
void wwSceneRowsEcho( QWidget * body );
//! The window size that shows every row's name and whole value (an invalid size with the red)
QSize wwSceneWantedSize( QWidget * body, const QWidget * window );

#endif
