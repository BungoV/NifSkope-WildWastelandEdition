#include "livepanel.h"
#include "wwswitch.h"
#include "features/weather/wxoverride.h"
#include "ui/widgets/wwnumberfield.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {

std::function<QString( const QString & )> & runner()
{
	static std::function<QString( const QString & )> r;
	return r;
}

std::function<void()> & repainter()
{
	static std::function<void()> r;
	return r;
}

QString valueNow( const WwSwitchRowSpec & s, bool * pinned )
{
	const QString v = wwEnvStr( s.sw.constData() ).trimmed();
	*pinned = !v.isEmpty();
	if ( *pinned )
		return v;
	return s.fallback ? s.fallback() : QString();
}

int decimalsFor( double step )
{
	if ( step >= 1.0 )
		return 0;
	return std::clamp( int( std::ceil( -std::log10( std::max( step, 1e-6 ) ) - 1e-9 ) ), 1, 6 );
}

QDoubleSpinBox * numberField( const WwSwitchRowSpec & s, QWidget * parent )
{
	auto * f = new QDoubleSpinBox( parent );
	f->setRange( s.min, s.max );
	f->setSingleStep( s.step > 0.0 ? s.step : 0.01 );
	f->setDecimals( decimalsFor( s.step ) );
	f->setKeyboardTracking( false );
	wwMakeScrubField( f );
	return f;
}

const char * stageWord( WwSwitchStage st )
{
	switch ( st ) {
	case WwSwitchStage::Live: return "live";
	case WwSwitchStage::Reopen: return "reopen";
	case WwSwitchStage::Shaders: return "shaders";
	case WwSwitchStage::Tool: return "tool";
	default: return "relaunch";
	}
}

}	// namespace

void wwLivePanelSetHooks( std::function<QString( const QString & )> run, std::function<void()> repaint )
{
	runner() = std::move( run );
	repainter() = std::move( repaint );
}

QString wwSwitchRowSet( const QByteArray & sw, const QString & value, bool unset, QWidget * asker )
{
	QString where;
	const WwSwitchStage st = wwSwitchStageOf( sw, &where );
	QString r;
	if ( st == WwSwitchStage::Live ) {
		r = wwSwitchSet( sw, unset ? QByteArray() : value.toLocal8Bit(), unset );
		if ( repainter() )
			repainter()();
	} else if ( st == WwSwitchStage::Reopen || st == WwSwitchStage::Shaders ) {
		const auto yes = QMessageBox::question( asker, QObject::tr( "Live Panel" ),
			QObject::tr( "%1 applies after a %2 (the cell reloads). Apply now?" ).arg( QString::fromLatin1( sw ),
				QString::fromLatin1( stageWord( st ) ) ) );
		if ( yes != QMessageBox::Yes )
			return QStringLiteral( "cancelled (%1 stage)" ).arg( QString::fromLatin1( stageWord( st ) ) );
		const QString step = unset ? QStringLiteral( "unset %1" ).arg( QString::fromLatin1( sw ) )
								   : QStringLiteral( "set %1=%2" ).arg( QString::fromLatin1( sw ), value );
		if ( runner() ) {
			r = runner()( step );
			if ( r.startsWith( QLatin1String( "ok" ) ) )
				r += QStringLiteral( "; " ) + runner()( QStringLiteral( "apply" ) );
		} else {
			r = wwSwitchSet( sw, unset ? QByteArray() : value.toLocal8Bit(), unset ) + QStringLiteral( " (applies at the next reopen)" );
		}
	} else {
		r = QStringLiteral( "refused (%1 is a relaunch-stage switch)" ).arg( QString::fromLatin1( sw ) );
	}
	std::fprintf( stderr, "live panel: %s %s%s -> %s\n", unset ? "unset" : "set", sw.constData(),
		unset ? "" : qPrintable( QLatin1Char( '=' ) + value ), qPrintable( r ) );
	return r;
}

