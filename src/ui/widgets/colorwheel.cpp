/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "colorwheel.h"
#include "ui/widgets/wwnumberfield.h"

#include "spellbook.h"
#include "data/niftypes.h"
#include "ui/widgets/floatslider.h"
#include "qt5compat.hpp"

#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDialog>
#include <QEventLoop>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QMenu>
#include <QMouseEvent>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QTransform>
#include <QTimer>

#ifdef Q_OS_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <algorithm>
#include <functional>
#include <math.h>
#include <utility>

namespace
{
QString colorChipStyle( const QColor & color )
{
	return QStringLiteral( "QPushButton { background: %1; border: 1px solid #777; border-radius: 2px; }"
		"QPushButton:hover { border: 2px solid white; }" ).arg( color.name( QColor::HexArgb ) );
}

QString colorSliderStyle( const QString & stops )
{
	return QStringLiteral(
		"QSlider::groove:horizontal { height: 8px; border: 1px solid #303030; border-radius: 4px;"
		" background: qlineargradient(x1:0, y1:0, x2:1, y2:0, %1); }"
		"QSlider::handle:horizontal { width: 10px; margin: -4px 0; border: 1px solid #d8d8d8;"
		" border-radius: 5px; background: #f0f0f0; }"
		"QSlider::handle:horizontal:hover { border-color: white; background: white; }" ).arg( stops );
}

QString twoColorStops( const QColor & first, const QColor & last )
{
	return QStringLiteral( "stop:0 %1, stop:1 %2" )
		.arg( first.name( QColor::HexRgb ), last.name( QColor::HexRgb ) );
}

/* The Blender number-field gesture used to be reimplemented here as ColorDragSpinBox.
 *
 * It is now ui/widgets/wwnumberfield.h, shared with every other panel.
 * Its one real reason to exist - integer channels, where a QDoubleSpinBox
 * would print "128.00" - is now a runtime property of the shared field, and
 * its one deliberate improvement (arrows hidden while scrubbing) became
 * canonical for everyone. Its three hardcoded hex colours went with it: this
 * file did not even include wwskin.h, so those stayed dark in the Light theme.
 */

// On Windows the sampler is a tiny mouse-transparent tooltip that follows the
// cursor while GetPixel / GetAsyncKeyState read the real desktop globally.
// Unlike a virtual-desktop overlay this remains fast and can click through to
// other applications. Other platforms retain the screenshot-overlay fallback.
class ScreenColorSampler final : public QDialog
{
public:
	explicit ScreenColorSampler( QWidget * parent = nullptr ) : QDialog( parent )
	{
		setWindowFlags( Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
		setAttribute( Qt::WA_TranslucentBackground );

#ifdef Q_OS_WIN
		setAttribute( Qt::WA_TransparentForMouseEvents );
		setFixedSize( 300, 62 );
		pollTimer = new QTimer( this );
		pollTimer->setTimerType( Qt::PreciseTimer );
		connect( pollTimer, &QTimer::timeout, this, [this]() { pollDesktop(); } );
		positionTooltip( QCursor::pos() );
#else
		setMouseTracking( true );
		setCursor( Qt::CrossCursor );

		QRect desktop;
		for ( QScreen * screen : QGuiApplication::screens() ) {
			Capture capture;
			capture.geometry = screen->geometry();
			QPixmap pixmap = screen->grabWindow( 0 );
			capture.ratio = pixmap.devicePixelRatio();
			capture.image = pixmap.toImage().convertToFormat( QImage::Format_ARGB32 );
			captures.append( capture );
			desktop = desktop.isNull() ? capture.geometry : desktop.united( capture.geometry );
		}
		setGeometry( desktop );
#endif
	}

	QColor selectedColor() const { return selected; }

	int run()
	{
		// Deliberately avoid QDialog::exec(): a nested application-modal sampler
		// can strand focus on an invisible modal window after another application
		// receives the sampling click. The outer color dialog remains visible.
		setWindowModality( Qt::NonModal );
		show();
		QEventLoop loop;
		connect( this, &QDialog::finished, &loop, &QEventLoop::quit );
		loop.exec();
		return result();
	}

protected:
	void showEvent( QShowEvent * event ) override
	{
		QDialog::showEvent( event );
#ifdef Q_OS_WIN
		armed = false;
		leftWasDown = true;
		pollTimer->start( 16 );
#else
		grabMouse();
		grabKeyboard();
#endif
	}

	void mouseMoveEvent( QMouseEvent * event ) override
	{
#ifdef Q_OS_WIN
		QDialog::mouseMoveEvent( event );
#else
		Q_UNUSED( event );
		hover = sampleAt( QCursor::pos() );
		update();
#endif
	}

	void mousePressEvent( QMouseEvent * event ) override
	{
#ifdef Q_OS_WIN
		QDialog::mousePressEvent( event );
#else
		if ( event->button() == Qt::LeftButton ) {
			selected = sampleAt( QCursor::pos() );
			releaseMouse();
			releaseKeyboard();
			accept();
		} else if ( event->button() == Qt::RightButton ) {
			releaseMouse();
			releaseKeyboard();
			reject();
		}
#endif
	}

	void keyPressEvent( QKeyEvent * event ) override
	{
		if ( event->key() == Qt::Key_Escape ) {
#ifndef Q_OS_WIN
			releaseMouse();
			releaseKeyboard();
#endif
			reject();
			return;
		}
		QDialog::keyPressEvent( event );
	}

	void paintEvent( QPaintEvent * event ) override
	{
		Q_UNUSED( event );
#ifdef Q_OS_WIN
		const QRect card = rect().adjusted( 0, 0, -1, -1 );
#else
		const QPoint cursor = mapFromGlobal( QCursor::pos() );
		QRect card( cursor + QPoint( 18, 18 ), QSize( 300, 62 ) );
		if ( card.right() > width() ) card.moveRight( cursor.x() - 18 );
		if ( card.bottom() > height() ) card.moveBottom( cursor.y() - 18 );
#endif
		QPainter painter( this );
		painter.setRenderHint( QPainter::Antialiasing );
		painter.setPen( QColor( 230, 230, 230 ) );
		painter.setBrush( QColor( 32, 32, 32, 235 ) );
		painter.drawRoundedRect( card, 5, 5 );
		QRect swatch = card.adjusted( 8, 8, -244, -8 );
		painter.setBrush( hover );
		painter.drawRect( swatch );
		painter.setBrush( Qt::NoBrush );
		painter.drawText( card.adjusted( 64, 7, -8, -31 ), Qt::AlignVCenter,
			hover.name( QColor::HexRgb ).toUpper() );
		painter.setPen( QColor( 180, 180, 180 ) );
		painter.drawText( card.adjusted( 64, 29, -8, -7 ), Qt::AlignVCenter,
			tr( "Click to sample · Esc to cancel" ) );
	}

private:
#ifdef Q_OS_WIN
	void positionTooltip( const QPoint & cursor )
	{
		QRect screenGeometry;
		if ( QScreen * screen = QGuiApplication::screenAt( cursor ) )
			screenGeometry = screen->geometry();
		else if ( QScreen * primary = QGuiApplication::primaryScreen() )
			screenGeometry = primary->geometry();
		QPoint position = cursor + QPoint( 18, 18 );
		if ( position.x() + width() > screenGeometry.right() ) position.setX( cursor.x() - width() - 18 );
		if ( position.y() + height() > screenGeometry.bottom() ) position.setY( cursor.y() - height() - 18 );
		move( position );
	}

	void pollDesktop()
	{
		const QPoint cursor = QCursor::pos();
		POINT nativeCursor = { cursor.x(), cursor.y() };
		GetCursorPos( &nativeCursor );
		HDC desktop = GetDC( nullptr );
		if ( desktop ) {
			const COLORREF pixel = GetPixel( desktop, nativeCursor.x, nativeCursor.y );
			ReleaseDC( nullptr, desktop );
			if ( pixel != CLR_INVALID )
				hover = QColor( GetRValue( pixel ), GetGValue( pixel ), GetBValue( pixel ) );
		}
		positionTooltip( cursor );
		update();

		// The low bit records a press since the previous call, so even a quick
		// click that begins and ends between two 16 ms ticks is not missed.
		const SHORT leftState = GetAsyncKeyState( VK_LBUTTON );
		const bool leftDown = ( leftState & 0x8000 ) != 0;
		const bool leftPressed = ( leftState & 0x0001 ) != 0;
		if ( !armed ) {
			if ( !leftDown ) { armed = true; leftWasDown = false; }
		} else if ( leftPressed || ( leftDown && !leftWasDown ) ) {
			selected = hover;
			pollTimer->stop();
			accept();
			return;
		}
		leftWasDown = leftDown;

		if ( ( GetAsyncKeyState( VK_RBUTTON ) & 0x8001 ) != 0
			|| ( GetAsyncKeyState( VK_ESCAPE ) & 0x8001 ) != 0 ) {
			pollTimer->stop();
			reject();
		}
	}
#endif

	struct Capture
	{
		QRect geometry;
		QImage image;
		qreal ratio = 1.0;
	};

	QColor sampleAt( const QPoint & global ) const
	{
		for ( const Capture & capture : captures ) {
			if ( !capture.geometry.contains( global ) )
				continue;
			if ( capture.image.isNull() )
				return Qt::black;
			const QPoint logical = global - capture.geometry.topLeft();
			const int x = qBound( 0, qRound( logical.x() * capture.ratio ), capture.image.width() - 1 );
			const int y = qBound( 0, qRound( logical.y() * capture.ratio ), capture.image.height() - 1 );
			return capture.image.pixelColor( x, y );
		}
		return Qt::black;
	}

	QVector<Capture> captures;
	QColor selected;
	QColor hover = Qt::black;
#ifdef Q_OS_WIN
	QTimer * pollTimer = nullptr;
	bool armed = false;
	bool leftWasDown = true;
#endif
};
}

ColorWheel::ColorWheel( QWidget * parent ) : QWidget( parent )
{
	H = 0.0; S = 0.0; V = 1.0;
}

ColorWheel::ColorWheel( const QColor & c, QWidget * parent ) : QWidget( parent )
{
	H = c.hueF();
	S = c.saturationF();
	V = c.valueF();
	A = c.alphaF();

	if ( H >= 1.0 || H < 0.0 )
		H = 0.0;

	if ( S > 1.0 || S < 0.0 )
		S = 1.0;

	if ( V > 1.0 || V < 0.0 )
		V = 1.0;
}

QColor ColorWheel::getColor() const
{
	return QColor::fromHsvF( H, S, V, A );
}

void ColorWheel::setColor( const QColor & c )
{
	double h = c.hueF();
	S = c.saturationF();
	V = c.valueF();
	A = c.alphaF();

	if ( h >= 1.0 || h < 0.0 )
		h = 0.0;

	if ( S > 1.0 || S < 0.0 )
		S = 1.0;

	if ( V > 1.0 || V < 0.0 )
		V = 1.0;

	H = h;
	update();
	emit sigColor( c );
}

bool ColorWheel::getAlpha() const
{
	return isAlpha;
}

void ColorWheel::setAlpha( const bool & b )
{
	isAlpha = b;
}

void ColorWheel::setAlphaValue( const float & f )
{
	A = f;

	setColor( QColor::fromHsvF( H, S, V, A ) );
}

QSize ColorWheel::sizeHint() const
{
	if ( sHint.isValid() )
		return sHint;

	return { 200, 200 };
}

void ColorWheel::setSizeHint( const QSize & s )
{
	sHint = s;
}

void ColorWheel::setDiscMode( bool enabled )
{
	discMode = enabled;
	update();
}

QSize ColorWheel::minimumSizeHint() const
{
	return { 50, 50 };
}

int ColorWheel::heightForWidth( int width ) const
{
	if ( width < minimumSizeHint().height() )
		return minimumSizeHint().height();

	return width;
}

void ColorWheel::paintEvent( QPaintEvent * e )
{
	Q_UNUSED( e );
	double s = qMin( width(), height() ) / 2.0;
	if ( discMode ) {
		QImage image( size(), QImage::Format_ARGB32_Premultiplied );
		image.fill( Qt::transparent );
		const QPointF center( width() * 0.5, height() * 0.5 );
		const double radius = qMax( 1.0, s - 2.0 );
		for ( int py = 0; py < height(); py++ ) {
			QRgb * scan = reinterpret_cast<QRgb *>( image.scanLine( py ) );
			for ( int px = 0; px < width(); px++ ) {
				const double dx = px + 0.5 - center.x();
				const double dy = py + 0.5 - center.y();
				const double distance = std::sqrt( dx * dx + dy * dy );
				if ( distance > radius ) continue;
				double hue = std::atan2( dy, dx ) / ( 2.0 * M_PI );
				if ( hue < 0.0 ) hue += 1.0;
				scan[px] = QColor::fromHsvF( hue, qBound( 0.0, distance / radius, 1.0 ), V ).rgba();
			}
		}
		QPainter painter( this );
		painter.setRenderHint( QPainter::Antialiasing );
		painter.drawImage( 0, 0, image );
		const double angle = H * 2.0 * M_PI;
		const QPointF marker = center + QPointF( std::cos( angle ), std::sin( angle ) ) * ( S * radius );
		painter.setBrush( QColor::fromHsvF( H, S, V ) );
		painter.setPen( QPen( QColor( 245, 245, 245 ), 2.0 ) );
		painter.drawEllipse( marker, 6.0, 6.0 );
		painter.setPen( QPen( QColor( 25, 25, 25 ), 1.0 ) );
		painter.setBrush( Qt::NoBrush );
		painter.drawEllipse( marker, 7.5, 7.5 );
		return;
	}
	double c = s - s / 5;

	QPainter p( this );
	p.translate( width() / 2, height() / 2 );
	p.setRenderHint( QPainter::Antialiasing );

	p.setPen( Qt::NoPen );

	QConicalGradient cgrad( QPointF( 0, 0 ), 90 );
	cgrad.setColorAt( 0.00, QColor::fromHsvF( 0.0, 1.0, 1.0 ) );

	for ( double d = 0.01; d < 1.00; d += 0.01 )
		cgrad.setColorAt( d, QColor::fromHsvF( d, 1.0, 1.0 ) );

	cgrad.setColorAt( 1.00, QColor::fromHsvF( 0.0, 1.0, 1.0 ) );

	p.setBrush( QBrush( cgrad ) );
	p.drawEllipse( QRectF( -s, -s, s * 2, s * 2 ) );
	p.setBrush( palette().color( QPalette::Window ) );
	p.drawEllipse( QRectF( -c, -c, c * 2, c * 2 ) );

	double x = ( H - 0.5 ) * 2 * M_PI;

	QPointF points[3];
	points[0] = QPointF( sin( x ) * c, cos( x ) * c );
	points[1] = QPointF( sin( x + 2 * M_PI / 3 ) * c, cos( x + 2 * M_PI / 3 ) * c );
	points[2] = QPointF( sin( x + 4 * M_PI / 3 ) * c, cos( x + 4 * M_PI / 3 ) * c );

	QColor colors[3][2];
	colors[0][0] = QColor::fromHsvF( H, 1.0, 1.0, 1.0 );
	colors[0][1] = QColor::fromHsvF( H, 0.0, 0.0, 0.0 );
	colors[1][0] = QColor::fromHsvF( H, 0.0, 0.0, 1.0 );
	colors[1][1] = QColor::fromHsvF( H, 0.0, 0.0, 0.0 );
	colors[2][0] = QColor::fromHsvF( H, 0.0, 1.0, 1.0 );
	colors[2][1] = QColor::fromHsvF( H, 0.0, 1.0, 0.0 );


	p.setBrush( QColor::fromHsvF( H, 0.0, .5 ) );
	p.drawPolygon( points, 3 );

	QLinearGradient lgrad( points[0], ( points[1] + points[2] ) / 2 );
	lgrad.setColorAt( 0.0, colors[0][0] );
	lgrad.setColorAt( 1.0, colors[0][1] );
	p.setBrush( lgrad );
	p.drawPolygon( points, 3 );

	lgrad = QLinearGradient( points[1], ( points[0] + points[2] ) / 2 );
	lgrad.setColorAt( 0.0, colors[1][0] );
	lgrad.setColorAt( 1.0, colors[1][1] );
	p.setBrush( lgrad );
	p.drawPolygon( points, 3 );

	lgrad = QLinearGradient( points[2], ( points[0] + points[1] ) / 2 );
	lgrad.setColorAt( 0.0, colors[2][0] );
	lgrad.setColorAt( 1.0, colors[2][1] );
	p.setBrush( lgrad );
	p.drawPolygon( points, 3 );

	p.setPen( QColor::fromHsvF( H, 0.0, 1.0, 0.8 ) );
	p.setBrush( QColor::fromHsvF( H, 1.0, 1.0, 1.0 ) );

	double z = (s - c) / 2;
	QPointF pointH( sin( x ) * ( c + z ), cos( x ) * ( c + z ) );
	p.drawEllipse( QRectF( pointH - QPointF( z / 2, z / 2 ), QSizeF( z, z ) ) );

	p.setBrush( QColor::fromHsvF( H, S, V, 1.0 ) );
	p.rotate( ( 1.0 - H ) * 360 - 120 );
	QPointF pointSV( ( S - 0.5 ) * sqrt( c * c - c * c / 4 ) * 2 * V, V * ( c + c / 2 ) - c );
	p.drawEllipse( QRectF( pointSV - QPointF( z / 2, z / 2 ), QSizeF( z, z ) ) );
}

void ColorWheel::mousePressEvent( QMouseEvent * e )
{
	if ( e->button() != Qt::LeftButton )
		return;

	double x = getQMouseEventPosition( e ).x();
	double y = getQMouseEventPosition( e ).y();
	double dx = abs( x - width() * 0.5 );
	double dy = abs( y - height() * 0.5 );
	double d  = sqrt( dx * dx + dy * dy );

	double s = qMin( width(), height() ) / 2.0;
	double c = s - s / 5;

	if ( d > s )
		pressed = Nope;
	else if ( discMode )
		pressed = Disc;
	else if ( d > c )
		pressed = Circle;
	else
		pressed = Triangle;

	setColor( x, y );
}

void ColorWheel::mouseMoveEvent( QMouseEvent * e )
{
	if ( e->buttons() & Qt::LeftButton )
		setColor( getQMouseEventPosition( e ).x(), getQMouseEventPosition( e ).y() );
}

void ColorWheel::contextMenuEvent( QContextMenuEvent * e )
{
	QMenu * menu = new QMenu( this );

	for ( const QString& name : QColor::colorNames() ) {
		QAction * act = new QAction( menu );
		act->setText( name );
		QPixmap pix( 16, 16 );
		QPainter paint( &pix );
		paint.setBrush( QColor( name ) );
		paint.setPen( Qt::black );
		paint.drawRect( pix.rect().adjusted( 0, 0, -1, -1 ) );
		act->setIcon( QIcon( pix ) );
		menu->addAction( act );
	}

	QAction * hex = new QAction( tr( "Hex Edit..." ), this );
	menu->addSeparator();
	menu->addAction( hex );
	connect( hex, &QAction::triggered, this, &ColorWheel::chooseHex );

	if ( QAction * act = menu->exec( e->globalPos() ) ) {
		if ( act != hex ) {
			setColor( QColor( act->text() ) );
			emit sigColorEdited( getColor() );
		}
	}

	delete menu;
}

void ColorWheel::setColor( double x, double y )
{
	if ( pressed == Disc ) {
		const double dx = x - width() * 0.5;
		const double dy = y - height() * 0.5;
		const double radius = qMax( 1.0, qMin( width(), height() ) * 0.5 - 2.0 );
		H = std::atan2( dy, dx ) / ( 2.0 * M_PI );
		if ( H < 0.0 ) H += 1.0;
		S = qBound( 0.0, std::sqrt( dx * dx + dy * dy ) / radius, 1.0 );
		update();
		emit sigColor( getColor() );
		emit sigColorEdited( getColor() );
	} else if ( pressed == Circle ) {
		QLineF l( QPointF( width() / 2.0, height() / 2.0 ), QPointF( x, y ) );
		H = l.angle() / 360.0 - 0.25;
		H -= std::floor( H );

		update();
		emit sigColor( getColor() );
		emit sigColorEdited( getColor() );
	} else if ( pressed == Triangle ) {
		QPointF mp( x - width() * 0.5, y - height() * 0.5 );

		QTransform m;
		m.rotate( ( H ) * 360.0 + 120 );
		QPointF p( m.map( mp ) );
		double c = qMin( width(), height() ) / 2.0;
		c -= c / 5;
		V  = ( p.y() + c ) / ( c + c / 2 );

		if ( V < 0.0 ) V = 0;
		if ( V > 1.0 ) V = 1.0;

		if ( V > 0 ) {
			double h = V * ( c + c / 2 ) / ( 2.0 * sin( deg2radd(60.0) ) );
			S = ( p.x() + h ) / ( h * 2 );

			if ( S < 0.0 ) S = 0.0;
			if ( S > 1.0 ) S = 1.0;
		} else {
			S = 1.0;
		}

		update();
		emit sigColor( getColor() );
		emit sigColorEdited( getColor() );
	}
}

ColorPickerPanel::ColorPickerPanel( const QColor & color, bool alpha,
	QWidget * parent ) : QWidget( parent ), originalColor( color ),
	currentColor( color ), alphaEnabled( alpha )
{
	setMinimumWidth( 650 );
	setStyleSheet( QStringLiteral(
		"QLabel { color: #dddddd; }"
		"QLineEdit { background: #545454; border: 1px solid #303030; border-radius: 3px;"
		" color: #eeeeee; padding: 3px 6px; selection-background-color: #4772b3; }"
		"QPushButton, QToolButton { background: #454545; border: 1px solid #606060; border-radius: 3px;"
		" color: #e2e2e2; padding: 4px 9px; }"
		"QPushButton:hover, QToolButton:hover { background: #525252; border-color: #777777; }"
		"QPushButton:pressed, QToolButton:pressed { background: #303030; }" ) );
	QVBoxLayout * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 8 );
	QHBoxLayout * content = new QHBoxLayout;
	content->setSpacing( 22 );
	outer->addLayout( content, 1 );

