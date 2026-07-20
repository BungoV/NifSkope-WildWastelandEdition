#include "spellbook.h"
#include "nifsnapshot.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QTreeWidget>

#include <algorithm>


// Brief description is deliberately not autolinked to class Spell
/*! \file flags.cpp
 * \brief Flag editing spells (spEditFlags)
 *
 * All classes here inherit from the Spell class.
 */

namespace
{
class ShaderFlagTree final : public QTreeWidget
{
public:
	explicit ShaderFlagTree( QWidget * parent = nullptr ) : QTreeWidget( parent ) {}

protected:
	void mousePressEvent( QMouseEvent * event ) override
	{
		if ( event->button() == Qt::LeftButton ) {
			if ( QTreeWidgetItem * item = itemAt( event->position().toPoint() ) ) {
				setCurrentItem( item );
				item->setCheckState( 0, item->checkState( 0 ) == Qt::Checked ? Qt::Unchecked : Qt::Checked );
				event->accept();
				return;
			}
		}
		QTreeWidget::mousePressEvent( event );
	}

	void keyPressEvent( QKeyEvent * event ) override
	{
		if ( event->key() == Qt::Key_Space && currentItem() ) {
			currentItem()->setCheckState( 0,
				currentItem()->checkState( 0 ) == Qt::Checked ? Qt::Unchecked : Qt::Checked );
			event->accept();
			return;
		}
		QTreeWidget::keyPressEvent( event );
	}
};

QString shaderFlagDisplayName( QString name )
{
	name.replace( QLatin1Char( '_' ), QLatin1Char( ' ' ) );
	name.replace( QRegularExpression( QStringLiteral( "([a-z])([A-Z])" ) ), QStringLiteral( "\\1 \\2" ) );
	name.replace( QStringLiteral( "ZBuffer" ), QStringLiteral( "Z-Buffer" ), Qt::CaseInsensitive );
	name.replace( QStringLiteral( "Localmap" ), QStringLiteral( "Local Map" ), Qt::CaseInsensitive );
	name.replace( QStringLiteral( "Premult" ), QStringLiteral( "Premultiplied" ), Qt::CaseInsensitive );
	return name;
}

QString shaderFlagKey( QString name )
{
	name = name.toLower();
	name.remove( QRegularExpression( QStringLiteral( "[^a-z0-9]" ) ) );
	return name;
}

QString shaderFlagCategory( const QString & rawName )
{
	const QString key = shaderFlagKey( rawName );
	if ( key.contains( "vertex" ) || key.contains( "skinned" ) || key.contains( "normal" )
		|| key.contains( "tessell" ) || key.contains( "projecteduv" ) || key.contains( "transformchanged" ) )
		return QObject::tr( "Geometry" );
	if ( key.contains( "specular" ) || key.contains( "lighting" ) || key.contains( "shadow" )
		|| key.contains( "environment" ) || key.contains( "falloff" ) || key.contains( "emittance" )
		|| key.contains( "emit" ) )
		return QObject::tr( "Lighting" );
	if ( key.contains( "palette" ) || key.contains( "tint" ) || key.contains( "texture" )
		|| key.contains( "glow" ) || key.contains( "parallax" ) || key.contains( "gradient" ) )
		return QObject::tr( "Material" );
	if ( key.contains( "zbuffer" ) || key.contains( "alpha" ) || key.contains( "fade" )
		|| key.contains( "sided" ) || key.contains( "wireframe" ) || key.contains( "decal" )
		|| key.contains( "refraction" ) )
		return QObject::tr( "Rendering" );
	return QObject::tr( "Specialized" );
}

// ---- generic flag-list dialog (the Shader Flags UI, reusable) -------------
// One filterable checkbox tree over the named bits of one or more flag
// fields, with the hex footer and clipboard Copy / Paste. Copy puts the raw
// field values on the system clipboard tagged with `kindId`; Paste only
// enables when the clipboard holds values copied from the SAME kind (and
// field count), so flag data moves between compatible fields only — e.g.
// shader flags between two FO4 shader properties, or node flags between two
// NiAVObjects, but never shader bits onto a node. The plain-text fallback
// ("F1 0x8040028B  F2 0x00000031") pastes nowhere and is just readable.

struct WwFlagField {
	QString label;		// "F1"; empty = single-field kind, shown as "Flags"
	quint32 value = 0;
};

struct WwFlagEntry {
	int field = 0;		// index into the fields vector
	int bit = 0;
	QString name;
	QString rawName;	// extra search text (may be empty)
	QString description;	// tooltip (may be empty)
	QString category;	// extra search text (may be empty)
};

constexpr const char * wwFlagsMimeType = "application/x-nifskope-flags";

QVector<quint32> wwFlagsFromClipboard( const QString & kindId, int fieldCount )
{
	const QMimeData * mime = QGuiApplication::clipboard()->mimeData();
	if ( !mime || !mime->hasFormat( QLatin1String( wwFlagsMimeType ) ) )
		return {};
	const QString payload = QString::fromUtf8( mime->data( QLatin1String( wwFlagsMimeType ) ) );
	const int split = payload.indexOf( QLatin1Char( '\n' ) );
	if ( split < 0 || payload.left( split ) != kindId )
		return {};
	QVector<quint32> values;
	for ( const QString & part : payload.mid( split + 1 ).split( QLatin1Char( ',' ) ) ) {
		bool ok = false;
		values.append( part.toUInt( &ok, 16 ) );
		if ( !ok )
			return {};
	}
	if ( values.size() != fieldCount )
		return {};
	return values;
}

bool wwFlagListDialog( const QString & title, const QString & kindId,
	QVector<WwFlagField> & fields, const QVector<WwFlagEntry> & entries )
{
	if ( fields.isEmpty() || entries.isEmpty() )
		return false;
	const bool multiField = fields.size() > 1;
	auto fieldTag = [&]( int f ) {
		return multiField ? fields.at( f ).label
			: ( fields.at( f ).label.isEmpty() ? QObject::tr( "Flags" ) : fields.at( f ).label );
	};

	QDialog dialog;
	dialog.setWindowTitle( title );
	dialog.resize( 460, 360 );
	dialog.setMinimumSize( 400, 320 );
	dialog.setStyleSheet( QStringLiteral(
		"QDialog { background: #383838; color: #dddddd; }"
		"QLabel { color: #dddddd; }"
		"QComboBox, QLineEdit, QTreeWidget { background: #454545; border: 1px solid #303030; color: #e4e4e4; }"
		"QLineEdit { border-radius: 3px; padding: 3px 7px; selection-background-color: #4772b3; }"
		"QTreeWidget::item { height: 26px; }"
		"QTreeWidget::item:hover { background: #505050; }"
		"QHeaderView::section { background: #333333; color: #bbbbbb; border: none;"
		" border-bottom: 1px solid #555555; padding: 4px 6px; }"
		"QPushButton { background: #454545; border: 1px solid #606060; border-radius: 3px;"
		" color: #e2e2e2; padding: 4px 10px; }"
		"QPushButton:hover { background: #525252; border-color: #777777; }"
		"QPushButton:disabled { color: #777777; border-color: #4a4a4a; }" ) );
	QVBoxLayout * layout = new QVBoxLayout( &dialog );
	layout->setContentsMargins( 10, 10, 10, 8 );
	layout->setSpacing( 7 );

	QHBoxLayout * toolbar = new QHBoxLayout;
	toolbar->addWidget( new QLabel( QObject::tr( "Filter" ), &dialog ) );
	QLineEdit * filterEdit = new QLineEdit( &dialog );
	filterEdit->setPlaceholderText( QObject::tr( "Type a flag name, category, or bit 25…" ) );
	filterEdit->setClearButtonEnabled( true );
	toolbar->addWidget( filterEdit, 1 );
	QLabel * countLabel = new QLabel( &dialog );
	countLabel->setStyleSheet( QStringLiteral( "color: #bbbbbb;" ) );
	toolbar->addWidget( countLabel );
	layout->addLayout( toolbar );

	ShaderFlagTree * tree = new ShaderFlagTree( &dialog );
	tree->setColumnCount( 2 );
	tree->setHeaderLabels( { QObject::tr( "Flag" ), QObject::tr( "Stored in" ) } );
	tree->setRootIsDecorated( false );
	tree->setAlternatingRowColors( true );
	tree->setSelectionMode( QAbstractItemView::SingleSelection );
	tree->header()->setStretchLastSection( false );
	tree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	tree->header()->setSectionResizeMode( 1, QHeaderView::Fixed );
	tree->header()->resizeSection( 1, 86 );
	layout->addWidget( tree, 1 );

	QLabel * rawValues = new QLabel( &dialog );
	rawValues->setStyleSheet( QStringLiteral( "color: #bdbdbd; font-family: monospace;" ) );
	QPushButton * copyButton = new QPushButton( QObject::tr( "Copy" ), &dialog );
	copyButton->setToolTip( QObject::tr( "Copy the raw flag value(s) to the clipboard" ) );
	QPushButton * pasteButton = new QPushButton( QObject::tr( "Paste" ), &dialog );
	pasteButton->setToolTip( QObject::tr( "Paste flag value(s) copied from a compatible flag field" ) );
	QDialogButtonBox * buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	QHBoxLayout * footer = new QHBoxLayout;
	footer->addWidget( rawValues, 1 );
	footer->addWidget( copyButton );
	footer->addWidget( pasteButton );
	footer->addWidget( buttons );
	layout->addLayout( footer );

	int shownCount = entries.size();
	auto refreshSummary = [&]() {
		int selected = 0;
		for ( const WwFlagField & f : fields ) {
			for ( int bit = 0; bit < 32; bit++ ) {
				if ( f.value & ( quint32( 1 ) << bit ) )
					selected++;
			}
		}
		countLabel->setText( QObject::tr( "%1 selected · %2 shown" ).arg( selected ).arg( shownCount ) );
		QStringList raw;
		for ( int f = 0; f < fields.size(); f++ ) {
			raw << QStringLiteral( "%1 0x%2" ).arg( fieldTag( f ) )
				.arg( QString::number( fields.at( f ).value, 16 ).rightJustified( 8, QLatin1Char( '0' ) ).toUpper() );
		}
		rawValues->setText( raw.join( QStringLiteral( "   " ) ) );
	};
	auto populate = [&]() {
		QSignalBlocker blocker( tree );
		tree->clear();
		shownCount = 0;
		const QString filter = filterEdit->text().trimmed();
		for ( const WwFlagEntry & entry : entries ) {
			const QString location = multiField
				? QObject::tr( "%1 bit %2" ).arg( fieldTag( entry.field ) ).arg( entry.bit )
				: QObject::tr( "bit %1" ).arg( entry.bit );
			const QString searchable = entry.name + QLatin1Char( ' ' ) + entry.rawName
				+ QLatin1Char( ' ' ) + entry.category + QLatin1Char( ' ' ) + location;
			if ( !filter.isEmpty() && !searchable.contains( filter, Qt::CaseInsensitive ) )
				continue;
			QTreeWidgetItem * item = new QTreeWidgetItem( tree );
			item->setText( 0, entry.name );
			item->setText( 1, multiField
				? QObject::tr( "%1 · bit %2" ).arg( fieldTag( entry.field ) ).arg( entry.bit )
				: QObject::tr( "bit %1" ).arg( entry.bit ) );
			item->setData( 0, Qt::UserRole, entry.field );
			item->setData( 0, Qt::UserRole + 1, entry.bit );
			item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
			const quint32 fieldValue = fields.at( entry.field ).value;
			item->setCheckState( 0, fieldValue & ( quint32( 1 ) << entry.bit ) ? Qt::Checked : Qt::Unchecked );
			if ( !entry.description.trimmed().isEmpty() )
				item->setToolTip( 0, entry.description.trimmed() );
			item->setForeground( 1, QColor( 170, 170, 170 ) );
			shownCount++;
		}
		refreshSummary();
	};
	auto refreshPaste = [&]() {
		pasteButton->setEnabled( !wwFlagsFromClipboard( kindId, fields.size() ).isEmpty() );
	};
	QObject::connect( filterEdit, &QLineEdit::textChanged, &dialog, [&]( const QString & ) { populate(); } );
	QObject::connect( tree, &QTreeWidget::itemChanged, &dialog, [&]( QTreeWidgetItem * item, int column ) {
		if ( column != 0 )
			return;
		const int field = item->data( 0, Qt::UserRole ).toInt();
		const int bit = item->data( 0, Qt::UserRole + 1 ).toInt();
		if ( field < 0 || field >= fields.size() )
			return;
		const quint32 mask = quint32( 1 ) << bit;
		if ( item->checkState( 0 ) == Qt::Checked )
			fields[field].value |= mask;
		else
			fields[field].value &= ~mask;
		refreshSummary();
	} );
	QObject::connect( copyButton, &QPushButton::clicked, &dialog, [&]() {
		QStringList hexes, readable;
		for ( int f = 0; f < fields.size(); f++ ) {
			hexes << QString::number( fields.at( f ).value, 16 );
			readable << QStringLiteral( "%1 0x%2" ).arg( fieldTag( f ) )
				.arg( QString::number( fields.at( f ).value, 16 ).rightJustified( 8, QLatin1Char( '0' ) ).toUpper() );
		}
		auto * mime = new QMimeData;
		mime->setData( QLatin1String( wwFlagsMimeType ),
			( kindId + QLatin1Char( '\n' ) + hexes.join( QLatin1Char( ',' ) ) ).toUtf8() );
		mime->setText( readable.join( QStringLiteral( "  " ) ) );
		QGuiApplication::clipboard()->setMimeData( mime );
	} );
	QObject::connect( pasteButton, &QPushButton::clicked, &dialog, [&]() {
		const QVector<quint32> values = wwFlagsFromClipboard( kindId, fields.size() );
		if ( values.isEmpty() )
			return;
		for ( int f = 0; f < fields.size(); f++ )
			fields[f].value = values.at( f );
		populate();
	} );
	QObject::connect( QGuiApplication::clipboard(), &QClipboard::dataChanged, &dialog, refreshPaste );
	QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	populate();
	refreshPaste();
	filterEdit->setFocus();

	return dialog.exec() == QDialog::Accepted;
}

}

