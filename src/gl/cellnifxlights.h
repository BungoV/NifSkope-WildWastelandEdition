#ifndef WW_CELLNIFXLIGHTS_H
#define WW_CELLNIFXLIGHTS_H

/* lane NIFXLIGHT1: a fixture mesh's .nifx `lights` turned into cell lights (FO4-Extension-Formats nifx/SPEC.md,
 * "lights"). Plain QtCore, so tests/nifxapply drives it without a window. The reader is src/io/nifxlights. */

#include "celllights.h"
#include "io/nifxlights.h"

#include <QByteArray>
#include <QHash>
#include <QStringList>

//! test reds (WW_NIFX_LIGHTS_RED, comma list): each must break a gate
enum WwNifxRed
{
	WwNifxRedIgnore = 1,		//!< "ignore": the reader ignores `lights`
	WwNifxRedTintDims = 2,		//!< "tintdims": the fade skips its / Y, so a tint dims the light
	WwNifxRedKeepPlaced = 4,	//!< "keepplaced": the placed light inside a fixture stays on
	WwNifxRedPrecedence = 8		//!< "precedence": the light record's fog.scale wins over the .nifx light's
};
int wwNifxRedOf( const QByteArray & words );

//! a light's fog.scale rung (nifx/SPEC.md): the placed lamp's .recx reference, the .nifx light, the light record, else 1
enum WwNifxFogRungs { WwNifxFogRef, WwNifxFogMesh, WwNifxFogRecord, WwNifxFogNone };
template <class Ref, class Record>
int wwNifxFogRung( Ref refClaim, bool own, Record recordClaim, bool redPrecedence )
{
	if ( refClaim() )
		return WwNifxFogRef;
	if ( own && !redPrecedence )
		return WwNifxFogMesh;
	if ( recordClaim() )
		return WwNifxFogRecord;
	return own ? WwNifxFogMesh : WwNifxFogNone;
}

//! the placed-light reach (spec: 64 units)
constexpr float kWwNifxReach = 64.0f;

/*! Edits L (a published cell's lights) in place: every fixture's lights from docs (keyed by WwNifxModel::sidecar)
 *  are converted and added (bulb tier to nifxBulbs), placed lights within reach are switched off and their state
 *  carried over. One census line per mesh, then the reader's warnings once per file, go to census. */
void wwNifxLightsApply( WwCellLighting & L, const QHash<QString, nifxl::Lights> & docs, int red, QStringList * census );

//! one line per drawn, off and bulb .nifx light (the WW_NIFX_LIGHTS_DUMP file and the unit gate read it)
QString wwNifxLightsDump( const WwCellLighting & L );

#endif // WW_CELLNIFXLIGHTS_H
