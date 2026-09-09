/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"
#include "io/lodmfile.h"
#include "gl/glproperty.h"
#include "glview.h"
#include "esmdata.h"
#include "gamemanager.h"
#include "nifskope.h"
#include "model/nifmodel.h"
#include "spellbook.h"
#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
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
#include <QButtonGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QFileInfo>
#include <QPainter>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QToolButton>

#include <atomic>
#include <thread>

#include "lodtfile.h"

/* The World LOD manager (docs/LODGEN_PLAN.md "Where it lives"): the GUI face
 * over the lodgen generators. One chunk is built per event-loop tick so the
 * window stays live and Cancel lands between chunks; every finished chunk is
 * also spliced into the workspace as a Loaded-NIFs document whose root
 * carries the chunk's WORLD translation, so the worldspace assembles tile by
 * tile in the viewport while the real (translation-free, engine-placed)
 * files land in the output folder. */

namespace
{

/*! An ordered list you drag to reorder - the block list's drag, which is what
 *  bungo asked the mod priority to reuse - accepting drops from the file
 *  manager of whatever it is a list of.
 *
 *  Two of them: the plugins (.esm/.esp/.esl) and the resources (mod folders and
 *  .ba2/.bsa archives). Both read the same way, MOD ORGANIZER'S way: the LAST
 *  row overrides the ones above it. */
class OrderedPathList final : public QListWidget
{
public:
	OrderedPathList( QWidget * parent, const QStringList & suffixes, bool acceptFolders )
		: QListWidget( parent ), okSuffixes( suffixes ), folders( acceptFolders )
	{
		setSelectionMode( QAbstractItemView::SingleSelection );
		setDragDropMode( QAbstractItemView::InternalMove );
		setDefaultDropAction( Qt::MoveAction );
		setAcceptDrops( true );
	}

	bool accepts( const QString & f ) const
	{
		if ( folders && QFileInfo( f ).isDir() )
			return true;
		for ( const QString & s : okSuffixes )
			if ( f.endsWith( s, Qt::CaseInsensitive ) )
				return true;
		return false;
	}

protected:
	bool ourUrls( const QMimeData * mime ) const
	{
		if ( !mime->hasUrls() )
			return false;
		for ( const QUrl & u : mime->urls() )
			if ( accepts( u.toLocalFile() ) )
				return true;
		return false;
	}

	void dragEnterEvent( QDragEnterEvent * e ) override
	{
		if ( ourUrls( e->mimeData() ) )
			e->acceptProposedAction();
		else
			QListWidget::dragEnterEvent( e );
	}

	void dragMoveEvent( QDragMoveEvent * e ) override
	{
		if ( ourUrls( e->mimeData() ) )
			e->acceptProposedAction();
		else
			QListWidget::dragMoveEvent( e );
	}

	void dropEvent( QDropEvent * e ) override
	{
		if ( ourUrls( e->mimeData() ) ) {
			for ( const QUrl & u : e->mimeData()->urls() ) {
				const QString f = u.toLocalFile();
				if ( accepts( f ) )
					addItem( f );
			}
			e->acceptProposedAction();
		} else {
			QListWidget::dropEvent( e );
		}
	}

private:
	QStringList okSuffixes;
	bool folders;
};

/*! Top-down map of the worldspace, one rectangle per cell, painted by bake
 *  state. North is up. It is the "watch the chunks bake" view: coarse
 *  pyramid levels fill the whole map first, each finer level refines it, and
 *  chunk jobs (.bto/.btr) stamp their squares as they land. Repaints are
 *  coalesced on a timer so a worker reporting thousands of blocks a second
 *  does not turn into thousands of paints. */
class LodgenProgressMap final : public QWidget
{
public:
	enum State : quint8 { Pending = 0, Scanned, Level3, Level2, Level1, Fine, Chunk, Failed };

	explicit LodgenProgressMap( QWidget * parent ) : QWidget( parent )
	{
		setObjectName( QStringLiteral( "LodgenProgressMap" ) );
		setMinimumSize( 160, 160 );
		setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
		setToolTip( tr( "The worldspace from above, north up. Cells fill in as they bake:\n"
			"grey = not yet, blue = scanned, deepening green = each finer LOD level,\n"
			"gold = an object/terrain chunk landed, red = failed." ) );
		repaintTimer.setInterval( 40 );
		repaintTimer.setSingleShot( true );
		connect( &repaintTimer, &QTimer::timeout, this, [this]() { update(); } );
	}

	void setWorld( int minX, int minY, int maxX, int maxY )
	{
		x0 = minX; y0 = minY;
		cw = qMax( 0, maxX - minX + 1 );
		ch = qMax( 0, maxY - minY + 1 );
		state.assign( size_t( cw ) * size_t( ch ), Pending );
		label.clear();
		schedule();
	}
	void clearWorld()
	{
		cw = ch = 0;
		state.clear();
		schedule();
	}
	void setLabel( const QString & s ) { label = s; schedule(); }
	QSize sizeHint() const override { return QSize( 320, 320 ); }

	//! a whole cell row (pass one)
	void markRow( int row, State st )
	{
		if ( row < 0 || row >= ch )
			return;
		for ( int x = 0; x < cw; x++ )
			raise( x, row, st );
		schedule();
	}
	//! a level-L block at (i, j): cells [i<<L, (i+1)<<L) x [j<<L, (j+1)<<L)
	void markLevel( int L, int i, int j, State st )
	{
		const int n = 1 << L;
		for ( int y = j * n; y < ( j + 1 ) * n; y++ )
			for ( int x = i * n; x < ( i + 1 ) * n; x++ )
				raise( x, y, st );
		schedule();
	}
	//! a dim x dim chunk whose south-west cell is (cx, cy) in WORLD cells
	void markChunk( int dim, int cx, int cy, State st )
	{
		for ( int y = cy - y0; y < cy - y0 + dim; y++ )
			for ( int x = cx - x0; x < cx - x0 + dim; x++ )
				raise( x, y, st );
		schedule();
	}
	int cellsDone( State atLeast ) const
	{
		int n = 0;
		for ( quint8 v : state )
			if ( v >= atLeast && v != Failed )
				n++;
		return n;
	}

protected:
	void paintEvent( QPaintEvent * ) override
	{
		QPainter p( this );
		p.fillRect( rect(), palette().color( QPalette::Base ) );
		if ( cw <= 0 || ch <= 0 ) {
			p.setPen( palette().color( QPalette::PlaceholderText ) );
			p.drawText( rect(), Qt::AlignCenter, tr( "pick a worldspace to see it here" ) );
			return;
		}
		// fit the cell grid into the widget, keep the aspect, centre it
		const int pad = 4;
		const double sx = double( width() - 2 * pad ) / cw;
		const double sy = double( height() - 2 * pad - ( label.isEmpty() ? 0 : 16 ) ) / ch;
		const double s = qMax( 0.01, qMin( sx, sy ) );
		const double ox = ( width() - s * cw ) * 0.5;
		const double oy = pad + ( height() - 2 * pad - ( label.isEmpty() ? 0 : 16 ) - s * ch ) * 0.5;
		static const QColor colours[8] = {
			QColor( 62, 62, 66 ),      // Pending
			QColor( 58, 78, 110 ),     // Scanned
			QColor( 46, 96, 78 ),      // Level3
			QColor( 56, 122, 86 ),     // Level2
			QColor( 72, 150, 96 ),     // Level1
			QColor( 96, 184, 110 ),    // Fine
			QColor( 214, 176, 64 ),    // Chunk
			QColor( 190, 60, 50 )      // Failed
		};
		p.setPen( Qt::NoPen );
		for ( int y = 0; y < ch; y++ ) {
			// row 0 is SOUTH; the map is north-up, so it goes at the bottom
			const double top = oy + s * ( ch - 1 - y );
			for ( int x = 0; x < cw; x++ ) {
				p.setBrush( colours[state[size_t( y ) * cw + size_t( x )] & 7] );
				p.drawRect( QRectF( ox + s * x, top, qMax( 1.0, s - ( s > 3 ? 0.5 : 0 ) ),
					qMax( 1.0, s - ( s > 3 ? 0.5 : 0 ) ) ) );
			}
		}
		p.setPen( palette().color( QPalette::Mid ) );
		p.setBrush( Qt::NoBrush );
		p.drawRect( QRectF( ox, oy, s * cw, s * ch ) );
		if ( !label.isEmpty() ) {
			p.setPen( palette().color( QPalette::Text ) );
			p.drawText( QRect( 0, height() - 16, width(), 16 ), Qt::AlignCenter, label );
		}
	}

private:
	void raise( int x, int y, State st )
	{
		if ( x < 0 || y < 0 || x >= cw || y >= ch )
			return;
		quint8 & v = state[size_t( y ) * cw + size_t( x )];
		if ( st == Failed || v < quint8( st ) )
			v = quint8( st );
	}
	void schedule()
	{
		if ( !repaintTimer.isActive() )
			repaintTimer.start();
	}
	int x0 = 0, y0 = 0, cw = 0, ch = 0;
	std::vector<quint8> state;
	QString label;
	QTimer repaintTimer;
};

/*! A collapsible sub-panel with its check box in the header - Blender's
 *  panel with a toggle in its title. The arrow folds the body; the box is
 *  the setting. A greyed body stays open (the caller's enable rule greys
 *  it), because a greyed body still says what the output would have done;
 *  the fold is the user's, and persists. */
class LodgenSection final : public QWidget
{
public:
	LodgenSection( QCheckBox * check, const QString & key, bool expandedByDefault, QWidget * parent )
		: QWidget( parent ), settingsKey( QStringLiteral( "LodGeneration/expanded/" ) + key )
	{
		setObjectName( QStringLiteral( "Lodgen" ) + key + QStringLiteral( "Section" ) );
		auto * v = new QVBoxLayout( this );
		v->setContentsMargins( 0, 0, 0, 0 );
		v->setSpacing( 4 );
		auto * header = new QHBoxLayout();
		header->setContentsMargins( 0, 0, 0, 0 );
		header->setSpacing( 2 );
		arrow = new QToolButton( this );
		arrow->setObjectName( QStringLiteral( "Lodgen" ) + key + QStringLiteral( "Expander" ) );
		arrow->setAutoRaise( true );
		arrow->setFixedSize( 16, 16 );
		arrow->setToolTip( tr( "Show or hide this output's settings" ) );
		header->addWidget( arrow, 0 );
		header->addWidget( check, 1 );
		v->addLayout( header );
		bodyWidget = new QWidget( this );
		bodyWidget->setObjectName( QStringLiteral( "Lodgen" ) + key + QStringLiteral( "Body" ) );
		v->addWidget( bodyWidget );
		open = QSettings().value( settingsKey, expandedByDefault ).toBool();
		apply();
		connect( arrow, &QToolButton::clicked, this, [this]() {
			open = !open;
			QSettings().setValue( settingsKey, open );
			apply();
		} );
	}
	QWidget * body() const { return bodyWidget; }
	bool isOpen() const { return open; }

private:
	void apply()
	{
		bodyWidget->setVisible( open );
		arrow->setArrowType( open ? Qt::DownArrow : Qt::RightArrow );
	}
	QToolButton * arrow = nullptr;
	QWidget * bodyWidget = nullptr;
	QString settingsKey;
	bool open = true;
};

/*! The LOD Generation workspace panel.
 *
 *  What it replaces: a modal "World LOD Generator" dialog. What it is: a dock
 *  that stays open while you work, organised the way a person thinks about the
 *  job - WHERE the world comes from, WHAT to make, HOW MUCH of it, and a map of
 *  it filling in.
 *
 *  Whole-worldspace outputs (the .lodl landscape file, its AO-only refresh,
 *  the shadow heightmap) run on a worker thread against their own EsmWorld
 *  and report through the writer's progress callback; the per-chunk outputs
 *  (.bto objects, legacy .btr terrain) run one chunk per event-loop tick on
 *  the GUI thread as before, because they preview into the viewport. Cancel
 *  is honoured by both: the writer removes its partial file, the chunk loop
 *  stops between chunks. */
class LodgenPanel final : public QWidget
{
public:
	explicit LodgenPanel( NifSkope * win, QWidget * parent = nullptr )
		: QWidget( parent ), skope( win )
	{
		setObjectName( QStringLiteral( "LodgenPanel" ) );
		/* The house style, not a form of its own.
		 *
		 * The first build of this panel had group-box titles for sections,
		 * plain Qt spin boxes for every number, selectors in their default
		 * chrome, two settings to a row and the explanation of each output
		 * after a dash in its label. bungo saw all of it from one screenshot,
		 * the way he caught the collision fields on 2026-08-05. Each of those
		 * is a helper this fork already had and this file did not call:
		 * wwHeading (the group-box title was one of the four idioms it
		 * retired), wwMakeScrubField (there is one number field in the
		 * program), wwMatchFieldStyle (a selector next to a scrub field is
		 * the same species of control), and the label | field grid with one
		 * setting per row and the explanation in the tooltip. The self-test
		 * counts each of them, so the next panel cannot ship without them.
		 *
		 * THREE BANDS. The settings scroll; the progress map and bar sit
		 * below them on a splitter the user drags; the summary line and the
		 * buttons are pinned under both and never scroll away. The first
		 * layout put Generate at the bottom of one long scrolling column,
		 * below the map, off the screen at 1080p. */
		auto * outer = new QVBoxLayout( this );
		outer->setContentsMargins( 0, 0, 0, 0 );
		outer->setSpacing( 0 );
		splitter = new QSplitter( Qt::Vertical, this );
		splitter->setObjectName( QStringLiteral( "LodgenSplitter" ) );
		splitter->setChildrenCollapsible( false );
		outer->addWidget( splitter, 1 );

		scroll = new QScrollArea( splitter );
		scroll->setObjectName( QStringLiteral( "LodgenSettingsScroll" ) );
		scroll->setWidgetResizable( true );
		scroll->setFrameShape( QFrame::NoFrame );
		auto * page = new QWidget( scroll );
		page->setObjectName( QStringLiteral( "LodgenSettingsPage" ) );
		auto * layout = new QVBoxLayout( page );
		layout->setContentsMargins( 6, 6, 6, 6 );
		layout->setSpacing( 5 );
		scroll->setWidget( page );
		splitter->addWidget( scroll );
		QSettings settings;

		/* One label | field grid per section, the field column stretching so
		 * every value is the same width; `indent` sets a sub-form under its
		 * parent check box. The label column is one width for the whole panel
		 * (less the indent), so the values line up down the page instead of
		 * each section starting its own column - the first grab of this
		 * panel had seven different value edges. add() hands the label back
		 * so a field that greys can grey its label with it. */
		struct Form
		{
			QGridLayout * g = nullptr;
			int row = 0;
			QLabel * add( QWidget * parent, const QString & label, QWidget * field, Qt::Alignment align = Qt::Alignment() )
			{
				auto * l = new QLabel( label, parent );
				g->addWidget( l, row, 0, align );
				g->addWidget( field, row++, 1 );
				return l;
			}
			void span( QWidget * w ) { g->addWidget( w, row++, 0, 1, 2 ); }
		};
		const int labelW = 176;		// fits "Overview samples per cell" under its indent
		auto form = [labelW]( int indent ) {
			Form f;
			f.g = new QGridLayout();
			f.g->setContentsMargins( indent, 0, 0, 0 );
			f.g->setHorizontalSpacing( 8 );
			f.g->setVerticalSpacing( 4 );
			f.g->setColumnMinimumWidth( 0, labelW - indent );
			f.g->setColumnStretch( 1, 1 );
			return f;
		};
		// a path field with its browse button, as one value
		auto browseHost = [this, page]( QLineEdit * edit, const QString & title ) {
			auto * host = new QWidget( page );
			auto * h = new QHBoxLayout( host );
			h->setContentsMargins( 0, 0, 0, 0 );
			h->setSpacing( 4 );
			h->addWidget( edit, 1 );
			h->addWidget( browseDirButton( host, edit, title ), 0 );
			return host;
		};

		// ---- Source ---------------------------------------------------------
		/* bungo, 2026-09-06: "in nifskope in lod baker, we can toggle either
		 * specified bake, where we select our plugins, archives or loose files
		 * and their order, or we select a MO2 automatic bake, loaded list that
		 * is already configured in MO2 with the order of plugins, files and
		 * what overwrites what on load known." One selector, two shapes of the
		 * same section - the list is yours to order, or it is MO2's and read
		 * only. Both read MOD ORGANIZER'S way: the LAST entry wins. */
		layout->addWidget( wwHeading( tr( "Source" ), page ) );
		Form src = form( 0 );
		sourceBox = new QComboBox( page );
		sourceBox->setObjectName( QStringLiteral( "LodgenSourceBox" ) );
		sourceBox->addItem( tr( "Specified" ), 0 );
		sourceBox->addItem( tr( "Mod Organizer 2" ), 1 );
		sourceBox->setCurrentIndex( settings.value( QStringLiteral( "LodGeneration/source" ), 0 ).toInt() == 1 ? 1 : 0 );
		sourceBox->setToolTip( tr( "Where the world and its assets come from. Specified is the lists below, in\n"
			"your order. Mod Organizer 2 takes the profile's enabled plugins and their\n"
			"archives from the virtual Data folder NifSkope was launched into - add\n"
			"NifSkope to Mod Organizer's executable list, the way FO4Edit is added." ) );
		wwMatchFieldStyle( sourceBox );
		src.add( page, tr( "Source" ), sourceBox );

		pluginList = new OrderedPathList( page,
			{ QStringLiteral( ".esm" ), QStringLiteral( ".esp" ), QStringLiteral( ".esl" ) }, false );
		pluginList->setObjectName( QStringLiteral( "LodgenPluginList" ) );
		pluginList->setMaximumHeight( 84 );
		{
			const QStringList saved = settings.value( QStringLiteral( "LodGeneration/plugins" ) ).toStringList();
			if ( saved.isEmpty() )
				pluginList->addItem( QStringLiteral( "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" ) );
			else
				pluginList->addItems( saved );
		}
		pluginList->setToolTip( tr(
			"The .esm / .esp / .esl files that describe the world, in load order -\n"
			"a later file's version of a record wins. Drop files here or use +.\n"
			"A plugin's masters must be listed above it." ) );
		auto * pluginHost = new QWidget( page );
		{
			auto * h = new QHBoxLayout( pluginHost );
			h->setContentsMargins( 0, 0, 0, 0 );
			h->setSpacing( 4 );
			h->addWidget( pluginList, 1 );
			auto * col = new QVBoxLayout();
			col->setSpacing( 2 );
			auto * addBtn = new QToolButton( pluginHost );
			addBtn->setText( QStringLiteral( "+" ) );
			addBtn->setToolTip( tr( "Add plugins" ) );
			auto * delBtn = new QToolButton( pluginHost );
			delBtn->setText( QStringLiteral( "\u2212" ) );
			delBtn->setToolTip( tr( "Remove the selected plugin" ) );
			col->addWidget( addBtn );
			col->addWidget( delBtn );
			col->addStretch();
			h->addLayout( col );
			pluginButtons = { addBtn, delBtn };
			connect( addBtn, &QToolButton::clicked, this, [this]() {
				const QStringList files = QFileDialog::getOpenFileNames( this,
					tr( "Add plugins" ), QString(), QStringLiteral( "Plugins (*.esm *.esp *.esl)" ) );
				for ( const QString & f : files )
					pluginList->addItem( f );
			} );
			connect( delBtn, &QToolButton::clicked, this, [this]() { delete pluginList->currentItem(); } );
			for ( auto sig : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved } )
				connect( pluginList->model(), sig, this, [this]() { scheduleWorldspaceRefresh(); } );
			connect( pluginList->model(), &QAbstractItemModel::rowsMoved, this,
				[this]() { scheduleWorldspaceRefresh(); } );
		}
		pluginLabel = src.add( page, tr( "Plugins" ), pluginHost, Qt::AlignTop );

