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

#include "ui/widgets/wwnumberfield.h"

#include "ui/widgets/floatedit.h"
#include "wwskin.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSpinBox>

#include <cfloat>
#include <cmath>

//! width of each side gutter, where the ‹ › arrows live
static constexpr int WW_ARROW_W = 16;

//! Marks a widget that already has the gesture, so a bulk retro-fit over a form
//! that contains a hand-built field cannot install it twice.
static const char * const WW_SCRUBBED = "wwScrubbed";

// ---------------------------------------------------------------------------
// WwScrub — the gesture
// ---------------------------------------------------------------------------

WwScrub::WwScrub( QWidget * host, QLineEdit * edit, const WwScrubSpec & spec )
	: QObject( host ), m_host( host ), m_edit( edit ), m_spec( spec )
{
	if ( !edit )
		return;
	edit->installEventFilter( this );
	edit->setMouseTracking( true );
	// THE ONLY Qt::SizeHorCursor IN src/. tests/spells/scrub_uniform.sh counts
	// them, and that count is the check that guards against a sixth copy.
	edit->setCursor( Qt::SizeHorCursor );
	if ( host ) {
		host->setProperty( WW_SCRUBBED, true );
		// a gesture must not outlive the events that drive it
		host->installEventFilter( this );
	}
}

void WwScrub::cancel()
{
	if ( !m_dragging )
		return;
	const bool wasScrubbing = m_moved;
	m_dragging = m_moved = false;
	if ( m_host )
		m_host->update();
	if ( wasScrubbing )
		emit scrubFinished();
}

bool WwScrub::isIntegral() const
{
	if ( m_spec.integral != WwScrubSpec::Auto )
		return m_spec.integral == WwScrubSpec::Yes;
	// Runtime, never two classes: the Ex redo panel expresses an integer
	// parameter as setDecimals(0) on a QDoubleSpinBox, so a separate int class
	// could not serve it.
	if ( qobject_cast<QSpinBox *>( m_host.data() ) )
		return true;
	if ( auto * d = qobject_cast<QDoubleSpinBox *>( m_host.data() ) )
		return d->decimals() == 0;
	return false;
}

double WwScrub::pixelStep() const
{
	if ( m_spec.pixelStep > 0.0 )
		return m_spec.pixelStep;
	double step = 1.0;
	if ( auto * d = qobject_cast<QDoubleSpinBox *>( m_host.data() ) )
		step = d->singleStep();
	else if ( auto * s = qobject_cast<QSpinBox *>( m_host.data() ) )
		step = s->singleStep();
	/* The step law, and why it is this one.
	 *
	 * Per-pixel motion scales with the field's CONFIGURED singleStep, not with
	 * the magnitude of the value. The alternative — max(0.01, |v| * 0.005),
	 * which Block Details used — was measured to turn FloatEdit's <float_max>
	 * sentinel into inf after a three-pixel drag, because 3.4e38 * 0.005 is
	 * 1.7e36 per pixel. It also ignores the per-field step data that call sites
	 * already supply.
	 *
	 * The cost of this choice is that a field whose singleStep is wrong for
	 * scrubbing now feels wrong, and the fix is at the call site — which also
	 * corrects its arrow and keyboard steps, today inconsistent with the drag.
	 */
	if ( isIntegral() )
		return step / 8.0;	// 8 px per unit; 10 px was too slow for an RGB channel
	return step;
}

double WwScrub::hostValue() const
{
	if ( auto * d = qobject_cast<QDoubleSpinBox *>( m_host.data() ) )
		return d->value();
	if ( auto * s = qobject_cast<QSpinBox *>( m_host.data() ) )
		return s->value();
	if ( auto * f = qobject_cast<FloatEdit *>( m_host.data() ) )
		return double( f->value() );
	if ( auto * le = qobject_cast<QLineEdit *>( m_host.data() ) )
		return le->text().toDouble();
	return 0.0;
}

void WwScrub::setHostValue( double v ) const
{
	if ( auto * d = qobject_cast<QDoubleSpinBox *>( m_host.data() ) ) {
		/* Wrapping has to be done here. QDoubleSpinBox::setValue clamps;
		 * wrapping() only affects stepBy. The heading field (0..360) is the one
		 * consumer that needs it, and clamping there would make a drag stick at
		 * the seam instead of coming round.
		 */
		if ( d->wrapping() ) {
			const double lo = d->minimum(), hi = d->maximum(), span = hi - lo;
			if ( span > 0.0 )
				v = lo + std::fmod( std::fmod( v - lo, span ) + span, span );
		}
		d->setValue( v );
		return;
	}
	if ( auto * s = qobject_cast<QSpinBox *>( m_host.data() ) ) {
		s->setValue( int( qRound64( v ) ) );
		return;
	}
	if ( auto * f = qobject_cast<FloatEdit *>( m_host.data() ) ) {
		f->setValue( float( v ) );
		return;
	}
	if ( auto * le = qobject_cast<QLineEdit *>( m_host.data() ) )
		le->setText( QString::number( v, 'f', 4 ) );
}

