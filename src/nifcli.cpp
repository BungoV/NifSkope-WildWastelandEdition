/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

See the LICENSE.md file for the full license text.

***** END LICENCE BLOCK *****/

#include "nifcli.h"

#include "gamemanager.h"
#include "nifmerge.h"
#include "spellbook.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "skeletontools.h"

#include <cmath>
#include "spells/animationsetup.h"
#include "io/pbrmfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

/*
 *  Headless batch mode. Fills the "Future command line batch tools here" slot
 *  that upstream left in main.cpp.
 *
 *  Everything here runs on the model layer only, which is what makes it
 *  possible at all: NIF blocks, links and arrays need no GL context and no
 *  viewport selection. The 195 registered spells are addressable by name
 *  through SpellBook::lookup(), so anything expressed as a spell is reachable
 *  from the command line for free.
 */

namespace
{

QTextStream & out()
{
	static QTextStream s( stdout );
	return s;
}

QTextStream & err()
{
	static QTextStream s( stderr );
	return s;
}

#ifdef Q_OS_WIN
//! The exe is linked -subsystem,windows, so it starts with no console and
//! stdout goes nowhere. Borrow the parent's console when we are a CLI.
//! Leaves an already-redirected handle (a pipe or a file) alone.
void attachParentConsole()
{
	HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
	if ( h && h != INVALID_HANDLE_VALUE )
		return;	// piped or redirected already — nothing to fix

	if ( AttachConsole( ATTACH_PARENT_PROCESS ) ) {
		FILE * f = nullptr;
		freopen_s( &f, "CONOUT$", "w", stdout );
		freopen_s( &f, "CONOUT$", "w", stderr );
	}
}
#endif

//! Silence Qt's chatter; the CLI's own output is the product.
void cliMessageHandler( QtMsgType type, const QMessageLogContext &, const QString & msg )
{
	if ( type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg )
		err() << msg << Qt::endl;
}

//! Shared init the GUI path does in main.cpp: settings identity, working
//! directory (nif.xml is resolved relative to it) and the format descriptions.
bool initModelLayer()
{
	QCoreApplication::setOrganizationName( "NifTools" );
	QCoreApplication::setOrganizationDomain( "niftools.org" );

	QDir::setCurrent( QCoreApplication::applicationDirPath() );
	qRegisterMetaType<NifValue>( "NifValue" );

	if ( !NifModel::loadXML() ) {
		err() << "error: could not load nif.xml from "
			  << QCoreApplication::applicationDirPath() << Qt::endl;
		return false;
	}
	KfmModel::loadXML();

	// NOTE: deliberately NOT calling Game::GameManager::get() here. Its init
	// builds a QProgressDialog (gamemanager.cpp:150) while scanning for game
	// installs, which is fatal without a QApplication. Nothing in the model
	// layer needs it; only resource resolution (textures, archives) does, so a
	// spell that reaches for game assets is out of scope for batch mode.
	return true;
}

//! Resolve a '/'-separated field path under a block: numeric segments index
//! arrays by row, everything else looks up by field name. Mirrors the path
//! convention the Block Details sticky state and pinned fields use.
QModelIndex resolvePath( const NifModel * nif, const QModelIndex & root, const QString & path )
{
	QModelIndex idx = root;
	if ( path.isEmpty() )
		return idx;

	for ( const QString & seg : path.split( QLatin1Char( '/' ), Qt::SkipEmptyParts ) ) {
		bool numeric = false;
		const int row = seg.toInt( &numeric );
		QModelIndex next;
		if ( numeric && nif->isArray( idx ) )
			next = nif->index( row, 0, idx );
		else
			next = nif->getIndex( idx, seg );
		if ( !next.isValid() )
			return QModelIndex();
		idx = next.sibling( next.row(), 0 );
	}
	return idx;
}

QString blockLabel( const NifModel * nif, int b )
{
	const QModelIndex iBlock = nif->getBlockIndex( b );
	const QString name = nif->get<QString>( iBlock, "Name" );
	return name.isEmpty()
		? QString( "[%1] %2" ).arg( b ).arg( nif->itemName( iBlock ) )
		: QString( "[%1] %2 '%3'" ).arg( b ).arg( nif->itemName( iBlock ), name );
}

bool loadNif( NifModel & nif, const QString & path )
{
	if ( !QFileInfo::exists( path ) ) {
		err() << "error: no such file: " << path << Qt::endl;
		return false;
	}
	if ( !nif.loadFromFile( path ) ) {
		err() << "error: failed to load " << path << Qt::endl;
		return false;
	}
	return true;
}

bool saveNif( const NifModel & nif, const QString & path )
{
	if ( path.isEmpty() ) {
		err() << "error: this command writes; pass -o <out.nif>" << Qt::endl;
		return false;
	}
	if ( !nif.saveToFile( path ) ) {
		err() << "error: failed to save " << path << Qt::endl;
		return false;
	}
	out() << "saved " << path << Qt::endl;
	return true;
}

// ---- commands -------------------------------------------------------------

int cmdSpells( const QString & pattern )
{
	// the registry keeps four disjoint lists; the union is every addressable spell
	QList<SpellPtr> all = SpellBook::spells();
	all += SpellBook::instants();
	all += SpellBook::sanitizers();
	all += SpellBook::checkers();

	QSet<QString> seen;
	QStringList lines;
	for ( const SpellPtr & sp : all ) {
		if ( !sp )
			continue;
		const QString id = sp->page().isEmpty()
			? sp->name() : sp->page() + QLatin1Char( '/' ) + sp->name();
		if ( seen.contains( id ) )
			continue;
		seen.insert( id );
		if ( !pattern.isEmpty() && !id.contains( pattern, Qt::CaseInsensitive ) )
			continue;
		QStringList tags;
		if ( sp->instant() )
			tags << QStringLiteral( "instant" );
		if ( sp->constant() )
			tags << QStringLiteral( "constant" );
		lines << QString( "  %1%2" ).arg( id, -58 )
			.arg( tags.isEmpty() ? QString() : QLatin1Char( '(' ) + tags.join( ',' ) + QLatin1Char( ')' ) );
	}
	lines.sort( Qt::CaseInsensitive );
	out() << lines.size() << " spell(s)"
		  << ( pattern.isEmpty() ? QString() : QString( " matching '%1'" ).arg( pattern ) )
		  << Qt::endl;
	for ( const QString & l : lines )
		out() << l << Qt::endl;
	return 0;
}

//! `pbrm <file.pbrm>` — parse a PBR Material Editor material and print the
//! resolved Minimal Standard slice. Exit 0 ok, 1 hard parse error, 3 valid but
//! unsupported (fail-closed), so a script can tell the three apart.
int cmdPbrm( const QString & file )
{
	const PbrmMaterial m = pbrmParseFile( file );

	if ( !m.error.isEmpty() ) {
		err() << "error: " << m.error << Qt::endl;
		return 1;
	}

	auto slotLine = [&]( const char * name, const PbrmMaterial::Slot & s ) {
		out() << QStringLiteral( "  %1 enabled=%2 valid=%3" )
			.arg( QLatin1String( name ), -12 ).arg( s.enabled ).arg( s.pathValid );
		if ( !s.path.isEmpty() )
			out() << "\n                 authored: " << s.path
			      << "\n                 lookup  : " << s.lookupPath;
		out() << Qt::endl;
	};

	out() << "envelope v" << m.envelopeVersion << "  shader: " << m.shader << Qt::endl;
	out() << "features: 0x" << Qt::hex << m.features << Qt::dec << Qt::endl;
	out() << "slots:" << Qt::endl;
	slotLine( "baseColor", m.baseColor );
	slotLine( "normal", m.normal );
	slotLine( "rmaos", m.rmaos );
	slotLine( "emissive", m.emissive );
	out() << "constants:" << Qt::endl;
	out() << "  colour     " << m.baseColorRGB[0] << " " << m.baseColorRGB[1] << " "
	      << m.baseColorRGB[2] << " (override " << m.overrideColor << ")" << Qt::endl;
	out() << "  opacity    " << m.opacity << " (override " << m.overrideOpacity << ")" << Qt::endl;
	out() << "  roughness  " << m.roughness << " (override " << m.overrideRoughness << ")" << Qt::endl;
	out() << "  metallic   " << m.metallic << " (override " << m.overrideMetallic << ")" << Qt::endl;
	out() << "  ao         " << m.ao << " (override " << m.overrideAo << ")" << Qt::endl;
	out() << "  f0         " << m.f0 << " (override " << m.overrideF0 << ")" << Qt::endl;
	out() << "  alpha      " << m.alphaCarries << Qt::endl;
	out() << "  porosity   " << m.porosity << " (override " << m.overridePorosity << ")" << Qt::endl;
	out() << "  normal str " << m.normalStrength << " (override " << m.overrideNormal
	      << ", heightInBlue " << m.heightInBlue << ", curvatureInAlpha " << m.curvatureInAlpha << ")" << Qt::endl;
	out() << "  emissive   " << m.emissiveRGB[0] << " " << m.emissiveRGB[1] << " "
	      << m.emissiveRGB[2] << " intensity " << m.emissiveIntensity << Qt::endl;

	for ( const QString & d : m.diagnostics )
		out() << "diagnostic: " << d << Qt::endl;

	if ( m.unsupported ) {
		out() << "UNSUPPORTED: valid document, outside this build's slice — fail closed" << Qt::endl;
		return 3;
	}
	out() << "OK" << Qt::endl;
	return 0;
}

/*! `pbrm-resolve <file.nif>` — for every shader property in a NIF, report which
 * `.pbrm` (if any) would be adopted and by which route. Resolution otherwise has
 * no observable effect until the PBR shader path exists, so without this it
 * could only be checked by reading the code.
 *
 * Deliberately reimplements the resolution rule rather than calling
 * BSShaderLightingProperty: that lives in the GL layer, which `-no-gui` never
 * builds. Keep the two in step — the rule is small and stated in both places.
 */
int cmdPbrmResolve( const QString & file )
{
	NifModel nif;
	if ( !nif.loadFromFile( file ) ) {
		err() << "error: cannot load " << file << Qt::endl;
		return 1;
	}

	QSettings settings;
	const bool autoReplace = settings.value( QStringLiteral( "Settings/Render/PBRM Auto Replace" ), true ).toBool();
	out() << "auto-replace: " << ( autoReplace ? "on" : "off" ) << Qt::endl;

	int shaders = 0, adopted = 0;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex iBlock = nif.getBlockIndex( b );
		if ( !nif.blockInherits( iBlock, "BSShaderProperty" ) )
			continue;
		const QString matName = nif.get<QString>( iBlock, "Name" );
		if ( matName.isEmpty() )
			continue;
		shaders++;

		QString candidate, route;
		if ( matName.endsWith( QLatin1String( ".pbrm" ), Qt::CaseInsensitive ) ) {
			candidate = matName;
			route = QStringLiteral( "direct" );
		} else if ( autoReplace
			&& ( matName.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
				|| matName.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) ) ) {
			candidate = matName.left( matName.length() - 5 ) + QStringLiteral( ".pbrm" );
			route = QStringLiteral( "same-name" );
		}