	QVBoxLayout * left = new QVBoxLayout;
	left->setSpacing( 7 );
	content->addLayout( left, 1 );
	QHBoxLayout * previews = new QHBoxLayout;
	previews->setAlignment( Qt::AlignHCenter );
	QPushButton * previousChip = new QPushButton( this );
	QPushButton * currentChip = new QPushButton( this );
	auto addPreview = [&]( QPushButton * chip, const QString & label ) {
		QVBoxLayout * column = new QVBoxLayout;
		column->setSpacing( 2 );
		chip->setFixedSize( 62, 38 );
		column->addWidget( chip, 0, Qt::AlignHCenter );
		QLabel * caption = new QLabel( label, this );
		caption->setAlignment( Qt::AlignHCenter );
		caption->setStyleSheet( QStringLiteral( "color: #bdbdbd;" ) );
		column->addWidget( caption );
		previews->addLayout( column );
	};
	addPreview( previousChip, tr( "Previous" ) );
	QLabel * arrow = new QLabel( QStringLiteral( "›" ), this );
	arrow->setAlignment( Qt::AlignCenter );
	previews->addWidget( arrow );
	addPreview( currentChip, tr( "Current" ) );
	previousChip->setToolTip( tr( "Previous color — click to restore" ) );
	currentChip->setToolTip( tr( "Current color" ) );
	currentChip->setAttribute( Qt::WA_TransparentForMouseEvents );
	previousChip->setStyleSheet( colorChipStyle( originalColor ) );
	left->addLayout( previews );