void WwScrub::emitCommit() const
{
	/* The commit is load-bearing, not decoration. In the operator panel
	 * editingFinished is wired to GLView::commitOperatorPreview, which rewrites
	 * the undo command's after-snapshot; without it an arrow-stepped decal
	 * offset reaches the model but not the undo entry. CollisionDragSpinBox
	 * dropped both emissions while its comment claimed equivalence with the
	 * reference — restoring them is a fix, and emitting where nothing is
	 * connected costs nothing.
	 */
	if ( auto * sb = qobject_cast<QAbstractSpinBox *>( m_host.data() ) ) {
		emit sb->editingFinished();
		return;
	}
	// FloatEdit::setValue emits valueChanged but never sigEdited, so a scrub
	// would otherwise be invisible to every sigEdited consumer.
	if ( auto * f = qobject_cast<FloatEdit *>( m_host.data() ) ) {
		emit f->sigEdited( f->value() );
		emit f->sigEdited( f->text() );
		return;
	}
	if ( auto * le = qobject_cast<QLineEdit *>( m_host.data() ) )
		emit le->editingFinished();
}

bool WwScrub::eventFilter( QObject * o, QEvent * ev )
{
	// A latched drag on a widget that stops receiving events would stay latched
	// for ever, which is how the panel-freeze bugs happened.
	if ( o == m_host ) {
		switch ( ev->type() ) {
		case QEvent::Hide:
		case QEvent::WindowDeactivate:
			cancel();
			break;
		case QEvent::EnabledChange:
			if ( m_host && !m_host->isEnabled() )
				cancel();
			break;
		default:
			break;
		}
		return QObject::eventFilter( o, ev );
	}

	if ( o != m_edit )
		return QObject::eventFilter( o, ev );

	switch ( ev->type() ) {
	case QEvent::MouseButtonPress: {
		auto * me = static_cast<QMouseEvent *>( ev );
		if ( me->button() != Qt::LeftButton )
			break;
		/* The press is ALWAYS armed, even when the field already has focus.
		 *
		 * An earlier version passed focused presses through so drag-select
		 * would work inside a number being edited. That traded the primary
		 * interaction for a secondary one: a plain click focuses the field, so
		 * from then on it could not be scrubbed at all until focus moved
		 * elsewhere - which is exactly what bungo hit on Move. Scrubbing wins.
		 *
		 * Caret placement survives: a click with no drag on an already-focused
		 * field positions the caret instead of re-selecting all (see the
		 * release branch). Only drag-select inside a focused field is given up.
		 */
		m_pressHadFocus = m_edit->hasFocus();
		m_pressLocalX = int( me->position().x() );
		/* The sentinel is a word, not a quantity. FloatEdit spells +-FLT_MAX as
		 * "<float_min>"/"<float_max>"; scrubbing from there has no meaning and
		 * used to overflow the field to inf in three pixels.
		 */
		if ( auto * f = qobject_cast<FloatEdit *>( m_host.data() ) ) {
			if ( f->isMin() || f->isMax() )
				break;
		}
		m_dragging = true;
		m_moved = false;
		// GLOBAL x: the redo panel repositions itself under the pointer, so a
		// local x would jump when it moves
		m_pressX = int( me->globalPosition().x() );
		m_startVal = hostValue();
		return true;	// swallow, or the line edit starts selecting text
	}
	case QEvent::MouseButtonDblClick:
		// word-select inside a focused field must keep working
		break;
	case QEvent::MouseMove: {
		if ( !m_dragging )
			break;
		auto * me = static_cast<QMouseEvent *>( ev );
		const int dx = int( me->globalPosition().x() ) - m_pressX;
		if ( !m_moved && std::abs( dx ) > 2 ) {
			// 2 px, deliberately twitchier than QApplication::startDragDistance
			// (~10 px): this is a value edit, not the start of a drag-and-drop.
			m_moved = true;
			if ( m_host )
				m_host->update();
			emit scrubStarted();
		}
		if ( m_moved ) {
			/* ABSOLUTE from the press, never incremental. Drag out 100 px and
			 * back and the exact start value returns with no float drift, and a
			 * value that hit min or max unsticks on the way back instead of
			 * having eaten the overshoot.
			 */
			const double scale = ( me->modifiers() & Qt::ShiftModifier ) ? 0.01 : 0.1;
			setHostValue( m_startVal + double( dx ) * pixelStep() * scale );
		}
		return true;
	}
	case QEvent::MouseButtonRelease: {
		if ( !m_dragging )
			break;
		m_dragging = false;
		// read it out before clearing: emitCommit can re-enter and rebuild the panel
		const bool finishedScrub = m_moved;
		if ( !m_moved && m_edit ) {
			// a plain click types. On RELEASE, because at press time it is not
			// yet knowable whether this becomes a scrub.
			if ( m_pressHadFocus ) {
				// already editing: put the caret where the click landed rather
				// than re-selecting the whole number under the user
				m_edit->setCursorPosition( m_edit->cursorPositionAt(
					QPoint( m_pressLocalX, m_edit->height() / 2 ) ) );
			} else {
				m_edit->selectAll();
				m_edit->setFocus();
			}
		}
		m_moved = false;
		if ( m_host )
			m_host->update();
		if ( finishedScrub ) {
			emitCommit();
			emit scrubFinished();
		}
		return true;
	}
	default:
		break;
	}
	return QObject::eventFilter( o, ev );
}

