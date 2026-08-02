#include "nifskope.h"
#include "glview.h"
#include "spellbook.h"
#include "gl/hknpdecode.h"
#include "skeletontools.h"
#include "gl/hknpencode.h"
#include "model/nifmodel.h"
#include "data/nifvalue.h"
#include "nifsnapshot.h"
#include "wwskin.h"
#include "lib/qhull.h"
#include "lib/nvtristripwrapper.h"
#include "spells/blocks.h"
#include "ui/widgets/physicspanel.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QColor>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QFormLayout>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QBuffer>
#include <QDataStream>

#include <algorithm>
#include <cmath>

bool tlBuildCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls,
	QVector<Vector3> & triangleSoup, QString & statistics );
QModelIndex tlCommitCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls );

/*! Block lookup and shape-chain descent, at file scope.
 *
 *  These were members of CollisionManagerPanel below, which is the only reason
 *  the collision lint could not be reached from anywhere else — they are `const`
 *  and touch nothing but `nif`. Lifted here so the whole-file collision check can
 *  walk the same shapes the panel does; the panel's own methods forward to them,
 *  so there is one implementation and not a call site changed.
 */
static QModelIndex tlCollBlockIndex( const NifModel * nif, int block )
{
	return nif && nif->isValidBlockNumber( block ) ? nif->getBlockIndex( block ) : QModelIndex();
}

//! The first shape that is not a wrapper: descends Mopp/Transform/List chains.
static int tlCollFirstLeafShape( const NifModel * nif, int shape, int depth = 0 )
{
	if ( !nif || !nif->isValidBlockNumber( shape ) || depth > 12 )
		return -1;
	QModelIndex i = tlCollBlockIndex( nif, shape );
	QString t = nif->itemName( i );
	if ( t == QLatin1String( "bhkMoppBvTreeShape" )
		 || t == QLatin1String( "bhkTransformShape" )
		 || t == QLatin1String( "bhkConvexTransformShape" ) )
		return tlCollFirstLeafShape( nif, nif->getLink( i, "Shape" ), depth + 1 );
	if ( t == QLatin1String( "bhkListShape" ) || t == QLatin1String( "bhkConvexListShape" ) ) {
		QVector<qint32> links = nif->getLinkArray( i, "Sub Shapes" );
		return links.isEmpty() ? shape : tlCollFirstLeafShape( nif, links.first(), depth + 1 );
	}
	return shape;
}

namespace {

enum CollisionRoles {
	ObjectBlockRole = Qt::UserRole + 1,
	NodeBlockRole,
	SystemBlockRole,
	BodyBlockRole,
	ShapeBlockRole,
	BodyIdRole,
	CompiledRole,
	ShapeIndexRole
};

static QString basicCollisionLayerName( quint32 layer )
{
	switch ( layer ) {
	case 1: return QObject::tr( "STATIC (1)" );
	case 10: return QObject::tr( "PROPS (10)" );
	case 31: return QObject::tr( "STAIRHELPER (31)" );
	case 56: return QObject::tr( "CHARBUMPER (56)" );
	default: return QObject::tr( "Layer %1" ).arg( layer );
	}
}

static QString materialCrcText( quint32 crc )
{
	if ( !crc ) return QObject::tr( "Unknown" );
	QString hex = QStringLiteral( "%1" ).arg( crc, 8, 16, QLatin1Char( '0' ) ).toUpper();
	return QObject::tr( "Unknown (0x%1)" ).arg( hex );
}

class CollisionTreeItem final : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;
	bool operator<( const QTreeWidgetItem & other ) const override
	{
		int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		if ( column == 4 )
			return text( column ).section( QLatin1Char( ' ' ), 0, 0 ).toDouble()
				< other.text( column ).section( QLatin1Char( ' ' ), 0, 0 ).toDouble();
		return QString::localeAwareCompare( text( column ), other.text( column ) ) < 0;
	}
};

//! The same Blender-style scrub field used by the transform operator panel:
//! drag horizontally to change, click to type, hover the edges to step.
class CollisionDragSpinBox final : public QDoubleSpinBox
{
public:
	explicit CollisionDragSpinBox( QWidget * parent = nullptr ) : QDoubleSpinBox( parent )
	{
		setButtonSymbols( QAbstractSpinBox::NoButtons );
		setAlignment( Qt::AlignCenter );
		setStyleSheet( QStringLiteral(
			"QDoubleSpinBox { background:%1; border:none; border-radius:3px; color:%2; }"
			"QLineEdit { background:transparent; border:none; color:%2;"
			" selection-background-color:%3; selection-color:white; }" )
			.arg( wwSkinColor( "bgInput" ), wwSkinColor( "text" ), wwSkinColor( "bgBtnDown" ) ) );
		if ( QLineEdit * edit = lineEdit() ) {
			edit->installEventFilter( this );
			edit->setMouseTracking( true );
			edit->setCursor( Qt::SizeHorCursor );
		}
	}
protected:
	static constexpr int arrowWidth = 16;
	void resizeEvent( QResizeEvent * event ) override
	{
		QDoubleSpinBox::resizeEvent( event );
		if ( QLineEdit * edit = lineEdit() )
			edit->setGeometry( arrowWidth, 0, std::max( width() - 2 * arrowWidth, 0 ), height() );
	}
	void enterEvent( QEnterEvent * event ) override { QDoubleSpinBox::enterEvent( event ); hover = true; update(); }
	void leaveEvent( QEvent * event ) override { QDoubleSpinBox::leaveEvent( event ); hover = false; update(); }
	void mousePressEvent( QMouseEvent * event ) override
	{
		if ( event->button() == Qt::LeftButton ) {
			int x = int( event->position().x() );
			if ( x < arrowWidth ) { stepBy( -1 ); return; }
			if ( x > width() - arrowWidth ) { stepBy( 1 ); return; }
		}
		QDoubleSpinBox::mousePressEvent( event );
	}
	bool eventFilter( QObject * object, QEvent * event ) override
	{
		QLineEdit * edit = lineEdit();
		if ( object == edit ) {
			if ( event->type() == QEvent::MouseButtonPress ) {
				auto * mouse = static_cast<QMouseEvent *>( event );
				if ( mouse->button() == Qt::LeftButton ) {
					dragging = true; moved = false; pressX = int( mouse->globalPosition().x() ); startValue = value();
					return true;
				}
			} else if ( event->type() == QEvent::MouseMove && dragging ) {
				auto * mouse = static_cast<QMouseEvent *>( event );
				int delta = int( mouse->globalPosition().x() ) - pressX;
				if ( !moved && std::abs( delta ) > 2 ) { moved = true; update(); }
				if ( moved ) {
					double sensitivity = ( mouse->modifiers() & Qt::ShiftModifier ) ? 0.01 : 0.1;
					setValue( startValue + double( delta ) * singleStep() * sensitivity );
				}
				return true;
			} else if ( event->type() == QEvent::MouseButtonRelease && dragging ) {
				dragging = false;
				if ( !moved ) { edit->selectAll(); edit->setFocus(); }
				moved = false; update(); return true;
			}
		}
		return QDoubleSpinBox::eventFilter( object, event );
	}
	void paintEvent( QPaintEvent * event ) override
	{
		QDoubleSpinBox::paintEvent( event );
		QPainter painter( this ); painter.setRenderHint( QPainter::Antialiasing );
		if ( dragging && moved ) {
			painter.setPen( Qt::NoPen ); painter.setBrush( QColor( 255, 255, 255, 45 ) );
			painter.drawRoundedRect( rect().adjusted( 0, 0, -1, -1 ), 3.0, 3.0 );
		}
		if ( hover && !( lineEdit() && lineEdit()->hasFocus() ) ) {
			painter.setPen( QColor( 230, 230, 230 ) );
			painter.drawText( QRect( 0, 0, arrowWidth, height() ), Qt::AlignCenter, QStringLiteral( "‹" ) );
			painter.drawText( QRect( width() - arrowWidth, 0, arrowWidth, height() ), Qt::AlignCenter, QStringLiteral( "›" ) );
		}
	}
private:
	bool dragging = false, moved = false, hover = false;
	int pressX = 0;
	double startValue = 0.0;
};

class CollisionManagerPanel final : public QWidget
{
public:
	CollisionManagerPanel( NifModel * model, QMainWindow * window, GLView * view, QDockWidget * owner )
		: QWidget( owner ), nif( model ), mw( window ), ogl( view ), dock( owner )
	{
		buildUi();
		connectModel();
		rebuild();
	}

private:
	NifModel * nif = nullptr;
	QMainWindow * mw = nullptr;
	GLView * ogl = nullptr;
	QDockWidget * dock = nullptr;
	QTreeWidget * tree = nullptr;
	QLabel * summary = nullptr;
	QLabel * inventoryHeader = nullptr;
	QLabel * sourceLabel = nullptr;
	QLabel * physicsHint = nullptr;
	QGroupBox * physicsGroup = nullptr;
	QWidget * physicsEditorBody = nullptr;
	QWidget * compiledSummaryWidget = nullptr;
	QWidget * emptyPhysicsWidget = nullptr;
	QLabel * compiledSummaryLabel = nullptr;
	PhysicsSimPanel * testPanel = nullptr;
	QWidget * advancedSection = nullptr;
	QWidget * advancedBody = nullptr;
	QDoubleSpinBox * mass = nullptr;
	QDoubleSpinBox * friction = nullptr;
	QDoubleSpinBox * restitution = nullptr;
	QDoubleSpinBox * linearDamping = nullptr;
	QDoubleSpinBox * angularDamping = nullptr;
	QDoubleSpinBox * maxLinearVelocity = nullptr;
	QDoubleSpinBox * maxAngularVelocity = nullptr;
	QComboBox * layer = nullptr;
	QComboBox * material = nullptr;
	QComboBox * motionSystem = nullptr;
	QComboBox * qualityType = nullptr;
	QComboBox * solverDeactivation = nullptr;
	QComboBox * deactivatorType = nullptr;
	QCheckBox * keyframed = nullptr;
	QCheckBox * linkedGroup = nullptr;
	QCheckBox * collisionWithinGroup = nullptr;
	QCheckBox * wind = nullptr;
	QCheckBox * phantom = nullptr;
	QCheckBox * shapePhantom = nullptr;
	QSpinBox * filterGroup = nullptr;
	QDoubleSpinBox * centerX = nullptr;
	QDoubleSpinBox * centerY = nullptr;
	QDoubleSpinBox * centerZ = nullptr;
	QDoubleSpinBox * inertiaX = nullptr;
	QDoubleSpinBox * inertiaY = nullptr;
	QDoubleSpinBox * inertiaZ = nullptr;
	QDoubleSpinBox * penetrationDepth = nullptr;
	QToolButton * primaryCollisionAction = nullptr;
	QAction * decompileSelectedAction = nullptr;
	QAction * compileSelectedAction = nullptr;
	QPushButton * createButton = nullptr;
	QPushButton * decimateButton = nullptr;
	QFrame * previewPanel = nullptr;
	QWidget * previewBody = nullptr;
	QComboBox * previewMethod = nullptr;
	QDoubleSpinBox * previewRatio = nullptr;
	QDoubleSpinBox * previewPrecision = nullptr;
	QDoubleSpinBox * previewThreshold = nullptr;
	QSpinBox * previewMaxHulls = nullptr;
	QLabel * previewStats = nullptr;
	QLabel * previewMethodLabel = nullptr;
	QLabel * previewPrecisionLabel = nullptr;
	QLabel * previewThresholdLabel = nullptr;
	QLabel * previewMaxHullsLabel = nullptr;
	QToolButton * previewTitle = nullptr;
	QTimer * previewTimer = nullptr;
	QPersistentModelIndex previewSource;
	int previewKind = 0; // 0 convex, 1 mesh/decimate
	bool updating = false;
	bool rebuildQueued = false;
	//! set while this dock is the one driving the selection, so the echo back
	//! through currentNifIndexChanged does not re-enter and fight the click
	bool syncingSelection = false;
	QSet<int> expandedObjects;

	QModelIndex blockIndex( int block ) const { return tlCollBlockIndex( nif, block ); }

	QString blockType( int block ) const
	{
		QModelIndex i = blockIndex( block );
		return i.isValid() ? nif->itemName( i ) : QString();
	}

	QString materialEnumType() const
	{
		if ( nif->getBSVersion() >= 170 ) return QStringLiteral( "Fallout76HavokMaterial" );
		if ( nif->getBSVersion() >= 130 ) return QStringLiteral( "Fallout4HavokMaterial" );
		if ( nif->getBSVersion() >= 83 ) return QStringLiteral( "SkyrimHavokMaterial" );
		if ( nif->getBSVersion() > 0 ) return QStringLiteral( "Fallout3HavokMaterial" );
		return QStringLiteral( "OblivionHavokMaterial" );
	}

	QVariantMap customMaterials() const
	{
		QSettings settings;
		return settings.value( "CollisionManager/CustomMaterials" ).toMap();
	}

	QString knownMaterialText( quint32 crc ) const
	{
		const auto & options = NifValue::enumOptionData( materialEnumType() ).o;
		auto it = options.constFind( crc );
		if ( it == options.cend() ) return QString();
		// Keep the CRC/internal identifier as the stored value, but present the
		// exact labels used by Bethesda's FO4 3ds Max exporter. nif.xml contains
		// several CK-style or later-added labels (ActorArmoredCrab,
		// MaterialPlasticBin...) that do not match the authoring dropdown.
		if ( materialEnumType() == QLatin1String( "Fallout4HavokMaterial" ) ) {
			switch ( crc ) {
				case 0u: return QStringLiteral( "NullMaterial" );
				case 186875565u: return QStringLiteral( "Generic" );
				case 3824112521u: return QStringLiteral( "ActorGeneric" );
				case 4288341836u: return QStringLiteral( "ActorCrabArmored" );
				case 2377431615u: return QStringLiteral( "ActorGhost" );
				case 1395526401u: return QStringLiteral( "ActorInsect" );
				case 1317396055u: return QStringLiteral( "ActorMetal" );
				case 1526052914u: return QStringLiteral( "ActorMetalLarge" );
				case 1994095519u: return QStringLiteral( "ActorMetalSmall" );
				case 2215730879u: return QStringLiteral( "ActorSkeleton" );
				case 1423343525u: return QStringLiteral( "ActorSkin" );
				case 1124183901u: return QStringLiteral( "ActorSkinLarge" );
				case 1865379056u: return QStringLiteral( "ActorSkinSmall" );
				case 748236019u: return QStringLiteral( "ArmorHeavy" );
				case 1035078235u: return QStringLiteral( "ArmorLight" );
				case 504699171u: return QStringLiteral( "Arrow" );
				case 2474963416u: return QStringLiteral( "Basket" );
				case 2605587041u: return QStringLiteral( "Bone" );
				case 3936451629u: return QStringLiteral( "Book" );
				case 493553910u: return QStringLiteral( "Bottle" );
				case 172611777u: return QStringLiteral( "BottleSmall" );
				case 4050795553u: return QStringLiteral( "Brick" );
				case 4028041609u: return QStringLiteral( "Carpet" );
				case 885278459u: return QStringLiteral( "CeramicMedium" );
				case 1998647255u: return QStringLiteral( "Chain" );
				case 3839073443u: return QStringLiteral( "Cloth" );
				case 630687820u: return QStringLiteral( "ClothCushion" );
				case 1949124673u: return QStringLiteral( "Coin" );
				case 911716378u: return QStringLiteral( "Concrete" );
				case 3106094762u: return QStringLiteral( "Dirt" );
				case 1340314491u: return QStringLiteral( "DirtStairs" );
				case 3739830338u: return QStringLiteral( "Glass" );
				case 2534329404u: return QStringLiteral( "GlassStairs" );
				case 1848600814u: return QStringLiteral( "Grass" );
				case 2926148967u: return QStringLiteral( "GrassStairs" );
				case 428587608u: return QStringLiteral( "Gravel" );
				case 873356572u: return QStringLiteral( "Ice" );
				case 668408902u: return QStringLiteral( "Insect" );
				case 2900281814u: return QStringLiteral( "Meat" );
				case 104858580u: return QStringLiteral( "Metal" );
				case 2050332274u: return QStringLiteral( "MetalBarrel" );
				case 4094939507u: return QStringLiteral( "MetalHeavy" );
				case 2993325253u: return QStringLiteral( "MetalHollow" );
				case 3845715931u: return QStringLiteral( "MetalLight" );
				case 706373264u: return QStringLiteral( "MetalSolid" );
				case 1486385281u: return QStringLiteral( "Mud" );
				case 2974920155u: return QStringLiteral( "Organic" );
				case 3028667025u: return QStringLiteral( "OtherParent" );
				case 2285410059u: return QStringLiteral( "Paper" );
				case 2113564165u: return QStringLiteral( "Plastic" );
				case 4013367152u: return QStringLiteral( "PotsPans" );
				case 2168343821u: return QStringLiteral( "Sand" );
				case 3504249533u: return QStringLiteral( "Skeleton" );
				case 591247106u: return QStringLiteral( "Skin" );
				case 2925203401u: return QStringLiteral( "ShieldHeavy" );
				case 3211798881u: return QStringLiteral( "ShieldLight" );
				case 398949039u: return QStringLiteral( "Snow" );
				case 950426180u: return QStringLiteral( "SnowStairs" );
				case 3741512247u: return QStringLiteral( "Stone" );
				case 1782067423u: return QStringLiteral( "StoneAsStairs" );
				case 2276150428u: return QStringLiteral( "StoneBoulderLarge" );
				case 3251579835u: return QStringLiteral( "StoneBoulderMedium" );
				case 2877656881u: return QStringLiteral( "StoneBoulderSmall" );
				case 1413383033u: return QStringLiteral( "StoneBroken" );
				case 811073006u: return QStringLiteral( "StoneBrokenStairs" );
				case 2253994566u: return QStringLiteral( "StoneHeavy" );
				case 2519395573u: return QStringLiteral( "StoneStairs" );
				case 1024582599u: return QStringLiteral( "Water" );
				case 2459239824u: return QStringLiteral( "WaterPuddle" );
				case 3934839107u: return QStringLiteral( "Web" );
				case 26782757u: return QStringLiteral( "WeaponAxe1Hand" );
				case 3284711549u: return QStringLiteral( "WeaponBlade1Hand" );
				case 361241712u: return QStringLiteral( "WeaponBlade1HandSmall" );
				case 694643242u: return QStringLiteral( "WeaponBlade2Hand" );
				case 4207714651u: return QStringLiteral( "WeaponBlunt1Hand" );
				case 3178027915u: return QStringLiteral( "WeaponBlunt2Hand" );
				case 238888479u: return QStringLiteral( "WeaponBowsStaves" );
				case 4146539321u: return QStringLiteral( "WeaponPistol" );
				case 1201612612u: return QStringLiteral( "WeaponRifle" );
				case 500811281u: return QStringLiteral( "Wood" );
				case 3403586657u: return QStringLiteral( "WoodBarrel" );
				case 3793849624u: return QStringLiteral( "WoodAsStairs" );
				case 3481395209u: return QStringLiteral( "WoodHeavy" );
				case 3735733921u: return QStringLiteral( "WoodLight" );
				case 1879074862u: return QStringLiteral( "WoodStairs" );
				default: break;
			}
		}
		QString text = it.value().second.trimmed();
		return text.isEmpty() ? it.value().first : text;
	}

	QString materialToolTip( quint32 crc ) const
	{
		const auto & options = NifValue::enumOptionData( materialEnumType() ).o;
		auto it = options.constFind( crc );
		QString id = it == options.cend() ? tr( "Unlisted/custom material" ) : it.value().first;
		QString hex = QStringLiteral( "%1" ).arg( crc, 8, 16, QLatin1Char( '0' ) ).toUpper();
		return tr( "%1\nCRC: 0x%2 (%3)" ).arg( id, hex ).arg( crc );
	}

	QString materialText( quint32 crc ) const
	{
		QString name = knownMaterialText( crc );
		if ( !name.isEmpty() ) return name;
		const QVariantMap custom = customMaterials();
		for ( auto it = custom.cbegin(); it != custom.cend(); ++it ) {
			if ( it.value().toUInt() == crc ) return it.key();
		}
		return materialCrcText( crc );
	}

	QString layerEnumType() const
	{
		if ( nif->getBSVersion() >= 170 ) return QStringLiteral( "Fallout76Layer" );
		if ( nif->getBSVersion() >= 130 ) return QStringLiteral( "Fallout4Layer" );
		if ( nif->getBSVersion() >= 83 ) return QStringLiteral( "SkyrimLayer" );
		if ( nif->getBSVersion() > 0 ) return QStringLiteral( "Fallout3Layer" );
		return QStringLiteral( "OblivionLayer" );
	}

