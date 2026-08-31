/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"
#include "esmdata.h"
#include "nifskope.h"
#include "model/nifmodel.h"
#include "spellbook.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

/* The World LOD manager (docs/LODGEN_PLAN.md "Where it lives"): the GUI face
 * over the lodgen generators. One chunk is built per event-loop tick so the
 * window stays live and Cancel lands between chunks; every finished chunk is
 * also spliced into the workspace as a Loaded-NIFs document whose root
 * carries the chunk's WORLD translation, so the worldspace assembles tile by
 * tile in the viewport while the real (translation-free, engine-placed)
 * files land in the output folder. */

namespace
{

class LodgenManagerDialog final : public QDialog
{
public:
	explicit LodgenManagerDialog( NifSkope * win )
		: QDialog( win ), skope( win )
	{
		setWindowTitle( tr( "World LOD Generator" ) );
		setAttribute( Qt::WA_DeleteOnClose );
		auto layout = new QVBoxLayout( this );

		auto grid = new QGridLayout();
		layout->addLayout( grid );
		int row = 0;

		grid->addWidget( new QLabel( tr( "ESM" ), this ), row, 0 );
		esmEdit = new QLineEdit(
			QStringLiteral( "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" ), this );
		grid->addWidget( esmEdit, row, 1, 1, 2 );
		auto browseEsm = new QPushButton( QStringLiteral( "..." ), this );
		grid->addWidget( browseEsm, row++, 3 );
		connect( browseEsm, &QPushButton::clicked, this, [this]() {
			const QString f = QFileDialog::getOpenFileName( this, tr( "Master file" ),
				esmEdit->text(), QStringLiteral( "ESM (*.esm)" ) );
			if ( !f.isEmpty() )
				esmEdit->setText( f );
		} );

		grid->addWidget( new QLabel( tr( "Worldspace (hex)" ), this ), row, 0 );
		wsEdit = new QLineEdit( QStringLiteral( "3C" ), this );
		grid->addWidget( wsEdit, row, 1 );
		grid->addWidget( new QLabel( tr( "Chunk dim" ), this ), row, 2 );
		dimBox = new QComboBox( this );
		for ( int d : { 4, 8, 16, 32 } )
			dimBox->addItem( QString::number( d ) );
		grid->addWidget( dimBox, row++, 3 );

		auto makeSpin = [this, grid]( int r, int c, const QString & label, int value ) {
			grid->addWidget( new QLabel( label, this ), r, c );
			auto spin = new QSpinBox( this );
			spin->setRange( -128, 127 );
			spin->setValue( value );
			grid->addWidget( spin, r, c + 1 );
			return spin;
		};
		x0Spin = makeSpin( row, 0, tr( "West cell" ), -24 );
		x1Spin = makeSpin( row, 2, tr( "East cell" ), -13 );
		row++;
		y0Spin = makeSpin( row, 0, tr( "South cell" ), 20 );
		y1Spin = makeSpin( row, 2, tr( "North cell" ), 31 );
		row++;

		grid->addWidget( new QLabel( tr( "Output Data folder" ), this ), row, 0 );
		outEdit = new QLineEdit( this );
		grid->addWidget( outEdit, row, 1, 1, 2 );
		auto browseOut = new QPushButton( QStringLiteral( "..." ), this );
		grid->addWidget( browseOut, row++, 3 );
		connect( browseOut, &QPushButton::clicked, this, [this]() {
			const QString d = QFileDialog::getExistingDirectory( this,
				tr( "Output Data folder" ), outEdit->text() );
			if ( !d.isEmpty() )
				outEdit->setText( d );
		} );

		terrainCheck = new QCheckBox( tr( "Terrain (.btr)" ), this );
		terrainCheck->setChecked( true );
		objectsCheck = new QCheckBox( tr( "Objects (.bto)" ), this );
		objectsCheck->setChecked( true );
		texCheck = new QCheckBox( tr( "Bake terrain textures" ), this );
		texCheck->setChecked( true );
		identityCheck = new QCheckBox( tr( "FO4CS identity channels + manifests" ), this );
		identityCheck->setChecked( true );
		previewCheck = new QCheckBox( tr( "Live preview in the workspace" ), this );
		previewCheck->setChecked( true );
		auto toggles = new QGridLayout();
		layout->addLayout( toggles );
		toggles->addWidget( terrainCheck, 0, 0 );
		toggles->addWidget( objectsCheck, 0, 1 );
		toggles->addWidget( texCheck, 0, 2 );
		toggles->addWidget( identityCheck, 1, 0, 1, 2 );
		toggles->addWidget( previewCheck, 1, 2 );

		progress = new QProgressBar( this );
		progress->setTextVisible( true );
		progress->setFormat( tr( "idle" ) );
		layout->addWidget( progress );

		auto buttons = new QDialogButtonBox( this );
		startButton = buttons->addButton( tr( "Generate" ), QDialogButtonBox::AcceptRole );
		cancelButton = buttons->addButton( QDialogButtonBox::Cancel );
		layout->addWidget( buttons );
		connect( startButton, &QPushButton::clicked, this, &LodgenManagerDialog::start );
		connect( cancelButton, &QPushButton::clicked, this, [this]() {
			if ( running )
				cancelled = true;    // honoured between chunks
			else
				close();
		} );
	}

private:
	void start()
	{
		if ( running )
			return;
		if ( outEdit->text().isEmpty() ) {
			progress->setFormat( tr( "choose an output folder first" ) );
			return;
		}
		QString error;
		world = std::make_unique<EsmWorld>();
		if ( !world->load( esmEdit->text(),
			wsEdit->text().toUInt( nullptr, 16 ), &error ) ) {
			progress->setFormat( tr( "ESM: %1" ).arg( error ) );
			world.reset();
			return;
		}
		dim = dimBox->currentText().toInt();
		auto floorTo = []( int v, int m ) {
			return v >= 0 ? v - v % m : -( ( -v + m - 1 ) / m ) * m;
		};
		queue.clear();
		for ( int cy = floorTo( y0Spin->value(), dim ); cy <= y1Spin->value(); cy += dim )
			for ( int cx = floorTo( x0Spin->value(), dim ); cx <= x1Spin->value(); cx += dim )
				queue.append( qMakePair( cx, cy ) );
		done = 0;
		cancelled = false;
		running = true;
		startButton->setEnabled( false );
		progress->setRange( 0, queue.size() );
		progress->setValue( 0 );
		meshDir = outEdit->text() + QStringLiteral( "/meshes/terrain/" )
			+ world->worldspaceEdid();
		texDir = outEdit->text() + QStringLiteral( "/textures/terrain/" )
			+ world->worldspaceEdid();
		QDir().mkpath( meshDir );
		if ( texCheck->isChecked() )
			QDir().mkpath( texDir );
		QTimer::singleShot( 0, this, &LodgenManagerDialog::step );
	}