		out() << QStringLiteral( "[%1] %2" ).arg( b ).arg( matName ) << Qt::endl;
		if ( candidate.isEmpty() ) {
			out() << "      no pbrm route" << Qt::endl;
			continue;
		}

		QByteArray data;
		nif.getResourceFile( data, candidate, "materials", "" );
		if ( data.isEmpty() ) {
			out() << "      " << route << " -> " << candidate << " : not found" << Qt::endl;
			continue;
		}
		const PbrmMaterial m = pbrmParse( data );
		if ( !m.error.isEmpty() ) {
			out() << "      " << route << " -> " << candidate << " : PARSE ERROR " << m.error << Qt::endl;
		} else if ( m.unsupported ) {
			out() << "      " << route << " -> " << candidate << " : unsupported (fail closed)" << Qt::endl;
		} else {
			out() << "      " << route << " -> " << candidate
			      << " : ADOPTED, features 0x" << Qt::hex << m.features << Qt::dec << Qt::endl;
			adopted++;
		}
	}

	out() << shaders << " shader properties, " << adopted << " would adopt a pbrm" << Qt::endl;
	return 0;
}

/*! `skeleton <file> [--validate]` - SKELETON_AND_POSE_PLAN.md A.8.
 *
 * Shares skeletonAnalyse() with the Skeleton Manager dock, so the two can never
 * disagree about which nodes are bones or how much of the skin each one drives.
 *
 * `--validate` exits non-zero when a finding fires, which is the real payoff: it
 * makes this usable as a pre-export gate in a build script. Read-only - phase 1
 * writes nothing, so the plan's `--prune-unused` is deliberately absent until
 * phase 2 lands together with its bone-index remap tests.
 */
