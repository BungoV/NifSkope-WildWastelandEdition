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
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
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

//! Plugin list accepting .esm/.esp/.esl drops from the file manager, with
//! drag-reordering of the load order (later file wins).
class PluginListWidget final : public QListWidget
{
public:
	PluginListWidget( QWidget * parent ) : QListWidget( parent )
	{
		setSelectionMode( QAbstractItemView::SingleSelection );
		setDragDropMode( QAbstractItemView::InternalMove );
		setDefaultDropAction( Qt::MoveAction );
		setAcceptDrops( true );
	}

protected:
	static bool pluginUrls( const QMimeData * mime )
	{
		if ( !mime->hasUrls() )
			return false;
		for ( const QUrl & u : mime->urls() ) {
			const QString f = u.toLocalFile();
			if ( f.endsWith( QLatin1String( ".esm" ), Qt::CaseInsensitive )
				|| f.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive )
				|| f.endsWith( QLatin1String( ".esl" ), Qt::CaseInsensitive ) )
				return true;
		}
		return false;
	}

	void dragEnterEvent( QDragEnterEvent * e ) override
	{
		if ( pluginUrls( e->mimeData() ) )
			e->acceptProposedAction();
		else
			QListWidget::dragEnterEvent( e );
	}

	void dragMoveEvent( QDragMoveEvent * e ) override
	{
		if ( pluginUrls( e->mimeData() ) )
			e->acceptProposedAction();
		else
			QListWidget::dragMoveEvent( e );
	}

	void dropEvent( QDropEvent * e ) override
	{
		if ( pluginUrls( e->mimeData() ) ) {
			for ( const QUrl & u : e->mimeData()->urls() ) {
				const QString f = u.toLocalFile();
				if ( f.endsWith( QLatin1String( ".esm" ), Qt::CaseInsensitive )
					|| f.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive )
					|| f.endsWith( QLatin1String( ".esl" ), Qt::CaseInsensitive ) )
					addItem( f );
			}
			e->acceptProposedAction();
		} else {
			QListWidget::dropEvent( e );
		}
	}
};

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

		grid->addWidget( new QLabel( tr( "Plugins (load order, later wins)" ), this ), row, 0 );
		pluginList = new PluginListWidget( this );
		pluginList->setMaximumHeight( 96 );
		pluginList->addItem( QStringLiteral(
			"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" ) );
		pluginList->setToolTip( tr(
			"Ordered master/plugin list; a later file's version of a record wins.\n"
			"Drop .esm/.esp/.esl files here, drag rows to reorder.\n"
			"Form IDs are remapped through each plugin's master list, so mod\n"
			"plugins merge correctly - a listed master must appear in this\n"
			"list (above its dependents) to be resolvable." ) );
		grid->addWidget( pluginList, row, 1, 1, 2 );
		{
			auto col = new QVBoxLayout();
			auto addBtn = new QPushButton( QStringLiteral( "+" ), this );
			auto delBtn = new QPushButton( QStringLiteral( "-" ), this );
			col->addWidget( addBtn );
			col->addWidget( delBtn );
			col->addStretch();
			grid->addLayout( col, row, 3 );
			connect( addBtn, &QPushButton::clicked, this, [this]() {
				const QStringList files = QFileDialog::getOpenFileNames( this,
					tr( "Add plugins" ), QString(),
					QStringLiteral( "Plugins (*.esm *.esp *.esl)" ) );
				for ( const QString & f : files )
					pluginList->addItem( f );
			} );
			connect( delBtn, &QPushButton::clicked, this, [this]() {
				delete pluginList->currentItem();
			} );
			for ( auto sig : { &QAbstractItemModel::rowsInserted,
				&QAbstractItemModel::rowsRemoved } )
				connect( pluginList->model(), sig, this,
					[this]() { scheduleWorldspaceRefresh(); } );
			connect( pluginList->model(), &QAbstractItemModel::rowsMoved, this,
				[this]() { scheduleWorldspaceRefresh(); } );
		}
		row++;

		grid->addWidget( new QLabel( tr( "Data root (mesh/texture sources)" ), this ), row, 0 );
		dataRootEdit = new QLineEdit(
			QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ), this );
		grid->addWidget( dataRootEdit, row, 1, 1, 2 );
		auto browseData = new QPushButton( QStringLiteral( "..." ), this );
		grid->addWidget( browseData, row++, 3 );
		connect( browseData, &QPushButton::clicked, this, [this]() {
			const QString d = QFileDialog::getExistingDirectory( this,
				tr( "Data folder holding the LOD mesh sources" ), dataRootEdit->text() );
			if ( !d.isEmpty() )
				dataRootEdit->setText( d );
		} );

		grid->addWidget( new QLabel( tr( "Impostor cards (optional)" ), this ), row, 0 );
		impostorEdit = new QLineEdit( this );
		impostorEdit->setPlaceholderText( tr( "directory from bake_impostor_cards.sh" ) );
		grid->addWidget( impostorEdit, row, 1, 1, 2 );
		auto browseImp = new QPushButton( QStringLiteral( "..." ), this );
		grid->addWidget( browseImp, row++, 3 );
		connect( browseImp, &QPushButton::clicked, this, [this]() {
			const QString d = QFileDialog::getExistingDirectory( this,
				tr( "Impostor card directory" ), impostorEdit->text() );
			if ( !d.isEmpty() )
				impostorEdit->setText( d );
		} );

		grid->addWidget( new QLabel( tr( "Worldspace" ), this ), row, 0 );
		wsBox = new QComboBox( this );
		grid->addWidget( wsBox, row, 1 );
		grid->addWidget( new QLabel( tr( "Chunk dim" ), this ), row, 2 );
		dimBox = new QComboBox( this );
		dimBox->addItem( tr( "all rings (4+8+16+32)" ) );
		for ( int d : { 4, 8, 16, 32 } )
			dimBox->addItem( QString::number( d ) );
		grid->addWidget( dimBox, row++, 3 );

		grid->addWidget( new QLabel( tr( "Terrain tris/cell at dim 4 (0 = full grid)" ), this ), row, 0 );
		trisSpin = new QSpinBox( this );
		trisSpin->setRange( 0, 2048 );
		trisSpin->setValue( 130 );
		grid->addWidget( trisSpin, row++, 1 );

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
		identityCheck = new QCheckBox( tr( "Object identity + manifests (FO4CS)" ), this );
		identityCheck->setChecked( true );
		terrainIdCheck = new QCheckBox( tr( "Terrain identity channels (FO4CS)" ), this );
		terrainIdCheck->setChecked( true );
		aoCheck = new QCheckBox( tr( "AO bake" ), this );
		aoCheck->setChecked( true );
		waterCheck = new QCheckBox( tr( "LOD water" ), this );
		waterCheck->setChecked( true );
		geomorphCheck = new QCheckBox( tr( "Geomorph weights (CS-only files)" ), this );
		geomorphCheck->setChecked( false );
		atlasCheck = new QCheckBox( tr( "Pack object atlas (draw-call optimization)" ), this );
		atlasCheck->setChecked( false );
		atlasCheck->setToolTip( tr(
			"Packs the generated BTOs' non-tiling textures onto one\n"
			"<ws>.LodgenObjects.DDS sheet. Direct references are stock-legal\n"
			"(the source LOD textures ship in the game's BA2s), so this is\n"
			"purely an optimization. Never reuses vanilla's atlas name." ) );
		previewCheck = new QCheckBox( tr( "Live preview in the workspace" ), this );
		previewCheck->setChecked( true );
		auto toggles = new QGridLayout();
		layout->addLayout( toggles );
		toggles->addWidget( terrainCheck, 0, 0 );
		toggles->addWidget( objectsCheck, 0, 1 );
		toggles->addWidget( texCheck, 0, 2 );
		toggles->addWidget( waterCheck, 0, 3 );
		toggles->addWidget( identityCheck, 1, 0 );
		toggles->addWidget( terrainIdCheck, 1, 1 );
		toggles->addWidget( aoCheck, 1, 2 );
		toggles->addWidget( geomorphCheck, 1, 3 );
		toggles->addWidget( atlasCheck, 2, 0, 1, 2 );
		toggles->addWidget( previewCheck, 2, 2, 1, 2 );

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
		refreshWorldspaces();
	}

