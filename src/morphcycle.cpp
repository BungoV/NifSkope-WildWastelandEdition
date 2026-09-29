/* The body shape and facebones preview cycles. The design, the formulas, their
   sources and what is unproven live in morphcycle.h.
   Lane MORPHCYC1, 2026-09-29. */

#include "morphcycle.h"

#include "bodybuild.h"
#include "gamemanager.h"
#include "ba2file.hpp"
#include "model/nifmodel.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSet>

#include <cmath>
#include <functional>
#include <string>

namespace
{

const float kPi = 3.14159265358979323846f;

//! The first Fallout4.esm the game manager serves (bodybuildpanel.cpp's own
//! search, which lives in that file's anonymous namespace).
QString morphFindEsm( QStringList & looked )
{
	// the Game Folders are Data/Textures, Data/Materials and Data/*.ba2 (the
	// harness scope and a fresh install), or Data itself (an older profile):
	// try each, the folder that holds it, and the game path's Data
	QStringList roots;
	for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) ) {
		const QFileInfo fi( f );
		roots << ( fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath() ) << fi.absolutePath();
	}
	const QString game = Game::GameManager::path( Game::FALLOUT_4 );
	if ( !game.isEmpty() )
		roots << game + QStringLiteral( "/Data" );
	roots.removeDuplicates();
	for ( const QString & r : roots ) {
		const QString c = r + QStringLiteral( "/Fallout4.esm" );
		looked << c;
		if ( QFile::exists( c ) )
			return c;
	}
	return QString();
}

bool morphGameFile( QByteArray & out, const QString & path )
{
	const std::string p = path.toLower().toStdString();
	return Game::GameManager::get_file( out, Game::FALLOUT_4, std::string_view( p ) );
}

bool morphOnlyName( void * p, const std::string_view & s )
{
	const std::string & want = *static_cast<const std::string *>( p );
	return s.size() >= want.size() && s.substr( s.size() - want.size() ) == want;
}

/*! A game NIF by its Data-relative path. NOT through GameManager::get_file:
 *  the Game Manager's shared Fallout 4 index filters every .nif OUT of the
 *  archives (gamemanager.cpp archiveFilterFunction_2, measured 2026-09-29: the
 *  regions .txt came back, skeleton_faceBones.nif beside it in the same
 *  Fallout4 - Meshes.ba2 did not). So: a loose copy under the Data folder a
 *  Game Folder names first, then each Game Folder archive opened on its own,
 *  its index filtered down to this one file name. */
bool morphGameNif( QByteArray & out, const QString & rel, QString & from )
{
	const QString relLower = rel.toLower();
	const QString base = QFileInfo( relLower ).fileName();
	const std::string wantName = base.toStdString();
	const std::string wantPath = relLower.toStdString();
	const QStringList folders = Game::GameManager::folders( Game::FALLOUT_4 );
	for ( const QString & f : folders ) {
		const QFileInfo fi( f );
		if ( !fi.isDir() )
			continue;
		for ( const QString & root : { fi.absoluteFilePath(), fi.absolutePath() } ) {
			QFile loose( root + QLatin1Char( '/' ) + rel );
			if ( loose.exists() && loose.open( QIODevice::ReadOnly ) ) {
				out = loose.readAll();
				from = loose.fileName();
				return !out.isEmpty();
			}
		}
	}
	for ( const QString & f : folders ) {
		if ( !f.endsWith( QLatin1String( ".ba2" ), Qt::CaseInsensitive )
			 && !f.endsWith( QLatin1String( ".bsa" ), Qt::CaseInsensitive ) )
			continue;
		try {
			BA2File one;
			std::string filter = wantName;
			one.loadArchivePath( f.toLocal8Bit().constData(), &morphOnlyName, &filter );
			if ( !one.findFile( wantPath ) )
				continue;
			BA2File::UCharArray buf;
			one.extractFile( buf, wantPath );
			out = QByteArray( reinterpret_cast<const char *>( buf.data ), qsizetype( buf.size ) );
			from = f;
			return !out.isEmpty();
		} catch ( ... ) {
			// unreadable archive: not the source
		}
	}
	return false;
}

HkxTransform morphIdentityTrs()
{
	HkxTransform x;
	x.translation = Vector3( 0, 0, 0 );
	x.rotation = Quat( 1.0f, 0.0f, 0.0f, 0.0f );
	x.scale = Vector3( 1, 1, 1 );
	return x;
}