int cmdSkeleton( const QString & file, bool validateOnly )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	const SkeletonReport report = skeletonAnalyse( &nif );

	if ( !validateOnly ) {
		out() << "file    " << file << Qt::endl;
		out() << "root    "
			  << ( report.rootBlock >= 0
					? QString( "%1 [%2]" )
						.arg( nif.get<QString>( nif.getBlockIndex( report.rootBlock ), "Name" ) )
						.arg( report.rootBlock )
					: QStringLiteral( "<none>" ) )
			  << Qt::endl;
		out() << "shapes  " << report.skinnedShapes << " skinned" << Qt::endl;
		out() << QString( "%1  %2 %3 %4" )
					.arg( "bone", -44 ).arg( "shapes", 7 ).arg( "verts", 8 ).arg( "weight", 10 )
			  << Qt::endl;

		for ( const SkeletonBoneInfo & b : report.bones ) {
			// Two spaces per level: the hierarchy has to stay legible in a
			// terminal without box-drawing characters.
			const QString name = QString( b.depth * 2, QLatin1Char( ' ' ) )
				+ ( b.name.isEmpty() ? QStringLiteral( "<unnamed>" ) : b.name );
			QString tag;
			if ( b.isNotABone() )
				tag = QStringLiteral( "   (not a bone)" );
			else if ( b.isUnusedBone() )
				tag = QStringLiteral( "   UNUSED" );
			out() << QString( "%1  %2 %3 %4" )
						.arg( name, -44 )
						.arg( b.shapes, 7 )
						.arg( b.verts, 8 )
						.arg( QString::number( b.weight, 'f', 2 ), 10 )
				  << tag << Qt::endl;
		}
		out() << Qt::endl;
	}

	const int problems = report.danglingSkinBones.size() + report.duplicateNames.size();
	out() << report.bones.size() << " node(s), "
		  << ( report.deformingCount() + report.unusedCount() ) << " bone(s), "
		  << report.deformingCount() << " deforming, "
		  << report.unusedCount() << " unused" << Qt::endl;
	for ( const QString & d : report.danglingSkinBones )
		out() << "  ! " << d << Qt::endl;
	for ( const QString & n : report.duplicateNames )
		out() << "  ! duplicate node name '" << n << "'" << Qt::endl;

	if ( validateOnly )
		return problems > 0 ? 1 : 0;
	return 0;
}