QWidget * wwSwitchRowWidget( const WwSwitchRowSpec & spec, QWidget * parent )
{
	auto * w = new QWidget( parent );
	auto * lay = new QHBoxLayout( w );
	lay->setContentsMargins( 0, 0, 0, 0 );
	lay->setSpacing( 4 );
	QString where;
	const WwSwitchStage st = wwSwitchStageOf( spec.sw, &where );
	const bool canSet = st == WwSwitchStage::Live || st == WwSwitchStage::Reopen || st == WwSwitchStage::Shaders;
	const QByteArray sw = spec.sw;
	auto send = [sw, w]( const QString & v ) { wwSwitchRowSet( sw, v, false, w ); };

	QList<QWidget *> editors;
	std::function<void( const QString & )> show;
	const QString type = spec.type.toLower();
	if ( type == QLatin1String( "toggle" ) ) {
		auto * cb = new QCheckBox( w );
		QObject::connect( cb, &QCheckBox::toggled, w, [send]( bool on ) { send( on ? QStringLiteral( "1" ) : QStringLiteral( "0" ) ); } );
		show = [cb]( const QString & v ) {
			const QString t = v.trimmed().toLower();
			cb->setChecked( t == QLatin1String( "1" ) || t == QLatin1String( "true" ) || t == QLatin1String( "on" ) );
		};
		editors << cb;
	} else if ( type == QLatin1String( "dropdown" ) ) {
		auto * box = new QComboBox( w );
		box->addItems( spec.options );
		box->setSizeAdjustPolicy( QComboBox::AdjustToContents );
		wwMatchFieldStyle( box );
		QObject::connect( box, &QComboBox::textActivated, w, [send]( const QString & t ) { send( t ); } );
		show = [box]( const QString & v ) { box->setCurrentIndex( box->findText( v.trimmed() ) ); };
		editors << box;
	} else if ( type == QLatin1String( "slider" ) ) {
		auto * sl = new QSlider( Qt::Horizontal, w );
		const double step = spec.step > 0.0 ? spec.step : 0.01;
		const int ticks = int( std::min( 100000.0, std::round( ( spec.max - spec.min ) / step ) ) );
		sl->setRange( 0, std::max( 1, ticks ) );
		sl->setMinimumWidth( 120 );
		auto * f = numberField( spec, w );
		QObject::connect( sl, &QSlider::valueChanged, w, [f, spec, step]( int i ) { f->setValue( spec.min + i * step ); } );
		QObject::connect( f, qOverload<double>( &QDoubleSpinBox::valueChanged ), w, [sl, f, spec, step, send]( double v ) {
			const QSignalBlocker b( sl );
			sl->setValue( int( std::lround( ( v - spec.min ) / step ) ) );
			send( f->textFromValue( v ) );
		} );
		show = [sl, f, spec, step]( const QString & v ) {
			bool ok = false;
			const double d = v.trimmed().toDouble( &ok );
			if ( !ok )
				return;
			const QSignalBlocker b( sl );
			f->setValue( d );
			sl->setValue( int( std::lround( ( d - spec.min ) / step ) ) );
		};
		editors << sl << f;
		lay->addWidget( sl, 1 );
	} else if ( type == QLatin1String( "color" ) ) {
		QList<QDoubleSpinBox *> f;
		auto * swatch = new QLabel( w );
		swatch->setFixedSize( 14, 14 );
		for ( int c = 0; c < 3; c++ )
			f << numberField( spec, w );
		auto paint = [swatch, f]() {
			double m = 1.0;
			for ( auto * x : f )
				m = std::max( m, x->value() );
			QColor col;
			col.setRgbF( float( f[0]->value() / m ), float( f[1]->value() / m ), float( f[2]->value() / m ) );
			swatch->setStyleSheet( QStringLiteral( "background:%1;border:1px solid #555;" ).arg( col.name() ) );
		};
		for ( auto * x : f )
			QObject::connect( x, qOverload<double>( &QDoubleSpinBox::valueChanged ), w, [f, send, paint]() {
				paint();
				send( QStringLiteral( "%1,%2,%3" ).arg( f[0]->textFromValue( f[0]->value() ), f[1]->textFromValue( f[1]->value() ),
					f[2]->textFromValue( f[2]->value() ) ) );
			} );
		show = [f, paint]( const QString & v ) {
			const QStringList p = v.split( QLatin1Char( ',' ) );
			for ( int c = 0; c < 3; c++ ) {
				bool ok = false;
				const double d = p.value( p.size() == 1 ? 0 : c ).trimmed().toDouble( &ok );
				if ( ok )
					f[c]->setValue( d );
			}
			paint();
		};
		lay->addWidget( swatch );
		for ( auto * x : f )
			editors << x;
	} else {	// number
		auto * f = numberField( spec, w );
		QObject::connect( f, qOverload<double>( &QDoubleSpinBox::valueChanged ), w,
			[f, send]( double v ) { send( f->textFromValue( v ) ); } );
		show = [f]( const QString & v ) {
			bool ok = false;
			const double d = v.trimmed().toDouble( &ok );
			if ( ok )
				f->setValue( d );
		};
		editors << f;
	}
	for ( QWidget * e : editors )
		if ( !qobject_cast<QSlider *>( e ) )
			lay->addWidget( e, type == QLatin1String( "number" ) || type == QLatin1String( "color" ) ? 1 : 0 );
	if ( st != WwSwitchStage::Live ) {
		auto * tag = new QLabel( QStringLiteral( "(%1)" ).arg( QString::fromLatin1( stageWord( st ) ) ), w );
		tag->setToolTip( where );
		lay->addWidget( tag );
	}
	auto * reset = new QToolButton( w );
	reset->setText( QObject::tr( "Reset" ) );
	reset->setToolTip( QObject::tr( "Back to the weather / the default (unset %1)" ).arg( QString::fromLatin1( sw ) ) );
	QObject::connect( reset, &QToolButton::clicked, w, [sw, w]() { wwSwitchRowSet( sw, QString(), true, w ); } );
	lay->addWidget( reset );
	if ( type != QLatin1String( "number" ) && type != QLatin1String( "color" ) && type != QLatin1String( "slider" ) )
		lay->addStretch( 1 );
	for ( QWidget * e : editors )
		e->setEnabled( canSet );
	w->setToolTip( QStringLiteral( "%1 (%2 stage)" ).arg( QString::fromLatin1( sw ), QString::fromLatin1( stageWord( st ) ) ) );

	// the shown value follows the store (a live port set, a reset, the weather's own value while unset)
	auto last = std::make_shared<QString>( QStringLiteral( "\x01" ) );
	auto refresh = [spec, editors, show, reset, last]() {
		for ( QWidget * e : editors )
			if ( e->hasFocus() || ( qobject_cast<QSlider *>( e ) && static_cast<QSlider *>( e )->isSliderDown() ) )
				return;
		bool pinned = false;
		const QString v = valueNow( spec, &pinned );
		reset->setEnabled( pinned );
		if ( v == *last )
			return;
		*last = v;
		QList<bool> was;
		for ( QWidget * e : editors )
			was << e->blockSignals( true );
		show( v );
		for ( int i = 0; i < editors.size(); i++ )
			editors[i]->blockSignals( was[i] );
		for ( QWidget * e : editors ) {
			QFont fnt = e->font();
			fnt.setItalic( !pinned );	// italic: the weather's (or the default) value, not a pin
			e->setFont( fnt );
		}
	};
	refresh();
	auto * t = new QTimer( w );
	t->setInterval( 250 );
	QObject::connect( t, &QTimer::timeout, w, [w, refresh]() {
		if ( w->isVisible() )
			refresh();
	} );
	t->start();
	return w;
}

