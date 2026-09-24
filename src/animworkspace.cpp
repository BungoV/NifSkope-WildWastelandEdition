/* THE ANIMATION WORKSPACE -- lane HKXEDIT2, 2026-09-10. See animworkspace.h
   for the shape; nifskope-ww-panel-style for every control's rule. */

#include "animworkspace.h"
#include "hkxanimui.h"
#include "hkxplayback.h"
#include "hkxwrite.h"
#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include "glview.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "model/undocommands.h"
#ifdef WW_ANIMWS_HKXMODEL
#include "hkxmodel.h"
#endif

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QShortcut>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>
#include <QVBoxLayout>

#include <cmath>


/*
 *  The one undo group, and the one command
 */

QUndoGroup * wwAnimUndoGroup()
{
	static QUndoGroup * g = new QUndoGroup( qApp );
	return g;
}

namespace
{

//! One edit = one command holding the document before and after. A snapshot
//! is exact, which is what gate (h) asks of undo; 8,835 transforms is 400 KB.
class AnimWsCommand final : public QUndoCommand
{
public:
	AnimWsCommand( AnimWorkspace * w, const QString & entry, const HkxClipDocument & before,
				   const HkxClipDocument & after, const QString & what, const QString & message )
		: QUndoCommand( what ), ws( w ), name( entry ), pre( before ), post( after ), msg( message ) {}
	void redo() override { ws->applyDocument( name, post, msg ); }
	void undo() override { ws->applyDocument( name, pre, QObject::tr( "Undid: %1" ).arg( text() ) ); }

private:
	AnimWorkspace * ws;
	QString name;
	HkxClipDocument pre, post;
	QString msg;
};

//! The list's two glyphs: a diamond for a loaded clip, a triangle for a NIF sequence.
QIcon wwAnimGlyph( bool clip, bool refused )
{
	QPixmap pm( 16, 16 );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing );
	const QColor c( wwSkinColor( refused ? "danger" : ( clip ? "accent" : "text" ) ) );
	QPainterPath path;
	if ( clip ) {
		path.moveTo( 8, 2 );
		path.lineTo( 14, 8 );
		path.lineTo( 8, 14 );
		path.lineTo( 2, 8 );
	} else {
		path.moveTo( 4, 2 );
		path.lineTo( 14, 8 );
		path.lineTo( 4, 14 );
	}
	path.closeSubpath();
	p.fillPath( path, c );
	return QIcon( pm );
}

/* ---- RULING 4 (bungo, 2026-09-12 01:45): "Most of these icons are very bad
   looking, and unclear to what they do. They need to be better."

   Blender's timeline transport, drawn as vector paths on ONE 16x16 grid, at
   ONE size and ONE weight (2 units of stroke, the same triangle everywhere),
   inked from the wwskin palette so they follow the skin like everything else.

   The divergence from the ruling's own words, said plainly: it asks for "our
   own SVG". These are the same vector shapes, drawn with QPainterPath instead
   of parsed out of an .svg file. NifSkope does not link Qt's Svg module and
   neither Qt6Svg.dll nor the qsvg image plugin is deployed beside the exe, so
   real .svg files would need a deploy change this lane cannot test. The shapes,
   the one weight, the one size and the palette are what the ruling is about;
   the file format is not. Bungo's call to overturn. */
const int WW_TRANSPORT_ICON_PX = 16;

enum WwTransportGlyph {
	GlyphToStart, GlyphPrevKey, GlyphPlayBack, GlyphPlay, GlyphPause,
	GlyphStop, GlyphNextKey, GlyphToEnd, GlyphLoop, GlyphRecord, GlyphPose
};

//! One glyph, inked in `ink`, on a 16x16 grid scaled to `px` logical pixels.
QPixmap wwTransportPixmap( int glyph, int px, const QColor & ink )
{
	const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
	QPixmap pm( int( px * dpr ), int( px * dpr ) );
	pm.setDevicePixelRatio( dpr );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing );
	p.scale( px * dpr / 16.0, px * dpr / 16.0 );

	// the one triangle every arrow in the set is made of
	auto tri = [&]( qreal tipX, qreal backX ) {
		QPainterPath t;
		t.moveTo( tipX, 8.0 );
		t.lineTo( backX, 3.0 );
		t.lineTo( backX, 13.0 );
		t.closeSubpath();
		p.fillPath( t, ink );
	};
	auto diamond = [&]( qreal cx, qreal r ) {
		QPainterPath d;
		d.moveTo( cx, 8.0 - r );
		d.lineTo( cx + r, 8.0 );
		d.lineTo( cx, 8.0 + r );
		d.lineTo( cx - r, 8.0 );
		d.closeSubpath();
		p.fillPath( d, ink );
	};
	auto bar = [&]( qreal x ) { p.fillRect( QRectF( x, 3.0, 2.0, 10.0 ), ink ); };

	switch ( glyph ) {
	case GlyphToStart:
		bar( 2.5 );
		tri( 5.5, 13.5 );
		break;
	case GlyphToEnd:
		bar( 11.5 );
		tri( 10.5, 2.5 );
		break;
	case GlyphPrevKey:
		tri( 2.5, 8.0 );
		diamond( 12.0, 3.0 );
		break;
	case GlyphNextKey:
		tri( 13.5, 8.0 );
		diamond( 4.0, 3.0 );
		break;
	case GlyphPlayBack:
		tri( 3.0, 13.0 );
		break;
	case GlyphPlay:
		tri( 13.0, 3.0 );
		break;
	case GlyphPause:
		bar( 4.0 );
		bar( 10.0 );
		break;
	case GlyphStop:
		p.fillRect( QRectF( 4.0, 4.0, 8.0, 8.0 ), ink );
		break;
	case GlyphRecord:
		/* Blender's auto-key record dot. Radius 5, not 4: every other glyph
		   in this set is ten units tall (the triangle's back, the bar), and a
		   dot of diameter 8 beside them read as a speck -- which is how bungo
		   described it (2026-09-12 06:0x, "the round dot"). */
		p.setBrush( ink );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 8.0, 8.0 ), 5.0, 5.0 );
		break;
	case GlyphPose: {
		/* POSE WITH THE GIZMO. Blender has no single icon for this: its
		   armature glyph is a bone, and the gizmo is a ring drawn in the
		   viewport, never on a button. So this glyph is the two put together,
		   and it is OURS, not Blender's -- said plainly because the house rule
		   is to follow Blender and name every divergence.

		   The bone is Blender's OCTAHEDRAL bone, the shape an armature draws:
		   a head at the top, two shoulders a third of the way down, a long
		   tail. Around it are the two side arcs of the rotate gizmo's ring,
		   open at the top and the bottom so the bone's two tips read as tips
		   instead of merging into the ring. Same 2 units of stroke and the
		   same 16x16 grid as every other glyph here. */
		const QRectF ring( 1.6, 1.6, 12.8, 12.8 );
		p.setPen( QPen( ink, 2.0, Qt::SolidLine, Qt::FlatCap ) );
		p.setBrush( Qt::NoBrush );
		p.drawArc( ring, int( -50 * 16 ), int( 100 * 16 ) );
		p.drawArc( ring, int( 130 * 16 ), int( 100 * 16 ) );
		p.setPen( Qt::NoPen );
		QPainterPath bone;
		bone.moveTo( 8.0, 1.8 );
		bone.lineTo( 10.4, 6.0 );
		bone.lineTo( 8.0, 14.2 );
		bone.lineTo( 5.6, 6.0 );
		bone.closeSubpath();
		p.fillPath( bone, ink );
		break;
	}
	case GlyphLoop: {
		/* the two-arrow cycle the ruling names: two arcs of one circle, each
		   with the same arrowhead at its leading end. */
		const QRectF box( 3.0, 3.0, 10.0, 10.0 );
		p.setPen( QPen( ink, 2.0, Qt::SolidLine, Qt::FlatCap ) );
		p.setBrush( Qt::NoBrush );
		p.drawArc( box, int( 60 * 16 ), int( 150 * 16 ) );
		p.drawArc( box, int( 240 * 16 ), int( 150 * 16 ) );
		p.setPen( Qt::NoPen );
		auto head = [&]( qreal deg ) {
			const qreal rad = deg * 0.017453292519943295;
			const QPointF at( 8.0 + 5.0 * std::cos( rad ), 8.0 - 5.0 * std::sin( rad ) );
			const QPointF t( -std::sin( rad ), -std::cos( rad ) );	// direction of travel
			const QPointF n( -t.y(), t.x() );
			QPainterPath a;
			a.moveTo( at + t * 3.0 );
			a.lineTo( at - t * 0.5 + n * 2.4 );
			a.lineTo( at - t * 0.5 - n * 2.4 );
			a.closeSubpath();
			p.fillPath( a, ink );
		};
		head( 210.0 );
		head( 30.0 );
		break;
	}
	default:
		break;
	}
	return pm;
}

//! The same glyph in the palette's ink and, for Disabled, in its muted ink.
QIcon wwTransportIcon( int glyph, int px = 16 )
{
	QIcon ic;
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "text" ) ) ), QIcon::Normal );
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "textMuted" ) ) ), QIcon::Disabled );
	return ic;
}

/*! A MODE toggle's icon: the same drawing, in the plain ink while the mode is
 *  off and in the palette's brightest ink, `textBright`, while it is on.
 *
 *  bungo, 2026-09-12 06:1x, asked for "Both icons" after "what is the \"pose\"
 *  button and the round dot" -- i.e. the two gizmo switches drawn like the
 *  rest of the row, with a LIT state so one can see at a glance that the mode
 *  is on. The first cut lit them in `accent`, and bungo overruled that at
 *  07:3x over icons_before_after.png: "Why do these turn yellow when
 *  selected? the buttons, keep them white". So ON is `textBright` (#f2f3f5),
 *  the brightest ink token in skinVars (nifskope_ui.cpp), and OFF stays
 *  `text` (#e6e8eb). That is a step of 33 in summed RGB out of a possible
 *  765 -- deliberately small, because the state signal the eye reads is the
 *  QSS `:checked` plate under the glyph (`bgBtnDown`, wwBoxedButtonQss) and
 *  the ink only has to stop contradicting it. Disabled/On is `text`:
 *  brighter than the `textMuted` a disabled OFF toggle draws, so a toggle
 *  that is ON but cannot be clicked does not read as OFF, and dimmer than
 *  the enabled white, so it still reads as disabled.
 *
 *  Qt picks the On pixmap for a checked QToolButton (QStyle sets State_On)
 *  and for a checked QAction in a menu.
 *
 *  WHO USES IT, and who does not. The two gizmo toggles do. Play and Play
 *  backwards do NOT: they already say they are running by swapping the
 *  glyph to the pause bars, and two signals for one state is one more than
 *  the button needs. Loop does NOT either, and that is a measurement, not
 *  a preference -- see the note in addAnimActions: the render toolbar owns
 *  that action's icon and re-skins it on every refresh. */
QIcon wwTransportToggleIcon( int glyph, int px = 16 )
{
	QIcon ic;
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "text" ) ) ), QIcon::Normal, QIcon::Off );
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "textBright" ) ) ), QIcon::Normal, QIcon::On );
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "textMuted" ) ) ), QIcon::Disabled, QIcon::Off );
	ic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( "text" ) ) ), QIcon::Disabled, QIcon::On );
	return ic;
}

const float WW_FRAME_EPS = 1.0e-4f;

} // namespace


/*
 *  Construction
 */

AnimWorkspace::AnimWorkspace( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "AnimWorkspace" ) );
	undo = new QUndoStack( this );
	undo->setObjectName( QStringLiteral( "AnimWsUndo" ) );
	wwAnimUndoGroup()->addStack( undo );
	refreshTimer = new QTimer( this );
	refreshTimer->setSingleShot( true );
	refreshTimer->setInterval( 50 );
	connect( refreshTimer, &QTimer::timeout, this, &AnimWorkspace::refresh );
	loadVocabulary();
	buildUi();
	connect( WwHkxAnimHub::instance(), &WwHkxAnimHub::clipsChanged, this, &AnimWorkspace::clipsChanged );
	connect( WwHkxAnimHub::instance(), &WwHkxAnimHub::sentenceChanged, this,
		[this]( const QString & text, bool refusal ) {
			/* THE LABEL IS A FEW WORDS, THE TOOLTIP IS THE SENTENCE (lane UI6,
			 * 2026-09-11). bungo, over the old dock's binding paragraph: "look
			 * at all this text clutter" -- the bone-binding report named every
			 * unmatched bone in a pinned label. "78 of 95 bones" is the label;
			 * the names are one hover away. */
			say( WwHkxAnimHub::instance()->sentenceShort(), refusal, text ); } );
	// the buttons must not take keyboard focus (Tab is the viewport's edit toggle)
	for ( QAbstractButton * b : findChildren<QAbstractButton *>() )
		b->setFocusPolicy( Qt::NoFocus );
	refreshSummary();
}

AnimWorkspace::~AnimWorkspace()
{
	qDeleteAll( docs );
	docs.clear();
}

QSize AnimWorkspace::sizeHint() const
{
	return { 900, 320 };
}

void AnimWorkspace::setNif( NifModel * n )
{
	if ( nif ) {
		disconnect( nif, nullptr, this, nullptr );
	}
	nif = n;
	if ( nif ) {
		connect( nif, &NifModel::modelReset, this, &AnimWorkspace::refreshLater );
		connect( nif, &NifModel::rowsInserted, this, &AnimWorkspace::refreshLater );
		connect( nif, &NifModel::rowsRemoved, this, &AnimWorkspace::refreshLater );
	}
	refreshLater();
}

void AnimWorkspace::setGLView( GLView * view )
{
	if ( glView )
		disconnect( glView, nullptr, this, nullptr );
	glView = view;
	if ( glView ) {
		connect( glView, &GLView::sequencesUpdated, this, &AnimWorkspace::refreshLater );
		if ( speedBox )
			speedBox->setValue( double( glView->animationSpeed() ) );
	}
	refreshLater();
}

void AnimWorkspace::addAnimActions( QAction * loop, QAction * sw )
{
	loopAction = loop;
	switchAction = sw;
	if ( btnLoop && loop ) {
		/* The icon and the tip go on the ACTION, not on the button: a button
		   with a default action re-reads text, icon, tip and checked state from
		   it every time the action changes, so anything set on the button alone
		   is lost the first time the action is toggled. The Animation menu's
		   own Loop entry gets the same drawing, which is the point of one set. */
		/* NOT the toggle icon, and this is measured, not assumed. The loop button
		   takes its icon from the SHARED aAnimLoop action, and the render
		   toolbar re-skins that action on every refresh of its popup
		   (src/nifskope_ui.cpp, `ui->aAnimLoop->setIcon( tlMakeIcon( "loop", ... ) )`)
		   with a single-state icon. A lit state put here is overwritten the next
		   time that popup refreshes: in transport_2x_on.png the checked loop
		   button's ink is the PLAIN ink over the checked blue, unmoved from its
		   unchecked ink, while pose's and auto-key's does move. The harness
		   prints all three every run -- gate (q), the "gizmo toggles' ink" line
		   and the Loop say() beside it. Making loop light means changing the
		   RENDER TOOLBAR's own loop
		   glyph, which is not what bungo asked for (06:0x named the pose button
		   and the dot), so it stays his call. */
		loop->setIcon( wwTransportIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );
		loop->setToolTip( tr( "Loop the animation" ) );
		btnLoop->setDefaultAction( loop );
		btnLoop->setToolButtonStyle( Qt::ToolButtonIconOnly );
		btnLoop->setIconSize( QSize( WW_TRANSPORT_ICON_PX, WW_TRANSPORT_ICON_PX ) );
	}
}

#ifdef WW_ANIMWS_HKXMODEL
void AnimWorkspace::setHkxModel( HkxModel * model )
{
	if ( hkxModel )
		disconnect( hkxModel, nullptr, this, nullptr );
	hkxModel = model;
	if ( hkxModel ) {
		connect( hkxModel, &HkxModel::dataChanged, this, &AnimWorkspace::hkxModelChanged );
		connect( hkxModel, &HkxModel::modelReset, this, &AnimWorkspace::hkxModelChanged );
		hkxUndo = hkxModel->undoStack;
	}
}
#endif

void AnimWorkspace::installUndoGroup( QUndoStack * nifStack, QUndoStack * hkxStack )
{
	nifUndo = nifStack;
	hkxUndo = hkxStack;
	QUndoGroup * g = wwAnimUndoGroup();
	if ( nifStack && !g->stacks().contains( nifStack ) )
		g->addStack( nifStack );
	if ( hkxStack && !g->stacks().contains( hkxStack ) )
		g->addStack( hkxStack );
	// the active stack follows keyboard focus: inside this widget = ours
	connect( qApp, &QApplication::focusChanged, this, [this]( QWidget *, QWidget * now ) {
		QUndoGroup * grp = wwAnimUndoGroup();
		bool inside = false;
		for ( QWidget * w = now; w; w = w->parentWidget() ) {
			if ( w == this ) {
				inside = true;
				break;
			}
		}
		if ( inside ) {
			grp->setActiveStack( undo );
		} else if ( hkxUndo && grp->stacks().contains( hkxUndo ) && curIsClip
#ifdef WW_ANIMWS_HKXMODEL
					&& !hkxModelEntry.isEmpty() && hkxModelEntry == curEntry
#endif
					) {
			grp->setActiveStack( hkxUndo );
		} else if ( nifUndo ) {
			grp->setActiveStack( nifUndo );
		}
	} );
	if ( nifStack )
		g->setActiveStack( nifStack );
}


/*
 *  The UI
 */