	QString collisionLayerName( quint32 value ) const
	{
		QString name = collisionLayerLabel( value );
		if ( !name.isEmpty() ) return QStringLiteral( "%1 (%2)" ).arg( name ).arg( value );
		return basicCollisionLayerName( value );
	}

	QString collisionLayerLabel( quint32 value ) const
	{
		if ( layerEnumType() == QLatin1String( "Fallout4Layer" ) ) {
			static const QStringList labels = {
				QStringLiteral( "Unidentified" ), QStringLiteral( "Static" ), QStringLiteral( "Anim Static" ),
				QStringLiteral( "Transparent" ), QStringLiteral( "Clutter" ), QStringLiteral( "Weapon" ),
				QStringLiteral( "Projectile" ), QStringLiteral( "Spell" ), QStringLiteral( "Biped" ),
				QStringLiteral( "Tree" ), QStringLiteral( "Prop" ), QStringLiteral( "Water" ),
				QStringLiteral( "Trigger" ), QStringLiteral( "Terrain" ), QStringLiteral( "Trap" ),
				QStringLiteral( "NonCollidable" ), QStringLiteral( "CloudTrap" ), QStringLiteral( "Ground" ),
				QStringLiteral( "Portal" ), QStringLiteral( "Small Debris" ), QStringLiteral( "Large Debris" ),
				QStringLiteral( "Acoustic Space" ), QStringLiteral( "ActorZone" ), QStringLiteral( "ProjectileZone" ),
				QStringLiteral( "GasTrap" ), QStringLiteral( "ShellCasing" ), QStringLiteral( "Transparent Small" ),
				QStringLiteral( "Invisible Wall" ), QStringLiteral( "Transparent Small Anim" ), QStringLiteral( "Clutter Large" ),
				QStringLiteral( "Character Controller" ), QStringLiteral( "Stair Helper" ), QStringLiteral( "Dead Bip" ),
				QStringLiteral( "Biped No CC" ), QStringLiteral( "AvoidBox" ), QStringLiteral( "CollisionBox" ),
				QStringLiteral( "Camera Sphere" ), QStringLiteral( "Door Detection" ), QStringLiteral( "Cone Projectile" ),
				QStringLiteral( "Camera Pick" ), QStringLiteral( "Item Pick" ), QStringLiteral( "Line of Sight" ),
				QStringLiteral( "Path Pick" ), QStringLiteral( "Custom Pick 1" ), QStringLiteral( "FX Collider" ),
				QStringLiteral( "Spell Explosion" ), QStringLiteral( "Dropping Pick" ), QStringLiteral( "Dead Actor Zone" ),
				QStringLiteral( "Falling Trap" ), QStringLiteral( "NavMesh Cut" ), QStringLiteral( "Critter" ),
				QStringLiteral( "spellTrigger" ), QStringLiteral( "Living And Dead Actors" ), QStringLiteral( "Detection" ),
				QStringLiteral( "Trap Trigger" ), QStringLiteral( "Clutter NoNavCut" ), QStringLiteral( "CharBumper" )
			};
			if ( value < quint32( labels.size() ) ) return labels.at( int( value ) );
		}
		QString name = NifValue::enumOptionName( layerEnumType(), value );
		if ( name.contains( QLatin1Char( '_' ) ) ) name = name.section( QLatin1Char( '_' ), 1 );
		return name;
	}

	static QString titleCaseEnum( QString name )
	{
		name.replace( QLatin1Char( '_' ), QLatin1Char( ' ' ) );
		QStringList words = name.toLower().split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		for ( QString & word : words ) if ( !word.isEmpty() ) word[0] = word[0].toUpper();
		return words.join( QLatin1Char( ' ' ) );
	}

	static void configureSearchableSelector( QComboBox * combo, const QString & placeholder )
	{
		combo->setEditable( true );
		combo->setInsertPolicy( QComboBox::NoInsert );
		combo->setMaxVisibleItems( 11 );
		combo->setSizeAdjustPolicy( QComboBox::AdjustToMinimumContentsLengthWithIcon );
		combo->setMinimumContentsLength( 12 );
		combo->view()->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
		combo->lineEdit()->setClearButtonEnabled( true );
		combo->lineEdit()->setPlaceholderText( placeholder );
		if ( QCompleter * completer = combo->completer() ) {
			completer->setCaseSensitivity( Qt::CaseInsensitive );
			completer->setFilterMode( Qt::MatchContains );
			completer->setCompletionMode( QCompleter::PopupCompletion );
			completer->setMaxVisibleItems( 11 );
			// QComboBox's default completer only replaces the edit text. Explicitly
			// commit the matching source row so currentData() changes and the
			// selected physics value is actually written.
			QObject::connect( completer, qOverload<const QModelIndex &>( &QCompleter::activated ), combo,
				[combo]( const QModelIndex & index ) {
					int row = combo->findData( index.data( Qt::UserRole ) );
					if ( row < 0 )
						row = combo->findText( index.data( Qt::DisplayRole ).toString(), Qt::MatchExactly );
					if ( row >= 0 )
						combo->setCurrentIndex( row );
				} );
		}
	}

	void populatePhysicsEnums()
	{
		if ( !layer || !material || !motionSystem || !qualityType || !solverDeactivation || !deactivatorType )
			return;
		QSignalBlocker bl( layer ), bm( material ), bmo( motionSystem ), bq( qualityType ),
			bs( solverDeactivation ), bd( deactivatorType );
		layer->clear(); material->clear(); motionSystem->clear(); qualityType->clear();
		solverDeactivation->clear(); deactivatorType->clear();
		const auto & layerOptions = NifValue::enumOptionData( layerEnumType() ).o;
		QSet<quint32> addedLayers;
		// FO4's authoring list is a contiguous 0..56 table. Add it explicitly so
		// useful entries such as Stair Helper remain available even when an older
		// nif.xml enum table is incomplete.
		if ( layerEnumType() == QLatin1String( "Fallout4Layer" )
			|| layerEnumType() == QLatin1String( "Fallout76Layer" ) ) {
			for ( quint32 value = 0; value <= 56; value++ ) {
				layer->addItem( QStringLiteral( "%1 (%2)" ).arg( collisionLayerLabel( value ) ).arg( value ), value );
				addedLayers.insert( value );
			}
		}
		for ( auto it = layerOptions.cbegin(); it != layerOptions.cend(); ++it ) {
			if ( addedLayers.contains( it.key() ) ) continue;
			QString name = collisionLayerLabel( it.key() );
			layer->addItem( QStringLiteral( "%1 (%2)" ).arg( name ).arg( it.key() ), it.key() );
		}

		QMap<QString, QPair<quint32, QString>> sortedMaterials;
		const auto & materialOptions = NifValue::enumOptionData( materialEnumType() ).o;
		for ( auto it = materialOptions.cbegin(); it != materialOptions.cend(); ++it ) {
			QString name = knownMaterialText( it.key() );
			sortedMaterials.insert( name, qMakePair( it.key(), it.value().first ) );
		}
		for ( auto it = sortedMaterials.cbegin(); it != sortedMaterials.cend(); ++it ) {
			material->addItem( it.key(), it.value().first );
			int row = material->count() - 1;
			material->setItemData( row, materialToolTip( it.value().first ), Qt::ToolTipRole );
			material->setItemData( row, it.value().second, Qt::UserRole + 1 );
		}
		const QVariantMap custom = customMaterials();
		for ( auto it = custom.cbegin(); it != custom.cend(); ++it ) {
			if ( material->findData( it.value().toUInt() ) < 0 )
				material->addItem( it.key(), it.value().toUInt() );
		}

		const auto addEnum = []( QComboBox * combo, const QString & type, const QString & prefix ) {
			const auto & options = NifValue::enumOptionData( type ).o;
			for ( auto it = options.cbegin(); it != options.cend(); ++it ) {
				QString name = it.value().first;
				if ( name.startsWith( prefix ) ) name.remove( 0, prefix.size() );
				combo->addItem( QStringLiteral( "%1 (%2)" ).arg( titleCaseEnum( name ) ).arg( it.key() ), it.key() );
			}
		};
		addEnum( motionSystem, QStringLiteral( "hkMotionType" ), QStringLiteral( "MO_SYS_" ) );
		addEnum( qualityType, QStringLiteral( "hkQualityType" ), QStringLiteral( "MO_QUAL_" ) );
		addEnum( solverDeactivation, QStringLiteral( "hkSolverDeactivation" ), QStringLiteral( "SOLVER_DEACTIVATION_" ) );
		addEnum( deactivatorType, QStringLiteral( "hkDeactivatorType" ), QStringLiteral( "DEACTIVATOR_" ) );
	}

	static void selectComboValue( QComboBox * combo, quint32 value, const QString & fallback = QString() )
	{
		int row = combo->findData( value );
		if ( row < 0 ) {
			combo->addItem( fallback.isEmpty() ? QObject::tr( "Unknown (%1)" ).arg( value ) : fallback, value );
			row = combo->count() - 1;
		}
		combo->setCurrentIndex( row );
	}

	int ownerNode( const QModelIndex & object ) const
	{
		int target = nif->getLink( object, "Target" );
		if ( nif->isValidBlockNumber( target ) )
			return target;
		return nif->getParent( nif->getBlockNumber( object ) );
	}

	/*! What this collision's owner node is, as a bone.
	 *
	 * Two independent sources, because the two file kinds answer it differently.
	 * A skinned mesh has a real skin to consult, so skeletonAnalyse() says
	 * whether the node deforms anything. A skeleton.nif has no skin at all -
	 * every node would come back "not a bone" - but its collision hangs off a
	 * bhkRagdollSystem, and that block existing IS the statement that these
	 * bodies are the bone collision. Hence the fallback: no skin evidence plus a
	 * ragdoll system means ragdoll bone.
	 */
	QString boneRole( const QHash<int, QString> & boneRoles, int node, int systemBlock ) const
	{
		const QString fromSkin = boneRoles.value( node );
		if ( !fromSkin.isEmpty() )
			return fromSkin;
		if ( nif->isNiBlock( blockIndex( systemBlock ), "bhkRagdollSystem" ) )
			return tr( "ragdoll bone" );
		return QString();
	}

	/*! node block -> bone role, for every node some skin actually names.
	 *
	 * Built once per rebuild() and passed down: calling skeletonAnalyse() per row
	 * would re-walk every vertex weight in the file once for each collision body.
	 */
	QHash<int, QString> collectBoneRoles() const
	{
		QHash<int, QString> roles;
		const SkeletonReport report = skeletonAnalyse( nif );
		for ( const SkeletonBoneInfo & b : report.bones ) {
			if ( b.isNotABone() )
				continue;
			roles.insert( b.block, b.verts > 0
				? tr( "deforming (%1 v)" ).arg( b.verts )
				: tr( "unused bone" ) );
		}
		return roles;
	}

	QString nodeName( int block ) const
	{
		QModelIndex i = blockIndex( block );
		if ( !i.isValid() )
			return tr( "<orphan>" );
		QString n = nif->get<QString>( i, "Name" );
		return n.isEmpty() ? tr( "Block %1" ).arg( block ) : n;
	}

