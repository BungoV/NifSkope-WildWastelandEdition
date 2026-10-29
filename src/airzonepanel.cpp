// lane AIRZONE1 (bungo 10-29): the Air zones band (src/airzonepanel.h)

#include "airzonepanel.h"

#include "airzone.h"
#include "esmweather.h"	// the game's Data folder: the default save root
#include "glview.h"
#include "wwskin.h"

#include "gl/cellairzone.h"
#include "ui/widgets/wwnumberfield.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kLabelW = 104;	// one label column down the band (the house style)
const char * const kShapes[5] = { "Box", "Cylinder", "Wedge", "Capsule", "Sphere" };

QString blocksText( const az::Volume & v )
{
	QStringList b;
	if ( v.gi.on )
		b << ( v.gi.threshold ? QStringLiteral( "gi %1-%2" ).arg( v.gi.rooms[0] ).arg( v.gi.rooms[1] ) : QStringLiteral( "gi %1" ).arg( v.gi.room ) );
	if ( v.fog.on )
		b << QStringLiteral( "fog" );
	if ( v.part.on )
		b << QStringLiteral( "particles" );
	return b.isEmpty() ? QStringLiteral( "-" ) : b.join( QStringLiteral( ", " ) );
}

} // namespace

AirZonePanel::AirZonePanel( QWidget * parent ) : QWidget( parent )
{
	setObjectName( QStringLiteral( "AirZonePanel" ) );
	buildUi();
	refresh();
	// a file edited outside (or saved by another provider): the list follows within a second, unless there is unsaved work
	auto * poll = new QTimer( this );
	poll->setInterval( 1000 );
	connect( poll, &QTimer::timeout, this, [this]() {
		if ( !nif || dirty || !isVisible() )
			return;
		quint64 v = 0;
		wwAirZoneFor( nif, &v );
		if ( v != seenVersion )
			refresh();
	} );
	poll->start();
}

