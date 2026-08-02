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
#include "io/material.h"

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
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QGridLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
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

//! Only the Path column (2) is inline-editable; every other column is display
//! only, mirroring how the Collision Manager tree presents read-only fields.
class TlPathEditDelegate final : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	QWidget * createEditor( QWidget * parent, const QStyleOptionViewItem & option,
		const QModelIndex & index ) const override
	{
		if ( index.column() != 2 )
			return nullptr;
		return QStyledItemDelegate::createEditor( parent, option, index );
	}
};

//! Material/texture tree with drag & drop: dragging a path drops its text onto
//! another item's Path column (also accepts plain-text drops from outside).
//! A real QTreeWidget so materials own their textures as native children, with
//! the same expand arrows, sort indicator and scrollbars as the Collision list.
class TlMaterialTree final : public QTreeWidget
{
public:
	TlMaterialTree( QWidget * parent = nullptr ) : QTreeWidget( parent )
	{
		setColumnCount( 4 );
		setDragEnabled( true );
		setAcceptDrops( true );
		viewport()->setAcceptDrops( true );
		setDropIndicatorShown( true );
		setDragDropMode( QAbstractItemView::DragDrop );
	}

protected:
	void startDrag( Qt::DropActions ) override
	{
		QTreeWidgetItem * it = currentItem();
		if ( !it || it->text( 2 ).isEmpty() )
			return;
		QMimeData * md = new QMimeData;
		md->setText( it->text( 2 ) );
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
		QTreeWidgetItem * it = itemAt( e->position().toPoint() );
		if ( it && e->mimeData()->hasText() ) {
			it->setText( 2, e->mimeData()->text() );	// itemChanged applies + normalizes
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

static QModelIndex tlShaderBlock( const NifModel * nif, const QModelIndex & index )
{
	QModelIndex block = nif ? nif->getBlockIndex( index ) : QModelIndex();
	if ( block.isValid() && nif->blockInherits( block, { "BSLightingShaderProperty", "BSEffectShaderProperty" } ) )
		return block;
	return QModelIndex();
}

static QString tlShaderMaterialPath( const NifModel * nif, const QModelIndex & shader )
{
	return shader.isValid() ? tlNormalizeResourcePath( nif->resolveString( shader, "Name" ) ) : QString();
}

static void tlSetIfPresent( NifModel * nif, const QModelIndex & block, const char * field, float value )
{
	if ( nif->getIndex( block, field ).isValid() )
		nif->set<float>( block, field, value );
}

template <typename T>
static void tlSetIfPresent( NifModel * nif, const QModelIndex & block, const char * field, const T & value )
{
	if ( nif->getIndex( block, field ).isValid() )
		nif->set<T>( block, field, value );
}

static QModelIndex tlFillShaderTextureSet( NifModel * nif, const QModelIndex & shader, const QStringList & textures )
{
	if ( !shader.isValid() || !nif->blockInherits( shader, "BSLightingShaderProperty" ) )
		return QModelIndex();
	int texNum = nif->getLink( shader, "Texture Set" );
	QModelIndex texSet = nif->getBlockIndex( texNum );
	if ( !texSet.isValid() || nif->itemName( texSet ) != QLatin1String( "BSShaderTextureSet" ) ) {
		texSet = nif->insertNiBlock( "BSShaderTextureSet" );
		if ( !texSet.isValid() )
			return QModelIndex();
		nif->setLink( shader, "Texture Set", nif->getBlockNumber( texSet ) );
	}
	int count = std::max( 10, int( textures.size() ) );
	nif->set<quint32>( texSet, "Num Textures", quint32( count ) );
	QModelIndex array = nif->getIndex( texSet, "Textures" );
	nif->updateArraySize( array );
	for ( int i = 0; i < nif->rowCount( array ); i++ )
		nif->assignString( nif->index( i, 0, array ), i < textures.size() ? tlNormalizeResourcePath( textures.at( i ) ) : QString() );
	return texSet;
}

static bool tlSyncShaderFromMaterial( NifModel * nif, const QModelIndex & shader, bool full, QString * error = nullptr )
{
	QString path = tlShaderMaterialPath( nif, shader );
	bool lighting = nif->blockInherits( shader, "BSLightingShaderProperty" );
	bool effect = nif->blockInherits( shader, "BSEffectShaderProperty" );
	if ( path.isEmpty() || ( lighting && !path.endsWith( ".bgsm" ) ) || ( effect && !path.endsWith( ".bgem" ) ) ) {
		if ( error ) *error = QObject::tr( "The shader does not reference a matching BGSM/BGEM material." );
		return false;
	}
	if ( lighting ) {
		ShaderMaterial mat( path, nif, shader );
		if ( !mat.isValid() ) { if ( error ) *error = QObject::tr( "Could not read '%1'." ).arg( path ); return false; }
		tlSetIfPresent( nif, shader, "UV Offset", mat.uvOffset() );
		tlSetIfPresent( nif, shader, "UV Scale", mat.uvScale() );
		tlSetIfPresent( nif, shader, "Alpha", mat.alpha() );
		tlSetIfPresent( nif, shader, "Refraction Strength", mat.refractionPower() );
		tlSetIfPresent( nif, shader, "Environment Map Scale", mat.environmentMapScale() );
		tlSetIfPresent( nif, shader, "Emissive Color", mat.emittanceColor() );
		tlSetIfPresent( nif, shader, "Emissive Multiple", mat.emittanceMultiple() );
		tlSetIfPresent( nif, shader, "Specular Color", mat.specularColor() );
		tlSetIfPresent( nif, shader, "Specular Strength", mat.specularStrength() );
		tlSetIfPresent( nif, shader, "Smoothness", mat.smoothness() );
		tlSetIfPresent( nif, shader, "Fresnel Power", mat.fresnelPower() );
		tlSetIfPresent( nif, shader, "Grayscale to Palette Scale", mat.grayscaleToPaletteScale() );
		if ( nif->getIndex( shader, "Root Material" ).isValid() )
			nif->assignString( shader, "Root Material", mat.rootMaterialPath() );
		tlFillShaderTextureSet( nif, shader, mat.textures() );
		if ( full ) {
			tlSetIfPresent( nif, shader, "Shader Flags 1", mat.commonShaderFlags1() );
			tlSetIfPresent( nif, shader, "Shader Flags 2", mat.shaderFlags2() );
			tlSetIfPresent( nif, shader, "Alpha Source Blend Mode", mat.alphaSourceBlend() );
			tlSetIfPresent( nif, shader, "Alpha Destination Blend Mode", mat.alphaDestinationBlend() );
			tlSetIfPresent( nif, shader, "Alpha Test Threshold", mat.alphaTestThreshold() );
		}
	} else {
		EffectMaterial mat( path, nif, shader );
		if ( !mat.isValid() ) { if ( error ) *error = QObject::tr( "Could not read '%1'." ).arg( path ); return false; }
		tlSetIfPresent( nif, shader, "UV Offset", mat.uvOffset() );
		tlSetIfPresent( nif, shader, "UV Scale", mat.uvScale() );
		tlSetIfPresent( nif, shader, "Alpha", mat.alpha() );
		tlSetIfPresent( nif, shader, "Base Color", mat.baseColor() );
		tlSetIfPresent( nif, shader, "Base Color Scale", mat.baseColorScale() );
		tlSetIfPresent( nif, shader, "Falloff Start Angle", mat.falloffStartAngle() );
		tlSetIfPresent( nif, shader, "Falloff Stop Angle", mat.falloffStopAngle() );
		tlSetIfPresent( nif, shader, "Falloff Start Opacity", mat.falloffStartOpacity() );
		tlSetIfPresent( nif, shader, "Falloff Stop Opacity", mat.falloffStopOpacity() );
		tlSetIfPresent( nif, shader, "Lighting Influence", mat.lightingInfluence() );
		tlSetIfPresent( nif, shader, "Env Map Min LOD", mat.environmentMapMinLod() );
		tlSetIfPresent( nif, shader, "Soft Falloff Depth", mat.softFalloffDepth() );
		tlSetIfPresent( nif, shader, "Emissive Color", mat.emittanceColor() );
		const QStringList tex = mat.textures();
		static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
		for ( int i = 0; i < 5 && i < tex.size(); i++ )
			if ( nif->getIndex( shader, fields[i] ).isValid() ) nif->assignString( shader, fields[i], tlNormalizeResourcePath( tex.at( i ) ) );
		if ( full ) {
			tlSetIfPresent( nif, shader, "Shader Flags 1", mat.commonShaderFlags1() );
			tlSetIfPresent( nif, shader, "Shader Flags 2", mat.effectShaderFlags2() );
		}
	}
	return true;
}

static QStringList tlMaterialIssues( NifModel * nif, const QModelIndex & shader )
{
	QStringList issues;
	QString path = tlShaderMaterialPath( nif, shader );
	bool lighting = nif->blockInherits( shader, "BSLightingShaderProperty" );
	if ( path.isEmpty() ) return { QObject::tr( "No BGSM/BGEM material is assigned." ) };
	if ( ( lighting && !path.endsWith( ".bgsm" ) ) || ( !lighting && !path.endsWith( ".bgem" ) ) )
		issues << QObject::tr( "Material type does not match the shader property type." );
	if ( tlFindResource( nif, path ).isEmpty() )
		issues << QObject::tr( "Material file is missing: %1" ).arg( path );
	QStringList textures;
	quint32 expected1 = 0, expected2 = 0;
	bool valid = false;
	if ( lighting ) {
		ShaderMaterial mat( path, nif, shader ); valid = mat.isValid(); textures = mat.textures();
		expected1 = mat.commonShaderFlags1(); expected2 = mat.shaderFlags2();
		if ( valid ) {
			QModelIndex set = nif->getBlockIndex( nif->getLink( shader, "Texture Set" ) );
			if ( !set.isValid() ) issues << QObject::tr( "BSShaderTextureSet is missing." );
			else {
				QModelIndex a = nif->getIndex( set, "Textures" );
				for ( int i = 0; i < textures.size() && i < nif->rowCount( a ); i++ )
					if ( tlNormalizeResourcePath( nif->resolveString( nif->index( i, 0, a ) ) ) != tlNormalizeResourcePath( textures.at( i ) ) ) {
						issues << QObject::tr( "BSShaderTextureSet differs from the BGSM." ); break;
					}
			}
		}
	} else {
		EffectMaterial mat( path, nif, shader ); valid = mat.isValid(); textures = mat.textures();
		expected1 = mat.commonShaderFlags1(); expected2 = mat.effectShaderFlags2();
	}
	if ( !valid ) issues << QObject::tr( "Material file could not be decoded." );
	for ( const QString & tex : textures )
		if ( !tex.isEmpty() && tlFindResource( nif, tex ).isEmpty() ) issues << QObject::tr( "Texture is missing: %1" ).arg( tex );
	if ( valid && ( nif->get<quint32>( shader, "Shader Flags 1" ) != expected1
		|| nif->get<quint32>( shader, "Shader Flags 2" ) != expected2 ) )
		issues << QObject::tr( "Shader flags are out of sync with the material file." );
	issues.removeDuplicates();
	return issues;
}

static QString tlMaterialComparison( NifModel * nif, const QModelIndex & shader )
{
	QStringList issues = tlMaterialIssues( nif, shader );
	QString path = tlShaderMaterialPath( nif, shader );
	QString result = QObject::tr( "Material: %1\nShader block: %2\n\n" ).arg( path, nif->itemName( shader ) );
	result += issues.isEmpty() ? QObject::tr( "MATCH — the NIF shader agrees with the material file." )
		: QObject::tr( "DIFFERENCES\n• %1" ).arg( issues.join( QStringLiteral( "\n• " ) ) );
	return result;
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

struct TlCopiedMaterialSetup
{
	bool valid = false;
	bool effect = false;
	QString path;
	quint32 flags1 = 0;
	quint32 flags2 = 0;
	QStringList textures;
};
static TlCopiedMaterialSetup tlCopiedMaterialSetup;


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

//! Build the Material Manager plus an independent movable Texture Preview.
QDockWidget * tlCreateMatTexManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	{
		QDockWidget * dock = new QDockWidget( QObject::tr( "Material Manager" ), mw );
		dock->setObjectName( QStringLiteral( "MatTexManagerDock" ) );
		dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

		QWidget * panel = new QWidget( dock );
		QVBoxLayout * materialLay = new QVBoxLayout( panel );
		materialLay->setContentsMargins( 6, 6, 6, 6 );
		materialLay->setSpacing( 6 );

		// Floating shell that only hosts the preview while it is detached; by
		// default the preview is nested at the bottom of the panel instead.
		QDialog * previewWindow = new QDialog( mw, Qt::Tool | Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint );
		previewWindow->setWindowTitle( QObject::tr( "Texture Preview" ) );
		previewWindow->setObjectName( QStringLiteral( "TexturePreviewWindow" ) );
		QVBoxLayout * previewWindowLay = new QVBoxLayout( previewWindow );
		previewWindowLay->setContentsMargins( 6, 6, 6, 6 );

		QHBoxLayout * fr = new QHBoxLayout;
		QLineEdit * edFilter = new QLineEdit( panel );
		edFilter->setPlaceholderText( QObject::tr( "Filter rows" ) );
		edFilter->setClearButtonEnabled( true );
		QPushButton * btnReplaceDlg = new QPushButton( QObject::tr( "Replace..." ), panel );
		btnReplaceDlg->setToolTip( QObject::tr( "Find & replace across every path" ) );
		fr->addWidget( edFilter, 1 );
		fr->addWidget( btnReplaceDlg );
		materialLay->addLayout( fr );

		QHBoxLayout * fr2 = new QHBoxLayout;
		QPushButton * btnBrowse = new QPushButton( QObject::tr( "Browse..." ), panel );
		btnBrowse->setToolTip( QObject::tr( "Pick a replacement from the game archives for the selected row" ) );
		QPushButton * btnRetarget = new QPushButton( QObject::tr( "Retarget Folder..." ), panel );
		btnRetarget->setToolTip( QObject::tr( "Replace a folder prefix on every matching path" ) );
		QPushButton * btnRefresh = new QPushButton( QObject::tr( "Refresh" ), panel );
		QPushButton * btnCheck = new QPushButton( QObject::tr( "Check Materials" ), panel );
		QPushButton * btnDuplicates = new QPushButton( QObject::tr( "Find Duplicates" ), panel );
		btnRetarget->hide(); btnCheck->hide(); btnDuplicates->hide();
		QPushButton * btnPreview = new QPushButton( QObject::tr( "Texture Preview..." ), panel );
		fr2->addWidget( btnBrowse );
		fr2->addWidget( btnPreview );
		fr2->addStretch( 1 );
		fr2->addWidget( btnRefresh );
		QToolButton * btnMore = new QToolButton( panel );
		btnMore->setText( QObject::tr( "More..." ) );
		btnMore->setPopupMode( QToolButton::InstantPopup );
		QMenu * moreMenu = new QMenu( btnMore );
		QAction * retargetAction = moreMenu->addAction( QObject::tr( "Retarget Folder..." ) );
		QAction * checkAction = moreMenu->addAction( QObject::tr( "Check Materials" ) );
		QAction * duplicatesAction = moreMenu->addAction( QObject::tr( "Find Duplicates" ) );
		btnMore->setMenu( moreMenu );
		fr2->addWidget( btnMore );
		QObject::connect( retargetAction, &QAction::triggered, btnRetarget, &QPushButton::click );
		QObject::connect( checkAction, &QAction::triggered, btnCheck, &QPushButton::click );
		QObject::connect( duplicatesAction, &QAction::triggered, btnDuplicates, &QPushButton::click );
		materialLay->addLayout( fr2 );

		// The list and the nested preview share a draggable vertical splitter.
		QSplitter * vSplit = new QSplitter( Qt::Vertical, panel );
		vSplit->setChildrenCollapsible( false );

		QWidget * listPane = new QWidget( vSplit );
		QVBoxLayout * listLay = new QVBoxLayout( listPane );
		listLay->setContentsMargins( 0, 0, 0, 0 );
		listLay->setSpacing( 4 );

		QLabel * inventoryHeader = new QLabel( QObject::tr( "Materials in file" ), listPane );
		inventoryHeader->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; padding: 4px 2px; }" ) );
		listLay->addWidget( inventoryHeader );

		// Materials own their textures as native tree children: same expand
		// arrows, single-column sort indicator, alternating rows and horizontal
		// scrollbar as the Collision Manager node list.
		TlMaterialTree * tree = new TlMaterialTree( listPane );
		tree->setHeaderLabels( { QObject::tr( "Material" ), QObject::tr( "Mesh" ), QObject::tr( "Path" ), QObject::tr( "Status" ) } );
		tree->setRootIsDecorated( true );
		tree->setUniformRowHeights( true );
		tree->setExpandsOnDoubleClick( true );
		tree->setAlternatingRowColors( true );
		tree->setSelectionMode( QAbstractItemView::SingleSelection );
		tree->setContextMenuPolicy( Qt::CustomContextMenu );
		tree->setEditTriggers( QAbstractItemView::EditKeyPressed );
		tree->setItemDelegate( new TlPathEditDelegate( tree ) );
		tree->setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );
		tree->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
		tree->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 3, QHeaderView::Stretch );
		tree->setMinimumHeight( 150 );
		tree->setSortingEnabled( true );
		tree->sortByColumn( 0, Qt::AscendingOrder );
		// same selection colours the Collision Manager tree uses
		tree->setStyleSheet( QStringLiteral(
			"QTreeWidget::item:selected { background: rgb(74,122,176); color: rgb(255,157,0); }"
			"QTreeWidget::item:selected:!active { background: rgb(43,66,95); color: rgb(255,114,0); }" ) );
		listLay->addWidget( tree, 1 );

		vSplit->addWidget( listPane );
		materialLay->addWidget( vSplit, 1 );

		// The preview is nested at the bottom of the panel inside its own
		// container so it can be reparented into the floating shell on demand.
		QWidget * previewContainer = new QWidget( vSplit );
		QVBoxLayout * previewLay = new QVBoxLayout( previewContainer );
		previewLay->setContentsMargins( 0, 0, 0, 0 );
		previewLay->setSpacing( 4 );

		QHBoxLayout * previewHeaderRow = new QHBoxLayout;
		QLabel * previewHeader = new QLabel( QObject::tr( "Texture Preview" ), previewContainer );
		previewHeader->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; padding: 4px 2px; }" ) );
		QToolButton * btnDetach = new QToolButton( previewContainer );
		btnDetach->setText( QObject::tr( "Detach" ) );
		btnDetach->setToolTip( QObject::tr( "Pop the texture preview out into its own window" ) );
		btnDetach->setAutoRaise( true );
		previewHeaderRow->addWidget( previewHeader );
		previewHeaderRow->addStretch( 1 );
		previewHeaderRow->addWidget( btnDetach );
		previewLay->addLayout( previewHeaderRow );

