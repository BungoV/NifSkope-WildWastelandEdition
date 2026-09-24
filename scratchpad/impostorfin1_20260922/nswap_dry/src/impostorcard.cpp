/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostorcard.h"

#include "io/lodmfile.h"
#include "impostoroct.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTextStream>

/* ---------------------------------------------------------------------------
 * The `.lodm` -> drawable set step. Everything here is reading and refusing;
 * there is no geometry in this file (that is `impostoroct.cpp`, which has no
 * Qt in it so a lane without the build slot can prove it) and no GL (that is
 * `gl/impostordraw.cpp`).
 * ------------------------------------------------------------------------- */

namespace {

//! A `.lodm` names textures as game paths with backslashes. This is the LAST
//! component of such a path, which is what a loose sibling would be called.
QString baseNameOf( const QString & gamePath )
{
	QString p = gamePath;
	p.replace( '\\', '/' );
	const int slash = p.lastIndexOf( '/' );
	return ( slash >= 0 ) ? p.mid( slash + 1 ) : p;
}

/*! Find the sheet beside the `.lodm`, if it is there.
 *
 *  Windows does not care about case and MSYS2 sometimes does, and the bake
 *  writes `.DDS` while half the corpus says `.dds`, so the four spellings are
 *  tried rather than assumed. Returns empty when nothing is on disk -- which
 *  is NOT an error: the set may well be installed, and then the game path is
 *  what the resource stack wants.
 */
QString findLocal( const QDir & dir, const QString & gamePath )
{
	const QString base = baseNameOf( gamePath );
	if ( base.isEmpty() )
		return QString();

	QString stem = base;
	const int dot = stem.lastIndexOf( '.' );
	if ( dot > 0 )
		stem = stem.left( dot );

	const QString candidates[] = { base, base.toLower(), stem + ".dds", stem + ".DDS" };
	for ( const QString & c : candidates ) {
		const QString full = dir.filePath( c );
		if ( QFileInfo::exists( full ) )
			return QDir::toNativeSeparators( full );
	}
	return QString();
}

/*! Read the set's coverage contract, whole or not at all.
 *
 *  `floor`, `test` and `base` only mean anything together (they are three
 *  points of one remap), so a block missing any of them, or one whose numbers
 *  are not ordered floor < test < base <= 255, is left at zero and the drawer
 *  takes the older contract where the alpha IS the fraction. Half a contract is
 *  never guessed at: a wrong decode would silently change the silhouette the
 *  bake measured `half` and every `frameOffset` against.
 */
void readCoverage( const QJsonObject & o, ImpostorCardSet & set )
{
	const QJsonObject cov = o.value( "coverage" ).toObject();
	if ( cov.isEmpty() )
		return;
	const int fl = cov.value( "floor" ).toInt( 0 );
	const int te = cov.value( "test" ).toInt( 0 );
	const int ba = cov.value( "base" ).toInt( 0 );
	if ( fl > 0 && te > fl && ba > te && ba <= 255 ) {
		set.covFloor = fl;
		set.covTest  = te;
		set.covBase  = ba;
	}
}

//! Fill one sheet: what the material named, what is on disk, which one wins.
//! The loose sibling WINS when it exists. A person who has just baked a set
//! into a folder and opened it there means that folder's sheets, not whatever
//! an older copy of the same name in the installed data happens to hold.
void fillSheet( ImpostorSheet & sheet, const QDir & dir, const QString & gamePath )
{
	sheet.gamePath = gamePath;
	if ( gamePath.isEmpty() )
		return;
	sheet.localPath = findLocal( dir, gamePath );
	if ( !sheet.localPath.isEmpty() ) {
		sheet.resolved = sheet.localPath;
		sheet.fromLocal = true;
	} else {
		// Handed to the resource stack as named. Whether IT has the file is
		// not knowable from here without dragging the whole game-data
		// machinery into a file the offline gate compiles; the drawer reports
		// the bind failure, and `notes()` says the sheet was not found loose.
		sheet.resolved = gamePath;
		sheet.fromLocal = false;
	}
}

//! A JSON array of n numbers into a float array. False when it is not that.
bool readFloats( const QJsonValue & v, float * out, int n )
{
	if ( !v.isArray() )
		return false;
	const QJsonArray a = v.toArray();
	if ( a.size() < n )
		return false;
	for ( int i = 0; i < n; i++ ) {
		if ( !a.at( i ).isDouble() )
			return false;
		out[i] = float( a.at( i ).toDouble() );
	}
	return true;
}

/*! `frameOffset` into the set: 2 floats per frame, in the frames' own sheet
 *  order. See `ImpostorCardSet::frameOffset` for what they mean and what
 *  ignoring them does to a blended card.
 *
 *  ABSENCE IS NOT AN ERROR, AND NEITHER IS A SHORT ARRAY. A set baked before
 *  per-frame positioning carries no key at all, and zero offsets are exactly
 *  the law it was baked under, so there is nothing here to refuse. What is
 *  DROPPED is a half-length or non-numeric array: taking the pairs it does
 *  have would silently mix the two laws inside one sheet, and a card wrong
 *  that way is wrong in a manner no reader could attribute. All or none.
 */
void readFrameOffsets( const QJsonObject & o, ImpostorCardSet & set )
{
	set.frameOffset.clear();
	const QJsonValue v = o.value( QStringLiteral( "frameOffset" ) );
	if ( !v.isArray() || set.oct <= 0 )
		return;
	const QJsonArray a = v.toArray();
	const int want = 2 * set.oct * set.oct;
	if ( a.size() < want )
		return;
	QVector<float> out;
	out.reserve( want );
	for ( int i = 0; i < want; i++ ) {
		if ( !a.at( i ).isDouble() )
			return;
		out << float( a.at( i ).toDouble() );
	}
	set.frameOffset = out;
}

QString sheetNote( const char * label, const ImpostorSheet & s )
{
	if ( s.gamePath.isEmpty() )
		return QString( "%1: absent" ).arg( label );
	if ( s.fromLocal )
		return QString( "%1: %2 (loose, beside the .lodm)" ).arg( label, s.resolved );
	return QString( "%1: %2 (game path, not found loose -- resource stack)" ).arg( label, s.resolved );
}

} // namespace