void AirZonePanel::buildUi()
{
	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 4 );
	outer->addWidget( wwHeading( tr( "Air zones" ), this ) );

	// the cell and the list
	auto * top = new QGridLayout;
	top->setColumnStretch( 1, 1 );
	top->setColumnMinimumWidth( 0, kLabelW );
	int r = 0;
	top->addWidget( new QLabel( tr( "Show volumes" ), this ), r, 0 );
	showBox = new QCheckBox( this );
	showBox->setObjectName( QStringLiteral( "AirZoneShow" ) );
	showBox->setChecked( QSettings().value( QStringLiteral( "CellEditor/AirZoneShow" ), true ).toBool() );
	top->addWidget( showBox, r++, 1 );
	top->addWidget( new QLabel( tr( "Cell" ), this ), r, 0 );
	cellBox = new QComboBox( this );
	cellBox->setObjectName( QStringLiteral( "AirZoneCell" ) );
	wwMatchFieldStyle( cellBox );
	top->addWidget( cellBox, r++, 1 );
	outer->addLayout( top );

	list = new QTreeWidget( this );
	list->setObjectName( QStringLiteral( "AirZoneList" ) );
	list->setColumnCount( 3 );
	list->setHeaderLabels( { tr( "Name" ), tr( "Shape" ), tr( "Blocks" ) } );
	list->setRootIsDecorated( false );
	list->setUniformRowHeights( true );
	list->setMaximumHeight( 140 );
	list->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	list->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	list->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
	list->setStyleSheet( wwSelectionTreeQss() );
	outer->addWidget( list );
	auto * listBar = new QHBoxLayout;
	listBar->setSpacing( 4 );
	addBtn = new QPushButton( tr( "Add" ), this );
	dupBtn = new QPushButton( tr( "Duplicate" ), this );
	delBtn = new QPushButton( tr( "Delete" ), this );
	addBtn->setObjectName( QStringLiteral( "AirZoneAdd" ) );
	dupBtn->setObjectName( QStringLiteral( "AirZoneDuplicate" ) );
	delBtn->setObjectName( QStringLiteral( "AirZoneDelete" ) );
	for ( QPushButton * b : { addBtn, dupBtn, delBtn } ) {
		b->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
		listBar->addWidget( b );
	}
	listBar->addStretch( 1 );
	outer->addLayout( listBar );

	// the selected volume: one setting a row
	auto * form = new QGridLayout;
	form->setColumnStretch( 1, 1 );
	form->setColumnMinimumWidth( 0, kLabelW );
	form->setVerticalSpacing( 2 );
	r = 0;
	auto heading = [&]( const QString & t ) {
		form->addWidget( wwHeading( t, this ), r++, 0, 1, 2 );
	};
	auto label = [&]( const QString & t ) {
		auto * l = new QLabel( t, this );
		form->addWidget( l, r, 0 );
		return l;
	};
	auto number = [&]( const QString & t, const char * name, int block, double lo, double hi, int decimals,
		std::function<double( const az::Volume & )> get, std::function<void( az::Volume &, double )> set ) {
		Num n;
		n.label = label( t );
		n.field = new WwNumberField( this );
		n.field->setObjectName( QLatin1String( name ) );
		n.field->setRange( lo, hi );
		n.field->setDecimals( decimals );
		n.field->setKeyboardTracking( false );
		wwGuardWheel( n.field );
		form->addWidget( n.field, r++, 1 );
		n.block = block;
		n.get = std::move( get );
		n.set = std::move( set );
		nums.push_back( n );
	};
	auto vec3 = [&]( const QString & t, const char * name, double lo, double hi, int decimals, float ( az::Shape::* m )[3] ) {
		static const char * const axes[3] = { "X", "Y", "Z" };
		for ( int k = 0; k < 3; k++ )
			number( QStringLiteral( "%1 %2" ).arg( t, QLatin1String( axes[k] ) ),
				( QByteArray( name ) + axes[k] ).constData(), 0, lo, hi, decimals,
				[m, k]( const az::Volume & v ) { return double( ( v.shape.*m )[k] ); },
				[m, k]( az::Volume & v, double x ) { ( v.shape.*m )[k] = float( x ); } );
	};
	auto rgb = [&]( const QString & t, const char * name, int block, float * ( *at )( az::Volume & ) ) {
		static const char * const ch[3] = { "R", "G", "B" };
		for ( int k = 0; k < 3; k++ )
			number( QStringLiteral( "%1 %2" ).arg( t, QLatin1String( ch[k] ) ), ( QByteArray( name ) + ch[k] ).constData(), block, 0.0, 16.0, 3,
				[at, k]( const az::Volume & v ) { return double( at( const_cast<az::Volume &>( v ) )[k] ); },
				[at, k]( az::Volume & v, double x ) { at( v )[k] = float( x ); } );
	};

	heading( tr( "Volume" ) );
	label( tr( "Name" ) );
	nameEdit = new QLineEdit( this );
	nameEdit->setObjectName( QStringLiteral( "AirZoneName" ) );
	form->addWidget( nameEdit, r++, 1 );
	label( tr( "Shape" ) );
	shapeBox = new QComboBox( this );
	shapeBox->setObjectName( QStringLiteral( "AirZoneShape" ) );
	for ( const char * s : kShapes )
		shapeBox->addItem( tr( s ) );
	wwMatchFieldStyle( shapeBox );
	form->addWidget( shapeBox, r++, 1 );
	vec3( tr( "Location" ), "AirZoneCenter", -1e7, 1e7, 1, &az::Shape::center );
	vec3( tr( "Size" ), "AirZoneSize", 1.0, 1e6, 1, &az::Shape::size );
	vec3( tr( "Rotation" ), "AirZoneRotation", -360.0, 360.0, 1, &az::Shape::rotation );
	number( tr( "Edge" ), "AirZoneEdge", 0, 0.0, 1e5, 1,
		[]( const az::Volume & v ) { return double( v.edge ); }, []( az::Volume & v, double x ) { v.edge = float( x ); } );
	number( tr( "Priority" ), "AirZonePriority", 0, -1000.0, 1000.0, 0,
		[]( const az::Volume & v ) { return double( v.priority ); }, []( az::Volume & v, double x ) { v.priority = int( x ); } );

	heading( tr( "GI" ) );
	label( tr( "Room shapes" ) );
	giModeBox = new QComboBox( this );
	giModeBox->setObjectName( QStringLiteral( "AirZoneGiMode" ) );
	giModeBox->addItem( tr( "Replace the rooms named" ), QStringLiteral( "rooms" ) );
	giModeBox->addItem( tr( "Replace every shape in the cell" ), QStringLiteral( "all" ) );
	wwMatchFieldStyle( giModeBox );
	form->addWidget( giModeBox, r++, 1 );
	label( tr( "GI" ) );
	giOn = new QCheckBox( this );
	giOn->setObjectName( QStringLiteral( "AirZoneGiOn" ) );
	form->addWidget( giOn, r++, 1 );
	label( tr( "Kind" ) );
	giKind = new QComboBox( this );
	giKind->setObjectName( QStringLiteral( "AirZoneGiKind" ) );
	giKind->addItem( tr( "Room" ) );
	giKind->addItem( tr( "Threshold" ) );
	wwMatchFieldStyle( giKind );
	form->addWidget( giKind, r++, 1 );
	number( tr( "Room" ), "AirZoneGiRoom", 1, 1.0, 1e6, 0,
		[]( const az::Volume & v ) { return double( v.gi.room ); }, []( az::Volume & v, double x ) { v.gi.room = int( x ); } );
	number( tr( "Room A" ), "AirZoneGiRoomA", 1, 0.0, 1e6, 0,
		[]( const az::Volume & v ) { return double( v.gi.rooms[0] ); }, []( az::Volume & v, double x ) { v.gi.rooms[0] = int( x ); } );
	number( tr( "Room B" ), "AirZoneGiRoomB", 1, 0.0, 1e6, 0,
		[]( const az::Volume & v ) { return double( v.gi.rooms[1] ); }, []( az::Volume & v, double x ) { v.gi.rooms[1] = int( x ); } );
	number( tr( "Blend width" ), "AirZoneGiBlend", 1, 0.0, 1e5, 1,
		[]( const az::Volume & v ) { return double( v.gi.blendWidth ); }, []( az::Volume & v, double x ) { v.gi.blendWidth = float( x ); } );

	heading( tr( "Fog" ) );
	label( tr( "Fog" ) );
	fogOn = new QCheckBox( this );
	fogOn->setObjectName( QStringLiteral( "AirZoneFogOn" ) );
	form->addWidget( fogOn, r++, 1 );
	number( tr( "Density scale" ), "AirZoneFogScale", 2, 0.0, 64.0, 3,
		[]( const az::Volume & v ) { return double( v.fog.scale ); }, []( az::Volume & v, double x ) { v.fog.scale = float( x ); } );
	number( tr( "Density add" ), "AirZoneFogAdd", 2, 0.0, 1000.0, 3,
		[]( const az::Volume & v ) { return double( v.fog.add ); }, []( az::Volume & v, double x ) { v.fog.add = float( x ); } );
	number( tr( "Height falloff" ), "AirZoneFogHeight", 2, 0.0, 1e6, 0,
		[]( const az::Volume & v ) { return double( v.fog.heightFalloff ); }, []( az::Volume & v, double x ) { v.fog.heightFalloff = float( x ); } );
	rgb( tr( "Colour" ), "AirZoneFogColour", 2, []( az::Volume & v ) { return v.fog.colour; } );
	number( tr( "Colour strength" ), "AirZoneFogColourStrength", 2, 0.0, 1.0, 3,
		[]( const az::Volume & v ) { return double( v.fog.colourStrength ); }, []( az::Volume & v, double x ) { v.fog.colourStrength = float( x ); } );
	number( tr( "Noise" ), "AirZoneFogNoise", 2, 0.0, 1.0, 3,
		[]( const az::Volume & v ) { return double( v.fog.noise ); }, []( az::Volume & v, double x ) { v.fog.noise = float( x ); } );
	number( tr( "Noise scale" ), "AirZoneFogNoiseScale", 2, 1.0, 1e6, 0,
		[]( const az::Volume & v ) { return double( v.fog.noiseScale ); }, []( az::Volume & v, double x ) { v.fog.noiseScale = float( x ); } );

	heading( tr( "Particles" ) );
	label( tr( "Particles" ) );
	partOn = new QCheckBox( this );
	partOn->setObjectName( QStringLiteral( "AirZonePartOn" ) );
	form->addWidget( partOn, r++, 1 );
	number( tr( "Density" ), "AirZonePartDensity", 3, 0.0, 64.0, 3,
		[]( const az::Volume & v ) { return double( v.part.density ); }, []( az::Volume & v, double x ) { v.part.density = float( x ); } );
	number( tr( "Light" ), "AirZonePartLight", 3, 0.0, 64.0, 3,
		[]( const az::Volume & v ) { return double( v.part.light ); }, []( az::Volume & v, double x ) { v.part.light = float( x ); } );
	rgb( tr( "Tint" ), "AirZonePartTint", 3, []( az::Volume & v ) { return v.part.tint; } );
	number( tr( "Tint strength" ), "AirZonePartTintStrength", 3, 0.0, 1.0, 3,
		[]( const az::Volume & v ) { return double( v.part.tintStrength ); }, []( az::Volume & v, double x ) { v.part.tintStrength = float( x ); } );
	outer->addLayout( form );

	// where Save writes a new file
	auto * where = new QGridLayout;
	where->setColumnStretch( 1, 1 );
	where->setColumnMinimumWidth( 0, kLabelW );
	where->addWidget( wwHeading( tr( "Files" ), this ), 0, 0, 1, 2 );
	where->addWidget( new QLabel( tr( "Save to" ), this ), 1, 0 );
	saveRoot = new QLineEdit( this );
	saveRoot->setObjectName( QStringLiteral( "AirZoneSaveRoot" ) );
	saveRoot->setText( QSettings().value( QStringLiteral( "CellEditor/AirZoneSaveRoot" ), EsmWeather::gameDataDir() ).toString() );
	saveRoot->setToolTip( tr( "The Data folder (or a mod folder) a cell with no file gets one in: Plugins/<Provider>/Cells" ) );
	where->addWidget( saveRoot, 1, 1 );
	where->addWidget( new QLabel( tr( "Provider" ), this ), 2, 0 );
	provider = new QLineEdit( this );
	provider->setObjectName( QStringLiteral( "AirZoneProvider" ) );
	provider->setText( QSettings().value( QStringLiteral( "CellEditor/AirZoneProvider" ), QStringLiteral( "FO4CS" ) ).toString() );
	provider->setToolTip( tr( "The mod that ships the data, by its plain name (the .recx standard)" ) );
	where->addWidget( provider, 2, 1 );
	outer->addLayout( where );

	// the action bar and the summary line
	auto * bar = new QHBoxLayout;
	bar->setSpacing( 4 );
	cursorBtn = new QPushButton( tr( "To 3D cursor" ), this );
	exportBtn = new QPushButton( tr( "Export GI shapes" ), this );
	revertBtn = new QPushButton( tr( "Revert" ), this );
	saveBtn = new QPushButton( tr( "Save" ), this );
	cursorBtn->setObjectName( QStringLiteral( "AirZoneToCursor" ) );
	exportBtn->setObjectName( QStringLiteral( "AirZoneExportGi" ) );
	revertBtn->setObjectName( QStringLiteral( "AirZoneRevert" ) );
	saveBtn->setObjectName( QStringLiteral( "AirZoneSave" ) );
	cursorBtn->setToolTip( tr( "Put the selected volume's centre on the viewport's 3D cursor (right-click a surface first)" ) );
	exportBtn->setToolTip( tr( "Write today's GI volumes of every cell in view into its .recx (the switch to hand-placed volumes)" ) );
	for ( QPushButton * b : { cursorBtn, exportBtn, revertBtn, saveBtn } )
		b->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
	bar->addWidget( cursorBtn );
	bar->addWidget( exportBtn );
	bar->addStretch( 1 );
	bar->addWidget( revertBtn );
	bar->addWidget( saveBtn );
	outer->addLayout( bar );
	note = new QLabel( this );
	note->setObjectName( QStringLiteral( "AirZoneNote" ) );
	note->setWordWrap( true );
	outer->addWidget( note );

	// ---- wiring
	connect( showBox, &QCheckBox::toggled, this, [this]( bool on ) {
		QSettings().setValue( QStringLiteral( "CellEditor/AirZoneShow" ), on );
		wwAirZoneViewSet( on && isVisible(), cell, vol );
		if ( glView )
			glView->update();
	} );
	connect( cellBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this]( int i ) {
		if ( !syncing )
			loadCell( i );
	} );
	connect( list, &QTreeWidget::currentItemChanged, this, [this]() {
		if ( !syncing )
			showVolume( list->currentItem() ? list->indexOfTopLevelItem( list->currentItem() ) : -1 );
	} );
	for ( Num & n : nums )
		connect( n.field, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this]() { fromFields(); } );
	connect( nameEdit, &QLineEdit::editingFinished, this, [this]() { fromFields(); } );
	for ( QComboBox * c : { shapeBox, giKind, giModeBox } )
		connect( c, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this]() { fromFields(); } );
	for ( QCheckBox * c : { giOn, fogOn, partOn } )
		connect( c, &QCheckBox::toggled, this, [this]() { fromFields(); } );
	connect( saveRoot, &QLineEdit::editingFinished, this, [this]() {
		QSettings().setValue( QStringLiteral( "CellEditor/AirZoneSaveRoot" ), saveRoot->text() );
	} );
	connect( provider, &QLineEdit::editingFinished, this, [this]() {
		QSettings().setValue( QStringLiteral( "CellEditor/AirZoneProvider" ), provider->text().trimmed() );
	} );
	connect( addBtn, &QPushButton::clicked, this, [this]() {
		if ( cell < 0 )
			return;
		az::Volume v;
		v.name = QStringLiteral( "zone %1" ).arg( work.size() + 1 ).toStdString();
		v.shape.size[0] = v.shape.size[1] = v.shape.size[2] = 512.0f;
		v.edge = 64.0f;
		v.fog.on = true;
		v.fog.scale = 2.0f;
		if ( glView )	// the 3D cursor, where the view's right click put it
			for ( int k = 0; k < 3; k++ )
				v.shape.center[k] = glView->cursorPos[k];
		work.push_back( v );
		dirty = true;
		pushLive();
		rebuildList();
		list->setCurrentItem( list->topLevelItem( int( work.size() ) - 1 ) );
	} );
	connect( dupBtn, &QPushButton::clicked, this, [this]() {
		if ( vol < 0 || size_t( vol ) >= work.size() )
			return;
		az::Volume v = work[size_t( vol )];
		v.name += " copy";
		v.shape.center[0] += 64.0f;
		v.gi.seq = -1;
		work.push_back( v );
		dirty = true;
		pushLive();
		rebuildList();
		list->setCurrentItem( list->topLevelItem( int( work.size() ) - 1 ) );
	} );
	connect( delBtn, &QPushButton::clicked, this, [this]() {
		if ( vol < 0 || size_t( vol ) >= work.size() )
			return;
		work.erase( work.begin() + vol );
		dirty = true;
		pushLive();
		rebuildList();
		showVolume( std::min( vol, int( work.size() ) - 1 ) );
	} );
	connect( cursorBtn, &QPushButton::clicked, this, [this]() {
		if ( !glView || vol < 0 || size_t( vol ) >= work.size() )
			return;
		for ( int k = 0; k < 3; k++ )
			work[size_t( vol )].shape.center[k] = glView->cursorPos[k];
		dirty = true;
		pushLive();
		showVolume( vol );
	} );
	connect( revertBtn, &QPushButton::clicked, this, [this]() {
		if ( cell < 0 )
			return;
		wwAirZoneEdit( nif, cell, {}, QString(), true );
		dirty = false;
		loadCell( cell );
		say( tr( "reverted to the file" ), false );
	} );
	connect( saveBtn, &QPushButton::clicked, this, [this]() {
		if ( cell < 0 )
			return;
		pushLive();
		QString path, err;
		if ( !wwAirZoneSave( nif, cell, saveRoot->text().trimmed(), provider->text().trimmed(), &path, &err ) ) {
			say( tr( "not saved: %1" ).arg( err ), true );
			return;
		}
		dirty = false;
		bool gi = false;
		for ( const az::Volume & v : work )
			gi = gi || v.gi.on;
		say( tr( "saved %1%2" ).arg( path, gi ? tr( "; the GI takes its volumes when the cell is opened again" ) : QString() ), false );
	} );
	connect( exportBtn, &QPushButton::clicked, this, [this]() {
		QString err;
		const QStringList files = wwAirZoneGiExport( nif, saveRoot->text().trimmed(), provider->text().trimmed(), &err );
		if ( !err.isEmpty() ) {
			say( tr( "not exported: %1" ).arg( err ), true );
			return;
		}
		refresh();
		say( tr( "today's GI volumes written into %1 file(s); open the cell again for the GI to read them" ).arg( files.size() ), false );
	} );
}