		// A single full-width preview pane. The material tree's texture rows are
		// the selector now, so the old per-texture slot list / type buttons are
		// gone, giving the preview all of the width.
		QScrollArea * prevScroll = new QScrollArea( previewContainer );
		prevScroll->setWidgetResizable( true );
		{
			QLabel * l = new QLabel( QObject::tr( "Select a texture row for a preview" ), prevScroll );
			l->setAlignment( Qt::AlignCenter );
			l->setWordWrap( true );
			prevScroll->setWidget( l );
		}
		previewLay->addWidget( prevScroll, 1 );

		vSplit->addWidget( previewContainer );
		// preview takes the larger share below a compact material list, matching
		// the default panel layout; the splitter stays user-draggable
		vSplit->setStretchFactor( 0, 2 );
		vSplit->setStretchFactor( 1, 3 );
		vSplit->setSizes( { 240, 560 } );

		// Detach pops the preview into the floating shell; docking (button again
		// or closing the float) nests it back under the list.
		auto detached = std::make_shared<bool>( false );
		auto dockPreview = [vSplit, previewContainer, previewWindow, btnDetach, detached]() {
			vSplit->addWidget( previewContainer );	// reparents out of the float
			previewContainer->show();
			previewWindow->hide();
			btnDetach->setText( QObject::tr( "Detach" ) );
			*detached = false;
		};
		auto detachPreview = [previewWindow, previewWindowLay, previewContainer, btnDetach, detached]() {
			previewWindowLay->addWidget( previewContainer );	// reparents into the float
			previewContainer->show();
			previewWindow->show();
			previewWindow->raise();
			previewWindow->activateWindow();
			btnDetach->setText( QObject::tr( "Dock" ) );
			*detached = true;
		};
		QObject::connect( btnDetach, &QToolButton::clicked, previewContainer,
			[detached, detachPreview, dockPreview]() { if ( *detached ) dockPreview(); else detachPreview(); } );
		QObject::connect( previewWindow, &QDialog::finished, previewContainer,
			[detached, dockPreview]( int ) { if ( *detached ) dockPreview(); } );