	void step()
	{
		if ( cancelled || done >= queue.size() ) {
			progress->setFormat( cancelled
				? tr( "cancelled after %1 chunk(s)" ).arg( done )
				: tr( "done — %1 chunk(s)" ).arg( done ) );
			running = false;
			cancelled = false;
			startButton->setEnabled( true );
			world.reset();
			return;
		}
		const int cx = queue[done].first, cy = queue[done].second;
		progress->setFormat( tr( "chunk (%1,%2) — %v of %m" ).arg( cx ).arg( cy ) );

		const QString stem = QString( "%1.%2.%3.%4" )
			.arg( world->worldspaceEdid() ).arg( dim ).arg( cx ).arg( cy );
		if ( terrainCheck->isChecked() ) {
			NifModel nif;
			LodgenTerrainOptions opts;
			opts.dim = dim;
			opts.terrainIdentity = identityCheck->isChecked();
			QString cerr;
			if ( lodgenBuildTerrainChunk( &nif, *world, cx, cy, opts, &cerr ) ) {
				nif.saveToFile( meshDir + "/" + stem + QStringLiteral( ".BTR" ) );
				preview( nif, cx, cy, stem + QStringLiteral( "_btr" ) );
				if ( texCheck->isChecked() )
					lodgenBakeTerrainTextures( *world, cx, cy, dim,
						QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ),
						texDir, &cerr );
			}
		}
		if ( objectsCheck->isChecked() ) {
			NifModel nif;
			LodgenObjectOptions opts;
			opts.dim = dim;
			opts.identity = identityCheck->isChecked();
			QString manifest, cerr;
			if ( lodgenBuildObjectChunk( &nif, *world, cx, cy, opts, &manifest, &cerr ) ) {
				const QString path = meshDir + "/" + stem + QStringLiteral( ".BTO" );
				nif.saveToFile( path );
				if ( opts.identity ) {
					QFile mf( path + QStringLiteral( ".manifest.txt" ) );
					if ( mf.open( QIODevice::WriteOnly | QIODevice::Text ) )
						mf.write( manifest.toUtf8() );
				}
				preview( nif, cx, cy, stem + QStringLiteral( "_bto" ) );
			}
		}
		done++;
		progress->setValue( done );
		QTimer::singleShot( 0, this, &LodgenManagerDialog::step );
	}