namespace {

struct Panel
{
	QPointer<QDockWidget> dock;
	QString path;
	QDateTime mtime;
	qint64 size = -1;
};

Panel & panel()
{
	static Panel p;
	return p;
}

QString rebuild()
{
	Panel & P = panel();
	if ( !P.dock )
		return QStringLiteral( "refused (no panel)" );
	const QFileInfo fi( P.path );
	P.mtime = fi.lastModified();
	P.size = fi.size();
	QFile f( P.path );
	QString err;
	QJsonDocument doc;
	if ( !f.open( QIODevice::ReadOnly ) ) {
		err = QStringLiteral( "cannot read %1" ).arg( P.path );
	} else {
		QJsonParseError pe;
		doc = QJsonDocument::fromJson( f.readAll(), &pe );
		if ( pe.error != QJsonParseError::NoError )
			err = QStringLiteral( "%1 at offset %2" ).arg( pe.errorString() ).arg( pe.offset );
	}
	auto * area = new QScrollArea( P.dock );
	area->setWidgetResizable( true );
	auto * body = new QWidget( area );
	auto * v = new QVBoxLayout( body );
	v->setContentsMargins( 6, 6, 6, 6 );
	auto * form = new QFormLayout();
	form->setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );
	form->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
	v->addLayout( form );
	int rows = 0, heads = 0;
	QString title = QObject::tr( "Live Panel" );
	if ( err.isEmpty() ) {
		QJsonArray list;
		if ( doc.isArray() ) {
			list = doc.array();
		} else {
			list = doc.object().value( QStringLiteral( "rows" ) ).toArray();
			const QString t = doc.object().value( QStringLiteral( "title" ) ).toString();
			if ( !t.isEmpty() )
				title = QObject::tr( "Live Panel: %1" ).arg( t );
		}
		for ( const QJsonValue & jv : list ) {
			const QJsonObject o = jv.toObject();
			if ( o.contains( QStringLiteral( "heading" ) ) ) {
				auto * h = new QLabel( o.value( QStringLiteral( "heading" ) ).toString(), body );
				QFont hf = h->font();
				hf.setWeight( QFont::DemiBold );
				h->setFont( hf );
				form->addRow( h );
				heads++;
				continue;
			}
			WwSwitchRowSpec s;
			s.sw = o.value( QStringLiteral( "switch" ) ).toString().trimmed().toLatin1();
			if ( !s.sw.startsWith( "WW_" ) ) {
				form->addRow( new QLabel( QObject::tr( "(skipped: no WW_ switch) %1" )
					.arg( o.value( QStringLiteral( "label" ) ).toString() ), body ) );
				continue;
			}
			s.type = o.value( QStringLiteral( "type" ) ).toString( QStringLiteral( "number" ) );
			s.min = o.value( QStringLiteral( "min" ) ).toDouble( 0.0 );
			s.max = o.value( QStringLiteral( "max" ) ).toDouble( 1.0 );
			s.step = o.value( QStringLiteral( "step" ) ).toDouble( 0.01 );
			for ( const QJsonValue & ov : o.value( QStringLiteral( "options" ) ).toArray() )
				s.options << ( ov.isString() ? ov.toString() : QString::number( ov.toDouble() ) );
			const int wx = wwWxFind( s.sw );	// a weather override shows the weather's own value while unset
			if ( wx >= 0 )
				s.fallback = [wx]() { return wwWxWeatherText( WwWx( wx ) ); };
			const QString label = o.value( QStringLiteral( "label" ) ).toString( QString::fromLatin1( s.sw ) );
			form->addRow( label, wwSwitchRowWidget( s, body ) );
			rows++;
		}
	} else {
		form->addRow( new QLabel( QObject::tr( "Panel file refused: %1" ).arg( err ), body ) );
	}
	v->addStretch( 1 );
	area->setWidget( body );
	QWidget * old = P.dock->widget();
	P.dock->setWidget( area );
	P.dock->setWindowTitle( title );
	if ( old )
		old->deleteLater();
	const QString r = err.isEmpty() ? QStringLiteral( "panel %1: %2 rows, %3 headings" ).arg( P.path ).arg( rows ).arg( heads )
									: QStringLiteral( "refused panel %1: %2" ).arg( P.path, err );
	std::fprintf( stderr, "live panel: built %s\n", qPrintable( r ) );
	return r;
}

}	// namespace