void AirZonePanel::setNif( const void * n )
{
	if ( n != nif ) {
		nif = n;
		cell = vol = -1;
		work.clear();
		dirty = false;
	}
	refresh();
}

void AirZonePanel::setGLView( GLView * v )
{
	glView = v;
}

void AirZonePanel::refresh()
{
	syncing = true;
	const auto vols = nif ? wwAirZoneFor( nif, &seenVersion ) : nullptr;
	const int was = cell;
	cellBox->clear();
	if ( vols )
		for ( const AirZoneCellVolumes & cv : *vols )
			cellBox->addItem( QStringLiteral( "%1 (%2)" ).arg( cv.cell.label(), cv.file.isEmpty() ? tr( "no file" )
				: tr( "%1 volume(s)" ).arg( cv.volumes.size() ) ) );
	syncing = false;
	if ( !vols || vols->empty() ) {
		cell = vol = -1;
		work.clear();
		rebuildList();
		showVolume( -1 );
		say( nif ? tr( "this view has no cell" ) : tr( "no cell open" ), true );
		return;
	}
	const int pick = was >= 0 && was < int( vols->size() ) ? was : int( vols->size() ) / 2;	// an exterior's middle cell
	syncing = true;
	cellBox->setCurrentIndex( pick );
	syncing = false;
	if ( pick != cell || !dirty )
		loadCell( pick );
	if ( !wwAirZonesOn() )
		say( tr( "Air zones are off (Display > Groups > Air zones): the volumes are edited and saved, the fog and particles do not read them" ), true );
}

