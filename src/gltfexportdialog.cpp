/* See src/gltfexportdialog.h. Lane GLTFEXPORT1, 2026-09-19.

   Every row here has a twin flag in gltfExportParseFlag() (src/gltfexportopts.cpp).
   options() writes the SAME struct the CLI parses into, and the caller hands
   that one struct to gltfExportCharacter(), so there is exactly one code path
   for an option and no second implementation to drift.

   DEFAULTS. bungo ruled them on 2026-09-19 09:45: the dialog opens
   Blender-ready -- skeleton auto, joints whole, units game, animation on,
   bones body only, textures copy. Until that hour every row opened on the
   value that reproduced the old export byte for byte, because an owed ruling
   never ships as a default. The way back to those values is the CLI's
   `--legacy-defaults`; by his instruction there is NO dialog row for it. The
   clip row was never a toggle over anything: the menu export could not carry
   the loaded clip at all, and that is a repair. */

#include "gltfexportdialog.h"

#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

//! A label in the Name column. One line, no wrapping into a paragraph: a
//! settings row is a name and a control, nothing else.
QLabel * nameLabel( const QString & text, QWidget * parent )
{
	QLabel * l = new QLabel( text, parent );
	l->setStyleSheet( QStringLiteral( "QLabel { color: %1; background: transparent; }" )
					  .arg( wwSkinColor( "text" ) ) );
	return l;
}

QComboBox * makeCombo( QWidget * parent, const QStringList & entries )
{
	QComboBox * c = new QComboBox( parent );
	c->addItems( entries );
	c->setCurrentIndex( 0 );
	wwMatchFieldStyle( c );
	wwGuardWheel( c );
	return c;
}

QPushButton * smallButton( const QString & text, QWidget * parent )
{
	QPushButton * b = new QPushButton( text, parent );
	b->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 8px" ) ) );
	b->setCursor( Qt::PointingHandCursor );
	return b;
}

} // namespace

// ---------------------------------------------------------------------------

