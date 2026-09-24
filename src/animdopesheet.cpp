/* The dope sheet of the animation workspace -- lane HKXEDIT2, 2026-09-10.
   Paints and edits what animdopesheet.h describes; every colour is a skin
   name through wwSkinColor (nifskope-ww-panel-style), never a literal. */

#include "animdopesheet.h"
#include "wwskin.h"

#include <QColor>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>
#include <QCompleter>
#include <QLineEdit>

#include <cstdlib>

static QColor wwC( const char * name )
{
	return QColor( wwSkinColor( name ) );
}

AnimDopeSheet::AnimDopeSheet( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "AnimWsDopeSheet" ) );
	setFocusPolicy( Qt::StrongFocus );
	setMouseTracking( true );
	setMinimumHeight( 96 );
	vbar = new QScrollBar( Qt::Vertical, this );
	vbar->setObjectName( QStringLiteral( "AnimWsDopeSheetScroll" ) );
	vbar->setRange( 0, 0 );
	connect( vbar, &QScrollBar::valueChanged, this, [this]( int v ) {
		scrollRow = v;
		update();
	} );
	setToolTip( tr( "Keys per bone track as diamonds; click a row to select its bone in the viewport, "
					"drag keys to move them (Shift = copy), drag on empty space to box-select, "
					"click the ruler to scrub, Ctrl+wheel to zoom, Delete to delete keys; "
					"drag the two pale grips on the ruler to set the play range, or Ctrl+Home "
					"and Ctrl+End to put its start or end on the playhead; right-click anywhere "
					"for the frame under the cursor (add an annotation, insert a key), M adds an "
					"annotation at the playhead and Ctrl+M renames the selected one" ) );
}

void AnimDopeSheet::setDocument( const HkxClipDocument * d )
{
	/* bungo's ruling 7a, verbatim: "when I zoom into the timeline and try to
	   drag an already existing annotation, the zoom resets to default zoom for
	   some reason".

	   MEASURED, not guessed: every edit goes
	     AnimWorkspace::edit -> AnimWsCommand::redo -> applyDocument ->
	     rebuildRows -> sheet->setDocument( same pointer )
	   and this function ended with frameAll(), which hard-sets view0 / view1 to
	   the whole clip. So it was never about annotations: ANY edit threw the
	   zoom away, and the selection with it. The marker drag is just where he
	   met it, because that is the one gesture whose own feedback is the thing
	   that moves.

	   An EDIT IS NOT A NEW DOCUMENT. The same pointer coming back means the
	   frames changed underneath, so the view, the selection and the play range
	   stay -- clamped to the new length, and stale selection refs pruned, so
	   nothing points at a key or an annotation that the edit removed. A
	   different document (another clip picked in the list) still starts clean:
	   its zoom and its selection are not this one's. */
	const bool sameDoc = ( d && d == doc );
	const float keep0 = view0, keep1 = view1;

	doc = d;
	if ( doc ) {
		fps = doc->fps() > 0.0f ? doc->fps() : 30.0f;
		frames = std::max( 1, doc->numFrames() );
	}
	if ( !sameDoc ) {
		selKeys.clear();
		actKey = HkxKeyRef();
		selMarker = AnimWsMarkerRef();
		selMarkers.clear();
		rngStart = 0;
		rngEnd = -1;
		cancelNameEdit();
	} else {
		pruneSelection();
	}
	if ( curFrame >= frames )
		curFrame = frames - 1;
	if ( sameDoc ) {
		/* the zoom survives. It is clamped, not restored blindly: a trim can
		   leave the old view entirely past the end of the clip. */
		const float span = std::min( keep1 - keep0, float( frames ) );
		float a = keep0, bEdge = keep0 + span;
		if ( bEdge > float( frames ) - 0.5f ) {
			bEdge = float( frames ) - 0.5f;
			a = bEdge - span;
		}
		if ( a < -0.5f ) {
			a = -0.5f;
			bEdge = std::min( a + span, float( frames ) - 0.5f );
		}
		setView( a, bEdge );
	} else {
		frameAll();
	}
	update();
}

void AnimDopeSheet::pruneSelection()
{
	/* Called when the SAME document comes back changed (see setDocument): keep
	   every selected key and annotation that still exists, drop the rest. A
	   stale ref is worse than no ref -- it paints an orange diamond on a key
	   that is gone, and the readout counts it. */
	if ( !doc ) {
		selKeys.clear();
		actKey = HkxKeyRef();
		selMarker = AnimWsMarkerRef();
		selMarkers.clear();
		return;
	}
	auto keyExists = [&]( const HkxKeyRef & r ) {
		if ( r.track < 0 || r.track >= doc->keys.count() )
			return false;
		for ( const HkxKey & k : doc->keys.at( r.track ) )
			if ( k.frame == r.frame )
				return true;
		return false;
	};
	QVector<HkxKeyRef> keep;
	for ( const HkxKeyRef & r : selKeys )
		if ( keyExists( r ) )
			keep.append( r );
	selKeys = keep;
	if ( !keyExists( actKey ) )
		actKey = selKeys.isEmpty() ? HkxKeyRef() : selKeys.last();
	auto markerExists = [&]( const AnimWsMarkerRef & m ) {
		return m.valid() && m.track < doc->clip.annotations.count()
			   && m.index < doc->clip.annotations.at( m.track ).count();
	};
	QVector<AnimWsMarkerRef> keepM;
	for ( const AnimWsMarkerRef & m : selMarkers )
		if ( markerExists( m ) )
			keepM.append( m );
	selMarkers = keepM;
	if ( !markerExists( selMarker ) )
		selMarker = selMarkers.isEmpty() ? AnimWsMarkerRef() : selMarkers.last();
	if ( renameRef.valid() && !markerExists( renameRef ) )
		cancelNameEdit();
}

void AnimDopeSheet::setRuler( float f, int n )
{
	fps = f > 0.0f ? f : 30.0f;
	frames = std::max( 1, n );
	frameAll();
	update();
}

void AnimDopeSheet::setRows( const QVector<AnimWsRow> & r )
{
	allRows = r;
	if ( curRow >= allRows.count() )
		curRow = -1;
	rebuildVisible();
	update();
}

