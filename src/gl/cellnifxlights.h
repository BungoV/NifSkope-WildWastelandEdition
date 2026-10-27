#ifndef WW_CELLNIFXLIGHTS_H
#define WW_CELLNIFXLIGHTS_H

/* lane NIFXLIGHT1: a fixture mesh's .nifx `lights` turned into cell lights (FO4-Extension-Formats nifx/SPEC.md,
 * "lights"). Plain QtCore, so tests/nifxapply drives it without a window. The reader is src/io/nifxlights. */

#include "celllights.h"
#include "io/nifxlights.h"

#include <QByteArray>
#include <QHash>
#include <QStringList>

#include <functional>
#include <vector>

//! test reds (WW_NIFX_LIGHTS_RED, comma list): each must break a gate
enum WwNifxRed
{
	WwNifxRedIgnore = 1,		//!< "ignore": the reader ignores `lights`
	WwNifxRedTintDims = 2,		//!< "tintdims": the fade skips its / Y, so a tint dims the light
	WwNifxRedKeepPlaced = 4,	//!< "keepplaced": the placed light inside a fixture stays on
	WwNifxRedPrecedence = 8,	//!< "precedence": the light record's fog.scale wins over the .nifx light's
	// lane NIFXLIGHT2
	WwNifxRedNoScale = 16,		//!< "noscale": the radius ignores the fixture's placed scale
	WwNifxRedNoFollow = 32,		//!< "nofollow": follows and NiLight edits ignore the NiLight's dimmer
	WwNifxRedBulbSurface = 64,	//!< "bulbsurface": a bulb-tier light is drawn on surfaces like a normal one
	WwNifxRedBulbLit = 128,		//!< "bulblit": a bulb that took a start-off light stays lit
	WwNifxRedNoRelight = 256,	//!< "norelight": a Mesh lights change does not relight the GI (src/probesky.cpp)
	WwNifxRedNoBulbGi = 512,	//!< "nobulbgi": the GI relight leaves the bulbs out
	// lane NIFXLIGHT3
	WwNifxRedStaticDim = 1024,	//!< "staticdim": a light keeps its still dimmer while the World animation row plays
	WwNifxRedNoBulbFlicker = 2048,	//!< "nobulbflicker": bulb-tier lights never flicker
	WwNifxRedNoLiveNew = 4096,	//!< "nolivenew": a .nifx made (or removed) after the build is not seen live
	WwNifxRedNoOffKept = 8192,	//!< "nooffkept": start-off placed lights kept only when a .nifx was found at the build
	// lane NIFXCAL1
	WwNifxRedAnchor800 = 16384,	//!< "anchor800": 800 lm is fade 1.0 again (the guess before the census)
	WwNifxRedRadius1Lux = 32768,	//!< "radius1lux": the radius law reaches 1 lux again (69.99 per sqrt candela)
	// lane NIFXFIX2
	WwNifxRedNoFollowDefault = 65536,	//!< "nofollowdefault": a light naming no NiLight ignores the mesh's only animated one
	WwNifxRedDarkRest = 131072	//!< "darkrest": a NiLight dark when still stays dark although a track plays it
};
int wwNifxRedOf( const QByteArray & words );
//! lane NIFXLIGHT2: WW_NIFX_LIGHTS_RED as bits, read live (src/gl/celllights.cpp)
int wwNifxLightsRed();

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

/*! lane NIFXLIGHT2: the lights the GI relight traces: the drawn lights, then the bulb-tier lights (a bulb lights the
 *  bounce and the fog only). False (no bulbs, or the red nobulbgi): the caller traces L as it is. */
bool wwNifxGiLights( const WwCellLighting & L, int red, WwCellLighting & out );

/* ---- lane NIFXLIGHT3 ---- */
//! a model's .nifx path under a root (Meshes\ added when the model path lacks it; empty unless it is a .nif)
QString wwNifxSidecarRel( const QString & model );
//! the first root (in order, empty ones skipped) holding the model's .nifx, cleaned; empty when none does
QString wwNifxSidecarFind( const QStringList & roots, const QString & model );
//! the build keeps the placed lights that start off for a later .nifx light to take (red nooffkept: only with a .nifx)
bool wwNifxKeepStartOff( const QVector<WwNifxFixture> & fixtures, int red );

//! the dimmer track's value at the clock's seconds (no keys: 1)
float wwNifxDimAt( const WwNifxDimTrack & k, double seconds );
//! the factor on a light's colour at the clock's seconds: its track over the dimmer the colour holds (1: no track)
float wwNifxDimFactor( const WwCellLight & l, double seconds );
/*! the lights (index i) and bulb-tier lights (index lights + j) a frame changes: flickering ones when `flicker` (the
 *  bulbs only with `bulbFlicker`) and those with a dimmer track when `dim`; *flickN and *dimN count each kind */
std::vector<int> wwNifxFrameList( const WwCellLighting & L, bool flicker, bool bulbFlicker, bool dim, int * flickN, int * dimN );

/*! A new .nifx seen without a reopen: the folders the placed models' .nifx would sit in, under each root, looked at in
 *  turn (a folder's time moves when a file is made or removed in it); a model whose .nifx appeared, moved or went is
 *  loaded again (or dropped) and its fixtures updated. */
struct WwNifxLive
{
	struct Dir
	{
		QString rel;
		QStringList models;		//!< lower-case model paths
		QVector<qint64> stamp;	//!< per root: -2 not looked at, -1 missing, else the folder's time
	};
	QStringList roots;
	QVector<Dir> dirs;
	QHash<QString, int> dirOf;
	QHash<QString, std::shared_ptr<const WwNifxModel>> models;	//!< lower-case model -> as resolved (null: no .nifx)
	QHash<QString, std::shared_ptr<const WwNifxModel>> loaded;	//!< its nodes once read (any sidecar)
	QHash<QString, QString> pathOf;	//!< lower-case model -> as placed
	QString dataRoot;
	int cursor = 0;
	qint64 looks = 0, found = 0, lost = 0;
};
using WwNifxLoad = std::function<std::shared_ptr<const WwNifxModel>( const QString & dataRoot, const QString & model, const QString & sidecar )>;
//! the fixtures as published: known models take the last resolution, the rest as built
void wwNifxLiveInit( WwNifxLive & live, QVector<WwNifxFixture> & fixtures );
//! looks at folders for up to budgetMs (at least one; all when the roots changed); returns the models whose .nifx changed
int wwNifxLiveScan( WwNifxLive & live, QVector<WwNifxFixture> & fixtures, const QStringList & roots, double budgetMs, const WwNifxLoad & load );
//! defined in src/cellview.cpp: a model's nodes, NiLight dimmers and dimmer tracks, with this .nifx
std::shared_ptr<const WwNifxModel> wwCellNifxModelLoad( const QString & dataRoot, const QString & model, const QString & sidecar );

#endif // WW_CELLNIFXLIGHTS_H