	ColorWheel * hsv = new ColorWheel( currentColor, this );
	hsv->setAlpha( alphaEnabled );
	hsv->setDiscMode( true );
	hsv->setFixedSize( 250, 250 );
	left->addWidget( hsv, 0, Qt::AlignHCenter );

	QHBoxLayout * paletteTitle = new QHBoxLayout;
	QLabel * paletteLabel = new QLabel( tr( "Palette" ), this );
	paletteLabel->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
	paletteTitle->addWidget( paletteLabel );
	QPushButton * addCustom = new QPushButton( tr( "+  Add" ), this );
	addCustom->setToolTip( tr( "Save the current color to the custom palette" ) );
	paletteTitle->addWidget( addCustom, 0, Qt::AlignRight );
	left->addLayout( paletteTitle );
	QGridLayout * palette = new QGridLayout;
	palette->setSpacing( 3 );
	left->addLayout( palette );
	const QList<QColor> presetColors = {
		Qt::black, QColor( 64, 64, 64 ), QColor( 128, 128, 128 ), QColor( 192, 192, 192 ), Qt::white,
		QColor( 128, 0, 0 ), Qt::red, QColor( 255, 128, 0 ), Qt::yellow, QColor( 128, 128, 0 ),
		QColor( 0, 128, 0 ), Qt::green, QColor( 0, 128, 128 ), Qt::cyan, QColor( 0, 0, 128 ),
		Qt::blue, QColor( 128, 0, 128 ), Qt::magenta, QColor( 255, 128, 192 ), QColor( 128, 64, 0 )
	};

