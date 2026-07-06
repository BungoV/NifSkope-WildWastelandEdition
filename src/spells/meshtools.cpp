/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellbook.h"
#include "nifsnapshot.h"
#include "message.h"

#include "data/nifitem.h"
#include "data/nifvalue.h"
#include "ui/widgets/ddspreview.h"
#include "ui/widgets/filebrowser.h"
#include "ui/widgets/timeline.h"
#include "glview.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QUrl>

#include <cstring>
#include <iterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

/*! \file meshtools.cpp
 *  \brief Material/texture find & replace manager and node-name authority spells.
 */

//! True for a directly readable string item (sized strings / file paths and the
//! string-table refs). Deliberately excludes tStringOffset, which resolveString
//! does not support and which only appears inside controller-sequence blocks.
static bool tlIsStringItem( const NifItem * item )
{
	if ( !item )
		return false;
	NifValue::Type t = item->value().type();
	return item->value().isString() || t == NifValue::tStringIndex;
}

//! Does the text look like a material or texture resource path? (Not behavior
//! graphs / collision - those are not textures or materials.)
static bool tlLooksLikeResource( const QString & s )
{
	if ( s.isEmpty() )
		return false;
	static const char * exts[] = { ".dds", ".bgsm", ".bgem", ".tga" };
	QString low = s.toLower();
	for ( const char * e : exts ) {
		if ( low.endsWith( QLatin1String( e ) ) )
			return true;
	}
	return low.contains( QLatin1String( "textures" ) ) || low.contains( QLatin1String( "materials" ) );
}

//! Nearest geometry / owning block for a shader/texture property, for context
static QString tlOwnerLabel( NifModel * nif, const QModelIndex & iItem )
{
	// walk up to the containing block, then find who links to it
	QModelIndex iBlock = iItem;
	while ( iBlock.isValid() && iBlock.parent().isValid() )
		iBlock = iBlock.parent();
	int bn = nif->getBlockNumber( iBlock );
	if ( bn < 0 )
		return QString();
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iOther = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iOther, "NiAVObject" ) )
			continue;
		const auto links = nif->getChildLinks( b );
		if ( links.contains( bn ) ) {
			QString nm = nif->resolveString( iOther, "Name" );
			return QString( "%1 %2%3" ).arg( b ).arg( nif->itemName( iOther ),
				nm.isEmpty() ? QString() : QStringLiteral( " \"" ) + nm + QStringLiteral( "\"" ) );
		}
	}
	return QString();
}

//! Recursively collect resource-path string items under a block
static void tlCollectResourceStrings( NifModel * nif, const QModelIndex & parent,
	QVector<QPersistentModelIndex> & out, QStringList & labels, QStringList & owners,
	const QString & owner, const QString & prefix, int depth = 0 )
{
	if ( depth > 12 )
		return;
	for ( int r = 0; r < nif->rowCount( parent ); r++ ) {
		QModelIndex row = nif->index( r, 0, parent );
		if ( !row.isValid() )
			continue;
		const NifItem * item = static_cast<const NifItem *>( row.internalPointer() );
		QString nm = nif->itemName( row );
		QString path = prefix.isEmpty() ? nm : prefix + QStringLiteral( "/" ) + nm;
		if ( tlIsStringItem( item ) ) {
			QString val = nif->resolveString( row );
			if ( tlLooksLikeResource( val ) ) {
				out.append( QPersistentModelIndex( row ) );
				labels.append( path );
				owners.append( owner );
			}
		}
		if ( nif->rowCount( row ) > 0 )
			tlCollectResourceStrings( nif, row, out, labels, owners, owner, path, depth + 1 );
	}
}


//! Archive listing filters for the resource browsers
static bool tlTexFileFilter( [[maybe_unused]] void * p, const std::string_view & s )
{
	return s.starts_with( "textures/" ) && s.ends_with( ".dds" );
}

static bool tlMatFileFilter( [[maybe_unused]] void * p, const std::string_view & s )
{
	return s.ends_with( ".bgsm" ) || s.ends_with( ".bgem" );
}

//! Standardize a Bethesda resource path: backslashes, no doubles, no leading
//! slash, lowercase, no stray quotes/whitespace
static QString tlNormalizeResourcePath( QString s )
{
	s = s.trimmed();
	s.remove( QChar( '"' ) );
	s.replace( QChar( '/' ), QChar( '\\' ) );
	while ( s.contains( QLatin1String( "\\\\" ) ) )
		s.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
	while ( s.startsWith( QChar( '\\' ) ) )
		s.remove( 0, 1 );
	return s.toLower();
}

//! Path table with drag & drop: dragging a path cell drops its text onto
//! another row's path (also accepts plain-text drops from outside)
class TlPathTable final : public QTableWidget
{
public:
	TlPathTable( QWidget * parent = nullptr ) : QTableWidget( 0, 3, parent )
	{
		setDragEnabled( true );
		setAcceptDrops( true );
		viewport()->setAcceptDrops( true );
		setDropIndicatorShown( true );
		setDragDropMode( QAbstractItemView::DragDrop );
	}

protected:
	void startDrag( Qt::DropActions ) override
	{
		QTableWidgetItem * it = currentItem();
		if ( !it )
			return;
		QTableWidgetItem * src = item( it->row(), 2 );
		if ( !src || src->text().isEmpty() )
			return;
		QMimeData * md = new QMimeData;
		md->setText( src->text() );
		QDrag * drag = new QDrag( this );
		drag->setMimeData( md );
		drag->exec( Qt::CopyAction );
	}
	void dragEnterEvent( QDragEnterEvent * e ) override
	{
		if ( e->mimeData()->hasText() )
			e->acceptProposedAction();
	}
	void dragMoveEvent( QDragMoveEvent * e ) override
	{
		if ( e->mimeData()->hasText() )
			e->acceptProposedAction();
	}
	void dropEvent( QDropEvent * e ) override
	{
		QTableWidgetItem * it = itemAt( e->position().toPoint() );
		if ( it && e->mimeData()->hasText() ) {
			QTableWidgetItem * dst = item( it->row(), 2 );
			if ( dst )
				dst->setText( e->mimeData()->text() );	// itemChanged applies + normalizes
			e->acceptProposedAction();
		}
	}
};

//! Resolve a resource path against the loaded archives, honouring its extension
static QString tlFindResource( const NifModel * nif, const QString & path )
{
	if ( path.isEmpty() )
		return QString();
	QString low = path.toLower();
	if ( low.endsWith( QLatin1String( ".bgsm" ) ) )
		return nif->findResourceFile( path, "materials", ".bgsm" );
	if ( low.endsWith( QLatin1String( ".bgem" ) ) )
		return nif->findResourceFile( path, "materials", ".bgem" );
	if ( low.endsWith( QLatin1String( ".tga" ) ) )
		return nif->findResourceFile( path, "textures", ".tga" );
	return nif->findResourceFile( path, "textures", ".dds" );
}

//! Extract .dds paths referenced by a binary material file (format-agnostic
//! printable-string scan; works for BGSM and BGEM)
static QStringList tlScanMaterialTextures( const QByteArray & data )
{
	QStringList out;
	QByteArray cur;
	for ( char ch : data ) {
		if ( ch >= 0x20 && ch != 0x7f ) {
			cur.append( ch );
			continue;
		}
		if ( cur.size() >= 5 ) {
			QString s = QString::fromLatin1( cur ).toLower();
			if ( s.endsWith( QLatin1String( ".dds" ) ) && !out.contains( s ) )
				out.append( s );
		}
		cur.clear();
	}
	if ( cur.size() >= 5 ) {
		QString s = QString::fromLatin1( cur ).toLower();
		if ( s.endsWith( QLatin1String( ".dds" ) ) && !out.contains( s ) )
			out.append( s );
	}
	return out;
}