void ImpostorCardSet::frameOffsetOf( int i, int j, float * right, float * up ) const
{
	if ( right )
		*right = 0.0f;
	if ( up )
		*up = 0.0f;
	if ( frameOffset.isEmpty() || oct <= 0 || i < 0 || j < 0 || i >= oct || j >= oct )
		return;
	// `lodgen.cpp:3000`: frame (i, j) is at j*oct + i, two floats each.
	const int k = 2 * ( j * oct + i );
	if ( k + 1 >= frameOffset.size() )
		return;
	if ( right )
		*right = frameOffset.at( k );
	if ( up )
		*up = frameOffset.at( k + 1 );
}

QStringList ImpostorCardSet::notes() const
{
	QStringList out;
	out << QString( "lodm: %1" ).arg( lodmPath );
	if ( !ok ) {
		out << QString( "REFUSED: %1" ).arg( error );
		return out;
	}
	out << QString( "family: %1 (%2)" ).arg( pbr ? "pbr" : "legacy",
			pbr ? "mask = RMAOS" : "mask = GSAOS" );
	out << QString( "kind: %1%2" ).arg( kind,
			( layer >= 0 ) ? QString( ", layer %1" ).arg( layer ) : QString() );
	out << QString( "grid: %1x%1 = %2 frames" ).arg( oct ).arg( oct * oct );
	out << QString( "frame: %1x%2 px, mips %3, auxDiv %4" )
			.arg( frameW ).arg( frameH ).arg( mips ).arg( auxDiv );
	out << QString( "half: %1 x %2, center %3 %4 %5" )
			.arg( double( halfW ) ).arg( double( halfH ) )
			.arg( double( center[0] ) ).arg( double( center[1] ) ).arg( double( center[2] ) );
	out << QString( "depthSpan: %1 (height 0.5 = the card plane; units = (h-0.5)*depthSpan)" )
			.arg( double( depthSpan ) );
	if ( frameOffset.isEmpty() ) {
		out << QStringLiteral( "frameOffset: absent -- every frame centred on `center`,"
				" the law from before per-frame positioning" );
	} else {
		float lo = 0.0f, hi = 0.0f;
		for ( float v : frameOffset ) {
			lo = qMin( lo, v );
			hi = qMax( hi, v );
		}
		out << QString( "frameOffset: %1 pairs, range %2 .. %3 model units"
				" (against half %4 x %5) -- each frame's quad is slid back by its own" )
				.arg( frameOffset.size() / 2 ).arg( double( lo ) ).arg( double( hi ) )
				.arg( double( halfW ) ).arg( double( halfH ) );
	}
	out << QString( "emissiveScale: %1" ).arg( double( emissiveScale ) );
	if ( covOk() )
		out << QString( "coverage: floor %1 test %2 base %3 -- the sheet's alpha is ENCODED;"
				" the fraction is floor + (a-base)*(255-floor)/(255-base)" )
				.arg( covFloor ).arg( covTest ).arg( covBase );
	else
		out << QString( "coverage: none declared -- the alpha is the raw fraction,"
				" floored at 16/255" );
	if ( conv.isEmpty() )
		out << QString( "convention: NONE DECLARED -- this set predates the azimuth repair"
				" of 2026-09-19, so its frames were photographed with the azimuth turned"
				" by 180 degrees. Drawn under AsBaked, which is a diagnostic: RE-BAKE IT." );
	else if ( conv == QLatin1String( "spec1" ) )
		out << QString( "convention: spec1 -- frame (i,j) is the view from direction (i,j)" );
	else
		out << QString( "convention: \"%1\", which this build does not know. Drawn under"
				" AsBaked rather than guessed at." ).arg( conv );
	out << sheetNote( "colour", colour );
	out << sheetNote( "normal", normal );
	out << sheetNote( pbr ? "rmaos" : "gsaos", mask );
	out << sheetNote( "emissive", emissive );
	return out;
}

