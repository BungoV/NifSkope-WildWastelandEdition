/* RACE Bone Scale Data + the game's own combination. See src/bodybuild.h for
   where every number and every RVA comes from. Lane GLTFEXPORT1, 2026-09-19.

   The record walk goes through ESMFile (lib/libfo76utils), the same reader
   src/esmdata.cpp uses, so there is one .esm parser in this tree and not two
   (CONSTITUTION 10, what is shared lives in the shared code). */

#include "bodybuild.h"

#include "esmfile.hpp"

#include <cmath>

namespace {

constexpr unsigned int SIG_RACE = 0x45434152u;   // "RACE"
constexpr unsigned int SIG_EDID = 0x44494445u;   // "EDID"
constexpr unsigned int SIG_BSMP = 0x504D5342u;   // "BSMP"
constexpr unsigned int SIG_BMMP = 0x504D4D42u;   // "BMMP"
constexpr unsigned int SIG_BSMB = 0x424D5342u;   // "BSMB"
constexpr unsigned int SIG_BSMS = 0x534D5342u;   // "BSMS"

float readF( const unsigned char * p )
{
	float f;
	std::memcpy( &f, p, 4 );
	return f;
}

} // namespace

bool BodyBuildBone::neutral() const
{
	auto one = []( const Vector3 & v ) {
		return v[0] == 1.0f && v[1] == 1.0f && v[2] == 1.0f;
	};
	return one( thin ) && one( muscular ) && one( fat );
}

const BodyBuildBone * BodyBuildSet::find( const QString & boneName ) const
{
	for ( const BodyBuildBone & b : bones )
		if ( b.name.compare( boneName, Qt::CaseInsensitive ) == 0 )
			return &b;
	return nullptr;
}

const BodyBuildSet * BodyBuildTable::set( int gender ) const
{
	for ( const BodyBuildSet & s : sets )
		if ( s.gender == gender )
			return &s;
	return nullptr;
}

// ---------------------------------------------------------------------------

namespace {

/*! Walk one RACE record's subrecords into `out`. Returns a refusal sentence,
 *  or "" -- an EMPTY table is not a parse failure, it is a race with no build
 *  triangle (creatures, power armour), and the caller says so in words. */
QString parseRace( ESMFile & esm, const ESMFile::ESMRecord & rec, BodyBuildTable & out )
{
	ESMFile::ESMField f( esm, rec );
	BodyBuildSet * cur = nullptr;
	bool rangeMode = false;
	QString pendingName;

	while ( f.next() ) {
		if ( f.type == SIG_EDID ) {
			out.editorId = QString::fromLatin1(
				reinterpret_cast<const char *>( f.data() ),
				int( f.size() ? f.size() - 1 : 0 ) );
		} else if ( f.type == SIG_BSMP ) {
			if ( f.size() < 4 )
				return QStringLiteral( "BSMP is %1 bytes, expected 4" ).arg( f.size() );
			BodyBuildSet s;
			std::memcpy( &s.gender, f.data(), 4 );
			out.sets.append( s );
			cur = &out.sets.last();
			rangeMode = false;
			pendingName.clear();
		} else if ( f.type == SIG_BMMP ) {
			rangeMode = true;
			pendingName.clear();
		} else if ( f.type == SIG_BSMB ) {
			pendingName = QString::fromLatin1(
				reinterpret_cast<const char *>( f.data() ),
				int( f.size() ? f.size() - 1 : 0 ) );
		} else if ( f.type == SIG_BSMS ) {
			if ( !cur )
				continue;
			const unsigned char * p = f.data();
			if ( !rangeMode && f.size() == 36 ) {
				BodyBuildBone b;
				b.name = pendingName;
				for ( int i = 0; i < 3; i++ ) {
					b.thin[i]     = readF( p + 0  + i * 4 );
					b.muscular[i] = readF( p + 12 + i * 4 );
					b.fat[i]      = readF( p + 24 + i * 4 );
				}
				cur->bones.append( b );
			} else if ( rangeMode && f.size() == 16 ) {
				BodyBuildRangeMod m;
				m.name = pendingName;
				m.minY = readF( p + 0 );
				m.minZ = readF( p + 4 );
				m.maxY = readF( p + 8 );
				m.maxZ = readF( p + 12 );
				cur->rangeMods.append( m );
			} else {
				// A size we do not know is a REFUSAL, never a silent skip: the
				// layout is the whole of this reader's correctness.
				return QStringLiteral( "BSMS for '%1' is %2 bytes; the layout knows 36 (scales) "
									   "and 16 (range modifiers) only" )
					.arg( pendingName ).arg( f.size() );
			}
			pendingName.clear();
		}
	}
	return QString();
}

} // namespace