// ---------------------------------------------------------------------------
// WwScrubChrome — arrows, well, highlight
// ---------------------------------------------------------------------------

WwScrubChrome::WwScrubChrome( QWidget * host, WwScrub * scrub )
	: QWidget( host ), m_host( host ), m_scrub( scrub )
{
	setAttribute( Qt::WA_TransparentForMouseEvents );
	setAttribute( Qt::WA_NoSystemBackground );
	// below the host's line edit, so the highlight shows THROUGH the number
	// rather than washing over it
	lower();
	if ( host ) {
		host->installEventFilter( this );
		// Also on the line edit: an event filter runs BEFORE the target's own
		// handler, so QAbstractSpinBox re-lays its editor immediately after we
		// inset it. Correcting the editor's OWN Resize/Move is what makes the
		// inset stick.
		if ( QLineEdit * le = host->findChild<QLineEdit *>() )
			le->installEventFilter( this );
		setGeometry( host->rect() );
		insetEditor();
		show();
	}
	restyle();
}

void WwScrubChrome::insetEditor()
{
	/* Hold the number out of the two gutters.
	 *
	 * Without this the line edit spans the whole widget, so the value paints
	 * OVER the ‹ › glyphs and - worse - covers the two click zones, which makes
	 * the arrows purely decorative: a press in the margin lands on the line
	 * edit and starts a scrub instead of stepping. Both symptoms have one
	 * cause, and this was simply dropped when the original field was moved out
	 * of nifskope_ui.cpp into this file.
	 *
	 * The std::max matters: a widget narrower than two gutters would otherwise
	 * get a negative setGeometry width, which is a Qt warning and a nonsense
	 * layout. Clamped to 0 the field degenerates to two step zones.
	 */
	if ( !m_host || m_insetting )
		return;
	QLineEdit * le = m_host->findChild<QLineEdit *>();
	if ( !le )
		return;
	const QRect want( WW_ARROW_W, 0,
		std::max( m_host->width() - 2 * WW_ARROW_W, 0 ), m_host->height() );
	if ( le->geometry() == want )
		return;
	// setGeometry re-enters this through the editor's own Resize/Move
	m_insetting = true;
	le->setGeometry( want );
	m_insetting = false;
}

void WwScrubChrome::restyle()
{
	if ( !m_host )
		return;
	/* Colours come from the skin, never from literals. The field this replaces
	 * hardcoded QColor(230,230,230) for the arrows and the colour panel had
	 * three more hex literals in a file that did not even include wwskin.h, so
	 * they stayed dark in the Light theme.
	 */
	if ( auto * sb = qobject_cast<QAbstractSpinBox *>( m_host.data() ) ) {
		/* The :disabled rules are not optional. Setting `color` at all overrides
		 * the palette's Disabled role, so without them a read-only field paints
		 * its number in the ordinary colour and reads as editable - which is
		 * exactly what happened beside the combo boxes in the Collision
		 * Manager, where a compiled body greys Motion and Quality but the
		 * numbers stayed bright.
		 */
		sb->setStyleSheet( QStringLiteral(
			"QAbstractSpinBox { background: %1; border: none; border-radius: 3px; color: %2; }"
			"QAbstractSpinBox:disabled { background: %4; color: %5; }"
			"QLineEdit { background: transparent; border: none; color: %2;"
			" selection-background-color: %3; selection-color: #ffffff; }"
			"QLineEdit:disabled { background: transparent; color: %5; }" )
			.arg( wwSkinColor( "bgInput" ), wwSkinColor( "text" ), wwSkinColor( "bgBtnDown" ),
				wwSkinColor( "bgAlt" ), wwSkinColor( "textMuted" ) ) );
	}
	update();
}