ImpostorCardSet impostorCardLoad( const QString & lodmPath, int layer )
{
	ImpostorCardSet set;
	set.lodmPath = lodmPath;

	QFile f( lodmPath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		set.error = QString( "cannot open %1" ).arg( lodmPath );
		return set;
	}
	const QByteArray bytes = f.readAll();
	f.close();

	const LodmMaterial mat = lodmParse( bytes );
	if ( !mat.ok ) {
		set.error = QString( "%1: %2" ).arg( lodmPath, mat.error );
		return set;
	}

	set.pbr  = mat.pbr;
	set.kind = mat.kind;
	set.emissiveScale = mat.emissiveScale;
	/* `_n` LAYOUT 2 (lane IMPOSTORFIN1 hook-up): height in ALPHA, sway in
	 * BLUE. A set written before it carries the two the other way round, and
	 * drawing it would read the sway as a depth -- refused by name. */
	if ( ( mat.kind == QLatin1String( "card" ) || mat.kind == QLatin1String( "cardArray" )
			|| mat.kind == QLatin1String( "aggregate" ) )
		&& mat.root.value( "nlayout" ).toInt( 1 ) != 2 ) {
		set.error = QString( "%1: _n layout %2, this build reads layout 2 (height in A,"
				" sway in B) -- rebake the set" ).arg( lodmPath )
				.arg( mat.root.value( "nlayout" ).toInt( 1 ) );
		return set;
	}

	const QDir dir = QFileInfo( lodmPath ).absoluteDir();

	if ( mat.kind == QLatin1String( "card" ) ) {
		const QJsonObject card = mat.root.value( "card" ).toObject();
		if ( card.isEmpty() ) {
			set.error = QString( "%1: kind is \"card\" but there is no card block" ).arg( lodmPath );
			return set;
		}
		set.oct  = card.value( "oct" ).toInt( 0 );
		set.mips = card.value( "mips" ).toInt( 0 );
		set.auxDiv = card.value( "auxDiv" ).toInt( 1 );
		set.depthSpan = float( card.value( "depthSpan" ).toDouble( 0.0 ) );
		set.conv = card.value( "conv" ).toString();
		readCoverage( card, set );

		float frame[2] = { 0.0f, 0.0f }, half[2] = { 0.0f, 0.0f };
		if ( !readFloats( card.value( "frame" ), frame, 2 ) ) {
			set.error = QString( "%1: card.frame is not [w,h]" ).arg( lodmPath );
			return set;
		}
		if ( !readFloats( card.value( "half" ), half, 2 ) ) {
			set.error = QString( "%1: card.half is not [w,h]" ).arg( lodmPath );
			return set;
		}
		if ( !readFloats( card.value( "center" ), set.center, 3 ) ) {
			set.error = QString( "%1: card.center is not [x,y,z]" ).arg( lodmPath );
			return set;
		}
		set.frameW = int( frame[0] );
		set.frameH = int( frame[1] );
		set.halfW  = half[0];
		set.halfH  = half[1];
		// After `oct`, which sizes the array.
		readFrameOffsets( card, set );

		fillSheet( set.colour,   dir, mat.color );
		fillSheet( set.normal,   dir, mat.normal );
		fillSheet( set.mask,     dir, mat.mask );
		fillSheet( set.emissive, dir, mat.emissive );

	} else if ( mat.kind == QLatin1String( "cardArray" ) ) {
		/* A cardArray's SHEETS are the array's (one texture array, N x N frames
		 * per layer, spec 335..389) and its EXTENTS are per layer, because two
		 * trees in one array are two different sizes. The grid, the frame size
		 * and the mips are shared -- that is what makes them arrayable at all
		 * (spec 341: "same frame size class, same grid"). */
		const QJsonObject arr = mat.root.value( "array" ).toObject();
		if ( arr.isEmpty() ) {
			set.error = QString( "%1: kind is \"cardArray\" but there is no array block" ).arg( lodmPath );
			return set;
		}
		const QJsonArray layers = arr.value( "layers" ).toArray();
		if ( layers.isEmpty() ) {
			set.error = QString( "%1: array.layers is empty" ).arg( lodmPath );
			return set;
		}
		int want = ( layer < 0 ) ? 0 : layer;
		if ( want >= layers.size() ) {
			set.error = QString( "%1: layer %2 asked for, the array has %3" )
					.arg( lodmPath ).arg( layer ).arg( layers.size() );
			return set;
		}
		set.layer = want;

		set.oct  = arr.value( "oct" ).toInt( 0 );
		set.mips = arr.value( "mips" ).toInt( 0 );
		set.auxDiv = arr.value( "auxDiv" ).toInt( 1 );

		float frame[2] = { 0.0f, 0.0f };
		if ( !readFloats( arr.value( "frame" ), frame, 2 ) ) {
			set.error = QString( "%1: array.frame is not [w,h]" ).arg( lodmPath );
			return set;
		}
		set.frameW = int( frame[0] );
		set.frameH = int( frame[1] );

		const QJsonObject L = layers.at( want ).toObject();
		float half[2] = { 0.0f, 0.0f };
		if ( !readFloats( L.value( "half" ), half, 2 ) ) {
			set.error = QString( "%1: layer %2 has no half [w,h]" ).arg( lodmPath ).arg( want );
			return set;
		}
		if ( !readFloats( L.value( "center" ), set.center, 3 ) ) {
			set.error = QString( "%1: layer %2 has no center [x,y,z]" ).arg( lodmPath ).arg( want );
			return set;
		}
		set.halfW = half[0];
		set.halfH = half[1];
		set.depthSpan = float( L.value( "depthSpan" ).toDouble( 0.0 ) );
		// per LAYER, not per array: a library part-way through the re-bake can
		// legitimately hold both vintages in one size class
		set.conv = L.value( "conv" ).toString();
		readCoverage( L, set );
		/* PER LAYER, for the reason the format doc gives (LODGEN_LODM_FORMAT
		 * 303): two sets in one array have different per-frame shifts, and a
		 * layer from a set baked before the law carries no key at all. */
		readFrameOffsets( L, set );

		// One emissive multiple PER LAYER (lodmfile.h: two layers of one array
		// are two different materials and do not share it).
		const QJsonArray es = arr.value( "emissiveScale" ).toArray();
		if ( want < es.size() && es.at( want ).isDouble() )
			set.emissiveScale = float( es.at( want ).toDouble() );

		fillSheet( set.colour,   dir, mat.color );
		fillSheet( set.normal,   dir, mat.normal );
		fillSheet( set.mask,     dir, mat.mask );
		fillSheet( set.emissive, dir, mat.emissive );

	} else {
		set.error = QString( "%1: kind is \"%2\" -- this preview draws \"card\" and \"cardArray\" only"
				" (a \"source\" .lodm is a material, not a baked set)" ).arg( lodmPath, mat.kind );
		return set;
	}

	// ---- the refusals, each naming the file --------------------------------
	if ( set.oct < ImpostorOct::kMinGrid || set.oct > ImpostorOct::kMaxGrid ) {
		set.error = QString( "%1: oct is %2, outside the bake's own %3..%4" )
				.arg( lodmPath ).arg( set.oct ).arg( ImpostorOct::kMinGrid ).arg( ImpostorOct::kMaxGrid );
		return set;
	}
	if ( set.frameW <= 0 || set.frameH <= 0 ) {
		set.error = QString( "%1: frame is %2x%3" ).arg( lodmPath ).arg( set.frameW ).arg( set.frameH );
		return set;
	}
	if ( !( set.halfW > 0.0f ) || !( set.halfH > 0.0f ) ) {
		set.error = QString( "%1: half is %2 x %3 -- a card with no extents is not drawable" )
				.arg( lodmPath ).arg( double( set.halfW ) ).arg( double( set.halfH ) );
		return set;
	}
	if ( !( set.depthSpan > 0.0f ) ) {
		/* Not a nicety. The height channel is a 0..1 window of the bake's
		 * orthographic depth (spec 284..293) and `depthSpan` is its unit. With
		 * no unit there is no pixel depth offset, no ghost-free blend and no
		 * transition to the model -- the card would be a flat sticker, which is
		 * exactly the thing this preview exists to not be. */
		set.error = QString( "%1: no depthSpan -- the height channel has no unit,"
				" so depth offset and the ghost-free blend cannot run" ).arg( lodmPath );
		return set;
	}
	if ( set.colour.gamePath.isEmpty() ) {
		set.error = QString( "%1: names no %2 sheet" ).arg( lodmPath,
				QLatin1String( lodmColorKey( set.pbr ) ) );
		return set;
	}
	if ( set.normal.gamePath.isEmpty() ) {
		set.error = QString( "%1: names no normal sheet -- without it there is nothing to light" )
				.arg( lodmPath );
		return set;
	}

	set.ok = true;
	return set;
}

