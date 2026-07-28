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
#include "starterscene.h"
#include "gl/hknpdecode.h"
#include "gl/hknpencode.h"
#include "physics/ragdollsim.h"

#include <bit>
#include <cmath>
#include <QtEndian>
#include "spells/animationsetup.h"
#include "io/pbrmfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
/*! Collision inventory, from the same decode the Collision Manager reads.
 *
 * Prints the binding chain that matters for compiled collision: node ->
 * bhkNPCollisionObject -> "Body ID" -> system, then what hknpDecode found in
 * each system (bodies, shapes, and which body each shape says it belongs to).
 *
 * The per-shape body id is the interesting column. A skeleton's ragdoll has one
 * collision object per bone, so if the shapes come back with distinct body ids
 * the decode preserves per-bone attribution; if they all come back -1 the
 * shapes survive but their bone association does not, and anything presenting
 * them as *bone* collision has to rebuild it some other way.
 */
/*! Run the ragdoll solver headlessly and report whether it stayed sane.
 *
 * Stability is the whole question for a ragdoll solver, and it is not something
 * to judge by looking at a viewport: jitter and slow joint drift are invisible
 * until they are catastrophic. So this reports kinetic energy (blow-up shows as
 * energy climbing instead of settling), the worst ball-socket separation (joint
 * drift) and the peak speed, and exits non-zero if anything diverged.
 */
/*! How far the NIF's node placement differs from the packfile's rest pose.
 *
 * This began as a test of a viewport composition that turned out to be wrong, and
 * is kept because of what it found. Each body's collision is drawn in its own
 * node's space, so the renderer's transform for body i is worldTrans(node_i),
 * while the solver holds that body's rest pose in the ragdoll's own space. It
 * would be convenient if worldTrans(node_i) * rest_i^-1 came out the same for
 * every body -- one ragdoll, one scene, one map between them.
 *
 * It does not. The brahmin and the human agree to 0.0006 and 0.0003 game units,
 * but the deathclaw is out by 14.3 and the turret by 47.1 -- and 47.1 game units
 * is 0.672 Havok metres, exactly the rest-pose pivot error 07-28h measured on that
 * same turret. Rotations agree everywhere to 1e-5, so it is purely translation.
 * glnode.cpp already says as much for stair helpers: the node transform is
 * authoritative for placement and cinfo's position is only a rest pose.
 *
 * So there is no single ragdoll-to-scene map, and the viewport must not use one.
 * The formulation that works is per-body RELATIVE motion:
 *
 *     T_draw_i = worldTrans(node_i) * ( rest_i^-1 * sim_i )
 *
 * which keeps each body's authoritative placement and applies only how far the
 * solver has moved it since rest. At rest the bracket is the identity and the
 * simulated draw is byte-for-byte the static draw -- the property worth having,
 * and one that holds however far the two disagree.
 */
static void checkSceneBridge( NifModel & nif, const RagdollSim & sim, qint32 sysBlock )
{
	const float SC = 69.99125f;

	// walk parents to get each node's world transform, as the renderer does
	auto worldOf = [&nif]( QModelIndex iNode ) {
		Transform t;
		for ( QModelIndex i = iNode; i.isValid(); ) {
			t = Transform( &nif, i ) * t;
			const qint32 p = nif.getParent( nif.getBlockNumber( i ) );
			i = ( p >= 0 ) ? nif.getBlockIndex( p ) : QModelIndex();
		}
		return t;
	};

	QVector<Transform> maps;
	QVector<int> mapBody;
	for ( qint32 b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex iObj = nif.getBlockIndex( b );
		if ( !nif.blockInherits( iObj, "bhkNPCollisionObject" ) )
			continue;
		if ( nif.getLink( iObj, "Data" ) != sysBlock )
			continue;
		const int body = int( nif.get<quint32>( iObj, "Body ID" ) );
		if ( body < 0 || body >= sim.bodies().size() )
			continue;
		const QModelIndex iTarget = nif.getBlockIndex( nif.getLink( iObj, "Target" ) );
		if ( !iTarget.isValid() )
			continue;
		const SimBody & sb = sim.bodies().at( body );

		// rest_i in ragdoll space, scaled to game units so it composes with the node
		Transform rest;
		rest.rotation.fromQuat( sb.q );
		rest.translation = sb.restOrigin * SC;

		// worldTrans(node) * rest^-1
		Transform inv;
		inv.rotation = rest.rotation.inverted();
		inv.translation = -( inv.rotation * rest.translation );
		maps.append( worldOf( iTarget ) * inv );
		mapBody.append( body );
	}

	if ( maps.size() < 2 ) {
		out() << "  node placement vs rest pose: only " << maps.size()
			  << " body/node binding(s), nothing to cross-check" << Qt::endl;
		return;
	}

	float worstT = 0.0f, worstR = 0.0f;
	int worstAt = -1;
	for ( int i = 1; i < maps.size(); i++ ) {
		const float dt = ( maps.at( i ).translation - maps.at( 0 ).translation ).length();
		float dr = 0.0f;
		for ( int r = 0; r < 3; r++ )
			for ( int c = 0; c < 3; c++ )
				dr = std::max( dr, std::fabs( maps.at( i ).rotation( r, c )
					- maps.at( 0 ).rotation( r, c ) ) );
		if ( dt > worstT || dr > worstR ) {
			if ( dt > worstT ) worstT = dt;
			if ( dr > worstR ) worstR = dr;
			worstAt = mapBody.at( i );
		}
	}
	out() << QString( "  node placement vs packfile rest pose: %1 bindings, spread "
					  "%2 game units / %3 in rotation (worst body %4)" )
				.arg( maps.size() ).arg( worstT, 0, 'f', 4 ).arg( worstR, 0, 'f', 5 )
				.arg( worstAt ) << Qt::endl;
}