//! Nearest NiAVObject reachable by walking the link graph upward from the
//! given block (transitive: BSShaderTextureSet -> shader property -> shape)
static int tlOwnerBlock( NifModel * nif, int bn )
{
	QSet<int> visited;
	QVector<int> frontier;
	frontier.append( bn );
	for ( int depth = 0; depth < 8 && !frontier.isEmpty(); depth++ ) {
		QVector<int> next;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			const auto links = nif->getChildLinks( b );
			for ( int f : frontier ) {
				if ( !links.contains( f ) )
					continue;
				if ( nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" ) )
					return b;
				if ( !visited.contains( b ) ) {
					visited.insert( b );
					next.append( b );
				}
				break;
			}
		}
		frontier = next;
	}
	return -1;
}

//! Clipboard for whole BSShaderTextureSet contents
static QStringList tlCopiedTexSet;


/*
 *  BGSM / BGEM material files (Fallout 4, format version 2)
 *
 *  Field order follows ousnius' Material Editor. Opening an existing file
 *  runs a parse -> serialize -> byte-compare round trip; on any mismatch the
 *  editor opens read-only so a wrong layout can never corrupt a material.
 */

struct TlMatField
{
	const char * name;
	char type;             // f float, u u32 (hex), y byte, b bool, t texture path, s string
	const char * cond;     // present only when this earlier bool field is set
};

static const TlMatField tlMatBase[] = {
	{ "Tile Flags", 'u', nullptr },
	{ "UV Offset U", 'f', nullptr }, { "UV Offset V", 'f', nullptr },
	{ "UV Scale U", 'f', nullptr }, { "UV Scale V", 'f', nullptr },
	{ "Alpha", 'f', nullptr },
	{ "Alpha Blend Mode 0", 'y', nullptr }, { "Alpha Blend Mode 1", 'u', nullptr }, { "Alpha Blend Mode 2", 'u', nullptr },
	{ "Alpha Test Ref", 'y', nullptr }, { "Alpha Test", 'b', nullptr },
	{ "ZBuffer Write", 'b', nullptr }, { "ZBuffer Test", 'b', nullptr },
	{ "Screen Space Reflections", 'b', nullptr }, { "Wetness Control SSR", 'b', nullptr },
	{ "Decal", 'b', nullptr }, { "Two Sided", 'b', nullptr }, { "Decal No Fade", 'b', nullptr }, { "Non Occluder", 'b', nullptr },
	{ "Refraction", 'b', nullptr }, { "Refraction Falloff", 'b', nullptr }, { "Refraction Power", 'f', nullptr },
	{ "Environment Mapping", 'b', nullptr }, { "EnvMap Mask Scale", 'f', nullptr },
	{ "Grayscale To Palette Color", 'b', nullptr }
};

static const TlMatField tlMatBgsm[] = {
	{ "Diffuse Texture", 't', nullptr }, { "Normal Texture", 't', nullptr },
	{ "SmoothSpec Texture", 't', nullptr }, { "Greyscale Texture", 't', nullptr },
	{ "EnvMap Texture", 't', nullptr }, { "Glow Texture", 't', nullptr },
	{ "Inner Layer Texture", 't', nullptr }, { "Wrinkles Texture", 't', nullptr },
	{ "Displacement Texture", 't', nullptr },
	{ "Enable Editor Alpha Ref", 'b', nullptr },
	{ "Rim Lighting", 'b', nullptr }, { "Rim Power", 'f', nullptr }, { "Backlight Power", 'f', nullptr },
	{ "Subsurface Lighting", 'b', nullptr }, { "Subsurface Rolloff", 'f', nullptr },
	{ "Specular Enabled", 'b', nullptr },
	{ "Specular Color R", 'f', nullptr }, { "Specular Color G", 'f', nullptr }, { "Specular Color B", 'f', nullptr },
	{ "Specular Mult", 'f', nullptr }, { "Smoothness", 'f', nullptr },
	{ "Fresnel Power", 'f', nullptr },
	{ "Wetness Spec Scale", 'f', nullptr }, { "Wetness Spec Power", 'f', nullptr },
	{ "Wetness Min Var", 'f', nullptr }, { "Wetness EnvMap Scale", 'f', nullptr },
	{ "Wetness Fresnel Power", 'f', nullptr }, { "Wetness Metalness", 'f', nullptr },
	{ "Root Material Path", 's', nullptr },
	{ "Aniso Lighting", 'b', nullptr }, { "Emit Enabled", 'b', nullptr },
	{ "Emittance Color R", 'f', "Emit Enabled" }, { "Emittance Color G", 'f', "Emit Enabled" },
	{ "Emittance Color B", 'f', "Emit Enabled" }, { "Emittance Mult", 'f', nullptr },
	{ "Model Space Normals", 'b', nullptr }, { "External Emittance", 'b', nullptr },
	{ "Back Lighting", 'b', nullptr },
	{ "Receive Shadows", 'b', nullptr }, { "Hide Secret", 'b', nullptr }, { "Cast Shadows", 'b', nullptr },
	{ "Dissolve Fade", 'b', nullptr }, { "Assume Shadowmask", 'b', nullptr },
	{ "Glowmap", 'b', nullptr },
	{ "EnvMap Window", 'b', nullptr }, { "EnvMap Eye", 'b', nullptr },
	{ "Hair", 'b', nullptr },
	{ "Hair Tint Color R", 'f', nullptr }, { "Hair Tint Color G", 'f', nullptr }, { "Hair Tint Color B", 'f', nullptr },
	{ "Tree", 'b', nullptr }, { "Facegen", 'b', nullptr }, { "Skin Tint", 'b', nullptr }, { "Tessellate", 'b', nullptr },
	{ "Displacement Bias", 'f', nullptr }, { "Displacement Scale", 'f', nullptr },
	{ "Tessellation PN Scale", 'f', nullptr }, { "Tessellation Base Factor", 'f', nullptr },
	{ "Tessellation Fade Distance", 'f', nullptr },
	{ "Grayscale To Palette Scale", 'f', nullptr },
	{ "Skew Specular Alpha", 'b', nullptr }
};

static const TlMatField tlMatBgem[] = {
	{ "Base Texture", 't', nullptr }, { "Grayscale Texture", 't', nullptr },
	{ "EnvMap Texture", 't', nullptr }, { "Normal Texture", 't', nullptr },
	{ "EnvMap Mask Texture", 't', nullptr },
	{ "Blood Enabled", 'b', nullptr }, { "Effect Lighting Enabled", 'b', nullptr },
	{ "Falloff Enabled", 'b', nullptr }, { "Falloff Color Enabled", 'b', nullptr },
	{ "Grayscale To Palette Alpha", 'b', nullptr }, { "Soft Enabled", 'b', nullptr },
	{ "Base Color R", 'f', nullptr }, { "Base Color G", 'f', nullptr }, { "Base Color B", 'f', nullptr },
	{ "Base Color Scale", 'f', nullptr },
	{ "Falloff Start Angle", 'f', nullptr }, { "Falloff Stop Angle", 'f', nullptr },
	{ "Falloff Start Opacity", 'f', nullptr }, { "Falloff Stop Opacity", 'f', nullptr },
	{ "Lighting Influence", 'f', nullptr }, { "EnvMap Min LOD", 'y', nullptr },
	{ "Soft Depth", 'f', nullptr }
};