void AirZonePanel::loadCell( int index )
{
	cell = index;
	const auto vols = nif ? wwAirZoneFor( nif ) : nullptr;
	if ( !vols || index < 0 || size_t( index ) >= vols->size() ) {
		work.clear();
		rebuildList();
		showVolume( -1 );
		return;
	}
	const AirZoneCellVolumes & cv = ( *vols )[size_t( index )];
	work = cv.volumes;
	giMode = cv.giMode;
	dirty = cv.edited;
	rebuildList();
	showVolume( work.empty() ? -1 : 0 );
	QString line = wwAirZoneCensus( nif );
	if ( !cv.file.isEmpty() )
		line += tr( "; this cell: %1 (%2)" ).arg( cv.file, cv.rung );
	if ( !cv.warnings.isEmpty() )
		line += tr( "; %1" ).arg( cv.warnings.join( QStringLiteral( "; " ) ) );
	say( line, !cv.warnings.isEmpty() );
}

void AirZonePanel::rebuildList()
{
	syncing = true;
	list->clear();
	for ( const az::Volume & v : work ) {
		auto * it = new QTreeWidgetItem( list );
		it->setText( 0, QString::fromStdString( v.name ) );
		it->setText( 1, tr( kShapes[std::clamp( v.shape.type, 0, 4 )] ) );
		it->setText( 2, blocksText( v ) );
	}
	syncing = false;
}