void AnimWorkspace::buildUi()
{
	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 0 );

	/* ---- ruling 6: the actions come first (the header menus and the sheet's
	   own context menus share them), then the dock's header, then the
	   transport row. The bottom button row is gone. */
	buildActions();
	outer->addWidget( buildMenuBar() );
	outer->addWidget( buildTransport() );

	// ---- the live part: the list column and the dope sheet on one splitter
	split = new QSplitter( Qt::Horizontal, this );
	split->setObjectName( QStringLiteral( "AnimWsSplitter" ) );

	auto * leftCol = new QWidget( split );
	leftCol->setObjectName( QStringLiteral( "AnimWsLeftColumn" ) );
	auto * leftLay = new QVBoxLayout( leftCol );
	leftLay->setContentsMargins( 0, 0, 0, 0 );
	leftLay->setSpacing( 2 );

	// the list's header bar: Load / Unload / Root motion
	auto * listBar = new QHBoxLayout();
	listBar->setContentsMargins( 4, 2, 4, 0 );
	listBar->setSpacing( 4 );
	/* RULING 4's last line: "Load..." and "root" are clipped in this row. They
	   were clipped because the heading had ALL the stretch: it grew to whatever
	   the column was, and when the column was narrower than heading + three
	   buttons the layout took the difference out of the buttons' text. The
	   stretch is now an empty spacer between the heading and the buttons, so
	   the three buttons keep the width they ask for, and the column is given a
	   minimum wide enough for the whole row so the splitter cannot squeeze it
	   below that either. */
	QLabel * listHeading = wwHeading( tr( "Animations" ), leftCol );
	listBar->addWidget( listHeading );
	listBar->addStretch( 1 );
	btnLoadAnim = new QToolButton( leftCol );
	btnLoadAnim->setObjectName( QStringLiteral( "AnimWsLoadAnim" ) );
	btnLoadAnim->setText( tr( "Load…" ) );
	btnLoadAnim->setToolTip( tr( "Load a Havok animation (.hkx) and add it to this list. It plays on the "
								 "bones of the open NIF whose names match its tracks." ) );
	btnLoadAnim->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 6px" ) ) );
	connect( btnLoadAnim, &QToolButton::clicked, this, &AnimWorkspace::hkxChooseFile );
	listBar->addWidget( btnLoadAnim );
	btnUnloadAnim = new QToolButton( leftCol );
	btnUnloadAnim->setObjectName( QStringLiteral( "AnimWsUnloadAnim" ) );
	btnUnloadAnim->setText( QStringLiteral( "✕" ) );
	btnUnloadAnim->setToolTip( tr( "Unload the selected loaded animation (the menu unloads any one, or all)" ) );
	btnUnloadAnim->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 6px" ) ) );
	unloadMenu = new QMenu( this );
	btnUnloadAnim->setMenu( unloadMenu );
	btnUnloadAnim->setPopupMode( QToolButton::MenuButtonPopup );
	connect( btnUnloadAnim, &QToolButton::clicked, this, [this]() {
		if ( curIsClip && !curEntry.isEmpty() && glView )
			WwHkxAnimHub::instance()->unload( glView, curEntry );
	} );
	connect( unloadMenu, &QMenu::aboutToShow, this, &AnimWorkspace::rebuildUnloadMenu );
	listBar->addWidget( btnUnloadAnim );
	btnRootMotion = new QToolButton( leftCol );
	btnRootMotion->setObjectName( QStringLiteral( "AnimWsRootMotion" ) );
	btnRootMotion->setText( tr( "root" ) );
	btnRootMotion->setCheckable( true );
	btnRootMotion->setToolTip( tr( "Apply the loaded animation's extracted root motion to the skeleton root" ) );
	btnRootMotion->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 6px" ) ) );
	connect( btnRootMotion, &QToolButton::toggled, this, [this]( bool on ) {
		if ( glView )
			WwHkxAnimHub::instance()->setRootMotion( glView, on );
	} );
	listBar->addWidget( btnRootMotion );
	leftLay->addLayout( listBar );
	{
		// the width the header row needs, measured from the widgets themselves
		int need = 8 + 8;	// the row's left+right margins, plus a little air
		for ( QWidget * w : { (QWidget *)btnLoadAnim, (QWidget *)btnUnloadAnim, (QWidget *)btnRootMotion } )
			need += w->sizeHint().width() + listBar->spacing();
		need += listHeading->sizeHint().width();
		leftCol->setMinimumWidth( need );
	}

	list = new QListWidget( leftCol );
	list->setObjectName( QStringLiteral( "AnimWsClipList" ) );
	list->setStyleSheet( wwSelectionTreeQss() );
	list->setToolTip( tr( "The NIF's own sequences (triangle) and the loaded .hkx clips (diamond); one selection drives the scene.\n"
						  "Rename F2 or double-click, Duplicate Shift+D, Copy Ctrl+C, Cut Ctrl+X, Paste Ctrl+V, "
						  "Delete Del or X, Select all Ctrl+A, Move up/down Ctrl+Up / Ctrl+Down, and drag a row to reorder.\n"
						  "Right-click for the same list of actions." ) );
	/* RULING 5: a list one can work in. Shift and Ctrl pick several rows (the
	   CURRENT row is still the one that drives the scene); a drag reorders the
	   loaded clips and Qt draws the drop line; the same actions are on the
	   keyboard and in the right-click menu. */
	list->setSelectionMode( QAbstractItemView::ExtendedSelection );
	list->setDragDropMode( QAbstractItemView::InternalMove );
	list->setDefaultDropAction( Qt::MoveAction );
	list->setDropIndicatorShown( true );
	list->setContextMenuPolicy( Qt::CustomContextMenu );
	list->setMinimumHeight( 60 );
	list->viewport()->installEventFilter( this );
	connect( list, &QListWidget::itemSelectionChanged, this, &AnimWorkspace::listRowChosen );
	connect( list, &QListWidget::customContextMenuRequested, this, &AnimWorkspace::listContextMenu );
	connect( list, &QListWidget::itemChanged, this, &AnimWorkspace::listItemChanged );
	connect( list, &QListWidget::itemDoubleClicked, this, &AnimWorkspace::listItemDoubleClicked );
	{
		// one shortcut per action, live only while the list has the keyboard
		auto sc = [this]( const QKeySequence & key, const char * name, void ( AnimWorkspace::*fn )() ) {
			auto * s = new QShortcut( key, list );
			s->setObjectName( QString::fromLatin1( name ) );
			s->setContext( Qt::WidgetWithChildrenShortcut );
			connect( s, &QShortcut::activated, this, fn );
		};
		sc( QKeySequence( Qt::Key_Delete ), "AnimWsListDelete", &AnimWorkspace::listDelete );
		sc( QKeySequence( Qt::Key_X ), "AnimWsListDeleteX", &AnimWorkspace::listDelete );	// Blender's X
		sc( QKeySequence::Copy, "AnimWsListCopy", &AnimWorkspace::listCopy );
		sc( QKeySequence::Paste, "AnimWsListPaste", &AnimWorkspace::listPaste );
		sc( QKeySequence::Cut, "AnimWsListCut", &AnimWorkspace::listCut );
		sc( QKeySequence( Qt::SHIFT | Qt::Key_D ), "AnimWsListDuplicate", &AnimWorkspace::listDuplicate );
		sc( QKeySequence( Qt::Key_F2 ), "AnimWsListRename", &AnimWorkspace::listRename );
		sc( QKeySequence::SelectAll, "AnimWsListSelectAll", &AnimWorkspace::listSelectAll );
		sc( QKeySequence( Qt::CTRL | Qt::Key_Up ), "AnimWsListMoveUp", &AnimWorkspace::listMoveUp );
		sc( QKeySequence( Qt::CTRL | Qt::Key_Down ), "AnimWsListMoveDown", &AnimWorkspace::listMoveDown );
	}
	leftLay->addWidget( list, 1 );

	/* ruling 6a: the settings are NOT under the list any more -- they are the
	   right-side panel, the third pane of this splitter. The left column keeps
	   only the Animations list. */
	split->addWidget( leftCol );

	sheet = new AnimDopeSheet( split );
	connect( sheet, &AnimDopeSheet::rowSelected, this, &AnimWorkspace::sheetRowSelected );
	connect( sheet, &AnimDopeSheet::frameScrubbed, this, &AnimWorkspace::sheetFrameScrubbed );
	connect( sheet, &AnimDopeSheet::keysDragged, this, &AnimWorkspace::sheetKeysDragged );
	connect( sheet, &AnimDopeSheet::markerDragged, this, &AnimWorkspace::sheetMarkerDragged );
	connect( sheet, &AnimDopeSheet::rangeDragged, this, &AnimWorkspace::sheetRangeDragged );
	connect( sheet, &AnimDopeSheet::markerActivated, this, &AnimWorkspace::sheetMarkerActivated );
	connect( sheet, &AnimDopeSheet::sheetContextMenu, this, &AnimWorkspace::sheetContextMenu );
	connect( sheet, &AnimDopeSheet::markerNameEntered, this, &AnimWorkspace::sheetMarkerNameEntered );
	connect( sheet, &AnimDopeSheet::markerRenamed, this, &AnimWorkspace::sheetMarkerRenamed );
	connect( sheet, &AnimDopeSheet::markerAddRequested, this, &AnimWorkspace::sheetMarkerAddRequested );
	connect( sheet, &AnimDopeSheet::markerRenameRequested, this, &AnimWorkspace::sheetMarkerRenameRequested );
	connect( sheet, &AnimDopeSheet::deleteRequested, this, &AnimWorkspace::deleteSelected );
	connect( sheet, &AnimDopeSheet::selectionChanged, this, &AnimWorkspace::refreshSummary );
	split->addWidget( sheet );

	/* ---- RULING 6a, verbatim: "Why not add another panel in the animation
	   manager, that opens from the right side, that contains more stuff."
	   It is the splitter's third pane, so it opens and closes from the RIGHT
	   edge of the dock, it can be dragged to any width, and the width is
	   remembered. The toggle is in the dock header and on the N key. */
	sidePanel = new QWidget( split );
	sidePanel->setObjectName( QStringLiteral( "AnimWsSidePanel" ) );
	sidePanel->setMinimumWidth( 160 );
	{
		auto * pv = new QVBoxLayout( sidePanel );
		pv->setContentsMargins( 0, 0, 0, 0 );
		pv->setSpacing( 0 );
		pv->addWidget( buildSettings() );
	}
	split->addWidget( sidePanel );
	split->setStretchFactor( 0, 0 );
	split->setStretchFactor( 1, 1 );
	split->setStretchFactor( 2, 0 );
	split->setCollapsible( 1, false );
	{
		QSettings s;
		const QByteArray st = s.value( QStringLiteral( "AnimWorkspace/splitter" ) ).toByteArray();
		if ( !st.isEmpty() )
			split->restoreState( st );
		else
			split->setSizes( { 300, 600 } );
	}
	connect( split, &QSplitter::splitterMoved, this, [this]() {
		QSettings s;
		s.setValue( QStringLiteral( "AnimWorkspace/splitter" ), split->saveState() );
		// the panel's width is remembered as dragged (ruling 6a)
		if ( sidePanel && sidePanel->isVisible() && sidePanel->width() > 60 ) {
			sidePanelWidth = sidePanel->width();
			s.setValue( QStringLiteral( "AnimWorkspace/sidePanelWidth" ), sidePanelWidth );
		}
	} );
	{
		QSettings s;
		sidePanelWidth = std::max( 160, s.value( QStringLiteral( "AnimWorkspace/sidePanelWidth" ), 260 ).toInt() );
		setSidePanelOpen( s.value( QStringLiteral( "AnimWorkspace/sidePanelOpen" ), true ).toBool() );
	}
	/* ---- ruling 6a: the N key, Blender's own sidebar key. It is installed on
	   the SHEET and on the LIST, not on the workspace: a shortcut with
	   WidgetWithChildren context on the workspace would take the letter n away
	   from the annotation Name field, which one has to be able to type. The
	   divergence from Blender (where N works anywhere in the editor) is named
	   in the lane report. */
	{
		QWidget * hosts[2] = { sheet, list };
		const char * names[2] = { "AnimWsSidePanelKey", "AnimWsSidePanelKey2" };
		for ( int i = 0; i < 2; i++ ) {
			auto * sc = new QShortcut( QKeySequence( Qt::Key_N ), hosts[i] );
			sc->setObjectName( QString::fromLatin1( names[i] ) );
			sc->setContext( Qt::WidgetWithChildrenShortcut );
			connect( sc, &QShortcut::activated, this, [this]() {
				setSidePanelOpen( !( sidePanel && sidePanel->isVisible() ) );
			} );
		}
	}
	/* ---- RULING 4 asks for tooltips that name "the action AND its shortcut".
	   Five of the transport buttons had no shortcut at all, so here they are,
	   Blender's own timeline keys, on the SHEET only (the list needs Up/Down
	   for its rows, and a text field needs its space bar). The sheet already
	   owns Left / Right = step one frame and Home = frame all. */
	if ( sheet ) {
		struct { QKeySequence key; const char * name; QToolButton ** btn; } keys[] = {
			{ QKeySequence( Qt::Key_Space ),                        "AnimWsPlayKey",     &btnPlay },
			{ QKeySequence( Qt::SHIFT | Qt::CTRL | Qt::Key_Space ), "AnimWsPlayBackKey", &btnPlayBack },
			{ QKeySequence( Qt::SHIFT | Qt::Key_Left ),             "AnimWsToStartKey",  &btnToStart },
			{ QKeySequence( Qt::SHIFT | Qt::Key_Right ),            "AnimWsToEndKey",    &btnToEnd },
			{ QKeySequence( Qt::Key_Up ),                           "AnimWsNextKeyKey",  &btnNextKey },
			{ QKeySequence( Qt::Key_Down ),                         "AnimWsPrevKeyKey",  &btnPrevKey },
		};
		for ( const auto & k : keys ) {
			auto * sc = new QShortcut( k.key, sheet );
			sc->setObjectName( QString::fromLatin1( k.name ) );
			sc->setContext( Qt::WidgetWithChildrenShortcut );
			QToolButton ** slot = k.btn;
			connect( sc, &QShortcut::activated, this, [slot]() {
				if ( *slot && ( *slot )->isEnabled() )
					( *slot )->click();
			} );
		}
	}
	outer->addWidget( split, 1 );

	// ---- pinned: the summary-or-refusal line and the action bar
	note = new QLabel( this );
	note->setObjectName( QStringLiteral( "AnimWsNote" ) );
	note->setWordWrap( true );
	note->setContentsMargins( 6, 2, 6, 2 );
	note->setTextInteractionFlags( Qt::TextSelectableByMouse );
	outer->addWidget( note );
	// no action bar under it any more: ruling 6 sent those fifteen to the menus
}

