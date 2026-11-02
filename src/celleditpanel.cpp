// lane ESPWRITE2 (2026-11-02): the Edits section (src/celleditpanel.h)

#include "celleditpanel.h"

#include "cellclick.h"
#include "celledit.h"
#include "cellpick.h"
#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace {
constexpr double kDeg = 180.0 / 3.14159265358979323846;
}

CellEditPanel::CellEditPanel( QWidget * parent ) : QWidget( parent )
{
	setObjectName( QStringLiteral( "CellEditPanel" ) );
	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->addWidget( wwHeading( tr( "Edits" ), this ) );

	auto * form = new QGridLayout;
	form->setColumnStretch( 1, 1 );
	int r = 0;
	auto row = [&]( const QString & label, QWidget * w, const QString & tip ) {
		auto * l = new QLabel( label, this );
		l->setToolTip( tip );
		w->setToolTip( tip );
		form->addWidget( l, r, 0 );
		form->addWidget( w, r++, 1 );
	};

	baseBox = new QComboBox( this );
	baseBox->setObjectName( QStringLiteral( "CellEditBase" ) );
	baseBox->addItems( { QStringLiteral( "XMarkerHeading" ), QStringLiteral( "XMarker" ), tr( "By EditorID" ) } );
	wwMatchFieldStyle( baseBox );
	row( tr( "Base" ), baseBox, tr( "The base form placed: a heading marker, a marker, or any placeable base by its EditorID" ) );
	edidEdit = new QLineEdit( this );
	edidEdit->setObjectName( QStringLiteral( "CellEditEdid" ) );
	edidEdit->setPlaceholderText( tr( "EditorID" ) );
	row( tr( "EditorID" ), edidEdit, tr( "The EditorID of the base form (STAT, MSTT, FURN, ACTI, ...)" ) );

	yawSpin = new QDoubleSpinBox( this );
	yawSpin->setObjectName( QStringLiteral( "CellEditYaw" ) );
	yawSpin->setRange( -360.0, 360.0 );
	yawSpin->setDecimals( 1 );
	yawSpin->setSuffix( QStringLiteral( " deg" ) );
	wwMakeScrubField( yawSpin );
	row( tr( "Yaw" ), yawSpin, tr( "The heading, clockwise from +Y seen from above. A drag in the view, T then a click, or R + wheel set it too" ) );

	alignBox = new QComboBox( this );
	alignBox->setObjectName( QStringLiteral( "CellEditAlign" ) );
	alignBox->addItems( { tr( "Z up" ), tr( "Flat on the surface" ) } );
	wwMatchFieldStyle( alignBox );
	row( tr( "Align" ), alignBox, tr( "Upright, or tilted to the surface under the cursor (N in the view)" ) );

	snapBox = new QCheckBox( tr( "Ground snap" ), this );
	snapBox->setObjectName( QStringLiteral( "CellEditSnap" ) );
	row( QString(), snapBox, tr( "A point on a wall or a ceiling drops to the floor below it (G in the view)" ) );
	persistBox = new QCheckBox( tr( "Persistent" ), this );
	persistBox->setObjectName( QStringLiteral( "CellEditPersistent" ) );
	row( QString(), persistBox, tr( "Written persistent (every vanilla marker is)" ) );

	placeBtn = new QPushButton( tr( "Place" ), this );
	placeBtn->setObjectName( QStringLiteral( "CellEditPlace" ) );
	placeBtn->setCheckable( true );
	row( QString(), placeBtn, tr( "Place mode (Shift+A in the view): click drops, drag turns, Esc leaves" ) );

	// ESPWRITE3B: change the picked reference (an override of its winner; never a delete)
	pickedLabel = new QLabel( this );
	pickedLabel->setObjectName( QStringLiteral( "CellEditPicked" ) );
	pickedLabel->setWordWrap( true );
	row( tr( "Picked" ), pickedLabel, tr( "The reference clicked in the view; the buttons below change it" ) );
	auto * moveRow = new QWidget( this );
	auto * mh = new QHBoxLayout( moveRow );
	mh->setContentsMargins( 0, 0, 0, 0 );
	for ( int k = 0; k < 3; k++ ) {
		moveSpin[k] = new QDoubleSpinBox( moveRow );
		moveSpin[k]->setObjectName( QStringLiteral( "CellEditMove%1" ).arg( QLatin1Char( "XYZ"[k] ) ) );
		moveSpin[k]->setRange( -1.0e6, 1.0e6 );
		moveSpin[k]->setDecimals( 1 );
		wwMakeScrubField( moveSpin[k] );
		mh->addWidget( moveSpin[k], 1 );
	}
	moveBtn = new QPushButton( tr( "Move" ), moveRow );
	moveBtn->setObjectName( QStringLiteral( "CellEditMove" ) );
	mh->addWidget( moveBtn );
	row( tr( "Position" ), moveRow, tr( "Move the picked reference to this point (game units)" ) );
	auto * turnRow = new QWidget( this );
	auto * th = new QHBoxLayout( turnRow );
	th->setContentsMargins( 0, 0, 0, 0 );
	turnLeftBtn = new QPushButton( tr( "-15 deg" ), turnRow );
	turnLeftBtn->setObjectName( QStringLiteral( "CellEditTurnLeft" ) );
	turnRightBtn = new QPushButton( tr( "+15 deg" ), turnRow );
	turnRightBtn->setObjectName( QStringLiteral( "CellEditTurnRight" ) );
	th->addWidget( turnLeftBtn, 1 );
	th->addWidget( turnRightBtn, 1 );
	row( tr( "Turn" ), turnRow, tr( "Turn the picked reference about Z" ) );
	auto * scaleRow = new QWidget( this );
	auto * sh = new QHBoxLayout( scaleRow );
	sh->setContentsMargins( 0, 0, 0, 0 );
	scaleSpin = new QDoubleSpinBox( scaleRow );
	scaleSpin->setObjectName( QStringLiteral( "CellEditScale" ) );
	scaleSpin->setRange( 0.01, 10.0 );
	scaleSpin->setDecimals( 3 );
	scaleSpin->setSingleStep( 0.05 );
	wwMakeScrubField( scaleSpin );
	scaleBtn = new QPushButton( tr( "Set" ), scaleRow );
	scaleBtn->setObjectName( QStringLiteral( "CellEditScaleSet" ) );
	sh->addWidget( scaleSpin, 1 );
	sh->addWidget( scaleBtn );
	row( tr( "Scale" ), scaleRow, tr( "The picked reference's scale (1 = none written)" ) );
	disableBtn = new QPushButton( tr( "Disable" ), this );
	disableBtn->setObjectName( QStringLiteral( "CellEditDisable" ) );
	row( QString(), disableBtn, tr( "Initially disabled: the reference stays in the file, hidden in the game (no delete)" ) );
	outer->addLayout( form );

	list = new QListWidget( this );
	list->setObjectName( QStringLiteral( "CellEditList" ) );
	list->setMinimumHeight( 80 );
	outer->addWidget( list );
	removeBtn = new QPushButton( tr( "Remove" ), this );
	removeBtn->setObjectName( QStringLiteral( "CellEditRemove" ) );
	removeBtn->setToolTip( tr( "Drop the selected edit (Ctrl+Z in the view drops the last)" ) );
	outer->addWidget( removeBtn );

	auto * file = new QGridLayout;
	file->setColumnStretch( 1, 1 );
	r = 0;
	form = file;
	auto * pathRow = new QWidget( this );
	auto * ph = new QHBoxLayout( pathRow );
	ph->setContentsMargins( 0, 0, 0, 0 );
	pathEdit = new QLineEdit( pathRow );
	pathEdit->setObjectName( QStringLiteral( "CellEditPlugin" ) );
	auto * browse = new QPushButton( tr( "Browse" ), pathRow );
	ph->addWidget( pathEdit, 1 );
	ph->addWidget( browse );
	row( tr( "Plugin" ), pathRow, tr( "The .esp written; by default WWCellEdits.esp beside the first plugin of the load order" ) );
	modeBox = new QComboBox( this );
	modeBox->setObjectName( QStringLiteral( "CellEditMode" ) );
	modeBox->addItems( { tr( "New file" ), tr( "Add to existing" ) } );
	wwMatchFieldStyle( modeBox );
	row( tr( "Mode" ), modeBox, tr( "Write a new file, or keep what the file holds and add to it" ) );
	eslBox = new QCheckBox( tr( "Light plugin (ESL flag)" ), this );
	eslBox->setObjectName( QStringLiteral( "CellEditEsl" ) );
	row( QString(), eslBox, tr( "A new file is flagged light (ids 0x800..0xFFF); Add to existing keeps the file's flag" ) );
	outer->addLayout( file );

	status = new QLabel( this );
	status->setObjectName( QStringLiteral( "CellEditStatus" ) );
	status->setWordWrap( true );
	outer->addWidget( status );
	auto * bar = new QHBoxLayout;
	summary = new QLabel( this );
	summary->setObjectName( QStringLiteral( "CellEditSummary" ) );
	summary->setWordWrap( true );
	saveBtn = new QPushButton( tr( "Save" ), this );
	saveBtn->setObjectName( QStringLiteral( "CellEditSave" ) );
	saveBtn->setToolTip( tr( "Write the plugin, then re-open the cell with it in the load order" ) );
	bar->addWidget( summary, 1 );
	bar->addWidget( saveBtn );
	outer->addLayout( bar );

	CellEditSession & s = cellEditSession();
	auto bus = CellEditBus::instance();

	connect( baseBox, &QComboBox::currentIndexChanged, this, [this]( int ) {
		if ( !syncing )
			applyBase();
	} );
	connect( edidEdit, &QLineEdit::editingFinished, this, [this]() {
		if ( !syncing && baseBox->currentIndex() == 2 )
			applyBase();
	} );
	connect( yawSpin, &QDoubleSpinBox::valueChanged, this, [&s, this]( double v ) {
		if ( syncing )
			return;
		s.yaw = float( v / kDeg );
		emit CellEditBus::instance()->changed();
	} );
	connect( alignBox, &QComboBox::currentIndexChanged, this, [&s, this]( int i ) {
		if ( !syncing ) {
			s.flat = i == 1;
			emit CellEditBus::instance()->changed();
		}
	} );
	connect( snapBox, &QCheckBox::toggled, this, [&s, this]( bool on ) {
		if ( !syncing )
			s.snap = on;
	} );
	connect( persistBox, &QCheckBox::toggled, this, [&s, this]( bool on ) {
		if ( !syncing )
			s.ghost.persistent = on;
	} );
	connect( placeBtn, &QPushButton::clicked, this, [this]( bool on ) {
		if ( on )
			cellEditStartPlace( view );
		else
			cellEditStopPlace();
		refresh();
	} );
	connect( removeBtn, &QPushButton::clicked, this, [this]() { cellEditRemove( list->currentRow() ); } );
	connect( moveBtn, &QPushButton::clicked, this, [this]() {
		const float v[3] = { float( moveSpin[0]->value() ), float( moveSpin[1]->value() ), float( moveSpin[2]->value() ) };
		cellEditChangeRef( pickedRef(), 'm', v );
	} );
	connect( turnLeftBtn, &QPushButton::clicked, this, [this]() {
		const float v[1] = { float( -15.0 / kDeg ) };
		cellEditChangeRef( pickedRef(), 't', v );
	} );
	connect( turnRightBtn, &QPushButton::clicked, this, [this]() {
		const float v[1] = { float( 15.0 / kDeg ) };
		cellEditChangeRef( pickedRef(), 't', v );
	} );
	connect( scaleBtn, &QPushButton::clicked, this, [this]() {
		const float v[1] = { float( scaleSpin->value() ) };
		cellEditChangeRef( pickedRef(), 's', v );
	} );
	connect( disableBtn, &QPushButton::clicked, this, [this]() { cellEditChangeRef( pickedRef(), 'd', nullptr ); } );
	connect( CellPickBus::instance(), &CellPickBus::picked, this, [this]( int, int ) { refreshPicked(); } );
	connect( CellPickBus::instance(), &CellPickBus::sceneChanged, this, [this]() { refreshPicked(); } );
	connect( browse, &QPushButton::clicked, this, [this]() {
		const QString p = QFileDialog::getSaveFileName( this, tr( "Plugin to write" ), pathEdit->text(),
			tr( "Plugins (*.esp *.esl)" ), nullptr, QFileDialog::DontConfirmOverwrite );
		if ( !p.isEmpty() ) {
			pathEdit->setText( p );
			cellEditSession().target.path = p;
			if ( QFileInfo::exists( p ) )
				modeBox->setCurrentIndex( 1 );
			refreshSummary();
		}
	} );
	connect( pathEdit, &QLineEdit::textEdited, this, [this]( const QString & t ) {
		cellEditSession().target.path = t.trimmed();
		refreshSummary();
	} );
	connect( modeBox, &QComboBox::currentIndexChanged, this, [this]( int i ) {
		if ( syncing )
			return;
		cellEditSession().target.append = i == 1;
		eslBox->setEnabled( i == 0 );
		refreshSummary();
	} );
	connect( eslBox, &QCheckBox::toggled, this, [this]( bool on ) {
		if ( !syncing ) {
			cellEditSession().target.light = on;
			refreshSummary();
		}
	} );
	connect( saveBtn, &QPushButton::clicked, this, [this]() {
		QString line;
		cellEditSaveSession( &line );
		refresh();
	} );
	connect( bus, &CellEditBus::changed, this, &CellEditPanel::refresh );
	connect( bus, &CellEditBus::status, this, [this]( const QString & text, bool isRefusal ) {
		status->setText( text );
		status->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( isRefusal ? "danger" : "textMuted" ) ) );
	} );
	refresh();
}