	QVBoxLayout * values = new QVBoxLayout;
	values->setSpacing( 7 );
	content->addLayout( values, 1 );
	QHBoxLayout * hexRow = new QHBoxLayout;
	hexRow->addWidget( new QLabel( tr( "Hex" ), this ) );
	QLineEdit * hex = new QLineEdit( this );
	hex->setMaxLength( 7 );
	hex->setValidator( new QRegularExpressionValidator(
		QRegularExpression( QStringLiteral( "#?[0-9A-Fa-f]{6}" ) ), hex ) );
	hex->setAlignment( Qt::AlignCenter );
	hexRow->addWidget( hex, 1 );
	QToolButton * copyHex = new QToolButton( this );
	copyHex->setText( tr( "Copy" ) );
	copyHex->setToolTip( tr( "Copy the hexadecimal color value" ) );
	hexRow->addWidget( copyHex );
	values->addLayout( hexRow );

	auto makeByteSpin = [this]() {
		QSpinBox * spin = new QSpinBox( this );
		wwMakeScrubField( spin );
		spin->setRange( 0, 255 );
		spin->setFixedWidth( 72 );
		return spin;
	};
	auto makeSlider = [this]( int maximum ) {
		QSlider * slider = new QSlider( Qt::Horizontal, this );
		slider->setRange( 0, maximum );
		slider->setSingleStep( 1 );
		slider->setPageStep( 1 );
		slider->setMinimumWidth( 150 );
		return slider;
	};
	QSpinBox * red = makeByteSpin();
	QSpinBox * green = makeByteSpin();
	QSpinBox * blue = makeByteSpin();
	QSpinBox * hue = new QSpinBox( this );
	wwMakeScrubField( hue );
	hue->setRange( 0, 359 ); hue->setSuffix( QStringLiteral( "°" ) ); hue->setFixedWidth( 72 );
	QSpinBox * saturation = new QSpinBox( this );
	wwMakeScrubField( saturation );
	saturation->setRange( 0, 100 ); saturation->setSuffix( QStringLiteral( "%" ) ); saturation->setFixedWidth( 72 );
	QSpinBox * value = new QSpinBox( this );
	wwMakeScrubField( value );
	value->setRange( 0, 100 ); value->setSuffix( QStringLiteral( "%" ) ); value->setFixedWidth( 72 );
	QSpinBox * opacity = makeByteSpin();
	QSlider * redSlider = makeSlider( 255 );
	QSlider * greenSlider = makeSlider( 255 );
	QSlider * blueSlider = makeSlider( 255 );
	QSlider * hueSlider = makeSlider( 359 );
	QSlider * saturationSlider = makeSlider( 100 );
	QSlider * valueSlider = makeSlider( 100 );
	QSlider * opacitySlider = makeSlider( 255 );
	if ( !alphaEnabled ) {
		opacity->hide();
		opacitySlider->hide();
	}

	auto sectionLabel = [this, values]( const QString & title, const QString & note ) {
		QHBoxLayout * row = new QHBoxLayout;
		QLabel * heading = new QLabel( title, this );
		heading->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
		row->addWidget( heading );
		QLabel * detail = new QLabel( note, this );
		detail->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
		row->addWidget( detail, 0, Qt::AlignRight );
		values->addLayout( row );
		QFrame * line = new QFrame( this );
		line->setFrameShape( QFrame::HLine );
		line->setStyleSheet( QStringLiteral( "color: #555555;" ) );
		values->addWidget( line );
	};
	auto addValueRow = [this, values]( const QString & label,
		QSlider * slider, QSpinBox * spin ) {
		QHBoxLayout * row = new QHBoxLayout;
		row->setSpacing( 8 );
		QLabel * name = new QLabel( label, this );
		name->setFixedWidth( 18 );
		name->setAlignment( Qt::AlignCenter );
		row->addWidget( name );
		row->addWidget( slider, 1 );
		row->addWidget( spin );
		values->addLayout( row );
	};
	sectionLabel( tr( "RGB" ), tr( "0–255" ) );
	addValueRow( tr( "R" ), redSlider, red );
	addValueRow( tr( "G" ), greenSlider, green );
	addValueRow( tr( "B" ), blueSlider, blue );
	sectionLabel( tr( "HSV" ), tr( "Hue · Saturation · Value" ) );
	addValueRow( tr( "H" ), hueSlider, hue );
	addValueRow( tr( "S" ), saturationSlider, saturation );
	addValueRow( tr( "V" ), valueSlider, value );
	if ( alphaEnabled ) {
		sectionLabel( tr( "Alpha" ), tr( "0–255" ) );
		addValueRow( tr( "A" ), opacitySlider, opacity );
	}

	auto linkSlider = [this]( QSlider * slider, QSpinBox * spin ) {
		connect( slider, &QSlider::valueChanged, spin, &QSpinBox::setValue );
		connect( spin, qOverload<int>( &QSpinBox::valueChanged ), slider, &QSlider::setValue );
	};
	linkSlider( redSlider, red );
	linkSlider( greenSlider, green );
	linkSlider( blueSlider, blue );
	linkSlider( hueSlider, hue );
	linkSlider( saturationSlider, saturation );
	linkSlider( valueSlider, value );
	linkSlider( opacitySlider, opacity );

	QPushButton * eyedropper = new QPushButton( tr( "Pick from Screen…" ), this );
	eyedropper->setIcon( QIcon::fromTheme( QStringLiteral( "color-picker" ) ) );
	eyedropper->setToolTip( tr( "Sample a color anywhere on any screen" ) );
	values->addWidget( eyedropper );

	QHBoxLayout * customTitle = new QHBoxLayout;
	QLabel * customLabel = new QLabel( tr( "Custom colors" ), this );
	customLabel->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
	customTitle->addWidget( customLabel );
	QLabel * customHint = new QLabel( tr( "Click Add to save" ), this );
	customHint->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
	customTitle->addWidget( customHint, 0, Qt::AlignRight );
	values->addLayout( customTitle );
	QGridLayout * customGrid = new QGridLayout;
	customGrid->setSpacing( 4 );
	values->addLayout( customGrid );
	values->addStretch();
	QSettings settings;
	customColors = settings.value( QStringLiteral( "ColorDialog/CustomColors" ) ).toStringList();
	while ( customColors.size() < 8 ) customColors.append( QStringLiteral( "#808080" ) );
	QList<QPushButton *> customChips;
	for ( int i = 0; i < 8; i++ ) {
		QPushButton * chip = new QPushButton( this );
		chip->setFixedSize( 34, 24 );
		chip->setStyleSheet( colorChipStyle( QColor( customColors.at( i ) ) ) );
		customGrid->addWidget( chip, i / 8, i % 8 );
		customChips.append( chip );
	}