void AnimDopeSheet::rebuildVisible()
{
	visible.clear();
	// a row is hidden when any ancestor is collapsed
	for ( int i = 0; i < allRows.count(); i++ ) {
		bool hidden = false;
		int p = allRows.at( i ).parentRow;
		int guard = 0;
		while ( p >= 0 && p < allRows.count() && guard++ < 512 ) {
			if ( allRows.at( p ).collapsed ) {
				hidden = true;
				break;
			}
			p = allRows.at( p ).parentRow;
		}
		allRows[i].hidden = hidden;
		if ( !hidden )
			visible.append( i );
	}
	updateScroll();
}

QVector<int> AnimDopeSheet::visibleRows() const
{
	return visible;
}

int AnimDopeSheet::keyCountOnRow( int row ) const
{
	if ( row < 0 || row >= allRows.count() )
		return 0;
	const AnimWsRow & r = allRows.at( row );
	switch ( r.kind ) {
	case AnimWsRow::Bone:
	case AnimWsRow::Unbound:
		return doc ? doc->keyCount( r.track ) : 0;
	case AnimWsRow::Float:
		return ( doc && r.floatTrack >= 0 && r.floatTrack < doc->floatTracks.count() ) ? doc->floatTracks.at( r.floatTrack ).keys.count() : 0;
	case AnimWsRow::Markers:
		return doc ? doc->annotationCount() : 0;
	case AnimWsRow::NifTrack:
		return r.nifKeyFrames.count();
	default:
		return 0;
	}
}

int AnimDopeSheet::rowOfTrack( int track ) const
{
	for ( int i = 0; i < allRows.count(); i++ ) {
		if ( ( allRows.at( i ).kind == AnimWsRow::Bone || allRows.at( i ).kind == AnimWsRow::Unbound ) && allRows.at( i ).track == track )
			return i;
	}
	return -1;
}

int AnimDopeSheet::rowOfNodeBlock( int block ) const
{
	if ( block < 0 )
		return -1;
	for ( int i = 0; i < allRows.count(); i++ ) {
		if ( allRows.at( i ).nodeBlock == block && allRows.at( i ).kind != AnimWsRow::Markers )
			return i;
	}
	return -1;
}

void AnimDopeSheet::updateScroll()
{
	const int area = std::max( 0, height() - rulerH - markerRowH );
	const int fit = std::max( 1, area / rowH );
	const int maxScroll = std::max( 0, int( visible.count() ) - fit );
	vbar->setRange( 0, maxScroll );
	vbar->setPageStep( fit );
	if ( scrollRow > maxScroll )
		scrollRow = maxScroll;
	vbar->setValue( scrollRow );
	vbar->setGeometry( width() - vbar->sizeHint().width(), rulerH, vbar->sizeHint().width(), height() - rulerH );
	vbar->setVisible( maxScroll > 0 );
}

void AnimDopeSheet::resizeEvent( QResizeEvent * e )
{
	QWidget::resizeEvent( e );
	updateScroll();
}


/*
 *  Geometry
 */

float AnimDopeSheet::frameToX( float frame ) const
{
	const int w = std::max( 1, width() - labelW - ( vbar->isVisible() ? vbar->width() : 0 ) - 8 );
	const float span = std::max( 1e-3f, view1 - view0 );
	return float( labelW + 4 ) + ( frame - view0 ) / span * float( w );
}

float AnimDopeSheet::xToFrame( int x ) const
{
	const int w = std::max( 1, width() - labelW - ( vbar->isVisible() ? vbar->width() : 0 ) - 8 );
	const float span = std::max( 1e-3f, view1 - view0 );
	return view0 + ( float( x - labelW - 4 ) / float( w ) ) * span;
}

int AnimDopeSheet::rowTop( int visibleIndex ) const
{
	return rulerH + markerRowH + ( visibleIndex - scrollRow ) * rowH;
}

int AnimDopeSheet::rowAtY( int y ) const
{
	if ( y < rulerH + markerRowH )
		return -1;
	const int vi = scrollRow + ( y - rulerH - markerRowH ) / rowH;
	if ( vi < 0 || vi >= visible.count() )
		return -1;
	return visible.at( vi );
}

int AnimDopeSheet::rowCenterY( int row ) const
{
	const int vi = visible.indexOf( row );
	if ( vi < 0 || vi < scrollRow )
		return -1;
	const int y = rowTop( vi ) + rowH / 2;
	if ( y < rulerH + markerRowH || y >= height() )
		return -1;
	return y;
}

void AnimDopeSheet::frameAll()
{
	view0 = -0.5f;
	view1 = float( std::max( 1, frames ) ) - 0.5f;
	positionNameEdit();
	update();
}

int AnimDopeSheet::rangeStart() const
{
	return std::max( 0, std::min( frames - 1, rngStart ) );
}

int AnimDopeSheet::rangeEnd() const
{
	if ( rngEnd < 0 )
		return frames - 1;
	return std::max( rangeStart(), std::min( frames - 1, rngEnd ) );
}

void AnimDopeSheet::setRange( int first, int last )
{
	rngStart = std::max( 0, std::min( frames - 1, first ) );
	rngEnd = std::max( rngStart, std::min( frames - 1, last ) );
	update();
}

QColor AnimDopeSheet::dimOutOfRange( const QColor & c )
{
	/* The band outside the range is `animOutOfRange` filled at alpha 155 over
	   the row's ground. This is that same composite, with the key's own colour
	   as the ground, so the diamond ends up the colour it would have been had
	   the wash been painted over it instead of under it. One arithmetic, one
	   token, and the harness calls this very function for its expectations. */
	const QColor out = wwC( "animOutOfRange" );
	const int a = 155;
	auto mix = []( int ink, int wash, int alpha ) {
		return ( ink * ( 255 - alpha ) + wash * alpha + 127 ) / 255;
	};
	return QColor( mix( c.red(), out.red(), a ), mix( c.green(), out.green(), a ), mix( c.blue(), out.blue(), a ) );
}

void AnimDopeSheet::setView( float a, float b )
{
	if ( b - a < 2.0f )
		b = a + 2.0f;
	view0 = a;
	view1 = b;
	positionNameEdit();
	update();
}