//! Sequential little-endian reader
struct TlMatCursor
{
	const QByteArray * d;
	int p = 0;
	bool ok = true;
	quint32 u32()
	{
		if ( p + 4 > d->size() ) { ok = false; return 0; }
		quint32 v;
		std::memcpy( &v, d->constData() + p, 4 );
		p += 4;
		return v;
	}
	float f32()
	{
		quint32 v = u32();
		float f;
		std::memcpy( &f, &v, 4 );
		return f;
	}
	quint8 u8()
	{
		if ( p + 1 > d->size() ) { ok = false; return 0; }
		return quint8( d->at( p++ ) );
	}
	QString str()
	{
		quint32 n = u32();
		if ( !ok || p + int( n ) > d->size() || n > 4096 ) { ok = false; return QString(); }
		QByteArray b( d->constData() + p, int( n ) );
		p += int( n );
		while ( b.endsWith( '\0' ) )
			b.chop( 1 );
		return QString::fromLatin1( b );
	}
};

static bool tlMatWalk( const TlMatField * spec, int n, TlMatCursor * rd, QByteArray * wr,
	QVector<QPair<QString, QVariant>> & vals )
{
	auto findVal = [&vals]( const char * nm ) -> QVariant {
		for ( const auto & v : vals ) {
			if ( v.first == QLatin1String( nm ) )
				return v.second;
		}
		return QVariant();
	};
	auto put32 = [wr]( quint32 v ) { wr->append( reinterpret_cast<const char *>( &v ), 4 ); };
	for ( int i = 0; i < n; i++ ) {
		const TlMatField & f = spec[i];
		if ( f.cond && !findVal( f.cond ).toBool() )
			continue;
		if ( rd ) {
			QVariant v;
			switch ( f.type ) {
			case 'f': v = double( rd->f32() ); break;
			case 'u': v = rd->u32(); break;
			case 'y': v = uint( rd->u8() ); break;
			case 'b': v = bool( rd->u8() != 0 ); break;
			default: v = rd->str(); break;
			}
			if ( !rd->ok )
				return false;
			vals.append( qMakePair( QString::fromLatin1( f.name ), v ) );
		} else {
			QVariant v = findVal( f.name );
			switch ( f.type ) {
			case 'f': {
				float fv = float( v.toDouble() );
				quint32 u;
				std::memcpy( &u, &fv, 4 );
				put32( u );
				break;
			}
			case 'u': put32( v.toUInt() ); break;
			case 'y': wr->append( char( v.toUInt() & 0xff ) ); break;
			case 'b': wr->append( char( v.toBool() ? 1 : 0 ) ); break;
			default: {
				QByteArray s = v.toString().toLatin1();
				s.append( '\0' );
				put32( quint32( s.size() ) );
				wr->append( s );
				break;
			}
			}
		}
	}
	return true;
}

static bool tlMatParse( const QByteArray & data, bool bgem, QVector<QPair<QString, QVariant>> & vals )
{
	TlMatCursor rd{ &data };
	quint32 sig = rd.u32(), ver = rd.u32();
	if ( sig != ( bgem ? 0x4D454742u : 0x4D534742u ) || ver != 2 )
		return false;
	if ( !tlMatWalk( tlMatBase, int( std::size( tlMatBase ) ), &rd, nullptr, vals ) )
		return false;
	const TlMatField * spec = bgem ? tlMatBgem : tlMatBgsm;
	int n = bgem ? int( std::size( tlMatBgem ) ) : int( std::size( tlMatBgsm ) );
	if ( !tlMatWalk( spec, n, &rd, nullptr, vals ) )
		return false;
	return rd.p == data.size();	// no trailing bytes = full layout coverage
}

static QByteArray tlMatSerialize( bool bgem, QVector<QPair<QString, QVariant>> & vals )
{
	QByteArray out;
	quint32 sig = bgem ? 0x4D454742u : 0x4D534742u, ver = 2;
	out.append( reinterpret_cast<const char *>( &sig ), 4 );
	out.append( reinterpret_cast<const char *>( &ver ), 4 );
	tlMatWalk( tlMatBase, int( std::size( tlMatBase ) ), nullptr, &out, vals );
	const TlMatField * spec = bgem ? tlMatBgem : tlMatBgsm;
	tlMatWalk( spec, bgem ? int( std::size( tlMatBgem ) ) : int( std::size( tlMatBgsm ) ), nullptr, &out, vals );
	return out;
}

//! Sensible defaults for a newly created material
static void tlMatDefaults( bool bgem, QVector<QPair<QString, QVariant>> & vals )
{
	auto walkDef = [&vals]( const TlMatField * spec, int n ) {
		for ( int i = 0; i < n; i++ ) {
			QVariant v;
			switch ( spec[i].type ) {
			case 'f': v = 0.0; break;
			case 'u': v = 0u; break;
			case 'y': v = 0u; break;
			case 'b': v = false; break;
			default: v = QString(); break;
			}
			vals.append( qMakePair( QString::fromLatin1( spec[i].name ), v ) );
		}
	};
	walkDef( tlMatBase, int( std::size( tlMatBase ) ) );
	const TlMatField * spec = bgem ? tlMatBgem : tlMatBgsm;
	walkDef( spec, bgem ? int( std::size( tlMatBgem ) ) : int( std::size( tlMatBgsm ) ) );
	auto set = [&vals]( const char * nm, const QVariant & v ) {
		for ( auto & p : vals ) {
			if ( p.first == QLatin1String( nm ) ) { p.second = v; return; }
		}
	};
	set( "UV Scale U", 1.0 );
	set( "UV Scale V", 1.0 );
	set( "Alpha", 1.0 );
	set( "Alpha Test Ref", 128u );
	set( "ZBuffer Write", true );
	set( "ZBuffer Test", true );
	if ( !bgem ) {
		set( "Specular Enabled", true );
		set( "Specular Color R", 1.0 );
		set( "Specular Color G", 1.0 );
		set( "Specular Color B", 1.0 );
		set( "Specular Mult", 1.0 );
		set( "Smoothness", 0.5 );
		set( "Fresnel Power", 5.0 );
		set( "Emittance Mult", 1.0 );
		set( "Receive Shadows", true );
		set( "Cast Shadows", true );
		set( "Grayscale To Palette Scale", 1.0 );
	} else {
		set( "Base Color R", 1.0 );
		set( "Base Color G", 1.0 );
		set( "Base Color B", 1.0 );
		set( "Base Color Scale", 1.0 );
		set( "Falloff Stop Angle", 1.0 );
		set( "Falloff Stop Opacity", 1.0 );
		set( "Soft Depth", 100.0 );
	}
}

//! Look up a field's type character in the specs
static char tlMatFieldType( bool bgem, const QString & name )
{
	auto find = [&name]( const TlMatField * spec, int n ) -> char {
		for ( int i = 0; i < n; i++ ) {
			if ( name == QLatin1String( spec[i].name ) )
				return spec[i].type;
		}
		return 0;
	};
	char t = find( tlMatBase, int( std::size( tlMatBase ) ) );
	if ( !t )
		t = find( bgem ? tlMatBgem : tlMatBgsm, bgem ? int( std::size( tlMatBgem ) ) : int( std::size( tlMatBgsm ) ) );
	return t ? t : 's';
}