	if ( !alphaEnabled ) currentColor.setAlpha( 255 );
	applyColor = [=, this]( const QColor & requested, bool updateWheel ) {
		if ( syncing || !requested.isValid() ) return;
		syncing = true;
		currentColor = requested;
		if ( !alphaEnabled ) currentColor.setAlpha( 255 );
		if ( updateWheel ) hsv->setColor( currentColor );
		QSignalBlocker redBlocker( red );
		QSignalBlocker greenBlocker( green );
		QSignalBlocker blueBlocker( blue );
		QSignalBlocker hueBlocker( hue );
		QSignalBlocker saturationBlocker( saturation );
		QSignalBlocker valueBlocker( value );
		QSignalBlocker opacityBlocker( opacity );
		QSignalBlocker redSliderBlocker( redSlider );
		QSignalBlocker greenSliderBlocker( greenSlider );
		QSignalBlocker blueSliderBlocker( blueSlider );
		QSignalBlocker hueSliderBlocker( hueSlider );
		QSignalBlocker saturationSliderBlocker( saturationSlider );
		QSignalBlocker valueSliderBlocker( valueSlider );
		QSignalBlocker opacitySliderBlocker( opacitySlider );
		QSignalBlocker hexBlocker( hex );
		red->setValue( currentColor.red() );
		green->setValue( currentColor.green() );
		blue->setValue( currentColor.blue() );
		hue->setValue( qMax( 0, currentColor.hsvHue() ) );
		saturation->setValue( qRound( currentColor.hsvSaturationF() * 100.0 ) );
		value->setValue( qRound( currentColor.valueF() * 100.0 ) );
		opacity->setValue( currentColor.alpha() );
		redSlider->setValue( currentColor.red() );
		greenSlider->setValue( currentColor.green() );
		blueSlider->setValue( currentColor.blue() );
		hueSlider->setValue( qMax( 0, currentColor.hsvHue() ) );
		saturationSlider->setValue( qRound( currentColor.hsvSaturationF() * 100.0 ) );
		valueSlider->setValue( qRound( currentColor.valueF() * 100.0 ) );
		opacitySlider->setValue( currentColor.alpha() );
		hex->setText( currentColor.name( QColor::HexRgb ).toUpper() );
		currentChip->setStyleSheet( colorChipStyle( currentColor ) );
		redSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor( 0, currentColor.green(), currentColor.blue() ),
			QColor( 255, currentColor.green(), currentColor.blue() ) ) ) );
		greenSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor( currentColor.red(), 0, currentColor.blue() ),
			QColor( currentColor.red(), 255, currentColor.blue() ) ) ) );
		blueSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor( currentColor.red(), currentColor.green(), 0 ),
			QColor( currentColor.red(), currentColor.green(), 255 ) ) ) );
		hueSlider->setStyleSheet( colorSliderStyle( QStringLiteral(
			"stop:0 #ff0000, stop:0.167 #ffff00, stop:0.333 #00ff00, stop:0.5 #00ffff,"
			" stop:0.667 #0000ff, stop:0.833 #ff00ff, stop:1 #ff0000" ) ) );
		const int hsvHue = qMax( 0, currentColor.hsvHue() );
		const int hsvValue = currentColor.value();
		saturationSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor::fromHsv( hsvHue, 0, hsvValue ), QColor::fromHsv( hsvHue, 255, hsvValue ) ) ) );
		valueSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			Qt::black, QColor::fromHsv( hsvHue, currentColor.hsvSaturation(), 255 ) ) ) );
		opacitySlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor( 48, 48, 48 ), currentColor ) ) );
		syncing = false;
		if ( colorChangedCallback ) colorChangedCallback( currentColor );
	};

	connect( hsv, &ColorWheel::sigColor, this, [=, this]( const QColor & wheelColor ) {
		QColor next = wheelColor;
		next.setAlpha( currentColor.alpha() );
		applyColor( next, false );
	} );
	auto rgbChanged = [=, this]() {
		applyColor( QColor( red->value(), green->value(), blue->value(), opacity->value() ), true );
	};
	connect( red, qOverload<int>( &QSpinBox::valueChanged ), this, rgbChanged );
	connect( green, qOverload<int>( &QSpinBox::valueChanged ), this, rgbChanged );
	connect( blue, qOverload<int>( &QSpinBox::valueChanged ), this, rgbChanged );
	auto hsvChanged = [=, this]() {
		QColor next = QColor::fromHsv( hue->value(), qRound( saturation->value() * 2.55 ),
			qRound( value->value() * 2.55 ), opacity->value() );
		applyColor( next, true );
	};
	connect( hue, qOverload<int>( &QSpinBox::valueChanged ), this, hsvChanged );
	connect( saturation, qOverload<int>( &QSpinBox::valueChanged ), this, hsvChanged );
	connect( value, qOverload<int>( &QSpinBox::valueChanged ), this, hsvChanged );
	connect( opacity, qOverload<int>( &QSpinBox::valueChanged ), this, [=, this]() {
		QColor next = currentColor;
		next.setAlpha( opacity->value() );
		applyColor( next, true );
	} );
	connect( hex, &QLineEdit::editingFinished, this, [=, this]() {
		QString text = hex->text();
		if ( !text.startsWith( QLatin1Char( '#' ) ) ) text.prepend( QLatin1Char( '#' ) );
		QColor next( text );
		next.setAlpha( currentColor.alpha() );
		applyColor( next, true );
	} );
	connect( copyHex, &QToolButton::clicked, this, [=]() {
		QApplication::clipboard()->setText( hex->text() );
	} );
	connect( previousChip, &QPushButton::clicked, this, [=, this]() {
		applyColor( originalColor, true );
	} );
	connect( eyedropper, &QPushButton::clicked, this, [=, this]() {
		QWidget * host = window();
		ScreenColorSampler sampler( host );
		if ( sampler.run() == QDialog::Accepted ) {
			QColor sampled = sampler.selectedColor();
			sampled.setAlpha( currentColor.alpha() );
			applyColor( sampled, true );
		}
		if ( host ) {
			host->raise();
			host->activateWindow();
		}
#ifdef Q_OS_WIN
		if ( host ) SetForegroundWindow( reinterpret_cast<HWND>( host->winId() ) );
#endif
	} );

	for ( int i = 0; i < presetColors.size(); i++ ) {
		QPushButton * chip = new QPushButton( this );
		chip->setFixedSize( 27, 20 );
		chip->setStyleSheet( colorChipStyle( presetColors.at( i ) ) );
		palette->addWidget( chip, i / 10, i % 10 );
		connect( chip, &QPushButton::clicked, this, [=, this]() {
			QColor next = presetColors.at( i );
			next.setAlpha( currentColor.alpha() );
			applyColor( next, true );
		} );
	}
	for ( int i = 0; i < customChips.size(); i++ ) {
		connect( customChips.at( i ), &QPushButton::clicked, this, [=, this]() {
			QColor next( customColors.at( i ) );
			next.setAlpha( currentColor.alpha() );
			applyColor( next, true );
		} );
	}
	connect( addCustom, &QPushButton::clicked, this, [=, this]() {
		QSettings settings;
		int slot = settings.value( QStringLiteral( "ColorDialog/NextCustomSlot" ), 0 ).toInt()
			% customChips.size();
		customColors[slot] = currentColor.name( QColor::HexRgb );
		customChips.at( slot )->setStyleSheet( colorChipStyle( currentColor ) );
		settings.setValue( QStringLiteral( "ColorDialog/CustomColors" ), customColors );
		settings.setValue( QStringLiteral( "ColorDialog/NextCustomSlot" ),
			( slot + 1 ) % customChips.size() );
	} );

	applyColor( currentColor, true );
	QLabel * interactionHint = new QLabel(
		tr( "Drag a slider or numeric field; hold Shift for fine control" ), this );
	interactionHint->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
	outer->addWidget( interactionHint );
}

QColor ColorPickerPanel::getColor() const
{
	return currentColor;
}

void ColorPickerPanel::setColor( const QColor & color )
{
	if ( applyColor ) applyColor( color, true );
}

void ColorPickerPanel::setColorChangedCallback(
	std::function<void( const QColor & )> callback )
{
	colorChangedCallback = std::move( callback );
}