int cmdSimulate( const QString & file, int steps, int substeps, int iterations, bool noLimits,
	const QString & onlyLimit, bool ground, bool noSelf, bool drop, bool jointedOnly,
	int dragBody, bool selfTest, bool verbose )
{
	if ( selfTest || file.isEmpty() ) {
		// Two bodies, one joint, damping off: total energy is a conserved
		// quantity, so any drift is the solver's own error and no decode is
		// involved. A correct solver also drifts LESS as substeps rise.
		/* The property that matters is boundedness: a preview solver may bleed or
		 * gain a little energy over ten seconds, but it must not run away. So the
		 * verdict is drawn at 25%, while the printed drift shows the finer
		 * behaviour -- in particular whether a case converges as substeps rise,
		 * which is what separates discretisation error from a modelling mistake.
		 */
		out() << "solver self-test: synthetic rigs, damping off, 600 steps"
			  << Qt::endl
			  << "  energy drift per rig; a sound case shrinks as substeps rise, "
				 "and none may run away (>25%)"
			  << Qt::endl << Qt::endl;
		static const char * const cases[] = { "pendulum", "chain3", "chain8", "fork",
											  "heavy", "chain8h", "forkh", "spun" };
		static const int subs[] = { 4, 8, 16, 32, 64 };
		out() << "  " << ( ( iterations > 0 ) ? iterations : RagdollSim().iterations )
			  << " solver sweep(s) per substep" << Qt::endl << Qt::endl;

		out() << QString( "  %1" ).arg( "case", -10 );
		for ( int ss : subs )
			out() << QString( "%1" ).arg( QString( "ss=%1" ).arg( ss ), 12 );
		out() << Qt::endl;

		int bad = 0;
		for ( const char * name : cases ) {
			out() << QString( "  %1" ).arg( QLatin1String( name ), -10 );
			for ( int ss : subs ) {
				RagdollSim sim;
				if ( !sim.buildTestCase( QLatin1String( name ) ) ) {
					out() << QString( "%1" ).arg( "n/a", 12 );
					continue;
				}
				sim.damping = 0.0f;
				if ( iterations > 0 )
					sim.iterations = iterations;
				const float e0 = sim.totalEnergy();
				for ( int i = 0; i < 600; i++ )
					sim.step( 1.0f / 60.0f, ss );
				const float e1 = sim.totalEnergy();
				const float drift = ( e0 != 0.0f ) ? ( e1 - e0 ) / std::fabs( e0 ) : 0.0f;
				if ( !std::isfinite( e1 ) ) {
					out() << QString( "%1" ).arg( "NaN", 12 );
					bad++;
				} else {
					out() << QString( "%1" ).arg(
						QString::number( drift * 100.0f, 'f', 2 ) + "%", 12 );
					if ( std::fabs( drift ) > 0.25f )
						bad++;
				}
			}
			out() << Qt::endl;
		}
		out() << Qt::endl
			  << ( bad ? QString( "  FAIL: energy ran away in %1 run(s)" ).arg( bad )
					   : QString( "  ok: every rig stayed bounded within 25%" ) ) << Qt::endl;

		/* Contacts dissipate, so energy conservation says nothing about them. The
		 * property that matters is that a box dropped on the plane comes to rest
		 * on it: neither sinking through nor being thrown off.
		 */
		out() << Qt::endl << "contact self-test: 1 kg box dropped on the plane"
			  << Qt::endl;
		int cbad = 0;
		for ( int ss : subs ) {
			RagdollSim sim;
			sim.buildTestCase( QStringLiteral( "box" ) );
			for ( int i = 0; i < 180; i++ )
				sim.step( 1.0f / 60.0f, ss );
			const SimStats st = sim.stats();
			const bool ok = std::isfinite( st.maxSpeed ) && st.maxSpeed < 0.1f
				&& st.maxPenetration < 0.001f;
			out() << QString( "  ss=%1  speed %2 m/s  penetration %3 m  %4" ).arg( ss, -4 )
						.arg( st.maxSpeed, 8, 'f', 4 ).arg( st.maxPenetration, 9, 'f', 6 )
						.arg( ok ? "ok" : "FAIL" ) << Qt::endl;
			if ( !ok )
				cbad++;
		}
		out() << ( cbad ? QString( "  FAIL: the box did not settle in %1 run(s)" ).arg( cbad )
						: QString( "  ok: the box settles on the plane" ) ) << Qt::endl;
		bad += cbad;
		return bad ? 1 : 0;
	}

	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int simulated = 0, failed = 0;
	for ( qint32 b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex iSys = nif.getBlockIndex( b );
		if ( !nif.blockInherits( iSys, "bhkPhysicsSystem" )
			&& !nif.blockInherits( iSys, "bhkRagdollSystem" ) )
			continue;
		const HknpSystem sys = hknpDecode( nif.get<QByteArray>( iSys, "Binary Data" ) );
		if ( !sys.valid || sys.constraints.isEmpty() )
			continue;

		RagdollSim sim;
		QString error;
		if ( !sim.build( sys, &error ) ) {
			out() << "system " << b << ": " << error << Qt::endl;
			failed++;
			continue;
		}
		// hold the root so the ragdoll hangs rather than falling out of the
		// world -- what we are testing is the joints, not gravity
		sim.angularLimits = !noLimits;
		sim.selfCollision = !noSelf;
		if ( iterations > 0 )
			sim.iterations = iterations;
		if ( !onlyLimit.isEmpty() ) {
			sim.useTwist = ( onlyLimit == QLatin1String( "twist" ) );
			sim.useCone  = ( onlyLimit == QLatin1String( "cone" ) );
			sim.usePlane = ( onlyLimit == QLatin1String( "plane" ) );
			sim.useHinge = ( onlyLimit == QLatin1String( "hinge" ) );
		}
		if ( ground || drop ) {
			/* Put the plane just under the lowest body so the ragdoll starts
			 * clear and falls onto it, rather than starting half buried and
			 * being shoved out -- the latter tests the push-out code, not the
			 * collision.
			 */
			sim.ground = true;
			sim.groundZ = sim.lowestPoint() - 0.25f;
			if ( verbose ) {
				out() << QString( "  ground at z %1 (lowest geometry %2)" )
							.arg( sim.groundZ, 0, 'f', 4 ).arg( sim.lowestPoint(), 0, 'f', 4 )
					  << Qt::endl;
				for ( int k = 0; k < sim.bodies().size(); k++ ) {
					const SimBody & sb = sim.bodies().at( k );
					out() << QString( "    body %1  shapes %2  points %3  invMass %4  "
									  "pinned %5  z %6" )
								.arg( k ).arg( sb.shapeCount ).arg( sb.points.size() )
								.arg( sb.invMass, 0, 'f', 4 ).arg( sb.pinned ? "yes" : "no" )
								.arg( sb.x[2], 0, 'f', 3 ) << Qt::endl;
				}
			}
		}
		/* --drop lets the whole thing fall; otherwise hold the root so what is
		 * under test is the joints rather than gravity.
		 *
		 * Dragging holds nothing else. Pinning the root as well as the grabbed
		 * body asks the chain between them to span whatever distance the drag
		 * covers, and once that exceeds the limb's reach the joints simply cannot
		 * be satisfied -- which is a fact about arms, not a solver failure. A real
		 * drag grabs one body and lets the rest dangle from it.
		 */
		if ( !drop && dragBody < 0 )
			sim.setPinned( 0, true );

		checkSceneBridge( nif, sim, b );

		const SimStats before = sim.stats();
		out() << Qt::endl << "system " << b << "   " << sim.bodies().size()
			  << " bodies, " << sim.joints().size() << " joints" << Qt::endl;

		/* The rest pose is the ragdoll's neutral stance, so a limit reported
		 * violated there means the decoded bounds and the measured angle are not
		 * in the same convention -- worth knowing before blaming the solver.
		 */
		{
			const QVector<SimLimitCheck> lim = sim.checkLimits();
			int viol = 0;
			for ( const SimLimitCheck & c : lim )
				if ( c.any() )
					viol++;
			out() << "  limits violated at rest: " << viol << " of " << lim.size()
				  << " joints" << Qt::endl;
			if ( verbose ) {
				auto deg = []( float r ) { return r * 57.2957795f; };
				for ( const SimLimitCheck & c : lim ) {
					if ( !c.any() )
						continue;
					const SimJoint & sj = sim.joints().at( c.joint );
					// angle against the bounds it failed, so an impossible bound
					// (min above max) is visible rather than inferred
					auto one = [&]( const char * nm, float v, const HknpAngLimit & l ) {
						return QString( "%1 %2 not in [%3, %4]" ).arg( QLatin1String( nm ) )
							.arg( deg( v ), 0, 'f', 1 ).arg( deg( l.min ), 0, 'f', 1 )
							.arg( deg( l.max ), 0, 'f', 1 );
					};
					QStringList w;
					if ( c.twistBad ) w << one( "twist", c.twist, sj.twist );
					if ( c.coneBad )  w << one( "cone", c.cone, sj.cone );
					if ( c.planeBad ) w << one( "plane", c.plane, sj.plane );
					if ( c.hingeBad ) w << one( "hinge", c.hinge, sj.hinge );
					out() << QString( "    joint %1 (%2 <- %3): %4" ).arg( c.joint )
								.arg( c.child ).arg( c.parent ).arg( w.join( ", " ) ) << Qt::endl;
				}
			}
		}

		if ( verbose ) {
			/* Which joints do not hold in the rest pose, and what KIND they are.
			 * A healthy ragdoll starts at 1e-6; anything above a millimetre means
			 * the decoded pivots and the body poses disagree, and grouping by
			 * class name is what shows whether one constraint type is at fault.
			 */
			/* Does the constraint data describe a DIFFERENT pose, or no coherent
			 * pose at all? Reconstructing from the joint frames alone answers it:
			 * a small spread means the ragdoll was authored against another bind
			 * pose, a large scattered one means the data is simply inconsistent.
			 */
			{
				const QVector<SimPoseCheck> pc = sim.checkPoseFromJoints();
				int placed = 0;
				float worstPos = 0.0f, worstRot = 0.0f, sumPos = 0.0f;
				for ( const SimPoseCheck & c : pc ) {
					if ( !c.placed )
						continue;
					placed++;
					sumPos += c.posDiff;
					worstPos = std::max( worstPos, c.posDiff );
					worstRot = std::max( worstRot, c.rotDiffDeg );
				}
				out() << QString( "  pose rebuilt from the constraints: %1/%2 bodies "
								  "placed, mean %3 m, worst %4 m / %5 deg" )
							.arg( placed ).arg( pc.size() )
							.arg( placed ? sumPos / float( placed ) : 0.0f, 0, 'f', 4 )
							.arg( worstPos, 0, 'f', 4 ).arg( worstRot, 0, 'f', 1 )
					  << Qt::endl;
			}

			int shown = 0;
			for ( int k = 0; k < sim.joints().size(); k++ ) {
				const SimJoint & sj = sim.joints().at( k );
				const SimBody & A = sim.bodies().at( sj.a );
				const SimBody & B = sim.bodies().at( sj.b );
				auto rot = []( const Quat & q, const Vector3 & v ) {
					const Vector3 u( q[1], q[2], q[3] );
					const Vector3 uv = Vector3::crossproduct( u, v );
					return v + ( uv * q[0] + Vector3::crossproduct( u, uv ) ) * 2.0f;
				};
				const float sep = ( ( A.x + rot( A.q, sj.pivotA ) )
					- ( B.x + rot( B.q, sj.pivotB ) ) ).length();
				if ( sep < 0.001f )
					continue;
				if ( !shown++ )
					out() << "  joints not holding at rest:" << Qt::endl;
				auto p3 = []( const Vector3 & v ) {
					return QString( "%1,%2,%3" ).arg( v[0], 7, 'f', 3 )
						.arg( v[1], 7, 'f', 3 ).arg( v[2], 7, 'f', 3 );
				};
				// pivots are printed back in BONE space (undoing the centre-of-mass
				// rebase) so they can be compared against the file directly
				out() << QString( "    joint %1 (%2 <- %3) sep %4  pivotA %5  pivotB %6  %7" )
							.arg( k ).arg( sj.a ).arg( sj.b ).arg( sep, 0, 'f', 4 )
							.arg( p3( sj.pivotA + A.com ) ).arg( p3( sj.pivotB + B.com ) )
							.arg( sj.kind ) << Qt::endl;
			}

			// a centre of mass is a short hop along the bone; anything the size of
			// the whole skeleton would mean it is an absolute position instead
			out() << QString( "  %1 %2 %3 %4 %5 %6" ).arg( "body", -6 )
						.arg( "bone origin", 26 ).arg( "cinfo position", 26 )
						.arg( "|posDiff|", 10 ).arg( "quatNorm", 10 ).arg( "rotDiffDeg", 11 )
				  << Qt::endl;
			for ( int k = 0; k < sim.bodies().size(); k++ ) {
				const SimBody & sb = sim.bodies().at( k );
				auto v3 = []( const Vector3 & v ) {
					return QString( "%1,%2,%3" ).arg( v[0], 7, 'f', 3 )
						.arg( v[1], 7, 'f', 3 ).arg( v[2], 7, 'f', 3 );
				};
				const Quat & cq = sb.cinfoRot;
				const float qn = std::sqrt( cq[0] * cq[0] + cq[1] * cq[1]
					+ cq[2] * cq[2] + cq[3] * cq[3] );
				/* Angle between what cinfo says the body's orientation is and what
				 * accumulating the skeleton produced. The position agrees exactly
				 * on every model tested, so if a ragdoll starts with its joints
				 * violated this is where it has to be coming from.
				 */
				const Quat & sq = sb.q;
				float dot = cq[0] * sq[0] + cq[1] * sq[1] + cq[2] * sq[2] + cq[3] * sq[3];
				const float rotDiff = 2.0f * std::acos(
					std::clamp( std::fabs( dot ), 0.0f, 1.0f ) ) * 57.2957795f;
				out() << QString( "  %1 %2 %3 %4 %5 %6" ).arg( k, -6 )
							.arg( v3( sb.restOrigin ), 26 ).arg( v3( sb.cinfoPos ), 26 )
							.arg( ( sb.cinfoPos - sb.restOrigin ).length(), 10, 'f', 4 )
							.arg( qn, 10, 'f', 5 ).arg( rotDiff, 11, 'f', 2 ) << Qt::endl;
			}
		}

		if ( sim.looseBodies() )
			out() << "  bodies no joint touches (a parts kit, not one ragdoll): "
				  << sim.looseBodies() << " of " << sim.bodies().size() << Qt::endl;
		if ( jointedOnly )
			sim.pinLooseBodies();
		out() << "  collision pairs excluded as overlapping at rest: "
			  << sim.restOverlaps() << Qt::endl;
		if ( sim.rebasedJoints() )
			out() << "  joints whose parent pivot the file left unset, derived from "
				  << "the rest pose: " << sim.rebasedJoints() << Qt::endl;
		out() << QString( "  %1 %2 %3 %4 %5 %6" ).arg( "step", -8 ).arg( "energy", 12 )
					.arg( "maxJointErr", 13 ).arg( "maxSpeed", 11 ).arg( "contacts", 9 )
					.arg( "maxPenetr", 11 ) << Qt::endl;
		out() << QString( "  %1 %2 %3 %4 %5 %6" ).arg( 0, -8 ).arg( before.energy, 12, 'f', 5 )
					.arg( before.maxJointError, 13, 'f', 6 ).arg( before.maxSpeed, 11, 'f', 4 )
					.arg( before.contacts, 9 ).arg( before.maxPenetration, 11, 'f', 6 )
			  << Qt::endl;

		/* Dragging a bone, which is what Physics Sim mode is for.
		 *
		 * The mechanic is the whole of XPBD's appeal here: pin the body, move it
		 * where the cursor is, and the solver resolves the rest of the ragdoll
		 * around it. No spring constant, no tuning, nothing to go unstable -- a
		 * pinned body simply has infinite mass for the substep.
		 *
		 * Testing it needs no window. Sweeping the grabbed body along a circle and
		 * watching the joints exercises exactly the code the mouse would drive,
		 * and reports whether the ragdoll follows or comes apart.
		 */
		Vector3 dragFrom;
		float dragR = 0.0f;
		if ( dragBody >= 0 && dragBody < sim.bodies().size() ) {
			sim.setPinned( dragBody, true );
			dragFrom = sim.bodies().at( dragBody ).x;
			// a quarter of the ragdoll's own height, so the pull is substantial
			// without being absurd for a cat or a Liberty Prime alike
			float lo = dragFrom[2], hi = dragFrom[2];
			for ( const SimBody & sb : sim.bodies() ) {
				lo = std::min( lo, sb.x[2] );
				hi = std::max( hi, sb.x[2] );
			}
			// a tenth of the ragdoll's height: a firm pull, well inside any limb's reach
			dragR = std::max( 0.05f, ( hi - lo ) * 0.10f );
			out() << QString( "  dragging body %1 in a %2 m circle" ).arg( dragBody )
						.arg( dragR, 0, 'f', 3 ) << Qt::endl;
		}

		SimStats st;
		/* Record the first step where the kinetic energy takes off. A blow-up
		 * always starts at one body: reporting the ragdoll's total tells us it
		 * broke, reporting where tells us why.
		 */
		SimStats onset;
		int onsetStep = -1;
		for ( int i = 0; i < steps; i++ ) {
			if ( dragR > 0.0f ) {
				// two seconds a lap, the speed a hand actually moves
				const float ang = 2.0f * float( M_PI ) * float( i ) / 120.0f;
				sim.setPosition( dragBody, dragFrom
					+ Vector3( std::cos( ang ) - 1.0f, std::sin( ang ), 0.0f ) * dragR );
			}
			sim.step( 1.0f / 60.0f, substeps );
			st = sim.stats();
			// 50 m/s: a hanging ragdoll swings at a few m/s, so this is well
			// clear of honest motion and fires only on a genuine runaway
			if ( onsetStep < 0 && st.maxSpeed > 50.0f ) {
				onset = st;
				onsetStep = i + 1;
			}
			if ( verbose || i == steps / 4 || i == steps / 2 || i == steps - 1 ) {
				out() << QString( "  %1 %2 %3 %4 %5 %6" ).arg( i + 1, -8 )
							.arg( st.energy, 12, 'f', 5 ).arg( st.maxJointError, 13, 'f', 6 )
							.arg( st.maxSpeed, 11, 'f', 4 ).arg( st.contacts, 9 )
							.arg( st.maxPenetration, 11, 'f', 6 ) << Qt::endl;
			}
			if ( st.diverged )
				break;
		}

		if ( onsetStep >= 0 && onset.worstBody >= 0 ) {
			const SimBody & b = sim.bodies().at( onset.worstBody );
			out() << QString( "  runaway: step %1, body %2 at %3 m/s"
							  "  invMass %4  invInertia %5,%6,%7  com %8,%9,%10" )
						.arg( onsetStep ).arg( onset.worstBody )
						.arg( onset.maxSpeed, 0, 'f', 2 ).arg( b.invMass, 0, 'f', 3 )
						.arg( b.invInertia[0], 0, 'f', 2 ).arg( b.invInertia[1], 0, 'f', 2 )
						.arg( b.invInertia[2], 0, 'f', 2 )
						.arg( b.com[0], 0, 'f', 3 ).arg( b.com[1], 0, 'f', 3 )
						.arg( b.com[2], 0, 'f', 3 ) << Qt::endl;
			// every joint that touches it, so the culprit constraint is named
			for ( int k = 0; k < sim.joints().size(); k++ ) {
				const SimJoint & j = sim.joints().at( k );
				if ( j.a != onset.worstBody && j.b != onset.worstBody )
					continue;
				out() << QString( "    joint %1: child %2 <- parent %3   "
								  "pivotA %4,%5,%6  pivotB %7,%8,%9" )
							.arg( k ).arg( j.a ).arg( j.b )
							.arg( j.pivotA[0], 0, 'f', 3 ).arg( j.pivotA[1], 0, 'f', 3 )
							.arg( j.pivotA[2], 0, 'f', 3 )
							.arg( j.pivotB[0], 0, 'f', 3 ).arg( j.pivotB[1], 0, 'f', 3 )
							.arg( j.pivotB[2], 0, 'f', 3 ) << Qt::endl;
			}
		}
		simulated++;
		if ( st.diverged ) {
			out() << "  DIVERGED" << Qt::endl;
			failed++;
		} else {
			// speed, not energy, is the honest settling test: energy scales with
			// mass, and Liberty Prime massing tens of tonnes reads as a blow-up
			// next to a cat while moving no faster
			out() << "  settled: maxSpeed " << QString::number( st.maxSpeed, 'f', 4 )
				  << ", energy " << QString::number( st.energy, 'f', 5 )
				  << ", worst joint separation " << QString::number( st.maxJointError, 'f', 6 )
				  << ", " << st.contacts << " contacts, worst penetration "
				  << QString::number( st.maxPenetration, 'f', 6 ) << Qt::endl;
		}
	}

	if ( !simulated ) {
		out() << "no jointed collision system to simulate in " << file << Qt::endl;
		return 1;
	}
	return failed ? 1 : 0;
}

