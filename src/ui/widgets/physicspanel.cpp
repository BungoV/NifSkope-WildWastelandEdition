#include "ui/widgets/physicspanel.h"

#include "glview.h"
#include "model/nifmodel.h"
#include "physics/physicspreview.h"
#include "wwskin.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

PhysicsSimPanel::PhysicsSimPanel( GLView * ogl, NifModel * nif, Mode mode,
	QAction * showAction, QWidget * parent )
	: QWidget( parent ), m_ogl( ogl ), m_nif( nif ), m_mode( mode ), m_showAction( showAction )
{
	QVBoxLayout * col = new QVBoxLayout( this );
	col->setContentsMargins( 10, 8, 10, 8 );
	col->setSpacing( 6 );

	auto heading = [this]( const QString & text ) {
		QLabel * l = new QLabel( text, this );
		QFont f = l->font();
		f.setBold( true );
		l->setFont( f );
		return l;
	};

	/* Viewport visibility, first, because it is the thing most often wanted.
	 *
	 * Driven by the EXISTING aShowCollision action rather than a second switch
	 * of its own: the Render toolbar and the settings dialog already toggle
	 * that one, and a duplicate would let the two disagree about whether
	 * collision is being drawn.
	 */
	QCheckBox * showChk = new QCheckBox( tr( "Show collision in viewport" ), this );
	showChk->setToolTip( tr( "Draw collision geometry over the scene. The same "
							 "toggle as View > Show Collision." ) );
	col->addWidget( showChk );
	QFrame * showRule = new QFrame( this );
	showRule->setFrameShape( QFrame::HLine );
	showRule->setFrameShadow( QFrame::Sunken );
	col->addWidget( showRule );

	// Run / stop
	QPushButton * runBtn = new QPushButton( tr( "Run Physics Sim" ), this );
	runBtn->setCheckable( true );
	runBtn->setToolTip( tr( "Simulate this file's ragdoll live. Nothing is written "
							"back to the file." ) );
	col->addWidget( runBtn );

	QLabel * status = new QLabel( this );
	status->setWordWrap( true );
	col->addWidget( status );

	/* Which system, when the file holds more than one.
	 *
	 * start() takes the first jointed system it finds, which is right for a
	 * creature file and arbitrary for a skeleton file carrying several -- and
	 * there was no way to tell which one you were looking at, let alone choose.
	 * Hidden entirely when there is only one, since a combo box with a single
	 * entry is a control that cannot be used.
	 */
	QComboBox * sysCombo = new QComboBox( this );
	sysCombo->setToolTip( tr( "Which collision system to simulate. Only shown when the "
							  "file carries more than one." ) );
	sysCombo->hide();
	col->addWidget( sysCombo );

	/* Tools, and only in the toolbar.
	 *
	 * Which tool is active, and how hard it hits, is the thing you change most
	 * often and reach for from the viewport -- so it lives one click away in the
	 * top bar. Repeating the whole row in the manager was two copies of one
	 * control on screen at once, each able to show the other as out of date for
	 * as long as it took a sync to run.
	 */
	QLabel * toolHead = heading( tr( "Tool" ) );
	col->addWidget( toolHead );
	QLabel * toolHint = new QLabel( tr( "Right-click pins a bone in place, with any tool." ), this );
	toolHint->setWordWrap( true );
	{
		QFont hf = toolHint->font();
		hf.setPointSizeF( hf.pointSizeF() - 0.5 );
		toolHint->setFont( hf );
	}
	col->addWidget( toolHint );
	QWidget * toolsRow = new QWidget( this );
	QGridLayout * tools = new QGridLayout( toolsRow );
	tools->setContentsMargins( 0, 0, 0, 0 );
	tools->setSpacing( 4 );
	QButtonGroup * toolGroup = new QButtonGroup( this );
	toolGroup->setExclusive( true );
	struct ToolEntry { PhysicsPreview::Tool tool; const char * label; const char * tip; };
	static const ToolEntry toolEntries[] = {
		{ PhysicsPreview::Tool::Grab,  QT_TR_NOOP( "Grab" ),
		  QT_TR_NOOP( "Pull a bone with a spring. Let go while moving and it is thrown; "
					  "let go while still and it drops. (1)" ) },
		{ PhysicsPreview::Tool::Shoot, QT_TR_NOOP( "Shoot" ),
		  QT_TR_NOOP( "Hit it with an impulse where you click. Off-centre hits spin it. (2)" ) },
		{ PhysicsPreview::Tool::Blast, QT_TR_NOOP( "Blast" ),
		  QT_TR_NOOP( "Radial impulse from the point you click. (3)" ) },
		{ PhysicsPreview::Tool::Wind,  QT_TR_NOOP( "Wind" ),
		  QT_TR_NOOP( "Steady push along the view while the button is held. (4)" ) },
		{ PhysicsPreview::Tool::Punt,  QT_TR_NOOP( "Punt" ),
		  QT_TR_NOOP( "Shove a body away along the view, or yank it toward you. "
					  "Heavier and more directed than Shoot. (5)" ) },
		{ PhysicsPreview::Tool::Prop,  QT_TR_NOOP( "Ball" ),
		  QT_TR_NOOP( "Throw a ball at the rig. The honest test of a collision mesh: "
					  "does anything actually bounce off it. (6)" ) },
	};
	int tRow = 0, tCol = 0;
	for ( const ToolEntry & te : toolEntries ) {
		QPushButton * b = new QPushButton( tr( te.label ), toolsRow );
		b->setCheckable( true );
		b->setToolTip( tr( te.tip ) );
		toolGroup->addButton( b, int( te.tool ) );
		tools->addWidget( b, tRow, tCol );
		// six tools, three to a row: two full rows rather than the 3+1 the
		// four-tool version left, which put Wind on a line by itself
		if ( ++tCol == 3 ) {
			tCol = 0;
			tRow++;
		}
	}
	col->addWidget( toolsRow );

	/* Parameters of the ACTIVE tool, and only that tool.
	 *
	 * Selecting Wind used to give you a 40 N push with nowhere to say
	 * otherwise; the same was true of Shoot's impulse and Blast's radius.
	 * Showing all of them at once would be a wall of spin boxes for settings
	 * that mostly do not apply, so the row swaps with the tool.
	 */
	QWidget * toolParams = new QWidget( this );
	QGridLayout * tp = new QGridLayout( toolParams );
	tp->setContentsMargins( 0, 0, 0, 0 );
	tp->setSpacing( 4 );
	int pRow = 0;
	auto addParam = [&]( const QString & label, QWidget * w ) {
		QLabel * l = new QLabel( label, toolParams );
		tp->addWidget( l, pRow, 0 );
		tp->addWidget( w, pRow++, 1 );
		return l;
	};

	/* One dial, not two.
	 *
	 * Firmness is the grab's compliance and Strength the cap on how hard it may
	 * pull, and both answer the same question -- how hard does the hand hold? --
	 * so they were two ways of asking it that had to be kept consistent by hand.
	 * Grip drives both: the mapping is chosen so the default of 0.9 lands on the
	 * firmness and strength the tool already shipped with.
	 */
	QDoubleSpinBox * firmSpin = new QDoubleSpinBox( toolParams );
	firmSpin->setRange( 0.0, 1.0 );
	firmSpin->setSingleStep( 0.05 );
	firmSpin->setDecimals( 2 );
	firmSpin->setToolTip( tr( "How hard the hand holds. 1 is rigid and strong; lower is "
							  "springier, lags behind the cursor, and gives up sooner "
							  "rather than dragging the whole rig." ) );
	QLabel * firmLbl = addParam( tr( "Grip" ), firmSpin );

	QCheckBox * noCollideChk = new QCheckBox( tr( "Held bone ignores the rig" ), toolParams );
	noCollideChk->setToolTip( tr( "Let the bone in hand pass through the rest of the "
								  "ragdoll while it is held, so a limb trapped inside "
								  "the torso can be pulled back out. Only the held bone; "
								  "everything else still collides." ) );
	tp->addWidget( noCollideChk, pRow++, 0, 1, 2 );

	QDoubleSpinBox * impulseSpin = new QDoubleSpinBox( toolParams );
	impulseSpin->setRange( 0.1, 500.0 );
	impulseSpin->setSingleStep( 2.0 );
	impulseSpin->setSuffix( tr( " kg m/s" ) );
	impulseSpin->setToolTip( tr( "Momentum delivered by a hit. 12 is about a heavy "
								 "pistol round." ) );
	QLabel * impulseLbl = addParam( tr( "Impulse" ), impulseSpin );

	QCheckBox * projChk = new QCheckBox( tr( "Physical projectile" ), toolParams );
	projChk->setToolTip( tr( "Fire a round that travels and drops, instead of hitting "
							 "instantly. It can miss." ) );
	tp->addWidget( projChk, pRow++, 0, 1, 2 );

	QDoubleSpinBox * projSpeed = new QDoubleSpinBox( toolParams );
	projSpeed->setRange( 1.0, 1000.0 );
	projSpeed->setSingleStep( 5.0 );
	projSpeed->setSuffix( tr( " m/s" ) );
	QLabel * projSpeedLbl = addParam( tr( "Speed" ), projSpeed );

	QDoubleSpinBox * projMass = new QDoubleSpinBox( toolParams );
	projMass->setRange( 0.001, 100.0 );
	projMass->setDecimals( 3 );
	projMass->setSingleStep( 0.01 );
	projMass->setSuffix( tr( " kg" ) );
	QLabel * projMassLbl = addParam( tr( "Mass" ), projMass );

	QDoubleSpinBox * projRadius = new QDoubleSpinBox( toolParams );
	projRadius->setRange( 0.005, 2.0 );
	projRadius->setDecimals( 3 );
	projRadius->setSingleStep( 0.01 );
	projRadius->setSuffix( tr( " m" ) );
	QLabel * projRadiusLbl = addParam( tr( "Radius" ), projRadius );

	QCheckBox * projGrav = new QCheckBox( tr( "Round drops" ), toolParams );
	projGrav->setToolTip( tr( "Let gravity pull the round on the way, so a long shot "
							  "lands lower than it was aimed." ) );
	tp->addWidget( projGrav, pRow++, 0, 1, 2 );

	QDoubleSpinBox * blastRadius = new QDoubleSpinBox( toolParams );
	blastRadius->setRange( 0.1, 50.0 );
	blastRadius->setSingleStep( 0.5 );
	blastRadius->setSuffix( tr( " m" ) );
	blastRadius->setToolTip( tr( "How far the blast reaches. It falls off to nothing "
								 "at this distance." ) );
	QLabel * blastRadiusLbl = addParam( tr( "Radius" ), blastRadius );

	QDoubleSpinBox * blastStrength = new QDoubleSpinBox( toolParams );
	blastStrength->setRange( 0.1, 2000.0 );
	blastStrength->setSingleStep( 5.0 );
	blastStrength->setSuffix( tr( " kg m/s" ) );
	QLabel * blastStrengthLbl = addParam( tr( "Strength" ), blastStrength );

	QDoubleSpinBox * windStrength = new QDoubleSpinBox( toolParams );
	windStrength->setRange( 0.1, 5000.0 );
	windStrength->setSingleStep( 5.0 );
	windStrength->setSuffix( tr( " N" ) );
	windStrength->setToolTip( tr( "Force applied along the view while the button is held." ) );
	QLabel * windLbl = addParam( tr( "Strength" ), windStrength );

	QDoubleSpinBox * puntStrength = new QDoubleSpinBox( toolParams );
	puntStrength->setRange( 0.1, 5000.0 );
	puntStrength->setSingleStep( 10.0 );
	puntStrength->setSuffix( tr( " kg m/s" ) );
	puntStrength->setToolTip( tr( "Momentum of the shove, along the view. Applied where "
								  "you click, so punting a foot spins the rig and punting "
								  "a chest sends it straight." ) );
	QLabel * puntLbl = addParam( tr( "Strength" ), puntStrength );

	QCheckBox * puntPullChk = new QCheckBox( tr( "Pull toward the camera" ), toolParams );
	puntPullChk->setToolTip( tr( "Reverse it: yank the body toward you instead of away." ) );
	tp->addWidget( puntPullChk, pRow++, 0, 1, 2 );

	QDoubleSpinBox * propRadius = new QDoubleSpinBox( toolParams );
	propRadius->setRange( 0.01, 5.0 );
	propRadius->setDecimals( 3 );
	propRadius->setSingleStep( 0.05 );
	propRadius->setSuffix( tr( " m" ) );
	propRadius->setToolTip( tr( "Size of the ball. A ball, not a crate, because the exact "
								"contact test is segment-to-segment plus a radius -- a box "
								"would have no faces between its corners and would fall "
								"through the rig." ) );
	QLabel * propRadiusLbl = addParam( tr( "Radius" ), propRadius );

	QDoubleSpinBox * propMass = new QDoubleSpinBox( toolParams );
	propMass->setRange( 0.01, 5000.0 );
	propMass->setDecimals( 2 );
	propMass->setSingleStep( 1.0 );
	propMass->setSuffix( tr( " kg" ) );
	propMass->setToolTip( tr( "How heavy. 5 kg is a shot put; raise it to bowl a rig over." ) );
	QLabel * propMassLbl = addParam( tr( "Mass" ), propMass );

	QDoubleSpinBox * propSpeed = new QDoubleSpinBox( toolParams );
	propSpeed->setRange( 0.0, 200.0 );
	propSpeed->setSingleStep( 1.0 );
	propSpeed->setSuffix( tr( " m/s" ) );
	propSpeed->setToolTip( tr( "Throw speed. 0 drops it where you click." ) );
	QLabel * propSpeedLbl = addParam( tr( "Speed" ), propSpeed );

	QPushButton * clearPropsBtn = new QPushButton( tr( "Clear balls" ), toolParams );
	clearPropsBtn->setToolTip( tr( "Remove every ball from the scene. (C)" ) );
	tp->addWidget( clearPropsBtn, pRow++, 0, 1, 2 );
	col->addWidget( toolParams );

	// Playback
	col->addWidget( heading( tr( "Playback" ) ) );
	QHBoxLayout * play = new QHBoxLayout();
	play->setSpacing( 4 );
	/* "Pause" and "Freeze" are genuinely different and the names never said how:
	 * one stops TIME, the other stops MOTION while the solver keeps working, so
	 * a settled heap holds its shape and can still be dragged. Named for the
	 * difference rather than for the mechanism.
	 */
	QPushButton * pauseBtn = new QPushButton( tr( "Stop time" ), this );
	pauseBtn->setCheckable( true );
	pauseBtn->setToolTip( tr( "Freeze the frame: nothing moves and nothing solves, and "
							  "the pose holds exactly where it is. (Space)" ) );
	QPushButton * stepBtn = new QPushButton( tr( "Step" ), this );
	stepBtn->setToolTip( tr( "Advance one frame while paused, to watch a joint "
							 "a frame at a time. (.)" ) );
	QPushButton * freezeBtn = new QPushButton( tr( "Stop motion" ), this );
	freezeBtn->setToolTip( tr( "Take the speed out but keep solving, so a settled heap "
							   "holds its shape and can still be dragged. (F)" ) );
	QPushButton * resetBtn = new QPushButton( tr( "Reset" ), this );
	resetBtn->setToolTip( tr( "Back to the pose stored in the file. (R)" ) );
	for ( QPushButton * b : { pauseBtn, stepBtn, freezeBtn, resetBtn } )
		play->addWidget( b );
	col->addLayout( play );

	/* Record and scrub.
	 *
	 * A ragdoll settles in about two seconds and the one frame worth keeping
	 * goes past in a sixtieth of one. Without a recording the only way back to
	 * it is to reset and try to catch it with the pause key, which is a game of
	 * reflexes rather than a tool.
	 */
	QWidget * recRow = new QWidget( this );
	QHBoxLayout * rec = new QHBoxLayout( recRow );
	rec->setContentsMargins( 0, 0, 0, 0 );
	rec->setSpacing( 4 );
	QPushButton * recBtn = new QPushButton( tr( "Record" ), this );
	recBtn->setCheckable( true );
	recBtn->setToolTip( tr( "Keep every frame as it is simulated, so it can be scrubbed "
							"back through. Capped at 20 seconds, oldest dropped." ) );
	QSlider * scrub = new QSlider( Qt::Horizontal, this );
	scrub->setToolTip( tr( "Move through the recording. Scrubbing pauses, since running "
						   "on from a frame you went back to would overwrite the rest." ) );
	QPushButton * liveBtn = new QPushButton( tr( "Live" ), this );
	liveBtn->setToolTip( tr( "Leave the recording and carry on simulating from the pose "
							 "on screen." ) );
	rec->addWidget( recBtn );
	rec->addWidget( scrub, 1 );
	rec->addWidget( liveBtn );
	col->addWidget( recRow );

	/* Actions that apply to the whole rig at once.
	 *
	 * Pins accumulate one right-click at a time and there was no way back from
	 * four of them short of a full reset; capture is the one control here that
	 * writes to the file, which is why it says so on the button.
	 */
	QWidget * actsRow = new QWidget( this );
	QHBoxLayout * acts = new QHBoxLayout( actsRow );
	acts->setContentsMargins( 0, 0, 0, 0 );
	acts->setSpacing( 4 );
	QPushButton * unpinBtn = new QPushButton( tr( "Unpin all" ), this );
	unpinBtn->setToolTip( tr( "Release every pinned bone. (U)" ) );
	QPushButton * captureBtn = new QPushButton( tr( "Capture pose" ), this );
	captureBtn->setToolTip( tr( "Write the simulated pose back to the nodes the bodies "
								"are bound to. This CHANGES the file -- everything else "
								"in this this is a preview. Undoable." ) );
	acts->addWidget( unpinBtn );
	acts->addWidget( captureBtn );
	col->addWidget( actsRow );

	/* World, then Solver, then Advanced -- split by how often a thing is touched.
	 *
	 * This was one flat list of eleven rows mixing gravity and the ground, which
	 * you change while watching a rig, with sweeps and substeps, which you set
	 * once per file and never look at again. The first group belongs in the
	 * toolbar dropdown as much as Playback does; the other two do not.
	 */
	QLabel * worldHead = heading( tr( "World" ) );
	col->addWidget( worldHead );
	QWidget * optsRow = new QWidget( this );
	QGridLayout * opts = new QGridLayout( optsRow );
	opts->setContentsMargins( 0, 0, 0, 0 );
	opts->setSpacing( 4 );
	int oRow = 0;

	QCheckBox * gravChk = new QCheckBox( tr( "Gravity" ), this );
	gravChk->setToolTip( tr( "Turn gravity off to inspect joint limits without "
							 "everything piling on the floor. (G)" ) );
	QDoubleSpinBox * gravSpin = new QDoubleSpinBox( this );
	gravSpin->setRange( 0.0, 100.0 );
	gravSpin->setSingleStep( 0.5 );
	// the real superscript, not the ASCII "2" this used to carry: a units
	// label that reads "m/s2" is a typo everywhere except in source code
	gravSpin->setSuffix( QStringLiteral( " m/s²" ) );
	opts->addWidget( gravChk, oRow, 0 );
	opts->addWidget( gravSpin, oRow++, 1 );

	QLabel * speedLbl = new QLabel( tr( "Speed" ), this );
	QSlider * speedSlider = new QSlider( Qt::Horizontal, this );
	speedSlider->setRange( 5, 200 );      // 0.05x .. 2.0x
	speedSlider->setToolTip( tr( "Slow motion, for watching a joint pop. Scales time, "
								 "not the solver, so accuracy is unchanged." ) );
	opts->addWidget( speedLbl, oRow, 0 );
	opts->addWidget( speedSlider, oRow++, 1 );

	QCheckBox * groundChk = new QCheckBox( tr( "Ground" ), this );
	groundChk->setToolTip( tr( "A floor to land on. Without it the ragdoll falls out "
							   "of the scene. (H)" ) );
	QDoubleSpinBox * groundSpin = new QDoubleSpinBox( this );
	groundSpin->setRange( -100000.0, 100000.0 );
	groundSpin->setDecimals( 1 );
	groundSpin->setSingleStep( 5.0 );
	groundSpin->setSuffix( tr( " units" ) );
	groundSpin->setToolTip( tr( "Floor height, in game units." ) );
	/* The reset sits ON the row it resets.
	 *
	 * It was a full-width button reading "Put the floor back under the rig" --
	 * a whole row of panel for an operation that belongs beside the number it
	 * puts back, which is how every other revert control in this application
	 * behaves.
	 */
	QToolButton * groundResetBtn = new QToolButton( this );
	groundResetBtn->setText( QStringLiteral( "↺" ) );
	groundResetBtn->setToolTip( tr( "Put the floor back just below the rig, where it "
									"started." ) );
	QWidget * groundCell = new QWidget( this );
	QHBoxLayout * groundLine = new QHBoxLayout( groundCell );
	groundLine->setContentsMargins( 0, 0, 0, 0 );
	groundLine->setSpacing( 2 );
	groundLine->addWidget( groundSpin, 1 );
	groundLine->addWidget( groundResetBtn );
	opts->addWidget( groundChk, oRow, 0 );
	opts->addWidget( groundCell, oRow++, 1 );

	QDoubleSpinBox * gripSpin = new QDoubleSpinBox( this );
	gripSpin->setRange( 0.0, 4.0 );
	gripSpin->setSingleStep( 0.1 );
	gripSpin->setDecimals( 2 );
	gripSpin->setToolTip( tr( "How much the floor grips, combined with each body's own "
							  "friction. 0 is ice and the rig slides for ever." ) );
	opts->addWidget( new QLabel( tr( "Floor grip" ), this ), oRow, 0 );
	opts->addWidget( gripSpin, oRow++, 1 );

	// moved up out of the display checkboxes: a floor you cannot see is the
	// first thing anyone turning the ground on wants to fix
	QCheckBox * groundVisChk = new QCheckBox( tr( "Show the ground" ), this );
	groundVisChk->setToolTip( tr( "Draw the floor as a solid surface. An invisible plane "
								   "that a ragdoll lands on looks like a bug." ) );
	opts->addWidget( groundVisChk, oRow++, 0, 1, 2 );
	col->addWidget( optsRow );

	/* Solver: the file's own numbers, ADJUSTED rather than replaced.
	 *
	 * Every body carries its own friction and bounce and the solver honours them
	 * now, so these are multipliers -- 1 means "as authored". A global that
	 * replaced the authored value would quietly discard what the file said, which
	 * is exactly what the old flat Body friction control did.
	 */
	QLabel * solverHead = heading( tr( "Solver" ) );
	col->addWidget( solverHead );
	QWidget * solverRow = new QWidget( this );
	QGridLayout * slv = new QGridLayout( solverRow );
	slv->setContentsMargins( 0, 0, 0, 0 );
	slv->setSpacing( 4 );
	int sRow = 0;

	QDoubleSpinBox * fricSpin = new QDoubleSpinBox( this );
	fricSpin->setRange( 0.0, 4.0 );
	fricSpin->setSingleStep( 0.1 );
	fricSpin->setDecimals( 2 );
	fricSpin->setToolTip( tr( "Scales the friction each body carries. 1 is the file as "
							  "authored; 0 lets everything slide freely." ) );
	slv->addWidget( new QLabel( tr( "Friction x" ), this ), sRow, 0 );
	slv->addWidget( fricSpin, sRow++, 1 );

	QDoubleSpinBox * bounceSpin = new QDoubleSpinBox( this );
	bounceSpin->setRange( 0.0, 4.0 );
	bounceSpin->setSingleStep( 0.1 );
	bounceSpin->setDecimals( 2 );
	bounceSpin->setToolTip( tr( "Scales the bounce each body carries. 1 is the file as "
								"authored; 0 makes every landing dead." ) );
	slv->addWidget( new QLabel( tr( "Bounce x" ), this ), sRow, 0 );
	slv->addWidget( bounceSpin, sRow++, 1 );

	QDoubleSpinBox * dampSpin = new QDoubleSpinBox( this );
	dampSpin->setRange( 0.0, 20.0 );
	dampSpin->setSingleStep( 0.1 );
	dampSpin->setDecimals( 2 );
	dampSpin->setToolTip( tr( "Extra drag on top of each body's authored damping. Raise it "
							  "to make a rig settle sooner; 0 leaves the file's own values "
							  "alone." ) );
	slv->addWidget( new QLabel( tr( "Damping +" ), this ), sRow, 0 );
	slv->addWidget( dampSpin, sRow++, 1 );
	col->addWidget( solverRow );

	/* Advanced, collapsed. Four rows nobody touches twice in a session, using
	 * the same disclosure the Collision Manager's own defaults section uses.
	 */
	QToolButton * advToggle = new QToolButton( this );
	advToggle->setText( tr( "Advanced" ) );
	advToggle->setCheckable( true );
	advToggle->setArrowType( Qt::RightArrow );
	advToggle->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	advToggle->setAutoRaise( true );
	col->addWidget( advToggle );

	QWidget * advRow = new QWidget( this );
	QGridLayout * adv = new QGridLayout( advRow );
	adv->setContentsMargins( 0, 0, 0, 0 );
	adv->setSpacing( 4 );
	int aRow = 0;
	advRow->setVisible( false );
	connect( advToggle, &QToolButton::toggled, this, [advToggle, advRow]( bool open ) {
		advToggle->setArrowType( open ? Qt::DownArrow : Qt::RightArrow );
		advRow->setVisible( open );
	} );

	/* Gravity DIRECTION, as a tilt off vertical and a heading round the compass.
	 * Two angles rather than a vector: nobody wants to normalise a triple by
	 * hand, and the question being asked is "what does this look like on a
	 * slope", which is exactly a tilt.
	 */
	QDoubleSpinBox * tiltSpin = new QDoubleSpinBox( this );
	tiltSpin->setRange( 0.0, 90.0 );
	tiltSpin->setSingleStep( 5.0 );
	tiltSpin->setDecimals( 1 );
	tiltSpin->setSuffix( QStringLiteral( "°" ) );
	tiltSpin->setToolTip( tr( "Tip gravity away from straight down, to see how a rig "
							  "behaves on a slope without authoring one." ) );
	adv->addWidget( new QLabel( tr( "Gravity tilt" ), this ), aRow, 0 );
	adv->addWidget( tiltSpin, aRow++, 1 );

	QDoubleSpinBox * headSpin = new QDoubleSpinBox( this );
	headSpin->setRange( 0.0, 360.0 );
	headSpin->setSingleStep( 15.0 );
	headSpin->setDecimals( 1 );
	headSpin->setSuffix( QStringLiteral( "°" ) );
	headSpin->setWrapping( true );
	headSpin->setToolTip( tr( "Which way the tilt points. No effect while the tilt is 0." ) );
	adv->addWidget( new QLabel( tr( "Tilt heading" ), this ), aRow, 0 );
	adv->addWidget( headSpin, aRow++, 1 );

	/* Solver cost, exposed because the stats overlay reports joint error and
	 * until now there was nothing to DO about a bad number.
	 */
	QSpinBox * iterSpin = new QSpinBox( this );
	iterSpin->setRange( 1, 32 );
	iterSpin->setToolTip( tr( "Constraint sweeps per substep. Four is measured; more buys "
							  "little on a healthy rig and can rescue a stiff one." ) );
	adv->addWidget( new QLabel( tr( "Sweeps" ), this ), aRow, 0 );
	adv->addWidget( iterSpin, aRow++, 1 );

	QSpinBox * subSpin = new QSpinBox( this );
	subSpin->setRange( 1, 32 );
	subSpin->setToolTip( tr( "Substeps per frame. The main stability control: raise it if "
							 "a rig jitters or a joint separates, at a proportional cost." ) );
	adv->addWidget( new QLabel( tr( "Substeps" ), this ), aRow, 0 );
	adv->addWidget( subSpin, aRow++, 1 );
	col->addWidget( advRow );

	/* Checkboxes in their own column, not in the label/value grid.
	 *
	 * Mixed in, they inherited the grid's first-column width and each sat at a
	 * different indent depending on whether it spanned one cell or two, which
	 * is what made the old panel look ragged down its left edge.
	 */
	QWidget * flagsRow = new QWidget( this );
	QVBoxLayout * flags = new QVBoxLayout( flagsRow );
	flags->setContentsMargins( 0, 0, 0, 0 );
	flags->setSpacing( 2 );
	QCheckBox * selfChk = new QCheckBox( tr( "Self-collision" ), this );
	selfChk->setToolTip( tr( "Let the rig collide with itself, as the file authorises." ) );
	QCheckBox * limitsChk = new QCheckBox( tr( "Angular limits" ), this );
	limitsChk->setToolTip( tr( "Honour the joint limits the constraints carry. Turn "
							   "off to see how much of a pose the limits are holding." ) );
	QCheckBox * hiLimitsChk = new QCheckBox( tr( "Highlight joints at their limits" ), this );
	hiLimitsChk->setToolTip( tr( "Colour bodies whose joints are outside the range the "
								 "constraints allow. Off by default, because a rig whose "
								 "authored pose already breaks a limit lights up from the start." ) );
	QCheckBox * statsChk = new QCheckBox( tr( "Stats overlay" ), this );
	statsChk->setToolTip( tr( "Speed, joint error, contacts and penetration, drawn over "
							  "the viewport. The solver computes these anyway." ) );
	for ( QCheckBox * c : { selfChk, limitsChk, hiLimitsChk, statsChk } )
		flags->addWidget( c );
	col->addWidget( flagsRow );

	/* Presets: several controls at once, for the setups actually reached for.
	 *
	 * Every one of these is a combination somebody assembles by hand every time
	 * -- zero-G to look at joint limits without a pile on the floor, slow motion
	 * plus record to watch a pop. Setting five controls one at a time to ask one
	 * question is the friction this removes.
	 */
	col->addWidget( heading( tr( "Presets" ) ) );
	QComboBox * presets = new QComboBox( this );
	presets->addItem( tr( "Choose a preset..." ) );
	presets->addItem( tr( "Zero gravity - inspect joint limits" ) );
	presets->addItem( tr( "Slow motion - watch a joint pop" ) );
	presets->addItem( tr( "Drop and settle" ) );
	presets->addItem( tr( "Ice floor" ) );
	presets->addItem( tr( "Stiff and stable" ) );
	presets->setToolTip( tr( "Set several options at once. Each is a combination that "
							 "answers one question." ) );
	col->addWidget( presets );

	/* No body list here.
	 *
	 * There was one, showing every body by name with its pin as a checkbox --
	 * sitting one panel below the Collision Manager's tree, which already lists
	 * exactly those bodies with their bone, shape, layer, material, mass and
	 * state. The only thing the list added was the checkbox, so the checkbox
	 * moved to the tree and the list went (CollisionManagerPanel::refreshSimPins).
	 */

	/* The shortcuts, written down.
	 *
	 * They existed before any of this panel did and were invisible unless you
	 * already knew them, which is the same complaint that produced the panel.
	 */
	QLabel * legend = new QLabel( this );
	legend->setText( tr(
		"<b>1-6</b> tool &nbsp; <b>Space</b> pause &nbsp; <b>.</b> step &nbsp; "
		"<b>F</b> freeze &nbsp; <b>G</b> gravity &nbsp; <b>R</b> reset<br>"
		"<b>U</b> unpin all &nbsp; <b>C</b> clear balls &nbsp; <b>Esc</b> leave<br>"
		"<b>Right-click</b> pin &nbsp; <b>Wheel</b> push/pull &nbsp; "
		"<b>Ctrl+drag</b> turn &nbsp; <b>Ctrl+wheel</b> roll &nbsp; <b>Shift</b> snap" ) );
	legend->setWordWrap( true );
	legend->setTextFormat( Qt::RichText );
	{
		QFont lf = legend->font();
		lf.setPointSizeF( lf.pointSizeF() - 0.5 );
		legend->setFont( lf );
		// wwSkinColor keeps this in step with the rest of the theme; a literal
		// grey here would be the one colour in the panel that ignores the skin
		legend->setStyleSheet( QStringLiteral( "color: %1;" )
			.arg( wwSkinColor( "textMuted" ) ) );
	}
	col->addWidget( legend );


	/* One place that pushes the preview's state into the widgets.
	 *
	 * The controls forward to PhysicsPreview and read back from it rather
	 * than holding their own copy, so a keyboard shortcut and a click on the
	 * same option cannot disagree.
	 */
	auto syncCollisionPanel = [=, this]() {
		PhysicsPreview & pv = m_ogl->physicsSim();
		const bool on = pv.active();
		QSignalBlocker bs( showChk );
		if ( m_showAction )
			showChk->setChecked( m_showAction->isChecked() );
		QSignalBlocker b0( runBtn ), b1( pauseBtn ), b2( gravChk ), b3( gravSpin );
		QSignalBlocker b4( speedSlider ), b5( groundChk ), b6( groundSpin );
		QSignalBlocker b7( selfChk ), b8( limitsChk ), b9( statsChk );
		runBtn->setChecked( on );
		runBtn->setText( on ? tr( "Stop Physics Sim" ) : tr( "Run Physics Sim" ) );
		/* Name the system, not just its size. "17 bodies, 16 joints" was true of
		 * whichever system happened to be first and said nothing about which.
		 */
		/* The COUNTS only. The system's own name goes in the picker below, and
		 * the Collision Manager says how many systems the file holds two inches
		 * above -- three places naming the same thing was two too many.
		 */
		status->setText( on
			? tr( "%1 bodies, %2 joints." ).arg( pv.bodyCount() ).arg( pv.jointCount() )
			: tr( "Not running." ) );
		for ( QAbstractButton * tb : toolGroup->buttons() )
			tb->setEnabled( on );
		if ( QAbstractButton * tb = toolGroup->button( int( pv.tool() ) ) ) {
			QSignalBlocker bt( tb );
			tb->setChecked( true );
		}
		for ( QWidget * w : { (QWidget *)pauseBtn, (QWidget *)stepBtn, (QWidget *)freezeBtn,
				(QWidget *)resetBtn, (QWidget *)gravChk, (QWidget *)gravSpin,
				(QWidget *)speedSlider, (QWidget *)speedLbl, (QWidget *)groundChk,
				(QWidget *)groundSpin, (QWidget *)selfChk, (QWidget *)limitsChk,
				(QWidget *)statsChk } )
			w->setEnabled( on );
		stepBtn->setEnabled( on && pv.paused() );
		pauseBtn->setChecked( pv.paused() );
		gravChk->setChecked( pv.gravityEnabled() );
		gravSpin->setValue( double( pv.gravityStrength() ) );
		gravSpin->setEnabled( on && pv.gravityEnabled() );
		speedSlider->setValue( int( pv.timeScale() * 100.0f ) );
		speedLbl->setText( tr( "Speed  %1x" ).arg( double( pv.timeScale() ), 0, 'g', 2 ) );
		groundChk->setChecked( pv.groundEnabled() );
		groundSpin->setValue( double( pv.groundHeight() ) );
		groundSpin->setEnabled( on && pv.groundEnabled() );
		selfChk->setChecked( pv.selfCollision() );
		limitsChk->setChecked( pv.angularLimits() );
		statsChk->setChecked( m_ogl->physicsStatsShown() );
		groundVisChk->setChecked( pv.groundVisible() );
		QSignalBlocker bg( gripSpin );
		gripSpin->setValue( double( pv.groundFriction() ) );
		gripSpin->setEnabled( on && pv.groundEnabled() );
		hiLimitsChk->setChecked( pv.highlightLimits() );
		groundVisChk->setEnabled( on && pv.groundEnabled() );
		groundResetBtn->setEnabled( on && pv.groundEnabled() );
		hiLimitsChk->setEnabled( on );

		// the solver knobs, which were all built and none reachable
		QSignalBlocker bk0( fricSpin ), bk1( dampSpin ), bk2( bounceSpin );
		QSignalBlocker bk3( iterSpin ), bk4( subSpin ), bk5( tiltSpin ), bk6( headSpin );
		fricSpin->setValue( double( pv.friction() ) );
		dampSpin->setValue( double( pv.damping() ) );
		bounceSpin->setValue( double( pv.restitution() ) );
		iterSpin->setValue( pv.iterations() );
		subSpin->setValue( pv.substeps() );
		/* Read the direction back as the two angles that set it, so the spins
		 * agree with the state even when a preset was what changed it.
		 */
		{
			const Vector3 g = pv.gravityDirection();
			const float tilt = std::acos( std::clamp( -g[2], -1.0f, 1.0f ) );
			tiltSpin->setValue( double( tilt ) * 180.0 / M_PI );
			if ( std::hypot( g[0], g[1] ) > 1.0e-4f )
				headSpin->setValue( std::fmod( std::atan2( double( g[1] ), double( g[0] ) )
					* 180.0 / M_PI + 360.0, 360.0 ) );
		}
		for ( QWidget * w : { (QWidget *)fricSpin, (QWidget *)dampSpin, (QWidget *)bounceSpin,
				(QWidget *)iterSpin, (QWidget *)subSpin, (QWidget *)tiltSpin,
				(QWidget *)headSpin, (QWidget *)presets,
				(QWidget *)unpinBtn, (QWidget *)captureBtn, (QWidget *)recBtn,
				(QWidget *)scrub, (QWidget *)liveBtn } )
			w->setEnabled( on );
		tiltSpin->setEnabled( on && pv.gravityEnabled() );
		headSpin->setEnabled( on && pv.gravityEnabled() && pv.gravityStrength() > 0.0f );
		unpinBtn->setEnabled( on && pv.pinnedCount() > 0 );

		// recording
		QSignalBlocker br( recBtn ), bsc( scrub );
		recBtn->setChecked( pv.recording() );
		scrub->setEnabled( on && pv.frameCount() > 1 );
		scrub->setRange( 0, std::max( 0, pv.frameCount() - 1 ) );
		if ( pv.frameIndex() >= 0 )
			scrub->setValue( pv.frameIndex() );
		else
			scrub->setValue( scrub->maximum() );
		liveBtn->setEnabled( on && pv.frameIndex() >= 0 );


		/* The system picker, only where there is a choice to make. Rebuilt from
		 * the file each time, because loading a different file changes it.
		 */
		{
			QSignalBlocker bsy( sysCombo );
			QList<int> systems;
			if ( m_nif ) {
				for ( qint32 b = 0; b < m_nif->getBlockCount(); b++ ) {
					const QModelIndex i = m_nif->getBlockIndex( b );
					if ( !m_nif->blockInherits( i, "bhkPhysicsSystem" )
						&& !m_nif->blockInherits( i, "bhkRagdollSystem" ) )
						continue;
					// the same test start() applies: a system with no constraints is
					// furniture, and offering it is offering a dead end
					const HknpSystem sys = hknpDecode( m_nif->get<QByteArray>( i, "Binary Data" ) );
					if ( sys.valid && !sys.constraints.isEmpty() )
						systems << int( b );
				}
			}
			/* Shown whenever anything is running, not only when there is a choice:
			 * it is the one place that names which system is on screen now that
			 * the status line carries counts alone.
			 *
			 * Only systems that can actually BE simulated are listed. It used to
			 * offer every bhkPhysicsSystem in the file, and picking a jointless
			 * one stopped the running sim, failed, and said nothing -- which is
			 * how a panel came to show bhkRagdollSystem [7] in its status and
			 * bhkPhysicsSystem [8] in its picker at the same time.
			 */
			sysCombo->setVisible( on );
			if ( on ) {
				if ( sysCombo->count() != systems.size() ) {
					sysCombo->clear();
					for ( int b : systems )
						sysCombo->addItem( tr( "%1 [%2]" )
							.arg( m_nif->itemName( m_nif->getBlockIndex( b ) ) ).arg( b ), b );
				}
				const int idx = sysCombo->findData( pv.systemBlock() );
				if ( idx >= 0 )
					sysCombo->setCurrentIndex( idx );
				sysCombo->setEnabled( systems.size() > 1 );
			}
		}

		// only the active tool's parameters, so the panel is not a wall of spin
		// boxes for settings that do not apply
		const PhysicsPreview::ToolSettings & ts = pv.settings();
		const PhysicsPreview::Tool t = pv.tool();
		const bool grabby = ( t == PhysicsPreview::Tool::Grab );
		const bool shooty = ( t == PhysicsPreview::Tool::Shoot );
		const bool blasty = ( t == PhysicsPreview::Tool::Blast );
		const bool windy  = ( t == PhysicsPreview::Tool::Wind );
		const bool punty  = ( t == PhysicsPreview::Tool::Punt );
		const bool ballsy = ( t == PhysicsPreview::Tool::Prop );
		firmSpin->setVisible( grabby );  firmLbl->setVisible( grabby );
		noCollideChk->setVisible( grabby );
		puntStrength->setVisible( punty ); puntLbl->setVisible( punty );
		puntPullChk->setVisible( punty );
		for ( QWidget * w : { (QWidget *)propRadius, (QWidget *)propRadiusLbl,
				(QWidget *)propMass, (QWidget *)propMassLbl, (QWidget *)propSpeed,
				(QWidget *)propSpeedLbl, (QWidget *)clearPropsBtn } )
			w->setVisible( ballsy );
		clearPropsBtn->setEnabled( pv.propCount() > 0 );
		impulseSpin->setVisible( shooty && !ts.shootProjectile );
		impulseLbl->setVisible( shooty && !ts.shootProjectile );
		projChk->setVisible( shooty );
		for ( QWidget * w : { (QWidget *)projSpeed, (QWidget *)projSpeedLbl, (QWidget *)projMass,
				(QWidget *)projMassLbl, (QWidget *)projRadius, (QWidget *)projRadiusLbl,
				(QWidget *)projGrav } )
			w->setVisible( shooty && ts.shootProjectile );
		blastRadius->setVisible( blasty );   blastRadiusLbl->setVisible( blasty );
		blastStrength->setVisible( blasty ); blastStrengthLbl->setVisible( blasty );
		windStrength->setVisible( windy );   windLbl->setVisible( windy );
		// ...but never in the manager, which has no tool row for them to belong to
		toolParams->setVisible( on && m_mode == Mode::Essentials );
		QSignalBlocker p0( firmSpin ), p1( impulseSpin ), p2( projChk ), p3( projSpeed );
		QSignalBlocker p4( projMass ), p5( projRadius ), p6( projGrav ), p7( blastRadius );
		QSignalBlocker p8( blastStrength ), p9( windStrength );
		QSignalBlocker q0( noCollideChk ), q1( puntStrength ), q2( puntPullChk );
		QSignalBlocker q3( propRadius ), q4( propMass ), q5( propSpeed );
		noCollideChk->setChecked( pv.dragNoCollide() );
		puntStrength->setValue( double( ts.puntStrength ) );
		puntPullChk->setChecked( ts.puntPull );
		propRadius->setValue( double( ts.propRadius ) );
		propMass->setValue( double( ts.propMass ) );
		propSpeed->setValue( double( ts.propSpeed ) );
		/* Read BACK through the same mapping the dial writes, so a preset that
		 * moved firmness shows on the dial instead of leaving it displaying
		 * whatever it was last set to.
		 */
		firmSpin->setValue( double( ( ts.grabFirmness - 0.15f ) / 0.85f ) );
		impulseSpin->setValue( double( ts.shootImpulse ) );
		projChk->setChecked( ts.shootProjectile );
		projSpeed->setValue( double( ts.projectileSpeed ) );
		projMass->setValue( double( ts.projectileMass ) );
		projRadius->setValue( double( ts.projectileRadius ) );
		projGrav->setChecked( ts.projectileGravity );
		blastRadius->setValue( double( ts.blastRadius ) );
		blastStrength->setValue( double( ts.blastStrength ) );
		windStrength->setValue( double( ts.windStrength ) );
	};

	/* The action is optional: a host with no Show Collision action to drive gets
	 * the checkbox hidden, rather than a checkbox that does nothing.
	 */
	if ( m_showAction ) {
		connect( showChk, &QCheckBox::toggled, this, [this]( bool on ) {
			if ( m_showAction->isChecked() != on )
				m_showAction->trigger();
		} );
		// and back the other way, so toggling it anywhere else updates the panel
		connect( m_showAction, &QAction::toggled, this,
			[showChk]( bool on ) {
				QSignalBlocker b( showChk );
				showChk->setChecked( on );
			} );
	} else {
		showChk->hide();
		showRule->hide();
	}

	connect( runBtn, &QPushButton::clicked, this, [this, syncCollisionPanel]( bool want ) {
		if ( want ) {
			m_ogl->setRiggingWeightPaintMode( false );
			m_ogl->setVertexPaintMode( false );
			m_ogl->setSegmentPaintMode( false );
			m_ogl->setPoseMode( false );
			m_ogl->setEditMode( false );
			if ( !m_ogl->setPhysicsSimMode( true ) )
				QMessageBox::information( this, tr( "Physics Sim" ),
					tr( "This file has no jointed collision to simulate.\n\n"
						"Physics Sim runs a ragdoll: it needs a bhkRagdollSystem, or a "
						"physics system whose bodies are joined by constraints." ) );
		} else {
			m_ogl->setPhysicsSimMode( false );
		}
		syncCollisionPanel();
	} );
	connect( toolGroup, &QButtonGroup::idClicked, this,
		[this, syncCollisionPanel]( int id ) {
			m_ogl->physicsSim().setTool( PhysicsPreview::Tool( id ) );
			syncCollisionPanel();
		} );
	connect( pauseBtn, &QPushButton::clicked, this,
		[this, syncCollisionPanel]( bool on ) {
			m_ogl->physicsSim().setPaused( on );
			syncCollisionPanel();
		} );
	connect( stepBtn, &QPushButton::clicked, this, [this]() {
		PhysicsPreview & pv = m_ogl->physicsSim();
		const bool was = pv.paused();
		pv.setPaused( false );
		pv.step( 1.0f / 60.0f );
		pv.setPaused( was );
		m_ogl->setCollisionPreview( pv.soup() );
	} );
	connect( freezeBtn, &QPushButton::clicked, this, [this]() {
		m_ogl->physicsSim().freeze();
	} );
	connect( resetBtn, &QPushButton::clicked, this, [this]() {
		m_ogl->physicsSim().reset();
		m_ogl->setCollisionPreview( m_ogl->physicsSim().soup() );
		m_ogl->update();
	} );
	connect( gravChk, &QCheckBox::toggled, this,
		[this, syncCollisionPanel]( bool on ) {
			m_ogl->physicsSim().setGravityEnabled( on );
			syncCollisionPanel();
		} );
	connect( gravSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setGravityStrength( float( v ) );
	} );
	connect( speedSlider, &QSlider::valueChanged, this,
		[this, speedLbl]( int v ) {
			m_ogl->physicsSim().setTimeScale( float( v ) / 100.0f );
			speedLbl->setText( tr( "Speed  %1x" ).arg( v / 100.0, 0, 'g', 2 ) );
		} );
	connect( groundChk, &QCheckBox::toggled, this,
		[this, syncCollisionPanel]( bool on ) {
			m_ogl->physicsSim().setGroundEnabled( on );
			syncCollisionPanel();
		} );
	connect( groundSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setGroundHeight( float( v ) );
	} );
	connect( selfChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().setSelfCollision( on );
	} );
	connect( limitsChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().setAngularLimits( on );
	} );
	connect( statsChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->setPhysicsStatsShown( on );
	} );
	connect( gripSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setGroundFriction( float( v ) );
	} );
	connect( groundVisChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().setGroundVisible( on );
		m_ogl->update();
	} );
	connect( groundResetBtn, &QPushButton::clicked, this,
		[this, syncCollisionPanel]() {
			m_ogl->physicsSim().resetGroundHeight();
			syncCollisionPanel();
			m_ogl->update();
		} );
	connect( hiLimitsChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().setHighlightLimits( on );
		m_ogl->update();
	} );
	/* Grip drives both halves of the grab.
	 *
	 * 0.15 + 0.85g and 3 + 25g, so the default of 0.9 gives firmness 0.915 and
	 * 25.5 times body weight -- within a hair of the 0.9 and 25 the two separate
	 * dials shipped with, so the tool feels exactly as it did.
	 */
	connect( firmSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		PhysicsPreview & pv = m_ogl->physicsSim();
		const float g = float( std::clamp( v, 0.0, 1.0 ) );
		pv.settings().grabFirmness = 0.15f + 0.85f * g;
		pv.settings().grabStrength = 3.0f + 25.0f * g;
		pv.sim().dragStrength = pv.settings().grabStrength;
	} );
	connect( impulseSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().shootImpulse = float( v );
	} );
	connect( projChk, &QCheckBox::toggled, this,
		[this, syncCollisionPanel]( bool on ) {
			m_ogl->physicsSim().settings().shootProjectile = on;
			syncCollisionPanel();
		} );
	connect( projSpeed, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().projectileSpeed = float( v );
	} );
	connect( projMass, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().projectileMass = float( v );
	} );
	connect( projRadius, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().projectileRadius = float( v );
	} );
	connect( projGrav, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().settings().projectileGravity = on;
	} );
	connect( blastRadius, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().blastRadius = float( v );
	} );
	connect( blastStrength, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().blastStrength = float( v );
	} );
	connect( windStrength, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().windStrength = float( v );
	} );
	connect( noCollideChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().setDragNoCollide( on );
	} );
	connect( puntStrength, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().puntStrength = float( v );
	} );
	connect( puntPullChk, &QCheckBox::toggled, this, [this]( bool on ) {
		m_ogl->physicsSim().settings().puntPull = on;
	} );
	connect( propRadius, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().propRadius = float( v );
	} );
	connect( propMass, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().propMass = float( v );
	} );
	connect( propSpeed, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().settings().propSpeed = float( v );
	} );
	connect( clearPropsBtn, &QPushButton::clicked, this,
		[this, syncCollisionPanel]() {
			m_ogl->physicsSim().clearProps();
			m_ogl->setCollisionPreview( m_ogl->physicsSim().soup() );
			m_ogl->update();
			syncCollisionPanel();
		} );

	connect( fricSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setFriction( float( v ) );
	} );
	connect( dampSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setDamping( float( v ) );
	} );
	connect( bounceSpin, &QDoubleSpinBox::valueChanged, this, [this]( double v ) {
		m_ogl->physicsSim().setRestitution( float( v ) );
	} );
	connect( iterSpin, &QSpinBox::valueChanged, this, [this]( int v ) {
		m_ogl->physicsSim().setIterations( v );
	} );
	connect( subSpin, &QSpinBox::valueChanged, this, [this]( int v ) {
		m_ogl->physicsSim().setSubsteps( v );
	} );

	/* Tilt and heading go in as a direction, which is why they share one
	 * handler: neither means anything without the other.
	 */
	auto applyGravityDir = [this, tiltSpin, headSpin]() {
		const double tilt = tiltSpin->value() * M_PI / 180.0;
		const double head = headSpin->value() * M_PI / 180.0;
		const double s = std::sin( tilt );
		m_ogl->physicsSim().setGravityDirection( Vector3( float( s * std::cos( head ) ),
			float( s * std::sin( head ) ), float( -std::cos( tilt ) ) ) );
	};
	connect( tiltSpin, &QDoubleSpinBox::valueChanged, this,
		[applyGravityDir]( double ) { applyGravityDir(); } );
	connect( headSpin, &QDoubleSpinBox::valueChanged, this,
		[applyGravityDir]( double ) { applyGravityDir(); } );

	connect( unpinBtn, &QPushButton::clicked, this, [this, syncCollisionPanel]() {
		m_ogl->physicsSim().unpinAll();
		m_ogl->update();
		syncCollisionPanel();
	} );
	connect( captureBtn, &QPushButton::clicked, this, [this]() {
		const int moved = m_ogl->physicsCapturePose();
		if ( moved > 0 ) {
			emit m_ogl->gizmoStatus( tr( "Captured the simulated pose onto %1 nodes" )
				.arg( moved ) );
		} else {
			QMessageBox::information( this, tr( "Capture pose" ),
				tr( "Nothing to capture.\n\n"
					"A pose is written through the bhkNPCollisionObject that binds each "
					"body to a node. This file's bodies are not bound to any." ) );
		}
	} );

	connect( recBtn, &QPushButton::clicked, this,
		[this, syncCollisionPanel]( bool on ) {
			m_ogl->physicsSim().setRecording( on );
			syncCollisionPanel();
		} );
	connect( scrub, &QSlider::valueChanged, this,
		[this, syncCollisionPanel]( int v ) {
			PhysicsPreview & pv = m_ogl->physicsSim();
			if ( pv.frameCount() < 2 )
				return;
			pv.seek( v );
			m_ogl->setCollisionPreview( pv.soup() );
			m_ogl->update();
			syncCollisionPanel();
		} );
	connect( liveBtn, &QPushButton::clicked, this, [this, syncCollisionPanel]() {
		PhysicsPreview & pv = m_ogl->physicsSim();
		pv.resumeLive();
		pv.setPaused( false );
		syncCollisionPanel();
	} );

	connect( sysCombo, &QComboBox::activated, this,
		[this, sysCombo, syncCollisionPanel]( int idx ) {
			const int block = sysCombo->itemData( idx ).toInt();
			if ( block >= 0 && m_ogl->physicsSimActive() )
				m_ogl->setPhysicsSimMode( true, block );
			syncCollisionPanel();
		} );

	/* Presets set several controls at once and then re-sync, so the panel shows
	 * what they did rather than leaving the widgets stale.
	 */
	connect( presets, &QComboBox::activated, this,
		[this, presets, syncCollisionPanel]( int idx ) {
			PhysicsPreview & pv = m_ogl->physicsSim();
			switch ( idx ) {
			case 1:     // zero gravity: look at the joints, not at a heap
				pv.setGravityEnabled( false );
				pv.setGroundEnabled( false );
				pv.setTimeScale( 1.0f );
				pv.setHighlightLimits( true );
				break;
			case 2:     // slow motion: watch a pop, and keep it
				pv.setTimeScale( 0.15f );
				pv.setRecording( true );
				break;
			case 3:     // drop and settle
				pv.setGravityEnabled( true );
				pv.setGravityStrength( 9.81f );
				pv.setGroundEnabled( true );
				pv.setGroundVisible( true );
				pv.resetGroundHeight();
				pv.setGroundFriction( 1.0f );
				pv.setDamping( 0.4f );
				pv.setTimeScale( 1.0f );
				pv.reset();
				break;
			case 4:     // ice
				pv.setGroundEnabled( true );
				pv.setGroundFriction( 0.0f );
				pv.setFriction( 0.0f );
				break;
			case 5:     // stiff and stable: buy accuracy with time
				pv.setIterations( 8 );
				pv.setSubsteps( 16 );
				pv.setDamping( 0.2f );
				break;
			default:
				return;
			}
			presets->setCurrentIndex( 0 );
			m_ogl->setCollisionPreview( pv.soup() );
			m_ogl->update();
			syncCollisionPanel();
		} );

	/* Essentials keeps what you touch every few seconds and drops the rest.
	 *
	 * Hidden rather than never built: sync() then refreshes every control without
	 * having to know which host it is in, and a control that exists but is not
	 * shown costs a pointer. Qt layouts skip hidden children, so the dropdown is
	 * genuinely short rather than short with gaps in it.
	 */
	QPushButton * openMgr = new QPushButton( tr( "More collision tools..." ), this );
	openMgr->setToolTip( tr( "Open the Collision Manager, which carries the world and "
							 "solver settings, the body list, recording and capture." ) );
	col->addWidget( openMgr );
	/* Slack goes to the BOTTOM, not between the rows.
	 *
	 * The minimum above is the worst case across all six tools, so on a tool with
	 * fewer parameter rows the panel has height to spare -- and a QVBoxLayout
	 * with nowhere to put it spreads it evenly, which pushes every heading a
	 * finger's width from the thing it labels.
	 */
	col->addStretch( 1 );
	connect( openMgr, &QPushButton::clicked, this,
		[this]() { emit openManagerRequested(); } );

	if ( m_mode == Mode::Essentials ) {
		/* The dropdown keeps World, because gravity, speed and the floor are
		 * watched-while-running controls -- gravity has had a keyboard shortcut
		 * since before this panel existed, which is a fair sign of how often it
		 * is wanted. Solver and Advanced stay behind in the manager.
		 */
		for ( QWidget * w : { (QWidget *)recRow, (QWidget *)actsRow,
				(QWidget *)solverHead, (QWidget *)solverRow, (QWidget *)advToggle,
				(QWidget *)advRow, (QWidget *)flagsRow, (QWidget *)legend } )
			w->hide();
	} else {
		openMgr->hide();
		// the tool row belongs to the toolbar; see above
		for ( QWidget * w : { (QWidget *)toolHead, (QWidget *)toolHint,
				(QWidget *)toolsRow, (QWidget *)toolParams } )
			w->hide();
	}

	/* The WIDTH is pinned to the worst case across all six tools, measured here
	 * while every parameter row is still visible. A popup that changed width
	 * when you picked a different tool would move the buttons under the cursor.
	 * Height is left to sync(), which knows which rows are actually up.
	 */
	col->activate();
	setMinimumWidth( col->minimumSize().width() );

	m_sync = syncCollisionPanel;
	syncCollisionPanel();
}

void PhysicsSimPanel::sync()
{
	if ( !m_sync )
		return;
	m_sync();

	/* Then re-fit, because sync() is what decides which rows are up.
	 *
	 * A QMenu sizes a widget action to the widget's hint and will hand it LESS
	 * than the layout needs, which does not clip -- it draws the rows on top of
	 * each other. Pinning the height to what the layout requires prevents that,
	 * and doing it here rather than once in the constructor means a tool with
	 * three parameter rows does not open a popup sized for one with five.
	 *
	 * The host syncs on aboutToShow, which is emitted before the popup is
	 * measured, so this lands in time for the size it opens at.
	 */
	if ( QLayout * l = layout() ) {
		l->activate();
		setMinimumHeight( l->minimumSize().height() );
		// a QWidgetAction sizes from the widget's cached hint, so changing the
		// constraint without saying so leaves the popup a row short
		updateGeometry();
	}
	// and the menu itself has to re-measure -- both when the tool changed while
	// it was open, and before it opens at all
	if ( QMenu * menu = qobject_cast<QMenu *>( parentWidget() ) )
		menu->adjustSize();
}