		/* The mods themselves: folders and archives, the same drag list, the
		 * same rule. bungo: "For a specified bake, we can use the nif block
		 * list dragging system for reordering mod priority ... Instead of top
		 * wins, we use MO2's standard, the last one in the order overrides the
		 * previous ones." */
		resourceList = new OrderedPathList( page,
			{ QStringLiteral( ".ba2" ), QStringLiteral( ".bsa" ) }, true );
		resourceList->setObjectName( QStringLiteral( "LodgenResourceList" ) );
		resourceList->setMaximumHeight( 84 );
		resourceList->addItems( settings.value( QStringLiteral( "LodGeneration/resources" ) ).toStringList() );
		resourceList->setToolTip( tr(
			"Mod folders and .ba2 / .bsa archives the meshes, textures and materials come\n"
			"from, in Mod Organizer's order: the LAST row overrides the ones above it.\n"
			"A loose file beats an archive wherever the archive sits in the list, as in the\n"
			"game. The game's own Data folder is read underneath them all. Drop folders or\n"
			"archives here, or use the buttons." ) );
		resourceHost = new QWidget( page );
		{
			auto * h = new QHBoxLayout( resourceHost );
			h->setContentsMargins( 0, 0, 0, 0 );
			h->setSpacing( 4 );
			h->addWidget( resourceList, 1 );
			auto * col = new QVBoxLayout();
			col->setSpacing( 2 );
			auto * addDirBtn = new QToolButton( resourceHost );
			addDirBtn->setObjectName( QStringLiteral( "LodgenResourceAddFolder" ) );
			addDirBtn->setText( QStringLiteral( "+" ) );
			addDirBtn->setToolTip( tr( "Add a mod folder" ) );
			auto * addBa2Btn = new QToolButton( resourceHost );
			addBa2Btn->setObjectName( QStringLiteral( "LodgenResourceAddArchive" ) );
			addBa2Btn->setText( QStringLiteral( "\u25A4" ) );
			addBa2Btn->setToolTip( tr( "Add an archive" ) );
			auto * delResBtn = new QToolButton( resourceHost );
			delResBtn->setText( QStringLiteral( "\u2212" ) );
			delResBtn->setToolTip( tr( "Remove the selected resource" ) );
			col->addWidget( addDirBtn );
			col->addWidget( addBa2Btn );
			col->addWidget( delResBtn );
			col->addStretch();
			h->addLayout( col );
			connect( addDirBtn, &QToolButton::clicked, this, [this]() {
				const QString d = QFileDialog::getExistingDirectory( this, tr( "Add a mod folder" ) );
				if ( !d.isEmpty() )
					resourceList->addItem( d );
			} );
			connect( addBa2Btn, &QToolButton::clicked, this, [this]() {
				const QStringList files = QFileDialog::getOpenFileNames( this,
					tr( "Add archives" ), QString(), QStringLiteral( "Archives (*.ba2 *.bsa)" ) );
				for ( const QString & f : files )
					resourceList->addItem( f );
			} );
			connect( delResBtn, &QToolButton::clicked, this, [this]() { delete resourceList->currentItem(); } );
			for ( auto sig : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved } )
				connect( resourceList->model(), sig, this, [this]() { refreshSummary(); } );
			connect( resourceList->model(), &QAbstractItemModel::rowsMoved, this,
				[this]() { refreshSummary(); } );
		}
		resourceLabel = src.add( page, tr( "Resources" ), resourceHost, Qt::AlignTop );

		//! what MO2 mode found, or why it found nothing
		sourceStatus = new QLabel( page );
		sourceStatus->setObjectName( QStringLiteral( "LodgenSourceStatus" ) );
		sourceStatus->setWordWrap( true );
		sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		src.span( sourceStatus );