QColor ColorWheel::choose( const QColor & c, bool alphaEnable, QWidget * parent )
{
	QDialog dlg( parent );
	dlg.setWindowTitle( tr( "Choose a Color" ) );
	dlg.setMinimumSize( 680, 445 );
	dlg.setStyleSheet( QStringLiteral(
		"QDialog { background: #383838; color: #dddddd; }"
		"QLabel { color: #dddddd; }"
		"QLineEdit { background: #545454; border: 1px solid #303030; border-radius: 3px;"
		" color: #eeeeee; padding: 3px 6px; selection-background-color: #4772b3; }"
		"QPushButton, QToolButton { background: #454545; border: 1px solid #606060; border-radius: 3px;"
		" color: #e2e2e2; padding: 4px 9px; }"
		"QPushButton:hover, QToolButton:hover { background: #525252; border-color: #777777; }"
		"QPushButton:pressed, QToolButton:pressed { background: #303030; }" ) );
	QVBoxLayout * outer = new QVBoxLayout( &dlg );
	outer->setContentsMargins( 12, 12, 12, 10 );
	outer->setSpacing( 10 );
	QHBoxLayout * content = new QHBoxLayout;
	content->setSpacing( 22 );
	outer->addLayout( content, 1 );

	QVBoxLayout * left = new QVBoxLayout;
	left->setSpacing( 7 );
	content->addLayout( left, 1 );
	QHBoxLayout * previews = new QHBoxLayout;
	previews->setAlignment( Qt::AlignHCenter );
	QPushButton * previousChip = new QPushButton( &dlg );
	QPushButton * currentChip = new QPushButton( &dlg );
	auto addPreview = [&]( QPushButton * chip, const QString & label ) {
		QVBoxLayout * column = new QVBoxLayout;
		column->setSpacing( 2 );
		chip->setFixedSize( 62, 38 );
		column->addWidget( chip, 0, Qt::AlignHCenter );
		QLabel * caption = new QLabel( label, &dlg );
		caption->setAlignment( Qt::AlignHCenter );
		caption->setStyleSheet( QStringLiteral( "color: #bdbdbd;" ) );
		column->addWidget( caption );
		previews->addLayout( column );
	};
	addPreview( previousChip, tr( "Previous" ) );
	QLabel * arrow = new QLabel( QStringLiteral( "›" ), &dlg );
	arrow->setAlignment( Qt::AlignCenter );
	previews->addWidget( arrow );
	addPreview( currentChip, tr( "Current" ) );
	previousChip->setToolTip( tr( "Previous color — click to restore" ) );
	currentChip->setToolTip( tr( "Current color" ) );
	currentChip->setAttribute( Qt::WA_TransparentForMouseEvents );
	previousChip->setStyleSheet( colorChipStyle( c ) );
	left->addLayout( previews );

	ColorWheel * hsv = new ColorWheel( c );
	hsv->setAlpha( alphaEnable );
	hsv->setDiscMode( true );
	hsv->setFixedSize( 250, 250 );
	left->addWidget( hsv, 0, Qt::AlignHCenter );

	QHBoxLayout * paletteTitle = new QHBoxLayout;
	QLabel * paletteLabel = new QLabel( tr( "Palette" ), &dlg );
	paletteLabel->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
	paletteTitle->addWidget( paletteLabel );
	QPushButton * addCustom = new QPushButton( tr( "+  Add" ), &dlg );
	addCustom->setToolTip( tr( "Save the current color to the custom palette" ) );
	paletteTitle->addWidget( addCustom, 0, Qt::AlignRight );
	left->addLayout( paletteTitle );
	QGridLayout * palette = new QGridLayout;
	palette->setSpacing( 3 );
	left->addLayout( palette );
	const QList<QColor> presetColors = {
		Qt::black, QColor( 64, 64, 64 ), QColor( 128, 128, 128 ), QColor( 192, 192, 192 ), Qt::white,
		QColor( 128, 0, 0 ), Qt::red, QColor( 255, 128, 0 ), Qt::yellow, QColor( 128, 128, 0 ),
		QColor( 0, 128, 0 ), Qt::green, QColor( 0, 128, 128 ), Qt::cyan, QColor( 0, 0, 128 ),
		Qt::blue, QColor( 128, 0, 128 ), Qt::magenta, QColor( 255, 128, 192 ), QColor( 128, 64, 0 )
	};

	QVBoxLayout * values = new QVBoxLayout;
	values->setSpacing( 7 );
	content->addLayout( values, 1 );
	QHBoxLayout * hexRow = new QHBoxLayout;
	hexRow->addWidget( new QLabel( tr( "Hex" ), &dlg ) );
	QLineEdit * hex = new QLineEdit( &dlg );
	hex->setMaxLength( 7 );
	hex->setValidator( new QRegularExpressionValidator( QRegularExpression( QStringLiteral( "#?[0-9A-Fa-f]{6}" ) ), hex ) );
	hex->setAlignment( Qt::AlignCenter );
	hexRow->addWidget( hex, 1 );
	QToolButton * copyHex = new QToolButton( &dlg );
	copyHex->setText( tr( "Copy" ) );
	copyHex->setToolTip( tr( "Copy the hexadecimal color value" ) );
	hexRow->addWidget( copyHex );
	values->addLayout( hexRow );

	auto makeByteSpin = [&dlg]() {
		QSpinBox * spin = new QSpinBox( &dlg );
		wwMakeScrubField( spin );
		spin->setRange( 0, 255 );
		spin->setFixedWidth( 72 );
		return spin;
	};
	auto makeSlider = [&dlg]( int maximum ) {
		QSlider * slider = new QSlider( Qt::Horizontal, &dlg );
		slider->setRange( 0, maximum );
		slider->setSingleStep( 1 );
		slider->setPageStep( 1 );
		slider->setMinimumWidth( 150 );
		return slider;
	};
	QSpinBox * red = makeByteSpin();
	QSpinBox * green = makeByteSpin();
	QSpinBox * blue = makeByteSpin();
	QSpinBox * hue = new QSpinBox( &dlg ); hue->setRange( 0, 359 ); hue->setSuffix( QStringLiteral( "°" ) ); hue->setFixedWidth( 72 );
	wwMakeScrubField( hue );
	QSpinBox * saturation = new QSpinBox( &dlg ); saturation->setRange( 0, 100 ); saturation->setSuffix( QStringLiteral( "%" ) ); saturation->setFixedWidth( 72 );
	wwMakeScrubField( saturation );
	QSpinBox * value = new QSpinBox( &dlg ); value->setRange( 0, 100 ); value->setSuffix( QStringLiteral( "%" ) ); value->setFixedWidth( 72 );
	wwMakeScrubField( value );
	QSpinBox * opacity = makeByteSpin();
	QSlider * redSlider = makeSlider( 255 );
	QSlider * greenSlider = makeSlider( 255 );
	QSlider * blueSlider = makeSlider( 255 );
	QSlider * hueSlider = makeSlider( 359 );
	QSlider * saturationSlider = makeSlider( 100 );
	QSlider * valueSlider = makeSlider( 100 );
	QSlider * opacitySlider = makeSlider( 255 );
	// Color3 has no alpha row. These widgets still participate in the shared
	// synchronization code, but must not float as orphan children at (0, 0).
	if ( !alphaEnable ) {
		opacity->hide();
		opacitySlider->hide();
	}

	auto sectionLabel = [&dlg, values]( const QString & title, const QString & note ) {
		QHBoxLayout * row = new QHBoxLayout;
		QLabel * heading = new QLabel( title, &dlg );
		heading->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
		row->addWidget( heading );
		QLabel * detail = new QLabel( note, &dlg );
		detail->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
		row->addWidget( detail, 0, Qt::AlignRight );
		values->addLayout( row );
		QFrame * line = new QFrame( &dlg );
		line->setFrameShape( QFrame::HLine );
		line->setStyleSheet( QStringLiteral( "color: #555555;" ) );
		values->addWidget( line );
	};
	auto addValueRow = [&dlg, values]( const QString & label, QSlider * slider, QSpinBox * spin ) {
		QHBoxLayout * row = new QHBoxLayout;
		row->setSpacing( 8 );
		QLabel * name = new QLabel( label, &dlg );
		name->setFixedWidth( 18 );
		name->setAlignment( Qt::AlignCenter );
		row->addWidget( name );
		row->addWidget( slider, 1 );
		row->addWidget( spin );
		values->addLayout( row );
	};
	sectionLabel( tr( "RGB" ), tr( "0–255" ) );
	addValueRow( tr( "R" ), redSlider, red );
	addValueRow( tr( "G" ), greenSlider, green );
	addValueRow( tr( "B" ), blueSlider, blue );
	sectionLabel( tr( "HSV" ), tr( "Hue · Saturation · Value" ) );
	addValueRow( tr( "H" ), hueSlider, hue );
	addValueRow( tr( "S" ), saturationSlider, saturation );
	addValueRow( tr( "V" ), valueSlider, value );
	if ( alphaEnable ) {
		sectionLabel( tr( "Alpha" ), tr( "0–255" ) );
		addValueRow( tr( "A" ), opacitySlider, opacity );
	}

	auto linkSlider = [&dlg]( QSlider * slider, QSpinBox * spin ) {
		connect( slider, &QSlider::valueChanged, spin, &QSpinBox::setValue );
		connect( spin, qOverload<int>( &QSpinBox::valueChanged ), slider, &QSlider::setValue );
	};
	linkSlider( redSlider, red );
	linkSlider( greenSlider, green );
	linkSlider( blueSlider, blue );
	linkSlider( hueSlider, hue );
	linkSlider( saturationSlider, saturation );
	linkSlider( valueSlider, value );
	linkSlider( opacitySlider, opacity );

	QPushButton * eyedropper = new QPushButton( tr( "Pick from Screen…" ), &dlg );
	eyedropper->setIcon( QIcon::fromTheme( QStringLiteral( "color-picker" ) ) );
	eyedropper->setToolTip( tr( "Sample a color anywhere on any screen" ) );
	values->addWidget( eyedropper );

	QHBoxLayout * customTitle = new QHBoxLayout;
	QLabel * customLabel = new QLabel( tr( "Custom colors" ), &dlg );
	customLabel->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
	customTitle->addWidget( customLabel );
	QLabel * customHint = new QLabel( tr( "Click Add to save" ), &dlg );
	customHint->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
	customTitle->addWidget( customHint, 0, Qt::AlignRight );
	values->addLayout( customTitle );
	QGridLayout * customGrid = new QGridLayout;
	customGrid->setSpacing( 4 );
	values->addLayout( customGrid );
	values->addStretch();
	QSettings settings;
	QStringList customColors = settings.value( QStringLiteral( "ColorDialog/CustomColors" ) ).toStringList();
	while ( customColors.size() < 8 ) customColors.append( QStringLiteral( "#808080" ) );
	QList<QPushButton *> customChips;
	for ( int i = 0; i < 8; i++ ) {
		QPushButton * chip = new QPushButton( &dlg );
		chip->setFixedSize( 34, 24 );
		chip->setStyleSheet( colorChipStyle( QColor( customColors.at( i ) ) ) );
		customGrid->addWidget( chip, i / 8, i % 8 );
		customChips.append( chip );
	}

	QColor current = c;
	if ( !alphaEnable ) current.setAlpha( 255 );
	bool syncing = false;
	std::function<void( const QColor &, bool )> setCurrent;
	setCurrent = [&]( const QColor & requested, bool updateWheel ) {
		if ( syncing || !requested.isValid() ) return;
		syncing = true;
		current = requested;
		if ( !alphaEnable ) current.setAlpha( 255 );
		if ( updateWheel ) hsv->setColor( current );
		QSignalBlocker redBlocker( red );
		QSignalBlocker greenBlocker( green );
		QSignalBlocker blueBlocker( blue );
		QSignalBlocker hueBlocker( hue );
		QSignalBlocker saturationBlocker( saturation );
		QSignalBlocker valueBlocker( value );
		QSignalBlocker opacityBlocker( opacity );
		QSignalBlocker redSliderBlocker( redSlider );
		QSignalBlocker greenSliderBlocker( greenSlider );
		QSignalBlocker blueSliderBlocker( blueSlider );
		QSignalBlocker hueSliderBlocker( hueSlider );
		QSignalBlocker saturationSliderBlocker( saturationSlider );
		QSignalBlocker valueSliderBlocker( valueSlider );
		QSignalBlocker opacitySliderBlocker( opacitySlider );
		QSignalBlocker hexBlocker( hex );
		red->setValue( current.red() );
		green->setValue( current.green() );
		blue->setValue( current.blue() );
		hue->setValue( qMax( 0, current.hsvHue() ) );
		saturation->setValue( qRound( current.hsvSaturationF() * 100.0 ) );
		value->setValue( qRound( current.valueF() * 100.0 ) );
		opacity->setValue( current.alpha() );
		redSlider->setValue( current.red() );
		greenSlider->setValue( current.green() );
		blueSlider->setValue( current.blue() );
		hueSlider->setValue( qMax( 0, current.hsvHue() ) );
		saturationSlider->setValue( qRound( current.hsvSaturationF() * 100.0 ) );
		valueSlider->setValue( qRound( current.valueF() * 100.0 ) );
		opacitySlider->setValue( current.alpha() );
		hex->setText( current.name( QColor::HexRgb ).toUpper() );
		currentChip->setStyleSheet( colorChipStyle( current ) );
		redSlider->setStyleSheet( colorSliderStyle( twoColorStops( QColor( 0, current.green(), current.blue() ), QColor( 255, current.green(), current.blue() ) ) ) );
		greenSlider->setStyleSheet( colorSliderStyle( twoColorStops( QColor( current.red(), 0, current.blue() ), QColor( current.red(), 255, current.blue() ) ) ) );
		blueSlider->setStyleSheet( colorSliderStyle( twoColorStops( QColor( current.red(), current.green(), 0 ), QColor( current.red(), current.green(), 255 ) ) ) );
		hueSlider->setStyleSheet( colorSliderStyle( QStringLiteral(
			"stop:0 #ff0000, stop:0.167 #ffff00, stop:0.333 #00ff00, stop:0.5 #00ffff,"
			" stop:0.667 #0000ff, stop:0.833 #ff00ff, stop:1 #ff0000" ) ) );
		const int hsvHue = qMax( 0, current.hsvHue() );
		const int hsvValue = current.value();
		saturationSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			QColor::fromHsv( hsvHue, 0, hsvValue ), QColor::fromHsv( hsvHue, 255, hsvValue ) ) ) );
		valueSlider->setStyleSheet( colorSliderStyle( twoColorStops(
			Qt::black, QColor::fromHsv( hsvHue, current.hsvSaturation(), 255 ) ) ) );
		opacitySlider->setStyleSheet( colorSliderStyle( twoColorStops( QColor( 48, 48, 48 ), current ) ) );
		syncing = false;
	};

	connect( hsv, &ColorWheel::sigColor, &dlg, [&]( const QColor & wheelColor ) {
		QColor next = wheelColor;
		next.setAlpha( current.alpha() );
		setCurrent( next, false );
	} );
	auto rgbChanged = [&]() { setCurrent( QColor( red->value(), green->value(), blue->value(), opacity->value() ), true ); };
	connect( red, qOverload<int>( &QSpinBox::valueChanged ), &dlg, rgbChanged );
	connect( green, qOverload<int>( &QSpinBox::valueChanged ), &dlg, rgbChanged );
	connect( blue, qOverload<int>( &QSpinBox::valueChanged ), &dlg, rgbChanged );
	auto hsvChanged = [&]() {
		QColor next = QColor::fromHsv( hue->value(), qRound( saturation->value() * 2.55 ),
			qRound( value->value() * 2.55 ), opacity->value() );
		setCurrent( next, true );
	};
	connect( hue, qOverload<int>( &QSpinBox::valueChanged ), &dlg, hsvChanged );
	connect( saturation, qOverload<int>( &QSpinBox::valueChanged ), &dlg, hsvChanged );
	connect( value, qOverload<int>( &QSpinBox::valueChanged ), &dlg, hsvChanged );
	connect( opacity, qOverload<int>( &QSpinBox::valueChanged ), &dlg, [&]() {
		QColor next = current; next.setAlpha( opacity->value() ); setCurrent( next, true );
	} );
	connect( hex, &QLineEdit::editingFinished, &dlg, [&]() {
		QString text = hex->text(); if ( !text.startsWith( QLatin1Char( '#' ) ) ) text.prepend( QLatin1Char( '#' ) );
		QColor next( text ); next.setAlpha( current.alpha() ); setCurrent( next, true );
	} );
	connect( copyHex, &QToolButton::clicked, &dlg, [&]() {
		QApplication::clipboard()->setText( hex->text() );
	} );
	connect( previousChip, &QPushButton::clicked, &dlg, [&]() { setCurrent( c, true ); } );
	connect( eyedropper, &QPushButton::clicked, &dlg, [&]() {
		ScreenColorSampler sampler( &dlg );
		if ( sampler.run() == QDialog::Accepted ) {
			QColor sampled = sampler.selectedColor(); sampled.setAlpha( current.alpha() ); setCurrent( sampled, true );
		}
		dlg.raise();
		dlg.activateWindow();
#ifdef Q_OS_WIN
		// The sampling click may activate another process. Bring the still-visible
		// color dialog back so the user can immediately confirm or keep editing.
		SetForegroundWindow( reinterpret_cast<HWND>( dlg.winId() ) );
#endif
	} );

	for ( int i = 0; i < presetColors.size(); i++ ) {
		QPushButton * chip = new QPushButton( &dlg );
		chip->setFixedSize( 27, 20 );
		chip->setStyleSheet( colorChipStyle( presetColors.at( i ) ) );
		palette->addWidget( chip, i / 10, i % 10 );
		connect( chip, &QPushButton::clicked, &dlg, [&, i]() {
			QColor next = presetColors.at( i ); next.setAlpha( current.alpha() ); setCurrent( next, true );
		} );
	}
	for ( int i = 0; i < customChips.size(); i++ ) {
		connect( customChips.at( i ), &QPushButton::clicked, &dlg, [&, i]() {
			QColor next( customColors.at( i ) ); next.setAlpha( current.alpha() ); setCurrent( next, true );
		} );
	}
	connect( addCustom, &QPushButton::clicked, &dlg, [&]() {
		int slot = settings.value( QStringLiteral( "ColorDialog/NextCustomSlot" ), 0 ).toInt() % customChips.size();
		customColors[slot] = current.name( QColor::HexRgb );
		customChips.at( slot )->setStyleSheet( colorChipStyle( current ) );
		settings.setValue( QStringLiteral( "ColorDialog/CustomColors" ), customColors );
		settings.setValue( QStringLiteral( "ColorDialog/NextCustomSlot" ), ( slot + 1 ) % customChips.size() );
	} );

	setCurrent( current, true );
	QHBoxLayout * footer = new QHBoxLayout;
	QLabel * interactionHint = new QLabel( tr( "Drag a slider or numeric field; hold Shift for fine control" ), &dlg );
	interactionHint->setStyleSheet( QStringLiteral( "color: #ababab;" ) );
	footer->addWidget( interactionHint, 1 );
	QDialogButtonBox * buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
	footer->addWidget( buttons );
	outer->addLayout( footer );
	connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

	if ( dlg.exec() == QDialog::Accepted )
		return current;

	return c;
}