//! T * R * diag(scale), as the playback composes it (hkxTrsPerAxis there)
Transform morphTrs( const HkxTransform & x )
{
	Transform p;
	Matrix r;
	r.fromQuat( x.rotation );
	Matrix s;
	s( 0, 0 ) = x.scale[0];
	s( 1, 1 ) = x.scale[1];
	s( 2, 2 ) = x.scale[2];
	p.rotation = r * s;
	p.translation = x.translation;
	p.scale = 1.0f;
	return p;
}

Matrix morphAxisRot( int axis, float degrees )
{
	Vector3 a( 0, 0, 0 );
	a[axis] = 1.0f;
	const float h = degrees * kPi / 180.0f * 0.5f;
	Matrix m;
	m.fromQuat( Quat( std::cos( h ), a[0] * std::sin( h ), a[1] * std::sin( h ), a[2] * std::sin( h ) ) );
	return m;
}

//! v for frame k (0..24) of one channel: 0 at 0, +1 at 6, -1 at 18, 0 at 24
float morphChannelValue( int k )
{
	if ( k <= 0 || k >= 24 )
		return 0.0f;
	if ( k <= 6 )
		return float( k ) / 6.0f;
	if ( k <= 18 )
		return 1.0f - 2.0f * float( k - 6 ) / 12.0f;
	return -1.0f + float( k - 18 ) / 6.0f;
}

Vector3 morphVec( const QJsonObject & o )
{
	return Vector3( float( o.value( QStringLiteral( "x" ) ).toDouble() ),
					float( o.value( QStringLiteral( "y" ) ).toDouble() ),
					float( o.value( QStringLiteral( "z" ) ).toDouble() ) );
}

struct MorphRegionBone
{
	QString node;					//!< "skin_bone_X"
	Vector3 posMax, posMin, rotMax, rotMin, sclMax, sclMin;
};

struct MorphChannel
{
	int region = -1;
	int comp = 0;					//!< 0..2 pos, 3..5 rot, 6 scale
	QString label;
};

const char * const kCompWord[7] = { "pos X", "pos Y", "pos Z", "rot X", "rot Y", "rot Z", "scale" };

struct MorphSkelBone
{
	QString name;
	int parent = -1;				//!< index into the bone list
	HkxTransform rest;				//!< the NIF's local, as T / quat / per-axis scale
	Transform restGlobal;
};

} // namespace

QString morphCycleGenderWord( int gender )
{
	return gender == 1 ? QObject::tr( "Female" ) : QObject::tr( "Male" );
}


/*
 *  BODY SHAPE
 */