		// preview a .dds path directly in the pane (helper used by row selection)
		auto showTexture = [nif, prevScroll]( const QString & path ) {
			if ( QWidget * old = prevScroll->takeWidget() )
				old->deleteLater();
			if ( path.isEmpty() ) {
				QLabel * l = new QLabel( QObject::tr( "Select a texture row for a preview" ), prevScroll );
				l->setAlignment( Qt::AlignCenter ); l->setWordWrap( true );
				prevScroll->setWidget( l );
				return;
			}
			try {
				prevScroll->setWidget( new DDSTextureInfo( nif->getGameResources(), path, prevScroll ) );
			} catch ( ... ) {
				QLabel * l = new QLabel( QObject::tr( "Texture not found:\n%1" ).arg( path ), prevScroll );
				l->setAlignment( Qt::AlignCenter ); l->setWordWrap( true );
				prevScroll->setWidget( l );
			}
		};
		// The toolbar button now pops the nested preview out into its window.
		QObject::connect( btnPreview, &QPushButton::clicked, previewWindow,
			[detached, detachPreview, previewWindow]() {
				if ( !*detached ) detachPreview();
				else { previewWindow->raise(); previewWindow->activateWindow(); }
			} );

		QLabel * hint = new QLabel( QObject::tr( "Live, undoable edits · Double-click a material to unfold · Right-click for tools" ), panel );
		hint->setWordWrap( true );
		hint->setMaximumHeight( 24 );
		materialLay->addWidget( hint );

		// shared state for the lambdas
		auto rows = std::make_shared<QVector<QPersistentModelIndex>>();
		auto applying = std::make_shared<bool>( false );