QWidget * AnimWorkspace::buildTransport()
{
	auto * bar = new QWidget( this );
	bar->setObjectName( QStringLiteral( "AnimWsTransport" ) );
	auto * lay = new QHBoxLayout( bar );
	lay->setContentsMargins( 4, 2, 4, 2 );
	lay->setSpacing( 4 );

	/* RULING 4: one weight, one size, drawn from the palette. A button with a
	   glyph keeps its WORD as its text (accessibility reads it, and it is what
	   a screen reader says) but shows the drawing; a button with no glyph is a
	   word, as it was. */
	auto mk = [&]( const char * name, const QString & text, const QString & tip, bool checkable, int glyph = -1 ) {
		auto * b = new QToolButton( bar );
		b->setObjectName( QString::fromLatin1( name ) );
		b->setText( text );
		b->setToolTip( tip );
		b->setCheckable( checkable );
		if ( glyph >= 0 ) {
			b->setIcon( wwTransportIcon( glyph, WW_TRANSPORT_ICON_PX ) );
			b->setIconSize( QSize( WW_TRANSPORT_ICON_PX, WW_TRANSPORT_ICON_PX ) );
			b->setToolButtonStyle( Qt::ToolButtonIconOnly );
		}
		b->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 6px" ) ) );
		lay->addWidget( b );
		return b;
	};
	btnToStart = mk( "AnimWsToStart", tr( "To start" ), tr( "Jump to the first frame (Shift+Left)" ), false, GlyphToStart );
	btnPrevKey = mk( "AnimWsPrevKey", tr( "Previous key" ), tr( "Jump to the previous key on the selected row, or on any row when none is selected (Down)" ), false, GlyphPrevKey );
	btnPlayBack = mk( "AnimWsPlayBack", tr( "Play backwards" ), tr( "Play backwards (Shift+Ctrl+Space)" ), true, GlyphPlayBack );
	btnPlay = mk( "AnimWsPlay", tr( "Play" ), tr( "Play, and pause while it plays (Space)" ), true, GlyphPlay );
	btnStop = mk( "AnimWsStop", tr( "Stop" ), tr( "Stop playing" ), false, GlyphStop );
	btnNextKey = mk( "AnimWsNextKey", tr( "Next key" ), tr( "Jump to the next key on the selected row, or on any row when none is selected (Up)" ), false, GlyphNextKey );
	btnToEnd = mk( "AnimWsToEnd", tr( "To end" ), tr( "Jump to the last frame (Shift+Right)" ), false, GlyphToEnd );
	btnLoop = mk( "AnimWsLoop", tr( "Loop" ), tr( "Loop the animation (the render toolbar's own switch)" ), true, GlyphLoop );
	/* ---- RULING 6, verbatim: "Gizmo options (Pose with the gizmo, Auto-key)
	   become two toggle buttons in the transport row, like Blender's auto-key
	   record button." Same object names as the two check boxes they replace. */
	/* Both of these were the odd ones out until 2026-09-12 06:1x: "pose" was
	   a WORD in a row of drawings, and its text sat on the font's baseline,
	   nine 2:1 pixels below the line every glyph beside it is centred on
	   (measured off transport_2x.png: glyph ink centre y 27.5, the word's
	   36.5). That is what read as "not centered". Both are glyphs now, on the
	   same 16x16 grid, lit in `textBright` while the mode is on (07:3x: the
	   lit ink was the accent for one build, and bungo asked for white). */
	poseCheck = mk( "AnimWsPose", tr( "pose" ),
		tr( "Pose with the gizmo: hold the selected bone out of the clip's pose so the viewport shows what the transform gizmo (G/R/S) does to it; Insert key then keys that pose at the playhead" ), true, GlyphPose );
	poseCheck->setIcon( wwTransportToggleIcon( GlyphPose, WW_TRANSPORT_ICON_PX ) );
	autoKeyCheck = mk( "AnimWsAutoKey", tr( "Auto-key" ),
		tr( "Auto-key gizmo transforms: after every committed gizmo transform, key the bone at the playhead" ), true, GlyphRecord );
	autoKeyCheck->setIcon( wwTransportToggleIcon( GlyphRecord, WW_TRANSPORT_ICON_PX ) );
	connect( poseCheck, &QToolButton::toggled, this, &AnimWorkspace::poseToggled );
	connect( autoKeyCheck, &QToolButton::toggled, this, [this]( bool on ) {
		if ( glView )
			glView->gizmoAutoKey = on;
	} );

	connect( btnToStart, &QToolButton::clicked, this, [this]() { emit timeChanged( sceneMin ); } );
	connect( btnToEnd, &QToolButton::clicked, this, [this]() { emit timeChanged( sceneMax ); } );
	connect( btnPlay, &QToolButton::clicked, this, [this]() { emit playPauseRequested( 1 ); } );
	connect( btnPlayBack, &QToolButton::clicked, this, [this]() { emit playPauseRequested( -1 ); } );
	connect( btnStop, &QToolButton::clicked, this, [this]() { emit playPauseRequested( 0 ); } );
	auto jump = [this]( int dir ) {
		const int cur = currentFrame();
		int best = -1;
		const QVector<AnimWsRow> & rows = sheet->rows();
		const int sel = sheet->currentRow();
		auto consider = [&]( int f ) {
			if ( dir > 0 && f > cur && ( best < 0 || f < best ) ) best = f;
			if ( dir < 0 && f < cur && ( best < 0 || f > best ) ) best = f;
		};
		const HkxClipDocument * d = document();
		for ( int i = 0; i < rows.count(); i++ ) {
			if ( sel >= 0 && i != sel )
				continue;
			const AnimWsRow & r = rows.at( i );
			if ( r.kind == AnimWsRow::NifTrack ) {
				for ( int f : r.nifKeyFrames ) consider( f );
			} else if ( d && ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) && r.track >= 0 && r.track < d->keys.count() ) {
				for ( const HkxKey & k : d->keys.at( r.track ) ) consider( k.frame );
			} else if ( d && r.kind == AnimWsRow::Float && r.floatTrack >= 0 && r.floatTrack < d->floatTracks.count() ) {
				for ( const HkxFloatKey & k : d->floatTracks.at( r.floatTrack ).keys ) consider( k.frame );
			}
		}
		if ( best >= 0 )
			emit timeChanged( timeOf( best ) );
	};
	connect( btnPrevKey, &QToolButton::clicked, this, [jump]() { jump( -1 ); } );
	connect( btnNextKey, &QToolButton::clicked, this, [jump]() { jump( 1 ); } );

	/* THE PLAY RANGE, in the transport row (bungo's ruling 9, 2026-09-12:
	   "Allow me to drag the starting and ending frame, add markers for them of
	   some kind I can drag"). These two boxes and the two grips on the sheet's
	   ruler are the same pair of numbers; the old Trim from / Trim to rows in
	   the settings are gone, and what was the Trim button is now "Trim to
	   range", the same cut made immediately.

	   No native spin arrows, same reason as the speed box below. */
	lay->addSpacing( 8 );
	auto rangeBox = [&]( const char * name, const QString & label, const QString & tip ) {
		auto * l = new QLabel( label, bar );
		l->setObjectName( QString::fromLatin1( name ) + QStringLiteral( "Label" ) );
		lay->addWidget( l );
		auto * s = new QSpinBox( bar );
		s->setObjectName( QString::fromLatin1( name ) );
		wwMakeScrubField( s, WwScrubSpec{ 1.0, WwScrubSpec::No, false } );
		s->setButtonSymbols( QAbstractSpinBox::NoButtons );
		s->setRange( 0, 1000000 );
		s->setToolTip( tip );
		s->setMaximumWidth( 64 );
		lay->addWidget( s );
		return s;
	};
	rangeStartBox = rangeBox( "AnimWsRangeStart", tr( "Start" ),
		tr( "First frame of the play range; drag the left grip on the ruler, or Ctrl+Home to put it on the playhead" ) );
	rangeEndBox = rangeBox( "AnimWsRangeEnd", tr( "End" ),
		tr( "Last frame of the play range; drag the right grip on the ruler, or Ctrl+End to put it on the playhead" ) );
	connect( rangeStartBox, &QSpinBox::editingFinished, this, &AnimWorkspace::setRangeFromBoxes );
	connect( rangeEndBox, &QSpinBox::editingFinished, this, &AnimWorkspace::setRangeFromBoxes );

	lay->addSpacing( 8 );
	speedBox = new QDoubleSpinBox( bar );
	speedBox->setObjectName( QStringLiteral( "AnimWsSpeed" ) );
	wwMakeScrubField( speedBox, WwScrubSpec{ 0.05, WwScrubSpec::No, false } );
	/* NO NATIVE SPIN ARROWS (bungo, 2026-09-10 21:1x, over the old dock's two
	 * fields whose steppers were squeezed to a sliver: "Do you see it?", then
	 * "we have a new standard for those sliders, don't you remember?").
	 *
	 * wwMakeScrubField only takes Qt's up/down buttons away when its `chrome`
	 * flag is on (src/ui/widgets/wwnumberfield.cpp:959), and this row asks for
	 * no chrome because the transport is tight. So these two were STAMPED as
	 * scrub fields and kept the native buttons anyway -- which lane UI6's new
	 * stepper gate found on its first run. Drag or type; there is nothing to
	 * squeeze. */
	speedBox->setButtonSymbols( QAbstractSpinBox::NoButtons );
	speedBox->setRange( 0.05, 10.0 );
	speedBox->setDecimals( 2 );
	speedBox->setSingleStep( 0.05 );
	speedBox->setValue( 1.0 );
	/* RULING 4: "speed a labelled spin, \"1.50x\"" -- the label goes AFTER the
	   number, which is how one says it. */
	speedBox->setSuffix( QStringLiteral( "x" ) );
	speedBox->setToolTip( tr( "Playback speed; drag or type (1.00x is the clip's own rate)" ) );
	speedBox->setMaximumWidth( 70 );
	connect( speedBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this]( double v ) {
		if ( glView )
			glView->setAnimSpeed( float( v ) );
	} );
	lay->addWidget( speedBox );

	rateLabel = new QLabel( bar );
	rateLabel->setObjectName( QStringLiteral( "AnimWsRate" ) );
	rateLabel->setToolTip( tr( "The selected animation's own frame rate (the ruler ticks at it)" ) );
	rateLabel->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( "textMuted" ) ) );
	lay->addWidget( rateLabel );

	frameBox = new QSpinBox( bar );
	frameBox->setObjectName( QStringLiteral( "AnimWsFrame" ) );
	wwMakeScrubField( frameBox, WwScrubSpec{ 0.0, WwScrubSpec::Yes, false } );
	frameBox->setButtonSymbols( QAbstractSpinBox::NoButtons );	// see Speed above
	frameBox->setRange( 0, 100000 );
	frameBox->setToolTip( tr( "Current frame; type or scrub to move the playhead" ) );
	frameBox->setMaximumWidth( 72 );
	connect( frameBox, qOverload<int>( &QSpinBox::valueChanged ), this, [this]( int v ) {
		if ( syncing )
			return;
		emit timeChanged( timeOf( v ) );
	} );
	lay->addWidget( frameBox );

	readout = new QLabel( bar );
	readout->setObjectName( QStringLiteral( "AnimWsReadout" ) );
	readout->setToolTip( tr( "Current frame of the selected animation, at its own rate" ) );
	readout->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( "textMuted" ) ) );
	lay->addWidget( readout );
	lay->addStretch( 1 );
	return bar;
}

QWidget * AnimWorkspace::buildSettings()
{
	settingsArea = new QScrollArea( this );
	settingsArea->setObjectName( QStringLiteral( "AnimWsSettings" ) );
	settingsArea->setWidgetResizable( true );
	settingsArea->setFrameShape( QFrame::NoFrame );
	auto * page = new QWidget( settingsArea );
	page->setObjectName( QStringLiteral( "AnimWsSettingsPage" ) );
	auto * v = new QVBoxLayout( page );
	v->setContentsMargins( 4, 2, 4, 4 );
	v->setSpacing( 4 );
	const int labelW = 120;

	auto grid = [&]( QWidget * parent ) {
		auto * g = new QGridLayout( parent );
		g->setContentsMargins( 0, 0, 0, 0 );
		g->setHorizontalSpacing( 6 );
		g->setVerticalSpacing( 2 );
		g->setColumnMinimumWidth( 0, labelW );
		g->setColumnStretch( 1, 1 );
		return g;
	};
	auto row = [&]( QGridLayout * g, int r, const QString & label, QWidget * field, const QString & tip ) {
		auto * l = new QLabel( label, g->parentWidget() );
		l->setToolTip( tip );
		field->setToolTip( tip );
		g->addWidget( l, r, 0 );
		g->addWidget( field, r, 1 );
		return l;
	};

	// ---- the selected NIF sequence's own controls (the old manager's, as rows)
	seqSection = new QWidget( page );
	seqSection->setObjectName( QStringLiteral( "AnimWsSequenceSection" ) );
	auto * sv = new QVBoxLayout( seqSection );
	sv->setContentsMargins( 0, 0, 0, 0 );
	sv->setSpacing( 2 );
	sv->addWidget( wwHeading( tr( "Sequence" ), seqSection ) );
	auto * sw = new QWidget( seqSection );
	auto * sg = grid( sw );
	cycleBox = new QComboBox( sw );
	cycleBox->setObjectName( QStringLiteral( "AnimWsCycleType" ) );
	cycleBox->addItems( { tr( "Loop" ), tr( "Reverse" ), tr( "Clamp" ) } );
	wwMatchFieldStyle( cycleBox );
	row( sg, 0, tr( "Cycle type" ), cycleBox, tr( "How the sequence repeats: loop, ping-pong (reverse), or hold at the end (clamp)" ) );
	freqBox = new QDoubleSpinBox( sw );
	freqBox->setObjectName( QStringLiteral( "AnimWsFrequency" ) );
	freqBox->setRange( 0.001, 1000.0 );
	freqBox->setDecimals( 4 );
	freqBox->setSingleStep( 0.1 );
	wwMakeScrubField( freqBox );
	row( sg, 1, tr( "Frequency" ), freqBox, tr( "The sequence's playback rate multiplier" ) );
	startBox = new QDoubleSpinBox( sw );
	startBox->setObjectName( QStringLiteral( "AnimWsStartTime" ) );
	startBox->setRange( -1000.0, 100000.0 );
	startBox->setDecimals( 4 );
	startBox->setSingleStep( 0.0333 );
	wwMakeScrubField( startBox );
	row( sg, 2, tr( "Start time" ), startBox, tr( "Where the sequence starts, in seconds" ) );
	stopBox = new QDoubleSpinBox( sw );
	stopBox->setObjectName( QStringLiteral( "AnimWsStopTime" ) );
	stopBox->setRange( -1000.0, 100000.0 );
	stopBox->setDecimals( 4 );
	stopBox->setSingleStep( 0.0333 );
	wwMakeScrubField( stopBox );
	row( sg, 3, tr( "Stop time" ), stopBox, tr( "Where the sequence ends, in seconds" ) );
	for ( QDoubleSpinBox * b : { freqBox, startBox, stopBox } )
		connect( b, &QDoubleSpinBox::editingFinished, this, &AnimWorkspace::sequenceFieldEdited );
	connect( cycleBox, qOverload<int>( &QComboBox::activated ), this, [this]( int ) { sequenceFieldEdited(); } );
	sv->addWidget( sw );
	v->addWidget( seqSection );

	/* ---- the edit rows (a loaded clip). RULING 6, his words: "it's getting
	   pretty crowded in here, isn't it?" -- over a Keys block that held two
	   check boxes and two tolerances, with the annotation, range, root-motion
	   and float rows under it and fifteen buttons below that, all of it on
	   screen at once whatever was selected.

	   THE SHAPE IS HIS: Blender's dope sheet. The actions went to the header
	   menus (buildActions / buildMenuBar), the two gizmo switches to the
	   transport row, and what is left here is ONLY what is being edited -- one
	   section per KIND of selection, and the others are not shown at all
	   (updateSections). The Keys block, as a block, is gone. */
	editSection = new QWidget( page );
	editSection->setObjectName( QStringLiteral( "AnimWsEditSection" ) );
	auto * ev = new QVBoxLayout( editSection );
	ev->setContentsMargins( 0, 0, 0, 0 );
	ev->setSpacing( 2 );

	auto section = [&]( const char * name, const QString & title, QWidget ** out ) {
		auto * sect = new QWidget( editSection );
		sect->setObjectName( QString::fromLatin1( name ) );
		auto * lv = new QVBoxLayout( sect );
		lv->setContentsMargins( 0, 0, 0, 0 );
		lv->setSpacing( 2 );
		lv->addWidget( wwHeading( title, sect ) );
		auto * body = new QWidget( sect );
		lv->addWidget( body );
		ev->addWidget( sect );
		*out = sect;
		return body;
	};

	// ---- Clip: the whole clip's own settings, while a clip is open
	{
		QWidget * w = section( "AnimWsClipSection", tr( "Clip" ), &clipSection );
		auto * g = grid( w );
		rootTrackBox = new QComboBox( w );
		rootTrackBox->setObjectName( QStringLiteral( "AnimWsRootTrack" ) );
		wwMatchFieldStyle( rootTrackBox );
		row( g, 0, tr( "Root motion track" ), rootTrackBox, tr( "The track whose translation travel Bake root moves into the clip's extracted motion (COM on a Fallout 4 character)" ) );
		retimeBox = new QDoubleSpinBox( w );
		retimeBox->setObjectName( QStringLiteral( "AnimWsRetimeFps" ) );
		retimeBox->setRange( 1.0, 240.0 );
		retimeBox->setDecimals( 2 );
		retimeBox->setValue( 30.0 );
		retimeBox->setSuffix( QStringLiteral( " fps" ) );
		wwMakeScrubField( retimeBox );
		row( g, 1, tr( "Retime to" ), retimeBox, tr( "Resample the clip to this rate; frames on the old grid are copied exactly, the rest interpolate" ) );
	}

	// ---- Key: only with keys selected
	{
		QWidget * w = section( "AnimWsKeySection", tr( "Key" ), &keySection );
		auto * g = grid( w );
		tolTransBox = new QDoubleSpinBox( w );
		tolTransBox->setObjectName( QStringLiteral( "AnimWsReduceTol" ) );
		tolTransBox->setRange( 0.0, 100.0 );
		tolTransBox->setDecimals( 4 );
		tolTransBox->setSingleStep( 0.01 );
		tolTransBox->setValue( 0.01 );
		wwMakeScrubField( tolTransBox );
		row( g, 0, tr( "Reduce tolerance" ), tolTransBox, tr( "Reduce keeps the fewest keys that regenerate every frame within this many units of translation" ) );
		tolDegBox = new QDoubleSpinBox( w );
		tolDegBox->setObjectName( QStringLiteral( "AnimWsReduceDeg" ) );
		tolDegBox->setRange( 0.0, 180.0 );
		tolDegBox->setDecimals( 3 );
		tolDegBox->setSingleStep( 0.05 );
		tolDegBox->setValue( 0.05 );
		tolDegBox->setSuffix( QStringLiteral( "°" ) );
		wwMakeScrubField( tolDegBox );
		row( g, 1, tr( "Reduce rotation" ), tolDegBox, tr( "... and within this many degrees of rotation" ) );
	}

	// ---- Annotation: only with an annotation selected
	{
		QWidget * w = section( "AnimWsAnnotationSection", tr( "Annotation" ), &annotSection );
		auto * g = grid( w );
		annotBox = new QComboBox( w );
		annotBox->setObjectName( QStringLiteral( "AnimWsAnnotName" ) );
		annotBox->setEditable( true );
		annotBox->setInsertPolicy( QComboBox::NoInsert );
		wwMatchFieldStyle( annotBox );
		row( g, 0, tr( "Name" ), annotBox, tr( "The event name the behaviour graphs listen for: pick one the game's own clips carry (most frequent first) or type any text" ) );
	}

	// ---- Track: only with a bone row selected
	{
		QWidget * w = section( "AnimWsTrackSection", tr( "Track" ), &trackSection );
		auto * g = grid( w );
		trackNameBox = new QLineEdit( w );
		trackNameBox->setObjectName( QStringLiteral( "AnimWsTrackName" ) );
		// the number fields' own tokens, so it reads as the same kind of thing
		trackNameBox->setStyleSheet( QStringLiteral(
			"QLineEdit { background: %1; border: none; border-radius: 3px; color: %2; padding-left: 6px; }"
			"QLineEdit:disabled { background: %3; color: %4; }" )
			.arg( wwSkinColor( "bgInput" ), wwSkinColor( "text" ), wwSkinColor( "bgAlt" ), wwSkinColor( "textMuted" ) ) );
		row( g, 0, tr( "Bone" ), trackNameBox, tr( "The bone this track drives; type another name and leave the field to rename the track (one undo step)" ) );
		connect( trackNameBox, &QLineEdit::editingFinished, this, &AnimWorkspace::trackNameEdited );
	}

	// ---- Float track: only with a float row selected
	{
		QWidget * w = section( "AnimWsFloatSection", tr( "Float track" ), &floatSection );
		auto * g = grid( w );
		floatValueBox = new QDoubleSpinBox( w );
		floatValueBox->setObjectName( QStringLiteral( "AnimWsFloatValue" ) );
		floatValueBox->setRange( -1.0e6, 1.0e6 );
		floatValueBox->setDecimals( 4 );
		wwMakeScrubField( floatValueBox );
		row( g, 0, tr( "Value" ), floatValueBox, tr( "The value Set float key writes at the playhead on the selected float row" ) );
	}

	v->addWidget( editSection );
	v->addStretch( 1 );
	settingsArea->setWidget( page );
	// every field in a scroll area takes the wheel only while focused
	for ( QWidget * w : page->findChildren<QComboBox *>() )
		wwGuardWheel( w );
	return settingsArea;
}