bool morphCycleBodyShape( int gender, HkxClipEntry & out, QString & sentence, MorphCycleStats * stats )
{
	QStringList looked;
	const QString esm = morphFindEsm( looked );
	if ( esm.isEmpty() ) {
		sentence = QObject::tr( "No Fallout4.esm in the game folders, so there is no HumanRace bone "
								"scale table to build the body shape cycle from. Looked in: %1" )
			.arg( looked.isEmpty() ? QObject::tr( "(no Fallout 4 folders set in Settings)" )
								   : looked.join( QStringLiteral( ", " ) ) );
		return false;
	}
	BodyBuildTable table;
	QString err;
	if ( !bodyBuildLoadRace( esm, BODYBUILD_HUMAN_RACE, table, err ) || table.isEmpty() ) {
		sentence = QObject::tr( "The HumanRace bone scale table could not be read from %1: %2" )
			.arg( esm, err.isEmpty() ? QObject::tr( "it is empty" ) : err );
		return false;
	}
	const BodyBuildSet * set = table.set( gender );
	if ( !set || set->bones.isEmpty() ) {
		sentence = QObject::tr( "HumanRace carries no %1 bone scale table." )
			.arg( morphCycleGenderWord( gender ).toLower() );
		return false;
	}

	QVector<const BodyBuildBone *> bones;
	for ( const BodyBuildBone & b : set->bones ) {
		if ( !b.neutral() && b.name.endsWith( QStringLiteral( "_skin" ), Qt::CaseInsensitive ) )
			bones << &b;
	}
	if ( bones.isEmpty() ) {
		sentence = QObject::tr( "The %1 table moves no *_skin bone." ).arg( morphCycleGenderWord( gender ) );
		return false;
	}

	const QVector<BodyBuildCycleKey> keys = bodyBuildCycleKeys();
	const float fps = 30.0f;
	HkxAnimClip c;
	c.name = QObject::tr( "Body Shape Cycle - %1" ).arg( morphCycleGenderWord( gender ) );
	c.originalSkeletonName = QStringLiteral( "skeleton.nif (*_skin: not in skeleton.hkx)" );
	c.blendHint = QStringLiteral( "ADDITIVE" );
	c.frameDuration = 1.0f / fps;
	c.numFrames = int( std::lround( keys.last().time * fps ) ) + 1;		// 181
	c.duration = float( c.numFrames - 1 ) * c.frameDuration;
	c.numTracks = bones.count();
	c.trackToBoneIsIdentity = true;
	c.annotations.resize( c.numTracks );
	const char * const cornerWord[3] = { "thin", "muscular", "fat" };
	for ( const BodyBuildCycleKey & k : keys ) {
		HkxAnnotation a;
		a.time = k.time;
		a.text = QString::fromLatin1( cornerWord[qBound( 0, k.corner, 2 )] );
		c.annotations[0].append( a );
	}
	QStringList names;
	for ( int t = 0; t < bones.count(); t++ ) {
		c.trackToBone << t;
		names << bones.at( t )->name;
	}
	c.frames.resize( c.numFrames );
	for ( int f = 0; f < c.numFrames; f++ ) {
		// gltfexportchar.cpp's own segment walk: which of THIN -> MUSC -> FAT -> THIN
		const float tm = float( f ) / fps;
		int seg = 0;
		while ( seg + 2 < keys.size() && tm >= keys[seg + 1].time )
			seg++;
		const float span = keys[seg + 1].time - keys[seg].time;
		const float u = span > 0.0f ? qBound( 0.0f, ( tm - keys[seg].time ) / span, 1.0f ) : 0.0f;
		float w0[3] = { 0, 0, 0 }, w1[3] = { 0, 0, 0 };
		bodyBuildCornerWeights( keys[seg].corner, w0[0], w0[1], w0[2] );
		bodyBuildCornerWeights( keys[seg + 1].corner, w1[0], w1[1], w1[2] );
		QVector<HkxTransform> row( c.numTracks );
		for ( int t = 0; t < c.numTracks; t++ ) {
			HkxTransform x = morphIdentityTrs();
			x.scale = bodyBuildScale( *bones.at( t ), w0[0] + ( w1[0] - w0[0] ) * u,
									  w0[1] + ( w1[1] - w0[1] ) * u, w0[2] + ( w1[2] - w0[2] ) * u );
			row[t] = x;
		}
		c.frames[f] = row;
	}

	out = HkxClipEntry();
	out.name = c.name;
	out.skeletonSource = QObject::tr( "HumanRace bone scale table, %1" ).arg( esm );
	out.clip = c;
	out.trackBone = names;
	out.additive = true;
	out.generated = QObject::tr( "body shape cycle" );
	out.genMode = HkxClipEntry::GenLocalScale;
	if ( stats ) {
		*stats = MorphCycleStats();
		stats->channels = bones.count();
		stats->tracks = bones.count();
		stats->source = out.skeletonSource;
	}
	sentence = QObject::tr( "%1: %2 *_skin bones, thin -> muscular -> fat -> thin, %3 frames at 30 fps. "
							"Exact at the corners, interpolated between them (k shape unproven)." )
		.arg( c.name ).arg( bones.count() ).arg( c.numFrames );
	return true;
}


/*
 *  FACEBONES
 */