		// text filter across all columns; a material stays visible when it or any
		// of its textures match, and matches auto-expand so hits are revealed.
		// Folding itself is native (the expand arrow / double-click), like the
		// Collision Manager tree.
		auto applyVis = [tree, edFilter]() {
			QString f = edFilter->text();
			auto matches = [&f]( QTreeWidgetItem * it ) {
				if ( f.isEmpty() )
					return true;
				for ( int c = 0; c < 4; c++ )
					if ( it->text( c ).contains( f, Qt::CaseInsensitive ) )
						return true;
				return false;
			};
			for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
				QTreeWidgetItem * mat = tree->topLevelItem( i );
				bool matMatch = matches( mat );
				bool anyChild = false;
				for ( int c = 0; c < mat->childCount(); c++ ) {
					bool childMatch = matMatch || matches( mat->child( c ) );
					mat->child( c )->setHidden( !childMatch );
					anyChild = anyChild || childMatch;
				}
				mat->setHidden( !( matMatch || anyChild ) );
				if ( !f.isEmpty() && anyChild )
					mat->setExpanded( true );	// reveal the matching textures
			}
		};

		// object-mode selection colours, mirroring the block list (active
		// light blue + #FF9D00, secondary dark blue + #FF7200); a material and
		// all of its textures share the owning node's highlight
		auto recolor = [tree, ogl, applying]() {
			*applying = true;
			auto colorItem = [ogl]( QTreeWidgetItem * it, int owner ) {
				bool sel = ( owner >= 0 && ogl && ogl->objSelection.contains( owner ) );
				bool act = ( sel && owner == ogl->objActive );
				QBrush bg = sel ? QBrush( act ? QColor( 74, 122, 176 ) : QColor( 43, 66, 95 ) ) : QBrush();
				QBrush fg = sel ? QBrush( act ? QColor( 255, 157, 0 ) : QColor( 255, 114, 0 ) ) : QBrush();
				for ( int c = 0; c < 4; c++ ) {
					it->setBackground( c, bg );
					it->setForeground( c, fg );
				}
				if ( !sel && it->data( 2, Qt::UserRole ).toBool() )
					it->setForeground( 2, QColor( 235, 90, 90 ) );	// missing resource
			};
			// only the material row carries the owner highlight; its texture
			// children keep their own colours (Blender highlights the parent
			// node, not its children)
			for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
				QTreeWidgetItem * mat = tree->topLevelItem( i );
				colorItem( mat, mat->data( 0, Qt::UserRole + 3 ).toInt() );
			}
			*applying = false;
		};

		auto rebuild = [nif, tree, inventoryHeader, rows, applying, recolor, applyVis]() {
			*applying = true;
			bool wasSorting = tree->isSortingEnabled();
			tree->setSortingEnabled( false );
			tree->clear();
			QVector<QPersistentModelIndex> items;
			QStringList labels, owners, explicitPaths;
			QVector<int> blockNums, ownerNums, groupNums;
			QVector<bool> parentRows;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex shader = nif->getBlockIndex( b );
				bool lighting = nif->blockInherits( shader, "BSLightingShaderProperty" );
				bool effect = nif->blockInherits( shader, "BSEffectShaderProperty" );
				if ( !lighting && !effect ) continue;
				int ownerNum = tlOwnerBlock( nif, b );
				QString ownerNode = QObject::tr( "Shader %1" ).arg( b );
				if ( ownerNum >= 0 ) {
					QModelIndex owner = nif->getBlockIndex( ownerNum );
					QString name = nif->resolveString( owner, "Name" );
					ownerNode = name.isEmpty() ? QObject::tr( "Block %1" ).arg( ownerNum ) : name;
				}
				auto appendRow = [&]( const QModelIndex & item, const QString & label, int containing, bool parent ) {
					if ( !item.isValid() ) return;
					items.append( QPersistentModelIndex( item ) ); labels.append( label ); owners.append( ownerNode );
					blockNums.append( containing ); ownerNums.append( ownerNum ); groupNums.append( b ); parentRows.append( parent );
					explicitPaths.append( QString() );
				};
				// A material-file (BGSM/BGEM) texture is not backed by any NIF field:
				// it carries an explicit path and an invalid index (preview-only row).
				auto appendMatTexture = [&]( const QString & path, const QString & label, int containing ) {
					items.append( QPersistentModelIndex() ); labels.append( label ); owners.append( ownerNode );
					blockNums.append( containing ); ownerNums.append( ownerNum ); groupNums.append( b ); parentRows.append( false );
					explicitPaths.append( path );
				};
				appendRow( nif->getIndex( shader, "Name" ), effect ? QObject::tr( "BGEM" ) : QObject::tr( "BGSM" ), b, true );
				if ( lighting ) {
					// Authoritative Bethesda texture-set slot meanings (see the
					// BSShaderTextureSet.Textures comment in nif.xml). Slot 7 is the
					// back-lighting map on Skyrim, but Fallout 4 / 76 (BS version
					// >= 130) repurpose it as the specular/gloss map — matching how
					// the renderer binds slot 7 as SpecularMap for those versions.
					quint32 bs = nif->getBSVersion();
					auto slotName = [bs]( int t ) -> QString {
						switch ( t ) {
						case 0: return QObject::tr( "Diffuse" );
						case 1: return QObject::tr( "Normal" );
						case 2: return QObject::tr( "Glow/Skin" );
						case 3: return QObject::tr( "Height" );
						case 4: return QObject::tr( "Environment" );
						case 5: return QObject::tr( "Env Mask" );
						case 6: return QObject::tr( "Subsurface" );
						case 7: return bs >= 130 ? QObject::tr( "Spec/Gloss" ) : QObject::tr( "Backlight" );
						case 9: return QObject::tr( "Reflectivity" );	// Fallout 76
						case 10: return QObject::tr( "Lighting" );		// Fallout 76
						default: return QObject::tr( "Texture %1" ).arg( t + 1 );
						}
					};
					// A BGSM's own texture array uses a DIFFERENT order than a
					// BSShaderTextureSet (see the BGSM1_*/BGSM20_* enum in
					// glproperty.cpp): index 2 is the specular/gloss map, not glow.
					// Label material-file textures by that native order, so the tree
					// agrees with what the renderer samples.
					auto bgsmTexLabel = []( int i, int count ) -> QString {
						if ( count >= 10 ) {			// Fallout 76 10-texture layout
							switch ( i ) {
							case 0: return QObject::tr( "Diffuse" );
							case 1: return QObject::tr( "Normal" );
							case 2: return QObject::tr( "Spec/Gloss" );
							case 3: return QObject::tr( "Greyscale" );
							case 4: return QObject::tr( "Glow" );
							case 5: return QObject::tr( "Env Mask" );
							case 6: return QObject::tr( "Reflectance" );
							case 7: return QObject::tr( "Lighting" );
							}
						} else {						// Fallout 4 9-texture layout
							switch ( i ) {
							case 0: return QObject::tr( "Diffuse" );
							case 1: return QObject::tr( "Normal" );
							case 2: return QObject::tr( "Spec/Gloss" );
							case 3: return QObject::tr( "Greyscale" );
							case 4: return QObject::tr( "Environment" );
							case 5: return QObject::tr( "Glow / Env Mask" );
							}
						}
						return QObject::tr( "Texture %1" ).arg( i + 1 );
					};
					// Fallout 4 materials are the source of truth: a linked BGSM
					// overrides any BSShaderTextureSet, so list the material file's
					// own textures as the children when it is readable.
					QString matPath = tlNormalizeResourcePath( nif->resolveString( shader, "Name" ) );
					QStringList matTex;
					if ( matPath.endsWith( ".bgsm" ) ) {
						ShaderMaterial mat( matPath, nif, shader );
						if ( mat.isValid() ) matTex = mat.textures();
					}
					if ( !matTex.isEmpty() ) {
						for ( int t = 0; t < matTex.size(); t++ ) {
							QString tp = matTex.at( t ).trimmed();
							if ( !tp.isEmpty() ) appendMatTexture( tp, bgsmTexLabel( t, matTex.size() ), b );
						}
					} else {
						// no readable material: fall back to the NIF texture set.
						// rowCount() of an invalid index returns the ROOT child count,
						// so guard against a shader that has no Texture Set at all.
						int setNum = nif->getLink( shader, "Texture Set" );
						QModelIndex set = nif->getBlockIndex( setNum );
						QModelIndex array = nif->getIndex( set, "Textures" );
						if ( array.isValid() )
							for ( int t = 0; t < nif->rowCount( array ); t++ ) if ( !nif->resolveString( nif->index( t, 0, array ) ).isEmpty() )
								appendRow( nif->index( t, 0, array ), slotName( t ), setNum, false );
					}
				} else {
					static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
					static const char * names[] = { "Diffuse", "Greyscale", "Environment", "Normal", "Environment Mask" };
					// The linked BGEM overrides the shader's inline texture fields.
					QString matPath = tlNormalizeResourcePath( nif->resolveString( shader, "Name" ) );
					QStringList matTex;
					if ( matPath.endsWith( ".bgem" ) ) {
						EffectMaterial mat( matPath, nif, shader );
						if ( mat.isValid() ) matTex = mat.textures();
					}
					if ( !matTex.isEmpty() ) {
						for ( int t = 0; t < matTex.size(); t++ ) {
							QString tp = matTex.at( t ).trimmed();
							if ( !tp.isEmpty() )
								appendMatTexture( tp, t < 5 ? QObject::tr( names[t] ) : QObject::tr( "Texture %1" ).arg( t + 1 ), b );
						}
					} else {
						for ( int t = 0; t < 5; t++ ) {
							QModelIndex field = nif->getIndex( shader, fields[t] );
							if ( field.isValid() && !nif->resolveString( field ).isEmpty() ) appendRow( field, QObject::tr( names[t] ), b, false );
						}
					}
				}
			}
			QHash<QString, int> materialUses;
			for ( const QPersistentModelIndex & item : items ) {
				QString p = tlNormalizeResourcePath( nif->resolveString( QModelIndex( item ) ) );
				if ( p.endsWith( ".bgsm" ) || p.endsWith( ".bgem" ) ) materialUses[p]++;
			}
			QHash<int, QTreeWidgetItem *> matItems;	// group (shader block) -> material row
			for ( int i = 0; i < items.size(); i++ ) {
				int source = i;
				int group = groupNums.at( source );
				bool isMaterial = parentRows.at( source );
				// material-file texture rows carry an explicit path (no NIF field)
				QString path = !explicitPaths.at( source ).isEmpty()
					? explicitPaths.at( source )
					: nif->resolveString( QModelIndex( items.at( source ) ) );
				// paths missing from the loaded archives show in red
				bool missing = !path.isEmpty() && tlFindResource( nif, path ).isEmpty();
				QStringList badges;
				if ( missing ) badges << QObject::tr( "MISSING" );
				QString normalized = tlNormalizeResourcePath( path );
				QModelIndex containing = nif->getBlockIndex( blockNums.at( source ) );
				QModelIndex shader = tlShaderBlock( nif, containing );
				if ( shader.isValid() && ( normalized.endsWith( ".bgsm" ) || normalized.endsWith( ".bgem" ) )
					&& !tlMaterialIssues( nif, shader ).isEmpty() ) badges << QObject::tr( "OUT OF SYNC" );
				if ( ( normalized.endsWith( ".bgsm" ) || normalized.endsWith( ".bgem" ) ) && materialUses.value( normalized ) > 1 )
					badges << QObject::tr( "SHARED ×%1" ).arg( materialUses.value( normalized ) );
				if ( nif->itemName( containing ) == QLatin1String( "BSShaderTextureSet" ) ) {
					for ( int s = 0; s < nif->getBlockCount(); s++ ) {
						QModelIndex candidate = nif->getBlockIndex( s );
						if ( nif->blockInherits( candidate, "BSLightingShaderProperty" )
							&& nif->getLink( candidate, "Texture Set" ) == blockNums.at( source ) ) {
							for ( const QString & issue : tlMaterialIssues( nif, candidate ) )
								if ( issue.contains( "TextureSet" ) ) { badges << QObject::tr( "OVERRIDDEN" ); break; }
							break;
						}
					}
				}

				QTreeWidgetItem * it;
				if ( isMaterial ) {
					// material row: col0 = material file (or type), col1 = owning mesh
					QString matLabel = ( normalized.endsWith( ".bgsm" ) || normalized.endsWith( ".bgem" ) )
						? QFileInfo( QString( path ).replace( QChar( '\\' ), QChar( '/' ) ) ).fileName()
						: labels.at( source );
					it = new QTreeWidgetItem( tree );
					it->setText( 0, matLabel );
					it->setText( 1, owners.at( source ) );	// owning mesh / node name
					it->setData( 0, Qt::UserRole, group );	// stable hierarchy group
					it->setData( 0, Qt::UserRole + 3, ownerNums.at( source ) );	// owning NiAVObject
					QFont f = it->font( 0 ); f.setBold( true ); it->setFont( 0, f );
					matItems.insert( group, it );
				} else {
					// texture row: col0 = slot label (Diffuse/Normal/...), col1 blank
					QTreeWidgetItem * parent = matItems.value( group, nullptr );
					it = parent ? new QTreeWidgetItem( parent ) : new QTreeWidgetItem( tree );
					it->setText( 0, labels.at( source ) );
				}
				it->setData( 1, Qt::UserRole, source );	// stable back-reference into rows
				it->setData( 1, Qt::UserRole + 1, blockNums.at( source ) );	// containing block
				it->setData( 1, Qt::UserRole + 4, group );	// owning shader/material group
				it->setText( 2, path );
				it->setData( 2, Qt::UserRole, missing );
				if ( missing )
					it->setForeground( 2, QColor( 235, 90, 90 ) );
				it->setText( 3, badges.join( QStringLiteral( "  " ) ) );
				if ( badges.contains( QObject::tr( "MISSING" ) ) || badges.contains( QObject::tr( "OUT OF SYNC" ) ) )
					it->setForeground( 3, QColor( 235, 150, 55 ) );
				// only the Path column is inline-editable (enforced by the delegate)
				bool nifBacked = items.at( source ).isValid();
					it->setData( 1, Qt::UserRole + 5, !nifBacked );	// material-file (preview-only) row
					if ( nifBacked )
						it->setFlags( it->flags() | Qt::ItemIsEditable );
			}
			*rows = items;
			int materialCount = matItems.size();
			int textureCount = items.size() - materialCount;
			inventoryHeader->setText( materialCount
				? QObject::tr( "Materials in file  -  %1 material(s), %2 texture(s)" ).arg( materialCount ).arg( textureCount )
				: QObject::tr( "Materials in file" ) );
			tree->setSortingEnabled( wasSorting );	// re-sort; materials start folded
			*applying = false;
			recolor();
			applyVis();
		};

		// live apply with path normalization; this replaces the old
		// apply-on-OK flow whose sorted-row mapping could silently drop edits
		QObject::connect( tree, &QTreeWidget::itemChanged, panel, [nif, rows, applying]( QTreeWidgetItem * it, int column ) {
			if ( *applying || !it || column != 2 )
				return;
			QString norm = tlNormalizeResourcePath( it->text( 2 ) );
			*applying = true;
			if ( norm != it->text( 2 ) )
				it->setText( 2, norm );
			bool missing = !norm.isEmpty() && tlFindResource( nif, norm ).isEmpty();
			it->setData( 2, Qt::UserRole, missing );
			it->setForeground( 2, missing ? QBrush( QColor( 235, 90, 90 ) ) : QBrush() );
			*applying = false;
			int i = it->data( 1, Qt::UserRole ).toInt();
			if ( i < 0 || i >= rows->size() )
				return;
			QModelIndex idx( rows->at( i ) );
			if ( !idx.isValid() || norm == nif->resolveString( idx ) )
				return;
			nifSnapshotOp( nif, QObject::tr( "Edit resource path" ), [&]() {
				nif->assignString( idx, norm );
			} );
		} );

		// clicking a column reveals the matching block: Material -> containing
		// shader/texture-set block, Mesh -> owning node, Path -> the string field
		QObject::connect( tree, &QTreeWidget::itemClicked, panel, [nif, rows, mw]( QTreeWidgetItem * it, int col ) {
			if ( !it || !mw )
				return;
			QTreeWidgetItem * matItem = it->parent() ? it->parent() : it;
			int owner = matItem->data( 0, Qt::UserRole + 3 ).toInt();
			int bn = it->data( 1, Qt::UserRole + 1 ).toInt();
			int i = it->data( 1, Qt::UserRole ).toInt();
			QModelIndex target;
			if ( col == 0 )
				target = nif->getBlockIndex( bn );
			else if ( col == 1 )
				target = nif->getBlockIndex( owner >= 0 ? owner : bn );
			else if ( i >= 0 && i < rows->size() )
				target = QModelIndex( rows->at( i ) );
			if ( target.isValid() )
				QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection, Q_ARG( QModelIndex, target ) );
		} );

		// selecting a texture row previews it directly; selecting a material row
		// previews its first (diffuse) texture.  Updates in place whether the
		// preview is nested or detached.
		QObject::connect( tree, &QTreeWidget::currentItemChanged, panel,
			[showTexture, nif]( QTreeWidgetItem * cur, QTreeWidgetItem * ) {
			if ( !cur ) { showTexture( QString() ); return; }
			QString pth = cur->text( 2 );
			if ( !pth.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ) {
				if ( cur->childCount() > 0 ) {
					pth = cur->child( 0 )->text( 2 );	// material -> its first texture child
				} else if ( pth.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
						 || pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) ) {
					// A BGSM/BGEM-driven material can have no NIF BSShaderTextureSet
					// (its textures live inside the material file). Read the material
					// file's own textures so its diffuse still previews.
					QModelIndex shader = nif->getBlockIndex( cur->data( 1, Qt::UserRole + 1 ).toInt() );
					QStringList tex;
					if ( shader.isValid() ) {
						if ( pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) ) {
							EffectMaterial mat( pth, nif, shader );
							if ( mat.isValid() ) tex = mat.textures();
						} else {
							ShaderMaterial mat( pth, nif, shader );
							if ( mat.isValid() ) tex = mat.textures();
						}
					}
					pth.clear();
					for ( const QString & t : tex )
						if ( t.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ) { pth = t; break; }
				}
			}
			showTexture( pth.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ? pth : QString() );
		} );

		// archive browser (the "Select Material" / texture picker) for the row
		auto browseRow = [nif, tree]() {
			QTreeWidgetItem * it = tree->currentItem();
			if ( !it )
				return;
			if ( it->data( 1, Qt::UserRole + 5 ).toBool() )
					return;	// material-file texture row: defined by the BGSM/BGEM, not editable here
				QString cur = it->text( 2 );
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
					it->setText( 2, QString::fromUtf8( s->data(), qsizetype( s->length() ) ) );
			}
		};
		QObject::connect( btnBrowse, &QPushButton::clicked, panel, browseRow );
		// double-clicking a texture (a leaf) opens the archive browser; a material
		// with textures instead toggles its children via the native expand arrow
		QObject::connect( tree, &QTreeWidget::itemDoubleClicked, panel,
			[browseRow]( QTreeWidgetItem * it, int ) {
			if ( it && it->childCount() == 0 )
				browseRow();
		} );

		// Find & Replace lives in its own small dialog (Notepad++ style) opened
		// from the "Replace..." button, keeping the toolbar row uncluttered.
		QDialog * replaceDlg = new QDialog( panel );
		replaceDlg->setWindowTitle( QObject::tr( "Replace in Paths" ) );
		QGridLayout * rg = new QGridLayout( replaceDlg );
		QLineEdit * edFind = new QLineEdit( replaceDlg );
		QLineEdit * edRepl = new QLineEdit( replaceDlg );
		edFind->setMinimumWidth( 240 );
		QCheckBox * cbCase = new QCheckBox( QObject::tr( "Match case" ), replaceDlg );
		QPushButton * btnReplaceAll = new QPushButton( QObject::tr( "Replace All" ), replaceDlg );
		QPushButton * btnCloseDlg = new QPushButton( QObject::tr( "Close" ), replaceDlg );
		QLabel * replaceStatus = new QLabel( replaceDlg );
		rg->addWidget( new QLabel( QObject::tr( "Find what:" ), replaceDlg ), 0, 0 );
		rg->addWidget( edFind, 0, 1 );
		rg->addWidget( btnReplaceAll, 0, 2 );
		rg->addWidget( new QLabel( QObject::tr( "Replace with:" ), replaceDlg ), 1, 0 );
		rg->addWidget( edRepl, 1, 1 );
		rg->addWidget( btnCloseDlg, 1, 2 );
		rg->addWidget( cbCase, 2, 1 );
		rg->addWidget( replaceStatus, 3, 0, 1, 3 );
		QObject::connect( btnReplaceDlg, &QPushButton::clicked, replaceDlg, [replaceDlg, replaceStatus]() {
			replaceStatus->clear();
			replaceDlg->show(); replaceDlg->raise(); replaceDlg->activateWindow();
		} );
		QObject::connect( btnCloseDlg, &QPushButton::clicked, replaceDlg, &QDialog::hide );
		QObject::connect( btnReplaceAll, &QPushButton::clicked, replaceDlg, [tree, edFind, edRepl, cbCase, replaceStatus]() {
			QString f = edFind->text();
			if ( f.isEmpty() )
				return;
			QString rep = edRepl->text();
			Qt::CaseSensitivity cs = cbCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
			int n = 0;
			for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
				QTreeWidgetItem * m = tree->topLevelItem( i );
				for ( int j = -1; j < m->childCount(); j++ ) {
					QTreeWidgetItem * it = ( j < 0 ) ? m : m->child( j );
					if ( it->text( 2 ).contains( f, cs ) ) {
						it->setText( 2, QString( it->text( 2 ) ).replace( f, rep, cs ) );
						n++;
					}
				}
			}
			replaceStatus->setText( QObject::tr( "%1 path(s) changed" ).arg( n ) );
		} );

		// live row filter (combined with the fold state)
		QObject::connect( edFilter, &QLineEdit::textChanged, panel, applyVis );

		// bulk folder retarget: swap a path prefix everywhere it matches
		QObject::connect( btnRetarget, &QPushButton::clicked, panel, [tree, panel]() {
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
			for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
				QTreeWidgetItem * m = tree->topLevelItem( i );
				for ( int j = -1; j < m->childCount(); j++ ) {
					QTreeWidgetItem * it = ( j < 0 ) ? m : m->child( j );
					if ( it->text( 2 ).startsWith( from, Qt::CaseInsensitive ) ) {
						it->setText( 2, to + it->text( 2 ).mid( from.length() ) );
						n++;
					}
				}
			}
			Message::info( panel, QObject::tr( "Retargeted %1 path(s)." ).arg( n ) );
		} );

		QObject::connect( btnCheck, &QPushButton::clicked, panel, [nif, panel]() {
			QStringList report;
			int shaderCount = 0;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex shader = tlShaderBlock( nif, nif->getBlockIndex( b ) );
				if ( !shader.isValid() ) continue;
				shaderCount++;
				QStringList issues = tlMaterialIssues( nif, shader );
				if ( !issues.isEmpty() ) report << QObject::tr( "Block %1 — %2\n  • %3" )
					.arg( b ).arg( tlShaderMaterialPath( nif, shader ), issues.join( QStringLiteral( "\n  • " ) ) );
			}
			QMessageBox::information( panel, QObject::tr( "Material Check" ), report.isEmpty()
				? QObject::tr( "Checked %1 shader properties. No problems were found." ).arg( shaderCount )
				: QObject::tr( "%1 shader properties need attention:\n\n%2" ).arg( report.size() ).arg( report.join( QStringLiteral( "\n\n" ) ) ) );
		} );

		QObject::connect( btnDuplicates, &QPushButton::clicked, panel, [nif, panel, rebuild]() {
			QHash<QString, QVector<int>> groups;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex shader = tlShaderBlock( nif, nif->getBlockIndex( b ) );
				if ( !shader.isValid() ) continue;
				QStringList textureSignature;
				if ( nif->blockInherits( shader, "BSLightingShaderProperty" ) ) {
					QModelIndex set = nif->getBlockIndex( nif->getLink( shader, "Texture Set" ) );
					QModelIndex a = nif->getIndex( set, "Textures" );
					if ( a.isValid() )
						for ( int i = 0; i < nif->rowCount( a ); i++ ) textureSignature << tlNormalizeResourcePath( nif->resolveString( nif->index( i, 0, a ) ) );
				} else {
					static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
					for ( const char * field : fields ) textureSignature << tlNormalizeResourcePath( nif->resolveString( shader, field ) );
				}
				QString sig = QStringLiteral( "%1|%2|%3|%4|%5" ).arg( nif->itemName( shader ), tlShaderMaterialPath( nif, shader ),
					QString::number( nif->get<quint32>( shader, "Shader Flags 1" ), 16 ),
					QString::number( nif->get<quint32>( shader, "Shader Flags 2" ), 16 ), textureSignature.join( QChar( '|' ) ) );
				groups[sig].append( b );
			}
			QStringList lines; QVector<QVector<int>> duplicates;
			for ( auto it = groups.cbegin(); it != groups.cend(); ++it ) if ( it.value().size() > 1 ) {
				duplicates.append( it.value() );
				QStringList nums; for ( int b : it.value() ) nums << QString::number( b );
				lines << QObject::tr( "Blocks %1" ).arg( nums.join( QStringLiteral( ", " ) ) );
			}
			if ( duplicates.isEmpty() ) { QMessageBox::information( panel, QObject::tr( "Duplicate Materials" ), QObject::tr( "No identical shader setups were found." ) ); return; }
			QMessageBox box( QMessageBox::Information, QObject::tr( "Duplicate Materials" ),
				QObject::tr( "Identical shader setups:\n\n%1\n\nSharing them reduces redundant shader blocks. The unused copies are left orphaned for normal cleanup." ).arg( lines.join( QChar( '\n' ) ) ),
				QMessageBox::Cancel, panel );
			QPushButton * consolidate = box.addButton( QObject::tr( "Share Identical Setups" ), QMessageBox::AcceptRole );
			box.exec(); if ( box.clickedButton() != consolidate ) return;
			nifSnapshotOp( nif, QObject::tr( "Consolidate duplicate materials" ), [&]() {
				for ( const QVector<int> & group : duplicates ) {
					int keep = group.first();
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex owner = nif->getBlockIndex( b );
						QModelIndex field = nif->getIndex( owner, "Shader Property" );
						if ( field.isValid() && group.mid( 1 ).contains( nif->getLink( field ) )) nif->setLink( field, keep );
					}
				}
			} );
			rebuild();
		} );

		// right-click menu: contextual reveal (depends on the clicked column),
		// browse, clear, copy/paste, fold, open externally, texture sets
		QObject::connect( tree, &QTreeWidget::customContextMenuRequested, panel,
			[nif, tree, rows, mw, ogl, browseRow, rebuild]( const QPoint & pos ) {
			QTreeWidgetItem * hit = tree->itemAt( pos );
			if ( !hit )
				return;
			int hitCol = tree->columnAt( pos.x() );
			if ( hitCol < 0 )
				hitCol = 2;
			tree->setCurrentItem( hit );
			QTreeWidgetItem * matItem = hit->parent() ? hit->parent() : hit;	// owning material row
			int i = hit->data( 1, Qt::UserRole ).toInt();
			int bn = hit->data( 1, Qt::UserRole + 1 ).toInt();
			int owner = matItem->data( 0, Qt::UserRole + 3 ).toInt();
			bool isTexSet = ( nif->itemName( nif->getBlockIndex( bn ) ) == QLatin1String( "BSShaderTextureSet" ) );
			QModelIndex shader = tlShaderBlock( nif, nif->getBlockIndex( bn ) );
			if ( !shader.isValid() && isTexSet ) for ( int s = 0; s < nif->getBlockCount(); s++ ) {
				QModelIndex candidate = nif->getBlockIndex( s );
				if ( nif->blockInherits( candidate, "BSLightingShaderProperty" ) && nif->getLink( candidate, "Texture Set" ) == bn ) { shader = candidate; break; }
			}

			QMenu menu( tree );
			// the first entry depends on which column was right-clicked
			QAction * aReveal;
			if ( hitCol == 0 )
				aReveal = menu.addAction( QObject::tr( "Reveal Material Block" ) );
			else if ( hitCol == 1 )
				aReveal = menu.addAction( QObject::tr( "Jump to Owning Mesh" ) );
			else
				aReveal = menu.addAction( QObject::tr( "Reveal Path Field in Block Details" ) );
			menu.addSeparator();
			QAction * aBrowse = menu.addAction( QObject::tr( "Browse Archives..." ) );
			QAction * aOpenExt = menu.addAction( QObject::tr( "Open in Default Application" ) );
			aOpenExt->setEnabled( !hit->text( 2 ).isEmpty() );
			bool rowIsMat = hit->text( 2 ).endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			                || hit->text( 2 ).endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			QAction * aEditMat = menu.addAction( QObject::tr( "Edit Material..." ) );
			aEditMat->setEnabled( rowIsMat );
			QAction * aCheckMat = menu.addAction( QObject::tr( "Check Material" ) );
			QAction * aCompareMat = menu.addAction( QObject::tr( "Compare with BGSM/BGEM" ) );
			QAction * aSyncSafe = menu.addAction( QObject::tr( "Sync from BGSM/BGEM (Safe)" ) );
			QAction * aSyncFull = menu.addAction( QObject::tr( "Sync from BGSM/BGEM (Full)" ) );
			for ( QAction * a : { aCheckMat, aCompareMat, aSyncSafe, aSyncFull } ) a->setEnabled( shader.isValid() );
			QAction * aNewMat = menu.addAction( QObject::tr( "New Material (.bgsm)..." ) );
			QAction * aNewEff = menu.addAction( QObject::tr( "New Effect Material (.bgem)..." ) );
			QAction * aAttach = menu.addAction( QObject::tr( "Attach Material to This Field..." ) );
			QAction * aClear = menu.addAction( QObject::tr( "Clear Path" ) );
			menu.addSeparator();
			QAction * aCopy = menu.addAction( QObject::tr( "Copy Path" ) );
			QAction * aPaste = menu.addAction( QObject::tr( "Paste Path" ) );
			aPaste->setEnabled( !QApplication::clipboard()->text().isEmpty() );
			QAction * aCopySetup = menu.addAction( QObject::tr( "Copy Material Setup" ) );
			QAction * aPasteSetup = menu.addAction( QObject::tr( "Paste Material Setup" ) );
			aCopySetup->setEnabled( shader.isValid() );
			aPasteSetup->setEnabled( shader.isValid() && tlCopiedMaterialSetup.valid
				&& tlCopiedMaterialSetup.effect == nif->blockInherits( shader, "BSEffectShaderProperty" ) );
			QAction * aSelectUsers = menu.addAction( QObject::tr( "Select All Geometry Using This Material" ) );
			QAction * aAssignSelected = menu.addAction( QObject::tr( "Assign to Selected Shapes" ) );
			aSelectUsers->setEnabled( shader.isValid() && ogl );
			aAssignSelected->setEnabled( shader.isValid() && ogl && !ogl->objSelection.isEmpty() );
			menu.addSeparator();
			QAction * aFold = menu.addAction( matItem->isExpanded()
				? QObject::tr( "Fold This Material's Textures" ) : QObject::tr( "Expand This Material's Textures" ) );
			aFold->setEnabled( matItem->childCount() > 0 );
			menu.addSeparator();
			QAction * aCopySet = menu.addAction( QObject::tr( "Copy Texture Set" ) );
			aCopySet->setEnabled( isTexSet );
			QAction * aPasteSet = menu.addAction( QObject::tr( "Paste Texture Set" ) );
			aPasteSet->setEnabled( isTexSet && !tlCopiedTexSet.isEmpty() );

			QAction * sel = menu.exec( tree->viewport()->mapToGlobal( pos ) );
			if ( !sel )
				return;
			if ( sel == aReveal && mw ) {
				QModelIndex target;
				if ( hitCol == 0 )
					target = nif->getBlockIndex( bn );
				else if ( hitCol == 1 )
					target = nif->getBlockIndex( owner >= 0 ? owner : bn );
				else if ( i >= 0 && i < rows->size() )
					target = QModelIndex( rows->at( i ) );
				if ( target.isValid() )
					QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection, Q_ARG( QModelIndex, target ) );
			} else if ( sel == aBrowse ) {
				browseRow();
			} else if ( sel == aOpenExt ) {
				// extract from the archives to temp and open with the default app
				QString pth = hit->text( 2 );
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
					Message::info( tree, QObject::tr( "Could not read '%1' from the loaded resources." ).arg( pth ) );
				}
			} else if ( sel == aEditMat ) {
				tlOpenMaterialEditor( nif, tree, hit->text( 2 ) );
			} else if ( sel == aCheckMat ) {
				QStringList issues = tlMaterialIssues( nif, shader );
				QMessageBox::information( tree, QObject::tr( "Check Material" ), issues.isEmpty()
					? QObject::tr( "No material problems were found." ) : QObject::tr( "• %1" ).arg( issues.join( QStringLiteral( "\n• " ) ) ) );
			} else if ( sel == aCompareMat ) {
				QMessageBox::information( tree, QObject::tr( "Material Comparison" ), tlMaterialComparison( nif, shader ) );
			} else if ( sel == aSyncSafe || sel == aSyncFull ) {
				QString error; bool ok = false;
				nifSnapshotOp( nif, QObject::tr( "Sync material" ), [&]() { ok = tlSyncShaderFromMaterial( nif, shader, sel == aSyncFull, &error ); } );
				if ( !ok ) QMessageBox::warning( tree, QObject::tr( "Sync Material" ), error );
				rebuild();
			} else if ( sel == aNewMat ) {
				tlOpenMaterialEditor( nif, tree, QString(), false );
			} else if ( sel == aNewEff ) {
				tlOpenMaterialEditor( nif, tree, QString(), true );
			} else if ( sel == aAttach ) {
				// pick a .bgsm/.bgem from the archives and assign it to this field
				std::set<std::string_view> files;
				nif->listResourceFiles( files, &tlMatFileFilter );
				std::string prv( QString( hit->text( 2 ) ).replace( QChar( '\\' ), QChar( '/' ) ).toLower().toStdString() );
				FileBrowserWidget browser( 720, 540, "Select Material", files, prv, &( nif->getGameResources() ) );
				if ( browser.exec() == QDialog::Accepted ) {
					const std::string_view * s = browser.getItemSelected();
					if ( s && !s->empty() )
						hit->setText( 2,QString::fromUtf8( s->data(), qsizetype( s->length() ) ) );
				}
			} else if ( sel == aClear ) {
				hit->setText( 2,QString() );
			} else if ( sel == aCopy ) {
				QApplication::clipboard()->setText( hit->text( 2 ) );
			} else if ( sel == aPaste ) {
				hit->setText( 2,QApplication::clipboard()->text() );
			} else if ( sel == aCopySetup ) {
				tlCopiedMaterialSetup = TlCopiedMaterialSetup();
				tlCopiedMaterialSetup.valid = true;
				tlCopiedMaterialSetup.effect = nif->blockInherits( shader, "BSEffectShaderProperty" );
				tlCopiedMaterialSetup.path = tlShaderMaterialPath( nif, shader );
				tlCopiedMaterialSetup.flags1 = nif->get<quint32>( shader, "Shader Flags 1" );
				tlCopiedMaterialSetup.flags2 = nif->get<quint32>( shader, "Shader Flags 2" );
				if ( !tlCopiedMaterialSetup.effect ) {
					QModelIndex a = nif->getIndex( nif->getBlockIndex( nif->getLink( shader, "Texture Set" ) ), "Textures" );
					if ( a.isValid() )
						for ( int t = 0; t < nif->rowCount( a ); t++ ) tlCopiedMaterialSetup.textures << nif->resolveString( nif->index( t, 0, a ) );
				} else {
					static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
					for ( const char * field : fields ) tlCopiedMaterialSetup.textures << nif->resolveString( shader, field );
				}
			} else if ( sel == aPasteSetup ) {
				nifSnapshotOp( nif, QObject::tr( "Paste material setup" ), [&]() {
					nif->assignString( shader, "Name", tlCopiedMaterialSetup.path );
					tlSetIfPresent( nif, shader, "Shader Flags 1", tlCopiedMaterialSetup.flags1 );
					tlSetIfPresent( nif, shader, "Shader Flags 2", tlCopiedMaterialSetup.flags2 );
					if ( !tlCopiedMaterialSetup.effect ) tlFillShaderTextureSet( nif, shader, tlCopiedMaterialSetup.textures );
					else {
						static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
						for ( int t = 0; t < 5 && t < tlCopiedMaterialSetup.textures.size(); t++ )
							if ( nif->getIndex( shader, fields[t] ).isValid() ) nif->assignString( shader, fields[t], tlCopiedMaterialSetup.textures.at( t ) );
					}
				} );
				rebuild();
			} else if ( sel == aSelectUsers ) {
				QSet<int> users; QString path = tlShaderMaterialPath( nif, shader );
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					QModelIndex shape = nif->getBlockIndex( b ); int linked = nif->getLink( shape, "Shader Property" );
					QModelIndex linkedShader = nif->getBlockIndex( linked );
					if ( linkedShader.isValid() && tlShaderMaterialPath( nif, linkedShader ) == path ) users.insert( b );
				}
				ogl->setObjectSelection( users, users.isEmpty() ? -1 : *users.constBegin() );
			} else if ( sel == aAssignSelected ) {
				int shaderNum = nif->getBlockNumber( shader );
				nifSnapshotOp( nif, QObject::tr( "Assign material to selected shapes" ), [&]() {
					for ( int b : ogl->objSelection ) {
						QModelIndex shape = nif->getBlockIndex( b );
						if ( nif->getIndex( shape, "Shader Property" ).isValid() ) nif->setLink( shape, "Shader Property", shaderNum );
					}
				} );
				rebuild();
			} else if ( sel == aFold ) {
				matItem->setExpanded( !matItem->isExpanded() );
			} else if ( sel == aCopySet ) {
				tlCopiedTexSet.clear();
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				if ( iTex.isValid() )
					for ( int t = 0; t < nif->rowCount( iTex ); t++ )
						tlCopiedTexSet.append( nif->resolveString( nif->index( t, 0, iTex ) ) );
			} else if ( sel == aPasteSet ) {
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				nifSnapshotOp( nif, QObject::tr( "Paste texture set" ), [&]() {
					int n = iTex.isValid() ? std::min( (int) tlCopiedTexSet.size(), nif->rowCount( iTex ) ) : 0;
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
		QObject::connect( dock, &QDockWidget::visibilityChanged, previewWindow, [previewWindow]( bool vis ) {
			if ( !vis ) previewWindow->hide();
		} );

		// selection sync: block-list / viewport selection colours matching rows
		if ( ogl ) {
			QObject::connect( ogl, &GLView::objectSelectionChanged, dock, [dock, recolor]() {
				if ( dock->isVisible() )
					recolor();
			} );
		}

		dock->setWidget( panel );
		if ( mw ) {
			mw->addDockWidget( Qt::RightDockWidgetArea, dock );
		}
		// The very first time the panel is opened, size it to a comfortable
		// default width (matching the Material Manager screenshot). After that
		// the user's saved layout / drags win, so we never fight them.
		QObject::connect( dock, &QDockWidget::visibilityChanged, dock, [dock, mw]( bool vis ) {
			if ( !vis )
				return;
			QSettings settings;
			if ( settings.value( QStringLiteral( "MatTexManager/InitialWidthSet" ), false ).toBool() )
				return;
			settings.setValue( QStringLiteral( "MatTexManager/InitialWidthSet" ), true );
			if ( mw )
				mw->resizeDocks( { dock }, { 690 }, Qt::Horizontal );
		} );
		previewWindow->resize( 760, 680 );
		previewWindow->hide();
		// docks on the right beside the Collision Manager rather than floating wide
		dock->setFloating( false );
		dock->hide();

		return dock;
	}
}

class spFillShaderTextureSet final : public Spell
{
public:
	QString name() const override final { return tr( "Fill BSShaderTextureSet from BGSM" ); }
	QString page() const override final { return tr( "Material" ); }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		return shader.isValid() && nif->blockInherits( shader, "BSLightingShaderProperty" )
			&& tlShaderMaterialPath( nif, shader ).endsWith( ".bgsm" );
	}
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		QString path = tlShaderMaterialPath( nif, shader );
		ShaderMaterial mat( path, nif, shader );
		if ( !mat.isValid() ) {
			QMessageBox::warning( QApplication::activeWindow(), tr( "Fill BSShaderTextureSet" ), tr( "Could not read '%1'." ).arg( path ) );
			return shader;
		}
		nifSnapshotOp( nif, tr( "Fill BSShaderTextureSet" ), [&]() { tlFillShaderTextureSet( nif, shader, mat.textures() ); } );
		return shader;
	}
};
REGISTER_SPELL( spFillShaderTextureSet )