QString AnimDopeSheet::niceFrameStep( int & step ) const
{
	const float pxPerFrame = frameToX( 1.0f ) - frameToX( 0.0f );
	const int candidates[] = { 1, 2, 5, 10, 20, 30, 50, 60, 100, 200, 500, 1000 };
	step = 1000;
	for ( int c : candidates ) {
		if ( pxPerFrame * float( c ) >= 44.0f ) {
			step = c;
			break;
		}
	}
	return QString();
}


/*
 *  Painting
 */

void AnimDopeSheet::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	const QColor bg = wwC( "bgPanel" ), bgAlt = wwC( "bgAlt" ), border = wwC( "border" );
	const QColor text = wwC( "text" ), muted = wwC( "textMuted" );
	const QColor bgHead = wwC( "bgHeader" ), focusC = wwC( "focus" );
	/* The sheet's own six colours, added to skinVars[] for this lane with
	   Blender 4.5's own values (bungo's rulings 2, 7b and 8 of 2026-09-12).
	   What was here before is the defect: a SELECTED diamond was filled with
	   "toggle" #4772b3 while the current row underneath it was filled with
	   "selBgActive" #4a7ab0 -- 3, 8 and 3 apart in R, G and B. Blue ink on a
	   blue plate. That is the key "going invisible" on click. */
	const QColor keyPlain = wwC( "animKey" ), keyAct = wwC( "animKeySel" ), keyOther = wwC( "animKeySelOther" );
	const QColor phC = wwC( "animPlayhead" ), phText = wwC( "animPlayheadText" );
	QColor outC = wwC( "animOutOfRange" );
	outC.setAlpha( 155 );
	p.fillRect( rect(), bg );

	const int keyLeft = labelW + 4;
	const int keyRight = width() - ( vbar->isVisible() ? vbar->width() : 0 ) - 4;

	/* ---- the darkened zone OUTSIDE the clip's range (ruling 8, verbatim:
	   "Areas inside of an animation should be as they are, outside like in
	   Blender, darkened"). Inside is not touched at all -- no tint, no wash --
	   so "as they are" is literally true.

	   It is painted band by band, each time right after that band's own
	   background and BEFORE its keys, rather than once over the finished
	   picture, so the label column keeps its full contrast.

	   THE KEYS ARE DIMMED SEPARATELY, and that is new. Painting the band under
	   the keys used to mean a key outside the range stayed as bright as one
	   inside, which is what Blender itself does (ANIM_draw_framerange runs in
	   the background pass, not over the channels). bungo overturned that on
	   2026-09-12, ruling 08:2x, verbatim: "also, grey out diamond keyframes out
	   of animations start and end range, to indicate they're not being taking
	   into consideration anymore". So the diamond painter below runs every
	   out-of-range key's colour through dimOutOfRange(), which applies THIS
	   band's arithmetic to the ink instead of to the ground. The band is
	   unchanged; only the diamonds are. */
	const int rs = rangeStart(), re = rangeEnd();
	const int xRangeStart = int( frameToX( float( rs ) - 0.5f ) );
	const int xRangeEnd = int( frameToX( float( re ) + 0.5f ) );
	const bool wholeClipInRange = ( rs <= 0 && re >= frames - 1 );
	auto darkenOutside = [&]( int y, int h ) {
		if ( wholeClipInRange )
			return;
		const int l = std::max( keyLeft, std::min( keyRight, xRangeStart ) );
		const int r = std::max( keyLeft, std::min( keyRight, xRangeEnd ) );
		if ( l > keyLeft )
			p.fillRect( QRect( keyLeft, y, l - keyLeft, h ), outC );
		if ( r < keyRight )
			p.fillRect( QRect( r, y, keyRight - r, h ), outC );
	};

	// ---- ruler, at the clip's rate
	p.fillRect( QRect( 0, 0, width(), rulerH ), bgHead );
	darkenOutside( 0, rulerH - 1 );
	int step = 1;
	niceFrameStep( step );
	p.setPen( muted );
	QFont f = p.font();
	f.setPointSizeF( std::max( 7.0, f.pointSizeF() - 1.0 ) );
	p.setFont( f );
	const QFontMetrics fm( f );
	const int first = std::max( 0, int( std::floor( view0 / float( step ) ) ) * step );
	for ( int fr = first; fr < frames && fr <= int( view1 ) + step; fr += step ) {
		const int x = int( frameToX( float( fr ) ) );
		if ( x < keyLeft || x > keyRight )
			continue;
		p.drawLine( x, rulerH - 6, x, rulerH - 1 );
		p.drawText( x + 2, rulerH - 7, QString::number( fr ) );
	}
	// the rate, at the ruler's left, so a 60 fps clip cannot pass for 30
	p.setPen( text );
	p.drawText( QRect( 4, 0, labelW - 8, rulerH ), Qt::AlignVCenter | Qt::AlignLeft,
				tr( "%1 fps, %2 frames" ).arg( fps ).arg( frames ) );
	p.setPen( border );
	p.drawLine( 0, rulerH - 1, width(), rulerH - 1 );

	/* ---- the range grips (bungo's ruling 9, verbatim: "Allow me to drag the
	   starting and ending frame, add markers for them of some kind I can
	   drag"). Two tabs in the ruler band at the range's outer edges, pale
	   against the dark scrub bar the way Blender draws the handles in its own
	   scrub bar (theme time_marker_line_selected #FFFFFFB3), each with the
	   frame number beside it so the drag can be read while it happens. */
	{
		const QColor grip = wwC( "textBright" );
		auto tab = [&]( int x, bool isEnd ) {
			if ( x < keyLeft - 8 || x > keyRight + 8 )
				return;
			const int w = 7;
			const QRect r = isEnd ? QRect( x, 1, w, rulerH - 3 ) : QRect( x - w, 1, w, rulerH - 3 );
			p.setPen( Qt::NoPen );
			p.setBrush( grip );
			p.drawRect( r );
			p.setBrush( Qt::NoBrush );
			p.setPen( QPen( grip, 1 ) );
			p.drawLine( x, 0, x, rulerH - 2 );
		};
		tab( int( frameToX( float( rs ) - 0.5f ) ), false );
		tab( int( frameToX( float( re ) + 0.5f ) ), true );
	}

	// ---- marker row (annotations)
	const int my = rulerH;
	p.fillRect( QRect( 0, my, width(), markerRowH ), bgAlt );
	darkenOutside( my, markerRowH - 1 );
	p.setPen( muted );
	p.drawText( QRect( 4, my, labelW - 8, markerRowH ), Qt::AlignVCenter | Qt::AlignLeft,
				doc ? tr( "Annotations (%1)" ).arg( doc->annotationCount() ) : tr( "Annotations" ) );
	if ( doc ) {
		for ( int t = 0; t < doc->clip.annotations.count(); t++ ) {
			const QVector<HkxAnnotation> & list = doc->clip.annotations.at( t );
			for ( int i = 0; i < list.count(); i++ ) {
				const float fr = float( list.at( i ).time ) * fps;
				const int x = int( frameToX( fr ) );
				if ( x < keyLeft - 2 || x > keyRight + 2 )
					continue;
				const AnimWsMarkerRef ref{ t, i };
				const bool isAct = ( selMarker == ref );
				const bool isSel = isAct || selMarkers.contains( ref );
				QPainterPath tri;
				tri.moveTo( x, my + markerRowH - 2 );
				tri.lineTo( x - 5, my + markerRowH - 10 );
				tri.lineTo( x + 5, my + markerRowH - 10 );
				tri.closeSubpath();
				p.fillPath( tri, isAct ? keyAct : ( isSel ? keyOther : keyPlain ) );
				p.setPen( isAct ? keyAct : ( isSel ? keyOther : text ) );
				const QString label = list.at( i ).text.isEmpty() ? QStringLiteral( "(unnamed)" ) : list.at( i ).text;
				p.drawText( x + 7, my + markerRowH - 9, label );
			}
		}
	}
	/* THE GHOST of an annotation being named (ruling 7): the marker is drawn
	   before it exists, hollow, so the inline editor has something to sit on
	   and Escape leaves nothing behind. */
	if ( newMarkerFrame >= 0 ) {
		const int x = int( frameToX( float( newMarkerFrame ) ) );
		QPainterPath tri;
		tri.moveTo( x, my + markerRowH - 2 );
		tri.lineTo( x - 5, my + markerRowH - 10 );
		tri.lineTo( x + 5, my + markerRowH - 10 );
		tri.closeSubpath();
		p.setBrush( Qt::NoBrush );
		p.setPen( QPen( keyAct, 1 ) );
		p.drawPath( tri );
	}
	p.setPen( border );
	p.drawLine( 0, my + markerRowH - 1, width(), my + markerRowH - 1 );

	// ---- rows
	p.setClipRect( QRect( 0, rulerH + markerRowH, width(), height() - rulerH - markerRowH ) );
	for ( int vi = scrollRow; vi < visible.count(); vi++ ) {
		const int row = visible.at( vi );
		const AnimWsRow & r = allRows.at( row );
		const int y = rowTop( vi );
		if ( y > height() )
			break;
		const bool isCur = ( row == curRow );
		p.fillRect( QRect( 0, y, width(), rowH ), ( vi & 1 ) ? bgAlt : bg );
		if ( isCur )
			p.fillRect( QRect( 0, y, width(), rowH ), QColor( wwSkinColor( "selBgActive" ) ) );
		darkenOutside( y, rowH - 1 );
		// label with hierarchy indent and a fold arrow
		const int indent = 6 + r.depth * 12;
		if ( r.hasChildren ) {
			p.setPen( muted );
			p.drawText( QRect( indent - 2, y, 12, rowH ), Qt::AlignVCenter, r.collapsed ? QStringLiteral( "▸" ) : QStringLiteral( "▾" ) );
		}
		p.setPen( r.kind == AnimWsRow::Unbound ? muted : ( isCur ? QColor( wwSkinColor( "selTextActive" ) ) : text ) );
		p.drawText( QRect( indent + 10, y, labelW - indent - 12, rowH ), Qt::AlignVCenter | Qt::AlignLeft,
					fm.elidedText( r.label, Qt::ElideRight, labelW - indent - 14 ) );
		/* keys as diamonds. RULING 08:2x: a key whose frame is outside
		   rangeStart()..rangeEnd() is filled with the dimmed form of the colour
		   it would otherwise have -- plain, selected and active alike, so a
		   selected key that has fallen out of the range still reads as selected
		   while it says "ignored". Nothing else about it changes: same size,
		   same shape, still hit-tested, still draggable. It follows the ruler
		   grips for free, because dragging one calls setRange(), which calls
		   update(). */
		auto diamond = [&]( int x, int frame, bool selected, bool active, const QColor & c ) {
			QPainterPath d;
			const int cy = y + rowH / 2;
			const int s = selected ? 5 : 4;
			d.moveTo( x, cy - s );
			d.lineTo( x + s, cy );
			d.lineTo( x, cy + s );
			d.lineTo( x - s, cy );
			d.closeSubpath();
			const QColor fill = active ? keyAct : ( selected ? keyOther : c );
			p.fillPath( d, ( frame < rs || frame > re ) ? dimOutOfRange( fill ) : fill );
		};
		if ( r.kind == AnimWsRow::NifTrack ) {
			for ( int fr : r.nifKeyFrames ) {
				const int x = int( frameToX( float( fr ) ) );
				if ( x >= keyLeft && x <= keyRight )
					diamond( x, fr, false, false, muted );
			}
		} else if ( doc && ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) && r.track >= 0 && r.track < doc->keys.count() ) {
			const QVector<HkxKey> & ks = doc->keys.at( r.track );
			for ( const HkxKey & k : ks ) {
				const int x = int( frameToX( float( k.frame ) ) );
				if ( x < keyLeft - 4 || x > keyRight + 4 )
					continue;
				const HkxKeyRef ref{ r.track, k.frame };
				diamond( x, k.frame, selKeys.contains( ref ), actKey == ref,
						 r.kind == AnimWsRow::Unbound ? muted : keyPlain );
			}
		} else if ( doc && r.kind == AnimWsRow::Float && r.floatTrack >= 0 && r.floatTrack < doc->floatTracks.count() ) {
			for ( const HkxFloatKey & k : doc->floatTracks.at( r.floatTrack ).keys ) {
				const int x = int( frameToX( float( k.frame ) ) );
				if ( x < keyLeft - 4 || x > keyRight + 4 )
					continue;
				// float keys are addressed as track = -(floatTrack+2) so they share the selection list
				const HkxKeyRef ref{ -( r.floatTrack + 2 ), k.frame };
				diamond( x, k.frame, selKeys.contains( ref ), actKey == ref, keyPlain );
			}
		} else if ( r.kind == AnimWsRow::Group ) {
			// a group row summarises its children: a tick per frame any child keys
		}
		p.setPen( border );
		p.drawLine( 0, y + rowH - 1, width(), y + rowH - 1 );
	}
	p.setClipping( false );

	// label column edge
	p.setPen( border );
	p.drawLine( labelW, 0, labelW, height() );

	/* ---- playhead: a BLUE line down the sheet and a BLUE box carrying the
	   frame number on the ruler. bungo, ruling 8: "Timeline marker where
	   you're at in the played animation should also be blue." It was the
	   orange accent, which is now the selection's colour and could not stay. */
	{
		const int x = int( frameToX( float( curFrame ) ) );
		if ( x >= keyLeft - 1 && x <= keyRight + 1 ) {
			p.setPen( QPen( phC, 2 ) );
			p.drawLine( x, 0, x, height() );
			const QRect box( x - 14, 1, 28, rulerH - 8 );
			p.setPen( Qt::NoPen );
			p.setBrush( phC );
			p.setRenderHint( QPainter::Antialiasing, true );
			p.drawRoundedRect( box, 3, 3 );
			p.setRenderHint( QPainter::Antialiasing, false );
			p.setBrush( Qt::NoBrush );
			p.setPen( phText );
			p.drawText( box, Qt::AlignCenter, QString::number( curFrame ) );
		}
	}

	// ---- box selection
	if ( drag == DragBox && !boxRect.isNull() ) {
		p.setPen( QPen( focusC, 1, Qt::DashLine ) );
		QColor fill = focusC;
		fill.setAlpha( 40 );
		p.fillRect( boxRect, fill );
		p.drawRect( boxRect );
	}
}