bool WwScrubChrome::eventFilter( QObject * o, QEvent * ev )
{
	if ( o != m_host ) {
		// the editor's own layout, corrected after Qt has done it
		if ( ev->type() == QEvent::Resize || ev->type() == QEvent::Move )
			insetEditor();
		return QWidget::eventFilter( o, ev );
	}
	if ( o == m_host ) {
		switch ( ev->type() ) {
		case QEvent::Resize:
			setGeometry( m_host->rect() );
			insetEditor();
			break;
		case QEvent::Enter:
			// hover is tracked on the FRAME, not the line edit: Qt sends no
			// Leave when the pointer moves onto a child, so frame tracking
			// keeps the arrows lit as the pointer crosses into the number.
			m_hover = true;
			update();
			break;
		case QEvent::Leave:
			m_hover = false;
			update();
			break;
		case QEvent::Paint:
			// repaint on top of whatever the host just drew
			update();
			break;
		default:
			break;
		}
	}
	return QWidget::eventFilter( o, ev );
}

void WwScrubChrome::paintEvent( QPaintEvent * )
{
	if ( !m_host )
		return;
	QPainter p( this );
	p.setRenderHint( QPainter::Antialiasing );

	// A disabled field offers nothing: Qt will not deliver it a mouse event, so
	// drawing the arrows would advertise a control that cannot respond.
	if ( !m_host->isEnabled() )
		return;

	const bool scrubbing = m_scrub && m_scrub->isScrubbing();
	if ( scrubbing ) {
		p.setPen( Qt::NoPen );
		p.setBrush( QColor( 255, 255, 255, 45 ) );
		p.drawRoundedRect( rect().adjusted( 0, 0, -1, -1 ), 3.0, 3.0 );
	}

	QLineEdit * le = m_host->findChild<QLineEdit *>();
	const bool typing = le && le->hasFocus();
	// hidden while typing, and while scrubbing — the arrows are an offer, and
	// during a drag the offer is already taken
	if ( m_hover && !typing && !scrubbing ) {
		p.setPen( QColor( wwSkinColor( "textBright" ) ) );
		p.drawText( QRect( 0, 0, WW_ARROW_W, height() ), Qt::AlignCenter, QStringLiteral( "‹" ) );
		p.drawText( QRect( width() - WW_ARROW_W, 0, WW_ARROW_W, height() ),
			Qt::AlignCenter, QStringLiteral( "›" ) );
	}
}

// ---------------------------------------------------------------------------
// WwNumberField
// ---------------------------------------------------------------------------

WwNumberField::WwNumberField( QWidget * parent, const WwScrubSpec & spec )
	: QDoubleSpinBox( parent )
{
	setButtonSymbols( QAbstractSpinBox::NoButtons );	// the ‹ › are ours
	setAlignment( Qt::AlignCenter );
	/* Not setSingleStep. Move X/Y/Z never called it, so it inherits Qt's
	 * default of 1.0 and scrubs at 0.1 units per pixel. That is the field
	 * bungo named, and the one number here that must not move.
	 */
	setKeyboardTracking( false );	// or typing "12" emits once for "1"
	m_scrub = new WwScrub( this, lineEdit(), spec );
	if ( spec.chrome )
		new WwScrubChrome( this, m_scrub );
}

void WwNumberField::mousePressEvent( QMouseEvent * e )
{
	// only clicks in the gutters reach here; the inset line edit covers the rest
	if ( e->button() == Qt::LeftButton ) {
		const int x = int( e->position().x() );
		if ( x < WW_ARROW_W ) { stepBy( -1 ); emit editingFinished(); return; }
		if ( x > width() - WW_ARROW_W ) { stepBy( 1 ); emit editingFinished(); return; }
	}
	QDoubleSpinBox::mousePressEvent( e );
}

// ---------------------------------------------------------------------------
// retro-fit
// ---------------------------------------------------------------------------