int cmdSkeletonSelfTest( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	QStringList fails;
	const SkeletonReport before = skeletonAnalyse( &nif );
	if ( before.bones.size() < 3 ) {
		err() << "error: need a file with at least 3 nodes" << Qt::endl;
		return 1;
	}

	// Pick a leaf bone with no skin weight: the ops must not be tested on
	// something whose removal would need a vertex bone-index remap.
	int leaf = -1, leafParent = -1;
	for ( const SkeletonBoneInfo & b : before.bones ) {
		if ( b.parent < 0 || b.verts > 0 )
			continue;
		if ( nif.getChildLinks( b.block ).isEmpty() ) {
			leaf = b.block;
			leafParent = b.parent;
			break;
		}
	}
	if ( leaf < 0 ) {
		err() << "error: no unweighted leaf bone to test on" << Qt::endl;
		return 1;
	}
	out() << "test bone  " << nif.get<QString>( nif.getBlockIndex( leaf ), "Name" )
		  << " [" << leaf << "], parent [" << leafParent << "]" << Qt::endl;
	out() << "ragdoll    " << ( skeletonFileHasRagdoll( &nif ) ? "present" : "none" ) << Qt::endl;

	// --- reference sweep finds the parent link -------------------------------
	const QList<SkeletonBoneRef> refs = skeletonBoneRefs( &nif, leaf );
	bool sawChildLink = false;
	for ( const SkeletonBoneRef & r : refs )
		if ( r.what == QLatin1String( "child of" ) && r.block == leafParent )
			sawChildLink = true;
	if ( !sawChildLink )
		fails << "reference sweep missed the parent Children link";
	out() << "refs       " << refs.size() << Qt::endl;

	// --- rename round-trips, and by-NAME references follow -------------------
	const QString origName = nif.get<QString>( nif.getBlockIndex( leaf ), "Name" );
	QString e;
	if ( !skeletonRenameBone( &nif, leaf, QStringLiteral( "WWSelfTestBone" ), &e ) )
		fails << "rename failed: " + e;
	if ( nif.get<QString>( nif.getBlockIndex( leaf ), "Name" ) != QLatin1String( "WWSelfTestBone" ) )
		fails << "rename did not take";
	if ( !skeletonRenameBone( &nif, leaf, origName, &e ) )
		fails << "rename back failed: " + e;

	// --- flip-name is an involution where a side exists ----------------------
	for ( const char * n : { "LArm1", "RLeg2", "skin_bone_L_Eyelid_Top", "Bone.L", "LeftHand" } ) {
		const QString a = QString::fromLatin1( n );
		const QString f = skeletonFlipBoneName( a );
		if ( f == a )
			fails << QString( "flip did nothing for %1" ).arg( a );
		else if ( skeletonFlipBoneName( f ) != a )
			fails << QString( "flip not reversible: %1 -> %2 -> %3" ).arg( a, f, skeletonFlipBoneName( f ) );
	}
	// ...and leaves a midline bone alone
	if ( skeletonFlipBoneName( QStringLiteral( "Spine1" ) ) != QLatin1String( "Spine1" ) )
		fails << "flip altered a midline bone (Spine1)";

	// --- extrude adds exactly one child --------------------------------------
	const int kidsBefore = nif.getChildLinks( leaf ).size();
	const int added = skeletonAddChildBone( &nif, leaf, QStringLiteral( "WWSelfTestChild" ), 5.0f );
	if ( added < 0 )
		fails << "extrude returned -1";
	else if ( nif.getChildLinks( leaf ).size() != kidsBefore + 1 )
		fails << "extrude did not attach the new bone";

	// --- THE MUST-NOT-MOVE CHECK --------------------------------------------
	// Reparent with Keep Transform must leave the bone's world transform where it
	// was. This is the check the plan demands before any transform work ships; if
	// it fails, a rig silently deforms.
	if ( added >= 0 ) {
		const Transform worldBefore = skeletonWorldTransform( &nif, added );
		if ( !skeletonReparent( &nif, added, leafParent, true, &e ) ) {
			fails << "reparent failed: " + e;
		} else {
			const Transform worldAfter = skeletonWorldTransform( &nif, added );
			const float dT = ( worldAfter.translation - worldBefore.translation ).length();
			float dR = 0.0f;
			for ( int i = 0; i < 3; i++ )
				for ( int j = 0; j < 3; j++ )
					dR = qMax( dR, std::fabs( worldAfter.rotation( i, j ) - worldBefore.rotation( i, j ) ) );
			out() << "keep-transform drift  translation " << dT << ", rotation " << dR << Qt::endl;
			if ( dT > 0.01f )
				fails << QString( "Keep Transform moved the bone by %1 units" ).arg( dT );
			if ( dR > 0.001f )
				fails << QString( "Keep Transform rotated the bone by %1" ).arg( dR );
		}
	}

	// --- mirror: X negated, and STILL A PROPER ROTATION ---------------------
	// The determinant check is the point. Mirroring by negating one column of the
	// rotation would give det = -1 — a left-handed basis — and the mirrored bone
	// would animate the wrong way round. Conjugation (M R M) must keep det = +1.
	{
		int sided = -1;
		for ( const SkeletonBoneInfo & b : skeletonAnalyse( &nif ).bones ) {
			if ( b.name.isEmpty() || b.parent < 0 )
				continue;
			if ( skeletonFlipBoneName( b.name ) != b.name ) {
				sided = b.block;
				break;
			}
		}
		if ( sided < 0 ) {
			out() << "mirror     skipped, no L/R-named bone in this file" << Qt::endl;
		} else {
			const QString srcName = nif.get<QString>( nif.getBlockIndex( sided ), "Name" );
			const Transform srcWorld = skeletonWorldTransform( &nif, sided );
			QString me;
			const int mirrored = skeletonMirrorBone( &nif, sided, false, true, &me );
			if ( mirrored < 0 ) {
				fails << "mirror failed: " + me;
			} else {
				const QString dstName = nif.get<QString>( nif.getBlockIndex( mirrored ), "Name" );
				if ( dstName != skeletonFlipBoneName( srcName ) )
					fails << QString( "mirror named the bone %1, expected %2" )
						.arg( dstName, skeletonFlipBoneName( srcName ) );

				const Transform dstWorld = skeletonWorldTransform( &nif, mirrored );
				const float dx = std::fabs( dstWorld.translation[0] + srcWorld.translation[0] );
				const float dy = std::fabs( dstWorld.translation[1] - srcWorld.translation[1] );
				const float dz = std::fabs( dstWorld.translation[2] - srcWorld.translation[2] );

				const Matrix & m = dstWorld.rotation;
				const float det =
					  m( 0, 0 ) * ( m( 1, 1 ) * m( 2, 2 ) - m( 1, 2 ) * m( 2, 1 ) )
					- m( 0, 1 ) * ( m( 1, 0 ) * m( 2, 2 ) - m( 1, 2 ) * m( 2, 0 ) )
					+ m( 0, 2 ) * ( m( 1, 0 ) * m( 2, 1 ) - m( 1, 1 ) * m( 2, 0 ) );

				out() << "mirror     " << srcName << " -> " << dstName
					  << "  dX " << dx << " dY " << dy << " dZ " << dz
					  << "  det " << det << Qt::endl;
				if ( dx > 0.01f )
					fails << QString( "mirror did not negate X (off by %1)" ).arg( dx );
				if ( dy > 0.01f || dz > 0.01f )
					fails << QString( "mirror moved Y/Z (%1, %2)" ).arg( dy ).arg( dz );
				if ( std::fabs( det - 1.0f ) > 0.01f )
					fails << QString( "mirrored rotation is improper, det = %1 (expected +1)" ).arg( det );
			}
		}
	}

	// --- reparent refuses a cycle -------------------------------------------
	if ( skeletonReparent( &nif, leafParent, leaf, true, &e ) )
		fails << "reparent allowed a cycle (parent under its own descendant)";

	// --- dissolve adopts children, delete removes the subtree ----------------
	const int probe = skeletonAddChildBone( &nif, leaf, QStringLiteral( "WWDissolveMe" ), 4.0f );
	if ( probe >= 0 ) {
		const int grandchild = skeletonAddChildBone( &nif, probe, QStringLiteral( "WWKeepMe" ), 3.0f );
		if ( grandchild >= 0 ) {
			const Transform gcBefore = skeletonWorldTransform( &nif, grandchild );
			const QString gcName = nif.get<QString>( nif.getBlockIndex( grandchild ), "Name" );
			if ( !skeletonDissolve( &nif, probe, &e ) ) {
				fails << "dissolve failed: " + e;
			} else {
				// The grandchild must survive, now under `leaf`, and not have moved.
				int found = -1;
				for ( int c : nif.getChildLinks( leaf ) )
					if ( c >= 0 && nif.get<QString>( nif.getBlockIndex( c ), "Name" ) == gcName )
						found = c;
				if ( found < 0 ) {
					fails << "dissolve orphaned the child instead of reparenting it";
				} else {
					const float dT = ( skeletonWorldTransform( &nif, found ).translation
						- gcBefore.translation ).length();
					out() << "dissolve drift        translation " << dT << Qt::endl;
					if ( dT > 0.01f )
						fails << QString( "dissolve moved the adopted child by %1 units" ).arg( dT );
				}
			}
		}
	}

	const int nodesNow = skeletonAnalyse( &nif ).bones.size();
	out() << "nodes      " << before.bones.size() << " -> " << nodesNow << Qt::endl;

	if ( fails.isEmpty() ) {
		out() << "SELFTEST PASS" << Qt::endl;
		return 0;
	}
	for ( const QString & f : fails )
		out() << "  FAIL " << f << Qt::endl;
	out() << "SELFTEST FAIL (" << fails.size() << ")" << Qt::endl;
	return 1;
}

int cmdInfo( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	out() << "file    " << file << Qt::endl;
	out() << "version " << nif.getVersion()
		  << "  user " << nif.getUserVersion()
		  << "  bs " << nif.getBSVersion() << Qt::endl;
	out() << "blocks  " << nif.getBlockCount() << Qt::endl;

	// per-type tally, so "what is in this file" is one glance
	QHash<QString, int> tally;
	for ( int b = 0; b < nif.getBlockCount(); b++ )
		tally[nif.itemName( nif.getBlockIndex( b ) )]++;
	QStringList types = tally.keys();
	types.sort();
	for ( const QString & t : types )
		out() << QString( "  %1 x%2" ).arg( t, -40 ).arg( tally.value( t ) ) << Qt::endl;
	return 0;
}