void CellEditPanel::applyBase()
{
	const int i = baseBox->currentIndex();
	edidEdit->setEnabled( i == 2 );
	const QString edid = i == 2 ? edidEdit->text().trimmed() : baseBox->currentText();
	if ( !edid.isEmpty() )
		cellEditSetBase( edid );
}

void CellEditPanel::refresh()
{
	const CellEditSession & s = cellEditSession();
	syncing = true;
	const QString e = s.ghost.baseEdid;
	if ( e.compare( QLatin1String( "XMarkerHeading" ), Qt::CaseInsensitive ) == 0 )
		baseBox->setCurrentIndex( 0 );
	else if ( e.compare( QLatin1String( "XMarker" ), Qt::CaseInsensitive ) == 0 )
		baseBox->setCurrentIndex( 1 );
	else {
		baseBox->setCurrentIndex( 2 );
		if ( !edidEdit->hasFocus() )
			edidEdit->setText( e );
	}
	edidEdit->setEnabled( baseBox->currentIndex() == 2 );
	double deg = double( s.yaw ) * kDeg;
	deg = std::fmod( deg, 360.0 );
	if ( !yawSpin->hasFocus() )
		yawSpin->setValue( deg );
	alignBox->setCurrentIndex( s.flat ? 1 : 0 );
	snapBox->setChecked( s.snap );
	persistBox->setChecked( s.ghost.persistent );
	placeBtn->setChecked( s.placing );
	const int cur = list->currentRow();
	list->clear();
	for ( const CellEditItem & it : s.edits )
		list->addItem( cellEditRowText( it ) );
	for ( const CellEditChange & c : s.changes )
		list->addItem( cellEditChangeText( c ) );
	if ( cur >= 0 && cur < list->count() )
		list->setCurrentRow( cur );
	removeBtn->setEnabled( list->count() > 0 );
	if ( !pathEdit->hasFocus() )
		pathEdit->setText( s.target.path );
	modeBox->setCurrentIndex( s.target.append ? 1 : 0 );
	eslBox->setChecked( s.target.light );
	eslBox->setEnabled( !s.target.append );
	syncing = false;
	refreshPicked();
	refreshSummary();
}

