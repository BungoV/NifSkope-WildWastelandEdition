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