/*! Re-encode every capsule in the file and check it against the bytes it came from.
 *
 * Two different claims get checked, because only one of them CAN be exact.
 *
 * Structure is checked byte for byte over the parts an encoder fully determines:
 * the header and flag word, the four hkRelArray descriptors, both end points, the
 * index-tagged w components, the face table, the index table and the sentinels.
 * A geometric comparison cannot see any of that, and it is where a
 * misunderstanding of the layout would show up.
 *
 * Geometry is checked as a distance, because byte-exactness is not achievable
 * from the decoded parameters. The core padding is not a function of the stored
 * radius -- across the corpus padding/radius scatters 1.6e-5 relative around
 * 1/99, hundreds of ULP -- and the roll about the axis is not a function of the
 * axis either. Both are fed back from the decode so the shape is preserved, but
 * they are recovered from the corners and that recovery costs a few ULP.
 *
 * So: structure must be exact, geometry must be tight. Reporting one number for
 * both would let a real layout error hide inside float noise.
 */
int cmdCollisionRoundTrip( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int total = 0, structOk = 0, fullyExact = 0, spheres = 0, spheresExact = 0;
	int massProps = 0, massPropsExact = 0, massPropsInert = 0;
	int polys = 0, polysExact = 0, comps = 0, compsExact = 0;
	int rdc = 0, rdcExact = 0, rdcFresh = 0, rdcFreshInert = 0;
	int lhc = 0, lhcExact = 0, lhcFresh = 0, lhcFreshInert = 0;
	int skel = 0, skelExact = 0, skelInert = 0;
	QSet<int> rdcFreshDiff;
	float worstVert = 0.0f, worstPlane = 0.0f;
	QMap<int, int> byteHist;   // offset -> how often the structural bytes differed

	// the byte ranges an encoder determines outright, so they must match exactly
	const QVector<QPair<int, int>> structural = {
		{ 0x00, 0x50 },    // header, flags, radius, material, the four relArrays
		{ 0x50, 0x20 },    // capA and capB with their w = 1
		{ 0x170, 0x40 }    // face table and index table
	};

	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( !nif.blockInherits( i, "bhkPhysicsSystem" ) && !nif.blockInherits( i, "bhkRagdollSystem" ) )
			continue;
		const QByteArray bytes = nif.get<QByteArray>( i, "Binary Data" );
		if ( bytes.isEmpty() )
			continue;
		const HknpSystem sys = hknpDecode( bytes );
		if ( !sys.valid )
			continue;

		/* hkaSkeleton: rebuilt entirely from the decoded bones, no source bytes fed
		 * back, so this is a real from-scratch test rather than a rewrite. The
		 * hkQsTransform w lanes hold SIMD residue as everywhere else in this format.
		 */
		if ( sys.skeletonRawOffset >= 0 && !sys.bones.isEmpty() ) {
			const QByteArray built = hknpEncodeSkeleton( sys.bones );
			if ( !built.isEmpty() && sys.skeletonRawOffset + built.size() <= bytes.size() ) {
				skel++;
				const QByteArray was = bytes.mid( sys.skeletonRawOffset, built.size() );
				if ( built == was ) {
					skelExact++;
				} else {
					const int n = int( sys.bones.size() );
					const qsizetype poseAt = ( ( 0x90 + 2 * n ) + 15 ) / 16 * 16 + 16 * n;
					bool inert = true;
					for ( int o = 0; o < built.size(); o += 4 ) {
						if ( built.mid( o, 4 ) == was.mid( o, 4 ) )
							continue;
						const qsizetype rel = o - poseAt;
						const bool wLane = rel >= 0 && ( ( rel % 48 ) == 12 || ( rel % 48 ) == 44 );
						if ( !wLane )
							inert = false;
					}
					if ( inert )
						skelInert++;
				}
			}
		}

		// constraint datas: fixed-size atom chains, 416 for a ragdoll and 304 for a hinge
		for ( const HknpConstraint & jc : sys.constraints ) {
			const bool isHinge = ( jc.kind == QLatin1String( "hkpLimitedHingeConstraintData" ) );
			if ( isHinge && jc.rawOffset >= 0 && jc.rawData.size() == 0x130 ) {
				lhc++;
				const QByteArray hWas = bytes.mid( jc.rawOffset, 0x130 );
				if ( hknpEncodeLimitedHingeConstraintData( jc ) == hWas )
					lhcExact++;
				HknpConstraint hFresh = jc;
				hFresh.rawData.clear();
				const QByteArray hBuilt = hknpEncodeLimitedHingeConstraintData( hFresh );
				bool hInert = true;
				for ( int o = 0; o < 0x130; o += 4 ) {
					if ( hBuilt.mid( o, 4 ) == hWas.mid( o, 4 ) )
						continue;
					const bool wLane = ( o == 0x3c || o == 0x4c || o == 0x5c || o == 0x6c
									  || o == 0x7c || o == 0x8c || o == 0x9c || o == 0xac );
					if ( !wLane )
						hInert = false;
				}
				if ( hBuilt == hWas )
					lhcFresh++;
				else if ( hInert )
					lhcFreshInert++;
				continue;
			}
			if ( jc.kind != QLatin1String( "hkpRagdollConstraintData" )
				|| jc.rawOffset < 0 || jc.rawData.size() != 0x1a0 )
				continue;
			rdc++;
			const QByteArray was = bytes.mid( jc.rawOffset, 0x1a0 );
			if ( hknpEncodeRagdollConstraintData( jc ) == was )
				rdcExact++;
			/* Starting from rawData proves the field OFFSETS but not the template:
			 * the constants come along for the ride. Encode again from scratch to
			 * test what a newly authored constraint would actually get.
			 */
			HknpConstraint fresh = jc;
			fresh.rawData.clear();
			const QByteArray built = hknpEncodeRagdollConstraintData( fresh );
			/* The w lanes of the rotation basis vectors are SIMD residue, not
			 * data: they hold values in [-1,1] of the same magnitude as the
			 * rotation itself, the third row is zero almost everywhere, and one
			 * vanilla pivot w holds outright garbage. Havok ignores them. So a
			 * freshly authored constraint cannot be byte-identical to vanilla and
			 * does not need to be -- counted apart rather than called a failure.
			 */
			bool inertOnly = true;
			for ( int o = 0; o < 0x1a0; o += 4 ) {
				if ( built.mid( o, 4 ) == was.mid( o, 4 ) )
					continue;
				const bool wLane = ( o == 0x3c || o == 0x4c || o == 0x5c
								  || o == 0x6c || o == 0x7c || o == 0x8c
								  || o == 0x9c || o == 0xac );
				if ( !wLane ) {
					inertOnly = false;
					rdcFreshDiff.insert( o );
				}
			}
			if ( built == was )
				rdcFresh++;
			else if ( inertOnly )
				rdcFreshInert++;
		}

		/* Compounds: the object, not the flattened children. Its pointer slots are
		 * raw zero in the file, so the bytes compare directly and the fixups the
		 * encoder reports are checked against the ones the packfile actually has.
		 */
		for ( const HknpCompound & comp : sys.compounds ) {
			if ( comp.rawOffset < 0 || comp.instances.isEmpty() )
				continue;
			HknpCompoundFixups fx;
			const QByteArray built = hknpEncodeCompoundShape( comp, &fx );
			if ( built.isEmpty() || comp.rawOffset + built.size() > bytes.size() )
				continue;
			comps++;
			if ( built == bytes.mid( comp.rawOffset, built.size() )
				&& fx.childPointers.size() == comp.instances.size() )
				compsExact++;
		}

		for ( const HknpShape & shp : sys.shapes ) {
			// mass properties: everything is stored, so this must be byte-exact too
			if ( shp.hasMassProps && shp.massPropsOffset >= 0
				&& shp.massPropsOffset + 0x30 <= bytes.size() ) {
				massProps++;
				const QByteArray was = bytes.mid( shp.massPropsOffset, 0x30 );
				const QByteArray now = hknpEncodeShapeMassProperties( shp.massCom,
					shp.massInertiaRaw, shp.massVolume, shp.massMass, shp.massMajorAxis );
				if ( now == was ) {
					massPropsExact++;
				} else {
					/* A packed vector whose three mantissas are all zero keeps
					 * whatever exponent Havok's arithmetic happened to land on --
					 * one vanilla centre of mass carries -45 where this writes -96.
					 * The decoded vector is identical either way, and the original
					 * exponent is genuinely unrecoverable: zero mantissas record no
					 * magnitude. Counted apart rather than called a pass or a fail.
					 */
					bool inert = true;
					for ( int o = 0; o < 0x30; o++ ) {
						if ( now.at( o ) == was.at( o ) )
							continue;
						const int base = ( o >= 0x10 && o < 0x18 ) ? 0x10
									   : ( o >= 0x18 && o < 0x20 ) ? 0x18 : -1;
						if ( base < 0 || o < base + 6 ) {	// not an exponent slot
							inert = false;
							break;
						}
						for ( int k = 0; k < 6; k++ )
							if ( was.at( base + k ) != 0 )
								inert = false;
					}
					if ( inert )
						massPropsInert++;
				}
			}
			/* A polytope: every byte of it survives the decode, so this checks the
			 * measured layout rules end to end -- array starts, both paddings, the
			 * running firstIndex, and the total size.
			 */
			if ( shp.primType == 0 && shp.isConvex && shp.rawOffset >= 0
				&& !shp.faces.isEmpty() && shp.faceAngles.size() == shp.faces.size() ) {
				HknpPolytopeInput pin;
				pin.verts = shp.verts;
				pin.planes = shp.planes;
				pin.faces = shp.faces;
				pin.faceAngles = shp.faceAngles;
				pin.convexRadius = shp.convexRadius;
				pin.materialCRC = shp.shapeMaterialCRC;
				pin.shapeFlags = shp.shapeFlags;
				const QByteArray built = hknpEncodeConvexPolytopeShape( pin );
				if ( !built.isEmpty() && shp.rawOffset + built.size() <= bytes.size() ) {
					polys++;
					if ( built == bytes.mid( shp.rawOffset, built.size() ) )
						polysExact++;
				}
				continue;
			}
			// a sphere derives nothing, so it must come back byte for byte
			if ( shp.primType == 1 && shp.rawOffset >= 0 && shp.rawOffset + 0x80 <= bytes.size() ) {
				spheres++;
				if ( hknpEncodeSphereShape( shp.capA, shp.convexRadius, shp.shapeMaterialCRC )
					== bytes.mid( shp.rawOffset, 0x80 ) )
					spheresExact++;
				continue;
			}
			if ( shp.primType != 2 || shp.rawOffset < 0 || shp.coreVerts.size() != 8 )
				continue;
			const qsizetype at = shp.rawOffset;
			if ( at + 0x1b0 > bytes.size() )
				continue;
			total++;
			const QByteArray original = bytes.mid( at, 0x1b0 );

			HknpCapsuleInput in;
			in.capA = shp.capA;
			in.capB = shp.capB;
			in.radius = shp.convexRadius;
			in.materialCRC = shp.shapeMaterialCRC;
			in.padding = shp.corePadding;
			// bit 1 of the vertex index selects the +u side, so the difference of
			// the two 4-corner centroids recovers u
			Vector3 hi, lo;
			for ( int v = 0; v < 8; v++ )
				( ( v & 2 ) ? hi : lo ) += shp.coreVerts.at( v );
			in.frameU = hi - lo;
			in.hasFrame = in.frameU.length() > 1.0e-12f;

			const QByteArray rebuilt = hknpEncodeCapsuleShape( in );
			if ( rebuilt == original )
				fullyExact++;

			bool ok = true;
			for ( const auto & range : structural ) {
				for ( int o = range.first; o < range.first + range.second; o += 4 ) {
					if ( qFromLittleEndian<quint32>( original.constData() + o )
						!= qFromLittleEndian<quint32>( rebuilt.constData() + o ) ) {
						byteHist[o]++;
						ok = false;
					}
				}
			}
			structOk += int( ok );

			auto f32at = []( const QByteArray & b, int o ) {
				return std::bit_cast<float>( qFromLittleEndian<quint32>( b.constData() + o ) );
			};
			for ( int v = 0; v < 8; v++ ) {
				Vector3 a, c;
				for ( int k = 0; k < 3; k++ ) {
					a[k] = f32at( original, 0x70 + v * 16 + k * 4 );
					c[k] = f32at( rebuilt, 0x70 + v * 16 + k * 4 );
				}
				worstVert = std::max( worstVert, ( a - c ).length() );
			}
			// planes are (n, d) with n.x + d = 0 on the face, so comparing d at a
			// unit normal is already a distance
			for ( int p = 0; p < 6; p++ ) {
				for ( int k = 0; k < 4; k++ ) {
					const float d = std::fabs( f32at( original, 0xf0 + p * 16 + k * 4 )
						- f32at( rebuilt, 0xf0 + p * 16 + k * 4 ) );
					worstPlane = std::max( worstPlane, d );
				}
			}
		}
	}

	out() << "file       " << file << Qt::endl;
	if ( spheres )
		out() << "spheres    " << spheres << "  byte-exact " << spheresExact
			  << " / " << spheres << Qt::endl;
	if ( skel )
		out() << "hkaSkeleton " << skel << "  byte-exact " << skelExact
			  << ", inert w lanes only " << skelInert << Qt::endl;
	if ( lhc )
		out() << "hingecon   " << lhc << "  byte-exact " << lhcExact << " / " << lhc
			  << "  (from template alone: " << lhcFresh << " exact, "
			  << lhcFreshInert << " inert w lanes)" << Qt::endl;
	if ( rdc ) {
		out() << "ragdollcon " << rdc << "  byte-exact " << rdcExact << " / " << rdc
			  << "  (from template alone: " << rdcFresh << " exact, "
			  << rdcFreshInert << " differing only in inert SIMD w lanes)" << Qt::endl;
		if ( !rdcFreshDiff.isEmpty() ) {
			QList<int> offs = rdcFreshDiff.values();
			std::sort( offs.begin(), offs.end() );
			out() << "  template misses (NOT w lanes):";
			for ( int o : offs )
				out() << " +0x" << QString::number( o, 16 );
			out() << Qt::endl;
		}
	}
	if ( comps )
		out() << "compounds  " << comps << "  byte-exact " << compsExact
			  << " / " << comps << Qt::endl;
	if ( polys )
		out() << "polytopes  " << polys << "  byte-exact " << polysExact
			  << " / " << polys << Qt::endl;
	if ( massProps ) {
		out() << "massprops  " << massProps << "  byte-exact " << massPropsExact
			  << " / " << massProps;
		if ( massPropsInert )
			out() << "  (+" << massPropsInert << " differing only in a zero vector's"
				  << " inert exponent)";
		out() << Qt::endl;
	}
	out() << "capsules   " << total << Qt::endl;
	if ( !total ) {
		out() << "  no capsules to check" << Qt::endl;
		return ( spheresExact < spheres || polysExact < polys || compsExact < comps
			|| rdcExact < rdc || lhcExact < lhc || skelExact + skelInert < skel
			|| massPropsExact + massPropsInert < massProps ) ? 1 : 0;
	}
	out() << "  structure byte-exact   " << structOk << " / " << total << Qt::endl;
	out() << "  whole object exact     " << fullyExact << " / " << total << Qt::endl;
	out() << "  worst vertex error     " << worstVert << " m" << Qt::endl;
	out() << "  worst plane error      " << worstPlane << Qt::endl;
	if ( structOk < total ) {
		out() << "  structural offsets that differed:";
		for ( auto it = byteHist.constBegin(); it != byteHist.constEnd(); ++it )
			out() << " +0x" << QString::number( it.key(), 16 ) << "(" << it.value() << ")";
		out() << Qt::endl;
	}
	return ( structOk < total || spheresExact < spheres || polysExact < polys
		|| compsExact < comps || rdcExact < rdc || lhcExact < lhc
		|| skelExact + skelInert < skel
		|| massPropsExact + massPropsInert < massProps ) ? 1 : 0;
}