//! Edit Shader Flags 1 and 2 as one compact, version-aware list.
class spEditShaderFlags final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Shader Flags…" ); }
	QString page() const override final { return Spell::tr( "Shader" ); }
	QIcon icon() const override final { return QIcon( ":/img/flag" ); }
	bool constant() const override final { return true; }
	bool instant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		const QString itemName = nif->itemName( index );
		if ( itemName != QLatin1String( "Shader Flags 1" ) && itemName != QLatin1String( "Shader Flags 2" ) )
			return false;
		const QModelIndex block = nif->getBlockIndex( index );
		const QModelIndex flags1 = nif->getIndex( block, "Shader Flags 1" );
		const QModelIndex flags2 = nif->getIndex( block, "Shader Flags 2" );
		return flags1.isValid() && flags2.isValid()
			&& NifValue::enumType( nif->itemStrType( flags1 ) ) == NifValue::eFlags
			&& NifValue::enumType( nif->itemStrType( flags2 ) ) == NifValue::eFlags;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QModelIndex block = nif->getBlockIndex( index );
		const QModelIndex flags1 = nif->getIndex( block, "Shader Flags 1" );
		const QModelIndex flags2 = nif->getIndex( block, "Shader Flags 2" );
		if ( !flags1.isValid() || !flags2.isValid() ) return index;

		QVector<WwFlagEntry> entries;
		auto collect = [&]( int field, const QModelIndex & flagIndex ) {
			const QString type = nif->itemStrType( flagIndex );
			const NifValue::EnumOptions & options = NifValue::enumOptionData( type );
			for ( auto it = options.o.cbegin(); it != options.o.cend(); ++it ) {
				WwFlagEntry entry;
				entry.field = field;
				entry.bit = int( it.key() );
				entry.rawName = it.value().first;
				entry.name = shaderFlagDisplayName( entry.rawName );
				entry.description = it.value().second;
				entry.category = shaderFlagCategory( entry.rawName );
				entries.append( entry );
			}
		};
		collect( 0, flags1 );
		collect( 1, flags2 );
		if ( entries.isEmpty() ) return index;

		QVector<WwFlagField> fields( 2 );
		fields[0].label = QStringLiteral( "F1" );
		fields[0].value = nif->get<quint32>( flags1 );
		fields[1].label = QStringLiteral( "F2" );
		fields[1].value = nif->get<quint32>( flags2 );
		const quint32 original1 = fields[0].value;
		const quint32 original2 = fields[1].value;

		// copy/paste compatibility follows the enum types, so e.g. Skyrim and
		// FO4 shader flag values never cross-paste (different bit meanings)
		const QString kindId = QStringLiteral( "shader/" ) + nif->itemStrType( flags1 )
			+ QLatin1Char( '+' ) + nif->itemStrType( flags2 );

		if ( wwFlagListDialog( Spell::tr( "Shader Flags" ), kindId, fields, entries )
			&& ( fields[0].value != original1 || fields[1].value != original2 ) ) {
			nifSnapshotOp( nif, Spell::tr( "Edit shader flags" ), [&]() {
				nif->set<quint32>( flags1, fields[0].value );
				nif->set<quint32>( flags2, fields[1].value );
			} );
		}
		return index;
	}
};

