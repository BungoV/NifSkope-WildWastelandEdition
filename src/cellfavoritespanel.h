/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLFAVORITESPANEL_H
#define CELLFAVORITESPANEL_H

#include "cellfavorites.h"

#include <QWidget>

class GLView;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/*! lane FAVCAM1: the Cell dock's Favorites tab. Save camera adds the view's spot and opens its name for typing;
 *  a click jumps there; F2 or Rename names it again; Del or Delete removes it. The start (the last one saved or
 *  jumped to) is marked: a first open of these cells stands there. */
class CellFavoritesPanel final : public QWidget
{
	Q_OBJECT

public:
	explicit CellFavoritesPanel( QWidget * parent = nullptr );
	void setGLView( GLView * v ) { glView = v; }

	// ---- the harness and the menu
	int count() const;
	QString nameAt( int i ) const;
	int startRow() const;
	//! Save the camera now as `name` (empty = "Favorite N" and the name opens for typing). The row, or -1.
	int saveCamera( const QString & name = QString() );
	bool jumpTo( int row );
	bool renameRow( int row, const QString & name );
	bool deleteRow( int row );

public slots:
	//! A cell opened or closed: read the file again for its worldspace.
	void refresh();

protected:
	bool eventFilter( QObject * o, QEvent * e ) override;

private:
	void write();
	void fill();
	int rowOf( QTreeWidgetItem * it ) const;
	void showMenu( const QPoint & at );

	GLView * glView = nullptr;
	QTreeWidget * list = nullptr;
	QPushButton * saveButton = nullptr;
	QLabel * note = nullptr;
	CellFavoriteStore store;
	QString key;
	bool interior = false;
	int cx = 0, cy = 0, n = 1;
	bool filling = false;
};

#endif // CELLFAVORITESPANEL_H