int cmdCollision( const QString & file, int extractBlock, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	if ( extractBlock >= 0 ) {
		const QModelIndex iSys = nif.getBlockIndex( extractBlock );
		const QByteArray bytes = nif.get<QByteArray>( iSys, "Binary Data" );
		if ( bytes.isEmpty() ) {
			err() << "error: block " << extractBlock << " has no Binary Data" << Qt::endl;
			return 1;
		}
		if ( outFile.isEmpty() ) {
			err() << "error: --extract writes; pass -o <out.bin>" << Qt::endl;
			return 2;
		}
		QFile f( outFile );
		if ( !f.open( QIODevice::WriteOnly ) ) {
			err() << "error: cannot write " << outFile << Qt::endl;
			return 1;
		}
		f.write( bytes );
		out() << "wrote " << bytes.size() << " bytes to " << outFile << Qt::endl;
		return 0;
	}

	// node bindings, gathered per system so the report groups by packfile
	QMap<int, QList<QPair<quint32, int>>> refs;   // system -> [(body id, node)]
	int editable = 0;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( nif.blockInherits( i, "bhkNPCollisionObject" ) ) {
			const int target = nif.getLink( i, "Target" );
			refs[nif.getLink( i, "Data" )].append(
				{ nif.get<quint32>( i, "Body ID" ),
				  nif.isValidBlockNumber( target ) ? target : nif.getParent( b ) } );
		} else if ( nif.blockInherits( i, "bhkCollisionObject" ) ) {
			editable++;
		}
	}

	out() << "file      " << file << Qt::endl;
	out() << "collision " << refs.size() << " compiled system(s), "
		  << editable << " editable object(s)" << Qt::endl;

	for ( auto it = refs.constBegin(); it != refs.constEnd(); ++it ) {
		const QModelIndex iSys = nif.getBlockIndex( it.key() );
		const QByteArray bytes = nif.get<QByteArray>( iSys, "Binary Data" );
		const HknpSystem sys = hknpDecode( bytes );
		out() << Qt::endl
			  << "system    " << blockLabel( &nif, it.key() ) << "  "
			  << bytes.size() << " bytes" << Qt::endl;
		out() << "  objects referencing it   " << it.value().size() << Qt::endl;
		out() << "  decoded                  " << ( sys.valid ? "ok" : qPrintable( sys.error ) )
			  << ", " << sys.shapes.size() << " shape(s), "
			  << sys.bodyPhys.size() << " body/bodies, "
			  << ( sys.dynamic ? "dynamic" : "static" ) << Qt::endl;
		if ( !sys.unknownShapes.isEmpty() )
			out() << "  not decoded              " << sys.unknownShapes.join( QStringLiteral( ", " ) )
				  << Qt::endl;

		// how many DISTINCT body ids the shapes carry: that is exactly how many
		// rigid bodies a decompile would produce
		QSet<int> shapeBodies;
		int unattributed = 0;
		for ( const HknpShape & shp : sys.shapes ) {
			if ( shp.bodyId < 0 )
				unattributed++;
			else
				shapeBodies.insert( shp.bodyId );
		}
		out() << "  attribution              "
			  << ( shapeBodies.isEmpty() ? "none"
				 : sys.positionalBodies ? "positional (shape index = body id, inferred)"
				 : "from the packfile body array" ) << Qt::endl;
		out() << "  shapes attributed to     " << shapeBodies.size() << " distinct body/bodies"
			  << ( unattributed ? QString( ", %1 unattributed" ).arg( unattributed ) : QString() )
			  << Qt::endl;

		out() << QString( "  %1 %2 %3 %4" ).arg( "body", -6 ).arg( "node", -34 )
					.arg( "layer", -6 ).arg( "shapes" ) << Qt::endl;
		for ( const auto & ref : it.value() ) {
			int mine = 0;
			for ( const HknpShape & shp : sys.shapes ) {
				if ( shp.bodyId >= 0 ? quint32( shp.bodyId ) == ref.first : ref.first == 0 )
					mine++;
			}
			const HknpBodyPhys phys = int( ref.first ) < sys.bodyPhys.size()
									? sys.bodyPhys.at( int( ref.first ) ) : HknpBodyPhys();
			out() << QString( "  %1 %2 %3 %4" )
						.arg( ref.first, -6 )
						.arg( nif.get<QString>( nif.getBlockIndex( ref.second ), "Name" ), -34 )
						.arg( phys.layer, -6 ).arg( mine )
				  << Qt::endl;
		}
		if ( !sys.bones.isEmpty() ) {
			int locked = 0;
			for ( const HknpBone & b : sys.bones )
				locked += b.lockTranslation ? 1 : 0;
			out() << "  hkaSkeleton bones        " << sys.bones.size()
				  << " (" << locked << " translation-locked)" << Qt::endl;
			out() << QString( "  %1 %2 %3 %4" ).arg( "bone", -5 ).arg( "parent", -7 )
						.arg( "rest translation", -26 ).arg( "rest rotation (wxyz)" ) << Qt::endl;
			for ( int i = 0; i < sys.bones.size(); i++ ) {
				const HknpBone & b = sys.bones.at( i );
				out() << QString( "  %1 %2 %3 %4 %5   %6 %7 %8 %9%10" )
							.arg( i, -5 ).arg( b.parent, -7 )
							.arg( b.translation[0], 8, 'f', 3 ).arg( b.translation[1], 8, 'f', 3 )
							.arg( b.translation[2], 8, 'f', 3 )
							.arg( b.rotation[0], 8, 'f', 4 ).arg( b.rotation[1], 8, 'f', 4 )
							.arg( b.rotation[2], 8, 'f', 4 ).arg( b.rotation[3], 8, 'f', 4 )
							.arg( b.lockTranslation ? "  locked" : "" )
					  << Qt::endl;
			}
		}
		if ( !sys.constraints.isEmpty() ) {
			out() << "  joints                   " << sys.constraints.size() << Qt::endl;
			out() << QString( "  %1 %2 %3" ).arg( "child", -34 ).arg( "parent", -34 )
						.arg( "constraint" ) << Qt::endl;
			auto nodeFor = [&]( int body ) {
				for ( const auto & ref : it.value() ) {
					if ( int( ref.first ) == body )
						return nif.get<QString>( nif.getBlockIndex( ref.second ), "Name" );
				}
				return QString( "body %1" ).arg( body );
			};
			// hkpRagdollConstraintData -> Ragdoll; the wrappers are hknp*, not hkp*
			auto shortKind = []( const QString & k ) {
				QString s = k;
				for ( auto p : { "hknp", "hkp" } )
					if ( s.startsWith( QLatin1String( p ) ) ) { s = s.mid( int( strlen( p ) ) ); break; }
				return s.remove( QLatin1String( "ConstraintData" ) );
			};
			// limits print in degrees: nobody authors a ragdoll in radians
			auto limit = []( const HknpAngLimit & l, const char * name ) {
				if ( !l.present )
					return QString();
				const float lo = rad2deg( l.min ), hi = rad2deg( l.max );
				if ( l.min < -99.0f )   // Havok's "no lower bound" sentinel
					return QString( "  %1 <%2" ).arg( name ).arg( hi, 0, 'f', 1 );
				// two vanilla hinges store min/max the wrong way round; show that
				// rather than folding it into a nonsense "+--0.1"
				if ( qAbs( lo + hi ) < 0.05f && hi >= 0.0f )
					return QString( "  %1 +-%2" ).arg( name ).arg( hi, 0, 'f', 1 );
				return QString( "  %1 %2..%3" ).arg( name )
					.arg( lo, 0, 'f', 1 ).arg( hi, 0, 'f', 1 );
			};
			for ( const HknpConstraint & jc : sys.constraints ) {
				QString detail;
				if ( jc.hasFrames ) {
					detail = QString( "  pivot %1 %2 %3" )
						.arg( jc.pivotB[0], 0, 'f', 3 ).arg( jc.pivotB[1], 0, 'f', 3 )
						.arg( jc.pivotB[2], 0, 'f', 3 );
				}
				detail += limit( jc.hinge, "hinge" ) + limit( jc.twist, "twist" )
					+ limit( jc.cone, "cone" ) + limit( jc.plane, "plane" );
				if ( jc.motorEnabled )
					detail += "  motor";
				if ( jc.breakable )
					detail += "  breakable";
				out() << QString( "  %1 %2 %3" )
							.arg( nodeFor( jc.childBody ), -34 )
							.arg( nodeFor( jc.parentBody ), -34 )
							.arg( shortKind( jc.kind ), -14 )
					  << detail << Qt::endl;
			}
		}
		out() << QString( "  %1 %2 %3 %4" ).arg( "shape", -6 ).arg( "class", -34 )
					.arg( "body", -6 ).arg( "geometry" ) << Qt::endl;
		for ( int i = 0; i < sys.shapes.size(); i++ ) {
			const HknpShape & shp = sys.shapes.at( i );
			QString geom = QString( "%1 v / %2 t" ).arg( shp.verts.size() ).arg( shp.tris.size() );
			if ( shp.primType == 1 )
				geom = QString( "sphere r %1" ).arg( shp.primRadius );
			else if ( shp.primType == 2 )
				geom = QString( "capsule r %1  len %2" ).arg( shp.primRadius )
						.arg( ( shp.capB - shp.capA ).length() );
			if ( shp.hasMassProps ) {
				// the physical inertia, with Havok's 1.5 scale undone
				const Vector3 mi = shp.massInertia();
				geom += QString( "  vol %1 mass %2 com %3,%4,%5 I %6,%7,%8" )
					.arg( shp.massVolume, 0, 'f', 6 ).arg( shp.massMass, 0, 'f', 6 )
					.arg( shp.massCom[0], 0, 'f', 4 ).arg( shp.massCom[1], 0, 'f', 4 )
					.arg( shp.massCom[2], 0, 'f', 4 )
					.arg( mi[0], 0, 'g', 4 ).arg( mi[1], 0, 'g', 4 ).arg( mi[2], 0, 'g', 4 );
			}
			out() << QString( "  %1 %2 %3 %4" )
						.arg( i, -6 ).arg( shp.className, -34 ).arg( shp.bodyId, -6 ).arg( geom )
				  << Qt::endl;
		}
	}
	return 0;
}