int cmdList( const QString & file, const QString & typeFilter )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QString type = nif.itemName( nif.getBlockIndex( b ) );
		if ( !typeFilter.isEmpty() && !type.contains( typeFilter, Qt::CaseInsensitive ) )
			continue;
		out() << blockLabel( &nif, b ) << Qt::endl;
	}
	return 0;
}

void dumpRows( const NifModel * nif, const QModelIndex & parent,
			   int depth, int maxDepth, int maxRows, const QString & indent, bool showAll )
{
	const int rows = nif->rowCount( parent );
	const int shown = ( maxRows > 0 ) ? qMin( rows, maxRows ) : rows;
	for ( int r = 0; r < shown; r++ ) {
		const QModelIndex idx = nif->index( r, 0, parent );
		if ( !idx.isValid() )
			continue;
		// Skip rows whose version/condition says they are not part of THIS
		// file, exactly as the GUI's row hiding does. Without this a
		// BSVertexData row prints both precision variants of "Vertex" — the
		// live one and a zeroed dead one — which reads as corruption.
		if ( !showAll ) {
			const NifItem * item = nif->getItem( idx );
			if ( item && ( !nif->evalVersion( item ) || !nif->evalCondition( item ) ) )
				continue;
		}
		const QString name = nif->itemName( idx );
		const QString type = nif->itemStrType( idx );
		const QString val  = nif->getValue( idx ).toString();
		out() << indent << name;
		if ( !type.isEmpty() )
			out() << "  <" << type << ">";
		if ( !val.isEmpty() )
			out() << "  = " << val.left( 120 );
		out() << Qt::endl;
		if ( depth < maxDepth && nif->rowCount( idx ) > 0 )
			dumpRows( nif, idx, depth + 1, maxDepth, maxRows, indent + QStringLiteral( "  " ), showAll );
	}
	if ( shown < rows )
		out() << indent << "... " << ( rows - shown ) << " more row(s)" << Qt::endl;
}

int cmdDump( const QString & file, int block, const QString & path, int depth, int maxRows, bool showAll )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	if ( block < 0 || block >= nif.getBlockCount() ) {
		err() << "error: -b <block> required, 0.." << ( nif.getBlockCount() - 1 ) << Qt::endl;
		return 1;
	}
	const QModelIndex root = resolvePath( &nif, nif.getBlockIndex( block ), path );
	if ( !root.isValid() ) {
		err() << "error: no such field path: " << path << Qt::endl;
		return 1;
	}
	out() << blockLabel( &nif, block ) << ( path.isEmpty() ? QString() : QString( " / %1" ).arg( path ) )
		  << Qt::endl;
	dumpRows( &nif, root, 0, depth, maxRows, QStringLiteral( "  " ), showAll );
	return 0;
}

int cmdGet( const QString & file, int block, const QString & path )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	if ( block < 0 || block >= nif.getBlockCount() || path.isEmpty() ) {
		err() << "error: -b <block> and -f <field path> are both required" << Qt::endl;
		return 1;
	}
	const QModelIndex idx = resolvePath( &nif, nif.getBlockIndex( block ), path );
	if ( !idx.isValid() ) {
		err() << "error: no such field path: " << path << Qt::endl;
		return 1;
	}
	out() << nif.getValue( idx ).toString() << Qt::endl;
	return 0;
}

int cmdSet( const QString & file, int block, const QString & path,
			const QString & value, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	if ( block < 0 || block >= nif.getBlockCount() || path.isEmpty() ) {
		err() << "error: -b <block> and -f <field path> are both required" << Qt::endl;
		return 1;
	}
	const QModelIndex idx = resolvePath( &nif, nif.getBlockIndex( block ), path );
	if ( !idx.isValid() ) {
		err() << "error: no such field path: " << path << Qt::endl;
		return 1;
	}

	NifItem * item = nif.getItem( idx );
	if ( !item ) {
		err() << "error: field is not a value row: " << path << Qt::endl;
		return 1;
	}
	const QString before = nif.getValue( idx ).toString();
	NifValue v = nif.getValue( idx );

	// Compound types parse from a COMMA-separated list, and their fromString()
	// silently leaves a default-constructed (all-zero) value when the string
	// does not match — while setFromString() still reports success. Left
	// unguarded, `set -f Translation -v "X 1 Y 2 Z 3"` writes zeros and calls it
	// a win. Validate the shape of the input before letting it through.
	int wantComponents = 0;
	switch ( v.type() ) {
	case NifValue::tVector2:
		wantComponents = 2; break;
	case NifValue::tVector3:
	case NifValue::tHalfVector3:
	case NifValue::tShortVector3:
	case NifValue::tUshortVector3:
	case NifValue::tByteVector3:
	case NifValue::tColor3:
		wantComponents = 3; break;
	case NifValue::tVector4:
	case NifValue::tByteVector4:
	case NifValue::tUDecVector4:
	case NifValue::tQuat:
	case NifValue::tQuatXYZW:
	case NifValue::tColor4:
	case NifValue::tByteColor4:
		wantComponents = 4; break;
	default:
		break;
	}
	if ( wantComponents > 0 ) {
		const QStringList parts = value.split( QLatin1Char( ',' ) );
		bool shapeOk = ( parts.size() == wantComponents );
		for ( const QString & p : parts ) {
			bool numOk = false;
			p.trimmed().toFloat( &numOk );
			if ( !numOk )
				shapeOk = false;
		}
		if ( !shapeOk ) {
			err() << "error: " << nif.itemStrType( idx ) << " takes " << wantComponents
				  << " comma-separated numbers, e.g. -v \""
				  << QStringList( QVector<QString>( wantComponents, QStringLiteral( "0.0" ) ).toList() )
					 .join( QLatin1Char( ',' ) )
				  << "\"  (got: " << value << ")" << Qt::endl;
			return 1;
		}
	}

	if ( !v.setFromString( value, &nif, item ) ) {
		err() << "error: cannot parse '" << value << "' as " << nif.itemStrType( idx ) << Qt::endl;
		return 1;
	}
	if ( !nif.setItemValue( item, v ) ) {
		err() << "error: write rejected for " << path << Qt::endl;
		return 1;
	}
	out() << path << ": " << before << " -> " << nif.getValue( idx ).toString() << Qt::endl;
	return saveNif( nif, outFile ) ? 0 : 1;
}