void AirZonePanel::showVolume( int index )
{
	vol = index;
	const bool have = index >= 0 && size_t( index ) < work.size();
	syncing = true;
	if ( have ) {
		const az::Volume & v = work[size_t( index )];
		if ( list->currentItem() != list->topLevelItem( index ) )
			list->setCurrentItem( list->topLevelItem( index ) );
		nameEdit->setText( QString::fromStdString( v.name ) );
		shapeBox->setCurrentIndex( std::clamp( v.shape.type, 0, 4 ) );
		giOn->setChecked( v.gi.on );
		giKind->setCurrentIndex( v.gi.threshold ? 1 : 0 );
		fogOn->setChecked( v.fog.on );
		partOn->setChecked( v.part.on );
		for ( Num & n : nums )
			n.field->setValue( n.get( v ) );
	}
	giModeBox->setCurrentIndex( giMode == QLatin1String( "all" ) ? 1 : 0 );
	syncing = false;
	for ( QWidget * w : std::initializer_list<QWidget *>{ nameEdit, shapeBox, giOn, giKind, fogOn, partOn, dupBtn, delBtn, cursorBtn } )
		w->setEnabled( have );
	for ( Num & n : nums ) {
		n.field->setEnabled( have );
		n.label->setEnabled( have );
	}
	addBtn->setEnabled( cell >= 0 );
	saveBtn->setEnabled( cell >= 0 );
	revertBtn->setEnabled( cell >= 0 );
	greyBlocks();
	wwAirZoneViewSet( showBox->isChecked() && isVisible(), cell, vol );
	if ( glView )
		glView->update();
}