REGISTER_SPELL( spEditShaderFlags )

//! Edit flags
class spEditFlags : public Spell
{
public:
	QString name() const override { return Spell::tr( "Flags" ); }
	bool constant() const override { return true; }
	bool instant() const override { return true; }
	QIcon icon() const override { return QIcon( ":/img/flag" ); }

	//! Node / Property types on which flags are applicable
	enum FlagType
	{
		Alpha,
		Billboard,
		Controller,
		MatColControl,
		Node,
		RigidBody,
		Shape,
		Stencil,
		TexDesc,
		VertexColor,
		ZBuffer,
		BSX,
		NiAVObject,
		None
	};

	//! Find the index of flags relative to a given NIF index
	QModelIndex getFlagIndex( const NifModel * nif, const QModelIndex & index ) const
	{
		if ( nif->itemName( index ) == "Flags" && nif->isNiBlock( index.parent() ) )
			return index;

		if ( nif->isNiBlock( index ) )
			return nif->getIndex( index, "Flags" );

		if ( nif->blockInherits( index, "bhkRigidBody" ) ) {
			QModelIndex iFlags = nif->getIndex( nif->getBlockIndex( index ), "Col Filter" );
			iFlags = iFlags.sibling( iFlags.row(), NifModel::ValueCol );

			if ( index == iFlags )
				return iFlags;
		} else if ( nif->blockInherits( index, "BSXFlags" ) ) {
			QModelIndex iFlags = nif->getIndex( nif->getBlockIndex( index ), "Integer Data" );
			iFlags = iFlags.sibling( iFlags.row(), NifModel::ValueCol );

			if ( index == iFlags )
				return iFlags;
		} else if ( nif->itemName( index ) == "Flags" && nif->itemStrType( index.parent() ) == "TexDesc" ) {
			return index;
		}

		return QModelIndex();
	}

	//! Determine the applicable flag editing dialog for a NIF block type
	FlagType queryType( const NifModel * nif, const QModelIndex & index ) const
	{
		if ( nif->getValue( index ).isCount() ) {
			QString name = nif->itemName( index.parent() );

			if ( name == "NiAlphaProperty" ) {
				return Alpha;
			} else if ( name == "NiBillboardNode" ) {
				return Billboard;
			} else if ( nif->inherits( name, "NiTimeController" ) ) {
				if ( name == "NiMaterialColorController" ) {
					return MatColControl;
				}

				return Controller;
			} else if ( name == "NiNode" && nif->getVersionNumber() != 0x14020007 ) {
				return Node;
			} else if ( name == "bhkRigidBody" || name == "bhkRigidBodyT" ) {
				return RigidBody;
			} else if ( (name == "NiTriShape" || name == "NiTriStrips") && nif->getVersionNumber() != 0x14020007 ) {
				return Shape;
			} else if ( name == "NiStencilProperty" ) {
				return Stencil;
			} else if ( nif->itemStrType( index.parent() ) == "TexDesc" ) {
				return TexDesc;
			} else if ( name == "NiVertexColorProperty" ) {
				return VertexColor;
			} else if ( name == "NiZBufferProperty" ) {
				return ZBuffer;
			} else if ( name == "BSXFlags" ) {
				return BSX;
			} else if ( nif->blockInherits( index.parent(), "NiAVObject" ) && nif->getVersionNumber() == 0x14020007 ) {
				return NiAVObject;
			}
		}

		return None;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override
	{
		if ( nif->getBSVersion() >= 170 && index.isValid() ) {
			const NifItem *	i = nif->getItem( index );
			for ( const NifItem * j = i; j; j = j->parent() ) {
				if ( j->isAbstract() && j->hasStrType( "BSLayeredMaterial" ) ) {
					return ( i->hasName( "Layer Enable Mask" ) || i->hasName( "Blender Enable Mask" )
							|| i->hasName( "Enable Mask" ) || i->hasName( "Write Mask" ) );
				}
			}
		}
		return queryType( nif, getFlagIndex( nif, index ) ) != None;
	}

	void sfMaterialFlags( NifModel * nif, const QModelIndex & index, NifItem * i );

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override
	{
		if ( nif->getBSVersion() >= 170 && index.isValid() ) {
			NifItem *	i = nif->getItem( index );
			for ( const NifItem * j = i; j; j = j->parent() ) {
				if ( j->isAbstract() && j->hasStrType( "BSLayeredMaterial" ) ) {
					if ( i->hasName( "Layer Enable Mask" ) || i->hasName( "Blender Enable Mask" )
						|| i->hasName( "Enable Mask" ) || i->hasName( "Write Mask" ) ) {
						sfMaterialFlags( nif, index, i );
					}
					return index;
				}
			}
		}

		QModelIndex iFlags = getFlagIndex( nif, index );

		switch ( queryType( nif, iFlags ) ) {
		case Alpha:
			alphaFlags( nif, iFlags );
			break;
		case Billboard:
			billboardFlags( nif, iFlags );
			break;
		case Controller:
			controllerFlags( nif, iFlags );
			break;
		case MatColControl:
			matColControllerFlags( nif, iFlags );
			break;
		case Node:
			nodeFlags( nif, iFlags );
			break;
		case RigidBody:
			bodyFlags( nif, iFlags );
			break;
		case Shape:
			shapeFlags( nif, iFlags );
			break;
		case Stencil:
			stencilFlags( nif, iFlags );
			break;
		case TexDesc:
			texDescFlags( nif, iFlags );
			break;
		case VertexColor:
			vertexColorFlags( nif, iFlags );
			break;
		case ZBuffer:
			zbufferFlags( nif, iFlags );
			break;
		case BSX:
			bsxFlags( nif, iFlags );
			break;
		case NiAVObject:
			niavFlags( nif, iFlags );
			break;
		default:
			break;
		}

		return index;
	}

	//! Set flags on an AlphaProperty
	void alphaFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		if ( !flags )
			flags = 0xed;

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		// See glBlendFunc; ONE and ZERO appear to swapped from where they should be (not to mention missing values before SATURATE)
		QStringList blendModes{
			Spell::tr( "One" ),
			Spell::tr( "Zero" ),
			Spell::tr( "Src Color" ),
			Spell::tr( "Inv Src Color" ),
			Spell::tr( "Dst Color" ),
			Spell::tr( "Inv Dst Color" ),
			Spell::tr( "Src Alpha" ),
			Spell::tr( "Inv Src Alpha" ),
			Spell::tr( "Dst Alpha" ),
			Spell::tr( "Inv Dst Alpha" ),
			Spell::tr( "Src Alpha Saturate" )
		};
		// ALWAYS and NEVER are swapped from where they should be; see glAlphaFunc
		QStringList testModes{
			Spell::tr( "Always" ),
			Spell::tr( "Less" ),
			Spell::tr( "Equal" ),
			Spell::tr( "Less or Equal" ),
			Spell::tr( "Greater" ),
			Spell::tr( "Not Equal" ),
			Spell::tr( "Greater or Equal" ),
			Spell::tr( "Never" )
		};

		QCheckBox * chkBlend = dlgCheck( vbox, Spell::tr( "Enable Blending" ) );
		chkBlend->setChecked( flags & 1 );

		/*
		 * The enabling/disabling of blend modes and test functions that was here until r4745
		 * disallows setting values that have been reported to otherwise work.
		 */
		QComboBox * cmbSrc = dlgCombo( vbox, Spell::tr( "Source Blend Mode" ), blendModes );
		cmbSrc->setCurrentIndex( flags >> 1 & 0x0f );

		QComboBox * cmbDst = dlgCombo( vbox, Spell::tr( "Destination Blend Mode" ), blendModes );
		cmbDst->setCurrentIndex( flags >> 5 & 0x0f );

		QCheckBox * chkTest = dlgCheck( vbox, Spell::tr( "Enable Testing" ) );
		chkTest->setChecked( flags & ( 1 << 9 ) );

		QComboBox * cmbTest = dlgCombo( vbox, Spell::tr( "Alpha Test Function" ), testModes );
		cmbTest->setCurrentIndex( flags >> 10 & 0x07 );

		QSpinBox * spnTest = dlgSpin( vbox, Spell::tr( "Alpha Test Threshold" ), 0x00, 0xff );
		spnTest->setValue( nif->get<int>( nif->getBlockIndex( index ), "Threshold" ) );

		QCheckBox * chkSort = dlgCheck( vbox, Spell::tr( "No Sorter" ) );
		chkSort->setChecked( ( flags & 0x2000 ) != 0 );

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = flags & 0xfffe;

			if ( chkBlend->isChecked() ) {
				flags |= 1;
			}

			flags = ( flags & 0xffe1 ) | ( cmbSrc->currentIndex() << 1 );
			flags = ( flags & 0xfe1f ) | ( cmbDst->currentIndex() << 5 );

			flags = flags & 0xe1ff;

			if ( chkTest->isChecked() ) {
				flags |= 0x0200;
			}

			flags = ( flags & 0xe3ff ) | ( cmbTest->currentIndex() << 10 );
			nif->set<int>( nif->getBlockIndex( index ), "Threshold", spnTest->value() );

			flags = ( flags & 0xdfff ) | ( chkSort->isChecked() ? 0x2000 : 0 );

			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a Node
	void nodeFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkHidden = dlgCheck( vbox, Spell::tr( "Hidden" ) );
		chkHidden->setChecked( flags & 1 );

		QStringList collideModes{
			Spell::tr( "None" ),
			Spell::tr( "Triangles" ),
			Spell::tr( "Bounding Box" ),
			Spell::tr( "Continue" )
		};

		QComboBox * cmbCollision = dlgCombo( vbox, Spell::tr( "Collision Detection" ), collideModes );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );

		QCheckBox * chkSkin = dlgCheck( vbox, Spell::tr( "Skin Influence" ) );
		chkSkin->setChecked( !( flags & 8 ) );

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfffe ) | ( chkHidden->isChecked() ? 1 : 0 );
			flags = ( flags & 0xfff9 ) | ( cmbCollision->currentIndex() << 1 );
			flags = ( flags & 0xfff7 ) | ( chkSkin->isChecked() ? 0 : 8 );
			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a Controller
	void controllerFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkActive = dlgCheck( vbox, Spell::tr( "Active" ) );
		chkActive->setChecked( flags & 8 );

		QStringList loopModes{
			Spell::tr( "Cycle" ),
			Spell::tr( "Reverse" ),
			Spell::tr( "Clamp" )
		};

		QComboBox * cmbLoop = dlgCombo( vbox, Spell::tr( "Loop Mode" ), loopModes );
		cmbLoop->setCurrentIndex( flags >> 1 & 3 );

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfff7 ) | ( chkActive->isChecked() ? 8 : 0 );
			flags = ( flags & 0xfff9 ) | ( cmbLoop->currentIndex() << 1 );
			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a bhkRigidBody
	void bodyFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout( &dlg );

		QCheckBox * chkLinked = dlgCheck( vbox, Spell::tr( "Linked" ) );
		chkLinked->setChecked( flags & 0x80 );
		QCheckBox * chkNoCol = dlgCheck( vbox, Spell::tr( "No Collision" ) );
		chkNoCol->setChecked( flags & 0x40 );
		QCheckBox * chkScaled = dlgCheck( vbox, Spell::tr( "Scaled" ) );
		chkScaled->setChecked( flags & 0x20 );

		QSpinBox * spnPartNo = dlgSpin( vbox, Spell::tr( "Part Number" ), 0, 0x1f, chkLinked );
		spnPartNo->setValue( flags & 0x1f );

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0x7f ) | ( chkLinked->isChecked() ? 0x80 : 0 );
			flags = ( flags & 0xbf ) | ( chkNoCol->isChecked() ? 0x40 : 0 );
			flags = ( flags & 0xdf ) | ( chkScaled->isChecked() ? 0x20 : 0 );
			flags = ( flags & 0xe0 ) | ( chkLinked->isChecked() ? spnPartNo->value() : 0 );
			nif->set<int>( index, flags );
			nif->set<int>( index.parent(), "Col Filter Copy", flags );
		}
	}

	//! Set flags on a Mesh
	void shapeFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkHidden = dlgCheck( vbox, Spell::tr( "Hidden" ) );
		chkHidden->setChecked( flags & 0x01 );

		QStringList collideModes{
			Spell::tr( "None" ),
			Spell::tr( "Triangles" ),
			Spell::tr( "Bounding Box" ),
			Spell::tr( "Continue" )
		};


		QComboBox * cmbCollision = dlgCombo( vbox, Spell::tr( "Collision Detection" ), collideModes );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );

		QCheckBox * chkShadow = nullptr;

		if ( nif->checkVersion( 0x04000002, 0x04000002 ) ) {
			chkShadow = dlgCheck( vbox, Spell::tr( "Shadow" ) );
			chkShadow->setChecked( flags & 0x40 );
		}

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfffe ) | ( chkHidden->isChecked() ? 0x01 : 0 );
			flags = ( flags & 0xfff9 ) | ( cmbCollision->currentIndex() << 1 );

			if ( chkShadow ) {
				flags = ( flags & 0xffbf ) | ( chkShadow->isChecked() ? 0x40 : 0 );
			}

			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a ZBufferProperty
	void zbufferFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkEnable = dlgCheck( vbox, Spell::tr( "Enable Z Buffer" ) );
		chkEnable->setChecked( flags & 1 );

		QCheckBox * chkROnly = dlgCheck( vbox, Spell::tr( "Z Buffer Read Only" ) );
		chkROnly->setChecked( ( flags & 2 ) == 0 );

		// ALWAYS and NEVER are swapped, otherwise values match glDepthFunc
		QStringList compareFunc{
			Spell::tr( "Always" ),           // 0
			Spell::tr( "Less" ),             // 1
			Spell::tr( "Equal" ),            // 2
			Spell::tr( "Less or Equal" ),    // 3
			Spell::tr( "Greater" ),          // 4
			Spell::tr( "Not Equal" ),        // 5
			Spell::tr( "Greater or Equal" ), // 6
			Spell::tr( "Never" )            // 7
		};

		QComboBox * cmbFunc = dlgCombo( vbox, Spell::tr( "Z Buffer Test Function" ), compareFunc, chkEnable );

		if ( nif->checkVersion( 0x0401000C, 0x14000005 ) )
			cmbFunc->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Function" ) );
		else
			cmbFunc->setCurrentIndex( ( flags >> 2 ) & 0x07 );

		QCheckBox * setFlags = nullptr;

		if ( nif->checkVersion( 0x0401000C, 0x14000005 ) ) {
			setFlags = dlgCheck( vbox, Spell::tr( "Set Flags also" ) );
			setFlags->setChecked( flags > 3 );
		}

		dlgButtons( &dlg, vbox );

		// TODO: check which is used for 4.1.0.12 < x < 20.0.0.5 if flags
		// and function conflict... perhaps set flags regardless?
		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfffe ) | ( chkEnable->isChecked() ? 1 : 0 );
			flags = ( flags & 0xfffd ) | ( chkROnly->isChecked() ? 0 : 2 );

			if ( nif->checkVersion( 0x0401000C, 0x14000005 ) ) {
				nif->set<int>( nif->getBlockIndex( index ), "Function", cmbFunc->currentIndex() );
			}

			if ( nif->checkVersion( 0x14010003, 0 ) || ( setFlags != 0 && setFlags->isChecked() ) ) {
				flags = ( flags & 0xffe3 ) | ( cmbFunc->currentIndex() << 2 );
			}

			nif->set<int>( index, flags );
		}
	}

	//! Set BSX flags
	void bsxFlags( NifModel * nif, const QModelIndex & index )
	{
		const quint32 flags = nif->get<int>( index );

		const QStringList flagNames{
			Spell::tr( "Animated" ),            // 1
			Spell::tr( "Havok" ),               // 2
			Spell::tr( "Ragdoll" ),             // 4
			Spell::tr( "Complex" ),             // 8
			Spell::tr( "Addon" ),               // 16
			Spell::tr( "Editor Marker" ),       // 32
			Spell::tr( "Dynamic" ),             // 64
			Spell::tr( "Articulated" ),         // 128
			Spell::tr( "Needs Transform Updates" ),  // 256
			Spell::tr( "External Emit" ),            // 512
			//Spell::tr( "Magic Shader Particles" ), // 1024
			//Spell::tr( "Lights" ),                 // 2048
			//Spell::tr( "Breakable" ),              // 4096
			//Spell::tr( "Searched Breakable (Runtime Only?)" ) // 8192
		};

		// generic flag dialog (filter, hex footer, Copy/Paste between BSXFlags
		// blocks). Every bit is listed so unknown high bits are visible and
		// ride along with copy/paste instead of being preserved invisibly.
		QVector<WwFlagField> fields( 1 );
		fields[0].value = flags;
		QVector<WwFlagEntry> entries;
		for ( int bit = 0; bit < 32; bit++ ) {
			WwFlagEntry entry;
			entry.bit = bit;
			entry.name = ( bit < flagNames.size() && !flagNames.at( bit ).isEmpty() )
				? flagNames.at( bit )
				: Spell::tr( "Bit %1" ).arg( bit );
			entries.append( entry );
		}

		if ( wwFlagListDialog( Spell::tr( "BSX Flags" ), QStringLiteral( "bsxflags" ), fields, entries )
			&& fields[0].value != flags ) {
			nif->set<int>( index, fields[0].value );
		}
	}

	//! Set NiAVObject flags
	void niavFlags( NifModel * nif, const QModelIndex & index )
	{
		const quint32 flags = nif->get<int>( index );

		const QStringList flagNames{
			Spell::tr( "Hidden" ),                          // 1
			Spell::tr( "Selective Update" ),                // 2
			Spell::tr( "Selective Update Transforms" ),     // 4
			Spell::tr( "Selective Update Controller" ),     // 8
			Spell::tr( "Selective Update Rigid" ),          // 16
			Spell::tr( "Display (UI?) Object" ),            // 32
			Spell::tr( "Disable Sorting" ),                 // 64
			Spell::tr( "Sel. Upd. Transforms Override" ),   // 128
			Spell::tr( "" ),                                // 256
			Spell::tr( "Save External Geom Data" ),         // 512
			Spell::tr( "No Decals" ),                       // 1024
			Spell::tr( "Always Draw" ),                     // 2048
			Spell::tr( "Mesh LOD (FO4)" ),                  // 4096
			Spell::tr( "Fixed Bound" ),                     // 8192
			Spell::tr( "Top Fade Node" ),                   // 16384
			Spell::tr( "Ignore Fade" ),                     // 32768
			Spell::tr( "No Anim Sync (X)" ),                // 65536
			Spell::tr( "No Anim Sync (Y)" ),                // 1 << 17
			Spell::tr( "No Anim Sync (Z)" ),                // 1 << 18
			Spell::tr( "No Anim Sync (S)" ),                // 1 << 19
			Spell::tr( "No Dismember" ),                    // 1 << 20
			Spell::tr( "No Dismember Validity" ),           // 1 << 21
			Spell::tr( "Render Use" ),                      // 1 << 22
			Spell::tr( "Materials Applied" ),               // 1 << 23
			Spell::tr( "High Detail" ),                     // 1 << 24
			Spell::tr( "Force Update" ),                    // 1 << 25
			Spell::tr( "Pre-Processed Node" ),              // 1 << 26
			Spell::tr( "Mesh LOD (Skyrim)" ),               // 1 << 27
			Spell::tr( "Bit 28" ),                          // 1 << 28
			Spell::tr( "Bit 29" ),                          // 1 << 29
			Spell::tr( "Bit 30" ),                          // 1 << 30
			Spell::tr( "Bit 31" ),                          // 1 << 31

		};

		// generic flag dialog (filter, hex footer, Copy/Paste between
		// NiAVObject flag fields — nodes and shapes share the same layout)
		QVector<WwFlagField> fields( 1 );
		fields[0].value = flags;
		QVector<WwFlagEntry> entries;
		for ( int bit = 0; bit < 32; bit++ ) {
			WwFlagEntry entry;
			entry.bit = bit;
			entry.name = ( bit < flagNames.size() && !flagNames.at( bit ).isEmpty() )
				? flagNames.at( bit )
				: Spell::tr( "Bit %1" ).arg( bit );
			entries.append( entry );
		}

		if ( wwFlagListDialog( Spell::tr( "Node Flags" ), QStringLiteral( "niavobject" ), fields, entries )
			&& fields[0].value != flags ) {
			nif->set<int>( index, fields[0].value );
		}
	}

	//! Set flags on a BillboardNode
	void billboardFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		if ( !flags )
			flags = 0x8;

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkHidden = dlgCheck( vbox, Spell::tr( "Hidden" ) );
		chkHidden->setChecked( flags & 1 );

		QStringList collideModes{
			Spell::tr( "None" ),                // 0
			Spell::tr( "Triangles" ),           // 2
			Spell::tr( "Bounding Box" ),        // 4
			Spell::tr( "Continue" ),            // 6
		};

		QStringList billboardModes{
			Spell::tr( "Always Face Camera" ),  // 0
			Spell::tr( "Rotate About Up" ),     // 32
			Spell::tr( "Rigid Face Camera" ),   // 64
			Spell::tr( "Always Face Center" )   // 96
		};

		QComboBox * cmbCollision = dlgCombo( vbox, Spell::tr( "Collision Detection" ), collideModes );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );

		QComboBox * cmbMode = dlgCombo( vbox, Spell::tr( "Billboard Mode" ), billboardModes );

		// Billboard Mode is an enum as of 10.1.0.0
		if ( nif->checkVersion( 0x0A010000, 0 ) ) {
			// this value doesn't exist before 10.1.0.0
			// ROTATE_ABOUT_UP2 is too hard to put in and possibly meaningless
			cmbMode->addItem( Spell::tr( "Rigid Face Center" ) );
			cmbMode->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Billboard Mode" ) );
		} else {
			cmbMode->setCurrentIndex( flags >> 5 & 3 );
		}

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfffe ) | ( chkHidden->isChecked() ? 1 : 0 );
			flags = ( flags & 0xfff9 ) | ( cmbCollision->currentIndex() << 1);

			if ( nif->checkVersion( 0x0A010000, 0 ) ) {
				nif->set<int>( nif->getBlockIndex( index ), "Billboard Mode", cmbMode->currentIndex() );
			} else {
				flags = ( flags & 0xff9f ) | ( cmbMode->currentIndex() << 5 );
				flags = ( flags & 0xfff7 ) | 8; // seems to always be set but has no known effect
			}

			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a StencilProperty
	void stencilFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkEnable = dlgCheck( vbox, Spell::tr( "Stencil Enable" ) );

		QStringList stencilActions{
			Spell::tr( "Keep" ),      // 0
			Spell::tr( "Zero" ),      // 1
			Spell::tr( "Replace" ),   // 2
			Spell::tr( "Increment" ), // 3
			Spell::tr( "Decrement" ), // 4
			Spell::tr( "Invert" )     // 5
		};


		QComboBox * cmbFail = dlgCombo( vbox, Spell::tr( "Fail Action" ), stencilActions );

		QComboBox * cmbZFail = dlgCombo( vbox, Spell::tr( "Z Fail Action" ), stencilActions );

		QComboBox * cmbPass = dlgCombo( vbox, Spell::tr( "Pass Action" ), stencilActions );

		QStringList drawModes{
			Spell::tr( "Counter clock wise or Both" ), // 0
			Spell::tr( "Draw counter clock wise" ),    // 1
			Spell::tr( "Draw clock wise" ),            // 2
			Spell::tr( "Draw Both" )                   // 3
		};

		QComboBox * cmbDrawMode = dlgCombo( vbox, Spell::tr( "Draw Mode" ), drawModes );

		// Appears to match glStencilFunc
		QStringList compareFunc{
			Spell::tr( "Never" ),            // 0
			Spell::tr( "Less" ),             // 1
			Spell::tr( "Equal" ),            // 2
			Spell::tr( "Less or Equal" ),    // 3
			Spell::tr( "Greater" ),          // 4
			Spell::tr( "Not Equal" ),        // 5
			Spell::tr( "Greater or Equal" ), // 6
			Spell::tr( "Always" )            // 7
		};

		QComboBox * cmbFunc = dlgCombo( vbox, Spell::tr( "Stencil Function" ), compareFunc );

		// prior to 20.1.0.3 flags itself appears unused; after 10.0.1.2 and until 20.1.0.3 it is not present
		// 20.0.0.5 is the last version with these values
		if ( nif->checkVersion( 0, 0x14000005 ) ) {
			// set based on Stencil Enabled, Stencil Function, Fail Action, Z Fail Action, Pass Action, Draw Mode
			// Possibly include Stencil Ref and Stencil Mask except they don't seem to ever vary from the default
			chkEnable->setChecked( nif->get<bool>( nif->getBlockIndex( index ), "Stencil Enabled" ) );
			cmbFail->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Fail Action" ) );
			cmbZFail->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Z Fail Action" ) );
			cmbPass->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Pass Action" ) );
			cmbDrawMode->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Draw Mode" ) );
			cmbFunc->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Stencil Function" ) );
		} else {
			// set based on flags itself
			chkEnable->setChecked( flags & 1 );
			cmbFail->setCurrentIndex( flags >> 1 & 7 );
			cmbZFail->setCurrentIndex( flags >> 4 & 7 );
			cmbPass->setCurrentIndex( flags >> 7 & 7 );
			cmbDrawMode->setCurrentIndex( flags >> 10 & 3 );
			cmbFunc->setCurrentIndex( flags >> 12 & 7 );
		}

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			if ( nif->checkVersion( 0, 0x14000005 ) ) {
				nif->set<bool>( nif->getBlockIndex( index ), "Stencil Enabled", chkEnable->isChecked() );
				nif->set<int>( nif->getBlockIndex( index ), "Fail Action", cmbFail->currentIndex() );
				nif->set<int>( nif->getBlockIndex( index ), "Z Fail Action", cmbZFail->currentIndex() );
				nif->set<int>( nif->getBlockIndex( index ), "Pass Action", cmbPass->currentIndex() );
				nif->set<int>( nif->getBlockIndex( index ), "Draw Mode", cmbDrawMode->currentIndex() );
				nif->set<int>( nif->getBlockIndex( index ), "Stencil Function", cmbFunc->currentIndex() );
			} else {
				flags = ( flags & 0xfffe ) | ( chkEnable->isChecked() ? 1 : 0 );
				flags = ( flags & 0xfff1 ) | ( cmbFail->currentIndex() << 1 );
				flags = ( flags & 0xff8f ) | ( cmbZFail->currentIndex() << 4 );
				flags = ( flags & 0xfc7f ) | ( cmbPass->currentIndex() << 7 );
				flags = ( flags & 0xf3ff ) | ( cmbDrawMode->currentIndex() << 10 );
				flags = ( flags & 0x8fff ) | ( cmbFunc->currentIndex() << 12 );
				nif->set<int>( index, flags );
			}
		}
	}

	//! Set flags on a VertexColorProperty
	void vertexColorFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		// For versions < 20.1.0.3 (<= 20.0.0.5), give option to set flags regardless
		QCheckBox * setFlags = nullptr;

		if ( nif->checkVersion( 0, 0x14000005 ) ) {
			setFlags = dlgCheck( vbox, Spell::tr( "Set Flags also" ) );
			setFlags->setChecked( flags != 0 );
		}

		QStringList lightMode{
			Spell::tr( "Emissive" ),
			Spell::tr( "Emissive + Ambient + Diffuse" )
		};

		QComboBox * cmbLight = dlgCombo( vbox, Spell::tr( "Lighting Mode" ), lightMode );

		QStringList vertMode{
			Spell::tr( "Source Ignore" ),
			Spell::tr( "Source Emissive" ),
			Spell::tr( "Source Ambient/Diffuse" )
		};

		QComboBox * cmbVert = dlgCombo( vbox, Spell::tr( "Vertex Mode" ), vertMode );

		// Use enums in preference to flags since they probably have a higher priority
		if ( nif->checkVersion( 0, 0x14000005 ) ) {
			cmbLight->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Lighting Mode" ) );
			cmbVert->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Vertex Mode" ) );
		} else {
			cmbLight->setCurrentIndex( flags >> 3 & 1 );
			cmbVert->setCurrentIndex( flags >> 4 & 3 );
		}

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			if ( nif->checkVersion( 0, 0x14000005 ) ) {
				nif->set<int>( nif->getBlockIndex( index ), "Lighting Mode", cmbLight->currentIndex() );
				nif->set<int>( nif->getBlockIndex( index ), "Vertex Mode", cmbVert->currentIndex() );
			}

			if ( nif->checkVersion( 0x14010003, 0 ) || ( setFlags != 0 && setFlags->isChecked() ) ) {
				flags = ( flags & 0xfff7 ) | ( cmbLight->currentIndex() << 3 );
				flags = ( flags & 0xffcf ) | ( cmbVert->currentIndex() << 4 );
				nif->set<int>( index, flags );
			}
		}
	}

	//! Set flags on a MaterialColorController
	void matColControllerFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QCheckBox * chkActive = dlgCheck( vbox, Spell::tr( "Active" ) );
		chkActive->setChecked( flags & 8 );

		QStringList loopModes{
			Spell::tr( "Cycle" ), Spell::tr( "Reverse" ), Spell::tr( "Clamp" )
		};

		QComboBox * cmbLoop = dlgCombo( vbox, Spell::tr( "Loop Mode" ), loopModes );
		cmbLoop->setCurrentIndex( flags >> 1 & 3 );

		QStringList targetColor{
			Spell::tr( "Ambient" ), Spell::tr( "Diffuse" ),
			Spell::tr( "Specular" ), Spell::tr( "Emissive" )
		};

		QComboBox * cmbColor = dlgCombo( vbox, Spell::tr( "Target Color" ), targetColor );

		// Target Color enum exists as of 10.1.0.0
		if ( nif->checkVersion( 0x0A010000, 0 ) ) {
			cmbColor->setCurrentIndex( nif->get<int>( nif->getBlockIndex( index ), "Target Color" ) );
		} else {
			cmbColor->setCurrentIndex( flags >> 4 & 3 );
		}

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0xfff7 ) | ( chkActive->isChecked() ? 8 : 0 );
			flags = ( flags & 0xfff9 ) | ( cmbLoop->currentIndex() << 1 );

			if ( nif->checkVersion( 0x0A010000, 0 ) ) {
				nif->set<int>( nif->getBlockIndex( index ), "Target Color", cmbColor->currentIndex() );
			} else {
				flags = ( flags & 0xffcf ) | ( cmbColor->currentIndex() << 4 );
			}

			nif->set<int>( index, flags );
		}
	}

	//! Set flags on a TexDesc struct
	void texDescFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QSpinBox * spnUVIndex = dlgSpin( vbox, Spell::tr( "UV Index" ), 0, 0xFF );
		spnUVIndex->setValue( flags & 0x00FF );

		QStringList clampModes{
			Spell::tr( "Clamp Both" ),
			Spell::tr( "Clamp S Wrap T" ),
			Spell::tr( "Wrap S Clamp T" ),
			Spell::tr( "Wrap Both" )
		};

		QComboBox * cmbClamp = dlgCombo( vbox, Spell::tr( "Clamp Mode" ), clampModes );
		cmbClamp->setCurrentIndex( ( flags & 0xF000 ) >> 0x0C );

		QStringList filterModes{
			Spell::tr( "FILTER_NEAREST" ),
			Spell::tr( "FILTER_BILERP" ),
			Spell::tr( "FILTER_TRILERP" ),
			Spell::tr( "FILTER_NEAREST_MIPNEAREST" ),
			Spell::tr( "FILTER_NEAREST_MIPLERP" ),
			Spell::tr( "FILTER_BILERP_MIPNEAREST" )
		};

		QComboBox * cmbFilter = dlgCombo( vbox, Spell::tr( "Filter Mode" ), filterModes );
		cmbFilter->setCurrentIndex( ( flags & 0x0F00 ) >> 0x08 );

		dlgButtons( &dlg, vbox );

		if ( dlg.exec() == QDialog::Accepted ) {
			flags = ( flags & 0x0FFF ) | ( cmbClamp->currentIndex() << 0x0C );
			flags = ( flags & 0xF0FF ) | ( cmbFilter->currentIndex() << 0x08 );
			flags = ( flags & 0xFF00 ) | spnUVIndex->value();
			nif->set<int>( index, flags );
		}
	}

	//! Add a checkbox to a dialog
	/*!
	 * \param vbox Vertical box layout to add the checkbox to
	 * \param name The name to give the checkbox
	 * \param chk A checkbox that enables or disables this checkbox
	 * \return A pointer to the checkbox
	 */
	QCheckBox * dlgCheck( QVBoxLayout * vbox, const QString & name, QCheckBox * chk = nullptr )
	{
		QCheckBox * box = new QCheckBox( name );
		vbox->addWidget( box );

		if ( chk ) {
			QObject::connect( chk, &QCheckBox::toggled, box, &QCheckBox::setEnabled );
			box->setEnabled( chk->isChecked() );
		}

		return box;
	}

	//! Add a combobox to a dialog
	/*!
	 * \param vbox Vertical box layout to add the combobox to
	 * \param name The name to give the combobox
	 * \param items The items to add to the combobox
	 * \param chk A checkbox that enables or disables this combobox
	 * \return A pointer to the combobox
	 */
	QComboBox * dlgCombo( QVBoxLayout * vbox, const QString & name, QStringList items, QCheckBox * chk = nullptr )
	{
		vbox->addWidget( new QLabel( name ) );
		QComboBox * cmb = new QComboBox;
		vbox->addWidget( cmb );
		cmb->addItems( items );

		if ( chk ) {
			QObject::connect( chk, &QCheckBox::toggled, cmb, &QComboBox::setEnabled );
			cmb->setEnabled( chk->isChecked() );
		}

		return cmb;
	}

	//! Add a spinbox to a dialog
	/*!
	 * \param vbox Vertical box layout to add the spinbox to
	 * \param name The name to give the spinbox
	 * \param min The minimum value of the spinbox
	 * \param max The maximum value of the spinbox
	 * \param chk A checkbox that enables or disables this spinbox
	 * \return A pointer to the spinbox
	 */
	QSpinBox * dlgSpin( QVBoxLayout * vbox, const QString & name, int min, int max, QCheckBox * chk = nullptr )
	{
		vbox->addWidget( new QLabel( name ) );
		QSpinBox * spn = new QSpinBox;
		vbox->addWidget( spn );
		spn->setRange( min, max );

		if ( chk ) {
			QObject::connect( chk, &QCheckBox::toggled, spn, &QSpinBox::setEnabled );
			spn->setEnabled( chk->isChecked() );
		}

		return spn;
	}

	//! Add standard buttons to a dialog
	/*!
	 * \param dlg The dialog to add buttons to
	 * \param vbox Vertical box layout used by the dialog
	 */
	void dlgButtons( QDialog * dlg, QVBoxLayout * vbox )
	{
		QHBoxLayout * hbox = new QHBoxLayout;
		vbox->addLayout( hbox );

		QPushButton * btAccept = new QPushButton( Spell::tr( "Accept" ) );
		hbox->addWidget( btAccept );
		QObject::connect( btAccept, &QPushButton::clicked, dlg, &QDialog::accept );

		QPushButton * btReject = new QPushButton( Spell::tr( "Cancel" ) );
		hbox->addWidget( btReject );
		QObject::connect( btReject, &QPushButton::clicked, dlg, &QDialog::reject );
	}
};