GltfExportDialog::GltfExportDialog( const QString & nifPathIn, bool haveClipIn,
									const QString & clipNameIn, QWidget * parent )
	: QDialog( parent ), nifPath( nifPathIn ), haveClip( haveClipIn ), clipName( clipNameIn )
{
	setWindowTitle( tr( "Export glTF" ) );
	setObjectName( QStringLiteral( "GltfExportDialog" ) );
	setStyleSheet( QStringLiteral( "QDialog { background: %1; color: %2; }" )
				   .arg( wwSkinColor( "bgWin" ), wwSkinColor( "text" ) ) );

	QVBoxLayout * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 12, 10, 12, 10 );
	outer->setSpacing( 8 );

	outer->addWidget( wwHeading( tr( "Export glTF" ), this ) );

	grid = new QGridLayout;
	grid->setContentsMargins( 0, 0, 0, 0 );
	grid->setHorizontalSpacing( 10 );
	grid->setVerticalSpacing( 4 );
	grid->setColumnStretch( 1, 1 );
	outer->addLayout( grid );

	// (2) skeleton ---------------------------------------------------------
	{
		QWidget * cell = new QWidget( this );
		QHBoxLayout * h = new QHBoxLayout( cell );
		h->setContentsMargins( 0, 0, 0, 0 );
		h->setSpacing( 4 );
		cbSkeleton = makeCombo( cell, { tr( "None" ), tr( "Find automatically" ), tr( "File..." ) } );
		edSkeleton = new QLineEdit( cell );
		edSkeleton->setPlaceholderText( QStringLiteral( "skeleton.nif" ) );
		edSkeleton->setEnabled( false );
		wwMatchFieldStyle( edSkeleton );
		QPushButton * br = smallButton( QStringLiteral( "..." ), cell );
		br->setEnabled( false );
		h->addWidget( cbSkeleton );
		h->addWidget( edSkeleton, 1 );
		h->addWidget( br );
		connect( cbSkeleton, QOverload<int>::of( &QComboBox::currentIndexChanged ),
				 this, [this, br]( int i ) {
					 edSkeleton->setEnabled( i == 2 );
					 br->setEnabled( i == 2 );
					 refreshSummary();
				 } );
		connect( br, &QPushButton::clicked, this, &GltfExportDialog::browseSkeleton );
		connect( edSkeleton, &QLineEdit::textChanged, this, &GltfExportDialog::refreshSummary );
		addRow( tr( "Skeleton" ), cell );
	}

	// (3) joints -----------------------------------------------------------
	cbJoints = makeCombo( this, { tr( "Weighted bones only" ), tr( "Whole skeleton, one joint list" ) } );
	addRow( tr( "Joints" ), cbJoints );

	// (4) units ------------------------------------------------------------
	cbUnits = makeCombo( this, { tr( "Metres" ), tr( "Game units" ) } );
	addRow( tr( "Units" ), cbUnits );

	// (5) parts ------------------------------------------------------------
	{
		QWidget * cell = new QWidget( this );
		QVBoxLayout * v = new QVBoxLayout( cell );
		v->setContentsMargins( 0, 0, 0, 0 );
		v->setSpacing( 4 );
		lwParts = new QListWidget( cell );
		lwParts->setStyleSheet( wwSelectionTreeQss() );
		lwParts->setMaximumHeight( 92 );
		QWidget * bar = new QWidget( cell );
		QHBoxLayout * hb = new QHBoxLayout( bar );
		hb->setContentsMargins( 0, 0, 0, 0 );
		hb->setSpacing( 4 );
		QPushButton * add = smallButton( tr( "Add part" ), bar );
		QPushButton * rem = smallButton( tr( "Remove" ), bar );
		hb->addWidget( add );
		hb->addWidget( rem );
		hb->addStretch( 1 );
		v->addWidget( lwParts );
		v->addWidget( bar );
		connect( add, &QPushButton::clicked, this, &GltfExportDialog::addPart );
		connect( rem, &QPushButton::clicked, this, &GltfExportDialog::removePart );
		addRow( tr( "Parts" ), cell );
	}

	// (6) bones ------------------------------------------------------------
	cbBones = makeCombo( this, { tr( "All bones" ), tr( "Body bones only" ) } );
	addRow( tr( "Bones" ), cbBones );

	// (7) textures ---------------------------------------------------------
	{
		QStringList entries{ tr( "Reference only" ), tr( "Copy beside the file" ) };
		if ( gltfExportPngAvailable() )
			entries << tr( "Convert to PNG" );
		cbTextures = makeCombo( this, entries );
		addRow( tr( "Textures" ), cbTextures );
	}

	// (8) root motion ------------------------------------------------------
	cbRootMotion = makeCombo( this, { tr( "Strip" ), tr( "Keep on the root bone" ),
									  tr( "Bake onto the object" ) } );
	addRow( tr( "Root motion" ), cbRootMotion );

	// (1) the clip ---------------------------------------------------------
	ckClip = new QCheckBox( haveClip ? clipName : tr( "no clip loaded" ), this );
	ckClip->setChecked( haveClip );
	ckClip->setEnabled( haveClip );
	addRow( tr( "Animation" ), ckClip );

	// Part 2 ---------------------------------------------------------------
	ckBuild = new QCheckBox( QString(), this );
	addRow( tr( "Bake body build" ), ckBuild );
	ckBuildCycle = new QCheckBox( QString(), this );
	ckBuildCycle->setEnabled( false );
	addRow( tr( "Body build cycle clip" ), ckBuildCycle );
	connect( ckBuild, &QCheckBox::toggled, this, [this]( bool on ) {
		ckBuildCycle->setEnabled( on );
		if ( !on )
			ckBuildCycle->setChecked( false );
		refreshSummary();
	} );

	for ( QComboBox * c : { cbJoints, cbUnits, cbBones, cbTextures, cbRootMotion } )
		connect( c, QOverload<int>::of( &QComboBox::currentIndexChanged ),
				 this, &GltfExportDialog::refreshSummary );
	connect( ckClip, &QCheckBox::toggled, this, &GltfExportDialog::refreshSummary );
	connect( ckBuildCycle, &QCheckBox::toggled, this, &GltfExportDialog::refreshSummary );

	// The one line of prose in the window, and it is a READBACK of the rows --
	// what the export will do, not an explanation of what a row means.
	lbSummary = new QLabel( this );
	lbSummary->setWordWrap( true );
	lbSummary->setStyleSheet( QStringLiteral( "QLabel { color: %1; background: transparent; }" )
							  .arg( wwSkinColor( "textMuted" ) ) );
	outer->addSpacing( 4 );
	outer->addWidget( lbSummary );

	QDialogButtonBox * bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
	bb->button( QDialogButtonBox::Ok )->setText( tr( "Export" ) );
	bb->button( QDialogButtonBox::Ok )->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "4px 14px" ) ) );
	bb->button( QDialogButtonBox::Cancel )->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "4px 14px" ) ) );
	connect( bb, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( bb, &QDialogButtonBox::rejected, this, &QDialog::reject );
	outer->addWidget( bb );

	/* The rows were BUILT at index 0, which was right while index 0 was also the
	 * default. Since the ruling of 2026-09-19 09:45 it is not: the struct opens
	 * on skeleton auto / joints whole / units game / bones body / textures copy.
	 * The dialog states the struct, never a second copy of it, so it is set from
	 * a default-constructed GltfExportOptions and from nothing else -- no
	 * QSettings, no remembered last export. */
	setOptions( GltfExportOptions() );

	refreshSummary();
}

void GltfExportDialog::addRow( const QString & label, QWidget * control )
{
	grid->addWidget( nameLabel( label, this ), nextRow, 0, Qt::AlignLeft | Qt::AlignVCenter );
	grid->addWidget( control, nextRow, 1 );
	labels << label;
	nextRow++;
}

// ---------------------------------------------------------------------------