void AirZonePanel::greyBlocks()
{
	const bool have = vol >= 0 && size_t( vol ) < work.size();
	const bool on[4] = { have, have && giOn->isChecked(), have && fogOn->isChecked(), have && partOn->isChecked() };
	const bool thr = giKind->currentIndex() == 1;
	giKind->setEnabled( on[1] );
	for ( Num & n : nums ) {
		bool e = on[n.block];
		if ( n.block == 1 ) {
			const QString name = n.field->objectName();
			const bool roomField = name == QLatin1String( "AirZoneGiRoom" );
			e = e && ( roomField ? !thr : thr );
		}
		n.field->setEnabled( e );
		n.label->setEnabled( e );
	}
}

void AirZonePanel::fromFields()
{
	if ( syncing )
		return;
	giMode = giModeBox->currentData().toString();
	if ( vol >= 0 && size_t( vol ) < work.size() ) {
		az::Volume & v = work[size_t( vol )];
		v.name = nameEdit->text().toStdString();
		v.shape.type = shapeBox->currentIndex();
		v.gi.on = giOn->isChecked();
		v.gi.threshold = giKind->currentIndex() == 1;
		v.fog.on = fogOn->isChecked();
		v.part.on = partOn->isChecked();
		for ( Num & n : nums )
			n.set( v, n.field->value() );
		if ( v.fog.on && v.fog.colourStrength <= 0.0f && ( v.fog.colour[0] != 1.0f || v.fog.colour[1] != 1.0f || v.fog.colour[2] != 1.0f ) )
			v.fog.colourStrength = 1.0f;	// a colour typed in shows at once (the file's rule: a colour alone is strength 1)
		if ( QTreeWidgetItem * it = list->topLevelItem( vol ) ) {
			it->setText( 0, QString::fromStdString( v.name ) );
			it->setText( 1, tr( kShapes[std::clamp( v.shape.type, 0, 4 )] ) );
			it->setText( 2, blocksText( v ) );
		}
	}
	dirty = true;
	greyBlocks();
	pushLive();
}