void spEditFlags::sfMaterialFlags( NifModel * nif, const QModelIndex & index, NifItem * i )
{
	static const char *	flagNames_layerEnableMask[6] = {
		"Enable Layer 1", "Enable Layer 2", "Enable Layer 3", "Enable Layer 4", "Enable Layer 5", "Enable Layer 6"
	};
	static const char *	flagNames_blenderEnableMask[5] = {
		"Enable Blender 1", "Enable Blender 2", "Enable Blender 3", "Enable Blender 4", "Enable Blender 5"
	};
	static const char *	flagNames_textureEnableMask[21] = {
		"Enable Albedo", "Enable Normal Map", "Enable Opacity", "Enable Roughness", "Enable Metalness",
		"Enable Ambient Occlusion", "Enable Height Map", "Enable Emissive Map", "Enable Transmissive Map",
		"Enable Curvature Map", "Enable Mask", "Enable Texture 11", "Enable Z Offset Map", "Enable Texture 13",
		"Enable Overlay Color", "Enable Overlay Roughness", "Enable Overlay Metalness", "Enable Texture 17",
		"Enable Texture 18", "Enable Texture 19", "Enable ID Texture"
	};
	static const char *	flagNames_decalWriteMask[24] = {
		"Output Albedo R", "Output Albedo G", "Output Albedo B", "Unused", "Output Normal X", "Output Normal Y",
		"Unused", "Unused", "Output Ambient Occlusion", "Output Roughness", "Output Metalness", "Unused",
		"Unused", "Unused", "Unused", "Unused", "Unused", "Unused", "Unused", "Unused",
		"Output Emissive R", "Output Emissive G", "Output Emissive B", "Output Emissive A"
	};

	quint32 flags = nif->get<int>( index );

	QDialog dlg;
	QVBoxLayout * vbox = new QVBoxLayout( &dlg );

	QStringList flagNames;
	const char * const *	flagNamesData = flagNames_layerEnableMask;
	size_t	flagNamesSize = sizeof( flagNames_layerEnableMask ) / sizeof( char * );
	if ( i->hasName( "Blender Enable Mask" ) ) {
		flagNamesData = flagNames_blenderEnableMask;
		flagNamesSize = sizeof( flagNames_blenderEnableMask ) / sizeof( char * );
	} else if ( i->hasName( "Enable Mask" ) ) {
		flagNamesData = flagNames_textureEnableMask;
		flagNamesSize = sizeof( flagNames_textureEnableMask ) / sizeof( char * );
	} else if ( i->hasName( "Write Mask" ) ) {
		flagNamesData = flagNames_decalWriteMask;
		flagNamesSize = sizeof( flagNames_decalWriteMask ) / sizeof( char * );
	}
	for ( size_t i = 0; i < flagNamesSize; i++ )
		flagNames.append( QString::fromLatin1( flagNamesData[i] ) );

	QList<QCheckBox *> chkBoxes;
	int x = 0;
	for ( const QString& flagName : flagNames ) {
		chkBoxes << dlgCheck( vbox, QString( "%1: %2 (%3)" ).arg( Spell::tr( "Bit %1" ).arg( x ) )
							  .arg( flagName ).arg( (uint)(1 << x) ) );
		chkBoxes.last()->setChecked( flags & (1 << x) );
		x++;
	}

	dlgButtons( &dlg, vbox );

	if ( dlg.exec() == QDialog::Accepted ) {
		x = 0;
		for ( QCheckBox * chk : chkBoxes ) {
			flags = (flags & (~(1 << x))) | (chk->isChecked() ? 1 << x : 0);
			x++;
		}
		for ( i = nif->getItem( index ); i; i = i->parent() ) {
			if ( i->hasStrType( "BSLayeredMaterial" ) && !nif->get<bool>( i, "Is Modified" ) ) {
				nif->set<bool>( i, "Is Modified", true );
				QMessageBox::warning( nullptr, "NifSkope warning", QString( "Changes to material data are not saved automatically, use the spell 'Material/Save Edited Material...'" ) );
				break;
			}
		}
		nif->set<int>( index, flags );
	}
}

