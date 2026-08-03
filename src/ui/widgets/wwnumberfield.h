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

#ifndef WWNUMBERFIELD_H
#define WWNUMBERFIELD_H

#include <QDoubleSpinBox>
#include <QObject>
#include <QPointer>
#include <QWidget>

//! @file wwnumberfield.h The one Blender-style number field.

/*! ONE implementation of the number-field gesture, for the whole program.
 *
 *  Press and drag a number left or right to scrub it, click it to type, hover
 *  it for ‹ › step arrows. bungo asked for this on every type-in field.
 *
 *  IT ALREADY EXISTED FIVE TIMES. DragSpinBox (nifskope_ui.cpp), UVDragSpinBox
 *  (uvtools.cpp), CollisionDragSpinBox (spells/collisiontools.cpp),
 *  ColorDragSpinBox (ui/widgets/colorwheel.cpp) and WwScrubFilter
 *  (ui/widgets/valueedit.cpp) were five copies of one idea, and the cause was
 *  not design but SCOPE: the original sat in an anonymous namespace inside a
 *  .cpp, unreachable from anywhere else, so every new panel wrote its own. They
 *  had drifted apart in step law, in which signals they emit, and in whether
 *  they draw arrows at all. src/wwskin.h records the same disease and the same
 *  cure; this is that cure applied to the number field.
 *
 *  So the rule for anything added later: exactly one file in src/ sets the
 *  horizontal-resize cursor on a number field, and it is wwnumberfield.cpp.
 *  tests/spells/scrub_uniform.sh counts them, which is why this sentence must
 *  not name the symbol.
 *
 *  TWO ENTRY POINTS, ONE MECHANISM. Neither shell holds gesture or painting
 *  code; both just construct a WwScrub and a WwScrubChrome.
 *
 *    WwNumberField      a QDoubleSpinBox subclass, for new code and for the
 *                       forms that need a promotable class name
 *    wwMakeScrubField   retro-fits a widget that already exists — needed
 *                       because FloatEdit is a QLineEdit, not a spin box, and
 *                       because whole forms come out of setupUi
 */

class QLineEdit;

//! Per-field overrides. The defaults are what the Move field does.
struct WwScrubSpec
{
	//! Value change per pixel, before the modifier scale. <= 0 means "use the
	//! host's singleStep()", which is what every normal field wants.
	double pixelStep = 0.0;

	//! Force integral behaviour (round on assignment, 8 px per unit). Leave
	//! Auto to resolve it at runtime: a QSpinBox host, or decimals() == 0.
	enum Integral { Auto, Yes, No };
	Integral integral = Auto;

	//! Draw the ‹ › arrows and the scrub highlight. Off for fields too narrow
	//! to give up two 16 px gutters.
	bool chrome = true;
};

/*! The gesture: press, threshold, latch, absolute map, release.
 *
 *  Installed as an event filter on the host's internal QLineEdit — NOT as
 *  mousePressEvent overrides on the host. A spin box's line edit covers the
 *  widget and eats the mouse, so a subclass override never fires; that was the
 *  original defect and it is why every copy filters.
 */
class WwScrub final : public QObject
{
	Q_OBJECT

public:
	WwScrub( QWidget * host, QLineEdit * edit, const WwScrubSpec & spec );

	//! Is a scrub in progress (not merely armed)?
	bool isScrubbing() const { return m_dragging && m_moved; }

	//! End any gesture in progress. Called when the host is hidden, disabled or
	//! loses its window, so a latched drag cannot outlive the events feeding it.
	void cancel();

signals:
	//! A drag actually started (past the threshold), and ended. Opt-in, for the
	//! few call sites that write the model on every valueChanged and have no
	//! undo merging of their own — they bracket the gesture in one macro.
	void scrubStarted();
	void scrubFinished();

protected:
	bool eventFilter( QObject * o, QEvent * ev ) override;

private:
	double hostValue() const;
	void setHostValue( double v ) const;
	bool isIntegral() const;
	double pixelStep() const;
	void emitCommit() const;