/*
 *  Hit tests
 */

bool AnimDopeSheet::keyAt( int row, int x, HkxKeyRef & out ) const
{
	if ( !doc || row < 0 || row >= allRows.count() )
		return false;
	const AnimWsRow & r = allRows.at( row );
	const float fr = xToFrame( x );
	const float tol = std::max( 0.5f, 6.0f / std::max( 1e-3f, frameToX( 1.0f ) - frameToX( 0.0f ) ) );
	if ( ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) && r.track >= 0 && r.track < doc->keys.count() ) {
		int best = -1;
		float bestD = tol;
		for ( const HkxKey & k : doc->keys.at( r.track ) ) {
			const float d = std::fabs( float( k.frame ) - fr );
			if ( d <= bestD ) {
				bestD = d;
				best = k.frame;
			}
		}
		if ( best < 0 )
			return false;
		out = HkxKeyRef{ r.track, best };
		return true;
	}
	if ( r.kind == AnimWsRow::Float && r.floatTrack >= 0 && r.floatTrack < doc->floatTracks.count() ) {
		int best = -1;
		float bestD = tol;
		for ( const HkxFloatKey & k : doc->floatTracks.at( r.floatTrack ).keys ) {
			const float d = std::fabs( float( k.frame ) - fr );
			if ( d <= bestD ) {
				bestD = d;
				best = k.frame;
			}
		}
		if ( best < 0 )
			return false;
		out = HkxKeyRef{ -( r.floatTrack + 2 ), best };
		return true;
	}
	return false;
}

