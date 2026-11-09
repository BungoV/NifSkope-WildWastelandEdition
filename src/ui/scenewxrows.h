#ifndef SCENEWXROWS_H
#define SCENEWXROWS_H

/* lane MODULES1 (items 4 + 12, 11-09): the Scene window's Weather Overrides rows and its row layout. */

#include <QSize>
#include <QString>

#include <functional>

class QTreeWidget;
class QWidget;

//! One row per weather override (WW_WX_*), grouped under headings, plus Reset all and Save as .recx
void wwSceneAddWeatherOverrides( const std::function<void( const QString & )> & heading,
	const std::function<void( const QString &, QWidget * )> & row, QTreeWidget * tree );

/* Item 4 (bungo: the value widgets sit shifted against their names, combo text cut, the window opens too small):
 * every row gets the height of a styled field (uniform rows took the first item's text height, so taller widgets
 * slid over the next row), combos size to their contents. Red WW_SCENE_LAYOUT_RED=1: as before. */
void wwSceneTidyRows( QTreeWidget * tree );
//! The window size that shows every row's name and whole value (an invalid size with the red)
QSize wwSceneWantedSize( QTreeWidget * tree, const QWidget * window );

#endif
