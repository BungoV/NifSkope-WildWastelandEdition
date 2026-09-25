/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

See the LICENSE.md file for the full license text.

***** END LICENCE BLOCK *****/

#include "nifcli.h"

#include "freezeanim.h"
#include "gamemanager.h"
#include "loadingscreen.h"
#include "nifmerge.h"
#include "spellbook.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "skeletontools.h"
#include "starterscene.h"
#include "btdterrain.h"
#include "esmdata.h"
#include "esmweather.h"
#include "lodgen.h"
#include "lodgenchunkpass.h"
#include "lodgenloadorder.h"
#include "lodgenlayout.h"
#include "lodgenparallel.h"
#include "nifparsestress.h"
#include "nativeemit.h"
#include "lodifile.h"
#include "lodofile.h"
#include "lodbfile.h"
#include <QDateTime>
#include <QDirIterator>
#include "lodtfile.h"
#include "watermark.h"
#include "io/lodmfile.h"
#include "io/lodvfile.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "btdfile.hpp"
#include <QSet>
#include <cstring>
#include <memory>
#include <QDataStream>
#include "gl/hknpdecode.h"
#include "gltfexportnif.h"			// lane HKX4
#include "gltfexportchar.h"			// lane GLTFEXPORT1
#include "gltfexportopts.h"			// lane GLTFEXPORT1
#include "gltfimport.h"				// lane BUILD8
#include "hkxwrite.h"				// lane BUILD8
#include "gl/hknpencode.h"
#include "physics/ragdollsim.h"

#include <bit>
#include <cmath>
#include <QColor>
#include <QtEndian>
#include "spells/animationsetup.h"
#include "spells/normaltransfer.h"
#include "io/pbrmfile.h"
#include "io/pbrmresolve.h"
#include "io/nifxfile.h"

#include <QCoreApplication>
#include <QMutex>
#include <cstdio>
#include <QCryptographicHash>
#include <QDir>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
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

//! collisiontools.cpp owns the joint mapping; see "A joint's editable form" there.
extern int tlCollWriteConstraint( NifModel * nif, const HknpConstraint & c,
								int childBody, int parentBody );
extern bool tlCollReadConstraint( const NifModel * nif, const QModelIndex & index,
								HknpConstraint & c, int * childBody, int * parentBody );
extern QModelIndex tlCollDescriptor( const NifModel * nif, const QModelIndex & iCon, bool ragdoll );
extern QString tlRiggingSegmentReport( const NifModel * nif, const QModelIndex & shape );

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

/* Silence Qt's chatter; the CLI's own output is the product.
 *
 * THIS HANDLER IS CALLED FROM WORKER THREADS, AND IT USED TO WRITE THROUGH
 * err() -- a function-local static QTextStream with no lock (lane RESUME3,
 * 2026-09-11). QTextStream is not reentrant: it grows one QString write
 * buffer in place. A `-no-gui lodgen --chunk-threads 16` bake reaches here
 * from every worker at once through qWarning() in
 * GameResources::get_file -- one warning per missing .bgsm, and the road
 * pass misses the same material on every placement -- and the heap went:
 * 0xC0000374 on 3 of 5 bare Sanctuary runs, with two of four symbolised
 * faults taken INSIDE this function and the other two in an innocent
 * QList reallocation that reached the corrupted heap first.
 *
 * The CRT locks the FILE *, so fputs from many threads is safe; the mutex is
 * only so a line and its newline cannot be split. On one thread the bytes
 * and their order are exactly what err() produced -- that is the way back.
 */
void cliMessageHandler( QtMsgType type, const QMessageLogContext &, const QString & msg )
{
	if ( type != QtWarningMsg && type != QtCriticalMsg && type != QtFatalMsg )
		return;
	static QMutex cliLogMutex;
	const QByteArray line = msg.toLocal8Bit();
	QMutexLocker lock( &cliLogMutex );
	std::fputs( line.constData(), stderr );
	std::fputc( '\n', stderr );
}

//! Shared init the GUI path does in main.cpp: settings identity, working
//! directory (nif.xml is resolved relative to it) and the format descriptions.
bool initModelLayer()
{
#ifdef Q_OS_WIN32
	/* NO CRASH DIALOG FROM A HEADLESS RUN (2026-09-11, lane BAKEPERF1).
	 *
	 * Six Windows "Application Error" boxes reached bungo's desktop while this
	 * lane was bisecting a fault in a `-no-gui lodgen` bake. A headless run has
	 * no business showing a window at all, and a modal error box obeys neither
	 * the second-monitor rule nor the never-foreground one. The mode is
	 * inherited by anything this process starts, so a driver script gets it
	 * too. A crash still fails the run and still sets the exit code -- what
	 * goes away is the dialog. */
	SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
		| SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX );
#endif
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
	if ( m.specularV6 ) {
		out() << "  specWeight " << m.specularWeight << " (override " << m.overrideSpecularWeight << ")" << Qt::endl;
		out() << "  specIor    " << m.specularIor << " max " << m.specularIorMax
		      << " (override " << m.overrideSpecularIor << ")" << Qt::endl;
		out() << "  specTint   " << m.specularTint[0] << " " << m.specularTint[1] << " " << m.specularTint[2]
		      << " (override " << m.overrideSpecularColor << ")" << Qt::endl;
		slotLine( "specColor", m.specularColor );
	}
	// The dielectric F0 LEVEL the upload carries (FO4CS wave 88 law), and the
	// same file read through the v4/v5 law -- the gate's red control.
	out() << "  F0 level   " << QString::number( double( pbrmDielectricF0( m ) ), 'f', 3 )
	      << ( m.specularV6 ? " (v6: weight x ((ior-1)/(ior+1))^2, cap 1)" : " (v4/v5: f0, cap 0.16)" )
	      << "  v5-law read " << QString::number( double( pbrmDielectricF0( m, PbrmF0Law::V5 ) ), 'f', 3 ) << Qt::endl;
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
 * `.pbrm` (if any) would be adopted, by which route, and why every other
 * candidate was declined.
 *
 * Calls THE ONE candidate function, pbrmResolve() in src/io/pbrmresolve.h, the
 * same one the viewport's BSShaderLightingProperty::resolvePbrm and lodgen's
 * material mask call (lane PBRR1). What this command cannot see is the
 * WW_PBRM_SWAP pin's swap (a viewport pin); the .nifx beside the NIF, the
 * Auto-replace setting (QSettings, or WW_PBRM_AUTOREPLACE=0/1), WW_PBRM_ORDER
 * and the FO76 step (FO4 NIFs, bsver 130-139) are honoured.
 */
int cmdPbrmResolve( const QString & file )
{
	NifModel nif;
	if ( !nif.loadFromFile( file ) ) {
		err() << "error: cannot load " << file << Qt::endl;
		return 1;
	}

	QSettings settings;
	bool autoReplace = settings.value( QStringLiteral( "Settings/Render/PBRM Auto Replace" ), true ).toBool();
	const QString envAuto = qEnvironmentVariable( "WW_PBRM_AUTOREPLACE" );
	if ( envAuto == QLatin1String( "0" ) )
		autoReplace = false;
	else if ( envAuto == QLatin1String( "1" ) )
		autoReplace = true;
	QList<PbrmRoute> order = pbrmDefaultOrder();
	const QString envOrder = qEnvironmentVariable( "WW_PBRM_ORDER" );
	if ( !envOrder.isEmpty() ) {
		QString why;
		if ( !pbrmParseOrder( envOrder, order, why ) ) {
			err() << "error: WW_PBRM_ORDER refused: " << why << Qt::endl;
			return 2;
		}
	}
	const PbrmF0Law law = qEnvironmentVariable( "WW_PBRM_F0_LAW" ) == QLatin1String( "v5" )
		? PbrmF0Law::V5 : PbrmF0Law::Auto;
	QStringList orderNames;
	for ( PbrmRoute r : order )
		orderNames << QLatin1String( pbrmRouteName( r ) );
	out() << "auto-replace: " << ( autoReplace ? "on" : "off" )
	      << "  order: " << orderNames.join( QLatin1Char( ',' ) )
	      << "  f0law: " << ( law == PbrmF0Law::V5 ? "v5" : "auto" ) << Qt::endl;

	auto reader = [&nif]( const QString & path, QByteArray & bytes ) -> bool {
		bytes.clear();
		if ( nif.findResourceFile( path, "materials", "" ).isEmpty() )
			return false;
		nif.getResourceFile( bytes, path, "materials", "" );
		return !bytes.isEmpty();
	};
	const QString nifAbs = QFileInfo( file ).absoluteFilePath();
	const quint32 bsver = nif.getBSVersion();

	int shaders = 0, adopted = 0;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex iBlock = nif.getBlockIndex( b );
		if ( !nif.blockInherits( iBlock, "BSShaderProperty" ) )
			continue;
		shaders++;

		PbrmResolveInput in;
		in.material = nif.get<QString>( iBlock, "Name" );
		const int parent = nif.getParent( b );
		if ( parent >= 0 )
			in.shapeName = nif.get<QString>( nif.getBlockIndex( parent ), "Name" );
		if ( !in.shapeName.isEmpty() )
			in.nifxPbrm = nifxMaterialFor( nifAbs, in.shapeName, &in.nifxNote );
		in.sibling = autoReplace;
		in.fo76 = ( bsver >= 130 && bsver < 140 );
		in.order = order;

		const PbrmResolveResult r = pbrmResolve( in, reader );
		QString f0 = QStringLiteral( "none" );
		if ( r.route != PbrmRoute::Legacy ) {
			f0 = QString::number( double( pbrmDielectricF0( r.material, law ) ), 'f', 3 );
			adopted++;
		}
		out() << QStringLiteral( "[%1] shape=\"%2\" material=\"%3\"" ).arg( b ).arg( in.shapeName, in.material ) << Qt::endl;
		out() << QStringLiteral( "      route=%1 path=\"%2\" envelope=%3 f0=%4" )
			.arg( QLatin1String( pbrmRouteName( r.route ) ), r.path, r.envelope, f0 ) << Qt::endl;
		if ( r.route == PbrmRoute::Legacy )
			out() << "      refusal=\"" << r.refusal << "\"" << Qt::endl;
		for ( const QString & t : r.tried )
			out() << "      tried " << t << Qt::endl;
	}

	out() << shaders << " shader properties, " << adopted << " would adopt a pbrm" << Qt::endl;
	return 0;
}

/*! `nifx <in.nifx> [--set <node>=<pbrm>]... [--remove <node>]... [--canonical] [--out <file>]`
 *
 * Reads a .nifx (docs/NIFSKOPE_PBR_RENDERER.md s2.4) with the span-preserving
 * reader, applies the edits in order and writes the bytes. With no edit the
 * output is the input, byte for byte: the round-trip gate (d). `--canonical`
 * re-serialises through QJsonDocument instead -- the RED control, since that
 * reorders keys and reformats unknown sections. Prints the parse summary.
 * Exit 0 ok, 1 read/parse/edit failure, 2 usage.
 */
int cmdNifx( const QStringList & args )
{
	QString in, outPath;
	bool canonical = false;
	QList<QPair<QString, QString>> edits;	// (node, pbrm); pbrm null = remove
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & t = args.at( i );
		if ( t == QLatin1String( "--set" ) && i + 1 < args.size() ) {
			const QString v = args.at( ++i );
			const int eq = v.indexOf( QLatin1Char( '=' ) );
			if ( eq <= 0 ) {
				err() << "error: --set wants <node>=<pbrm>" << Qt::endl;
				return 2;
			}
			edits.append( { v.left( eq ), v.mid( eq + 1 ) } );
		} else if ( t == QLatin1String( "--remove" ) && i + 1 < args.size() ) {
			edits.append( { args.at( ++i ), QString() } );
		} else if ( t == QLatin1String( "--canonical" ) ) {
			canonical = true;
		} else if ( t == QLatin1String( "--out" ) && i + 1 < args.size() ) {
			outPath = args.at( ++i );
		} else if ( !t.startsWith( QLatin1String( "--" ) ) && in.isEmpty() ) {
			in = t;
		} else {
			err() << "error: unknown nifx argument " << t << Qt::endl;
			return 2;
		}
	}
	if ( in.isEmpty() ) {
		err() << "error: 'nifx' needs a <file.nifx>" << Qt::endl;
		return 2;
	}

	NifxDocument d = nifxParseFile( in );
	if ( !d.ok ) {
		err() << "error: " << d.error << Qt::endl;
		return 1;
	}
	for ( const auto & e : edits ) {
		QString why;
		const bool ok = e.second.isNull() ? nifxRemoveMaterial( d, e.first, &why )
			: nifxSetMaterial( d, e.first, e.second, &why );
		if ( !ok ) {
			err() << "error: edit of \"" << e.first << "\" refused: " << why << Qt::endl;
			return 1;
		}
	}

	out() << "nifx version=" << d.version << " members=" << d.members.size()
	      << " unknown=" << d.unknownSections.join( QLatin1Char( ',' ) )
	      << " material=" << d.material.size() << Qt::endl;
	for ( const auto & m : d.material )
		out() << "  node=\"" << m.node << "\" pbrm=\"" << m.pbrm << "\" valid=" << ( m.valid ? 1 : 0 )
		      << ( m.problem.isEmpty() ? QString() : QStringLiteral( " problem=\"%1\"" ).arg( m.problem ) ) << Qt::endl;
	for ( const QString & w : d.warnings )
		out() << "  warning: " << w << Qt::endl;

	if ( !outPath.isEmpty() ) {
		const QByteArray bytes = canonical
			? QJsonDocument::fromJson( d.bytes ).toJson( QJsonDocument::Indented )
			: nifxSerialize( d );
		QFile f( outPath );
		if ( !f.open( QIODevice::WriteOnly ) || f.write( bytes ) != bytes.size() ) {
			err() << "error: cannot write " << outPath << Qt::endl;
			return 1;
		}
		out() << "wrote " << bytes.size() << " bytes to " << outPath
		      << ( canonical ? " (canonical: QJsonDocument re-serialised)" : "" ) << Qt::endl;
	}
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
	int dragBody, bool dragSpring, float dragFirmness, bool selfTest, bool verbose )
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
		/* Picking, checked before anything moves.
		 *
		 * For each body, fire a ray at one of its own shape points from just
		 * outside that point's radius, along a direction that cannot be blocked by
		 * the body itself. The pick must come back with that body. It is a property
		 * rather than a fixture -- no hand-written coordinates to go stale, and it
		 * holds for every rig in the corpus at once.
		 *
		 * A ray fired from far away would legitimately hit whatever is in front, so
		 * this starts close: what is being tested is that the transform chain and
		 * the sphere intersection agree, not occlusion order.
		 */
		{
			int tried = 0, wrong = 0;
			// worst error in toWorld(pick.body, pick.localPoint) == pick.worldPoint,
			// which is the identity the viewport leans on: the point picking returns
			// is handed straight to setDrag, and a transform error there would grab
			// the right body in the wrong place
			float worstRT = 0.0f;
			for ( int i = 0; i < sim.bodies().size(); i++ ) {
				const SimBody & sb = sim.bodies().at( i );
				if ( sb.points.isEmpty() )
					continue;
				const Vector3 target = sim.toWorld( i, sb.points.first().p - sb.com );
				const float r = std::max( sb.points.first().r, 0.02f );
				for ( int axis = 0; axis < 3; axis++ ) {
					Vector3 dir;
					dir[axis] = 1.0f;
					const SimPick p = sim.pick( target - dir * ( r * 1.5f ), dir );
					tried++;
					// another body's geometry may genuinely sit closer along that ray,
					// so a different answer is only wrong if it is no answer
					if ( !p.hit() )
						wrong++;
					else
						worstRT = std::max( worstRT,
							( sim.toWorld( p.body, p.localPoint ) - p.worldPoint ).length() );
				}
			}
			if ( wrong )
				out() << "  pick self-test: " << wrong << " of " << tried
					  << " rays hit NOTHING" << Qt::endl;
			else if ( tried )
				out() << "  pick: " << tried << " rays, every one hit a body; worst"
					  << " local->world round trip " << QString::number( worstRT, 'e', 2 )
					  << " m" << Qt::endl;
			failed += ( wrong > 0 ) ? 1 : 0;
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
		float dragR = 0.0f, worstDragLag = 0.0f;
		if ( dragBody >= 0 && dragBody < sim.bodies().size() ) {
			if ( !dragSpring )
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
			/* The spring is grabbed OFF the body's origin. At the origin the lever
			 * arm is zero, so the correction carries no torque and the one thing a
			 * mouse drag has to do -- swing the limb it grabbed rather than sliding
			 * it -- never gets exercised.
			 */
			if ( dragSpring )
				sim.setDrag( dragBody, Vector3( dragR * 0.25f, 0.0f, 0.0f ), dragFrom, dragFirmness );
			out() << QString( "  dragging body %1 in a %2 m circle by a %3" ).arg( dragBody )
						.arg( dragR, 0, 'f', 3 )
						.arg( dragSpring ? QStringLiteral( "spring" ) : QStringLiteral( "hard pin" ) )
				  << Qt::endl;
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
				const Vector3 target = dragFrom
					+ Vector3( std::cos( ang ) - 1.0f, std::sin( ang ), 0.0f ) * dragR;
				if ( dragSpring ) {
					sim.moveDrag( target );
				} else {
					sim.setPosition( dragBody, target );
				}
			}
			sim.step( 1.0f / 60.0f, substeps );
			// AFTER the step: measured before it, this reads how far the target moved
			// this frame rather than how well the grab tracked it, and comes out the
			// same for a rigid grab and a loose one
			if ( dragSpring && dragR > 0.0f )
				worstDragLag = std::max( worstDragLag, sim.dragError() );
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
			if ( dragSpring && dragR > 0.0f ) {
				// how far the grabbed point ever lagged the hand. A spring is SUPPOSED
				// to lag -- that is the difference between it and a pin -- so this is a
				// characterisation, not a pass mark. It only fails if the grab lets go
				// entirely, which shows up as a lag comparable to the drag radius.
				out() << "  drag: worst lag " << QString::number( worstDragLag, 'f', 4 )
					  << " m over a " << QString::number( dragR, 'f', 3 )
					  << " m pull (" << QString::number( 100.0f * worstDragLag / dragR, 'f', 1 )
					  << "% of the radius), final "
					  << QString::number( sim.dragError(), 'f', 4 ) << " m" << Qt::endl;
			}
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
int cmdCollisionRoundTrip( const QString & file, const QString & rebuildTo )
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
	int packs = 0, packsExact = 0, packsSkipped = 0, packsLeafOnly = 0, packsDerived = 0, packsDerivedExact = 0;
	QMap<QString, int> packDiffs;   // object kind -> how many bytes differed in it
	qsizetype packFirstDiff = -1, packSizeWas = 0, packSizeNow = 0;
	QString packError;
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

		/* The whole packfile, reassembled. Every object encoder above proves its own
		 * bytes; this is the only check that covers what holds them together -- the
		 * class-name table and its order, where each object lands, all three fixup
		 * tables and their orderings, and the section headers. Nothing short of a
		 * byte comparison against the original file tests those.
		 */
		if ( !sys.bodyPhys.isEmpty() ) {
			QString err;
			const QByteArray built = hknpEncodeSystem( sys, &err );
			if ( !rebuildTo.isEmpty() && !built.isEmpty() ) {
				// the reassembled bytes, so a mismatch can be diffed and not guessed at.
				// One file per system: a NIF can hold several, and writing them all to
				// one name leaves only the last, which is how the Gorilla skeleton's
				// failing static system got diffed against its healthy ragdoll.
				QFile f( rebuildTo + QStringLiteral( ".%1" ).arg( b ) );
				if ( f.open( QIODevice::WriteOnly ) )
					f.write( built );
			}
			/* Assembled twice, because the two runs answer different questions.
			 * With the stored shape bytes in hand this is what NifSkope writes for
			 * a file whose collision nobody touched, and it has to come back
			 * identical. With them cleared every shape is re-derived, which is
			 * what an EDITED shape gets, and whatever survives then says how much
			 * of the format is genuinely reconstructed rather than copied.
			 */
			HknpSystem fresh = sys;
			for ( HknpShape & s : fresh.shapes ) {
				s.rawData.clear();
				s.massRawData.clear();
			}
			for ( HknpBodyPhys & p : fresh.bodyPhys )
				p.propsRawData.clear();
			const QByteArray derived = hknpEncodeSystem( fresh, nullptr );
			if ( !derived.isEmpty() ) {
				packsDerived++;
				if ( derived == bytes )
					packsDerivedExact++;
			}
			if ( built.isEmpty() ) {
				// a clean refusal is not a wrong answer: report it, do not fail on it
				packsSkipped++;
				if ( packError.isEmpty() )
					packError = err;
			} else if ( packs++, built == bytes ) {
				packsExact++;
			} else {
				/* WHERE the differences fall matters more than that there are any.
				 * A capsule's core box is DERIVED from (capA, capB, radius, roll),
				 * so it comes back a few ULP off and cannot be bit-exact from the
				 * model alone -- which is what the per-shape checks below measure.
				 * What this has to establish is the assembly: object order, where
				 * each one lands, the class table, the three fixup tables and the
				 * section headers. So classify every differing byte by the object
				 * it lands in, and hold it against the assembly only when it lands
				 * outside a leaf whose own encoder already reports the same.
				 */
				QMap<qsizetype, QString> objAt;   // offset -> what starts there
				if ( sys.rootRawOffset >= 0 )
					objAt.insert( sys.rootRawOffset, QStringLiteral( "root" ) );
				if ( sys.skeletonRawOffset >= 0 )
					objAt.insert( sys.skeletonRawOffset, QStringLiteral( "skeleton" ) );
				for ( const HknpShape & s : sys.shapes ) {
					if ( s.rawOffset >= 0 )
						objAt.insert( s.rawOffset, QStringLiteral( "shape" ) );
					if ( s.massPropsOffset >= 0 )
						objAt.insert( s.massPropsOffset, QStringLiteral( "massprops" ) );
				}
				for ( const HknpConstraint & c : sys.constraints ) {
					if ( c.rawOffset >= 0 )
						objAt.insert( c.rawOffset, QStringLiteral( "constraint" ) );
				}
				bool structural = ( built.size() != bytes.size() );
				const qsizetype n = std::min( built.size(), bytes.size() );
				for ( qsizetype o = 0; o < n; o++ ) {
					if ( built.at( o ) == bytes.at( o ) )
						continue;
					auto it = objAt.upperBound( o );
					const QString where = ( it == objAt.constBegin() )
						? QStringLiteral( "before any object" ) : ( --it ).value();
					packDiffs[where]++;
					if ( where == QLatin1String( "shape" ) || where == QLatin1String( "massprops" ) )
						continue;
					structural = true;
					if ( packFirstDiff < 0 ) {
						packFirstDiff = o;
						packSizeWas = bytes.size();
						packSizeNow = built.size();
					}
				}
				if ( !structural )
					packsLeafOnly++;
			}
		}

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
			/* An hknpScaledConvexShape carries its child's geometry, so it satisfies
			 * every convex test here while its rawOffset names a 112-byte wrapper.
			 * Checking it as a polytope compares a polytope against a wrapper; the
			 * packfile assembly is what covers it.
			 */
			if ( shp.primType == 0 && shp.isConvex && shp.rawOffset >= 0 && !shp.scaledChild
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
	if ( packs || packsSkipped ) {
		out() << "packfile   " << packs << "  byte-exact " << packsExact
			  << " / " << packs;
		if ( packsSkipped )
			out() << "  (" << packsSkipped << " not assembled)";
		out() << Qt::endl;
		if ( !packError.isEmpty() )
			out() << "  not assembled: " << packError << Qt::endl;
		if ( packsDerived )
			out() << "  from the model alone (shapes re-derived): byte-exact "
				  << packsDerivedExact << " / " << packsDerived << Qt::endl;
		if ( packsLeafOnly )
			out() << "  +" << packsLeafOnly << " differing only inside leaf objects"
				  << " (derived bytes, not the assembly)" << Qt::endl;
		if ( packFirstDiff >= 0 )
			out() << "  first structural difference at +0x" << QString::number( packFirstDiff, 16 )
				  << ", size " << packSizeNow << " vs " << packSizeWas << Qt::endl;
		if ( !packDiffs.isEmpty() ) {
			out() << "  differing bytes by object:";
			for ( auto it = packDiffs.constBegin(); it != packDiffs.constEnd(); ++it )
				out() << " " << it.key() << "=" << it.value();
			out() << Qt::endl;
		}
	}
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
			|| massPropsExact + massPropsInert < massProps
			|| packsExact + packsLeafOnly < packs ) ? 1 : 0;
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
		|| massPropsExact + massPropsInert < massProps
		|| packsExact + packsLeafOnly < packs ) ? 1 : 0;
}

/*! Does a joint survive the trip through its editable NIF form?
 *
 * `--roundtrip` already measures the two ends of the pipe: it encodes each joint
 * from its stored bytes and again from the bare template, and reports both
 * against the file. Neither run touches a NIF block, so neither says anything
 * about the part that was actually missing.
 *
 * This does. Every joint is encoded twice from the SAME template path -- once
 * straight from the decode, and once after being written into a
 * bhkRagdollConstraint / bhkLimitedHingeConstraint block and read back out -- and
 * the two must be byte-identical. The comparison is against the direct path
 * rather than against vanilla on purpose: it isolates the carrier. Anything the
 * mapping drops, mis-names or swaps changes those bytes, and nothing else does.
 *
 * It also measures the identity the frame NAMES rest on: the third basis vector
 * of each side is the cross product of the first two ("Motor A" is
 * "Twist A x Plane A", "Perp Axis In A2" is "Axis A x Perp Axis In A1"). If the
 * four hkVector4s were named in any other order that would not hold.
 */
int cmdCollisionConstraints( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int joints = 0, carried = 0, otherKinds = 0, unwritten = 0;
	int vanillaExact = 0, vanillaInert = 0, vanillaSeen = 0;
	int breakables = 0, motors = 0;
	float worstCross = 0.0f, worstNamed = 0.0f;
	int named = 0, namedBad = 0;
	QMap<int, int> lostAt;      // offset -> how often the carried bytes differed there

	auto cross = []( const Vector3 & u, const Vector3 & v ) {
		return Vector3( u[1] * v[2] - u[2] * v[1],
						u[2] * v[0] - u[0] * v[2],
						u[0] * v[1] - u[1] * v[0] );
	};

	// blocks are appended, so the systems keep their numbers while this inserts
	const int systemBlocks = nif.getBlockCount();
	for ( int b = 0; b < systemBlocks; b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( !nif.blockInherits( i, "bhkPhysicsSystem" ) && !nif.blockInherits( i, "bhkRagdollSystem" ) )
			continue;
		const QByteArray bytes = nif.get<QByteArray>( i, "Binary Data" );
		if ( bytes.isEmpty() )
			continue;
		const HknpSystem sys = hknpDecode( bytes );
		if ( !sys.valid )
			continue;

		for ( const HknpConstraint & jc : sys.constraints ) {
			const bool ragdoll = ( jc.kind == QLatin1String( "hkpRagdollConstraintData" ) );
			const bool hinge = ( jc.kind == QLatin1String( "hkpLimitedHingeConstraintData" ) );
			if ( !ragdoll && !hinge ) {
				otherKinds++;
				continue;
			}
			joints++;
			if ( jc.breakable )
				breakables++;
			if ( !jc.motorPointers.isEmpty() )
				motors++;
			if ( jc.hasFrames ) {
				worstCross = std::max( worstCross,
					( jc.rotA[2] - cross( jc.rotA[0], jc.rotA[1] ) ).length() );
				worstCross = std::max( worstCross,
					( jc.rotB[2] - cross( jc.rotB[0], jc.rotB[1] ) ).length() );
			}

			auto encode = [ragdoll]( const HknpConstraint & c ) {
				return ragdoll ? hknpEncodeRagdollConstraintData( c )
							   : hknpEncodeLimitedHingeConstraintData( c );
			};
			// the template path, which is what a joint with no original bytes gets
			HknpConstraint fresh = jc;
			fresh.rawData.clear();
			const QByteArray direct = encode( fresh );

			const int blk = tlCollWriteConstraint( &nif, jc, -1, -1 );
			HknpConstraint back;
			if ( blk < 0 || !tlCollReadConstraint( &nif, nif.getBlockIndex( blk ), back, nullptr, nullptr ) ) {
				unwritten++;
				continue;
			}
			/* AND THE NAMES, which the check above cannot see.
			 *
			 * Writer and reader share one name table, so exchanging two of them
			 * cancels out and the bytes still match -- swapping "Plane A" with
			 * "Motor A" passed 38 of 38 before this existed. What separates a right
			 * naming from a wrong one is a property of the FIELDS: the third basis
			 * vector is the cross product of the first two, which is how NifSkope's
			 * own "Recompute B Frame from A" authors Motor A. So read the block back
			 * by NIF field name and require that identity to hold there.
			 */
			{
				const QModelIndex iDesc = tlCollDescriptor( &nif, nif.getBlockIndex( blk ), ragdoll );
				static const char * const rag[6] = { "Twist A", "Plane A", "Motor A",
					"Twist B", "Plane B", "Motor B" };
				static const char * const hng[6] = { "Axis A", "Perp Axis In A1", "Perp Axis In A2",
					"Axis B", "Perp Axis In B1", "Perp Axis In B2" };
				const char * const * f = ragdoll ? rag : hng;
				float worst = 0.0f;
				for ( int side = 0; side < 2; side++ ) {
					const Vector3 v0( nif.get<Vector4>( iDesc, QLatin1String( f[side * 3] ) ) );
					const Vector3 v1( nif.get<Vector4>( iDesc, QLatin1String( f[side * 3 + 1] ) ) );
					const Vector3 v2( nif.get<Vector4>( iDesc, QLatin1String( f[side * 3 + 2] ) ) );
					worst = std::max( worst, ( v2 - cross( v0, v1 ) ).length() );
				}
				named++;
				worstNamed = std::max( worstNamed, worst );
				if ( worst > 1.0e-3f )
					namedBad++;
			}

			const QByteArray through = encode( back );
			if ( through == direct ) {
				carried++;
			} else {
				for ( int o = 0; o + 4 <= direct.size(); o += 4 ) {
					if ( through.mid( o, 4 ) != direct.mid( o, 4 ) )
						lostAt[o]++;
				}
			}

			/* And against the file, with the same inert-w-lane tolerance
			 * --roundtrip applies: the w lanes of a rotation basis are SIMD
			 * residue Havok ignores, so a freshly authored joint cannot match
			 * them and does not need to.
			 */
			if ( jc.rawOffset >= 0 && jc.rawData.size() == direct.size() ) {
				vanillaSeen++;
				const QByteArray was = bytes.mid( jc.rawOffset, direct.size() );
				bool inert = true;
				for ( int o = 0; o + 4 <= was.size(); o += 4 ) {
					if ( through.mid( o, 4 ) == was.mid( o, 4 ) )
						continue;
					const bool wLane = ( o == 0x3c || o == 0x4c || o == 0x5c || o == 0x6c
									  || o == 0x7c || o == 0x8c || o == 0x9c || o == 0xac );
					if ( !wLane )
						inert = false;
				}
				if ( through == was )
					vanillaExact++;
				else if ( inert )
					vanillaInert++;
			}
		}
	}

	out() << "file        " << file << Qt::endl;
	out() << "joints      " << joints;
	if ( breakables )
		out() << "  (" << breakables << " breakable)";
	if ( motors )
		out() << "  (" << motors << " with a motor)";
	out() << Qt::endl;
	if ( otherKinds )
		out() << "  " << otherKinds << " of a kind with no NIF block, not carried" << Qt::endl;
	if ( !joints ) {
		out() << "  no joints to check" << Qt::endl;
		return 0;
	}
	out() << "  through the NIF form, byte-identical  " << carried << " / " << joints << Qt::endl;
	if ( unwritten )
		out() << "  could not be written at all         " << unwritten << Qt::endl;
	if ( !lostAt.isEmpty() ) {
		out() << "  offsets the carrier changed:";
		for ( auto it = lostAt.constBegin(); it != lostAt.constEnd(); ++it )
			out() << " +0x" << QString::number( it.key(), 16 ) << "(" << it.value() << ")";
		out() << Qt::endl;
	}
	out() << "  worst |row2 - row0 x row1|            " << worstCross << Qt::endl;
	if ( named )
		out() << "  same identity read back BY FIELD NAME  " << ( named - namedBad ) << " / "
			  << named << ", worst " << worstNamed << Qt::endl;
	if ( vanillaSeen )
		out() << "  vs the file: exact " << vanillaExact << ", inert w lanes only "
			  << vanillaInert << " of " << vanillaSeen << Qt::endl;
	return ( carried < joints || namedBad ) ? 1 : 0;
}

/*! Is a ragdoll's reference pose derivable from the NIF's own node hierarchy?
 *
 * `hknpRagdollData` carries an `hkaSkeleton` -- the ragdoll's private copy of the
 * bone tree -- and no NIF block holds one. Two ways to get it back: carry it in a
 * new block, or DERIVE it, since bone index equals body index, every body names
 * its node, and a node already has a transform.
 *
 * Deriving is much the better answer if it is true, and it is exactly the kind of
 * claim that is comfortable to assume and cheap to check. So check it: for every
 * bone, take its node's world transform and its PARENT BONE's node's world
 * transform, express one in the other, and compare against the stored pose.
 *
 * The parent chain is compared too. The pose is local to the parent BONE, and a
 * NIF node's transform is local to its parent NODE -- those are only the same
 * thing if the two hierarchies agree, which is itself worth measuring rather than
 * assuming.
 */
int cmdCollisionSkeleton( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int skeletons = 0, bones = 0, resolved = 0, roots = 0;
	int chainSame = 0, chainSeen = 0;
	int transOk = 0, rotOk = 0, checked = 0;
	int bTransOk = 0, bRotOk = 0, bChecked = 0;
	int rootIsBody = 0, rootIsIdentity = 0, rootSeen = 0;
	float worstBTrans = 0.0f, worstBRot = 0.0f;
	int scaleUnit = 0, scaleNear = 0, scaleOther = 0;
	int lockT = 0, lockRule = 0;
	// the three fields the node hierarchy cannot supply: do they follow a rule?
	QMap<quint32, int> transW, scaleW;
	QMap<quint32, int> scaleBitsRoot, scaleBitsChild;
	float worstTrans = 0.0f, worstRot = 0.0f;
	QString worstWhere;
	QStringList offenders;

	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( !nif.blockInherits( i, "bhkPhysicsSystem" ) && !nif.blockInherits( i, "bhkRagdollSystem" ) )
			continue;
		const QByteArray bytes = nif.get<QByteArray>( i, "Binary Data" );
		if ( bytes.isEmpty() )
			continue;
		const HknpSystem sys = hknpDecode( bytes );
		if ( !sys.valid || sys.bones.isEmpty() )
			continue;
		skeletons++;

		// body index -> the node its collision object names, which is the only
		// binding between a packfile body and anything in the NIF
		QHash<int, int> nodeOfBody;
		const int sysNum = b;
		for ( int o = 0; o < nif.getBlockCount(); o++ ) {
			const QModelIndex io = nif.getBlockIndex( o );
			if ( !nif.blockInherits( io, "bhkNPCollisionObject" ) || nif.getLink( io, "Data" ) != sysNum )
				continue;
			const int target = nif.getLink( io, "Target" );
			nodeOfBody.insert( int( nif.get<quint32>( io, "Body ID" ) ),
				nif.isValidBlockNumber( target ) ? target : nif.getParent( o ) );
		}

		for ( qsizetype k = 0; k < sys.bones.size(); k++ ) {
			const HknpBone & bone = sys.bones.at( k );
			bones++;
			if ( bone.lockTranslation )
				lockT++;
			// the candidate rule: set on every bone except the root
			if ( bone.lockTranslation == ( bone.parent >= 0 ) )
				lockRule++;
			transW[bone.poseTransW]++;
			scaleW[bone.poseScaleW]++;
			{
				quint32 bits = 0;
				std::memcpy( &bits, &bone.scale[0], 4 );
				( bone.parent < 0 ? scaleBitsRoot : scaleBitsChild )[bits]++;
			}
			// the scale question, asked as a bit pattern: 0.99999994 prints as 1.0000
			if ( bone.scale[0] == 1.0f && bone.scale[1] == 1.0f && bone.scale[2] == 1.0f )
				scaleUnit++;
			else if ( std::fabs( bone.scale[0] - 1.0f ) < 1.0e-6f )
				scaleNear++;
			else
				scaleOther++;

			const int node = nodeOfBody.value( int( k ), -1 );
			if ( node < 0 )
				continue;
			resolved++;
			if ( bone.parent < 0 ) {
				roots++;
				/* A root bone's pose is local to nothing, so it is either the root
				 * BODY's own rest transform or the identity. The builder has to
				 * write one of them and there is no way to tell by looking.
				 */
				if ( int( k ) < sys.bodyPhys.size() ) {
					rootSeen++;
					const HknpBodyPhys & rp = sys.bodyPhys.at( int( k ) );
					const bool tBody = ( rp.position - bone.translation ).length() <= 1.0e-4f;
					float rdot = 0.0f;
					for ( int c = 0; c < 4; c++ )
						rdot += rp.orientation[c] * bone.rotation[c];
					bool rBody = true;
					for ( int c = 0; c < 4; c++ )
						rBody = rBody && std::fabs( ( rdot < 0.0f ? -rp.orientation[c] : rp.orientation[c] )
							- bone.rotation[c] ) <= 1.0e-4f;
					const bool ident = bone.translation.length() <= 1.0e-6f
						&& std::fabs( std::fabs( bone.rotation[0] ) - 1.0f ) <= 1.0e-6f;
					if ( ident )
						rootIsIdentity++;
					else if ( tBody && rBody )
						rootIsBody++;
				}
				continue;
			}
			const int parentNode = nodeOfBody.value( bone.parent, -1 );
			if ( parentNode < 0 )
				continue;

			// does the NODE tree agree with the BONE tree about who the parent is?
			chainSeen++;
			{
				int walk = nif.getParent( node );
				while ( walk >= 0 && walk != parentNode )
					walk = nif.getParent( walk );
				if ( walk == parentNode )
					chainSame++;
			}

			const Transform wc = skeletonWorldTransform( &nif, node );
			const Transform wp = skeletonWorldTransform( &nif, parentNode );
			const Matrix rInv = wp.rotation.inverted();
			const float ps = ( wp.scale != 0.0f ) ? wp.scale : 1.0f;
			const Vector3 relT = rInv * ( wc.translation - wp.translation ) / ps;
			const Matrix relR = rInv * wc.rotation;

			// the pose is in Havok units, the node in game units
			const Vector3 derived = relT / 69.99125f;
			const float dt = ( derived - bone.translation ).length();
			const Quat rq = relR.toQuat();
			// a quaternion and its negation are the same rotation
			float dot = 0.0f;
			for ( int c = 0; c < 4; c++ )
				dot += rq[c] * bone.rotation[c];
			float dr = 0.0f;
			for ( int c = 0; c < 4; c++ ) {
				const float d = ( dot < 0.0f ? -rq[c] : rq[c] ) - bone.rotation[c];
				dr = std::max( dr, std::fabs( d ) );
			}
			/* The second derivation: from the BODIES.
			 *
			 * cinfo +0x30 is the body's own rest position, and on a ragdoll that is
			 * the bone origin -- Decompile already carries it as bhkRigidBody's
			 * Center, with +0x40 as its Rotation. If the pose comes from these
			 * rather than from the node transforms, nothing new has to be carried
			 * at all.
			 */
			if ( int( k ) < sys.bodyPhys.size() && bone.parent < sys.bodyPhys.size() ) {
				const HknpBodyPhys & pc = sys.bodyPhys.at( int( k ) );
				const HknpBodyPhys & pp = sys.bodyPhys.at( bone.parent );
				Matrix mc, mp;
				mc.fromQuat( pc.orientation );
				mp.fromQuat( pp.orientation );
				const Matrix mpInv = mp.inverted();
				const Vector3 bt = mpInv * ( pc.position - pp.position );
				const Matrix br = mpInv * mc;
				const float bdt = ( bt - bone.translation ).length();
				const Quat bq = br.toQuat();
				float bdot = 0.0f;
				for ( int c = 0; c < 4; c++ )
					bdot += bq[c] * bone.rotation[c];
				float bdr = 0.0f;
				for ( int c = 0; c < 4; c++ )
					bdr = std::max( bdr, std::fabs( ( bdot < 0.0f ? -bq[c] : bq[c] ) - bone.rotation[c] ) );
				bChecked++;
				if ( bdt <= 1.0e-4f )
					bTransOk++;
				if ( bdr <= 1.0e-4f )
					bRotOk++;
				worstBTrans = std::max( worstBTrans, bdt );
				worstBRot = std::max( worstBRot, bdr );
			}
			checked++;
			if ( dt <= 1.0e-4f )
				transOk++;
			if ( dr <= 1.0e-4f )
				rotOk++;
			if ( ( dt > 1.0e-4f || dr > 1.0e-4f ) && offenders.size() < 8 ) {
				auto v3 = []( const Vector3 & v ) {
					return QStringLiteral( "%1 %2 %3" ).arg( v[0], 0, 'f', 4 )
						.arg( v[1], 0, 'f', 4 ).arg( v[2], 0, 'f', 4 );
				};
				offenders << QStringLiteral( "    %1 (parent %2): stored T %3 | from node %4 | dT %5 dR %6" )
					.arg( nif.get<QString>( nif.getBlockIndex( node ), "Name" ) )
					.arg( nif.get<QString>( nif.getBlockIndex( parentNode ), "Name" ) )
					.arg( v3( bone.translation ), v3( derived ) )
					.arg( dt, 0, 'g', 3 ).arg( dr, 0, 'g', 3 );
			}
			if ( dt > worstTrans ) {
				worstTrans = dt;
				worstWhere = nif.get<QString>( nif.getBlockIndex( node ), "Name" );
			}
			worstRot = std::max( worstRot, dr );
		}
	}

	out() << "file        " << file << Qt::endl;
	if ( !skeletons ) {
		out() << "  no hkaSkeleton in this file" << Qt::endl;
		return 0;
	}
	out() << "skeletons   " << skeletons << "   bones " << bones << Qt::endl;
	out() << "  bone -> node resolved                " << resolved << " / " << bones
		  << "  (" << roots << " root)" << Qt::endl;
	out() << "  node tree agrees about the parent    " << chainSame << " / " << chainSeen << Qt::endl;
	out() << "  translation derivable from the node  " << transOk << " / " << checked
		  << "   worst " << worstTrans << " m";
	if ( !worstWhere.isEmpty() )
		out() << " at " << worstWhere;
	out() << Qt::endl;
	out() << "  rotation    derivable from the node  " << rotOk << " / " << checked
		  << "   worst " << worstRot << Qt::endl;
	out() << "  translation derivable from the BODY  " << bTransOk << " / " << bChecked
		  << "   worst " << worstBTrans << " m" << Qt::endl;
	out() << "  rotation    derivable from the BODY  " << bRotOk << " / " << bChecked
		  << "   worst " << worstBRot << Qt::endl;
	for ( const QString & o : std::as_const( offenders ) )
		out() << o << Qt::endl;
	out() << "  reference scale: exactly 1  " << scaleUnit
		  << ", within 1e-6  " << scaleNear << ", other " << scaleOther << Qt::endl;
	out() << "  root pose: identity " << rootIsIdentity << ", its own body "
		  << rootIsBody << ", neither " << ( rootSeen - rootIsIdentity - rootIsBody )
		  << " of " << rootSeen << Qt::endl;
	out() << "  lockTranslation set on               " << lockT << " / " << bones
		  << "   follows \"every bone but the root\" " << lockRule << " / " << bones << Qt::endl;
	auto hist = [&]( const char * label, const QMap<quint32, int> & m ) {
		out() << "  " << label;
		for ( auto it = m.constBegin(); it != m.constEnd(); ++it )
			out() << " 0x" << QString::number( it.key(), 16 ) << "=" << it.value();
		out() << Qt::endl;
	};
	hist( "pose translation w lane ", transW );
	hist( "pose scale w lane       ", scaleW );
	hist( "scale.x bits, ROOT      ", scaleBitsRoot );
	hist( "scale.x bits, non-root  ", scaleBitsChild );
	return 0;
}

/*! Per-body physics, so a rebuilt file can be diffed against vanilla's.
 *
 * The collision inventory prints what a body IS -- layer, shapes, material -- and
 * nothing about what it WEIGHS. Mass, inertia and the motion properties are
 * exactly the fields a ragdoll's behaviour comes from, and the fields no
 * comparison of shapes or bones can see, so they need their own dump.
 *
 * Written because a rebuilt human ragdoll came back light and unstable in game
 * while every offline check of it was green.
 */
int cmdCollisionBodies( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	auto f = []( float v ) { return QString::number( v, 'g', 7 ); };
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
		out() << "system " << nif.itemName( i ) << "  bodies " << sys.bodyPhys.size()
			  << "  motion " << sys.motionCount << "  inertia " << sys.inertiaCount << Qt::endl;
		for ( qsizetype k = 0; k < sys.bodyPhys.size(); k++ ) {
			const HknpBodyPhys & p = sys.bodyPhys.at( k );
			out() << "  body " << k
				  << " mass " << f( p.mass )
				  << " density " << f( p.density )
				  << " invMass " << f( p.invMassStored )
				  << " invInertia " << f( p.invInertia[0] ) << "," << f( p.invInertia[1] )
				  << "," << f( p.invInertia[2] )
				  << " motionIdx " << p.motionIndex
				  << " grav " << f( p.gravityFactor )
				  << " linDamp " << f( p.linDamping )
				  << " angDamp " << f( p.angDamping )
				  << " maxLin " << f( p.maxLinVelocity )
				  << " maxAng " << f( p.maxAngVelocity )
				  << " fric " << f( p.friction )
				  << " rest " << f( p.restitution )
				  << " flags 0x" << QString::number( p.cinfoFlags, 16 )
				  << " matFlags 0x" << QString::number( p.materialFlags, 16 )
				  << " trigger " << p.triggerType
				  << " layer " << p.layer
				  << " com " << f( p.motionCom[0] ) << "," << f( p.motionCom[1] )
				  << "," << f( p.motionCom[2] )
				  << Qt::endl;
		}
	}
	return 0;
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
		// recognised, carried, and contributing nothing — the state hknpConvexShape
		// sat in undetected until 17 files refused to re-assemble
		if ( !sys.geometrylessShapes.isEmpty() )
			out() << "  no geometry              " << sys.geometrylessShapes.join( QStringLiteral( ", " ) )
				  << Qt::endl;
		if ( sys.readTruncated )
			out() << "  read out of range        first at +0x"
				  << QString::number( sys.readTruncatedAt, 16 ) << " (blob is "
				  << bytes.size() << " bytes); each failure is confined to its own item"
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

		/* Zero-area triangles, which nothing can hit, see or collide with.
		 *
		 * Reported because they were ours: the sphere and capsule previews put a
		 * full ring of coincident vertices at each pole, so 24 of every 144
		 * triangles covered nothing while being drawn, ray-tested and counted
		 * against the collision budget. A number here is the guard against that
		 * coming back, and against a hull arriving with a repeated corner.
		 */
		int degenerate = 0, totalTris = 0;
		for ( const HknpShape & shp : sys.shapes ) {
			for ( const Triangle & t : shp.tris ) {
				totalTris++;
				if ( t[0] >= shp.verts.size() || t[1] >= shp.verts.size()
					|| t[2] >= shp.verts.size() )
					continue;
				const Vector3 e1 = shp.verts.at( t[1] ) - shp.verts.at( t[0] );
				const Vector3 e2 = shp.verts.at( t[2] ) - shp.verts.at( t[0] );
				if ( Vector3::crossproduct( e1, e2 ).length() < 1.0e-12f )
					degenerate++;
			}
		}
		out() << "  preview triangles        " << totalTris
			  << ( degenerate ? QString( ", %1 DEGENERATE" ).arg( degenerate ) : QString() )
			  << Qt::endl;

		out() << QString( "  %1 %2 %3 %4 %5 %6 %7" ).arg( "body", -6 ).arg( "node", -34 )
					.arg( "layer", -6 ).arg( "shapes", -7 ).arg( "friction", -9 )
					.arg( "restitution", -12 ).arg( "material" ) << Qt::endl;
		for ( const auto & ref : it.value() ) {
			int mine = 0;
			for ( const HknpShape & shp : sys.shapes ) {
				if ( shp.bodyId >= 0 ? quint32( shp.bodyId ) == ref.first : ref.first == 0 )
					mine++;
			}
			const HknpBodyPhys phys = int( ref.first ) < sys.bodyPhys.size()
									? sys.bodyPhys.at( int( ref.first ) ) : HknpBodyPhys();
			/* Friction and restitution too: the solver is about to honour them per
			 * body, and until now the only place either was visible was the
			 * Collision Manager's selected-body editor -- one body at a time,
			 * which is no way to learn what a corpus actually carries.
			 */
			out() << QString( "  %1 %2 %3 %4 %5 %6 %7" )
						.arg( ref.first, -6 )
						.arg( nif.get<QString>( nif.getBlockIndex( ref.second ), "Name" ), -34 )
						.arg( phys.layer, -6 ).arg( mine, -7 )
						.arg( phys.friction, -9, 'f', 3 ).arg( phys.restitution, -12, 'f', 3 )
						.arg( QStringLiteral( "0x%1" )
							.arg( phys.materialCRC, 8, 16, QLatin1Char( '0' ) ).toUpper() )
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
		out() << QString( "  %1 %2 %3 %4 %5" ).arg( "shape", -6 ).arg( "class", -34 )
					.arg( "body", -6 ).arg( "material", -12 ).arg( "geometry" ) << Qt::endl;
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
			out() << QString( "  %1 %2 %3 %4 %5" )
						.arg( i, -6 ).arg( shp.className, -34 ).arg( shp.bodyId, -6 )
						.arg( QStringLiteral( "0x%1" )
							.arg( shp.shapeMaterialCRC, 8, 16, QLatin1Char( '0' ) ).toUpper(), -12 )
						.arg( geom )
				  << Qt::endl;
			/* A compressed mesh can hold MANY materials -- one per primitive,
			 * through the CMSD run table -- and the column above shows only the
			 * shape-level fallback, which on Toilet01 is zero while its three
			 * real materials sit in the table. So print the table and how the
			 * triangles divide over it whenever there is more than one.
			 */
			if ( shp.materialTable.size() > 1 ) {
				QStringList parts;
				for ( int m = 0; m < shp.materialTable.size(); m++ ) {
					const quint32 crc = shp.materialTable.at( m );
					int n = 0;
					for ( quint32 t : shp.triMaterial ) if ( t == crc ) n++;
					parts << QStringLiteral( "0x%1 (%2 t)" )
						.arg( QStringLiteral( "%1" ).arg( crc, 8, 16, QLatin1Char( '0' ) ).toUpper() )
						.arg( n );
				}
				out() << QString( "  %1 materials %2: %3" ).arg( "", -6 )
							.arg( shp.materialTable.size() ).arg( parts.join( QStringLiteral( ", " ) ) )
					  << Qt::endl;
			}
		}
	}
	return 0;
}

/*! Write the document NifSkope opens with when no file was given.
 *
 * Same builder as the GUI startup path, so this is how that document gets
 * checked without a window.
 *
 * `--cube` puts one cube shape in it instead. That is a FIXTURE, not the
 * program's new document: the harnesses for the block list, renaming, merging
 * and collision need a small Fallout 4 scene with real geometry and must not
 * need a game corpus to get one, and Add Primitive cannot make the first shape
 * in a document because it clones an existing one.
 */
int cmdNew( const QString & outFile, bool cube, float size )
{
	NifModel nif;
	QString error;
	const bool built = cube
		? nifCreateCubeScene( &nif, size > 0.0f ? size : STARTER_CUBE_SIZE, &error )
		: nifCreateStarterScene( &nif, &error );
	if ( !built ) {
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

//! `btd <file.btd>` — FO76 terrain database to terrain geometry. --info prints
//! the header and stops; otherwise the region (default: the whole worldspace
//! at LOD4) is built with the same generator the GUI's File > Open uses.
int cmdBtd( const QString & file, bool infoOnly, bool haveRegion,
	int rx0, int ry0, int rx1, int ry1, int lod, const QString & outFile )
{
	BtdWorldInfo info;
	QString error;
	if ( !btdReadWorldInfo( file, info, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	out() << "worldspace cells [" << info.cellMinX << "," << info.cellMinY
		  << "]..[" << info.cellMaxX << "," << info.cellMaxY << "]"
		  << "  heights " << info.heightMin << " to " << info.heightMax
		  << "  land textures " << info.landTextureCount
		  << "  ground covers " << info.groundCoverCount << Qt::endl;
	if ( infoOnly )
		return 0;

	BtdRegionSpec spec = btdDefaultRegion( info );
	if ( haveRegion ) {
		spec.x0 = rx0;
		spec.y0 = ry0;
		spec.x1 = rx1;
		spec.y1 = ry1;
	}
	if ( lod >= 0 )
		spec.lod = lod;

	qint64 shapes = 0, vertCount = 0;
	if ( !btdEstimateRegion( info, spec, &shapes, &vertCount, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	out() << "region [" << spec.x0 << "," << spec.y0 << "]..[" << spec.x1
		  << "," << spec.y1 << "] LOD" << spec.lod << ": " << shapes
		  << " shape(s), " << vertCount << " vertices" << Qt::endl;

	NifModel nif;
	if ( !nifCreateBtdTerrainScene( &nif, file, spec, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	return saveNif( nif, outFile ) ? 0 : 1;
}

/*! `lodl <file.lodl>` — our own whole-worldspace landscape file to terrain
 *  geometry, through the SAME generator the GUI's File > Open uses, so a
 *  headless render and a window show the same scene. `--info` prints the header
 *  and the plane inventory and stops.
 *
 *  `--plane KEY` picks which stored plane paints the surface; the notes the
 *  builder returns are printed, because a plane that reads back as one constant
 *  value and a plane that is missing look identical in a picture and the
 *  difference has to be a NUMBER.
 */
int cmdLodt( const QString & file, bool infoOnly, bool haveRegion,
	int rx0, int ry0, int rx1, int ry1, int lod, const QString & planeKey,
	const QString & outFile, bool waterCensus, bool waterSelfTest )
{
	LodtWorldInfo info;
	QString error;
	/* --water-census reads the FILE, not the writer that made it, and prints
	 * the body table it finds. It runs before the header print because a file
	 * with no bodies must SAY so and stop, rather than draw an empty table. */
	if ( waterSelfTest ) {
		/* The control needs no file at all -- the worldspace it classifies is
		 * built in memory -- but the command takes one, so it is accepted and
		 * ignored rather than made a second spelling of the command. */
		QString report;
		const bool ok = lodtWaterSelfTest( &report, &error );
		out() << report << Qt::endl;
		if ( !ok && !error.isEmpty() )
			err() << "error: " << error << Qt::endl;
		return ok ? 0 : 1;
	}
	if ( waterCensus ) {
		QString census;
		if ( !lodtWaterCensus( file, &census, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << census << Qt::endl;
		return 0;
	}
	if ( !lodtReadWorldInfo( file, info, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	out() << "worldspace cells [" << info.cellMinX << "," << info.cellMinY
		  << "]..[" << info.cellMaxX << "," << info.cellMaxY << "]"
		  << "  heights " << info.heightMin << " to " << info.heightMax
		  << "  quantum " << info.heightQuantum
		  << "  samples/cell " << info.samplesPerCell
		  << "  block edge " << info.blockEdge
		  << "  levels " << info.levelCount
		  << "  blocks " << info.blockCount << Qt::endl;
	out() << "sections 0x" << QString::number( info.sectionFlags, 16 )
		  << "  ltex " << info.ltexCount << "  watr " << info.watrCount
		  << "  gcvr " << info.gcvrCount
		  << "  ao " << info.aoSamples << "/cell"
		  << "  overview " << info.overviewSamples << "/cell" << Qt::endl;
	{
		QStringList keys;
		for ( LodtPlane p : lodtAvailablePlanes( info ) )
			keys << QLatin1String( lodtPlaneKey( p ) );
		out() << "planes " << keys.join( QLatin1Char( ' ' ) ) << Qt::endl;
	}
	if ( infoOnly )
		return 0;

	LodtRegionSpec spec = lodtDefaultRegion( info );
	if ( haveRegion ) {
		spec.x0 = rx0;
		spec.y0 = ry0;
		spec.x1 = rx1;
		spec.y1 = ry1;
	}
	if ( lod >= 0 )
		spec.lod = lod;
	if ( !planeKey.isEmpty() && !lodtPlaneFromKey( planeKey, spec.plane ) ) {
		err() << "error: unknown plane '" << planeKey << "'" << Qt::endl;
		return 1;
	}

	qint64 shapes = 0, vertCount = 0;
	if ( !lodtEstimateRegion( info, spec, &shapes, &vertCount, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	out() << "region [" << spec.x0 << "," << spec.y0 << "]..[" << spec.x1
		  << "," << spec.y1 << "] LOD" << spec.lod
		  << " plane " << QLatin1String( lodtPlaneKey( spec.plane ) )
		  << ": " << shapes << " shape(s), " << vertCount << " vertices" << Qt::endl;

	NifModel nif;
	QString notes;
	if ( !nifCreateLodtTerrainScene( &nif, file, spec, &error, &notes ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	if ( !notes.isEmpty() )
		out() << notes << Qt::endl;
	return outFile.isEmpty() ? 0 : ( saveNif( nif, outFile ) ? 0 : 1 );
}

/*! The pre-flight estimate, printed by --vt-estimate, by the panel's summary
 *  line through the same estimator, and by --vt itself BEFORE it does any
 *  work. Without it `--vt --vt-finest 1` silently begins a job of several
 *  gigabytes, and `--vt-content 512 --vt-finest 1` a very much larger one: the
 *  refusals bound each knob alone and never their product.
 *
 *  `minutes` is printed as `unmeasured` and stays that way until a game-down
 *  timing run with a stated sample count has been recorded in WW_CHANGES.md.
 *  A number here that came from an extrapolation would be the kind of figure
 *  docs/MISTAKES.md exists for. */
static bool cmdLodgenVtEstimate( const EsmWorld & world, const LodgenVtOptions & opts,
	bool alsoBtr )
{
	LodgenVtEstimateOut e;
	if ( !lodgenVtEstimate( world, opts, alsoBtr, &e ) ) {
		err() << "error: the worldspace has no indexed cells to build a pyramid over" << Qt::endl;
		return false;
	}
	out() << QString( "vt estimate: levels %1 tiles %2 finest %3 coarsest %4 content %5 "
		"border %6 mips %7 cover %8 compression %9" )
		.arg( e.levels ).arg( e.tiles ).arg( opts.finestDim ).arg( e.coarsestDim )
		.arg( opts.content ).arg( opts.border ).arg( opts.mips )
		.arg( opts.cover.cover ? 1 : 0 ).arg( opts.compression ) << Qt::endl;
	out() << QString( "vt estimate: pyramid %1 btr %2 delivered %3 minutes unmeasured" )
		.arg( e.pyramidBytes ).arg( e.btrBytes ).arg( e.deliveredBytes ) << Qt::endl;
	for ( int i = 0; i < e.levels; i++ )
		out() << QString( "vt level %1 dim %2 tiles %3 unitsPerTexel %4 worldUnitsPerTile %5" )
			.arg( i ).arg( e.levelDims[i] ).arg( e.levelTiles[i] )
			.arg( e.levelDims[i] * 4096 / opts.content ).arg( e.levelDims[i] * 4096 ) << Qt::endl;
	if ( e.shortened )
		out() << QString( "note: this worldspace is tile-aligned only to dim %1; the pyramid stops "
			"there - %2 levels" ).arg( e.coarsestDim ).arg( e.levels ) << Qt::endl;
	out().flush();
	return true;
}

/*! The `.lodl` water-body module's switches, filled by the argument loop.
 *
 *  `cmdLodgen` already carries forty-five parameters; five more for one
 *  optional section would be churn nobody reads. Default-constructed means the
 *  module is OFF, which is the state every run that does not name
 *  `--water-bodies` is in. */
static LodtWaterOptions gLodlWater;

/*! INCREMENTAL REGENERATION (lane INCR1, 2026-09-12), filled by the argument
 *  loop for the same reason gLodlWater is: cmdLodgen already carries
 *  forty-five parameters.
 *
 *  `gLgIncremental` is the out-dir the operator pointed `--incremental` at,
 *  empty when the flag was not given.
 *
 *  `gLgSwitchDigest` is sha1 over the ARGUMENT VECTOR, in order, with the
 *  tokens in gLgSwitchSkip (src/lodgenchunkpass.cpp) and their values dropped;
 *  the record's `switches` line folds the IDENTITY WORD into it as well
 *  (lodgenSwitchesWithIdentity, lane INCRGATE1). It is deliberately
 *  CONSERVATIVE: reordering flags, or spelling a default explicitly, changes
 *  the digest and forces a full bake. That costs time and can never cost
 *  correctness, which is the right way round -- a switch digest that missed a
 *  flag would ship yesterday's sheets under today's settings.
 *  docs/LODGEN_LEDGER_FORMAT.md section 3 has the skip list and why each
 *  entry on it cannot reach a single output byte. */
static QString gLgIncremental;
static QString gLgSwitchDigest;

/*! The argument vector verbatim and the resource stack as the run was given
 *  them (lane BAKEREC1, 2026-09-17). Globals for the same reason the two above
 *  are: `cmdLodgen` already carries forty-five parameters. The record writes
 *  both, so the way back to reproducing a bake exactly is IN the bake. */
static QStringList gLgArgv;
static QStringList gLgResourceStack;

/*! `--bake-record <ws.lodb>` (lane BAKEREC1, 2026-09-17): print a record's
 *  summary and diff its plugin list against the one in hand. The output INCR1
 *  acts on. A global for the same reason the others are. */
static QString gLgBakeRecord;
/*! THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17), on by default.
 *  `--no-native-cache` is the EXACT way back: no `.lodj` is written, the
 *  record's `out` rows are what they were before this lane, and
 *  `--incremental --native` refuses exactly as it used to (CONSTITUTION 10).
 *  Sticky like `gLgBakeRecord` beside it, for the same reason: the parser
 *  and the driver are different functions and this is not worth a parameter
 *  in a signature that already has thirty. */
static bool gLgNativeCache = true;

/*! Print a line AND record it if it is a census line. One choke point, so a
 *  census line that is printed is a census line that is recorded. The FLOOR is
 *  in the gate: `tests/spells/lodgen_bakerec.sh` leg (a) greps the bake's own
 *  log for the registered keywords and requires the record's set to equal it,
 *  so a print site that forgot this helper fails a check. */
static void censusOut( const QString & line )
{
	lodbNoteCensus( line );
	out() << line << Qt::endl;
}

/* lodbRecordPath(), lodbFindRecord(), the switch-digest skip lists and
 * lodgenSwitchDigestOf() moved to src/lodgenchunkpass.cpp with the rest of the
 * ledger (lane INCRGATE1, 2026-09-24), so the panel writes the same record. */

/*! `--keep-bto` (lane BTOFREE1, 2026-09-16), a global for the same reason the
 *  two above are: cmdLodgen already carries forty-five parameters.
 *
 *  Under the FO4CS target (`--native <dir>`) the `.BTO` chunk files are
 *  scaffolding, not output: the texture arrays, the card arrays, the shape
 *  merge and the far-ring cut read them back, and nothing downstream of the
 *  bake does. Default now builds them in a scratch directory and removes them.
 *  `--keep-bto` is the exact way back -- the chunks land in the mod folder
 *  exactly as they did, byte for byte, which is what the gate pins. It is NOT
 *  in the switch-digest skip list, because it decides what is on disk. */
static bool gLgKeepBto = false;

//! `lodgen <file.esm>` — the LOD generation campaign's ESM record layer
//! (docs/LODGEN_PLAN.md rung 0). --list-worldspaces enumerates WRLD records;
//! --worldspace/--cell inspect one cell: LAND corner heights, REFR counts,
//! how many bases carry LOD models. The generation rungs build on this.
int cmdLodgen( const QString & file, bool listWorldspaces, quint32 worldspace,
	bool haveCell, int cellX, int cellY,
	bool haveTerrain, int chunkX, int chunkY, int dim, const QString & outFile,
	bool haveRegion, const int * region, const QString & outDir,
	bool haveObjects, const QString & dataRoot, bool identity,
	const QString & texDir, bool geomorph, bool terrainIdentity,
	const QString & impostors, int impostorFromLevel, const QString & candidateKind,
	bool listCandidates, bool atlas, bool arrays, bool merge, bool bakeAO,
	bool cullBuried, float cullMargin, bool aoGrey, int aoSkirt, int waterSubdiv,
	bool shoreDenser, int shoreDensity, int targetTris,
	const QString & heightmapDir, int heightmapSize, const QString & lodtDir,
	const QString & btdPath, bool btdProbe, bool verifyOnly,
	const QString & dumpLand, const QString & dumpLayers, bool refreshAo,
	bool slotFallback, bool atlasBc1, const LodgenSimplifyOptions & simplify,
	const LodgenCoverOptions & coverOpts, const LodgenVtOptions & vtOptsIn,
	const QString & vtDir, int vtBtr, bool vtEstimate, const QString & lodmCheck,
	const QString & lodvCheck, bool corpusHash, int cardAuxDiv, const QString & nativeDir, const QString & nativeVerifyLodo,
	const QString & nativeVerifyLodi, const QString & nativeFixture, const QString & nativeMeshReport,
	bool nativeVerifyCorpus, bool nativeLadder, bool nativeOccluders,
	bool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo, bool vertexAo, bool lodiV7,
	bool scrappable, bool identityJoinLegacy, float identityJoinGap, bool treesOnly,
	bool aggregate, int aggMin, int aggTile, int aggViews )
{
	/* The layout clause starts blank for this run and is filled by the writers
	 * themselves as they open their files (lane LAYOUT1, 2026-09-16). It is
	 * cleared HERE, at the head of the whole sub-command, because the pyramid,
	 * the landscape file and the native pair are written by three different
	 * branches below and a clear next to any one of them would throw the
	 * others' notes away. */
	lodgenClearLayoutCensus();
	/* Ground cover refusals, before any plugin is opened: a flag out of range
	 * must say so in one line rather than bake a worldspace and be wrong. */
	if ( !( coverOpts.tintStrength >= 0.0f && coverOpts.tintStrength <= 1.0f ) ) {
		err() << "error: --grass-tint takes a value in 0..1" << Qt::endl;
		return 2;
	}
	if ( !( coverOpts.coverFull >= 1.0f && coverOpts.coverFull <= 65535.0f ) ) {
		err() << "error: --cover-full takes a value in 1..65535" << Qt::endl;
		return 2;
	}
	if ( coverOpts.cover && texDir.isEmpty() ) {
		err() << "error: --cover needs --tex-dir; the cover plane lives in the "
				 "terrain data sheet" << Qt::endl;
		return 2;
	}
	/* Read a .lodm or a .lodt (the terrain texture sheets) through the repo's
	 * OWN parser/validator, so a
	 * harness checks the shipped implementation and not a reimplementation
	 * of it. Both print keyword lines and return 1 on a refusal. */
	if ( !nativeFixture.isEmpty() ) {
		QStringList rep; QString nerr;
		if ( !lodNativeFixtureWrite( nativeFixture, &rep, &nerr ) ) { err() << "error: " << nerr << Qt::endl; return 1; }
		for ( const QString & l : rep ) out() << l << Qt::endl;
		return 0;
	}
	/* ===== `--bake-record <ws.lodb>` (lane BAKEREC1, 2026-09-17) =========
	 *
	 * Prints what the record says, then -- when a plugin list is in hand
	 * (positional, `--plugins-txt` or `--mo2`) -- says what has moved since.
	 * Keyword lines, one fact a line, because this is the output lane INCR1
	 * parses to decide whether a partial rebake is even legal.
	 *
	 * It reads a file and loads no ESM of its own, so it answers in
	 * milliseconds; the plugin BYTE hashes are only recomputed for the
	 * plugins whose name and size still match, which is the only case the
	 * cheap fields cannot settle. */
	if ( !gLgBakeRecord.isEmpty() ) {
		LodgenLedger rec;
		QString rerr;
		if ( !lodgenReadLedger( gLgBakeRecord, &rec, &rerr ) ) {
			out() << "bake-record REFUSED " << rerr << Qt::endl;
			return 1;
		}
		out() << "bake-record path " << gLgBakeRecord << Qt::endl;
		out() << "bake-record worldspace " << rec.worldEdid << Qt::endl;
		out() << "bake-record worldspaceForm " << QString::number( rec.worldspace, 16 ) << Qt::endl;
		out() << "bake-record dim " << rec.dim << Qt::endl;
		out() << "bake-record region " << QString( "%1 %2 %3 %4" ).arg( rec.region[0] )
			.arg( rec.region[1] ).arg( rec.region[2] ).arg( rec.region[3] ) << Qt::endl;
		out() << "bake-record target " << ( rec.fo4csTarget ? "fo4cs" : "stock" ) << Qt::endl;
		out() << "bake-record exe " << rec.exeStamp << Qt::endl;
		out() << "bake-record exeBytes " << rec.exeBytes << Qt::endl;
		out() << "bake-record baked " << rec.bakedUtc << Qt::endl;
		out() << "bake-record switchesDigest " << rec.switches << Qt::endl;
		for ( const QString & n : { QStringLiteral( "loadOrderHash" ), QStringLiteral( "pluginCorpusHash" ),
				QStringLiteral( "objectCorpusHash" ), QStringLiteral( "modelCorpusHash" ),
				QStringLiteral( "cardCorpusHash" ) } ) {
			const QString v = n == QLatin1String( "loadOrderHash" )    ? rec.loadOrderHashHex
							: n == QLatin1String( "pluginCorpusHash" ) ? rec.pluginCorpusHashHex
							: n == QLatin1String( "objectCorpusHash" ) ? rec.objectCorpusHashHex
							: n == QLatin1String( "modelCorpusHash" )  ? rec.modelCorpusHashHex
							                                          : rec.cardCorpusHashHex;
			//  A hash the bake never wrote reads `n/a`, never a zero: a zero is
			//  a value and would be believed.
			out() << "bake-record hash " << n << " " << ( v.isEmpty() ? QStringLiteral( "n/a" ) : v ) << Qt::endl;
		}
		out() << "bake-record plugins " << rec.plugins.size() << Qt::endl;
		for ( const LodbPlugin & pl : rec.plugins )
			out() << QString( "bake-record plugin %1 %2 %3 %4 %5" ).arg( pl.index ).arg( pl.name )
				.arg( pl.bytes ).arg( pl.hash, 16, 16, QChar( '0' ) ).arg( pl.path ) << Qt::endl;
		out() << "bake-record resources " << rec.resources.size() << Qt::endl;
		for ( const LodbResource & r : rec.resources )
			out() << QString( "bake-record resource %1 %2 %3 %4" ).arg( r.kind ).arg( r.path )
				.arg( r.bytes ).arg( r.mtimeIso.isEmpty() ? QStringLiteral( "-" ) : r.mtimeIso ) << Qt::endl;
		out() << "bake-record switchTokens " << rec.switchTokens.size() << Qt::endl;
		out() << "bake-record command " << rec.switchTokens.join( QChar( ' ' ) ) << Qt::endl;
		out() << "bake-record chunks " << rec.chunks.size() << Qt::endl;
		int outs = 0;
		for ( const LodgenLedgerEntry & e : rec.chunks )
			outs += e.outFiles.size();
		out() << "bake-record outputs " << outs << Qt::endl;
		for ( const LodgenLedgerEntry & e : rec.chunks )
			out() << QString( "bake-record chunk %1 %2 %3 %4" ).arg( e.cx ).arg( e.cy )
				.arg( e.dim ).arg( e.inputs ) << Qt::endl;
		out() << "bake-record census " << rec.census.size() << Qt::endl;
		for ( const QString & c : rec.census )
			out() << "bake-record censusLine " << c << Qt::endl;
		out() << "bake-record endFiles " << rec.endFiles << Qt::endl;
		out() << "bake-record endBytes " << rec.endBytes << Qt::endl;

		/* THE END LINE, CHECKED against the folder the record sits in: a bake
		 * that died between two files, or a record truncated on the way to
		 * disk, shows here and nowhere else. */
		{
			int nf = 0;
			qint64 nb = 0;
			const QString dir = QFileInfo( gLgBakeRecord ).absolutePath();
			const QString self = QDir::fromNativeSeparators(
				QFileInfo( gLgBakeRecord ).absoluteFilePath() ).toLower();
			QDirIterator it( dir, QDir::Files, QDirIterator::Subdirectories );
			while ( it.hasNext() ) {
				it.next();
				if ( QDir::fromNativeSeparators( it.fileInfo().absoluteFilePath() ).toLower() == self )
					continue;
				nf++;
				nb += it.fileInfo().size();
			}
			out() << "bake-record endFilesNow " << nf << Qt::endl;
			out() << "bake-record endBytesNow " << nb << Qt::endl;
			out() << "bake-record endAgrees " << ( ( nf == rec.endFiles && nb == rec.endBytes ) ? 1 : 0 )
				  << Qt::endl;
		}

		/* THE TWO NORMALISERS, HELD AGAINST EACH OTHER (lane INCR1,
		 * 2026-09-17). `ww-volatile-field-law` step 3 says the mask is
		 * written twice, in two languages, because two implementations of
		 * one rule is the only way a wrong mask is caught. In this tree it
		 * WAS written twice and never compared: `lodbNormalise()` had no
		 * caller anywhere in src/, and every gate used the Python half
		 * (`tests/spells/lodb_read.py`) alone, so a fifth volatile field
		 * masked in one half and not the other would have gone unnoticed.
		 *
		 * A DIGEST is enough: the gate normalises the same file with the
		 * Python reader, hashes it the same way and compares one hex string,
		 * with no 97-line dump on either side. Empty lines are dropped before
		 * masking and the join ends in one newline, which is exactly what
		 * `lodb_read.normalise()` does -- the two agree on the TEXT, not just
		 * on the rule. */
		{
			QFile nf( gLgBakeRecord );
			if ( nf.open( QIODevice::ReadOnly ) ) {
				QStringList raw = QString::fromUtf8( nf.readAll() )
				                  .split( QChar( '\n' ) );
				raw.removeAll( QString() );
				const QStringList norm = lodbNormalise( raw );
				const QByteArray joined =
				    ( norm.join( QChar( '\n' ) )
				      + QChar( '\n' ) ).toUtf8();
				out() << "bake-record normalisedLines " << norm.size() << Qt::endl;
				out() << "bake-record normalisedSha1 "
				      << QString::fromLatin1( QCryptographicHash::hash( joined,
				            QCryptographicHash::Sha1 ).toHex() ) << Qt::endl;
			} else {
				out() << "bake-record normalisedSha1 n/a (cannot reopen the record)"
				      << Qt::endl;
			}
		}
		/* THE DIFF. `file` is the comma list this run was given -- positional,
		 * or the one `--plugins-txt` / `--mo2` resolved a few hundred lines
		 * above -- so the same list the bake would use. */
		if ( file.isEmpty() ) {
			out() << "bake-record diff n/a (no plugin list given: pass one positionally, "
					 "or --plugins-txt / --mo2)" << Qt::endl;
			return 0;
		}
		const QStringList moved = lodbDiffPlugins( rec.plugins, file );
		out() << "bake-record diffAgainst " << file << Qt::endl;
		out() << "bake-record moved " << moved.size() << Qt::endl;
		for ( const QString & m : moved )
			out() << "bake-record moved: " << m << Qt::endl;
		out() << "bake-record verdict "
			  << ( moved.isEmpty() ? "the load order is the one this bake was made from"
								   : "a partial rebake must treat every chunk those plugins reach as dirty" )
			  << Qt::endl;
		return moved.isEmpty() ? 0 : 1;
	}
	if ( !nativeVerifyLodo.isEmpty() ) {
		QString rep, nerr;
		/* --native-verify-corpus re-reads the plugin and recomputes the three
		 * staleness hashes. It is OPT-IN because the synthetic fixture's
		 * hashes are hand-made constants belonging to no plugin, and because
		 * loading Fallout4.esm costs seconds. A load failure REFUSES in words
		 * rather than quietly verifying less than was asked for. */
		EsmWorld corpusWorld;
		const EsmWorld * cw = nullptr;
		if ( nativeVerifyCorpus ) {
			QString werr;
			if ( !corpusWorld.load( file, worldspace ? worldspace : 0x3CU, &werr ) ) {
				err() << "error: --native-verify-corpus cannot read " << file << ": " << werr << Qt::endl;
				return 1;
			}
			cw = &corpusWorld;
		}
		if ( !lodgenNativeVerify( nativeVerifyLodo, nativeVerifyLodi, &rep, &nerr, cw ) ) { out() << "native REFUSED " << nerr << Qt::endl; return 1; }
		out() << rep << Qt::endl;
		return 0;
	}
	if ( !lodmCheck.isEmpty() ) {
		QFile mf( lodmCheck );
		if ( !mf.open( QIODevice::ReadOnly ) ) {
			err() << "error: cannot open " << lodmCheck << Qt::endl;
			return 1;
		}
		const QByteArray bytes = mf.readAll();
		const LodmMaterial m = lodmParse( bytes );
		out() << "lodm ok " << ( m.ok ? 1 : 0 ) << Qt::endl;
		if ( !m.ok ) {
			out() << "lodm error " << m.error << Qt::endl;
			return 1;
		}
		out() << "lodm version " << m.version << Qt::endl;
		out() << "lodm family " << m.family << Qt::endl;
		out() << "lodm kind " << m.kind << Qt::endl;
		out() << "lodm fileBytes " << bytes.size() << Qt::endl;
		out() << "lodm payloadBytes " << ( bytes.size() - 12 ) << Qt::endl;
		if ( m.root.contains( QStringLiteral( "terrain" ) ) ) {
			const QJsonObject t = m.root.value( QStringLiteral( "terrain" ) ).toObject();
			for ( auto it = t.constBegin(); it != t.constEnd(); ++it ) {
				const QJsonValue jv = it.value();
				if ( jv.isObject() || jv.isArray() )
					continue;
				// spelled out rather than through QVariant, so a bool prints
				// `true` and not `1` and a harness can grep for the word
				QString sv;
				if ( jv.isBool() )
					sv = jv.toBool() ? QStringLiteral( "true" ) : QStringLiteral( "false" );
				else if ( jv.isDouble() )
					sv = QString::number( jv.toDouble(), 'g', 12 );
				else
					sv = jv.toString();
				out() << "terrain " << it.key() << " " << sv << Qt::endl;
			}
			const QJsonArray ls = t.value( QStringLiteral( "levels" ) ).toArray();
			for ( const QJsonValue & v : ls ) {
				const QJsonObject o = v.toObject();
				out() << "level " << o.value( QStringLiteral( "index" ) ).toInt()
					  << " dim " << o.value( QStringLiteral( "dim" ) ).toInt()
					  << " tilesX " << o.value( QStringLiteral( "tilesX" ) ).toInt()
					  << " tilesY " << o.value( QStringLiteral( "tilesY" ) ).toInt()
					  << " unitsPerTexel " << o.value( QStringLiteral( "unitsPerTexel" ) ).toInt()
					  << " worldUnitsPerTile " << o.value( QStringLiteral( "worldUnitsPerTile" ) ).toInt()
					  << " tiles " << o.value( QStringLiteral( "tiles" ) ).toInt()
					  << " present " << o.value( QStringLiteral( "present" ) ).toInt()
					  << " container " << o.value( QStringLiteral( "container" ) ).toString()
					  << Qt::endl;
			}
		}
		return 0;
	}
	if ( !lodvCheck.isEmpty() ) {
		LodvHeaderFields hf;
		std::vector<LodvTileEntry> table;
		QString verr;
		if ( !lodvValidate( lodvCheck, &hf, &table, true, &verr ) ) {
			out() << "lodt ok 0" << Qt::endl;
			out() << "lodt " << verr << Qt::endl;
			return 1;
		}
		out() << "lodt ok 1" << Qt::endl;
		for ( const QString & l : lodvDescribe( hf, table ) )
			out() << "lodt " << l << Qt::endl;
		return 0;
	}
	if ( corpusHash ) {
		/* Both corpus hashes and the LTEX/GRAS census, with no bake: every
		 * floor in the terrain harnesses is a property of ONE plugin set, and
		 * a harness that cannot pin the corpus silently redefines all of them
		 * while still printing ok. */
		EsmWorld chWorld;
		QString cherr;
		if ( !chWorld.load( file, worldspace ? worldspace : 0x3CU, &cherr ) ) {
			err() << "error: " << cherr << Qt::endl;
			return 1;
		}
		int lands = 0, landsSorted = 0;
		const quint64 vh = chWorld.vhgtCorpusHash( &lands );
		const quint64 vs = chWorld.vhgtCorpusHashSorted( &landsSorted );
		const quint64 ph = chWorld.paintCorpusHash();
		const EsmCoverCensus & cc = chWorld.coverCensus();
		int minX = 0, minY = 0, maxX = 0, maxY = 0;
		chWorld.cellBounds( minX, minY, maxX, maxY );
		auto hex16 = []( quint64 v ) {
			return QStringLiteral( "0x" )
				+ QString::number( v, 16 ).toUpper().rightJustified( 16, QChar( '0' ) );
		};
		out() << QString( "corpus worldspace %1 cells %2 west %3 south %4 east %5 north %6" )
			.arg( chWorld.worldspaceEdid() ).arg( chWorld.cellCount() )
			.arg( minX ).arg( minY ).arg( maxX ).arg( maxY ) << Qt::endl;
		out() << QString( "corpus vhgtCorpusHash %1 lands %2" ).arg( hex16( vh ) ).arg( lands ) << Qt::endl;
		out() << QString( "corpus vhgtCorpusHashSorted %1 lands %2" ).arg( hex16( vs ) )
			.arg( landsSorted ) << Qt::endl;
		out() << QString( "corpus paintCorpusHash %1" ).arg( hex16( ph ) ) << Qt::endl;
		out() << QString( "corpus ltexTotal %1 grasTotal %2 gnamLinks %3 ltexWithGnam %4 "
			"grasDataMin %5 grasDataMax %6 grasWithoutData %7" )
			.arg( cc.ltexTotal ).arg( cc.grasTotal ).arg( cc.gnamLinks ).arg( cc.ltexWithGnam )
			.arg( cc.grasDataMin ).arg( cc.grasDataMax ).arg( cc.grasWithoutData ) << Qt::endl;
		out().flush();
		return 0;
	}
	LodgenVtOptions vtOpts = vtOptsIn;
	vtOpts.cover = coverOpts;
	if ( !vtDir.isEmpty() || vtEstimate ) {
		if ( vtOpts.finestDim != 1 && vtOpts.finestDim != 2 ) {
			err() << "error: --vt-finest must be 1 or 2" << Qt::endl;
			return 2;
		}
		if ( vtOpts.content < 128 || vtOpts.content > 512
			|| ( vtOpts.content & ( vtOpts.content - 1 ) ) ) {
			err() << "error: --vt-content must be a power of two between 128 and 512" << Qt::endl;
			return 2;
		}
		if ( vtOpts.border % 4 ) {
			err() << "error: --vt-border must be a multiple of 4; a BC block is 4x4 and a border"
					 " that" << Qt::endl;
			err() << "       splits one makes a tile depend on its neighbour's bake" << Qt::endl;
			return 2;
		}
		if ( vtOpts.mips < 1 || ( vtOpts.border >> ( vtOpts.mips - 1 ) ) % 4
			|| ( ( vtOpts.border >> ( vtOpts.mips - 1 ) ) << ( vtOpts.mips - 1 ) ) != vtOpts.border ) {
			err() << QString( "error: --vt-border %1 cannot carry %2 mips; the border halves at "
				"every mip and" ).arg( vtOpts.border ).arg( vtOpts.mips ) << Qt::endl;
			err() << QString( "       must stay a multiple of 4 (that needs at least %1)" )
				.arg( 4 << ( vtOpts.mips - 1 ) ) << Qt::endl;
			return 2;
		}
		if ( vtOpts.compression < 0 || vtOpts.compression > 1 ) {
			err() << "error: --vt-compress must be none or zlib" << Qt::endl;
			return 2;
		}
		if ( vtOpts.halfAux && vtOpts.mips < 2 ) {
			err() << "error: --vt-half-aux drops each aux sheet's top mip and keeps the rest, so "
					 "it needs --vt-mips 2 or more" << Qt::endl;
			return 2;
		}
		if ( !worldspace ) {
			err() << "error: --vt needs --worldspace; a tile pyramid is one worldspace's" << Qt::endl;
			return 2;
		}
		if ( vtBtr == 1 && texDir.isEmpty() ) {
			err() << "error: --vt-btr needs --tex-dir; there are no chunk sheets to take from "
					 "the pyramid" << Qt::endl;
			return 2;
		}
	}
	/* Converting a Fallout 76 .btd needs no plugin: the .btd is the whole
	 * landscape. This runs before the .esm branch and returns on its own. */
	if ( !btdPath.isEmpty() ) {
		if ( lodtDir.isEmpty() && !btdProbe ) {
			err() << "error: --from-btd needs --lodl <dir>" << Qt::endl;
			return 1;
		}
		/* Report what the source actually is BEFORE the long conversion, so a
		 * run that is going to take minutes says what it is working on rather
		 * than sitting silent. Opening a .btd is just a header parse. */
		try {
			BTDFile probe( btdPath.toLocal8Bit().constData() );
			out() << "btd: " << btdPath << Qt::endl;
			out() << "  cells " << ( probe.getCellMaxX() - probe.getCellMinX() + 1 )
				  << "x" << ( probe.getCellMaxY() - probe.getCellMinY() + 1 )
				  << " at (" << probe.getCellMinX() << "," << probe.getCellMinY() << ")"
				  << "  height " << probe.getMinHeight() << ".." << probe.getMaxHeight()
				  << "  LTEX " << probe.getLandTextureCount()
				  << "  GCVR " << probe.getGroundCoverCount() << Qt::endl;
			out().flush();
		} catch ( const std::exception & e ) {
			err() << "error: cannot open .btd: " << e.what() << Qt::endl;
			return 1;
		}

		/* --btd-probe: answer, from the .btd alone and in seconds, the two
		 * questions the converter had been ASSUMING the answer to.
		 *
		 * 1. Which alpha field pairs with which texture slot. libfo76utils
		 *    reverses both on read so they line up as field s <-> t[s+1], but
		 *    that is reading their conventions, not measuring the data. So:
		 *    under each candidate pairing, count nonzero alphas whose paired
		 *    slot is EMPTY. The true pairing scores ~0; a reversed one lights
		 *    up wherever a quadrant has fewer than five layers.
		 * 2. What bit 15 is. The A16 codec calls it a sixth layer's 1-bit
		 *    alpha; the converter copies it through verbatim either way, but
		 *    the spec should say what it is rather than not mention it. */
		if ( btdProbe ) {
			BTDFile src( btdPath.toLocal8Bit().constData() );
			src.setTileCacheSize( 8 );
			const int cw = src.getCellMaxX() - src.getCellMinX() + 1;
			const int stride = qMax( 1, cw / 20 );
			std::vector<quint16> cellA( size_t( 128 ) * 128 ), cellC( size_t( 32 ) * 32 );
			std::vector<unsigned char> cellG( size_t( 128 ) * 128 );
			unsigned char tset[64];
			qint64 nz = 0, badFwd = 0, badRev = 0, b15 = 0, b15NoBase = 0, n = 0;
			// ground cover: the same question, mask bit b <-> g[b] or g[7-b]?
			qint64 gnz = 0, gBadFwd = 0, gBadRev = 0;
			qint64 cn = 0, cAlpha = 0;
			double sr = 0, sg = 0, sb = 0;
			QSet<quint16> distinct;
			int cells = 0;
			for ( int cy = src.getCellMinY(); cy <= src.getCellMaxY(); cy += stride ) {
				for ( int cx = src.getCellMinX(); cx <= src.getCellMaxX(); cx += stride ) {
					src.getCellTextureSet( tset, cx, cy );
					src.getCellLandTexture( cellA.data(), cx, cy, 0 );
					src.getCellTerrainColor( cellC.data(), cx, cy, 2 );
					src.getCellGroundCover( cellG.data(), cx, cy, 0 );
					cells++;
					for ( int r = 0; r < 128; r++ ) {
						for ( int c = 0; c < 128; c++ ) {
							const int q = ( r >= 64 ? 2 : 0 ) + ( c >= 64 ? 1 : 0 );
							const unsigned char * t = tset + ( q << 4 );
							const quint16 w = cellA[size_t( r ) * 128 + size_t( c )];
							n++;
							for ( int s = 0; s < 5; s++ ) {
								const int a = ( w >> ( s * 3 ) ) & 7;
								if ( !a )
									continue;
								nz++;
								if ( t[s + 1] == 0xFF ) badFwd++;   // field s <-> t[s+1]
								if ( t[5 - s] == 0xFF ) badRev++;   // field s <-> t[5-s]
							}
							if ( w & 0x8000 ) {
								b15++;
								if ( t[0] == 0xFF )
									b15NoBase++;
							}
							const unsigned char * g = t + 8;
							const unsigned char gm = cellG[size_t( r ) * 128 + size_t( c )];
							for ( int bb = 0; bb < 8; bb++ ) {
								if ( !( gm & ( 1 << bb ) ) )
									continue;
								gnz++;
								if ( g[bb] == 0xFF ) gBadFwd++;       // bit b <-> g[b]
								if ( g[7 - bb] == 0xFF ) gBadRev++;   // bit b <-> g[7-b]
							}
						}
					}
					for ( quint16 v : cellC ) {
						cn++;
						sr += ( v >> 10 ) & 0x1F;
						sg += ( v >> 5 ) & 0x1F;
						sb += v & 0x1F;
						if ( v & 0x8000 ) cAlpha++;
						if ( distinct.size() < 65536 ) distinct.insert( v );
					}
				}
			}
			out() << "probe: " << cells << " cells, " << n << " samples, "
				  << nz << " nonzero alpha fields" << Qt::endl;
			out() << "  pairing field s <-> t[s+1]: " << badFwd
				  << " nonzero alphas on an EMPTY slot" << Qt::endl;
			out() << "  pairing field s <-> t[5-s]: " << badRev
				  << " nonzero alphas on an EMPTY slot" << Qt::endl;
			out() << "  ground cover: " << gnz << " set mask bits; bit b <-> g[b]: "
				  << gBadFwd << " on an EMPTY slot; bit b <-> g[7-b]: " << gBadRev
				  << " on an EMPTY slot" << Qt::endl;
			out() << "  bit 15 set on " << b15 << " samples ("
				  << QString::number( n ? 100.0 * double( b15 ) / double( n ) : 0.0, 'f', 2 )
				  << "%), of which " << b15NoBase << " have no base texture" << Qt::endl;
			out() << "  colour A1R5G5B5 over " << cn << " samples: mean R "
				  << QString::number( cn ? sr / double( cn ) : 0.0, 'f', 1 )
				  << " G " << QString::number( cn ? sg / double( cn ) : 0.0, 'f', 1 )
				  << " B " << QString::number( cn ? sb / double( cn ) : 0.0, 'f', 1 )
				  << " of 31, alpha bit on " << cAlpha << ", "
				  << distinct.size() << " distinct words" << Qt::endl;
			return 0;
		}

		LodtOptions lopts;
		QString written, berr, notes;
		/* Water for a .btd: a .btd has none, the plugin does. With a <file> and
		 * --worldspace on the command line the plugin is loaded for its water
		 * records only; the landscape still comes from the .btd. */
		std::unique_ptr<EsmWorld> waterWorld;
		if ( !file.isEmpty() && worldspace ) {
			waterWorld = std::make_unique<EsmWorld>();
			QString werr;
			if ( !waterWorld->load( file, worldspace, &werr ) ) {
				err() << "error: water plugin: " << werr << Qt::endl;
				return 1;
			}
			out() << "water from: " << file << "  worldspace " << waterWorld->worldspaceEdid()
				  << " (" << waterWorld->cellCount() << " cells indexed)" << Qt::endl;
		}
		/* --verify-only: an EXISTING file against its source, every check the
		 * write path runs and none of the writing. A 25-minute conversion is
		 * the wrong price for asking whether a file still matches its source,
		 * and a consumer (FO4CS) will want to ask exactly that. */
		if ( verifyOnly ) {
			// the landscape file moved to FO4CSLOD\<ws>\ (lane LAYOUT1, 2026-09-16)
			written = lodgenFo4csWorldDir( lodtDir, QFileInfo( btdPath ).completeBaseName() )
				+ QChar( '/' ) + QFileInfo( btdPath ).completeBaseName() + QStringLiteral( ".lodl" );
			if ( !QFileInfo::exists( written ) ) {
				err() << "error: --verify-only: no file at " << written << Qt::endl;
				return 1;
			}
			out() << "verify: " << written << Qt::endl;
		} else {
			if ( !lodtWriteBtd( btdPath, lodtDir, lopts, &written, &berr, &notes, waterWorld.get() ) ) {
				err() << "error: " << berr << Qt::endl;
				return 1;
			}
			out() << "lodl: " << written << Qt::endl;
			out() << "  " << berr << Qt::endl;
			out() << notes << Qt::endl;
		}

		LodtFile rf;
		QString rerr;
		if ( !rf.open( written, &rerr ) ) {
			err() << "readback FAILED: " << rerr << Qt::endl;
			return 1;
		}
		out() << "  readback: cells " << rf.cellsX() << "x" << rf.cellsY()
			  << "  rate " << rf.samplesPerCell()
			  << "  levels " << rf.levelCount()
			  << "  blocks " << rf.blockCount()
			  << "  LTEX " << rf.ltexCount()
			  << "  quantum " << rf.heightQuantum() << Qt::endl;

		/* Cross-check the reader against the .btd ITSELF, the same way the
		 * .esm path checks against LAND records. Heights cannot be exact:
		 * theirs are normalised across the whole world and ours are a
		 * quantum, and the two grids sit half a step apart -- so the bound is
		 * HALF a quantum, not one. One quantum was the first bound, and it
		 * let a truncate-instead-of-round bug through every run: a tolerance
		 * set from what the code produced is not a tolerance.
		 *
		 * Alpha words, by contrast, are copied verbatim, so they must match
		 * EXACTLY -- and this is the only check that reads plane 1 of a
		 * four-plane block, so it is also the addressing test for the fourth
		 * plane. */
		{
			BTDFile src( btdPath.toLocal8Bit().constData() );
			const float lo = src.getMinHeight(), hi = src.getMaxHeight();
			std::vector<quint16> cellH( size_t( 128 ) * 128 ), cellA( size_t( 128 ) * 128 );
			std::vector<quint16> cellC( size_t( 32 ) * 32 );
			std::vector<unsigned char> cellG( size_t( 128 ) * 128 );
			int bad = 0, tested = 0, aBad = 0, aNonZero = 0;
			int cBad = 0, gBad = 0, gNonZero = 0;
			double worst = 0.0;
			/* Half a quantum is the requantisation bound; the rest is what
			 * float32 can carry at the tallest height. The reader decodes in
			 * float32, so (stored - 32767) * quantum near 38,000 units is only
			 * good to ~0.004. Eight ulps of the tallest height is derived from
			 * the arithmetic, not from what a run produced; 0.5 * q * 1.001 was
			 * the latter kind of number and missed by 0.0002 on Appalachia. */
			const double tallest = qMax( std::fabs( double( lo ) ), std::fabs( double( hi ) ) );
			const double bound = 0.5 * double( rf.heightQuantum() )
				+ 8.0 * tallest * 5.96e-8;
			const int stride = qMax( 1, rf.cellsX() / 16 );
			for ( int cy = rf.cellMinY(); cy <= rf.cellMaxY(); cy += stride ) {
				for ( int cx = rf.cellMinX(); cx <= rf.cellMaxX(); cx += stride ) {
					src.getCellHeightMap( cellH.data(), cx, cy, 0 );
					src.getCellLandTexture( cellA.data(), cx, cy, 0 );
					src.getCellTerrainColor( cellC.data(), cx, cy, 2 );
					src.getCellGroundCover( cellG.data(), cx, cy, 0 );
					const int x0 = ( cx - rf.cellMinX() ) * rf.samplesPerCell();
					const int y0 = ( cy - rf.cellMinY() ) * rf.samplesPerCell();
					for ( int r = 0; r < 128; r += 16 ) {
						for ( int c = 0; c < 128; c += 16 ) {
							const size_t k = size_t( r ) * 128 + size_t( c );
							const double want = double( lo )
								+ ( double( cellH[k] ) / 65535.0 ) * ( double( hi ) - double( lo ) );
							const double got = rf.height( x0 + c, y0 + r );
							tested++;
							const double d = std::fabs( got - want );
							worst = qMax( worst, d );
							if ( d > bound )
								bad++;
							if ( cellA[k] )
								aNonZero++;
							if ( rf.alphaWord( x0 + c, y0 + r ) != cellA[k] )
								aBad++;
							// colour: A1R5G5B5 at 32x32, repacked to 5-5-5 and upsampled 4x
							const quint16 v = cellC[size_t( r >> 2 ) * 32 + size_t( c >> 2 )];
							const quint16 wantC = quint16( ( ( ( v >> 10 ) & 0x1F ) << 11 )
								| ( ( ( v >> 5 ) & 0x1F ) << 6 ) | ( v & 0x1F ) );
							if ( rf.colourWord( x0 + c, y0 + r ) != wantC )
								cBad++;
							if ( cellG[k] )
								gNonZero++;
							if ( rf.groundCover( x0 + c, y0 + r ) != quint16( cellG[k] ) )
								gBad++;
						}
					}
				}
			}
			out() << "  cross-check: " << tested << " samples against the .btd, "
				  << bad << " past half a quantum, worst " << worst
				  << " (bound " << bound << ")" << Qt::endl;
			out() << "  alpha words: " << aBad << " of " << tested
				  << " differ from the .btd (" << aNonZero << " nonzero)" << Qt::endl;
			out() << "  colour words: " << cBad << " of " << tested
				  << " differ; ground cover: " << gBad << " of " << tested
				  << " differ (" << gNonZero << " nonzero)" << Qt::endl;
			if ( bad || aBad || cBad || gBad )
				return 1;
		}
		return 0;
	}

	/* --dump-land <file>: every cell's FULL 33x33 VHGT grid, once, so a rule
	 * about shared edges can be tested offline in seconds instead of through
	 * a four-minute build per hypothesis. Layout: int32 minX, minY, cellsX,
	 * cellsY; then per cell (row-major from the south-west) one uint8
	 * presence flag; then per cell 33*33 int16 heights in units of 8, row 0
	 * south, column 0 west, absent cells zero-filled. */
	/* --dump-layers <file>: the painted area, from the MASTER, not from our own
	 * output. A cell counts as painted where a quadrant carries a BTXT base or
	 * any ATXT layer. Defining it from a regenerated bake instead would be
	 * circular - the bake is the thing being checked. */
	if ( !dumpLayers.isEmpty() ) {
		EsmWorld world;
		QString derr;
		if ( !world.load( file, worldspace, &derr ) ) {
			err() << "error: " << derr << Qt::endl;
			return 1;
		}
		int mnx, mny, mxx, mxy;
		world.cellBounds( mnx, mny, mxx, mxy );
		QFile f( dumpLayers );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
			err() << "error: cannot write " << dumpLayers << Qt::endl;
			return 1;
		}
		QTextStream ts( &f );
		ts << "# lodgen dump-layers 1 worldspace "
		   << QString( "%1" ).arg( worldspace, 8, 16, QChar( '0' ) )
		   << " cells (" << mnx << "," << mny << ")..(" << mxx << "," << mxy << ")\n";
		ts << "# L cx cy land vclr base=q0,q1,q2,q3 layers=n0,n1,n2,n3 ltex=<distinct> blend=<q0>|<q1>|<q2>|<q3> each ltex:meanOpacity;...\n";
		EsmLand lnd;
		int haveLand = 0, painted = 0, withBase = 0, withLayers = 0, withVclr = 0;
		QSet<quint32> allLtex;
		for ( int cy = mny; cy <= mxy; cy++ ) {
			for ( int cx = mnx; cx <= mxx; cx++ ) {
				if ( !world.land( cx, cy, lnd ) )
					continue;
				haveLand++;
				QSet<quint32> here;
				QStringList bases, counts;
				int nBase = 0, nLayer = 0;
				for ( int q = 0; q < 4; q++ ) {
					bases << QString( "%1" ).arg( lnd.baseTex[q], 8, 16, QChar( '0' ) );
					counts << QString::number( lnd.layers[q].size() );
					if ( lnd.baseTex[q] ) {
						nBase++;
						here.insert( lnd.baseTex[q] );
					}
					for ( const EsmLandLayer & l : lnd.layers[q] ) {
						nLayer++;
						if ( l.ltex )
							here.insert( l.ltex );
					}
				}
				if ( nBase ) withBase++;
				if ( nLayer ) withLayers++;
				if ( lnd.hasColors ) withVclr++;
				if ( nBase || nLayer ) painted++;
				allLtex.unite( here );
				/* The BLEND, per quadrant: every layer's LTEX with its MEAN opacity
				 * over the quadrant's 17x17 alpha grid, and the BTXT base at 1.0 in
				 * front of them. This is what the recovery learns against - an id
				 * alone does not say whether it covers the quadrant or one corner. */
				QStringList blend;
				for ( int q = 0; q < 4; q++ ) {
					QStringList parts;
					if ( lnd.baseTex[q] )
						parts << QString( "%1:1.000" ).arg( lnd.baseTex[q], 8, 16, QChar( '0' ) );
					for ( const EsmLandLayer & la : lnd.layers[q] ) {
						double s = 0.0;
						for ( int r = 0; r < 17; r++ )
							for ( int c2 = 0; c2 < 17; c2++ )
								s += double( la.opacity[r][c2] );
						parts << QString( "%1:%2" ).arg( la.ltex, 8, 16, QChar( '0' ) )
							.arg( s / ( 17.0 * 17.0 ), 0, 'f', 3 );
					}
					blend << ( parts.isEmpty() ? QStringLiteral( "-" ) : parts.join( QChar( ';' ) ) );
				}
				QStringList ids;
				QList<quint32> sorted = here.values();
				std::sort( sorted.begin(), sorted.end() );
				for ( quint32 id : sorted )
					ids << QString( "%1" ).arg( id, 8, 16, QChar( '0' ) );
				ts << "L " << cx << " " << cy << " 1 " << ( lnd.hasColors ? 1 : 0 )
				   << " base=" << bases.join( QChar( ',' ) )
				   << " layers=" << counts.join( QChar( ',' ) )
				   << " ltex=" << ( ids.isEmpty() ? QStringLiteral( "-" ) : ids.join( QChar( ',' ) ) )
				   << " blend=" << blend.join( QChar( '|' ) )
				   << "\n";
			}
		}
		ts << "# cells with LAND " << haveLand << ", painted " << painted
		   << " (base " << withBase << ", layers " << withLayers << "), vclr " << withVclr
		   << ", distinct LTEX " << allLtex.size() << "\n";
		ts.flush();
		f.close();
		out() << "dump-layers: " << dumpLayers << "  " << haveLand << " cells with LAND, "
			  << painted << " painted, " << allLtex.size() << " distinct LTEX" << Qt::endl;
		return 0;
	}

	if ( !dumpLand.isEmpty() ) {
		EsmWorld world;
		QString derr;
		if ( !world.load( file, worldspace, &derr ) ) {
			err() << "error: " << derr << Qt::endl;
			return 1;
		}
		int mnx, mny, mxx, mxy;
		world.cellBounds( mnx, mny, mxx, mxy );
		const int cw = mxx - mnx + 1, ch = mxy - mny + 1;
		QFile f( dumpLand );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			err() << "error: cannot write " << dumpLand << Qt::endl;
			return 1;
		}
		QByteArray hdr;
		QDataStream hs( &hdr, QIODevice::WriteOnly );
		hs.setByteOrder( QDataStream::LittleEndian );
		hs << qint32( mnx ) << qint32( mny ) << qint32( cw ) << qint32( ch );
		f.write( hdr );
		QByteArray present( cw * ch, char( 0 ) );
		QByteArray grid;
		grid.resize( qsizetype( cw ) * ch * 33 * 33 * 2 );
		grid.fill( 0 );
		EsmLand lnd;
		int have = 0;
		for ( int cy = mny; cy <= mxy; cy++ ) {
			for ( int cx = mnx; cx <= mxx; cx++ ) {
				const qsizetype ci = qsizetype( cy - mny ) * cw + ( cx - mnx );
				if ( !world.land( cx, cy, lnd ) )
					continue;
				present[ci] = 1;
				have++;
				for ( int r = 0; r < 33; r++ )
					for ( int c = 0; c < 33; c++ ) {
						const qint16 v = qint16( qRound( lnd.heights[r][c] / 8.0f ) );
						const qsizetype o = ( ci * 33 * 33 + r * 33 + c ) * 2;
						grid[o] = char( v & 0xFF );
						grid[o + 1] = char( ( v >> 8 ) & 0xFF );
					}
			}
		}
		f.write( present );
		f.write( grid );
		f.close();
		out() << "dump-land: " << dumpLand << "  cells " << cw << "x" << ch
			  << " from (" << mnx << "," << mny << "), " << have << " with LAND, "
			  << ( f.size() / 1048576 ) << " MB" << Qt::endl;
		return 0;
	}

	if ( !lodtDir.isEmpty() ) {
		EsmWorld world;
		QString berr;
		if ( !world.load( file, worldspace, &berr ) ) {
			err() << "error: " << berr << Qt::endl;
			return 1;
		}
		LodtOptions lopts;
		/* The water module's switches. They ride a file-scope struct rather
		 * than five more parameters on a function that already takes
		 * forty-five; what matters is that they are OFF unless the command line
		 * said otherwise, so a run that did not ask for bodies writes the bytes
		 * it always wrote. */
		lopts.water = gLodlWater;
		if ( lopts.water.enabled && lopts.water.velocityPlugin.isEmpty() )
			lopts.water.velocityPlugin = file;   // the WATR NAM0 fallback floor
		QString written;
		if ( refreshAo ) {
			// only the AO plane, in place; then the usual read-back and cross-check
			written = lodgenFo4csWorldDir( lodtDir, world.worldspaceEdid() )
				+ QChar( '/' ) + world.worldspaceEdid() + QStringLiteral( ".lodl" );
			QString aerr;
			if ( !lodtRefreshAo( written, &aerr ) ) {
				err() << "error: --refresh-ao: " << aerr << Qt::endl;
				return 1;
			}
			out() << "refresh-ao: " << written << Qt::endl;
			out() << "  " << aerr << Qt::endl;
		} else if ( verifyOnly ) {
			written = lodgenFo4csWorldDir( lodtDir, world.worldspaceEdid() )
				+ QChar( '/' ) + world.worldspaceEdid() + QStringLiteral( ".lodl" );
			if ( !QFileInfo::exists( written ) ) {
				err() << "error: --verify-only: no file at " << written << Qt::endl;
				return 1;
			}
			out() << "verify: " << written << Qt::endl;
		} else {
			/* THE LANDSCAPE STAGE. A `.lodl` run is where this one of the four
			 * moves; a region bake writes no landscape file and prints 0.0 for
			 * it, which is the other half of the written-and-moves pair. */
			QElapsedTimer landscapeTimer;
			landscapeTimer.start();
			const bool lodlOk = lodtWrite( world, lodtDir, lopts, &written, &berr );
			const qint64 msLandscape = landscapeTimer.elapsed();
			if ( !lodlOk ) {
				err() << "error: " << berr << Qt::endl;
				return 1;
			}
			out() << "lodl: " << written << Qt::endl;
			out() << "  " << berr << Qt::endl;   // the writer reports its census here
			censusOut( lodgenStageTimeLine( msLandscape, 0, 0, 0 ) );
			censusOut( lodgenBakeCensusLine() );
		}

		/* Read it straight back with the independent reader. A writer checked
		 * only by its own assumptions is not checked: this is the same class of
		 * defect as a DDS header whose pixel-format block sat four bytes wrong
		 * and parsed perfectly through the parser that produced it. */
		LodtFile rf;
		QString rerr;
		if ( !rf.open( written, &rerr ) ) {
			err() << "readback FAILED: " << rerr << Qt::endl;
			return 1;
		}
		out() << "  readback: cells " << rf.cellsX() << "x" << rf.cellsY()
			  << "  rate " << rf.samplesPerCell()
			  << "  levels " << rf.levelCount()
			  << "  blocks " << rf.blockCount()
			  << "  LTEX " << rf.ltexCount() << "  WATR " << rf.watrCount()
			  << "  quantum " << rf.heightQuantum() << Qt::endl;

		/* Cross-check the READER against the SOURCE, not against the writer's
		 * own idea of what it wrote. Every corner of a sampled grid of cells,
		 * which lands on all four pyramid levels: any mismatch at all is a bug,
		 * because the 8-unit quantum is VHGT's own and nothing rounds. */
		{
			int bad = 0, tested = 0;
			double worst = 0.0;
			// alphas and colour are what FO4CS will actually read; heights alone
			// proved the pyramid, not the planes
			int aBad = 0, aNonZero = 0, cBad = 0, cColoured = 0;
			int seamRaised = 0;   // samples the max rule lifted above the cell's own record
			EsmLand lnd, nS, nW, nSW;
			const int stride = qMax( 1, rf.cellsX() / 24 );
			for ( int cy = rf.cellMinY(); cy <= rf.cellMaxY(); cy += stride ) {
				for ( int cx = rf.cellMinX(); cx <= rf.cellMaxX(); cx += stride ) {
					if ( !world.land( cx, cy, lnd ) )
						continue;
					// the seam rule's other holders of this cell's row 0 / column 0
					const bool hS = world.land( cx, cy - 1, nS );
					const bool hW = world.land( cx - 1, cy, nW );
					const bool hSW = world.land( cx - 1, cy - 1, nSW );
					const int x0 = ( cx - rf.cellMinX() ) * rf.samplesPerCell();
					const int y0 = ( cy - rf.cellMinY() ) * rf.samplesPerCell();
					for ( int r = 0; r < 32; r += 4 ) {
						for ( int c = 0; c < 32; c += 4 ) {
							/* Rebuilt from the ESM the way the spec says the slot is
							 * filled: the MAXIMUM over every cell holding the sample. */
							double want = lnd.heights[r][c];
							if ( r == 0 && hS )
								want = qMax( want, double( nS.heights[32][c] ) );
							if ( c == 0 && hW )
								want = qMax( want, double( nW.heights[r][32] ) );
							if ( r == 0 && c == 0 && hSW )
								want = qMax( want, double( nSW.heights[32][32] ) );
							if ( want > double( lnd.heights[r][c] ) )
								seamRaised++;
							const double got = rf.height( x0 + c, y0 + r );
							tested++;
							const double d = std::fabs( got - want );
							worst = qMax( worst, d );
							if ( d > 0.001 )
								bad++;

							/* Rebuild the alpha word from the ESM the way a CONSUMER
							 * would: slots from the file's quadrant table, forms from
							 * its LTEX table, opacities from the LAND record. Nothing
							 * here comes from the writer's own idea of what it packed. */
							{
								const int q = ( r >= 16 ? 2 : 0 ) + ( c >= 16 ? 1 : 0 );
								const int qr = r >= 16 ? r - 16 : r;
								const int qc = c >= 16 ? c - 16 : c;
								quint16 qslots[6];   // not "slots": that is a Qt keyword macro and vanishes
								rf.quadrantSlots( cx, cy, q, qslots );
								quint16 wantA = 0;
								for ( int s = 0; s < 5; s++ ) {
									if ( qslots[s] == 0xFFFFU )
										continue;
									const quint32 form = rf.ltexForm( int( qslots[s] ) );
									float a = 0.0f;
									for ( const EsmLandLayer & ly : lnd.layers[q] )
										if ( ly.ltex == form ) {
											a = ly.opacity[qr][qc];
											break;
										}
									wantA |= quint16( quint16( qBound( 0.0f, a * 7.0f + 0.5f, 7.0f ) )
										<< ( s * 3 ) );
								}
								if ( wantA )
									aNonZero++;
								if ( rf.alphaWord( x0 + c, y0 + r ) != wantA )
									aBad++;

								const quint16 wantC = lnd.hasColors
									? quint16( ( ( lnd.colors[r][c][0] >> 3 ) << 11 )
										| ( ( lnd.colors[r][c][1] >> 3 ) << 6 )
										| ( lnd.colors[r][c][2] >> 3 ) )
									: quint16( 0xFFFFU );
								if ( lnd.hasColors )
									cColoured++;
								if ( rf.colourWord( x0 + c, y0 + r ) != wantC )
									cBad++;
							}
						}
					}
				}
			}
			out() << "  cross-check: " << tested << " samples against the ESM, "
				  << bad << " mismatched, worst " << worst
				  << " (" << seamRaised << " seam samples raised by the max rule)" << Qt::endl;
			out() << "  alpha words: " << aBad << " of " << tested << " differ ("
				  << aNonZero << " nonzero); colour words: " << cBad << " of " << tested
				  << " differ (" << cColoured << " coloured)" << Qt::endl;
			if ( bad || aBad || cBad ) {
				err() << "error: the file does not reproduce its source" << Qt::endl;
				return 1;
			}
		}
		return 0;
	}
	if ( !heightmapDir.isEmpty() ) {
		EsmWorld world;
		QString berr;
		if ( !world.load( file, worldspace, &berr ) ) {
			err() << "error: " << berr << Qt::endl;
			return 1;
		}
		QString written;
		QElapsedTimer landscapeTimer;
		landscapeTimer.start();
		const bool hmOk = lodgenBakeHeightmap( world, heightmapDir, heightmapSize, &written, &berr );
		const qint64 msLandscape = landscapeTimer.elapsed();
		if ( !hmOk ) {
			err() << "error: " << berr << Qt::endl;
			return 1;
		}
		int mnx, mny, mxx, mxy;
		world.cellBounds( mnx, mny, mxx, mxy );
		out() << "height map: " << written << Qt::endl;
		out() << "  " << berr << Qt::endl;   // the baker reports its provenance here
		// the shadow heightmap is the landscape stage too
		censusOut( lodgenStageTimeLine( msLandscape, 0, 0, 0 ) );
		censusOut( lodgenBakeCensusLine() );
		/* The loader pins the Commonwealth's corpus hash as a constant and
		 * refuses any other value. Ours is computed from the ESM by the same
		 * walk; if the two ever disagree this run fails loudly here rather than
		 * the map failing silently in game. */
		if ( world.worldspaceEdid() == QLatin1String( "Commonwealth" ) ) {
			const quint64 got = world.vhgtCorpusHash();
			if ( got != EsmWorld::kCommonwealthVhgtCorpusHash ) {
				err() << "error: Commonwealth corpus hash " << QString::number( got, 16 )
					  << " does not match the loader's pinned "
					  << QString::number( EsmWorld::kCommonwealthVhgtCorpusHash, 16 )
					  << " -- the map would be refused" << Qt::endl;
				return 1;
			}
			out() << "  corpus hash matches the loader's pinned Commonwealth constant" << Qt::endl;
		}
		out() << "  worldspace " << world.worldspaceEdid()
			  << "  cells S " << mny << " W " << mnx << " N " << mxy << " E " << mxx
			  << "  (" << ( mxx - mnx + 1 ) << "x" << ( mxy - mny + 1 ) << " cells)"
			  << "  " << ( heightmapSize > 0
					? QStringLiteral( "%1^2" ).arg( heightmapSize )
					: QStringLiteral( "%1x%2 native" ).arg( ( mxx - mnx + 1 ) * 32 ).arg( ( mxy - mny + 1 ) * 32 ) )
			  << " R16_UNORM" << Qt::endl;
		return 0;
	}
	if ( listCandidates && haveRegion ) {
		EsmWorld world;
		QString error;
		if ( !world.load( file, worldspace ? worldspace : 0x3CU, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		/* Bases referenced in the region, one `formid model` line each. The
		 * model is the base's own NEAR mesh when the record names one - the
		 * bake photographs the base, not a LOD derivative (bungo: "the base
		 * is more detailed") - else the first filled LOD slot. Which bases:
		 *   missing  far MNAM slots empty: these fall back to heavy near
		 *            meshes at dim16/32 without a card (the default)
		 *   trees    every tree, and ONLY trees. It meant `tree || missing`
		 *            until 2026-09-09, which is a SUPERSET of the default and
		 *            not a tree list at all: 14 of a 33-candidate Sanctuary
		 *            run were shacks and rock cliffs
		 *   all      every base with LOD
		 * SCOL parts are walked to their bases: most of Sanctuary's trees are
		 * parts, and a card library that skipped them would miss the forest. */
		QSet<quint32> seen;
		auto consider = [&]( quint32 baseId ) {
			if ( !baseId || seen.contains( baseId ) )
				return;
			seen.insert( baseId );
			const EsmLodBase & b = world.lodBase( baseId );
			if ( !b.hasLod )
				return;
			const bool missing = b.models[2].isEmpty() || b.models[3].isEmpty();
			QString source = b.model;
			for ( int l = 0; l < 4 && source.isEmpty(); l++ )
				source = b.models[l];
			if ( source.isEmpty() )
				return;
			const bool tree = std::memcmp( &b.type, "TREE", 4 ) == 0 || lodgenIsTreeModel( source );
			bool want = missing;
			if ( candidateKind == QLatin1String( "all" ) )
				want = true;
			else if ( candidateKind == QLatin1String( "trees" ) )
				want = tree;
			if ( want ) {
				/* `formid extent model`, the extent SECOND so the model - the only
				 * token that can hold a space - stays the line's remainder for a
				 * `read -r id extent model`. The extent is the larger of the
				 * model's horizontal radius and its half-height, in world units:
				 * the scalar the card baker's size ladder measures a base against.
				 * Zero when the mesh will not load, which the driver reads as
				 * "no opinion" rather than "infinitely small". */
				float ew = 0.0f, eh = 0.0f;
				lodgenModelExtent( dataRoot.isEmpty()
					? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot,
					source, &ew, &eh );
				out() << QString( "%1" ).arg( baseId, 8, 16, QChar( '0' ) ) << " "
					  << QString::number( qMax( ew, eh ), 'f', 1 ) << " " << source << Qt::endl;
			}
		};
		for ( int cy = region[1]; cy <= region[3]; cy++ ) {
			for ( int cx = region[0]; cx <= region[2]; cx++ ) {
				for ( const EsmRefr & r : world.refrs( cx, cy ) ) {
					if ( r.initiallyDisabled || r.deleted || !r.base )
						continue;
					if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
						for ( const EsmScolPart & part : world.scolParts( r.base ) )
							consider( part.base );
					} else {
						consider( r.base );
					}
				}
			}
		}
		return 0;
	}
	if ( haveObjects ) {
		EsmWorld world;
		QString error;
		if ( !world.load( file, worldspace ? worldspace : 0x3CU, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		LodgenObjectOptions opts;
		opts.dim = dim > 0 ? dim : 4;
		opts.identity = identity;
		opts.bakeAO = bakeAO;
		opts.cullBuried = cullBuried;
		opts.cullMargin = cullMargin;
		opts.aoGrey = aoGrey;
		opts.aoSkirtCells = aoSkirt;
		opts.impostorDir = impostors;
		opts.impostorFromLevel = impostorFromLevel;
		opts.cardAuxDiv = cardAuxDiv;
		opts.treesOnly = treesOnly;
		opts.slotFallback = slotFallback;
		opts.dataRoot = dataRoot.isEmpty()
			? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
		NifModel nif;
		QString manifest;
		if ( !lodgenBuildObjectChunk( &nif, world, chunkX, chunkY, opts, &manifest, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << "object chunk (" << chunkX << "," << chunkY << ") dim " << opts.dim
			  << ": " << nif.getBlockCount() << " blocks — " << error << Qt::endl;
		/* THE MANIFEST IS A SIDECAR, not chunk data (lane DEFAULTS1,
		 * 2026-09-12): it is written whatever the identity flag says, because
		 * the arrays, the cards and the far-ring cut all read it back and the
		 * 15:56 ruling is about what goes INSIDE the .BTO. */
		if ( !outFile.isEmpty() ) {
			QFile mf( outFile + QStringLiteral( ".manifest.txt" ) );
			if ( mf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				mf.write( manifest.toUtf8() );
		}
		return saveNif( nif, outFile ) ? 0 : 1;
	}
	if ( ( !vtDir.isEmpty() || vtEstimate ) && !haveRegion ) {
		EsmWorld vtWorld;
		QString verr;
		if ( !vtWorld.load( file, worldspace, &verr ) ) {
			err() << "error: " << verr << Qt::endl;
			return 1;
		}
		if ( !cmdLodgenVtEstimate( vtWorld, vtOpts, !texDir.isEmpty() ) )
			return 1;
		if ( vtEstimate )
			return 0;
		LodgenVtOptions vo = vtOpts;
		if ( vtBtr != 0 && !texDir.isEmpty() ) {
			QDir().mkpath( texDir );
			vo.btrTexDir = texDir;
			vo.btrDims = QVector<int>{ 4, 8, 16, 32 };
		}
		QString vtReport;
		if ( !lodgenBakeTerrainVt( vtWorld,
			dataRoot.isEmpty() ? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot,
			vtDir, vo, nullptr, &vtReport, &verr ) ) {
			err() << "error: " << verr << Qt::endl;
			return 1;
		}
		censusOut( vtReport );
		return 0;
	}
	if ( haveRegion ) {
		if ( outDir.isEmpty() ) {
			err() << "error: --terrain-region needs --out-dir" << Qt::endl;
			return 2;
		}
		EsmWorld world;
		QString error;
		if ( !world.load( file, worldspace ? worldspace : 0x3CU, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		LodgenTerrainOptions opts;
		opts.dim = dim > 0 ? dim : 4;
		opts.geomorph = geomorph;
		opts.terrainIdentity = terrainIdentity;
		opts.waterSubdiv = waterSubdiv;
		opts.shoreDenser = shoreDenser;
		opts.shoreDensity = shoreDensity;
		if ( targetTris >= 0 )
			opts.targetTrisPerCell = targetTris;
		const int d = opts.dim;
		// snap the requested cell region outward to chunk alignment
		auto floorTo = []( int v, int m ) { return v >= 0 ? v - v % m : -( ( -v + m - 1 ) / m ) * m; };
		const int x0 = floorTo( region[0], d ), y0 = floorTo( region[1], d );
		QDir().mkpath( outDir );
		/* THE FOUR STAGE TIMES, the command line's half of bungo's ask. The
		 * panel keeps the same four and prints them with the same words
		 * (lodgenStageTimeLine). A region bake writes no `.lodl`, so its
		 * landscape stage is 0 by construction -- the `--lodt-dir` run is where
		 * that one moves, and it prints the same line. */
		qint64 msLandscape = 0, msMeshes = 0, msTextures = 0, msImpostors = 0;
		struct StageTimer
		{
			qint64 * acc;
			QElapsedTimer t;
			explicit StageTimer( qint64 * a ) : acc( a ) { t.start(); }
			~StageTimer() { *acc += t.elapsed(); }
		};
		const QString nativeDataRoot = dataRoot.isEmpty()
			? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
		if ( !nativeDir.isEmpty() ) {
			// the v4/v5 knobs are sticky and read by Begin; set them first
			lodgenNativeLadderOptions( libraryNear, ladderFoliage, silhouetteMin, placementAo );
			lodgenNativeVertexAoOption( vertexAo );
			lodgenNativeLodiV7Option( lodiV7 );
			lodgenNativeScrappableOption( scrappable );
			lodgenNativeIdentityJoinOption( identityJoinLegacy, identityJoinGap );
			/* `--native` NAMES A MOD FOLDER from today (lane LAYOUT1,
			 * 2026-09-16), exactly as `--vt` and `--lodl` already did: the pair
			 * lands at `<MODFOLDER>/FO4CSLOD/<ws>/`, not in the directory
			 * itself. Every harness that opened `<dir>/<ws>.lodo` was re-based
			 * in the same lane. */
			lodgenNativeBegin( &world, lodgenFo4csWorldDir( nativeDir, world.worldspaceEdid() ),
				lodgenNativeLoadModel, const_cast<QString *>( &nativeDataRoot ),
				nativeMeshReport, nativeLadder, nativeOccluders );
		}
		/* AGGREGATE RING-3 IMPOSTORS: armed only when asked for, and only when
		 * there is a card library to composite from. Without one the module
		 * REFUSES IN WORDS rather than writing an empty table -- an aggregate is
		 * made of the cards it replaces and cannot be invented. */
		if ( aggregate && !nativeDir.isEmpty() ) {
			if ( impostors.isEmpty() ) {
				err() << "error: --aggregate needs --impostors <card bake tree>: an aggregate sheet is "
					"composited from the cell's own trees' card sheets, so there is nothing to "
					"photograph without them" << Qt::endl;
				return 2;
			}
			QStringList aggNotes;
			const QHash<quint32, LodgenAggCard> aggCards =
				lodgenAggregateCards( world, region, impostors, cardAuxDiv, &aggNotes );
			for ( const QString & n : aggNotes )
				out() << n << Qt::endl;
			if ( aggCards.isEmpty() ) {
				err() << "error: --aggregate found no usable card set for any tree base of this region in "
					<< impostors << " (see the line above for what was refused)" << Qt::endl;
				return 2;
			}
			LodgenAggOptions ao;
			ao.minTrees = aggMin;
			ao.tile = aggTile;
			ao.views = aggViews;
			ao.auxDiv = cardAuxDiv;
			lodgenNativeSetAggregate( ao, aggCards );
		}
		/* ONE texture/LTEX/GRAS cache for the whole region: the per-chunk path
		 * used to own a local one and decode every landscape diffuse again for
		 * each chunk. Bounded by an LRU, so a whole worldspace does not hold
		 * sixty 21 MiB decodes at once. */
		LodgenBakeCaches * bakeCaches = lodgenCreateBakeCaches();
		struct RegionCacheGuard
		{
			LodgenBakeCaches * p;
			~RegionCacheGuard() { lodgenDestroyBakeCaches( p ); }
		} regionCacheGuard{ bakeCaches };
		/* The pyramid runs FIRST and ONCE. It has to: when the chunk sheets
		 * come from it, they are assembled while its staging is live, and a
		 * pass that ran after the chunk queue would have nothing to read. */
		bool texFromVt = false;
		if ( !vtDir.isEmpty() || vtEstimate ) {
			if ( !cmdLodgenVtEstimate( world, vtOpts, !texDir.isEmpty() ) )
				return 1;
		}
		if ( !vtDir.isEmpty() ) {
			LodgenVtOptions vo = vtOpts;
			vo.haveRegion = true;
			for ( int r = 0; r < 4; r++ )
				vo.region[r] = region[r];
			if ( vtBtr != 0 && !texDir.isEmpty() ) {
				QDir().mkpath( texDir );
				vo.btrTexDir = texDir;
				vo.btrDims = QVector<int>{ 4, 8, 16, 32 };
				texFromVt = true;
			}
			QString vtReport, vterr;
			bool vtOk = false;
			{
				StageTimer st( &msTextures );		// the pyramid is a TEXTURE stage
				vtOk = lodgenBakeTerrainVt( world,
					dataRoot.isEmpty() ? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot,
					vtDir, vo, bakeCaches, &vtReport, &vterr );
			}
			if ( !vtOk ) {
				err() << "error: " << vterr << Qt::endl;
				return 1;
			}
			censusOut( vtReport );
			out().flush();
		}
		int done = 0, skipped = 0, failed = 0;
		QStringList writtenBto;
		/* THE CHUNK QUEUE. The doubly-nested loop that used to stand here is
		 * now lodgenRunChunkPass (lodgenchunkpass.h), shared with the panel and
		 * fanned over lodgenThreadCount() workers. Every line below is printed
		 * from `retire`, which the pass calls on THIS thread in JOB ORDER --
		 * so `writtenBto`, the `[n] <name>` lines and the native accumulator
		 * see the same sequence a one-thread run produces. `--threads 1` is
		 * the exact way back. */
		QVector<LodgenChunkJob> jobs;
		for ( int cy = y0; cy <= region[3]; cy += d )
			for ( int cx = x0; cx <= region[2]; cx += d )
				jobs.append( LodgenChunkJob{ d, cx, cy } );

		/* ===== INCREMENTAL REGENERATION AND THE BAKE RECORD ===============
		 *
		 * Lane INCR1 (2026-09-12) put the filter HERE, on the job list, and
		 * nowhere else: everything downstream -- the retire callback,
		 * `writtenBto`, the `[n]` lines, the native accumulator -- consumes
		 * the pass IN JOB ORDER, so a filtered list is still in job order and
		 * the bytes of the chunks that DO run cannot depend on which of their
		 * neighbours ran beside them.
		 *
		 * Lane INCRGATE1 (2026-09-24) moved the ledger itself -- the diff, the
		 * refusals, the `.lodj` cache hooks and the record -- into
		 * `src/lodgenchunkpass.cpp`, so the LOD Generation panel runs the same
		 * code. The words printed here did not change.
		 *
		 * The pass is built FIRST because the IDENTITY WORD is read off it: the
		 * record's `switches` is now the argv digest AND every effective
		 * setting, so a default that moved between two builds refuses an
		 * incremental run instead of keeping yesterday's chunks. */
		LodgenChunkPassOptions pass;
		pass.plugins = file;
		pass.worldspace = worldspace ? worldspace : 0x3CU;
		pass.worldEdid = world.worldspaceEdid();
		pass.wantBtr = true;
		pass.wantBto = true;
		pass.wantTex = !texDir.isEmpty() && !texFromVt;
		pass.terrain = opts;
		pass.cover = coverOpts;
		pass.texDataRoot = dataRoot.isEmpty()
			? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
		pass.meshDir = outDir;
		pass.texDir = texDir;
		{
			LodgenObjectOptions oopts;
			oopts.dim = d;
			oopts.identity = identity;
			oopts.bakeAO = bakeAO;
			oopts.cullBuried = cullBuried;
			oopts.cullMargin = cullMargin;
			oopts.aoGrey = aoGrey;
			oopts.aoSkirtCells = aoSkirt;
			oopts.impostorDir = impostors;
			oopts.impostorFromLevel = impostorFromLevel;
			oopts.cardAuxDiv = cardAuxDiv;
			oopts.treesOnly = treesOnly;
			oopts.slotFallback = slotFallback;
			oopts.dataRoot = dataRoot.isEmpty()
				? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
			pass.object = oopts;
		}
		LodgenIdentityExtras idx;
		idx.atlas = atlas;
		idx.arrays = arrays;
		idx.merge = merge;
		idx.atlasBc1 = atlasBc1;
		idx.keepBto = gLgKeepBto;
		idx.texFromVt = texFromVt;
		idx.simplify = simplify;
		idx.vt = vtOpts;
		idx.vtBtr = vtBtr;
		idx.nativeLadder = nativeLadder;
		idx.nativeOccluders = nativeOccluders;
		idx.libraryNear = libraryNear;
		idx.ladderFoliage = ladderFoliage;
		idx.silhouetteMin = silhouetteMin;
		idx.placementAo = placementAo;
		idx.vertexAo = vertexAo;
		idx.lodiV7 = lodiV7;
		idx.scrappable = scrappable;
		idx.identityJoinLegacy = identityJoinLegacy;
		idx.identityJoinGap = identityJoinGap;
		idx.aggregate = aggregate;
		idx.aggMin = aggMin;
		idx.aggTile = aggTile;
		idx.aggViews = aggViews;
		const QStringList idDump = lodgenIdentityDump( pass, idx );
		const QString idWord = lodgenIdentityWord( idDump );
		out() << "identity: " << idWord << ", " << idDump.size() << " setting(s)" << Qt::endl;
		{
			// WW_LODGEN_IDENTITY_DUMP=<file>: the lines the word is hashed from, for a gate to diff
			const QString dumpTo = qEnvironmentVariable( "WW_LODGEN_IDENTITY_DUMP" );
			QFile df( dumpTo );
			if ( !dumpTo.isEmpty() && df.open( QIODevice::WriteOnly ) )
				df.write( ( idDump.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
		}

		LodgenIncrementalRun inc;
		inc.fromDir = gLgIncremental;
		inc.requireRecord = true;
		inc.outDir = outDir;
		inc.nativeDir = nativeDir;
		inc.digestRoot = pass.texDataRoot;
		inc.worldspace = pass.worldspace;
		inc.dim = d;
		for ( int k = 0; k < 4; k++ )
			inc.region[k] = region[k];
		inc.switches = lodgenSwitchesWithIdentity( gLgSwitchDigest, idWord );
		inc.regionProducts = atlas || arrays || !impostors.isEmpty();
		inc.nativeCache = gLgNativeCache;
		inc.warn = []( const QString & w ) { err() << w << Qt::endl; };
		{
			QString incCensus, incDetail;
			QStringList incReasons;
			const LodgenIncrementalVerdict v =
				lodgenIncrementalBegin( inc, world, jobs, &incCensus, &incReasons, &incDetail );
			if ( v != LodgenIncrementalVerdict::Go ) {
				for ( const QString & l : lodgenIncrementalRefusal( v, inc, incDetail ) )
					err() << l << Qt::endl;
				return 1;
			}
			if ( !incCensus.isEmpty() ) {
				censusOut( incCensus );
				for ( const QString & r : incReasons )
					out() << r << Qt::endl;
				out().flush();
			}
		}

		/* ===== THE .BTO SCRATCH FOLDER (lane BTOFREE1, 2026-09-16) =========
		 *
		 * bungo, 2026-09-12 18:3x: "essentially, no legacy vanilla file types
		 * are now used by us or baked in the FO4CS lod bake". The `.BTO` was
		 * the last one left, and it was left because four post-passes read it
		 * back -- not because anything downstream of the bake wants it.
		 *
		 * So under the FO4CS target it is built HERE instead, every read-back
		 * works on it here, and the teardown below removes it. The directory
		 * sits inside the output folder rather than in %TEMP% so that an
		 * interrupted bake leaves its scaffolding where the operator can see
		 * it; a run that finds one from a dead bake removes it first, which
		 * makes that self-healing rather than a second failure. */
		QString btoScratch;
		lodgenClearBtoDisposition();
		if ( !nativeDir.isEmpty() && !gLgKeepBto ) {
			btoScratch = QDir( outDir ).absolutePath() + QStringLiteral( "/lodgen_bto_scratch" );
			QDir( btoScratch ).removeRecursively();
			if ( !QDir().mkpath( btoScratch ) ) {
				err() << "error: cannot create the .BTO scratch folder " << btoScratch
					  << " -- pass --keep-bto to write the chunks into the output folder "
						 "instead" << Qt::endl;
				return 1;
			}
		}

		pass.btoScratchDir = btoScratch;

		/* THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17): written on every
		 * `--native` bake unless `--no-native-cache`, replayed for the chunks an
		 * incremental run skips. The hooks live in lodgenchunkpass.cpp. */
		lodgenIncrementalArmCache( inc, world.worldspaceEdid(), pass );

		{
			QString passErr;
			const bool passOk = lodgenRunChunkPass( jobs, pass,
				[&]( const LodgenChunkOutcome & r ) {
					lodgenIncrementalNoteRetired( inc, pass, r );
					if ( pass.wantBtr ) {
						if ( !r.btrBuilt ) {
							if ( r.btrNoLand )
								skipped++;
							else {
								err() << "chunk (" << r.cx << "," << r.cy << "): "
									  << r.btrError << Qt::endl;
								failed++;
							}
						} else if ( !r.btrSaved ) {
							err() << "chunk (" << r.cx << "," << r.cy << "): save failed" << Qt::endl;
							failed++;
						} else {
							done++;
							out() << "[" << done << "] "
								  << QFileInfo( r.btrPath ).fileName() << Qt::endl;
							out().flush();
							if ( !r.texError.isEmpty() )
								err() << "texture bake (" << r.cx << "," << r.cy << "): "
									  << r.texError << Qt::endl;
						}
					}
					if ( r.btoBuilt ) {
						if ( r.btoSaved ) {
							done++;
							writtenBto.append( r.btoPath );
							out() << "[" << done << "] "
								  << QFileInfo( r.btoPath ).fileName() << Qt::endl;
							out().flush();
						} else {
							err() << "objects (" << r.cx << "," << r.cy << "): save failed" << Qt::endl;
							failed++;
						}
					}
				},
				std::function<bool()>(), &msMeshes, &msTextures, &passErr );
			if ( !passOk ) {
				err() << "error: " << passErr << Qt::endl;
				return 1;
			}
			out() << "chunk pass: " << lodgenLastPassJobs() << " job(s) over "
				  << lodgenLastPassWorkers() << " worker(s)" << Qt::endl;
			out().flush();
		}

		/* WHERE THE OBJECT SHEETS GO, once, for the arrays, the atlas and the
		 * card arrays (lane LAYOUT1, 2026-09-16). There is no `--target` flag
		 * on the command line: `--native` IS the FO4CS target. Under it every
		 * sheet we write goes under the one root with the rest of our types,
		 * and the game-relative string baked into the chunks says the same; the
		 * stock target keeps the engine path it always had, byte for byte. */
		const bool fo4csTarget = !nativeDir.isEmpty();
		auto objectsDir = [&]() {
			return fo4csTarget
				? lodgenFo4csWorldDir( outDir, world.worldspaceEdid() ) + QStringLiteral( "/Objects" )
				: ( texDir.isEmpty() ? outDir : texDir ) + QStringLiteral( "/Objects" );
		};
		auto objectsGame = [&]( const QString & stem ) {
			const QString ws = world.worldspaceEdid();
			return fo4csTarget
				? QStringLiteral( "data\\" ) + lodgenFo4csGameWorldPath( ws )
					+ QChar( 92 ) + QStringLiteral( "Objects" ) + QChar( 92 ) + ws + QChar( '.' ) + stem
				: QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.%2" ).arg( ws ).arg( stem );
		};
		if ( arrays && !writtenBto.isEmpty() ) {
			// before the atlas: the arrays key on the shapes' own diffuse paths
			const QString ws = world.worldspaceEdid();
			const QString arrDir = objectsDir();
			QDir().mkpath( arrDir );
			QString rep, aerr;
			StageTimer st( &msTextures );
			if ( !lodgenBuildTextureArrays( writtenBto,
				dataRoot.isEmpty() ? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot,
				arrDir + "/" + ws + QStringLiteral( ".LodgenArrays" ),
				objectsGame( QStringLiteral( "LodgenArrays" ) ),
				&rep, &aerr ) ) {
				err() << "arrays: " << aerr << Qt::endl;
				failed++;
			} else {
				censusOut( QStringLiteral( "arrays written: " ) + rep );
				if ( fo4csTarget )
					lodgenNoteLayoutDir( arrDir );
			}
		}
		if ( atlas && !writtenBto.isEmpty() ) {
			const QString ws = world.worldspaceEdid();
			/* `/Objects`, like the arrays above and like the panel: the game
			 * path baked into every atlased shape names that subdirectory,
			 * so writing the sheets to <texDir> itself put them one level
			 * above where the chunks look for them. */
			const QString atlasDir = objectsDir();
			QDir().mkpath( atlasDir );
			/* Loose copies of textures the atlas cannot absorb go into the
			 * output DATA tree: derived when outDir follows the vanilla
			 * meshes/terrain/<ws> layout, else beside the chunks. */
			QString looseRoot = outDir;
			{
				const QString norm = QDir( outDir ).absolutePath();
				const QString suffix = QString( "/meshes/terrain/%1" ).arg( ws );
				if ( norm.endsWith( suffix, Qt::CaseInsensitive ) )
					looseRoot = norm.left( norm.size() - suffix.size() );
			}
			QString aerr;
			StageTimer st( &msTextures );
			/* NOT vanilla's "<ws>.Objects" name: a loose file at that path
			 * SHADOWS the archived vanilla sheet, and every vanilla BTO
			 * still in play (unregenerated chunks, the legacy fallback set)
			 * would sample OUR cell layout with THEIR UVs — bungo hit
			 * exactly that. Vanilla's name is only safe when the ENTIRE
			 * worldspace's BTOs are regenerated together. */
			if ( !lodgenBuildAtlas( writtenBto,
				dataRoot.isEmpty()
					? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" )
					: dataRoot,
				atlasDir + "/" + ws + QStringLiteral( ".LodgenObjects" ),
				objectsGame( QStringLiteral( "LodgenObjects" ) ),
				looseRoot, atlasBc1, &aerr ) ) {
				err() << "atlas: " << aerr << Qt::endl;
				failed++;
			} else {
				out() << "atlas written: " << ws << ".LodgenObjects.DDS (+_n, _s)" << Qt::endl;
				if ( fo4csTarget )
					lodgenNoteLayoutDir( atlasDir );
			}
		}
		if ( merge && !writtenBto.isEmpty() ) {
			// last: one shape per material the engine can tell apart (after the atlas and the arrays)
			QString rep, merr;
			StageTimer st( &msMeshes );
			if ( lodgenMergeChunkShapes( writtenBto, &rep, &merr ) )
				censusOut( QStringLiteral( "merged: " ) + rep );
			else
				err() << "merge: " << merr << Qt::endl;
		}
		if ( simplify.enabled && !writtenBto.isEmpty() ) {
			/* After the merge, because the proxy is the MERGED shape: the merge
			 * has already made one shape per material, which is the cluster a
			 * far ring wants one simplified mesh of. Ring 0 is never cut. */
			QString rep, serr;
			StageTimer st( &msMeshes );
			if ( lodgenSimplifyFarRings( writtenBto, simplify, &rep, &serr ) )
				censusOut( QStringLiteral( "far rings: " ) + rep );
			else
				err() << "far rings: " << serr << Qt::endl;
		}
		if ( arrays && !impostors.isEmpty() && !writtenBto.isEmpty() ) {
			// the card sets the chunks stand on, as arrays beside the mesh arrays
			const QString ws = world.worldspaceEdid();
			const QString arrDir = objectsDir();
			QDir().mkpath( arrDir );
			QString rep, cerr2;
			StageTimer st( &msImpostors );		// the IMPOSTOR stage
			if ( lodgenBuildCardArrays( writtenBto, impostors,
				arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ),
				objectsGame( QStringLiteral( "LodgenCards" ) ),
				cardAuxDiv, &rep, &cerr2 ) ) {
				censusOut( QStringLiteral( "card arrays written: " ) + rep );
				if ( fo4csTarget )
					lodgenNoteLayoutDir( arrDir );
			} else {
				err() << "card arrays: " << cerr2 << Qt::endl;
			}
		}
		if ( lodgenNativeActive() ) {
			{
				const QString cc = lodgenIncrementalCacheCensus( inc );
				if ( !cc.isEmpty() ) {
					censusOut( cc );
					out().flush();
				}
				const QStringList refused = lodgenIncrementalCacheRefusal( inc );
				if ( !refused.isEmpty() ) {
					for ( const QString & l : refused )
						err() << l << Qt::endl;
					lodgenNativeEnd();
					return 1;
				}
				lodgenIncrementalOfferReuse( inc );
			}
			/* THE CARD LINK (lane CARDLINK1, 2026-09-24). The card-arrays pass
			 * above has appended `<array .lodm> <layer>` to every C line it
			 * placed; the emitter reads them so the `.lodo` carries cardLayer,
			 * cardCount and cardCorpusHash and the `.lodi` FORCE_CARD. That is
			 * the one reason this whole native block now runs AFTER the object
			 * passes rather than straight after the chunk pass. */
			if ( arrays && !impostors.isEmpty() && !writtenBto.isEmpty() ) {
				QString lerr;
				if ( !lodgenNativeLinkCards( writtenBto,
					objectsDir() + "/" + world.worldspaceEdid() + QStringLiteral( ".LodgenCards" ), &lerr ) ) {
					err() << "error: " << lerr << Qt::endl;
					lodgenNativeEnd();
					return 1;
				}
			}
			QString nrep, nerr;
			bool nativeOk = false;
			{
				StageTimer st( &msMeshes );
				nativeOk = lodgenNativeWrite( &nrep, &nerr );
			}
			if ( !nativeOk ) {
				err() << "error: " << nerr << Qt::endl;
				lodgenNativeEnd();
				return 1;
			}
			censusOut( nrep );
			/* The aggregate SHEETS, written after the pair because the compositor
			 * runs inside the .lodi write and the DDS writer lives in lodgen.cpp.
			 * They go into the output DATA tree, never the card bake tree: a set is
			 * per WORLDSPACE CELL and the card tree is per base. */
			if ( !lodgenNativeAggregateSets().isEmpty() ) {
				QString aggRoot = outDir;
				{
					const QString norm = QDir( outDir ).absolutePath();
					const QString suffix = QString( "/meshes/terrain/%1" ).arg( world.worldspaceEdid() );
					if ( norm.endsWith( suffix, Qt::CaseInsensitive ) )
						aggRoot = norm.left( norm.size() - suffix.size() );
				}
				QStringList aggWritten;
				QString aggErr;
				bool aggOk = true;
				{
					StageTimer st2( &msImpostors );
					for ( const LodgenAggSet & a : lodgenNativeAggregateSets() )
						if ( !lodgenAggregateWrite( aggRoot, world.worldspaceEdid(), a, &aggWritten, &aggErr ) ) {
							aggOk = false;
							break;
						}
				}
				if ( !aggOk ) {
					err() << "error: " << aggErr << Qt::endl;
					lodgenNativeEnd();
					return 1;
				}
				out() << "native-aggregate: " << lodgenNativeAggregateSets().size()
					<< " card set(s), " << aggWritten.size() << " files under "
					<< lodgenFo4csWorldDir( aggRoot, world.worldspaceEdid() ) << "/Aggregate"
					<< Qt::endl;
			}
			lodgenNativeEnd();
		}
		/* ===== THE SCRATCH TEARDOWN (lane BTOFREE1, 2026-09-16) ============
		 *
		 * LAST of the object passes and FIRST of the bookkeeping: every
		 * read-back above -- the texture arrays, the atlas, the shape merge,
		 * the far-ring cut and the card arrays -- has had the chunks and their
		 * manifests side by side, exactly as it did when they lived in the
		 * output folder. Now the manifest sidecars move to the output folder
		 * (bungo's open call is to keep them) and the chunks go.
		 *
		 * The count and the bytes are MEASURED here, from the files, not
		 * predicted from `writtenBto.size()`: a census field states what is on
		 * disk or it states nothing (CONSTITUTION 4). */
		if ( !btoScratch.isEmpty() ) {
			/* THE SIDECARS LAND UNDER THE ONE ROOT (lane LAYOUT1, 2026-09-16):
			 * they describe files that now live under `FO4CSLOD\<ws>\`, so
			 * they sit beside them rather than at the mod folder's own root. */
			const QString manifestDir =
				lodgenFo4csWorldDir( outDir, world.worldspaceEdid() );
			QDir().mkpath( manifestDir );
			const LodgenBtoScratchResult r =
				lodgenDropBtoScratch( writtenBto, btoScratch, manifestDir );
			for ( const QString & w : r.warnings )
				err() << w << Qt::endl;
			censusOut( QString( "bto scratch: %1 chunk(s) built in %2, %3 removed, "
							  "%4 manifest sidecar(s) kept, %5 bytes freed" )
				.arg( r.built ).arg( btoScratch ).arg( r.dropped ).arg( r.manifests )
				.arg( r.freed ) );
			out().flush();
		} else if ( pass.wantBto ) {
			lodgenSetBtoDisposition( QString(), writtenBto.size(), 0, 0 );
		}

		/* THE LEDGER GOES HERE, LAST, and the reason is a defect this lane
		 * shipped and its own gate caught. It used to be written straight
		 * after the chunk pass -- which is where the chunks are finished,
		 * but NOT where the FILES are: the merge and the far-ring simplify
		 * both reopen every written .BTO and save it again. Digesting them
		 * before those passes recorded a hash of a file that no longer
		 * existed by the time the run ended, so the very next --incremental
		 * run found all nine outputs "lost" and rebaked the whole region
		 * while reporting, accurately and uselessly, 9 of 9 dirty.
		 *
		 * The null arm of gate B3 is what found it: it passed byte identity
		 * (a full rebake trivially matches a full bake) and failed the
		 * census line beside it. That is exactly why the census is printed
		 * next to the verdict instead of trusted behind it. */
		out() << done << " chunk(s) written to " << outDir
			  << ", " << skipped << " empty, " << failed << " failed" << Qt::endl;
		censusOut( lodgenStageTimeLine( msLandscape, msMeshes, msTextures, msImpostors,
			lodgenNativeLibrarySplit() ) );
		censusOut( lodgenBakeCensusLine() );
		/* ===== THE BAKE RECORD, written by EVERY region bake ==============
		 *
		 * Lane INCR1 wrote it first, as the ledger, and its reason still holds:
		 * not behind a flag, because a feature that needs yesterday to have been
		 * clairvoyant is not a feature -- the first time anyone wants
		 * --incremental, the record has to already be there.
		 *
		 * Lane BAKEREC1 (2026-09-17) made it the BAKE RECORD as well. It goes
		 * LAST, after every other file is closed AND after every census line is
		 * printed -- the second half is new, and it is why this block moved down
		 * past the two `censusOut` calls now above it: the record carries the census
		 * VERBATIM, and a record written before the last two lines were printed
		 * would have carried a census that was two lines short while looking
		 * complete. Its presence beside the outputs is what says "this bake
		 * finished".
		 *
		 * It is DETERMINISTIC except for the FIVE things `src/lodbfile.h` names
		 * -- the `baked` line, a plugin's path field, the resource lines, the
		 * `stage times:` census line and the `peak working set:` clause of the
		 * `bake census:` one -- so two full bakes of the same tree write the
		 * same bytes everywhere else.
		 * Gate B4 and `tests/spells/lodgen_bakerec.sh` leg (h) check exactly
		 * that. */
		{
			QStringList recWarn;
			QString recLine;
			lodgenIncrementalWriteRecord( inc, world, gLgArgv, gLgResourceStack, &recWarn, &recLine );
			for ( const QString & w : recWarn )
				err() << w << Qt::endl;
			if ( !recLine.isEmpty() )
				out() << recLine << Qt::endl;
			out().flush();
		}

		return failed ? 1 : 0;
	}
	if ( haveTerrain ) {
		EsmWorld world;
		QString error;
		if ( !world.load( file, worldspace ? worldspace : 0x3CU, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		LodgenTerrainOptions opts;
		opts.dim = dim > 0 ? dim : 4;
		opts.geomorph = geomorph;
		opts.terrainIdentity = terrainIdentity;
		opts.waterSubdiv = waterSubdiv;
		opts.shoreDenser = shoreDenser;
		opts.shoreDensity = shoreDensity;
		if ( targetTris >= 0 )
			opts.targetTrisPerCell = targetTris;
		NifModel nif;
		if ( !lodgenBuildTerrainChunk( &nif, world, chunkX, chunkY, opts, &error ) ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << "terrain chunk (" << chunkX << "," << chunkY << ") dim " << opts.dim
			  << ": " << nif.getBlockCount() << " blocks" << Qt::endl;
		for ( int b = 0; b < nif.getBlockCount(); b++ )
			out() << "  " << blockLabel( &nif, b ) << Qt::endl;
		return saveNif( nif, outFile ) ? 0 : 1;
	}
	if ( listWorldspaces || !worldspace ) {
		QString error;
		const auto worlds = EsmWorld::listWorldspaces( file, &error );
		if ( !error.isEmpty() ) {
			err() << "error: " << error << Qt::endl;
			return 1;
		}
		out() << worlds.size() << " worldspace(s)" << Qt::endl;
		for ( const auto & w : worlds )
			out() << "  " << QString( "%1" ).arg( w.first, 8, 16, QChar( '0' ) )
				  << "  " << w.second << Qt::endl;
		if ( listWorldspaces )
			return 0;
		err() << "error: pass --worldspace <hex form id> to inspect one" << Qt::endl;
		return 2;
	}

	EsmWorld world;
	QString error;
	if ( !world.load( file, worldspace, &error ) ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}
	int minX, minY, maxX, maxY;
	world.cellBounds( minX, minY, maxX, maxY );
	out() << "worldspace " << QString( "%1" ).arg( world.worldspace(), 8, 16, QChar( '0' ) )
		  << " '" << world.worldspaceEdid() << "'  cells " << world.cellCount()
		  << "  grid [" << minX << "," << minY << "]..[" << maxX << "," << maxY << "]" << Qt::endl;
	if ( !haveCell )
		return 0;

	if ( !world.hasCell( cellX, cellY ) ) {
		err() << "error: no exterior cell (" << cellX << "," << cellY << ")" << Qt::endl;
		return 1;
	}
	EsmLand land;
	if ( world.land( cellX, cellY, land ) ) {
		out() << "cell (" << cellX << "," << cellY << ") LAND corners"
			  << "  SW " << land.heights[0][0] << "  SE " << land.heights[0][32]
			  << "  NW " << land.heights[32][0] << "  NE " << land.heights[32][32]
			  << "  center " << land.heights[16][16] << Qt::endl;
	} else {
		out() << "cell (" << cellX << "," << cellY << ") has no LAND" << Qt::endl;
	}
	const QVector<EsmRefr> refs = world.refrs( cellX, cellY );
	int withLod = 0, disabled = 0;
	for ( const EsmRefr & r : refs ) {
		if ( r.initiallyDisabled || r.deleted )
			disabled++;
		else if ( r.base && world.lodBase( r.base ).hasLod )
			withLod++;
	}
	const float cellMinX = float( cellX ) * 4096.0f, cellMinY = float( cellY ) * 4096.0f;
	const auto persistent = world.persistentRefrsIn(
		cellMinX, cellMinY, cellMinX + 4096.0f, cellMinY + 4096.0f );
	int pWithLod = 0;
	for ( const EsmRefr & r : persistent )
		if ( !r.initiallyDisabled && !r.deleted && r.base && world.lodBase( r.base ).hasLod )
			pWithLod++;
	out() << "  refs " << refs.size() << " (" << withLod << " with LOD models, "
		  << disabled << " disabled/deleted)"
		  << "  + persistent in-bounds " << persistent.size()
		  << " (" << pWithLod << " with LOD)" << Qt::endl;
	// sample the first few LOD-bearing refs, generation-style
	int shown = 0;
	for ( const EsmRefr & r : refs ) {
		if ( shown >= 5 )
			break;
		if ( r.initiallyDisabled || r.deleted || !r.base )
			continue;
		const EsmLodBase & b = world.lodBase( r.base );
		if ( !b.hasLod )
			continue;
		out() << "    " << QString( "%1" ).arg( r.formID, 8, 16, QChar( '0' ) )
			  << " at (" << r.pos[0] << ", " << r.pos[1] << ", " << r.pos[2] << ") scale " << r.scale
			  << "  lod0 " << ( b.models[0].isEmpty() ? QStringLiteral( "-" ) : b.models[0] ) << Qt::endl;
		shown++;
	}
	return 0;
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

//! Dump every BSTriShape-family block's raw vertex positions, one "v x y z"
//! line each, prefixed by "b <block> <type> '<name>'" — for external
//! geometry comparison (the LODGEN parity audit diffs these against
//! vanilla chunks, which share the same miniature-space conventions).
int cmdVerts( const QString & file )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		QModelIndex iShape = nif.getBlockIndex( b );
		if ( !nif.isNiBlock( iShape, "BSTriShape" )
			&& !nif.isNiBlock( iShape, "BSSubIndexTriShape" )
			&& !nif.isNiBlock( iShape, "BSMeshLODTriShape" ) )
			continue;
		QModelIndex iVerts = nif.getIndex( iShape, "Vertex Data" );
		const int numVerts = int( nif.get<quint32>( iShape, "Num Vertices" ) );
		if ( !iVerts.isValid() || numVerts <= 0 )
			continue;
		out() << "b " << b << " " << nif.itemName( iShape )
			  << " '" << nif.get<QString>( iShape, "Name" ) << "'" << Qt::endl;
		const BSVertexDesc desc( nif.get<BSVertexDesc>( iShape, "Vertex Desc" ) );
		const bool full = ( desc.GetFlags() & VertexFlags::VF_FULLPREC );
		const bool colors = ( desc.GetFlags() & VertexFlags::VF_COLORS );
		for ( int v = 0; v < numVerts; v++ ) {
			QModelIndex row = nif.index( v, 0, iVerts );
			const Vector3 p = full ? nif.get<Vector3>( row, "Vertex" )
				: Vector3( nif.get<HalfVector3>( row, "Vertex" ) );
			out() << "v " << p[0] << " " << p[1] << " " << p[2];
			if ( colors ) {
				// LODGEN identity: object index = R + G*256
				const ByteColor4 c = nif.get<ByteColor4>( row, "Vertex Colors" );
				out() << " " << ( int( c[0] * 255.0f + 0.5f )
					+ int( c[1] * 255.0f + 0.5f ) * 256 );
			}
			if ( desc.GetFlags() & VertexFlags::VF_UV ) {
				const Vector2 uv = nif.get<HalfVector2>( row, "UV" );
				out() << " uv " << uv[0] << " " << uv[1];
			}
			out() << Qt::endl;
		}
	}
	return 0;
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

//! Where every NiAVObject actually IS, once its parent chain is applied.
/*! Exists because "did this edit move anything" is otherwise unanswerable from
 *  the CLI: a block's own Translation says nothing when the chain above it
 *  changed. Printed as translation, the nine rotation terms and scale, so two
 *  files can be diffed by name — which is what proves a loading-screen convert
 *  put a kept effect branch exactly where the skeleton had it. */
int cmdWorld( const QString & file, int block, const QString & typeFilter )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	auto f = []( float v ) { return QString::number( v, 'f', 4 ); };
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		if ( block >= 0 && b != block )
			continue;
		QModelIndex idx = nif.getBlockIndex( b );
		if ( !nif.blockInherits( idx, "NiAVObject" ) )
			continue;
		if ( !typeFilter.isEmpty() && !nif.blockInherits( idx, typeFilter ) )
			continue;
		const Transform t = skeletonWorldTransform( &nif, b );
		QStringList rot;
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				rot << f( t.rotation( i, j ) );
		out() << "[" << b << "] " << nif.itemName( idx )
			  << " '" << nif.get<QString>( idx, "Name" ) << "'"
			  << " T=(" << f( t.translation[0] ) << ", " << f( t.translation[1] )
			  << ", " << f( t.translation[2] ) << ")"
			  << " R=(" << rot.join( QStringLiteral( " " ) ) << ")"
			  << " S=" << f( t.scale ) << Qt::endl;
	}
	return 0;
}

/*! Every shape's segment/subsegment table, with the shared data resolved.
 *
 *  Bone IDs are hashes; the names come from the same lookup the .ssf writer
 *  uses, so an unresolved one prints as #hash rather than being guessed at.
 */
/*! Every checker spell, the way the Issue Manager runs them.
 *
 *  The panel's own scan, headless: cast each `checker()` spell with an invalid
 *  index in MSG_TEST mode and print what it logs. Without MSG_TEST the findings
 *  go nowhere a CLI can see, which is why casting a checker directly appears to
 *  do nothing.
 *
 *  `constant()` is deliberately NOT the filter here, for the reason the panel
 *  records: it promises a spell does not modify the file, not that it stays
 *  quiet, and several constant spells open a message box -- which in a headless
 *  QCoreApplication aborts the process.
 */
int cmdCheck( const QString & file, const QString & only )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	QList<SpellPtr> checkers;
	for ( SpellPtr s : SpellBook::spells() ) {
		if ( !s || checkers.contains( s ) || !s->checker() )
			continue;
		if ( !only.isEmpty() && !s->name().contains( only, Qt::CaseInsensitive ) )
			continue;
		checkers.append( s );
	}
	std::sort( checkers.begin(), checkers.end(),
		[]( const SpellPtr & a, const SpellPtr & b ) { return a->name() < b->name(); } );

	const BaseModel::MsgMode was = nif.getMessageMode();
	nif.setMessageMode( BaseModel::MSG_TEST );
	int findings = 0, worst = 2;
	for ( SpellPtr s : checkers ) {
		if ( !s->isApplicable( &nif, QModelIndex() ) )
			continue;
		nif.getMessages();						// drain anything pending
		s->cast( &nif, QModelIndex() );
		const QList<TestMessage> messages = nif.getMessages();
		if ( messages.isEmpty() )
			continue;
		out() << s->name() << Qt::endl;
		for ( const TestMessage & msg : messages ) {
			const QtMsgType type = msg.type();
			const char * mark = ( type == QtCriticalMsg || type == QtFatalMsg ) ? "!!"
				: ( type == QtWarningMsg ? " !" : "  " );
			out() << "  " << mark << " " << QString( msg ) << Qt::endl;
			findings++;
			worst = qMin( worst, ( type == QtCriticalMsg || type == QtFatalMsg ) ? 0
				: ( type == QtWarningMsg ? 1 : 2 ) );
		}
	}
	nif.setMessageMode( was );

	if ( !findings ) {
		out() << "no findings" << Qt::endl;
		return 0;
	}
	out() << findings << " finding(s). The Issue Manager groups these and offers "
		  << "the spell that repairs each one." << Qt::endl;
	// A clean exit code for "nothing wrong", 1 for anything worse than a note,
	// so a script can gate on it.
	return worst < 2 ? 1 : 0;
}

int cmdSegments( const QString & file, int block )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	int shapes = 0;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		if ( block >= 0 && b != block )
			continue;
		QModelIndex shape = nif.getBlockIndex( b );
		if ( !nif.isNiBlock( shape, "BSSubIndexTriShape" ) )
			continue;
		const QString report = tlRiggingSegmentReport( &nif, shape );
		if ( report.isEmpty() )
			continue;
		shapes++;
		out() << blockLabel( &nif, b ) << Qt::endl;
		out() << report;
	}
	if ( !shapes ) {
		err() << "no BSSubIndexTriShape with segments" << Qt::endl;
		return 1;
	}
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
	/* Colours are the exception, and were broken both ways before this.
	 *
	 * NifValue::setFromString parses every colour type through QColor(s), so it
	 * wants "#rrggbb" or a colour name — but the numeric guard below demanded
	 * comma-separated components. The result was the worst of both: "#ff0000"
	 * was REJECTED as malformed, while "1,0,0" passed the guard, failed inside
	 * QColor, and silently wrote BLACK while reporting success. Setting a light
	 * to red turned it off instead.
	 *
	 * Accept both. Components are the natural thing to type for a value that
	 * prints as three floats in HDR, so they are converted to the form QColor
	 * actually understands rather than being handed over to fail.
	 */
	QString colorText = value;   // colours may be rewritten into QColor form
	bool isColor = false;
	switch ( v.type() ) {
	case NifValue::tColor3:
	case NifValue::tColor4:
	case NifValue::tByteColor4:
	case NifValue::tByteColor4BGRA:
		isColor = true; break;
	default:
		break;
	}
	if ( isColor ) {
		const QStringList parts = value.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
		if ( parts.size() >= 3 ) {
			float c[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			bool allNum = true;
			for ( int i = 0; i < parts.size() && i < 4; i++ ) {
				bool numOk = false;
				c[i] = parts.at( i ).trimmed().toFloat( &numOk );
				allNum = allNum && numOk;
			}
			if ( !allNum ) {
				err() << "error: " << nif.itemStrType( idx )
					  << " components must be numbers  (got: " << value << ")" << Qt::endl;
				return 1;
			}
			// 0..1 is how these are stored; >1 means someone typed 0..255
			const float scale = ( c[0] > 1.0f || c[1] > 1.0f || c[2] > 1.0f ) ? ( 1.0f / 255.0f ) : 1.0f;
			QColor col;
			col.setRgbF( qBound( 0.0f, c[0] * scale, 1.0f ), qBound( 0.0f, c[1] * scale, 1.0f ),
				qBound( 0.0f, c[2] * scale, 1.0f ), qBound( 0.0f, c[3], 1.0f ) );
			colorText = col.name( QColor::HexArgb );
		} else if ( !QColor::isValidColorName( value ) ) {
			err() << "error: " << nif.itemStrType( idx )
				  << " takes \"#rrggbb\", a colour name, or comma-separated components"
				  << "  (got: " << value << ")" << Qt::endl;
			return 1;
		}
	}

	int wantComponents = 0;
	switch ( v.type() ) {
	case NifValue::tVector2:
		wantComponents = 2; break;
	case NifValue::tVector3:
	case NifValue::tHalfVector3:
	case NifValue::tShortVector3:
	case NifValue::tUshortVector3:
	case NifValue::tByteVector3:
		wantComponents = 3; break;
	case NifValue::tVector4:
	case NifValue::tByteVector4:
	case NifValue::tUDecVector4:
	case NifValue::tQuat:
	case NifValue::tQuatXYZW:
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

	if ( !v.setFromString( colorText, &nif, item ) ) {
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
int cmdMerge( const QString & file, const QStringList & adds, const QStringList & addAttach,
			  bool noDedupe, const QString & outFile )
{
	if ( adds.isEmpty() ) {
		err() << "error: --add <file> is required (repeat it for each piece)" << Qt::endl;
		return 1;
	}
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	QStringList allDupes;
	int totalShapes = 0, totalReused = 0;
	for ( int i = 0; i < adds.size(); i++ ) {
		const QString & add = adds.at( i );
		NifMergeResult r;
		if ( !nifMergeFile( &nif, add, !noDedupe, r, addAttach.value( i ) ) ) {
			err() << "error: " << r.error << Qt::endl;
			return 1;
		}
		out() << QString( "merged %1: +%2 block(s), %3 shape(s), %4 node(s) added, "
						  "%5 reused by name, %6 re-parented, %7 rebased" )
			.arg( QFileInfo( add ).fileName() ).arg( r.blocksAdded ).arg( r.shapesAdded )
			.arg( r.nodesAdded ).arg( r.nodesReused ).arg( r.reparented ).arg( r.rebased ) << Qt::endl;
		// where an effect landed is the whole question for an ArtObject, so it is
		// always stated -- including "the root", which is usually not what is wanted
		if ( !r.namedAttachments.isEmpty() )
			out() << "  branches attached by name to "
				  << r.namedAttachments.join( QStringLiteral( ", " ) ) << Qt::endl;
		if ( !r.attachedTo.isEmpty() )
			out() << "  attached to " << r.attachedTo << Qt::endl;
		else if ( r.isEffect && r.namedAttachments.isEmpty() )
			out() << "  attached to the ROOT: this file's AttachT names no node "
					 "(its ARTO record in the ESP does). Use --attach <node>." << Qt::endl;
		totalShapes += r.shapesAdded;
		totalReused += r.nodesReused;
		allDupes << r.duplicateNames;
	}
	out() << "total: " << totalShapes << " shape(s) added, "
		  << totalReused << " node(s) shared with the target" << Qt::endl;
	if ( totalReused == 0 && !noDedupe )
		out() << "note: no nodes matched by name — the merged pieces do NOT share a\n"
			  << "      skeleton, so posing them as one rig will not work." << Qt::endl;

	// A rig binds bones by NAME, so a repeated name silently sends a pose to the
	// wrong node. It should never happen; it is reported either way, because the
	// symptom (a rig that poses into a heap) says nothing about the cause.
	allDupes.removeDuplicates();
	allDupes.sort();
	if ( allDupes.isEmpty() ) {
		out() << "no duplicate bone names introduced" << Qt::endl;
	} else {
		out() << "WARNING: the merge introduced " << allDupes.size()
			  << " duplicate bone name(s); posing will address the wrong node:" << Qt::endl;
		out() << "  " << allDupes.mid( 0, 12 ).join( QStringLiteral( ", " ) )
			  << ( allDupes.size() > 12 ? QStringLiteral( ", ..." ) : QString() ) << Qt::endl;
	}
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

//! Bake a sequence to a still at one time.
/*! With no --sequence, lists what the file has and each one's range, so picking
 *  a time is not a guess. */
/*! A sequence's authored Cycle Type, by name, for the listing.
 *
 *  Worth printing next to the range because it is the other half of "what does
 *  this clip do": 0..0.1s tells you nothing about whether it then stops, repeats
 *  or runs back. Missing before 10.1.0.106, where the row genuinely is not
 *  there.
 */
static QString seqCycleName( const NifModel * nif, const QString & seqName )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex iSeq = nif->getBlockIndex( b, "NiControllerSequence" );
		if ( !iSeq.isValid() || nif->resolveString( iSeq, "Name" ) != seqName )
			continue;
		if ( !nif->getIndex( iSeq, "Cycle Type" ).isValid() )
			break;
		switch ( nif->get<int>( iSeq, "Cycle Type" ) ) {
		case 0:  return QStringLiteral( "CYCLE_LOOP" );
		case 1:  return QStringLiteral( "CYCLE_REVERSE" );
		case 2:  return QStringLiteral( "CYCLE_CLAMP" );
		default: return QStringLiteral( "CYCLE_?" );
		}
	}
	return QStringLiteral( "(no cycle type)" );
}

int cmdFreeze( const QString & file, const QString & sequence, float time,
			   bool keepGraph, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	if ( sequence.isEmpty() ) {
		const QStringList seqs = AnimSetup::sequenceNames( &nif );
		if ( seqs.isEmpty() ) {
			out() << "no sequences in this file" << Qt::endl;
			return 0;
		}
		out() << "sequences:" << Qt::endl;
		for ( const QString & s : seqs ) {
			float a = 0, b = 0;
			FreezeAnim::sequenceRange( &nif, s, &a, &b );
			out() << "  " << s << "  " << a << " .. " << b << " s"
				  << "  " << seqCycleName( &nif, s ) << Qt::endl;
		}
		return 0;
	}

	QString error;
	const FreezeAnim::Result r = FreezeAnim::freeze( &nif, sequence, time, !keepGraph, &error );
	if ( !r.ok ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}

	out() << "froze '" << sequence << "' at " << time << "s: "
		  << r.baked << " baked, " << r.skipped << " skipped, "
		  << r.blocksRemoved << " block(s) removed" << Qt::endl;
	for ( const QString & u : r.unhandled )
		out() << "  skipped: " << u << Qt::endl;
	for ( const QString & n : r.notes )
		out() << "  note: " << n << Qt::endl;

	return outFile.isEmpty() ? 0 : ( saveNif( nif, outFile ) ? 0 : 1 );
}

//! Bake a posed, assembled rig into loading-screen art.
/*! Copy one mesh's normals onto another — the Transfer Normals spell without
 *  its dialog.
 *
 *  It exists so the mapping can be checked: a modal dialog cannot be driven
 *  headlessly, and an algorithm nobody can run in a test is one nobody should
 *  trust. Scripting it over a folder of meshes is the other half of the reason.
 */
int cmdTransferNormals( const QString & file, const QVector<int> & fromBlocks, int toBlock,
                        int mapping, float mix, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;
	if ( fromBlocks.isEmpty() || toBlock < 0 ) {
		err() << "error: --from and --to are required (block numbers); --from repeats" << Qt::endl;
		return 1;
	}
	if ( mapping < 0 || mapping >= NormalTransfer::MappingCount ) {
		err() << "error: --mapping must be 0.." << ( NormalTransfer::MappingCount - 1 ) << Qt::endl;
		return 1;
	}

	QVector<NormalTransfer::Mesh> parts;
	for ( int b : fromBlocks ) {
		NormalTransfer::Mesh m = NormalTransfer::read( &nif, b );
		if ( !m.valid() ) {
			err() << "error: block " << b << " has no normals to read" << Qt::endl;
			return 1;
		}
		parts.append( m );
	}
	// several sources are one surface: every mapping asks what is NEAREST, and
	// over a set of meshes that is the union of them
	const NormalTransfer::Mesh src = NormalTransfer::combine( parts );
	const NormalTransfer::Mesh tgt = NormalTransfer::read( &nif, toBlock );
	if ( !tgt.valid() ) {
		err() << "error: block " << toBlock << " has no normals to read" << Qt::endl;
		return 1;
	}
	if ( mapping == NormalTransfer::Topology && fromBlocks.size() > 1 ) {
		err() << "error: topology mapping takes one source; index N of a combination means nothing"
			  << Qt::endl;
		return 1;
	}
	if ( mapping == NormalTransfer::Topology && src.pos.size() != tgt.pos.size() ) {
		err() << "error: topology mapping needs equal vertex counts (" << src.pos.size()
			  << " vs " << tgt.pos.size() << ")" << Qt::endl;
		return 1;
	}

	const QVector<Vector3> result = NormalTransfer::map( src, tgt, mapping, mix );
	const int written = NormalTransfer::apply( &nif, toBlock, result );

	// how far each normal actually turned, which is the thing worth reporting:
	// "500 normals written" is true of a transfer that changed nothing
	double worst = 0.0, total = 0.0;
	for ( int v = 0; v < result.size() && v < tgt.nrm.size(); v++ ) {
		Vector3 a = tgt.nrm.at( v ), b = result.at( v );
		if ( a.squaredLength() < 1.0e-12f || b.squaredLength() < 1.0e-12f )
			continue;
		a.normalize();
		b.normalize();
		const double ang = std::acos( std::clamp( double( Vector3::dotproduct( a, b ) ), -1.0, 1.0 ) )
			* 180.0 / M_PI;
		worst = std::max( worst, ang );
		total += ang;
	}
	QStringList fromList;
	for ( int b : fromBlocks )
		fromList << QString::number( b );
	out() << written << " of " << tgt.pos.size() << " normal(s) transferred from block(s) "
		  << fromList.join( QLatin1Char( ',' ) ) << " using "
		  << NormalTransfer::mappingName( mapping ) << Qt::endl;
	out() << "  turned by " << ( result.isEmpty() ? 0.0 : total / result.size() )
		  << " deg on average, " << worst << " deg at most" << Qt::endl;

	return outFile.isEmpty() ? 0 : ( saveNif( nif, outFile ) ? 0 : 1 );
}

int cmdLoadingScreen( const QString & file, bool noZoomTarget, bool keepParticles,
					  bool keepEffects, const QString & outFile )
{
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	QString error;
	const LoadingScreen::Result r = LoadingScreen::convert( &nif, !noZoomTarget, keepParticles,
		keepEffects, &error );
	if ( !r.ok ) {
		err() << "error: " << error << Qt::endl;
		return 1;
	}

	out() << r.shapesBaked << " skinned shape(s) evaluated, "
		  << r.shapesFolded << " rigid shape(s) folded, "
		  << r.nodesRemoved << " node(s) and " << r.blocksRemoved << " block(s) removed"
		  << ( r.zoomTargetAdded ? ", LoadingMenuZoomTarget added" : "" ) << Qt::endl;
	if ( r.effectBranches > 0 )
		out() << "  " << r.effectBranches << " effect branch(es) kept live, "
			  << r.effectBlocks << " block(s), attached to "
			  << ( r.attachNodes.isEmpty() ? QStringLiteral( "the root" )
			                               : r.attachNodes.join( QStringLiteral( ", " ) ) ) << Qt::endl;
	for ( const QString & n : r.notes )
		out() << "  note: " << n << Qt::endl;

	return outFile.isEmpty() ? 0 : ( saveNif( nif, outFile ) ? 0 : 1 );
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

/*! The clip TSV both BUILD8 commands write: one row per frame per track,
 *  `frame track bone tx ty tz qx qy qz qw sx sy sz`, root motion as track -1
 *  with `tx ty tz yaw`.  Nine significant digits, the same as
 *  tests/hkxwrite_dump.cpp -- lane BUILD8.
 */
bool cmdHkxTsvWrite( const HkxAnimClip & c, const QString & out )
{
	QFile fo( out );
	if ( !fo.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		err() << "cannot write " << out << Qt::endl;
		return false;
	}
	QTextStream ts( &fo );
	ts.setRealNumberPrecision( 9 );
	ts << "# frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz\n";
	for ( int fr = 0; fr < c.numFrames; fr++ ) {
		for ( int t = 0; t < c.numTracks; t++ ) {
			const HkxTransform & x = c.frames[fr][t];
			ts << fr << '\t' << t << '\t' << ( t < c.trackToBone.size() ? c.trackToBone[t] : t );
			const float v[10] = { x.translation[0], x.translation[1], x.translation[2],
				x.rotation[1], x.rotation[2], x.rotation[3], x.rotation[0],
				x.scale[0], x.scale[1], x.scale[2] };
			for ( float e : v )
				ts << '\t' << QString::number( double( e ), 'g', 9 );
			ts << '\n';
		}
	}
	for ( int fr = 0; fr < c.rootMotion.size(); fr++ ) {
		const HkxRootMotion & r = c.rootMotion[fr];
		ts << fr << "\t-1\t-1";
		const float v[4] = { r.translation[0], r.translation[1], r.translation[2], r.yaw };
		for ( float e : v )
			ts << '\t' << QString::number( double( e ), 'g', 9 );
		ts << '\n';
	}
	ts.flush();
	return fo.error() == QFile::NoError;
}

/*! `gltf <file.nif> -o out.gltf [--clip C.hkx] [--bones skeleton.hkx]
 *  [--root-motion]` -- lane HKX4.
 *
 *  Writes out.gltf and out.bin: the NIF's node tree, its skinned shapes and,
 *  when --clip is given, that clip as a glTF animation at its own frame rate.
 *  --bones names the file whose hkaSkeleton gives the tracks their bone names
 *  (a clip usually carries none of its own; use the game's skeleton.hkx).
 *  Without --root-motion the clip plays in place and the travel is recorded
 *  in the animation's extras. docs/GLTF_INTERCHANGE.md is the contract.
 */
int cmdGltf( const QString & file, const QString & outFile, const QString & clipFile,
			 const QString & bonesFile, bool rootMotion, const GltfExportOptions & optsIn )
{
	if ( outFile.isEmpty() ) {
		err() << "gltf: -o <out.gltf> is required" << Qt::endl;
		return 2;
	}
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	HkxAnimClip clip;
	QStringList boneNames;
	bool haveClip = false;
	if ( !clipFile.isEmpty() ) {
		const HkxAnimFile cf = hkxAnimLoad( clipFile );
		if ( !cf.ok() ) {
			err() << "gltf: " << cf.error << Qt::endl;
			return 1;
		}
		if ( cf.clips.isEmpty() ) {
			err() << "gltf: " << clipFile << " carries no animation" << Qt::endl;
			return 1;
		}
		clip = cf.clips.first();
		haveClip = true;
		if ( !bonesFile.isEmpty() ) {
			const HkxAnimFile bf = hkxAnimLoad( bonesFile );
			if ( !bf.ok() || bf.skeletons.isEmpty() ) {
				err() << "gltf: " << ( bf.ok() ? QStringLiteral( "%1 carries no hkaSkeleton" ).arg( bonesFile )
											   : bf.error ) << Qt::endl;
				return 1;
			}
			boneNames = bf.skeletons.first().boneNames;
		} else if ( !cf.skeletons.isEmpty() ) {
			boneNames = cf.skeletons.first().boneNames;
		} else {
			err() << "gltf: " << clipFile << " carries no skeleton; pass --bones skeleton.hkx" << Qt::endl;
			return 1;
		}
	}

	GltfExportReport report;
	GltfExportCharReport charReport;
	QString error;
	// lane GLTFEXPORT1. optsIn carries the flags; --root-motion is folded in so
	// the old spelling still means what it meant. With no new flag at all this
	// is gltfExportOptionsAreLegacy() and gltfExportCharacter() forwards to the
	// old writer unchanged -- the byte-identity row of the gate.
	GltfExportOptions opts = optsIn;
	if ( rootMotion && opts.rootMotion == GltfExportOptions::RootMotion::Strip )
		opts.rootMotion = GltfExportOptions::RootMotion::Root;
	if ( !haveClip )
		opts.includeClip = false;
	if ( !gltfExportCharacter( &nif, QModelIndex(), haveClip ? &clip : nullptr, boneNames,
							   opts, outFile, report, charReport, error ) ) {
		err() << "gltf: " << error << Qt::endl;
		return 1;
	}
	for ( const QString & s : gltfExportOptionsSummary( opts ) )
		out() << "  " << s << Qt::endl;
	for ( const QString & s : charReport.notes )
		out() << "  " << s << Qt::endl;
	if ( !charReport.unmatchedBones.isEmpty() )
		out() << "  " << charReport.unmatchedBones.size()
			  << " skin bone(s) the skeleton did not carry (they kept the part's own node): "
			  << charReport.unmatchedBones.join( QStringLiteral( ", " ) ) << Qt::endl;
	/* lane GLTFEXPORT1: the MEASURED line. One `key=value` row, so
	 * tests/spells/gltf_export_options.sh reads numbers out of the export
	 * itself and does not have to re-derive them from the .gltf -- and so a
	 * number that moves shows up in the log a human reads too. Every one of
	 * these is a count the exporter took while it worked, not a re-count. */
	out() << "  MEASURED"
		  << " skeleton_nodes=" << charReport.skeletonNodes
		  << " parts_merged=" << charReport.partsMerged
		  << " matched_by_name=" << charReport.nodesMatchedByName
		  << " nodes_added=" << charReport.nodesAdded
		  << " helpers_dropped=" << charReport.helpersDropped
		  << " helpers_kept_weighted=" << charReport.helpersKeptWeighted
		  << " joints_per_skin=" << charReport.jointsPerSkin
		  << " skins_unified=" << charReport.skinsUnified
		  << " inverse_binds_derived=" << charReport.inverseBindsDerived
		  << " textures_copied=" << charReport.texturesCopied
		  << " textures_missing=" << charReport.texturesMissing
		  << " build_bones_scaled=" << charReport.buildBonesScaled
		  << " build_cycle_channels=" << charReport.buildCycleChannels
		  << " unmatched_bones=" << charReport.unmatchedBones.size()
		  << " skeleton_auto_missed=" << int( charReport.skeletonAutoMissed )
		  << " legacy_defaults=" << int( gltfExportOptionsAreLegacy( opts ) )
		  << " metres_per_unit=" << QString::number( opts.metresPerUnit(), 'g', 9 )
		  << " tracks_matched=" << report.tracksMatched
		  << " tracks_unmatched=" << report.tracksUnmatched
		  << " nodes=" << report.nodes
		  << " shapes=" << report.shapes
		  << " skinned=" << report.skinnedShapes
		  << Qt::endl;
	out() << "wrote " << outFile << ": " << report.nodes << " nodes, " << report.shapes
		  << " shapes (" << report.skinnedShapes << " skinned), " << report.vertices
		  << " vertices, " << report.triangles << " triangles" << Qt::endl;
	if ( haveClip )
		out() << "  clip '" << clip.name << "': " << clip.numFrames << " frames at "
			  << clip.frameDuration << " s, " << report.tracksMatched << " of "
			  << ( report.tracksMatched + report.tracksUnmatched ) << " tracks matched a node"
			  << Qt::endl;
	for ( const QString & n : report.notes )
		out() << "  " << n << Qt::endl;
	return 0;
}

/*! `gltf-import <in.gltf> -o OUT.hkx [--bones S.hkx] [--fps N] [--source-rate]
 *  [--root-motion] [--route a|b] [--tsv PATH] [--skeleton NAME]` -- lane BUILD8.
 *
 *  The inverse of `gltf`: reads one glTF animation (src/gltfimport.cpp) and
 *  writes it back as a Fallout 4 interleaved .hkx (src/hkxwrite.cpp).
 *  --bones names the .hkx whose hkaSkeleton gives the target bone order; with
 *  none, the glTF's own node names become the bone names and the map is the
 *  identity.  --tsv also dumps the IMPORTED clip, before it is written, in the
 *  13 columns tests/hkxwrite_dump.cpp uses, so the in-memory clip can be
 *  compared without going through the writer at all.
 *  docs/GLTF_IMPORT.md is the contract.
 */
int cmdGltfImport( const QString & file, const QString & outFile, const QString & bonesFile,
				   float fps, bool sourceRate, bool rootMotion, const QString & route,
				   const QString & tsvFile, const QString & skeletonName,
				   const QString & rootNode )			// lane BUILD8: --root-node
{
	if ( outFile.isEmpty() ) {
		err() << "gltf-import: -o <out.hkx> is required" << Qt::endl;
		return 2;
	}

	GltfImportOptions opt;
	if ( fps > 0.0f )
		opt.targetFps = fps;
	opt.preserveSourceRate = sourceRate;
	opt.extractRootMotion = rootMotion;
	// The clip's travel is composed onto the ROOT BONE's node on export, not
	// onto the NIF's root NiNode -- which is what the single-scene-root rule
	// would pick, and which reaches no bone.  Name it: --root-node Root.
	if ( !rootNode.isEmpty() )
		opt.rootNodeName = rootNode;
	if ( !skeletonName.isEmpty() )
		opt.originalSkeletonName = skeletonName;
	if ( !bonesFile.isEmpty() ) {
		const HkxAnimFile bf = hkxAnimLoad( bonesFile );
		if ( !bf.ok() || bf.skeletons.isEmpty() ) {
			err() << "gltf-import: " << ( bf.ok() ? QStringLiteral( "%1 carries no hkaSkeleton" ).arg( bonesFile )
												  : bf.error ) << Qt::endl;
			return 1;
		}
		opt.skeletonBoneNames = bf.skeletons.first().boneNames;
	}

	HkxAnimClip clip;
	GltfImportReport rep;
	if ( !gltfImportRead( file, opt, clip, rep ) ) {
		err() << "gltf-import: " << rep.error << Qt::endl;
		return 1;
	}
	out() << "read " << file << ": " << rep.summary() << Qt::endl;
	out() << "  container " << rep.container << "; up-axis " << rep.upAxisArm << Qt::endl;
	out() << "  rate " << rep.rateArm << Qt::endl;
	out() << "  mapping " << rep.mappingArm << Qt::endl;
	out() << "  " << clip.numTracks << " tracks x " << clip.numFrames << " frames, "
		  << rep.matched.size() << " matched, " << rep.unmatchedNodes.size()
		  << " nodes unmatched, " << rep.unmatchedBones.size() << " bones undriven, "
		  << rep.staticTracks << " static" << Qt::endl;
	if ( rep.rootMotionExtracted )
		out() << "  root motion from '" << rep.rootMotionNode << "': max |T| "
			  << rep.rootMotionMaxTranslation << ", max yaw " << rep.rootMotionMaxYawDeg
			  << " deg" << Qt::endl;

	if ( !tsvFile.isEmpty() && !cmdHkxTsvWrite( clip, tsvFile ) )
		return 1;

	HkxWriteOptions wopt;
	if ( route == QLatin1String( "a" ) )
		wopt.route = HkxWriteOptions::RouteXmlPack;
	if ( !skeletonName.isEmpty() )
		wopt.skeletonName = skeletonName;
	HkxWriteReport wrep;
	if ( !hkxWrite( clip, outFile, wopt, wrep ) ) {
		err() << "gltf-import: " << wrep.error << Qt::endl;
		return 1;
	}
	out() << "wrote " << outFile << ": " << wrep.summary() << Qt::endl;
	return 0;
}

/*! `hkx-tsv <in.hkx> -o OUT.tsv` -- lane BUILD8.
 *
 *  Lane HKX1's decode of a clip, as the 13 columns
 *  `frame track bone tx ty tz qx qy qz qw sx sy sz` with root motion on track
 *  -1, which is what tests/hkxwrite_dump.cpp writes and what
 *  scratchpad/hkx5_20260910/tsvcmp.py reads.  It exists so the round trip can
 *  be measured on the built exe's OWN reader as well as on the independent
 *  Python decoder.
 */
int cmdHkxTsv( const QString & file, const QString & outFile )
{
	if ( outFile.isEmpty() ) {
		err() << "hkx-tsv: -o <out.tsv> is required" << Qt::endl;
		return 2;
	}
	const HkxAnimFile f = hkxAnimLoad( file );
	if ( !f.ok() ) {
		err() << "hkx-tsv: " << f.error << Qt::endl;
		return 1;
	}
	if ( f.clips.isEmpty() ) {
		err() << "hkx-tsv: " << file << " carries no animation" << Qt::endl;
		return 1;
	}
	const HkxAnimClip & c = f.clips.first();
	if ( !cmdHkxTsvWrite( c, outFile ) )
		return 1;
	out() << "wrote " << outFile << ": " << c.numFrames << " frames x " << c.numTracks
		  << " tracks, " << c.rootMotion.size() << " root-motion samples, frameDuration "
		  << c.frameDuration << Qt::endl;
	return 0;
}

int usage()
{
	out() << "NifSkope headless batch mode\n\n"
		  << "  NifSkope -no-gui <command> [options]\n\n"
		  << "Commands:\n"
		  << "  new -o OUT [--cube [--size N]]          write the starter document: an\n"
		  << "                                          empty Fallout 4 scene, header plus\n"
		  << "                                          one root NiNode, the same document\n"
		  << "                                          the GUI opens with. --cube adds a\n"
		  << "                                          cube shape, for test fixtures that\n"
		  << "                                          need geometry and no game corpus\n"
		  << "                                          (N defaults to 2 m in FO4 units)\n"
		  << "  spells [pattern]                        list spells addressable by name\n"
		  << "  info <file>                             version, block count, per-type tally\n"
		  << "  transfer-normals <file> --from N --to M [--mapping 0..5] [--mix F] -o OUT\n"
		  << "                                          copy block N's normals onto block M;\n"
		  << "                                          mapping is Blender's Data Transfer list:\n"
		  << "                                          0 topology, 1/2 nearest corner by normal\n"
		  << "                                          / by face normal, 3 nearest corner of\n"
		  << "                                          nearest face, 4 nearest face interpolated,\n"
		  << "                                          5 projected\n"
		  << "  world <file> [-b N] [-t <type>]         each NiAVObject's WORLD transform,\n"
		  << "                                          for diffing two files by name\n"
		  << "  list <file> [-t <type>]                 block list, optionally filtered\n"
		  << "  check <file> [-t <name>]                the Issue Manager's scan, headless:\n"
		  << "                                          every checker spell, its findings\n"
		  << "                                          marked !! critical / ! warning;\n"
		  << "                                          -t runs only checks whose name\n"
		  << "                                          contains that text. Exit 1 if\n"
		  << "                                          anything worse than a note\n"
		  << "  segments <file> [-b N]                  FO4 dismemberment table: every\n"
		  << "                                          segment and subsegment, its\n"
		  << "                                          triangles, its owning bone and\n"
		  << "                                          the .ssf id that addresses it\n"
		  << "  gltf <file> -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]\n"
		  << "                                          glTF 2.0 export: the node tree,\n"
		  << "                                          the skinned shapes and one .hkx\n"
		  << "                                          clip, opened natively by Blender.\n"
		  << "                                          --bones names the skeleton whose\n"
		  << "                                          bone names the tracks are read by\n"
		  << gltfExportOptionsHelp()
		  << "  gltf-import <in.gltf> -o OUT.hkx [--bones S.hkx] [--fps N] [--source-rate]\n"
		  << "                                          the inverse: one glTF animation\n"
		  << "                                          back to a Fallout 4 .hkx.\n"
		  << "                                          --route a packs through HKXPACK,\n"
		  << "                                          b (default) emits the packfile;\n"
		  << "                                          --tsv also dumps the clip it read\n"
		  << "                                          [--root-motion --root-node Root]\n"
		  << "                                          lifts the travel off that node\n"
		  << "  hkx-tsv <in.hkx> -o OUT.tsv             decode a clip to frame/track rows\n"
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
		  << "  collision <file> --roundtrip [-o F.bin] re-encode every shape from its decoded\n"
		  << "                                          parameters, reassemble the whole packfile,\n"
		  << "                                          diff both against the original, and with\n"
		  << "                                          -o write the reassembled bytes out\n"
		  << "  collision <file> --constraints          write every joint into its NIF block and\n"
		  << "                                          read it back, proving the editable form\n"
		  << "                                          loses nothing the encoder would keep\n"
		  << "  collision <file> --skeleton             is a ragdoll's reference pose derivable\n"
		  << "                                          from the NIF node hierarchy? compares\n"
		  << "                                          every bone against its node's transform\n"
		  << "  collision <file> --bodies               per-body mass, inertia and motion\n"
		  << "                                          properties, for diffing a rebuild\n"
		  << "                                          against vanilla\n"
		  << "  merge <file> [--attach NODE] --add PIECE.nif [...] -o OUT\n"
		  << "                                          splice pieces in, sharing bones by\n"
		  << "                                          name; --attach applies to the next\n"
		  << "                                          --add and overrides its AttachT\n"
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
		  << "                                          into a NiControllerSequence\n"
		  << "  freeze <file>                           list sequences and their ranges\n"
		  << "  freeze <file> --sequence NAME --time T [--keep-graph] -o OUT\n"
		  << "                                          bake the sequence at T seconds into\n"
		  << "                                          the fields it drives and strip the\n"
		  << "                                          controller graph (--keep-graph bakes\n"
		  << "                                          the values but leaves it animating)\n"
		  << "  btd <file.btd> --info                   FO76 terrain database header:\n"
		  << "                                          worldspace extent, height range\n"
		  << "  btd <file.btd> [--region X0 Y0 X1 Y1] [--lod 0..4] -o OUT.nif\n"
		  << "                                          build the region's terrain as\n"
		  << "                                          BSTriShape geometry (default: the\n"
		  << "                                          whole worldspace at LOD4; lod 0 is\n"
		  << "                                          one sample every 32 units, each\n"
		  << "                                          level doubles that)\n"
		  << "  lodl <file.lodl> --info                 our landscape file: extent, height\n"
		  << "                                          range and quantum, sample rate, the\n"
		  << "                                          sections present and the plane keys\n"
		  << "  lodl <file.lodl> --water-census          the version-3 water body table,\n"
		  << "                                          read back out of the file itself\n"
		  << "  lodl <file.lodl> --water-selftest       the classifier's known-answer\n"
		  << "                                          control, with its refuter\n"
		  << "  lodl <file.lodl> --water-mark-selftest  the MARKING tool's gates. It\n"
		  << "                                          REWRITES the file it is given,\n"
		  << "                                          so give it a copy\n"
		  << "  lodl <file.lodl> [--region X0 Y0 X1 Y1] [--lod N] [--plane KEY] [-o OUT.nif]\n"
		  << "                                          build the region as BSTriShape\n"
		  << "                                          geometry, painted with one stored\n"
		  << "                                          plane as vertex colours (height ao\n"
		  << "                                          blend colour groundcover waterheight\n"
		  << "                                          watertype cellflags cellrange\n"
		  << "                                          overview); prints what the plane\n"
		  << "                                          measured. Same generator as\n"
		  << "                                          File > Open\n"
		  << "  lodgen <file.esm> --list-worldspaces    LOD generation (rung 0): list\n"
		  << "  parsestress <a.nif> [--stress-file b.nif]... [--stress-threads N]\n"
		  << "              [--stress-reps N] [--stress-sabotage digest|share]\n"
		  << "                                          worldspaces in an ESM\n"
		  << "  lodgen <file.esm> --worldspace HEX [--cell X Y]\n"
		  << "                                          inspect a worldspace / one cell:\n"
		  << "                                          LAND heights, refs, LOD models\n"
		  << "  lodgen <file.esm> --worldspace HEX --terrain-region X0 Y0 X1 Y1\n"
		  << "         [--dim 4] --out-dir DIR [--atlas]\n"
		  << "                                          sweep: every chunk touching the\n"
		  << "                                          cell region, vanilla file naming.\n"
		  << "                                          Default: direct source-texture refs\n"
		  << "                                          (stock-legal; they ship in the\n"
		  << "                                          game's BA2s). --atlas packs the\n"
		  << "                                          BTOs' non-tiling textures onto one\n"
		  << "                                          <ws>.LodgenObjects sheet (+_n, _s)\n"
		  << "                                          under <tex-dir>/Objects, as a\n"
		  << "                                          draw-call optimization - own name,\n"
		  << "                                          never vanilla's (collision shadows\n"
		  << "                                          the vanilla sheet for vanilla BTOs)\n"
		  << "  lodgen <file.esm> --worldspace HEX --objects X Y [--dim 4]\n"
		  << "         [--data-root DIR] [--identity] [--no-identity] [--no-ao] [--arrays]\n"
		  << "         [--no-merge]                     (shapes merge per material after\n"
		  << "                                          the atlas; --no-merge keeps one\n"
		  << "                                          shape per source material)\n"
		  << "         [--cull-buried [--cull-margin N]] [--ao-grey] [--ao-skirt N]\n"
		  << "         -o OUT.bto                       --ao-skirt is how many cells\n"
		  << "                                          of neighbouring terrain and\n"
		  << "                                          objects the AO bake sees past\n"
		  << "                                          the chunk edge (default 1,\n"
		  << "                                          0 = chunk only, which seams)\n"
		  << "                                          rung 2: stitch one object chunk\n"
		  << "                                          from per-object _LOD meshes. The\n"
		  << "                                          .manifest.txt sidecar is ALWAYS\n"
		  << "                                          written beside the chunk; --identity\n"
		  << "                                          (DEFAULT OFF since 2026-09-12) adds\n"
		  << "                                          the FO4CS per-vertex channels to the\n"
		  << "                                          .BTO itself. Off, the .BTO carries\n"
		  << "                                          vanilla's vertex layout exactly\n"
		  << "         [--impostors DIR [--impostors-from-level N]]\n"
		  << "                                          a card library (bake_impostor_cards.sh)\n"
		  << "                                          stands in where a ring has no model;\n"
		  << "                                          from MNAM level N (0 = dim 4) on it\n"
		  << "                                          replaces the ring's mesh too (FO4CS)\n"
		  << "  lodgen <file.esm> --worldspace HEX --terrain-region X0 Y0 X1 Y1\n"
		  << "         --list-impostor-candidates [--candidates missing|trees]\n"
		  << "         --card-half-aux                 impostor normal, mask and emissive\n"
		  << "                                         sheets at half the base colour's side\n"
		  << "                                         (54% off a card; the silhouette is in\n"
		  << "                                         the base colour's alpha and never divides)\n"
		  << "                                          `formid model` per base, the model\n"
		  << "                                          being the base's own near mesh for\n"
		  << "                                          the card bake; missing = far slots\n"
		  << "                                          empty (default), trees = the tree set\n"
		  << "                                          and only it. `all` was RETIRED on\n"
		  << "                                          2026-09-11 and refuses by name\n"
		  << "  lodgen ... [--trees-only] [--no-trees-only]\n"
		  << "                                          which placements may stand on a card\n"
		  << "                                          during the bake: --trees-only (the\n"
		  << "                                          default, the panel's Trees only row)\n"
		  << "                                          is the tree set; --no-trees-only is\n"
		  << "                                          any base whose ring slot is EMPTY.\n"
		  << "                                          --impostors-from-level is tree-only\n"
		  << "                                          in both states\n"
		  << "  lodgen ... [--terrain-identity] [--no-terrain-identity]\n"
		  << "                                          material class, wetness, occlusion and\n"
		  << "                                          shore in the .BTR's vertex colours.\n"
		  << "                                          DEFAULT OFF since 2026-09-12: the\n"
		  << "                                          legacy .BTR carries vanilla's vertex\n"
		  << "                                          layout and the FO4CS data lives in the\n"
		  << "                                          .lod* files only\n"
		  << "  lodgen ... [--resource DIR|ARCHIVE]...    the resource stack, in Mod\n"
		  << "                                          Organizer's order: the LAST one\n"
		  << "                                          given overrides the earlier ones,\n"
		  << "                                          and a loose file beats an archive\n"
		  << "                                          wherever the archive sits\n"
		  << "  lodgen ... [--plugins-txt FILE] [--mo2]  take the plugin list (and, with\n"
		  << "                                          --mo2, the whole stack) from a\n"
		  << "                                          plugins.txt: '*Name.esp' lines are\n"
		  << "                                          the enabled plugins, in load order.\n"
		  << "                                          --mo2 reads the profile's own file\n"
		  << "                                          at %LOCALAPPDATA%\\Fallout4 and\n"
		  << "                                          stacks each enabled plugin's\n"
		  << "                                          archives in that order\n"
		  << "  lodgen [--mo2-profile DIR] [--mo2-mods DIR]  his MO2 load order read off\n"
		  << "                                          disk, MO2 not running: Fallout4.esm,\n"
		  << "                                          the DLC and CC masters in Data, then\n"
		  << "                                          plugins.txt's enabled plugins as full\n"
		  << "                                          paths (overwrite, the enabled mods\n"
		  << "                                          top-down, then Data); the stack Data,\n"
		  << "                                          then modlist.txt bottom-up, then\n"
		  << "                                          overwrite. Mods default to\n"
		  << "                                          <profile>/../../mods, Data to\n"
		  << "                                          --data-root or ModOrganizer.ini\n"
		  << "  lodgen [--resource ...] --probe RELPATH [--probe-out FILE]\n"
		  << "                                          where one asset actually resolves\n"
		  << "                                          from: the stack entry, loose or\n"
		  << "                                          archived, the size and a sha1\n"
		  << "                                          (exit 1 when nothing has it)\n"
		  << "  lodgen [--resource ...] --print-source   the stack as set (last wins) and\n"
		  << "                                          as indexed (first wins)\n"
		  << "  lodgen [--resource ...] --list-files N   up to N paths the stack's index\n"
		  << "                                          holds, sorted\n"
		  << "  lodgen <file.esm> --worldspace HEX --dump-layers FILE\n"
		  << "                                          which cells carry painted material\n"
		  << "                                          (BTXT base / ATXT layers) and which\n"
		  << "                                          LTEX they name, one line a cell.\n"
		  << "                                          This is what defines the playable\n"
		  << "                                          area, never a regenerated bake -\n"
		  << "                                          that would be circular.\n"
		  << "  lodgen --dump-shapes FILE.BTO            one line per shape: the shader\n"
		  << "                                          constants a LOD material is\n"
		  << "                                          composed from - Shader Flags 1,\n"
		  << "                                          Own-Emit, the emissive colour and\n"
		  << "                                          multiple, smoothness, specular\n"
		  << "                                          strength, the alpha property and\n"
		  << "                                          slot 0\n"
		  << "  lodgen --dump-geometry FILE.BTO          one line per shape: what it\n"
		  << "                                          weighs (vertices, triangles,\n"
		  << "                                          segments) and the counts its\n"
		  << "                                          own invariants stand on -\n"
		  << "                                          triangles in the wrong segment\n"
		  << "                                          cell, centroids outside the\n"
		  << "                                          chunk, vertices outside the\n"
		  << "                                          bounding sphere or the node\n"
		  << "                                          AABB - plus an `i` line with\n"
		  << "                                          its object identity indices\n"
		  << "  lodgen ... --terrain-region ... --native MODFOLDER\n"
		  << "                                          ALSO write the FO4CS-native far\n"
		  << "                                          field. MODFOLDER is a MOD FOLDER\n"
		  << "                                          (the mod's Data), as --vt and --lodl\n"
		  << "                                          already are: the pair lands at\n"
		  << "                                          <MODFOLDER>/FO4CSLOD/<ws>/<ws>.lodo\n"
		  << "                                          + .lodi, with every other FO4CS\n"
		  << "                                          output of this bake under the same\n"
		  << "                                          FO4CSLOD/<ws>/ folder\n"
		  << "                                          (docs/LODGEN_NATIVE_LODO_LODI.md);\n"
		  << "                                          the .BTO chunks are built in a\n"
		  << "                                          scratch folder and REMOVED after\n"
		  << "                                          the arrays, cards, merge and\n"
		  << "                                          far-ring cut have read them\n"
		  << "  lodgen ... --native <dir> --keep-bto     the way back: leave the .BTO chunks\n"
		  << "                                          in the output folder exactly as a\n"
		  << "                                          bake before 2026-09-16 did. The\n"
		  << "                                          manifest sidecar is written either\n"
		  << "                                          way; a bake with no --native is\n"
		  << "                                          the stock target and is untouched\n"
		  << "  lodgen ... --native <dir> --native-mesh-report <file>\n"
		  << "                                          ALSO write one line a library mesh:\n"
		  << "                                          triangles, vertices, the GPU cache\n"
		  << "                                          order before and after, and the\n"
		  << "                                          boundary-edge silhouette counts\n"
		  << "  lodgen --native-verify <ws.lodo> <ws.lodi> [--native-verify-corpus]\n"
		  << "                                          read a pair back, every check;\n"
		  << "                                          --native-verify-corpus re-reads the\n"
		  << "                                          plugin and refuses a STALE pair\n"
		  << "  lodgen --native-fixture <dir>           write the synthetic known-answer pair\n"
		  << "  archlock-probe <file.nif> --data-root <a;b;c> [--probe <tex>]\n"
		  << "                                          the archive-lock refuter: the file is\n"
		  << "                                          loaded through a BUFFER, so the\n"
		  << "                                          document has an empty data path and\n"
		  << "                                          every lookup falls through to the\n"
		  << "                                          shared index\n"
		  << "  lodgen --bake-record <ws.lodb> [<plugins>]\n"
		  << "                                          print the bake record beside a bake:\n"
		  << "                                          exe, date, the five staleness hashes,\n"
		  << "                                          plugins with sizes and FNV-1a 64,\n"
		  << "                                          resources, switches, one line a chunk\n"
		  << "                                          with its input digest, the census and\n"
		  << "                                          the end counts re-read from disk. With\n"
		  << "                                          a plugin list (positional, --plugins-txt\n"
		  << "                                          or --mo2) it also names every plugin\n"
		  << "                                          added, removed, reordered, resized or\n"
		  << "                                          edited since, and exits 1 if any moved\n"
	  << "  lodgen ... --native <dir> --native-ladder | --native-no-ladder\n"
	  << "                                          no-ladder (the default): one level a\n"
	  << "                                          mesh, the authored model as is,\n"
	  << "                                          nothing simplified. --native-ladder\n"
	  << "                                          builds the simplified cluster ladder\n"
	  << "  lodgen ... --native <dir> --library near|mnam\n"
	  << "                                          where the library's level 0 comes\n"
	  << "                                          from: the authored MNAM LOD slots\n"
	  << "                                          (mnam, the default) or the base's\n"
	  << "                                          near MODL (near)\n"
	  << "  lodgen ... --native <dir> --native-ladder-foliage\n"
	  << "                                          let alpha-tested foliage clusters\n"
	  << "                                          ladder; off by default because a\n"
	  << "                                          simplified leaf card is fragments\n"
	  << "  lodgen ... --native <dir> --native-silhouette <0..1>\n"
	  << "                                          the fraction of level 0's horizon\n"
	  << "                                          outline a level must keep or be\n"
	  << "                                          refused (default 0.70; 0 = no gate)\n"
	  << "  lodgen ... --native <dir> --native-no-placement-ao\n"
  << "  lodgen ... --native <dir> --native-no-vertex-ao\n"
	  << "  lodgen ... --native <dir> --lodi-v6\n"
	  << "  lodgen ... --native <dir> --lodi-v7\n"
	  << "  lodgen ... --native <dir> --scrappable  (off by default; .lodi v9 instance bit 6)\n"
	  << "  lodgen ... --native <dir> --identity-join-gap 64 | --identity-join legacy\n"
	  << "                                          v7 GROUPING: a non-tree placement joins\n"
	  << "                                          a group when its LOD MESH is within the\n"
	  << "                                          gap of another's (bungo 2026-09-19);\n"
	  << "                                          `legacy` is the old architecture-only\n"
	  << "                                          16-unit BOX rule, byte for byte\n"
	  << "                                          write no group table and no\n"
	  << "                                          per-vertex sky stream; the\n"
	  << "                                          .lodi stays at version 6,\n"
	  << "                                          byte for byte\n"
	  << "                                          write no per-instance placement-AO\n"
	  << "                                          byte; the .lodi stays at version 3\n"
	  << "                                          or 4, byte for byte\n"
	  << "  lodgen ... --native <dir> --native-no-occluders\n"
	  << "                                          write no occluder boxes; the\n"
	  << "                                          census says so in words\n"
	  << "  lodgen ... --native <dir> --impostors <cards> --aggregate\n"
	  << "                --aggregate-min 8 --aggregate-tile 64 --aggregate-views 8\n"
	  << "                                          AGGREGATE RING-3 IMPOSTORS: one card\n"
	  << "                                          set a FORESTED cell, composited from\n"
	  << "                                          that cell own trees cards at the\n"
	  << "                                          rotation and mirror the repetition\n"
	  << "                                          breaker gives them, photographed from\n"
	  << "                                          --aggregate-views azimuths at the\n"
	  << "                                          horizon. A cell is forested at\n"
	  << "                                          --aggregate-min tree placements. The\n"
	  << "                                          .lodi then carries one aggregate row a\n"
	  << "                                          cell and the list of instances it\n"
	  << "                                          stands for, and is written at VERSION\n"
	  << "                                          4. OFF is the exact way back: without\n"
	  << "                                          it the pair is version 3 and byte for\n"
	  << "                                          byte what it always was\n"
		  << "  lodgen ... --terrain-region ... [--chunk-threads N]\n"
		  << "                                          how many chunks the queue builds\n"
		  << "                                          at once. DEFAULT 1, and it stays 1\n"
		  << "                                          because of the MEMORY. Measured on\n"
		  << "                                          this machine, lane PERF1 2026-09-17,\n"
		  << "                                          8 threads against 1: a 9-chunk FO4CS\n"
		  << "                                          region 2.29 -> 9.32 GB of peak\n"
		  << "                                          working set for 49.3 s -> 41.6 s; a\n"
		  << "                                          16-chunk one 2.55 -> 10.48 GB for\n"
		  << "                                          59.4 s -> 46.9 s. So about a fifth of\n"
		  << "                                          the wall clock for about four times\n"
		  << "                                          the memory. It is CLEAN: 20 of 20\n"
		  << "                                          consecutive runs a region, 0 faults,\n"
		  << "                                          every file byte-identical to the\n"
		  << "                                          1-thread bake (57 and 99). This\n"
		  << "                                          supersedes the older 2.3x-slower\n"
		  << "                                          reading, taken before the library was\n"
		  << "                                          parallel; an earlier lane measured\n"
		  << "                                          25 GB at 16 threads on 25 chunks, so\n"
		  << "                                          the ceiling here is memory, not cores\n"
		  << "  lodgen ... --terrain-region ... [--land-sample MODE]\n"
		  << "                                          how the land texture is read\n"
		  << "                                          inside one repeat. footprint\n"
		  << "                                          (DEFAULT base rule) = one texel of\n"
		  << "                                          the matching mip; average = its mean\n"
		  << "                                          over a whole repeat; stochastic = the\n"
		  << "                                          hex tiling (256 units, bias -0.22)\n"
		  << "                                          with the warp amplitude forced to 0;\n"
		  << "                                          warp = lane TILING3's domain warp,\n"
		  << "                                          kept for the record\n"
		  << "  lodgen ... --terrain-region ... [--blend-edges off|quadrant]\n"
		  << "                                          the 2,048-unit quadrant lines of the\n"
		  << "                                          land colour. DEFAULT quadrant since\n"
		  << "                                          2026-09-23 (bungo): cross-faded over\n"
		  << "                                          --blend-margin units (default 128)\n"
		  << "                                          either side; colour sheets only.\n"
		  << "                                          off = hard lines, the exact way back\n"
		  << "  lodgen ... --terrain-region ... [--land-hex UNITS]\n"
		  << "                                          the hex tile size on its own.\n"
		  << "                                          DEFAULT 256 since 2026-09-12 (bungo's\n"
		  << "                                          pick, panel (c) of a_land_guide_*.png).\n"
		  << "                                          --land-hex 0 --land-warp 0\n"
		  << "                                          --land-mip-bias 0 --land-guide off is\n"
		  << "                                          the exact way back to the bake before\n"
		  << "                                          that ruling, byte for byte\n"
		  << "  lodgen ... --terrain-region ... [--land-guide RULE[:K]]\n"
		  << "                                 [--land-guide-scale UNITS] [--land-guide-slope TAN]\n"
		  << "                                          terrain-guided land sampling (lane\n"
		  << "                                          LAND1): the land texture's phase is\n"
		  << "                                          steered by the HEIGHTMAP's own low-pass\n"
		  << "                                          slope at --land-guide-scale (128..2048,\n"
		  << "                                          default 1024). RULE is off, drag,\n"
		  << "                                          aspect, aspecthex, slopewarp or flatwarp.\n"
		  << "                                          DEFAULT flatwarp:1.0 with --land-warp 341\n"
		  << "                                          since 2026-09-12 (bungo's pick); off is\n"
		  << "                                          part of the way back above.\n"
		  << "                                          K is world units for drag, a 0..1\n"
		  << "                                          fraction for the two aspects, and a\n"
		  << "                                          multiplier on --land-warp for the warps.\n"
		  << "  lodgen ... --terrain-region ... [--incremental OUT-DIR]\n"
		  << "                                          rebake only the chunks whose INPUTS\n"
		  << "                                          changed since the bake that wrote\n"
		  << "                                          OUT-DIR/<Worldspace>.lodb, plus every\n"
		  << "                                          chunk within one cell of one. Every\n"
		  << "                                          region bake writes that ledger, so the\n"
		  << "                                          first incremental run needs only a\n"
		  << "                                          previous ordinary one. REFUSES rather\n"
		  << "                                          than silently full-baking when there is\n"
		  << "                                          no ledger, when the region or the\n"
		  << "                                          switches differ, or when --atlas,\n"
		  << "                                          --arrays, the merge or --impostors are\n"
		  << "                                          asked for -- those four consume the\n"
		  << "                                          written .BTO list in order and a\n"
		  << "                                          filtered list would corrupt them.\n"
		  << "                                          Under --native it also KEEPS the\n"
		  << "                                          previous .lodo when the base census,\n"
		  << "                                          the load order, the plugin corpus and\n"
		  << "                                          the object corpus are all unmoved and\n"
		  << "                                          the file reads back whole: the census\n"
		  << "                                          line native-library-build: says reused or\n"
		  << "                                          rebuilt (why). Occluders being ON\n"
		  << "                                          always rebuilds (lane PERF1)\n"
		  << "                                          docs/LODGEN_LEDGER_FORMAT.md\n"
		  << "  lodgen ... --incremental ... [--no-native-cache]\n"
		  << "                                          do not write the per-chunk\n"
		  << "                                          .lodj cache the FO4CS target\n"
		  << "                                          needs. A full bake still\n"
		  << "                                          writes its LOD; an\n"
		  << "                                          --incremental --native run\n"
		  << "                                          then REFUSES, because a\n"
		  << "                                          skipped chunk has nothing to\n"
		  << "                                          replay into the .lodo/.lodi\n"
		  << "                                          pair and would be silently\n"
		  << "                                          missing from it. Only for\n"
		  << "                                          measuring what the cache\n"
		  << "                                          costs.\n"
		  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]\n"
		  << "                                          world units one repeat of a\n"
		  << "                                          landscape texture covers.\n"
		  << "                                          DEFAULT 341.333 = 128/0.375,\n"
		  << "                                          the engine's own tiling;\n"
		  << "                                          2048 is the exact way back\n"
		  << "  lodgen ... --terrain-region ... [--threads N]\n"
		  << "                                          how many cores the chunk queue,\n"
		  << "                                          the tile bakes, the BC encoders and\n"
		  << "                                          the object library (model load, cap 4;\n"
		  << "                                          mesh ladder, uncapped) may use.\n"
		  << "                                          0 or absent = the machine; measured\n"
		  << "                                          82.0 s -> 49.3 s on a 9-chunk FO4CS\n"
		  << "                                          region and 91.5 s -> 59.4 s on a\n"
		  << "                                          16-chunk one, lane PERF1 2026-09-17;\n"
		  << "                                          1 is the EXACT way back (one world,\n"
		  << "                                          one cache set, one chunk at a time)\n"
		  << "                                          and every output file is\n"
		  << "                                          byte-identical either way\n"
		  << "  lodgen ... --terrain-region ... [--slot-fallback]\n"
		  << "                                          keep an object at a ring whose\n"
		  << "                                          MNAM slot is empty by using the\n"
		  << "                                          nearest filled one. OFF matches\n"
		  << "                                          vanilla, where it drops out;\n"
		  << "                                          measured, 0 of 19,507 refs in\n"
		  << "                                          chunk (-32,16) fill slot 2, so\n"
		  << "                                          Sanctuary's ring 2 is empty\n"
		  << "                                          without this or --impostors\n"
		  << "  lodgen ... --terrain-region ... [--atlas [--atlas-bc1]]\n"
		  << "                                          --atlas-bc1 writes the diffuse\n"
		  << "                                          sheet as DXT1 with one-bit alpha,\n"
		  << "                                          which is what vanilla's own sheet\n"
		  << "                                          is (measured) and half the\n"
		  << "                                          memory; BC3 is the default and\n"
		  << "                                          keeps eight-bit alpha for FO4CS\n"
		  << "  lodgen ... --terrain-region ... [--no-simplify]\n"
		  << "         [--simplify8 R] [--simplify16 R] [--simplify32 R]\n"
		  << "         [--simplify-error UNITS]         far-ring proxies: after the\n"
		  << "                                          merge, each ring's merged shapes\n"
		  << "                                          keep R of their triangles\n"
		  << "                                          (default 1 / 0.35 / 0.2 at dim\n"
		  << "                                          8 / 16 / 32; ring 0 is never\n"
		  << "                                          touched). Alpha-tested shapes\n"
		  << "                                          and impostor cards keep every\n"
		  << "                                          triangle; the error is world\n"
		  << "                                          units at ring 0, scaled by the\n"
		  << "                                          ring's dim (default 32)\n"
		  << "  lodgen ... --terrain-region ... --tex-dir DIR\n"
		  << "                                          bake the terrain sheets for every\n"
		  << "                                          chunk in the region into DIR:\n"
		  << "                                          <ws>.<dim>.<x>.<y>.DDS (albedo),\n"
		  << "                                          _msn.DDS (model-space normal) and\n"
		  << "                                          _data.DDS (R sky-free AO, G flow\n"
		  << "                                          wetness, B shore proximity)\n"
		  << "  lodgen ... --tex-dir DIR [--cover] [--no-cover] [--grass-tint F]\n"
		  << "         [--cover-full N] [--dump-cover FILE]\n"
		  << "                                          --cover reads the landscape\n"
		  << "                                          textures' grass records (LTEX ->\n"
		  << "                                          GNAM -> GRAS), composites a cover\n"
		  << "                                          value against the splat paint,\n"
		  << "                                          gates it on slope, and writes it\n"
		  << "                                          into the data sheet's alpha - the\n"
		  << "                                          sheet turns BC3 and gains a 'WWCV'\n"
		  << "                                          stamp only when the chunk has any.\n"
		  << "                                          --grass-tint F (default 0.35) is\n"
		  << "                                          how far the far albedo moves toward\n"
		  << "                                          the grass colour at full cover; 0\n"
		  << "                                          leaves the albedo byte-identical\n"
		  << "                                          and still writes the plane.\n"
		  << "                                          --cover-full N (default 96, the\n"
		  << "                                          largest Density in the shipped\n"
		  << "                                          corpus) is the FIXED normalisation\n"
		  << "                                          the byte is measured against; a\n"
		  << "                                          run-derived one would make two\n"
		  << "                                          bakes incomparable. --dump-cover\n"
		  << "                                          also writes the raw 512^2 u8 plane,\n"
		  << "                                          north-up and headerless.\n"
		  << "                                          Off, the three sheets are byte for\n"
		  << "                                          byte what they have always been.\n"
		  << "  lodgen ... [--roads] [--no-roads] [--road-cover-suppress F]\n"
		  << "                                          --roads (ON by default, both\n"
		  << "                                          targets) rasterises the placed\n"
		  << "                                          road meshes top-down into the far\n"
		  << "                                          terrain COLOUR sheet, the way\n"
		  << "                                          vanilla does: a STAT whose model\n"
		  << "                                          sits under Landscape/Roads or\n"
		  << "                                          Landscape/Sidewalks, its own\n"
		  << "                                          material diffuse, the topmost\n"
		  << "                                          triangle winning, alpha-tested\n"
		  << "                                          shapes honouring their cut-out.\n"
		  << "                                          The NORMAL sheet is not touched:\n"
		  << "                                          vanilla does not put the road in\n"
		  << "                                          it (measured). Ground cover under\n"
		  << "                                          a road is scaled by\n"
		  << "                                          --road-cover-suppress F (default 1\n"
		  << "                                          = no grass under the road, 0 =\n"
		  << "                                          leave the cover plane alone).\n"
		  << "                                          --no-roads is byte-identical to\n"
		  << "                                          the bake before roads existed.\n"
		  << "  lodgen ... [--road-opacity 0..1]\n"
		  << "                                          --road-opacity A (default 1)\n"
		  << "                                          scales how strongly the road\n"
		  << "                                          paint is mixed into the ground\n"
		  << "                                          under it: 1 is the bake as it\n"
		  << "                                          has always been and the multiply\n"
		  << "                                          is branched over, 0 paints\n"
		  << "                                          nothing while the road still\n"
		  << "                                          suppresses the ground cover\n"
		  << "                                          under it. Vanilla's far road\n"
		  << "                                          stands +4.29 and +4.40 levels\n"
		  << "                                          over its surround on two tiles;\n"
		  << "                                          ours stands +29.96 and +3.84, so\n"
		  << "                                          the error is not the same on\n"
		  << "                                          every chunk and no one value\n"
		  << "                                          fixes both (lane ROADS3).\n"
		  << "  lodgen ... [--road-composite max-z|blend] [--road-detail 0..1]\n"
		  << "             [--road-ground-paint 0..1]\n"
		  << "             [--road-raised] [--no-road-raised]\n"
		  << "             [--road-sidewalks] [--no-road-sidewalks] [--roads-legacy]\n"
		  << "                                          --road-detail lerps the diffuse\n"
		  << "                                          sample toward the texture's own\n"
		  << "                                          average: 1 (the default) prints the\n"
		  << "                                          footprint sample; 0 is one flat\n"
		  << "                                          colour a material, which is what\n"
		  << "                                          vanilla's far road measures as but\n"
		  << "                                          is NOT the default -- bungo ruled\n"
		  << "                                          on 2026-09-12, over a picture of\n"
		  << "                                          both, that 0 looks terrible and is\n"
		  << "                                          never to be used. 1 does band the\n"
		  << "                                          road at its 256-unit UV repeat.\n"
		  << "                                          --road-ground-paint is how much\n"
		  << "                                          a shape INSIDE a road model whose\n"
		  << "                                          material lives under\n"
		  << "                                          materials/Landscape/Ground/ paints\n"
		  << "                                          the sheet -- the verge, modelled\n"
		  << "                                          and materialled as terrain. Such\n"
		  << "                                          shapes win 36.1 per cent of the\n"
		  << "                                          road plane on chunk (-20,20) and\n"
		  << "                                          24.9 on (-8,8), and the step where\n"
		  << "                                          they meet the asphalt reads 15.387\n"
		  << "                                          and 8.010 against vanilla 5.362\n"
		  << "                                          and 5.138 (lane ROADS4). 0 -- THE\n"
		  << "                                          DEFAULT since 2026-09-12, bungo's\n"
		  << "                                          16:55 ruling that the grass meshes\n"
		  << "                                          in the road nifs are excluded --\n"
		  << "                                          leaves the landscape colour there\n"
		  << "                                          and also stops that shape\n"
		  << "                                          suppressing ground cover, since the\n"
		  << "                                          multiply is on coverage;\n"
		  << "                                          --road-ground-paint 1 is the way\n"
		  << "                                          back and is ROADS1s bake.\n"
		  << "                                          --road-composite max-z (the\n"
		  << "                                          default) lets the topmost\n"
		  << "                                          triangle win the texel; blend\n"
		  << "                                          paints the pieces in order --\n"
		  << "                                          ascending mean world Z, non-decal\n"
		  << "                                          before decal -- and composites\n"
		  << "                                          dst = lerp(dst, src, srcAlpha).\n"
		  << "                                          max-z is the default because it\n"
		  << "                                          scores 0.3404 on the road metric\n"
		  << "                                          of tests/spells and blend 0.2669,\n"
		  << "                                          against bars 0.2694 and 0.3228.\n"
		  << "                                          --no-road-raised (the default)\n"
		  << "                                          refuses a road base that carries\n"
		  << "                                          its own Distant LOD mesh and\n"
		  << "                                          anything under\n"
		  << "                                          Landscape/Roads/HighwayOverpass or\n"
		  << "                                          .../Bridge, because those are\n"
		  << "                                          drawn as objects at distance.\n"
		  << "                                          --no-road-sidewalks (the default)\n"
		  << "                                          keeps Landscape/Sidewalks out of\n"
		  << "                                          the paint: on chunk (-8,8) ours\n"
		  << "                                          read 128.4 mean luminance there\n"
		  << "                                          against vanilla's 86.5, while the\n"
		  << "                                          flat road matched vanilla to\n"
		  << "                                          0.001 of its own floor clearance.\n"
		  << "                                          --roads-legacy IS THE WAY BACK,\n"
		  << "                                          one token, and means all four:\n"
		  << "                                          max-z, full detail, the raised\n"
		  << "                                          families and the sidewalks, so\n"
		  << "                                          such a bake is byte-identical to\n"
		  << "                                          the bake before this existed.\n"
		  << "  lodgen ... [--terrain-object-ao]\n"
		  << "             [--terrain-object-ao-strength 0..4, default 0.5]\n"
		  << "             [--terrain-object-ao-slab 0|1, default 1; 0 = the old\n"
		  << "              max-Z reading, where a deck blocks from the ground up]\n"
		  << "             [--dump-object-ao FILE]\n"
		  << "                                          the far terrain receives ambient\n"
		  << "                                          occlusion from the PLACED OBJECTS,\n"
		  << "                                          not only from its own horizon: the\n"
		  << "                                          same eight-direction march, run a\n"
		  << "                                          second time over a 128-unit max-Z\n"
		  << "                                          field of the level-0 LOD meshes, and\n"
		  << "                                          multiplied in as a second visibility\n"
		  << "                                          fraction. Reach 1458 units (the\n"
		  << "                                          march's longest step). OFF by\n"
		  << "                                          default and off is the previous\n"
		  << "                                          bake's BYTES -- the term is exactly\n"
		  << "                                          1.0 where nothing is in reach. A\n"
		  << "                                          base with no distant-LOD mesh is not\n"
		  << "                                          drawn at distance, so it does not\n"
		  << "                                          shadow at distance: it is refused by\n"
		  << "                                          name into the census. Refused in\n"
		  << "                                          combination with --lodl, because the\n"
		  << "                                          .lodl AO plane and --refresh-ao\n"
		  << "                                          share one function over the stored\n"
		  << "                                          heights alone.\n"
		  << "                                          The strength default 0.5 is the\n"
		  << "                                          largest sampled value at which the\n"
		  << "                                          AO byte never clamps to 0 on the\n"
		  << "                                          measured region: at 1.0, 276,234 of\n"
		  << "                                          1,048,576 texels there go flat to\n"
		  << "                                          zero and stop carrying occlusion.\n"
		  << "                                          One region, and a forested one.\n"
		  << "  lodgen ... [--erosion 0..8, default 0] [--erosion-iterations 1..16]\n"
		  << "             [--erosion-seed N] [--land-detail-source erosion]\n"
		  << "                                          a hydraulic erosion pass at BAKE\n"
		  << "                                          resolution: droplets traced over the\n"
		  << "                                          sheet's own height lattice cut and\n"
		  << "                                          fill a height delta, and that delta\n"
		  << "                                          reaches the normal sheet as an added\n"
		  << "                                          gradient and the colour as the same\n"
		  << "                                          crevice shading vanilla's residual\n"
		  << "                                          fitted (-3.242 levels). It does NOT\n"
		  << "                                          tint by material: TILING3 measured an\n"
		  << "                                          R-squared ceiling of 0.018-0.023 on\n"
		  << "                                          any per-texel law from the fine\n"
		  << "                                          normal to vanilla's fine colour, so a\n"
		  << "                                          palette would be a taste, not a\n"
		  << "                                          measurement. 0 is the default, builds\n"
		  << "                                          no lattice and is the previous bake's\n"
		  << "                                          BYTES. Deterministic by construction:\n"
		  << "                                          paths traced on a STATIC field, start\n"
		  << "                                          positions seeded from world position,\n"
		  << "                                          droplets summed in world order, and a\n"
		  << "                                          32-cell border, so a droplet that\n"
		  << "                                          never reaches a node never changes it.\n"
		  << "                                          On Commonwealth pass\n"
		  << "                                          --land-detail-source erosion as well,\n"
		  << "                                          or the default (vanilla) copies\n"
		  << "                                          vanilla's _msn over the pass.\n"
		  << "  lodgen ... [--sheet-format vanilla|legacy] [--msn-cache DIR|auto]\n"
		  << "                                          --sheet-format vanilla writes the far\n"
		  << "                                          terrain sheets the way every one of\n"
		  << "                                          the 6,120 shipped Commonwealth sheets\n"
		  << "                                          is written: DXT5, mips all the way to\n"
		  << "                                          1x1 (10 at 512), alpha a constant 255\n"
		  << "                                          (measured: one distinct alpha value\n"
		  << "                                          over 13.1M texels per family). legacy\n"
		  << "                                          is the default and is the previous\n"
		  << "                                          bake's BYTES. A sheet copied from\n"
		  << "                                          vanilla is never re-encoded either\n"
		  << "                                          way. --msn-cache names a directory of\n"
		  << "                                          cleaned <ws>.<dim>.<x>.<y>.png normal\n"
		  << "                                          sheets; each one found replaces that\n"
		  << "                                          chunk's _msn, written UNCOMPRESSED\n"
		  << "                                          with a full mip chain (a BC re-encode\n"
		  << "                                          puts the block grid back). Its R is\n"
		  << "                                          east and its G is north; up is\n"
		  << "                                          recomputed and the normal is\n"
		  << "                                          renormalised. A <name>_msn.DDS in the\n"
		  << "                                          same directory wins over the .png:\n"
		  << "                                          uncompressed R8G8B8A8 (DX10 header,\n"
		  << "                                          one mip) in vanilla's order, R east,\n"
		  << "                                          G up, B north, and its G is USED, not\n"
		  << "                                          recomputed. Anything else is refused\n"
		  << "                                          with a reason on stderr. Empty is the\n"
		  << "                                          default and reads no directory.\n"
		  << "  lodgen <file.esm> --worldspace HEX --vt MODFOLDER [--tex-dir DIR]\n"
		  << "         [--vt-finest 2] [--vt-content 256] [--vt-border 8] [--vt-mips 2]\n"
		  << "         [--vt-density 32|16|8]           the finest level's texel size in\n"
		  << "                                          world units, as one word: 32 =\n"
		  << "                                          --vt-finest 2 --vt-content 256 (the\n"
		  << "                                          CLI default, ~1.7 GB for the whole\n"
		  << "                                          Commonwealth), 16 = finest 2 content\n"
		  << "                                          512 (the panel's default, ~6.4 GB),\n"
		  << "                                          8 = finest 1 content 512 (the upscaled\n"
		  << "                                          normal sheets' own density, ~26 GB).\n"
		  << "                                          Refused beside --vt-finest/--vt-content\n"
		  << "         [--vt-half-aux]                  normal, mask, height and emissive\n"
		  << "                                          tiles at HALF the texels a side (their\n"
		  << "                                          top mip is not stored; the header's\n"
		  << "                                          sheet descriptor byte 6 says so); the\n"
		  << "                                          colour keeps the full density. OFF by\n"
		  << "                                          default; needs --vt-mips 2 or more.\n"
		  << "                                          With --msn-cache set, the pyramid's\n"
		  << "                                          NORMAL is that folder's sheets, box-\n"
		  << "                                          filtered as vectors to each level;\n"
		  << "                                          a chunk with no sheet keeps the\n"
		  << "                                          heights normal. DIR may be the\n"
		  << "                                          sheets' own folder or a mod / Data\n"
		  << "                                          folder holding them under\n"
		  << "                                          Textures/Terrain/<world>/. auto: the\n"
		  << "                                          last --resource folder with sheets\n"
		  << "                                          wider than vanilla's 512, or none.\n"
		  << "         [--vt-compress none|zlib] [--vt-btr] [--no-vt-btr] [--vt-estimate]\n"
		  << "         [--vt-cover-in-color]            put the ground-cover byte in the\n"
		  << "                                          COLOUR sheet alpha (the object\n"
		  << "                                          family coverage slot) instead of the\n"
		  << "                                          mask alpha. OFF: the colour alpha is\n"
		  << "                                          the one slot .lodm 2.1 defines as\n"
		  << "                                          OPACITY, so a consumer that\n"
		  << "                                          alpha-tests it would punch holes in\n"
		  << "                                          thin grass; and it costs 46,240 bytes\n"
		  << "                                          a tile on every cover-FREE tile.\n"
		  << "         [--vt-height]                    a fourth R16 height sheet per tile,\n"
		  << "                                          OFF by default: uncompressed where the\n"
		  << "                                          other three are BC1, so +133% on a tile,\n"
		  << "                                          and for a Fallout 4 source it is\n"
		  << "                                          interpolation - the worldspace heightmap\n"
		  << "                                          already carries every real height in\n"
		  << "                                          75.5 MB. Worth it for a source finer than\n"
		  << "                                          FO4's 32 land samples a cell\n"
		  << "                                          the terrain virtual texture: a\n"
		  << "                                          pyramid of 256-texel tiles with an\n"
		  << "                                          8-texel border, one .lodt per level\n"
		  << "                                          under <MODFOLDER>/FO4CSLOD/<ws>/,\n"
		  << "                                          indexed\n"
		  << "                                          by <ws>.VT.lodm. Four sheets a tile:\n"
		  << "                                          colour, model-space normal, data and\n"
		  << "                                          HEIGHT (R16, the shadow heightmap's\n"
		  << "                                          own encoding). Coarser levels are box\n"
		  << "                                          filters of the finer ones, aligned to\n"
		  << "                                          one origin so a coarse tile covers\n"
		  << "                                          exactly four fine ones. --vt-btr (on)\n"
		  << "                                          ASSEMBLES the .btr chunk sheets from\n"
		  << "                                          the pyramid instead of baking them\n"
		  << "                                          again; --vt-estimate prints the cost\n"
		  << "                                          and exits without baking. With\n"
		  << "                                          --terrain-region the pyramid covers\n"
		  << "                                          that rectangle only and its index\n"
		  << "                                          says so (partial: true).\n"
		  << "  lodgen <file.esm> --worldspace HEX --corpus-hash\n"
		  << "                                          both corpus hashes and the LTEX/GRAS\n"
		  << "                                          census, with no bake: what a harness\n"
		  << "                                          pins its floors to\n"
		  << "  lodgen --lodm-check FILE.lodm            parse a .lodm through this tree's\n"
		  << "                                          own parser and print keyword lines\n"
		  << "  lodgen --lodt-check FILE.lodt            validate a terrain texture level by\n"
		  << "                                          every rule of\n"
		  << "                                          docs/LODGEN_TERRAIN_VT.md, checking\n"
		  << "                                          every tile's CRC; a refusal NAMES the\n"
		  << "                                          field that failed\n"
		  << "  lodgen <file.esm> --worldspace HEX --terrain X Y [--dim 4] -o OUT.btr\n"
		  << "                                          rung 1: generate one terrain chunk\n"
		  << "                                          (X,Y = SW cell, dim-aligned) in the\n"
		  << "                                          vanilla .btr anatomy, textures\n"
		  << "                                          pointing at the game's own bakes\n"
		  << "  loading-screen <file> [--no-zoom-target] [--keep-particles]\n"
		  << "                       [--keep-effects] -o OUT\n"
		  << "                                          bake the file AS IT IS POSED into\n"
		  << "                                          loading-screen art: skins evaluated\n"
		  << "                                          away, skeleton dropped, each shape\n"
		  << "                                          re-centred on its own origin.\n"
		  << "                                          --keep-effects leaves ArtObject\n"
		  << "                                          branches running instead, on a stub\n"
		  << "                                          of the bone they hung from\n\n"
		  << "Field paths are '/'-separated; numeric segments index arrays by row,\n"
		  << "e.g. -f \"Vertex Data/0/Vertex Colors\".\n\n"
		  << "Scope: spells and model edits only. Viewport modelling tools (extrude,\n"
		  << "loop cut, join...) live on GLView and need a GL context, so they are not\n"
		  << "reachable from here.\n";
	return 0;
}

/*! `archlock-probe <file.nif> --data-root <Data folder|.ba2> [--probe <texture>]`
 *  -- lane ARCHLOCK1, 2026-09-17. The CLI half of the archive-lock refuter.
 *
 *  It reproduces, with no window, the two halves of the condition the deadlock
 *  needed, and it FORCES both rather than hoping for them:
 *
 *    1. a document whose own resource set has an EMPTY data path. The bytes are
 *       read here and handed to NifModel::load through a QBuffer, exactly as
 *       the Files tab's configured-resource row does, so the model never sees a
 *       file name and getNIFDataPath gives it nothing. Opening the same file BY
 *       PATH does not reproduce this: the data path is then derived from the
 *       name and the document builds the shared index from its own
 *       init_archives(), with no lock held.
 *    2. a shared index that is NOT BUILT YET -- close_archives() first, then the
 *       pointer is printed, because the whole probe is meaningless if something
 *       already warmed it.
 *
 *  On the build before the fix this hangs forever at the first lookup, on this
 *  thread, with no CPU: get_file took the READ lock, missed, and recursed into
 *  the parent's init_archives(), whose first line takes the WRITE lock on the
 *  same QReadWriteLock. -no-gui never brings the game manager up (see
 *  initModelLayer), so the folders are set here explicitly; without them the
 *  parent's dataPaths are empty, init_archives is never reached and the bug
 *  cannot be seen from a CLI run at all.
 *
 *  Keyword lines, one fact a line, like the rest of this file's probes. */
int cmdArchLockProbe( const QString & nifPath, const QString & dataRoot, const QString & texture )
{
	if ( nifPath.isEmpty() || dataRoot.isEmpty() ) {
		err() << "archlock-probe: needs <file.nif> and --data-root <Data folder or .ba2>" << Qt::endl;
		return 2;
	}
	Game::GameManager::update_other_games_fallback( false );
	/* A ;-SEPARATED LIST, not one folder. The mesh, its material and its
	 * texture live in three different .ba2 files, and a probe given only one
	 * of them would report `not-found` for a lookup that is working
	 * perfectly -- which is the same red as the bug. */
	const QStringList roots =
		dataRoot.split( QChar( ';' ), Qt::SkipEmptyParts );
	Game::GameManager::update_folders( Game::FALLOUT_4, roots );
	Game::GameManager::update_status( Game::FALLOUT_4, true );
	Game::GameManager::GameResources & parent =
		Game::GameManager::getGameResources( Game::FALLOUT_4 );
	parent.close_archives();
	out() << "archlock-probe roots " << roots.size() << Qt::endl;
	for ( const QString & rt : roots )
		out() << "archlock-probe dataRoot " << rt << Qt::endl;
	out() << "archlock-probe sharedIndexBuilt " << ( parent.ba2File ? 1 : 0 ) << Qt::endl;

	QFile f( nifPath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		err() << "archlock-probe: cannot read " << nifPath << Qt::endl;
		return 2;
	}
	QByteArray raw = f.readAll();
	f.close();
	QBuffer buf( &raw );
	if ( !buf.open( QIODevice::ReadOnly ) )
		return 2;
	NifModel nif;
	const bool loaded = nif.load( buf );
	buf.close();
	out() << "archlock-probe loaded " << ( loaded ? 1 : 0 ) << Qt::endl;
	out() << "archlock-probe blocks " << nif.getBlockCount() << Qt::endl;
	Game::GameManager::GameResources & r = nif.getGameResources();
	out() << "archlock-probe documentDataPaths " << r.dataPaths.size() << Qt::endl;
	out() << "archlock-probe fallsThroughToShared "
		<< ( r.parent == &parent ? 1 : 0 ) << Qt::endl;

	// the first material the file names, which is what the renderer asks for
	QString mat;
	for ( int i = 0; i < nif.getBlockCount() && mat.isEmpty(); i++ ) {
		const QString n = nif.get<QString>( nif.getBlockIndex( i ), "Name" );
		if ( n.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			|| n.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) )
			mat = n;
	}
	out() << "archlock-probe material " << ( mat.isEmpty() ? QStringLiteral( "n/a" ) : mat ) << Qt::endl;
	int fails = 0;
	if ( mat.isEmpty() ) {
		// a NIF with no material names nothing to look up: say so, do not pass
		out() << "archlock-probe getFile REFUSED (the file names no material)" << Qt::endl;
		fails++;
	} else {
		QByteArray data;
		const bool got = nif.getResourceFile( data, mat, "materials", "" );
		out() << "archlock-probe getFile " << ( got ? "found" : "not-found" )
			<< " " << data.size() << " byte(s)" << Qt::endl;
		if ( !got || data.isEmpty() )
			fails++;
	}
	const QString wanted = texture.isEmpty() ? mat : texture;
	if ( !wanted.isEmpty() ) {
		const bool isTex = !texture.isEmpty();
		const QString where = nif.findResourceFile( wanted,
			isTex ? "textures" : "materials", isTex ? ".dds" : "" );
		out() << "archlock-probe findFile " << ( where.isEmpty() ? "not-found" : "found" )
			<< " " << ( where.isEmpty() ? wanted : where ) << Qt::endl;
		if ( where.isEmpty() )
			fails++;
	}
	out() << "archlock-probe sharedIndexBuiltNow " << ( parent.ba2File ? 1 : 0 ) << Qt::endl;
	out() << "archlock-probe verdict " << ( fails == 0 ? "answered" : "refused" ) << Qt::endl;
	return fails == 0 ? 0 : 1;
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
	// `nifx` reads and writes a sidecar, never a NIF: its own arguments, no
	// model layer (lane PBRR1, gate d).
	if ( cmd == QLatin1String( "nifx" ) ) {
		const int rc = cmdNifx( a );
		out().flush();
		err().flush();
		return rc;
	}
	// `weather` reads WTHR/CLMT records from a plugin load list: no NIF, no model
	// layer (lane PBRR2B, the W1 gates; src/esmweather.cpp)
	if ( cmd == QLatin1String( "weather" ) ) {
		out().flush();
		const int rc = cmdWeather( a );
		err().flush();
		return rc;
	}

	// options
	QString file, path, value, outFile, spellId, type, pattern, sequence;
	QStringList controllers, adds, addAttach;
	QString pendingAttach;
	QString saveName, applyName, importOs, exportOs;
	QString gltfClip, gltfBones;			// lane HKX4
	bool gltfRootMotion = false;			// lane HKX4
	GltfExportOptions gltfOpts;			// lane GLTFEXPORT1: one struct, two front ends
	bool gltfUsedNext = false;
	QString gltfFlagError;
	QString gltfRoute, gltfTsv, gltfSkeletonName;	// lane BUILD8
	QString gltfRootNode;						// lane BUILD8
	float gltfFps = 0.0f;						// lane BUILD8
	bool gltfSourceRate = false;				// lane BUILD8
	float blend = 1.0f;
	float freezeTime = 0.0f;
	bool keepGraph = false;
	bool noZoomTarget = false, keepParticles = false, keepEffects = false;
	QVector<int> tnFrom;			// --from repeats: several sources are one surface
	int tnTo = -1, tnMapping = 4;	// 4 = Nearest Face Interpolated
	float tnMix = 1.0f;
	int steps = 0;
	int substeps = 0;
	int iterations = 0;
	QString onlyLimit;
	bool useGround = false, noSelf = false, drop = false, jointedOnly = false;
	int dragBody = -1;
	bool dragSpring = false;
	float dragFirmness = 0.9f;
	bool noLimits = false;
	bool verboseSim = false;
	float cubeSize = STARTER_CUBE_SIZE;
	bool wantCube = false;
	bool noDedupe = false;
	int block = -1, depth = 2, maxRows = 40;
	int effectVar = -1, intVar = -1;
	bool showAll = false, newSequence = false, standalone = false, listOnly = false;
	bool validateOnly = false;
	bool selfTest = false;
	bool extract = false;
	bool roundTrip = false;
	bool btdInfo = false;
	bool btdHaveRegion = false;
	int btdRegion[4] = { 0, 0, 0, 0 };
	int btdLod = -1;
	QString lodtPlaneName;
	bool lodtWaterCensusOnly = false;
	bool lodtWaterSelfTestOnly = false;
	bool lodtWaterMarkSelfTestOnly = false;
	bool lgListWorldspaces = false;
	quint32 lgWorldspace = 0;
	bool lgHaveCell = false;
	int lgCell[2] = { 0, 0 };
	bool lgHaveTerrain = false;
	int lgChunk[2] = { 0, 0 };
	int lgDim = 4;
	bool lgHaveRegion = false;
	int lgRegion[4] = { 0, 0, 0, 0 };
	QString lgOutDir;
	bool lgHaveObjects = false;
	QString lgDataRoot;
	/* OFF since 2026-09-12 (lane DEFAULTS1), bungo's ruling of 15:56 verbatim:
	 * "Legacy terrain bakes stay as they were, no extra data for FO4CS to be
	 * included in them. Only the .lod ones have new data in them." The legacy
	 * .BTO therefore carries VANILLA'S vertex layout and nothing else, and
	 * `--identity` is the opt-in that puts the channels back. The .manifest.txt
	 * SIDECAR, the texture arrays and the impostor cards do NOT ride on this
	 * flag any more -- it is not inside the .BTO, so the ruling is untouched. */
	bool lgIdentity = false;
	// The AO bake is the identity channel's B. Off leaves it at 255, which is
	// what makes each object ONE flat colour -- the index and nothing else.
	bool lgBakeAO = true;
	bool lgCullBuried = false;
	float lgCullMargin = 128.0f;
	bool lgAoGrey = false;
	// Cells of neighbours to bake AO against; 0 is the old chunk-only bake.
	int lgAoSkirt = 1;
	int lgWaterSubdiv = 3;
	bool lgShoreDenser = false;
	QString lgHeightmapDir;
	QString lgLodtDir;
	QString lgBtdPath;
	bool lgBtdProbe = false;
	bool lgLodtVerify = false;
	bool lgRefreshAo = false;
	QString lgDumpLand;
	// the painted area, per cell, from the master's LAND records
	QString lgDumpLayers;
	QString lgDumpShapes;
	int lgHeightmapSize = 0;   // 0 = native, cells*32 a side; the lossless size
	int lgShoreDensity = 1;
	// -1 = leave the option default; 0 = no decimation at all (the source
	// heightfield as a mesh, which is what a fidelity reference needs)
	int lgTargetTris = -1;
	QString lgTexDir;
	/* Ground cover and the grass tint (lodgen.h). OFF by default, and off is
	 * byte-identical to every bake this generator has ever written. */
	LodgenCoverOptions lgCover;
	/* Which of the road knobs the command line actually named, so
	 * `--roads-legacy` can mean ROADS1's WHOLE pipeline without overriding a
	 * value the caller asked for in the same breath. */
	bool lgRoadGroundPaintSet = false;
	bool lgRoadDetailSet = false, lgRoadRaisedSet = false,
		lgRoadSidewalksSet = false, lgRoadsLegacy = false,
		lgRoadOpacitySet = false;
	/* The terrain virtual texture (lodgen.h). OFF by default; --vt names the
	 * mod folder to write Terrain/ under. */
	LodgenVtOptions lgVt;
	QString lgVtDir;
	/* THE FINEST TEXEL DENSITY AS ONE WORD (lane VTNORMAL1, bungo's ruling
	 * 2026-09-23 09:4x): 32, 16 or 8 world units a texel. It NAMES existing
	 * pairs and adds nothing underneath -- 32 = finest 2 / content 256 (the
	 * default), 16 = finest 2 / content 512, 8 = finest 1 / content 512 (his
	 * sheets' own density) -- so it is refused beside either of them. */
	int lgVtDensity = 0;
	bool lgVtFinestGiven = false, lgVtContentGiven = false;
	int lgVtBtr = -1;
	bool lgVtEstimate = false;
	QString lgLodmCheck, lgLodvCheck;
	QString lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture, lgNativeMeshReport;
	bool lgNativeVerifyCorpus = false;
	// v3 (lane NATIVE1b): the two exact ways back off the ladder and the occluders
	/* bungo 2026-09-17: "Authored LODs only". The ladder ships OFF and the library
	 * is the MNAM slots; --native-ladder and --library near are the ways back. */
	bool lgNativeLadder = false, lgNativeOccluders = true;
	/* v4/v5 (lane NATIVE1c): where the library's level 0 comes from, whether
	 * alpha-tested foliage may ladder, the silhouette floor, and the
	 * placement-AO module. Each default is stated beside its way back:
	 *   --library mnam            the v3 choice of level 0, byte for byte
	 *   --native-ladder-foliage   let leaf clusters ladder again
	 *   --native-silhouette 0     turn the level gate off (0 keeps everything)
	 *   --native-no-placement-ao  no AO blob, no .lodi version 5
 *   --native-no-vertex-ao     no per-instance vertex-AO stream, no .lodi version 6
 *   --lodi-v6                 no group table, no per-vertex sky stream; the .lodi
 *                             stays at version 6, byte for byte
 *   --lodi-v7                 accepted and a NO-OP since 2026-09-19: version 7 is
 *                             what a default bake writes. It is kept because
 *                             command lines and gates carry it, and because it
 *                             still says what it always said -- "this .lodi is
 *                             version 7". The baked-horizon route it used to
 *                             turn off no longer exists; see the history
 *                             paragraph in docs/LODGEN_NATIVE_LODO_LODI.md.
 *   --scrappable              mark the placements a player can scrap at a
 *                             workshop (.lodi v9 instance bit 6; off = v7)
 *   --identity-join-gap <u>   v7 GROUPING (bungo's ruling 2026-09-19): how close
 *                             two placements' LOD MESHES must come, in world
 *                             units, before they are one identity. Default 64,
 *                             measured by lane IDENTPROX; 128 is the last gap at
 *                             which no identity holds two different reference
 *                             buildings. Trees never join.
 *   --identity-join legacy    the way back: the pre-2026-09-19 rule, only an
 *                             `architecture`-pathed placement, joined on a WORLD
 *                             AXIS-ALIGNED BOX gap of 16 u. `--identity-join
 *                             proximity` says the default out loud. */
	bool lgLibraryNear = false;
	bool lgNativeLadderFoliage = LODO_LADDER_FOLIAGE_DEFAULT;
	float lgNativeSilhouette = LODO_SILHOUETTE_MIN_DEFAULT;
	bool lgNativePlacementAo = true;
	bool lgNativeVertexAo = true;
	bool lgLodiV7 = true;
	/* v9 (lane HORIZON3, 2026-09-19; kept by lane HORIZONOUT when the rest of
	 * that lane's baked-horizon route was dropped the same day): mark the
	 * placements a player can scrap at a workshop. OFF, and off is the exact
	 * way back -- no bit is written and the .lodi stays at version 7, byte for
	 * byte. */
	bool lgScrappable = false;
	/* v7 GROUPING, bungo's ruling of 2026-09-19 (lane IDENTPROX measured it,
	 * lane HORIZONOUT shipped it). This one is NOT off by default: it is a
	 * RULING, not a module, and the rule it replaces is the way back. */
	bool lgIdentityJoinLegacy = false;
	float lgIdentityJoinGap = 64.0f;
	/* THE AGGREGATE MODULE, AND IT SHIPS OFF. Aggregation is
	 * a module, and CONSTITUTION 10 makes its off value the exact way back --
	 * with it off the .lodi is written at version 3 and every output file is
	 * byte for byte what the same bake wrote before this lane. */
	bool lgAggregate = false;
	int lgAggMin = 8, lgAggTile = 64, lgAggViews = 8;
	bool lgCorpusHash = false;
	bool lgGeomorph = false;
	/* OFF since 2026-09-12 (lane DEFAULTS1), bungo's ruling of 15:53 verbatim:
	 * "We don't bake BTR for FO4CS, and so we do not use of that data for it
	 * at all." `--terrain-identity` is the opt-in. Still matching both
	 * LodgenTerrainOptions and the LOD Manager checkbox: these three drifted
	 * apart once already, and the GUI wrote the terrain channels while the
	 * identical CLI run silently did not. */
	bool lgTerrainIdentity = false;
	QString lgImpostors;
	int lgImpostorFromLevel = -1;
	QString lgCandidates = QStringLiteral( "missing" );
	/* The bake's own trees-only gate, the panel row's CLI face. Default ON,
	 * as the row is. The list above and this are two halves of the same
	 * ruling: the list says which bases get cards BAKED, this says which
	 * placements may stand on one. */
	bool lgTreesOnly = true;
	/* Opt-in. The "atlas is REQUIRED, textures are CK-only" episode was a
	 * broken membership probe: every source LOD texture checked ships in
	 * Fallout4 - Textures6.ba2 (BA2 name-table grep is the ground truth,
	 * ba2x's hash lookup false-negatives). Direct refs are stock-legal,
	 * xLODGen-style; the atlas remains a draw-call optimization. */
	bool lgAtlas = false;
	bool lgArrays = false;
	bool lgMerge = true;
	/* Vanilla's own diffuse sheet is DXT1 (measured: Commonwealth.Objects.DDS,
	 * 4096x2048, 13 mips, 5,592,552 bytes), so BC1 is parity AND half the
	 * memory. Ours has been BC3 since the atlas shipped; the panel picks it
	 * off the Target, the CLI off this flag. */
	bool lgAtlasBc1 = false;
	/* The panel has had this toggle since the chunk builder shipped; the CLI
	 * had no way to reach it, which made every far ring over Sanctuary empty
	 * (measured: 0 of 19,507 refs in chunk (-32,16) fill MNAM slot 2). */
	bool lgSlotFallback = false;
	/* Far-ring proxies (lodgen.h). --no-simplify turns the pass off; the
	 * three ratios and the error bound are per ring. Ring 0 is never cut. */
	LodgenSimplifyOptions lgSimplify;
	QString lgDumpGeometry;
	bool lgListCandidates = false;
	// the divisor on an impostor's normal, mask and emissive sheets; 1 = none
	int lgCardAuxDiv = 1;
	/* The resource stack (lodgen.h). --resource is repeatable and reads in MOD
	 * ORGANIZER's order: the LAST one given overrides the earlier ones. */
	QStringList lgResources;
	QStringList stressFiles;          // lane NIFPARSE1
	int stressThreads = 16;
	int stressReps = 8;
	QString stressSabotage;
	QString lgPluginsTxt;
	bool lgMo2 = false;
	QString lgMo2Profile;             // lane LOADORDER1: his MO2 profile off disk
	QString lgMo2Mods;
	QString lgProbe;
	QString lgProbeOut;
	bool lgPrintSource = false;
	int lgListFiles = 0;
	bool constraintsOnly = false;
	bool skeletonOnly = false;
	bool bodiesOnly = false;
	gLgIncremental.clear();
	gLgKeepBto = false;
	gLgSwitchDigest = lodgenSwitchDigestOf( a );
	/* THE ARGUMENT VECTOR ITSELF (lane BAKEREC1, 2026-09-17). The digest above
	 * answers "is this the same command"; the record answers "what WAS the
	 * command", which is the only form of it an operator can retype. It is the
	 * SAME vector, so the two can never describe different runs. */
	gLgArgv = a;
	gLgBakeRecord.clear();
	gLgNativeCache = true;
	lodbClearCensus();
	/* A VALUED SWITCH SPELLED WITHOUT ITS VALUE (lane AUDIT1, 2026-09-17).
	 * `next()` used to hand back an empty QString at the end of the vector, and
	 * an empty value cannot be told from a switch that was never given:
	 * `--incremental` last on the line parsed, set an empty directory, skipped
	 * the whole incremental block with every refusal in it, and FULL-baked at
	 * exit 0 while the operator was watching the clock for a cached run. Which
	 * switch it was is remembered here and refused after the loop, so the
	 * refusal comes before any work and names the switch. */
	QString missingValueFor;
	for ( int i = 0; i < a.size(); i++ ) {
		const QString & t = a.at( i );
		auto next = [&]() -> QString {
			if ( i + 1 < a.size() )
				return a.at( ++i );
			missingValueFor = t;
			return QString();
		};
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
		else if ( t == QLatin1String( "--clip" ) ) gltfClip = next();			// lane HKX4
		else if ( t == QLatin1String( "--bones" ) ) gltfBones = next();		// lane HKX4
		else if ( t == QLatin1String( "--root-motion" ) ) gltfRootMotion = true;	// lane HKX4
		else if ( t == QLatin1String( "--fps" ) ) gltfFps = next().toFloat();	// lane BUILD8
		else if ( t == QLatin1String( "--source-rate" ) ) gltfSourceRate = true;	// lane BUILD8
		else if ( t == QLatin1String( "--route" ) ) gltfRoute = next();		// lane BUILD8
		else if ( t == QLatin1String( "--tsv" ) ) gltfTsv = next();			// lane BUILD8
		else if ( t == QLatin1String( "--skeleton-name" ) ) gltfSkeletonName = next();	// lane BUILD8
		else if ( t == QLatin1String( "--root-node" ) ) gltfRootNode = next();	// lane BUILD8
		// lane GLTFEXPORT1: every export option, through the SAME function the
		// dialog's rows drive, so a flag and a row cannot mean different things.
		// ONLY for the gltf commands (lane PBRLODFIX1, 2026-09-24): this loop is
		// shared by every command, and the export's `--data-root` and `--skeleton`
		// shadowed lodgen's `--data-root` and collision's `--skeleton` further
		// down -- every lodgen bake since 2026-09-19 ran without its loose root.
		else if ( int gr = ( cmd == QLatin1String( "gltf" ) || cmd == QLatin1String( "gltf-export" ) )
				? gltfExportParseFlag( t, ( i + 1 < a.size() ) ? a.at( i + 1 ) : QString(),
					gltfOpts, gltfUsedNext, gltfFlagError ) : 0 ) {
			if ( gr < 0 ) { err() << "gltf: " << gltfFlagError << Qt::endl; return 2; }
			if ( gltfUsedNext ) i++;
		}
		else if ( t == QLatin1String( "--selftest" ) ) selfTest = true;
		else if ( t == QLatin1String( "--extract" ) ) extract = true;
		else if ( t == QLatin1String( "--info" ) ) btdInfo = true;
		else if ( t == QLatin1String( "--region" ) ) {
			btdHaveRegion = true;
			for ( int r = 0; r < 4; r++ )
				btdRegion[r] = next().toInt();
		}
		else if ( t == QLatin1String( "--lod" ) ) btdLod = next().toInt();
		else if ( t == QLatin1String( "--plane" ) ) lodtPlaneName = next();
		else if ( t == QLatin1String( "--water-census" ) ) lodtWaterCensusOnly = true;
		else if ( t == QLatin1String( "--water-selftest" ) ) lodtWaterSelfTestOnly = true;
		else if ( t == QLatin1String( "--water-mark-selftest" ) ) lodtWaterMarkSelfTestOnly = true;
		else if ( t == QLatin1String( "--list-worldspaces" ) ) lgListWorldspaces = true;
		else if ( t == QLatin1String( "--worldspace" ) ) lgWorldspace = next().toUInt( nullptr, 16 );
		else if ( t == QLatin1String( "--cell" ) ) {
			lgHaveCell = true;
			lgCell[0] = next().toInt();
			lgCell[1] = next().toInt();
		}
		else if ( t == QLatin1String( "--terrain" ) ) {
			lgHaveTerrain = true;
			lgChunk[0] = next().toInt();
			lgChunk[1] = next().toInt();
		}
		else if ( t == QLatin1String( "--dim" ) ) lgDim = next().toInt();
		else if ( t == QLatin1String( "--terrain-region" ) ) {
			lgHaveRegion = true;
			for ( int r = 0; r < 4; r++ )
				lgRegion[r] = next().toInt();
		}
		else if ( t == QLatin1String( "--out-dir" ) ) lgOutDir = next();
		else if ( t == QLatin1String( "--objects" ) ) {
			lgHaveObjects = true;
			lgChunk[0] = next().toInt();
			lgChunk[1] = next().toInt();
		}
		else if ( t == QLatin1String( "--data-root" ) ) lgDataRoot = next();
		else if ( t == QLatin1String( "--resource" ) ) lgResources << next();
		else if ( t == QLatin1String( "--plugins-txt" ) ) lgPluginsTxt = next();
		else if ( t == QLatin1String( "--mo2" ) ) lgMo2 = true;
		else if ( t == QLatin1String( "--mo2-profile" ) ) lgMo2Profile = next();
		else if ( t == QLatin1String( "--mo2-mods" ) ) lgMo2Mods = next();
		else if ( t == QLatin1String( "--bake-record" ) ) gLgBakeRecord = next();
		else if ( t == QLatin1String( "--no-native-cache" ) ) gLgNativeCache = false;
		else if ( t == QLatin1String( "--probe" ) ) lgProbe = next();
		else if ( t == QLatin1String( "--probe-out" ) ) lgProbeOut = next();
		else if ( t == QLatin1String( "--print-source" ) ) lgPrintSource = true;
		else if ( t == QLatin1String( "--list-files" ) ) lgListFiles = next().toInt();
		/* `--identity` is the OPT-IN (the default went off 2026-09-12);
		 * `--no-identity` stays, so every script written before the flip
		 * still says what it means. */
		else if ( t == QLatin1String( "--identity" ) ) lgIdentity = true;
		else if ( t == QLatin1String( "--no-identity" ) ) lgIdentity = false;
		else if ( t == QLatin1String( "--no-ao" ) ) lgBakeAO = false;
		/* THE THREAD BUDGET, and the exact way back. `--threads 1` runs the
		 * generator on one core the way it always ran: one world, one cache
		 * set, one chunk at a time, written inline. 0 or absent = the
		 * machine. Nothing about the arithmetic depends on it -- the gate
		 * is that every output file is byte-identical either way. */
		else if ( t == QLatin1String( "--threads" ) ) lodgenSetThreadCount( next().toInt() );
		/* The CHUNK queue's own number, default 1. See lodgenparallel.h.
		 * It is SAFE as of 2026-09-11 (lane RESUME3: 20 of 20 clean at 16
		 * on each of two regions, byte-identical to the serial bake), and it
		 * still defaults to 1 because it is SLOWER and much hungrier --
		 * 2.3x the wall time on 9 chunks, 2.2x on 25, and 25 GB of peak
		 * working set against 3.8. The default is a speed decision now, not
		 * a safety one. */
		else if ( t == QLatin1String( "--chunk-threads" ) ) lodgenSetChunkThreadCount( next().toInt() );
		/* THE LANDSCAPE TEXTURES' WORLD-SPACE TILING (lane SPLAT1 measured it,
		 * lane RESUME3 landed it). Default 341.3333 = 128/0.375, the engine's
		 * own number out of Fallout4.exe 1.10.155. `--land-tiling 2048` is the
		 * exact way back to the pre-2026-09-11 bake. */
		else if ( t == QLatin1String( "--land-tiling" ) ) lodgenSetLandTiling( next().toFloat() );
		/* HOW THE LAND TEXTURE IS SAMPLED INSIDE THAT REPEAT (lane TILING2).
		 * `footprint` is the default and is the 2026-09-11 bake exactly: one
		 * texel of the mip that matches the bake texel's world footprint, which
		 * prints the same ~11-texel picture of the texture in every repeat.
		 * `average` reads the texture's 1x1 mip -- its exact mean over one
		 * whole repeat -- so no periodic term can reach the sheet at all, and
		 * `--land-detail k` adds back k of the footprint sample's departure
		 * from that average (k=1 IS the footprint bake).
		 *
		 * `stochastic` (lane TILING3) is the third mode and the only one that
		 * keeps the grain AND drops the repeat: one token for the four numbers
		 * that lane picked -- warp amplitude 683 world units, lattice 1024, one
		 * octave, mip bias -1.00. It leaves `average` OFF, so it replaces
		 * nothing and composes with nothing; set any of the four switches below
		 * AFTER it to override one of them. */
		else if ( t == QLatin1String( "--land-sample" ) ) {
			const QString v = next().toLower();
			lodgenSetLandSampleAverage( v == QLatin1String( "average" ) );
			/* `stochastic` MEANS THE HEX TILING as of lane TILING4: the warp it
			 * used to mean reads as swirls (the swirl instrument convicts it on
			 * 5 of 7 shipped sheets, and bungo saw it), and the hex tiling is
			 * clean on 7 of 7 and 7 of 7 for the same repeat count. The warp is
			 * still reachable, as `warp`, so the measurement can be repeated.
			 * Each mode turns the OTHER geometry off, so the two words cannot
			 * silently compose into a third thing nobody picked. */
			if ( v == QLatin1String( "stochastic" ) ) {
				lodgenSetLandHexSize( 256.0f );
				lodgenSetLandWarpAmp( 0.0f );
				lodgenSetLandMipBias( -0.22f );
			}
			else if ( v == QLatin1String( "warp" ) ) {
				lodgenSetLandHexSize( 0.0f );
				lodgenSetLandWarpAmp( 683.0f );
				lodgenSetLandWarpLattice( 1024.0f );
				lodgenSetLandWarpOctaves( 1 );
				lodgenSetLandMipBias( -1.0f );
			}
		}
		else if ( t == QLatin1String( "--land-detail" ) ) lodgenSetLandDetail( next().toFloat() );
		/* THE FOUR NUMBERS OF THE STOCHASTIC SAMPLE, individually (lane
		 * TILING3). Since 2026-09-12 the DEFAULTS are bungo's pick -- amplitude
		 * 341, bias -0.22, hex 256, guide flatwarp:1.0 -- so the way back to
		 * the 2026-09-11 bake, byte for byte, is
		 * `--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off`. */
		else if ( t == QLatin1String( "--land-warp" ) ) lodgenSetLandWarpAmp( next().toFloat() );
		else if ( t == QLatin1String( "--land-warp-lattice" ) ) lodgenSetLandWarpLattice( next().toFloat() );
		else if ( t == QLatin1String( "--land-warp-octaves" ) ) lodgenSetLandWarpOctaves( next().toInt() );
		else if ( t == QLatin1String( "--land-mip-bias" ) ) lodgenSetLandMipBias( next().toFloat() );
		/* THE HEX TILE SIZE (lane TILING4), individually. 256 world units --
		 * what the lane picked, and the DEFAULT since 2026-09-12 -- against a
		 * land repeat of 341.3333. `--land-hex 0` is one quarter of the way
		 * back to the 2026-09-11 bake; see the comment above for all four. */
		else if ( t == QLatin1String( "--land-hex" ) ) lodgenSetLandHexSize( next().toFloat() );
		/* TERRAIN-GUIDED LAND SAMPLING (lane LAND1), bungo 2026-09-12:
		 * "since we're reusing vanilla terain normals and slope maps,
		 * might as well use them to guide this a bit". The guide is the
		 * HEIGHTMAP's own low-pass slope, not the `_msn` sheet, so it is
		 * continuous across every border. `off` is the default and is the
		 * rung's bytes; the strength after the colon means world units for
		 * `drag`, a 0..1 fraction of the rotation for `aspect`/`aspecthex`,
		 * and a multiplier on --land-warp's amplitude for the two warps. */
		else if ( t == QLatin1String( "--land-guide" ) ) {
			QString v = next().toLower();
			const int colon = v.indexOf( QLatin1Char( ':' ) );
			if ( colon >= 0 ) {
				bool ok = false;
				const float k = v.mid( colon + 1 ).toFloat( &ok );
				if ( ok )
					lodgenSetLandGuideStrength( k );
				else
					fprintf( stderr, "lodgen: --land-guide %s has no readable strength after the colon; the default stands\n",
						v.toLatin1().constData() );
				v = v.left( colon );
			}
			if ( v == QLatin1String( "off" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_OFF );
			else if ( v == QLatin1String( "drag" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_DRAG );
			else if ( v == QLatin1String( "aspect" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_ASPECT );
			else if ( v == QLatin1String( "aspecthex" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_ASPECTHEX );
			else if ( v == QLatin1String( "slopewarp" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_SLOPEWARP );
			else if ( v == QLatin1String( "flatwarp" ) )
				lodgenSetLandGuideRule( LODGEN_LANDGUIDE_FLATWARP );
			else
				/* "the default stands", not "off stands" (lane AUDIT1,
				 * 2026-09-17): this branch calls NOTHING, so what stands is
				 * whatever the defaults ruling put there -- flatwarp:1.0 since
				 * lane DEFAULTS1 -- and the sheets are flatwarp sheets. The
				 * sibling message above says it this way for the same reason. */
				fprintf( stderr, "lodgen: --land-guide %s is not one of off|drag|aspect|aspecthex|slopewarp|flatwarp; the default stands\n",
					v.toLatin1().constData() );
		}
		/* INCREMENTAL REGENERATION (lane INCR1). The value is the out-dir of
		 * a PREVIOUS bake -- the one carrying the .lodb ledger -- and it is
		 * normally the same directory --out-dir names. What it does and every
		 * case in which it REFUSES rather than quietly full-baking is in
		 * docs/LODGEN_LEDGER_FORMAT.md section 4. */
		else if ( t == QLatin1String( "--incremental" ) ) gLgIncremental = next();
		else if ( t == QLatin1String( "--land-guide-scale" ) ) lodgenSetLandGuideScale( next().toFloat() );
		else if ( t == QLatin1String( "--land-guide-slope" ) ) lodgenSetLandGuideSlopeRef( next().toFloat() );
		/* VANILLA FAR-TERRAIN REUSE (lane TILING3), bungo's ruling 2026-09-11:
		 * "so now we do not use our own normal map if that is toggled, but
		 * reuse these ones for terrain chunks" -- and, on the out-of-bounds
		 * ground, "out of bounds terrain blends are not included in the actual
		 * cells out of bounds, they never were, so we can't recover the color
		 * data anymore, because it was baked in a different tool outside of
		 * fo4".
		 *
		 *   `vanilla` (THE DEFAULT) -- a chunk with a shipped vanilla `_msn`
		 *       writes VANILLA'S FILE byte for byte and our normal bake is
		 *       skipped for it; a chunk with no land paint on any cell writes
		 *       vanilla's COLOUR file byte for byte too; every other chunk
		 *       keeps our composite and takes the crevice term.
		 *   `vanilla-blend` -- vanilla's fine detail over OUR coarse normal,
		 *       up recomputed so the normal stays unit length. Reshaped
		 *       terrain. Never the default.
		 *   `none` -- the rung's bytes, exactly.
		 *
		 * `--vanilla-lod-root` names where vanilla's sheets are READ AS LOOSE
		 * FILES. It is never the resource stack on purpose: the stack would
		 * serve our own previously installed output out of the game's Data and
		 * the bake would "reuse vanilla" by copying yesterday's copy of
		 * itself. */
		else if ( t == QLatin1String( "--land-detail-source" ) ) {
			const QString v = next().toLower();
			if ( v == QLatin1String( "none" ) )
				lodgenSetLandDetailSource( LODGEN_LANDDETAIL_NONE );
			else if ( v == QLatin1String( "vanilla-blend" ) )
				lodgenSetLandDetailSource( LODGEN_LANDDETAIL_VANILLA_BLEND );
			else if ( v == QLatin1String( "vanilla" ) )
				lodgenSetLandDetailSource( LODGEN_LANDDETAIL_VANILLA );
			else if ( v == QLatin1String( "erosion" ) )
				lodgenSetLandDetailSource( LODGEN_LANDDETAIL_EROSION );
			else
				fprintf( stderr, "lodgen: --land-detail-source %s is not one of "
					"none|vanilla|vanilla-blend|erosion; the default (vanilla) stands\n",
					v.toLatin1().constData() );
		}
		else if ( t == QLatin1String( "--vanilla-lod-root" ) ) lodgenSetVanillaLodRoot( next() );
		/* THE SHEET FORMAT (lane TERRAINFMT1). `legacy` is the default and is
		 * the previous bake's bytes -- the writer is called with the arguments
		 * it was called with before. `vanilla` is Bethesda's law as MEASURED
		 * over the whole shipped corpus: all 6,120 Commonwealth terrain sheets
		 * are DXT5, 512x512, 10 mips (512 down to 1, past the 4x4 block floor
		 * ours stops at), and the ALPHA of both families is a constant 255 --
		 * one distinct value over 13,107,200 texels on each of a 50-sheet
		 * colour sample and a 50-sheet _msn sample. So `vanilla` writes DXT5
		 * with the chain to 1x1 and 255 in the alpha, and nothing else. It
		 * cannot collide with the WWCV cover stamp: that stamp lives in
		 * dwReserved1 of _data.DDS, a sheet vanilla does not ship at all, and
		 * no shipped sheet has a non-zero dwReserved1. A sheet COPIED from
		 * vanilla is untouched by this switch. */
		else if ( t == QLatin1String( "--sheet-format" ) ) {
			const QString v = next().toLower();
			if ( v == QLatin1String( "vanilla" ) )
				lodgenSetSheetFormat( LODGEN_SHEETFMT_VANILLA );
			else if ( v == QLatin1String( "legacy" ) )
				lodgenSetSheetFormat( LODGEN_SHEETFMT_LEGACY );
			else
				fprintf( stderr, "lodgen: --sheet-format %s is not one of "
					"vanilla|legacy; the default (legacy) stands\n",
					v.toLatin1().constData() );
		}
		/* THE CLEANED _msn CACHE (lane TERRAINFMT1, ADDED ITEM 8). A directory
		 * of <ws>.<dim>.<x>.<y>.png sheets; empty is the default and reads no
		 * directory. The cache is NOT in vanilla's channel layout and this is
		 * measured, not assumed: cache R is east (r 0.956 against vanilla's R),
		 * cache G is north (r 0.794 against vanilla's B) and cache B is
		 * identically 0 on 14 of 16 sampled sheets, so UP is recomputed here
		 * and the triple RENORMALISED, never clamped. The sheet is written
		 * UNCOMPRESSED (B8G8R8A8 through a DX10 header) with a full mip chain,
		 * because a BC re-encode puts back the 4x4 block grid that is the one
		 * thing the cache removed. BC7 (src/lodgenbc7.h) is not used here. What
		 * this costs over a worldspace is in the lane report; it is not a
		 * decision this flag makes for anyone. */
		else if ( t == QLatin1String( "--msn-cache" ) ) lodgenSetMsnCacheDir( next() );
		/* The crevice coefficient, in 8-bit luminance levels per unit of
		 * detail-normal divergence. -3.242 is the median fitted on seven
		 * vanilla sheets (scratchpad/tiling3_20260911/d3_shade.py); 0 turns the
		 * shading off while leaving the sheet reuse on. */
		else if ( t == QLatin1String( "--land-shade" ) ) lodgenSetLandShade( next().toFloat() );
		/* THE EROSION PASS (lane GROUND1 Part B). A deterministic hydraulic
		 * pass over the bake's own height lattice, whose height delta reaches
		 * BOTH the `_msn` sheet (as an added gradient) and the colour (as the
		 * crevice term's own shading). 0 is the default, takes no branch and
		 * builds no lattice, so a bake without the flag is the rung's bytes.
		 * The strength multiplies the gradient it adds and the shading it
		 * casts, in that one place each, so the two cannot drift apart.
		 * On Commonwealth the DEFAULT --land-detail-source vanilla replaces
		 * our `_msn` with vanilla's copy, which would throw the pass away:
		 * `--land-detail-source erosion` is the value that keeps ours and
		 * also stops vanilla's crevice term shading the same sheet twice. */
		else if ( t == QLatin1String( "--erosion" ) ) lodgenSetErosion( next().toFloat() );
		else if ( t == QLatin1String( "--erosion-iterations" ) )
			lodgenSetErosionIterations( next().toInt() );
		else if ( t == QLatin1String( "--erosion-seed" ) )
			lodgenSetErosionSeed( quint32( next().toUInt() ) );
		/* THE COLOUR GRADE (lane GRADE1). Every baked colour texel x k before
		 * quantisation, in both writers, after the road and the grass tint and
		 * before the crevice term. 1 is the default and skips the branch, so
		 * the bake is byte-identical without the flag. There is no k that helps
		 * everywhere: the per-tile optimum runs 0.615..1.241 over a 25-tile
		 * census and flips sign between the two reference tiles. 0.840 is the
		 * pooled optimum if a single number is ever wanted. */
		else if ( t == QLatin1String( "--grade" ) ) lodgenSetLandGrade( next().toFloat() );
		/* THE QUADRANT BORDER (lane TILING2). `quadrant` is the DEFAULT since
		 * 2026-09-23 (bungo, lane DEFAULTS2); `off` is the bake before that
		 * ruling exactly. `quadrant` cross-fades the neighbouring
		 * quadrant's composite over --blend-margin units either side of every
		 * 2,048-unit quadrant line. */
		else if ( t == QLatin1String( "--blend-edges" ) ) {
			const QString v = next().toLower();
			lodgenSetBlendEdges( v == QLatin1String( "quadrant" ) ? 1 : 0 );
		}
		else if ( t == QLatin1String( "--blend-margin" ) ) lodgenSetBlendMargin( next().toFloat() );
		/* THE MODEL LAYER ON N THREADS, and nothing else in the picture
		 * (lane NIFPARSE1, see src/nifparsestress.h). A bake crash cannot
		 * tell the parser apart from the plugin reader, the texture cache,
		 * the archive layer and the message sink; this can. */
		else if ( t == QLatin1String( "--stress-file" ) ) stressFiles << next();
		else if ( t == QLatin1String( "--stress-threads" ) ) stressThreads = next().toInt();
		else if ( t == QLatin1String( "--stress-reps" ) ) stressReps = next().toInt();
		else if ( t == QLatin1String( "--stress-sabotage" ) ) stressSabotage = next();
		else if ( t == QLatin1String( "--cull-buried" ) ) lgCullBuried = true;
		else if ( t == QLatin1String( "--cull-margin" ) ) lgCullMargin = next().toFloat();
		else if ( t == QLatin1String( "--ao-grey" ) ) lgAoGrey = true;
		else if ( t == QLatin1String( "--ao-skirt" ) ) lgAoSkirt = next().toInt();
		else if ( t == QLatin1String( "--water-subdiv" ) ) lgWaterSubdiv = next().toInt();
		else if ( t == QLatin1String( "--shore-denser" ) ) lgShoreDenser = true;
		else if ( t == QLatin1String( "--no-shore-denser" ) ) lgShoreDenser = false;
		else if ( t == QLatin1String( "--target-tris" ) ) lgTargetTris = next().toInt();
		else if ( t == QLatin1String( "--heightmap" ) ) lgHeightmapDir = next();
		else if ( t == QLatin1String( "--lodl" ) ) lgLodtDir = next();
		/* The retired spellings NAME their replacement instead of falling into
		 * the generic "unknown option": these two are the commands a harness,
		 * a script or bungo's own shell history will still be carrying, and a
		 * bare "unknown option --lodt" does not say that .lodt now means
		 * something else. */
		else if ( t == QLatin1String( "--lodt" ) ) {
			err() << "error: --lodt is retired: the landscape file is .lodl now "
					 "(.lodt names the terrain texture sheets) -- use --lodl <dir>" << Qt::endl;
			err().flush();
			return 2;
		}
		else if ( t == QLatin1String( "--from-btd" ) ) lgBtdPath = next();
		else if ( t == QLatin1String( "--btd-probe" ) ) lgBtdProbe = true;
		else if ( t == QLatin1String( "--verify-only" ) ) lgLodtVerify = true;
		else if ( t == QLatin1String( "--refresh-ao" ) ) lgRefreshAo = true;
		/* The water-body module (docs/LODGEN_BTD_FORMAT.md, version 3). It is
		 * the ONLY thing that raises the written version to 3, so a run without
		 * it is byte-identical to what this writer produced before. */
		else if ( t == QLatin1String( "--water-bodies" ) ) gLodlWater.enabled = true;
		else if ( t == QLatin1String( "--water-bridge" ) ) gLodlWater.bridgeGap = next().toInt();
		else if ( t == QLatin1String( "--water-near" ) ) gLodlWater.nearTexels = next().toInt();
		else if ( t == QLatin1String( "--water-body-samples" ) ) gLodlWater.bodySamples = next().toInt();
		else if ( t == QLatin1String( "--water-flow-samples" ) ) gLodlWater.flowSamples = next().toInt();
		else if ( t == QLatin1String( "--water-no-shore" ) ) gLodlWater.shore = false;
		else if ( t == QLatin1String( "--water-velocities" ) ) gLodlWater.velocityPlugin = next();
		else if ( t == QLatin1String( "--water-report" ) ) gLodlWater.reportPath = next();
		else if ( t == QLatin1String( "--dump-land" ) ) lgDumpLand = next();
		else if ( t == QLatin1String( "--dump-layers" ) ) lgDumpLayers = next();
		else if ( t == QLatin1String( "--dump-shapes" ) ) lgDumpShapes = next();
		else if ( t == QLatin1String( "--heightmap-size" ) ) {
			const QString v = next();
			lgHeightmapSize = v.compare( QLatin1String( "native" ), Qt::CaseInsensitive ) == 0 ? 0 : v.toInt();
		}
		else if ( t == QLatin1String( "--shore-density" ) ) lgShoreDensity = next().toInt();
		else if ( t == QLatin1String( "--tex-dir" ) ) lgTexDir = next();
		else if ( t == QLatin1String( "--cover" ) ) lgCover.cover = true;
		else if ( t == QLatin1String( "--no-cover" ) ) lgCover.cover = false;
		else if ( t == QLatin1String( "--grass-tint" ) ) lgCover.tintStrength = next().toFloat();
		else if ( t == QLatin1String( "--cover-full" ) ) lgCover.coverFull = next().toFloat();
		else if ( t == QLatin1String( "--dump-cover" ) ) lgCover.dumpCoverPath = next();
		/* ROADS AND DECALS (bungo 2026-09-11: "We do the same with roads and
		 * decals as vanilla"). ON by default under both targets; --no-roads is
		 * the exact way back and the bake is byte-identical without it. */
		else if ( t == QLatin1String( "--roads" ) ) lgCover.roads = true;
		else if ( t == QLatin1String( "--no-roads" ) ) lgCover.roads = false;
		else if ( t == QLatin1String( "--road-cover-suppress" ) )
			lgCover.roadCoverSuppress = next().toFloat();
		/* HOW STRONGLY the road paint is mixed into the ground under it
		 * (lane ROADS3). 1 is the default and is branched over, so the off
		 * value is the previous bake's bytes; 0 paints nothing while the
		 * road geometry still suppresses the ground cover. The numbers that
		 * decided the default are on LodgenCoverOptions::roadOpacity. */
		else if ( t == QLatin1String( "--road-opacity" ) ) {
			lgCover.roadOpacity = qBound( 0.0f, next().toFloat(), 1.0f );
			lgRoadOpacitySet = true;
		}
		/* HOW road pieces combine, how much of the road diffuse's own pattern
		 * is printed, and which road families are painted at all (lane ROADS2).
		 * `--roads-legacy` is the exact way back and means ROADS1's WHOLE
		 * pipeline: unless they are given on the same command line it restores
		 * full detail, the raised families and the sidewalks, so a legacy bake
		 * is byte-identical to the bake before this lane existed. */
		else if ( t == QLatin1String( "--road-composite" ) ) {
			const QString v = next().toLower();
			lgCover.roadComposite = ( v == QLatin1String( "blend" ) )
				? LodgenCoverOptions::RoadBlend : LodgenCoverOptions::RoadMaxZ;
		}
		/* THE FAR TERRAIN RECEIVES AMBIENT OCCLUSION FROM THE PLACED OBJECTS
		 * (lane GROUND1). OFF is the default and off is the rung's BYTES: the
		 * object term is its own horizon march and returns exactly 1.0f where no
		 * occluder is within 1,458 units, so the multiply cannot move a byte.
		 * `--no-terrain-object-ao` is the way back on a command line that has
		 * the flag in it already. */
		else if ( t == QLatin1String( "--terrain-object-ao" ) )
			lgCover.terrainObjectAo = true;
		else if ( t == QLatin1String( "--no-terrain-object-ao" ) )
			lgCover.terrainObjectAo = false;
		/* THE DIAL. 1 is the law as written and the default; 0 is the rung's
		 * bytes even with the switch on. It is NOT set away from 1 here, and
		 * that is a refusal with a reason: vanilla's far terrain has no object
		 * occlusion to score an absolute strength against. */
		else if ( t == QLatin1String( "--terrain-object-ao-strength" ) )
			lgCover.terrainObjectAoStrength = qBound( 0.0f, next().toFloat(), 4.0f );
		/* THE SLAB SUB-TOGGLE (lane SLAB1, 2026-09-18). Not a dial -- there is
		 * nothing to tune -- but a boolean way back, default = the new law:
		 * a square whose object span stands entirely above the sample is a
		 * CEILING and blocks only from its nearest escape up to the zenith,
		 * so the ground under an overpass deck is lit from the sides. 0 is the
		 * max-Z reading bit for bit, which is what the bakes bungo has already
		 * looked at carry and what the behaviour gate runs red against. */
		else if ( t == QLatin1String( "--terrain-object-ao-slab" ) )
			lgCover.terrainObjectAoSlab = next().toInt() != 0;
		else if ( t == QLatin1String( "--no-terrain-object-ao-slab" ) )
			lgCover.terrainObjectAoSlab = false;
		else if ( t == QLatin1String( "--dump-object-ao" ) )
			lgCover.dumpObjectAoPath = next();
		else if ( t == QLatin1String( "--road-detail" ) ) {
			lgCover.roadDetail = qBound( 0.0f, next().toFloat(), 1.0f );
			lgRoadDetailSet = true;
		}
		/* THE GROUND-MATERIAL SHAPES inside a road model (lane ROADS4):
		 * the coverage multiplier for a shape whose material lives under
		 * materials/Landscape/Ground/. 1 paints them as road, 0 leaves the
		 * landscape's own colour there. The numbers are in
		 * LodgenCoverOptions::roadGroundPaint. */
		else if ( t == QLatin1String( "--road-ground-paint" ) ) {
			lgCover.roadGroundPaint = qBound( 0.0f, next().toFloat(), 1.0f );
			lgRoadGroundPaintSet = true;
		}
		else if ( t == QLatin1String( "--road-raised" ) ) {
			lgCover.roadRaised = true;
			lgRoadRaisedSet = true;
		}
		else if ( t == QLatin1String( "--no-road-raised" ) ) {
			lgCover.roadRaised = false;
			lgRoadRaisedSet = true;
		}
		else if ( t == QLatin1String( "--road-sidewalks" ) ) {
			lgCover.roadSidewalks = true;
			lgRoadSidewalksSet = true;
		}
		else if ( t == QLatin1String( "--no-road-sidewalks" ) ) {
			lgCover.roadSidewalks = false;
			lgRoadSidewalksSet = true;
		}
		else if ( t == QLatin1String( "--roads-legacy" ) ) {
			lgRoadsLegacy = true;
			lgCover.roadComposite = LodgenCoverOptions::RoadMaxZ;
		}
		else if ( t == QLatin1String( "--vt" ) ) lgVtDir = next();
		else if ( t == QLatin1String( "--no-vt" ) ) lgVtDir.clear();
		else if ( t == QLatin1String( "--vt-finest" ) ) {
			lgVt.finestDim = next().toInt();
			lgVtFinestGiven = true;
		}
		else if ( t == QLatin1String( "--vt-content" ) ) {
			lgVt.content = next().toInt();
			lgVtContentGiven = true;
		}
		else if ( t == QLatin1String( "--vt-density" ) ) lgVtDensity = qMax( -1, next().toInt() );
		else if ( t == QLatin1String( "--vt-half-aux" ) ) lgVt.halfAux = true;
		else if ( t == QLatin1String( "--vt-border" ) ) lgVt.border = next().toInt();
		else if ( t == QLatin1String( "--vt-mips" ) ) lgVt.mips = next().toInt();
		else if ( t == QLatin1String( "--vt-compress" ) ) {
			const QString v = next();
			lgVt.compression = ( v == QLatin1String( "none" ) ) ? 0
				: ( v == QLatin1String( "zlib" ) ) ? 1 : -1;
		}
		else if ( t == QLatin1String( "--vt-height" ) ) lgVt.height = true;
		/* The OTHER arm of bungo's open question on where the ground-cover byte
		 * lives (lodgen.h, LodgenVtOptions::coverInColor). Off is what ships. */
		else if ( t == QLatin1String( "--vt-cover-in-color" ) ) lgVt.coverInColor = true;
		else if ( t == QLatin1String( "--vt-cover-in-mask" ) ) lgVt.coverInColor = false;
		else if ( t == QLatin1String( "--vt-btr" ) ) lgVtBtr = 1;
		else if ( t == QLatin1String( "--no-vt-btr" ) ) lgVtBtr = 0;
		else if ( t == QLatin1String( "--vt-estimate" ) ) lgVtEstimate = true;
		else if ( t == QLatin1String( "--lodm-check" ) ) lgLodmCheck = next();
		else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();
		else if ( t == QLatin1String( "--keep-bto" ) ) gLgKeepBto = true;
		else if ( t == QLatin1String( "--native-verify" ) ) { lgNativeVerifyLodo = next(); lgNativeVerifyLodi = next(); }
		else if ( t == QLatin1String( "--native-fixture" ) ) lgNativeFixture = next();
		else if ( t == QLatin1String( "--native-mesh-report" ) ) lgNativeMeshReport = next();
		else if ( t == QLatin1String( "--native-verify-corpus" ) ) lgNativeVerifyCorpus = true;
		else if ( t == QLatin1String( "--native-no-ladder" ) ) lgNativeLadder = false;
		else if ( t == QLatin1String( "--native-ladder" ) ) lgNativeLadder = true;
		else if ( t == QLatin1String( "--native-no-occluders" ) ) lgNativeOccluders = false;
		else if ( t == QLatin1String( "--library" ) ) {
			/* `next()` and NOT `args[++i]`: the loop walks `a`, which starts at the
			 * subcommand's first argument, while `args` is the whole argv. Reading
			 * the wrong list took the token three places to the left and this switch
			 * never worked until tests/spells/lodgen_ladder.sh ran it. */
			const QString v = next().toLower();
			if ( v == QLatin1String( "near" ) ) {
				lgLibraryNear = true;
			} else if ( v == QLatin1String( "mnam" ) ) {
				lgLibraryNear = false;
			} else {
				err() << "error: --library takes near or mnam, not '" << v << "'" << Qt::endl;
				return 2;
			}
		}
		else if ( t == QLatin1String( "--native-ladder-foliage" ) ) lgNativeLadderFoliage = true;
		else if ( t == QLatin1String( "--native-no-placement-ao" ) ) lgNativePlacementAo = false;
		else if ( t == QLatin1String( "--native-no-vertex-ao" ) ) lgNativeVertexAo = false;
		else if ( t == QLatin1String( "--lodi-v6" ) ) lgLodiV7 = false;
		/* ACCEPTED AND A NO-OP since lane HORIZONOUT (2026-09-19): a default
		 * bake writes version 7 already. It stays on the command line because
		 * gates and saved command lines carry it and because it still states a
		 * true fact about the file that comes out. */
		else if ( t == QLatin1String( "--lodi-v7" ) ) { }
		/* v9 (lane HORIZON3, 2026-09-19). A placement a player can scrap is a
		 * placement that WILL NOT BE THERE, and a far field that keeps drawing
		 * it is wrong about a settlement from the first hour of a save onwards.
		 * The bit says which ones those are; the three-clause rule is read out
		 * of the plugin (src/esmdata.h, EsmScrapIndex). */
		else if ( t == QLatin1String( "--scrappable" ) ) lgScrappable = true;
		/* v7 GROUPING. The MEASURE is not a knob and the GAP is: lane IDENTPROX
		 * measured that a box gap of any size bridges a street, so `legacy` gets
		 * the whole old rule (measure and number together) and there is no way to
		 * ask for the old measure at a new number. */
		else if ( t == QLatin1String( "--identity-join" ) ) {
			const QString v = next().toLower();
			if ( v == QLatin1String( "legacy" ) ) {
				lgIdentityJoinLegacy = true;
			} else if ( v == QLatin1String( "proximity" ) ) {
				lgIdentityJoinLegacy = false;
			} else {
				err() << "error: --identity-join takes proximity or legacy, not '" << v << "'" << Qt::endl;
				return 2;
			}
		}
		else if ( t == QLatin1String( "--identity-join-gap" ) ) {
			bool ok = false;
			const QString sv = next();
			const float v = sv.toFloat( &ok );
			if ( !ok || !( v >= 0.0f ) || v > 100000.0f ) {
				err() << "error: --identity-join-gap takes world units in 0..100000, not '"
					  << sv << "'" << Qt::endl;
				return 2;
			}
			lgIdentityJoinGap = v;
		}
		else if ( t == QLatin1String( "--native-silhouette" ) ) {
			bool ok = false;
			const QString sv = next();   // see the note on --library above
			const float v = sv.toFloat( &ok );
			if ( !ok || v < 0.0f || v > 1.0f ) {
				err() << "error: --native-silhouette takes a fraction 0..1, not '" << sv << "'" << Qt::endl;
				return 2;
			}
			lgNativeSilhouette = v;
		}
		else if ( t == QLatin1String( "--aggregate" ) ) lgAggregate = true;
		else if ( t == QLatin1String( "--no-aggregate" ) ) lgAggregate = false;
		else if ( t == QLatin1String( "--aggregate-min" ) ) lgAggMin = next().toInt();
		else if ( t == QLatin1String( "--aggregate-tile" ) ) lgAggTile = next().toInt();
		else if ( t == QLatin1String( "--aggregate-views" ) ) lgAggViews = next().toInt();
		else if ( t == QLatin1String( "--lodt-check" ) ) lgLodvCheck = next();
		else if ( t == QLatin1String( "--lodv-check" ) ) {
			err() << "error: --lodv-check is retired: the terrain texture sheets are "
					 ".lodt now -- use --lodt-check <file.lodt>" << Qt::endl;
			err().flush();
			return 2;
		}
		else if ( t == QLatin1String( "--corpus-hash" ) ) lgCorpusHash = true;
		else if ( t == QLatin1String( "--geomorph" ) ) lgGeomorph = true;
		else if ( t == QLatin1String( "--terrain-identity" ) ) lgTerrainIdentity = true;
		else if ( t == QLatin1String( "--no-terrain-identity" ) ) lgTerrainIdentity = false;
		else if ( t == QLatin1String( "--impostors" ) ) lgImpostors = next();
		else if ( t == QLatin1String( "--impostors-from-level" ) ) lgImpostorFromLevel = next().toInt();
		/* `all` was retired by bungo on 2026-09-11 07:0x ("I've only wanted
		 * trees for the impostors"), so it refuses here BY NAME rather than
		 * quietly listing every base with LOD -- a script that still passes it
		 * is asking for a card library nobody wants, and a silent downgrade to
		 * `missing` would hand it one that merely looks smaller. */
		else if ( t == QLatin1String( "--candidates" ) ) {
			lgCandidates = next();
			if ( lgCandidates != QLatin1String( "trees" ) && lgCandidates != QLatin1String( "missing" ) ) {
				err() << "error: --candidates takes trees or missing; '" << lgCandidates
					  << "' is not offered" << Qt::endl;
				if ( lgCandidates == QLatin1String( "all" ) )
					err() << "       `all` was retired on 2026-09-11: impostor cards are trees only, "
						  << "and `missing` is the way back to a card for any empty far slot" << Qt::endl;
				return 2;
			}
		}
		else if ( t == QLatin1String( "--trees-only" ) ) lgTreesOnly = true;
		else if ( t == QLatin1String( "--no-trees-only" ) ) lgTreesOnly = false;
		else if ( t == QLatin1String( "--atlas" ) ) lgAtlas = true;
		else if ( t == QLatin1String( "--no-atlas" ) ) lgAtlas = false;
		else if ( t == QLatin1String( "--arrays" ) ) lgArrays = true;
		else if ( t == QLatin1String( "--no-arrays" ) ) lgArrays = false;
		else if ( t == QLatin1String( "--merge" ) ) lgMerge = true;
		else if ( t == QLatin1String( "--no-merge" ) ) lgMerge = false;
		else if ( t == QLatin1String( "--atlas-bc1" ) ) lgAtlasBc1 = true;
		else if ( t == QLatin1String( "--slot-fallback" ) ) lgSlotFallback = true;
		else if ( t == QLatin1String( "--no-simplify" ) ) lgSimplify.enabled = false;
		else if ( t == QLatin1String( "--simplify8" ) ) lgSimplify.ratio8 = next().toFloat();
		else if ( t == QLatin1String( "--simplify16" ) ) lgSimplify.ratio16 = next().toFloat();
		else if ( t == QLatin1String( "--simplify32" ) ) lgSimplify.ratio32 = next().toFloat();
		else if ( t == QLatin1String( "--simplify-error" ) ) lgSimplify.errorWorld = next().toFloat();
		else if ( t == QLatin1String( "--dump-geometry" ) ) lgDumpGeometry = next();
		else if ( t == QLatin1String( "--list-impostor-candidates" ) ) lgListCandidates = true;
		else if ( t == QLatin1String( "--card-half-aux" ) ) lgCardAuxDiv = 2;
		else if ( t == QLatin1String( "--roundtrip" ) ) roundTrip = true;
		else if ( t == QLatin1String( "--constraints" ) ) constraintsOnly = true;
		else if ( t == QLatin1String( "--skeleton" ) ) skeletonOnly = true;
		else if ( t == QLatin1String( "--bodies" ) ) bodiesOnly = true;
		// --attach applies to the NEXT --add and then clears, so a command line
		// reads left to right: --attach L_Pauldron --add arm_fx.nif
		else if ( t == QLatin1String( "--attach" ) ) pendingAttach = next();
		else if ( t == QLatin1String( "--add" ) ) {
			adds << QDir::current().filePath( next() );
			addAttach << pendingAttach;
			pendingAttach.clear();
		}
		else if ( t == QLatin1String( "--no-dedupe" ) ) noDedupe = true;
		else if ( t == QLatin1String( "--save" ) ) saveName = next();
		else if ( t == QLatin1String( "--apply" ) ) applyName = next();
		else if ( t == QLatin1String( "--blend" ) ) blend = next().toFloat();
		else if ( t == QLatin1String( "--time" ) ) freezeTime = next().toFloat();
		else if ( t == QLatin1String( "--keep-graph" ) ) keepGraph = true;
		else if ( t == QLatin1String( "--no-zoom-target" ) ) noZoomTarget = true;
		else if ( t == QLatin1String( "--keep-particles" ) ) keepParticles = true;
		else if ( t == QLatin1String( "--keep-effects" ) ) keepEffects = true;
		else if ( t == QLatin1String( "--from" ) ) tnFrom.append( next().toInt() );
		else if ( t == QLatin1String( "--to" ) ) tnTo = next().toInt();
		else if ( t == QLatin1String( "--mapping" ) ) tnMapping = next().toInt();
		else if ( t == QLatin1String( "--mix" ) ) tnMix = next().toFloat();
		else if ( t == QLatin1String( "--steps" ) ) steps = next().toInt();
		else if ( t == QLatin1String( "--substeps" ) ) substeps = next().toInt();
		else if ( t == QLatin1String( "--iterations" ) ) iterations = next().toInt();
		else if ( t == QLatin1String( "--only-limit" ) ) onlyLimit = next();
		else if ( t == QLatin1String( "--ground" ) ) useGround = true;
		else if ( t == QLatin1String( "--no-self" ) ) noSelf = true;
		else if ( t == QLatin1String( "--drop" ) ) drop = true;
		else if ( t == QLatin1String( "--jointed-only" ) ) jointedOnly = true;
		else if ( t == QLatin1String( "--drag" ) ) dragBody = next().toInt();
		else if ( t == QLatin1String( "--drag-spring" ) ) dragSpring = true;
		else if ( t == QLatin1String( "--drag-firmness" ) ) { dragSpring = true; dragFirmness = next().toFloat(); }
		else if ( t == QLatin1String( "--no-limits" ) ) noLimits = true;
		else if ( t == QLatin1String( "--trace" ) ) verboseSim = true;
		else if ( t == QLatin1String( "--size" ) ) cubeSize = next().toFloat();
		// `new --cube`: the fixture scene, not the program's new document
		else if ( t == QLatin1String( "--cube" ) ) wantCube = true;
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

	if ( !missingValueFor.isEmpty() ) {
		err() << "error: " << missingValueFor << " needs a value" << Qt::endl;
		err().flush();
		return 2;
	}

	/* `--roads-legacy` IS THE WAY BACK, and the way back is ROADS1's whole
	 * pipeline: the max-z overwrite, the full-detail diffuse sample, the raised
	 * families and the sidewalks all painted. Anything the caller named
	 * explicitly on the same command line still wins, so the flag can also be
	 * used to move exactly one thing away from ROADS1. */
	if ( lgRoadsLegacy ) {
		if ( !lgRoadDetailSet )
			lgCover.roadDetail = 1.0f;
		/* ROADS1 painted every shape in a road model, the verge included. */
		if ( !lgRoadGroundPaintSet )
			lgCover.roadGroundPaint = 1.0f;
		/* A no-op while the default is 1.0, and written anyway so that the
		 * way back stays the way back if the default is ever moved. */
		if ( !lgRoadOpacitySet )
			lgCover.roadOpacity = 1.0f;
		if ( !lgRoadRaisedSet )
			lgCover.roadRaised = true;
		if ( !lgRoadSidewalksSet )
			lgCover.roadSidewalks = true;
	}

	if ( cmd == QLatin1String( "new" ) ) {
		if ( !initModelLayer() ) { err().flush(); return 1; }
		const int rc = cmdNew( outFile, wantCube, cubeSize );
		out().flush();
		err().flush();
		return rc;
	}

	/* lane ARCHLOCK1's CLI refuter. Early, beside `spells`, because it takes
	 * its file itself (through a QBuffer, which is the whole point) and must
	 * not fall into the <file> check below. */
	if ( cmd == QLatin1String( "archlock-probe" ) ) {
		if ( !initModelLayer() ) { err().flush(); return 1; }
		const int rc = cmdArchLockProbe( file, lgDataRoot, lgProbe );
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

	/* The solver self-test builds its own bodies, a .btd conversion reads a
	 * whole landscape that is not a plugin, and asking where an asset comes from
	 * (--probe / --print-source) is a question about the resource stack, not
	 * about a plugin -- none of them needs a <file>.
	 *
	 * ADD EVERY NEW QUESTION ABOUT A FILE WE WROTE TO THIS LIST. Three have been
	 * forgotten so far (--dump-geometry, --lodm-check, --lodt-check) and the
	 * symptom is never a compile error: the question answers "needs a <file>",
	 * the harness that asks it measures nothing, and its checks fail pointing at
	 * the feature instead of at this line. --corpus-hash is correctly ABSENT: it
	 * hashes a plugin's VHGT payloads and really does need one. */
	if ( file.isEmpty() && !( cmd == QLatin1String( "simulate" ) && selfTest )
		&& !( cmd == QLatin1String( "lodgen" )
			&& ( !lgBtdPath.isEmpty() || !lgProbe.isEmpty() || !lgDumpShapes.isEmpty()
				|| !lgDumpGeometry.isEmpty() || !lgLodmCheck.isEmpty() || !lgLodvCheck.isEmpty()
				|| lgPrintSource || lgListFiles > 0 || !lgMo2Profile.isEmpty()
				/* `--bake-record` reads a file and diffs an OPTIONAL plugin list:
				 * with none it prints the record and says `diff n/a`, which is
				 * the useful answer when all you have is the bake's output
				 * folder (lane BAKEREC1, 2026-09-17). */
				|| !gLgBakeRecord.isEmpty() ) ) ) {
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

	/* THE SOURCE, before anything reads an asset (bungo, 2026-09-06: "we can
	 * toggle either specified bake, where we select our plugins, archives or
	 * loose files and their order, or we select a MO2 automatic bake").
	 *
	 *   --resource <folder|archive>   repeatable, MO2's order: the LAST one
	 *                                 given overrides the earlier ones
	 *   --plugins-txt <file>          take the plugin list from a plugins.txt
	 *   --mo2                         both: the profile's plugins.txt from
	 *                                 %LOCALAPPDATA%, the stack from the Data
	 *                                 folder's archives in plugin order
	 *
	 * -no-gui never brings the game manager up, so this stack is the only
	 * resource set a CLI run has besides --data-root. */
	if ( cmd == QLatin1String( "lodgen" ) ) {
		QStringList stack = lgResources;
		QStringList mo2Plugins;
		if ( !lgMo2Profile.isEmpty() ) {
			/* His MO2 load order read off disk, no usvfs (lane LOADORDER1,
			 * src/lodgenloadorder.h): the plugins as full paths, masters first,
			 * and the stack Data -> mods bottom-up -> overwrite, --resource above. */
			if ( !lodgenApplyMo2Profile( lgMo2Profile, lgMo2Mods, lgDataRoot, lgResources,
					lgMo2 || !lgPluginsTxt.isEmpty(), &stack, &file, out(), err() ) )
				return 2;
		} else if ( lgMo2 || !lgPluginsTxt.isEmpty() ) {
			/* The Data folder MO2 virtualises. --data-root when given (it IS a
			 * Data folder), else the folder the first plugin argument sits in,
			 * else the game path the manager recorded in QSettings. */
			QString dataDir = lgDataRoot;
			if ( dataDir.isEmpty() && !file.isEmpty() )
				dataDir = QFileInfo( file.section( QChar( ',' ), 0, 0 ) ).absolutePath();
			if ( dataDir.isEmpty() ) {
				QSettings s;
				const QVariantMap paths = s.value( QStringLiteral( "Game Paths" ) ).toMap();
				const QString p = paths.value( QStringLiteral( "Fallout 4" ) ).toString();
				if ( !p.isEmpty() )
					dataDir = p + QStringLiteral( "/Data" );
			}
			const QString txt = lgPluginsTxt.isEmpty() ? lodgenPluginsTxtPath() : lgPluginsTxt;
			QString perr;
			mo2Plugins = lodgenReadPluginsTxt( txt, &perr );
			out() << "plugins-txt: " << txt << Qt::endl;
			if ( !perr.isEmpty() )
				err() << "warning: " << perr << Qt::endl;
			out() << "data: " << dataDir << Qt::endl;
			if ( lgMo2 ) {
				out() << "mo2: " << ( lodgenUnderMo2() ? "yes" : "no (not launched from Mod Organizer 2)" )
					  << Qt::endl;
				stack = lodgenMo2Stack( dataDir, mo2Plugins ) + lgResources;
			}
			/* the plugin list becomes the comma list EsmFile merges, in load order,
			 * led by the masters plugins.txt never lists (Fallout4.esm, DLC, CC);
			 * a plugin not in Data is refused by name (lane LOADORDER1) */
			QStringList resolved;
			QString rerr;
			if ( !mo2Plugins.isEmpty()
				&& !lodgenLoadOrderFromPluginsTxt( dataDir, mo2Plugins, &resolved, &rerr ) ) {
				err() << "error: --plugins-txt refused: " << rerr << Qt::endl;
				return 2;
			}
			out() << "plugins: " << resolved.size() << Qt::endl;
			for ( int i = 0; i < resolved.size(); i++ )
				out() << "plugin " << i << ": " << resolved.at( i ) << Qt::endl;
			if ( !resolved.isEmpty() )
				file = resolved.join( QChar( ',' ) );
		}
		lodgenSetResources( stack );
		/* The stack the record writes down (lane BAKEREC1, 2026-09-17): what the
		 * run was actually given, after --mo2 has expanded a profile, in the order
		 * the bake will search it. Read back from lodgenResources(), never from
		 * the local that was handed to it. */
		gLgResourceStack = lodgenResources();
		if ( lgPrintSource ) {
			const QStringList set = lodgenResources();
			out() << "source: " << ( !lgMo2Profile.isEmpty() ? "mo2-profile" : lgMo2 ? "mo2" : "specified" )
				  << Qt::endl;
			out() << "resources: " << set.size() << " (last wins)" << Qt::endl;
			for ( int i = 0; i < set.size(); i++ )
				out() << "resource " << i << ": " << set.at( i ) << Qt::endl;
			const QStringList search = lodgenResourceSearchPaths();
			out() << "search: " << search.size() << " (first wins)" << Qt::endl;
			for ( int i = 0; i < search.size(); i++ )
				out() << "search " << i << ": " << search.at( i ) << Qt::endl;
		}
		if ( lgListFiles > 0 ) {
			// what the stack's own index holds, so a harness can pick a real
			// path out of an archive instead of guessing one
			const QStringList files = lodgenListResourceFiles( lgListFiles );
			out() << "files: " << files.size() << Qt::endl;
			for ( const QString & f : files )
				out() << "file: " << f << Qt::endl;
		}
		if ( !lgProbe.isEmpty() ) {
			QString entry, kind, path;
			QByteArray bytes;
			const bool found = lodgenProbeAsset( lgDataRoot, lgProbe, &entry, &kind, &path, &bytes );
			out() << "probe: " << lgProbe << Qt::endl;
			out() << "found: " << ( found ? "yes" : "no" ) << Qt::endl;
			if ( found ) {
				out() << "entry: " << ( entry.isEmpty() ? QStringLiteral( "(not the stack)" ) : entry ) << Qt::endl;
				out() << "kind: " << kind << Qt::endl;
				out() << "path: " << path << Qt::endl;
				out() << "size: " << bytes.size() << Qt::endl;
				out() << "sha1: "
					  << QString::fromLatin1( QCryptographicHash::hash( bytes,
							QCryptographicHash::Sha1 ).toHex() ) << Qt::endl;
				if ( !lgProbeOut.isEmpty() ) {
					QFile f( lgProbeOut );
					if ( f.open( QIODevice::WriteOnly ) )
						f.write( bytes );
					out() << "wrote: " << lgProbeOut << Qt::endl;
				}
			}
			out().flush();
			err().flush();
			return found ? 0 : 1;
		}
		/* --dump-shapes is a QUESTION about a written chunk: what does each of
		 * its shapes carry? It exists so a gate can check a generated LOD
		 * material against the SOURCE and not against the pass that wrote it -
		 * an emissiveScale read back out of the sidecar that produced it is not
		 * a measurement. One line per shape:
		 *
		 *   S <block> flags1 <u> ownemit <0|1> emit <r> <g> <b> mult <f>
		 *     smooth <f> spec <f> alpha <0|1> tex0 <path>
		 *
		 * The colour is the emissive colour the chunk builder carried over from
		 * the source's BGSM (or its shader property), which is what the legacy
		 * `_g` sheet is multiplied by, and `mult` x `ownemit` is the set's
		 * `emissiveScale` (docs/LODGEN_IMPOSTOR_SPEC.md). */
		if ( !lgDumpShapes.isEmpty() ) {
			NifModel dnif;
			if ( !loadNif( dnif, lgDumpShapes ) ) {
				err().flush();
				return 1;
			}
			int shapes = 0;
			out() << "# lodgen dump-shapes 1 " << lgDumpShapes << Qt::endl;
			for ( int b = 0; b < dnif.getBlockCount(); b++ ) {
				const QModelIndex iShape = dnif.getBlockIndex( b );
				if ( !dnif.blockInherits( iShape, "BSTriShape" ) )
					continue;
				const QModelIndex iShader = dnif.getBlockIndex( dnif.getLink( iShape, "Shader Property" ) );
				if ( !iShader.isValid() || !dnif.isNiBlock( iShader, "BSLightingShaderProperty" ) )
					continue;
				const quint32 f1 = dnif.get<quint32>( iShader, "Shader Flags 1" );
				const Color3 ec = dnif.get<Color3>( iShader, "Emissive Color" );
				QString tex0;
				const QModelIndex iTexSet = dnif.getBlockIndex( dnif.getLink( iShader, "Texture Set" ) );
				if ( iTexSet.isValid() ) {
					const QModelIndex iArr = dnif.getIndex( iTexSet, "Textures" );
					if ( iArr.isValid() )
						tex0 = dnif.get<QString>( dnif.getIndex( iArr, 0 ) );
				}
				out() << "S " << b
					<< " flags1 " << f1
					<< " ownemit " << ( ( f1 & 0x400000U ) ? 1 : 0 )
					<< " emit " << ec.red() << " " << ec.green() << " " << ec.blue()
					<< " mult " << dnif.get<float>( iShader, "Emissive Multiple" )
					<< " smooth " << dnif.get<float>( iShader, "Smoothness" )
					<< " spec " << dnif.get<float>( iShader, "Specular Strength" )
					<< " alpha " << ( dnif.getBlockIndex( dnif.getLink( iShape, "Alpha Property" ) ).isValid() ? 1 : 0 )
					<< " tex0 " << ( tex0.isEmpty() ? QStringLiteral( "-" ) : tex0 ) << Qt::endl;
				shapes++;
			}
			out() << "shapes: " << shapes << Qt::endl;
			out().flush();
			err().flush();
			return shapes ? 0 : 1;
		}
		/* --dump-geometry is the other question a gate has to ask of a written
		 * chunk: what does each shape WEIGH, and do the invariants the file
		 * itself asserts still hold? One `G` line per shape and one `i` line
		 * listing that shape's object identity indices, so a harness can weigh
		 * and diff two builds of the same chunk without a NIF reader of its
		 * own. Everything printed is a RAW COUNT — the comparisons belong to
		 * the harness, and every count here can be non-zero on a real file:
		 *
		 *   G <block> alpha <0|1> scale <s> verts <n> tris <n> segs <n>
		 *     segbad <n> outofchunk <n> sphereout <n> aabbout <n>
		 *     ids <n> layers <n>
		 *   i <block> <id> <id> ...
		 *
		 * segbad counts triangles whose CENTROID falls in a different cell
		 * from the segment they are listed under (only asked when the shape
		 * has one segment per cell); outofchunk counts centroids outside the
		 * chunk's own 4096-unit miniature square; sphereout and aabbout count
		 * vertices outside the shape's bounding sphere and its node's
		 * multi-bound box. The tolerance is a half-precision ulp at 4096
		 * (4 miniature units), because a chunk's positions are halves while
		 * the bounds that contain them were computed from floats. */
		if ( !lgDumpGeometry.isEmpty() ) {
			NifModel dnif;
			if ( !loadNif( dnif, lgDumpGeometry ) ) {
				err().flush();
				return 1;
			}
			const QStringList nameParts = QFileInfo( lgDumpGeometry ).fileName().split( QChar( '.' ) );
			const int gdim = nameParts.size() >= 5 ? nameParts[1].toInt() : 0;
			constexpr float EPS = 4.0f;                       // one half-float ulp at 4096
			out() << "# lodgen dump-geometry 1 " << lgDumpGeometry << " dim " << gdim << Qt::endl;
			int shapes = 0;
			qint64 totalVerts = 0, totalTris = 0;
			for ( int b = 0; b < dnif.getBlockCount(); b++ ) {
				const QModelIndex iShape = dnif.getBlockIndex( b );
				if ( !dnif.blockInherits( iShape, "BSTriShape" ) )
					continue;
				const QModelIndex iVD = dnif.getIndex( iShape, "Vertex Data" );
				const int nv = int( dnif.get<quint32>( iShape, "Num Vertices" ) );
				if ( !iVD.isValid() || nv <= 0 )
					continue;
				const BSVertexDesc desc( dnif.get<BSVertexDesc>( iShape, "Vertex Desc" ) );
				const bool full = ( desc.GetFlags() & VertexFlags::VF_FULLPREC );
				const bool colors = ( desc.GetFlags() & VertexFlags::VF_COLORS );
				const bool uv2 = ( desc.GetFlags() & VertexFlags::VF_UV_2 );
				const float scale = dnif.get<float>( iShape, "Scale" );
				QVector<Vector3> pos( nv );
				QSet<int> ids, layers;
				for ( int v = 0; v < nv; v++ ) {
					const QModelIndex row = dnif.index( v, 0, iVD );
					pos[v] = full ? dnif.get<Vector3>( row, "Vertex" )
						: Vector3( dnif.get<HalfVector3>( row, "Vertex" ) );
					if ( colors ) {
						const ByteColor4 c = dnif.get<ByteColor4>( row, "Vertex Colors" );
						ids.insert( int( c[0] * 255.0f + 0.5f ) + int( c[1] * 255.0f + 0.5f ) * 256 );
					}
					if ( uv2 ) {
						const Vector2 t = dnif.get<HalfVector2>( row, "UV 2" );
						layers.insert( qRound( t[1] ) );
					}
				}
				const QVector<Triangle> tris = dnif.getArray<Triangle>( dnif.getIndex( iShape, "Triangles" ) );
				const QModelIndex iSegs = dnif.getIndex( iShape, "Segment" );
				const int ns = iSegs.isValid() ? dnif.rowCount( iSegs ) : 0;
				// which segment each triangle is listed under
				QVector<int> segOf( tris.size(), 0 );
				for ( int s = 0; s < ns; s++ ) {
					const QModelIndex seg = dnif.index( s, 0, iSegs );
					const int start = int( dnif.get<quint32>( seg, "Start Index" ) ) / 3;
					const int count = int( dnif.get<quint32>( seg, "Num Primitives" ) );
					for ( int t = start; t < start + count && t < tris.size(); t++ )
						segOf[t] = s;
				}
				const int cells = gdim > 0 ? gdim * gdim : 0;
				const float cellSpan = gdim > 0 ? 4096.0f / float( gdim ) : 4096.0f;
				int segbad = 0, outofchunk = 0;
				for ( int t = 0; t < tris.size(); t++ ) {
					const Vector3 c = ( pos[tris[t].v1()] + pos[tris[t].v2()] + pos[tris[t].v3()] ) * ( 1.0f / 3.0f );
					if ( c[0] < -EPS || c[1] < -EPS || c[0] > 4096.0f + EPS || c[1] > 4096.0f + EPS )
						outofchunk++;
					if ( ns == cells && cells > 1 ) {
						const int lx = qBound( 0, int( c[0] / cellSpan ), gdim - 1 );
						const int ly = qBound( 0, int( c[1] / cellSpan ), gdim - 1 );
						if ( ly * gdim + lx != segOf[t] )
							segbad++;
					}
				}
				int sphereout = 0, aabbout = 0;
				const QModelIndex iBound = dnif.getIndex( iShape, "Bounding Sphere" );
				if ( iBound.isValid() ) {
					const Vector3 c = dnif.get<Vector3>( iBound, "Center" );
					const float r = dnif.get<float>( iBound, "Radius" );
					for ( const Vector3 & p : pos )
						if ( ( p - c ).length() > r + EPS )
							sphereout++;
				}
				const QModelIndex iNode = dnif.getBlockIndex( dnif.getParent( b ) );
				const QModelIndex iMB = iNode.isValid()
					? dnif.getBlockIndex( dnif.getLink( iNode, "Multi Bound" ) ) : QModelIndex();
				const QModelIndex iBox = iMB.isValid() ? dnif.getBlockIndex( dnif.getLink( iMB, "Data" ) ) : QModelIndex();
				if ( iBox.isValid() ) {
					const Vector3 bp = dnif.get<Vector3>( iBox, "Position" ), be = dnif.get<Vector3>( iBox, "Extent" );
					const float tol = EPS * qMax( 1.0f, scale );
					for ( const Vector3 & p : pos ) {
						const Vector3 w = p * scale;
						if ( w[0] < bp[0] - be[0] - tol || w[0] > bp[0] + be[0] + tol
							|| w[1] < bp[1] - be[1] - tol || w[1] > bp[1] + be[1] + tol
							|| w[2] < bp[2] - be[2] - tol || w[2] > bp[2] + be[2] + tol )
							aabbout++;
					}
				}
				out() << "G " << b
					<< " alpha " << ( dnif.getBlockIndex( dnif.getLink( iShape, "Alpha Property" ) ).isValid() ? 1 : 0 )
					<< " scale " << scale
					<< " verts " << nv
					<< " tris " << tris.size()
					<< " segs " << ns
					<< " segbad " << segbad
					<< " outofchunk " << outofchunk
					<< " sphereout " << sphereout
					<< " aabbout " << aabbout
					<< " ids " << ids.size()
					<< " layers " << layers.size() << Qt::endl;
				if ( !ids.isEmpty() ) {
					QList<int> sorted = ids.values();
					std::sort( sorted.begin(), sorted.end() );
					out() << "i " << b;
					for ( int id : sorted )
						out() << " " << id;
					out() << Qt::endl;
				}
				shapes++;
				totalVerts += nv;
				totalTris += tris.size();
			}
			out() << "total shapes " << shapes << " verts " << totalVerts << " tris " << totalTris << Qt::endl;
			out().flush();
			err().flush();
			return shapes ? 0 : 1;
		}
		// --print-source and --list-files are QUESTIONS about the stack: they
		// answer and stop, so neither can be mistaken for a bake
		if ( lgPrintSource || lgListFiles > 0 ) {
			out().flush();
			err().flush();
			return 0;
		}
	}

	int rc = 2;
	if ( cmd == QLatin1String( "pbrm-resolve" ) )
		rc = cmdPbrmResolve( file );
	else if ( cmd == QLatin1String( "info" ) )
		rc = cmdInfo( file );
	else if ( cmd == QLatin1String( "verts" ) )
		rc = cmdVerts( file );
	else if ( cmd == QLatin1String( "list" ) )
		rc = cmdList( file, type );
	else if ( cmd == QLatin1String( "segments" ) )
		rc = cmdSegments( file, block );
	else if ( cmd == QLatin1String( "gltf" ) || cmd == QLatin1String( "gltf-export" ) )	// lane HKX4 / GLTFEXPORT1
		rc = cmdGltf( file, outFile, gltfClip, gltfBones, gltfRootMotion, gltfOpts );
	else if ( cmd == QLatin1String( "gltf-import" ) )		// lane BUILD8
		rc = cmdGltfImport( file, outFile, gltfBones, gltfFps, gltfSourceRate,
							gltfRootMotion, gltfRoute, gltfTsv, gltfSkeletonName,
							gltfRootNode );
	else if ( cmd == QLatin1String( "hkx-tsv" ) )			// lane BUILD8
		rc = cmdHkxTsv( file, outFile );
	else if ( cmd == QLatin1String( "check" ) )
		rc = cmdCheck( file, type );
	else if ( cmd == QLatin1String( "world" ) )
		rc = cmdWorld( file, block, type );
	else if ( cmd == QLatin1String( "dump" ) )
		rc = cmdDump( file, block, path, depth, maxRows, showAll );
	else if ( cmd == QLatin1String( "get" ) )
		rc = cmdGet( file, block, path );
	else if ( cmd == QLatin1String( "set" ) )
		rc = cmdSet( file, block, path, value, outFile );
	else if ( cmd == QLatin1String( "cast" ) )
		rc = cmdCast( file, spellId, block, path, outFile );
	else if ( cmd == QLatin1String( "merge" ) )
		rc = cmdMerge( file, adds, addAttach, noDedupe, outFile );
	else if ( cmd == QLatin1String( "pose" ) )
		rc = cmdPose( file, listOnly, saveName, applyName, blend, importOs, exportOs, outFile );
	else if ( cmd == QLatin1String( "freeze" ) )
		rc = cmdFreeze( file, sequence, freezeTime, keepGraph, outFile );
	else if ( cmd == QLatin1String( "transfer-normals" ) )
		rc = cmdTransferNormals( file, tnFrom, tnTo, tnMapping, tnMix, outFile );
	else if ( cmd == QLatin1String( "loading-screen" ) )
		rc = cmdLoadingScreen( file, noZoomTarget, keepParticles, keepEffects, outFile );
	else if ( cmd == QLatin1String( "simulate" ) )
		rc = cmdSimulate( file, steps > 0 ? steps : 120, substeps > 0 ? substeps : 8,
			iterations, noLimits, onlyLimit, useGround, noSelf, drop, jointedOnly,
			dragBody, dragSpring, dragFirmness, selfTest, verboseSim );
	else if ( cmd == QLatin1String( "collision" ) )
		rc = bodiesOnly ? cmdCollisionBodies( file )
			 : skeletonOnly ? cmdCollisionSkeleton( file )
			 : constraintsOnly ? cmdCollisionConstraints( file )
			 : roundTrip ? cmdCollisionRoundTrip( file, outFile )
					   : cmdCollision( file, extract ? block : -1, outFile );
	else if ( cmd == QLatin1String( "skeleton" ) )
		rc = selfTest ? cmdSkeletonSelfTest( file ) : cmdSkeleton( file, validateOnly );
	else if ( cmd == QLatin1String( "btd" ) )
		rc = cmdBtd( file, btdInfo, btdHaveRegion,
			btdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod, outFile );
	else if ( cmd == QLatin1String( "lodl" ) ) {
		/* --water-mark-selftest runs the MARKING tool's own gates
		 * (src/watermark.cpp), which rewrite the file, so it is answered
		 * here rather than inside the scene builder. */
		if ( lodtWaterMarkSelfTestOnly ) {
			QString report, werr;
			const bool ok = lodtWaterMarkSelfTest( file, &report, &werr );
			out() << report << Qt::endl;
			if ( !ok && !werr.isEmpty() )
				err() << "error: " << werr << Qt::endl;
			rc = ok ? 0 : 1;
		} else {
			rc = cmdLodt( file, btdInfo, btdHaveRegion,
				btdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod,
				lodtPlaneName, outFile, lodtWaterCensusOnly, lodtWaterSelfTestOnly );
		}
	}
	else if ( cmd == QLatin1String( "lodt" ) ) {
		err() << "error: the 'lodt' command is retired: the landscape file is .lodl "
				 "now (.lodt names the terrain texture sheets) -- use "
				 "'lodl <file.lodl>'" << Qt::endl;
		rc = 2;
	}
	/* THE MODEL LAYER ON N THREADS -- see src/nifparsestress.h and edit 3.
	 * The positional <file> is the first NIF; --stress-file adds more. */
	else if ( cmd == QLatin1String( "parsestress" ) ) {
		NifParseStressOptions so;
		if ( !file.isEmpty() )
			so.paths << file;
		so.paths << stressFiles;
		so.threads = stressThreads;
		so.reps = stressReps;
		so.sabotage = stressSabotage;
		rc = nifParseStressRun( so, out() ) ? 1 : 0;
	}
	else if ( cmd == QLatin1String( "lodgen" ) ) {
	/* THE .lodl REFUSAL (lane GROUND1), in words, the way --incremental refuses
	 * --native above.
	 *
	 * The .lodl's AO plane is computed by ONE function (lodtComputeAo,
	 * src/lodtfile.cpp) from the container's own stored height word and nothing
	 * else, and that same function serves both the writer and --refresh-ao.
	 * That is what makes a refreshed plane byte-identical to a written one -- it
	 * is the plane's stated contract, not a coincidence. Putting the placed
	 * objects into it would make --refresh-ao produce a DIFFERENT plane from the
	 * one that was written, silently, because the objects are not in the file.
	 * The two honest alternatives -- a header bit saying "written with objects"
	 * so refresh can refuse, or a stored object-height plane -- are both .lodl
	 * format bumps, and this lane does not bump the format.
	 *
	 * So the sheets carry the object term and the .lodl keeps the bytes it has.
	 * A run that asks for both is refused here rather than quietly writing one
	 * of them without it. */
		if ( lgCover.terrainObjectAo && !lgLodtDir.isEmpty() ) {
			err() << "refused: --terrain-object-ao writes the object occlusion into the "
					 "terrain SHEETS, while the .lodl's own AO plane is computed from the "
					 "container's stored heights by the one function that also serves "
					 "--refresh-ao -- putting objects there would make a refreshed plane "
					 "differ from the written one without saying so." << Qt::endl;
			err() << "  bake the sheets with --terrain-object-ao and write the .lodl in a "
					 "separate run without it; the .lodl plane is unchanged either way."
				  << Qt::endl;
			err().flush();
			return 2;
		}
		if ( lgVtDensity ) {
			if ( lgVtFinestGiven || lgVtContentGiven ) {
				err() << "error: --vt-density names a --vt-finest / --vt-content pair; give "
						 "one or the other, not both" << Qt::endl;
				return 2;
			}
			if ( lgVtDensity == 32 ) {
				lgVt.finestDim = 2;
				lgVt.content = 256;
			} else if ( lgVtDensity == 16 ) {
				lgVt.finestDim = 2;
				lgVt.content = 512;
			} else if ( lgVtDensity == 8 ) {
				lgVt.finestDim = 1;
				lgVt.content = 512;
			} else {
				err() << "error: --vt-density must be 32, 16 or 8 (world units a texel at the "
						 "finest level)" << Qt::endl;
				return 2;
			}
		}
		rc = cmdLodgen( file, lgListWorldspaces, lgWorldspace,
			lgHaveCell, lgCell[0], lgCell[1],
			lgHaveTerrain, lgChunk[0], lgChunk[1], lgDim, outFile,
			lgHaveRegion, lgRegion, lgOutDir,
			lgHaveObjects, lgDataRoot, lgIdentity, lgTexDir, lgGeomorph,
			lgTerrainIdentity, lgImpostors, lgImpostorFromLevel, lgCandidates, lgListCandidates, lgAtlas, lgArrays, lgMerge, lgBakeAO,
			lgCullBuried, lgCullMargin, lgAoGrey, lgAoSkirt, lgWaterSubdiv,
			lgShoreDenser, lgShoreDensity, lgTargetTris,
			lgHeightmapDir, lgHeightmapSize, lgLodtDir, lgBtdPath, lgBtdProbe,
			lgLodtVerify, lgDumpLand, lgDumpLayers, lgRefreshAo,
			lgSlotFallback, lgAtlasBc1, lgSimplify, lgCover,
			lgVt, lgVtDir, lgVtBtr, lgVtEstimate, lgLodmCheck, lgLodvCheck, lgCorpusHash,
			lgCardAuxDiv, lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture,
			lgNativeMeshReport, lgNativeVerifyCorpus, lgNativeLadder, lgNativeOccluders,
			lgLibraryNear, lgNativeLadderFoliage, lgNativeSilhouette, lgNativePlacementAo, lgNativeVertexAo, lgLodiV7,
			lgScrappable, lgIdentityJoinLegacy, lgIdentityJoinGap,
			lgTreesOnly, lgAggregate, lgAggMin, lgAggTile, lgAggViews );
	}
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