REGISTER_SPELL( spEditFlags )


class spEditVertexDesc final : public spEditFlags
{
public:
	QString name() const override final { return Spell::tr( "Vertex Flags" ); }
	bool instant() const override final { return true; }
	QIcon icon() const override final { return QIcon( ":/img/flag" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->blockInherits( index.parent(), "BSTriShape" ) && nif->itemName( index ) == "Vertex Desc";
	}

	static void getVertexPositions( QVector<Vector4> & verts, const NifModel * nif, const QModelIndex & iShape );
	static void setVertexPositions( const QVector<Vector4> & verts, NifModel * nif, const QModelIndex & iShape );

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		auto desc = nif->get<BSVertexDesc>( index );
		bool dynamic = nif->blockInherits( index.parent(), "BSDynamicTriShape" );

		QStringList flagNames {
			Spell::tr( "Vertex" ),	  // VA_POSITION = 0x0,
			Spell::tr( "UVs" ),		  // VA_TEXCOORD0 = 0x1,
			Spell::tr( "UVs 2" ),	  // VA_TEXCOORD1 = 0x2,
			Spell::tr( "Normals" ),	  // VA_NORMAL = 0x3,
			Spell::tr( "Tangents" ),  // VA_BINORMAL = 0x4,
			Spell::tr( "Colors" ),	  // VA_COLOR = 0x5,
			Spell::tr( "Skinned" ),	  // VA_SKINNING = 0x6,
			Spell::tr( "Land Data" ), // VA_LANDDATA = 0x7,
			Spell::tr( "Eye Data" ),  // VA_EYEDATA = 0x8,
			Spell::tr( "" ),
			Spell::tr( "Full Precision" ) // 1 << 10
		};

		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );

		QList<QCheckBox *> chkBoxes;
		int x = 0;
		for ( const QString& flagName : flagNames ) {
			chkBoxes << dlgCheck( vbox, flagName );
			chkBoxes.last()->setChecked( desc.HasFlag( VertexAttribute(x) ) );
			// Hide unused attributes
			if ( x == 2 || x == 7 || x == 9 )
				chkBoxes.last()->setHidden( true );
			x++;
		}

		dlgButtons( &dlg, vbox );

		bool fullPrecisionChanged = false;
		if ( dlg.exec() == QDialog::Accepted ) {
			x = 0;
			for ( QCheckBox * chk : chkBoxes ) {
				if ( x == 10 )
					fullPrecisionChanged = ( chk->isChecked() != desc.HasFlag( VertexAttribute(x) ) );
				if ( chk->isChecked() )
					desc.SetFlag( VertexAttribute(x) );
				else
					desc.RemoveFlag( VertexAttribute(x) );
				x++;
			}

			// Make sure sizes and offsets in rest of vertexDesc are updated from flags
			desc.ResetAttributeOffsets( nif->getBSVersion() );
			if ( dynamic )
				desc.MakeDynamic();

			QModelIndex iBlock = nif->getBlockIndex( index );

			QVector<Vector4> verts;
			if ( fullPrecisionChanged )
				getVertexPositions( verts, nif, iBlock );

			if ( nif->set<BSVertexDesc>( index, desc ) ) {
				auto iDataSize = nif->getIndex( iBlock, "Data Size" );
				auto numVerts = nif->get<uint>( iBlock, "Num Vertices" );
				auto numTris = nif->get<uint>( iBlock, "Num Triangles" );

				if ( iDataSize.isValid() )
					nif->set<uint>( iDataSize, desc.GetVertexSize() * numVerts + 6 * numTris );

				if ( auto iData = nif->getIndex( iBlock, "Vertex Data" ); iData.isValid() )
					nif->updateArraySize( iData );

				if ( fullPrecisionChanged )
					setVertexPositions( verts, nif, iBlock );
			}

			if ( ( desc & VertexFlags::VF_SKINNED ) && nif->getBSVersion() == 100 ) {
				// Skinned SSE
				auto skinID = nif->getLink( nif->getIndex( iBlock, "Skin" ) );
				auto partID = nif->getLink( nif->getBlockIndex( skinID, "NiSkinInstance" ), "Skin Partition" );
				if ( auto iPartBlock = nif->getBlockIndex( partID, "NiSkinPartition" ); iPartBlock.isValid() ) {
					if ( fullPrecisionChanged )
						getVertexPositions( verts, nif, iPartBlock );
					nif->set<BSVertexDesc>( iPartBlock, "Vertex Desc", desc );
					quint32 numVerts = 0;
					if ( auto iData = nif->getIndex( iPartBlock, "Vertex Data" ); iData.isValid() )
						numVerts = quint32( nif->rowCount( iData ) );
					nif->set<quint32>( iPartBlock, "Vertex Size", quint32( desc.GetVertexSize() ) );
					nif->set<quint32>( iPartBlock, "Data Size", quint32( desc.GetVertexSize() ) * numVerts );
					if ( fullPrecisionChanged )
						setVertexPositions( verts, nif, iPartBlock );
				}
			}
		}