private:
	QString pluginString() const
	{
		QStringList files;
		for ( int i = 0; i < pluginList->count(); i++ )
			files.append( pluginList->item( i )->text() );
		return files.join( QChar( ',' ) );
	}

	void scheduleWorldspaceRefresh()
	{
		if ( wsRefreshPending )
			return;
		wsRefreshPending = true;
		QTimer::singleShot( 0, this, [this]() {
			wsRefreshPending = false;
			refreshWorldspaces();
		} );
	}

	void refreshWorldspaces()
	{
		wsBox->clear();
		QString error;
		const auto worlds = EsmWorld::listWorldspaces( pluginString(), &error );
		for ( const auto & w : worlds )
			wsBox->addItem( QString( "%1  (%2)" ).arg( w.second )
				.arg( w.first, 8, 16, QChar( '0' ) ), w.first );
		if ( wsBox->count() == 0 )
			wsBox->addItem( tr( "no worldspaces found" ), 0U );
	}

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
		if ( !world->load( pluginString(),
			wsBox->currentData().toUInt(), &error ) ) {
			progress->setFormat( tr( "ESM: %1" ).arg( error ) );
			world.reset();
			return;
		}
		QVector<int> dims;
		if ( dimBox->currentIndex() == 0 )
			dims = { 4, 8, 16, 32 };
		else
			dims = { dimBox->currentText().toInt() };
		auto floorTo = []( int v, int m ) {
			return v >= 0 ? v - v % m : -( ( -v + m - 1 ) / m ) * m;
		};
		queue.clear();
		for ( int d : dims )
			for ( int cy = floorTo( y0Spin->value(), d ); cy <= y1Spin->value(); cy += d )
				for ( int cx = floorTo( x0Spin->value(), d ); cx <= x1Spin->value(); cx += d )
					queue.append( ChunkJob{ d, cx, cy } );
		done = 0;
		writtenBto.clear();
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
			QString tail;
			if ( !cancelled && !writtenBto.isEmpty() && atlasCheck->isChecked() ) {
				/* Optional draw-call optimization. Direct source-texture
				 * refs are stock-legal (they ship in the game's BA2s — the
				 * "CK-only" scare was a broken membership probe, see
				 * docs/MISTAKES.md 2026-08-31b). Own name, never
				 * vanilla's. */
				const QString ws = world->worldspaceEdid();
				const QString atlasDir = texDir + QStringLiteral( "/Objects" );
				QDir().mkpath( atlasDir );
				progress->setFormat( tr( "packing the object atlas…" ) );
				QCoreApplication::processEvents();
				QString aerr;
				// own name, never vanilla's: a loose "<ws>.Objects.DDS"
				// shadows the archived sheet under every vanilla BTO still
				// in play
				if ( lodgenBuildAtlas( writtenBto, dataRootEdit->text(),
					atlasDir + "/" + ws + QStringLiteral( ".LodgenObjects" ),
					QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.LodgenObjects" )
						.arg( ws ), outEdit->text(), &aerr ) )
					tail = tr( ", atlas written" );
				else
					tail = tr( ", atlas: %1" ).arg( aerr );
			}
			progress->setFormat( cancelled
				? tr( "cancelled after %1 chunk(s)" ).arg( done )
				: tr( "done — %1 chunk(s)%2" ).arg( done ).arg( tail ) );
			running = false;
			cancelled = false;
			startButton->setEnabled( true );
			writtenBto.clear();
			world.reset();
			return;
		}
		const int dim = queue[done].dim;
		const int cx = queue[done].cx, cy = queue[done].cy;
		progress->setFormat( tr( "dim %1 chunk (%2,%3) — %v of %m" )
			.arg( dim ).arg( cx ).arg( cy ) );

		const QString stem = QString( "%1.%2.%3.%4" )
			.arg( world->worldspaceEdid() ).arg( dim ).arg( cx ).arg( cy );
		if ( terrainCheck->isChecked() ) {
			NifModel nif;
			LodgenTerrainOptions opts;
			opts.dim = dim;
			opts.water = waterCheck->isChecked();
			opts.targetTrisPerCell = trisSpin->value();
			opts.terrainIdentity = terrainIdCheck->isChecked();
			opts.geomorph = geomorphCheck->isChecked();
			QString cerr;
			if ( lodgenBuildTerrainChunk( &nif, *world, cx, cy, opts, &cerr ) ) {
				nif.saveToFile( meshDir + "/" + stem + QStringLiteral( ".BTR" ) );
				preview( nif, cx, cy, stem + QStringLiteral( "_btr" ) );
				if ( texCheck->isChecked() )
					lodgenBakeTerrainTextures( *world, cx, cy, dim,
						dataRootEdit->text(), texDir, &cerr );
			}
		}
		if ( objectsCheck->isChecked() ) {
			NifModel nif;
			LodgenObjectOptions opts;
			opts.dim = dim;
			opts.identity = identityCheck->isChecked();
			opts.bakeAO = aoCheck->isChecked();
			opts.dataRoot = dataRootEdit->text();
			opts.impostorDir = impostorEdit->text();
			QString manifest, cerr;
			if ( lodgenBuildObjectChunk( &nif, *world, cx, cy, opts, &manifest, &cerr ) ) {
				const QString path = meshDir + "/" + stem + QStringLiteral( ".BTO" );
				if ( nif.saveToFile( path ) )
					writtenBto.append( path );
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
		// BTO shapes self-place (world translation baked, per vanilla) —
		// only terrain needs the preview offset
		if ( tag.endsWith( QStringLiteral( "_bto" ) ) ) {
			const QString previewPath = QDir::tempPath()
				+ QStringLiteral( "/lodgen_preview_" ) + tag + QStringLiteral( ".nif" );
			if ( nif.saveToFile( previewPath ) )
				skope->addWorkspaceDocumentFromFile( previewPath );
			return;
		}
		QModelIndex root = nif.getBlockIndex( 0 );
		nif.set<Vector3>( root, "Translation",
			Vector3( float( cx ) * 4096.0f, float( cy ) * 4096.0f, 0.0f ) );
		const QString previewPath = QDir::tempPath()
			+ QStringLiteral( "/lodgen_preview_" ) + tag + QStringLiteral( ".nif" );
		if ( nif.saveToFile( previewPath ) )
			skope->addWorkspaceDocumentFromFile( previewPath );
		nif.set<Vector3>( root, "Translation", Vector3() );
	}

	struct ChunkJob { int dim, cx, cy; };

	NifSkope * skope;
	QListWidget * pluginList;
	bool wsRefreshPending = false;
	QLineEdit * outEdit, * dataRootEdit, * impostorEdit;
	QComboBox * wsBox, * dimBox;
	QSpinBox * x0Spin, * y0Spin, * x1Spin, * y1Spin, * trisSpin;
	QCheckBox * terrainCheck, * objectsCheck, * texCheck, * identityCheck,
		* terrainIdCheck, * aoCheck, * waterCheck, * geomorphCheck,
		* atlasCheck, * previewCheck;
	QProgressBar * progress;
	QPushButton * startButton, * cancelButton;
	std::unique_ptr<EsmWorld> world;
	QVector<ChunkJob> queue;
	QStringList writtenBto;
	QString meshDir, texDir;
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