/* ---- RULING 6 of 2026-09-12, verbatim: "a header menu bar (Key, Channel,
   Marker, View) holds the actions", "The bottom button row goes away; every
   action stays reachable by menu, context menu and shortcut", "Save / Save as
   move to the dock's header menu (Clip > Save) and File; not a button at the
   bottom."

   Every one of the fifteen buttons is the SAME action here, with the same text,
   the same tooltip and the same object name, so nothing that asked for one by
   name -- a gate, a context menu -- has to change its question. */

void AnimWorkspace::buildActions()
{
	auto mk = [this]( const char * name, const QString & text, const QString & tip, void ( AnimWorkspace::*slot )() ) {
		auto * a = new QAction( text, this );
		a->setObjectName( QString::fromLatin1( name ) );
		a->setToolTip( tip );
		connect( a, &QAction::triggered, this, slot );
		return a;
	};
	actInsertKey = mk( "AnimWsInsertKey", tr( "Insert key" ), tr( "Key the selected bone's current local transform (what the gizmo left on its NIF block) at the playhead" ), &AnimWorkspace::insertKeyAtPlayhead );
	actDeleteKeys = mk( "AnimWsDeleteKeys", tr( "Delete" ), tr( "Delete the selected keys or the selected annotation (Del or X on the sheet)" ), &AnimWorkspace::deleteSelected );
	actReduce = mk( "AnimWsReduce", tr( "Reduce" ), tr( "Thin every track to the fewest keys within the Key section's two tolerances" ), &AnimWorkspace::reduceKeys );
	actAddAnnot = mk( "AnimWsAddAnnot", tr( "Add annotation" ), tr( "Add an annotation with the Annotation section's Name at the playhead (M on the sheet; right-click the sheet to add one at the clicked frame)" ), &AnimWorkspace::addAnnotationAtPlayhead );
	actRenameAnnot = mk( "AnimWsRenameAnnot", tr( "Rename" ), tr( "Rename the selected annotation to the Annotation section's Name (Ctrl+M on the sheet opens the inline editor)" ), &AnimWorkspace::renameSelectedAnnotation );
	actDeleteAnnot = mk( "AnimWsDeleteAnnot", tr( "Delete annotation" ), tr( "Delete the selected annotation (Del or X on the sheet)" ), &AnimWorkspace::deleteSelectedAnnotation );
	actTrim = mk( "AnimWsTrim", tr( "Trim to range" ), tr( "Cut the clip down to the play range (Start..End) now; a save writes the range even without this" ), &AnimWorkspace::trimToRange );
	actRetime = mk( "AnimWsRetime", tr( "Retime" ), tr( "Resample the clip to the Clip section's Retime to rate" ), &AnimWorkspace::retimeClip );
	actBake = mk( "AnimWsBake", tr( "Bake root" ), tr( "Move the root-motion track's translation travel into the extracted motion" ), &AnimWorkspace::bakeRootMotion );
	actUnbake = mk( "AnimWsUnbake", tr( "Unbake" ), tr( "Put the baked travel back on the track" ), &AnimWorkspace::unbakeRootMotion );
	actRemoveTrack = mk( "AnimWsRemoveTrack", tr( "Remove track" ), tr( "Remove the selected track from the clip" ), &AnimWorkspace::removeSelectedTrack );
	actRenameTrack = mk( "AnimWsRenameTrack", tr( "Rename track" ), tr( "Rename the selected track's bone" ), &AnimWorkspace::renameSelectedTrack );
	actRemoveAxes = mk( "AnimWsRemoveAxes", tr( "Remove transform axes…" ), tr( "Hold the chosen translation or rotation axes of the selected track at their frame 0 value" ), &AnimWorkspace::removeTransformAxesDialog );
	actAddFloat = mk( "AnimWsAddFloat", tr( "Add float track" ), tr( "Add a float track row (a document row; the .hkx writer does not carry float tracks yet)" ), &AnimWorkspace::addFloatTrack );
	actSetFloat = mk( "AnimWsSetFloat", tr( "Set float key" ), tr( "Key the Float track section's Value on the selected float row at the playhead" ), &AnimWorkspace::setFloatKeyAtPlayhead );
	actSave = mk( "AnimWsSave", tr( "Save" ), tr( "Write the clip back to its .hkx (interleaved, the class the game loads by name)" ), &AnimWorkspace::saveClip );
	actSaveAs = mk( "AnimWsSaveAs", tr( "Save as…" ), tr( "Write the clip to a new .hkx" ), &AnimWorkspace::saveClipAs );

	actFrameAll = new QAction( tr( "Frame all" ), this );
	actFrameAll->setObjectName( QStringLiteral( "AnimWsFrameAll" ) );
	actFrameAll->setToolTip( tr( "Fit the whole clip across the sheet (Home on the sheet)" ) );
	connect( actFrameAll, &QAction::triggered, this, [this]() {
		if ( sheet )
			sheet->frameAll();
	} );

	actSidePanel = new QAction( tr( "Side panel" ), this );
	actSidePanel->setObjectName( QStringLiteral( "AnimWsSidePanel" ) );
	actSidePanel->setCheckable( true );
	actSidePanel->setToolTip( tr( "Show the selected thing's settings in the panel on the right (N on the sheet or the list)" ) );
	connect( actSidePanel, &QAction::toggled, this, &AnimWorkspace::setSidePanelOpen );
}

QWidget * AnimWorkspace::buildMenuBar()
{
	auto * bar = new QWidget( this );
	bar->setObjectName( QStringLiteral( "AnimWsHeader" ) );
	auto * lay = new QHBoxLayout( bar );
	lay->setContentsMargins( 2, 0, 4, 0 );
	lay->setSpacing( 4 );

	headerMenu = new QMenuBar( bar );
	headerMenu->setObjectName( QStringLiteral( "AnimWsMenuBar" ) );
	headerMenu->setNativeMenuBar( false );		// it is this dock's header, not the window's
	headerMenu->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	auto add = [this]( const char * name, const QString & title, const QList<QAction *> & acts ) {
		QMenu * m = headerMenu->addMenu( title );
		m->setObjectName( QString::fromLatin1( name ) );
		/* The tooltips the buttons carried are not lost: Qt hides menu
		   tooltips unless asked. No blurbs in the menu itself -- label and
		   shortcut only, the sentence one hover away (bungo's standing rule). */
		m->setToolTipsVisible( true );
		for ( QAction * a : acts ) {
			if ( a )
				m->addAction( a );
			else
				m->addSeparator();
		}
		return m;
	};
	add( "AnimWsMenuClip", tr( "Clip" ), { actTrim, actRetime, nullptr, actBake, actUnbake, nullptr, actSave, actSaveAs } );
	add( "AnimWsMenuKey", tr( "Key" ), { actInsertKey, actDeleteKeys, nullptr, actReduce } );
	add( "AnimWsMenuChannel", tr( "Channel" ), { actRenameTrack, actRemoveTrack, actRemoveAxes, nullptr, actAddFloat, actSetFloat } );
	add( "AnimWsMenuMarker", tr( "Marker" ), { actAddAnnot, actRenameAnnot, actDeleteAnnot } );
	add( "AnimWsMenuView", tr( "View" ), { actSidePanel, nullptr, actFrameAll } );
	lay->addWidget( headerMenu );
	lay->addStretch( 1 );

	btnSidePanel = new QToolButton( bar );
	btnSidePanel->setObjectName( QStringLiteral( "AnimWsSidePanelBtn" ) );
	btnSidePanel->setCheckable( true );
	btnSidePanel->setText( QStringLiteral( "›" ) );
	btnSidePanel->setToolTip( tr( "Show the selected thing's settings in the panel on the right (N on the sheet or the list)" ) );
	btnSidePanel->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "2px 6px" ) ) );
	connect( btnSidePanel, &QToolButton::toggled, this, &AnimWorkspace::setSidePanelOpen );
	lay->addWidget( btnSidePanel );
	return bar;
}

void AnimWorkspace::setSidePanelOpen( bool on )
{
	if ( !sidePanel || !split )
		return;
	// what it was dragged to is what it opens at next time (ruling 6a)
	if ( !on && sidePanel->isVisible() && sidePanel->width() > 60 )
		sidePanelWidth = sidePanel->width();
	sidePanel->setVisible( on );
	if ( on ) {
		QList<int> sizes = split->sizes();
		if ( sizes.count() == 3 ) {
			const int want = std::max( 160, sidePanelWidth );
			// the sheet gives the width, never the list, and never below 160 px
			const int take = std::min( want - sizes.at( 2 ), std::max( 0, sizes.at( 1 ) - 160 ) );
			if ( take > 0 ) {
				sizes[1] -= take;
				sizes[2] += take;
				split->setSizes( sizes );
			}
		}
	}
	if ( btnSidePanel ) {
		QSignalBlocker b( btnSidePanel );
		btnSidePanel->setChecked( on );
		btnSidePanel->setText( on ? QStringLiteral( "›" ) : QStringLiteral( "‹" ) );
	}
	if ( actSidePanel ) {
		QSignalBlocker b( actSidePanel );
		actSidePanel->setChecked( on );
	}
	QSettings s;
	s.setValue( QStringLiteral( "AnimWorkspace/sidePanelOpen" ), on );
	s.setValue( QStringLiteral( "AnimWorkspace/sidePanelWidth" ), sidePanelWidth );
}

void AnimWorkspace::updateSections()
{
	/* RULING 6: "the sidebar holds only what is being edited, in collapsible
	   sections that show ONLY when their subject is selected". One section per
	   kind, in this order of precedence; the Clip section is the subject of the
	   whole dock, so it stays while a clip is open. */
	if ( !clipSection || !sheet )
		return;
	const HkxClipDocument * d = document();
	const bool clip = ( d != nullptr );
	const int r = sheet->currentRow();
	const bool floatRow = clip && r >= 0 && r < sheet->rows().count()
		&& sheet->rows().at( r ).kind == AnimWsRow::Float;
	const bool marker = clip && sheet->selectedMarker().valid();
	const bool keys = clip && !sheet->selectedKeys().isEmpty();
	const int track = selectedTrack();
	clipSection->setVisible( clip );
	annotSection->setVisible( marker );
	keySection->setVisible( keys && !marker );
	floatSection->setVisible( floatRow && !marker && !keys );
	trackSection->setVisible( clip && track >= 0 && !floatRow && !marker && !keys );
	if ( trackNameBox && track >= 0 && !trackNameBox->hasFocus() ) {
		const QString name = d ? d->trackNames.value( track ) : QString();
		if ( trackNameBox->text() != name ) {
			QSignalBlocker b( trackNameBox );
			trackNameBox->setText( name );
		}
	}
}

void AnimWorkspace::trackNameEdited()
{
	if ( syncing || !trackNameBox )
		return;
	const int track = selectedTrack();
	const HkxClipDocument * d = document();
	if ( !d || track < 0 )
		return;
	const QString want = trackNameBox->text().trimmed();
	if ( want.isEmpty() || want == d->trackNames.value( track ) ) {
		// nothing typed, or the same name: put the field back, change nothing
		QSignalBlocker b( trackNameBox );
		trackNameBox->setText( d->trackNames.value( track ) );
		return;
	}
	const HkxSkeleton * sk = nullptr;
	if ( playback() && !playback()->knownSkeletons().isEmpty() )
		sk = &playback()->knownSkeletons().last();
	edit( tr( "Rename track %1" ).arg( track ), [track, want, sk]( HkxClipDocument & dd ) { return dd.renameTrack( track, want, sk ); } );
}


/*
 *  The list and the rows
 */

void AnimWorkspace::showEvent( QShowEvent * event )
{
	QWidget::showEvent( event );
	// a refresh asked for while hidden is replayed now (a dock defers its work while hidden)
	if ( refreshPending )
		refreshTimer->start();
}

void AnimWorkspace::refreshLater()
{
	if ( !isVisible() ) {
		refreshPending = true;
		return;
	}
	refreshTimer->start();
}

void AnimWorkspace::refresh()
{
	if ( !isVisible() ) {
		refreshPending = true;
		return;
	}
	refreshPending = false;
	rebuildList();
	rebuildRows();
	refreshSummary();
	updateReadout();
}

void AnimWorkspace::clipsChanged()
{
	// a clip that went away takes its document with it
	QStringList names;
	if ( playback() )
		names = playback()->names();
	for ( auto it = docs.begin(); it != docs.end(); ) {
		if ( !names.contains( it.key() ) ) {
			delete it.value();
			it = docs.erase( it );
		} else {
			++it;
		}
	}
#ifdef WW_ANIMWS_HKXMODEL
	hkxModelEntry.clear();
	if ( hkxModel && !hkxModel->getFilename().isEmpty() ) {
		const QString stem = QFileInfo( hkxModel->getFilename() ).completeBaseName();
		for ( const QString & n : names ) {
			if ( n.compare( stem, Qt::CaseInsensitive ) == 0 ) {
				hkxModelEntry = n;
				break;
			}
		}
	}
#endif
	refreshLater();
}

void AnimWorkspace::rebuildList()
{
	syncing = true;
	/* WHAT WAS SELECTED, before the list is thrown away and built again.
	   Restoring only the current row (which is what this did until
	   2026-09-12) meant every refresh silently reduced a multi-selection to
	   one row, so Delete / Copy / Cut acted on one clip however many the
	   user had picked. The key is kind + name, because a NIF sequence and a
	   loaded clip may share a name. */
	QSet<QString> wasSelected;
	for ( int i = 0; i < list->count(); i++ ) {
		const QListWidgetItem * it = list->item( i );
		if ( it->isSelected() )
			wasSelected.insert( ( it->data( Qt::UserRole + 1 ).toBool() ? QStringLiteral( "C|" ) : QStringLiteral( "S|" ) )
				+ it->data( Qt::UserRole ).toString() );
	}
	list->clear();
	sequences.clear();
	clipEntries.clear();
	if ( nif ) {
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			const QModelIndex iBlock = nif->getBlockIndex( b );
			if ( !nif->isNiBlock( iBlock, QStringLiteral( "NiControllerSequence" ) ) )
				continue;
			sequences.append( QPersistentModelIndex( iBlock ) );
			const QString name = nif->get<QString>( iBlock, "Name" );
			auto * it = new QListWidgetItem( wwAnimGlyph( false, false ), name.isEmpty() ? tr( "(unnamed sequence)" ) : name );
			it->setData( Qt::UserRole, name );
			it->setData( Qt::UserRole + 1, false );
			it->setData( Qt::UserRole + 2, b );
			it->setToolTip( tr( "NiControllerSequence block %1" ).arg( b ) );
			list->addItem( it );
		}
	}
	if ( glView ) {
		const QVector<WwHkxListEntry> entries = WwHkxAnimHub::instance()->entries( glView );
		for ( const WwHkxListEntry & e : entries ) {
			clipEntries.append( e.name );
			auto * it = new QListWidgetItem( wwAnimGlyph( true, e.refused ), e.label() );
			it->setData( Qt::UserRole, e.name );
			it->setData( Qt::UserRole + 1, true );
			it->setData( Qt::UserRole + 2, -1 );
			it->setToolTip( e.refused ? e.reason : tr( "%1 — %2 frames at %3 fps, from %4" ).arg( e.name ).arg( e.numFrames ).arg( e.fps ).arg( e.path ) );
			if ( e.refused )
				it->setForeground( QColor( wwSkinColor( "danger" ) ) );
			list->addItem( it );
		}
	}
	// keep the selection -- the current row AND every other row that was selected
	QListWidgetItem * cur = nullptr;
	for ( int i = 0; i < list->count(); i++ ) {
		QListWidgetItem * it = list->item( i );
		const QString name = it->data( Qt::UserRole ).toString();
		const bool isClip = it->data( Qt::UserRole + 1 ).toBool();
		if ( !cur && name == curEntry && isClip == curIsClip )
			cur = it;
		if ( wasSelected.contains( ( isClip ? QStringLiteral( "C|" ) : QStringLiteral( "S|" ) ) + name ) )
			it->setSelected( true );
	}
	if ( cur ) {
		// the same rule as the driven re-entry: do not let Qt's ClearAndSelect
		// undo the selection this function has just put back
		list->setCurrentItem( cur, cur->isSelected() ? QItemSelectionModel::NoUpdate
												  : QItemSelectionModel::ClearAndSelect );
	}
	if ( !list->currentItem() && !curEntry.isEmpty() ) {
		curEntry.clear();
		curIsClip = false;
		curSeq = QPersistentModelIndex();
	}
	syncing = false;
}

/*
 *  THE ANIMATIONS LIST (bungo's ruling 5, 2026-09-12)
 *
 *  WHERE THE ORDER LIVES. The list shows the NIF's own NiControllerSequence
 *  blocks first, in the order they sit in the NIF -- that IS a file order, and
 *  changing it means moving blocks, which is a Blocks-tab edit; these rows
 *  refuse every action here with one sentence saying so. Under them are the
 *  loaded .hkx clips, and each of those is its OWN file: their order is the
 *  session's, not any file's, so a reorder writes nothing and nothing is lost
 *  when they are put back in another order tomorrow.
 */