/*! Write the document NifSkope opens with when no file was given.
 *
 * Same builder as the GUI startup path, so this is how that document gets
 * checked without a window.
 */
int cmdNew( const QString & outFile, float size )
{
	NifModel nif;
	QString error;
	if ( !nifCreateStarterScene( &nif, size > 0.0f ? size : STARTER_CUBE_SIZE, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	out() << "version  " << nif.getVersion()
		  << "  user " << nif.getUserVersion()
		  << "  bs " << nif.getBSVersion() << Qt::endl;
	for ( int b = 0; b < nif.getBlockCount(); b++ )
		out() << "  " << blockLabel( &nif, b ) << Qt::endl;
	return saveNif( nif, outFile ) ? 0 : 1;
}

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
		  << "  new -o OUT [--size N]                   write the starter document: a\n"
		  << "                                          Fallout 4 scene with one cube\n"
		  << "                                          (N defaults to 2 m in FO4 units)\n"
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
		  << "  simulate <file> [--steps N] [-v]        run the ragdoll solver headless and\n"
		  << "                                          report energy, joint drift, speed\n"
		  << "  collision <file>                        collision inventory: node -> body ->\n"
		  << "                                          system bindings, and what the hknp\n"
		  << "                                          decode found in each packfile\n"
		  << "  collision <file> --extract -b N -o F.bin\n"
		  << "                                          write a system's Binary Data verbatim\n"
		  << "  collision <file> --roundtrip            re-encode every capsule from its decoded\n"
		  << "                                          parameters and diff against the original\n"
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
	int steps = 0;
	int substeps = 0;
	int iterations = 0;
	QString onlyLimit;
	bool useGround = false, noSelf = false, drop = false, jointedOnly = false;
	int dragBody = -1;
	bool noLimits = false;
	bool verboseSim = false;
	float cubeSize = STARTER_CUBE_SIZE;
	bool noDedupe = false;
	int block = -1, depth = 2, maxRows = 40;
	int effectVar = -1, intVar = -1;
	bool showAll = false, newSequence = false, standalone = false, listOnly = false;
	bool validateOnly = false;
	bool selfTest = false;
	bool extract = false;
	bool roundTrip = false;
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
		else if ( t == QLatin1String( "--extract" ) ) extract = true;
		else if ( t == QLatin1String( "--roundtrip" ) ) roundTrip = true;
		else if ( t == QLatin1String( "--add" ) ) adds << QDir::current().filePath( next() );
		else if ( t == QLatin1String( "--no-dedupe" ) ) noDedupe = true;
		else if ( t == QLatin1String( "--save" ) ) saveName = next();
		else if ( t == QLatin1String( "--apply" ) ) applyName = next();
		else if ( t == QLatin1String( "--blend" ) ) blend = next().toFloat();
		else if ( t == QLatin1String( "--steps" ) ) steps = next().toInt();
		else if ( t == QLatin1String( "--substeps" ) ) substeps = next().toInt();
		else if ( t == QLatin1String( "--iterations" ) ) iterations = next().toInt();
		else if ( t == QLatin1String( "--only-limit" ) ) onlyLimit = next();
		else if ( t == QLatin1String( "--ground" ) ) useGround = true;
		else if ( t == QLatin1String( "--no-self" ) ) noSelf = true;
		else if ( t == QLatin1String( "--drop" ) ) drop = true;
		else if ( t == QLatin1String( "--jointed-only" ) ) jointedOnly = true;
		else if ( t == QLatin1String( "--drag" ) ) dragBody = next().toInt();
		else if ( t == QLatin1String( "--no-limits" ) ) noLimits = true;
		else if ( t == QLatin1String( "--trace" ) ) verboseSim = true;
		else if ( t == QLatin1String( "--size" ) ) cubeSize = next().toFloat();
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

	if ( cmd == QLatin1String( "new" ) ) {
		if ( !initModelLayer() ) { err().flush(); return 1; }
		const int rc = cmdNew( outFile, cubeSize );
		out().flush();
		err().flush();
		return rc;
	}

	if ( cmd == QLatin1String( "spells" ) ) {
		// spells needs no file, but does need the format descriptions loaded
		if ( !initModelLayer() ) { err().flush(); return 1; }
		const int rc = cmdSpells( pattern );
		out().flush();
		return rc;
	}

	// the solver self-test builds its own bodies, so it needs no file
	if ( file.isEmpty() && !( cmd == QLatin1String( "simulate" ) && selfTest ) ) {
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
	else if ( cmd == QLatin1String( "simulate" ) )
		rc = cmdSimulate( file, steps > 0 ? steps : 120, substeps > 0 ? substeps : 8,
			iterations, noLimits, onlyLimit, useGround, noSelf, drop, jointedOnly,
			dragBody, selfTest, verboseSim );
	else if ( cmd == QLatin1String( "collision" ) )
		rc = roundTrip ? cmdCollisionRoundTrip( file )
					   : cmdCollision( file, extract ? block : -1, outFile );
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