	QString compiledShapeSummary( const HknpSystem & sys, quint32 bodyId,
		quint32 * material, int * verts, int * tris ) const
	{
		int count = 0;
		QString one;
		for ( const HknpShape & s : sys.shapes ) {
			if ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId )
				continue;
			count++;
			*verts += s.verts.size();
			*tris += s.tris.size();
			if ( !*material && s.materialCRC )
				*material = s.materialCRC;
			one = compiledShapeText( s );
		}
		if ( count > 1 )
			return tr( "Compound (%1)" ).arg( count );
		return count == 1 ? one : tr( "No decompiled shape" );
	}

	QString compiledShapeText( const HknpShape & shape ) const
	{
		if ( shape.primType == 1 )
			return tr( "Sphere" );
		if ( shape.primType == 2 )
			return tr( "Capsule" );
		if ( shape.isConvex )
			return tr( "Hull (%1 v)" ).arg( shape.verts.size() );
		return tr( "Mesh (%1 tri)" ).arg( shape.tris.size() );
	}

	/*! Fill the Bone cell.
	 *
	 * Column 6 is the LOGICAL index; the header moves it next to Node visually
	 * (see the tree setup). Appending rather than inserting keeps every other
	 * column index in this file unchanged - there are 42 literal ones.
	 */
	void setBoneCell( QTreeWidgetItem * item, const QString & role ) const
	{
		if ( role.isEmpty() )
			return;
		item->setText( 6, role );
		item->setForeground( 6, QColor( wwSkinColor( "accentText" ) ) );
	}

	void setItemRoles( QTreeWidgetItem * item, int objectBlock, int nodeBlock,
		int systemBlock, int bodyBlock, int shapeBlock, quint32 bodyId,
		bool compiled, int shapeIndex = -1 ) const
	{
		item->setData( 0, ObjectBlockRole, objectBlock );
		item->setData( 0, NodeBlockRole, nodeBlock );
		item->setData( 0, SystemBlockRole, systemBlock );
		item->setData( 0, BodyBlockRole, bodyBlock );
		item->setData( 0, ShapeBlockRole, shapeBlock );
		item->setData( 0, BodyIdRole, bodyId );
		item->setData( 0, CompiledRole, compiled );
		item->setData( 0, ShapeIndexRole, shapeIndex );
	}

	int firstLeafShape( int shape, int depth = 0 ) const
	{
		return tlCollFirstLeafShape( nif, shape, depth );
	}

	struct CollisionMesh
	{
		QVector<Vector3> verts;
		QVector<Triangle> tris;
	};

	static void appendMesh( CollisionMesh & out, const QVector<Vector3> & verts,
		const QVector<Triangle> & tris, const Matrix4 & transform, float scale )
	{
		if ( verts.isEmpty() || tris.isEmpty() || out.verts.size() + verts.size() > 65535 ) return;
		quint16 base = quint16( out.verts.size() );
		for ( const Vector3 & v : verts ) out.verts.append( transform * v * scale );
		for ( Triangle t : tris ) {
			if ( t[0] >= verts.size() || t[1] >= verts.size() || t[2] >= verts.size() ) continue;
			t[0] += base; t[1] += base; t[2] += base; out.tris.append( t );
		}
	}

	static QVector<Triangle> boxTriangles()
	{
		return {
			{ 0, 2, 1 }, { 1, 2, 3 }, { 4, 5, 6 }, { 5, 7, 6 },
			{ 0, 1, 4 }, { 1, 5, 4 }, { 2, 6, 3 }, { 3, 6, 7 },
			{ 0, 4, 2 }, { 2, 4, 6 }, { 1, 3, 5 }, { 3, 7, 5 }
		};
	}

	static void appendSphereMesh( CollisionMesh & out, const Vector3 & center, float radius,
		const Matrix4 & transform, float scale )
	{
		constexpr int slices = 16, stacks = 8;
		constexpr float pi = 3.14159265358979323846f;
		QVector<Vector3> verts;
		QVector<Triangle> tris;
		for ( int y = 0; y <= stacks; y++ ) {
			float phi = -0.5f * pi + pi * float( y ) / float( stacks );
			float cp = std::cos( phi ), sp = std::sin( phi );
			for ( int x = 0; x < slices; x++ ) {
				float a = 2.0f * pi * float( x ) / float( slices );
				verts.append( center + Vector3( std::cos( a ) * cp, std::sin( a ) * cp, sp ) * radius );
			}
		}
		for ( int y = 0; y < stacks; y++ ) for ( int x = 0; x < slices; x++ ) {
			quint16 a = quint16( y * slices + x ), b = quint16( y * slices + ( x + 1 ) % slices );
			quint16 c = quint16( ( y + 1 ) * slices + x ), d = quint16( ( y + 1 ) * slices + ( x + 1 ) % slices );
			tris.append( Triangle( a, b, c ) ); tris.append( Triangle( b, d, c ) );
		}
		appendMesh( out, verts, tris, transform, scale );
	}

	static void appendCapsuleMesh( CollisionMesh & out, Vector3 a, Vector3 b, float radius,
		const Matrix4 & transform, float scale )
	{
		constexpr int slices = 16, hemi = 4;
		constexpr float pi = 3.14159265358979323846f;
		Vector3 axis = b - a;
		if ( axis.length() < 1.0e-6f ) { appendSphereMesh( out, a, radius, transform, scale ); return; }
		axis.normalize();
		Vector3 u = Vector3::crossproduct( axis, std::fabs( axis[2] ) < 0.9f ? Vector3( 0, 0, 1 ) : Vector3( 0, 1, 0 ) );
		u.normalize(); Vector3 v = Vector3::crossproduct( axis, u ); v.normalize();
		QVector<QPair<Vector3, float>> rings;
		for ( int i = 0; i <= hemi; i++ ) {
			float t = -0.5f * pi + 0.5f * pi * float( i ) / float( hemi );
			rings.append( { a + axis * ( std::sin( t ) * radius ), std::cos( t ) * radius } );
		}
		rings.append( { b, radius } );
		for ( int i = 1; i <= hemi; i++ ) {
			float t = 0.5f * pi * float( i ) / float( hemi );
			rings.append( { b + axis * ( std::sin( t ) * radius ), std::cos( t ) * radius } );
		}
		QVector<Vector3> verts; QVector<Triangle> tris;
		for ( const auto & ring : rings ) for ( int s = 0; s < slices; s++ ) {
			float ang = 2.0f * pi * float( s ) / float( slices );
			verts.append( ring.first + ( u * std::cos( ang ) + v * std::sin( ang ) ) * ring.second );
		}
		for ( int r = 0; r + 1 < rings.size(); r++ ) for ( int s = 0; s < slices; s++ ) {
			quint16 p0 = quint16( r * slices + s ), p1 = quint16( r * slices + ( s + 1 ) % slices );
			quint16 p2 = quint16( ( r + 1 ) * slices + s ), p3 = quint16( ( r + 1 ) * slices + ( s + 1 ) % slices );
			tris.append( Triangle( p0, p1, p2 ) ); tris.append( Triangle( p1, p3, p2 ) );
		}
		appendMesh( out, verts, tris, transform, scale );
	}

	void appendEditableMesh( int shapeBlock, CollisionMesh & out, const Matrix4 & transform = Matrix4(), int depth = 0 ) const
	{
		if ( !nif->isValidBlockNumber( shapeBlock ) || depth > 20 ) return;
		QModelIndex shape = blockIndex( shapeBlock );
		QString type = nif->itemName( shape );
		if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
			for ( qint32 child : nif->getLinkArray( shape, "Sub Shapes" ) ) appendEditableMesh( child, out, transform, depth + 1 );
			return;
		}
		if ( type == QLatin1String( "bhkMoppBvTreeShape" ) ) {
			appendEditableMesh( nif->getLink( shape, "Shape" ), out, transform, depth + 1 ); return;
		}
		if ( type == QLatin1String( "bhkTransformShape" ) || type == QLatin1String( "bhkConvexTransformShape" ) ) {
			Matrix4 next( transform ); next.multiply4x3( nif->get<Matrix4>( shape, "Transform" ) );
			appendEditableMesh( nif->getLink( shape, "Shape" ), out, next, depth + 1 ); return;
		}
		if ( type == QLatin1String( "bhkBoxShape" ) ) {
			Vector3 d = nif->get<Vector3>( shape, "Dimensions" ); QVector<Vector3> v;
			for ( int z = -1; z <= 1; z += 2 ) for ( int y = -1; y <= 1; y += 2 ) for ( int x = -1; x <= 1; x += 2 )
				v.append( Vector3( d[0] * x, d[1] * y, d[2] * z ) );
			appendMesh( out, v, boxTriangles(), transform, 69.99125f ); return;
		}
		if ( type == QLatin1String( "bhkSphereShape" ) ) {
			appendSphereMesh( out, Vector3(), nif->get<float>( shape, "Radius" ), transform, 69.99125f ); return;
		}
		if ( type == QLatin1String( "bhkCapsuleShape" ) ) {
			appendCapsuleMesh( out, nif->get<Vector3>( shape, "First Point" ), nif->get<Vector3>( shape, "Second Point" ),
				nif->get<float>( shape, "Radius" ), transform, 69.99125f ); return;
		}
		if ( type == QLatin1String( "bhkConvexVerticesShape" ) ) {
			QVector<Vector4> vv = nif->getArray<Vector4>( shape, "Vertices" ); QVector<Vector3> v;
			for ( const Vector4 & p : vv ) v.append( Vector3( p[0], p[1], p[2] ) );
			QVector<Vector4> hullVerts, hullNorms; QVector<Triangle> tris = compute_convex_hull( v, hullVerts, hullNorms );
			v.clear(); for ( const Vector4 & p : hullVerts ) v.append( Vector3( p[0], p[1], p[2] ) );
			appendMesh( out, v, tris, transform, 69.99125f ); return;
		}
		if ( type == QLatin1String( "bhkNiTriStripsShape" ) ) {
			for ( qint32 dataBlock : nif->getLinkArray( shape, "Strips Data" ) ) {
				QModelIndex data = blockIndex( dataBlock ); QVector<Vector3> v = nif->getArray<Vector3>( data, "Vertices" );
				QVector<QVector<quint16>> strips; QModelIndex points = nif->getIndex( data, "Points" );
				for ( int r = 0; r < nif->rowCount( points ); r++ ) strips.append( nif->getArray<quint16>( nif->getIndex( points, r ) ) );
				appendMesh( out, v, triangulate( strips ), transform, 1.0f );
			}
		}
	}

	CollisionMesh selectedCollisionMesh() const
	{
		CollisionMesh out; QTreeWidgetItem * item = tree->currentItem(); if ( !item ) return out;
		if ( item->data( 0, CompiledRole ).toBool() ) {
			QModelIndex sysBlock = blockIndex( item->data( 0, SystemBlockRole ).toInt() );
			HknpSystem sys = hknpDecode( nif->get<QByteArray>( sysBlock, "Binary Data" ) );
			int selectedShape = item->data( 0, ShapeIndexRole ).toInt(); quint32 bodyId = item->data( 0, BodyIdRole ).toUInt();
			for ( int i = 0; i < sys.shapes.size(); i++ ) {
				const HknpShape & s = sys.shapes.at( i );
				if ( selectedShape >= 0 ? i != selectedShape : ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId ) ) continue;
				QVector<Vector3> v; for ( const Vector3 & p : s.verts ) v.append( s.transformed( p ) );
				appendMesh( out, v, s.tris, Matrix4(), 69.99125f );
			}
		} else {
			appendEditableMesh( item->data( 0, ShapeBlockRole ).toInt(), out );
		}
		return out;
	}

	static double meshVolumeGameUnits( const CollisionMesh & mesh )
	{
		if ( mesh.verts.size() < 4 || mesh.tris.size() < 4 )
			return 0.0;
		Vector3 centre;
		for ( const Vector3 & v : mesh.verts )
			centre += v;
		centre /= float( mesh.verts.size() );
		double volume = 0.0;
		for ( const Triangle & t : mesh.tris ) {
			if ( t[0] >= mesh.verts.size() || t[1] >= mesh.verts.size() || t[2] >= mesh.verts.size() ) continue;
			Vector3 a = mesh.verts[t[0]] - centre;
			Vector3 b = mesh.verts[t[1]] - centre;
			Vector3 c = mesh.verts[t[2]] - centre;
			volume += std::fabs( double( Vector3::dotproduct( a, Vector3::crossproduct( b, c ) ) ) ) / 6.0;
		}
		return volume;
	}

	QString editableShapeSummary( int rootShape, quint32 * material, int * verts, int * tris ) const
	{
		if ( !nif->isValidBlockNumber( rootShape ) )
			return tr( "Missing shape" );
		QModelIndex root = blockIndex( rootShape );
		QString rt = nif->itemName( root );
		if ( rt == QLatin1String( "bhkListShape" ) || rt == QLatin1String( "bhkConvexListShape" ) ) {
			QVector<qint32> links = nif->getLinkArray( root, "Sub Shapes" );
			if ( !links.isEmpty() ) {
				int leaf = firstLeafShape( links.first() );
				if ( leaf >= 0 )
					*material = nif->get<quint32>( blockIndex( leaf ), "Material" );
			}
			return tr( "Compound (%1)" ).arg( links.size() );
		}

		int leaf = firstLeafShape( rootShape );
		QModelIndex i = blockIndex( leaf );
		QString t = i.isValid() ? nif->itemName( i ) : rt;
		if ( i.isValid() )
			*material = nif->get<quint32>( i, "Material" );
		if ( t == QLatin1String( "bhkBoxShape" ) ) return tr( "Box" );
		if ( t == QLatin1String( "bhkSphereShape" ) ) return tr( "Sphere" );
		if ( t == QLatin1String( "bhkCapsuleShape" ) ) return tr( "Capsule" );
		if ( t == QLatin1String( "bhkConvexVerticesShape" ) ) {
			*verts = nif->get<uint>( i, "Num Vertices" );
			return tr( "Hull (%1 v)" ).arg( *verts );
		}
		if ( t == QLatin1String( "bhkNiTriStripsShape" ) ) {
			for ( qint32 d : nif->getLinkArray( i, "Strips Data" ) ) {
				QModelIndex di = blockIndex( d );
				*verts += nif->get<int>( di, "Num Vertices" );
				*tris += nif->get<int>( di, "Num Triangles" );
			}
			return tr( "Mesh (%1 tri)" ).arg( *tris );
		}
		return t.isEmpty() ? rt : t;
	}

	void appendEditableShapeItems( QTreeWidgetItem * parent, int shapeBlock,
		int objectBlock, int nodeBlock, int bodyBlock, quint32 collLayer,
		int & totalVerts, int & totalTris, int depth = 0 )
	{
		if ( !nif->isValidBlockNumber( shapeBlock ) || depth > 16 )
			return;
		QModelIndex shape = blockIndex( shapeBlock );
		QString type = nif->itemName( shape );
		if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
			for ( qint32 child : nif->getLinkArray( shape, "Sub Shapes" ) )
				appendEditableShapeItems( parent, child, objectBlock, nodeBlock, bodyBlock,
					collLayer, totalVerts, totalTris, depth + 1 );
			return;
		}
		if ( type == QLatin1String( "bhkMoppBvTreeShape" )
			 || type == QLatin1String( "bhkTransformShape" )
			 || type == QLatin1String( "bhkConvexTransformShape" ) ) {
			appendEditableShapeItems( parent, nif->getLink( shape, "Shape" ), objectBlock,
				nodeBlock, bodyBlock, collLayer, totalVerts, totalTris, depth + 1 );
			return;
		}

		quint32 material = 0;
		int verts = 0, tris = 0;
		QString label = editableShapeSummary( shapeBlock, &material, &verts, &tris );
		auto * child = new CollisionTreeItem( parent );
		child->setText( 0, tr( "Shape %1" ).arg( parent->childCount() ) );
		child->setText( 1, label );
		child->setText( 2, collisionLayerName( collLayer ) );
		child->setText( 3, materialText( material ) );
		child->setToolTip( 3, materialToolTip( material ) );
		child->setText( 5, tr( "EDITABLE" ) );
		child->setForeground( 5, QColor( 90, 169, 230 ) );
		setItemRoles( child, objectBlock, nodeBlock, -1, bodyBlock, shapeBlock, 0, false );
		totalVerts += verts;
		totalTris += tris;
	}

	void addCompiled( int objectBlock, const QHash<int, QString> & boneRoles,
		int & totalVerts, int & totalTris, int & totalBytes )
	{
		QModelIndex object = blockIndex( objectBlock );
		int systemBlock = nif->getLink( object, "Data" );
		QModelIndex system = blockIndex( systemBlock );
		quint32 bodyId = nif->get<quint32>( object, "Body ID" );
		int node = ownerNode( object );
		QByteArray bytes = system.isValid() ? nif->get<QByteArray>( system, "Binary Data" ) : QByteArray();
		// A skeleton points every bone's collision object at ONE ragdoll system,
		// so this ran the full packfile decode once per bone (41x on a brahmin).
		// The cache is keyed on the data, so shared systems decode once.
		const HknpSystem & sys = hknpDecodeCached( bytes );
		quint32 mat = 0, collLayer = 1;
		int nv = 0, nt = 0;
		QString shape = sys.valid ? compiledShapeSummary( sys, bodyId, &mat, &nv, &nt ) : tr( "Invalid packfile" );
		float m = 0.0f;
		if ( int( bodyId ) < sys.bodyPhys.size() ) {
			collLayer = sys.bodyPhys.at( int( bodyId ) ).layer;
			// this body's own mass, not the system's first body's
			m = sys.dynamic ? sys.bodyPhys.at( int( bodyId ) ).mass : 0.0f;
		} else if ( sys.dynamic ) {
			m = sys.mass;
		}

		auto * item = new CollisionTreeItem( tree );
		item->setText( 0, nodeName( node ) );
		item->setText( 1, shape );
		item->setText( 2, collisionLayerName( collLayer ) );
		item->setText( 3, materialText( mat ) );
		item->setToolTip( 3, materialToolTip( mat ) );
		item->setText( 4, tr( "%1 kg" ).arg( m, 0, 'f', 1 ) );
		item->setText( 5, nif->isNiBlock( system, "bhkRagdollSystem" )
			? tr( "RAGDOLL" ) : tr( "COMPILED" ) );
		item->setForeground( 5, QColor( 226, 165, 61 ) );
		setBoneCell( item, boneRole( boneRoles, node, systemBlock ) );
		setItemRoles( item, objectBlock, node, systemBlock, -1, -1, bodyId, true );
		item->setToolTip( 1, sys.valid ? QString() : sys.error );
		if ( nt > 500 || bytes.size() > 128 * 1024 ) {
			item->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
			item->setForeground( 1, QColor( 232, 169, 63 ) );
			item->setToolTip( 1, tr( "%1\nBudget warning: %2 triangles, %3 packfile bytes. Consider decimating this collision." )
				.arg( item->toolTip( 1 ) ).arg( nt ).arg( bytes.size() ).trimmed() );
		}
		int shapeNumber = 0;
		for ( int shapeIndex = 0; shapeIndex < sys.shapes.size(); shapeIndex++ ) {
			const HknpShape & s = sys.shapes.at( shapeIndex );
			if ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId )
				continue;
			auto * child = new CollisionTreeItem( item );
			child->setText( 0, tr( "Shape %1" ).arg( ++shapeNumber ) );
			child->setText( 1, compiledShapeText( s ) );
			child->setText( 2, collisionLayerName( collLayer ) );
			child->setText( 3, materialText( s.materialCRC ) );
			child->setToolTip( 3, materialToolTip( s.materialCRC ) );
			child->setText( 5, tr( "COMPILED" ) );
			child->setForeground( 5, QColor( 226, 165, 61 ) );
			if ( s.tris.size() > 500 ) {
				child->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
				child->setForeground( 1, QColor( 232, 169, 63 ) );
				child->setToolTip( 1, tr( "%1 triangles; consider decimating this collision shape." ).arg( s.tris.size() ) );
			}
			setItemRoles( child, objectBlock, node, systemBlock, -1, -1,
				bodyId, true, shapeIndex );
		}
		item->setExpanded( expandedObjects.contains( objectBlock ) );
		totalVerts += nv;
		totalTris += nt;
		totalBytes += bytes.size();
	}

	void addEditable( int objectBlock, const QHash<int, QString> & boneRoles,
		int & totalVerts, int & totalTris )
	{
		QModelIndex object = blockIndex( objectBlock );
		int bodyBlock = nif->getLink( object, "Body" );
		QModelIndex body = blockIndex( bodyBlock );
		int shapeBlock = body.isValid() ? nif->getLink( body, "Shape" ) : -1;
		int node = ownerNode( object );
		QModelIndex info = body.isValid() ? nif->getIndex( body, "Rigid Body Info" ) : QModelIndex();
		QModelIndex filter = info.isValid() ? nif->getIndex( info, "Havok Filter" ) : QModelIndex();
		quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 1;
		float m = info.isValid() ? nif->get<float>( info, "Mass" ) : 0.0f;
		if ( collLayer == 0 ) collLayer = ( info.isValid() && nif->get<quint32>( info, "Motion System" ) == 3 ) ? 10u : 1u;
		quint32 mat = 0;
		int nv = 0, nt = 0;
		QString shape = editableShapeSummary( shapeBlock, &mat, &nv, &nt );

		auto * item = new CollisionTreeItem( tree );
		item->setText( 0, nodeName( node ) );
		item->setText( 1, shape );
		item->setText( 2, collisionLayerName( collLayer ) );
		item->setText( 3, materialText( mat ) );
		item->setToolTip( 3, materialToolTip( mat ) );
		item->setText( 4, tr( "%1 kg" ).arg( m, 0, 'f', 1 ) );
		item->setText( 5, tr( "EDITABLE" ) );
		item->setForeground( 5, QColor( 90, 169, 230 ) );
		setBoneCell( item, boneRole( boneRoles, node, -1 ) );
		if ( nt > 500 ) {
			item->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
			item->setForeground( 1, QColor( 232, 169, 63 ) );
			item->setToolTip( 1, tr( "%1 triangles; consider using Optimize Source Mesh." ).arg( nt ) );
		}
		setItemRoles( item, objectBlock, node, -1, bodyBlock, shapeBlock, 0, false );
		appendEditableShapeItems( item, shapeBlock, objectBlock, node, bodyBlock,
			collLayer, totalVerts, totalTris );
		item->setExpanded( expandedObjects.contains( objectBlock ) );
	}

	void rebuild()
	{
		if ( updating || !nif )
			return;
		updating = true;
		int selectedBlock = tree->currentItem() ? tree->currentItem()->data( 0, ObjectBlockRole ).toInt() : -1;
		tree->clear();
		int compiled = 0, editable = 0, totalVerts = 0, totalTris = 0, totalBytes = 0;
		QSet<int> systems;
		const QHash<int, QString> boneRoles = collectBoneRoles();
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				systems.insert( nif->getLink( i, "Data" ) );
				addCompiled( b, boneRoles, totalVerts, totalTris, totalBytes );
				compiled++;
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				addEditable( b, boneRoles, totalVerts, totalTris );
				editable++;
			}
		}
		inventoryHeader->setText( tr( "Collision in file  -  %1 system(s), %2 body/bodies" )
			.arg( systems.size() + editable ).arg( compiled + editable ) );
		for ( int r = 0; r < tree->topLevelItemCount(); r++ ) {
			QTreeWidgetItem * item = tree->topLevelItem( r );
			if ( item->data( 0, ObjectBlockRole ).toInt() == selectedBlock ) {
				tree->setCurrentItem( item );
				break;
			}
		}
		QString budget = tr( "%1 editable | %2 compiled | %3 verts | %4 tris | %5 packfile bytes" )
			.arg( editable ).arg( compiled ).arg( totalVerts ).arg( totalTris ).arg( totalBytes );
		bool overBudget = totalTris > 2000 || totalBytes > 256 * 1024;
		if ( overBudget ) budget += tr( " | Budget warning - decimation recommended" );
		summary->setText( budget );
		summary->setStyleSheet( overBudget
			? QStringLiteral( "QLabel { color:%1; padding:3px; }" ).arg( wwSkinColor( "accentText" ) )
			: QStringLiteral( "QLabel { color: %1; padding: 3px; }" ).arg( wwSkinColor( "textMuted" ) ) );
		updating = false;
		refreshSimPins();
		updateDetails();
	}

	/*! Put each body's PIN on its own row in the tree.
	 *
	 * The simulator carried a second list of the same bodies purely to hang a
	 * checkbox off, one panel below a tree that already showed every one of them
	 * with its bone, shape, layer, material, mass and state. The only thing that
	 * list added was the checkbox, so the checkbox came here instead.
	 *
	 * Only rows belonging to the system actually being simulated get one: the
	 * others have no body to pin, and a checkbox that does nothing is worse than
	 * no checkbox.
	 */
	void refreshSimPins()
	{
		if ( !ogl )
			return;
		PhysicsPreview & pv = ogl->physicsSim();
		const bool live = pv.active();
		const int sysBlock = pv.systemBlock();
		// itemChanged fires on every setCheckState below, so the handler has to
		// be told this is us and not the user
		const bool wasUpdating = updating;
		updating = true;
		for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * item = tree->topLevelItem( i );
			bool ok = false;
			const int body = item->data( 0, BodyIdRole ).toInt( &ok );
			const bool mine = live && ok && item->data( 0, SystemBlockRole ).toInt() == sysBlock
				&& body >= 0 && body < pv.bodyCount();
			if ( !mine ) {
				item->setData( 0, Qt::CheckStateRole, QVariant() );
				continue;
			}
			item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
			item->setCheckState( 0, pv.sim().bodies().at( body ).pinned
				? Qt::Checked : Qt::Unchecked );
			item->setToolTip( 0, tr( "Tick to pin this body where it is. The same pin the "
									 "right mouse button sets in the viewport." ) );
		}
		updating = wasUpdating;
	}

	/*! Highlight the row that speaks for this block, if any.
	 *
	 * Matches on the collision object, its shape, and its OWNER NODE - the last
	 * is the one that makes bone -> collision work, since selecting a bone
	 * selects a node block that no collision role otherwise mentions. A shape
	 * child wins over its parent so that clicking a decoded shape block lands on
	 * the shape row rather than the body row.
	 */
	void selectRowForBlock( int block )
	{
		if ( block < 0 )
			return;
		QTreeWidgetItem * match = nullptr;
		for ( int r = 0; r < tree->topLevelItemCount() && !match; r++ ) {
			QTreeWidgetItem * top = tree->topLevelItem( r );
			for ( int c = 0; c < top->childCount(); c++ ) {
				QTreeWidgetItem * child = top->child( c );
				if ( child->data( 0, ShapeBlockRole ).toInt() == block ) { match = child; break; }
			}
			if ( match )
				break;
			if ( top->data( 0, ObjectBlockRole ).toInt() == block
				|| top->data( 0, NodeBlockRole ).toInt() == block
				|| top->data( 0, BodyBlockRole ).toInt() == block
				|| top->data( 0, ShapeBlockRole ).toInt() == block )
				match = top;
		}
		if ( !match || match == tree->currentItem() )
			return;
		syncingSelection = true;
		tree->setCurrentItem( match );
		tree->scrollToItem( match );
		syncingSelection = false;
	}

	void queueRebuild()
	{
		if ( rebuildQueued )
			return;
		rebuildQueued = true;
		QTimer::singleShot( 0, this, [this]() {
			rebuildQueued = false;
			if ( dock->isVisible() )
				rebuild();
		} );
	}

	void updateDetails()
	{
		QTreeWidgetItem * item = tree->currentItem();
		bool have = item != nullptr;
		bool compiled = have && item->data( 0, CompiledRole ).toBool();
		decompileSelectedAction->setEnabled( compiled );
		compileSelectedAction->setEnabled( have && !compiled );
		primaryCollisionAction->setDefaultAction( compiled ? decompileSelectedAction : compileSelectedAction );
		primaryCollisionAction->setEnabled( have );
		physicsEditorBody->setVisible( have && !compiled );
		compiledSummaryWidget->setVisible( have && compiled );
		emptyPhysicsWidget->setVisible( !have );
		advancedSection->setVisible( have && !compiled );
		mass->setEnabled( have && !compiled );
		friction->setEnabled( have && !compiled );
		restitution->setEnabled( have && !compiled );
		layer->setEnabled( have && !compiled );
		material->setEnabled( have && !compiled );
		const QList<QWidget *> physicsEditors = { linearDamping, angularDamping,
			maxLinearVelocity, maxAngularVelocity, motionSystem, qualityType, solverDeactivation,
			deactivatorType, keyframed, linkedGroup, collisionWithinGroup, wind, filterGroup,
			centerX, centerY, centerZ, inertiaX, inertiaY, inertiaZ, penetrationDepth };
		for ( QWidget * editor : physicsEditors )
			editor->setEnabled( have && !compiled );
		if ( !have ) {
			physicsGroup->setTitle( tr( "Selected collision properties" ) );
			physicsHint->setText( tr( "Select a collision row to inspect or edit it." ) );
			return;
		}

		physicsGroup->setTitle( tr( "Selected collision properties - %1" ).arg( item->text( 0 ) ) );
		QSignalBlocker bm( mass ), bf( friction ), br( restitution ), bl( layer );
		QSignalBlocker bld( linearDamping ), bad( angularDamping ), bml( maxLinearVelocity ),
			bma( maxAngularVelocity ), bmo( motionSystem ), bq( qualityType ), bs( solverDeactivation ),
			bmat( material ), bdt( deactivatorType ), bk( keyframed ), blg( linkedGroup ),
			bcg( collisionWithinGroup ), bw( wind ), bfg( filterGroup ), bcx( centerX ), bcy( centerY ),
			bcz( centerZ ), bix( inertiaX ), biy( inertiaY ), biz( inertiaZ ), bpd( penetrationDepth );
		if ( compiled ) {
			int systemBlock = item->data( 0, SystemBlockRole ).toInt();
			quint32 bodyId = item->data( 0, BodyIdRole ).toUInt();
			QModelIndex system = blockIndex( systemBlock );
			// name the QByteArray: the cached decode keys on the data, and passing a
			// temporary trips -Wdangling-reference even though the returned
			// reference points into the cache rather than into the argument
			const QByteArray systemBytes = nif->get<QByteArray>( system, "Binary Data" );
			const HknpSystem & sys = hknpDecodeCached( systemBytes );
			HknpBodyPhys phys = int( bodyId ) < sys.bodyPhys.size() ? sys.bodyPhys.at( int( bodyId ) ) : HknpBodyPhys();
			// per-body: on a ragdoll every bone has its own mass and damping
			mass->setValue( phys.mass );
			friction->setValue( phys.friction );
			restitution->setValue( phys.restitution );
			selectComboValue( layer, phys.layer );
			linearDamping->setValue( phys.linDamping );
			angularDamping->setValue( phys.angDamping );
			maxLinearVelocity->setValue( phys.maxLinVelocity );
			maxAngularVelocity->setValue( phys.maxAngVelocity );
			selectComboValue( motionSystem, phys.hasMotion ? 3u : 5u );
			selectComboValue( qualityType, phys.hasMotion ? 4u : 0u );
			selectComboValue( solverDeactivation, phys.hasMotion ? 2u : 1u );
			selectComboValue( deactivatorType, 1u );
			keyframed->setChecked( false ); linkedGroup->setChecked( phys.filterFlags & 0x80u );
			collisionWithinGroup->setChecked( !( phys.filterFlags & 0x40u ) ); wind->setChecked( false );
			filterGroup->setValue( phys.filterGroup );
			centerX->setValue( phys.position[0] ); centerY->setValue( phys.position[1] ); centerZ->setValue( phys.position[2] );
			// the packfile holds inverse inertia; this field shows the real tensor
			auto trueI = []( float v ) { return ( v > 1.0e-12f ) ? 1.0f / v : 0.0f; };
			inertiaX->setValue( trueI( phys.invInertia[0] ) );
			inertiaY->setValue( trueI( phys.invInertia[1] ) );
			inertiaZ->setValue( trueI( phys.invInertia[2] ) );
			penetrationDepth->setValue( 0.15 );
			quint32 materialValue = 0;
			int shapeIndex = item->data( 0, ShapeIndexRole ).toInt();
			if ( shapeIndex >= 0 && shapeIndex < sys.shapes.size() ) {
				materialValue = sys.shapes.at( shapeIndex ).materialCRC;
			} else {
				for ( const HknpShape & shape : sys.shapes ) {
					if ( shape.bodyId < 0 || quint32( shape.bodyId ) == bodyId ) { materialValue = shape.materialCRC; break; }
				}
			}
			selectComboValue( material, materialValue, materialText( materialValue ) );
			compiledSummaryLabel->setText( tr( "<b>%1 - %2</b><br>%3 | %4 | %5 | friction %6 | restitution %7" )
				.arg( item->text( 0 ), item->text( 1 ), item->text( 2 ), materialText( materialValue ), item->text( 4 ) )
				.arg( phys.friction, 0, 'f', 3 ).arg( phys.restitution, 0, 'f', 3 ) );
		} else {
			QModelIndex body = blockIndex( item->data( 0, BodyBlockRole ).toInt() );
			QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
			QModelIndex filter = nif->getIndex( info, "Havok Filter" );
			mass->setValue( nif->get<float>( info, "Mass" ) );
			friction->setValue( nif->get<float>( info, "Friction" ) );
			restitution->setValue( nif->get<float>( info, "Restitution" ) );
			quint32 editableLayer = nif->get<quint32>( filter, "Layer" );
			if ( editableLayer == 0 ) editableLayer = nif->get<quint32>( info, "Motion System" ) == 3 ? 10u : 1u;
			selectComboValue( layer, editableLayer );
			linearDamping->setValue( nif->get<float>( info, "Linear Damping" ) );
			angularDamping->setValue( nif->get<float>( info, "Angular Damping" ) );
			maxLinearVelocity->setValue( nif->get<float>( info, "Max Linear Velocity" ) );
			maxAngularVelocity->setValue( nif->get<float>( info, "Max Angular Velocity" ) );
			selectComboValue( motionSystem, nif->get<quint32>( info, "Motion System" ) );
			selectComboValue( qualityType, nif->get<quint32>( info, "Quality Type" ) );
			selectComboValue( solverDeactivation, nif->get<quint32>( info, "Solver Deactivation" ) );
			selectComboValue( deactivatorType, nif->get<quint32>( info, "Deactivator Type" ) );
			quint32 filterFlags = nif->get<quint32>( filter, "Flags" );
			linkedGroup->setChecked( filterFlags & 0x80u );
			collisionWithinGroup->setChecked( !( filterFlags & 0x40u ) );
			filterGroup->setValue( nif->get<quint32>( filter, "Group" ) );
			keyframed->setChecked( nif->get<quint32>( info, "Motion System" ) == 6u );
			wind->setChecked( nif->get<quint32>( body, "Body Flags" ) & 1u );
			Vector4 center = nif->get<Vector4>( info, "Center" );
			centerX->setValue( center[0] ); centerY->setValue( center[1] ); centerZ->setValue( center[2] );
			QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
			inertiaX->setValue( nif->get<float>( inertia, "m11" ) ); inertiaY->setValue( nif->get<float>( inertia, "m22" ) ); inertiaZ->setValue( nif->get<float>( inertia, "m33" ) );
			penetrationDepth->setValue( nif->get<float>( info, "Penetration Depth" ) );
			int shapeBlock = firstLeafShape( item->data( 0, ShapeBlockRole ).toInt() );
			QModelIndex shape = blockIndex( shapeBlock );
			selectComboValue( material, shape.isValid() ? nif->get<quint32>( shape, "Material" ) : 0 );
			physicsHint->setText( tr( "Values live-edit the selected bhkRigidBody." ) );
		}
	}

	void applyLayerSelection( int row )
	{
		if ( updating || row < 0 || !tree->currentItem()
			|| tree->currentItem()->data( 0, CompiledRole ).toBool() )
			return;
		QVariant selected = layer->itemData( row );
		if ( !selected.isValid() )
			return;
		QModelIndex body = blockIndex( tree->currentItem()->data( 0, BodyBlockRole ).toInt() );
		QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		QModelIndex filter = nif->getIndex( info, "Havok Filter" );
		QModelIndex layerIndex = nif->getIndex( filter, "Layer" );
		if ( !layerIndex.isValid() )
			return;
		quint32 value = selected.toUInt();
		/* Snapshotted, like every other write in this panel.
		 *
		 * NifModel::set does not push an undo command by itself, so these live
		 * editors were writing straight through the model while Compile, Import
		 * Donor and Apply Safe Fixes in the same panel all went through
		 * nifSnapshotOp. Undo therefore looked like it worked and then reverted
		 * whichever of THOSE ran last, silently keeping the edit made here.
		 *
		 * One entry per edit rather than per pixel, because the spins are
		 * connected to editingFinished and the combos to activation.
		 */
		bool ok = false;
		nifSnapshotOp( nif, tr( "Set collision layer" ), [&]() {
			ok = nif->set<quint32>( layerIndex, value );
		} );
		if ( ok ) {
			physicsHint->setText( tr( "Collision layer set to %1." ).arg( collisionLayerName( value ) ) );
			queueRebuild();
		}
	}

	void applyMaterialSelection( int row )
	{
		if ( updating || row < 0 || !tree->currentItem()
			|| tree->currentItem()->data( 0, CompiledRole ).toBool() )
			return;
		QVariant selected = material->itemData( row );
		if ( !selected.isValid() )
			return;
		int shapeBlock = firstLeafShape( tree->currentItem()->data( 0, ShapeBlockRole ).toInt() );
		QModelIndex shape = blockIndex( shapeBlock );
		QModelIndex materialIndex = nif->getIndex( shape, "Material" );
		if ( !materialIndex.isValid() )
			return;
		quint32 value = selected.toUInt();
		bool ok = false;			// undoable — see applyLayerSelection
		nifSnapshotOp( nif, tr( "Set collision material" ), [&]() {
			ok = nif->set<quint32>( materialIndex, value );
		} );
		if ( ok ) {
			physicsHint->setText( tr( "Material set to %1." ).arg( materialText( value ) ) );
			queueRebuild();
		}
	}

	void applyPhysics()
	{
		if ( updating || !tree->currentItem() || tree->currentItem()->data( 0, CompiledRole ).toBool() )
			return;
		QModelIndex body = blockIndex( tree->currentItem()->data( 0, BodyBlockRole ).toInt() );
		QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		if ( !info.isValid() )
			return;
		/* Every field below in ONE undo entry — see applyLayerSelection for why
		 * these needed wrapping at all. One entry for the lot rather than one
		 * per field, because a single editingFinished can legitimately rewrite
		 * several of them (keyframed forces Motion System and Quality Type).
		 */
		nifSnapshotOp( nif, tr( "Edit rigid body" ), [&]() {
			QModelIndex filter = nif->getIndex( info, "Havok Filter" );
			nif->set<float>( info, "Mass", float( mass->value() ) );
			nif->set<float>( info, "Friction", float( friction->value() ) );
			nif->set<float>( info, "Restitution", float( restitution->value() ) );
			nif->set<float>( info, "Linear Damping", float( linearDamping->value() ) );
			nif->set<float>( info, "Angular Damping", float( angularDamping->value() ) );
			nif->set<float>( info, "Max Linear Velocity", float( maxLinearVelocity->value() ) );
			nif->set<float>( info, "Max Angular Velocity", float( maxAngularVelocity->value() ) );
			quint32 motionValue = motionSystem->currentData().toUInt();
			quint32 qualityValue = qualityType->currentData().toUInt();
			if ( keyframed->isChecked() ) { motionValue = 6u; qualityValue = 2u; }
			else if ( motionValue == 6u ) { motionValue = mass->value() > 0.0 ? 3u : 5u; qualityValue = mass->value() > 0.0 ? 4u : 0u; }
			nif->set<quint32>( info, "Motion System", motionValue );
			nif->set<quint32>( info, "Quality Type", qualityValue );
			nif->set<quint32>( info, "Solver Deactivation", solverDeactivation->currentData().toUInt() );
			nif->set<quint32>( info, "Deactivator Type", deactivatorType->currentData().toUInt() );
			nif->set<float>( info, "Penetration Depth", float( penetrationDepth->value() ) );
			nif->set<Vector4>( info, "Center", Vector4( float( centerX->value() ), float( centerY->value() ), float( centerZ->value() ), 0.0f ) );
			QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
			if ( inertia.isValid() ) {
				nif->set<float>( inertia, "m11", float( inertiaX->value() ) );
				nif->set<float>( inertia, "m22", float( inertiaY->value() ) );
				nif->set<float>( inertia, "m33", float( inertiaZ->value() ) );
			}
			quint32 bodyFlags = nif->get<quint32>( body, "Body Flags" );
			bodyFlags = wind->isChecked() ? ( bodyFlags | 1u ) : ( bodyFlags & ~1u );
			nif->set<quint32>( body, "Body Flags", bodyFlags );
			if ( filter.isValid() ) {
				quint32 flags = nif->get<quint32>( filter, "Flags" ) & ~0xc0u;
				if ( linkedGroup->isChecked() ) {
					flags |= 0x80u;
					if ( !collisionWithinGroup->isChecked() ) flags |= 0x40u;
				}
				nif->set<quint32>( filter, "Flags", flags );
				nif->set<quint32>( filter, "Group", quint32( filterGroup->value() ) );
			}
		} );
		if ( ogl ) {
			ogl->flush();
			ogl->update();
		}
		queueRebuild();
	}

	int selectedTargetNode() const
	{
		if ( tree->currentItem() ) {
			int node = tree->currentItem()->data( 0, NodeBlockRole ).toInt();
			if ( nif->isValidBlockNumber( node ) ) return node;
		}
		QModelIndex source = currentSource();
		int block = nif->getBlockNumber( source );
		if ( nif->blockInherits( source, "NiNode" ) ) return block;
		if ( nif->blockInherits( source, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } ) )
			return nif->getParent( block );
		return -1;
	}

	void collisionToBSTriShape()
	{
		CollisionMesh mesh = selectedCollisionMesh();
		if ( mesh.verts.size() < 3 || mesh.tris.isEmpty() ) {
			QMessageBox::information( this, tr( "Collision to BSTriShape" ), tr( "Select a collision row with usable geometry." ) );
			return;
		}
		int targetNode = selectedTargetNode();
		if ( !nif->isValidBlockNumber( targetNode ) ) {
			QMessageBox::warning( this, tr( "Collision to BSTriShape" ), tr( "The selected collision has no valid owning node." ) );
			return;
		}
		QVector<Vector3> normals( mesh.verts.size(), Vector3() );
		for ( const Triangle & tri : mesh.tris ) {
			Vector3 n = Vector3::crossproduct( mesh.verts[tri[1]] - mesh.verts[tri[0]], mesh.verts[tri[2]] - mesh.verts[tri[0]] );
			if ( n.length() > 1.0e-8f ) { n.normalize(); normals[tri[0]] += n; normals[tri[1]] += n; normals[tri[2]] += n; }
		}
		for ( Vector3 & n : normals ) { if ( n.length() > 1.0e-8f ) n.normalize(); else n = Vector3( 0, 0, 1 ); }
		QPersistentModelIndex created;
		nifSnapshotOp( nif, tr( "Collision to BSTriShape" ), [&, this]() {
			nif->holdUpdates( true );
			QModelIndex shape = nif->insertNiBlock( "BSTriShape" ); created = shape;
			nif->set<QString>( shape, "Name", QStringLiteral( "COL_PROXY_%1" ).arg( nodeName( targetNode ) ) );
			nif->set<quint32>( shape, "Flags", 14 );
			std::uint64_t vertexDesc = 0x0041B00000650407ULL;
			nif->set<BSVertexDesc>( shape, "Vertex Desc", vertexDesc );
			nif->set<quint32>( shape, "Num Triangles", quint32( mesh.tris.size() ) );
			nif->set<quint32>( shape, "Num Vertices", quint32( mesh.verts.size() ) );
			nif->set<quint32>( shape, "Data Size", quint32( mesh.verts.size() * 28 + mesh.tris.size() * 6 ) );
			nif->setState( BaseModel::Processing );
			QModelIndex vertexData = nif->getIndex( shape, "Vertex Data" ); nif->updateArraySize( vertexData );
			for ( int i = 0; i < mesh.verts.size(); i++ ) {
				QModelIndex vertex = nif->getIndex( vertexData, i );
				nif->set<Vector3>( vertex, "Vertex", mesh.verts.at( i ) );
				nif->set<HalfVector2>( vertex, "UV", HalfVector2( 0.0f, 0.0f ) );
				nif->set<ByteVector3>( vertex, "Normal", ByteVector3( normals.at( i ) ) );
			}
			QModelIndex triangleData = nif->getIndex( shape, "Triangles" ); nif->updateArraySize( triangleData );
			nif->setArray<Triangle>( triangleData, mesh.tris );
			BoundSphere bounds( mesh.verts, true ); bounds.update( nif, shape );
			nif->restoreState(); nif->holdUpdates( false );
			addLink( nif, blockIndex( targetNode ), "Children", nif->getBlockNumber( shape ) );
		} );
		if ( created.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( QModelIndex( created ) );
		queueRebuild();
	}

	static double recommendedDensity( const QString & materialName )
	{
		QString n = materialName.toLower();
		if ( n.contains( "metal" ) || n.contains( "armor" ) || n.contains( "weapon" ) ) return 7800.0;
		if ( n.contains( "stone" ) || n.contains( "concrete" ) || n.contains( "brick" ) ) return 2400.0;
		if ( n.contains( "glass" ) || n.contains( "ceramic" ) ) return 2500.0;
		if ( n.contains( "wood" ) ) return 550.0;
		if ( n.contains( "dirt" ) || n.contains( "sand" ) || n.contains( "gravel" ) ) return 1600.0;
		if ( n.contains( "rubber" ) ) return 1100.0;
		if ( n.contains( "plastic" ) ) return 950.0;
		if ( n.contains( "cloth" ) || n.contains( "paper" ) || n.contains( "carpet" ) ) return 300.0;
		if ( n.contains( "water" ) || n.contains( "organic" ) || n.contains( "skin" ) || n.contains( "meat" ) ) return 1000.0;
		return 1000.0;
	}

	void calculateMassFromMaterial()
	{
		if ( !tree->currentItem() || tree->currentItem()->data( 0, CompiledRole ).toBool() ) {
			QMessageBox::information( this, tr( "Mass from Material" ), tr( "Select editable collision first." ) ); return;
		}
		CollisionMesh mesh = selectedCollisionMesh();
		double gameVolume = meshVolumeGameUnits( mesh );
		double volume = gameVolume / std::pow( 69.99125, 3.0 );
		if ( !std::isfinite( volume ) || volume <= 1.0e-9 ) {
			QMessageBox::warning( this, tr( "Mass from Material" ),
				tr( "Could not derive a usable collision volume. The selected shape may be open, flat, or degenerate." ) ); return;
		}
		QString matName = material->currentText(); double suggested = recommendedDensity( matName ); bool ok = false;
		double density = QInputDialog::getDouble( this, tr( "Mass from Material" ),
			tr( "%1\nCollision volume: %2 m³\nDensity (kg/m³):" ).arg( matName ).arg( volume, 0, 'g', 6 ),
			suggested, 1.0, 50000.0, 1, &ok );
		if ( !ok ) return;
		double derivedMass = volume * density;
		mass->setValue( derivedMass ); applyPhysics();
		physicsHint->setText( tr( "Mass %1 kg = %2 m³ × %3 kg/m³" ).arg( derivedMass, 0, 'g', 6 ).arg( volume, 0, 'g', 6 ).arg( density, 0, 'g', 6 ) );
	}

	static void collectBranchBlocks( const NifModel * model, int block, QList<qint32> & blocks )
	{
		if ( !model->isValidBlockNumber( block ) || blocks.contains( block ) ) return;
		blocks.append( block );
		for ( int child : model->getChildLinks( block ) ) collectBranchBlocks( model, child, blocks );
	}

	void importDonorCollision()
	{
		int targetNode = selectedTargetNode();
		if ( !nif->isValidBlockNumber( targetNode ) ) {
			QMessageBox::information( this, tr( "Import Donor Collision" ), tr( "Select the target NiNode, BSTriShape, or collision row first." ) ); return;
		}
		QString fileName = QFileDialog::getOpenFileName( this, tr( "Choose Donor NIF" ), nif->getFolder(), tr( "NIF files (*.nif)" ) );
		if ( fileName.isEmpty() ) return;
		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( this, tr( "Import Donor Collision" ), tr( "Could not load the donor NIF." ) ); return;
		}
		if ( donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( this, tr( "Import Donor Collision" ), tr( "The donor uses a different Bethesda NIF version." ) ); return;
		}
		QStringList labels; QVector<int> objects;
		for ( int b = 0; b < donor.getBlockCount(); b++ ) {
			QModelIndex object = donor.getBlockIndex( b );
			if ( !donor.blockInherits( object, { "bhkCollisionObject", "bhkNPCollisionObject" } ) ) continue;
			int node = donor.getLink( object, "Target" ); if ( !donor.isValidBlockNumber( node ) ) node = donor.getParent( b );
			QString name = donor.isValidBlockNumber( node ) ? donor.get<QString>( donor.getBlockIndex( node ), "Name" ) : QString();
			labels.append( tr( "%1 — %2 [block %3]" ).arg( name.isEmpty() ? tr( "Unnamed" ) : name, donor.itemName( object ) ).arg( b ) );
			objects.append( b );
		}
		if ( objects.isEmpty() ) { QMessageBox::information( this, tr( "Import Donor Collision" ), tr( "The donor NIF contains no collision objects." ) ); return; }
		bool ok = false; QString chosen = QInputDialog::getItem( this, tr( "Import Donor Collision" ), tr( "Collision object:" ), labels, 0, false, &ok );
		if ( !ok ) return;
		int choice = labels.indexOf( chosen );
		if ( choice < 0 ) return;
		int donorObject = objects.at( choice ); QList<qint32> blocks; collectBranchBlocks( &donor, donorObject, blocks );
		QByteArray serialized; QBuffer writeBuffer( &serialized ); writeBuffer.open( QIODevice::WriteOnly ); QDataStream out( &writeBuffer );
		out << int( blocks.size() ); QMap<qint32, qint32> map;
		for ( int i = 0; i < blocks.size(); i++ ) map.insert( blocks.at( i ), nif->getBlockCount() + i );
		for ( int block : blocks ) { out << donor.itemName( donor.getBlockIndex( block ) ); donor.saveIndex( writeBuffer, donor.getBlockIndex( block ) ); }
		writeBuffer.close();
		QPersistentModelIndex imported;
		nifSnapshotOp( nif, tr( "Import donor collision" ), [&, this]() {
			QBuffer readBuffer( &serialized ); readBuffer.open( QIODevice::ReadOnly ); QDataStream in( &readBuffer ); int count = 0; in >> count;
			nif->holdUpdates( true );
			for ( int i = 0; i < count; i++ ) {
				QString type; in >> type; QModelIndex block = nif->insertNiBlock( type );
				if ( i == 0 ) imported = block;
				if ( !nif->loadAndMapLinks( readBuffer, block, map ) ) break;
			}
			nif->holdUpdates( false );
			QModelIndex target = blockIndex( targetNode ); QModelIndex old = blockIndex( nif->getLink( target, "Collision Object" ) );
			if ( old.isValid() ) { spRemoveBranch remove; remove.castIfApplicable( nif, old ); }
			if ( imported.isValid() ) {
				nif->setLink( QModelIndex( imported ), "Target", targetNode );
				nif->setLink( nif->getIndex( target, "Collision Object" ), nif->getBlockNumber( QModelIndex( imported ) ) );
			}
		} );
		if ( imported.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( QModelIndex( imported ) );
		queueRebuild();
	}

	QModelIndex currentSource() const
	{
		if ( auto * w = dynamic_cast<NifSkope *>( mw ) )
			return w->currentNifIndex();
		return QModelIndex();
	}

	void runSpell( const QString & id, const QModelIndex & index )
	{
		SpellPtr spell = SpellBook::lookup( id );
		if ( !spell ) {
			QMessageBox::warning( this, tr( "Collision Manager" ), tr( "The '%1' action is unavailable." ).arg( id ) );
			return;
		}
		if ( !spell->isApplicable( nif, index ) ) {
			QMessageBox::information( this, tr( "Collision Manager" ), tr( "Select a compatible block for '%1'." ).arg( spell->name() ) );
			return;
		}
		SpellBook book( nif, index );
		book.cast( nif, index, spell );
		queueRebuild();
	}

	void compileSelectedCollision()
	{
		QTreeWidgetItem * item = tree->currentItem();
		if ( !item || item->data( 0, CompiledRole ).toBool() ) {
			QMessageBox::information( this, tr( "Compile Collision" ), tr( "Select an editable collision body first." ) ); return;
		}
		int objectBlock = item->data( 0, ObjectBlockRole ).toInt();
		int bodyBlock = item->data( 0, BodyBlockRole ).toInt();
		int nodeBlock = item->data( 0, NodeBlockRole ).toInt();
		QModelIndex body = blockIndex( bodyBlock ), node = blockIndex( nodeBlock );
		if ( !body.isValid() || !node.isValid() ) {
			QMessageBox::warning( this, tr( "Compile Collision" ), tr( "The selected collision has a broken body or target node." ) ); return;
		}
		// Compile writes ONE static body as a compressed mesh in a
		// bhkPhysicsSystem. That is right for world collision and wrong for a
		// bone: it triangulates the capsule and cannot restore a ragdoll (see the
		// Decompile warning). Bone collision is exactly where this would be a
		// one-way trip, so ask first and default to Cancel.
		const QString role = boneRole( collectBoneRoles(), nodeBlock, -1 );
		if ( !role.isEmpty()
			&& QMessageBox::warning( this, tr( "Compile Collision" ),
				tr( "%1 is a bone (%2).\n\n"
					"Compile writes a single static body as a triangle mesh in a "
					"bhkPhysicsSystem. It cannot write bone collision, and it cannot rebuild "
					"a ragdoll - the joint constraints and the ragdoll skeleton are not "
					"decoded, so a ragdoll this came from stays lost.\n\n"
					"Compile anyway?" ).arg( nodeName( nodeBlock ), role ),
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
			return;

		CollisionMesh mesh; int rootShape = nif->getLink( body, "Shape" ); appendEditableMesh( rootShape, mesh );
		HknpEncodeInput input; input.verts.reserve( mesh.verts.size() );
		for ( const Vector3 & v : mesh.verts ) input.verts.append( v / 69.99125f );
		input.tris = mesh.tris;
		QModelIndex info = nif->getIndex( body, "Rigid Body Info" ); QModelIndex filter = nif->getIndex( info, "Havok Filter" );
		quint32 sourceMotion = info.isValid() ? nif->get<quint32>( info, "Motion System" ) : 5u;
		QStringList unsupported;
		if ( sourceMotion != 3u && sourceMotion != 5u ) unsupported << tr( "Motion/Keyframed mode %1" ).arg( sourceMotion );
		quint32 expectedQuality = sourceMotion == 3u ? 4u : 0u, expectedSolver = sourceMotion == 3u ? 2u : 1u;
		if ( info.isValid() && nif->get<quint32>( info, "Quality Type" ) != expectedQuality ) unsupported << tr( "Quality Type" );
		if ( info.isValid() && nif->get<quint32>( info, "Solver Deactivation" ) != expectedSolver ) unsupported << tr( "Solver Deactivation" );
		if ( nif->get<quint32>( body, "Body Flags" ) & 1u ) unsupported << tr( "Wind" );
		if ( info.isValid() && nif->get<quint32>( info, "Deactivator Type" ) != 1u ) unsupported << tr( "Deactivator Type" );
		if ( info.isValid() && std::fabs( nif->get<float>( info, "Penetration Depth" ) - 0.15f ) > 1.0e-5f ) unsupported << tr( "Allowed Penetration" );
		if ( !unsupported.isEmpty() ) {
			QMessageBox::warning( this, tr( "Compile Collision" ),
				tr( "These settings are valid on editable collision but their FO4 hknp byte layout is not validated yet, so Compile will not discard them:\n\n%1" )
					.arg( unsupported.join( QStringLiteral( "\n" ) ) ) );
			return;
		}
		input.layer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 1u;
		input.filterFlags = filter.isValid() ? quint8( nif->get<quint32>( filter, "Flags" ) ) : 0u;
		input.filterGroup = filter.isValid() ? quint16( nif->get<quint32>( filter, "Group" ) ) : 0u;
		input.mass = info.isValid() ? nif->get<float>( info, "Mass" ) : 0.0f;
		input.dynamic = sourceMotion == 3u;
		input.friction = info.isValid() ? nif->get<float>( info, "Friction" ) : 0.5f;
		input.restitution = info.isValid() ? nif->get<float>( info, "Restitution" ) : 0.4f;
		input.gravityFactor = info.isValid() ? nif->get<float>( info, "Gravity Factor" ) : 1.0f;
		input.maxLinVelocity = info.isValid() ? nif->get<float>( info, "Max Linear Velocity" ) : 104.375f;
		input.maxAngVelocity = info.isValid() ? nif->get<float>( info, "Max Angular Velocity" ) : 31.57f;
		input.linDamping = info.isValid() ? nif->get<float>( info, "Linear Damping" ) : 0.1f;
		input.angDamping = info.isValid() ? nif->get<float>( info, "Angular Damping" ) : 0.05f;
		if ( info.isValid() ) {
			Vector4 center = nif->get<Vector4>( info, "Center" ); input.center = Vector3( center[0], center[1], center[2] );
			QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
			input.inertia = Vector3( nif->get<float>( inertia, "m11" ), nif->get<float>( inertia, "m22" ), nif->get<float>( inertia, "m33" ) );
		}
		QModelIndex materialShape = blockIndex( firstLeafShape( rootShape ) );
		input.materialCRC = materialShape.isValid() ? nif->get<quint32>( materialShape, "Material" ) : 0u;
		QString error; QByteArray bytes = hknpEncodeCompressedMesh( input, &error );
		if ( bytes.isEmpty() ) {
			QMessageBox::warning( this, tr( "Compile Collision" ), error ); return;
		}
		HknpSystem roundTrip = hknpDecode( bytes );
		if ( !roundTrip.valid || roundTrip.shapes.isEmpty() || roundTrip.shapes.first().tris.isEmpty() ) {
			QMessageBox::critical( this, tr( "Compile Collision" ), tr( "The generated packfile failed NifSkope's round-trip check: %1" ).arg( roundTrip.error ) ); return;
		}
		// multi-section files must decode to exactly the input triangles
		// (section boundaries duplicate verts, so only the tri count is stable)
		if ( roundTrip.shapes.first().tris.size() != input.tris.size() ) {
			QMessageBox::critical( this, tr( "Compile Collision" ),
				tr( "Round-trip triangle count mismatch (%1 encoded, %2 decoded) — the packfile was not written." )
					.arg( input.tris.size() ).arg( roundTrip.shapes.first().tris.size() ) );
			return;
		}
		QPersistentModelIndex oldObject = blockIndex( objectBlock ), target = node;
		QPersistentModelIndex newObject, newSystem;
		nifSnapshotOp( nif, tr( "Compile collision" ), [&, this]() {
			nif->holdUpdates( true );
			newSystem = nif->insertNiBlock( "bhkPhysicsSystem" );
			nif->set<QByteArray>( QModelIndex( newSystem ), "Binary Data", bytes );
			newObject = nif->insertNiBlock( "bhkNPCollisionObject" );
			nif->setLink( QModelIndex( newObject ), "Target", nodeBlock );
			nif->setLink( QModelIndex( newObject ), "Data", nif->getBlockNumber( QModelIndex( newSystem ) ) );
			nif->set<quint32>( QModelIndex( newObject ), "Body ID", 0u );
			nif->holdUpdates( false );
			if ( oldObject.isValid() ) { spRemoveBranch remove; remove.castIfApplicable( nif, QModelIndex( oldObject ) ); }
			if ( target.isValid() && newObject.isValid() )
				nif->setLink( nif->getIndex( QModelIndex( target ), "Collision Object" ), nif->getBlockNumber( QModelIndex( newObject ) ) );
		} );
		if ( newObject.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( QModelIndex( newObject ) );
		queueRebuild();
	}

	void lintCollision()
	{
		int dangling = 0, suspiciousHulls = 0, nearBoxes = 0, nonUniformPrimitives = 0;
		int stairWithoutSlope = 0, visibleWithoutCollision = 0, compiled = 0, editable = 0;
		QVector<int> danglingObjects;
		QVector<QPersistentModelIndex> zeroLayerFilters;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				compiled++;
				if ( !blockIndex( nif->getLink( i, "Data" ) ).isValid() ) {
					dangling++; danglingObjects.append( b );
				}
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				editable++;
				int body = nif->getLink( i, "Body" );
				if ( !blockIndex( body ).isValid() ) {
					dangling++; danglingObjects.append( b );
				}
				int shape = blockIndex( body ).isValid() ? nif->getLink( blockIndex( body ), "Shape" ) : -1;
				int leaf = firstLeafShape( shape );
				QModelIndex li = blockIndex( leaf );
				if ( li.isValid() && nif->isNiBlock( li, "bhkConvexVerticesShape" ) ) {
					QVector<Vector4> vertices = nif->getArray<Vector4>( li, "Vertices" );
					if ( vertices.size() > 64 ) suspiciousHulls++;
					if ( vertices.size() >= 8 ) {
						Vector3 mn( vertices.first()[0], vertices.first()[1], vertices.first()[2] ), mx( mn );
						for ( const Vector4 & v : vertices ) for ( int axis = 0; axis < 3; axis++ ) {
							mn[axis] = std::min( mn[axis], v[axis] ); mx[axis] = std::max( mx[axis], v[axis] );
						}
						float tolerance = std::max( { mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2] } ) * 0.03f + 1.0e-5f;
						bool boxLike = true;
						for ( const Vector4 & v : vertices ) for ( int axis = 0; axis < 3; axis++ )
							if ( std::min( std::fabs( v[axis] - mn[axis] ), std::fabs( v[axis] - mx[axis] ) ) > tolerance ) boxLike = false;
						if ( boxLike ) nearBoxes++;
					}
				}
				QModelIndex info = blockIndex( body ).isValid() ? nif->getIndex( blockIndex( body ), "Rigid Body Info" ) : QModelIndex();
				QModelIndex filter = info.isValid() ? nif->getIndex( info, "Havok Filter" ) : QModelIndex();
				quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 0;
				if ( filter.isValid() && collLayer == 0 ) zeroLayerFilters.append( filter );
				if ( collLayer == 31 && li.isValid() && ( nif->isNiBlock( li, "bhkBoxShape" )
						|| nif->isNiBlock( li, "bhkSphereShape" ) || nif->isNiBlock( li, "bhkCapsuleShape" ) ) ) stairWithoutSlope++;
			}
			if ( nif->isNiBlock( i, "bhkTransformShape" ) || nif->isNiBlock( i, "bhkConvexTransformShape" ) ) {
				QModelIndex child = blockIndex( firstLeafShape( nif->getLink( i, "Shape" ) ) );
				if ( child.isValid() && ( nif->isNiBlock( child, "bhkBoxShape" ) || nif->isNiBlock( child, "bhkSphereShape" )
						|| nif->isNiBlock( child, "bhkCapsuleShape" ) ) ) {
					Vector3 trans, scales; Matrix rot; nif->get<Matrix4>( i, "Transform" ).decompose( trans, rot, scales );
					float lo = std::min( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					float hi = std::max( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					if ( hi - lo > std::max( 1.0e-4f, hi * 0.001f ) ) nonUniformPrimitives++;
				}
			}
			if ( nif->blockInherits( i, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } )
					&& !( nif->get<quint32>( i, "Flags" ) & 1u ) ) {
				bool protectedByCollision = false; int owner = nif->getParent( b );
				for ( int depth = 0; nif->isValidBlockNumber( owner ) && depth < 32; depth++, owner = nif->getParent( owner ) ) {
					QModelIndex node = blockIndex( owner );
					if ( nif->blockInherits( node, "NiNode" ) && nif->isValidBlockNumber( nif->getLink( node, "Collision Object" ) ) ) {
						protectedByCollision = true; break;
					}
				}
				if ( !protectedByCollision ) visibleWithoutCollision++;
			}
		}
		QStringList findings;
		if ( dangling ) findings << tr( "%1 dangling collision reference(s) - safe fix: remove the broken collision object" ).arg( dangling );
		if ( !zeroLayerFilters.isEmpty() ) findings << tr( "%1 collision layer(s) are Unidentified (0) - safe fix: infer Static or Props from motion" ).arg( zeroLayerFilters.size() );
		if ( suspiciousHulls ) findings << tr( "%1 convex hull(s) exceed 64 vertices - optimize the source mesh or rebuild Convex" ).arg( suspiciousHulls );
		if ( nearBoxes ) findings << tr( "%1 hull(s) are box-like - Box collision would be cheaper" ).arg( nearBoxes );
		if ( nonUniformPrimitives ) findings << tr( "%1 primitive collision transform(s) use non-uniform scale" ).arg( nonUniformPrimitives );
		if ( stairWithoutSlope ) findings << tr( "%1 STAIRHELPER body/bodies appear to contain no sloped shape" ).arg( stairWithoutSlope );
		if ( visibleWithoutCollision ) findings << tr( "%1 visible geometry block(s) have no collision on their node hierarchy" ).arg( visibleWithoutCollision );
		if ( compiled && editable ) findings << tr( "File contains mixed compiled and editable collision - finish editing, then compile consistently" );
		if ( findings.isEmpty() ) {
			QMessageBox::information( this, tr( "Check Collision" ), tr( "No collision issues found." ) ); return;
		}
		QMessageBox report( QMessageBox::Warning, tr( "Check Collision" ), findings.join( QStringLiteral( "\n\n" ) ), QMessageBox::NoButton, this );
		auto * fix = report.addButton( tr( "Apply Safe Fixes" ), QMessageBox::AcceptRole );
		report.addButton( QMessageBox::Close ); report.exec();
		if ( report.clickedButton() != fix || ( danglingObjects.isEmpty() && zeroLayerFilters.isEmpty() ) ) return;
		nifSnapshotOp( nif, tr( "Fix collision warnings" ), [&, this]() {
			for ( const QPersistentModelIndex & filter : zeroLayerFilters ) if ( filter.isValid() ) {
				QModelIndex info = filter.parent();
				quint32 motion = nif->get<quint32>( info, "Motion System" );
				nif->set<quint32>( QModelIndex( filter ), "Layer", motion == 3 ? 10u : 1u );
			}
			std::sort( danglingObjects.begin(), danglingObjects.end(), std::greater<int>() );
			for ( int block : danglingObjects ) if ( nif->isValidBlockNumber( block ) ) {
				spRemoveBranch remove; remove.castIfApplicable( nif, blockIndex( block ) );
			}
		} );
		queueRebuild();
	}

	QFrame * separator()
	{
		auto * line = new QFrame( this );
		line->setFrameShape( QFrame::HLine );
		line->setFrameShadow( QFrame::Sunken );
		return line;
	}

	void positionPreviewPanel()
	{
		if ( !previewPanel || !ogl ) return;
		previewPanel->adjustSize();
		QPoint local( std::max( 10, ogl->width() - previewPanel->width() - 10 ),
			std::max( 10, ogl->height() - previewPanel->height() - 10 ) );
		previewPanel->move( ogl->mapToGlobal( local ) );
	}

	void clearCollisionPreview()
	{
		if ( previewTimer ) previewTimer->stop();
		if ( ogl ) ogl->clearCollisionPreview();
		previewSource = QPersistentModelIndex();
		if ( previewPanel ) previewPanel->hide();
	}

	void updatePreviewControlVisibility()
	{
		if ( !previewPanel ) return;
		const bool convex = ( previewKind == 0 );
		const bool decomp = convex && previewMethod->currentIndex() == 1;
		const QList<QWidget *> convexControls = { previewMethodLabel, previewMethod, previewPrecisionLabel, previewPrecision };
		const QList<QWidget *> decompositionControls = { previewThresholdLabel, previewThreshold, previewMaxHullsLabel, previewMaxHulls };
		for ( QWidget * widget : convexControls ) widget->setVisible( convex );
		for ( QWidget * widget : decompositionControls ) widget->setVisible( decomp );
		QString title = convex ? tr( "Collision - Convex Preview" ) : tr( "Collision - Mesh Optimization" );
		previewPanel->setProperty( "titleText", title );
		previewTitle->setText( ( previewPanel->property( "collapsed" ).toBool()
			? QStringLiteral( "˃  " ) : QStringLiteral( "˅  " ) ) + title );
		positionPreviewPanel();
	}

	void rebuildCollisionPreview()
	{
		if ( !previewPanel || !previewPanel->isVisible() || !previewSource.isValid() ) return;
		QVector<Vector3> soup;
		QString statistics;
		bool ok = tlBuildCollisionPreview( nif, QModelIndex( previewSource ), previewKind,
			float( previewRatio->value() / 100.0 ), previewMethod->currentIndex() == 1,
			float( previewPrecision->value() ), float( previewThreshold->value() ),
			previewMaxHulls->value(), soup, statistics );
		previewStats->setText( statistics );
		previewStats->setStyleSheet( ok ? QStringLiteral( "color:%1" ).arg( wwSkinColor( "textMuted" ) )
										: QStringLiteral( "color:%1" ).arg( wwSkinColor( "danger" ) ) );
		if ( ok ) ogl->setCollisionPreview( soup ); else ogl->clearCollisionPreview();
		positionPreviewPanel();
	}

	void ensurePreviewPanel()
	{
		if ( previewPanel ) return;
		previewPanel = new QFrame( mw, Qt::Tool | Qt::FramelessWindowHint );
		previewPanel->setObjectName( QStringLiteral( "CollisionOperatorPanel" ) );
		previewPanel->setFrameShape( QFrame::StyledPanel );
		previewPanel->setAutoFillBackground( true );
		previewPanel->setAttribute( Qt::WA_ShowWithoutActivating );
		previewPanel->setProperty( "collapsed", false );
		previewPanel->setStyleSheet( QStringLiteral(
			"QFrame#CollisionOperatorPanel { background:%1; border:1px solid %2; }"
			"QLabel { color:%3; background:transparent; }"
			"QToolButton { color:%3; background:transparent; border:none; }"
			"QToolButton:hover { color:%4; }"
			"QPushButton { background:%5; color:%3; border:none; border-radius:3px; padding:4px 14px; }"
			"QPushButton:hover { background:%6; }"
			"QPushButton:pressed { background:%7; }"
			"QComboBox { background:%8; color:%3; border:none; border-radius:3px; padding:2px 6px; }"
			"QSpinBox { background:%5; color:%3; border:none; border-radius:3px; padding:2px 6px; }" )
			.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ),
				  wwSkinColor( "textBright" ), wwSkinColor( "bgInput" ), wwSkinColor( "bgBtnHover" ),
				  wwSkinColor( "bgBtnDown" ), wwSkinColor( "bgPanel" ) ) );

		auto * outer = new QVBoxLayout( previewPanel );
		outer->setContentsMargins( 10, 8, 10, 8 ); outer->setSpacing( 4 );
		auto * header = new QHBoxLayout;
		previewTitle = new QToolButton( previewPanel );
		previewTitle->setAutoRaise( true );
		QFont titleFont = previewTitle->font(); titleFont.setBold( true ); previewTitle->setFont( titleFont );
		auto * close = new QToolButton( previewPanel ); close->setText( QStringLiteral( "✕" ) ); close->setAutoRaise( true );
		header->addWidget( previewTitle ); header->addStretch(); header->addWidget( close ); outer->addLayout( header );
		previewBody = new QWidget( previewPanel );
		auto * bodyLayout = new QVBoxLayout( previewBody ); bodyLayout->setContentsMargins( 0, 0, 0, 0 ); bodyLayout->setSpacing( 4 );
		auto * grid = new QGridLayout; grid->setContentsMargins( 0, 0, 0, 0 ); grid->setHorizontalSpacing( 8 ); grid->setVerticalSpacing( 3 );
		previewRatio = new CollisionDragSpinBox( previewBody );
		previewRatio->setRange( 1.0, 100.0 ); previewRatio->setDecimals( 0 ); previewRatio->setSuffix( QStringLiteral( "%" ) );
		previewRatio->setSingleStep( 1.0 ); previewRatio->setKeyboardTracking( false ); previewRatio->setMinimumWidth( 150 );
		previewRatio->setToolTip( tr( "Drag left/right to change the triangle percentage; Shift-drag for fine control" ) );
		previewMethod = new QComboBox( previewBody ); previewMethod->addItems( { tr( "Single Hull" ), tr( "Decomposition (CoACD)" ) } );
		previewPrecision = new CollisionDragSpinBox( previewBody );
		previewPrecision->setRange( 0.0, 5.0 ); previewPrecision->setDecimals( 3 ); previewPrecision->setSingleStep( 0.01 ); previewPrecision->setKeyboardTracking( false );
		previewPrecision->setToolTip( tr( "Drag left/right to change hull precision; Shift-drag for fine control" ) );
		previewThreshold = new CollisionDragSpinBox( previewBody );
		previewThreshold->setRange( 0.01, 1.0 ); previewThreshold->setDecimals( 3 ); previewThreshold->setSingleStep( 0.01 ); previewThreshold->setKeyboardTracking( false );
		previewThreshold->setToolTip( tr( "Drag left/right to change the decomposition threshold; Shift-drag for fine control" ) );
		previewMaxHulls = new QSpinBox( previewBody ); previewMaxHulls->setRange( 1, 256 ); previewMaxHulls->setKeyboardTracking( false );
		previewMethodLabel = new QLabel( tr( "Method" ), previewBody );
		previewPrecisionLabel = new QLabel( tr( "Hull precision" ), previewBody );
		previewThresholdLabel = new QLabel( tr( "Threshold" ), previewBody );
		previewMaxHullsLabel = new QLabel( tr( "Max hulls" ), previewBody );
		grid->addWidget( new QLabel( tr( "Triangles" ), previewBody ), 0, 0 ); grid->addWidget( previewRatio, 0, 1 );
		grid->addWidget( previewMethodLabel, 1, 0 ); grid->addWidget( previewMethod, 1, 1 );
		grid->addWidget( previewPrecisionLabel, 2, 0 ); grid->addWidget( previewPrecision, 2, 1 );
		grid->addWidget( previewThresholdLabel, 3, 0 ); grid->addWidget( previewThreshold, 3, 1 );
		grid->addWidget( previewMaxHullsLabel, 4, 0 ); grid->addWidget( previewMaxHulls, 4, 1 );
		bodyLayout->addLayout( grid );
		previewStats = new QLabel( previewBody ); previewStats->setWordWrap( true ); bodyLayout->addWidget( previewStats );
		auto * buttons = new QHBoxLayout;
		auto * cancel = new QPushButton( tr( "Cancel" ), previewBody );
		auto * apply = new QPushButton( tr( "Apply" ), previewBody );
		buttons->addWidget( cancel ); buttons->addWidget( apply ); bodyLayout->addLayout( buttons );
		outer->addWidget( previewBody );

		previewTimer = new QTimer( this ); previewTimer->setSingleShot( true ); previewTimer->setInterval( 80 );
		auto queuePreview = [this]() {
			updatePreviewControlVisibility();
			// Throttle expensive mesh rebuilding while a scrub gesture is active,
			// instead of postponing the preview until the drag has stopped.
			if ( !previewTimer->isActive() ) previewTimer->start();
		};
		connect( previewTimer, &QTimer::timeout, this, [this]() { rebuildCollisionPreview(); } );
		connect( previewRatio, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewPrecision, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewThreshold, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewMaxHulls, qOverload<int>( &QSpinBox::valueChanged ), this, [queuePreview]( int ) { queuePreview(); } );
		connect( previewMethod, qOverload<int>( &QComboBox::currentIndexChanged ), this, [queuePreview]( int ) { queuePreview(); } );
		connect( previewTitle, &QToolButton::clicked, this, [this]() {
			bool collapsed = !previewPanel->property( "collapsed" ).toBool();
			previewPanel->setProperty( "collapsed", collapsed );
			previewBody->setVisible( !collapsed );
			updatePreviewControlVisibility();
			previewPanel->adjustSize();
			previewPanel->resize( previewPanel->sizeHint() );
			positionPreviewPanel();
		} );
		connect( close, &QToolButton::clicked, this, [this]() { clearCollisionPreview(); } );
		connect( cancel, &QPushButton::clicked, this, [this]() { clearCollisionPreview(); } );
		connect( apply, &QPushButton::clicked, this, [this]() {
			if ( !previewSource.isValid() ) return;
			QModelIndex result = tlCommitCollisionPreview( nif, QModelIndex( previewSource ), previewKind,
				float( previewRatio->value() / 100.0 ), previewMethod->currentIndex() == 1,
				float( previewPrecision->value() ), float( previewThreshold->value() ), previewMaxHulls->value() );
			ogl->clearCollisionPreview(); previewPanel->hide(); previewSource = QPersistentModelIndex();
			if ( result.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( result );
			queueRebuild();
		} );
		connect( ogl, &QWindow::widthChanged, this, [this]( int ) { if ( previewPanel->isVisible() ) positionPreviewPanel(); } );
		connect( ogl, &QWindow::heightChanged, this, [this]( int ) { if ( previewPanel->isVisible() ) positionPreviewPanel(); } );
	}

	void showCollisionPreview( int kind, double initialRatio, bool decomposition = false )
	{
		QModelIndex source = currentSource();
		if ( !source.isValid() ) {
			QMessageBox::information( this, tr( "Collision Preview" ), tr( "Select a BSTriShape or NiNode first." ) );
			return;
		}
		ensurePreviewPanel();
		for ( const char * name : { "GizmoRedoPanel", "OperatorRedoPanel", "BoxSelectRedoPanel" } )
			if ( QFrame * panel = mw->findChild<QFrame *>( QLatin1String( name ) ); panel ) panel->hide();
		previewKind = kind;
		previewSource = source;
		QSettings settings;
		const QList<QObject *> controls = { previewRatio, previewMethod, previewPrecision, previewThreshold, previewMaxHulls };
		for ( QObject * control : controls ) control->blockSignals( true );
		previewRatio->setValue( std::clamp( initialRatio, 1.0, 100.0 ) );
		previewMethod->setCurrentIndex( decomposition ? 1 : 0 );
		previewPrecision->setValue( settings.value( "CollisionManager/Preview/Precision", 0.25 ).toDouble() );
		previewThreshold->setValue( settings.value( "CollisionManager/Preview/Threshold", 0.05 ).toDouble() );
		previewMaxHulls->setValue( settings.value( "CollisionManager/Preview/MaxHulls", 16 ).toInt() );
		for ( QObject * control : controls ) control->blockSignals( false );
		previewPanel->show(); previewPanel->raise();
		updatePreviewControlVisibility();
		rebuildCollisionPreview();
	}

	void buildUi()
	{
		auto * root = new QVBoxLayout( this );
		root->setContentsMargins( 6, 6, 6, 6 );
		root->setSpacing( 6 );

		auto * display = new QHBoxLayout;
		auto * showCollision = new QCheckBox( tr( "Show collision" ), this );
		QAction * showAction = mw ? mw->findChild<QAction *>( QStringLiteral( "aShowCollision" ) ) : nullptr;
		showCollision->setChecked( !showAction || showAction->isChecked() );
		connect( showCollision, &QCheckBox::toggled, this, [showAction]( bool on ) {
			if ( showAction && showAction->isChecked() != on )
				showAction->trigger();
		} );
		auto * colour = new QComboBox( this );
		colour->addItems( { tr( "Material" ), tr( "Layer" ), tr( "State" ), tr( "Type" ) } );
		colour->setToolTip( tr( "Collision fill colour mode" ) );
		auto * solid = new QCheckBox( tr( "Solid" ), this );
		auto * xray = new QCheckBox( tr( "X-ray" ), this );
		auto * only = new QCheckBox( tr( "Only" ), this );
		only->setToolTip( tr( "Hide render geometry and show only collision" ) );
		auto * labels = new QComboBox( this );
		labels->addItems( { tr( "Off" ), tr( "Selected" ), tr( "All" ) } );
		QSettings settings;
		colour->setCurrentIndex( settings.value( "CollisionManager/ColourBy", 0 ).toInt() );
		solid->setChecked( settings.value( "CollisionManager/Solid", true ).toBool() );
		xray->setChecked( settings.value( "CollisionManager/XRay", false ).toBool() );
		only->setChecked( settings.value( "CollisionManager/CollisionOnly", false ).toBool() );
		labels->setCurrentIndex( settings.value( "CollisionManager/Labels", 1 ).toInt() );
		auto saveDisplay = [this, colour, solid, xray, only, labels]() {
			QSettings s;
			s.setValue( "CollisionManager/ColourBy", colour->currentIndex() );
			s.setValue( "CollisionManager/Solid", solid->isChecked() );
			s.setValue( "CollisionManager/XRay", xray->isChecked() );
			s.setValue( "CollisionManager/CollisionOnly", only->isChecked() );
			s.setValue( "CollisionManager/Labels", labels->currentIndex() );
			// Scene::draw reads a cached copy (a per-frame QSettings read is a
			// registry access) — push the change to it.
			Scene::refreshCollisionOnlySetting();
			if ( ogl ) ogl->update();
		};
		connect( colour, qOverload<int>( &QComboBox::currentIndexChanged ), this, [saveDisplay]( int ) { saveDisplay(); } );
		connect( solid, &QCheckBox::toggled, this, [saveDisplay]( bool ) { saveDisplay(); } );
		connect( xray, &QCheckBox::toggled, this, [saveDisplay]( bool ) { saveDisplay(); } );
		connect( only, &QCheckBox::toggled, this, [saveDisplay]( bool ) { saveDisplay(); } );
		connect( labels, qOverload<int>( &QComboBox::currentIndexChanged ), this, [saveDisplay]( int ) { saveDisplay(); } );
		display->addWidget( showCollision );
		display->addWidget( new QLabel( tr( "Colour by" ), this ) );
		display->addWidget( colour, 1 );
		display->addWidget( solid );
		display->addWidget( xray );
		display->addWidget( only );
		display->addWidget( new QLabel( tr( "Labels" ), this ) );
		display->addWidget( labels );
		root->addLayout( display );

		inventoryHeader = new QLabel( tr( "Collision in file" ), this );
		inventoryHeader->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
		root->addWidget( inventoryHeader );

		tree = new QTreeWidget( this );
		tree->setColumnCount( 7 );
		tree->setHeaderLabels( { tr( "Node" ), tr( "Shape" ), tr( "Layer" ), tr( "Material" ),
			tr( "Mass" ), tr( "State" ), tr( "Bone" ) } );
		tree->setRootIsDecorated( true );
		tree->setAlternatingRowColors( true );
		tree->setSelectionMode( QAbstractItemView::SingleSelection );
		tree->setContextMenuPolicy( Qt::CustomContextMenu );
		tree->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
		tree->header()->setSectionResizeMode( 3, QHeaderView::Stretch );
		tree->header()->setSectionResizeMode( 6, QHeaderView::ResizeToContents );
		// Bone is logical column 6 but belongs beside Node; moving the visual
		// section leaves the 42 literal column indices in this file alone.
		tree->header()->moveSection( 6, 1 );
		tree->setMinimumHeight( 150 );
		tree->setStyleSheet( wwSelectionTreeQss() );
		int sortColumn = std::clamp( settings.value( "CollisionManager/SortColumn", 0 ).toInt(), 0, 5 );
		Qt::SortOrder initialOrder = settings.value( "CollisionManager/SortOrder", int( Qt::AscendingOrder ) ).toInt()
			? Qt::DescendingOrder : Qt::AscendingOrder;
		tree->setSortingEnabled( true );
		tree->sortItems( sortColumn, initialOrder );
		connect( tree->header(), &QHeaderView::sortIndicatorChanged, this,
			[]( int column, Qt::SortOrder order ) {
				QSettings s; s.setValue( "CollisionManager/SortColumn", column ); s.setValue( "CollisionManager/SortOrder", int( order ) );
			} );
		root->addWidget( tree, 1 );

		auto * browserActions = new QHBoxLayout;
		decompileSelectedAction = new QAction( tr( "Decompile Selected" ), this );
		compileSelectedAction = new QAction( tr( "Compile Selected" ), this );
		compileSelectedAction->setToolTip( tr( "Compile the selected editable collision into an FO4 hknp packfile" ) );
		auto * decompileAllAction = new QAction( tr( "Decompile All" ), this );
		primaryCollisionAction = new QToolButton( this );
		primaryCollisionAction->setToolButtonStyle( Qt::ToolButtonTextOnly );
		primaryCollisionAction->setPopupMode( QToolButton::MenuButtonPopup );
		primaryCollisionAction->setDefaultAction( decompileSelectedAction );
		auto * primaryMenu = new QMenu( primaryCollisionAction );
		primaryMenu->addAction( decompileSelectedAction );
		primaryMenu->addAction( decompileAllAction );
		primaryMenu->addSeparator();
		primaryMenu->addAction( compileSelectedAction );
		primaryCollisionAction->setMenu( primaryMenu );
		primaryCollisionAction->setEnabled( false );

		auto * lint = new QPushButton( tr( "Check Collision" ), this );
		auto * donor = new QPushButton( tr( "Import Donor" ), this );
		lint->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		donor->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
		donor->setToolTip( tr( "Copy collision from another NIF onto the selected target node" ) );
		auto * more = new QToolButton( this );
		more->setText( tr( "More..." ) );
		more->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 8px" ) ) );
		more->setPopupMode( QToolButton::InstantPopup );
		auto * moreMenu = new QMenu( more );
		auto * reverseAction = moreMenu->addAction( tr( "Create Editable Mesh Copy" ) );
		reverseAction->setToolTip( tr( "Create a visible BSTriShape proxy from the selected collision" ) );
		auto * refreshAction = moreMenu->addAction( tr( "Refresh" ) );
		more->setMenu( moreMenu );
		browserActions->addWidget( primaryCollisionAction );
		browserActions->addWidget( lint );
		browserActions->addWidget( donor );
		browserActions->addStretch();
		browserActions->addWidget( more );
		root->addLayout( browserActions );
		root->addWidget( separator() );

		auto * createGroup = new QGroupBox( tr( "Collision Creation" ), this );
		auto * createLayout = new QGridLayout( createGroup );
		sourceLabel = new QLabel( tr( "Select a BSTriShape or NiNode in the viewport/block list" ), createGroup );
		sourceLabel->setWordWrap( true );
		auto * shapeButtons = new QHBoxLayout;
		shapeButtons->setSpacing( 4 );
		auto * shapeGroup = new QButtonGroup( createGroup );
		shapeGroup->setExclusive( true );
		const QStringList shapeNames = {
			tr( "Box" ), tr( "Sphere" ), tr( "Capsule" ), tr( "Convex" ), tr( "Mesh" )
		};
		for ( int mode = 0; mode < shapeNames.size(); mode++ ) {
			auto * button = new QToolButton( createGroup );
			button->setText( shapeNames.at( mode ) );
			button->setCheckable( true );
			button->setMinimumSize( 55, 30 );
			button->setToolButtonStyle( Qt::ToolButtonTextOnly );
			shapeGroup->addButton( button, mode );
			shapeButtons->addWidget( button, 1 );
		}
		shapeGroup->button( 3 )->setChecked( true );
		auto * convexMethod = new QComboBox( createGroup );
		convexMethod->addItems( { tr( "Single Hull (qhull)" ), tr( "Decomposition (CoACD)" ) } );
		createGroup->setStyleSheet( QStringLiteral(
			"QToolButton { border: 1px solid %1; border-radius: 3px; padding: 3px; background: %2; }"
			"QToolButton:hover { background: %3; }"
			"QToolButton:checked { border-color: %4; color: %5; background: %6; }" )
			.arg( wwSkinColor( "border" ), wwSkinColor( "bgBtn" ), wwSkinColor( "bgBtnHover" ),
				  wwSkinColor( "accent" ), wwSkinColor( "accentText" ), wwSkinColor( "accentBg" ) ) );
		QSettings createSettings;
		convexMethod->setCurrentIndex( createSettings.value( "CollisionManager/Create/ConvexMethod", 0 ).toInt() );
		auto * preset = new QComboBox( createGroup );
		// Stable data IDs preserve settings written by the original three-item
		// list while allowing the authoring-oriented display order below.
		preset->addItem( tr( "Static" ), 0 );
		preset->addItem( tr( "Anim Static" ), 3 );
		preset->addItem( tr( "Prop (dynamic)" ), 1 );
		preset->addItem( tr( "Stairhelper" ), 4 );
		preset->addItem( tr( "Custom" ), 2 );
		int savedPresetRow = preset->findData( createSettings.value( "CollisionManager/Create/Preset", 1 ).toInt() );
		preset->setCurrentIndex( savedPresetRow >= 0 ? savedPresetRow : preset->findData( 1 ) );
		auto * savePreset = new QToolButton( createGroup );
		savePreset->setIcon( style()->standardIcon( QStyle::SP_DialogSaveButton ) );
		savePreset->setToolTip( tr( "Save these collision creation defaults" ) );
		auto * materialEdit = new QComboBox( createGroup );
		materialEdit->setEditable( true );
		materialEdit->setInsertPolicy( QComboBox::NoInsert );
		materialEdit->lineEdit()->setPlaceholderText( tr( "Search material name or enter a custom name/CRC" ) );
		QMap<QString, QPair<quint32, QString>> createMaterials;
		const auto & createMaterialOptions = NifValue::enumOptionData( materialEnumType() ).o;
		for ( auto it = createMaterialOptions.cbegin(); it != createMaterialOptions.cend(); ++it )
			createMaterials.insert( knownMaterialText( it.key() ), qMakePair( it.key(), it.value().first ) );
		for ( auto it = createMaterials.cbegin(); it != createMaterials.cend(); ++it ) {
			materialEdit->addItem( it.key(), it.value().second );
			int row = materialEdit->count() - 1;
			materialEdit->setItemData( row, it.value().first, Qt::UserRole + 1 );
			materialEdit->setItemData( row, materialToolTip( it.value().first ), Qt::ToolTipRole );
		}
		const QVariantMap createCustomMaterials = customMaterials();
		for ( auto it = createCustomMaterials.cbegin(); it != createCustomMaterials.cend(); ++it ) {
			if ( materialEdit->findData( it.value().toUInt(), Qt::UserRole + 1 ) >= 0 ) continue;
			materialEdit->addItem( it.key(), QStringLiteral( "0x%1" ).arg( it.value().toUInt(), 8, 16, QLatin1Char( '0' ) ) );
			int row = materialEdit->count() - 1;
			materialEdit->setItemData( row, it.value().toUInt(), Qt::UserRole + 1 );
			materialEdit->setItemData( row, materialToolTip( it.value().toUInt() ), Qt::ToolTipRole );
		}
		if ( QCompleter * completer = materialEdit->completer() ) {
			completer->setCaseSensitivity( Qt::CaseInsensitive );
			completer->setFilterMode( Qt::MatchContains );
			completer->setCompletionMode( QCompleter::PopupCompletion );
		}
		QString savedMaterial = createSettings.value( "CollisionManager/Create/Material", "MaterialMetalSolid" ).toString().trimmed();
		int savedMaterialRow = materialEdit->findData( savedMaterial );
		bool savedIsNumber = false;
		quint32 savedMaterialValue = savedMaterial.toUInt( &savedIsNumber, 0 );
		if ( savedMaterialRow < 0 && savedIsNumber )
			savedMaterialRow = materialEdit->findData( savedMaterialValue, Qt::UserRole + 1 );
		if ( savedMaterialRow >= 0 ) materialEdit->setCurrentIndex( savedMaterialRow );
		else materialEdit->setEditText( savedMaterial );
		auto * replace = new QCheckBox( tr( "Replace" ), createGroup );
		replace->setChecked( createSettings.value( "CollisionManager/Create/Replace", true ).toBool() );
		createButton = new QPushButton( tr( "Create Collision" ), createGroup );
		decimateButton = new QPushButton( tr( "Optimize Source Mesh..." ), createGroup );
		decimateButton->setToolTip( tr( "Open the live decimation preview for the selected render mesh" ) );
		createLayout->addWidget( sourceLabel, 0, 0, 1, 2 );
		createLayout->addLayout( shapeButtons, 1, 0, 1, 2 );
		auto * presetLine = new QHBoxLayout;
		presetLine->addWidget( preset, 1 );
		presetLine->addWidget( savePreset );
		auto * convexMethodLabel = new QLabel( tr( "Convex method" ), createGroup );
		createLayout->addWidget( convexMethodLabel, 2, 0 );
		createLayout->addWidget( convexMethod, 2, 1 );
		createLayout->addWidget( new QLabel( tr( "Preset" ), createGroup ), 3, 0 );
		createLayout->addLayout( presetLine, 3, 1 );
		auto * defaultsToggle = new QToolButton( createGroup );
		defaultsToggle->setText( tr( "New collision defaults" ) );
		defaultsToggle->setCheckable( true );
		defaultsToggle->setChecked( createSettings.value( "CollisionManager/Create/DefaultsExpanded", false ).toBool() );
		defaultsToggle->setArrowType( defaultsToggle->isChecked() ? Qt::DownArrow : Qt::RightArrow );
		defaultsToggle->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		/* Two material pickers live in this dock and neither said which was which.
		 * This one is a DEFAULT for whatever Create makes next; the other, in the
		 * selected-body editor, edits the body you have selected right now.
		 */
		auto * materialLabel = new QLabel( tr( "Material for new collision" ), createGroup );
		auto * materialLineWidget = new QWidget( createGroup );
		auto * materialLine = new QHBoxLayout( materialLineWidget );
		materialLine->setContentsMargins( 0, 0, 0, 0 );
		materialLine->addWidget( materialEdit, 1 );
		materialLine->addWidget( replace );
		createLayout->addWidget( defaultsToggle, 4, 0, 1, 2 );
		createLayout->addWidget( materialLabel, 5, 0 );
		createLayout->addWidget( materialLineWidget, 5, 1 );
		materialLabel->setVisible( defaultsToggle->isChecked() );
		materialLineWidget->setVisible( defaultsToggle->isChecked() );
		connect( defaultsToggle, &QToolButton::toggled, this,
			[defaultsToggle, materialLabel, materialLineWidget]( bool expanded ) {
				defaultsToggle->setArrowType( expanded ? Qt::DownArrow : Qt::RightArrow );
				materialLabel->setVisible( expanded );
				materialLineWidget->setVisible( expanded );
				QSettings().setValue( "CollisionManager/Create/DefaultsExpanded", expanded );
			} );
		auto * createActions = new QHBoxLayout;
		createActions->addWidget( decimateButton );
		createActions->addWidget( createButton );
		createLayout->addLayout( createActions, 6, 0, 1, 2 );
		/* Create and Test share the bottom of the panel, one at a time.
		 *
		 * They are the two things you do with collision once you can see it --
		 * author it, then throw something at it -- and they are never wanted at
		 * the same moment. Side by side they would each get half the width and the
		 * panel would be twice as tall as a dock usefully is; stacked, whichever
		 * one you are not using is scrolled past on every trip. A switch costs one
		 * row and removes both problems.
		 */
		auto * bottomSwitch = new QWidget( this );
		auto * bottomSwitchLayout = new QHBoxLayout( bottomSwitch );
		bottomSwitchLayout->setContentsMargins( 0, 0, 0, 0 );
		bottomSwitchLayout->setSpacing( 0 );
		auto * bottomGroup = new QButtonGroup( bottomSwitch );
		bottomGroup->setExclusive( true );
		for ( int i = 0; i < 2; i++ ) {
			auto * b = new QToolButton( bottomSwitch );
			b->setText( i == 0 ? tr( "Collision Creation" ) : tr( "Collision Simulation" ) );
			b->setToolTip( i == 0
				? tr( "Build collision for the selected mesh" )
				: tr( "Simulate this file's ragdoll: drop it, drag it, shoot at it" ) );
			b->setCheckable( true );
			b->setMinimumHeight( 26 );
			b->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
			bottomGroup->addButton( b, i );
			bottomSwitchLayout->addWidget( b, 1 );
		}
		// the same checked-button styling the shape picker above uses, so two
		// segmented controls in one panel do not look like different mechanisms
		bottomSwitch->setStyleSheet( createGroup->styleSheet() );
		root->addWidget( bottomSwitch );
		root->addWidget( createGroup );

		/* The simulator, in its full form. The toolbar dropdown carries the same
		 * class in its Essentials mode -- one implementation in two sizes, so the
		 * two cannot come to disagree about what a control does.
		 */
		testPanel = new PhysicsSimPanel( ogl, nif, PhysicsSimPanel::Mode::Full,
			nullptr, this );
		testPanel->setObjectName( QStringLiteral( "CollisionTestPanel" ) );
		root->addWidget( testPanel );

		connect( bottomGroup, &QButtonGroup::idClicked, this,
			[this, createGroup]( int id ) {
				createGroup->setVisible( id == 0 );
				testPanel->setVisible( id == 1 );
				if ( id == 1 ) {
					testPanel->sync();
					refreshSimPins();
				}
				QSettings().setValue( "CollisionManager/BottomSection", id );
			} );
		// remembered, because which of the two you want is a property of the job
		// you are doing rather than of the session
		const int bottomSection = QSettings().value( "CollisionManager/BottomSection", 0 ).toInt() == 1 ? 1 : 0;
		bottomGroup->button( bottomSection )->setChecked( true );
		createGroup->setVisible( bottomSection == 0 );
		testPanel->setVisible( bottomSection == 1 );

		physicsGroup = new QGroupBox( tr( "Selected collision properties" ), this );
		auto * physicsOuter = new QVBoxLayout( physicsGroup );
		emptyPhysicsWidget = new QLabel( tr( "Select a collision row to inspect or edit it." ), physicsGroup );
		compiledSummaryWidget = new QWidget( physicsGroup );
		auto * compiledSummaryLayout = new QHBoxLayout( compiledSummaryWidget );
		compiledSummaryLayout->setContentsMargins( 0, 0, 0, 0 );
		compiledSummaryLabel = new QLabel( compiledSummaryWidget );
		compiledSummaryLabel->setWordWrap( true );
		auto * decompilePhysicsButton = new QPushButton( tr( "Decompile to Edit Physics" ), compiledSummaryWidget );
		compiledSummaryLayout->addWidget( compiledSummaryLabel, 1 );
		compiledSummaryLayout->addWidget( decompilePhysicsButton );
		physicsEditorBody = new QWidget( physicsGroup );
		auto * form = new QGridLayout( physicsEditorBody );
		form->setContentsMargins( 0, 0, 0, 0 );
		form->setColumnStretch( 1, 1 );
		form->setColumnStretch( 3, 1 );
		mass = new QDoubleSpinBox( physicsGroup );
		mass->setRange( 0.0, 1000000.0 ); mass->setDecimals( 3 );
		friction = new QDoubleSpinBox( physicsGroup );
		friction->setRange( 0.0, 10.0 ); friction->setDecimals( 3 );
		restitution = new QDoubleSpinBox( physicsGroup );
		restitution->setRange( 0.0, 1.0 ); restitution->setDecimals( 3 );
		layer = new QComboBox( physicsGroup );
		material = new QComboBox( physicsGroup );
		configureSearchableSelector( layer, tr( "Search collision layers" ) );
		configureSearchableSelector( material, tr( "Search materials" ) );
		linearDamping = new QDoubleSpinBox( physicsGroup );
		linearDamping->setRange( 0.0, 100.0 ); linearDamping->setDecimals( 3 );
		angularDamping = new QDoubleSpinBox( physicsGroup );
		angularDamping->setRange( 0.0, 100.0 ); angularDamping->setDecimals( 3 );
		maxLinearVelocity = new QDoubleSpinBox( physicsGroup );
		maxLinearVelocity->setRange( 0.0, 1000000.0 ); maxLinearVelocity->setDecimals( 2 );
		maxAngularVelocity = new QDoubleSpinBox( physicsGroup );
		maxAngularVelocity->setRange( 0.0, 1000000.0 ); maxAngularVelocity->setDecimals( 2 );
		motionSystem = new QComboBox( physicsGroup );
		qualityType = new QComboBox( physicsGroup );
		solverDeactivation = new QComboBox( physicsGroup );
		deactivatorType = new QComboBox( physicsGroup );
		populatePhysicsEnums();
		int dynamicRow = motionSystem->findData( 3 );
		if ( dynamicRow >= 0 ) motionSystem->setItemText( dynamicRow, tr( "Dynamic (3)" ) );
		int staticRow = motionSystem->findData( 5 );
		if ( staticRow >= 0 ) motionSystem->setItemText( staticRow, tr( "Static (5)" ) );
		auto * materialEditor = new QWidget( physicsGroup );
		auto * materialEditorLayout = new QHBoxLayout( materialEditor );
		materialEditorLayout->setContentsMargins( 0, 0, 0, 0 );
		materialEditorLayout->setSpacing( 3 );
		materialEditorLayout->addWidget( material, 1 );
		auto * addMaterial = new QToolButton( materialEditor );
		addMaterial->setText( QStringLiteral( "+" ) );
		addMaterial->setToolTip( tr( "Add a custom material name and numeric value" ) );
		materialEditorLayout->addWidget( addMaterial );
		physicsHint = new QLabel( tr( "Physics values appear here." ), physicsGroup );
		physicsHint->setWordWrap( true );
		auto addPhysicsRow = [form, this]( int row, const QString & leftLabel, QWidget * left,
			const QString & rightLabel, QWidget * right ) {
			form->addWidget( new QLabel( leftLabel, physicsGroup ), row, 0 );
			form->addWidget( left, row, 1 );
			form->addWidget( new QLabel( rightLabel, physicsGroup ), row, 2 );
			form->addWidget( right, row, 3 );
		};
		addPhysicsRow( 0, tr( "Collision layer" ), layer,
			tr( "Material of this body" ), materialEditor );
		addPhysicsRow( 1, tr( "Mass" ), mass, tr( "Motion" ), motionSystem );
		addPhysicsRow( 2, tr( "Friction" ), friction, tr( "Quality" ), qualityType );
		addPhysicsRow( 3, tr( "Restitution" ), restitution, tr( "Solver deact." ), solverDeactivation );
		addPhysicsRow( 4, tr( "Lin. damping" ), linearDamping, tr( "Max lin. vel." ), maxLinearVelocity );
		addPhysicsRow( 5, tr( "Ang. damping" ), angularDamping, tr( "Max ang. vel." ), maxAngularVelocity );
		auto * massFromMaterial = new QPushButton( tr( "Mass from Material..." ), physicsGroup );
		massFromMaterial->setToolTip( tr( "Calculate mass from collision volume and a suggested material density" ) );
		form->addWidget( massFromMaterial, 6, 0, 1, 4 );
		form->addWidget( physicsHint, 7, 0, 1, 4 );
		physicsOuter->addWidget( emptyPhysicsWidget );
		physicsOuter->addWidget( compiledSummaryWidget );
		physicsOuter->addWidget( physicsEditorBody );
		emptyPhysicsWidget->show();
		compiledSummaryWidget->hide();
		physicsEditorBody->hide();
		root->addWidget( physicsGroup );

		advancedSection = new QWidget( this );
		auto * advancedOuter = new QVBoxLayout( advancedSection );
		advancedOuter->setContentsMargins( 0, 0, 0, 0 );
		auto * advancedToggle = new QToolButton( advancedSection );
		advancedToggle->setText( tr( "Advanced physics and collision group" ) );
		advancedToggle->setCheckable( true );
		advancedToggle->setChecked( settings.value( "CollisionManager/AdvancedExpanded", false ).toBool() );
		advancedToggle->setArrowType( advancedToggle->isChecked() ? Qt::DownArrow : Qt::RightArrow );
		advancedToggle->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		advancedBody = new QWidget( advancedSection );
		auto * advancedGroup = advancedBody;
		auto * advanced = new QGridLayout( advancedBody );
		advanced->setContentsMargins( 6, 3, 6, 3 );
		advancedOuter->addWidget( advancedToggle, 0, Qt::AlignLeft );
		advancedOuter->addWidget( advancedBody );
		advancedBody->setVisible( advancedToggle->isChecked() );
		connect( advancedToggle, &QToolButton::toggled, this, [advancedToggle, this]( bool expanded ) {
			advancedToggle->setArrowType( expanded ? Qt::DownArrow : Qt::RightArrow );
			advancedBody->setVisible( expanded );
			QSettings().setValue( "CollisionManager/AdvancedExpanded", expanded );
		} );
		keyframed = new QCheckBox( tr( "Keyframed" ), advancedGroup );
		linkedGroup = new QCheckBox( tr( "Linked Group" ), advancedGroup );
		collisionWithinGroup = new QCheckBox( tr( "Collision within Group" ), advancedGroup );
		wind = new QCheckBox( tr( "Wind" ), advancedGroup );
		phantom = new QCheckBox( tr( "Phantom" ), advancedGroup );
		shapePhantom = new QCheckBox( tr( "Shape Phantom" ), advancedGroup );
		phantom->setEnabled( false ); shapePhantom->setEnabled( false );
		phantom->setToolTip( tr( "Requires conversion to a bhkPCollisionObject phantom graph; compiled hknp phantom encoding is not validated yet" ) );
		shapePhantom->setToolTip( tr( "Requires bhkSPCollisionObject and bhkSimpleShapePhantom encoding" ) );
		filterGroup = new QSpinBox( advancedGroup ); filterGroup->setRange( 0, 65535 );
		penetrationDepth = new QDoubleSpinBox( advancedGroup ); penetrationDepth->setRange( 0.0, 100000.0 ); penetrationDepth->setDecimals( 4 );
		centerX = new QDoubleSpinBox( advancedGroup ); centerY = new QDoubleSpinBox( advancedGroup ); centerZ = new QDoubleSpinBox( advancedGroup );
		inertiaX = new QDoubleSpinBox( advancedGroup ); inertiaY = new QDoubleSpinBox( advancedGroup ); inertiaZ = new QDoubleSpinBox( advancedGroup );
		for ( QDoubleSpinBox * spin : { centerX, centerY, centerZ } ) { spin->setRange( -1000000.0, 1000000.0 ); spin->setDecimals( 4 ); }
		for ( QDoubleSpinBox * spin : { inertiaX, inertiaY, inertiaZ } ) { spin->setRange( 0.0, 1000000000.0 ); spin->setDecimals( 5 ); }
		auto * centerRow = new QWidget( advancedGroup ); auto * centerLayout = new QHBoxLayout( centerRow ); centerLayout->setContentsMargins( 0, 0, 0, 0 );
		centerLayout->addWidget( new QLabel( tr( "X" ), centerRow ) ); centerLayout->addWidget( centerX ); centerLayout->addWidget( new QLabel( tr( "Y" ), centerRow ) ); centerLayout->addWidget( centerY ); centerLayout->addWidget( new QLabel( tr( "Z" ), centerRow ) ); centerLayout->addWidget( centerZ );
		auto * inertiaRow = new QWidget( advancedGroup ); auto * inertiaLayout = new QHBoxLayout( inertiaRow ); inertiaLayout->setContentsMargins( 0, 0, 0, 0 );
		inertiaLayout->addWidget( new QLabel( tr( "X" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaX ); inertiaLayout->addWidget( new QLabel( tr( "Y" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaY ); inertiaLayout->addWidget( new QLabel( tr( "Z" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaZ );
		advanced->addWidget( keyframed, 0, 0 ); advanced->addWidget( linkedGroup, 0, 1 );
		advanced->addWidget( collisionWithinGroup, 1, 0 ); advanced->addWidget( wind, 1, 1 );
		advanced->addWidget( phantom, 2, 0 ); advanced->addWidget( shapePhantom, 2, 1 );
		advanced->addWidget( new QLabel( tr( "Collision Filter Group" ), advancedGroup ), 3, 0 ); advanced->addWidget( filterGroup, 3, 1 );
		advanced->addWidget( new QLabel( tr( "C.O.M. (local)" ), advancedGroup ), 4, 0 ); advanced->addWidget( centerRow, 4, 1 );
		advanced->addWidget( new QLabel( tr( "Inertia tensor diagonal" ), advancedGroup ), 5, 0 ); advanced->addWidget( inertiaRow, 5, 1 );
		advanced->addWidget( new QLabel( tr( "Allowed penetration" ), advancedGroup ), 6, 0 ); advanced->addWidget( penetrationDepth, 6, 1 );
		advanced->addWidget( new QLabel( tr( "Deactivator Type" ), advancedGroup ), 7, 0 ); advanced->addWidget( deactivatorType, 7, 1 );
		root->addWidget( advancedSection );

		summary = new QLabel( this );
		summary->setWordWrap( true );
		summary->setStyleSheet( QStringLiteral( "QLabel { color: %1; padding: 3px; }" ).arg( wwSkinColor( "textMuted" ) ) );
		root->addWidget( summary );

		connect( tree, &QTreeWidget::currentItemChanged, this, [this]( QTreeWidgetItem * current ) {
			updateDetails();
			if ( current ) {
				if ( auto * w = dynamic_cast<NifSkope *>( mw ) ) {
					int block = current->parent() && current->data( 0, ShapeBlockRole ).toInt() >= 0
						? current->data( 0, ShapeBlockRole ).toInt()
						: current->data( 0, ObjectBlockRole ).toInt();
					syncingSelection = true;
					w->select( blockIndex( block ) );
					syncingSelection = false;
				}
			}
		} );
		connect( tree, &QTreeWidget::customContextMenuRequested, this, [this]( const QPoint & pos ) {
			QTreeWidgetItem * item = tree->itemAt( pos );
			if ( !item ) return;
			tree->setCurrentItem( item );
			const bool compiled = item->data( 0, CompiledRole ).toBool();
			QMenu menu( this );
			QAction * decompile = menu.addAction( tr( "Decompile Selected" ) );
			decompile->setEnabled( compiled );
			QAction * decompileAllAction = menu.addAction( tr( "Decompile All" ) );
			QAction * compileAction = menu.addAction( tr( "Compile Selected" ) );
			compileAction->setEnabled( !compiled );
			menu.addSeparator();
			QAction * check = menu.addAction( tr( "Check Collision" ) );
			QAction * makeRenderProxy = menu.addAction( tr( "Create Editable Mesh Copy" ) );
			QAction * massFromMaterialAction = menu.addAction( tr( "Mass from Material..." ) );
			massFromMaterialAction->setEnabled( !compiled );
			QAction * expand = menu.addAction( item->isExpanded() ? tr( "Collapse Shapes" ) : tr( "Expand Shapes" ) );
			expand->setEnabled( item->childCount() > 0 );
			QAction * selectBlock = menu.addAction( tr( "Select in Block List" ) );
			menu.addSeparator();
			QAction * copyMaterial = menu.addAction( tr( "Copy Material Name and Value" ) );
			QAction * useMaterial = menu.addAction( tr( "Use Material for New Collision" ) );
			QAction * refreshAction = menu.addAction( tr( "Refresh" ) );
			QAction * chosen = menu.exec( tree->viewport()->mapToGlobal( pos ) );
			if ( chosen == decompile )
				runSpell( QStringLiteral( "Havok/Decompile Compiled Collision" ),
					blockIndex( item->data( 0, ObjectBlockRole ).toInt() ) );
			else if ( chosen == decompileAllAction )
				runSpell( QStringLiteral( "Havok/Decompile All Compiled Collision" ), QModelIndex() );
			else if ( chosen == compileAction ) compileSelectedCollision();
			else if ( chosen == check ) lintCollision();
			else if ( chosen == makeRenderProxy ) collisionToBSTriShape();
			else if ( chosen == massFromMaterialAction ) calculateMassFromMaterial();
			else if ( chosen == expand ) item->setExpanded( !item->isExpanded() );
			else if ( chosen == selectBlock ) {
				if ( auto * w = dynamic_cast<NifSkope *>( mw ) ) {
					int block = item->parent() && item->data( 0, ShapeBlockRole ).toInt() >= 0
						? item->data( 0, ShapeBlockRole ).toInt() : item->data( 0, ObjectBlockRole ).toInt();
					w->select( blockIndex( block ) );
				}
			} else if ( chosen == copyMaterial ) {
				quint32 value = material->currentData().toUInt();
				QGuiApplication::clipboard()->setText( QStringLiteral( "%1 (0x%2)" )
					.arg( material->currentText() )
					.arg( QStringLiteral( "%1" ).arg( value, 8, 16, QLatin1Char( '0' ) ).toUpper() ) );
			} else if ( chosen == useMaterial ) {
				quint32 value = material->currentData().toUInt();
				QSettings s; s.setValue( "CollisionManager/Create/Material",
					QStringLiteral( "0x%1" ).arg( value, 8, 16, QLatin1Char( '0' ) ) );
			} else if ( chosen == refreshAction ) rebuild();
		} );
		connect( addMaterial, &QToolButton::clicked, this, [this]() {
			bool ok = false;
			QString name = QInputDialog::getText( this, tr( "Add Custom Collision Material" ),
				tr( "Material name:" ), QLineEdit::Normal, QString(), &ok ).trimmed();
			if ( !ok || name.isEmpty() ) return;
			QString valueText = QInputDialog::getText( this, tr( "Add Custom Collision Material" ),
				tr( "Numeric value (decimal or 0x hexadecimal):" ), QLineEdit::Normal,
				QStringLiteral( "0x00000000" ), &ok ).trimmed();
			if ( !ok ) return;
			quint32 value = valueText.toUInt( &ok, 0 );
			if ( !ok ) {
				QMessageBox::warning( this, tr( "Custom Collision Material" ),
					tr( "'%1' is not a valid 32-bit material value." ).arg( valueText ) );
				return;
			}
			QSettings settings;
			QVariantMap custom = settings.value( "CollisionManager/CustomMaterials" ).toMap();
			custom.insert( name, value );
			settings.setValue( "CollisionManager/CustomMaterials", custom );
			int row = material->findData( value );
			if ( row < 0 ) { material->addItem( name, value ); row = material->count() - 1; }
			else if ( !NifValue::enumOptionData( materialEnumType() ).o.contains( value ) ) material->setItemText( row, name );
			material->setCurrentIndex( row );
		} );
		/* Ticking a row pins that body. Guarded by `updating`, which
		 * refreshSimPins() raises while it writes the check states -- without it
		 * every refresh would read as a user click and toggle what it just set.
		 */
		connect( tree, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem * item, int column ) {
			if ( updating || column != 0 || !ogl || !item )
				return;
			PhysicsPreview & pv = ogl->physicsSim();
			bool ok = false;
			const int body = item->data( 0, BodyIdRole ).toInt( &ok );
			if ( !ok || !pv.active() || body < 0 || body >= pv.bodyCount() )
				return;
			if ( item->data( 0, SystemBlockRole ).toInt() != pv.systemBlock() )
				return;
			pv.sim().setPinned( body, item->checkState( 0 ) == Qt::Checked );
			ogl->update();
		} );
		// entering or leaving the mode changes whether the pins mean anything
		connect( ogl, &GLView::physicsSimModeChanged, this,
			[this]( bool ) { refreshSimPins(); } );

		connect( tree, &QTreeWidget::itemExpanded, this, [this]( QTreeWidgetItem * item ) {
			if ( !item->parent() ) expandedObjects.insert( item->data( 0, ObjectBlockRole ).toInt() );
		} );
		connect( tree, &QTreeWidget::itemCollapsed, this, [this]( QTreeWidgetItem * item ) {
			if ( !item->parent() ) expandedObjects.remove( item->data( 0, ObjectBlockRole ).toInt() );
		} );
		connect( decompileSelectedAction, &QAction::triggered, this, [this]() {
			if ( tree->currentItem() )
				runSpell( QStringLiteral( "Havok/Decompile Compiled Collision" ),
					blockIndex( tree->currentItem()->data( 0, ObjectBlockRole ).toInt() ) );
		} );
		connect( decompileAllAction, &QAction::triggered, this, [this]() {
			runSpell( QStringLiteral( "Havok/Decompile All Compiled Collision" ), QModelIndex() );
		} );
		connect( compileSelectedAction, &QAction::triggered, this, [this]() { compileSelectedCollision(); } );
		connect( decompilePhysicsButton, &QPushButton::clicked, decompileSelectedAction, &QAction::trigger );
		connect( refreshAction, &QAction::triggered, this, [this]() { rebuild(); } );
		connect( lint, &QPushButton::clicked, this, [this]() { lintCollision(); } );
		connect( donor, &QPushButton::clicked, this, [this]() { importDonorCollision(); } );
		connect( reverseAction, &QAction::triggered, this, [this]() { collisionToBSTriShape(); } );
		connect( massFromMaterial, &QPushButton::clicked, this, [this]() { calculateMassFromMaterial(); } );
		connect( shapeGroup, &QButtonGroup::idToggled, this,
			[convexMethod, convexMethodLabel, this]( int id, bool checked ) {
				if ( checked ) {
					convexMethod->setVisible( id == 3 );
					convexMethodLabel->setVisible( id == 3 );
					decimateButton->setVisible( id == 3 || id == 4 );
				}
			} );
		auto saveCreationSettings = [preset, materialEdit, replace, convexMethod]() {
			QSettings settings;
			int presetId = preset->currentData().toInt();
			settings.setValue( "CollisionManager/Create/Preset", presetId );
			int createLayer = presetId == 0 ? 1 : presetId == 1 ? 10
				: presetId == 3 ? 2 : presetId == 4 ? 31
				: settings.value( "CollisionManager/Create/Layer", 10 ).toInt();
			settings.setValue( "CollisionManager/Create/Layer", createLayer );
			QString materialValue = materialEdit->currentText().trimmed();
			int materialRow = materialEdit->findText( materialValue, Qt::MatchFixedString );
			if ( materialRow >= 0 ) materialValue = materialEdit->itemData( materialRow ).toString();
			settings.setValue( "CollisionManager/Create/Material", materialValue );
			settings.setValue( "CollisionManager/Create/Replace", replace->isChecked() );
			settings.setValue( "CollisionManager/Create/ConvexMethod", convexMethod->currentIndex() );
			settings.beginGroup( "Spells/Havok/Create Convex Shapes" );
			settings.setValue( "Replace Shape", replace->isChecked() );
			settings.endGroup();
		};
		connect( savePreset, &QToolButton::clicked, this, [saveCreationSettings]() { saveCreationSettings(); } );
		connect( createButton, &QPushButton::clicked, this, [this, shapeGroup, convexMethod, saveCreationSettings]() {
			saveCreationSettings();
			int mode = std::clamp( shapeGroup->checkedId(), 0, 4 );
			if ( mode == 3 ) { showCollisionPreview( 0, 100.0, convexMethod->currentIndex() == 1 ); return; }
			if ( mode == 4 ) { showCollisionPreview( 1, 100.0 ); return; }
			QString spellId;
			if ( mode == 0 ) spellId = QStringLiteral( "Havok/Create Box Collision" );
			else if ( mode == 1 ) spellId = QStringLiteral( "Havok/Create Sphere Collision" );
			else if ( mode == 2 ) spellId = QStringLiteral( "Havok/Create Capsule Collision" );
			runSpell( spellId, currentSource() );
		} );
		connect( decimateButton, &QPushButton::clicked, this, [this]() {
			showCollisionPreview( 1, 50.0 );
		} );
		for ( QDoubleSpinBox * spin : { mass, friction, restitution, linearDamping,
			angularDamping, maxLinearVelocity, maxAngularVelocity, centerX, centerY, centerZ,
			inertiaX, inertiaY, inertiaZ, penetrationDepth } )
			connect( spin, &QDoubleSpinBox::editingFinished, this, [this]() { applyPhysics(); } );
		connect( layer, qOverload<int>( &QComboBox::currentIndexChanged ), this,
			[this]( int row ) { applyLayerSelection( row ); } );
		connect( material, qOverload<int>( &QComboBox::currentIndexChanged ), this,
			[this]( int row ) { applyMaterialSelection( row ); } );
		for ( QComboBox * combo : { motionSystem, qualityType, solverDeactivation, deactivatorType } )
			connect( combo, qOverload<int>( &QComboBox::currentIndexChanged ), this,
				[this]( int ) { applyPhysics(); } );
		for ( QCheckBox * check : { keyframed, linkedGroup, collisionWithinGroup, wind } )
			connect( check, &QCheckBox::toggled, this, [this]( bool ) { applyPhysics(); } );
		connect( filterGroup, &QSpinBox::editingFinished, this, [this]() { applyPhysics(); } );
		connect( linkedGroup, &QCheckBox::toggled, collisionWithinGroup, &QWidget::setEnabled );
	}

	void connectModel()
	{
		connect( nif, &QAbstractItemModel::modelReset, this, [this]() {
			// The manager is constructed before a file is loaded. Rebuild all
			// version-specific enums now that BS Version is known.
			populatePhysicsEnums();
			queueRebuild();
		} );
		connect( nif, &QAbstractItemModel::rowsInserted, this,
			[this]( const QModelIndex &, int, int ) { queueRebuild(); } );
		connect( nif, &QAbstractItemModel::rowsRemoved, this,
			[this]( const QModelIndex &, int, int ) { queueRebuild(); } );
		connect( nif, &QAbstractItemModel::dataChanged, this,
			[this]( const QModelIndex &, const QModelIndex &, const QList<int> & ) { queueRebuild(); } );
		connect( nif, &NifModel::linksChanged, this, [this]() { queueRebuild(); } );
		connect( dock, &QDockWidget::visibilityChanged, this, [this]( bool visible ) {
			if ( visible ) rebuild(); else clearCollisionPreview();
		} );
		if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) {
			connect( window, &NifSkope::completeLoading, this, [this]() { clearCollisionPreview(); } );
			// Selecting a bone anywhere - Skeleton Manager, block list, viewport -
			// highlights that bone's collision body here. The bone is the node the
			// collision object targets, which is already on every row.
			connect( window, &NifSkope::currentNifIndexChanged, this,
				[this]( const QModelIndex & index ) {
					if ( syncingSelection || updating || !dock->isVisible() || !nif )
						return;
					selectRowForBlock( nif->getBlockNumber( nif->getBlockIndex( index ) ) );
				} );
		}
	}
};

} // namespace

QDockWidget * tlCreateCollisionManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * dock = new QDockWidget( QObject::tr( "Collision Manager" ), mw );
	dock->setObjectName( QStringLiteral( "CollisionManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
	auto * scroll = new QScrollArea( dock );
	scroll->setObjectName( QStringLiteral( "CollisionManagerScrollArea" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	scroll->setWidget( new CollisionManagerPanel( nif, mw, ogl, dock ) );
	dock->setWidget( scroll );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}


/*! The collision lint, whole file, reported through logMessage.
 *
 *  The Collision Manager's Check button has done these tests for a while, but it
 *  ends in a QMessageBox and it counts rather than locates — "3 dangling collision
 *  reference(s)" tells you the file is wrong and not which block to open. Both
 *  make it useless to the Unfuck panel, which casts checks with an invalid index,
 *  drains logMessage, and offers Go to on whatever block a finding names.
 *
 *  So this reports ONE FINDING PER BLOCK, with the block number, which is the
 *  part that had to be rewritten rather than hoisted. The detection is the same
 *  walk, over the same helpers, now at file scope.
 *
 *  SEVERITY IS MEASURED, NOT ASSUMED — see spCheckAllMaterials in meshtools.cpp
 *  for how that went the first time. A dangling reference is a broken file. A
 *  box-like hull or an uncollided visible mesh is an observation: effect meshes
 *  and decorative geometry legitimately have no collision, so reporting those as
 *  warnings would put amber rows on most of the FO4 effects tree.
 */
class spCheckCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Collision Problems" ); }
	QString page() const override final { return Spell::tr( "Error Checking" ); }
	bool constant() const override final { return true; }
	bool checker() const override final { return true; }
	static QString message() { return Spell::tr( "Collision problems were found." ); }

	// cheap: SpellBook::checkers() is cast per-file by the bulk directory scan
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return !index.isValid() && nif && nif->getBlockCount() > 0;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final
	{
		auto say = [nif]( int b, const QString & what, QMessageBox::Icon lvl ) {
			nif->logMessage( message(), Spell::tr( "[%1] %2" ).arg( b ).arg( what ), lvl );
		};

		int compiled = 0, editable = 0;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );

			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				compiled++;
				if ( !tlCollBlockIndex( nif, nif->getLink( i, "Data" ) ).isValid() )
					say( b, Spell::tr( "Compiled collision object's Data reference is missing." ),
						QMessageBox::Critical );
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				editable++;
				const int body = nif->getLink( i, "Body" );
				const QModelIndex bi = tlCollBlockIndex( nif, body );
				if ( !bi.isValid() ) {
					say( b, Spell::tr( "Collision object's Body reference is missing." ),
						QMessageBox::Critical );
					continue;			// nothing below can be read without the body
				}

				const int leaf = tlCollFirstLeafShape( nif, nif->getLink( bi, "Shape" ) );
				const QModelIndex li = tlCollBlockIndex( nif, leaf );
				if ( li.isValid() && nif->isNiBlock( li, "bhkConvexVerticesShape" ) ) {
					const QVector<Vector4> vertices = nif->getArray<Vector4>( li, "Vertices" );
					if ( vertices.size() > 64 )
						say( leaf, Spell::tr( "Convex hull has %1 vertices; Havok handles 64 well." )
							.arg( vertices.size() ), QMessageBox::Warning );
					if ( vertices.size() >= 8 ) {
						Vector3 mn( vertices.first()[0], vertices.first()[1], vertices.first()[2] ), mx( mn );
						for ( const Vector4 & v : vertices )
							for ( int axis = 0; axis < 3; axis++ ) {
								mn[axis] = std::min( mn[axis], v[axis] );
								mx[axis] = std::max( mx[axis], v[axis] );
							}
						const float tolerance =
							std::max( { mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2] } ) * 0.03f + 1.0e-5f;
						bool boxLike = true;
						for ( const Vector4 & v : vertices )
							for ( int axis = 0; axis < 3; axis++ )
								if ( std::min( std::fabs( v[axis] - mn[axis] ),
										std::fabs( v[axis] - mx[axis] ) ) > tolerance )
									boxLike = false;
						if ( boxLike )
							say( leaf, Spell::tr( "Convex hull is box-shaped; a Box collision would be cheaper." ),
								QMessageBox::Information );
					}
				}

				const QModelIndex info = nif->getIndex( bi, "Rigid Body Info" );
				const QModelIndex filter = info.isValid() ? nif->getIndex( info, "Havok Filter" ) : QModelIndex();
				const quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 0;
				if ( filter.isValid() && collLayer == 0 )
					say( nif->getBlockNumber( bi ),
						Spell::tr( "Collision layer is Unidentified (0)." ), QMessageBox::Warning );
				if ( collLayer == 31 && li.isValid()
					&& ( nif->isNiBlock( li, "bhkBoxShape" ) || nif->isNiBlock( li, "bhkSphereShape" )
						|| nif->isNiBlock( li, "bhkCapsuleShape" ) ) )
					say( nif->getBlockNumber( bi ),
						Spell::tr( "STAIRHELPER body contains no sloped shape." ), QMessageBox::Warning );
			}

			if ( nif->isNiBlock( i, "bhkTransformShape" ) || nif->isNiBlock( i, "bhkConvexTransformShape" ) ) {
				const QModelIndex child =
					tlCollBlockIndex( nif, tlCollFirstLeafShape( nif, nif->getLink( i, "Shape" ) ) );
				if ( child.isValid() && ( nif->isNiBlock( child, "bhkBoxShape" )
						|| nif->isNiBlock( child, "bhkSphereShape" )
						|| nif->isNiBlock( child, "bhkCapsuleShape" ) ) ) {
					Vector3 trans, scales; Matrix rot;
					nif->get<Matrix4>( i, "Transform" ).decompose( trans, rot, scales );
					const float lo = std::min( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					const float hi = std::max( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					if ( hi - lo > std::max( 1.0e-4f, hi * 0.001f ) )
						say( b, Spell::tr( "Primitive collision transform uses non-uniform scale; Havok ignores it." ),
							QMessageBox::Warning );
				}
			}

			if ( nif->blockInherits( i, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } )
					&& !( nif->get<quint32>( i, "Flags" ) & 1u ) ) {
				bool protectedByCollision = false;
				int owner = nif->getParent( b );
				for ( int depth = 0; nif->isValidBlockNumber( owner ) && depth < 32;
						depth++, owner = nif->getParent( owner ) ) {
					const QModelIndex node = tlCollBlockIndex( nif, owner );
					if ( nif->blockInherits( node, "NiNode" )
						&& nif->isValidBlockNumber( nif->getLink( node, "Collision Object" ) ) ) {
						protectedByCollision = true;
						break;
					}
				}
				if ( !protectedByCollision )
					say( b, Spell::tr( "Visible geometry has no collision on its node hierarchy." ),
						QMessageBox::Information );
			}
		}

		// whole-file, so no block to name
		if ( compiled && editable )
			nif->logMessage( message(), Spell::tr(
				"File mixes compiled and editable collision; finish editing, then compile consistently." ),
				QMessageBox::Warning );

		return QModelIndex();
	}
};

REGISTER_SPELL( spCheckCollision )


/*! The two repairs the Collision Manager's Check button offers, as spells.
 *
 *  They existed only inside CollisionManagerPanel::lintCollision, behind a modal
 *  whose "Apply Safe Fixes" button applied BOTH to the WHOLE FILE at once. That
 *  is not something the Unfuck panel can offer against one finding, and it is
 *  not what a user clicking one row means.
 *
 *  Split into two per-block spells, so a finding that names block 12 repairs
 *  block 12 and nothing else. The whole-file pass in the dock is unchanged.
 */
class spFixCollisionLayer final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Set Collision Layer from Motion" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString hint() const override final
	{
		return Spell::tr( "Replaces an Unidentified (0) collision layer with Props for a "
			"keyframed body, or Static for anything else." );
	}

	//! The Havok Filter of a body whose layer is Unidentified, or an invalid index.
	static QModelIndex zeroLayerFilter( const NifModel * nif, const QModelIndex & index )
	{
		if ( !nif )
			return QModelIndex();
		const QModelIndex body = nif->getBlockIndex( index );
		if ( !body.isValid() || !nif->blockInherits( body, "bhkRigidBody" ) )
			return QModelIndex();
		const QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		if ( !info.isValid() )
			return QModelIndex();
		const QModelIndex filter = nif->getIndex( info, "Havok Filter" );
		if ( !filter.isValid() || nif->get<quint32>( filter, "Layer" ) != 0 )
			return QModelIndex();
		return filter;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return zeroLayerFilter( nif, index ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QModelIndex filter = zeroLayerFilter( nif, index );
		if ( !filter.isValid() )
			return index;
		/* Motion System 3 is MO_SYS_KEYFRAMED: something the game moves, which
		 * belongs on Props (10). Everything else is scenery, which is Static (1).
		 * Same rule the dock's safe-fix pass uses -- this is that code, not a
		 * second opinion about it.
		 */
		const quint32 motion = nif->get<quint32>( filter.parent(), "Motion System" );
		nifSnapshotOp( nif, Spell::tr( "Set collision layer" ), [&]() {
			nif->set<quint32>( QModelIndex( filter ), "Layer", motion == 3 ? 10u : 1u );
		} );
		return index;
	}
};

REGISTER_SPELL( spFixCollisionLayer )

class spRemoveBrokenCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove Broken Collision Object" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString hint() const override final
	{
		return Spell::tr( "Deletes a collision object whose Body or Data block is missing. "
			"It cannot collide with anything as it stands." );
	}

	/* Destructive, and it says so if cast from a menu. The Unfuck panel's per-row
	 * button calls cast() directly and therefore does not confirm -- that button
	 * is labelled with what it does, the row above it names the block, and
	 * runFixAt snapshots the model so Ctrl+Z takes it back.
	 */
	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final
	{
		return Spell::tr( "Remove collision object [%1]? Its %2 block is missing, so it collides "
			"with nothing." ).arg( nif->getBlockNumber( index ) )
			.arg( nif->blockInherits( nif->getBlockIndex( index ), "bhkNPCollisionObject" )
				? QStringLiteral( "Data" ) : QStringLiteral( "Body" ) );
	}

	static bool isBroken( const NifModel * nif, const QModelIndex & index )
	{
		if ( !nif )
			return false;
		const QModelIndex i = nif->getBlockIndex( index );
		if ( nif->blockInherits( i, "bhkNPCollisionObject" ) )
			return !nif->isValidBlockNumber( nif->getLink( i, "Data" ) );
		if ( nif->blockInherits( i, "bhkCollisionObject" ) )
			return !nif->isValidBlockNumber( nif->getLink( i, "Body" ) );
		return false;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return isBroken( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		spRemoveBranch remove;
		remove.castIfApplicable( nif, nif->getBlockIndex( index ) );
		return QModelIndex();
	}
};

REGISTER_SPELL( spRemoveBrokenCollision )