QStringList AnimWorkspace::selectedClipNames() const
{
	QStringList out;
	for ( int i = 0; i < list->count(); i++ ) {
		const QListWidgetItem * it = list->item( i );
		if ( it->isSelected() && it->data( Qt::UserRole + 1 ).toBool() )
			out << it->data( Qt::UserRole ).toString();
	}
	return out;
}

void AnimWorkspace::selectListRow( const QString & name )
{
	for ( int i = 0; i < list->count(); i++ ) {
		QListWidgetItem * it = list->item( i );
		if ( it->data( Qt::UserRole ).toString() == name ) {
			list->setCurrentItem( it );
			list->clearSelection();
			it->setSelected( true );
			return;
		}
	}
}

void AnimWorkspace::listDelete()
{
	const QStringList names = selectedClipNames();
	if ( names.isEmpty() ) {
		say( tr( "Select a loaded animation to remove; a sequence inside the NIF is removed in the Blocks tab." ), true );
		return;
	}
	if ( !glView )
		return;
	for ( const QString & n : names )
		WwHkxAnimHub::instance()->unload( glView, n );
	say( names.count() == 1 ? tr( "Removed %1 from the list." ).arg( names.first() )
							: tr( "Removed %1 animations from the list." ).arg( names.count() ), false );
}

void AnimWorkspace::listCopy()
{
	const QStringList names = selectedClipNames();
	listClipboard.clear();
	for ( const QString & n : names ) {
		const HkxClipEntry * e = glView ? WwHkxAnimHub::instance()->clipEntry( glView, n ) : nullptr;
		if ( !e )
			continue;
		ClipCopy c;
		c.name = e->name;
		c.path = e->path;
		c.skeletonSource = e->skeletonSource;
		c.clip = e->clip;
		c.trackBone = e->trackBone;
		c.additive = e->additive;
		listClipboard.append( c );
	}
	if ( listClipboard.isEmpty() ) {
		say( tr( "Select a loaded animation to copy; a sequence inside the NIF is copied in the Blocks tab." ), true );
		return;
	}
	say( listClipboard.count() == 1 ? tr( "Copied %1." ).arg( listClipboard.first().name )
									: tr( "Copied %1 animations." ).arg( listClipboard.count() ), false );
}

void AnimWorkspace::listCut()
{
	listCopy();
	if ( !listClipboard.isEmpty() )
		listDelete();
}

void AnimWorkspace::listPaste()
{
	if ( listClipboard.isEmpty() ) {
		say( tr( "Nothing has been copied yet." ), true );
		return;
	}
	if ( !glView )
		return;
	// after the selected row, like every list that pastes
	int at = WwHkxAnimHub::instance()->clipIndex( glView, curIsClip ? curEntry : QString() );
	at = at < 0 ? clipEntries.count() : at + 1;
	QString last;
	for ( const ClipCopy & c : listClipboard ) {
		HkxClipEntry e;
		e.name = c.name;
		e.path = c.path;
		e.skeletonSource = c.skeletonSource;
		e.clip = c.clip;
		e.trackBone = c.trackBone;
		e.additive = c.additive;
		QString made;
		const QString why = WwHkxAnimHub::instance()->pasteEntry( glView, e, at, &made );
		if ( !why.isEmpty() ) {
			say( why, true );
			return;
		}
		last = made;
		at++;
	}
	refresh();
	if ( !last.isEmpty() )
		selectListRow( last );
	say( listClipboard.count() == 1 ? tr( "Pasted %1." ).arg( last )
									: tr( "Pasted %1 animations." ).arg( listClipboard.count() ), false );
}

void AnimWorkspace::listDuplicate()
{
	const QStringList names = selectedClipNames();
	if ( names.isEmpty() ) {
		say( tr( "Select a loaded animation to duplicate; a sequence inside the NIF is copied in the Blocks tab." ), true );
		return;
	}
	if ( !glView )
		return;
	QString last;
	for ( const QString & n : names ) {
		QString made;
		const QString why = WwHkxAnimHub::instance()->duplicateEntry( glView, n, &made );
		if ( !why.isEmpty() ) {
			say( why, true );
			return;
		}
		last = made;
	}
	refresh();
	if ( !last.isEmpty() )
		selectListRow( last );
	say( names.count() == 1 ? tr( "%1 duplicated as %2." ).arg( names.first(), last )
							: tr( "Duplicated %1 animations." ).arg( names.count() ), false );
}

void AnimWorkspace::listSelectAll()
{
	list->selectAll();
	say( tr( "%1 rows selected." ).arg( list->selectedItems().count() ), false );
}

void AnimWorkspace::listMoveUp()
{
	listMoveBy( -1 );
}

void AnimWorkspace::listMoveDown()
{
	listMoveBy( 1 );
}

void AnimWorkspace::listMoveBy( int delta )
{
	const QStringList sel = selectedClipNames();
	if ( sel.isEmpty() || delta == 0 ) {
		say( tr( "Select a loaded animation to move; the NIF's own sequences keep the order of their blocks." ), true );
		return;
	}
	if ( !glView )
		return;
	QStringList order = clipEntries;
	// sweep from the edge the rows are moving towards, so a block of selected
	// rows keeps its own order and nobody jumps over a neighbour twice
	QStringList sweep = sel;
	if ( delta > 0 )
		std::reverse( sweep.begin(), sweep.end() );
	int moved = 0;
	for ( const QString & n : sweep ) {
		const int i = order.indexOf( n );
		const int j = i + delta;
		if ( i < 0 || j < 0 || j >= order.count() )
			continue;
		order.move( i, j );
		moved++;
	}
	if ( moved == 0 ) {
		say( delta < 0 ? tr( "Already at the top of the list." ) : tr( "Already at the bottom of the list." ), true );
		return;
	}
	WwHkxAnimHub::instance()->setEntryOrder( glView, order );
	refresh();
	for ( const QString & n : sel )
		for ( int i = 0; i < list->count(); i++ )
			if ( list->item( i )->data( Qt::UserRole ).toString() == n )
				list->item( i )->setSelected( true );
	say( delta < 0 ? tr( "Moved %1 up." ).arg( moved ) : tr( "Moved %1 down." ).arg( moved ), false );
}

void AnimWorkspace::listRename()
{
	QListWidgetItem * it = list->currentItem();
	if ( !it ) {
		say( tr( "Select an animation to rename." ), true );
		return;
	}
	if ( !it->data( Qt::UserRole + 1 ).toBool() ) {
		say( tr( "A sequence inside the NIF is renamed in the Blocks tab, where its name lives." ), true );
		return;
	}
	renameItem = it;
	renameFrom = it->data( Qt::UserRole ).toString();
	syncing = true;
	it->setText( renameFrom );		// the row's label carries the length; the name alone is edited
	it->setFlags( it->flags() | Qt::ItemIsEditable );
	syncing = false;
	list->editItem( it );
}

void AnimWorkspace::listItemDoubleClicked( QListWidgetItem * item )
{
	if ( !item )
		return;
	list->setCurrentItem( item );
	listRename();
}

void AnimWorkspace::listItemChanged( QListWidgetItem * item )
{
	if ( syncing || !item || item != renameItem )
		return;
	const QString from = renameFrom;
	const QString to = item->text().trimmed();
	renameItem = nullptr;
	item->setFlags( item->flags() & ~Qt::ItemIsEditable );
	if ( to.isEmpty() || to == from ) {
		refreshLater();
		return;
	}
	/* The edited document is keyed by the entry name, so it is carried over by
	   hand: the hub's clipsChanged drops documents whose entry is gone, and a
	   rename looks exactly like that from there. */
	HkxClipDocument * d = docs.take( from );
	const QString why = glView ? WwHkxAnimHub::instance()->renameEntry( glView, from, to )
							   : tr( "There is no scene." );
	if ( why.isEmpty() ) {
		if ( d )
			docs.insert( to, d );
		if ( curEntry == from )
			curEntry = to;
		say( tr( "%1 is now %2." ).arg( from, to ), false );
	} else {
		if ( d )
			docs.insert( from, d );
		say( why, true );
	}
	refreshLater();
}

void AnimWorkspace::commitListOrder()
{
	if ( syncing || !glView )
		return;
	QStringList order;
	bool clipSeen = false, seqUnderClip = false;
	for ( int i = 0; i < list->count(); i++ ) {
		const QListWidgetItem * it = list->item( i );
		if ( it->data( Qt::UserRole + 1 ).toBool() ) {
			order << it->data( Qt::UserRole ).toString();
			clipSeen = true;
		} else if ( clipSeen ) {
			seqUnderClip = true;
		}
	}
	if ( order.isEmpty() )
		return;
	const bool moved = WwHkxAnimHub::instance()->setEntryOrder( glView, order );
	refresh();
	if ( seqUnderClip )
		say( tr( "The NIF's own sequences stay above the loaded animations; the row moved to the top of the loaded ones." ), false );
	else if ( moved )
		say( tr( "The animations are in a new order." ), false );
}

bool AnimWorkspace::eventFilter( QObject * watched, QEvent * event )
{
	/* The drop is Qt's to carry out (it draws the drop line and moves the
	   rows); what the order MEANS is ours, so the push to the hub is queued to
	   run the moment the view is done moving rows. */
	if ( list && watched == list->viewport() && event->type() == QEvent::Drop )
		QTimer::singleShot( 0, this, &AnimWorkspace::commitListOrder );
	return QWidget::eventFilter( watched, event );
}

void AnimWorkspace::listContextMenu( const QPoint & pos )
{
	QListWidgetItem * under = list->itemAt( pos );
	if ( under && !under->isSelected() )
		list->setCurrentItem( under );
	const bool onClip = under && under->data( Qt::UserRole + 1 ).toBool();
	const int clips = selectedClipNames().count();

	QMenu menu( list );
	menu.setObjectName( QStringLiteral( "AnimWsListMenu" ) );
	auto add = [&menu]( const QString & text, const QKeySequence & key, bool on, AnimWorkspace * ws, void ( AnimWorkspace::*fn )() ) {
		QAction * a = menu.addAction( text, ws, fn );
		a->setShortcut( key );
		a->setEnabled( on );
		return a;
	};
	add( tr( "Rename" ), QKeySequence( Qt::Key_F2 ), onClip, this, &AnimWorkspace::listRename );
	add( tr( "Duplicate" ), QKeySequence( Qt::SHIFT | Qt::Key_D ), clips > 0, this, &AnimWorkspace::listDuplicate );
	add( tr( "Copy" ), QKeySequence::Copy, clips > 0, this, &AnimWorkspace::listCopy );
	add( tr( "Cut" ), QKeySequence::Cut, clips > 0, this, &AnimWorkspace::listCut );
	add( tr( "Paste" ), QKeySequence::Paste, !listClipboard.isEmpty(), this, &AnimWorkspace::listPaste );
	menu.addSeparator();
	add( tr( "Move up" ), QKeySequence( Qt::CTRL | Qt::Key_Up ), clips > 0, this, &AnimWorkspace::listMoveUp );
	add( tr( "Move down" ), QKeySequence( Qt::CTRL | Qt::Key_Down ), clips > 0, this, &AnimWorkspace::listMoveDown );
	menu.addSeparator();
	add( tr( "Delete" ), QKeySequence( Qt::Key_Delete ), clips > 0, this, &AnimWorkspace::listDelete );
	add( tr( "Select all" ), QKeySequence::SelectAll, list->count() > 0, this, &AnimWorkspace::listSelectAll );
	menu.exec( list->viewport()->mapToGlobal( pos ) );
}

void AnimWorkspace::listRowChosen()
{
	if ( syncing )
		return;
	QListWidgetItem * it = list->currentItem();
	if ( !it )
		return;
	selectEntry( it->data( Qt::UserRole ).toString(), it->data( Qt::UserRole + 1 ).toBool(), true );
}

void AnimWorkspace::selectEntry( const QString & name, bool isClip, bool drive )
{
	if ( poseCheck && poseCheck->isChecked() )
		poseCheck->setChecked( false );
	curEntry = name;
	curIsClip = isClip;
	curSeq = QPersistentModelIndex();
	if ( !isClip && nif ) {
		for ( const QPersistentModelIndex & s : sequences ) {
			if ( nif->get<QString>( QModelIndex( s ), "Name" ) == name ) {
				curSeq = s;
				break;
			}
		}
	}
	if ( drive ) {
		if ( isClip && glView ) {
			WwHkxAnimHub::instance()->activate( glView, name );	// measures whether it bound
		} else {
			emit sequenceActivated( name );
		}
	}
	rebuildRows();
	refreshSummary();
	updateReadout();
}

void AnimWorkspace::setSequenceByName( const QString & name )
{
	if ( syncing )
		return;
	syncing = true;
	bool found = false;
	for ( int i = 0; i < list->count(); i++ ) {
		QListWidgetItem * it = list->item( i );
		if ( it->data( Qt::UserRole ).toString() == name ) {
			/* THE DRIVEN RE-ENTRY (lane UINOTES2, 2026-09-12). This is reached
			   from the VIEWPORT: something activated a clip and the list is
			   being told about it. Qt's one-argument setCurrentItem carries
			   ClearAndSelect, so when the activation came from the list's own
			   selection -- Ctrl+A selects every row, which activates the
			   current one -- this call threw every other selected row away and
			   Ctrl+A ended with one row selected. If the row is ALREADY part of
			   the selection, move the current row and leave the selection
			   alone; if it is not, select it as before, because then the
			   viewport is showing something the list is not and the row has to
			   become visible.

			   What would refute it: a user with a multi-selection who expects
			   the list to collapse to the one clip the viewport switched to.
			   That trade is deliberate -- a selection the user made outranks
			   the viewport's echo of it. */
			list->setCurrentItem( it, it->isSelected() ? QItemSelectionModel::NoUpdate
													 : QItemSelectionModel::ClearAndSelect );
			syncing = false;
			selectEntry( name, it->data( Qt::UserRole + 1 ).toBool(), false );
			syncing = true;
			found = true;
			break;
		}
	}
	if ( !found ) {
		list->setCurrentItem( nullptr );
		curEntry.clear();
		curIsClip = false;
		curSeq = QPersistentModelIndex();
		rebuildRows();
		refreshSummary();
	}
	syncing = false;
}

HkxPlayback * AnimWorkspace::playback() const
{
	if ( !glView )
		return nullptr;
	Scene * sc = glView->getScene();
	return sc ? sc->hkx : nullptr;
}

QStringList AnimWorkspace::trackNamesOf( const QString & entry ) const
{
	if ( const HkxPlayback * pb = playback() ) {
		if ( const HkxClipEntry * e = pb->find( entry ) )
			return e->trackBone;
	}
	return QStringList();
}

HkxClipDocument * AnimWorkspace::docFor( const QString & entry, bool create )
{
	auto it = docs.find( entry );
	if ( it != docs.end() )
		return it.value();
	if ( !create )
		return nullptr;
	HkxPlayback * pb = playback();
	const HkxClipEntry * e = pb ? pb->find( entry ) : nullptr;
	if ( !e )
		return nullptr;
	auto * d = new HkxClipDocument( HkxClipDocument::fromClip( e->clip, e->trackBone ) );
	d->sourcePath = e->path;
	docs.insert( entry, d );
	return d;
}

const HkxClipDocument * AnimWorkspace::document() const
{
	if ( !curIsClip || curEntry.isEmpty() )
		return nullptr;
	auto it = docs.constFind( curEntry );
	return it != docs.constEnd() ? it.value() : nullptr;
}

