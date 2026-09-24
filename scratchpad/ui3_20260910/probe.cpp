/* UI3 probe: WHY ARE THE BAR BUTTONS 39 PX INSIDE A 35 PX ROW?
 *
 * Standalone, ~120 lines, links Qt6Widgets only. It rebuilds the exact stack
 * NifSkope's top row is made of --
 *   - release/style.qss as the APPLICATION sheet (already colour-substituted
 *     at link time, so no skinVars table is needed here),
 *   - a QToolBar named tView, icon size 16x16 (NifSkope::setToolbarSize,
 *     src/nifskope_ui.cpp:30129), min/max height pinned to the row the way
 *     wwAlignBarRow pins it,
 *   - four QToolButtons in ToolButtonTextBesideIcon with InstantPopup menus
 *     and their OWN stylesheet, wwBoxedButtonQss("3px 6px")
 *     (src/nifskope_ui.cpp:520, applied at :27540 and :26929),
 *   - the bar-row child sheet appended to the bar, as wwAlignBarRow does.
 *
 * and prints every button's height under each candidate sheet, so the fix is
 * chosen against a measurement instead of against Qt's documentation.
 *
 * Run:  release/ui3_probe.exe            (default: the offscreen platform)
 */
#include <QApplication>
#include <QFile>
#include <QFontMetrics>
#include <QMainWindow>
#include <QMenu>
#include <QPixmap>
#include <QPainter>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QLayout>
#include <QStyle>

static const int ROW = 35;

//! wwBoxedButtonQss, colours dropped -- only the box matters for a height.
static QString boxedButtonQss( const QString & padding )
{
	return QStringLiteral(
		"QToolButton { padding: %1; border: 1px solid transparent; border-radius: 3px;"
		" background: transparent; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" ).arg( padding );
}

//! BUILD12's shipped arithmetic -- the sheet that produced 39.
static QString oldBarSheet( int rowHeight )
{
	const int inner = qMax( 12, rowHeight - 4 );
	const int pad = qMax( 2, ( rowHeight - 18 ) / 2 );
	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding: %2px 8px; }"
		"QToolButton { min-height: %1px; padding: %2px 4px; }" )
		.arg( inner ).arg( pad );
}

//! The candidate: one box for the whole row -- the glyph line's height and the
//! air above and below it, and nothing horizontal, so each button keeps its own
//! width. `contentHeight` is what wwAlignBarRow calibrates.
static const int PAD = 2;

static QString newBarSheet( int contentHeight )
{
	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }" )
		.arg( contentHeight ).arg( PAD );
}

struct Case { const char * name; QString barSheet; bool alsoOnButtons; };