//! View / edit / create a Fallout 4 material file in a property-grid dialog
static void tlOpenMaterialEditor( NifModel * nif, QWidget * parent, const QString & path, bool newBgem = false )
{
	bool bgem = path.isEmpty() ? newBgem : path.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
	QVector<QPair<QString, QVariant>> vals;
	bool readOnly = false;
	QString title;

	if ( !path.isEmpty() ) {
		const char * ext = bgem ? ".bgem" : ".bgsm";
		std::string fullPath = Game::GameManager::get_full_path( path, "materials/", ext );
		QByteArray data;
		if ( !nif->getGameResources().get_file( data, fullPath ) || data.isEmpty() ) {
			QMessageBox::warning( parent, QObject::tr( "Material Editor" ),
				QObject::tr( "Could not read '%1' from the loaded resources." ).arg( path ) );
			return;
		}
		if ( !tlMatParse( data, bgem, vals ) ) {
			QMessageBox::warning( parent, QObject::tr( "Material Editor" ),
				QObject::tr( "'%1' is not a Fallout 4 version-2 material (or uses an unsupported layout)." ).arg( path ) );
			return;
		}
		// round-trip safety gate: refuse to save if our writer would not
		// reproduce the original file byte for byte
		if ( tlMatSerialize( bgem, vals ) != data ) {
			readOnly = true;
			title = QObject::tr( "Material Editor - %1 [READ ONLY: layout mismatch]" ).arg( path );
		} else {
			title = QObject::tr( "Material Editor - %1" ).arg( path );
		}
	} else {
		tlMatDefaults( bgem, vals );
		title = QObject::tr( "Material Editor - new %1" ).arg( bgem ? "BGEM" : "BGSM" );
	}

	QDialog dlg( parent );
	dlg.setWindowTitle( title );
	QVBoxLayout * lay = new QVBoxLayout( &dlg );

	QSplitter * split = new QSplitter( Qt::Horizontal, &dlg );
	QTableWidget * grid = new QTableWidget( vals.size(), 2, split );
	grid->setHorizontalHeaderLabels( { QObject::tr( "Property" ), QObject::tr( "Value" ) } );
	grid->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	grid->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
	grid->verticalHeader()->setVisible( false );
	for ( int i = 0; i < vals.size(); i++ ) {
		QTableWidgetItem * k = new QTableWidgetItem( vals.at( i ).first );
		k->setFlags( k->flags() & ~Qt::ItemIsEditable );
		grid->setItem( i, 0, k );
		char t = tlMatFieldType( bgem, vals.at( i ).first );
		QTableWidgetItem * v = new QTableWidgetItem;
		if ( t == 'b' ) {
			v->setFlags( ( v->flags() | Qt::ItemIsUserCheckable ) & ~Qt::ItemIsEditable );
			v->setCheckState( vals.at( i ).second.toBool() ? Qt::Checked : Qt::Unchecked );
		} else if ( t == 'u' ) {
			v->setText( QString::number( vals.at( i ).second.toUInt(), 16 ) );
		} else if ( t == 'f' ) {
			v->setText( QString::number( vals.at( i ).second.toDouble() ) );
		} else if ( t == 'y' ) {
			v->setText( QString::number( vals.at( i ).second.toUInt() ) );
		} else {
			v->setText( vals.at( i ).second.toString() );
		}
		if ( readOnly )
			v->setFlags( v->flags() & ~( Qt::ItemIsEditable | Qt::ItemIsUserCheckable ) );
		grid->setItem( i, 1, v );
	}

	QScrollArea * prev = new QScrollArea( split );
	prev->setWidgetResizable( true );
	prev->setMinimumWidth( 260 );
	{
		QLabel * l = new QLabel( QObject::tr( "Select a texture property for a preview" ), prev );
		l->setAlignment( Qt::AlignCenter );
		l->setWordWrap( true );
		prev->setWidget( l );
	}
	split->addWidget( grid );
	split->addWidget( prev );
	split->setStretchFactor( 0, 3 );
	split->setStretchFactor( 1, 2 );
	lay->addWidget( split, 1 );

	// preview + double-click texture browsing
	QObject::connect( grid, &QTableWidget::currentCellChanged, &dlg, [nif, grid, prev, bgem]( int r, int, int, int ) {
		if ( r < 0 )
			return;
		char t = tlMatFieldType( bgem, grid->item( r, 0 )->text() );
		QString pth = grid->item( r, 1 )->text();
		if ( QWidget * old = prev->takeWidget() )
			old->deleteLater();
		if ( t == 't' && !pth.isEmpty() ) {
			try {
				prev->setWidget( new DDSTextureInfo( nif->getGameResources(), pth, prev ) );
				return;
			} catch ( ... ) {}
		}
		QLabel * l = new QLabel( ( t == 't' && !pth.isEmpty() )
			? QObject::tr( "Texture not found:\n%1" ).arg( pth )
			: QObject::tr( "Select a texture property for a preview" ), prev );
		l->setAlignment( Qt::AlignCenter );
		l->setWordWrap( true );
		prev->setWidget( l );
	} );
	QObject::connect( grid, &QTableWidget::cellDoubleClicked, &dlg, [nif, grid, bgem, readOnly]( int r, int col ) {
		if ( readOnly || col != 1 || r < 0 )
			return;
		if ( tlMatFieldType( bgem, grid->item( r, 0 )->text() ) != 't' )
			return;
		std::set<std::string_view> files;
		nif->listResourceFiles( files, &tlTexFileFilter );
		std::string prv( grid->item( r, 1 )->text().replace( QChar( '\\' ), QChar( '/' ) ).toLower().toStdString() );
		FileBrowserWidget browser( 720, 540, "Select Texture", files, prv, &( nif->getGameResources() ) );
		if ( browser.exec() == QDialog::Accepted ) {
			const std::string_view * s = browser.getItemSelected();
			if ( s && !s->empty() )
				grid->item( r, 1 )->setText( tlNormalizeResourcePath( QString::fromUtf8( s->data(), qsizetype( s->length() ) ) ) );
		}
	} );

	QDialogButtonBox * bb = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Close, &dlg );
	bb->button( QDialogButtonBox::Save )->setText( QObject::tr( "Save As..." ) );
	bb->button( QDialogButtonBox::Save )->setEnabled( !readOnly );
	lay->addWidget( bb );
	QObject::connect( bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
	QObject::connect( bb, &QDialogButtonBox::accepted, &dlg, [&dlg, grid, bgem, nif, path]() {
		// collect the grid back into ordered values
		QVector<QPair<QString, QVariant>> outVals;
		for ( int i = 0; i < grid->rowCount(); i++ ) {
			QString nm = grid->item( i, 0 )->text();
			char t = tlMatFieldType( bgem, nm );
			QTableWidgetItem * v = grid->item( i, 1 );
			QVariant val;
			if ( t == 'b' )
				val = ( v->checkState() == Qt::Checked );
			else if ( t == 'u' )
				val = v->text().toUInt( nullptr, 16 );
			else if ( t == 'y' )
				val = v->text().toUInt();
			else if ( t == 'f' )
				val = v->text().toDouble();
			else
				val = tlNormalizeResourcePath( v->text() );
			outVals.append( qMakePair( nm, val ) );
		}
		QByteArray out = tlMatSerialize( bgem, outVals );
		QString base = path.isEmpty() ? QString( bgem ? "new.bgem" : "new.bgsm" )
			: QFileInfo( QString( path ).replace( QChar( '\\' ), QChar( '/' ) ) ).fileName();
		QString fn = QFileDialog::getSaveFileName( &dlg, QObject::tr( "Save material" ),
			QDir( nif->getFolder() ).filePath( base ),
			bgem ? QObject::tr( "Effect material (*.bgem)" ) : QObject::tr( "Material (*.bgsm)" ) );
		if ( fn.isEmpty() )
			return;
		QFile f( fn );
		if ( f.open( QIODevice::WriteOnly ) ) {
			f.write( out );
			f.close();
			Message::info( &dlg, QObject::tr( "Saved %1 (%2 bytes). Place it under a loose 'materials' folder the game loads." )
				.arg( fn ).arg( out.size() ) );
		} else {
			QMessageBox::warning( &dlg, QObject::tr( "Material Editor" ), QObject::tr( "Could not write '%1'." ).arg( fn ) );
		}
	} );

	dlg.resize( 860, 640 );
	dlg.exec();
}