		wsBox = new QComboBox( page );
		wsBox->setObjectName( QStringLiteral( "LodgenWorldspaceBox" ) );
		wsBox->setToolTip( tr( "Every worldspace the plugins define. Interiors are not worldspaces." ) );
		wwMatchFieldStyle( wsBox );
		src.add( page, tr( "Worldspace" ), wsBox );
		connect( wsBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int ) { worldChanged(); } );

		/* No assets row. Meshes and textures come from the game's own folders
		 * and archives, as set under Settings > Resources - the same place the
		 * viewer reads them from. The first cut asked for an unpacked Data
		 * folder; bungo: "there's no point in that game data thing". */

		/* The output is the MOD FOLDER itself - bungo's ask, made twice: the
		 * first cut wanted a Data path, the second split it into a mods root
		 * and a name. In Mod Organizer a mod folder IS a Data folder, so this
		 * one field is the place a person thinks in: pick a mod, or make a new
		 * folder in the picker and that is the new mod. The files land inside
		 * it under Terrain\, Textures\Terrain\ and meshes\terrain\. */
		outEdit = new QLineEdit( page );
		outEdit->setObjectName( QStringLiteral( "LodgenOutputEdit" ) );
		{
			QString out = settings.value( QStringLiteral( "LodGeneration/output" ) ).toString();
			if ( out.isEmpty() && QDir( QStringLiteral( "E:/Projects/Fallout 4 Mods/mods" ) ).exists() )
				out = QStringLiteral( "E:/Projects/Fallout 4 Mods/mods/Generated LOD" );
			outEdit->setText( out );
		}
		outEdit->setPlaceholderText( tr( "the mod folder the files land in" ) );
		outEdit->setToolTip( tr( "A mod folder - in Mod Organizer, a folder under its mods folder; it is a Data\n"
			"folder. The landscape file goes to Terrain\\, the heightmap to Textures\\Terrain\\,\n"
			"chunks to meshes\\terrain\\. A folder that does not exist yet is created when you\n"
			"generate; enable it in Mod Organizer afterwards." ) );
		src.add( page, tr( "Output mod" ), browseHost( outEdit, tr( "Output mod folder" ) ) );
		layout->addLayout( src.g );

		// ---- What to generate -----------------------------------------------
		layout->addWidget( wwHeading( tr( "What to generate" ), page ) );
		{
			/* One choice a user can make without knowing the formats: what
			 * will read the files. It sets the outputs and hides the ones the
			 * other reader has no use for, the way Blender's render engine
			 * hides the panels of the engine not chosen. */
			Form f = form( 0 );
			targetBox = new QComboBox( page );
			targetBox->setObjectName( QStringLiteral( "LodgenTargetBox" ) );
			targetBox->addItem( tr( "FO4 Community Shaders" ), 0 );
			targetBox->addItem( tr( "Stock engine" ), 1 );
			targetBox->setToolTip( tr( "What will read the files. FO4 Community Shaders takes the landscape file, the\n"
				"shadow heightmap and the identity channels; the stock engine reads only the\n"
				"object and legacy terrain chunks. Choosing sets the outputs below." ) );
			targetBox->setCurrentIndex( settings.value( QStringLiteral( "LodGeneration/target" ), 0 ).toInt() == 1 ? 1 : 0 );
			wwMatchFieldStyle( targetBox );
			f.add( page, tr( "Target" ), targetBox );
			layout->addLayout( f.g );
		}

		lodtCheck = new QCheckBox( tr( "Landscape file (.lodl)" ), page );
		lodtCheck->setObjectName( QStringLiteral( "LodgenLodtCheck" ) );
		lodtCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/lodt" ), true ).toBool() );
		lodtCheck->setToolTip( tr( "Heights, land textures, water, colour, ground cover and ambient occlusion for\n"
			"the whole worldspace in one file: Terrain\\<worldspace>.lodl." ) );
		lodtSection = new LodgenSection( lodtCheck, QStringLiteral( "Lodt" ), true, page );
		layout->addWidget( lodtSection );
		{
			Form f = form( 24 );
			lodtFullRadio = new QRadioButton( tr( "Generate everything" ), page );
			lodtFullRadio->setObjectName( QStringLiteral( "LodgenLodtFullRadio" ) );
			lodtFullRadio->setChecked( true );
			lodtAoOnlyRadio = new QRadioButton( tr( "Only refresh the AO plane in the existing file" ), page );
			lodtAoOnlyRadio->setObjectName( QStringLiteral( "LodgenLodtAoOnlyRadio" ) );
			lodtAoOnlyRadio->setToolTip( tr( "Recomputes ambient occlusion from the file's own heights and writes it in place.\n"
				"Nothing else in the file changes. Use it after tuning the AO setting." ) );
			auto * grp = new QButtonGroup( page );
			grp->addButton( lodtFullRadio );
			grp->addButton( lodtAoOnlyRadio );
			f.span( lodtFullRadio );
			f.span( lodtAoOnlyRadio );
			aoSpin = new QSpinBox( page );
			aoSpin->setObjectName( QStringLiteral( "LodgenAoSpin" ) );
			aoSpin->setRange( 0, 32 );
			aoSpin->setValue( settings.value( QStringLiteral( "LodGeneration/aoSamples" ), 8 ).toInt() );
			aoSpin->setToolTip( tr( "Resolution of the baked sky-occlusion plane, per cell edge. 8 is 512 units;\n0 leaves it out. Coarse on purpose - AO is smooth." ) );
			QLabel * aoLabel = f.add( page, tr( "AO samples per cell" ), aoSpin );
			ovSpin = new QSpinBox( page );
			ovSpin->setObjectName( QStringLiteral( "LodgenOverviewSpin" ) );
			ovSpin->setRange( 0, 32 );
			ovSpin->setValue( settings.value( QStringLiteral( "LodGeneration/overviewSamples" ), 8 ).toInt() );
			ovSpin->setToolTip( tr( "The always-resident coarse height grid a renderer draws the horizon from." ) );
			QLabel * ovLabel = f.add( page, tr( "Overview samples per cell" ), ovSpin );
			lodtSection->body()->setLayout( f.g );
			auto syncLodt = [this, aoLabel, ovLabel]() {
				const bool on = lodtCheck->isChecked();
				const bool aoOnly = lodtAoOnlyRadio->isChecked();
				lodtFullRadio->setEnabled( on );
				lodtAoOnlyRadio->setEnabled( on );
				for ( QWidget * w : QList<QWidget *>{ aoSpin, ovSpin, aoLabel, ovLabel } )
					w->setEnabled( on && !aoOnly );
			};
			connect( lodtCheck, &QCheckBox::toggled, this, syncLodt );
			connect( lodtAoOnlyRadio, &QRadioButton::toggled, this, syncLodt );
			syncLodt();
		}

		heightmapCheck = new QCheckBox( tr( "Shadow heightmap (.HeightMap.dds)" ), page );
		heightmapCheck->setObjectName( QStringLiteral( "LodgenHeightmapCheck" ) );
		heightmapCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/heightmap" ), true ).toBool() );
		heightmapCheck->setToolTip( tr( "The map FO4CS draws far terrain shadows from, written to\n"
			"Textures\\Terrain\\<worldspace>\\ with the provenance block its loader checks." ) );
		heightmapSection = new LodgenSection( heightmapCheck, QStringLiteral( "Heightmap" ), true, page );
		layout->addWidget( heightmapSection );
		{
			Form f = form( 24 );
			heightmapSizeBox = new QComboBox( page );
			heightmapSizeBox->setObjectName( QStringLiteral( "LodgenHeightmapSizeBox" ) );
			heightmapSizeBox->addItem( tr( "Native - one texel per land sample, lossless" ), 0 );
			heightmapSizeBox->addItem( QStringLiteral( "4096" ), 4096 );
			heightmapSizeBox->addItem( QStringLiteral( "8192" ), 8192 );
			heightmapSizeBox->setToolTip( tr( "Native is 32 texels a cell (6144 for the Commonwealth) and reproduces the\n"
				"reference map byte for byte. A fixed size resamples." ) );
			wwMatchFieldStyle( heightmapSizeBox );
			QLabel * sizeLabel = f.add( page, tr( "Size" ), heightmapSizeBox );
			heightmapSection->body()->setLayout( f.g );
			connect( heightmapCheck, &QCheckBox::toggled, heightmapSizeBox, &QWidget::setEnabled );
			connect( heightmapCheck, &QCheckBox::toggled, sizeLabel, &QWidget::setEnabled );
			heightmapSizeBox->setEnabled( heightmapCheck->isChecked() );
			sizeLabel->setEnabled( heightmapCheck->isChecked() );
		}

		objectsCheck = new QCheckBox( tr( "Object LOD chunks (.bto)" ), page );
		objectsCheck->setObjectName( QStringLiteral( "LodgenObjectsCheck" ) );
		objectsCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/objects" ), false ).toBool() );
		objectsCheck->setToolTip( tr( "Distant buildings, trees and rocks: one mesh per chunk under\n"
			"meshes\\terrain\\<worldspace>\\, for the cells in the chunk range below." ) );
		objectsSection = new LodgenSection( objectsCheck, QStringLiteral( "Objects" ), true, page );
		layout->addWidget( objectsSection );
		{
			Form f = form( 24 );
			identityCheck = new QCheckBox( tr( "Identity channels and manifests" ), page );
			identityCheck->setObjectName( QStringLiteral( "LodgenIdentityCheck" ) );
			identityCheck->setChecked( true );
			identityCheck->setToolTip( tr( "A per-vertex object identity in the chunk and a manifest beside it, which FO4CS\n"
				"reads to treat each distant object as itself. The stock engine ignores both." ) );
			aoCheck = new QCheckBox( tr( "Bake vertex AO" ), page );
			aoCheck->setChecked( true );
			atlasCheck = new QCheckBox( tr( "Pack an object texture atlas" ), page );
			atlasCheck->setObjectName( QStringLiteral( "LodgenAtlasCheck" ) );
			atlasCheck->setChecked( false );
			atlasCheck->setToolTip( tr( "Packs the chunks' non-tiling textures onto one sheet. An optimisation only;\nnever reuses vanilla's atlas name." ) );
			f.span( identityCheck );
			/* The rest of what a .bto carries, which the first panel never
			 * showed - bungo: "where are the toggles for data other than
			 * identity and AO?". Sway and the two channels ride on the identity
			 * profile, so they grey with it and hide with it under the stock
			 * engine, where they are inert bytes. */
			swayCheck = new QCheckBox( tr( "Tree sway weights" ), page );
			swayCheck->setObjectName( QStringLiteral( "LodgenSwayCheck" ) );
			swayCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/treeSway" ), true ).toBool() );
			swayCheck->setToolTip( tr( "A per-vertex sway weight in the vertex alpha: 0 at the trunk base, 1 at the branch\n"
				"tips, for FO4CS to move distant trees. The stock engine never reads it." ) );
			channelsCheck = new QCheckBox( tr( "Sky and ground channels" ), page );
			channelsCheck->setObjectName( QStringLiteral( "LodgenObjectChannelsCheck" ) );
			channelsCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/objectChannels" ), true ).toBool() );
			channelsCheck->setToolTip( tr( "Per-vertex sky visibility and ground-contact blend, baked per placement; no tiling\n"
				"texture can say how open the sky is at one vertex. Widens each vertex from 24 to\n"
				"32 bytes." ) );
			arraysCheck = new QCheckBox( tr( "Texture arrays" ), page );
			arraysCheck->setObjectName( QStringLiteral( "LodgenArraysCheck" ) );
			arraysCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/arrays" ), true ).toBool() );
			arraysCheck->setToolTip( tr( "One texture array per texture size over every texture the chunks use, with\n"
				"real mips and no atlas bleed, the layer in each vertex; FO4CS draws a chunk's\n"
				"objects in one call per array. The stock engine keeps the plain textures." ) );
			f.span( swayCheck );
			f.span( channelsCheck );
			f.span( arraysCheck );
			f.span( aoCheck );
			aoSkirtSpin = new QSpinBox( page );
			aoSkirtSpin->setObjectName( QStringLiteral( "LodgenAoSkirtSpin" ) );
			aoSkirtSpin->setRange( 0, 4 );
			aoSkirtSpin->setValue( settings.value( QStringLiteral( "LodGeneration/aoSkirt" ), 1 ).toInt() );
			aoSkirtSpin->setToolTip( tr( "How many cells of neighbouring terrain and objects the AO bake sees past the\n"
				"chunk's edge. 0 stops at the border, which leaves every edge too bright and reads\n"
				"as a seam (measured 17% at Sanctuary)." ) );
			QLabel * aoSkirtLabel = f.add( page, tr( "AO skirt (cells)" ), aoSkirtSpin );
			cullCheck = new QCheckBox( tr( "Drop geometry buried in the terrain" ), page );
			cullCheck->setObjectName( QStringLiteral( "LodgenCullCheck" ) );
			cullCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/cullBuried" ), false ).toBool() );
			cullCheck->setToolTip( tr( "What vanilla's generator does: a triangle goes only when all three of its vertices\n"
				"sit below the ground by the margin, and a placement never loses its last triangle." ) );
			f.span( cullCheck );
			cullMarginSpin = new QSpinBox( page );
			cullMarginSpin->setObjectName( QStringLiteral( "LodgenCullMarginSpin" ) );
			cullMarginSpin->setRange( 0, 2048 );
			cullMarginSpin->setValue( settings.value( QStringLiteral( "LodGeneration/cullMargin" ), 128 ).toInt() );
			cullMarginSpin->setToolTip( tr( "World units below the surface before a vertex counts as buried." ) );
			QLabel * cullMarginLabel = f.add( page, tr( "Buried margin (units)" ), cullMarginSpin );
			slotFallbackCheck = new QCheckBox( tr( "Use a nearer LOD slot when the ring's is empty" ), page );
			slotFallbackCheck->setObjectName( QStringLiteral( "LodgenSlotFallbackCheck" ) );
			slotFallbackCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/slotFallback" ), false ).toBool() );
			slotFallbackCheck->setToolTip( tr( "Off matches vanilla, where an object with no model for a ring drops out there.\n"
				"On keeps it with a nearer, heavier model: far chunks grow to many times vanilla's\n"
				"size. Impostor cards are the better answer." ) );
			f.span( slotFallbackCheck );
			/* Far-ring proxies. bungo, 2026-09-06: "Proxy meshes for the far
			 * rings. Every engine since 2017 replaces far clusters with one
			 * simplified mesh per cell ... ring 2 and 3 chunks could ship at a
			 * quarter of their triangles with the same textures." It is
			 * geometry, not a channel, so it stays visible under BOTH targets:
			 * the stock engine draws the smaller mesh for the same picture
			 * exactly as FO4CS does. */
			simplifyCheck = new QCheckBox( tr( "Far-ring simplification" ), page );
			simplifyCheck->setObjectName( QStringLiteral( "LodgenSimplifyCheck" ) );
			simplifyCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/simplify" ), true ).toBool() );
			simplifyCheck->setToolTip( tr( "After the shapes merge, each far ring's meshes are decimated to the fraction\n"
				"of their triangles set below. Alpha-tested shapes and impostor cards keep every\n"
				"triangle - a cut-out is a silhouette, not a surface - and ring 0, the one you\n"
				"walk up to, is never touched." ) );
			f.span( simplifyCheck );
			auto ratioField = [this, page, &f, &settings]( QDoubleSpinBox *& field, const char * objName,
				const QString & label, const QString & key, double dflt, const QString & tip ) {
				field = new QDoubleSpinBox( page );
				field->setObjectName( QLatin1String( objName ) );
				field->setRange( 0.05, 1.00 );
				field->setSingleStep( 0.05 );
				field->setDecimals( 2 );
				field->setValue( settings.value( key, dflt ).toDouble() );
				field->setToolTip( tip );
				wwMakeScrubField( field );
				return f.add( page, label, field );
			};
			QLabel * s8Label = ratioField( simplify8Spin, "LodgenSimplify8Spin",
				tr( "Ring 1 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing8" ), 1.00,
				tr( "The fraction of ring 1 (dim 8) triangles that survive. 1.00 leaves the ring\n"
					"alone, which is the default: ring 1 is still close enough to read as geometry." ) );
			QLabel * s16Label = ratioField( simplify16Spin, "LodgenSimplify16Spin",
				tr( "Ring 2 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing16" ), 0.35,
				tr( "The fraction of ring 2 (dim 16) triangles that survive." ) );
			QLabel * s32Label = ratioField( simplify32Spin, "LodgenSimplify32Spin",
				tr( "Ring 3 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing32" ), 0.20,
				tr( "The fraction of ring 3 (dim 32) triangles that survive." ) );
			simplifyErrorSpin = new QDoubleSpinBox( page );
			simplifyErrorSpin->setObjectName( QStringLiteral( "LodgenSimplifyErrorSpin" ) );
			simplifyErrorSpin->setRange( 1.0, 1024.0 );
			simplifyErrorSpin->setSingleStep( 4.0 );
			simplifyErrorSpin->setDecimals( 1 );
			simplifyErrorSpin->setValue( settings.value( QStringLiteral( "LodGeneration/simplifyError" ), 32.0 ).toDouble() );
			simplifyErrorSpin->setToolTip( tr( "How far a simplified surface may move, in world units at ring 0, scaled by\n"
				"each ring's own size. The decimator stops early rather than exceed it, so the\n"
				"fractions above are targets and this is the rail." ) );
			wwMakeScrubField( simplifyErrorSpin );
			QLabel * sErrLabel = f.add( page, tr( "Simplification error" ), simplifyErrorSpin );
			f.span( atlasCheck );
			impostorEdit = new QLineEdit( settings.value( QStringLiteral( "LodGeneration/impostors" ) ).toString(), page );
			impostorEdit->setPlaceholderText( tr( "optional" ) );
			impostorEdit->setToolTip( tr( "A directory of cards from bake_impostor_cards.sh. A card stands in for a model\n"
				"that has no far LOD of its own." ) );
			QWidget * impostorHost = browseHost( impostorEdit, tr( "Impostor card directory" ) );
			QLabel * impostorLabel = f.add( page, tr( "Impostor cards" ), impostorHost );
			impostorLevelBox = new QComboBox( page );
			impostorLevelBox->setObjectName( QStringLiteral( "LodgenImpostorLevelBox" ) );
			impostorLevelBox->addItem( tr( "Only where a ring has no model" ), -1 );
			impostorLevelBox->addItem( tr( "Ring 0 (dim 4) and beyond" ), 0 );
			impostorLevelBox->addItem( tr( "Ring 1 (dim 8) and beyond" ), 1 );
			impostorLevelBox->addItem( tr( "Ring 2 (dim 16) and beyond" ), 2 );
			impostorLevelBox->addItem( tr( "Ring 3 (dim 32)" ), 3 );
			impostorLevelBox->setCurrentIndex( qBound( 0,
				settings.value( QStringLiteral( "LodGeneration/impostorFromLevel" ), -1 ).toInt() + 1, 4 ) );
			impostorLevelBox->setToolTip( tr( "From this ring on, an object with a card stands on the card even where the ring\n"
				"has a mesh: one quad per tree, for FO4 Community Shaders, which draws the octahedral\n"
				"sheets. The stock engine would show the crossed quads there." ) );
			wwMatchFieldStyle( impostorLevelBox );
			impostorLevelLabel = f.add( page, tr( "Cards from ring" ), impostorLevelBox );

			/* The octahedral grid, in FRAMES PER SIDE - the bake's own convention
			 * (WW_IMPOSTOR_OCT=4 writes a sheet four frames wide, not five). Costs
			 * are measured per frame and multiply by the square, so the choice is
			 * really a memory dial: at 30 far-field tree bases the three come to
			 * 6.0, 13.5 and 24.0 MiB. Grids are not mixed within a run: the card
			 * arrays group by sheet size, so two grids in one directory become two
			 * arrays and two binds. */
			cardFramesBox = new QComboBox( page );
			cardFramesBox->setObjectName( QStringLiteral( "LodgenCardFramesBox" ) );
			cardFramesBox->addItem( tr( "4 x 4 (16 views)" ), 4 );
			cardFramesBox->addItem( tr( "6 x 6 (36 views)" ), 6 );
			cardFramesBox->addItem( tr( "8 x 8 (64 views)" ), 8 );
			{
				const int saved = settings.value( QStringLiteral( "LodGeneration/cardFrames" ), 8 ).toInt();
				const int idx = cardFramesBox->findData( saved );
				cardFramesBox->setCurrentIndex( idx >= 0 ? idx : 2 );
			}
			cardFramesBox->setToolTip( tr( "Views around the upper hemisphere on each impostor card, as frames per side.\n"
				"More views means a smaller angular step and a less visible blend as a card turns;\n"
				"fewer means a sharper view for the same bytes. This row and the two below it\n"
				"multiply, so the sheet they produce is spelled out beneath them. A run refuses if\n"
				"the cards in the directory above were baked at a different grid." ) );
			wwMatchFieldStyle( cardFramesBox );
			cardFramesLabel = f.add( page, tr( "Card frames" ), cardFramesBox );

			/* THE RESOLUTION IS THE LARGEST BASE'S. bungo, 2026-09-06: "you
			 * should also be able to select their base resolution", and then
			 * "if it's really small, roughly half the size of the biggest tree
			 * we generated an impostor for, we can scale that down". So this is
			 * the top of a ladder, not a flat setting: the card baker measures
			 * every base against the largest in the run and takes it down by
			 * halves, then takes the short side down again by the silhouette's
			 * aspect. A worldspace of trees therefore costs far less than this
			 * figure times the number of trees, which the cost line says. */
			cardResBox = new QComboBox( page );
			cardResBox->setObjectName( QStringLiteral( "LodgenCardResBox" ) );
			cardResBox->addItem( tr( "64 px" ), 64 );
			cardResBox->addItem( tr( "128 px" ), 128 );
			cardResBox->addItem( tr( "256 px" ), 256 );
			{
				const int saved = settings.value( QStringLiteral( "LodGeneration/cardRes" ), 128 ).toInt();
				const int idx = cardResBox->findData( saved );
				cardResBox->setCurrentIndex( idx >= 0 ? idx : 1 );
			}
			cardResBox->setToolTip( tr( "The long side of one view, in texels, for the LARGEST base in the run.\n"
				"Every smaller base comes down a halving ladder by its own world size - half the\n"
				"size, half the side, three rungs at most - and a thin tree gets a narrower frame\n"
				"again from its silhouette. Cost grows with the square of this and with the square\n"
				"of the frame count. A run refuses if the cards above were baked at another size." ) );
			wwMatchFieldStyle( cardResBox );
			cardResLabel = f.add( page, tr( "Card resolution" ), cardResBox );

			/* The base colour is the one sheet that cannot come down: its ALPHA
			 * is the coverage, so it is the silhouette, and a soft silhouette is
			 * the single fault an impostor cannot hide. The other three are
			 * lit-appearance data at LOD distance. */
			cardHalfAuxCheck = new QCheckBox( tr( "Half-resolution normal, mask and emissive sheets" ), page );
			cardHalfAuxCheck->setObjectName( QStringLiteral( "LodgenCardHalfAuxCheck" ) );
			cardHalfAuxCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/cardHalfAux" ), false ).toBool() );
			cardHalfAuxCheck->setToolTip( tr( "Keeps the base colour at full size and halves each side of the other three.\n"
				"A sheet texel costs 3.5 bytes as four full sheets and 1.625 with this on, so it\n"
				"takes 54% off every card. The base colour never divides: its alpha is the coverage,\n"
				"which is the cut-out around every leaf. Not yet looked at in a game." ) );
			f.span( cardHalfAuxCheck );

			/* One computed line for all three rows, because they multiply and
			 * the arithmetic is not obvious. Typed figures were wrong here once
			 * already - "0.80 MB a tree" was true of a 64 px frame while the
			 * driver defaulted to 128 - so nothing here is typed. */
			cardCostLabel = new QLabel( page );
			cardCostLabel->setObjectName( QStringLiteral( "LodgenCardCostLabel" ) );
			cardCostLabel->setWordWrap( true );
			auto refreshCardCost = [this]() {
				if ( !cardCostLabel || !cardFramesBox || !cardResBox || !cardHalfAuxCheck )
					return;
				const int n = cardFramesBox->currentData().toInt();
				const int t = cardResBox->currentData().toInt();
				const int div = cardHalfAuxCheck->isChecked() ? 2 : 1;
				const qint64 texels = qint64( n ) * t * qint64( n ) * t;
				/* Three BC3 sheets at a byte a texel and one BC1 at a half;
				 * all but the base colour divide by `div` on each side. The
				 * mip chain runs while a frame still spans eight texels, which
				 * is very nearly the full 4/3. */
				const double bytes = ( double( texels ) * 1.0
					+ double( texels ) / ( div * div ) * 2.0
					+ double( texels ) / ( div * div ) * 0.5 ) * 4.0 / 3.0;
				cardCostLabel->setText( tr( "%1 x %1 sheets at %2 x %2 a frame: %3 MB for the largest base, "
					"and less for every smaller one." )
					.arg( n * t ).arg( t )
					.arg( bytes / ( 1024.0 * 1024.0 ), 0, 'f', 2 ) );
			};
			connect( cardFramesBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), page, refreshCardCost );
			connect( cardResBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), page, refreshCardCost );
			connect( cardHalfAuxCheck, &QCheckBox::toggled, page, refreshCardCost );
			refreshCardCost();
			f.span( cardCostLabel );
			objectsSection->body()->setLayout( f.g );
			objectsSub = { identityCheck, swayCheck, channelsCheck, arraysCheck, aoCheck, aoSkirtSpin, aoSkirtLabel, cullCheck,
				cullMarginSpin, cullMarginLabel, slotFallbackCheck, atlasCheck, impostorHost, impostorLabel,
				impostorLevelBox, impostorLevelLabel, cardFramesBox, cardFramesLabel,
				cardResBox, cardResLabel, cardHalfAuxCheck, cardCostLabel,
				simplifyCheck, simplify8Spin, s8Label,
				simplify16Spin, s16Label, simplify32Spin, s32Label, simplifyErrorSpin, sErrLabel };
			auto sync = [this, aoSkirtLabel, cullMarginLabel, s8Label, s16Label, s32Label, sErrLabel]() {
				const bool on = objectsCheck->isChecked();
				for ( QWidget * w : objectsSub )
					w->setEnabled( on );
				const bool ident = on && identityCheck->isChecked();
				swayCheck->setEnabled( ident );
				channelsCheck->setEnabled( ident );
				arraysCheck->setEnabled( ident );
				aoSkirtSpin->setEnabled( on && aoCheck->isChecked() );
				aoSkirtLabel->setEnabled( on && aoCheck->isChecked() );
				cullMarginSpin->setEnabled( on && cullCheck->isChecked() );
				cullMarginLabel->setEnabled( on && cullCheck->isChecked() );
				const bool simp = on && simplifyCheck->isChecked();
				for ( QWidget * w : { (QWidget *) simplify8Spin, (QWidget *) s8Label,
						(QWidget *) simplify16Spin, (QWidget *) s16Label,
						(QWidget *) simplify32Spin, (QWidget *) s32Label,
						(QWidget *) simplifyErrorSpin, (QWidget *) sErrLabel } )
					w->setEnabled( simp );
			};
			connect( objectsCheck, &QCheckBox::toggled, this, sync );
			connect( identityCheck, &QCheckBox::toggled, this, sync );
			connect( aoCheck, &QCheckBox::toggled, this, sync );
			connect( cullCheck, &QCheckBox::toggled, this, sync );
			connect( simplifyCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		btrCheck = new QCheckBox( tr( "Legacy terrain chunks (.btr)" ), page );
		btrCheck->setObjectName( QStringLiteral( "LodgenBtrCheck" ) );
		btrCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/btr" ), false ).toBool() );
		btrCheck->setToolTip( tr( "The per-chunk terrain path the stock engine and older FO4CS builds read.\n"
			"The landscape file replaces it." ) );
		// folded by default: kept for the stock engine, no longer maintained
		btrSection = new LodgenSection( btrCheck, QStringLiteral( "Btr" ), false, page );
		layout->addWidget( btrSection );
		{
			Form f = form( 24 );
			trisSpin = new QSpinBox( page );
			trisSpin->setRange( 0, 2048 );
			trisSpin->setValue( 130 );
			trisSpin->setToolTip( tr( "The triangle budget of a 4-cell chunk; the wider rings scale from it.\n0 keeps the full grid." ) );
			QLabel * trisLabel = f.add( page, tr( "Triangles per cell at dim 4" ), trisSpin );
			waterCheck = new QCheckBox( tr( "LOD water" ), page );
			waterCheck->setChecked( true );
			texCheck = new QCheckBox( tr( "Bake terrain textures" ), page );
			texCheck->setObjectName( QStringLiteral( "LodgenTexCheck" ) );
			texCheck->setChecked( true );
			terrainIdCheck = new QCheckBox( tr( "Terrain identity channels" ), page );
			terrainIdCheck->setObjectName( QStringLiteral( "LodgenTerrainIdentityCheck" ) );
			terrainIdCheck->setChecked( true );
			terrainIdCheck->setToolTip( tr( "Material class, wetness and water depth in the chunk's vertex colours, for FO4CS.\nThe stock engine ignores them." ) );
			geomorphCheck = new QCheckBox( tr( "Geomorph weights" ), page );
			geomorphCheck->setChecked( false );
			shoreCheck = new QCheckBox( tr( "Denser geometry at shorelines" ), page );
			shoreCheck->setChecked( false );
			shoreCheck->setToolTip( tr( "Measured against the full-resolution source it made fidelity WORSE (p95 error\n30.9 -> 89.5), so it is off. Kept as an experiment." ) );
			f.span( waterCheck );
			f.span( texCheck );
			coverCheck = new QCheckBox( tr( "Ground cover and grass tint" ), page );
			coverCheck->setObjectName( QStringLiteral( "LodgenCoverCheck" ) );
			coverCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/cover" ), false ).toBool() );
			coverCheck->setToolTip( tr( "Reads the landscape textures' grass records. Puts a cover value in the\n"
				"terrain data sheet's alpha and mixes a grass colour into the far albedo." ) );
			f.span( coverCheck );
			tintSpin = new QSpinBox( page );
			tintSpin->setObjectName( QStringLiteral( "LodgenTintSpin" ) );
			tintSpin->setRange( 0, 100 );
			tintSpin->setSuffix( QStringLiteral( " %" ) );
			tintSpin->setValue( settings.value( QStringLiteral( "LodGeneration/grassTint" ), 35 ).toInt() );
			tintSpin->setToolTip( tr( "How far the far albedo moves toward the grass colour where cover is full.\n"
				"0 keeps the albedo exactly as it is and still writes the cover plane." ) );
			tintLabel = f.add( page, tr( "Grass tint strength" ), tintSpin );
			f.span( terrainIdCheck );
			f.span( geomorphCheck );
			f.span( shoreCheck );
			shoreDensitySpin = new QSpinBox( page );
			shoreDensitySpin->setRange( 1, 10 );
			shoreDensitySpin->setValue( 1 );
			shoreDensitySpin->setToolTip( tr( "How many extra subdivisions a shoreline triangle gets." ) );
			QLabel * shoreLabel = f.add( page, tr( "Shoreline density" ), shoreDensitySpin );
			btrSection->body()->setLayout( f.g );
			btrSub = { trisSpin, trisLabel, waterCheck, texCheck, coverCheck, tintSpin, tintLabel,
				terrainIdCheck, geomorphCheck, shoreCheck, shoreDensitySpin, shoreLabel };
			auto sync = [this, shoreLabel]() {
				const bool on = btrCheck->isChecked();
				for ( QWidget * w : btrSub ) w->setEnabled( on );
				shoreDensitySpin->setEnabled( on && shoreCheck->isChecked() );
				shoreLabel->setEnabled( on && shoreCheck->isChecked() );
				// the cover plane lives IN the data sheet, so the tint is only a
				// question once the sheets are being baked and cover is asked for
				coverCheck->setEnabled( on && texCheck->isChecked() );
				const bool tint = on && texCheck->isChecked() && coverCheck->isChecked();
				tintSpin->setEnabled( tint );
				tintLabel->setEnabled( tint );
			};
			connect( btrCheck, &QCheckBox::toggled, this, sync );
			connect( shoreCheck, &QCheckBox::toggled, this, sync );
			connect( texCheck, &QCheckBox::toggled, this, sync );
			connect( coverCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		/* The terrain virtual texture. A LodgenSection whose header IS its
		 * check box, folded by default like the .btr one - not a QGroupBox,
		 * which the panel self-test counts at zero, and not a wwHeading, which
		 * cannot carry the enable. */
		vtCheck = new QCheckBox( tr( "Terrain virtual texture (.lodt)" ), page );
		vtCheck->setObjectName( QStringLiteral( "LodgenVtCheck" ) );
		vtCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/vt" ), false ).toBool() );
		vtCheck->setToolTip( tr( "A tile pyramid the consumer streams instead of loading whole chunk\n"
			"sheets. 256-texel tiles with an 8-texel border, five levels, colour,\n"
			"normal, data and height." ) );
		vtSection = new LodgenSection( vtCheck, QStringLiteral( "Vt" ), false, page );
		layout->addWidget( vtSection );
		{
			Form f = form( 24 );
			vtFinestBox = new QComboBox( page );
			vtFinestBox->setObjectName( QStringLiteral( "LodgenVtFinestBox" ) );
			vtFinestBox->addItem( tr( "2 cells per tile (32 units a texel)" ), 2 );
			vtFinestBox->addItem( tr( "1 cell per tile (16 units a texel, full)" ), 1 );
			vtFinestBox->setCurrentIndex(
				settings.value( QStringLiteral( "LodGeneration/vtFinest" ), 2 ).toInt() == 1 ? 1 : 0 );
			vtFinestBox->setToolTip( tr( "The densest level the pyramid carries. Two cells a tile is exactly\n"
				"the density of vanilla's finest terrain ring; one cell is twice that\n"
				"and four times the files." ) );
			wwMatchFieldStyle( vtFinestBox );
			vtFinestLabel = f.add( page, tr( "Finest level" ), vtFinestBox );
			vtBtrCheck = new QCheckBox( tr( "Chunk textures from the pyramid" ), page );
			vtBtrCheck->setObjectName( QStringLiteral( "LodgenVtBtrCheck" ) );
			vtBtrCheck->setChecked(
				settings.value( QStringLiteral( "LodGeneration/vtBtr" ), true ).toBool() );
			vtBtrCheck->setToolTip( tr( "Assemble each legacy chunk sheet from the four pyramid tiles that\n"
				"cover it instead of baking the same ground twice. The chunk files are\n"
				"unchanged; only where they came from is." ) );
			f.span( vtBtrCheck );
			vtSummary = new QLabel( page );
			vtSummary->setObjectName( QStringLiteral( "LodgenVtSummary" ) );
			vtSummary->setWordWrap( true );
			vtSummary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
			f.span( vtSummary );
			vtSection->body()->setLayout( f.g );
			vtSub = { vtFinestBox, vtFinestLabel, vtBtrCheck, vtSummary };
			auto sync = [this]() {
				const bool on = vtCheck->isChecked();
				for ( QWidget * w : vtSub )
					w->setEnabled( on );
				// there are no chunk sheets to take from the pyramid unless the
				// legacy chunks and their textures are both being written
				vtBtrCheck->setEnabled( on && btrCheck->isChecked() && texCheck->isChecked() );
			};
			connect( vtCheck, &QCheckBox::toggled, this, sync );
			connect( btrCheck, &QCheckBox::toggled, this, sync );
			connect( texCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		// ---- Chunk range ----------------------------------------------------
		/* One widget, so the whole section greys together while no chunk
		 * output is ticked: the range means nothing to a whole-worldspace file. */
		rangeBox = new QWidget( page );
		{
			auto * v = new QVBoxLayout( rangeBox );
			v->setContentsMargins( 0, 0, 0, 0 );
			v->setSpacing( 5 );
			v->addWidget( wwHeading( tr( "Chunk range" ), rangeBox ) );
			auto * hint = new QLabel( tr( "Object and legacy terrain chunks only. The landscape file and the "
				"heightmap always cover the whole worldspace." ), rangeBox );
			hint->setWordWrap( true );
			hint->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
			v->addWidget( hint );
			Form f = form( 0 );
			auto makeSpin = [this, &settings]( const QString & key, int value ) {
				auto * spin = new QSpinBox( rangeBox );
				spin->setRange( -512, 511 );
				spin->setValue( settings.value( QStringLiteral( "LodGeneration/" ) + key, value ).toInt() );
				spin->setObjectName( QStringLiteral( "Lodgen" ) + key + QStringLiteral( "Spin" ) );
				return spin;
			};
			x0Spin = makeSpin( QStringLiteral( "West" ), -24 );
			f.add( rangeBox, tr( "West cell" ), x0Spin );
			x1Spin = makeSpin( QStringLiteral( "East" ), -13 );
			f.add( rangeBox, tr( "East cell" ), x1Spin );
			y0Spin = makeSpin( QStringLiteral( "South" ), 20 );
			f.add( rangeBox, tr( "South cell" ), y0Spin );
			y1Spin = makeSpin( QStringLiteral( "North" ), 31 );
			f.add( rangeBox, tr( "North cell" ), y1Spin );
			dimBox = new QComboBox( rangeBox );
			dimBox->addItem( tr( "all rings (4+8+16+32)" ) );
			for ( int d : { 4, 8, 16, 32 } )
				dimBox->addItem( QString::number( d ) );
			dimBox->setToolTip( tr( "Which LOD ring to write. A ring is the chunk size in cells; the engine\nshows the wider rings further away." ) );
			wwMatchFieldStyle( dimBox );
			f.add( rangeBox, tr( "Chunk size" ), dimBox );
			wholeButton = new QPushButton( tr( "Whole worldspace" ), rangeBox );
			wholeButton->setObjectName( QStringLiteral( "LodgenWholeWorldButton" ) );
			wholeButton->setToolTip( tr( "Set the range to every cell the worldspace has." ) );
			f.g->addWidget( wholeButton, f.row++, 1, Qt::AlignLeft );
			connect( wholeButton, &QPushButton::clicked, this, [this]() { fillWholeWorld(); } );
			v->addLayout( f.g );
		}
		layout->addWidget( rangeBox );
		layout->addStretch( 1 );
		auto syncRange = [this]() { rangeBox->setEnabled( objectsCheck->isChecked() || btrCheck->isChecked() ); };
		connect( objectsCheck, &QCheckBox::toggled, this, syncRange );
		connect( btrCheck, &QCheckBox::toggled, this, syncRange );
		syncRange();

		// ---- Progress: the map and the bar, on the splitter's lower pane ----
		auto * progressPage = new QWidget( splitter );
		progressPage->setObjectName( QStringLiteral( "LodgenProgressPage" ) );
		auto * pl = new QVBoxLayout( progressPage );
		pl->setContentsMargins( 6, 4, 6, 4 );
		pl->setSpacing( 5 );
		pl->addWidget( wwHeading( tr( "Progress" ), progressPage ) );
		map = new LodgenProgressMap( progressPage );
		pl->addWidget( map, 1 );
		progress = new QProgressBar( progressPage );
		progress->setObjectName( QStringLiteral( "LodgenProgressBar" ) );
		progress->setTextVisible( true );
		progress->setFormat( tr( "idle" ) );
		pl->addWidget( progress );
		previewCheck = new QCheckBox( tr( "Show chunks in the viewport as they finish" ), progressPage );
		previewCheck->setChecked( true );
		previewCheck->setToolTip( tr( "Each finished .bto/.btr chunk is added to the workspace at its world position,\nwith the viewport set to an orthographic top view. The .lodl is not a mesh; it\npaints the map above instead." ) );
		pl->addWidget( previewCheck );
		{
			Form f = form( 0 );
			previewLabel = new QLabel( tr( "Preview channel" ), progressPage );
			previewBox = new QComboBox( progressPage );
			previewBox->addItem( tr( "Off (normal shading)" ), 0 );
			previewBox->addItem( tr( "Identity - hashed colour per object" ), 1 );
			previewBox->addItem( tr( "Identity - raw R+G bytes" ), 2 );
			previewBox->addItem( tr( "Ambient occlusion (B)" ), 3 );
			previewBox->addItem( tr( "Tree sway / shore proximity (A)" ), 4 );
			previewBox->addItem( tr( "Terrain material class (R, hashed)" ), 5 );
			previewBox->addItem( tr( "Terrain wetness (G)" ), 6 );
			previewBox->addItem( tr( "Water depth (R)" ), 7 );
			previewBox->setToolTip( tr( "Draw one generated vertex channel flat, with no textures or lighting.\nOnly for an open .bto or .btr - on any other mesh these channels mean\nsomething else." ) );
			wwMatchFieldStyle( previewBox );
			f.g->addWidget( previewLabel, f.row, 0 );
			f.g->addWidget( previewBox, f.row++, 1 );
			pl->addLayout( f.g );
			connect( previewBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]( int i ) {
				wwLodChannelView = previewBox->itemData( i ).toInt();
				if ( skope && skope->getGLView() )
					skope->getGLView()->update();
			} );
		}
		splitter->addWidget( progressPage );
		splitter->setStretchFactor( 0, 3 );
		splitter->setStretchFactor( 1, 2 );
		{
			QList<int> sizes;
			for ( const QVariant & v : settings.value( QStringLiteral( "LodGeneration/split" ) ).toList() )
				sizes << v.toInt();
			if ( sizes.size() == 2 && sizes[0] > 0 && sizes[1] > 0 )
				splitter->setSizes( sizes );
			else
				splitter->setSizes( { 600, 360 } );
			connect( splitter, &QSplitter::splitterMoved, this, [this]( int, int ) {
				QVariantList out;
				for ( int s : splitter->sizes() )
					out << s;
				QSettings().setValue( QStringLiteral( "LodGeneration/split" ), out );
			} );
		}

		// ---- The action bar: what will happen, and the button that does it --
		auto * actionBar = new QWidget( this );
		actionBar->setObjectName( QStringLiteral( "LodgenActionBar" ) );
		actionBar->setAttribute( Qt::WA_StyledBackground, true );
		actionBar->setStyleSheet( QStringLiteral( "#LodgenActionBar { border-top: 1px solid %1; }" )
			.arg( wwSkinColor( "borderDim" ) ) );
		auto * ab = new QHBoxLayout( actionBar );
		ab->setContentsMargins( 6, 5, 6, 6 );
		ab->setSpacing( 6 );
		summary = new QLabel( actionBar );
		summary->setObjectName( QStringLiteral( "LodgenSummaryLabel" ) );
		summary->setWordWrap( true );
		summary->setTextInteractionFlags( Qt::TextSelectableByMouse );
		ab->addWidget( summary, 1 );
		startButton = new QPushButton( tr( "Generate" ), actionBar );
		startButton->setObjectName( QStringLiteral( "LodgenGenerateButton" ) );
		startButton->setDefault( true );
		cancelButton = new QPushButton( tr( "Cancel" ), actionBar );
		cancelButton->setObjectName( QStringLiteral( "LodgenCancelButton" ) );
		cancelButton->setEnabled( false );
		ab->addWidget( startButton, 0, Qt::AlignTop );
		ab->addWidget( cancelButton, 0, Qt::AlignTop );
		outer->addWidget( actionBar, 0 );

		// every number is the fork's one number field: drag to scrub, click to type
		for ( QSpinBox * s : { aoSpin, ovSpin, trisSpin, shoreDensitySpin, x0Spin, x1Spin, y0Spin, y1Spin,
				aoSkirtSpin, cullMarginSpin, tintSpin } )
			wwMakeScrubField( s );

		/* The target hides what its reader cannot use and, when CHOSEN, ticks
		 * what it needs. Restoring a saved target only hides; the ticks come
		 * back from their own settings. */
		auto applyTarget = [this]( bool chosen ) {
			const bool cs = fo4cs();
			lodtSection->setVisible( cs );
			heightmapSection->setVisible( cs );
			identityCheck->setVisible( cs );
			swayCheck->setVisible( cs );
			channelsCheck->setVisible( cs );
			arraysCheck->setVisible( cs );
			impostorLevelBox->setVisible( cs );
			impostorLevelLabel->setVisible( cs );
			terrainIdCheck->setVisible( cs );
			// the stock engine has no .lodt texture-pyramid reader and no VT sampler
			vtSection->setVisible( cs );
			if ( chosen ) {
				lodtCheck->setChecked( cs );
				heightmapCheck->setChecked( cs );
				identityCheck->setChecked( cs );
				terrainIdCheck->setChecked( cs );
				objectsCheck->setChecked( true );
				btrCheck->setChecked( !cs );
			}
			refreshSummary();
		};
		connect( targetBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[applyTarget]( int ) { applyTarget( true ); } );
		applyTarget( false );

		connect( sourceBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int ) { applySource( true ); } );
		applySource( false );

		// the summary follows every setting it reads
		for ( QCheckBox * c : { lodtCheck, heightmapCheck, objectsCheck, btrCheck, texCheck,
				coverCheck, vtCheck, vtBtrCheck } )
			connect( c, &QCheckBox::toggled, this, [this]( bool ) { refreshSummary(); } );
		connect( lodtAoOnlyRadio, &QRadioButton::toggled, this, [this]( bool ) { refreshSummary(); } );
		for ( QSpinBox * s : { x0Spin, x1Spin, y0Spin, y1Spin } )
			connect( s, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this]( int ) { refreshSummary(); } );
		for ( QComboBox * c : { dimBox, heightmapSizeBox, vtFinestBox } )
			connect( c, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]( int ) { refreshSummary(); } );
		connect( outEdit, &QLineEdit::textChanged, this, [this]( const QString & ) { refreshSummary(); } );

		connect( startButton, &QPushButton::clicked, this, [this]() { start(); } );
		connect( cancelButton, &QPushButton::clicked, this, [this]() {
			cancelFlag = true;
			progress->setFormat( tr( "cancelling\u2026" ) );
		} );
		if ( skope )
			connect( skope, &NifSkope::completeLoading, this,
				[this]( bool, QString & ) { refreshPreviewAvailability(); } );
		refreshPreviewAvailability();
		refreshWorldspaces();
	}

	~LodgenPanel() override
	{
		cancelFlag = true;
		if ( worker.joinable() )
			worker.join();
		restoreManagerFolders();
	}

	/*! The preview only means anything on generated LOD, so it is only offered
	 *  there. Switching to another file turns it off rather than leaving a
	 *  stale mode painting an unrelated mesh's vertex colours. */
	void refreshPreviewAvailability()
	{
		bool isLod = false;
		if ( skope ) {
			const QString name = skope->getNifModel()
				? skope->getNifModel()->getFileInfo().fileName().toLower() : QString();
			isLod = name.endsWith( QLatin1String( ".bto" ) ) || name.endsWith( QLatin1String( ".btr" ) );
		}
		previewBox->setEnabled( isLod );
		previewLabel->setEnabled( isLod );
		if ( !isLod && wwLodChannelView != 0 ) {
			previewBox->setCurrentIndex( 0 );
			wwLodChannelView = 0;
			if ( skope && skope->getGLView() )
				skope->getGLView()->update();
		}
	}