class spSyncShaderMaterial final : public Spell
{
public:
	QString name() const override final { return tr( "Sync Shader Property from BGSM/BGEM..." ); }
	QString page() const override final { return tr( "Material" ); }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return tlShaderBlock( nif, index ).isValid();
	}
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		QMessageBox box( QMessageBox::Question, tr( "Sync Shader Property" ),
			tr( "Safe Sync updates textures and numeric shader settings.\n\nFull Sync also replaces both shader-flag fields and blend/test settings." ),
			QMessageBox::Cancel, QApplication::activeWindow() );
		QPushButton * safe = box.addButton( tr( "Safe Sync" ), QMessageBox::AcceptRole );
		QPushButton * full = box.addButton( tr( "Full Sync" ), QMessageBox::DestructiveRole );
		box.setDefaultButton( safe );
		box.exec();
		if ( box.clickedButton() != safe && box.clickedButton() != full ) return shader;
		bool doFull = box.clickedButton() == full;
		QString error;
		bool ok = false;
		nifSnapshotOp( nif, doFull ? tr( "Full material sync" ) : tr( "Safe material sync" ), [&]() {
			ok = tlSyncShaderFromMaterial( nif, shader, doFull, &error );
		} );
		if ( !ok ) QMessageBox::warning( QApplication::activeWindow(), tr( "Sync Shader Property" ), error );
		return shader;
	}
};
REGISTER_SPELL( spSyncShaderMaterial )

