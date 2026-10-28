/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLFAVORITES_H
#define CELLFAVORITES_H

#include <QHash>
#include <QString>
#include <QVector>

/* lane FAVCAM1 (bungo 10-28): favorite camera spots in the cell view.
 * One JSON file (cellFavoritesPath) holds every worldspace's list; an interior cell is its own list.
 * A cell's first open stands the camera at the favorite last jumped to or saved there, when that spot lies in the
 * opened cells; else the spawn rule (src/cellspawn.cpp). WW_CELL_SPAWN_EYE and a pinned harness camera still win. */
struct CellFavorite
{
	QString name;
	float eye[3] = { 0, 0, 0 };	//!< world units
	float yawDeg = 0.0f;		//!< GLView Rot z: 0 looks north (+y), as a spawn's yaw
	float pitchDeg = 0.0f;		//!< GLView Rot x + 90: 0 is level
	float rollDeg = 0.0f;		//!< GLView Rot y
	float fovDeg = 0.0f;		//!< vertical field of view, degrees; 0 = keep the current one
	float dist = 64.0f;			//!< the orbit pivot's distance ahead of the eye (half of it, as GLView::Dist)
	float at[3] = { 0, 0, 0 };	//!< where it points: a spot 512 units ahead (written for reading; the jump uses the angles)
	qint64 used = 0;			//!< the store's counter when it was last saved or jumped to: the highest is the start
};

struct CellFavoriteStore
{
	QHash<QString, QVector<CellFavorite>> lists;	//!< by cellFavoritesKey
	qint64 counter = 0;
};

//! WW_CELL_FAVORITES=<file> when set, else camera_favorites.json in the app's data folder (follows WW_SETTINGS_SCOPE).
QString cellFavoritesPath();
//! "world/<edid lower>" for an exterior, "interior/<cell lower>" for an interior.
QString cellFavoritesKey( bool interior, const QString & world, const QString & interiorCell );

//! A missing file reads as an empty store and returns true; a broken one returns false with err.
bool cellFavoritesRead( const QString & path, CellFavoriteStore & out, QString * err = nullptr );
bool cellFavoritesWrite( const QString & path, const CellFavoriteStore & store, QString * err = nullptr );

//! The marked start of a list: the highest `used`, or -1 for an empty list.
int cellFavoritesStart( const QVector<CellFavorite> & list );
/*! The favorite a cell's open stands at: the highest `used` whose eye lies in the opened block (cells cx +- n/2,
 *  cy +- n/2; an interior takes any). -1 = none, the spawn rule applies. WW_CELL_FAVORITES_RED=range ignores the range. */
int cellFavoritesPick( const QVector<CellFavorite> & list, bool interior, int cx, int cy, int n );

//! Stamp `f.used` with the next counter value (saved or jumped to: it becomes the start).
void cellFavoritesTouch( CellFavoriteStore & store, CellFavorite & f );
//! "Favorite N", the first N not taken in the list.
QString cellFavoritesNewName( const QVector<CellFavorite> & list );

#endif // CELLFAVORITES_H