/* ---------------------------------------------------------------------------
 * The manifest `C` line (spec 329..333).
 * ------------------------------------------------------------------------- */

ImpostorPlacement impostorParseCLine( const QString & line )
{
	ImpostorPlacement p;

	QString s = line.trimmed();
	if ( s.startsWith( QLatin1String( "C " ) ) || s.startsWith( QLatin1String( "C\t" ) ) )
		s = s.mid( 1 ).trimmed();

	const QStringList t = s.split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
	// index cx cy cz halfW halfH N depthspan lodm  = 9 tokens, then the two
	// optional array ones. Fewer than nine is not a `C` line we can place.
	if ( t.size() < 9 ) {
		p.error = QString( "C line has %1 fields, needs at least 9: %2" ).arg( t.size() ).arg( line.trimmed() );
		return p;
	}

	bool okAll = true, b = false;
	p.index = t.at( 0 ).toInt( &b );                 okAll = okAll && b;
	p.center[0] = t.at( 1 ).toFloat( &b );           okAll = okAll && b;
	p.center[1] = t.at( 2 ).toFloat( &b );           okAll = okAll && b;
	p.center[2] = t.at( 3 ).toFloat( &b );           okAll = okAll && b;
	p.halfW = t.at( 4 ).toFloat( &b );               okAll = okAll && b;
	p.halfH = t.at( 5 ).toFloat( &b );               okAll = okAll && b;
	p.oct = t.at( 6 ).toInt( &b );                   okAll = okAll && b;
	p.depthSpan = t.at( 7 ).toFloat( &b );           okAll = okAll && b;
	p.lodm = t.at( 8 );

	if ( !okAll ) {
		p.error = QString( "C line has a field that is not a number: %1" ).arg( line.trimmed() );
		return p;
	}
	if ( p.lodm.isEmpty() ) {
		p.error = QString( "C line names no .lodm: %1" ).arg( line.trimmed() );
		return p;
	}

	// The two optional trailing fields: the array and the layer in it. A reader
	// that stops at nine is unaffected by them, which is why they are last.
	if ( t.size() >= 11 ) {
		p.arrayLodm = t.at( 9 );
		p.layer = t.at( 10 ).toInt( &b );
		if ( !b ) {
			p.error = QString( "C line's layer field is not a number: %1" ).arg( line.trimmed() );
			return p;
		}
	}

	p.ok = true;
	return p;
}

QVector<ImpostorPlacement> impostorReadManifest( const QString & manifestPath, QString * error )
{
	QVector<ImpostorPlacement> out;

	QFile f( manifestPath );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( error )
			*error = QString( "cannot open %1" ).arg( manifestPath );
		return out;
	}
	QTextStream in( &f );
	while ( !in.atEnd() ) {
		const QString line = in.readLine();
		const QString t = line.trimmed();
		if ( !t.startsWith( QLatin1Char( 'C' ) ) )
			continue;
		// `C` and only `C`: a manifest has other letters and some of them
		// start with the same one in a longer word.
		if ( t.size() > 1 && !t.at( 1 ).isSpace() )
			continue;
		out.append( impostorParseCLine( t ) );
	}
	if ( error )
		error->clear();
	return out;
}