class spCheckMaterial final : public Spell
{
public:
	QString name() const override final { return tr( "Check Material" ); }
	QString page() const override final { return tr( "Material" ); }
	bool constant() const override final { return true; }
	bool checker() const override final { return true; }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final { return tlShaderBlock( nif, index ).isValid(); }
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		QStringList issues = tlMaterialIssues( nif, shader );
		QMessageBox::information( QApplication::activeWindow(), tr( "Check Material" ), issues.isEmpty()
			? tr( "No material problems were found." ) : tr( "• %1" ).arg( issues.join( QStringLiteral( "\n• " ) ) ) );
		return shader;
	}
};
REGISTER_SPELL( spCheckMaterial )

/*! Every shader block's material problems, whole file, reported through logMessage.
 *
 *  WHY THIS EXISTS AS A SEPARATE SPELL. `spCheckMaterial` above already knows how
 *  to find these — but it wants a selected shader block, and it ends in a
 *  `QMessageBox`. Neither works for the Unfuck panel, which casts its checks with
 *  an INVALID index and drains `nif->getMessages()`: the selection gate means the
 *  spell is never applicable during a scan, and a modal raised behind a panel that
 *  is still building is how that panel hung once already.
 *
 *  `tlMaterialIssues` itself opens nothing and writes nothing, so the walk is safe
 *  to run headless. It is the reporting that had to change, not the checking.
 *
 *  COST, STATED PLAINLY. This is the only check in the panel that touches the
 *  filesystem: each shader block resolves its material path against the loaded
 *  archives and then extracts and re-parses the .bgsm/.bgem, uncached. That is one
 *  archive read per shader block — eight on a typical effect mesh, more on a large
 *  static. Acceptable for a scan the user asked for by opening the panel; it is
 *  why `isApplicable` gates hard rather than letting the walk decide, because
 *  `SpellBook::checkers()` is also cast per-file by the bulk directory scan in
 *  xmlcheck.cpp and by the CLI.
 *
 *  WHAT IT CANNOT SEE. When a shader's embedded Material struct has `Is Modified`
 *  set, `Material::createMaterialData` synthesizes the material bytes from the
 *  NIF's own fields instead of reading the file. "out of sync" and "TextureSet
 *  differs" then compare the NIF against itself and can never fire. Those two
 *  findings are therefore absent-not-clean on a modified material, and no amount
 *  of walking harder here changes that.
 */