void AnimWorkspace::rebuildRows()
{
	QVector<AnimWsRow> rows;
	AnimWsRow markers;
	markers.kind = AnimWsRow::Markers;
	markers.label = tr( "Annotations" );
	rows.append( markers );

	if ( curIsClip ) {
		HkxClipDocument * d = docFor( curEntry, true );
		sheet->setDocument( d );
		/* setDocument starts the sheet clean -- no selection, the whole clip in
		   range. The play range is the DOCUMENT'S, so it goes straight back in
		   (ruling 9), or an edit would silently widen it. */
		if ( d )
			sheet->setRange( d->rangeFirstFrame(), d->rangeLastFrame() );
		if ( d ) {
			// NIF node by name (case-insensitive), and each node's parent
			QHash<QString, int> byLower;
			if ( nif ) {
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					const QModelIndex iB = nif->getBlockIndex( b );
					if ( !nif->blockInherits( iB, QStringLiteral( "NiAVObject" ) ) )
						continue;
					const QString nm = nif->get<QString>( iB, "Name" ).toLower();
					if ( !nm.isEmpty() && !byLower.contains( nm ) )
						byLower.insert( nm, b );
				}
			}
			QVector<int> trackBlock( d->numTracks(), -1 );
			QHash<int, int> blockTrack;
			for ( int t = 0; t < d->numTracks(); t++ ) {
				const int b = byLower.value( d->trackNames.value( t ).toLower(), -1 );
				if ( b >= 0 && !blockTrack.contains( b ) ) {
					trackBlock[t] = b;
					blockTrack.insert( b, t );
				}
			}
			// parent track = the nearest ancestor node that has a track
			QVector<int> parentTrack( d->numTracks(), -1 );
			for ( int t = 0; t < d->numTracks(); t++ ) {
				if ( trackBlock.at( t ) < 0 || !nif )
					continue;
				int p = nif->getParent( trackBlock.at( t ) );
				int guard = 0;
				while ( p >= 0 && guard++ < 512 ) {
					if ( blockTrack.contains( p ) ) {
						parentTrack[t] = blockTrack.value( p );
						break;
					}
					p = nif->getParent( p );
				}
			}
			QVector<QVector<int>> children( d->numTracks() );
			QVector<int> roots;
			for ( int t = 0; t < d->numTracks(); t++ ) {
				if ( trackBlock.at( t ) < 0 )
					continue;
				if ( parentTrack.at( t ) >= 0 )
					children[parentTrack.at( t )].append( t );
				else
					roots.append( t );
			}
			// depth-first, the NIF's order
			QVector<int> stack;
			QHash<int, int> rowOfTrack;
			std::function<void( int, int, int )> emitRow = [&]( int t, int depth, int parentRow ) {
				AnimWsRow r;
				r.kind = AnimWsRow::Bone;
				r.track = t;
				r.nodeBlock = trackBlock.at( t );
				r.depth = depth;
				r.parentRow = parentRow;
				r.label = d->trackNames.value( t );
				r.hasChildren = !children.at( t ).isEmpty();
				r.tooltip = tr( "track %1 -> NiNode block %2, %3 keys" ).arg( t ).arg( r.nodeBlock ).arg( d->keyCount( t ) );
				rows.append( r );
				const int myRow = rows.count() - 1;
				rowOfTrack.insert( t, myRow );
				for ( int c : children.at( t ) )
					emitRow( c, depth + 1, myRow );
			};
			for ( int t : roots )
				emitRow( t, 0, -1 );
			// unbound tracks under one folded group
			QVector<int> unbound;
			for ( int t = 0; t < d->numTracks(); t++ )
				if ( trackBlock.at( t ) < 0 )
					unbound.append( t );
			if ( !unbound.isEmpty() ) {
				AnimWsRow g;
				g.kind = AnimWsRow::Group;
				g.label = tr( "%1 track(s) with no node in this NIF" ).arg( unbound.count() );
				g.hasChildren = true;
				g.collapsed = true;
				rows.append( g );
				const int gRow = rows.count() - 1;
				for ( int t : unbound ) {
					AnimWsRow r;
					r.kind = AnimWsRow::Unbound;
					r.track = t;
					r.depth = 1;
					r.parentRow = gRow;
					r.label = d->trackNames.value( t ).isEmpty() ? tr( "track %1 (unnamed)" ).arg( t ) : d->trackNames.value( t );
					r.tooltip = tr( "track %1 has no node of that name in the open NIF; it is kept and written" ).arg( t );
					rows.append( r );
				}
			}
			for ( int f = 0; f < d->floatTracks.count(); f++ ) {
				AnimWsRow r;
				r.kind = AnimWsRow::Float;
				r.floatTrack = f;
				r.label = tr( "float: %1" ).arg( d->floatTracks.at( f ).name );
				r.tooltip = tr( "float track %1, %2 keys" ).arg( f ).arg( d->floatTracks.at( f ).keys.count() );
				rows.append( r );
			}
			// the root-motion track combo follows the document
			syncing = true;
			rootTrackBox->clear();
			for ( int t = 0; t < d->numTracks(); t++ )
				rootTrackBox->addItem( d->trackNames.value( t ).isEmpty() ? tr( "track %1" ).arg( t ) : d->trackNames.value( t ), t );
			const int com = d->findTrack( QStringLiteral( "COM" ) );
			if ( d->rootMotionBaked() )
				rootTrackBox->setCurrentIndex( d->rootMotionTrack() );
			else if ( com >= 0 )
				rootTrackBox->setCurrentIndex( com );
			// the play range: the boxes, the sheet's darkened zone and its two
			// grips all read the ONE pair of numbers on the document
			rangeStartBox->setMaximum( std::max( 0, d->numFrames() - 1 ) );
			rangeEndBox->setMaximum( std::max( 0, d->numFrames() - 1 ) );
			rangeStartBox->setValue( d->rangeFirstFrame() );
			rangeEndBox->setValue( d->rangeLastFrame() );
			sheet->setRange( d->rangeFirstFrame(), d->rangeLastFrame() );
			pushRangeToScene();
			// the vocabulary: the clip's own names first, then the archive's
			const QString typed = annotBox->currentText();
			annotBox->clear();
			QStringList own;
			for ( const QVector<HkxAnnotation> & l : d->clip.annotations )
				for ( const HkxAnnotation & a : l )
					if ( !a.text.isEmpty() && !own.contains( a.text ) )
						own.append( a.text );
			annotBox->addItems( own );
			for ( const QString & v : vocab )
				if ( !own.contains( v ) )
					annotBox->addItem( v );
			annotBox->setEditText( typed.isEmpty() ? ( own.isEmpty() ? vocab.value( 0 ) : own.first() ) : typed );
			syncing = false;
		}
		seqSection->setVisible( false );
		editSection->setVisible( true );
		updateSections();
	} else {
		sheet->setDocument( nullptr );
		if ( curSeq.isValid() && nif )
			buildNifRows( QModelIndex( curSeq ) );
		int frames = 1;
		if ( glView && glView->getScene() )
			frames = std::max( 1, int( std::lround( ( sceneMax - sceneMin ) * 30.0f ) ) + 1 );
		sheet->setRuler( 30.0f, frames );
		// the sequence rows
		syncing = true;
		if ( curSeq.isValid() && nif ) {
			const QModelIndex iSeq( curSeq );
			cycleBox->setCurrentIndex( std::max( 0, std::min( 2, nif->get<int>( iSeq, "Cycle Type" ) ) ) );
			freqBox->setValue( double( nif->get<float>( iSeq, "Frequency" ) ) );
			startBox->setValue( double( nif->get<float>( iSeq, "Start Time" ) ) );
			stopBox->setValue( double( nif->get<float>( iSeq, "Stop Time" ) ) );
		}
		syncing = false;
		seqSection->setVisible( curSeq.isValid() );
		editSection->setVisible( false );
		updateSections();
	}
	// rows built for a NIF sequence come out of buildNifRows through `rows`? no:
	// buildNifRows sets the sheet's rows itself; a clip's rows are set here.
	if ( curIsClip || !curSeq.isValid() )
		sheet->setRows( rows );
	rateLabel->setText( tr( "%1 fps" ).arg( sheetFps() ) );
}

void AnimWorkspace::buildNifRows( const QModelIndex & iSeq )
{
	// read-only: the interpolators' key times at 30 fps (editing NIF keys stays
	// in the Blocks tab until the follow-up)
	QVector<AnimWsRow> rows;
	AnimWsRow markers;
	markers.kind = AnimWsRow::Markers;
	markers.label = tr( "Text keys" );
	rows.append( markers );
	const QModelIndex iBlocks = nif->getIndex( iSeq, "Controlled Blocks" );
	auto collect = [&]( const QModelIndex & iKeys, QVector<int> & out ) {
		if ( !iKeys.isValid() )
			return;
		for ( int k = 0; k < nif->rowCount( iKeys ); k++ ) {
			const float t = nif->get<float>( nif->index( k, 0, iKeys ), "Time" );
			const int f = int( std::lround( t * 30.0f ) );
			if ( !out.contains( f ) )
				out.append( f );
		}
	};
	for ( int i = 0; iBlocks.isValid() && i < nif->rowCount( iBlocks ); i++ ) {
		const QModelIndex iCB = nif->index( i, 0, iBlocks );
		AnimWsRow r;
		r.kind = AnimWsRow::NifTrack;
		r.label = nif->get<QString>( iCB, "Node Name" );
		if ( r.label.isEmpty() )
			r.label = tr( "controlled block %1" ).arg( i );
		const QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iCB, "Interpolator" ) );
		if ( iInterp.isValid() ) {
			const QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
			if ( iData.isValid() ) {
				collect( nif->getIndex( iData, "Quaternion Keys" ), r.nifKeyFrames );
				collect( nif->getIndex( nif->getIndex( iData, "Translations" ), "Keys" ), r.nifKeyFrames );
				collect( nif->getIndex( nif->getIndex( iData, "Scales" ), "Keys" ), r.nifKeyFrames );
				collect( nif->getIndex( nif->getIndex( iData, "Data" ), "Keys" ), r.nifKeyFrames );
				const QModelIndex iXYZ = nif->getIndex( iData, "XYZ Rotations" );
				for ( int a = 0; iXYZ.isValid() && a < nif->rowCount( iXYZ ); a++ )
					collect( nif->getIndex( nif->index( a, 0, iXYZ ), "Keys" ), r.nifKeyFrames );
			}
		}
		std::sort( r.nifKeyFrames.begin(), r.nifKeyFrames.end() );
		// the node in the viewport
		if ( !r.label.isEmpty() ) {
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				const QModelIndex iB = nif->getBlockIndex( b );
				if ( nif->blockInherits( iB, QStringLiteral( "NiAVObject" ) ) && nif->get<QString>( iB, "Name" ) == r.label ) {
					r.nodeBlock = b;
					break;
				}
			}
		}
		r.tooltip = tr( "controlled block %1, %2 key time(s), read-only here" ).arg( i ).arg( r.nifKeyFrames.count() );
		rows.append( r );
	}
	sheet->setRows( rows );
}

float AnimWorkspace::sheetFps() const
{
	const HkxClipDocument * d = document();
	return d && d->fps() > 0.0f ? d->fps() : 30.0f;
}

int AnimWorkspace::frameOf( float t ) const
{
	const float fd = 1.0f / sheetFps();
	const float f = t / fd;
	const int n = int( std::lround( f ) );
	if ( std::fabs( f - float( n ) ) < WW_FRAME_EPS )
		return n;
	return int( std::floor( f + WW_FRAME_EPS ) );
}

float AnimWorkspace::timeOf( int frame ) const
{
	return float( frame ) / sheetFps();
}

int AnimWorkspace::currentFrame() const
{
	return sheet ? sheet->currentFrame() : 0;
}


/*
 *  Time and playing state
 */

void AnimWorkspace::setTime( float t, float mn, float mx )
{
	curTime = t;
	sceneMin = mn;
	sceneMax = mx;
	sheet->setCurrentFrame( frameOf( t ) );
	updateReadout();
}

void AnimWorkspace::updateReadout()
{
	const int f = frameOf( curTime );
	syncing = true;
	frameBox->setValue( f );
	syncing = false;
	const HkxClipDocument * d = document();
	if ( d ) {
		readout->setText( tr( "frame %1 / %2 · %3 s" ).arg( f ).arg( std::max( 0, d->numFrames() - 1 ) ).arg( curTime, 0, 'f', 3 ) );
	} else {
		readout->setText( tr( "frame %1 · %2 s" ).arg( f ).arg( curTime, 0, 'f', 3 ) );
	}
	if ( !curIsClip ) {
		const int frames = std::max( 1, int( std::lround( ( sceneMax - sceneMin ) * 30.0f ) ) + 1 );
		if ( frames != sheet->numFrames() )
			sheet->setRuler( 30.0f, frames );
	}
}

void AnimWorkspace::setPlayingState( bool playing, bool reverse )
{
	/* RULING 4 / Blender: the play button IS the pause button while it plays,
	   so the glyph says what pressing it will do. The drawing is only redone
	   when the state actually changes -- this runs on every refresh. */
	const bool nowPlay = playing && !reverse, nowBack = playing && reverse;
	if ( nowPlay != btnPlay->isChecked() )
		btnPlay->setIcon( wwTransportIcon( nowPlay ? GlyphPause : GlyphPlay, WW_TRANSPORT_ICON_PX ) );
	if ( nowBack != btnPlayBack->isChecked() )
		btnPlayBack->setIcon( wwTransportIcon( nowBack ? GlyphPause : GlyphPlayBack, WW_TRANSPORT_ICON_PX ) );
	btnPlay->setChecked( nowPlay );
	btnPlayBack->setChecked( nowBack );
}

void AnimWorkspace::sheetFrameScrubbed( int frame )
{
	emit timeChanged( timeOf( frame ) );
}


/*
 *  Selection, both ways
 */

void AnimWorkspace::sheetRowSelected( int row )
{
	const QVector<AnimWsRow> & rows = sheet->rows();
	if ( row < 0 || row >= rows.count() )
		return;
	const AnimWsRow & r = rows.at( row );
	if ( r.nodeBlock >= 0 && nif ) {
		const QModelIndex iNode = nif->getBlockIndex( r.nodeBlock );
		if ( iNode.isValid() )
			emit indexSelected( iNode );
	}
	if ( poseCheck->isChecked() )
		holdSelectedBone( true );
	refreshSummary();
}

void AnimWorkspace::setCurrentIndex( const QModelIndex & idx )
{
	if ( !nif || !idx.isValid() )
		return;
	const int block = nif->getBlockNumber( idx );
	const int row = sheet->rowOfNodeBlock( block );
	if ( row >= 0 && row != sheet->currentRow() ) {
		sheet->selectRow( row, true );
		if ( poseCheck->isChecked() )
			holdSelectedBone( true );
		refreshSummary();
	}
}

int AnimWorkspace::selectedTrack() const
{
	const int row = sheet->currentRow();
	if ( row < 0 || row >= sheet->rows().count() )
		return -1;
	const AnimWsRow & r = sheet->rows().at( row );
	return ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) ? r.track : -1;
}

int AnimWorkspace::selectedNodeBlock() const
{
	const int row = sheet->currentRow();
	if ( row < 0 || row >= sheet->rows().count() )
		return -1;
	return sheet->rows().at( row ).nodeBlock;
}

int AnimWorkspace::boneRowCount() const
{
	int n = 0;
	for ( const AnimWsRow & r : sheet->rows() )
		if ( r.kind == AnimWsRow::Bone ) n++;
	return n;
}

int AnimWorkspace::unboundRowCount() const
{
	int n = 0;
	for ( const AnimWsRow & r : sheet->rows() )
		if ( r.kind == AnimWsRow::Unbound ) n++;
	return n;
}

int AnimWorkspace::floatRowCount() const
{
	int n = 0;
	for ( const AnimWsRow & r : sheet->rows() )
		if ( r.kind == AnimWsRow::Float ) n++;
	return n;
}

QString AnimWorkspace::noteText() const
{
	return note ? note->text() : QString();
}

QString AnimWorkspace::noteDetail() const
{
	return note ? note->toolTip() : QString();
}

QStringList AnimWorkspace::vocabulary() const
{
	QStringList v;
	for ( int i = 0; i < annotBox->count(); i++ )
		v.append( annotBox->itemText( i ) );
	return v;
}

void AnimWorkspace::loadVocabulary()
{
	vocab.clear();
	QStringList paths;
	const QByteArray env = qgetenv( "WW_HKX_VOCAB" );
	if ( !env.isEmpty() )
		paths.append( QString::fromLocal8Bit( env ) );
	paths.append( QCoreApplication::applicationDirPath() + QStringLiteral( "/hkx_annotation_vocabulary.txt" ) );
	paths.append( QCoreApplication::applicationDirPath() + QStringLiteral( "/../res/hkx_annotation_vocabulary.txt" ) );
	for ( const QString & p : paths ) {
		QFile f( p );
		if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
			continue;
		QTextStream in( &f );
		while ( !in.atEnd() ) {
			const QString line = in.readLine();
			if ( line.startsWith( QLatin1Char( '#' ) ) )
				continue;
			const int tab = line.indexOf( QLatin1Char( '\t' ) );
			const QString name = ( tab >= 0 ? line.mid( tab + 1 ) : line ).trimmed();
			if ( !name.isEmpty() && vocab.count() < 400 )
				vocab.append( name );
		}
		break;
	}
}


/*
 *  The summary line and the buttons' enabled state
 */

void AnimWorkspace::say( const QString & text, bool refusal, const QString & detail )
{
	noteRefusal = refusal;
	note->setText( text );
	/* NO BLURBS IN THE DOCK (bungo's standing rule, and 2026-09-10 21:1x over
	 * this very line: "look at all this text clutter"). The label says the few
	 * words; everything the sentence carried is one hover away. */
	note->setToolTip( detail.isEmpty() ? text : detail );
	note->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( refusal ? "danger" : "textMuted" ) ) );
}

void AnimWorkspace::refreshSummary()
{
	const HkxClipDocument * d = document();
	const bool clip = ( d != nullptr );
	const int track = selectedTrack();
	const bool haveSel = !sheet->selectedKeys().isEmpty() || sheet->selectedMarker().valid();
	const int row = sheet->currentRow();
	const bool floatRow = row >= 0 && row < sheet->rows().count() && sheet->rows().at( row ).kind == AnimWsRow::Float;
	actInsertKey->setEnabled( clip && track >= 0 && selectedNodeBlock() >= 0 );
	actDeleteKeys->setEnabled( clip && haveSel );
	actReduce->setEnabled( clip );
	actAddAnnot->setEnabled( clip );
	actRenameAnnot->setEnabled( clip && sheet->selectedMarker().valid() );
	actDeleteAnnot->setEnabled( clip && sheet->selectedMarker().valid() );
	actTrim->setEnabled( clip );
	actRetime->setEnabled( clip );
	actBake->setEnabled( clip && !d->rootMotionBaked() );
	actUnbake->setEnabled( clip && d->rootMotionBaked() );
	actRemoveTrack->setEnabled( clip && track >= 0 );
	actRenameTrack->setEnabled( clip && track >= 0 );
	actRemoveAxes->setEnabled( clip && track >= 0 );
	actAddFloat->setEnabled( clip );
	actSetFloat->setEnabled( clip && floatRow );
	actSave->setEnabled( clip && !d->sourcePath.isEmpty() );
	actSaveAs->setEnabled( clip );
	// and the panel shows the one section this selection is about (ruling 6)
	updateSections();
	btnUnloadAnim->setEnabled( curIsClip );
	btnRootMotion->setEnabled( glView != nullptr );
	if ( noteRefusal && !note->text().isEmpty() )
		return;	// a refusal stays until the next action
	/* EVERY BRANCH: a few words in the label, the whole sentence in the
	 * tooltip (lane UI6, 2026-09-11, bungo's "look at all this text clutter").
	 * The long strings are unchanged -- they are still written, still exact,
	 * and still reachable; only where they are SHOWN has moved. */
	if ( clip ) {
		QString s = d->summary();
		if ( track >= 0 )
			s += tr( "  Selected: %1 (%2 keys)%3." ).arg( d->trackNames.value( track ).isEmpty() ? tr( "track %1" ).arg( track ) : d->trackNames.value( track ) )
				.arg( d->keyCount( track ) ).arg( selectedNodeBlock() < 0 ? tr( ", no node in this NIF" ) : QString() );
		if ( !sheet->selectedKeys().isEmpty() )
			s += tr( "  %1 key(s) selected." ).arg( sheet->selectedKeys().count() );
		QString brief = tr( "%1 frames @ %2 fps" ).arg( d->numFrames() ).arg( d->fps() );
		if ( track >= 0 )
			brief += tr( ", %1 keys" ).arg( d->keyCount( track ) );
		say( brief, false, s );
	} else if ( curSeq.isValid() && nif ) {
		const QModelIndex iSeq( curSeq );
		const QModelIndex iBlocks = nif->getIndex( iSeq, "Controlled Blocks" );
		const int nBlocks = iBlocks.isValid() ? nif->rowCount( iBlocks ) : 0;
		say( tr( "%1 controlled block(s)" ).arg( nBlocks ), false,
			tr( "NiControllerSequence '%1': %2 controlled block(s), %3..%4 s, frequency %5. Its keys are shown read-only here (edit them in the Blocks tab)." )
			.arg( curEntry ).arg( nBlocks )
			.arg( nif->get<float>( iSeq, "Start Time" ) ).arg( nif->get<float>( iSeq, "Stop Time" ) ).arg( nif->get<float>( iSeq, "Frequency" ) ) );
	} else if ( curIsClip && !curEntry.isEmpty() ) {
		say( WwHkxAnimHub::instance()->sentenceShort(),
			WwHkxAnimHub::instance()->sentenceIsRefusal(),
			WwHkxAnimHub::instance()->sentence() );
	} else {
		say( tr( "Select an animation" ), false,
			tr( "Select an animation: one of the NIF's sequences, or Load… a .hkx clip (drop one on the window works too)." ) );
	}
}