protected:
	/*! The dock is built and hidden at startup; the first time it is actually
	 *  opened, MO2 mode re-reads the profile (it may have changed since) and
	 *  pays for the file count it skipped while nobody could see it. */
	void showEvent( QShowEvent * e ) override
	{
		QWidget::showEvent( e );
		if ( !shownOnce ) {
			shownOnce = true;
			if ( mo2Mode() )
				applySource( false );
		}
	}

private:
	QToolButton * browseDirButton( QWidget * parent, QLineEdit * edit, const QString & title )
	{
		auto * b = new QToolButton( parent );
		b->setText( QStringLiteral( "\u2026" ) );
		connect( b, &QToolButton::clicked, this, [this, edit, title]() {
			const QString d = QFileDialog::getExistingDirectory( this, title, edit->text() );
			if ( !d.isEmpty() )
				edit->setText( d );
		} );
		return b;
	}

	QString pluginString() const
	{
		QStringList files;
		for ( int i = 0; i < pluginList->count(); i++ )
			files.append( pluginList->item( i )->text() );
		return files.join( QChar( ',' ) );
	}

	// ---- the source: Specified, or Mod Organizer 2 -------------------------

	bool mo2Mode() const { return sourceBox->currentData().toInt() == 1; }

	//! The game's own Data folder - under MO2 this is the VIRTUAL one.
	static QString gameDataDir()
	{
		const QString p = Game::GameManager::path( Game::FALLOUT_4 );
		return p.isEmpty() ? QString() : QDir::cleanPath( p + QStringLiteral( "/Data" ) );
	}

	QStringList resourceRows() const
	{
		QStringList rows;
		for ( int i = 0; i < resourceList->count(); i++ )
			rows.append( resourceList->item( i )->text() );
		return rows;
	}

	/*! The stack the run reads, LAST WINS. Specified: the game's Data
	 *  underneath, then the rows in order. MO2: the virtual Data, the base
	 *  archives, then each enabled plugin's archives in load order.
	 *
	 *  Empty in Specified mode with no rows, and that is deliberate: with
	 *  nothing to layer, the generator reads what it always read (the game
	 *  manager's own resources), so the byte-identity gate keeps measuring the
	 *  thing it was written to measure. */
	QStringList resourceStack() const
	{
		if ( mo2Mode() )
			return lodgenMo2Stack( gameDataDir(), mo2Plugins );
		const QStringList rows = resourceRows();
		if ( rows.isEmpty() )
			return QStringList();
		QStringList stack;
		if ( !gameDataDir().isEmpty() )
			stack << gameDataDir();
		return stack + rows;
	}

	/*! Hand the stack to the generator AND to the renderer, so a mod's trees
	 *  photograph with their own textures and the viewport shows what the bake
	 *  will see. The game manager takes a list of folders and archives, FIRST
	 *  wins, which is exactly what lodgenResourceSearchPaths() hands back; its
	 *  own folders stay underneath as a fallback. Nothing is written to
	 *  QSettings - GameManager::save() is what persists, and only the Settings
	 *  dialog calls it - and the original list is put back when the panel goes.
	 *  (Opening Settings > Resources calls GameManager::load(), which reloads
	 *  from QSettings and so DROPS this session view; reopen the panel or press
	 *  Generate to put it back.) */
	void installResources()
	{
		const QStringList stack = resourceStack();
		lodgenSetResources( stack );
		if ( stack.isEmpty() ) {
			restoreManagerFolders();
			return;
		}
		if ( !managerFoldersPushed ) {
			savedManagerFolders = Game::GameManager::folders( Game::FALLOUT_4 );
			managerFoldersPushed = true;
		}
		QStringList view = lodgenResourceSearchPaths();
		for ( const QString & f : savedManagerFolders )
			if ( !view.contains( f, Qt::CaseInsensitive ) )
				view.append( f );
		Game::GameManager::update_folders( Game::FALLOUT_4, view );
		Game::GameManager::close_resources();
	}

	void restoreManagerFolders()
	{
		if ( !managerFoldersPushed )
			return;
		managerFoldersPushed = false;
		Game::GameManager::update_folders( Game::FALLOUT_4, savedManagerFolders );
		Game::GameManager::close_resources();
	}

	/*! Switching the source reshapes the section: Specified shows the Resources
	 *  list and lets you order the plugins; Mod Organizer 2 fills the plugin
	 *  list from the profile's own plugins.txt and makes it read only, because
	 *  the order is MO2's. Off MO2 it says so and Generate refuses. */
	void applySource( bool userChose )
	{
		const bool mo2 = mo2Mode();
		resourceHost->setVisible( !mo2 );
		resourceLabel->setVisible( !mo2 );
		pluginList->setDragDropMode( mo2 ? QAbstractItemView::NoDragDrop : QAbstractItemView::InternalMove );
		for ( QWidget * b : pluginButtons )
			b->setEnabled( !mo2 );
		if ( !mo2 ) {
			mo2Plugins.clear();
			sourceStatus->setText( QString() );
			sourceStatus->setVisible( false );
			if ( userChose && !specifiedPlugins.isEmpty() ) {
				// your own list, exactly as it was when MO2 took the list over
				pluginList->clear();
				pluginList->addItems( specifiedPlugins );
				specifiedPlugins.clear();
				scheduleWorldspaceRefresh();
			}
			refreshSummary();
			return;
		}
		if ( specifiedPlugins.isEmpty() ) {
			for ( int i = 0; i < pluginList->count(); i++ )
				specifiedPlugins << pluginList->item( i )->text();
		}
		sourceStatus->setVisible( true );
		if ( !lodgenUnderMo2() ) {
			mo2Plugins.clear();
			sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) );
			sourceStatus->setText( mo2Refusal() );
			refreshSummary();
			return;
		}
		QString perr;
		mo2Plugins = lodgenReadPluginsTxt( lodgenPluginsTxtPath(), &perr );
		const QString data = gameDataDir();
		const QStringList stack = lodgenMo2Stack( data, mo2Plugins );
		int archives = 0;
		for ( const QString & e : stack )
			if ( !QFileInfo( e ).isDir() )
				archives++;
		pluginList->clear();
		for ( const QString & p : mo2Plugins ) {
			const QString full = QDir( data ).filePath( p );
			pluginList->addItem( QFileInfo( full ).isFile() ? QDir::cleanPath( full ) : p );
		}
		sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		QString line = tr( "Mod Organizer 2: %1 plugins, %2 archives, files from %3" )
			.arg( mo2Plugins.size() ).arg( archives ).arg( data );
		/* The file count walks the virtual Data folder, so it is only paid for
		 * once the dock is on screen - this panel is built (and hidden) at
		 * startup, and a walk of every enabled mod's meshes there would be a
		 * delay at every launch for no one's benefit. */
		if ( isVisible() )
			line += tr( " (%1 files)" ).arg( countDataFiles( data ) );
		sourceStatus->setText( line );
		scheduleWorldspaceRefresh();
		refreshSummary();
	}

	//! the one sentence the status line and the refusal both say
	static QString mo2Refusal()
	{
		return tr( "not launched from Mod Organizer 2: add NifSkope to its executable list "
			"(like FO4Edit), or use Specified" );
	}

	/*! How many loose files the (virtual) Data folder shows. MO2's VFS cannot
	 *  say which mod a file came from, so this counts FILES, not mods, and it
	 *  stops at a cap rather than walking every enabled mod's textures folder
	 *  while the panel waits. */
	static QString countDataFiles( const QString & data )
	{
		const int cap = 20000;
		int n = 0;
		for ( const char * sub : { "meshes", "textures", "materials" } ) {
			QDirIterator it( data + QChar( '/' ) + QLatin1String( sub ),
				QDir::Files, QDirIterator::Subdirectories );
			while ( it.hasNext() && n < cap ) {
				it.next();
				n++;
			}
			if ( n >= cap )
				break;
		}
		return n >= cap ? QStringLiteral( "%1+" ).arg( cap ) : QString::number( n );
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
			wsBox->addItem( QString( "%1  (%2)" ).arg( w.second ).arg( w.first, 8, 16, QChar( '0' ) ), w.first );
		if ( wsBox->count() == 0 )
			wsBox->addItem( tr( "no worldspaces found" ), 0U );
		worldChanged();
	}

	//! the map shows the chosen worldspace's cell bounds before anything runs
	void worldChanged()
	{
		if ( running )
			return;
		int minX, minY, maxX, maxY;
		haveBounds = worldBounds( minX, minY, maxX, maxY );
		if ( haveBounds ) {
			bMinX = minX; bMinY = minY; bMaxX = maxX; bMaxY = maxY;
			map->setWorld( minX, minY, maxX, maxY );
			map->setLabel( tr( "%1 x %2 cells" ).arg( maxX - minX + 1 ).arg( maxY - minY + 1 ) );
		} else {
			map->clearWorld();
		}
		refreshSummary();
	}

	bool worldBounds( int & minX, int & minY, int & maxX, int & maxY )
	{
		const quint32 form = wsBox->currentData().toUInt();
		if ( !form )
			return false;
		QString error;
		EsmWorld w;
		if ( !w.load( pluginString(), form, &error ) )
			return false;
		w.cellBounds( minX, minY, maxX, maxY );
		return minX <= maxX && minY <= maxY;
	}

	void fillWholeWorld()
	{
		int minX, minY, maxX, maxY;
		if ( !worldBounds( minX, minY, maxX, maxY ) ) {
			progress->setFormat( tr( "no worldspace to take the range from" ) );
			return;
		}
		x0Spin->setValue( minX );
		x1Spin->setValue( maxX );
		y0Spin->setValue( minY );
		y1Spin->setValue( maxY );
	}

	//! the mod folder the run writes into - it IS the Data folder
	QString outputDir() const
	{
		const QString out = outEdit->text().trimmed();
		return out.isEmpty() ? QString() : QDir::cleanPath( out );
	}

	bool fo4cs() const { return targetBox->currentData().toInt() == 0; }
	/* An output counts only when its reader can use it: a tick under a hidden
	 * section is a setting saved under the other target, not a request. */
	bool wantLodt() const { return lodtCheck->isChecked() && !lodtSection->isHidden(); }
	bool wantHeightmap() const { return heightmapCheck->isChecked() && !heightmapSection->isHidden(); }
	bool wantIdentity() const { return identityCheck->isChecked() && !identityCheck->isHidden(); }
	bool wantTerrainId() const { return terrainIdCheck->isChecked() && !terrainIdCheck->isHidden(); }
	/*! Ground cover is available on BOTH targets - the tint is FOR the stock
	 *  engine, which reads no data sheet at all, and the alpha plane costs a
	 *  stock user nothing on a chunk with no grass. It still reads tick AND
	 *  visible, and it needs the sheets it lives in. */
	//! A tick under a hidden section is a setting saved under the other
	//! target, not a request.
	bool wantVt() const { return vtCheck->isChecked() && !vtSection->isHidden(); }
	/*! The estimate for the settings as they stand. The worldspace's own
	 *  bounds when they are known; otherwise the chunk range, said out loud,
	 *  so the section always carries a number that MOVES with its settings
	 *  rather than a sentence that cannot. */
	bool vtEstimateNow( LodgenVtEstimateOut * e, bool * fromRange = nullptr ) const
	{
		const LodgenVtOptions vo = vtOptions();
		const bool alsoBtr = btrCheck->isChecked() && texCheck->isChecked();
		if ( fromRange )
			*fromRange = !haveBounds;
		if ( haveBounds )
			return lodgenVtEstimateBounds( bMinX, bMinY, bMaxX, bMaxY, vo, alsoBtr, e );
		return lodgenVtEstimateBounds( qMin( x0Spin->value(), x1Spin->value() ),
			qMin( y0Spin->value(), y1Spin->value() ),
			qMax( x0Spin->value(), x1Spin->value() ),
			qMax( y0Spin->value(), y1Spin->value() ), vo, alsoBtr, e );
	}
	void refreshVtSummary()
	{
		if ( !vtSummary )
			return;
		LodgenVtEstimateOut e;
		bool fromRange = false;
		if ( !vtEstimateNow( &e, &fromRange ) ) {
			vtSummary->setText( tr( "Choose a worldspace to size the pyramid." ) );
			return;
		}
		vtSummary->setText( tr( "%1 levels, %2 tiles%3. Pyramid about %4 GB, chunk sheets "
			"about %5 GB, about %6 GB on disk. Bake time is unmeasured." )
			.arg( e.levels ).arg( e.tiles )
			.arg( fromRange ? tr( " over the chunk range" ) : QString() )
			.arg( double( e.pyramidBytes ) / 1073741824.0, 0, 'f', 2 )
			.arg( double( e.btrBytes ) / 1073741824.0, 0, 'f', 2 )
			.arg( double( e.deliveredBytes ) / 1073741824.0, 0, 'f', 2 ) );
	}
	LodgenVtOptions vtOptions() const
	{
		LodgenVtOptions o;
		o.finestDim = vtFinestBox->currentData().toInt() == 1 ? 1 : 2;
		o.cover = coverOptions();
		return o;
	}
	LodgenCoverOptions coverOptions() const
	{
		LodgenCoverOptions o;
		o.cover = coverCheck->isChecked() && !coverCheck->isHidden()
			&& texCheck->isChecked() && btrCheck->isChecked();
		o.tintStrength = float( tintSpin->value() ) / 100.0f;
		return o;
	}

	/*! The line beside the buttons: what Generate will write, with sizes from
	 *  what was measured (a .lodl is about 1 KB a cell; a native heightmap is
	 *  32 texels a cell at 2 bytes each), or the one reason it cannot run.
	 *  Generate is greyed while there is a reason, and the reason is the text
	 *  - a greyed button with no sentence next to it is a broken button. */
	void refreshSummary()
	{
		if ( !summary || running )
			return;
		const QString ws = wsBox->currentText().section( QLatin1String( "  (" ), 0, 0 );
		/* The pyramid's line is computed BEFORE the refusal chain can return.
		 * It describes what the pyramid would cost, which is a fact about the
		 * settings and not about whether the run can start; hiding it behind
		 * "choose an output folder" would make the one live number in the
		 * section look like a fixed sentence. */
		refreshVtSummary();
		const bool chunks = objectsCheck->isChecked() || btrCheck->isChecked();
		QString why;
		/* The source comes first: with MO2 chosen and no MO2 around it, every
		 * other reason is beside the point - nothing would be read. */
		if ( mo2Mode() && !lodgenUnderMo2() )
			why = mo2Refusal();
		else if ( !wsBox->currentData().toUInt() )
			why = tr( "Choose a worldspace." );
		else if ( outputDir().isEmpty() )
			why = tr( "Choose an output mod folder." );
		else if ( !wantLodt() && !wantHeightmap() && !wantVt() && !chunks )
			why = tr( "Tick something to generate." );
		else if ( chunks && ( x0Spin->value() > x1Spin->value() || y0Spin->value() > y1Spin->value() ) )
			why = tr( "The chunk range is empty: west must not pass east, nor south pass north." );
		else if ( btrCheck->isChecked() && coverCheck->isChecked() && !texCheck->isChecked() )
			why = tr( "Ground cover needs the terrain textures ticked." );
		else if ( ( objectsCheck->isChecked() || ( btrCheck->isChecked() && texCheck->isChecked() ) )
			&& !Game::GameManager::status( Game::FALLOUT_4 ) )
			why = tr( "Fallout 4 is not enabled under Settings > Resources; meshes and textures are read from its archives." );
		else if ( objectsCheck->isChecked() ) {
			// the cards on disk against the two rows: a mismatch is otherwise silent
			const QPair<int, int> have = cardsOnDisk();
			const int wantN = cardFramesBox->currentData().toInt();
			const int wantT = cardResBox->currentData().toInt();
			if ( have.first && have.first != wantN )
				why = tr( "Those cards were baked at %1 x %1 frames, not %2 x %2. Re-bake them or change Card frames." )
					.arg( have.first ).arg( wantN );
			else if ( have.second && have.second != wantT )
				why = tr( "Those cards were baked at %1 px, not %2. Re-bake them or change Card resolution." )
					.arg( have.second ).arg( wantT );
		}
		startButton->setEnabled( why.isEmpty() );
		if ( !why.isEmpty() ) {
			summary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) );
			summary->setText( why );
			return;
		}
		QStringList parts;
		const int cellsX = haveBounds ? bMaxX - bMinX + 1 : 0;
		const int cellsY = haveBounds ? bMaxY - bMinY + 1 : 0;
		if ( wantLodt() ) {
			if ( lodtAoOnlyRadio->isChecked() )
				parts << tr( "refresh the AO plane of Terrain\\%1.lodl" ).arg( ws );
			else
				parts << tr( "Terrain\\%1.lodl (about %2 MB)" ).arg( ws )
					.arg( qMax( qint64( 1 ), ( qint64( cellsX ) * cellsY ) / 1024 ) );
		}
		if ( wantHeightmap() ) {
			const int fixed = heightmapSizeBox->currentData().toInt();
			const int w = fixed ? fixed : qMin( 8192, cellsX * 32 );
			const int h = fixed ? fixed : qMin( 8192, cellsY * 32 );
			parts << tr( "Textures\\Terrain\\%1\\%1.HeightMap.dds (%2 x %3, %4 MB)" ).arg( ws ).arg( w ).arg( h )
				.arg( ( qint64( w ) * h * 2 ) >> 20 );
		}
		if ( wantVt() ) {
			LodgenVtEstimateOut e;
			if ( vtEstimateNow( &e ) )
				parts << tr( "Terrain\\%1.VT.*.lodt (%2 levels, %3 tiles, about %4 GB)" ).arg( ws )
					.arg( e.levels ).arg( e.tiles )
					.arg( double( e.pyramidBytes ) / 1073741824.0, 0, 'f', 2 );
		}
		if ( chunks ) {
			QStringList kinds;
			if ( objectsCheck->isChecked() )
				kinds << tr( "object" );
			if ( btrCheck->isChecked() )
				kinds << tr( "terrain" );
			parts << tr( "%1 chunks (%2) under meshes\\terrain\\%3" ).arg( buildQueue().size() )
				.arg( kinds.join( tr( " and " ) ) ).arg( ws );
		}
		summary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		// the folder's own name stands for the mod; the whole path is the tooltip
		const QString mod = QFileInfo( outputDir() ).fileName();
		summary->setToolTip( outputDir() );
		summary->setText( tr( "Will write to %1%2: " ).arg( mod.isEmpty() ? outputDir() : mod )
			.arg( QDir( outputDir() ).exists() ? QString() : tr( " (a new folder)" ) )
			+ parts.join( QLatin1String( "; " ) ) + QChar( '.' ) );
	}

	void saveSettings()
	{
		QSettings s;
		// in MO2 mode the plugin list is MO2's, not yours: it is not saved over
		// the list you typed, so switching back brings yours home
		if ( !mo2Mode() ) {
			QStringList plugins;
			for ( int i = 0; i < pluginList->count(); i++ )
				plugins << pluginList->item( i )->text();
			s.setValue( QStringLiteral( "LodGeneration/plugins" ), plugins );
		}
		s.setValue( QStringLiteral( "LodGeneration/source" ), sourceBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/resources" ), resourceRows() );
		s.setValue( QStringLiteral( "LodGeneration/output" ), outputDir() );
		s.setValue( QStringLiteral( "LodGeneration/aoSamples" ), aoSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/overviewSamples" ), ovSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/objects" ), objectsCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/btr" ), btrCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/cover" ), coverCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/grassTint" ), tintSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/vt" ), vtCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/vtFinest" ), vtFinestBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/vtBtr" ), vtBtrCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/impostors" ), impostorEdit->text() );
		s.setValue( QStringLiteral( "LodGeneration/impostorFromLevel" ), impostorLevelBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/cardFrames" ), cardFramesBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/cardRes" ), cardResBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/cardHalfAux" ), cardHalfAuxCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/treeSway" ), swayCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/arrays" ), arraysCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/objectChannels" ), channelsCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/aoSkirt" ), aoSkirtSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/cullBuried" ), cullCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/cullMargin" ), cullMarginSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/slotFallback" ), slotFallbackCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/simplify" ), simplifyCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/simplifyRing8" ), simplify8Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/simplifyRing16" ), simplify16Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/simplifyRing32" ), simplify32Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/simplifyError" ), simplifyErrorSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/target" ), targetBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/lodt" ), lodtCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/heightmap" ), heightmapCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/West" ), x0Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/East" ), x1Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/South" ), y0Spin->value() );
		s.setValue( QStringLiteral( "LodGeneration/North" ), y1Spin->value() );
	}

	//! everything the worker needs, copied out of the widgets on the GUI thread
	struct WorldJob
	{
		QString plugins, outDir;
		quint32 form = 0;
		bool lodt = false, aoOnly = false, heightmap = false;
		int aoSamples = 8, overviewSamples = 8, heightmapSize = 0;
	};

	void start()
	{
		if ( running )
			return;
		if ( outputDir().isEmpty() ) {
			progress->setFormat( tr( "choose an output mod folder first" ) );
			return;
		}
		if ( !wsBox->currentData().toUInt() ) {
			progress->setFormat( tr( "choose a worldspace first" ) );
			return;
		}
		if ( mo2Mode() && !lodgenUnderMo2() ) {
			progress->setFormat( mo2Refusal() );
			return;
		}
		saveSettings();
		/* The stack goes in before anything reads an asset, and stays for the
		 * session so the viewport shows the same worldspace the bake sees. */
		installResources();
		QDir().mkpath( outputDir() );		// a new mod is a folder; Mod Organizer lists it on refresh
		cancelFlag = false;
		running = true;
		startButton->setEnabled( false );
		cancelButton->setEnabled( true );
		worldChanged();

		if ( previewCheck->isChecked() && skope && skope->getGLView() ) {
			// the whole landscape from above, as the request put it
			skope->getGLView()->setOrientation( GLView::ViewTop, true );
			skope->getGLView()->setProjection( false );
			framePending = true;
		}

		WorldJob job;
		job.plugins = pluginString();
		job.outDir = outputDir();
		job.form = wsBox->currentData().toUInt();
		job.lodt = wantLodt();
		job.aoOnly = lodtAoOnlyRadio->isChecked();
		job.heightmap = wantHeightmap();
		job.aoSamples = aoSpin->value();
		job.overviewSamples = ovSpin->value();
		job.heightmapSize = heightmapSizeBox->currentData().toInt();
		if ( job.lodt || job.heightmap ) {
			progress->setRange( 0, 1000 );
			progress->setValue( 0 );
			progress->setFormat( tr( "loading the worldspace\u2026" ) );
			if ( worker.joinable() )
				worker.join();
			worker = std::thread( [this, job]() { runWorldJob( job ); } );
		} else {
			startChunks();
		}
	}

	//! ON THE WORKER THREAD. Talks to the GUI only through post().
	void runWorldJob( const WorldJob & job )
	{
		auto post = [this]( std::function<void()> fn ) {
			QMetaObject::invokeMethod( this, std::move( fn ), Qt::QueuedConnection );
		};
		QString err;
		EsmWorld w;
		if ( !w.load( job.plugins, job.form, &err ) ) {
			post( [this, err]() { finishWorld( false, tr( "plugins: %1" ).arg( err ) ); } );
			return;
		}
		QString report;
		if ( job.lodt ) {
			if ( job.aoOnly ) {
				const QString path = job.outDir + QStringLiteral( "/Terrain/" ) + w.worldspaceEdid() + QStringLiteral( ".lodl" );
				post( [this]() { progress->setFormat( tr( "refreshing the AO plane\u2026 %p%" ) ); } );
				if ( !lodtRefreshAo( path, &err, [this, post]( int done, int total ) {
						post( [this, done, total]() { progress->setValue( total ? done * 1000 / total : 0 ); } );
						return !cancelFlag.load();
					} ) ) {
					post( [this, err]() { finishWorld( false, err ); } );
					return;
				}
				report += err + QChar( '\n' );
			} else {
				LodtOptions o;
				o.aoSamples = job.aoSamples;
				o.overviewSamples = job.overviewSamples;
				o.progress = [this, post]( int phase, int done, int total, int level, int i, int j ) {
					post( [this, phase, done, total, level, i, j]() {
						if ( phase == 0 ) {
							map->markRow( done, LodgenProgressMap::Scanned );
							progress->setFormat( tr( "reading cells\u2026 %p%" ) );
							progress->setValue( total ? done * 200 / total : 0 );
						} else {
							map->markLevel( level, i, j, LodgenProgressMap::State( qBound( 2, 5 - level, 5 ) ) );
							progress->setFormat( tr( "landscape level %1 \u2014 %p%" ).arg( level ) );
							progress->setValue( 200 + ( total ? done * 800 / total : 0 ) );
						}
					} );
					return !cancelFlag.load();
				};
				QString written;
				if ( !lodtWrite( w, job.outDir, o, &written, &err ) ) {
					post( [this, err]() { finishWorld( false, err ); } );
					return;
				}
				report += QStringLiteral( ".lodl: " ) + err.section( QChar( '\n' ), 0, 0 ) + QChar( '\n' );
			}
		}
		if ( job.heightmap && !cancelFlag ) {
			post( [this]() { progress->setFormat( tr( "baking the shadow heightmap\u2026" ) ); } );
			QString written;
			if ( !lodgenBakeHeightmap( w, job.outDir, job.heightmapSize, &written, &err ) ) {
				post( [this, err]() { finishWorld( false, err ); } );
				return;
			}
			report += QStringLiteral( "heightmap: " ) + QFileInfo( written ).fileName() + QChar( '\n' );
		}
		post( [this, report]() { finishWorld( true, report ); } );
	}

	//! GUI thread: the world job ended; hand over to the chunk loop or stop
	void finishWorld( bool ok, const QString & report )
	{
		if ( worker.joinable() )
			worker.join();
		lastReport = report;
		if ( !ok || cancelFlag ) {
			finishAll( ok ? tr( "cancelled" ) : report );
			return;
		}
		startChunks();
	}

	struct ChunkJob { int dim, cx, cy; };

	//! every chunk the range and ring selection ask for; the summary counts it, the run walks it
	QVector<ChunkJob> buildQueue() const
	{
		QVector<int> dims;
		if ( dimBox->currentIndex() == 0 )
			dims = { 4, 8, 16, 32 };
		else
			dims = { dimBox->currentText().toInt() };
		auto floorTo = []( int v, int m ) { return v >= 0 ? v - v % m : -( ( -v + m - 1 ) / m ) * m; };
		QVector<ChunkJob> q;
		for ( int d : dims )
			for ( int cy = floorTo( y0Spin->value(), d ); cy <= y1Spin->value(); cy += d )
				for ( int cx = floorTo( x0Spin->value(), d ); cx <= x1Spin->value(); cx += d )
					q.append( ChunkJob{ d, cx, cy } );
		return q;
	}

	void startChunks()
	{
		if ( !( objectsCheck->isChecked() || btrCheck->isChecked() || wantVt() ) ) {
			finishAll( tr( "done" ) );
			return;
		}
		QString error;
		world = std::make_unique<EsmWorld>();
		if ( !world->load( pluginString(), wsBox->currentData().toUInt(), &error ) ) {
			world.reset();
			finishAll( tr( "plugins: %1" ).arg( error ) );
			return;
		}
		queue = buildQueue();
		done = 0;
		writtenBto.clear();
		progress->setRange( 0, qMax( 1, queue.size() ) );
		progress->setValue( 0 );
		meshDir = outputDir() + QStringLiteral( "/meshes/terrain/" ) + world->worldspaceEdid();
		texDir = outputDir() + QStringLiteral( "/textures/terrain/" ) + world->worldspaceEdid();
		QDir().mkpath( meshDir );
		if ( texCheck->isChecked() && btrCheck->isChecked() )
			QDir().mkpath( texDir );
		/* ONE texture/LTEX/GRAS cache for the whole queue. Per chunk it would
		 * decode every landscape diffuse again, and the grass tint would reread
		 * seventy meshes for every chunk in the range. */
		lodgenDestroyBakeCaches( bakeCaches );
		bakeCaches = lodgenCreateBakeCaches();
		/* The pyramid runs ONCE and FIRST. When the chunk sheets come from it
		 * they are assembled while its staging is live, so a pass that ran
		 * after the chunk queue would have nothing to read; and the output is
		 * the mod folder itself - the writer creates Terrain/ under it. */
		vtSuppliesTex = false;
		if ( wantVt() ) {
			progress->setFormat( tr( "the terrain virtual texture\u2026" ) );
			QCoreApplication::processEvents();
			LodgenVtOptions vo = vtOptions();
			if ( vtBtrCheck->isChecked() && btrCheck->isChecked() && texCheck->isChecked() ) {
				vo.btrTexDir = texDir;
				vo.btrDims = QVector<int>{ 4, 8, 16, 32 };
				vtSuppliesTex = true;
			}
			vo.progress = &LodgenPanel::vtProgressThunk;
			vo.progressUser = this;
			QString rep, verr;
			if ( lodgenBakeTerrainVt( *world, QString(), outputDir(), vo, bakeCaches, &rep, &verr ) )
				lastReport = rep;
			else
				lastReport = tr( "terrain virtual texture: %1" ).arg( verr );
		}
		QTimer::singleShot( 0, this, [this]() { step(); } );
	}

	/*! The pyramid's per-row callback: repaint, and honour Cancel. A
	 *  whole-worldspace pass is minutes long and must not freeze the
	 *  window; the bake takes a plain function pointer so lodgen.cpp keeps
	 *  no Qt object of ours. */
	static bool vtProgressThunk( void * user, int rowsDone, int rowsTotal )
	{
		LodgenPanel * self = static_cast<LodgenPanel *>( user );
		if ( !self )
			return true;
		self->progress->setFormat( tr( "terrain virtual texture \u2014 tile row %1 of %2" )
			.arg( rowsDone ).arg( rowsTotal ) );
		QCoreApplication::processEvents();
		return !self->cancelFlag;
	}

	void step()
	{
		if ( cancelFlag || done >= queue.size() ) {
			QString tail;
			if ( !cancelFlag && !writtenBto.isEmpty() && objectsCheck->isChecked() && wantIdentity()
				&& arraysCheck->isChecked() ) {
				// before the atlas: the arrays key on the shapes' own diffuse paths
				const QString ws = world->worldspaceEdid();
				const QString arrDir = texDir + QStringLiteral( "/Objects" );
				QDir().mkpath( arrDir );
				progress->setFormat( tr( "writing the texture arrays\u2026" ) );
				QCoreApplication::processEvents();
				QString rep, aerr;
				if ( lodgenBuildTextureArrays( writtenBto, QString(),
					arrDir + "/" + ws + QStringLiteral( ".LodgenArrays" ),
					QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.LodgenArrays" ).arg( ws ), &rep, &aerr ) )
					tail += tr( ", arrays: %1" ).arg( rep );
				else
					tail += tr( ", arrays: %1" ).arg( aerr );
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && atlasCheck->isChecked() && objectsCheck->isChecked() ) {
				const QString ws = world->worldspaceEdid();
				const QString atlasDir = texDir + QStringLiteral( "/Objects" );
				QDir().mkpath( atlasDir );
				progress->setFormat( tr( "packing the object atlas\u2026" ) );
				QCoreApplication::processEvents();
				QString aerr;
				/* Vanilla's own sheet is DXT1 (measured), so the stock target gets
				 * BC1 with one-bit alpha for the cut-outs - parity and half the
				 * memory - and FO4CS keeps BC3's eight-bit alpha, which only a
				 * consumer that soft-blends card edges can spend. */
				const bool atlasBc1 = !fo4cs();
				if ( lodgenBuildAtlas( writtenBto, QString(),
					atlasDir + "/" + ws + QStringLiteral( ".LodgenObjects" ),
					QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.LodgenObjects" ).arg( ws ),
					outputDir(), atlasBc1, &aerr ) )
					tail = tr( ", atlas %1" ).arg( atlasBc1 ? tr( "written (BC1)" ) : tr( "written (BC3)" ) );
				else
					tail = tr( ", atlas: %1" ).arg( aerr );
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && objectsCheck->isChecked() ) {
				// last: one shape per material the engine can tell apart
				progress->setFormat( tr( "merging the chunk shapes\u2026" ) );
				QCoreApplication::processEvents();
				QString rep, merr;
				if ( lodgenMergeChunkShapes( writtenBto, &rep, &merr ) )
					tail += tr( ", merged %1" ).arg( rep );
				else
					tail += tr( ", merge: %1" ).arg( merr );
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && objectsCheck->isChecked()
				&& simplifyCheck->isChecked() ) {
				// the far rings, on the MERGED shapes: one proxy per cluster
				progress->setFormat( tr( "simplifying the far rings\u2026" ) );
				QCoreApplication::processEvents();
				LodgenSimplifyOptions sopts;
				sopts.ratio8 = float( simplify8Spin->value() );
				sopts.ratio16 = float( simplify16Spin->value() );
				sopts.ratio32 = float( simplify32Spin->value() );
				sopts.errorWorld = float( simplifyErrorSpin->value() );
				QString rep2, serr;
				if ( lodgenSimplifyFarRings( writtenBto, sopts, &rep2, &serr ) )
					tail += tr( ", far rings: %1" ).arg( rep2 );
				else
					tail += tr( ", far rings: %1" ).arg( serr );
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && objectsCheck->isChecked() && wantIdentity()
				&& arraysCheck->isChecked() && !impostorEdit->text().trimmed().isEmpty() ) {
				// the card sets the chunks stand on, as arrays beside the mesh arrays
				const QString ws = world->worldspaceEdid();
				const QString arrDir = texDir + QStringLiteral( "/Objects" );
				QDir().mkpath( arrDir );
				progress->setFormat( tr( "writing the card arrays\u2026" ) );
				QCoreApplication::processEvents();
				QString rep, cerr2;
				if ( lodgenBuildCardArrays( writtenBto, impostorEdit->text().trimmed(),
					arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ),
					QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.LodgenCards" ).arg( ws ),
					cardHalfAuxCheck->isChecked() ? 2 : 1, &rep, &cerr2 ) )
					tail += tr( ", card arrays: %1" ).arg( rep );
				else
					tail += tr( ", card arrays: %1" ).arg( cerr2 );
			}
			world.reset();
			lodgenDestroyBakeCaches( bakeCaches );
			bakeCaches = nullptr;
			writtenBto.clear();
			finishAll( cancelFlag ? tr( "cancelled after %1 chunk(s)" ).arg( done )
				: tr( "done \u2014 %1 chunk(s)%2" ).arg( done ).arg( tail ) );
			return;
		}
		const int dim = queue[done].dim;
		const int cx = queue[done].cx, cy = queue[done].cy;
		progress->setFormat( tr( "chunk %1 at (%2,%3) \u2014 %v of %m" ).arg( dim ).arg( cx ).arg( cy ) );
		const QString stem = QString( "%1.%2.%3.%4" ).arg( world->worldspaceEdid() ).arg( dim ).arg( cx ).arg( cy );
		bool okChunk = true;
		if ( btrCheck->isChecked() ) {
			NifModel nif;
			LodgenTerrainOptions opts;
			opts.dim = dim;
			opts.water = waterCheck->isChecked();
			opts.targetTrisPerCell = trisSpin->value();
			opts.terrainIdentity = wantTerrainId();
			opts.geomorph = geomorphCheck->isChecked();
			opts.shoreDenser = shoreCheck->isChecked();
			opts.shoreDensity = shoreDensitySpin->value();
			QString cerr;
			if ( lodgenBuildTerrainChunk( &nif, *world, cx, cy, opts, &cerr ) ) {
				nif.saveToFile( meshDir + "/" + stem + QStringLiteral( ".BTR" ) );
				preview( nif, cx, cy, stem + QStringLiteral( "_btr" ) );
				if ( texCheck->isChecked() && !vtSuppliesTex )
					lodgenBakeTerrainTextures( *world, cx, cy, dim, QString(), texDir,
						coverOptions(), bakeCaches, &cerr );
			} else {
				okChunk = false;
			}
		}
		if ( objectsCheck->isChecked() ) {
			NifModel nif;
			LodgenObjectOptions opts;
			opts.dim = dim;
			opts.identity = wantIdentity();
			opts.treeSway = opts.identity && swayCheck->isChecked();
			opts.objectChannels = opts.identity && channelsCheck->isChecked();
			opts.aoSkirtCells = aoSkirtSpin->value();
			opts.cullBuried = cullCheck->isChecked();
			opts.cullMargin = float( cullMarginSpin->value() );
			opts.slotFallback = slotFallbackCheck->isChecked();
			opts.bakeAO = aoCheck->isChecked();
			opts.dataRoot.clear();		// the game's own folders and archives
			opts.impostorDir = impostorEdit->text();
			opts.impostorFromLevel = impostorLevelBox->currentData().toInt();
			opts.cardAuxDiv = cardHalfAuxCheck->isChecked() ? 2 : 1;
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
			} else {
				okChunk = false;
			}
		}
		map->markChunk( dim, cx, cy, okChunk ? LodgenProgressMap::Chunk : LodgenProgressMap::Failed );
		done++;
		progress->setValue( done );
		QTimer::singleShot( 0, this, [this]() { step(); } );
	}

	void finishAll( const QString & message )
	{
		running = false;
		cancelButton->setEnabled( false );
		refreshSummary();		// Generate comes back only if the panel can still run
		progress->setFormat( message.isEmpty() ? tr( "done" ) : message.section( QChar( '\n' ), 0, 0 ) );
		if ( progress->maximum() > 0 && !cancelFlag )
			progress->setValue( progress->maximum() );
		map->setLabel( message.section( QChar( '\n' ), 0, 0 ) );
		cancelFlag = false;
	}

	//! Splice a finished chunk into the workspace at its WORLD position (the
	//! shipped file stays at the origin - the engine places it by filename).
	void preview( NifModel & nif, int cx, int cy, const QString & tag )
	{
		if ( !previewCheck->isChecked() || !skope )
			return;
		const QString previewPath = QDir::tempPath() + QStringLiteral( "/lodgen_preview_" ) + tag + QStringLiteral( ".nif" );
		bool added = false;
		if ( tag.endsWith( QStringLiteral( "_bto" ) ) ) {
			// BTO shapes self-place (world translation baked, per vanilla)
			added = nif.saveToFile( previewPath ) && skope->addWorkspaceDocumentFromFile( previewPath );
		} else {
			QModelIndex root = nif.getBlockIndex( 0 );
			nif.set<Vector3>( root, "Translation", Vector3( float( cx ) * 4096.0f, float( cy ) * 4096.0f, 0.0f ) );
			added = nif.saveToFile( previewPath ) && skope->addWorkspaceDocumentFromFile( previewPath );
			nif.set<Vector3>( root, "Translation", Vector3() );
		}
		/* Frame what has landed so far. As chunks accumulate the frame grows
		 * with them, which is the "whole landscape filling in" the request
		 * describes; the user can zoom once the run is over. */
		if ( added && skope->getGLView() && framePending )
			skope->getGLView()->frameAll();
	}

	NifSkope * skope;
	OrderedPathList * pluginList;
	OrderedPathList * resourceList = nullptr;
	QWidget * resourceHost = nullptr;
	QLabel * pluginLabel = nullptr, * resourceLabel = nullptr, * sourceStatus = nullptr;
	QComboBox * sourceBox = nullptr;
	QList<QWidget *> pluginButtons;
	QStringList mo2Plugins;
	//! the plugin list you typed, held while MO2 mode owns the list widget
	QStringList specifiedPlugins;
	QStringList savedManagerFolders;
	bool managerFoldersPushed = false;
	bool shownOnce = false;
	bool wsRefreshPending = false;
	QLineEdit * outEdit, * impostorEdit;
	QComboBox * impostorLevelBox = nullptr;
	QComboBox * cardFramesBox = nullptr;
	QLabel * cardFramesLabel = nullptr;
	QComboBox * cardResBox = nullptr;
	QLabel * cardResLabel = nullptr;
	QCheckBox * cardHalfAuxCheck = nullptr;
	QLabel * cardCostLabel = nullptr;

	/*! The grid and the RUN's resolution of the card sets already in
	 *  `impostorEdit`'s directory, read from the first `<id>_oct.lodm` there,
	 *  or zeroes when it holds none.
	 *
	 *  The run's resolution, not the frame: a frame below it is that base's
	 *  rung on the size ladder and perfectly correct. A card carries its own
	 *  geometry, so without this a run configured one way loads cards baked
	 *  another without a word, and the card arrays then split by sheet size
	 *  into separate arrays and separate binds. */
	QPair<int, int> cardsOnDisk() const
	{
		const QString dir = impostorEdit ? impostorEdit->text().trimmed() : QString();
		if ( dir.isEmpty() )
			return { 0, 0 };
		QDir d( dir );
		const QStringList sets = d.entryList( { QStringLiteral( "*_oct.lodm" ) }, QDir::Files, QDir::Name );
		if ( sets.isEmpty() )
			return { 0, 0 };
		QFile f( d.filePath( sets.first() ) );
		if ( !f.open( QIODevice::ReadOnly ) )
			return { 0, 0 };
		const LodmMaterial lm = lodmParse( f.readAll() );
		if ( !lm.ok )
			return { 0, 0 };
		const QJsonObject card = lm.root.value( QStringLiteral( "card" ) ).toObject();
		return { card.value( QStringLiteral( "oct" ) ).toInt(),
			card.value( QStringLiteral( "base" ) ).toInt() };
	}
	QLabel * impostorLevelLabel = nullptr;
	QComboBox * wsBox, * dimBox, * heightmapSizeBox;
	QComboBox * previewBox = nullptr;
	QLabel * previewLabel = nullptr;
	QSpinBox * x0Spin, * y0Spin, * x1Spin, * y1Spin, * trisSpin, * aoSpin, * ovSpin, * shoreDensitySpin;
	QSpinBox * aoSkirtSpin = nullptr, * cullMarginSpin = nullptr;
	QCheckBox * swayCheck = nullptr, * channelsCheck = nullptr, * cullCheck = nullptr, * slotFallbackCheck = nullptr;
	QCheckBox * simplifyCheck = nullptr;
	QDoubleSpinBox * simplify8Spin = nullptr, * simplify16Spin = nullptr,
		* simplify32Spin = nullptr, * simplifyErrorSpin = nullptr;
	QCheckBox * arraysCheck = nullptr;
	QCheckBox * lodtCheck, * heightmapCheck, * objectsCheck, * btrCheck;
	QRadioButton * lodtFullRadio, * lodtAoOnlyRadio;
	QCheckBox * texCheck, * identityCheck, * terrainIdCheck, * aoCheck, * waterCheck,
		* geomorphCheck, * atlasCheck, * shoreCheck, * previewCheck;
	QCheckBox * coverCheck = nullptr;
	QSpinBox * tintSpin = nullptr;
	QLabel * tintLabel = nullptr;
	QCheckBox * vtCheck = nullptr, * vtBtrCheck = nullptr;
	QComboBox * vtFinestBox = nullptr;
	QLabel * vtSummary = nullptr, * vtFinestLabel = nullptr;
	QList<QWidget *> vtSub;
	bool vtSuppliesTex = false;
	QList<QWidget *> objectsSub, btrSub;
	QWidget * rangeBox;
	QSplitter * splitter = nullptr;
	QScrollArea * scroll = nullptr;
	QComboBox * targetBox = nullptr;
	LodgenSection * lodtSection = nullptr, * heightmapSection = nullptr, * objectsSection = nullptr, * btrSection = nullptr;
	LodgenSection * vtSection = nullptr;
	QLabel * summary = nullptr;
	bool haveBounds = false;
	int bMinX = 0, bMinY = 0, bMaxX = -1, bMaxY = -1;
	QPushButton * wholeButton, * startButton, * cancelButton;
	QProgressBar * progress;
	LodgenProgressMap * map;
	std::unique_ptr<EsmWorld> world;
	LodgenBakeCaches * bakeCaches = nullptr;
	QVector<ChunkJob> queue;
	QStringList writtenBto;
	QString meshDir, texDir, lastReport;
	int done = 0;
	bool running = false;
	bool framePending = false;
	std::atomic<bool> cancelFlag { false };
	std::thread worker;
};