bool bodyBuildLoadRace( const QString & esmPath, quint32 formID,
						BodyBuildTable & out, QString & error )
{
	error.clear();
	out = BodyBuildTable();
	out.formID = formID;
	try {
		ESMFile esm( esmPath.toLocal8Bit().constData() );
		const ESMFile::ESMRecord * r = esm.findRecord( formID );
		if ( !r ) {
			error = QStringLiteral( "%1 has no form %2" )
				.arg( esmPath ).arg( formID, 8, 16, QLatin1Char( '0' ) );
			return false;
		}
		if ( r->type != SIG_RACE ) {
			error = QStringLiteral( "form %1 is not a RACE" )
				.arg( formID, 8, 16, QLatin1Char( '0' ) );
			return false;
		}
		const QString bad = parseRace( esm, *r, out );
		if ( !bad.isEmpty() ) {
			error = bad;
			out = BodyBuildTable();
			return false;
		}
	} catch ( std::exception & e ) {
		error = QStringLiteral( "%1 could not be read: %2" ).arg( esmPath, QString::fromUtf8( e.what() ) );
		return false;
	}
	if ( out.isEmpty() ) {
		error = QStringLiteral( "RACE '%1' carries no Bone Scale Data, so it has no build triangle" )
			.arg( out.editorId.isEmpty() ? QStringLiteral( "?" ) : out.editorId );
		return false;
	}
	return true;
}

bool bodyBuildListRaces( const QString & esmPath,
						 QVector<QPair<quint32, QString>> & out, QString & error )
{
	error.clear();
	out.clear();
	try {
		ESMFile esm( esmPath.toLocal8Bit().constData() );
		for ( unsigned int id = 0; id < 0x01000000u; id++ ) {
			const ESMFile::ESMRecord * r = esm.findRecord( id );
			if ( !r || r->type != SIG_RACE )
				continue;
			BodyBuildTable t;
			if ( !parseRace( esm, *r, t ).isEmpty() || t.isEmpty() )
				continue;
			out.append( qMakePair( quint32( id ), t.editorId ) );
		}
	} catch ( std::exception & e ) {
		error = QString::fromUtf8( e.what() );
		return false;
	}
	if ( out.isEmpty() )
		error = QStringLiteral( "no RACE in %1 carries Bone Scale Data" ).arg( esmPath );
	return !out.isEmpty();
}

// ---------------------------------------------------------------------------
// The combination. Every constant is quoted to its RVA in src/bodybuild.h.

float bodyBuildCentroidK( float wThin, float wMuscular, float wFat )
{
	const double h = std::sqrt( 3.0 ) * 0.5;       // the triangle's height
	const double R = 1.0 / std::sqrt( 3.0 );       // its CIRCUMRADIUS: centroid -> corner
	double s = double( wThin ) + double( wMuscular ) + double( wFat );
	if ( !( s > 0.0 ) )
		return 1.0f;                               // no weight at all = the centre
	const double a = double( wThin ) / s, b = double( wMuscular ) / s, c = double( wFat ) / s;
	// P0 = (0, h), P1 = (0.5, 0), P2 = (1, h)
	const double px = a * 0.0 + b * 0.5 + c * 1.0;
	const double py = a * h + b * 0.0 + c * h;
	const double cx = 0.5, cy = 2.0 * h / 3.0;
	const double d = std::sqrt( ( px - cx ) * ( px - cx ) + ( py - cy ) * ( py - cy ) );
	// The normaliser is R and NOT h. With h the corners come out at k = 1/3,
	// and the corners are the half of this that the shipped corpus MEASURES:
	// see the header. Anything that is 0 at a corner and 1 at the centre fits
	// the evidence; this is the one the disassembly reads as, and the interior
	// is the part that is still unproven.
	double k = ( R - d ) / R;
	if ( k < 0.0 )
		k = 0.0;
	if ( k > 1.0 )
		k = 1.0;
	return float( k );
}

Vector3 bodyBuildScale( const BodyBuildBone & b, float wThin, float wMuscular, float wFat )
{
	double s = double( wThin ) + double( wMuscular ) + double( wFat );
	double a, m, f;
	if ( !( s > 0.0 ) ) {
		a = m = f = 1.0 / 3.0;                     // kDefaultMorphWeight @ 0x3715478
	} else {
		a = double( wThin ) / s;
		m = double( wMuscular ) / s;
		f = double( wFat ) / s;
	}
	const double k = double( bodyBuildCentroidK( wThin, wMuscular, wFat ) );
	Vector3 out;
	for ( int i = 0; i < 3; i++ ) {
		const double v0 = double( b.thin[i] ), v1 = double( b.muscular[i] ), v2 = double( b.fat[i] );
		const double mean = ( v0 + v1 + v2 ) * ( 1.0 / 3.0 );
		out[i] = float( a * v0 + m * v1 + f * v2 - k * ( mean - 1.0 ) );
	}
	return out;
}

void bodyBuildCornerWeights( int which, float & wThin, float & wMuscular, float & wFat )
{
	wThin = wMuscular = wFat = 0.0f;
	switch ( which ) {
	case 0: wThin = 1.0f; break;
	case 1: wMuscular = 1.0f; break;
	case 2: wFat = 1.0f; break;
	default: wThin = wMuscular = wFat = 1.0f / 3.0f; break;
	}
}

QStringList bodyBuildBoneNames( const BodyBuildSet & s )
{
	QStringList out;
	for ( const BodyBuildBone & b : s.bones )
		out << b.name;
	return out;
}

QVector<BodyBuildCycleKey> bodyBuildCycleKeys()
{
	QVector<BodyBuildCycleKey> k;
	k.append( { 0.0f, 0 } );     // THIN
	k.append( { 2.0f, 1 } );     // MUSCULAR
	k.append( { 4.0f, 2 } );     // FAT
	k.append( { 6.0f, 0 } );     // THIN, the loop closes exactly
	return k;
}