QString wwLivePanelOpen( QMainWindow * mw, const QString & pathIn )
{
	const QString path = pathIn.trimmed().isEmpty() ? wwEnvStr( "WW_LIVEPANEL" ).trimmed() : pathIn.trimmed();
	if ( path.isEmpty() )
		return QStringLiteral( "refused (no panel file: give a path or set WW_LIVEPANEL)" );
	if ( !QFileInfo::exists( path ) )
		return QStringLiteral( "refused (no file %1)" ).arg( path );
	if ( !mw )
		return QStringLiteral( "refused (no main window)" );
	Panel & P = panel();
	P.path = QFileInfo( path ).absoluteFilePath();
	if ( !P.dock ) {
		P.dock = new QDockWidget( QObject::tr( "Live Panel" ), mw );
		P.dock->setObjectName( QStringLiteral( "LivePanelDock" ) );
		mw->addDockWidget( Qt::RightDockWidgetArea, P.dock );
		// the panel rebuilds itself when its file changes (the manager writes a new one while bungo flies)
		auto * watch = new QTimer( P.dock );
		watch->setInterval( 1000 );
		QObject::connect( watch, &QTimer::timeout, P.dock, []() {
			Panel & Q = panel();
			const QFileInfo fi( Q.path );
			if ( fi.exists() && ( fi.lastModified() != Q.mtime || fi.size() != Q.size ) )
				rebuild();
		} );
		watch->start();
	}
	const QString r = rebuild();
	P.dock->show();
	P.dock->raise();
	return r;
}