int cmdCast( const QString & file, const QString & spellId, int block,
			 const QString & path, const QString & outFile )
{
	if ( spellId.isEmpty() ) {
		err() << "error: -s <\"Page/Name\"> is required (see: spells)" << Qt::endl;
		return 1;
	}
	SpellPtr spell = SpellBook::lookup( spellId );
	if ( !spell ) {
		err() << "error: no spell '" << spellId << "' (see: spells)" << Qt::endl;
		return 1;
	}

	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	QModelIndex target;	// empty = whole-file spell
	if ( block >= 0 ) {
		if ( block >= nif.getBlockCount() ) {
			err() << "error: block out of range 0.." << ( nif.getBlockCount() - 1 ) << Qt::endl;
			return 1;
		}
		target = resolvePath( &nif, nif.getBlockIndex( block ), path );
		if ( !target.isValid() ) {
			err() << "error: no such field path: " << path << Qt::endl;
			return 1;
		}
	}

	if ( !spell->isApplicable( &nif, target ) ) {
		err() << "error: '" << spellId << "' is not applicable to that target" << Qt::endl;
		return 1;
	}
	// A spell that prompts will block here with no window to prompt into.
	// `spells` marks the instant/constant ones, which are the safe subset.
	spell->cast( &nif, target );
	out() << "cast '" << spellId << "'"
		  << ( block >= 0 ? QString( " on block %1" ).arg( block ) : QString( " on the file" ) )
		  << Qt::endl;
	return saveNif( nif, outFile ) ? 0 : 1;
}

//! Merge other NIFs into this one (armour set + skeleton -> one poseable file).
int cmdMerge( const QString & file, const QStringList & adds, bool noDedupe,
			  const QString & outFile )
{
	if ( adds.isEmpty() ) {
		err() << "error: --add <file> is required (repeat it for each piece)" << Qt::endl;
		return 1;
	}
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int totalShapes = 0, totalReused = 0;
	for ( const QString & add : adds ) {
		NifMergeResult r;
		if ( !nifMergeFile( &nif, add, !noDedupe, r ) ) {
			err() << "error: " << r.error << Qt::endl;
			return 1;
		}
		out() << QString( "merged %1: +%2 block(s), %3 shape(s), %4 node(s) added, "
						  "%5 reused by name, %6 re-parented" )
			.arg( QFileInfo( add ).fileName() ).arg( r.blocksAdded ).arg( r.shapesAdded )
			.arg( r.nodesAdded ).arg( r.nodesReused ).arg( r.reparented ) << Qt::endl;
		totalShapes += r.shapesAdded;
		totalReused += r.nodesReused;
	}
	out() << "total: " << totalShapes << " shape(s) added, "
		  << totalReused << " node(s) shared with the target" << Qt::endl;
	if ( totalReused == 0 && !noDedupe )
		out() << "note: no nodes matched by name — the merged pieces do NOT share a\n"
			  << "      skeleton, so posing them as one rig will not work." << Qt::endl;
	return saveNif( nif, outFile ) ? 0 : 1;
}