class spCheckAllMaterials final : public Spell
{
public:
	QString name() const override final { return tr( "Material Problems" ); }
	QString page() const override final { return tr( "Error Checking" ); }
	bool constant() const override final { return true; }
	bool checker() const override final { return true; }
	static QString message() { return tr( "Material problems were found." ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		// Whole-file only, and only where BGSM/BGEM materials exist at all: below
		// BSVersion 130 no shader references one, so every block would report
		// "No BGSM/BGEM material is assigned" and bury a Skyrim file in noise.
		return !index.isValid() && nif && nif->getBSVersion() >= 130;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final
	{
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex shader = tlShaderBlock( nif, nif->getBlockIndex( b ) );
			if ( !shader.isValid() )
				continue;

			/* A SHADER WITH NO MATERIAL IS NOT MATERIAL-DRIVEN, and a material
			 * check has nothing to say about it. Saying it anyway is how a linter
			 * becomes noise — this rule took three measurements to get right, and
			 * every one of them contradicted what I expected.
			 *
			 * 1. ElectricalExplosionSmall.nif, a shipping vanilla FO4 effect mesh,
			 *    contains zero ".bgem" strings: all seven of its shaders drive
			 *    themselves from Source Texture. Reporting them would open a
			 *    healthy asset with seven warnings. So: skip effect shaders that
			 *    carry an inline texture.
			 *
			 * 2. Too narrow. BeamSmoky03.nif still raised one, on a shader with no
			 *    material AND no textures — attached to "EditorMarker121:0", a
			 *    Creation Kit helper that never renders. Nothing is configured
			 *    because there is nothing to configure. So: skip effect shaders
			 *    with no material at all.
			 *
			 * 3. Still wrong, and this one killed the assumption underneath.
			 *    ACDuctMedEnd03.nif raised a WARNING on a BSLightingShaderProperty
			 *    with no material — and a perfectly good BSShaderTextureSet in the
			 *    next block. FO4 supports both paths; a lighting shader that names
			 *    its textures directly is legal and common in vanilla. "Lighting
			 *    shaders are always material-driven in FO4" was simply false.
			 *
			 * What survives is narrow and defensible: a shader with no material is
			 * skipped, EXCEPT a lighting shader that also has no texture set —
			 * that one has no way to be textured at all, which is a real defect
			 * rather than a different authoring style.
			 *
			 * spCheckMaterial keeps reporting all of it, correctly: there the user
			 * pointed at one block and asked about it.
			 */
			if ( tlShaderMaterialPath( nif, shader ).isEmpty() ) {
				if ( nif->blockInherits( shader, "BSLightingShaderProperty" )
					&& !nif->isValidBlockNumber( nif->getLink( shader, "Texture Set" ) ) )
					nif->logMessage( message(), tr(
						"[%1] Lighting shader has neither a material nor a texture set." ).arg( b ),
						QMessageBox::Critical );
				continue;
			}

			for ( const QString & issue : tlMaterialIssues( nif, shader ) ) {
				/* ABSENCE IS A DEFECT. DISAGREEMENT IS BETHESDA'S NORMAL.
				 *
				 * Measured before deciding, because the obvious reading is wrong.
				 * Five vanilla FO4 statics sampled at random from SetDressing:
				 * five of five reported "Shader flags are out of sync", four also
				 * reported "BSShaderTextureSet differs", one reported
				 * "BSShaderTextureSet is missing" — and NONE reported a missing
				 * material or a missing texture.
				 *
				 * So the NIF-vs-BGSM comparison is true of essentially every
				 * shipping asset: the game applies the material at runtime and
				 * the stored set and flags are a stale fallback it does not mind.
				 * Reported as warnings, they would put two amber rows on every
				 * static anyone ever opens, which is how this panel would earn
				 * the reputation the dialog it replaced had.
				 *
				 * They are still worth knowing if you are deliberately keeping a
				 * NIF and its material in step, so they stay — as notes, in
				 * plain text, below anything that is actually broken. Drift is
				 * tested first because "BSShaderTextureSet is missing" would
				 * otherwise match the absence rule below it.
				 */
				static const char * const drift[] = {
					"BSShaderTextureSet is missing",
					"BSShaderTextureSet differs",
					"Shader flags are out of sync",
				};
				bool isDrift = false;
				for ( const char * d : drift )
					if ( issue.contains( QLatin1String( d ) ) )
						isDrift = true;

				QMessageBox::Icon lvl = QMessageBox::Warning;
				if ( isDrift )
					lvl = QMessageBox::Information;
				else if ( issue.contains( QLatin1String( "is missing" ) ) )
					lvl = QMessageBox::Critical;	// the file or texture is not there

				nif->logMessage( message(), tr( "[%1] %2" ).arg( b ).arg( issue ), lvl );
			}
		}
		return QModelIndex();
	}
};
REGISTER_SPELL( spCheckAllMaterials )

class spCompareMaterial final : public Spell
{
public:
	QString name() const override final { return tr( "Compare with BGSM/BGEM" ); }
	QString page() const override final { return tr( "Material" ); }
	bool constant() const override final { return true; }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final { return tlShaderBlock( nif, index ).isValid(); }
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		QMessageBox::information( QApplication::activeWindow(), tr( "Material Comparison" ), tlMaterialComparison( nif, shader ) );
		return shader;
	}
};
REGISTER_SPELL( spCompareMaterial )