void AnimWorkspace::rebuildUnloadMenu()
{
	unloadMenu->clear();
	if ( clipEntries.isEmpty() ) {
		QAction * a = unloadMenu->addAction( tr( "No animation loaded" ) );
		a->setEnabled( false );
		return;
	}
	for ( const QString & n : clipEntries ) {
		QAction * a = unloadMenu->addAction( tr( "Unload %1" ).arg( n ) );
		connect( a, &QAction::triggered, this, [this, n]() {
			if ( glView )
				WwHkxAnimHub::instance()->unload( glView, n );
		} );
	}
	unloadMenu->addSeparator();
	QAction * all = unloadMenu->addAction( tr( "Unload all" ) );
	connect( all, &QAction::triggered, this, [this]() {
		if ( glView )
			WwHkxAnimHub::instance()->unloadAll( glView );
	} );
}

void AnimWorkspace::hkxChooseFile()
{
	if ( !glView )
		return;
	const QString path = QFileDialog::getOpenFileName( this, tr( "Load animation" ), QString(),
		WwHkxAnimHub::fileDialogFilter() );
	if ( path.isEmpty() )
		return;
	WwHkxAnimHub::instance()->loadFiles( glView, { path }, true );
}


/*
 *  The NIF sequence rows (the old manager's controls)
 */

template <typename T>
static void wwPushTyped( NifModel * nif, const QModelIndex & iBlock, const char * field, const T & newVal )
{
	const QModelIndex iField = nif->getIndex( iBlock, field );
	if ( !iField.isValid() )
		return;
	const QModelIndex vIdx = iField.sibling( iField.row(), NifModel::ValueCol );
	const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
	if ( !item )
		return;
	NifValue oldVal = item->value();
	NifValue nv = oldVal;
	if ( nv.set( newVal, nif, item ) && nif->undoStack )
		nif->undoStack->push( new ChangeValueCommand( vIdx, oldVal, nv, nif->itemName( iField ), nif ) );
}

void AnimWorkspace::pushSequenceField( const QModelIndex & iSeq, const char * field, const QVariant & v )
{
	if ( !nif || !iSeq.isValid() )
		return;
	if ( v.typeId() == QMetaType::Int || v.typeId() == QMetaType::UInt )
		wwPushTyped<quint32>( nif, iSeq, field, v.toUInt() );
	else
		wwPushTyped<float>( nif, iSeq, field, v.toFloat() );
}

void AnimWorkspace::sequenceFieldEdited()
{
	if ( syncing || !nif || !curSeq.isValid() )
		return;
	const QModelIndex iSeq( curSeq );
	ChangeValueCommand::createTransaction();
	if ( nif->get<int>( iSeq, "Cycle Type" ) != cycleBox->currentIndex() )
		pushSequenceField( iSeq, "Cycle Type", quint32( cycleBox->currentIndex() ) );
	if ( nif->get<float>( iSeq, "Frequency" ) != float( freqBox->value() ) )
		pushSequenceField( iSeq, "Frequency", float( freqBox->value() ) );
	if ( nif->get<float>( iSeq, "Start Time" ) != float( startBox->value() ) )
		pushSequenceField( iSeq, "Start Time", float( startBox->value() ) );
	if ( nif->get<float>( iSeq, "Stop Time" ) != float( stopBox->value() ) )
		pushSequenceField( iSeq, "Stop Time", float( stopBox->value() ) );
	// the scene's own copy of the cycle and the range
	if ( glView && glView->getScene() ) {
		Scene * sc = glView->getScene();
		sc->animCycle.insert( curEntry, cycleBox->currentIndex() );
		QMap<QString, float> tags = sc->animTags.value( curEntry );
		tags.insert( QStringLiteral( "start" ), float( startBox->value() ) );
		tags.insert( QStringLiteral( "end" ), float( stopBox->value() ) );
		sc->animTags.insert( curEntry, tags );
		sc->transformDirty = true;
		glView->update();
	}
	refreshSummary();
}


/*
 *  Editing
 */

bool AnimWorkspace::edit( const QString & what, const std::function<HkxEditResult( HkxClipDocument & )> & op )
{
	if ( !curIsClip || curEntry.isEmpty() ) {
		say( tr( "Editing needs a loaded .hkx clip selected; a NIF sequence's keys are edited in the Blocks tab." ), true );
		return false;
	}
	HkxClipDocument * d = docFor( curEntry, true );
	if ( !d ) {
		say( tr( "%1 has no decoded clip to edit (it refused to load)." ).arg( curEntry ), true );
		return false;
	}
	HkxClipDocument after = *d;
	const HkxEditResult r = op( after );
	if ( !r.ok ) {
		say( r.message, true );
		return false;
	}
	undo->push( new AnimWsCommand( this, curEntry, *d, after, what, r.message ) );
	return true;
}

void AnimWorkspace::applyDocument( const QString & entry, const HkxClipDocument & d, const QString & what )
{
	HkxClipDocument * cur = docs.value( entry, nullptr );
	if ( !cur ) {
		cur = new HkxClipDocument( d );
		docs.insert( entry, cur );
	} else {
		*cur = d;
	}
	if ( HkxPlayback * pb = playback() ) {
		const QString err = pb->replaceClip( entry, cur->clip, cur->trackNames );
		if ( !err.isEmpty() )
			say( err, true );
	}
	if ( entry == curEntry && curIsClip ) {
		const int keepRow = sheet->currentRow();
		const int keepFrame = sheet->currentFrame();
		rebuildRows();
		sheet->selectRow( std::min( keepRow, int( sheet->rows().count() ) - 1 ), true );
		sheet->setCurrentFrame( keepFrame );
		// the transport's Start / End boxes are the document's range as well
		if ( rangeStartBox && rangeEndBox ) {
			rangeStartBox->setMaximum( std::max( 0, cur->numFrames() - 1 ) );
			rangeEndBox->setMaximum( std::max( 0, cur->numFrames() - 1 ) );
			rangeStartBox->setValue( cur->rangeFirstFrame() );
			rangeEndBox->setValue( cur->rangeLastFrame() );
		}
	}
	// the play range is what the viewport plays (ruling 9)
	pushRangeToScene();
	if ( glView ) {
		if ( Scene * sc = glView->getScene() )
			sc->transformDirty = true;
		glView->update();
	}
	noteRefusal = false;
	say( what, false );
	refreshSummary();
	// the summary shows the sentence of what happened
	say( what + QStringLiteral( "  " ) + ( document() ? document()->summary() : QString() ), false );
	updateReadout();
}

bool AnimWorkspace::readBlockTransform( int nodeBlock, HkxTransform & out ) const
{
	if ( !nif || nodeBlock < 0 )
		return false;
	const QModelIndex iNode = nif->getBlockIndex( nodeBlock );
	if ( !iNode.isValid() || !nif->blockInherits( iNode, QStringLiteral( "NiAVObject" ) ) )
		return false;
	out.translation = nif->get<Vector3>( iNode, "Translation" );
	out.rotation = nif->get<Matrix>( iNode, "Rotation" ).toQuat();
	const float s = nif->get<float>( iNode, "Scale" );
	out.scale = Vector3( s, s, s );
	return true;
}

void AnimWorkspace::writeBlockTransform( int nodeBlock, const HkxTransform & xf )
{
	if ( !nif || nodeBlock < 0 )
		return;
	const QModelIndex iNode = nif->getBlockIndex( nodeBlock );
	if ( !iNode.isValid() )
		return;
	Matrix m;
	m.fromQuat( xf.rotation );
	nif->set<Vector3>( iNode, "Translation", xf.translation );
	nif->set<Matrix>( iNode, "Rotation", m );
	nif->set<float>( iNode, "Scale", xf.scale[0] );
}

void AnimWorkspace::holdSelectedBone( bool on )
{
	HkxPlayback * pb = playback();
	// release the previous hold, putting its block back
	if ( heldBlock >= 0 ) {
		if ( pb )
			pb->setHeldNode( -1 );
		writeBlockTransform( heldBlock, heldBind );
		heldBlock = -1;
	}
	if ( on ) {
		const int block = selectedNodeBlock();
		if ( block >= 0 && readBlockTransform( block, heldBind ) ) {
			heldBlock = block;
			if ( pb )
				pb->setHeldNode( block );
		}
	}
	if ( glView ) {
		if ( Scene * sc = glView->getScene() )
			sc->transformDirty = true;
		glView->update();
	}
}

void AnimWorkspace::poseToggled( bool on )
{
	holdSelectedBone( on );
	if ( on && heldBlock < 0 )
		say( tr( "Pose needs a bone row with a node in this NIF selected." ), true );
	else if ( on )
		say( tr( "Posing %1 with the gizmo: G/R/S in the viewport, then Insert key." ).arg( nif ? nif->get<QString>( nif->getBlockIndex( heldBlock ), "Name" ) : QString() ), false );
}

void AnimWorkspace::insertKeyAtPlayhead()
{
	insertKeyAtFrame( currentFrame() );
}

void AnimWorkspace::insertKeyAtFrame( int frame )
{
	const int track = selectedTrack();
	const int block = selectedNodeBlock();
	if ( track < 0 ) {
		say( tr( "Select a bone row first." ), true );
		return;
	}
	HkxTransform xf;
	if ( !readBlockTransform( block, xf ) ) {
		say( tr( "The selected track has no node in this NIF, so there is no pose to key." ), true );
		return;
	}
	edit( tr( "Insert key at frame %1" ).arg( frame ), [track, frame, xf]( HkxClipDocument & d ) {
		return d.insertKey( track, frame, xf );
	} );
}

void AnimWorkspace::keyNodeTransform( int nodeBlock )
{
	if ( !curIsClip )
		return;
	const int row = sheet->rowOfNodeBlock( nodeBlock );
	if ( row < 0 )
		return;
	const AnimWsRow & r = sheet->rows().at( row );
	HkxTransform xf;
	if ( !readBlockTransform( nodeBlock, xf ) )
		return;
	const int frame = currentFrame();
	const int track = r.track;
	edit( tr( "Auto-key at frame %1" ).arg( frame ), [track, frame, xf]( HkxClipDocument & d ) {
		return d.insertKey( track, frame, xf );
	} );
}

void AnimWorkspace::deleteSelected()
{
	const QVector<HkxKeyRef> sel = sheet->selectedKeys();
	const AnimWsMarkerRef m = sheet->selectedMarker();
	if ( sel.isEmpty() && m.valid() ) {
		deleteSelectedAnnotation();
		return;
	}
	if ( sel.isEmpty() ) {
		say( tr( "Nothing is selected on the dope sheet." ), true );
		return;
	}
	QVector<HkxKeyRef> xf;
	QVector<HkxKeyRef> fl;
	for ( const HkxKeyRef & k : sel )
		( k.track >= 0 ? xf : fl ).append( k );
	edit( tr( "Delete %1 key(s)" ).arg( sel.count() ), [xf, fl]( HkxClipDocument & d ) {
		QString msg;
		if ( !xf.isEmpty() ) {
			const HkxEditResult r = d.deleteKeys( xf );
			if ( !r.ok )
				return r;
			msg = r.message;
		}
		for ( const HkxKeyRef & k : fl ) {
			const HkxEditResult r = d.deleteFloatKey( -k.track - 2, k.frame );
			if ( !r.ok )
				return r;
			msg += QStringLiteral( " " ) + r.message;
		}
		return HkxEditResult::done( msg );
	} );
	sheet->clearSelection();
}

void AnimWorkspace::reduceKeys()
{
	const float tT = float( tolTransBox->value() ), tR = float( tolDegBox->value() );
	edit( tr( "Reduce keys" ), [tT, tR]( HkxClipDocument & d ) {
		return d.reduce( tT, tR, 0.001f );
	} );
}

void AnimWorkspace::addAnnotationAtPlayhead()
{
	addAnnotationAtFrame( currentFrame(), annotBox->currentText().trimmed() );
}

void AnimWorkspace::addAnnotationAtFrame( int frame, const QString & name )
{
	const HkxClipDocument * d = document();
	if ( !d )
		return;
	const float time = d->timeOfFrame( frame );
	edit( tr( "Add annotation '%1' at frame %2" ).arg( name ).arg( frame ), [name, time]( HkxClipDocument & dd ) {
		return dd.addAnnotation( 0, time, name );
	} );
}

void AnimWorkspace::sheetMarkerNameEntered( int frame, const QString & name )
{
	/* The inline editor on a ghost marker (ruling 7). The annotation is added
	   HERE, on commit, so one gesture is one undoable command and Escape adds
	   nothing at all. An empty name is allowed -- Blender's markers can be
	   unnamed and the sheet draws "(unnamed)" -- but the Name box's text is a
	   better default than nothing, so it fills in. */
	QString n = name.trimmed();
	if ( n.isEmpty() && annotBox )
		n = annotBox->currentText().trimmed();
	addAnnotationAtFrame( frame, n );
	// the new marker becomes the active one, so 7b's orange lands on it
	if ( const HkxClipDocument * d = document() ) {
		int idx = -1;
		for ( int i = 0; i < d->clip.annotations.value( 0 ).count(); i++ )
			if ( d->frameOfTime( d->clip.annotations.value( 0 ).at( i ).time ) == frame )
				idx = i;
		if ( idx >= 0 )
			sheet->selectMarker( AnimWsMarkerRef{ 0, idx }, false );
	}
}

void AnimWorkspace::sheetMarkerRenamed( const AnimWsMarkerRef & marker, const QString & name )
{
	if ( !marker.valid() )
		return;
	const QString n = name.trimmed();
	edit( tr( "Rename annotation to '%1'" ).arg( n ), [marker, n]( HkxClipDocument & d ) {
		return d.renameAnnotation( marker.track, marker.index, n );
	} );
}

void AnimWorkspace::sheetMarkerAddRequested( int frame )
{
	sheet->beginNewMarker( frame, vocabulary() );
}

void AnimWorkspace::sheetMarkerRenameRequested( const AnimWsMarkerRef & marker )
{
	sheet->beginMarkerRename( marker, vocabulary() );
}

void AnimWorkspace::renameSelectedAnnotation()
{
	const AnimWsMarkerRef m = sheet->selectedMarker();
	if ( !m.valid() ) {
		say( tr( "Select an annotation on the marker row first." ), true );
		return;
	}
	const QString name = annotBox->currentText().trimmed();
	edit( tr( "Rename annotation" ), [m, name]( HkxClipDocument & d ) {
		return d.renameAnnotation( m.track, m.index, name );
	} );
}

void AnimWorkspace::deleteSelectedAnnotation()
{
	const AnimWsMarkerRef m = sheet->selectedMarker();
	if ( !m.valid() ) {
		say( tr( "Select an annotation on the marker row first." ), true );
		return;
	}
	edit( tr( "Delete annotation" ), [m]( HkxClipDocument & d ) {
		return d.deleteAnnotation( m.track, m.index );
	} );
	sheet->clearSelection();
}

void AnimWorkspace::sheetMarkerDragged( const AnimWsMarkerRef & marker, int frame )
{
	const HkxClipDocument * d = document();
	if ( !d )
		return;
	const float time = d->timeOfFrame( frame );
	edit( tr( "Move annotation to frame %1" ).arg( frame ), [marker, time]( HkxClipDocument & dd ) {
		return dd.moveAnnotation( marker.track, marker.index, time );
	} );
}

void AnimWorkspace::sheetMarkerActivated( const AnimWsMarkerRef & marker )
{
	/* Double-click on an annotation renames it WHERE IT IS (ruling 7: "opening
	   an inline name editor"), instead of the modal QInputDialog this used to
	   put up in the middle of the window. The commit comes back through
	   markerRenamed -> sheetMarkerRenamed. */
	sheet->beginMarkerRename( marker, vocabulary() );
}