void AirZonePanel::pushLive()
{
	if ( cell < 0 || !nif )
		return;
	wwAirZoneEdit( nif, cell, work, giMode );
	wwAirZoneViewSet( showBox->isChecked() && isVisible(), cell, vol );
	if ( glView )
		glView->update();
	say( tr( "unsaved: the fog and particles show it now; Save writes the file" ), false );
}

void AirZonePanel::say( const QString & text, bool isRefusal )
{
	refusal = isRefusal;
	note->setText( text );
	note->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( isRefusal ? "danger" : "textMuted" ) ) );
}

void AirZonePanel::showEvent( QShowEvent * e )
{
	QWidget::showEvent( e );
	wwAirZoneViewSet( showBox->isChecked(), cell, vol );
	if ( glView )
		glView->update();
}

void AirZonePanel::hideEvent( QHideEvent * e )
{
	QWidget::hideEvent( e );
	wwAirZoneViewSet( false, cell, vol );
	if ( glView )
		glView->update();
}

int AirZonePanel::cellRows() const
{
	return cellBox ? cellBox->count() : 0;
}

int AirZonePanel::volumeRows() const
{
	return list ? list->topLevelItemCount() : 0;
}

QString AirZonePanel::noteText() const
{
	return note ? note->text() : QString();
}