bool morphCycleFaceBones( int gender, HkxClipEntry & out, QString & sentence, MorphCycleStats * stats,
						  const MorphCycleOptions & opts )
{
	const QString dir = QStringLiteral( "meshes/actors/character/characterassets/" );
	const QString regionsFile = gender == 1 ? QStringLiteral( "HumanRaceFacialBoneRegionsFemale.txt" )
											: QStringLiteral( "HumanRaceFacialBoneRegionsMale.txt" );
	const QString skelFile = gender == 1 ? QStringLiteral( "skeleton_female_faceBones.nif" )
										 : QStringLiteral( "skeleton_faceBones.nif" );

	// ---- the regions -------------------------------------------------------
	QByteArray raw;
	if ( !morphGameFile( raw, dir + regionsFile ) || raw.isEmpty() ) {
		sentence = QObject::tr( "%1 is not in the Fallout 4 archives the Game Manager serves "
								"(Settings > Resources), so there is no facebones table." ).arg( regionsFile );
		return false;
	}
	QJsonParseError pe;
	const QJsonDocument doc = QJsonDocument::fromJson( raw, &pe );
	if ( !doc.isArray() ) {
		sentence = QObject::tr( "%1 is not a JSON list of regions: %2" ).arg( regionsFile, pe.errorString() );
		return false;
	}
	const QJsonArray regionsJ = doc.array();

	QVector<QString> regionName;
	QVector<QVector<MorphRegionBone>> regionBones;
	QSet<QString> allBones;
	for ( const QJsonValue & rv : regionsJ ) {
		const QJsonObject r = rv.toObject();
		regionName << r.value( QStringLiteral( "Name" ) ).toString();
		QVector<MorphRegionBone> rb;
		for ( const QJsonValue & bv : r.value( QStringLiteral( "BonesA" ) ).toArray() ) {
			const QJsonObject b = bv.toObject();
			MorphRegionBone m;
			m.node = QStringLiteral( "skin_" ) + b.value( QStringLiteral( "Bone" ) ).toString();
			const QJsonObject mx = b.value( QStringLiteral( "Maxima" ) ).toObject();
			const QJsonObject mn = b.value( QStringLiteral( "Minima" ) ).toObject();
			m.posMax = morphVec( mx.value( QStringLiteral( "Position" ) ).toObject() );
			m.posMin = morphVec( mn.value( QStringLiteral( "Position" ) ).toObject() );
			m.rotMax = morphVec( mx.value( QStringLiteral( "Rotation" ) ).toObject() );
			m.rotMin = morphVec( mn.value( QStringLiteral( "Rotation" ) ).toObject() );
			m.sclMax = morphVec( mx.value( QStringLiteral( "Scale" ) ).toObject() );
			m.sclMin = morphVec( mn.value( QStringLiteral( "Scale" ) ).toObject() );
			rb << m;
			allBones.insert( m.node.toLower() );
		}
		regionBones << rb;
	}

	// ---- the channels ------------------------------------------------------
	QVector<MorphChannel> channels;
	QSet<int> liveRegions;
	for ( int r = 0; r < regionBones.count(); r++ ) {
		for ( int comp = 0; comp < 7; comp++ ) {
			bool live = false;
			for ( const MorphRegionBone & b : regionBones.at( r ) ) {
				for ( int a = 0; a < 3; a++ ) {
					if ( comp == 6 ) {
						if ( b.sclMax[a] != 0.0f || b.sclMin[a] != 0.0f )
							live = true;
					} else if ( comp >= 3 ) {
						if ( a == comp - 3 && ( b.rotMax[a] != 0.0f || b.rotMin[a] != 0.0f ) )
							live = true;
					} else if ( a == comp && ( b.posMax[a] != 0.0f || b.posMin[a] != 0.0f ) ) {
						live = true;
					}
				}
			}
			if ( !live )
				continue;
			MorphChannel ch;
			ch.region = r;
			ch.comp = comp;
			ch.label = QStringLiteral( "%1: %2" ).arg( regionName.at( r ), QString::fromLatin1( kCompWord[comp] ) );
			channels << ch;
			liveRegions.insert( r );
		}
	}
	if ( channels.isEmpty() ) {
		sentence = QObject::tr( "%1 carries no region with a non-zero minimum or maximum." ).arg( regionsFile );
		return false;
	}

	// ---- the skeleton ------------------------------------------------------
	QByteArray skelRaw;
	QString skelFrom;
	if ( !morphGameNif( skelRaw, dir + skelFile, skelFrom ) || skelRaw.isEmpty() ) {
		sentence = QObject::tr( "%1 is in no Game Folder of Fallout 4 (loose or in an archive), and the "
								"facebones globals are composed down its chain." ).arg( skelFile );
		return false;
	}
	NifModel sk;
	{
		QBuffer buf( &skelRaw );
		buf.open( QIODevice::ReadOnly );
		if ( !sk.load( buf ) ) {
			sentence = QObject::tr( "%1 could not be read as a NIF." ).arg( skelFile );
			return false;
		}
	}
	QVector<MorphSkelBone> bones;
	QHash<int, int> boneOfBlock;
	for ( int i = 0; i < sk.getBlockCount(); i++ ) {
		const QModelIndex idx = sk.getBlockIndex( i );
		if ( !sk.blockInherits( idx, "NiNode" ) )
			continue;
		MorphSkelBone b;
		b.name = sk.get<QString>( idx, "Name" );
		const Transform t( &sk, idx );
		b.rest.translation = t.translation;
		b.rest.rotation = t.rotation.toQuat();
		b.rest.scale = Vector3( t.scale, t.scale, t.scale );
		boneOfBlock.insert( i, bones.count() );
		bones << b;
	}
	for ( auto it = boneOfBlock.constBegin(); it != boneOfBlock.constEnd(); ++it )
		bones[it.value()].parent = boneOfBlock.value( sk.getParent( it.key() ), -1 );
	// rest globals, from the SAME quantised T / quat / scale the clip stores, so
	// the rest frame of the clip is the identity delta exactly
	{
		QVector<int> state( bones.count(), 0 );
		std::function<void( int )> glob = [&]( int b ) {
			if ( state[b] == 2 )
				return;
			state[b] = 1;
			const int p = bones[b].parent;
			if ( p >= 0 && state[p] == 0 )
				glob( p );
			bones[b].restGlobal = ( p >= 0 && state[p] == 2 ) ? bones[p].restGlobal * morphTrs( bones[b].rest )
															   : morphTrs( bones[b].rest );
			state[b] = 2;
		};
		for ( int b = 0; b < bones.count(); b++ )
			glob( b );
	}
	QHash<QString, int> boneByName;
	for ( int b = 0; b < bones.count(); b++ )
		boneByName.insert( bones[b].name.toLower(), b );

	// ---- the tracks: every region bone and every skeleton descendant of one
	QSet<int> moved;
	QStringList missing;
	for ( const QString & n : std::as_const( allBones ) ) {
		const int b = boneByName.value( n, -1 );
		if ( b < 0 )
			missing << n;
		else
			moved.insert( b );
	}
	QVector<int> trackBone;			// track -> skeleton bone
	QVector<int> trackOf( bones.count(), -1 );
	for ( int b = 0; b < bones.count(); b++ ) {
		bool in = false;
		for ( int a = b, guard = 0; a >= 0 && guard < 512; a = bones[a].parent, guard++ ) {
			if ( moved.contains( a ) ) {
				in = true;
				break;
			}
		}
		if ( in ) {
			trackOf[b] = trackBone.count();
			trackBone << b;
		}
	}
	if ( trackBone.isEmpty() ) {
		sentence = QObject::tr( "None of the %1 region bones of %2 is a node of %3." )
			.arg( allBones.count() ).arg( regionsFile, skelFile );
		return false;
	}

	// ---- the frames ----------------------------------------------------------
	const int perCh = 24;
	HkxAnimClip c;
	c.name = QObject::tr( "Facebones Cycle - %1" ).arg( morphCycleGenderWord( gender ) );
	c.originalSkeletonName = skelFile + QStringLiteral( " (skin_bone_*: not in skeleton.hkx)" );
	c.blendHint = QStringLiteral( "NORMAL" );
	c.frameDuration = 1.0f / 24.0f;
	c.numFrames = 1 + perCh * channels.count();
	c.duration = float( c.numFrames - 1 ) * c.frameDuration;
	c.numTracks = trackBone.count();
	c.trackToBoneIsIdentity = true;
	c.annotations.resize( c.numTracks );
	for ( int ch = 0; ch < channels.count(); ch++ ) {
		HkxAnnotation a;
		a.time = float( ch * perCh ) * c.frameDuration;
		a.text = channels.at( ch ).label;
		c.annotations[0].append( a );
	}
	QStringList names;
	for ( int t = 0; t < trackBone.count(); t++ ) {
		c.trackToBone << t;
		names << bones[trackBone[t]].name;
	}

	QVector<HkxTransform> restRow( c.numTracks );
	for ( int t = 0; t < c.numTracks; t++ )
		restRow[t] = bones[trackBone[t]].rest;

	c.frames.resize( c.numFrames );
	for ( int f = 0; f < c.numFrames; f++ ) {
		QVector<HkxTransform> row = restRow;
		const int ch = std::min( f / perCh, int( channels.count() ) - 1 );
		const float v = morphChannelValue( f - ch * perCh );
		if ( v != 0.0f ) {
			const MorphChannel & mc = channels.at( ch );
			for ( const MorphRegionBone & rb : regionBones.at( mc.region ) ) {
				const int b = boneByName.value( rb.node.toLower(), -1 );
				const int t = b >= 0 ? trackOf[b] : -1;
				if ( t < 0 )
					continue;
				// CalcWeightedTransform: v > 0 ? v * Max : |v| * Min (Min stored signed)
				auto pick = [v]( const Vector3 & mx, const Vector3 & mn, int a ) {
					return v > 0.0f ? v * mx[a] : std::fabs( v ) * mn[a];
				};
				Vector3 d( 0, 0, 0 ), ang( 0, 0, 0 ), s( 0, 0, 0 );
				if ( mc.comp < 3 )
					d[mc.comp] = pick( rb.posMax, rb.posMin, mc.comp );
				else if ( mc.comp < 6 )
					ang[mc.comp - 3] = pick( rb.rotMax, rb.rotMin, mc.comp - 3 );
				else
					for ( int a = 0; a < 3; a++ )
						s[a] = pick( rb.sclMax, rb.sclMin, a );

				const HkxTransform & rest = restRow[t];
				Matrix restR;
				restR.fromQuat( rest.rotation );
				const float rs = rest.scale[0];
				HkxTransform x = rest;
				// R = Rz(-z) * Rx(-x) * Ry(-y): FromEulerAnglesZXY, angles negated
				const float sg = opts.flipRotationSign ? -1.0f : 1.0f;		// harness-only reading
				const Matrix r = morphAxisRot( 2, -sg * ang[2] ) * morphAxisRot( 0, -sg * ang[0] )
					* morphAxisRot( 1, -sg * ang[1] );
				if ( opts.parentFrame ) {
					// the WRONG reading (harness floor): posed local = T(d) * R * S * restLocal,
					// the whole morph in the PARENT's frame, pivoting on the parent's origin
					Matrix sd;
					sd( 0, 0 ) = 1.0f + s[0];
					sd( 1, 1 ) = 1.0f + s[1];
					sd( 2, 2 ) = 1.0f + s[2];
					x.translation = d + r * ( sd * rest.translation );
					x.rotation = ( r * restR ).toQuat();
				} else {
					// posed local = restLocal * T(d) * R * S
					x.translation = rest.translation + restR * d * rs;
					x.rotation = ( restR * r ).toQuat();
				}
				x.scale = Vector3( rs * ( 1.0f + s[0] ), rs * ( 1.0f + s[1] ), rs * ( 1.0f + s[2] ) );
				row[t] = x;
			}
		}
		c.frames[f] = row;
	}

	out = HkxClipEntry();
	out.name = c.name;
	out.skeletonSource = QObject::tr( "%1 (Game Manager) + %2 (%3)" ).arg( regionsFile, skelFile, skelFrom );
	out.clip = c;
	out.trackBone = names;
	out.generated = QObject::tr( "facebones cycle" );
	out.genMode = HkxClipEntry::GenSkeletonSpace;
	out.genParentTrack.resize( c.numTracks );
	out.genParentRest.resize( c.numTracks );
	out.genRestInv.resize( c.numTracks );
	for ( int t = 0; t < c.numTracks; t++ ) {
		const MorphSkelBone & b = bones[trackBone[t]];
		out.genParentTrack[t] = b.parent >= 0 ? trackOf[b.parent] : -1;
		out.genParentRest[t] = b.parent >= 0 ? bones[b.parent].restGlobal : Transform();
		out.genRestInv[t] = b.restGlobal.inverted();
	}

	if ( stats ) {
		*stats = MorphCycleStats();
		stats->regions = regionBones.count();
		stats->liveRegions = liveRegions.count();
		stats->channels = channels.count();
		stats->regionBones = allBones.count();
		stats->regionBonesFound = moved.count();
		stats->tracks = c.numTracks;
		stats->missing = missing;
		stats->source = out.skeletonSource;
	}
	sentence = QObject::tr( "%1: %2 regions, %3 channels, %4 region bones (%5 tracks with their "
							"descendants), %6 frames at 24 fps.%7 Rotation sign unproven." )
		.arg( c.name ).arg( liveRegions.count() ).arg( channels.count() ).arg( moved.count() )
		.arg( c.numTracks ).arg( c.numFrames )
		.arg( missing.isEmpty() ? QString()
								: QObject::tr( " Not in the skeleton: %1." ).arg( missing.join( QStringLiteral( ", " ) ) ) );
	return true;
}

float morphCycleChannelTime( const HkxClipEntry & e, const QString & channel, int v )
{
	if ( e.clip.annotations.isEmpty() )
		return -1.0f;
	for ( const HkxAnnotation & a : e.clip.annotations.first() ) {
		if ( a.text.compare( channel, Qt::CaseInsensitive ) != 0 )
			continue;
		const int k = v > 0 ? 6 : ( v < 0 ? 18 : 0 );
		return a.time + float( k ) * e.clip.frameDuration;
	}
	return -1.0f;
}