int main( int argc, char ** argv )
{
	QApplication app( argc, argv );

	QString sheet;
	QFile f( QStringLiteral( "release/style.qss" ) );
	if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
		sheet = QString::fromUtf8( f.readAll() );
	app.setStyleSheet( sheet );

	QTextStream out( stdout );
	out << "style.qss loaded: " << sheet.size() << " chars\n";
	out << "font: " << app.font().family() << " " << app.font().pointSize()
		<< "  fm.height=" << QFontMetrics( app.font() ).height() << "\n";

	const QStringList labels = { QStringLiteral( "Workspaces" ), QStringLiteral( "LOD" ),
								 QStringLiteral( "Animation" ), QStringLiteral( "Collision" ) };

	const QString boxQss = boxedButtonQss( QStringLiteral( "3px 6px" ) );
	int lineHeight = 0;

	QList<Case> cases = {
		{ "no bar sheet at all (the naked button)", QString(), false },
		{ "BUILD12's shipped sheet, on the bar", oldBarSheet( ROW ), false },
		{ "BUILD12's shipped sheet, on the bar AND the buttons", oldBarSheet( ROW ), true },
		{ "UI3 candidate, content = ROW (the calibration pass)", newBarSheet( ROW ), true },
		{ "UI3 candidate, content = ROW - 9 (the calibrated sheet)", newBarSheet( ROW - 9 ), true },
		{ "UI3 candidate, TWO-PASS calibration in the code", QString(), true },
	};

	for ( int ci = 0; ci < cases.size(); ci++ ) {
		Case & c = cases[ci];

		QMainWindow win;
		QToolBar * bar = new QToolBar( &win );
		bar->setObjectName( QStringLiteral( "tView" ) );
		bar->setIconSize( QSize( 16, 16 ) );
		win.addToolBar( bar );

		QPixmap px( 16, 16 );
		px.fill( Qt::red );

		QList<QToolButton *> buttons;
		for ( const QString & l : labels ) {
			auto * b = new QToolButton( &win );
			b->setPopupMode( QToolButton::InstantPopup );
			b->setText( l );
			b->setIcon( QIcon( px ) );
			b->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
			b->setAutoRaise( false );
			b->setStyleSheet( boxQss );
			b->setMenu( new QMenu( b ) );
			bar->addWidget( b );
			buttons << b;
		}

		if ( lineHeight <= 0 )
			lineHeight = qMax( bar->iconSize().height(), QFontMetrics( bar->font() ).height() );

		// wwAlignBarRow: pin the bar, then append the child sheet
		bar->setProperty( "wwBarRow", true );
		bar->setMinimumHeight( ROW );
		bar->setMaximumHeight( ROW );

		const QString barSaved = bar->styleSheet();
		QStringList btnSaved;
		for ( QToolButton * b : buttons )
			btnSaved << b->styleSheet();
		auto apply = [&]( const QString & sheet ) {
			bar->setStyleSheet( barSaved + sheet );
			if ( c.alsoOnButtons )
				for ( int i = 0; i < buttons.size(); i++ )
					buttons[i]->setStyleSheet( btnSaved[i] + sheet );
		};

		if ( ci == 5 ) {
			/* THE TWO-PASS CALIBRATION, exactly as wwAlignBarRow will do it:
			 * ask for a content height that is certainly at least as tall as
			 * any glyph line in the row, read what the style added to it, and
			 * take that much back out of the row. No Qt constant is typed. */
			apply( newBarSheet( ROW ) );
			int overhead = -1;
			for ( QToolButton * b : buttons ) {
				b->ensurePolished();
				overhead = qMax( overhead, b->sizeHint().height() - ROW );
			}
			const int content = ( overhead >= 0 && ROW - overhead >= 8 )
				? ROW - overhead : qMax( 8, ROW - 9 );
			out << "\n    [calibration] overhead measured = " << overhead
				<< ", content = " << content << "\n";
			c.barSheet = newBarSheet( content );
			apply( c.barSheet );
		} else if ( !c.barSheet.isEmpty() ) {
			apply( c.barSheet );
		}

		win.resize( 1200, 300 );
		win.show();
		app.processEvents();
		app.processEvents();

		out << "\n--- case " << ci << ": " << c.name << "\n";
		out << "    bar h=" << bar->height() << "  sheet=\"" << c.barSheet << "\"\n";
		for ( int i = 0; i < buttons.size(); i++ ) {
			QToolButton * b = buttons[i];
			out << "    " << labels[i] << ": height=" << b->height()
				<< " sizeHint=" << b->sizeHint().height()
				<< " minHint=" << b->minimumSizeHint().height() << "\n";
		}
		out.flush();
		win.hide();
	}

	out << "\nline height used = " << lineHeight << " (max of icon 16 and fm.height)\n";

	/* THE MAP: what the toolbar's own layout does with the hint the sheet asks
	 * for. The gate reads height(), not sizeHint(), so this is the function
	 * that has to be inverted. */
	out << "\n--- sweep: content -> sizeHint -> height, bar pinned to " << ROW << "\n";
	{
		QMainWindow win;
		QToolBar * bar = new QToolBar( &win );
		bar->setObjectName( QStringLiteral( "tView" ) );
		bar->setIconSize( QSize( 16, 16 ) );
		win.addToolBar( bar );
		QPixmap px( 16, 16 );
		px.fill( Qt::red );
		QList<QToolButton *> buttons;
		for ( const QString & l : labels ) {
			auto * b = new QToolButton( &win );
			b->setPopupMode( QToolButton::InstantPopup );
			b->setText( l );
			b->setIcon( QIcon( px ) );
			b->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
			b->setAutoRaise( false );
			b->setStyleSheet( boxQss );
			b->setMenu( new QMenu( b ) );
			bar->addWidget( b );
			buttons << b;
		}
		bar->setMinimumHeight( ROW );
		bar->setMaximumHeight( ROW );
		win.resize( 1200, 300 );
		win.show();
		app.processEvents();

		const QMargins lm = bar->layout() ? bar->layout()->contentsMargins() : QMargins();
		out << "    bar contentsMargins " << bar->contentsMargins().top() << "/"
			<< bar->contentsMargins().bottom()
			<< "  layout margins " << lm.top() << "/" << lm.bottom()
			<< "  PM_ToolBarItemMargin " << bar->style()->pixelMetric( QStyle::PM_ToolBarItemMargin, nullptr, bar )
			<< "  PM_ToolBarFrameWidth " << bar->style()->pixelMetric( QStyle::PM_ToolBarFrameWidth, nullptr, bar )
			<< "\n";

		const QString barSaved = bar->styleSheet();
		QStringList btnSaved;
		for ( QToolButton * b : buttons )
			btnSaved << b->styleSheet();
		for ( int content = 18; content <= 40; content++ ) {
			const QString s = newBarSheet( content );
			bar->setStyleSheet( barSaved + s );
			for ( int i = 0; i < buttons.size(); i++ )
				buttons[i]->setStyleSheet( btnSaved[i] + s );
			app.processEvents();
			app.processEvents();
			out << "    content " << content
				<< "  hint " << buttons[0]->sizeHint().height()
				<< "  y " << buttons[0]->y()
				<< "  height " << buttons[0]->height()
				<< ( buttons[0]->height() == ROW ? "   <== the row" : "" ) << "\n";
		}
	}
	out.flush();
	return 0;
}