	QPointer<QWidget> m_host;
	QPointer<QLineEdit> m_edit;
	WwScrubSpec m_spec;
	bool m_dragging = false, m_moved = false;
	bool m_pressHadFocus = false;	//!< decides selectAll vs caret placement
	int m_pressX = 0;
	int m_pressLocalX = 0;
	double m_startVal = 0.0;
};

/*! The ‹ › arrows, the well and the scrub highlight.
 *
 *  A child widget rather than a paintEvent override, because a QObject filter
 *  cannot paint over what it watches — which is exactly why Block Details has
 *  no arrows today, on the largest field population in the program. Transparent
 *  to the mouse and stacked BELOW the host's line edit, so the highlight shows
 *  through the number instead of washing it out, and presses still reach the
 *  host's margins.
 */
class WwScrubChrome final : public QWidget
{
	Q_OBJECT

public:
	WwScrubChrome( QWidget * host, WwScrub * scrub );

	//! Re-read wwSkinColor. Nothing calls this at construction time today —
	//! fields are built before the theme loads — so the theme reload must.
	void restyle();

	//! Hold the host's line edit out of the two arrow gutters.
	void insetEditor();

protected:
	bool eventFilter( QObject * o, QEvent * ev ) override;
	void paintEvent( QPaintEvent * ) override;

private:
	QPointer<QWidget> m_host;
	QPointer<WwScrub> m_scrub;
	bool m_hover = false;
	bool m_insetting = false;	//!< re-entry guard: setGeometry raises Resize
};

//! The number field, for new code.
/*! A subclass and not just a filter because eight viewport key guards test
 *  inherits("QAbstractSpinBox") to decide whether a keystroke belongs to the
 *  3D view or to a text field, because Designer promotion needs a class name,
 *  and because SettingsPane persistence qobject_casts on QDoubleSpinBox.
 *
 *  It deliberately does NOT call setSingleStep. Move X/Y/Z never did, so it
 *  inherits Qt's default of 1.0 and scrubs at 0.1 units per pixel — the one
 *  number in this whole exercise that must not move.
 */
class WwNumberField final : public QDoubleSpinBox
{
	// no Q_OBJECT: it adds no signals, reusing QDoubleSpinBox::valueChanged and
	// editingFinished exactly as the field it replaces did
public:
	explicit WwNumberField( QWidget * parent = nullptr, const WwScrubSpec & spec = {} );

	//! The gesture object, for call sites that need scrubStarted/scrubFinished.
	WwScrub * scrub() const { return m_scrub; }

protected:
	void mousePressEvent( QMouseEvent * e ) override;

private:
	WwScrub * m_scrub = nullptr;
};

//! Mark a field as never-scrub, so a bulk sweep over its form skips it.
/*! Use this for anything numeric that is not a QUANTITY: bitmasks, enum
 *  ordinals, indices into other structures, hashes, sentinel-bearing fields.
 *  Dragging those walks through values that are individually meaningless and
 *  each of which is a real write. Call it BEFORE wwMakeScrubFields.
 */
void wwNeverScrub( QWidget * field );

//! Give an existing widget the gesture (and, where it can, the chrome).
/*! Works on any QAbstractSpinBox, on FloatEdit, and on a bare QLineEdit that
 *  holds a number. Safe to call twice — the second call is a no-op.
 */
void wwMakeScrubField( QWidget * host, const WwScrubSpec & spec = {} );

//! Retro-fit every numeric field under `root`, including `root` itself.
/*! Replaces the old wwAttachScrubbers. Skips the types that must not scrub —
 *  see the exclusion list in the .cpp, which is a design decision and not an
 *  oversight.
 */
void wwMakeScrubFields( QWidget * root );

//! Re-read theme colours for every scrub field under `root`.
void wwRestyleScrubFields( QWidget * root );

#endif // WWNUMBERFIELD_H