quint32 CellEditPanel::pickedRef() const
{
	const int i = cellPickCurrent();
	return i >= 0 && i < cellPickTable().size() ? cellPickTable().at( i ).refForm : 0;
}

void CellEditPanel::refreshPicked()
{
	const int i = cellPickCurrent();
	const bool on = i >= 0 && i < cellPickTable().size() && cellPickTable().at( i ).refForm;
	for ( QWidget * w : { static_cast<QWidget *>( moveBtn ), static_cast<QWidget *>( turnLeftBtn ),
			static_cast<QWidget *>( turnRightBtn ), static_cast<QWidget *>( scaleBtn ), static_cast<QWidget *>( disableBtn ),
			static_cast<QWidget *>( scaleSpin ), static_cast<QWidget *>( moveSpin[0] ), static_cast<QWidget *>( moveSpin[1] ),
			static_cast<QWidget *>( moveSpin[2] ) } )
		w->setEnabled( on );
	if ( !on ) {
		pickedLabel->setText( tr( "Click a reference in the view" ) );
		return;
	}
	const CellPickEntry & e = cellPickTable().at( i );
	// the pending change's values when there is one, else the reference's own
	const float * pos = e.pos;
	float scale = e.scale;
	for ( const CellEditChange & c : cellEditSession().changes )
		if ( c.ref == e.refForm ) {
			pos = c.pos;
			scale = c.scale;
		}
	pickedLabel->setText( QStringLiteral( "%1  %2%3" ).arg( CellPickTable::formName( e.refForm ), e.baseEdid,
		e.precombined ? tr( "  (precombined)" ) : QString() ) );
	for ( int k = 0; k < 3; k++ )
		if ( !moveSpin[k]->hasFocus() )
			moveSpin[k]->setValue( double( pos[k] ) );
	if ( !scaleSpin->hasFocus() )
		scaleSpin->setValue( double( scale ) );
}

void CellEditPanel::refreshSummary()
{
	bool ref = true;
	const QString t = cellEditSummary( &ref );
	refusal = ref;
	summary->setText( t );
	summary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( ref ? "danger" : "textMuted" ) ) );
	saveBtn->setEnabled( !ref );
}

QString CellEditPanel::summaryText() const
{
	return summary->text();
}