bool AnimDopeSheet::markerAt( int x, int y, AnimWsMarkerRef & out ) const
{
	if ( !doc || y < rulerH || y >= rulerH + markerRowH )
		return false;
	const QFontMetrics fm( font() );
	for ( int t = 0; t < doc->clip.annotations.count(); t++ ) {
		const QVector<HkxAnnotation> & list = doc->clip.annotations.at( t );
		for ( int i = list.count() - 1; i >= 0; i-- ) {
			const int mx = int( frameToX( float( list.at( i ).time ) * fps ) );
			const int w = fm.horizontalAdvance( list.at( i ).text.isEmpty() ? QStringLiteral( "(unnamed)" ) : list.at( i ).text );
			if ( x >= mx - 6 && x <= mx + 8 + w ) {
				out = AnimWsMarkerRef{ t, i };
				return true;
			}
		}
	}
	return false;
}


int AnimDopeSheet::gripAt( int x, int y ) const
{
	/* The two grips live in the RULER band only, so dragging the range can
	   never be confused with scrubbing further down the sheet. The catch is 6
	   px either side of the edge; when the range is one frame wide and both
	   grips land on the same pixel, the END grip wins, because that is the one
	   an animator reaches for first. */
	if ( y < 0 || y >= rulerH )
		return -1;
	const int xe = int( frameToX( float( rangeEnd() ) + 0.5f ) );
	const int xs = int( frameToX( float( rangeStart() ) - 0.5f ) );
	if ( std::abs( x - xe ) <= 6 )
		return 1;
	if ( std::abs( x - xs ) <= 6 )
		return 0;
	return -1;
}


/*
 *  Selection
 */

void AnimDopeSheet::setCurrentFrame( int frame )
{
	frame = std::max( 0, std::min( frames - 1, frame ) );
	if ( frame == curFrame )
		return;
	curFrame = frame;
	update();
}

void AnimDopeSheet::selectRow( int row, bool quiet )
{
	if ( row >= allRows.count() )
		row = -1;
	curRow = row;
	update();
	if ( !quiet && row >= 0 )
		emit rowSelected( row );
}

void AnimDopeSheet::selectKeys( const QVector<HkxKeyRef> & keys, const HkxKeyRef & active )
{
	selKeys = keys;
	/* The ACTIVE key is the one painted orange; the rest of the selection is
	   orange-red (ruling 2). A caller that does not name one gets the last of
	   the list, so a one-key selection always has an active member and the two
	   colours cannot both be missing from the picture. */
	if ( active.frame >= 0 && selKeys.contains( active ) )
		actKey = active;
	else if ( !selKeys.contains( actKey ) )
		actKey = selKeys.isEmpty() ? HkxKeyRef() : selKeys.last();
	update();
	emit selectionChanged();
}