GltfExportOptions GltfExportDialog::options() const
{
	GltfExportOptions o;

	switch ( cbSkeleton->currentIndex() ) {
	case 1:  o.skeleton = GltfExportOptions::Skeleton::Auto; break;
	case 2:  o.skeleton = GltfExportOptions::Skeleton::Path;
			 o.skeletonPath = edSkeleton->text().trimmed(); break;
	default: o.skeleton = GltfExportOptions::Skeleton::None; break;
	}

	o.joints = cbJoints->currentIndex() == 1
			 ? GltfExportOptions::Joints::Whole : GltfExportOptions::Joints::Weighted;
	o.units = cbUnits->currentIndex() == 1
			? GltfExportOptions::Units::GameUnits : GltfExportOptions::Units::Metres;

	for ( int i = 0; i < lwParts->count(); i++ )
		o.parts << lwParts->item( i )->text();

	o.bones = cbBones->currentIndex() == 1
			? GltfExportOptions::Bones::Body : GltfExportOptions::Bones::All;

	switch ( cbTextures->currentIndex() ) {
	case 1:  o.textures = GltfExportOptions::Textures::Copy; break;
	case 2:  o.textures = GltfExportOptions::Textures::Png; break;
	default: o.textures = GltfExportOptions::Textures::Reference; break;
	}

	switch ( cbRootMotion->currentIndex() ) {
	case 1:  o.rootMotion = GltfExportOptions::RootMotion::Root; break;
	case 2:  o.rootMotion = GltfExportOptions::RootMotion::Object; break;
	default: o.rootMotion = GltfExportOptions::RootMotion::Strip; break;
	}

	o.includeClip = ckClip->isChecked();
	o.bakeBodyBuild = ckBuild->isChecked();
	o.buildCycleClip = ckBuildCycle->isChecked();
	return o;
}

void GltfExportDialog::setOptions( const GltfExportOptions & o )
{
	switch ( o.skeleton ) {
	case GltfExportOptions::Skeleton::Auto: cbSkeleton->setCurrentIndex( 1 ); break;
	case GltfExportOptions::Skeleton::Path: cbSkeleton->setCurrentIndex( 2 ); break;
	default: cbSkeleton->setCurrentIndex( 0 ); break;
	}
	edSkeleton->setText( o.skeletonPath );

	cbJoints->setCurrentIndex( o.joints == GltfExportOptions::Joints::Whole ? 1 : 0 );
	cbUnits->setCurrentIndex( o.units == GltfExportOptions::Units::GameUnits ? 1 : 0 );

	lwParts->clear();
	for ( const QString & p : o.parts )
		lwParts->addItem( p );

	cbBones->setCurrentIndex( o.bones == GltfExportOptions::Bones::Body ? 1 : 0 );

	int ti = 0;
	if ( o.textures == GltfExportOptions::Textures::Copy )
		ti = 1;
	else if ( o.textures == GltfExportOptions::Textures::Png )
		ti = 2;
	if ( ti >= cbTextures->count() )   // PNG asked for on a build with no converter
		ti = 1;
	cbTextures->setCurrentIndex( ti );

	switch ( o.rootMotion ) {
	case GltfExportOptions::RootMotion::Root:   cbRootMotion->setCurrentIndex( 1 ); break;
	case GltfExportOptions::RootMotion::Object: cbRootMotion->setCurrentIndex( 2 ); break;
	default: cbRootMotion->setCurrentIndex( 0 ); break;
	}

	ckClip->setChecked( o.includeClip && haveClip );
	ckBuild->setChecked( o.bakeBodyBuild );
	ckBuildCycle->setEnabled( o.bakeBodyBuild );
	ckBuildCycle->setChecked( o.buildCycleClip && o.bakeBodyBuild );
	refreshSummary();
}

int GltfExportDialog::rowCount() const { return labels.size(); }
int GltfExportDialog::controlCount() const { return nextRow; }
QStringList GltfExportDialog::rowLabels() const { return labels; }
QString GltfExportDialog::summaryText() const { return lbSummary ? lbSummary->text() : QString(); }

// ---------------------------------------------------------------------------

void GltfExportDialog::browseSkeleton()
{
	QString start = edSkeleton->text().trimmed();
	if ( start.isEmpty() && !nifPath.isEmpty() )
		start = QFileInfo( nifPath ).absolutePath();
	const QString f = QFileDialog::getOpenFileName( this, tr( "Skeleton NIF" ), start,
													QStringLiteral( "NIF (*.nif)" ) );
	if ( !f.isEmpty() ) {
		edSkeleton->setText( QDir::toNativeSeparators( f ) );
		cbSkeleton->setCurrentIndex( 2 );
	}
}

void GltfExportDialog::addPart()
{
	QString start = nifPath.isEmpty() ? QString() : QFileInfo( nifPath ).absolutePath();
	const QStringList fs = QFileDialog::getOpenFileNames( this, tr( "Add part" ), start,
														  QStringLiteral( "NIF (*.nif)" ) );
	for ( const QString & f : fs )
		lwParts->addItem( QDir::toNativeSeparators( f ) );
	refreshSummary();
}

void GltfExportDialog::removePart()
{
	qDeleteAll( lwParts->selectedItems() );
	refreshSummary();
}

void GltfExportDialog::refreshSummary()
{
	if ( !lbSummary )
		return;
	lbSummary->setText( gltfExportOptionsSummary( options() ).join( QStringLiteral( "  " ) ) );
}
