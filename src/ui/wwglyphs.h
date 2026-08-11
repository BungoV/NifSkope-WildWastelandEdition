#ifndef WWGLYPHS_H
#define WWGLYPHS_H

#include <QColor>
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QRectF>
#include <QString>

#include <algorithm>

/*! The two DISPLAY toggles — the eye and the see-through disc — as one drawing.
 *
 *  These shipped inline inside `LoadedNifsDelegate::paint` (src/nifskope.cpp) and
 *  are now painted in two places: that row strip, and the Block List's visibility
 *  column, which replaced the Summary column. Two surfaces controlling the SAME
 *  underlying scene state (`Scene::hiddenNodes` / `Scene::ghostNodes`) must not
 *  drift apart visually, so there is one function per glyph and both callers pass
 *  their own rect and ink.
 *
 *  No colour literals: the callers hand in `wwSkinColor("accent")` and
 *  `wwSkinColor("textMuted")`, the way the strip always did.
 */
namespace WwGlyphs
{

//! One glyph box, matching the Loaded-NIFs strip (LoadedNifGlyphWidth).
constexpr int SlotWidth = 20;
//! Gap between boxes, matching the strip's `LoadedNifGlyphWidth + 2` stride.
constexpr int SlotGap = 2;

//! The Block List's visibility column carries exactly these two, in this order.
enum VisSlot { SlotVisible = 0, SlotGhost = 1 };
constexpr int VisSlotCount = 2;

//! The width a column needs to show `count` glyphs plus the strip's own margin.
constexpr int stripWidth( int count )
{
	return count * ( SlotWidth + SlotGap ) + 4;
}

/*! One glyph's box inside a cell, laid out right to left from the cell's right
 *  edge — the same geometry rule the Loaded-NIFs strip uses, so the two read as
 *  the same control at the same size however wide the column happens to be. */
inline QRect visSlotRect( const QRect & cell, int slot, int count = VisSlotCount )
{
	const int size = std::min( SlotWidth, cell.height() - 2 );
	const int top = cell.top() + ( cell.height() - size ) / 2;
	return QRect( cell.right() - ( count - slot ) * ( SlotWidth + SlotGap ) - 2,
		top, size, size );
}

/*! Blender's outliner eye: an open almond with a pupil means drawn, a shut lid
 *  means hidden. Verbatim from the Loaded-NIFs strip, only the rect and ink
 *  are parameters. `f` is the glyph box already inset. */
inline void drawEye( QPainter * painter, const QRectF & f, bool visible,
	const QColor & lit, const QColor & dim )
{
	const qreal cx = f.center().x(), cy = f.center().y();
	const qreal hw = f.width() * 0.46, hh = f.height() * 0.30;
	painter->setPen( QPen( visible ? lit : dim, 1.3 ) );
	painter->setBrush( Qt::NoBrush );
	QPainterPath p;
	if ( visible ) {
		// almond: two arcs meeting at the corners, with a pupil
		p.moveTo( cx - hw, cy );
		p.quadTo( cx, cy - hh * 2.1, cx + hw, cy );
		p.quadTo( cx, cy + hh * 2.1, cx - hw, cy );
		painter->drawPath( p );
		painter->setPen( Qt::NoPen );
		painter->setBrush( lit );
		painter->drawEllipse( QPointF( cx, cy ), hh * 0.78, hh * 0.78 );
	} else {
		// a shut lid: the lower arc alone, which is unmistakably not an eye
		p.moveTo( cx - hw, cy - hh * 0.5 );
		p.quadTo( cx, cy + hh * 1.7, cx + hw, cy - hh * 0.5 );
		painter->drawPath( p );
	}
}

/*! The see-through toggle: the conventional half-filled opacity disc. Hidden
 *  rows draw it inert (dim and faded) rather than lit-but-doing-nothing, which
 *  is the rule the Loaded-NIFs strip already established. */
inline void drawSeeThrough( QPainter * painter, const QRectF & f, bool ghost,
	bool visible, const QColor & lit, const QColor & dim )
{
	const QColor ink = ( ghost && visible ) ? lit : dim;
	painter->setOpacity( visible ? 1.0 : 0.45 );
	painter->setPen( QPen( ink, 1.1 ) );
	painter->setBrush( Qt::NoBrush );
	painter->drawEllipse( f );
	if ( ghost ) {
		painter->setPen( Qt::NoPen );
		painter->setBrush( ink );
		painter->drawChord( f, 90 * 16, 180 * 16 );	// left half filled
	}
	painter->setOpacity( 1.0 );
}

/*! ---- What the two display glyphs SAY on hover ------------------------------
 *
 *  One sentence per (glyph, state), here for the same reason the drawings are:
 *  two surfaces controlling one piece of scene state must not describe it in two
 *  vocabularies. The Loaded-NIFs strip and the Block List's visibility column
 *  both call these, so a wording change reaches both or neither.
 *
 *  `inherited` is the Block List's case alone — a block hidden or drawn
 *  see-through by an ANCESTOR reports that state and cannot change it from its
 *  own row, so the sentence has to send the reader to the parent instead of
 *  inviting a click that will do nothing. A document has no ancestor, so the
 *  strip passes false.
 *
 *  The hidden-row wordings are not padding: a see-through setting made on a
 *  hidden row is deliberately kept rather than applied, and the tooltip is the
 *  only place that says so before the click.
 */
inline QString visibleTip( bool visible, bool inherited = false )
{
	if ( inherited )
		return QCoreApplication::translate( "WwGlyphs",
			"Hidden by a parent — unhide the parent instead" );
	return visible
		? QCoreApplication::translate( "WwGlyphs", "Visible — click to hide" )
		: QCoreApplication::translate( "WwGlyphs", "Hidden — click to show" );
}

inline QString seeThroughTip( bool ghost, bool visible, bool inherited = false )
{
	if ( inherited )
		return QCoreApplication::translate( "WwGlyphs",
			"See-through from a parent — change it on the parent" );
	if ( ghost )
		return visible
			? QCoreApplication::translate( "WwGlyphs",
				"See-through — click to draw it solid" )
			: QCoreApplication::translate( "WwGlyphs",
				"See-through once it is shown again — click to turn that off" );
	return visible
		? QCoreApplication::translate( "WwGlyphs",
			"Solid — click to draw it see-through (X-ray)" )
		: QCoreApplication::translate( "WwGlyphs",
			"Solid — click to make it see-through when it is shown again" );
}

} // namespace WwGlyphs

#endif // WWGLYPHS_H