//! Build the Material / Texture Manager panel: live editing, find & replace,
//! archive browser, drag & drop, texture preview, selection sync
QDockWidget * tlCreateMatTexManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	{
		QDockWidget * dock = new QDockWidget( QObject::tr( "Material / Texture Manager" ), mw );
		dock->setObjectName( QStringLiteral( "MatTexManagerDock" ) );

		QWidget * panel = new QWidget( dock );
		QVBoxLayout * lay = new QVBoxLayout( panel );

		QHBoxLayout * fr = new QHBoxLayout;
		QLineEdit * edFilter = new QLineEdit( panel );
		edFilter->setPlaceholderText( QObject::tr( "Filter rows" ) );
		edFilter->setClearButtonEnabled( true );
		QLineEdit * edFind = new QLineEdit( panel );
		edFind->setPlaceholderText( QObject::tr( "Find" ) );
		QLineEdit * edRepl = new QLineEdit( panel );
		edRepl->setPlaceholderText( QObject::tr( "Replace with" ) );
		QPushButton * btnReplace = new QPushButton( QObject::tr( "Replace All" ), panel );
		fr->addWidget( edFilter, 1 );
		fr->addWidget( edFind, 1 );
		fr->addWidget( edRepl, 1 );
		fr->addWidget( btnReplace );
		lay->addLayout( fr );

		QHBoxLayout * fr2 = new QHBoxLayout;
		QPushButton * btnBrowse = new QPushButton( QObject::tr( "Browse..." ), panel );
		btnBrowse->setToolTip( QObject::tr( "Pick a replacement from the game archives for the selected row" ) );
		QPushButton * btnRetarget = new QPushButton( QObject::tr( "Retarget Folder..." ), panel );
		btnRetarget->setToolTip( QObject::tr( "Replace a folder prefix on every matching path" ) );
		QCheckBox * cbGroup = new QCheckBox( QObject::tr( "Group by node" ), panel );
		QPushButton * btnRefresh = new QPushButton( QObject::tr( "Refresh" ), panel );
		fr2->addWidget( btnBrowse );
		fr2->addWidget( btnRetarget );
		fr2->addWidget( cbGroup );
		fr2->addStretch( 1 );
		fr2->addWidget( btnRefresh );
		lay->addLayout( fr2 );

		QSplitter * split = new QSplitter( Qt::Horizontal, panel );
		TlPathTable * tbl = new TlPathTable( split );
		tbl->setHorizontalHeaderLabels( { QObject::tr( "Owner node" ), QObject::tr( "Material" ), QObject::tr( "Path (editable, drag && drop)" ) } );
		tbl->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
		tbl->verticalHeader()->setVisible( false );
		tbl->setContextMenuPolicy( Qt::CustomContextMenu );

		QScrollArea * prevScroll = new QScrollArea( split );
		prevScroll->setWidgetResizable( true );
		prevScroll->setMinimumWidth( 280 );
		{
			QLabel * l = new QLabel( QObject::tr( "Select a texture row for a preview" ), prevScroll );
			l->setAlignment( Qt::AlignCenter );
			l->setWordWrap( true );
			prevScroll->setWidget( l );
		}
		split->addWidget( tbl );
		split->addWidget( prevScroll );
		split->setStretchFactor( 0, 3 );
		split->setStretchFactor( 1, 2 );
		lay->addWidget( split, 1 );

		QLabel * hint = new QLabel( QObject::tr( "Edits apply immediately (undoable). Paths are normalized on input. Click a row to jump to the node using it; right-click for more." ), panel );
		hint->setWordWrap( true );
		lay->addWidget( hint );

		// shared state for the lambdas
		auto rows = std::make_shared<QVector<QPersistentModelIndex>>();
		auto applying = std::make_shared<bool>( false );

		// row visibility: text filter + per-node collapsed groups (double-click
		// the owner column, or use the context menu, to fold a node's rows)
		auto collapsed = std::make_shared<QSet<int>>();
		auto applyVis = [tbl, edFilter, collapsed]() {
			QString f = edFilter->text();
			QSet<int> seen;
			bool wasSorting = tbl->isSortingEnabled();
			tbl->setSortingEnabled( false );
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				bool match = f.isEmpty();
				for ( int c = 0; c < 3 && !match; c++ ) {
					if ( QTableWidgetItem * it = tbl->item( r, c ) )
						match = it->text().contains( f, Qt::CaseInsensitive );
				}
				bool hide = !match;
				if ( QTableWidgetItem * c0 = tbl->item( r, 0 ) ) {
					int owner = c0->data( Qt::UserRole ).toInt();
					bool folded = collapsed->contains( owner );
					if ( !hide && folded ) {
						if ( seen.contains( owner ) )
							hide = true;	// only the first row of a folded node stays
						else
							seen.insert( owner );
					}
					// fold indicator icon (icons never affect the sort order,
					// unlike a text prefix which pushed folded rows around)
					static const QIcon icoFold = tlMakeIcon( QStringLiteral( "chevron_right" ), QColor( 228, 228, 232 ) );
					static const QIcon icoOpen = tlMakeIcon( QStringLiteral( "chevron_down" ), QColor( 228, 228, 232 ) );
					c0->setIcon( c0->data( Qt::UserRole + 2 ).toString().isEmpty()
						? QIcon() : ( folded ? icoFold : icoOpen ) );
				}
				tbl->setRowHidden( r, hide );
			}
			tbl->setSortingEnabled( wasSorting );
		};

		// object-mode selection colours, mirroring the block list (active
		// light blue + #FF9D00, secondary dark blue + #FF7200)
		auto recolor = [tbl, ogl, applying]() {
			*applying = true;
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				QTableWidgetItem * c1 = tbl->item( r, 0 );	// owner column
				QTableWidgetItem * c2 = tbl->item( r, 2 );
				if ( !c1 || !c2 )
					continue;
				int owner = c1->data( Qt::UserRole ).toInt();
				bool sel = ( owner >= 0 && ogl && ogl->objSelection.contains( owner ) );
				bool act = ( sel && owner == ogl->objActive );
				QBrush bg = sel ? QBrush( act ? QColor( 74, 122, 176 ) : QColor( 43, 66, 95 ) ) : QBrush();
				QBrush fg = sel ? QBrush( act ? QColor( 255, 157, 0 ) : QColor( 255, 114, 0 ) ) : QBrush();
				for ( int c = 0; c < 3; c++ ) {
					if ( QTableWidgetItem * it = tbl->item( r, c ) ) {
						it->setBackground( bg );
						it->setForeground( fg );
					}
				}
				if ( !sel && c2->data( Qt::UserRole ).toBool() )
					c2->setForeground( QColor( 235, 90, 90 ) );	// missing resource
			}
			*applying = false;
		};

		auto rebuild = [nif, tbl, rows, applying, recolor, applyVis]() {
			*applying = true;
			tbl->setSortingEnabled( false );
			tbl->setRowCount( 0 );
			QVector<QPersistentModelIndex> items;
			QStringList labels, owners;
			QVector<int> blockNums, ownerNums;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iBlock = nif->getBlockIndex( b );
				QString head = QString( "%1 %2" ).arg( b ).arg( nif->itemName( iBlock ) );
				int before = items.size();
				tlCollectResourceStrings( nif, iBlock, items, labels, owners, QString(), head );
				if ( items.size() > before ) {
					// a shader/texture path inside an AVObject block belongs to it;
					// otherwise walk the link graph up to the nearest one
					int ownerNum = nif->blockInherits( iBlock, "NiAVObject" ) ? b : tlOwnerBlock( nif, b );
					QString ownerNode;
					if ( ownerNum >= 0 ) {
						QModelIndex iOwner = nif->getBlockIndex( ownerNum );
						QString nm = nif->resolveString( iOwner, "Name" );
						ownerNode = QString( "%1 %2%3" ).arg( ownerNum ).arg( nif->itemName( iOwner ),
							nm.isEmpty() ? QString() : QStringLiteral( " \"" ) + nm + QStringLiteral( "\"" ) );
					}
					for ( int k = before; k < owners.size(); k++ ) {
						owners[k] = ownerNode;
						blockNums.append( b );
						ownerNums.append( ownerNum );
					}
				}
			}
			tbl->setRowCount( items.size() );
			for ( int i = 0; i < items.size(); i++ ) {
				QTableWidgetItem * c0 = new QTableWidgetItem( owners.at( i ) );
				c0->setFlags( c0->flags() & ~Qt::ItemIsEditable );
				c0->setData( Qt::UserRole, ownerNums.at( i ) );	// owning NiAVObject
				c0->setData( Qt::UserRole + 2, owners.at( i ) );	// base label for the fold indicator
				tbl->setItem( i, 0, c0 );
				QTableWidgetItem * c1 = new QTableWidgetItem( labels.at( i ) );
				c1->setFlags( c1->flags() & ~Qt::ItemIsEditable );
				c1->setData( Qt::UserRole, i );	// stable back-reference after sorting
				c1->setData( Qt::UserRole + 1, blockNums.at( i ) );	// containing block
				tbl->setItem( i, 1, c1 );
				QString path = nif->resolveString( QModelIndex( items.at( i ) ) );
				QTableWidgetItem * c2 = new QTableWidgetItem( path );
				// paths missing from the loaded archives show in red
				bool missing = !path.isEmpty() && tlFindResource( nif, path ).isEmpty();
				c2->setData( Qt::UserRole, missing );
				if ( missing )
					c2->setForeground( QColor( 235, 90, 90 ) );
				tbl->setItem( i, 2, c2 );
			}
			*rows = items;
			tbl->setSortingEnabled( true );
			*applying = false;
			recolor();
			applyVis();
		};

		// live apply with path normalization; this replaces the old
		// apply-on-OK flow whose sorted-row mapping could silently drop edits
		QObject::connect( tbl, &QTableWidget::itemChanged, panel, [nif, tbl, rows, applying]( QTableWidgetItem * it ) {
			if ( *applying || !it || it->column() != 2 )
				return;
			QString norm = tlNormalizeResourcePath( it->text() );
			*applying = true;
			if ( norm != it->text() )
				it->setText( norm );
			bool missing = !norm.isEmpty() && tlFindResource( nif, norm ).isEmpty();
			it->setData( Qt::UserRole, missing );
			it->setForeground( missing ? QBrush( QColor( 235, 90, 90 ) ) : QBrush() );
			*applying = false;
			QTableWidgetItem * c0 = tbl->item( it->row(), 1 );	// field column
			if ( !c0 )
				return;
			int i = c0->data( Qt::UserRole ).toInt();
			if ( i < 0 || i >= rows->size() )
				return;
			QModelIndex idx( rows->at( i ) );
			if ( !idx.isValid() || norm == nif->resolveString( idx ) )
				return;
			nifSnapshotOp( nif, QObject::tr( "Edit resource path" ), [&]() {
				nif->assignString( idx, norm );
			} );
		} );

		// selecting a row jumps to the node using the path and previews textures;
		// material files list the .dds paths they reference
		QObject::connect( tbl, &QTableWidget::currentCellChanged, panel,
			[nif, tbl, rows, applying, mw, prevScroll]( int r, int col, int, int ) {
			if ( *applying || r < 0 )
				return;
			QTableWidgetItem * c0 = tbl->item( r, 0 );	// owner
			QTableWidgetItem * c1 = tbl->item( r, 1 );	// material/field
			QTableWidgetItem * c2 = tbl->item( r, 2 );	// path
			if ( c0 && c1 && mw ) {
				// column-aware navigation: owner -> node, material -> containing
				// block, path -> the exact string field in Block Details
				int owner = c0->data( Qt::UserRole ).toInt();
				int bn = c1->data( Qt::UserRole + 1 ).toInt();
				int i = c1->data( Qt::UserRole ).toInt();
				QModelIndex target;
				if ( col == 0 )
					target = nif->getBlockIndex( owner >= 0 ? owner : bn );
				else if ( col == 1 )
					target = nif->getBlockIndex( bn );
				else if ( i >= 0 && i < rows->size() )
					target = QModelIndex( rows->at( i ) );
				if ( target.isValid() )
					QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection, Q_ARG( QModelIndex, target ) );
			}
			// preview on the right
			if ( QWidget * old = prevScroll->takeWidget() )
				old->deleteLater();
			QString pth = c2 ? c2->text() : QString();
			bool isMat = pth.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			             || pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			if ( pth.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ) {
				try {
					prevScroll->setWidget( new DDSTextureInfo( nif->getGameResources(), pth, prevScroll ) );
					return;
				} catch ( ... ) {
					// fall through to the placeholder
				}
			} else if ( isMat ) {
				// list the textures the material file references
				const char * ext = pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) ? ".bgem" : ".bgsm";
				std::string fullPath = Game::GameManager::get_full_path( pth, "materials/", ext );
				QByteArray data;
				if ( nif->getGameResources().get_file( data, fullPath ) && !data.isEmpty() ) {
					QStringList texs = tlScanMaterialTextures( data );
					QLabel * l = new QLabel( QObject::tr( "Textures referenced by\n%1:\n\n%2" )
						.arg( pth, texs.isEmpty() ? QObject::tr( "(none found)" ) : texs.join( QStringLiteral( "\n" ) ) ), prevScroll );
					l->setAlignment( Qt::AlignTop | Qt::AlignLeft );
					l->setWordWrap( true );
					l->setTextInteractionFlags( Qt::TextSelectableByMouse );
					l->setMargin( 8 );
					prevScroll->setWidget( l );
					return;
				}
			}
			QLabel * l = new QLabel( pth.isEmpty() ? QObject::tr( "Select a texture row for a preview" )
				: ( isMat ? QObject::tr( "Material not found:\n%1" ).arg( pth )
					: QObject::tr( "Texture not found:\n%1" ).arg( pth ) ), prevScroll );
			l->setAlignment( Qt::AlignCenter );
			l->setWordWrap( true );
			prevScroll->setWidget( l );
		} );

		// archive browser (the "Select Material" / texture picker) for the row
		auto browseRow = [nif, tbl]() {
			int r = tbl->currentRow();
			if ( r < 0 )
				return;
			QTableWidgetItem * c2 = tbl->item( r, 2 );
			if ( !c2 )
				return;
			QString cur = c2->text();
			bool isMat = cur.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			             || cur.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			std::set<std::string_view> files;
			nif->listResourceFiles( files, isMat ? &tlMatFileFilter : &tlTexFileFilter );
			std::string prv( QString( cur ).replace( QChar( '\\' ), QChar( '/' ) ).toLower().toStdString() );
			FileBrowserWidget browser( 720, 540, isMat ? "Select Material" : "Select Texture",
				files, prv, &( nif->getGameResources() ) );
			if ( browser.exec() == QDialog::Accepted ) {
				const std::string_view * s = browser.getItemSelected();
				if ( s && !s->empty() )
					c2->setText( QString::fromUtf8( s->data(), qsizetype( s->length() ) ) );
			}
		};
		QObject::connect( btnBrowse, &QPushButton::clicked, panel, browseRow );
		QObject::connect( tbl, &QTableWidget::cellDoubleClicked, panel,
			[tbl, browseRow, collapsed, applyVis]( int r, int col ) {
			if ( col == 1 ) {
				browseRow();	// double-click Field opens the archive browser
			} else if ( col == 0 ) {
				// double-click Owner folds/unfolds that node's rows
				QTableWidgetItem * c0 = tbl->item( r, 0 );
				if ( !c0 )
					return;
				int owner = c0->data( Qt::UserRole ).toInt();
				if ( collapsed->contains( owner ) )
					collapsed->remove( owner );
				else
					collapsed->insert( owner );
				applyVis();
			}
		} );

		QObject::connect( btnReplace, &QPushButton::clicked, panel, [tbl, edFind, edRepl, btnReplace]() {
			QString f = edFind->text();
			if ( f.isEmpty() )
				return;
			QString rep = edRepl->text();
			int n = 0;
			for ( int i = 0; i < tbl->rowCount(); i++ ) {
				QTableWidgetItem * it = tbl->item( i, 2 );
				if ( it && it->text().contains( f, Qt::CaseInsensitive ) ) {
					it->setText( QString( it->text() ).replace( f, rep, Qt::CaseInsensitive ) );
					n++;
				}
			}
			btnReplace->setText( QObject::tr( "Replace All (%1 changed)" ).arg( n ) );
		} );

		// live row filter (combined with the fold state)
		QObject::connect( edFilter, &QLineEdit::textChanged, panel, applyVis );

		// group rows by their owning node
		QObject::connect( cbGroup, &QCheckBox::toggled, panel, [tbl, applyVis]( bool on ) {
			tbl->sortItems( on ? 0 : 1 );
			applyVis();
		} );

		// bulk folder retarget: swap a path prefix everywhere it matches
		QObject::connect( btnRetarget, &QPushButton::clicked, panel, [tbl, panel]() {
			bool ok = false;
			QString from = QInputDialog::getText( panel, QObject::tr( "Retarget folder" ),
				QObject::tr( "Replace this folder prefix:" ), QLineEdit::Normal, QString(), &ok );
			if ( !ok || from.isEmpty() )
				return;
			QString to = QInputDialog::getText( panel, QObject::tr( "Retarget folder" ),
				QObject::tr( "...with this prefix:" ), QLineEdit::Normal, from, &ok );
			if ( !ok )
				return;
			from = tlNormalizeResourcePath( from );
			to = tlNormalizeResourcePath( to );
			int n = 0;
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				QTableWidgetItem * it = tbl->item( r, 2 );
				if ( it && it->text().startsWith( from, Qt::CaseInsensitive ) ) {
					it->setText( to + it->text().mid( from.length() ) );
					n++;
				}
			}
			Message::info( panel, QObject::tr( "Retargeted %1 path(s)." ).arg( n ) );
		} );

		// right-click menu: contextual reveal (depends on the clicked column),
		// browse, clear, copy/paste, fold, open externally, texture sets
		QObject::connect( tbl, &QTableWidget::customContextMenuRequested, panel,
			[nif, tbl, rows, mw, browseRow, rebuild, collapsed, applyVis]( const QPoint & pos ) {
			QTableWidgetItem * hit = tbl->itemAt( pos );
			if ( !hit )
				return;
			int r = hit->row();
			int hitCol = hit->column();
			tbl->setCurrentCell( r, 2 );
			QTableWidgetItem * c0 = tbl->item( r, 0 );	// owner
			QTableWidgetItem * c1 = tbl->item( r, 1 );	// field
			QTableWidgetItem * c2 = tbl->item( r, 2 );	// path
			if ( !c0 || !c1 || !c2 )
				return;
			int i = c1->data( Qt::UserRole ).toInt();
			int bn = c1->data( Qt::UserRole + 1 ).toInt();
			int owner = c0->data( Qt::UserRole ).toInt();
			bool isTexSet = ( nif->itemName( nif->getBlockIndex( bn ) ) == QLatin1String( "BSShaderTextureSet" ) );

			QMenu menu( tbl );
			// the first entry depends on which column was right-clicked
			QAction * aReveal;
			if ( hitCol == 0 )
				aReveal = menu.addAction( QObject::tr( "Jump to Owner Node" ) );
			else if ( hitCol == 1 )
				aReveal = menu.addAction( QObject::tr( "Reveal Containing Block" ) );
			else
				aReveal = menu.addAction( QObject::tr( "Reveal Path Field in Block Details" ) );
			menu.addSeparator();
			QAction * aBrowse = menu.addAction( QObject::tr( "Browse Archives..." ) );
			QAction * aOpenExt = menu.addAction( QObject::tr( "Open in Default Application" ) );
			aOpenExt->setEnabled( !c2->text().isEmpty() );
			bool rowIsMat = c2->text().endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			                || c2->text().endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			QAction * aEditMat = menu.addAction( QObject::tr( "Edit Material..." ) );
			aEditMat->setEnabled( rowIsMat );
			QAction * aNewMat = menu.addAction( QObject::tr( "New Material (.bgsm)..." ) );
			QAction * aNewEff = menu.addAction( QObject::tr( "New Effect Material (.bgem)..." ) );
			QAction * aAttach = menu.addAction( QObject::tr( "Attach Material to This Field..." ) );
			QAction * aClear = menu.addAction( QObject::tr( "Clear Path" ) );
			menu.addSeparator();
			QAction * aCopy = menu.addAction( QObject::tr( "Copy Path" ) );
			QAction * aPaste = menu.addAction( QObject::tr( "Paste Path" ) );
			aPaste->setEnabled( !QApplication::clipboard()->text().isEmpty() );
			menu.addSeparator();
			QAction * aFold = menu.addAction( collapsed->contains( owner )
				? QObject::tr( "Expand This Node's Rows" ) : QObject::tr( "Fold This Node's Rows" ) );
			aFold->setEnabled( owner >= 0 );
			menu.addSeparator();
			QAction * aCopySet = menu.addAction( QObject::tr( "Copy Texture Set" ) );
			aCopySet->setEnabled( isTexSet );
			QAction * aPasteSet = menu.addAction( QObject::tr( "Paste Texture Set" ) );
			aPasteSet->setEnabled( isTexSet && !tlCopiedTexSet.isEmpty() );

			QAction * sel = menu.exec( tbl->viewport()->mapToGlobal( pos ) );
			if ( !sel )
				return;
			if ( sel == aReveal && mw ) {
				QModelIndex target;
				if ( hitCol == 0 )
					target = nif->getBlockIndex( owner >= 0 ? owner : bn );
				else if ( hitCol == 1 )
					target = nif->getBlockIndex( bn );
				else if ( i >= 0 && i < rows->size() )
					target = QModelIndex( rows->at( i ) );
				if ( target.isValid() )
					QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection, Q_ARG( QModelIndex, target ) );
			} else if ( sel == aBrowse ) {
				browseRow();
			} else if ( sel == aOpenExt ) {
				// extract from the archives to temp and open with the default app
				QString pth = c2->text();
				QString low = pth.toLower();
				const char * fld = ( low.endsWith( QLatin1String( ".bgsm" ) ) || low.endsWith( QLatin1String( ".bgem" ) ) )
					? "materials/" : "textures/";
				const char * ext = low.endsWith( QLatin1String( ".bgem" ) ) ? ".bgem"
					: low.endsWith( QLatin1String( ".bgsm" ) ) ? ".bgsm"
					: low.endsWith( QLatin1String( ".tga" ) ) ? ".tga" : ".dds";
				std::string fullPath = Game::GameManager::get_full_path( pth, fld, ext );
				QByteArray data;
				if ( nif->getGameResources().get_file( data, fullPath ) && !data.isEmpty() ) {
					QString tmp = QDir::temp().filePath(
						QFileInfo( QString( pth ).replace( QChar( '\\' ), QChar( '/' ) ) ).fileName() );
					QFile f( tmp );
					if ( f.open( QIODevice::WriteOnly ) ) {
						f.write( data );
						f.close();
						QDesktopServices::openUrl( QUrl::fromLocalFile( tmp ) );
					}
				} else {
					Message::info( tbl, QObject::tr( "Could not read '%1' from the loaded resources." ).arg( pth ) );
				}
			} else if ( sel == aEditMat ) {
				tlOpenMaterialEditor( nif, tbl, c2->text() );
			} else if ( sel == aNewMat ) {
				tlOpenMaterialEditor( nif, tbl, QString(), false );
			} else if ( sel == aNewEff ) {
				tlOpenMaterialEditor( nif, tbl, QString(), true );
			} else if ( sel == aAttach ) {
				// pick a .bgsm/.bgem from the archives and assign it to this field
				std::set<std::string_view> files;
				nif->listResourceFiles( files, &tlMatFileFilter );
				std::string prv( QString( c2->text() ).replace( QChar( '\\' ), QChar( '/' ) ).toLower().toStdString() );
				FileBrowserWidget browser( 720, 540, "Select Material", files, prv, &( nif->getGameResources() ) );
				if ( browser.exec() == QDialog::Accepted ) {
					const std::string_view * s = browser.getItemSelected();
					if ( s && !s->empty() )
						c2->setText( QString::fromUtf8( s->data(), qsizetype( s->length() ) ) );
				}
			} else if ( sel == aClear ) {
				c2->setText( QString() );
			} else if ( sel == aCopy ) {
				QApplication::clipboard()->setText( c2->text() );
			} else if ( sel == aPaste ) {
				c2->setText( QApplication::clipboard()->text() );
			} else if ( sel == aFold ) {
				if ( collapsed->contains( owner ) )
					collapsed->remove( owner );
				else
					collapsed->insert( owner );
				applyVis();
			} else if ( sel == aCopySet ) {
				tlCopiedTexSet.clear();
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				for ( int t = 0; t < nif->rowCount( iTex ); t++ )
					tlCopiedTexSet.append( nif->resolveString( nif->index( t, 0, iTex ) ) );
			} else if ( sel == aPasteSet ) {
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				nifSnapshotOp( nif, QObject::tr( "Paste texture set" ), [&]() {
					int n = std::min( (int) tlCopiedTexSet.size(), nif->rowCount( iTex ) );
					for ( int t = 0; t < n; t++ )
						nif->assignString( nif->index( t, 0, iTex ), tlCopiedTexSet.at( t ) );
				} );
				rebuild();
			}
		} );

		// Refresh re-scans the NIF's paths AND flushes the GL texture cache so
		// textures edited on disk reload in the viewport and the preview
		QObject::connect( btnRefresh, &QPushButton::clicked, panel, [rebuild, ogl]() {
			if ( ogl ) {
				ogl->flush();
				ogl->update();
			}
			rebuild();
		} );
		// rebuild when a different file is loaded, or when the panel is opened
		QObject::connect( nif, &QAbstractItemModel::modelReset, dock, [dock, rebuild]() {
			if ( dock->isVisible() )
				rebuild();
		} );
		QObject::connect( dock, &QDockWidget::visibilityChanged, dock, [rebuild]( bool vis ) {
			if ( vis )
				rebuild();
		} );

		// selection sync: block-list / viewport selection colours matching rows
		if ( ogl ) {
			QObject::connect( ogl, &GLView::objectSelectionChanged, dock, [dock, recolor]() {
				if ( dock->isVisible() )
					recolor();
			} );
		}

		dock->setWidget( panel );
		if ( mw )
			mw->addDockWidget( Qt::RightDockWidgetArea, dock );
		// starts as its own small floating window; drag onto the main window to dock
		dock->setFloating( true );
		dock->resize( 1000, 560 );
		dock->hide();

		return dock;
	}
}