//! Batch > LOD Generation: opens the workspace of the same name.
class spWorldLodGenerator final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "LOD Generation workspace" ); }
	QString page() const override final { return Spell::tr( "Batch" ); }
	bool instant() const override final { return false; }
	bool constant() const override final { return true; }
	bool isApplicable( const NifModel *, const QModelIndex & ) override final { return true; }

	QModelIndex cast( NifModel *, const QModelIndex & index ) override final
	{
		NifSkope * win = qobject_cast<NifSkope *>( QApplication::activeWindow() );
		if ( !win ) {
			for ( QWidget * w : QApplication::topLevelWidgets() )
				if ( ( win = qobject_cast<NifSkope *>( w ) ) )
					break;
		}
		if ( win ) {
			if ( auto * wsBtn = win->findChild<QToolButton *>( QStringLiteral( "ViewWorkspacesButton" ) ) ) {
				if ( QMenu * m = wsBtn->menu() )
					for ( QAction * a : m->actions() )
						if ( a->text().remove( QLatin1Char( '&' ) ) == QLatin1String( "LOD Generation" ) ) {
							a->trigger();
							break;
						}
			}
		}
		return index;
	}
};

REGISTER_SPELL( spWorldLodGenerator )

} // namespace

//! The LOD Generation workspace dock. Same shape as the other manager docks.
QDockWidget * tlCreateLodGenerationDock( NifModel *, QMainWindow * mw, GLView * )
{
	auto * dock = new QDockWidget( QObject::tr( "LOD Generation" ), mw );
	dock->setObjectName( QStringLiteral( "LodGenerationDock" ) );
	// the panel scrolls its own settings and pins its action bar: no wrapper
	auto * panel = new LodgenPanel( qobject_cast<NifSkope *>( mw ) );
	dock->setWidget( panel );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