Color3 ColorWheel::choose( const Color3 & c, QWidget * parent )
{
	if ( c.red() > 1.0 || c.green() > 1.0 || c.blue() > 1.0 )
		return c;

	return Color3( choose( c.toQColor(), false, parent ) );
}

Color4 ColorWheel::choose( const Color4 & c, QWidget * parent )
{
	if ( c.red() > 1.0 || c.green() > 1.0 || c.blue() > 1.0 || c.alpha() > 1.0 )
		return c;

	return Color4( choose( c.toQColor(), true, parent ) );
}

void ColorWheel::chooseHex()
{
	QDialog * dlg = new QDialog();
	ColorSpinBox * r, * g, * b, * a;
	QGridLayout * grid = new QGridLayout;
	dlg->setLayout( grid );

	//: Red
	grid->addWidget( new QLabel( tr( "R" ) ), 0, 0, 1, 1 );
	grid->addWidget( r = new ColorSpinBox(), 0, 1, 1, 1 );
	r->setSingleStep( 1 );
	r->setRange( 0, 255 );
	r->setValue( getColor().red() );

	//: Green
	grid->addWidget( new QLabel( tr( "G" ) ), 0, 2, 1, 1 );
	grid->addWidget( g = new ColorSpinBox(), 0, 3, 1, 1 );
	g->setSingleStep( 1 );
	g->setRange( 0, 255 );
	g->setValue( getColor().green() );

	//: Blue
	grid->addWidget( new QLabel( tr( "B" ) ), 0, 4, 1, 1 );
	grid->addWidget( b = new ColorSpinBox(), 0, 5, 1, 1 );
	b->setSingleStep( 1 );
	b->setRange( 0, 255 );
	b->setValue( getColor().blue() );

	QLabel * alphaLabel;
	//: Alpha
	grid->addWidget( alphaLabel = new QLabel( tr( "A" ) ), 0, 6, 1, 1 );
	grid->addWidget( a = new ColorSpinBox(), 0, 7, 1, 1 );
	a->setSingleStep( 1 );
	a->setRange( 0, 255 );
	a->setValue( getColor().alpha() );
	alphaLabel->setVisible( getAlpha() );
	a->setVisible( getAlpha() );

	QHBoxLayout * hBox  = new QHBoxLayout;
	QPushButton * btnOk = new QPushButton( tr( "Ok" ) );
	QPushButton * btnCancel = new QPushButton( tr( "Cancel" ) );
	hBox->addWidget( btnOk );
	hBox->addWidget( btnCancel );
	grid->addLayout( hBox, 1, 0, 1, -1 );

	connect( btnOk, &QPushButton::clicked, dlg, &QDialog::accept );
	connect( btnCancel, &QPushButton::clicked, dlg, &QDialog::reject );

	if ( dlg->exec() == QDialog::Accepted ) {
		const QColor temp( r->value(), g->value(), b->value(), a->value() );
		setColor( temp );
		emit sigColorEdited( temp );
	}
}