//! Propagate a node's name to the object palette and controller-sequence blocks
static int tlPropagateNodeName( NifModel * nif, int nodeNum, const QString & oldName, const QString & newName )
{
	int fixes = 0;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );

		if ( nif->blockInherits( iBlock, "NiDefaultAVObjectPalette" ) ) {
			QModelIndex iObjs = nif->getIndex( iBlock, "Objs" );
			for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
				QModelIndex iObj = nif->index( r, 0, iObjs );
				bool match = ( nif->getLink( iObj, "AV Object" ) == nodeNum );
				if ( !match && !oldName.isEmpty() )
					match = ( nif->resolveString( iObj, "Name" ) == oldName );
				if ( match ) {
					nif->assignString( iObj, "Name", newName );
					fixes++;
				}
			}
		} else if ( nif->blockInherits( iBlock, "NiControllerSequence" ) ) {
			QModelIndex iCB = nif->getIndex( iBlock, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
				QModelIndex iRow = nif->index( r, 0, iCB );
				if ( !oldName.isEmpty() && nif->resolveString( iRow, "Node Name" ) == oldName ) {
					nif->assignString( iRow, "Node Name", newName );
					fixes++;
				}
			}
		}
	}
	return fixes;
}


//! Rename a node and keep the palette + controller sequences in sync
class spRenameNodeSynced final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Rename (sync animation)..." ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif ? nif->getBlockIndex( index ) : QModelIndex();
		return iBlock.isValid() && nif->getIndex( iBlock, "Name" ).isValid()
		       && nif->blockInherits( iBlock, "NiObjectNET" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		int nodeNum = nif->getBlockNumber( iBlock );
		QString oldName = nif->resolveString( iBlock, "Name" );

		bool ok = false;
		QString newName = QInputDialog::getText( nullptr, name(),
			Spell::tr( "New name (the palette and all controller sequences will be updated to match):" ),
			QLineEdit::Normal, oldName, &ok );
		if ( !ok || newName == oldName )
			return index;

		int fixes = 0;
		nifSnapshotOp( nif, Spell::tr( "Rename node to %1" ).arg( newName ), [&]() {
			nif->assignString( iBlock, "Name", newName );
			fixes = tlPropagateNodeName( nif, nodeNum, oldName, newName );
		} );

		if ( fixes > 0 )
			Message::info( nullptr, Spell::tr( "Updated %1 palette/sequence reference(s)." ).arg( fixes ) );

		return iBlock;
	}
};