void AnimDopeSheet::selectMarker( const AnimWsMarkerRef & m, bool additive )
{
	if ( !additive )
		selMarkers.clear();
	if ( m.valid() && !selMarkers.contains( m ) )
		selMarkers.append( m );
	selMarker = m;
	update();
	emit selectionChanged();
}

void AnimDopeSheet::clearSelection()
{
	selKeys.clear();
	actKey = HkxKeyRef();
	selMarker = AnimWsMarkerRef();
	selMarkers.clear();
	update();
	emit selectionChanged();
}

void AnimDopeSheet::boxSelect( const QRect & r, bool additive )
{
	if ( !doc )
		return;
	if ( !additive )
		selKeys.clear();
	const float f0 = xToFrame( r.left() ) - 0.01f, f1 = xToFrame( r.right() ) + 0.01f;
	// (the active key is settled at the end: a box select does not click one)
	for ( int vi = scrollRow; vi < visible.count(); vi++ ) {
		const int row = visible.at( vi );
		const int y = rowTop( vi );
		if ( y + rowH <= r.top() || y >= r.bottom() )
			continue;
		const AnimWsRow & rw = allRows.at( row );
		if ( ( rw.kind == AnimWsRow::Bone || rw.kind == AnimWsRow::Unbound ) && rw.track >= 0 && rw.track < doc->keys.count() ) {
			for ( const HkxKey & k : doc->keys.at( rw.track ) ) {
				if ( float( k.frame ) >= f0 && float( k.frame ) <= f1 ) {
					const HkxKeyRef ref{ rw.track, k.frame };
					if ( !selKeys.contains( ref ) )
						selKeys.append( ref );
				}
			}
		} else if ( rw.kind == AnimWsRow::Float && rw.floatTrack >= 0 && rw.floatTrack < doc->floatTracks.count() ) {
			for ( const HkxFloatKey & k : doc->floatTracks.at( rw.floatTrack ).keys ) {
				if ( float( k.frame ) >= f0 && float( k.frame ) <= f1 ) {
					const HkxKeyRef ref{ -( rw.floatTrack + 2 ), k.frame };
					if ( !selKeys.contains( ref ) )
						selKeys.append( ref );
				}
			}
		}
	}
	if ( !selKeys.contains( actKey ) )
		actKey = selKeys.isEmpty() ? HkxKeyRef() : selKeys.last();
	update();
	emit selectionChanged();
}

void AnimDopeSheet::toggleCollapse( int row )
{
	if ( row < 0 || row >= allRows.count() || !allRows.at( row ).hasChildren )
		return;
	allRows[row].collapsed = !allRows.at( row ).collapsed;
	rebuildVisible();
	update();
}


/*
 *  Gestures
 */

void AnimDopeSheet::mousePressEvent( QMouseEvent * e )
{
	setFocus();
	const QPoint pos = e->pos();
	dragStart = pos;
	dragMoved = false;
	dragLastDelta = 0;
	dragCopy = ( e->modifiers() & Qt::ShiftModifier ) != 0;

	if ( e->button() == Qt::MiddleButton ) {
		drag = DragPan;
		panView0 = view0;
		return;
	}
	if ( e->button() != Qt::LeftButton )
		return;

	// the ruler: a range grip first, then scrub
	if ( pos.y() < rulerH ) {
		const int g = gripAt( pos.x(), pos.y() );
		if ( g >= 0 ) {
			drag = ( g == 0 ) ? DragRangeStart : DragRangeEnd;
			dragRange0 = rangeStart();
			dragRange1 = rangeEnd();
			dragStartFrame = int( std::lround( xToFrame( pos.x() ) ) );
			return;
		}
		drag = DragScrub;
		const int fr = int( std::lround( xToFrame( pos.x() ) ) );
		setCurrentFrame( fr );
		emit frameScrubbed( curFrame );
		return;
	}
	// the marker row
	AnimWsMarkerRef m;
	if ( pos.y() >= rulerH && pos.y() < rulerH + markerRowH ) {
		if ( markerAt( pos.x(), pos.y(), m ) ) {
			// Ctrl+click adds: the clicked annotation becomes the ACTIVE one
			// (orange) and the others stay selected (orange-red), ruling 7b
			if ( !( e->modifiers() & Qt::ControlModifier ) )
				selMarkers.clear();
			if ( !selMarkers.contains( m ) )
				selMarkers.append( m );
			selMarker = m;
			dragMarker = m;
			drag = DragMarker;
			dragStartFrame = int( std::lround( xToFrame( pos.x() ) ) );
			update();
			emit selectionChanged();
		} else {
			drag = DragScrub;
			setCurrentFrame( int( std::lround( xToFrame( pos.x() ) ) ) );
			emit frameScrubbed( curFrame );
		}
		return;
	}
	const int row = rowAtY( pos.y() );
	if ( row < 0 ) {
		drag = DragBox;
		boxRect = QRect( pos, pos );
		if ( !( e->modifiers() & Qt::ControlModifier ) )
			clearSelection();
		return;
	}
	// the fold arrow
	const AnimWsRow & r = allRows.at( row );
	const int indent = 6 + r.depth * 12;
	if ( pos.x() < labelW ) {
		if ( r.hasChildren && pos.x() >= indent - 4 && pos.x() <= indent + 10 ) {
			toggleCollapse( row );
			return;
		}
		selectRow( row );
		return;
	}
	HkxKeyRef k;
	if ( keyAt( row, pos.x(), k ) ) {
		if ( e->modifiers() & Qt::ControlModifier ) {
			if ( selKeys.contains( k ) )
				selKeys.removeAll( k );
			else
				selKeys.append( k );
		} else if ( !selKeys.contains( k ) ) {
			selKeys.clear();
			selKeys.append( k );
		}
		// the key just clicked is the active one; if Ctrl took it back OUT of
		// the selection, the active member falls to whatever is left
		if ( selKeys.contains( k ) )
			actKey = k;
		else if ( !selKeys.contains( actKey ) )
			actKey = selKeys.isEmpty() ? HkxKeyRef() : selKeys.last();
		if ( curRow != row )
			selectRow( row );
		drag = DragKeys;
		dragStartFrame = int( std::lround( xToFrame( pos.x() ) ) );
		update();
		emit selectionChanged();
		return;
	}
	// empty space in a row: box select from here (Blender: click-drag box)
	drag = DragBox;
	boxRect = QRect( pos, pos );
	if ( !( e->modifiers() & Qt::ControlModifier ) )
		clearSelection();
	if ( curRow != row )
		selectRow( row );
}