ColorLineEdit::ColorLineEdit( QWidget * parent ) : QWidget( parent )
{
	QHBoxLayout * layout = new QHBoxLayout( this );

	setLayout( layout );

	title = new QLabel( this );
	title->setText( "Color" );
	title->setSizePolicy( QSizePolicy( QSizePolicy::Maximum, QSizePolicy::MinimumExpanding ) );

	lblColor = new QLabel( this );

	color = new QLineEdit( this );
	color->setText( "#FFFFFF" );
	color->setMaxLength( 7 );
	color->setAlignment( Qt::AlignCenter );
	color->setMaximumWidth( 60 );

	alpha = new QDoubleSpinBox( this );
	alpha->setDecimals( 4 );
	alpha->setMinimum( 0.0 );
	alpha->setMaximum( 1.0 );
	alpha->setSingleStep( 0.01 );
	alpha->setVisible( false );
	alpha->setHidden( true );

	layout->setAlignment( Qt::AlignLeft );
	layout->addWidget( title );
	layout->addWidget( lblColor );
	layout->addWidget( color );
	layout->addWidget( alpha );
}

QColor ColorLineEdit::getColor() const
{
	QColor c = QColor( color->text() );
	if ( hasAlpha )
		c.setAlphaF( alpha->value() );

	return c;
}

void ColorLineEdit::setWheel( ColorWheel * cw, const QString & str )
{
	wheel = cw;

	if ( !str.isEmpty() )
		setTitle( str );

	connect( wheel, &ColorWheel::sigColor, this, &ColorLineEdit::setColor );
	connect( wheel, &ColorWheel::sigColor, [this]() {
		lblColor->setStyleSheet( "background-color: " + wheel->getColor().name( QColor::HexArgb ) + ";" );
	} );

	connect( color, &QLineEdit::cursorPositionChanged, [this]() {
		if ( color->cursorPosition() == 0 ) {
			color->setCursorPosition( 1 );
		}
	} );

	connect( color, &QLineEdit::textEdited, [this]() {
		QString colorTxt = color->text();

		// Prevent "#" deletion
		if ( !color->text().startsWith( "#" ) ) {
			color->setText( "#" + color->text() );
		}

		QColor c = QColor( colorTxt );
		if ( hasAlpha )
			c.setAlphaF( alpha->value() );

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
		if ( (color->text().length() % 2 == 0) || !QColor::isValidColor( colorTxt ) )
			return;
#else
		if ( (color->text().length() % 2 == 0) || !QColor::isValidColorName( colorTxt ) )
			return;
#endif

		QColor wc = wheel->getColor();
		if ( c.toRgb() != wc.toRgb() )
			wheel->setColor( c );

		emit textEdited( colorTxt );
	} );
}

void ColorLineEdit::setTitle( const QString & str )
{
	title->setText( str );
}

void ColorLineEdit::setColor( const QColor & c )
{
	color->setText( c.name( QColor::HexRgb ) );

	if ( hasAlpha )
		alpha->setValue( c.alphaF() );

	QColor wc = wheel->getColor();

	// Sync color wheel
	//	Do NOT compare entire QColor, will create
	//	infinite loop between their ::setColor()
	if ( hasAlpha && c.alphaF() != wc.alphaF() )
		wheel->setColor( c );

	if ( c.red() != wc.red() || c.green() != wc.green() || c.blue() != wc.blue() )
		wheel->setColor( c );
}

void ColorLineEdit::setAlpha( float a )
{
	hasAlpha = true;

	alpha->setValue( a );
}