		return index;
	}

};

void spEditVertexDesc::getVertexPositions( QVector<Vector4> & verts, const NifModel * nif, const QModelIndex & iShape )
{
	if ( auto iData = nif->getIndex( iShape, "Vertex Data" ); iData.isValid() ) {
		int n = nif->rowCount( iData );
		verts.resize( n );
		for ( int i = 0; i < n; i++ ) {
			if ( auto iVertex = nif->getItem( iData, i, false ); iVertex ) {
				Vector4 v = Vector4( nif->get<Vector3>( iVertex, "Vertex" ) );
				v[3] = nif->get<float>( iVertex, "Bitangent X" );
				verts[i] = v;
			}
		}
	}
}

void spEditVertexDesc::setVertexPositions( const QVector<Vector4> & verts, NifModel * nif, const QModelIndex & iShape )
{
	if ( auto iData = nif->getIndex( iShape, "Vertex Data" ); iData.isValid() ) {
		nif->setState( BaseModel::Processing );
		int n = nif->rowCount( iData );
		for ( int i = 0; i < n; i++ ) {
			if ( auto iVertex = nif->getItem( iData, i, false ); iVertex ) {
				Vector4 v;
				if ( i < verts.size() )
					v = verts.at( i );
				if ( auto iVertexPosition = nif->getItem( iVertex, "Vertex" ); iVertexPosition ) {
					if ( iVertexPosition->valueType() == NifValue::tVector3 )
						nif->set<Vector3>( iVertexPosition, Vector3(v) );
					else
						nif->set<HalfVector3>( iVertexPosition, HalfVector3( Vector3(v) ) );
				}
				if ( auto iBitangentX = nif->getItem( iVertex, "Bitangent X" ); iBitangentX )
					nif->set<float>( iBitangentX, v[3] );
			}
		}
		nif->restoreState();
	}
}

REGISTER_SPELL( spEditVertexDesc )