	//! Splice a finished chunk into the workspace: the preview copy's root
	//! gets the chunk's WORLD translation (the shipped file stays at the
	//! origin — the engine places it by filename).
	void preview( NifModel & nif, int cx, int cy, const QString & tag )
	{
		if ( !previewCheck->isChecked() || !skope )
			return;
		QModelIndex root = nif.getBlockIndex( 0 );
		nif.set<Vector3>( root, "Translation",
			Vector3( float( cx ) * 4096.0f, float( cy ) * 4096.0f, 0.0f ) );
		const QString previewPath = QDir::tempPath()
			+ QStringLiteral( "/lodgen_preview_" ) + tag + QStringLiteral( ".nif" );
		if ( nif.saveToFile( previewPath ) )
			skope->addWorkspaceDocumentFromFile( previewPath );
		nif.set<Vector3>( root, "Translation", Vector3() );
	}

	NifSkope * skope;
	QLineEdit * esmEdit, * wsEdit, * outEdit;
	QComboBox * dimBox;
	QSpinBox * x0Spin, * y0Spin, * x1Spin, * y1Spin;
	QCheckBox * terrainCheck, * objectsCheck, * texCheck, * identityCheck, * previewCheck;
	QProgressBar * progress;
	QPushButton * startButton, * cancelButton;
	std::unique_ptr<EsmWorld> world;
	QVector<QPair<int, int>> queue;
	QString meshDir, texDir;
	int dim = 4;
	int done = 0;
	bool running = false;
	bool cancelled = false;
};

//! The launcher spell: Batch > World LOD Generator.
class spWorldLodGenerator final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "World LOD Generator..." ); }
	QString page() const override final { return Spell::tr( "Batch" ); }
	bool instant() const override final { return false; }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel *, const QModelIndex & ) override final
	{
		return true;
	}

	QModelIndex cast( NifModel *, const QModelIndex & index ) override final
	{
		NifSkope * win = qobject_cast<NifSkope *>( QApplication::activeWindow() );
		if ( !win ) {
			for ( QWidget * w : QApplication::topLevelWidgets() )
				if ( ( win = qobject_cast<NifSkope *>( w ) ) )
					break;
		}
		if ( win ) {
			auto dlg = new LodgenManagerDialog( win );
			dlg->show();
		}
		return index;
	}
};

REGISTER_SPELL( spWorldLodGenerator )

} // namespace