//! Pose library: capture / apply bone transforms as one-key sequences.
int cmdPose( const QString & file, bool listOnly, const QString & saveName,
			 const QString & applyName, float blend, const QString & importOs,
			 const QString & exportOs, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	// Outfit Studio pose XML: the delta base is the file's current (bind) pose.
	if ( !importOs.isEmpty() || !exportOs.isEmpty() ) {
		// The delta base is the file's current (bind) pose. Standalone, there is
		// no separately captured rest, so export only makes sense when the file
		// is already posed relative to a DIFFERENT baseline — in practice OS
		// export is a GUI operation (pose mode captures the rest on entry). Here
		// rest == current, so a plain export writes nothing; import is the useful
		// direction (apply an OS pose onto a bind-pose skeleton).
		QHash<QString, Transform> restByName;
		QHash<int, Transform> restByBlock;
		for ( int b : AnimSetup::poseBoneNodes( &nif ) ) {
			const QString nm = nif.get<QString>( nif.getBlockIndex( b ), "Name" );
			Transform t( &nif, nif.getBlockIndex( b ) );
			restByBlock.insert( b, t );
			if ( !nm.isEmpty() )
				restByName.insert( nm, t );
		}
		QString error;
		if ( !importOs.isEmpty() ) {
			int applied = 0, missing = 0;
			if ( !AnimSetup::applyOutfitStudioPose( &nif, importOs, restByName, blend, &applied, &missing, &error ) ) {
				err() << "error: " << error << Qt::endl;
				return 1;
			}
			out() << "imported " << QFileInfo( importOs ).fileName() << ": "
				  << applied << " bone(s) posed, " << missing << " not in this skeleton" << Qt::endl;
		}
		if ( !exportOs.isEmpty() ) {
			// captured BEFORE any import above, so exporting after --import-os
			// reproduces the imported pose as deltas
			if ( !AnimSetup::writeOutfitStudioPose( &nif, exportOs,
					QFileInfo( exportOs ).completeBaseName(), restByBlock, &error ) ) {
				err() << "error: " << error << Qt::endl;
				return 1;
			}
			out() << "exported pose to " << exportOs << Qt::endl;
		}
		return outFile.isEmpty() ? 0 : ( saveNif( nif, outFile ) ? 0 : 1 );
	}

	if ( listOnly || ( saveName.isEmpty() && applyName.isEmpty() ) ) {
		const QVector<int> bones = AnimSetup::poseBoneNodes( &nif );
		out() << bones.size() << " bone(s) drive skinned geometry" << Qt::endl;
		const QStringList seqs = AnimSetup::sequenceNames( &nif );
		out() << "sequences (any of these can be applied as a pose):" << Qt::endl;
		if ( seqs.isEmpty() )
			out() << "  (none)" << Qt::endl;
		for ( const QString & s : seqs )
			out() << "  " << s << Qt::endl;
		return 0;
	}

	QString error;
	if ( !saveName.isEmpty() ) {
		if ( !AnimSetup::savePose( &nif, saveName, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << "saved pose '" << saveName << "' from "
			  << AnimSetup::poseBoneNodes( &nif ).size() << " bone(s)" << Qt::endl;
	}
	if ( !applyName.isEmpty() ) {
		if ( !AnimSetup::applyPose( &nif, applyName, blend, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << "applied pose '" << applyName << "'"
			  << ( blend < 1.0f ? QString( " at %1%" ).arg( int( blend * 100 + 0.5f ) ) : QString() )
			  << Qt::endl;
		if ( !error.isEmpty() )
			out() << "  note: " << error << Qt::endl;
	}
	return saveNif( nif, outFile ) ? 0 : 1;
}

//! Animation rigging: attach controllers and wire them into a sequence.
/*! The workflow the GUI's "Setup Controllers" dialog drives, addressable by
 *  name so it can be scripted over many nodes. */
int cmdAnimSetup( const QString & file, int block, const QStringList & controllers,
				  const QString & sequence, bool newSequence, bool standalone,
				  int effectVar, int intVar, bool listOnly, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	if ( block < 0 || block >= nif.getBlockCount() ) {
		err() << "error: -b <block> required, 0.." << ( nif.getBlockCount() - 1 ) << Qt::endl;
		return 1;
	}
	const QModelIndex iBlock = nif.getBlockIndex( block );
	const QVector<AnimSetup::CtlrOption> options = AnimSetup::controllerOptions( &nif, iBlock );

	if ( listOnly ) {
		out() << blockLabel( &nif, block ) << Qt::endl;
		if ( options.isEmpty() ) {
			out() << "  (no controllers apply to this block type)" << Qt::endl;
		} else {
			out() << "  controllers:" << Qt::endl;
			for ( const auto & o : options ) {
				QStringList extra;
				if ( o.hasEffectVar )
					extra << QStringLiteral( "--effect-var 0..9" );
				if ( o.hasIntVar )
					extra << QStringLiteral( "--int-var N" );
				out() << QString( "    %1  %2%3" ).arg( o.type, -44 ).arg( o.label )
					.arg( extra.isEmpty() ? QString() : QLatin1String( "  [" ) + extra.join( ", " ) + QLatin1Char( ']' ) )
					<< Qt::endl;
			}
		}
		const QStringList seqs = AnimSetup::sequenceNames( &nif );
		out() << "  sequences: " << ( seqs.isEmpty() ? QStringLiteral( "(none)" ) : seqs.join( QStringLiteral( ", " ) ) )
			  << Qt::endl;
		return 0;
	}

	if ( controllers.isEmpty() ) {
		err() << "error: --controller <Type> is required (see: anim-setup <file> -b N --list)" << Qt::endl;
		return 1;
	}

	// map the requested type names onto option indices
	AnimSetup::Params p;
	for ( const QString & want : controllers ) {
		int found = -1;
		for ( int i = 0; i < options.size(); i++ ) {
			if ( options.at( i ).type.compare( want, Qt::CaseInsensitive ) == 0 ) {
				found = i;
				break;
			}
		}
		if ( found < 0 ) {
			err() << "error: '" << want << "' is not available for this block; try --list" << Qt::endl;
			return 1;
		}
		p.chosen.append( found );
	}

	p.useSequence  = !standalone;
	p.newSequence  = newSequence;
	p.sequenceName = sequence;
	if ( effectVar >= 0 )
		p.effectVar = effectVar;
	if ( intVar >= 0 )
		p.intVar = intVar;

	QString error;
	if ( !AnimSetup::setupControllers( &nif, iBlock, p, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}

	out() << "rigged " << blockLabel( &nif, block ) << Qt::endl;
	out() << "  controllers: " << controllers.join( QStringLiteral( ", " ) ) << Qt::endl;
	if ( p.useSequence )
		out() << "  sequence:    " << ( p.sequenceName.isEmpty() ? QStringLiteral( "(first)" ) : p.sequenceName )
			  << ( p.newSequence ? QStringLiteral( " (created)" ) : QString() ) << Qt::endl;
	else
		out() << "  sequence:    standalone (always playing)" << Qt::endl;
	return saveNif( nif, outFile ) ? 0 : 1;
}

int usage()
{
	out() << "NifSkope headless batch mode\n\n"
		  << "  NifSkope -no-gui <command> [options]\n\n"
		  << "Commands:\n"
		  << "  spells [pattern]                        list spells addressable by name\n"
		  << "  info <file>                             version, block count, per-type tally\n"
		  << "  list <file> [-t <type>]                 block list, optionally filtered\n"
		  << "  dump <file> -b N [-f PATH] [-d DEPTH] [-n MAX] [--all]\n"
		  << "                                          print a block's fields\n"
		  << "                                          (--all also shows rows this file's\n"
		  << "                                           version/conditions exclude)\n"
		  << "  get  <file> -b N -f PATH                print one field value\n"
		  << "  set  <file> -b N -f PATH -v VALUE -o OUT\n"
		  << "                                          write one field value\n"
		  << "  cast <file> -s \"Page/Name\" [-b N] [-f PATH] -o OUT\n"
		  << "                                          run a spell\n"
		  << "  merge <file> --add OTHER.nif [--add ...] [--no-dedupe] -o OUT\n"
		  << "                                          merge NIFs into one; NiNodes with\n"
		  << "                                          matching names are SHARED, so the\n"
		  << "                                          pieces pose as one skeleton\n"
		  << "  skeleton <file>                         skeleton tree, which nodes are\n"
		  << "                                          bones, and per-bone influence\n"
		  << "  skeleton <file> --validate              findings only; exit 1 if any fire\n"
		  << "  pose <file> --list                      bones and existing poses\n"
		  << "  pose <file> --save NAME -o OUT          capture the current bone\n"
		  << "                                          transforms as a pose\n"
		  << "  pose <file> --apply NAME [--blend F] -o OUT\n"
		  << "                                          write a pose onto the bones; F<1\n"
		  << "                                          blends from the current pose (0..1)\n"
		  << "  pose <file> --import-os POSE.xml [--blend F] -o OUT\n"
		  << "                                          apply an Outfit Studio pose (.xml)\n"
		  << "  pose <file> --export-os POSE.xml        save current pose as Outfit Studio .xml\n"
		  << "  anim-setup <file> -b N --list           what can be rigged on this block\n"
		  << "  anim-setup <file> -b N --controller TYPE [--controller TYPE ...]\n"
		  << "        [--sequence NAME] [--new-sequence] [--standalone]\n"
		  << "        [--effect-var 0..9] [--int-var N] -o OUT\n"
		  << "                                          attach controllers and wire them\n"
		  << "                                          into a NiControllerSequence\n\n"
		  << "Field paths are '/'-separated; numeric segments index arrays by row,\n"
		  << "e.g. -f \"Vertex Data/0/Vertex Colors\".\n\n"
		  << "Scope: spells and model edits only. Viewport modelling tools (extrude,\n"
		  << "loop cut, join...) live on GLView and need a GL context, so they are not\n"
		  << "reachable from here.\n";
	return 0;
}

} // namespace

int nifskopeCliMain( const QStringList & args )
{
#ifdef Q_OS_WIN
	attachParentConsole();
#endif
	qInstallMessageHandler( cliMessageHandler );

	// strip the program name and the -no-gui marker that selected this path
	QStringList a = args.mid( 1 );
	a.removeAll( QStringLiteral( "-no-gui" ) );

	if ( a.isEmpty() || a.first() == QLatin1String( "-h" )
		 || a.first() == QLatin1String( "--help" ) || a.first() == QLatin1String( "help" ) ) {
		usage();
		out().flush();
		return 0;
	}

	const QString cmd = a.takeFirst();

	// options
	QString file, path, value, outFile, spellId, type, pattern, sequence;
	QStringList controllers, adds;
	QString saveName, applyName, importOs, exportOs;
	float blend = 1.0f;
	bool noDedupe = false;
	int block = -1, depth = 2, maxRows = 40;
	int effectVar = -1, intVar = -1;
	bool showAll = false, newSequence = false, standalone = false, listOnly = false;
	bool validateOnly = false;
	bool selfTest = false;
	for ( int i = 0; i < a.size(); i++ ) {
		const QString & t = a.at( i );
		auto next = [&]() -> QString { return ( i + 1 < a.size() ) ? a.at( ++i ) : QString(); };
		if ( t == QLatin1String( "-b" ) )      block   = next().toInt();
		else if ( t == QLatin1String( "-f" ) ) path    = next();
		else if ( t == QLatin1String( "-v" ) ) value   = next();
		else if ( t == QLatin1String( "-o" ) ) outFile = next();
		else if ( t == QLatin1String( "-s" ) ) spellId = next();
		else if ( t == QLatin1String( "-t" ) ) type    = next();
		else if ( t == QLatin1String( "-d" ) ) depth   = next().toInt();
		else if ( t == QLatin1String( "-n" ) ) maxRows = next().toInt();
		else if ( t == QLatin1String( "--all" ) ) showAll = true;
		else if ( t == QLatin1String( "--controller" ) ) controllers << next();
		else if ( t == QLatin1String( "--sequence" ) ) sequence = next();
		else if ( t == QLatin1String( "--new-sequence" ) ) newSequence = true;
		else if ( t == QLatin1String( "--standalone" ) ) standalone = true;
		else if ( t == QLatin1String( "--effect-var" ) ) effectVar = next().toInt();
		else if ( t == QLatin1String( "--int-var" ) ) intVar = next().toInt();
		else if ( t == QLatin1String( "--list" ) ) listOnly = true;
		else if ( t == QLatin1String( "--validate" ) ) validateOnly = true;
		else if ( t == QLatin1String( "--selftest" ) ) selfTest = true;
		else if ( t == QLatin1String( "--add" ) ) adds << QDir::current().filePath( next() );
		else if ( t == QLatin1String( "--no-dedupe" ) ) noDedupe = true;
		else if ( t == QLatin1String( "--save" ) ) saveName = next();
		else if ( t == QLatin1String( "--apply" ) ) applyName = next();
		else if ( t == QLatin1String( "--blend" ) ) blend = next().toFloat();
		else if ( t == QLatin1String( "--import-os" ) ) importOs = QDir::current().filePath( next() );
		else if ( t == QLatin1String( "--export-os" ) ) exportOs = QDir::current().filePath( next() );
		else if ( t.startsWith( QLatin1Char( '-' ) ) ) {
			err() << "error: unknown option " << t << Qt::endl;
			err().flush();
			return 2;
		} else if ( file.isEmpty() && cmd != QLatin1String( "spells" ) ) {
			file = QDir::current().filePath( t );
		} else {
			pattern = t;
		}
	}

	if ( cmd == QLatin1String( "spells" ) ) {
		// spells needs no file, but does need the format descriptions loaded
		if ( !initModelLayer() ) { err().flush(); return 1; }
		const int rc = cmdSpells( pattern );
		out().flush();
		return rc;
	}

	if ( file.isEmpty() ) {
		err() << "error: '" << cmd << "' needs a <file>" << Qt::endl;
		err().flush();
		return 2;
	}
	// pbrm reads a standalone material rather than a NIF, so it runs before the
	// model layer is brought up — it needs no nif.xml.
	if ( cmd == QLatin1String( "pbrm" ) ) {
		const int rc = cmdPbrm( file );
		out().flush();
		err().flush();
		return rc;
	}

	if ( !initModelLayer() ) { err().flush(); return 1; }

	int rc = 2;
	if ( cmd == QLatin1String( "pbrm-resolve" ) )
		rc = cmdPbrmResolve( file );
	else if ( cmd == QLatin1String( "info" ) )
		rc = cmdInfo( file );
	else if ( cmd == QLatin1String( "list" ) )
		rc = cmdList( file, type );
	else if ( cmd == QLatin1String( "dump" ) )
		rc = cmdDump( file, block, path, depth, maxRows, showAll );
	else if ( cmd == QLatin1String( "get" ) )
		rc = cmdGet( file, block, path );
	else if ( cmd == QLatin1String( "set" ) )
		rc = cmdSet( file, block, path, value, outFile );
	else if ( cmd == QLatin1String( "cast" ) )
		rc = cmdCast( file, spellId, block, path, outFile );
	else if ( cmd == QLatin1String( "merge" ) )
		rc = cmdMerge( file, adds, noDedupe, outFile );
	else if ( cmd == QLatin1String( "pose" ) )
		rc = cmdPose( file, listOnly, saveName, applyName, blend, importOs, exportOs, outFile );
	else if ( cmd == QLatin1String( "skeleton" ) )
		rc = selfTest ? cmdSkeletonSelfTest( file ) : cmdSkeleton( file, validateOnly );
	else if ( cmd == QLatin1String( "anim-setup" ) )
		rc = cmdAnimSetup( file, block, controllers, sequence, newSequence,
						   standalone, effectVar, intVar, listOnly, outFile );
	else {
		err() << "error: unknown command '" << cmd << "'" << Qt::endl;
		usage();
	}

	out().flush();
	err().flush();
	return rc;
}