/*! Should this widget scrub at all?
 *
 *  The answer is no more often than it looks. A per-pixel step is only
 *  meaningful for a bounded quantity with an ordering the user cares about, and
 *  several field types in this program look numeric without being quantities.
 *  Excluding them is a decision, recorded here so the next reader does not
 *  "fix" the omission:
 *
 *    - full-span ints (INT_MIN..INT_MAX, 0..0xffffffff): no step can both
 *      traverse the range and express a value
 *    - bitmasks and flags: every intermediate pattern is a real write
 *    - enum ordinals: not a continuum (and FO4 havok material ordinals are
 *      CRC32 hashes)
 *    - indices into other structures: string indices, block links, triangle
 *      vertex indices
 *    - hex and CRC entry, file paths, version strings, search boxes
 *
 *  Those are excluded at their call sites or by the tests below; this function
 *  only catches the structural cases a bulk sweep would otherwise grab.
 */
static const char * const WW_NO_SCRUB = "wwNoScrub";

void wwNeverScrub( QWidget * field )
{
	if ( !field )
		return;
	field->setProperty( WW_NO_SCRUB, true );
	/* Order-independent on purpose. Several of these fields come out of a
	 * factory that has already attached the gesture, so marking one only
	 * matters if the mark also REMOVES what is there — otherwise the exclusion
	 * reads correct and does nothing, which is the worst of both.
	 */
	for ( WwScrub * s : field->findChildren<WwScrub *>() ) {
		s->cancel();
		delete s;
	}
	for ( WwScrubChrome * c : field->findChildren<WwScrubChrome *>() )
		delete c;
	if ( QLineEdit * le = field->findChild<QLineEdit *>() )
		le->unsetCursor();
	field->setProperty( WW_SCRUBBED, false );
}

static bool wwScrubbable( QWidget * w )
{
	if ( !w || w->property( WW_SCRUBBED ).toBool() || w->property( WW_NO_SCRUB ).toBool() )
		return false;
	if ( auto * s = qobject_cast<QSpinBox *>( w ) ) {
		// a span this wide has no usable per-pixel step
		const qint64 span = qint64( s->maximum() ) - qint64( s->minimum() );
		return span > 0 && span < 1000000;
	}
	if ( qobject_cast<QDoubleSpinBox *>( w ) )
		return true;
	if ( qobject_cast<FloatEdit *>( w ) )
		return true;
	return false;
}

void wwMakeScrubField( QWidget * host, const WwScrubSpec & spec )
{
	if ( !host || host->property( WW_SCRUBBED ).toBool() || host->property( WW_NO_SCRUB ).toBool() )
		return;

	QLineEdit * le = nullptr;
	if ( auto * sb = qobject_cast<QAbstractSpinBox *>( host ) )
		le = sb->findChild<QLineEdit *>();
	else
		le = qobject_cast<QLineEdit *>( host );
	if ( !le )
		return;

	auto * scrub = new WwScrub( host, le, spec );

	// Centre the number, as the reference field does. Without this every
	// retro-fitted field (which is most of them) kept Qt's left alignment and
	// the value sat against the left gutter instead of between the arrows.
	if ( auto * sb = qobject_cast<QAbstractSpinBox *>( host ) )
		sb->setAlignment( Qt::AlignCenter );
	else
		le->setAlignment( Qt::AlignCenter );

	/* Chrome needs the two gutters, and a retro-fitted host may be too narrow
	 * to give them up. A spin box also has to lose its native buttons first, or
	 * the arrows land on top of them.
	 */
	if ( spec.chrome && host->width() >= 4 * WW_ARROW_W ) {
		if ( auto * sb = qobject_cast<QAbstractSpinBox *>( host ) )
			sb->setButtonSymbols( QAbstractSpinBox::NoButtons );
		new WwScrubChrome( host, scrub );
	}
}

void wwMakeScrubFields( QWidget * root )
{
	if ( !root )
		return;
	if ( wwScrubbable( root ) )
		wwMakeScrubField( root );
	const QList<QWidget *> kids = root->findChildren<QWidget *>();
	for ( QWidget * w : kids ) {
		if ( wwScrubbable( w ) )
			wwMakeScrubField( w );
	}
}

void wwRestyleScrubFields( QWidget * root )
{
	if ( !root )
		return;
	const QList<WwScrubChrome *> chrome = root->findChildren<WwScrubChrome *>();
	for ( WwScrubChrome * c : chrome )
		c->restyle();
}