REGISTER_SPELL( spRenameNodeSynced )


//! Make every node's name authoritative: resync the palette and controller sequences
class spEnforceNameAuthority final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Enforce Node Name Authority" ); }
	QString page() const override final { return Spell::tr( "Sanitize" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & ) override final
	{
		if ( !nif )
			return false;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( nif->blockInherits( nif->getBlockIndex( b ), "NiDefaultAVObjectPalette" ) )
				return true;
		}
		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		int fixes = 0;
		nifSnapshotOp( nif, name(), [&]() {
			// palette Objs are the bridge: the linked node's name is authoritative
			QHash<QString, QString> rename;	// old palette name -> authoritative node name
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iPal = nif->getBlockIndex( b );
				if ( !nif->blockInherits( iPal, "NiDefaultAVObjectPalette" ) )
					continue;
				QModelIndex iObjs = nif->getIndex( iPal, "Objs" );
				for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
					QModelIndex iObj = nif->index( r, 0, iObjs );
					QModelIndex iNode = nif->getBlockIndex( nif->getLink( iObj, "AV Object" ) );
					if ( !iNode.isValid() )
						continue;
					QString auth = nif->resolveString( iNode, "Name" );
					QString cur = nif->resolveString( iObj, "Name" );
					if ( !auth.isEmpty() && auth != cur ) {
						rename.insert( cur, auth );
						nif->assignString( iObj, "Name", auth );
						fixes++;
					}
				}
			}
			// carry the rename through the controller sequences
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iSeq = nif->getBlockIndex( b );
				if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
					continue;
				QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
				for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
					QModelIndex iRow = nif->index( r, 0, iCB );
					QString cur = nif->resolveString( iRow, "Node Name" );
					if ( rename.contains( cur ) ) {
						nif->assignString( iRow, "Node Name", rename.value( cur ) );
						fixes++;
					}
				}
			}
		} );

		Message::info( nullptr, fixes > 0
			? Spell::tr( "Fixed %1 name reference(s) to match their nodes." ).arg( fixes )
			: Spell::tr( "All names already match their nodes." ) );
		return index;
	}
};

REGISTER_SPELL( spEnforceNameAuthority )
