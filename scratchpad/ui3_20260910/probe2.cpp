/* UI3 probe 2: can the SKIN zero the toolbar's own layout margin?
 *
 * probe.cpp established the map for the shipped bar: a button's y is pinned at
 * 4 (the QToolBar layout's margin, PM_ToolBarItemMargin 2 + PM_ToolBarFrameWidth
 * 2) and its height is content + 6, so a button that reads 35 px still hangs 4
 * px below a 35 px bar and is clipped. For the button to BE the row it has to
 * start at y 0, which means the bar's own margin has to go -- and it has to go
 * through the sheet, because the rule must live in the skin.
 *
 * Sweeps content 18..40 with and without a `QToolBar { border/padding/margin: 0 }`
 * reset in the bar's own sheet, printing y and height for each.
 */
#include <QApplication>
#include <QFile>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QPixmap>
#include <QStyle>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>

static const int ROW = 35;
static const int PAD = 2;

static QString boxedButtonQss( const QString & padding )
{
	return QStringLiteral(
		"QToolButton { padding: %1; border: 1px solid transparent; border-radius: 3px;"
		" background: transparent; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" ).arg( padding );
}

static QString barRowQss( int contentHeight, bool resetBar )
{
	QString s;
	if ( resetBar )
		s += QStringLiteral( "QToolBar { border: 0px; padding: 0px; margin: 0px; }" );
	s += QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }" )
		.arg( contentHeight ).arg( PAD );
	return s;
}

int main( int argc, char ** argv )
{
	QApplication app( argc, argv );
	QString sheet;
	QFile f( QStringLiteral( "release/style.qss" ) );
	if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
		sheet = QString::fromUtf8( f.readAll() );
	app.setStyleSheet( sheet );

	QTextStream out( stdout );
	const QStringList labels = { QStringLiteral( "Workspaces" ), QStringLiteral( "LOD" ),
								 QStringLiteral( "Animation" ), QStringLiteral( "Collision" ) };
	const QString boxQss = boxedButtonQss( QStringLiteral( "3px 6px" ) );

	for ( int variant = 0; variant < 2; variant++ ) {
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

		out << "\n=== variant " << variant
			<< ( variant ? "  (the sheet zeroes the bar's box)" : "  (as shipped)" ) << "\n";
		const QString barSaved = bar->styleSheet();
		QStringList btnSaved;
		for ( QToolButton * b : buttons )
			btnSaved << b->styleSheet();

		for ( int content = 18; content <= 40; content++ ) {
			const QString s = barRowQss( content, variant != 0 );
			bar->setStyleSheet( barSaved + s );
			for ( int i = 0; i < buttons.size(); i++ )
				buttons[i]->setStyleSheet( btnSaved[i] + s );
			app.processEvents();
			app.processEvents();
			if ( content == 18 )
				out << "    layout margins "
					<< ( bar->layout() ? bar->layout()->contentsMargins().top() : -1 ) << "/"
					<< ( bar->layout() ? bar->layout()->contentsMargins().bottom() : -1 )
					<< "  PM_ToolBarItemMargin "
					<< bar->style()->pixelMetric( QStyle::PM_ToolBarItemMargin, nullptr, bar )
					<< "  PM_ToolBarFrameWidth "
					<< bar->style()->pixelMetric( QStyle::PM_ToolBarFrameWidth, nullptr, bar )
					<< "  bar h " << bar->height() << "\n";
			out << "    content " << content
				<< "  hint " << buttons[0]->sizeHint().height()
				<< "  y " << buttons[0]->y()
				<< "  height " << buttons[0]->height()
				<< ( buttons[0]->y() == 0 && buttons[0]->height() == ROW
					 ? "   <== fills the row" : "" ) << "\n";
		}
		out.flush();
	}
	return 0;
}