class spCopyMaterialSetup final : public Spell
{
public:
	QString name() const override final { return tr( "Copy Material Setup" ); }
	QString page() const override final { return tr( "Material" ); }
	bool constant() const override final { return true; }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final { return tlShaderBlock( nif, index ).isValid(); }
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		tlCopiedMaterialSetup = TlCopiedMaterialSetup();
		tlCopiedMaterialSetup.valid = true;
		tlCopiedMaterialSetup.effect = nif->blockInherits( shader, "BSEffectShaderProperty" );
		tlCopiedMaterialSetup.path = tlShaderMaterialPath( nif, shader );
		tlCopiedMaterialSetup.flags1 = nif->get<quint32>( shader, "Shader Flags 1" );
		tlCopiedMaterialSetup.flags2 = nif->get<quint32>( shader, "Shader Flags 2" );
		if ( !tlCopiedMaterialSetup.effect ) {
			QModelIndex set = nif->getBlockIndex( nif->getLink( shader, "Texture Set" ) );
			QModelIndex a = nif->getIndex( set, "Textures" );
			if ( a.isValid() )
				for ( int i = 0; i < nif->rowCount( a ); i++ ) tlCopiedMaterialSetup.textures << nif->resolveString( nif->index( i, 0, a ) );
		} else {
			static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
			for ( const char * field : fields ) tlCopiedMaterialSetup.textures << nif->resolveString( shader, field );
		}
		return shader;
	}
};
REGISTER_SPELL( spCopyMaterialSetup )

class spPasteMaterialSetup final : public Spell
{
public:
	QString name() const override final { return tr( "Paste Material Setup" ); }
	QString page() const override final { return tr( "Material" ); }
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		return tlCopiedMaterialSetup.valid && shader.isValid()
			&& tlCopiedMaterialSetup.effect == nif->blockInherits( shader, "BSEffectShaderProperty" );
	}
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex shader = tlShaderBlock( nif, index );
		nifSnapshotOp( nif, tr( "Paste material setup" ), [&]() {
			nif->assignString( shader, "Name", tlCopiedMaterialSetup.path );
			tlSetIfPresent( nif, shader, "Shader Flags 1", tlCopiedMaterialSetup.flags1 );
			tlSetIfPresent( nif, shader, "Shader Flags 2", tlCopiedMaterialSetup.flags2 );
			if ( !tlCopiedMaterialSetup.effect ) tlFillShaderTextureSet( nif, shader, tlCopiedMaterialSetup.textures );
			else {
				static const char * fields[] = { "Source Texture", "Greyscale Texture", "Env Map Texture", "Normal Texture", "Env Mask Texture" };
				for ( int i = 0; i < 5 && i < tlCopiedMaterialSetup.textures.size(); i++ )
					if ( nif->getIndex( shader, fields[i] ).isValid() ) nif->assignString( shader, fields[i], tlCopiedMaterialSetup.textures.at( i ) );
			}
		} );
		return shader;
	}
};
REGISTER_SPELL( spPasteMaterialSetup )


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
		       && nif->blockInherits( iBlock, "NiAVObject" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		int nodeNum = nif->getBlockNumber( iBlock );
		QString oldName = nif->resolveString( iBlock, "Name" );

		bool ok = false;
		QString newName = QInputDialog::getText( nullptr, name(),
			Spell::tr( "New unique scene-object name (palettes and controller sequences will be updated):" ),
			QLineEdit::Normal, oldName, &ok ).trimmed();
		if ( !ok || newName == oldName )
			return index;
		if ( newName.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "A scene-object name cannot be empty." ) );
			return index;
		}
		for ( int block = 0; block < nif->getBlockCount(); block++ ) {
			if ( block == nodeNum ) continue;
			QModelIndex other = nif->getBlockIndex( block );
			if ( nif->blockInherits( other, "NiAVObject" )
				&& nif->resolveString( other, "Name" ).compare( newName, Qt::CaseInsensitive ) == 0 ) {
				QMessageBox::warning( nullptr, name(), Spell::tr(
					"Another scene object already uses the name '%1'. Choose a unique name." ).arg( newName ) );
				return index;
			}
		}

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
