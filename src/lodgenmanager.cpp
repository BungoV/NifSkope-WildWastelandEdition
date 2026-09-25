/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodbfile.h"
#include "lodgen.h"
#include "lodgenchunkpass.h"
#include "lodgenparallel.h"
#include "lodgenlayout.h"
#include "lodgenloadorder.h"
#include "nativeemit.h"
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
#include <QDebug>
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
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>

#include <QElapsedTimer>

#include <atomic>
#include <functional>
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
		headerRow = header;
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

	/*! A SECOND check in the header row, of which exactly one is shown at a
	 *  time (lane LODUI1). The object settings below are the same settings
	 *  whichever reader the run is for -- the same placements, channels,
	 *  cull, simplification and cards -- and only the FILE they end up in
	 *  differs: `.bto` chunks for the stock engine, the `.lodo`/`.lodi` pair
	 *  for FO4 Community Shaders. One section with two faces keeps every
	 *  setting reachable under both targets; two sections would have split
	 *  them and doubled the rows. */
	void addHeaderCheck( QCheckBox * alt )
	{
		if ( headerRow && alt )
			headerRow->addWidget( alt, 1 );
	}

private:
	void apply()
	{
		bodyWidget->setVisible( open );
		arrow->setArrowType( open ? Qt::DownArrow : Qt::RightArrow );
	}
	QHBoxLayout * headerRow = nullptr;
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

		/* THE 2026-09-12 RULINGS REACH A PANEL THAT HAS ALREADY BEEN SAVED
		 * (lane DEFAULTS1). Five rows had their defaults moved by bungo's
		 * rulings of that day, but a panel that has been used once holds the
		 * OLD numbers in QSettings and would keep baking the old look while
		 * the command line baked the new one. This runs once, and it only
		 * touches a key that is still EXACTLY the old default -- a number he
		 * chose himself is not the old default and is left alone. The marker
		 * is what makes it once; delete it and the sweep runs again. */
		if ( !settings.value( QStringLiteral( "LodGeneration/defaults1Applied" ), false ).toBool() ) {
			struct Moved { const char * key; double was, now; };
			static const Moved moved[] = {
				{ "landHex",         0.0, 256.0 },	// bungo's pick, panel (c)
				{ "landWarp",        0.0, 341.0 },
				{ "landMipBias",     0.0,  -0.22 },
				{ "landGuide",       0.0,   5.0 },	// off -> flat warp
				{ "roadGroundPaint", 1.0,   0.0 },	// the verge is not road
			};
			int moveds = 0, kept = 0;
			for ( const Moved & m : moved ) {
				const QString k = QStringLiteral( "LodGeneration/" ) + QLatin1String( m.key );
				if ( !settings.contains( k ) )
					continue;			// absent: the row's own new default is what it reads
				const double v = settings.value( k ).toDouble();
				if ( qAbs( v - m.was ) <= 1.0e-6 ) {
					settings.setValue( k, m.now );
					moveds++;
				} else {
					kept++;			// his own number, untouched
				}
			}
			settings.setValue( QStringLiteral( "LodGeneration/defaults1Applied" ), true );
			if ( moveds || kept )
				qDebug() << "LOD Generation: 2026-09-12 defaults applied to" << moveds
					<< "saved rows," << kept << "left as they were set";
		}

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

		/* ---- THE EXTRA ROWS' FIVE MAKERS (lane PANEL1, 2026-09-12) --------
		 * bungo: "They should all be configurable in the gen menu". Fifty-odd
		 * rows written out one at a time is fifty-odd chances to forget the
		 * scrub field, the tooltip, the save or the load, so each kind of row
		 * is made ONCE here and every row is registered under its settings key
		 * in `extras`. The save, the load, the run and the self-test all walk
		 * that one registry, so they cannot drift from each other.
		 *
		 * The DEFAULT handed to each maker is the command line's own default,
		 * unchanged (bungo's call, not this lane's), and it is kept beside the
		 * row: a row the target has HIDDEN reads back as its default, which is
		 * the panel's tick-AND-VISIBLE rule applied to a value. */
		auto xReg = [this]( const QString & key, QWidget * field, const QVariant & dflt, QLabel * label ) {
			extras.insert( key, WwExtraRow{ field, dflt } );
			if ( label )
				extraLabels.insert( key, label );
		};
		// a fractional number: drag to scrub, click to type (wwMakeScrubField
		// also guards the wheel, so a scroll over it does not change it)
		auto xD = [this, page, &settings, &xReg]( Form & f, const char * name, const QString & key,
				const QString & label, double dflt, double lo, double hi, int dec, double step,
				const QString & tip ) {
			auto * w = new QDoubleSpinBox( page );
			w->setObjectName( QLatin1String( name ) );
			w->setRange( lo, hi );
			w->setDecimals( dec );
			w->setSingleStep( step );
			w->setValue( settings.value( QStringLiteral( "LodGeneration/" ) + key, dflt ).toDouble() );
			w->setToolTip( tip );
			wwMakeScrubField( w );
			xReg( key, w, dflt, f.add( page, label, w ) );
		};
		// a whole number
		auto xI = [this, page, &settings, &xReg]( Form & f, const char * name, const QString & key,
				const QString & label, int dflt, int lo, int hi, const QString & tip ) {
			auto * w = new QSpinBox( page );
			w->setObjectName( QLatin1String( name ) );
			w->setRange( lo, hi );
			w->setValue( settings.value( QStringLiteral( "LodGeneration/" ) + key, dflt ).toInt() );
			w->setToolTip( tip );
			wwMakeScrubField( w );
			xReg( key, w, dflt, f.add( page, label, w ) );
		};
		// a one-of-several selector; the item DATA is what the run reads
		auto xC = [this, page, &settings, &xReg]( Form & f, const char * name, const QString & key,
				const QString & label, int dflt, const QList<QPair<QString, int>> & items,
				const QString & tip ) {
			auto * w = new QComboBox( page );
			w->setObjectName( QLatin1String( name ) );
			for ( const QPair<QString, int> & it : items )
				w->addItem( it.first, it.second );
			const int idx = w->findData( settings.value( QStringLiteral( "LodGeneration/" ) + key, dflt ).toInt() );
			w->setCurrentIndex( idx < 0 ? qMax( 0, w->findData( dflt ) ) : idx );
			w->setToolTip( tip );
			wwMatchFieldStyle( w );
			xReg( key, w, dflt, f.add( page, label, w ) );
		};
		// a ticked box, Blender's kind: the whole row is the box and its words
		auto xB = [this, page, &settings, &xReg]( Form & f, const char * name, const QString & key,
				const QString & label, bool dflt, const QString & tip ) {
			auto * w = new QCheckBox( label, page );
			w->setObjectName( QLatin1String( name ) );
			w->setChecked( settings.value( QStringLiteral( "LodGeneration/" ) + key, dflt ).toBool() );
			w->setToolTip( tip );
			xReg( key, w, dflt, nullptr );
			f.span( w );
		};
		// a folder, with the same browse button every other folder row has
		auto xP = [this, page, &settings, &xReg, &browseHost]( Form & f, const char * name, const QString & key,
				const QString & label, const QString & dflt, const QString & tip, bool folder ) {
			auto * w = new QLineEdit( settings.value( QStringLiteral( "LodGeneration/" ) + key, dflt ).toString(), page );
			w->setObjectName( QLatin1String( name ) );
			w->setPlaceholderText( dflt.isEmpty() ? tr( "optional" ) : dflt );
			w->setToolTip( tip );
			xReg( key, w, dflt, f.add( page, label, folder ? browseHost( w, label ) : static_cast<QWidget *>( w ) ) );
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
		sourceBox->addItem( tr( "Mod Organizer 2 profile" ), 2 );
		{
			const int saved = settings.value( QStringLiteral( "LodGeneration/source" ), 0 ).toInt();
			sourceBox->setCurrentIndex( ( saved == 1 || saved == 2 ) ? saved : 0 );
		}
		sourceBox->setToolTip( tr( "Where the world and its assets come from. Specified is the lists below, in\n"
			"your order. Mod Organizer 2 takes the profile's enabled plugins and their\n"
			"archives from the virtual Data folder NifSkope was launched into - add\n"
			"NifSkope to Mod Organizer's executable list, the way FO4Edit is added.\n"
			"Mod Organizer 2 profile reads a profile's modlist.txt and plugins.txt off\n"
			"disk, with no Mod Organizer running: each plugin from its own mod folder, the\n"
			"enabled mods stacked in its priority order, disabled mods left out." ) );
		wwMatchFieldStyle( sourceBox );
		src.add( page, tr( "Source" ), sourceBox );

		/* THE PROFILE, OFF DISK (lane LOADORDER1, 2026-09-24). The same reader
		 * as the command line's --mo2-profile / --mo2-mods
		 * (lodgenLoadOrderFromMo2), so the panel and a scripted bake resolve one
		 * load order the same way. Shown only under that source. */
		mo2ProfileEdit = new QLineEdit( page );
		mo2ProfileEdit->setObjectName( QStringLiteral( "LodgenMo2ProfileEdit" ) );
		{
			QString p = settings.value( QStringLiteral( "LodGeneration/mo2Profile" ) ).toString();
			if ( p.isEmpty() && QFileInfo( QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default/modlist.txt" ) ).isFile() )
				p = QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default" );
			mo2ProfileEdit->setText( p );
		}
		mo2ProfileEdit->setPlaceholderText( tr( "the profile folder that holds modlist.txt" ) );
		mo2ProfileEdit->setToolTip( tr( "A Mod Organizer 2 profile folder (profiles\\<name>). Its modlist.txt orders\n"
			"the mods, the top line winning, and its plugins.txt lists the enabled plugins.\n"
			"Command line: --mo2-profile" ) );
		mo2ProfileHost = browseHost( mo2ProfileEdit, tr( "Mod Organizer 2 profile folder" ) );
		mo2ProfileLabel = src.add( page, tr( "Profile" ), mo2ProfileHost );
		mo2ModsEdit = new QLineEdit( settings.value( QStringLiteral( "LodGeneration/mo2Mods" ) ).toString(), page );
		mo2ModsEdit->setObjectName( QStringLiteral( "LodgenMo2ModsEdit" ) );
		mo2ModsEdit->setPlaceholderText( tr( "the instance's mods folder" ) );
		mo2ModsEdit->setToolTip( tr( "The Mod Organizer 2 instance's mods folder. Empty = the mods folder two\n"
			"levels above the profile, where Mod Organizer keeps it.\n"
			"Command line: --mo2-mods" ) );
		mo2ModsHost = browseHost( mo2ModsEdit, tr( "Mod Organizer 2 mods folder" ) );
		mo2ModsLabel = src.add( page, tr( "Mods folder" ), mo2ModsHost );
		for ( QLineEdit * e : { mo2ProfileEdit, mo2ModsEdit } )
			connect( e, &QLineEdit::textChanged, this, [this]( const QString & ) {
				if ( mo2DiskMode() )
					applySource( false );
			} );

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

		//! Mod Organizer 2 profile: the resolved stack, read only, lowest first
		mo2StackList = new QListWidget( page );
		mo2StackList->setObjectName( QStringLiteral( "LodgenMo2StackList" ) );
		mo2StackList->setMaximumHeight( 84 );
		mo2StackList->setSelectionMode( QAbstractItemView::NoSelection );
		mo2StackList->setToolTip( tr( "Where the meshes, textures and materials come from, resolved from the\n"
			"profile: the game's Data folder first, then each enabled mod from the bottom\n"
			"of the mod list to the top, then overwrite. The LAST row overrides the ones\n"
			"above it and a loose file beats an archive. Disabled mods are not in it." ) );
		mo2StackLabel = src.add( page, tr( "Mod order" ), mo2StackList, Qt::AlignTop );

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
		 * folder in the picker and that is the new mod. Under FO4 Community
		 * Shaders every file this panel writes lands inside it under ONE root,
		 * FO4CSLOD\ (bungo, 2026-09-16; lane LAYOUT1) -- the far shadow
		 * heightmap under Textures\Terrain\ is the one exception, because that
		 * path is the game's. Under the stock engine nothing moved: chunks go
		 * to meshes\terrain\ and their sheets to textures\terrain\. */
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
			"folder. Under FO4 Community Shaders everything lands under one root,\n"
			"%1\\<worldspace>\\, with the shared impostor cards in %1\\Cards\\ and the\n"
			"far shadow heightmap in Textures\\Terrain\\. Under the stock engine chunks go to\n"
			"meshes\\terrain\\ and their sheets to textures\\terrain\\. A folder that does not\n"
			"exist yet is created when you generate; enable it in Mod Organizer afterwards." )
			.arg( lodgenFo4csFolderName() ) );
		src.add( page, tr( "Output mod" ), browseHost( outEdit, tr( "Output mod folder" ) ) );
		/* THE ROOT, on the panel (lane LAYOUT1, 2026-09-16). One line under the
		 * output field saying where this bake's files land inside that mod
		 * folder, because the answer changed today and a person cannot see a
		 * layout from a folder picker. A label, not a sentence: it reads
		 * `FO4CSLOD\Commonwealth\`. Hidden under the stock engine, whose files
		 * did not move. */
		outRootLabel = new QLabel( page );
		outRootLabel->setObjectName( QStringLiteral( "LodgenOutputRootLabel" ) );
		outRootLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
		outRootName = src.add( page, tr( "LOD root" ), outRootLabel );
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
			"the whole worldspace in one file: %1\\<worldspace>\\<worldspace>.lodl." )
			.arg( lodgenFo4csFolderName() ) );
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
		/* THE NATIVE PAIR, the FO4CS face of the same section (bungo
		 * 2026-09-11 06:4x: "we should only have those 5 .lod types in fo4
		 * community shaders target"). The settings below belong to the OBJECT
		 * PASS, not to the .bto container, so the section keeps them and only
		 * its head changes with the target. */
		nativeCheck = new QCheckBox( tr( "Native object files (.lodo/.lodi)" ), page );
		nativeCheck->setObjectName( QStringLiteral( "LodgenNativeCheck" ) );
		nativeCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/native" ), true ).toBool() );
		nativeCheck->setToolTip( tr( "The object library and its instance table for FO4 Community Shaders, written to\n"
			"%1\\<worldspace>\\<worldspace>.lodo and .lodi: every LOD mesh once, a cluster\n"
			"ladder with screen error, and one 24-byte record per placement." )
			.arg( lodgenFo4csFolderName() ) );
		objectsSection->addHeaderCheck( nativeCheck );
		layout->addWidget( objectsSection );
		{
			Form f = form( 24 );
			/* OFF since 2026-09-12 (lane DEFAULTS1, bungo's 15:56 ruling), and
			 * the manifest is no longer part of what this row controls: the
			 * sidecar is written either way, so the row names only what goes
			 * inside the chunk. */
			identityCheck = new QCheckBox( tr( "Identity channels" ), page );
			identityCheck->setObjectName( QStringLiteral( "LodgenIdentityCheck" ) );
			identityCheck->setChecked( false );
			identityCheck->setToolTip( tr( "A per-vertex object identity inside the chunk, which FO4CS reads to treat each\n"
				"distant object as itself. Off, the chunk carries vanilla's vertex layout.\n"
				"The manifest beside the chunk is written either way.\nCommand line: --identity" ) );
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
				"that has no far LOD of its own.\n"
				"Empty under FO4 Community Shaders means the standard place, %1\\Cards\n"
				"under the output folder, when that folder is there; a folder typed here wins." )
				.arg( lodgenFo4csFolderName() ) );
			QWidget * impostorHost = browseHost( impostorEdit, tr( "Impostor card directory" ) );
			QLabel * impostorLabel = f.add( page, tr( "Impostor cards" ), impostorHost );
			/* TREES ONLY. bungo, 2026-09-11 07:0x: "I've only wanted trees for
			 * the impostors", then 07:1x "Make trees only a toggle" and 07:2x
			 * "let's keep it simple like that for now". ON is the tree set --
			 * TREE records, a model under a trees folder, a model file named
			 * tree... Off is the old rule and nothing else: any base whose ring
			 * slot is empty. A per-object picker is parked on his word. */
			treesOnlyCheck = new QCheckBox( tr( "Trees only" ), page );
			treesOnlyCheck->setObjectName( QStringLiteral( "LodgenTreesOnlyCheck" ) );
			treesOnlyCheck->setChecked(
				settings.value( QStringLiteral( "LodGeneration/treesOnly" ), true ).toBool() );
			treesOnlyCheck->setToolTip( tr( "On, only trees stand on cards; off, any object whose ring has no model of its own does." ) );
			f.span( treesOnlyCheck );
			impostorLevelBox = new QComboBox( page );
			impostorLevelBox->setObjectName( QStringLiteral( "LodgenImpostorLevelBox" ) );
			impostorLevelBox->addItem( tr( "Only where a ring has no model" ), -1 );
			impostorLevelBox->addItem( tr( "Ring 0 (dim 4) and beyond" ), 0 );
			impostorLevelBox->addItem( tr( "Ring 1 (dim 8) and beyond" ), 1 );
			impostorLevelBox->addItem( tr( "Ring 2 (dim 16) and beyond" ), 2 );
			impostorLevelBox->addItem( tr( "Ring 3 (dim 32)" ), 3 );
			impostorLevelBox->setCurrentIndex( qBound( 0,
				settings.value( QStringLiteral( "LodGeneration/impostorFromLevel" ), -1 ).toInt() + 1, 4 ) );
			impostorLevelBox->setToolTip( tr( "From this ring on a tree stands on its card even where the ring has a mesh; no other object is touched." ) );
			wwMatchFieldStyle( impostorLevelBox );
			// trees only in effect since 2026-09-11, and the label says so
			impostorLevelLabel = f.add( page, tr( "Tree cards from ring" ), impostorLevelBox );

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
			// bungo, 2026-09-11 07:3x, asked for 512: "Add it, why not". The
			// bake hook already clamps to 32..512; only this list stopped at 256.
			cardResBox->addItem( tr( "512 px" ), 512 );
			{
				// 256 px since 2026-09-23 (bungo: tree cards "8x8 at 2k", lane DEFAULTS2)
				const int saved = settings.value( QStringLiteral( "LodGeneration/cardRes" ), 256 ).toInt();
				const int idx = cardResBox->findData( saved );
				cardResBox->setCurrentIndex( idx >= 0 ? idx : 2 );
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
				treesOnlyCheck, impostorLevelBox, impostorLevelLabel, cardFramesBox, cardFramesLabel,
				cardResBox, cardResLabel, cardHalfAuxCheck, cardCostLabel,
				simplifyCheck, simplify8Spin, s8Label,
				simplify16Spin, s16Label, simplify32Spin, s32Label, simplifyErrorSpin, sErrLabel };
			auto sync = [this, aoSkirtLabel, cullMarginLabel, s8Label, s16Label, s32Label, sErrLabel]() {
				// EITHER head runs the object pass: .bto under the stock engine,
				// the .lodo/.lodi pair under FO4CS. The rows below are its.
				const bool on = objectPassOn();
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
			connect( nativeCheck, &QCheckBox::toggled, this, sync );
			connect( identityCheck, &QCheckBox::toggled, this, sync );
			connect( aoCheck, &QCheckBox::toggled, this, sync );
			connect( cullCheck, &QCheckBox::toggled, this, sync );
			connect( simplifyCheck, &QCheckBox::toggled, this, sync );
			syncObjectRows = sync;		// the target runs it again after it hides a head
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
			// OFF since 2026-09-12 (lane DEFAULTS1, bungo's 15:53 ruling)
			terrainIdCheck->setChecked( false );
			terrainIdCheck->setToolTip( tr( "Material class, wetness and water depth in the chunk's vertex colours, for FO4CS.\n"
				"Off, the .BTR carries vanilla's vertex layout.\nCommand line: --terrain-identity" ) );
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
			/* THE FINEST TEXEL DENSITY, ONE ROW (lane VTNORMAL1, bungo's ruling
			 * 2026-09-23 09:4x: three values, 16 the default). The item data is
			 * world units a texel; vtOptions() turns it into the finest level
			 * and the tile content, the same pairs `--vt-density` names. The
			 * old "Tile content" row is gone: it is decided here now. */
			vtFinestBox->addItem( tr( "32 units a texel" ), 32 );
			vtFinestBox->addItem( tr( "16 units a texel" ), 16 );
			vtFinestBox->addItem( tr( "8 units a texel" ), 8 );
			{
				const int d = settings.value( QStringLiteral( "LodGeneration/vtDensity" ), 16 ).toInt();
				vtFinestBox->setCurrentIndex( d == 32 ? 0 : ( d == 8 ? 2 : 1 ) );
			}
			vtFinestBox->setToolTip( tr( "How fine the pyramid's densest level is, in world units a texel.\n"
				"32 is vanilla's finest terrain ring (about 1.7 GB for the Commonwealth),\n"
				"16 is twice that on each side (about 6.4 GB), 8 is the upscaled normal\n"
				"sheets' own density (about 26 GB).\n"
				"Command line: --vt-density" ) );
			wwMatchFieldStyle( vtFinestBox );
			vtFinestLabel = f.add( page, tr( "Finest texel size" ), vtFinestBox );
			/* The rest of the pyramid's own numbers (lane PANEL1): they were
			 * command-line switches with no row, and they live here rather
			 * than in a section of their own because they only mean anything
			 * while the pyramid is being written. */
			xI( f, "LodgenVtBorderSpin", QStringLiteral( "vtBorder" ),
				tr( "Tile border" ), 8, 0, 64,
				tr( "How many texels of the neighbouring tile are copied around each tile so a\n"
					"filtered read never crosses the seam.\nCommand line: --vt-border" ) );
			xI( f, "LodgenVtMipsSpin", QStringLiteral( "vtMips" ),
				tr( "Tile mips" ), 2, 1, 8,
				tr( "How many mips each tile carries.\nCommand line: --vt-mips" ) );
			xC( f, "LodgenVtCompressBox", QStringLiteral( "vtCompress" ),
				tr( "Tile compression" ), 0,
				{ { tr( "None" ), 0 }, { tr( "Zlib" ), 1 } },
				tr( "How the tile payload is stored in the pyramid file.\n"
					"Command line: --vt-compress" ) );
			xB( f, "LodgenVtHeightCheck", QStringLiteral( "vtHeight" ),
				tr( "Carry a height layer" ), false,
				tr( "Each tile also carries the ground height, so a consumer can displace the\n"
					"far terrain from the pyramid instead of from a mesh.\n"
					"Command line: --vt-height" ) );
			xB( f, "LodgenVtFillVanillaCheck", QStringLiteral( "vtFillVanilla" ),
				tr( "Fill unpainted ground with vanilla's colour" ), false,
				tr( "Ground no landscape record paints is blended toward the game's own\n"
					"terrain LOD colour, matched in tone where painted ground meets it.\n"
					"Command line: --vt-fill-vanilla" ) );
			xB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),
				tr( "Ground cover in the colour layer" ), false,
				tr( "The ground cover is tinted into the colour tiles instead of being left in\n"
					"the mask layer for the consumer to apply.\n"
					"Command line: --vt-cover-in-color / --vt-cover-in-mask" ) );
			xB( f, "LodgenVtHalfAuxCheck", QStringLiteral( "vtHalfAux" ),
				tr( "Half-resolution normal, mask, height and emissive tiles" ), false,
				tr( "Keeps the colour at the finest texel size and halves each side of the\n"
					"other layers, which is most of the pyramid's size.\n"
					"Command line: --vt-half-aux" ) );
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
			// the pyramid's own numbers grey with it too
			for ( const char * k : { "vtBorder", "vtMips", "vtCompress",
					"vtHeight", "vtFillVanilla", "vtCoverInColor", "vtHalfAux" } ) {
				if ( QWidget * w = extras.value( QLatin1String( k ) ).field )
					vtSub << w;
				if ( QLabel * l = extraLabels.value( QLatin1String( k ) ) )
					vtSub << l;
			}
			// the half-resolution row moves the pyramid's size, so its summary
			if ( auto * ha = qobject_cast<QCheckBox *>( extras.value( QStringLiteral( "vtHalfAux" ) ).field ) )
				connect( ha, &QCheckBox::toggled, this, [this]( bool ) { refreshSummary(); } );
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

		/* ==== EVERY OTHER BAKE SETTING THE COMMAND LINE HAS =================
		 * bungo, 2026-09-12 15:4x: "Erosion is a knob in the menu, corret?" --
		 * it was not -- then "They should all be configurable in the gen menu,
		 * anything else we're missing in that menu?". The audit of `lodgen`'s
		 * own option parser (src/nifcli.cpp) is in the lane report; every bake
		 * setting it found without a row is a row here, grouped under the
		 * family its switches belong to.
		 *
		 * EVERY DEFAULT IS THE COMMAND LINE'S, unchanged: with nothing touched
		 * the panel writes the bytes a switch-free command line writes, and
		 * that is gate (a) of this lane. */

		// ---- Terrain --------------------------------------------------------
		layout->addWidget( wwHeading( tr( "Terrain" ), page ) );
		{
			Form f = form( 0 );
			xI( f, "LodgenWaterSubdivSpin", QStringLiteral( "waterSubdiv" ),
				tr( "Water subdivision" ), 3, 0, 8,
				tr( "How many times a water cell's quad is split before it is written, so a\n"
					"shoreline can follow the land instead of cutting across it.\n"
					"Command line: --water-subdiv" ) );
			layout->addLayout( f.g );
		}

		// ---- Land detail ----------------------------------------------------
		/* How the landscape TEXTURE is read into the far sheets: the repeat,
		 * the sample rule, the geometry that breaks the repeat, the guide that
		 * steers it, where the fine detail comes from, and the two colour
		 * terms. Every one of these is a texture decision, not a mesh one. */
		layout->addWidget( wwHeading( tr( "Land detail" ), page ) );
		{
			Form f = form( 0 );
			xD( f, "LodgenLandTilingSpin", QStringLiteral( "landTiling" ),
				tr( "Texture repeat" ), 341.3333, 16.0, 8192.0, 4, 16.0,
				tr( "How many world units one repeat of a landscape texture covers. 341.3333 is\n"
					"the engine's own number out of Fallout4.exe 1.10.155; 2048 is the way back\n"
					"to the bake before 2026-09-11.\nCommand line: --land-tiling" ) );
			xC( f, "LodgenLandSampleBox", QStringLiteral( "landSample" ),
				tr( "Sample rule" ), 0,
				{ { tr( "Footprint (one texel of the matching mip)" ), 0 },
				  { tr( "Average (the texture's mean, no repeat)" ), 1 },
				  { tr( "Stochastic (hex tiling)" ), 2 },
				  { tr( "Stochastic (warp)" ), 3 } },
				tr( "How a bake texel reads the landscape texture. Footprint is the plain\n"
					"sample and the default. Average reads the texture's 1x1 mip, so no\n"
					"periodic pattern can reach the sheet. The two stochastic modes each set\n"
					"the numbers below for you and hide them.\nCommand line: --land-sample" ) );
			xD( f, "LodgenLandDetailSpin", QStringLiteral( "landDetail" ),
				tr( "Detail over the average" ), 0.0, 0.0, 1.0, 2, 0.05,
				tr( "With the average rule, how much of the footprint sample's departure from\n"
					"that average is added back. 1 is the footprint bake exactly.\n"
					"Command line: --land-detail" ) );
			xD( f, "LodgenLandHexSpin", QStringLiteral( "landHex" ),
				tr( "Hex tile size" ), 256.0, 0.0, 4096.0, 1, 16.0,
				tr( "The size of one hexagonal tile of the stochastic tiling, in world units.\n"
					"0 turns it off; 256 is the default (bungo 2026-09-12). One repeat is\n"
					"341.3333.\nCommand line: --land-hex" ) );
			xD( f, "LodgenLandWarpSpin", QStringLiteral( "landWarp" ),
				tr( "Warp amplitude" ), 341.0, 0.0, 4096.0, 1, 16.0,
				tr( "How far the sample point is pushed around before it reads the texture, in\n"
					"world units. 0 turns it off; 341 is the default (bungo 2026-09-12).\n"
					"Command line: --land-warp" ) );
			xD( f, "LodgenLandWarpLatticeSpin", QStringLiteral( "landWarpLattice" ),
				tr( "Warp lattice" ), 1024.0, 16.0, 8192.0, 1, 64.0,
				tr( "The world size of one cell of the noise that does the pushing.\n"
					"Command line: --land-warp-lattice" ) );
			xI( f, "LodgenLandWarpOctavesSpin", QStringLiteral( "landWarpOctaves" ),
				tr( "Warp octaves" ), 1, 1, 8,
				tr( "How many halvings of that noise are summed.\n"
					"Command line: --land-warp-octaves" ) );
			xD( f, "LodgenLandMipBiasSpin", QStringLiteral( "landMipBias" ),
				tr( "Mip bias" ), -0.22, -4.0, 4.0, 2, 0.10,
				tr( "Shifts which mip of the landscape texture the sample comes from; negative\n"
					"is sharper. 0 turns it off; -0.22 is the default (bungo 2026-09-12).\n"
					"Command line: --land-mip-bias" ) );
			xC( f, "LodgenLandGuideBox", QStringLiteral( "landGuide" ),
				tr( "Guide rule" ), 5,
				{ { tr( "Off" ), 0 },
				  { tr( "Drag" ), 1 },
				  { tr( "Aspect" ), 2 },
				  { tr( "Aspect, hex" ), 3 },
				  { tr( "Slope warp" ), 4 },
				  { tr( "Flat warp" ), 5 } },
				tr( "Steers the land sample by the ground's own shape instead of by the grid:\n"
					"drag follows the downhill direction, aspect follows which way the slope\n"
					"faces, the two warps push the sample along it. Flat warp at strength 1 is\n"
					"the default (bungo 2026-09-12); Off turns it off.\nCommand line: --land-guide" ) );
			xD( f, "LodgenLandGuideStrengthSpin", QStringLiteral( "landGuideStrength" ),
				tr( "Guide strength" ), 1.0, 0.0, 4.0, 2, 0.05,
				tr( "How hard the guide rule pulls. 1 is the rule as written.\n"
					"Command line: --land-guide <rule>:<strength>" ) );
			xD( f, "LodgenLandGuideScaleSpin", QStringLiteral( "landGuideScale" ),
				tr( "Guide scale" ), 1024.0, 128.0, 2048.0, 1, 64.0,
				tr( "Over how many world units the guide reads the ground's shape.\n"
					"Command line: --land-guide-scale" ) );
			xD( f, "LodgenLandGuideSlopeSpin", QStringLiteral( "landGuideSlope" ),
				tr( "Guide slope reference" ), 0.5, 0.01, 4.0, 3, 0.05,
				tr( "The slope, as a tangent, the guide calls a full slope; gentler ground is\n"
					"steered proportionally less.\nCommand line: --land-guide-slope" ) );
			xC( f, "LodgenLandDetailSourceBox", QStringLiteral( "landDetailSource" ),
				tr( "Fine detail from" ), 1,
				{ { tr( "Nothing (our own normals only)" ), 0 },
				  { tr( "Vanilla's far terrain normals" ), 1 },
				  { tr( "Vanilla's normals, blended" ), 2 },
				  { tr( "Erosion (the grown pass)" ), 3 } },
				tr( "Where the fine relief in the far terrain's normal map comes from.\n"
					"Command line: --land-detail-source" ) );
			xP( f, "LodgenVanillaLodRootEdit", QStringLiteral( "vanillaLodRoot" ),
				tr( "Vanilla LOD root" ), QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ),
				tr( "The unpacked Data folder the vanilla far-terrain normals are read from.\n"
					"Command line: --vanilla-lod-root" ), true );
			xD( f, "LodgenLandShadeSpin", QStringLiteral( "landShade" ),
				tr( "Crevice shading" ), -3.242, -16.0, 16.0, 3, 0.1,
				tr( "How much the creases in the land darken the baked colour. 0 turns the\n"
					"crevice term off.\nCommand line: --land-shade" ) );
			xD( f, "LodgenLandGradeSpin", QStringLiteral( "landGrade" ),
				tr( "Colour grade" ), 1.0, 0.1, 4.0, 3, 0.01,
				tr( "Every baked colour texel is multiplied by this before it is written.\n"
					"1 is off and skips the branch, so the bake is unchanged.\n"
					"Command line: --grade" ) );
			xC( f, "LodgenBlendEdgesBox", QStringLiteral( "blendEdges" ),
				tr( "Quadrant edges" ), 1,
				{ { tr( "Hard" ), 0 }, { tr( "Cross-faded" ), 1 } },
				tr( "Cross-fades the neighbouring quadrant's composite over the margin below,\n"
					"either side of every 2,048-unit quadrant line. Cross-faded is the default;\n"
					"Hard is the bake without it.\nCommand line: --blend-edges" ) );
			xD( f, "LodgenBlendMarginSpin", QStringLiteral( "blendMargin" ),
				tr( "Quadrant margin" ), 128.0, 0.0, 1024.0, 1, 16.0,
				tr( "How wide that cross-fade is, in world units.\n"
					"Command line: --blend-margin" ) );
			layout->addLayout( f.g );
		}

		// ---- Erosion --------------------------------------------------------
		/* bungo asked for this one by name. Strength 0 is off and is the bake
		 * without it, byte for byte, so the section can sit at its defaults
		 * without moving anything. */
		layout->addWidget( wwHeading( tr( "Erosion" ), page ) );
		{
			Form f = form( 0 );
			xD( f, "LodgenErosionSpin", QStringLiteral( "erosion" ),
				tr( "Strength" ), 0.0, 0.0, 4.0, 3, 0.05,
				tr( "How far the grown erosion pass moves the far terrain's relief. 0 is off and\n"
					"is the bake without it, byte for byte.\nCommand line: --erosion" ) );
			xI( f, "LodgenErosionIterationsSpin", QStringLiteral( "erosionIterations" ),
				tr( "Rounds" ), 1, 1, 8,
				tr( "How many feedback rounds the pass runs.\n"
					"Command line: --erosion-iterations" ) );
			xI( f, "LodgenErosionSeedSpin", QStringLiteral( "erosionSeed" ),
				tr( "Seed" ), 1, 0, 2000000000,
				tr( "The number the pass's randomness starts from; the same seed gives the same\n"
					"erosion every time.\nCommand line: --erosion-seed" ) );
			layout->addLayout( f.g );
		}

		// ---- Sheets and cache -----------------------------------------------
		layout->addWidget( wwHeading( tr( "Sheets and cache" ), page ) );
		{
			Form f = form( 0 );
			xC( f, "LodgenSheetFormatBox", QStringLiteral( "sheetFormat" ),
				tr( "Sheet format" ), 0,
				{ { tr( "Legacy (BC1, eight mips)" ), 0 },
				  { tr( "Vanilla (BC3, mips to 1x1)" ), 1 } },
				tr( "How a chunk's texture sheets are compressed and how far their mip chain\n"
					"goes. Legacy is what this fork has always written.\n"
					"Command line: --sheet-format" ) );
			/* The tooltip described a decode cache this folder never was (lane
			 * VTNORMAL1): it names the cleaned or upscaled normal sheets, and
			 * since 2026-09-23 they are the pyramid's normal as well as the
			 * chunk sheets'. The row is remembered like every other one. */
			xP( f, "LodgenMsnCacheEdit", QStringLiteral( "msnCache" ),
				tr( "Normal sheets folder" ), QString(),
				tr( "A folder of cleaned or upscaled terrain normal sheets, one\n"
					"<world>.4.<x>.<y>_msn.DDS per chunk: the sheets' own folder, or the\n"
					"mod folder that holds them under Textures\\Terrain\\<world>. They are\n"
					"the chunk sheets' normal and the terrain pyramid's, reduced to its\n"
					"texel size; a chunk with no sheet keeps the normal from the heights.\n"
					"Empty: the last resource folder holding an upscaled set is used, if\n"
					"any. none: never.\n"
					"Command line: --msn-cache DIR|auto" ), true );
			layout->addLayout( f.g );
		}

		// ---- Ground cover ---------------------------------------------------
		layout->addWidget( wwHeading( tr( "Ground cover" ), page ) );
		{
			Form f = form( 0 );
			xD( f, "LodgenCoverFullSpin", QStringLiteral( "coverFull" ),
				tr( "Full cover at" ), 96.0, 0.0, 255.0, 1, 4.0,
				tr( "The ground-cover density, on the record's own 0..255 scale, that counts as\n"
					"fully covered; anything denser paints the same.\n"
					"Command line: --cover-full" ) );
			layout->addLayout( f.g );
		}

		// ---- Roads ----------------------------------------------------------
		/* A folding section whose header check IS `--roads`: roads on is the
		 * default, and the rows under it only mean anything while it is on.
		 * There is no opacity row -- bungo, 2026-09-12 16:0x, "we don't use
		 * that opacity at all, we render roads at their full diffuse" -- and no
		 * legacy row; both stay on the command line. */
		roadsCheck = new QCheckBox( tr( "Roads painted into the far terrain" ), page );
		roadsCheck->setObjectName( QStringLiteral( "LodgenRoadsCheck" ) );
		roadsCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/roads" ), true ).toBool() );
		roadsCheck->setToolTip( tr( "Paints the road models' own diffuse into the far terrain sheets.\n"
			"Command line: --roads / --no-roads" ) );
		extras.insert( QStringLiteral( "roads" ), WwExtraRow{ roadsCheck, true } );
		roadsSection = new LodgenSection( roadsCheck, QStringLiteral( "Roads" ), false, page );
		layout->addWidget( roadsSection );
		{
			Form f = form( 24 );
			xD( f, "LodgenRoadDetailSpin", QStringLiteral( "roadDetail" ),
				tr( "Diffuse detail kept" ), 1.0, 0.0, 1.0, 2, 0.05,
				tr( "How much of the road texture's own pattern is printed into the sheet;\n"
					"0 paints its average colour instead.\nCommand line: --road-detail" ) );
			xD( f, "LodgenRoadGroundPaintSpin", QStringLiteral( "roadGroundPaint" ),
				tr( "Verge painted as road" ), 0.0, 0.0, 1.0, 2, 0.05,
				tr( "How strongly the ground-material shapes inside a road model are painted as\n"
					"road; 0 -- the default since 2026-09-12 -- leaves the landscape's own\n"
					"colour on the verge.\n"
					"Command line: --road-ground-paint" ) );
			xD( f, "LodgenRoadCoverSuppressSpin", QStringLiteral( "roadCoverSuppress" ),
				tr( "Cover suppressed under" ), 1.0, 0.0, 4.0, 2, 0.05,
				tr( "How much of the ground cover the road geometry removes from under itself.\n"
					"Command line: --road-cover-suppress" ) );
			xC( f, "LodgenRoadCompositeBox", QStringLiteral( "roadComposite" ),
				tr( "Pieces combine by" ), 0,
				{ { tr( "Highest piece wins" ), 0 }, { tr( "Blended together" ), 1 } },
				tr( "Where two road pieces overlap, whether the higher one overwrites the other\n"
					"or the two are blended.\nCommand line: --road-composite" ) );
			xB( f, "LodgenRoadRaisedCheck", QStringLiteral( "roadRaised" ),
				tr( "Paint raised road families" ), false,
				tr( "Bridges, overpasses and the other raised families are painted onto the\n"
					"ground they pass over as well.\n"
					"Command line: --road-raised / --no-road-raised" ) );
			xB( f, "LodgenRoadSidewalksCheck", QStringLiteral( "roadSidewalks" ),
				tr( "Paint sidewalks" ), false,
				tr( "Sidewalk models are painted like roads.\n"
					"Command line: --road-sidewalks / --no-road-sidewalks" ) );
			roadsSection->body()->setLayout( f.g );
			auto sync = [this]() {
				const bool on = roadsCheck->isChecked();
				for ( const char * k : { "roadDetail", "roadGroundPaint", "roadCoverSuppress",
						"roadComposite", "roadRaised", "roadSidewalks" } )
					enableExtra( k, on );
			};
			connect( roadsCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		// ---- Object occlusion on the terrain --------------------------------
		/* OFF is the default and off is the rung's BYTES: the object term is
		 * its own horizon march and returns exactly 1 where nothing occludes,
		 * so the multiply cannot move a byte while the box is clear. */
		terrainObjAoCheck = new QCheckBox( tr( "Far terrain shaded by the objects on it" ), page );
		terrainObjAoCheck->setObjectName( QStringLiteral( "LodgenTerrainObjectAoCheck" ) );
		terrainObjAoCheck->setChecked(
			settings.value( QStringLiteral( "LodGeneration/terrainObjectAo" ), false ).toBool() );
		terrainObjAoCheck->setToolTip( tr( "The placed objects cast their ambient occlusion onto the far terrain\n"
			"under them.\nCommand line: --terrain-object-ao / --no-terrain-object-ao" ) );
		extras.insert( QStringLiteral( "terrainObjectAo" ), WwExtraRow{ terrainObjAoCheck, false } );
		terrainObjAoSection = new LodgenSection( terrainObjAoCheck, QStringLiteral( "TerrainObjectAo" ), false, page );
		layout->addWidget( terrainObjAoSection );
		{
			Form f = form( 24 );
			xD( f, "LodgenTerrainObjectAoStrengthSpin", QStringLiteral( "terrainObjectAoStrength" ),
				tr( "Strength" ), 0.5, 0.0, 4.0, 2, 0.05,
				tr( "How dark that shading goes.\n"
					"Command line: --terrain-object-ao-strength" ) );
			terrainObjAoSection->body()->setLayout( f.g );
			auto sync = [this]() { enableExtra( "terrainObjectAoStrength", terrainObjAoCheck->isChecked() ); };
			connect( terrainObjAoCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		// ---- Water bodies in the landscape file -----------------------------
		/* A module of the `.lodl` writer, off by default: unarmed, the file is
		 * the one the same bake wrote before the module existed. */
		waterBodiesCheck = new QCheckBox( tr( "Water bodies in the landscape file" ), page );
		waterBodiesCheck->setObjectName( QStringLiteral( "LodgenWaterBodiesCheck" ) );
		waterBodiesCheck->setChecked(
			settings.value( QStringLiteral( "LodGeneration/waterBodies" ), false ).toBool() );
		waterBodiesCheck->setToolTip( tr( "Writes each connected body of water, its shore and its flow into the\n"
			".lodl beside the landscape.\nCommand line: --water-bodies" ) );
		extras.insert( QStringLiteral( "waterBodies" ), WwExtraRow{ waterBodiesCheck, false } );
		waterBodiesSection = new LodgenSection( waterBodiesCheck, QStringLiteral( "WaterBodies" ), false, page );
		layout->addWidget( waterBodiesSection );
		{
			Form f = form( 24 );
			xI( f, "LodgenWaterBridgeSpin", QStringLiteral( "waterBridge" ),
				tr( "Bridge gap" ), 2, 0, 16,
				tr( "How many empty cells two patches of water may be apart and still count as\n"
					"one body.\nCommand line: --water-bridge" ) );
			xI( f, "LodgenWaterNearSpin", QStringLiteral( "waterNear" ),
				tr( "Near texels" ), 64, 0, 512,
				tr( "The texel width of the near band a body carries for the shore.\n"
					"Command line: --water-near" ) );
			xI( f, "LodgenWaterBodySamplesSpin", QStringLiteral( "waterBodySamples" ),
				tr( "Body samples" ), 0, 0, 4096,
				tr( "How many points inside each body are sampled; 0 lets the writer choose.\n"
					"Command line: --water-body-samples" ) );
			xI( f, "LodgenWaterFlowSamplesSpin", QStringLiteral( "waterFlowSamples" ),
				tr( "Flow samples" ), 0, 0, 4096,
				tr( "How many points the flow direction is measured at; 0 lets the writer\n"
					"choose.\nCommand line: --water-flow-samples" ) );
			xB( f, "LodgenWaterShoreCheck", QStringLiteral( "waterShore" ),
				tr( "Write the shore line" ), true,
				tr( "The outline where each body meets the land.\n"
					"Command line: --water-no-shore" ) );
			xP( f, "LodgenWaterVelocitiesEdit", QStringLiteral( "waterVelocities" ),
				tr( "Velocity plugin" ), QString(),
				tr( "A plugin that supplies measured water velocities instead of the writer's\n"
					"own estimate.\nCommand line: --water-velocities" ), false );
			waterBodiesSection->body()->setLayout( f.g );
			auto sync = [this]() {
				const bool on = waterBodiesCheck->isChecked();
				for ( const char * k : { "waterBridge", "waterNear", "waterBodySamples",
						"waterFlowSamples", "waterShore", "waterVelocities" } )
					enableExtra( k, on );
			};
			connect( waterBodiesCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		// ---- Aggregate impostors --------------------------------------------
		/* Ring 3's forested cells stand on one composited sheet instead of one
		 * card each. Off by default, and it REFUSES IN WORDS without a card
		 * directory rather than writing an empty table. */
		aggregateCheck = new QCheckBox( tr( "Aggregate impostors for forested cells" ), page );
		aggregateCheck->setObjectName( QStringLiteral( "LodgenAggregateCheck" ) );
		aggregateCheck->setChecked(
			settings.value( QStringLiteral( "LodGeneration/aggregate" ), false ).toBool() );
		aggregateCheck->setToolTip( tr( "In ring 3, a forested cell's trees are photographed together onto one\n"
			"sheet, composited from their own cards. Needs the impostor card folder.\n"
			"Command line: --aggregate / --no-aggregate" ) );
		extras.insert( QStringLiteral( "aggregate" ), WwExtraRow{ aggregateCheck, false } );
		aggregateSection = new LodgenSection( aggregateCheck, QStringLiteral( "Aggregate" ), false, page );
		layout->addWidget( aggregateSection );
		{
			Form f = form( 24 );
			xI( f, "LodgenAggregateMinSpin", QStringLiteral( "aggregateMin" ),
				tr( "Forested at" ), 8, 1, 256,
				tr( "How many tree placements make a cell forested enough to aggregate.\n"
					"Command line: --aggregate-min" ) );
			xI( f, "LodgenAggregateTileSpin", QStringLiteral( "aggregateTile" ),
				tr( "Frame size" ), 64, 8, 512,
				tr( "The long side of one aggregate frame, in texels.\n"
					"Command line: --aggregate-tile" ) );
			xI( f, "LodgenAggregateViewsSpin", QStringLiteral( "aggregateViews" ),
				tr( "Views" ), 8, 1, 32,
				tr( "How many directions around the cell are photographed.\n"
					"Command line: --aggregate-views" ) );
			aggregateSection->body()->setLayout( f.g );
			auto sync = [this]() {
				const bool on = aggregateCheck->isChecked();
				for ( const char * k : { "aggregateMin", "aggregateTile", "aggregateViews" } )
					enableExtra( k, on );
			};
			connect( aggregateCheck, &QCheckBox::toggled, this, sync );
			sync();
		}

		// ---- Objects --------------------------------------------------------
		/* The two native-pair modules and the two legacy chunk steps. Which
		 * pair is shown follows the target, in applyTarget below. */
		layout->addWidget( wwHeading( tr( "Object modules" ), page ) );
		{
			Form f = form( 0 );
			xB( f, "LodgenNativeLadderCheck", QStringLiteral( "nativeLadder" ),
				tr( "Build the distance ladder" ), false,
				tr( "The object library carries several levels per mesh with a measured error\n"
					"for each, so the runtime can pick one by distance. Off, it is one level\n"
					"per mesh, the authored LOD model as is (the default).\n"
					"Command line: --native-ladder / --native-no-ladder" ) );
			xB( f, "LodgenNativeOccludersCheck", QStringLiteral( "nativeOccluders" ),
				tr( "Build the occluder boxes" ), true,
				tr( "The .lodi carries a box per large object so the runtime can skip what is\n"
					"behind it. Off, the box table is empty.\n"
					"Command line: --native-no-occluders" ) );
			xB( f, "LodgenMergeCheck", QStringLiteral( "merge" ),
				tr( "Merge the chunk shapes" ), true,
				tr( "After the chunks are written, the shapes in each are merged down to one per\n"
					"material the engine can tell apart.\n"
					"Command line: --merge / --no-merge" ) );
			xB( f, "LodgenKeepBtoCheck", QStringLiteral( "keepBto" ),
				tr( "Keep legacy .BTO chunks" ), false,
				tr( "The FO4CS target builds the .BTO chunk files in a scratch folder and\n"
					"removes them once the texture arrays, the card arrays, the shape merge\n"
					"and the far-ring cut have read them: nothing after the bake reads them.\n"
					"On, they are left in the mod folder as bakes before 2026-09-16 left\n"
					"them, byte for byte. The manifest sidecar is kept either way, and the\n"
					"stock engine target is not affected.\nCommand line: --keep-bto" ) );
			xC( f, "LodgenAtlasFormatBox", QStringLiteral( "atlasFormat" ),
				tr( "Atlas format" ), -1,
				{ { tr( "Match the target" ), -1 },
				  { tr( "BC1 (one-bit alpha)" ), 1 },
				  { tr( "BC3 (eight-bit alpha)" ), 0 } },
				tr( "How the packed object atlas is compressed. Matching the target is what the\n"
					"panel has always done: BC1 for the stock engine, which is what vanilla\n"
					"ships, BC3 for FO4 Community Shaders.\nCommand line: --atlas-bc1" ) );
			layout->addLayout( f.g );
		}

		// ---- Run ------------------------------------------------------------
		layout->addWidget( wwHeading( tr( "Run" ), page ) );
		{
			Form f = form( 0 );
			xI( f, "LodgenThreadsSpin", QStringLiteral( "threads" ),
				tr( "Model threads" ), 0, 0, 64,
				tr( "How many threads read and build models. 0 is one per core; 1 is the exact\n"
					"way back to a serial run.\nCommand line: --threads" ) );
			xI( f, "LodgenChunkThreadsSpin", QStringLiteral( "chunkThreads" ),
				tr( "Chunk threads" ), 1, 1, 64,
				tr( "How many chunks are baked at once. More is slower and much hungrier here\n"
					"-- each worker owns its own plugin reader and texture cache -- which is\n"
					"why one is the default.\nCommand line: --chunk-threads" ) );
			/* REBAKE ONLY WHAT CHANGED (lane INCRGATE1, 2026-09-24): INCR1's
			 * `--incremental` as a standing row, OFF by default -- with it off the
			 * bake is byte for byte what it was. On, every run writes the bake
			 * record and the per-chunk native caches, and a run that finds a
			 * record beside it rebuilds only the chunks whose inputs moved. A run
			 * the record cannot vouch for (another range or chunk size, a setting
			 * moved, a region-wide product ticked) bakes whole, rewrites the record
			 * and says why: the command line refuses there, but a standing row
			 * that refused would have no way back short of deleting the record.
			 * WW_LODGEN_INCREMENTAL=0|1 forces it for a harness. */
			xB( f, "LodgenIncrementalCheck", QStringLiteral( "incremental" ),
				tr( "Rebake only what changed" ), false,
				tr( "Keep a bake record in the output folder and, on the next run, rebuild only\n"
					"the chunks whose plugins, models or textures changed. The first run bakes\n"
					"everything and writes the record. Another range or chunk size, a changed\n"
					"setting, or texture arrays, the object atlas or card arrays ticked: the\n"
					"run bakes everything again and says why. One chunk size at a time. A model\n"
					"or texture edited while NifSkope stays open is seen after a restart.\n"
					"Command line: --incremental" ) );
			if ( qEnvironmentVariableIsSet( "WW_LODGEN_INCREMENTAL" ) )
				if ( auto * c = qobject_cast<QCheckBox *>( extras.value( QStringLiteral( "incremental" ) ).field ) )
					c->setChecked( qEnvironmentVariableIntValue( "WW_LODGEN_INCREMENTAL" ) != 0 );
			layout->addLayout( f.g );
		}

		/* The stochastic sample's own numbers belong to the SELECTOR when a
		 * stochastic mode is chosen: those rows go away rather than sit there
		 * disagreeing with the mode above them. The command line composes by
		 * the order the switches are typed in, which a panel has no way to
		 * say, so the panel says it by hiding what the mode owns. */
		{
			auto sync = [this]() {
				const int mode = xi( "landSample" );
				const bool own = mode < 2;
				for ( const char * k : { "landHex", "landWarp", "landMipBias" } )
					showExtra( k, own );
				enableExtra( "landDetail", mode == 1 );
			};
			connect( qobject_cast<QComboBox *>( extras.value( QStringLiteral( "landSample" ) ).field ),
				&QComboBox::currentIndexChanged, this, [sync]( int ) { sync(); } );
			sync();
		}
		// the guide's three numbers mean nothing while the rule is off
		{
			auto sync = [this]() {
				const bool on = xi( "landGuide" ) != 0;
				for ( const char * k : { "landGuideStrength", "landGuideScale", "landGuideSlope" } )
					enableExtra( k, on );
			};
			connect( qobject_cast<QComboBox *>( extras.value( QStringLiteral( "landGuide" ) ).field ),
				&QComboBox::currentIndexChanged, this, [sync]( int ) { sync(); } );
			sync();
		}
		// erosion's rounds and seed mean nothing at strength 0
		{
			auto sync = [this]() {
				const bool on = xf( "erosion" ) > 0.0f;
				enableExtra( "erosionIterations", on );
				enableExtra( "erosionSeed", on );
			};
			connect( qobject_cast<QDoubleSpinBox *>( extras.value( QStringLiteral( "erosion" ) ).field ),
				&QDoubleSpinBox::valueChanged, this, [sync]( double ) { sync(); } );
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
		auto syncRange = [this]() { rangeBox->setEnabled( objectPassOn() || btrCheck->isChecked() ); };
		connect( objectsCheck, &QCheckBox::toggled, this, syncRange );
		connect( nativeCheck, &QCheckBox::toggled, this, syncRange );
		connect( btrCheck, &QCheckBox::toggled, this, syncRange );
		syncRangeRows = syncRange;
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
		/* THE RESULT LINE, under the summary in the pinned bar: what the last
		 * run cost, by stage. bungo asked for the four stage times of a GUI
		 * bake (2026-09-11 10:0x, the end-to-end item), and a number that only
		 * exists in a log is a number he has to go and find. Empty until a run
		 * finishes, so the bar does not carry a sentence about nothing. */
		resultLabel = new QLabel( actionBar );
		resultLabel->setObjectName( QStringLiteral( "LodgenResultLabel" ) );
		resultLabel->setWordWrap( true );
		resultLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
		resultLabel->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		resultLabel->setVisible( false );
		{
			auto * col = new QVBoxLayout();
			col->setContentsMargins( 0, 0, 0, 0 );
			col->setSpacing( 2 );
			col->addWidget( summary );
			col->addWidget( resultLabel );
			ab->addLayout( col, 1 );
		}
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
		/* THE FIVE .lod TYPES (bungo, 2026-09-11 06:4x, verbatim: "we should
		 * only have those 5 .lod types in fo4 community shaders target").
		 * Under FO4 Community Shaders the panel offers `.lodl`, `.lodt`, the
		 * `.lodo`/`.lodi` pair and the `.lodm` sidecars that ride with them,
		 * and every legacy row goes: the `.btr` section whole (with its
		 * terrain-texture bake, its cover rows and its identity channels), the
		 * `.bto` head, the object ATLAS (a stock draw-call optimisation --
		 * FO4CS binds the texture arrays instead) and "Chunk textures from the
		 * pyramid", which exists only to fill legacy chunk sheets. The stock
		 * engine keeps all of them and loses the native head instead. */
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
			// the object pass has two heads and exactly one is offered
			objectsCheck->setVisible( !cs );
			nativeCheck->setVisible( cs );
			// legacy, and hidden whole under FO4CS
			btrSection->setVisible( !cs );
			atlasCheck->setVisible( !cs );
			vtBtrCheck->setVisible( !cs );
			// the stock engine has no .lodt texture-pyramid reader and no VT sampler
			vtSection->setVisible( cs );
			// the one root is the FO4CS target's; the stock tree did not move
			if ( outRootLabel ) outRootLabel->setVisible( cs );
			if ( outRootName ) outRootName->setVisible( cs );
			/* The two native-pair modules belong to the FO4CS head and the two
			 * legacy chunk steps to the stock one; each pair is hidden under
			 * the other target, and a hidden row reads as its default. */
			showExtra( "keepBto", cs );
			showExtra( "nativeLadder", cs );
			showExtra( "nativeOccluders", cs );
			showExtra( "merge", !cs );
			showExtra( "atlasFormat", !cs );
			if ( chosen ) {
				lodtCheck->setChecked( cs );
				heightmapCheck->setChecked( cs );
				/* NOT ticked by the FO4CS target any more (lane DEFAULTS1,
				 * 2026-09-12): bungo's 15:56 ruling is that the LEGACY files
				 * carry no FO4CS data even under that target -- the .lod*
				 * files above are where it lives. */
				identityCheck->setChecked( false );
				terrainIdCheck->setChecked( false );
				objectsCheck->setChecked( true );
				nativeCheck->setChecked( cs );
				btrCheck->setChecked( !cs );
			}
			/* The rows below the head grey against whichever head is OFFERED,
			 * so hiding one has to re-run them; and the chunk range follows the
			 * same answer. */
			if ( syncObjectRows )
				syncObjectRows();
			if ( syncRangeRows )
				syncRangeRows();
			refreshSummary();
		};
		connect( targetBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[applyTarget]( int ) { applyTarget( true ); } );
		applyTarget( false );

		connect( sourceBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int ) { applySource( true ); } );
		applySource( false );

		// the summary follows every setting it reads
		for ( QCheckBox * c : { lodtCheck, heightmapCheck, objectsCheck, nativeCheck, btrCheck, texCheck,
				coverCheck, vtCheck, vtBtrCheck, arraysCheck, identityCheck } )
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
		// the emitter is process-wide: a panel that goes away mid-run must not
		// leave it armed for whatever arms it next
		if ( lodgenNativeActive() )
			lodgenNativeEnd();
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
			if ( mo2Mode() || mo2DiskMode() )
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
	//! Mod Organizer 2 profile: the profile read off disk (lane LOADORDER1)
	bool mo2DiskMode() const { return sourceBox->currentData().toInt() == 2; }

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
		if ( mo2DiskMode() )
			return mo2DiskStack;
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
		const bool disk = mo2DiskMode();
		const bool mo2 = mo2Mode() || disk;
		resourceHost->setVisible( !mo2 );
		resourceLabel->setVisible( !mo2 );
		for ( QWidget * w : { static_cast<QWidget *>( mo2ProfileHost ), static_cast<QWidget *>( mo2ProfileLabel ),
				static_cast<QWidget *>( mo2ModsHost ), static_cast<QWidget *>( mo2ModsLabel ),
				static_cast<QWidget *>( mo2StackList ), static_cast<QWidget *>( mo2StackLabel ) } )
			w->setVisible( disk );
		if ( !disk ) {
			mo2DiskStack.clear();
			mo2DiskError.clear();
			mo2StackList->clear();
		}
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
		if ( disk ) {
			applyMo2Disk();
			return;
		}
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

	/*! Mod Organizer 2 profile: read the profile off disk and show what it
	 *  resolved -- every plugin as a full path in load order, the mod order as
	 *  the stack, and one line saying what was read. A refusal names its cause
	 *  on the status line and Generate carries the same words. */
	void applyMo2Disk()
	{
		mo2Plugins.clear();
		mo2DiskStack.clear();
		mo2DiskError.clear();
		mo2StackList->clear();
		const QString profile = mo2ProfileEdit->text().trimmed();
		const QString mods = mo2ModsEdit->text().trimmed();
		LodgenLoadOrder lo;
		QString err;
		bool ok = false;
		if ( profile.isEmpty() ) {
			err = tr( "choose a profile folder" );
		} else {
			/* The game's Data folder: the one Mod Organizer's own ini names for
			 * this instance, else the game folder Settings > Resources knows. */
			const QString instance = mods.isEmpty()
				? QDir::cleanPath( profile + QStringLiteral( "/../.." ) )
				: QDir::cleanPath( mods + QStringLiteral( "/.." ) );
			QString from;
			QString data = lodgenMo2GameData( instance, &from );
			if ( data.isEmpty() )
				data = gameDataDir();
			ok = lodgenLoadOrderFromMo2( profile, mods, data, &lo, &err );
		}
		pluginList->clear();
		if ( !ok ) {
			mo2DiskError = tr( "Mod Organizer 2 profile: %1" ).arg( err );
			sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) );
			sourceStatus->setText( mo2DiskError );
			scheduleWorldspaceRefresh();
			refreshSummary();
			return;
		}
		mo2DiskStack = lo.stack;
		int fromMods = 0;
		for ( int i = 0; i < lo.plugins.size(); i++ ) {
			pluginList->addItem( lo.plugins.at( i ) );
			pluginList->item( i )->setToolTip( lo.pluginFrom.value( i ) );
			if ( i >= lo.masters && lo.pluginFrom.value( i ) != QLatin1String( "data" ) )
				fromMods++;
		}
		for ( int i = 0; i < lo.stack.size(); i++ ) {
			const QString from = lo.stackFrom.value( i );
			QString text = from;
			if ( from == QLatin1String( "data" ) )
				text = tr( "game Data" );
			else if ( from.startsWith( QLatin1String( "mod " ) ) )
				text = from.mid( 4 );
			auto * it = new QListWidgetItem( text, mo2StackList );
			it->setToolTip( lo.stack.at( i ) );
		}
		sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		QString line = tr( "Mod Organizer 2 profile: %1 plugins (%2 from mod folders), %3 mods enabled, "
			"%4 disabled, files from %5" )
			.arg( lo.plugins.size() ).arg( fromMods ).arg( lo.modsEnabled ).arg( lo.modsDisabled ).arg( lo.dataDir );
		if ( !lo.modsMissing.isEmpty() )
			line += tr( "; %1 enabled mod folder(s) missing" ).arg( lo.modsMissing.size() );
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
	//! the `.bto` chunk files, offered under the stock engine only
	bool wantObjects() const { return objectsCheck->isChecked() && !objectsCheck->isHidden(); }
	//! the `.lodo`/`.lodi` pair, offered under FO4 Community Shaders only
	bool wantNative() const { return nativeCheck && nativeCheck->isChecked() && !nativeCheck->isHidden(); }
	/*! Whether the OBJECT PASS runs at all. Both heads drive the same walk of
	 *  the placements; they differ only in what is written at the end of it. */
	bool objectPassOn() const { return wantObjects() || wantNative(); }
	/*! WHERE THE OBJECT TEXTURE SETS GO (lane LAYOUT1, 2026-09-16).
	 *
	 *  The mesh arrays, the object atlas and the card arrays are the one group
	 *  the ruling table does not name, because they are written by the object
	 *  pass rather than by a terrain writer. Gate (a) settles it all the same:
	 *  NOTHING of ours lands outside `FO4CSLOD/` under the FO4CS target, and
	 *  the "stays where it is" clause is explicitly about the STOCK target's
	 *  `textures/terrain/<ws>/`. So under FO4CS they follow the rest into
	 *  `FO4CSLOD/<ws>/Objects/` and their game-relative strings say so; under
	 *  the stock engine not one byte moves. The command line composes the same
	 *  two strings from the same function (nifcli.cpp). */
	QString objectsDir() const
	{
		return wantNative()
			? lodgenFo4csWorldDir( outputDir(), world->worldspaceEdid() ) + QStringLiteral( "/Objects" )
			: texDir + QStringLiteral( "/Objects" );
	}
	/*! THE IMPOSTOR CARD FOLDER, and its default (lane LAYOUT1, 2026-09-16).
	 *
	 *  Cards are per TREE, not per worldspace, so bungo's one root gives them
	 *  one home: `<out>/FO4CSLOD/Cards`. Under the FO4CS target an EMPTY field
	 *  now MEANS that folder -- but only when it is there, so "empty = no
	 *  impostor cards" still means exactly that on a tree that has none, and
	 *  no bake starts compositing (or refusing) where it used to say nothing.
	 *  A typed folder always wins; the tooltip says so. */
	QString cardSourceDir() const
	{
		const QString typed = impostorEdit ? impostorEdit->text().trimmed() : QString();
		if ( !typed.isEmpty() || !fo4cs() || !outEdit )
			return typed;
		const QString dflt = lodgenFo4csCardDir( outputDir() );
		return ( !outputDir().isEmpty() && QDir( dflt ).exists() ) ? dflt : QString();
	}
	//! the game-relative string a consumer opens that set by
	QString objectsGame( const QString & stem ) const
	{
		const QString ws = world->worldspaceEdid();
		return wantNative()
			? QStringLiteral( "data\\" ) + lodgenFo4csGameWorldPath( ws )
				+ QChar( 92 ) + QStringLiteral( "Objects" ) + QChar( 92 ) + ws + QChar( '.' ) + stem
			: QString( "data\\Textures\\Terrain\\%1\\Objects\\%1.%2" ).arg( ws ).arg( stem );
	}
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
	/* ---- THE EXTRA ROWS: read, show, enable (lane PANEL1, 2026-09-12) -----
	 *
	 *  `xraw` is what the widget says. `xvar` is what the RUN reads, and the
	 *  difference is the panel's tick-AND-VISIBLE rule applied to a value: a
	 *  row the target has hidden reads back as the command line's default, so
	 *  a setting the target cannot use cannot reach the bake either. The save
	 *  uses `xraw`, so hiding a row never overwrites what is in it. */
	QVariant xraw( const QString & key ) const
	{
		const WwExtraRow r = extras.value( key );
		if ( !r.field )
			return r.dflt;
		if ( auto * d = qobject_cast<QDoubleSpinBox *>( r.field ) )
			return d->value();
		if ( auto * s = qobject_cast<QSpinBox *>( r.field ) )
			return s->value();
		if ( auto * c = qobject_cast<QCheckBox *>( r.field ) )
			return c->isChecked();
		if ( auto * b = qobject_cast<QComboBox *>( r.field ) )
			return b->currentData();
		if ( auto * e = qobject_cast<QLineEdit *>( r.field ) )
			return e->text();
		return r.dflt;
	}
	QVariant xvar( const QString & key ) const
	{
		const WwExtraRow r = extras.value( key );
		if ( !r.field || r.field->isHidden() )
			return r.dflt;
		return xraw( key );
	}
	float xf( const char * k ) const { return float( xvar( QLatin1String( k ) ).toDouble() ); }
	int xi( const char * k ) const { return xvar( QLatin1String( k ) ).toInt(); }
	bool xb( const char * k ) const { return xvar( QLatin1String( k ) ).toBool(); }
	QString xs( const char * k ) const { return xvar( QLatin1String( k ) ).toString(); }
	//! hide a row AND its label: hidden means the run reads the default
	void showExtra( const char * key, bool on )
	{
		const QString k = QLatin1String( key );
		if ( QWidget * w = extras.value( k ).field )
			w->setVisible( on );
		if ( QLabel * l = extraLabels.value( k ) )
			l->setVisible( on );
	}
	//! grey a row and its label: it still reads, it just cannot be typed into
	void enableExtra( const char * key, bool on )
	{
		const QString k = QLatin1String( key );
		if ( QWidget * w = extras.value( k ).field )
			w->setEnabled( on );
		if ( QLabel * l = extraLabels.value( k ) )
			l->setEnabled( on );
	}

	/*! THE PROCESS-WIDE GENERATOR SETTINGS, written from the rows at the top
	 *  of every run.
	 *
	 *  They are file statics inside lodgen.cpp reached only through these
	 *  setters, so they are written EVERY run and not only when a row moves:
	 *  otherwise the LAST run's value would still be standing. Every row is at
	 *  the command line's default until somebody moves it, so a default run
	 *  writes the defaults and no byte moves -- which is gate (a). */
	void applyGeneratorSettings()
	{
		lodgenSetThreadCount( xi( "threads" ) );
		lodgenSetChunkThreadCount( xi( "chunkThreads" ) );
		lodgenSetLandTiling( xf( "landTiling" ) );
		const int sample = xi( "landSample" );
		lodgenSetLandSampleAverage( sample == 1 );
		if ( sample == 2 ) {
			// `--land-sample stochastic`: the hex tiling, the four numbers it sets
			lodgenSetLandHexSize( 256.0f );
			lodgenSetLandWarpAmp( 0.0f );
			lodgenSetLandMipBias( -0.22f );
			lodgenSetLandWarpLattice( xf( "landWarpLattice" ) );
			lodgenSetLandWarpOctaves( xi( "landWarpOctaves" ) );
		} else if ( sample == 3 ) {
			// `--land-sample warp`: the older four, kept so the measurement can be repeated
			lodgenSetLandHexSize( 0.0f );
			lodgenSetLandWarpAmp( 683.0f );
			lodgenSetLandWarpLattice( 1024.0f );
			lodgenSetLandWarpOctaves( 1 );
			lodgenSetLandMipBias( -1.0f );
		} else {
			lodgenSetLandHexSize( xf( "landHex" ) );
			lodgenSetLandWarpAmp( xf( "landWarp" ) );
			lodgenSetLandWarpLattice( xf( "landWarpLattice" ) );
			lodgenSetLandWarpOctaves( xi( "landWarpOctaves" ) );
			lodgenSetLandMipBias( xf( "landMipBias" ) );
		}
		lodgenSetLandDetail( xf( "landDetail" ) );
		lodgenSetLandGuideRule( xi( "landGuide" ) );
		lodgenSetLandGuideStrength( xf( "landGuideStrength" ) );
		lodgenSetLandGuideScale( xf( "landGuideScale" ) );
		lodgenSetLandGuideSlopeRef( xf( "landGuideSlope" ) );
		lodgenSetLandDetailSource( xi( "landDetailSource" ) );
		lodgenSetVanillaLodRoot( xs( "vanillaLodRoot" ) );
		lodgenSetLandShade( xf( "landShade" ) );
		lodgenSetLandGrade( xf( "landGrade" ) );
		lodgenSetBlendEdges( xi( "blendEdges" ) );
		lodgenSetBlendMargin( xf( "blendMargin" ) );
		lodgenSetErosion( xf( "erosion" ) );
		lodgenSetErosionIterations( xi( "erosionIterations" ) );
		lodgenSetErosionSeed( quint32( xvar( QStringLiteral( "erosionSeed" ) ).toUInt() ) );
		lodgenSetSheetFormat( xi( "sheetFormat" ) );
		{
			// bungo 2026-09-24: an empty row means FIND his upscaled set in the
			// resources (the generator's "auto"); "none" turns the sheets off
			const QString mc = xs( "msnCache" ).trimmed();
			lodgenSetMsnCacheDir( mc.isEmpty() ? QStringLiteral( "auto" )
				: mc.compare( QStringLiteral( "none" ), Qt::CaseInsensitive ) == 0 ? QString() : mc );
		}
	}

	LodgenVtOptions vtOptions() const
	{
		LodgenVtOptions o;
		// the density row names a finest-level / content pair (--vt-density)
		const int density = vtFinestBox->currentData().toInt();
		o.finestDim = density == 8 ? 1 : 2;
		o.content = density == 32 ? 256 : 512;
		o.halfAux = xb( "vtHalfAux" );
		o.border = xi( "vtBorder" );
		o.mips = xi( "vtMips" );
		o.compression = xi( "vtCompress" );
		o.height = xb( "vtHeight" );
		o.vanillaFill = xb( "vtFillVanilla" );
		o.coverInColor = xb( "vtCoverInColor" );
		o.cover = coverOptions();
		return o;
	}
	LodgenCoverOptions coverOptions() const
	{
		LodgenCoverOptions o;
		o.cover = coverCheck->isChecked() && !coverCheck->isHidden()
			&& texCheck->isChecked() && btrCheck->isChecked();
		o.tintStrength = float( tintSpin->value() ) / 100.0f;
		o.coverFull = xf( "coverFull" );
		o.roads = xb( "roads" );
		o.roadCoverSuppress = xf( "roadCoverSuppress" );
		o.roadComposite = xi( "roadComposite" ) == 1
			? LodgenCoverOptions::RoadBlend : LodgenCoverOptions::RoadMaxZ;
		o.roadDetail = xf( "roadDetail" );
		o.roadGroundPaint = xf( "roadGroundPaint" );
		o.roadRaised = xb( "roadRaised" );
		o.roadSidewalks = xb( "roadSidewalks" );
		o.terrainObjectAo = xb( "terrainObjectAo" );
		o.terrainObjectAoStrength = xf( "terrainObjectAoStrength" );
		// roadOpacity is left at its own default: bungo, 2026-09-12 16:0x,
		// "we don't use that opacity at all, we render roads at their full
		// diffuse", so there is no row for it and the panel never moves it.
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
		/* The root line, kept current with the worldspace box and set BEFORE
		 * the refusal chain can return: it is a fact about the layout, not
		 * about whether this run can start. */
		if ( outRootLabel )
			outRootLabel->setText( ws.isEmpty()
				? lodgenFo4csFolderName() + QChar( 92 ) + tr( "<worldspace>" ) + QChar( 92 )
				: lodgenFo4csGameWorldPath( ws ) + QChar( 92 ) );
		/* The pyramid's line is computed BEFORE the refusal chain can return.
		 * It describes what the pyramid would cost, which is a fact about the
		 * settings and not about whether the run can start; hiding it behind
		 * "choose an output folder" would make the one live number in the
		 * section look like a fixed sentence. */
		refreshVtSummary();
		const bool chunks = objectPassOn() || btrCheck->isChecked();
		QString why;
		/* The source comes first: with MO2 chosen and no MO2 around it, every
		 * other reason is beside the point - nothing would be read. */
		if ( mo2Mode() && !lodgenUnderMo2() )
			why = mo2Refusal();
		else if ( mo2DiskMode() && !mo2DiskError.isEmpty() )
			why = mo2DiskError;
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
		else if ( ( objectPassOn() || ( btrCheck->isChecked() && texCheck->isChecked() ) )
			&& !Game::GameManager::status( Game::FALLOUT_4 ) )
			why = tr( "Fallout 4 is not enabled under Settings > Resources; meshes and textures are read from its archives." );
		else if ( objectPassOn() ) {
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
				parts << tr( "refresh the AO plane of %1\\%2.lodl" )
					.arg( lodgenFo4csGameWorldPath( ws ) ).arg( ws );
			else
				parts << tr( "%1\\%2.lodl (about %3 MB)" )
					.arg( lodgenFo4csGameWorldPath( ws ) ).arg( ws )
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
				parts << tr( "%1\\%2.VT.*.lodt (%3 levels, %4 tiles, about %5 GB)" )
					.arg( lodgenFo4csGameWorldPath( ws ) ).arg( ws )
					.arg( e.levels ).arg( e.tiles )
					.arg( double( e.pyramidBytes ) / 1073741824.0, 0, 'f', 2 );
		}
		/* THE NATIVE PAIR, named by extension like everything else. It is the
		 * FO4CS object output and it sits beside the landscape file. */
		if ( wantNative() )
			parts << tr( "%1\\%2.lodo and %1\\%2.lodi" )
				.arg( lodgenFo4csGameWorldPath( ws ) ).arg( ws );
		if ( chunks ) {
			QStringList kinds;
			if ( wantObjects() )
				kinds << tr( "object .bto" );
			if ( btrCheck->isChecked() && !btrSection->isHidden() )
				kinds << tr( "terrain .btr" );
			if ( !kinds.isEmpty() ) {
				parts << tr( "%1 chunks (%2) under meshes\\terrain\\%3" ).arg( buildQueue().size() )
					.arg( kinds.join( tr( " and " ) ) ).arg( ws );
			} else if ( xb( "keepBto" ) ) {
				/* The way back, ticked. The chunks land in the mod folder as
				 * every bake before 2026-09-16 left them. */
				parts << tr( "%1 object chunks under meshes\\terrain\\%2, kept because "
					"\"Keep legacy .BTO chunks\" is on" )
					.arg( buildQueue().size() ).arg( ws );
			} else {
				/* RULED 2026-09-16 (bungo, 2026-09-12 18:3x: "essentially, no
				 * legacy vanilla file types are now used by us or baked in the
				 * FO4CS lod bake"). The object pass still walks the chunk queue
				 * and still builds a `.bto` per chunk, because five passes read
				 * them back -- but it builds them in a scratch folder and drops
				 * them, so the mod folder gets our types and nothing else. The
				 * manifest sidecar stays. Said out loud, because a folder that
				 * loses files at the end of a run should not be a surprise. */
				/* WHERE THE SIDECAR LANDS (lane LAYOUT1, 2026-09-16): under the
				 * one root with the files it describes, so the sentence asks
				 * the composer rather than naming the folder it used to be
				 * in.  A summary that names a path nothing writes is the
				 * census rule broken in the one place bungo reads. */
				parts << tr( "%1 object chunks, built in a scratch folder and dropped "
					"(the manifest sidecars stay under %2)" )
					.arg( buildQueue().size() ).arg( lodgenFo4csGameWorldPath( ws ) );
			}
		}
		/* NOT on wantIdentity() since 2026-09-12 (lane DEFAULTS1): the arrays
		 * are their own module and the identity flag is not their master
		 * switch -- gating them on it emptied the far rings. */
		if ( objectPassOn() && arraysCheck->isChecked() )
			parts << tr( "the object texture arrays and their .lodm" );
		summary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		// the folder's own name stands for the mod; the whole path is the tooltip
		const QString mod = QFileInfo( outputDir() ).fileName();
		summary->setToolTip( outputDir() );
		summary->setText( tr( "Will write to %1%2: " ).arg( mod.isEmpty() ? outputDir() : mod )
			.arg( QDir( outputDir() ).exists() ? QString() : tr( " (a new folder)" ) )
			+ parts.join( QLatin1String( "; " ) ) + QChar( '.' ) );
	}


	/*! A SETTINGS SAVE THE SELF-TEST CAN ASK FOR (lane PANEL1, 2026-09-12).
	 *
	 *  WW_LODGEN_TEST checks that every row round-trips through its own
	 *  QSettings key, which means it has to make the panel save between moving
	 *  a row and reading the key. This class has no Q_OBJECT and so no slot to
	 *  call; a dynamic property does the same job with no moc: setting
	 *  `wwSaveSettings` on the panel raises DynamicPropertyChange here and the
	 *  rows are written. It changes nothing about a normal run. */
	bool event( QEvent * e ) override
	{
		if ( e->type() == QEvent::DynamicPropertyChange
			&& static_cast<QDynamicPropertyChangeEvent *>( e )->propertyName() == "wwSaveSettings" )
			saveSettings();
		return QWidget::event( e );
	}
	void saveSettings()
	{
		QSettings s;
		// in MO2 mode the plugin list is MO2's, not yours: it is not saved over
		// the list you typed, so switching back brings yours home
		if ( sourceBox->currentData().toInt() == 0 ) {
			QStringList plugins;
			for ( int i = 0; i < pluginList->count(); i++ )
				plugins << pluginList->item( i )->text();
			s.setValue( QStringLiteral( "LodGeneration/plugins" ), plugins );
		}
		s.setValue( QStringLiteral( "LodGeneration/source" ), sourceBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/mo2Profile" ), mo2ProfileEdit->text().trimmed() );
		s.setValue( QStringLiteral( "LodGeneration/mo2Mods" ), mo2ModsEdit->text().trimmed() );
		s.setValue( QStringLiteral( "LodGeneration/resources" ), resourceRows() );
		s.setValue( QStringLiteral( "LodGeneration/output" ), outputDir() );
		s.setValue( QStringLiteral( "LodGeneration/aoSamples" ), aoSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/overviewSamples" ), ovSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/objects" ), objectsCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/native" ), nativeCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/treesOnly" ), treesOnlyCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/btr" ), btrCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/cover" ), coverCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/grassTint" ), tintSpin->value() );
		s.setValue( QStringLiteral( "LodGeneration/vt" ), vtCheck->isChecked() );
		s.setValue( QStringLiteral( "LodGeneration/vtDensity" ), vtFinestBox->currentData().toInt() );
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
		/* Every row lane PANEL1 added, from the one registry they were built
		 * from -- xraw, not xvar, so a row the target has hidden keeps what is
		 * in it instead of being written back as the default. */
		for ( auto it = extras.constBegin(); it != extras.constEnd(); ++it )
			s.setValue( QStringLiteral( "LodGeneration/" ) + it.key(), xraw( it.key() ) );
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
		if ( mo2DiskMode() && !mo2DiskError.isEmpty() ) {
			progress->setFormat( mo2DiskError );
			return;
		}
		saveSettings();
		// the process-wide generator settings, from their rows, before any
		// stage reads one of them (lane PANEL1)
		applyGeneratorSettings();
		// the four stage times start at zero for every run, so a stage that
		// does not run this time reads 0 and not the last run's number
		msLandscape = msMeshes = msTextures = msImpostors = 0;
		msLandscapeWorker.store( 0 );
		/* The layout clause starts blank for every run and is filled by the
		 * writers themselves (lane LAYOUT1, 2026-09-16). It is cleared HERE and
		 * not in startChunks(): the landscape file is written by the world job,
		 * which runs first, and clearing later would throw its note away. */
		lodgenClearLayoutCensus();
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
		/* THE LANDSCAPE STAGE, timed on this thread and handed over as an
		 * atomic: the plugin load belongs to it too, because a .lodl run pays
		 * for it and a chunk-only run does not. */
		QElapsedTimer landscapeTimer;
		landscapeTimer.start();
		QString report;
		if ( job.lodt ) {
			if ( job.aoOnly ) {
				// the landscape file moved to FO4CSLOD\<ws>\ (lane LAYOUT1, 2026-09-16)
				const QString path = lodgenFo4csWorldDir( job.outDir, w.worldspaceEdid() )
					+ QChar( '/' ) + w.worldspaceEdid() + QStringLiteral( ".lodl" );
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
				/* The water-body module of the .lodl writer, off by default:
				 * unarmed, the file is the one this bake wrote before the
				 * module existed. */
				o.water.enabled = xb( "waterBodies" );
				o.water.bridgeGap = xi( "waterBridge" );
				o.water.nearTexels = xi( "waterNear" );
				o.water.bodySamples = xi( "waterBodySamples" );
				o.water.flowSamples = xi( "waterFlowSamples" );
				o.water.shore = xb( "waterShore" );
				o.water.velocityPlugin = xs( "waterVelocities" );
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
		msLandscapeWorker.store( landscapeTimer.elapsed() );
		post( [this, report]() { finishWorld( true, report ); } );
	}

	//! GUI thread: the world job ended; hand over to the chunk loop or stop
	void finishWorld( bool ok, const QString & report )
	{
		if ( worker.joinable() )
			worker.join();
		msLandscape = msLandscapeWorker.load();		// joined: the worker is done writing it
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
		if ( !( objectPassOn() || btrCheck->isChecked() || wantVt() ) ) {
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
		/* EMPTY-FOLDER HYGIENE (lane LAYOUT1, 2026-09-16): the legacy chunk
		 * folder is created only when a legacy chunk is going to be written
		 * into it. Under the FO4CS target nothing does. */
		if ( wantObjects() || ( btrCheck->isChecked() && !btrSection->isHidden() ) || xb( "keepBto" ) )
			QDir().mkpath( meshDir );
		if ( texCheck->isChecked() && btrCheck->isChecked() )
			QDir().mkpath( texDir );
		/* ===== REBAKE ONLY WHAT CHANGED (lane INCRGATE1, 2026-09-24) =======
		 *
		 * The command line's ledger, verbatim: lodgenIncrementalBegin() in
		 * src/lodgenchunkpass.cpp diffs this run against the record and
		 * filters the job list, and the queue below is rebuilt from what it
		 * kept, in job order. Two differences, both because a row is a
		 * standing setting and a flag is a request:
		 *   - no record yet is not a refusal: the whole range bakes and the
		 *     record is written, so the next run can diff;
		 *   - a run the record cannot vouch for bakes whole and says why,
		 *     instead of refusing -- a refusing row would have no way back.
		 * The switches are "--panel" and the IDENTITY WORD, which hashes every
		 * setting the bake reads: the pass's own options, the post passes, and
		 * every extra row the RUN reads (xvar, so a hidden row counts as its
		 * default). Threads, folders and this row itself are not settings of
		 * the bytes and stay out. */
		incOn = false;
		incCensus.clear();
		incNotes.clear();
		incRun = LodgenIncrementalRun();
		if ( xb( "incremental" ) && !queue.isEmpty() ) {
			bool oneDim = true;
			for ( const ChunkJob & j : queue )
				oneDim = oneDim && j.dim == queue.first().dim;
			if ( !oneDim ) {
				incCensus = tr( "rebake only what changed: off for this run, it works on one chunk size" );
			} else {
				incOn = true;
				lodbClearCensus();
				LodgenChunkPassOptions idPass = chunkPassOptions();
				const bool vtTex = wantVt() && vtBtrCheck->isChecked() && !vtBtrCheck->isHidden()
					&& btrCheck->isChecked() && texCheck->isChecked();
				LodgenIdentityExtras idx;
				idx.atlas = atlasCheck->isChecked() && !atlasCheck->isHidden();
				idx.arrays = arraysCheck->isChecked();
				idx.merge = xb( "merge" );
				const int atlasFmt = xi( "atlasFormat" );
				idx.atlasBc1 = atlasFmt < 0 ? !fo4cs() : ( atlasFmt == 1 );
				idx.keepBto = xb( "keepBto" );
				idx.texFromVt = vtTex;
				idx.simplify.enabled = simplifyCheck->isChecked();
				idx.simplify.ratio8 = float( simplify8Spin->value() );
				idx.simplify.ratio16 = float( simplify16Spin->value() );
				idx.simplify.ratio32 = float( simplify32Spin->value() );
				idx.simplify.errorWorld = float( simplifyErrorSpin->value() );
				if ( wantVt() )
					idx.vt = vtOptions();
				idx.vtBtr = vtTex ? 1 : 0;
				idx.nativeLadder = xb( "nativeLadder" );
				idx.nativeOccluders = xb( "nativeOccluders" );
				for ( auto it = extras.constBegin(); it != extras.constEnd(); ++it ) {
					const QString & k = it.key();
					if ( k == QLatin1String( "threads" ) || k == QLatin1String( "chunkThreads" )
						|| k == QLatin1String( "incremental" ) || qobject_cast<QLineEdit *>( it.value().field ) )
						continue;
					idx.more << QStringLiteral( "panel." ) + k + QChar( '=' ) + xvar( k ).toString();
				}
				auto yn = []( bool v ) { return v ? QStringLiteral( "1" ) : QStringLiteral( "0" ); };
				idx.more << QStringLiteral( "panel.target=" ) + yn( fo4cs() )
					<< QStringLiteral( "panel.objects=" ) + yn( wantObjects() )
					<< QStringLiteral( "panel.native=" ) + yn( wantNative() )
					<< QStringLiteral( "panel.btr=" ) + yn( btrCheck->isChecked() )
					<< QStringLiteral( "panel.tex=" ) + yn( btrCheck->isChecked() && texCheck->isChecked() && !vtTex )
					<< QStringLiteral( "panel.cards=" ) + yn( !cardSourceDir().isEmpty() );
				const QString word = lodgenIdentityWord( lodgenIdentityDump( idPass, idx ) );

				incRun.fromDir = outputDir();
				incRun.requireRecord = false;
				incRun.outDir = outputDir();
				incRun.nativeDir = wantNative() ? outputDir() : QString();
				incRun.digestRoot.clear();		// the game's own folders and archives, as the pass reads
				incRun.worldspace = wsBox->currentData().toUInt();
				incRun.dim = queue.first().dim;
				incRun.region[0] = x0Spin->value();
				incRun.region[1] = y0Spin->value();
				incRun.region[2] = x1Spin->value();
				incRun.region[3] = y1Spin->value();
				incRun.switches = lodgenSwitchesWithIdentity(
					lodgenSwitchDigestOf( { QStringLiteral( "--panel" ) } ), word );
				incRun.regionProducts = objectPassOn()
					&& ( arraysCheck->isChecked() || idx.atlas || !cardSourceDir().isEmpty() );
				incRun.nativeCache = true;
				incRun.warn = [this]( const QString & w ) { incNotes << w; };
				QVector<LodgenChunkJob> jobs;
				for ( const ChunkJob & j : queue )
					jobs.append( LodgenChunkJob{ j.dim, j.cx, j.cy } );
				QStringList reasons;
				QString detail;
				const LodgenIncrementalVerdict v =
					lodgenIncrementalBegin( incRun, *world, jobs, &incCensus, &reasons, &detail );
				if ( v != LodgenIncrementalVerdict::Go ) {
					QString why;
					switch ( v ) {
					case LodgenIncrementalVerdict::Shape:
						why = QStringLiteral( "the record covers another worldspace, chunk size or range" );
						break;
					case LodgenIncrementalVerdict::Switches:
						why = QStringLiteral( "a setting moved since the record (or a default of the exe did)" );
						break;
					case LodgenIncrementalVerdict::RegionProducts:
						why = QStringLiteral( "texture arrays, the object atlas and card arrays are built "
							"from the whole range" );
						break;
					default:
						why = QStringLiteral( "the record cannot vouch for this run" );
						break;
					}
					incNotes += lodgenIncrementalRefusal( v, incRun, detail );
					incRun.fromDir.clear();
					lodgenIncrementalBegin( incRun, *world, jobs, nullptr, nullptr, nullptr );
					incCensus = QString( "incremental: every chunk baked because %1; all %2 chunk(s) "
						"baked and the record rewritten" ).arg( why ).arg( jobs.size() );
				}
				incNotes += reasons;
				lodbNoteCensus( incCensus );
				QVector<ChunkJob> kept;
				for ( const LodgenChunkJob & j : jobs )
					kept.append( ChunkJob{ j.dim, j.cx, j.cy } );
				queue = kept;
				progress->setRange( 0, qMax( 1, queue.size() ) );
			}
		}
		/* ===== THE .BTO SCRATCH FOLDER (lane BTOFREE1, 2026-09-16) =========
		 *
		 * bungo, 2026-09-12 18:3x: "essentially, no legacy vanilla file types
		 * are now used by us or baked in the FO4CS lod bake". The `.BTO` was
		 * the last one, and it was last because five passes read it BACK --
		 * not because anything downstream of the bake wants it. So under the
		 * FO4CS target it is built here instead, every read-back works on it
		 * here, and the teardown after the card arrays removes it.
		 *
		 * The folder sits inside the mod folder rather than in %TEMP% so an
		 * interrupted bake leaves its scaffolding where the operator can see
		 * it; a run that finds one left by a dead bake removes it first, which
		 * makes that self-healing instead of a second failure. Same path, same
		 * name and the same teardown function as the command line, because
		 * lodgen_byte_gate.sh compares what the two leave on disk. */
		btoScratch.clear();
		lodgenClearBtoDisposition();
		if ( wantNative() && objectPassOn() && !xb( "keepBto" ) ) {
			btoScratch = outputDir() + QStringLiteral( "/lodgen_bto_scratch" );
			QDir( btoScratch ).removeRecursively();
			if ( !QDir().mkpath( btoScratch ) ) {
				finishAll( tr( "cannot create the .BTO scratch folder %1 \u2014 tick "
					"\"Keep legacy .BTO chunks\" to write the chunks into the mod "
					"folder instead" ).arg( btoScratch ) );
				return;
			}
		}
		/* THE NATIVE PAIR. Armed here and disarmed in step()'s tail, around the
		 * same chunk loop the CLI's region driver wraps (nifcli.cpp) -- the
		 * emitter is a process-wide accumulator, so the hook inside the chunk
		 * builder is a no-op until this call and the stock path is untouched
		 * when the row is off. The files land in FO4CSLOD\<ws>\ beside the
		 * .lodl (lane LAYOUT1, 2026-09-16: one root for every FO4CS-target
		 * output), because the output mod folder IS a Data folder. One
		 * function composes that root -- lodgenFo4csWorldDir() in
		 * src/lodgenlayout.cpp -- and every writer here calls it. The loader takes an
		 * EMPTY data root, which means the session's own resource stack -- the
		 * same assets the chunk pass and the viewport read. */
		nativeDataRoot.clear();
		if ( wantNative() ) {
			const QString nativeDir = lodgenFo4csWorldDir( outputDir(), world->worldspaceEdid() );
			QDir().mkpath( nativeDir );
			/* The two modules of the pair, from their rows (lane PANEL1): the
			 * ladder is the several-levels-per-mesh library and the occluders
			 * are the .lodi's box table. The ladder defaults OFF (bungo
			 * 2026-09-17, authored LODs only), the occluders ON. */
			lodgenNativeBegin( world.get(), nativeDir, lodgenNativeLoadModel, &nativeDataRoot,
				QString(), xb( "nativeLadder" ), xb( "nativeOccluders" ) );
			/* AGGREGATE RING-3 IMPOSTORS, armed the same way the command line
			 * arms them and refusing in the same words: a set is composited
			 * from the cell's own trees' card sheets, so there is nothing to
			 * photograph without a card tree. The refusal is carried to the
			 * result line rather than thrown away. */
			aggRefusal.clear();
			if ( xb( "aggregate" ) ) {
				const QString cardDir = cardSourceDir();
				if ( cardDir.isEmpty() ) {
					aggRefusal = tr( "aggregate impostors need the impostor card folder: a set is "
						"composited from the cell's own trees' cards" );
				} else {
					const int region[4] = { qMin( x0Spin->value(), x1Spin->value() ),
						qMin( y0Spin->value(), y1Spin->value() ),
						qMax( x0Spin->value(), x1Spin->value() ),
						qMax( y0Spin->value(), y1Spin->value() ) };
					QStringList notes;
					const QHash<quint32, LodgenAggCard> cards = lodgenAggregateCards( *world, region,
						cardDir, cardHalfAuxCheck->isChecked() ? 2 : 1, &notes );
					if ( cards.isEmpty() ) {
						aggRefusal = tr( "aggregate impostors found no usable card set in %1" ).arg( cardDir );
					} else {
						LodgenAggOptions ao;
						ao.minTrees = xi( "aggregateMin" );
						ao.tile = xi( "aggregateTile" );
						ao.views = xi( "aggregateViews" );
						ao.auxDiv = cardHalfAuxCheck->isChecked() ? 2 : 1;
						lodgenNativeSetAggregate( ao, cards );
					}
				}
			}
		}
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
			QElapsedTimer vtTimer;
			vtTimer.start();
			LodgenVtOptions vo = vtOptions();
			if ( vtBtrCheck->isChecked() && !vtBtrCheck->isHidden()
				&& btrCheck->isChecked() && texCheck->isChecked() ) {
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
			msTextures += vtTimer.elapsed();		// the pyramid is a TEXTURE stage
		}
		runChunkQueue();
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

	/*! The chunk pass's options, from the rows. One function, because the
	 *  identity word of "Rebake only what changed" is read off the same
	 *  options the pass bakes with (lane INCRGATE1, 2026-09-24). */
	LodgenChunkPassOptions chunkPassOptions()
	{
		LodgenChunkPassOptions pass;
		pass.plugins = pluginString();
		pass.worldspace = wsBox->currentData().toUInt();
		pass.worldEdid = world->worldspaceEdid();
		pass.wantBtr = btrCheck->isChecked();
		pass.wantBto = objectPassOn();
		pass.wantTex = btrCheck->isChecked() && texCheck->isChecked() && !vtSuppliesTex;
		pass.meshDir = meshDir;
		pass.btoScratchDir = btoScratch;
		pass.texDir = texDir;
		pass.texDataRoot.clear();          // the game's own folders and archives
		pass.cover = coverOptions();
		if ( previewCheck->isChecked() && skope )
			pass.previewDir = QDir::tempPath();

		LodgenTerrainOptions topts;
		topts.water = waterCheck->isChecked();
		topts.targetTrisPerCell = trisSpin->value();
		topts.terrainIdentity = wantTerrainId();
		topts.geomorph = geomorphCheck->isChecked();
		topts.shoreDenser = shoreCheck->isChecked();
		topts.shoreDensity = shoreDensitySpin->value();
		topts.waterSubdiv = xi( "waterSubdiv" );
		pass.terrain = topts;

		LodgenObjectOptions oopts;
		oopts.identity = wantIdentity();
		oopts.treeSway = oopts.identity && swayCheck->isChecked();
		oopts.objectChannels = oopts.identity && channelsCheck->isChecked();
		oopts.aoSkirtCells = aoSkirtSpin->value();
		oopts.cullBuried = cullCheck->isChecked();
		oopts.cullMargin = float( cullMarginSpin->value() );
		oopts.slotFallback = slotFallbackCheck->isChecked();
		oopts.bakeAO = aoCheck->isChecked();
		oopts.dataRoot.clear();            // the game's own folders and archives
		oopts.impostorDir = cardSourceDir();
		oopts.impostorFromLevel = impostorLevelBox->currentData().toInt();
		oopts.cardAuxDiv = cardHalfAuxCheck->isChecked() ? 2 : 1;
		oopts.treesOnly = treesOnlyCheck->isChecked();
		pass.object = oopts;
		return pass;
	}

	/*! THE CHUNK QUEUE, over the machine.
	 *
	 *  The panel used to build ONE chunk per event-loop tick, so a 3,060-chunk
	 *  Commonwealth ran on one core with fifteen idle. The loop is now
	 *  lodgenRunChunkPass (lodgenchunkpass.h), shared with the command line and
	 *  fanned over lodgenThreadCount() workers.
	 *
	 *  The window stays live because the pass calls `retire` on THIS thread,
	 *  once per chunk, in QUEUE ORDER, and `retire` pumps the event loop -- the
	 *  same thing the pyramid pass already does through vtProgressThunk.
	 *  Cancel still lands between chunks: the workers poll cancelFlag before
	 *  they pick a job up.
	 *
	 *  The live preview is spliced from `retire` too, so the documents arrive
	 *  in chunk order however the workers finish. No NifModel crosses a thread:
	 *  a worker writes the preview copy as a file, translation already applied,
	 *  and the main thread only opens it. */
	void runChunkQueue()
	{
		// an incremental run with nothing dirty still runs: every chunk replays its cache
		if ( queue.isEmpty() && !( incOn && incRun.incremental ) )
			return;
		QVector<LodgenChunkJob> jobs;
		jobs.reserve( queue.size() );
		for ( const ChunkJob & j : queue )
			jobs.append( LodgenChunkJob{ j.dim, j.cx, j.cy } );

		LodgenChunkPassOptions pass = chunkPassOptions();
		if ( incOn )
			lodgenIncrementalArmCache( incRun, world->worldspaceEdid(), pass );

		QString perr;
		lodgenRunChunkPass( jobs, pass,
			[this, &pass]( const LodgenChunkOutcome & r ) {
				if ( incOn )
					lodgenIncrementalNoteRetired( incRun, pass, r );
				progress->setFormat( tr( "chunk %1 at (%2,%3) — %v of %m" )
					.arg( r.dim ).arg( r.cx ).arg( r.cy ) );
				const bool okChunk = ( !btrCheck->isChecked() || r.btrBuilt )
					&& ( !objectPassOn() || r.btoBuilt );
				if ( r.btoSaved )
					writtenBto.append( r.btoPath );
				bool added = false;
				if ( skope && previewCheck->isChecked() ) {
					if ( !r.btrPreviewPath.isEmpty() )
						added = skope->addWorkspaceDocumentFromFile( r.btrPreviewPath ) || added;
					if ( !r.btoPreviewPath.isEmpty() )
						added = skope->addWorkspaceDocumentFromFile( r.btoPreviewPath ) || added;
					if ( added && skope->getGLView() && framePending )
						skope->getGLView()->frameAll();
				}
				map->markChunk( r.dim, r.cx, r.cy,
					okChunk ? LodgenProgressMap::Chunk : LodgenProgressMap::Failed );
				done++;
				progress->setValue( done );
				// the window must stay live: this is the only place that pumps
				QCoreApplication::processEvents();
			},
			[this]() { return cancelFlag.load(); },
			&msMeshes, &msTextures, &perr );
		if ( !perr.isEmpty() )
			lastReport = perr;
	}


	void step()
	{
		if ( cancelFlag || done >= queue.size() ) {
			QString tail;
			// not on wantIdentity() since 2026-09-12 (lane DEFAULTS1)
			if ( !cancelFlag && !writtenBto.isEmpty() && objectPassOn()
				&& arraysCheck->isChecked() ) {
				// before the atlas: the arrays key on the shapes' own diffuse paths
				const QString ws = world->worldspaceEdid();
				const QString arrDir = objectsDir();
				QDir().mkpath( arrDir );
				progress->setFormat( tr( "writing the texture arrays\u2026" ) );
				QCoreApplication::processEvents();
				QElapsedTimer t;
				t.start();
				QString rep, aerr;
				if ( lodgenBuildTextureArrays( writtenBto, QString(),
					arrDir + "/" + ws + QStringLiteral( ".LodgenArrays" ),
					objectsGame( QStringLiteral( "LodgenArrays" ) ), &rep, &aerr ) ) {
					tail += tr( ", arrays: %1" ).arg( rep );
					if ( wantNative() )
						lodgenNoteLayoutDir( arrDir );
				} else
					tail += tr( ", arrays: %1" ).arg( aerr );
				msTextures += t.elapsed();
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && atlasCheck->isChecked()
				&& !atlasCheck->isHidden() && objectPassOn() ) {
				const QString ws = world->worldspaceEdid();
				const QString atlasDir = objectsDir();
				QDir().mkpath( atlasDir );
				progress->setFormat( tr( "packing the object atlas\u2026" ) );
				QCoreApplication::processEvents();
				QElapsedTimer t;
				t.start();
				QString aerr;
				/* Vanilla's own sheet is DXT1 (measured), so the stock target gets
				 * BC1 with one-bit alpha for the cut-outs - parity and half the
				 * memory - and FO4CS keeps BC3's eight-bit alpha, which only a
				 * consumer that soft-blends card edges can spend. */
				/* The row picks it; "Match the target" is what the panel has
				 * always done and is its default (lane PANEL1). */
				const int atlasFmt = xi( "atlasFormat" );
				const bool atlasBc1 = atlasFmt < 0 ? !fo4cs() : ( atlasFmt == 1 );
				if ( lodgenBuildAtlas( writtenBto, QString(),
					atlasDir + "/" + ws + QStringLiteral( ".LodgenObjects" ),
					objectsGame( QStringLiteral( "LodgenObjects" ) ),
					outputDir(), atlasBc1, &aerr ) ) {
					tail = tr( ", atlas %1" ).arg( atlasBc1 ? tr( "written (BC1)" ) : tr( "written (BC3)" ) );
					if ( wantNative() )
						lodgenNoteLayoutDir( atlasDir );
				} else
					tail = tr( ", atlas: %1" ).arg( aerr );
				msTextures += t.elapsed();
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && objectPassOn() && xb( "merge" ) ) {
				// last: one shape per material the engine can tell apart
				progress->setFormat( tr( "merging the chunk shapes\u2026" ) );
				QCoreApplication::processEvents();
				QElapsedTimer t;
				t.start();
				QString rep, merr;
				if ( lodgenMergeChunkShapes( writtenBto, &rep, &merr ) )
					tail += tr( ", merged %1" ).arg( rep );
				else
					tail += tr( ", merge: %1" ).arg( merr );
				msMeshes += t.elapsed();
			}
			if ( !cancelFlag && !writtenBto.isEmpty() && objectPassOn()
				&& simplifyCheck->isChecked() ) {
				// the far rings, on the MERGED shapes: one proxy per cluster
				progress->setFormat( tr( "simplifying the far rings\u2026" ) );
				QCoreApplication::processEvents();
				QElapsedTimer t;
				t.start();
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
				msMeshes += t.elapsed();
			}
			// not on wantIdentity() since 2026-09-12 (lane DEFAULTS1)
			if ( !cancelFlag && !writtenBto.isEmpty() && objectPassOn()
				&& arraysCheck->isChecked() && !cardSourceDir().isEmpty() ) {
				// the card sets the chunks stand on, as arrays beside the mesh arrays
				const QString ws = world->worldspaceEdid();
				const QString arrDir = objectsDir();
				QDir().mkpath( arrDir );
				progress->setFormat( tr( "writing the card arrays\u2026" ) );
				QCoreApplication::processEvents();
				QElapsedTimer t;
				t.start();
				QString rep, cerr2;
				if ( lodgenBuildCardArrays( writtenBto, cardSourceDir(),
					arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ),
					objectsGame( QStringLiteral( "LodgenCards" ) ),
					cardHalfAuxCheck->isChecked() ? 2 : 1, &rep, &cerr2 ) ) {
					tail += tr( ", card arrays: %1" ).arg( rep );
					if ( wantNative() )
						lodgenNoteLayoutDir( arrDir );
					/* CARDLINK1: the emitter links the arrays just written, while the
					 * manifests still sit beside the chunks (the teardown below
					 * removes them before the pair is written). */
					if ( lodgenNativeActive() ) {
						QString lerr;
						if ( !lodgenNativeLinkCards( writtenBto,
							arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ), &lerr ) ) {
							tail += tr( ", native: not written, %1" ).arg( lerr );
							lodgenNativeEnd();
						}
					}
				} else
					tail += tr( ", card arrays: %1" ).arg( cerr2 );
				msImpostors += t.elapsed();		// the IMPOSTOR stage
			}
			/* ===== THE SCRATCH TEARDOWN (lane BTOFREE1, 2026-09-16) ========
			 *
			 * LAST of the object passes: the texture arrays, the atlas, the
			 * shape merge, the far-ring cut and the card arrays above have all
			 * had the chunks and their manifests side by side, exactly as they
			 * did when the chunks lived in the mod folder. Now the sidecars
			 * move into the mod folder and the chunks go. Before the pair,
			 * because the pair is written from the emitter and not from the
			 * files, and after a CANCEL as well -- a cancelled run must not
			 * leave a scratch folder behind either. */
			if ( !btoScratch.isEmpty() ) {
				/* The sidecars BTOFREE1 keeps land beside the files they
				 * describe, which since lane LAYOUT1 (2026-09-16) means
				 * FO4CSLOD\<ws>\ and no longer meshes\terrain\<ws>\. The
				 * scratch folder only ever exists under the FO4CS target. */
				const QString manifestDir = lodgenFo4csWorldDir( outputDir(), world->worldspaceEdid() );
				QDir().mkpath( manifestDir );
				const LodgenBtoScratchResult r =
					lodgenDropBtoScratch( writtenBto, btoScratch, manifestDir );
				tail += tr( ", %1 .bto chunk(s) dropped from the mod folder (%2 bytes)" )
					.arg( r.dropped ).arg( r.freed );
				for ( const QString & w : r.warnings )
					tail += tr( ", %1" ).arg( w );
				writtenBto.clear();
				btoScratch.clear();
			}
			/* The native pair, written once the chunk queue has handed the
			 * emitter every placement -- the same order the CLI's region
			 * driver uses. Disarmed on every path, including a cancel, so a
			 * cancelled run cannot leave the accumulator armed for the next. */
			/* REBAKE ONLY WHAT CHANGED: the pair is written from the rebuilt
			 * chunks AND the replayed caches, so INCR1's two cache refusals
			 * hold here as on the command line. A refused pair is not written
			 * and neither is the record, so the next run diffs against the old
			 * one and rebuilds what this run touched. */
			bool incPairRefused = false;
			if ( incOn && !cancelFlag && lodgenNativeActive() ) {
				const QString cc = lodgenIncrementalCacheCensus( incRun );
				if ( !cc.isEmpty() )
					lodbNoteCensus( cc );
				const QStringList refused = lodgenIncrementalCacheRefusal( incRun );
				if ( !refused.isEmpty() ) {
					incPairRefused = true;
					incNotes += refused;
					tail += tr( ", the native pair was NOT rewritten — untick \"Rebake only what "
						"changed\" for one whole bake" );
				} else {
					lodgenIncrementalOfferReuse( incRun );
				}
			}
			if ( lodgenNativeActive() ) {
				if ( !cancelFlag && !incPairRefused ) {
					progress->setFormat( tr( "writing the native object files…" ) );
					QCoreApplication::processEvents();
					QElapsedTimer t;
					t.start();
					QString nrep, nerr;
					if ( lodgenNativeWrite( &nrep, &nerr ) )
						tail += tr( ", %1" ).arg( nrep.section( QChar( '\n' ), 0, 0 ) );
					else
						tail += tr( ", native: %1" ).arg( nerr );
					msMeshes += t.elapsed();
					/* The aggregate SHEETS, written after the pair because the
					 * compositor runs inside the .lodi write. They go into the
					 * output Data tree, never the card bake tree: a set is per
					 * worldspace CELL and the card tree is per base. */
					if ( !lodgenNativeAggregateSets().isEmpty() ) {
						QStringList aggWritten;
						QString aggErr;
						bool aggOk = true;
						for ( const LodgenAggSet & a : lodgenNativeAggregateSets() )
							if ( !lodgenAggregateWrite( outputDir(), world->worldspaceEdid(),
									a, &aggWritten, &aggErr ) ) {
								aggOk = false;
								break;
							}
						tail += aggOk
							? tr( ", %1 aggregate set(s), %2 file(s)" )
								.arg( lodgenNativeAggregateSets().size() ).arg( aggWritten.size() )
							: tr( ", aggregate: %1" ).arg( aggErr );
					}
					if ( !aggRefusal.isEmpty() )
						tail += tr( ", aggregate: %1" ).arg( aggRefusal );
				}
				lodgenNativeEnd();
			}
			/* REBAKE ONLY WHAT CHANGED: the record, LAST, after every file it
			 * lists is on disk -- the command line's order. Not after a cancel
			 * and not after a refused pair: a record must only ever describe a
			 * finished bake. */
			if ( incOn ) {
				if ( !cancelFlag && !incPairRefused ) {
					lodbNoteCensus( stageTimeLine() );
					lodbNoteCensus( lodgenBakeCensusLine() );
					QStringList recWarn;
					QString recLine;
					const bool recOk = lodgenIncrementalWriteRecord( incRun, *world,
						{ QStringLiteral( "--panel" ) }, QStringList(), &recWarn, &recLine );
					incNotes += recWarn;
					if ( !recLine.isEmpty() )
						incNotes << recLine;
					tail += recOk ? QStringLiteral( ", " ) + incCensus + tr( ", bake record written" )
						: QStringLiteral( ", " ) + incCensus + tr( ", the bake record was NOT written" );
				}
				incOn = false;
			} else if ( !incCensus.isEmpty() ) {
				tail += QStringLiteral( ", " ) + incCensus;
			}
			incCensus.clear();
			world.reset();
			lodgenDestroyBakeCaches( bakeCaches );
			bakeCaches = nullptr;
			writtenBto.clear();
			/* the notes (refusals, warnings, the record's read-back) go under the
			 * first line, so they reach the result's tooltip and not the bar */
			const QString incTail = incNotes.isEmpty() ? QString()
				: QStringLiteral( "\n" ) + incNotes.join( QChar( '\n' ) );
			incNotes.clear();
			finishAll( ( cancelFlag ? tr( "cancelled after %1 chunk(s)" ).arg( done )
				: tr( "done \u2014 %1 chunk(s)%2" ).arg( done ).arg( tail ) ) + incTail );
			return;
		}
	}

	/*! The four stage times, in one line, in the order the work happens.
	 *
	 *  Each is accumulated from the calls that belong to that stage and from
	 *  nothing else, so a stage that did not run reads exactly 0.0 s -- which
	 *  is what makes the line a measurement rather than a decoration (the
	 *  three rules of 2026-09-04: a status line ships with a test that it is
	 *  written AND that it moves). Seconds to one decimal: the numbers a
	 *  person compares are tens of seconds and minutes, and milliseconds in a
	 *  pinned bar read as noise. */
	QString stageTimeLine() const
	{
		// the words live in lodgen.cpp, so the panel and the command line
		// cannot drift apart on them
		return lodgenStageTimeLine( msLandscape, msMeshes, msTextures, msImpostors,
			lodgenNativeLibrarySplit() );
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
		if ( resultLabel ) {
			/* The four stage times, then the bake census -- how many threads the
			 * run was allowed, how many chunk jobs it had, how many workers the
			 * queue could actually use, and what it cost the machine. One
			 * formatter with the command line (lodgenBakeCensusLine). */
			resultLabel->setText( stageTimeLine() + QStringLiteral( "\n" ) + lodgenBakeCensusLine() );
			resultLabel->setToolTip( message );
			resultLabel->setVisible( true );
		}
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
	//! Mod Organizer 2 profile (off disk): its rows, its resolved stack, its refusal
	QLineEdit * mo2ProfileEdit = nullptr, * mo2ModsEdit = nullptr;
	QWidget * mo2ProfileHost = nullptr, * mo2ModsHost = nullptr;
	QLabel * mo2ProfileLabel = nullptr, * mo2ModsLabel = nullptr, * mo2StackLabel = nullptr;
	QListWidget * mo2StackList = nullptr;
	QStringList mo2DiskStack;
	QString mo2DiskError;
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
		const QString dir = cardSourceDir();
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
	QCheckBox * nativeCheck = nullptr;			//!< the FO4CS head of the object section
	QCheckBox * treesOnlyCheck = nullptr;		//!< impostor cards are trees only
	std::function<void()> syncObjectRows, syncRangeRows;
	/*! The four stage times of the last run, in milliseconds, in the order the
	 *  result line prints them. Each is accumulated from the calls that belong
	 *  to that stage and NOTHING else, so a stage that did not run reads 0 --
	 *  which is what makes the line testable (the three rules of 2026-09-04:
	 *  written AND moving). */
	qint64 msLandscape = 0, msMeshes = 0, msTextures = 0, msImpostors = 0;
	//! atomics: the landscape stage is accumulated on the worker thread
	std::atomic<qint64> msLandscapeWorker { 0 };
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
	/*! ONE REGISTRY FOR THE ROWS LANE PANEL1 ADDED (2026-09-12).
	 *
	 *  Key = the `LodGeneration/` settings key without its group. `dflt` is the
	 *  COMMAND LINE'S default for that switch, kept beside the widget so a
	 *  hidden row reads back as the default and so the self-test can check
	 *  every row round-trips. The save, the load, the run and the self-test
	 *  all walk this one hash. */
	struct WwExtraRow
	{
		QWidget * field = nullptr;
		QVariant dflt;
	};
	QHash<QString, WwExtraRow> extras;
	QHash<QString, QLabel *> extraLabels;
	QCheckBox * roadsCheck = nullptr, * terrainObjAoCheck = nullptr,
		* waterBodiesCheck = nullptr, * aggregateCheck = nullptr;
	LodgenSection * roadsSection = nullptr, * terrainObjAoSection = nullptr,
		* waterBodiesSection = nullptr, * aggregateSection = nullptr;
	QString aggRefusal;			//!< why the aggregate module did not arm, in words
	bool vtSuppliesTex = false;
	QList<QWidget *> objectsSub, btrSub;
	QWidget * rangeBox;
	QSplitter * splitter = nullptr;
	QScrollArea * scroll = nullptr;
	QComboBox * targetBox = nullptr;
	LodgenSection * lodtSection = nullptr, * heightmapSection = nullptr, * objectsSection = nullptr, * btrSection = nullptr;
	LodgenSection * vtSection = nullptr;
	QLabel * summary = nullptr;
	//! the FO4CS root line under the output field, and its name cell (lane LAYOUT1)
	QLabel * outRootLabel = nullptr, * outRootName = nullptr;
	QLabel * resultLabel = nullptr;		//!< the four stage times of the last run
	QString nativeDataRoot;				//!< empty = the session's own resource stack
	bool haveBounds = false;
	int bMinX = 0, bMinY = 0, bMaxX = -1, bMaxY = -1;
	QPushButton * wholeButton, * startButton, * cancelButton;
	QProgressBar * progress;
	LodgenProgressMap * map;
	std::unique_ptr<EsmWorld> world;
	LodgenBakeCaches * bakeCaches = nullptr;
	QVector<ChunkJob> queue;
	QStringList writtenBto;
	QString meshDir, texDir, btoScratch, lastReport;
	/*! "Rebake only what changed" (lane INCRGATE1, 2026-09-24): the ledger
	 *  run the command line's `--incremental` uses, armed once per bake in
	 *  startChunks() and closed in step(). `incOn` is false for every bake
	 *  with the row off, and then nothing below touches the run. */
	LodgenIncrementalRun incRun;
	bool incOn = false;
	QString incCensus;			//!< the one census line: what was dirty, or why all of it was
	QStringList incNotes;		//!< refusal words, warnings, the record's read-back: the tooltip
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

	/* THE PANEL'S OWN SEGMENTED STRIP: LOD | Water (lane WATER8, 2026-09-10).
	 *
	 * bungo, on seeing lane WATER7 put the Water tab in the LEFT strip:
	 * *"What? I wanted it in that right panel though"*. Read with what he had
	 * said an hour earlier -- *"They should be in the LOD gen workspace"*, then,
	 * over a screenshot of Header | Blocks | Files, *"You'd access them like
	 * this"* -- the strip was the STYLE and THIS panel is the PLACE. WATER7
	 * took it for the place; the left strip goes back to its three tabs.
	 *
	 * So this dock stops holding the settings page directly and holds a strip
	 * above a stack instead. The strip is built exactly as the left editor's is
	 * (src/nifskope_ui.cpp:24402-24429): a QTabBar in document mode with no
	 * base, expanding, no scroll buttons, wwSegmentedTabBarQss(), tabData
	 * carrying the STACK PAGE INDEX in both directions, over a QStackedWidget
	 * in a zero-margin column. Same helper, same skin, so the two strips cannot
	 * drift apart -- gate L5 of tests/spells/water_ui.sh compares the two
	 * sheets byte for byte.
	 *
	 * The tab index means nothing outside this widget: everything asks through
	 * tabData, which is what makes adding the water page (src/watermarkpanel.cpp,
	 * which finds this strip by object name) a one-call change rather than a
	 * renumbering.
	 *
	 * The row height is NOT set here. `wwAlignBarRow` states it once, after
	 * restoreState, for every bar at the top of the window at once
	 * (src/nifskope_ui.cpp, restoreUi) -- three setFixedHeight calls at three
	 * call sites is exactly how the bars came to disagree in the first place.
	 * Until then this strip carries the compact default, which is also what it
	 * keeps if the row never gets a height.
	 *
	 * FALLBACK FLOOR (CONSTITUTION 10): with no second page ever added the
	 * strip is a single `LOD` tab over the settings the dock always held, so
	 * the workspace works exactly as it did before this lane. */
	auto * host = new QWidget( dock );
	host->setObjectName( QStringLiteral( "LodPanelHost" ) );
	auto * hostLayout = new QVBoxLayout( host );
	hostLayout->setContentsMargins( 0, 0, 0, 0 );
	hostLayout->setSpacing( 0 );

	auto * tabs = new QTabBar( host );
	tabs->setObjectName( QStringLiteral( "LodPanelModeSelector" ) );
	tabs->setDocumentMode( true );
	tabs->setDrawBase( false );
	tabs->setExpanding( true );
	tabs->setUsesScrollButtons( false );
	tabs->setStyleSheet( wwSegmentedTabBarQss() );
	tabs->setAccessibleName( QObject::tr( "LOD Generation panel mode" ) );

	auto * stack = new QStackedWidget( host );
	stack->setObjectName( QStringLiteral( "LodPanelStack" ) );
	const int lodPage = stack->addWidget( panel );
	const int lodTab = tabs->addTab( QObject::tr( "LOD" ) );
	tabs->setTabData( lodTab, lodPage );
	tabs->setTabToolTip( lodTab, QObject::tr( "Generate terrain, object and impostor LOD" ) );

	QObject::connect( tabs, &QTabBar::currentChanged, stack, [tabs, stack]( int index ) {
		if ( index < 0 )
			return;
		const int page = tabs->tabData( index ).toInt();
		if ( page >= 0 && page < stack->count() )
			stack->setCurrentIndex( page );
	} );

	hostLayout->addWidget( tabs, 0 );
	hostLayout->addWidget( stack, 1 );
	dock->setWidget( host );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