void AnimDopeSheet::mouseMoveEvent( QMouseEvent * e )
{
	const QPoint pos = e->pos();
	switch ( drag ) {
	case DragScrub: {
		const int fr = int( std::lround( xToFrame( pos.x() ) ) );
		if ( fr != curFrame ) {
			setCurrentFrame( fr );
			emit frameScrubbed( curFrame );
		}
		break;
	}
	case DragKeys:
	case DragMarker: {
		const int d = int( std::lround( xToFrame( pos.x() ) ) ) - dragStartFrame;
		if ( d != dragLastDelta ) {
			dragLastDelta = d;
			dragMoved = true;
		}
		break;
	}
	case DragRangeStart:
	case DragRangeEnd: {
		const int d = int( std::lround( xToFrame( pos.x() ) ) ) - dragStartFrame;
		if ( d != dragLastDelta ) {
			dragLastDelta = d;
			dragMoved = true;
			// live, so the darkened zone follows the hand; the UNDOABLE commit
			// happens once on release, not once per pixel
			if ( drag == DragRangeStart )
				setRange( std::min( dragRange0 + d, dragRange1 - 1 ), dragRange1 );
			else
				setRange( dragRange0, std::max( dragRange1 + d, dragRange0 + 1 ) );
		}
		break;
	}
	case DragBox:
		boxRect = QRect( dragStart, pos ).normalized();
		dragMoved = true;
		update();
		break;
	case DragPan: {
		const float df = xToFrame( dragStart.x() ) - xToFrame( pos.x() );
		const float span = view1 - view0;
		view0 = panView0 + df;
		view1 = view0 + span;
		positionNameEdit();
		update();
		break;
	}
	default:
		// not dragging: the pointer says when it is over a grip
		setCursor( gripAt( pos.x(), pos.y() ) >= 0 ? Qt::SizeHorCursor : Qt::ArrowCursor );
		break;
	}
}

void AnimDopeSheet::mouseReleaseEvent( QMouseEvent * e )
{
	Q_UNUSED( e );
	const Drag d = drag;
	drag = DragNone;
	switch ( d ) {
	case DragKeys:
		if ( dragMoved && dragLastDelta != 0 && !selKeys.isEmpty() )
			emit keysDragged( selKeys, dragLastDelta, dragCopy );
		break;
	case DragMarker:
		if ( dragMoved && dragLastDelta != 0 && dragMarker.valid() && doc ) {
			const int fr = doc->frameOfTime( doc->clip.annotations.value( dragMarker.track ).value( dragMarker.index ).time ) + dragLastDelta;
			emit markerDragged( dragMarker, std::max( 0, std::min( frames - 1, fr ) ) );
		}
		break;
	case DragRangeStart:
	case DragRangeEnd:
		if ( dragMoved && ( rangeStart() != dragRange0 || rangeEnd() != dragRange1 ) )
			emit rangeDragged( rangeStart(), rangeEnd() );
		break;
	case DragBox:
		if ( dragMoved )
			boxSelect( boxRect, ( e->modifiers() & Qt::ControlModifier ) != 0 );
		boxRect = QRect();
		update();
		break;
	default:
		break;
	}
}

void AnimDopeSheet::mouseDoubleClickEvent( QMouseEvent * e )
{
	AnimWsMarkerRef m;
	if ( markerAt( e->pos().x(), e->pos().y(), m ) ) {
		if ( !selMarkers.contains( m ) )
			selMarkers.append( m );
		selMarker = m;
		emit markerActivated( m );
		return;
	}
	const int row = rowAtY( e->pos().y() );
	if ( row >= 0 && e->pos().x() < labelW )
		toggleCollapse( row );
}

void AnimDopeSheet::wheelEvent( QWheelEvent * e )
{
	if ( e->modifiers() & Qt::ControlModifier ) {
		// zoom about the cursor's frame (Blender: Ctrl+wheel in the dope sheet)
		const float at = xToFrame( int( e->position().x() ) );
		const float factor = e->angleDelta().y() > 0 ? 0.8f : 1.25f;
		float a = at + ( view0 - at ) * factor;
		float b = at + ( view1 - at ) * factor;
		if ( b - a < 4.0f ) {
			a = at - 2.0f;
			b = at + 2.0f;
		}
		if ( b - a > float( frames ) * 4.0f )
			frameAll();
		else
			setView( a, b );
		e->accept();
		return;
	}
	// the wheel scrolls the rows, never a value (the panel rule)
	const int steps = -e->angleDelta().y() / 120;
	vbar->setValue( vbar->value() + steps * 3 );
	e->accept();
}

void AnimDopeSheet::keyPressEvent( QKeyEvent * e )
{
	switch ( e->key() ) {
	case Qt::Key_Delete:
	case Qt::Key_X:
		if ( !selKeys.isEmpty() || selMarker.valid() )
			emit deleteRequested();
		return;
	case Qt::Key_Left:
		setCurrentFrame( curFrame - 1 );
		emit frameScrubbed( curFrame );
		return;
	case Qt::Key_Right:
		setCurrentFrame( curFrame + 1 );
		emit frameScrubbed( curFrame );
		return;
	case Qt::Key_Home:
		if ( e->modifiers() & Qt::ControlModifier ) {
			/* Ctrl+Home and Ctrl+End put the play range's start or end on the
			   playhead. Blender uses bare S and E in the timeline for this; S
			   and E are not free in this window, and Home alone already frames
			   the whole clip, so the pair moves onto the Ctrl row. */
			if ( curFrame < rangeEnd() ) {
				setRange( curFrame, rangeEnd() );
				emit rangeDragged( rangeStart(), rangeEnd() );
			}
			return;
		}
		frameAll();
		return;
	case Qt::Key_End:
		if ( e->modifiers() & Qt::ControlModifier ) {
			if ( curFrame > rangeStart() ) {
				setRange( rangeStart(), curFrame );
				emit rangeDragged( rangeStart(), rangeEnd() );
			}
			return;
		}
		break;
	case Qt::Key_M:
		/* Blender's marker keys: M adds one at the playhead, Ctrl+M (Blender:
		   F2) renames the active one. The sheet does not know the annotation
		   vocabulary, so it asks the workspace to open the editor. */
		if ( e->modifiers() & Qt::ControlModifier ) {
			if ( selMarker.valid() )
				emit markerRenameRequested( selMarker );
			return;
		}
		if ( !( e->modifiers() & Qt::AltModifier ) ) {
			emit markerAddRequested( curFrame );
			return;
		}
		break;
	case Qt::Key_A:
		if ( e->modifiers() & Qt::ControlModifier ) {
			// Ctrl+A / Blender's A: every key of every visible row
			QVector<HkxKeyRef> all;
			if ( doc ) {
				for ( int row : visible ) {
					const AnimWsRow & r = allRows.at( row );
					if ( ( r.kind == AnimWsRow::Bone || r.kind == AnimWsRow::Unbound ) && r.track >= 0 && r.track < doc->keys.count() )
						for ( const HkxKey & k : doc->keys.at( r.track ) )
							all.append( HkxKeyRef{ r.track, k.frame } );
				}
			}
			selectKeys( all );
			return;
		}
		break;
	default:
		break;
	}
	QWidget::keyPressEvent( e );
}