void AnimWorkspace::sheetKeysDragged( const QVector<HkxKeyRef> & keys, int deltaFrames, bool copy )
{
	QVector<HkxKeyRef> xf;
	for ( const HkxKeyRef & k : keys )
		if ( k.track >= 0 )
			xf.append( k );
	if ( xf.isEmpty() )
		return;
	if ( edit( copy ? tr( "Copy %1 key(s) by %2" ).arg( xf.count() ).arg( deltaFrames ) : tr( "Move %1 key(s) by %2" ).arg( xf.count() ).arg( deltaFrames ),
			   [xf, deltaFrames, copy]( HkxClipDocument & d ) { return d.moveKeys( xf, deltaFrames, copy ); } ) ) {
		QVector<HkxKeyRef> moved;
		const HkxClipDocument * d = document();
		for ( const HkxKeyRef & k : xf ) {
			const int f = std::max( 0, std::min( d ? d->numFrames() - 1 : k.frame + deltaFrames, k.frame + deltaFrames ) );
			moved.append( HkxKeyRef{ k.track, f } );
		}
		sheet->selectKeys( moved );
	}
}

void AnimWorkspace::pushRangeToScene()
{
	const HkxClipDocument * d = document();
	if ( !d || !glView || !glView->getScene() || curEntry.isEmpty() || !curIsClip )
		return;
	Scene * sc = glView->getScene();
	QMap<QString, float> tags = sc->animTags.value( curEntry );
	tags.insert( QStringLiteral( "start" ), d->timeOfFrame( d->rangeFirstFrame() ) );
	tags.insert( QStringLiteral( "end" ), d->timeOfFrame( d->rangeLastFrame() ) );
	sc->animTags.insert( curEntry, tags );
	sc->transformDirty = true;
	glView->update();
}

void AnimWorkspace::trimToRange()
{
	const HkxClipDocument * d0 = document();
	if ( !d0 ) {
		say( tr( "No clip is selected." ), true );
		return;
	}
	const int a = d0->rangeFirstFrame(), b = d0->rangeLastFrame();
	if ( a == 0 && b == d0->numFrames() - 1 ) {
		say( tr( "The play range is the whole clip: there is nothing to cut. Drag the ruler's grips or edit Start / End first." ), true );
		return;
	}
	edit( tr( "Trim to %1..%2" ).arg( a ).arg( b ), [a, b]( HkxClipDocument & d ) { return d.trim( a, b ); } );
}

void AnimWorkspace::setRangeFromBoxes()
{
	if ( syncing || !document() )
		return;
	const int a = rangeStartBox->value(), b = rangeEndBox->value();
	if ( a == document()->rangeFirstFrame() && b == document()->rangeLastFrame() )
		return;
	edit( tr( "Play range %1..%2" ).arg( a ).arg( b ), [a, b]( HkxClipDocument & d ) { return d.setPlayRange( a, b ); } );
}

void AnimWorkspace::sheetRangeDragged( int first, int last )
{
	if ( !document() )
		return;
	edit( tr( "Play range %1..%2" ).arg( first ).arg( last ),
		  [first, last]( HkxClipDocument & d ) { return d.setPlayRange( first, last ); } );
}

void AnimWorkspace::retimeClip()
{
	const float fps = float( retimeBox->value() );
	edit( tr( "Retime to %1 fps" ).arg( fps ), [fps]( HkxClipDocument & d ) { return d.retime( fps ); } );
}

void AnimWorkspace::bakeRootMotion()
{
	const int track = rootTrackBox->currentData().toInt();
	edit( tr( "Bake root motion" ), [track]( HkxClipDocument & d ) { return d.bakeRootMotion( track ); } );
}

void AnimWorkspace::unbakeRootMotion()
{
	edit( tr( "Unbake root motion" ), []( HkxClipDocument & d ) { return d.unbakeRootMotion(); } );
}

void AnimWorkspace::removeSelectedTrack()
{
	const int track = selectedTrack();
	if ( track < 0 ) {
		say( tr( "Select a track row first." ), true );
		return;
	}
	edit( tr( "Remove track %1" ).arg( track ), [track]( HkxClipDocument & d ) { return d.removeTrack( track ); } );
}

void AnimWorkspace::removeTransformAxesDialog()
{
	/* bungo's ruling 3, verbatim: "Add an option here, under remove track, to
	   remove all transforms in specific directions, so x, y, z or a
	   combination of them. Same goes for rotation direction. That way, I can
	   make it so that the slide stays in the center, but the COM still moves
	   downward when the player crouches."

	   Six boxes, nothing else -- except the one line the ruling asks for, which
	   names the rotation convention. (That line is the exception the ruling
	   makes to the no-descriptions rule; it is a statement of fact about the
	   numbers, not a blurb about the feature.) */
	const int track = selectedTrack();
	if ( track < 0 ) {
		say( tr( "Select a bone row first." ), true );
		return;
	}
	const HkxClipDocument * d = document();
	const QString who = d && !d->trackNames.value( track ).isEmpty()
		? d->trackNames.value( track ) : tr( "track %1" ).arg( track );

	QDialog dlg( this );
	dlg.setObjectName( QStringLiteral( "AnimWsAxisDialog" ) );
	dlg.setWindowTitle( tr( "Remove transform axes -- %1" ).arg( who ) );
	auto * v = new QVBoxLayout( &dlg );
	auto * grid = new QGridLayout;
	v->addLayout( grid );
	auto boxAt = [&]( const char * name, const QString & label, int r, int c ) {
		auto * cb = new QCheckBox( label, &dlg );
		cb->setObjectName( QString::fromLatin1( name ) );
		grid->addWidget( cb, r, c );
		return cb;
	};
	auto * lt = new QLabel( tr( "Translation" ), &dlg );
	auto * lr = new QLabel( tr( "Rotation" ), &dlg );
	grid->addWidget( lt, 0, 0 );
	grid->addWidget( lr, 1, 0 );
	QCheckBox * tx = boxAt( "AnimWsAxisTX", tr( "X" ), 0, 1 );
	QCheckBox * ty = boxAt( "AnimWsAxisTY", tr( "Y" ), 0, 2 );
	QCheckBox * tz = boxAt( "AnimWsAxisTZ", tr( "Z" ), 0, 3 );
	QCheckBox * rx = boxAt( "AnimWsAxisRX", tr( "X" ), 1, 1 );
	QCheckBox * ry = boxAt( "AnimWsAxisRY", tr( "Y" ), 1, 2 );
	QCheckBox * rz = boxAt( "AnimWsAxisRZ", tr( "Z" ), 1, 3 );
	auto * conv = new QLabel( tr( "Rotation X, Y, Z are the Euler triple about the bone's own axes, "
								  "the same three numbers a rotation row shows in the Blocks tab." ), &dlg );
	conv->setObjectName( QStringLiteral( "AnimWsAxisConvention" ) );
	conv->setWordWrap( true );
	v->addWidget( conv );
	auto * bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
	bb->setObjectName( QStringLiteral( "AnimWsAxisButtons" ) );
	bb->button( QDialogButtonBox::Ok )->setObjectName( QStringLiteral( "AnimWsAxisOk" ) );
	bb->button( QDialogButtonBox::Ok )->setText( tr( "Remove" ) );
	v->addWidget( bb );
	connect( bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	connect( bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
	if ( dlg.exec() != QDialog::Accepted )
		return;
	HkxAxisMask mask;
	mask.tx = tx->isChecked();
	mask.ty = ty->isChecked();
	mask.tz = tz->isChecked();
	mask.rx = rx->isChecked();
	mask.ry = ry->isChecked();
	mask.rz = rz->isChecked();
	applyAxisStrip( track, mask );
}

void AnimWorkspace::applyAxisStrip( int track, const HkxAxisMask & mask )
{
	if ( !mask.any() ) {
		say( tr( "Nothing was ticked, so nothing was removed." ), true );
		return;
	}
	edit( tr( "Remove transform axes on track %1" ).arg( track ),
		  [track, mask]( HkxClipDocument & d ) { return d.removeTransformAxes( track, mask ); } );
}

void AnimWorkspace::renameSelectedTrack()
{
	const int track = selectedTrack();
	const HkxClipDocument * d = document();
	if ( track < 0 || !d ) {
		say( tr( "Select a track row first." ), true );
		return;
	}
	bool ok = false;
	const QString name = QInputDialog::getText( this, tr( "Rename track" ), tr( "Bone name" ), QLineEdit::Normal,
		d->trackNames.value( track ), &ok );
	if ( !ok || name.trimmed().isEmpty() )
		return;
	const HkxSkeleton * sk = nullptr;
	if ( playback() && !playback()->knownSkeletons().isEmpty() )
		sk = &playback()->knownSkeletons().last();
	edit( tr( "Rename track %1" ).arg( track ), [track, name, sk]( HkxClipDocument & dd ) { return dd.renameTrack( track, name, sk ); } );
}

void AnimWorkspace::addFloatTrack()
{
	bool ok = false;
	const QString name = QInputDialog::getText( this, tr( "Add float track" ), tr( "Name" ), QLineEdit::Normal, QString(), &ok );
	if ( !ok )
		return;
	edit( tr( "Add float track" ), [name]( HkxClipDocument & d ) { return d.addFloatTrack( name ); } );
}

void AnimWorkspace::setFloatKeyAtPlayhead()
{
	const int row = sheet->currentRow();
	if ( row < 0 || row >= sheet->rows().count() || sheet->rows().at( row ).kind != AnimWsRow::Float ) {
		say( tr( "Select a float row first." ), true );
		return;
	}
	const int ft = sheet->rows().at( row ).floatTrack;
	const int frame = currentFrame();
	const float v = float( floatValueBox->value() );
	edit( tr( "Set float key at frame %1" ).arg( frame ), [ft, frame, v]( HkxClipDocument & d ) { return d.setFloatKey( ft, frame, v ); } );
}

void AnimWorkspace::sheetContextMenu( int row, int frame, const AnimWsMarkerRef & marker, const QPoint & globalPos )
{
	/* bungo's rulings 7 (01:56) and 7a (01:58), verbatim: "Now, why can't I
	   right click and insert an annotation anywhere?" / "we need to add and
	   remove annotations with right click".

	   So the menu comes up ANYWHERE -- the ruler, the marker row, a bone row,
	   a float row -- and the annotation entries are always in it. Every entry
	   names THE CLICKED FRAME, never the playhead. Label + action only, no
	   blurbs (the panel rule); the shortcut rides on the action. */
	QMenu menu( this );
	if ( row >= 0 && row < sheet->rows().count() )
		sheet->selectRow( row );

	// ---- the annotation block: first, because that is what the ruling is about
	if ( marker.valid() ) {
		const HkxClipDocument * d = document();
		const QString nm = d ? d->clip.annotations.value( marker.track ).value( marker.index ).text : QString();
		const QString shown = nm.isEmpty() ? tr( "(unnamed)" ) : nm;
		QAction * ren = menu.addAction( tr( "Rename annotation '%1'…" ).arg( shown ), this,
			[this, marker]() { sheet->beginMarkerRename( marker, vocabulary() ); } );
		ren->setShortcut( QKeySequence( QStringLiteral( "Ctrl+M" ) ) );
		menu.addAction( tr( "Remove annotation '%1'" ).arg( shown ), this, [this, marker, shown]() {
			edit( tr( "Remove annotation '%1'" ).arg( shown ), [marker]( HkxClipDocument & d2 ) {
				return d2.deleteAnnotation( marker.track, marker.index );
			} );
		} );
		menu.addSeparator();
	}
	QAction * add = menu.addAction( tr( "Add annotation at frame %1" ).arg( frame ), this,
		[this, frame]() { sheet->beginNewMarker( frame, vocabulary() ); } );
	add->setShortcut( QKeySequence( QStringLiteral( "M" ) ) );

	// ---- what the clicked ROW offers, at the clicked frame
	if ( row >= 0 && row < sheet->rows().count() ) {
		const AnimWsRow & r = sheet->rows().at( row );
		if ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) {
			menu.addSeparator();
			menu.addAction( tr( "Insert key at frame %1" ).arg( frame ), this, [this, frame]() { insertKeyAtFrame( frame ); } );
			menu.addAction( tr( "Select all keys on this track" ), this, [this, r]() {
				QVector<HkxKeyRef> all;
				if ( const HkxClipDocument * d = document() )
					for ( const HkxKey & k : d->keys.value( r.track ) )
						all.append( HkxKeyRef{ r.track, k.frame } );
				sheet->selectKeys( all );
			} );
			menu.addAction( tr( "Rename track…" ), this, &AnimWorkspace::renameSelectedTrack );
			menu.addAction( tr( "Remove track" ), this, &AnimWorkspace::removeSelectedTrack );
			// ruling 3: "Add an option here, under remove track"
			menu.addAction( tr( "Remove transform axes…" ), this, &AnimWorkspace::removeTransformAxesDialog );
			if ( r.nodeBlock >= 0 )
				menu.addAction( tr( "Isolate in the viewport" ), this, [this, r]() { emit isolateBlock( r.nodeBlock ); } );
		} else if ( r.kind == AnimWsRow::Float ) {
			menu.addSeparator();
			menu.addAction( tr( "Set float key at frame %1" ).arg( frame ), this, [this, r, frame]() {
				const int ft = r.floatTrack;
				const float v = float( floatValueBox->value() );
				edit( tr( "Set float key at frame %1" ).arg( frame ), [ft, frame, v]( HkxClipDocument & d ) { return d.setFloatKey( ft, frame, v ); } );
			} );
			menu.addAction( tr( "Remove float track" ), this, [this, r]() {
				edit( tr( "Remove float track" ), [r]( HkxClipDocument & d ) { return d.removeFloatTrack( r.floatTrack ); } );
			} );
		} else if ( r.kind == AnimWsRow::NifTrack && r.nodeBlock >= 0 ) {
			menu.addSeparator();
			menu.addAction( tr( "Isolate in the viewport" ), this, [this, r]() { emit isolateBlock( r.nodeBlock ); } );
		}
	}

	// ---- the clicked frame itself, from anywhere on the sheet
	menu.addSeparator();
	menu.addAction( tr( "Put the playhead on frame %1" ).arg( frame ), this, [this, frame]() {
		sheet->setCurrentFrame( frame );
		sheetFrameScrubbed( frame );
	} );
	menu.addAction( tr( "Play range starts at frame %1" ).arg( frame ), this, [this, frame]() {
		if ( const HkxClipDocument * d = document() )
			sheetRangeDragged( frame, std::max( frame + 1, d->rangeLastFrame() ) );
	} );
	menu.addAction( tr( "Play range ends at frame %1" ).arg( frame ), this, [this, frame]() {
		if ( const HkxClipDocument * d = document() )
			sheetRangeDragged( std::min( d->rangeFirstFrame(), std::max( 0, frame - 1 ) ), frame );
	} );
	menu.exec( globalPos );
}


/*
 *  Save, and the .hkx document
 */

void AnimWorkspace::saveClip()
{
	const HkxClipDocument * d = document();
	if ( !d )
		return;
	if ( d->sourcePath.isEmpty() || !d->sourcePath.endsWith( QStringLiteral( ".hkx" ), Qt::CaseInsensitive ) ) {
		saveClipAs();
		return;
	}
	HkxWriteReport rep;
	QString err;
	if ( !d->save( d->sourcePath, rep, err ) ) {
		say( err, true );
		return;
	}
	undo->setClean();
	say( tr( "Saved %1: %2" ).arg( d->sourcePath, rep.summary() ), false );
#ifdef WW_ANIMWS_HKXMODEL
	if ( hkxModel && curEntry == hkxModelEntry ) {
		// the Blocks tab shows the document that was just written
		QByteArray bytes = d->toPackfile( err );
		QBuffer buf( &bytes );
		buf.open( QIODevice::ReadOnly );
		syncing = true;
		hkxModel->load( buf, d->sourcePath.toLocal8Bit().constData() );
		syncing = false;
	}
#endif
}

void AnimWorkspace::saveClipAs()
{
	const HkxClipDocument * d = document();
	if ( !d )
		return;
	const QString path = QFileDialog::getSaveFileName( this, tr( "Save animation as" ),
		d->sourcePath.isEmpty() ? curEntry + QStringLiteral( ".hkx" ) : d->sourcePath,
		tr( "Havok animation (*.hkx)" ) );
	if ( path.isEmpty() )
		return;
	HkxWriteReport rep;
	QString err;
	if ( !d->save( path, rep, err ) ) {
		say( err, true );
		return;
	}
	docs[curEntry]->sourcePath = path;
	undo->setClean();
	say( tr( "Saved %1: %2" ).arg( path, rep.summary() ), false );
	refreshSummary();
}

void AnimWorkspace::hkxModelChanged()
{
#ifdef WW_ANIMWS_HKXMODEL
	if ( syncing || !hkxModel || hkxModelEntry.isEmpty() )
		return;
	// a Blocks-tab edit: re-decode the clip into this workspace (the snapshots
	// on the undo stack describe a document that no longer exists -> cleared)
	const HkxAnimFile f = hkxModel->animFile();
	if ( !f.ok() || f.clips.isEmpty() ) {
		say( tr( "The .hkx document no longer decodes as a clip: %1" ).arg( f.error ), true );
		return;
	}
	HkxClipDocument * d = docFor( hkxModelEntry, true );
	if ( !d )
		return;
	const QString path = d->sourcePath;
	*d = HkxClipDocument::fromClip( f.clips.first(), d->trackNames.count() == f.clips.first().numTracks ? d->trackNames : trackNamesOf( hkxModelEntry ) );
	d->sourcePath = path;
	undo->clear();
	if ( HkxPlayback * pb = playback() )
		pb->replaceClip( hkxModelEntry, d->clip, d->trackNames );
	if ( curEntry == hkxModelEntry )
		rebuildRows();
	if ( glView )
		glView->update();
	say( tr( "Re-decoded %1 from the Blocks tab's edit." ).arg( hkxModelEntry ), false );
#endif
}
