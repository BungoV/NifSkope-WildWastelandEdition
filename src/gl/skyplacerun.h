#ifndef SKYPLACERUN_H
#define SKYPLACERUN_H

/* lane INTSKY1: the sun's place for the open cell (gl/skyplace.h has the rule). The cell view hands over its load
 * order and worldspace (or interior) at the open; the ephemeris (shadowset.cpp) asks wwSkyPlaceNow() per use, which
 * re-resolves when a Climates .recx file changed (at most every half second) or WW_LOCATION_RED moved.
 * No cell open (a single NIF) = Boston, as before. Census on stderr once per change:
 *   sky place: <world> (<why>) -> lat .. lon .. <clock> start YYYY-MM-DD by <rule> (<name>) climate <EDID> */

#include "gl/skyplace.h"

#include <QString>
#include <QStringList>

class ESMFile;

//! the worldspaces of a load order (every WRLD of the opened plugins), with their climate and location fields
std::vector<wwplace::World> wwSkyPlaceWorlds( ESMFile & esm, const QStringList & plugins );

//! the location chain of a CELL (XLCN, then each LCTN PNAM up), load-order form ids
std::vector<std::uint32_t> wwSkyPlaceLocationChain( ESMFile & esm, quint32 cellForm );

/*! at a cell open: the load order (paths), the opened worldspace's form (0 for an interior) or the interior CELL's form.
 *  The worlds are read here once (a few ms); the resolve runs again on a .recx or red change. */
void wwSkyPlaceSetCell( ESMFile * esm, const QStringList & plugins, const QString & dataDir, quint32 worldForm,
	quint32 interiorCellForm, const QString & interiorPlugin );

//! the place the sun and moon are computed for now (Boston with no cell)
const wwplace::Place & wwSkyPlaceNow();

//! the census line of the last resolve
QString wwSkyPlaceSummary();

//! WW_LOCATION_RED: table | noparent | norecx (comma list, live) -> wwplace::Red bits
int wwSkyPlaceRed();

//! `-no-gui skyplace --plugins <a,b,...> [--data <Data>] [--interior <EDID>...]`: every worldspace's resolve (+ interiors)
int wwSkyPlaceCli( const QStringList & args );

#endif // SKYPLACERUN_H