void AnimDopeSheet::contextMenuEvent( QContextMenuEvent * e )
{
	/* bungo's ruling 7, verbatim: "Now, why can't I right click and insert an
	   annotation anywhere?" -- because this answered only when rowAtY( y ) was
	   a bone row, and passed no x at all, so every entry it built read the
	   PLAYHEAD. Now it fires anywhere on the sheet, the ruler and the marker
	   row included, and carries the clicked frame and the annotation under the
	   cursor. */
	const int row = rowAtY( e->pos().y() );
	const int frame = std::max( 0, std::min( frames - 1, int( std::lround( xToFrame( e->pos().x() ) ) ) ) );
	AnimWsMarkerRef m;
	if ( !markerAt( e->pos().x(), e->pos().y(), m ) )
		m = AnimWsMarkerRef();
	if ( m.valid() )
		selectMarker( m, false );
	emit sheetContextMenu( row, frame, m, e->globalPos() );
}


/*
 *  The inline name editor on the marker row (ruling 7)
 */

void AnimDopeSheet::beginNewMarker( int frame, const QStringList & vocabulary )
{
	cancelNameEdit();
	newMarkerFrame = std::max( 0, std::min( frames - 1, frame ) );
	openNameEdit( QString(), vocabulary );
}

void AnimDopeSheet::beginMarkerRename( const AnimWsMarkerRef & marker, const QStringList & vocabulary )
{
	if ( !doc || !marker.valid() || marker.track >= doc->clip.annotations.count()
		 || marker.index >= doc->clip.annotations.at( marker.track ).count() )
		return;
	cancelNameEdit();
	renameRef = marker;
	openNameEdit( doc->clip.annotations.at( marker.track ).at( marker.index ).text, vocabulary );
}

void AnimDopeSheet::openNameEdit( const QString & text, const QStringList & vocabulary )
{
	if ( !nameEdit ) {
		nameEdit = new QLineEdit( this );
		nameEdit->setObjectName( QStringLiteral( "AnimWsMarkerNameEdit" ) );
		nameEdit->installEventFilter( this );
		connect( nameEdit, &QLineEdit::returnPressed, this, &AnimDopeSheet::commitNameEdit );
	}
	auto * comp = new QCompleter( vocabulary, nameEdit );
	comp->setCaseSensitivity( Qt::CaseInsensitive );
	comp->setCompletionMode( QCompleter::PopupCompletion );
	if ( QCompleter * old = nameEdit->completer() )
		old->deleteLater();
	nameEdit->setCompleter( comp );
	nameEdit->setText( text );
	nameEdit->selectAll();
	positionNameEdit();
	nameEdit->show();
	nameEdit->setFocus( Qt::OtherFocusReason );
	update();
}

void AnimDopeSheet::positionNameEdit()
{
	if ( !nameEdit || !nameEdit->isVisible() )
		return;
	int frame = newMarkerFrame;
	if ( frame < 0 && renameRef.valid() && doc && renameRef.track < doc->clip.annotations.count()
		 && renameRef.index < doc->clip.annotations.at( renameRef.track ).count() )
		frame = doc->frameOfTime( doc->clip.annotations.at( renameRef.track ).at( renameRef.index ).time );
	if ( frame < 0 )
		return;
	const int x = int( frameToX( float( frame ) ) );
	const int w = 150;
	const int left = std::max( labelW + 4, std::min( x + 7, width() - w - 2 ) );
	nameEdit->setGeometry( left, rulerH + 2, w, markerRowH - 5 );
}

void AnimDopeSheet::commitNameEdit()
{
	if ( !nameEdit || !nameEdit->isVisible() )
		return;
	const QString name = nameEdit->text().trimmed();
	const int frame = newMarkerFrame;
	const AnimWsMarkerRef ref = renameRef;
	// hidden BEFORE the signal: the edit it commits rebuilds the rows
	nameEdit->hide();
	newMarkerFrame = -1;
	renameRef = AnimWsMarkerRef();
	setFocus( Qt::OtherFocusReason );
	update();
	if ( frame >= 0 )
		emit markerNameEntered( frame, name );
	else if ( ref.valid() )
		emit markerRenamed( ref, name );
}

void AnimDopeSheet::cancelNameEdit()
{
	newMarkerFrame = -1;
	renameRef = AnimWsMarkerRef();
	if ( nameEdit && nameEdit->isVisible() ) {
		nameEdit->hide();
		setFocus( Qt::OtherFocusReason );
	}
	update();
}

bool AnimDopeSheet::eventFilter( QObject * o, QEvent * e )
{
	if ( nameEdit && o == nameEdit ) {
		if ( e->type() == QEvent::KeyPress ) {
			auto * ke = static_cast<QKeyEvent *>( e );
			if ( ke->key() == Qt::Key_Escape ) {
				// Escape adds nothing and renames nothing
				cancelNameEdit();
				return true;
			}
		} else if ( e->type() == QEvent::FocusOut ) {
			// clicking away is the same as Escape, not a silent commit
			cancelNameEdit();
		}
	}
	return QWidget::eventFilter( o, e );
}
