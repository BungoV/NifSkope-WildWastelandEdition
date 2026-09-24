#!/usr/bin/env python
# Lane SKELOVERLAY -- the edits to src/glview.cpp (a CRLF file: 23,000 CR to
# 23,071 LF before this ran).  Written as a script and not by hand because a
# heredoc or an editor that inserts bare LF into this file leaves a mixed run
# nobody notices until the commit's dCR does not add up (CONSTITUTION rule 8).
#
# Refuses unless every anchor is found EXACTLY ONCE and writes nothing on a
# refusal.  --check writes nothing at all.
import io, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'glview.cpp')

def crlf(s):
    return s.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8')

EDITS = []

def repl(name, old, new):
    EDITS.append((name, crlf(old), crlf(new)))

# ---------------------------------------------------------------- 1. include
repl('include',
'''#include "spells/animationsetup.h"
''',
'''#include "spells/animationsetup.h"
#include "skeletontools.h"	// skeletonAnalyse: the Skeleton Manager's own classes
''')

# ------------------------------------------- 2. refreshPoseBoneSize delegates
repl('refreshPoseBoneSize',
'''void GLView::refreshPoseBoneSize()
{
	if ( !scene || !model || poseBones.size() < 2 )
		return;
	QVector<Vector3> pos;
	pos.reserve( poseBones.size() );
	for ( int b : poseBones )
		if ( Node * n = scene->getNode( model, model->getBlockIndex( b ) ) )
			pos.append( n->worldTrans().translation );
	QVector<float> nn;
	for ( int i = 0; i < pos.size(); i++ ) {
		float best = -1.0f;
		for ( int j = 0; j < pos.size(); j++ ) {
			if ( i == j ) continue;
			float d = ( pos[i] - pos[j] ).length();
			if ( d > 1e-4f && ( best < 0 || d < best ) )
				best = d;
		}
		if ( best > 0 )
			nn.append( best );
	}
	if ( !nn.isEmpty() ) {
		std::sort( nn.begin(), nn.end() );
		poseBoneSize = qMax( 0.5f, nn.at( nn.size() / 2 ) * 0.6f );
	}
}
''',
'''void GLView::refreshPoseBoneSize()
{
	const float s = characteristicBoneSize( poseBones );
	if ( s > 0.0f )
		poseBoneSize = s;
}

/*! The law itself, against an arbitrary drawn set.
 *
 * Extracted from refreshPoseBoneSize() so the Overlays armature sizes its bones
 * by exactly the same rule as the Pose Mode / Skeleton Manager one. Two copies
 * of this would be two armatures that look different on the same rig, which is
 * the one thing bungo's ruling asks not to happen ("basically the same view as
 * in the skeleton manager"). Returns -1 when there is nothing to measure, so
 * the caller keeps whatever size it had rather than collapsing to a default.
 */
float GLView::characteristicBoneSize( const QVector<int> & drawn ) const
{
	if ( !scene || !model || drawn.size() < 2 )
		return -1.0f;
	QVector<Vector3> pos;
	pos.reserve( drawn.size() );
	for ( int b : drawn )
		if ( Node * n = scene->getNode( model, model->getBlockIndex( b ) ) )
			pos.append( n->worldTrans().translation );
	QVector<float> nn;
	for ( int i = 0; i < pos.size(); i++ ) {
		float best = -1.0f;
		for ( int j = 0; j < pos.size(); j++ ) {
			if ( i == j ) continue;
			float d = ( pos[i] - pos[j] ).length();
			if ( d > 1e-4f && ( best < 0 || d < best ) )
				best = d;
		}
		if ( best > 0 )
			nn.append( best );
	}
	if ( nn.isEmpty() )
		return -1.0f;
	std::sort( nn.begin(), nn.end() );
	return qMax( 0.5f, nn.at( nn.size() / 2 ) * 0.6f );
}
''')

# ----------------------------------------------- 3. poseBoneTail delegates
repl('poseBoneTail',
'''Vector3 GLView::poseBoneTail( int boneBlock ) const
{
	// The bone is drawn as a short shape from its head toward its tail, with the
	// length CAPPED to the characteristic bone size so a bone parented to a
	// far-off root doesn't stretch across the screen. Direction is toward the
	// mean of child bones, or the bone's local +Y for a leaf.
	Node * n = scene ? scene->getNode( model, model->getBlockIndex( boneBlock ) ) : nullptr;
	if ( !n )
		return Vector3();
	const Vector3 head = n->worldTrans().translation;
	const float cap = poseBoneSize * 2.0f;

	Vector3 sum;
	int count = 0;
	for ( int c : model->getChildLinks( boneBlock ) ) {
		if ( !poseBones.contains( c ) )
			continue;
		if ( Node * cn = scene->getNode( model, model->getBlockIndex( c ) ) ) {
			sum += cn->worldTrans().translation;
			count++;
		}
	}
	Vector3 dir;
	if ( count > 0 )
		dir = ( sum / float( count ) ) - head;      // toward children
	else
		dir = n->worldTrans().rotation * Vector3( 0, 1, 0 );  // leaf: local +Y

	float len = dir.length();
	if ( len < 1e-4f )
		return head + Vector3( 0, 0, cap );          // degenerate; nominal up
	return head + dir * ( qMin( len, cap ) / len );
}
''',
'''Vector3 GLView::poseBoneTail( int boneBlock ) const
{
	return boneTailIn( boneBlock, poseBones, poseBoneSize * 2.0f );
}

Vector3 GLView::boneTailIn( int boneBlock, const QVector<int> & drawn, float cap ) const
{
	// The bone is drawn as a short shape from its head toward its tail, with the
	// length CAPPED to the characteristic bone size so a bone parented to a
	// far-off root doesn't stretch across the screen. Direction is toward the
	// mean of child bones, or the bone's local +Y for a leaf.
	Node * n = scene ? scene->getNode( model, model->getBlockIndex( boneBlock ) ) : nullptr;
	if ( !n )
		return Vector3();
	const Vector3 head = n->worldTrans().translation;

	Vector3 sum;
	int count = 0;
	for ( int c : model->getChildLinks( boneBlock ) ) {
		if ( !drawn.contains( c ) )
			continue;
		if ( Node * cn = scene->getNode( model, model->getBlockIndex( c ) ) ) {
			sum += cn->worldTrans().translation;
			count++;
		}
	}
	Vector3 dir;
	if ( count > 0 )
		dir = ( sum / float( count ) ) - head;      // toward children
	else
		dir = n->worldTrans().rotation * Vector3( 0, 1, 0 );  // leaf: local +Y

	float len = dir.length();
	if ( len < 1e-4f )
		return head + Vector3( 0, 0, cap );          // degenerate; nominal up
	return head + dir * ( qMin( len, cap ) / len );
}
''')

# --------------------------------- 4. the overlay itself, after drawPoseSkeleton
repl('overlay-body',
'''void GLView::setVertexPaintPreviewColors( int targetBlock, const QVector<Color4> & colors )
''',
'''/* ===================================================================
 * Overlays > Show Skeleton   (lane SKELOVERLAY, bungo 2026-09-10)
 *
 * His words: "Add to the overlays: View skeleton, shows you the bones,
 * basically the same view as in the skeleton manager".
 *
 * THREE THINGS MAKE THE TWO VIEWS AGREE, and none of them is a coincidence:
 *
 *  1. the bone LIST is skeletonAnalyse()'s -- the same call the Skeleton
 *     Manager dock builds its tree from, and the same call the `skeleton` CLI
 *     prints. Not poseBones, which is built from the skinned shapes' Bones
 *     arrays by a different rule and reaches a different set;
 *  2. the CLASSES are the dock's own three predicates (SkeletonBoneInfo:
 *     verts > 0 = deforming, isUnusedBone() = listed but unused,
 *     isNotABone() = no skin references it), so the dock's Deforming and
 *     Unused filter counts are this overlay's colour counts;
 *  3. the SHAPE is drawOctahedralBone() and the joint dot, which is what
 *     drawPoseSkeleton() draws for the dock.
 *
 * WHAT IS DIFFERENT from drawPoseSkeleton(), deliberately:
 *
 *  - no depth ramp. The pose armature dims far bones so a dense cluster is
 *    pickable; nothing here is pickable, and a colour that also encodes depth
 *    cannot also encode a class;
 *  - a FIXED pixel width, so the line weight is the same at every zoom;
 *  - no selection / hover / pin colours. This is a read-only overlay.
 *
 * BLENDER, the reference (CONSTITUTION rule 10). Blender's Armature "In Front"
 * plus the Viewport Overlays popover is the interaction being copied: one tick
 * draws the whole armature through the mesh, at a constant screen weight, and
 * it follows the animation. DIVERGENCES, stated:
 *
 *  - Blender puts Names, Axes, Shapes, Group Colors and Relationship Lines in
 *    the ARMATURE data tab as five more checkboxes. This is one tick. Bone
 *    names ride on the Overlays menu's existing "Show Nodes" entry instead of
 *    a sixth row of its own -- the same toggle that labels the scene's nodes
 *    now labels the bones (bungo's END-menu rule: rows only when a setting is
 *    really added);
 *  - Blender colours bones by BONE GROUP (an authored property). A NIF has no
 *    bone groups, so the colour carries the only classification this file
 *    actually has, which is the Skeleton Manager's;
 *  - Blender draws the bone from head to tail as authored. A NIF bone has no
 *    tail, so a parent's body is drawn to each child it has (one segment per
 *    parent -> child pair, which is what the ruling asks for) and a leaf gets
 *    a capped stub down its own +Y -- the same rule poseBoneTail() uses.
 * =================================================================== */

void GLView::setSkeletonOverlay( bool on )
{
	if ( skeletonOverlay == on )
		return;
	skeletonOverlay = on;
	if ( on ) {
		// Always rebuild on the way in: the flag is restored from QSettings
		// during construction, before any file is open, so "it was already on"
		// is not evidence that the list belongs to the model now loaded. The
		// Skeleton Manager's own setSkeletonView() carries the same note, and
		// for the same measured reason.
		skelOverlayDirty = true;
		skelOverlayCensus = SkeletonOverlayCensus();
	} else {
		skelOverlayBones.clear();
		skelOverlayClass.clear();
		skelOverlayDrawnAt.clear();
		skelOverlayCensus = SkeletonOverlayCensus();
	}
	update();
}

void GLView::refreshSkeletonOverlay()
{
	skelOverlayDirty = false;
	skelOverlayBones.clear();
	skelOverlayClass.clear();
	skelOverlayMissing = 0;
	if ( !model || !scene )
		return;

	const SkeletonReport report = skeletonAnalyse( model );
	for ( const SkeletonBoneInfo & b : report.bones ) {
		if ( b.block < 0 )
			continue;
		if ( !scene->getNode( model, model->getBlockIndex( b.block ) ) ) {
			// The analysis reads the FILE; the overlay draws the SCENE. A block
			// the scene never built has no world transform to draw at, and
			// silently dropping it would make the overlay's count quietly
			// smaller than the dock's. Counted instead, and the harness holds
			// the sum against the dock.
			skelOverlayMissing++;
			continue;
		}
		skelOverlayBones.append( b.block );
		skelOverlayClass.insert( b.block, b.isNotABone() ? int( SkelNotABone )
			: ( b.isUnusedBone() ? int( SkelUnused ) : int( SkelDeforming ) ) );
	}
	std::sort( skelOverlayBones.begin(), skelOverlayBones.end() );

	const float s = characteristicBoneSize( skelOverlayBones );
	if ( s > 0.0f )
		skelOverlaySize = s;
}

void GLView::drawSkeletonOverlay()
{
	if ( !skeletonOverlay || !model || !scene )
		return;
	if ( skelOverlayDirty )
		refreshSkeletonOverlay();
	if ( skelOverlayBones.isEmpty() )
		return;

	// The Skeleton Manager dock's own three colours, from the same palette call
	// it makes: normal text for a deforming bone, the palette's accent for an
	// unused one (the dock's "attention" colour), muted for a node no skin
	// references. Read per draw rather than cached so a skin change is picked
	// up without a restart; wwSkinColor() is a lookup in a static table.
	auto skin = []( const char * name, float alpha ) {
		const QColor c = QColor::fromString( wwSkinColor( name ) );
		return FloatVector4( float( c.redF() ), float( c.greenF() ),
							 float( c.blueF() ), alpha );
	};
	const FloatVector4 colClass[3] = {
		skin( "text", 0.95f ),        // SkelDeforming
		skin( "accent", 0.95f ),      // SkelUnused
		skin( "textMuted", 0.70f )    // SkelNotABone
	};

	SkeletonOverlayCensus c;
	c.draws = skelOverlayCensus.draws + 1;
	c.missingNodes = skelOverlayMissing;
	skelOverlayDrawnAt.clear();
	skelOverlayDrawnAt.reserve( skelOverlayBones.size() );

	// THROUGH THE MESH. Blender's "In Front": the armature is a diagram of the
	// rig, and a diagram you can only see half of is not one.
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	scene->loadModelViewMatrix( viewTransform() );

	// A FIXED pixel width, taken through the device pixel ratio so it is the
	// same physical weight on a scaled display. Not scaled by depth or by zoom:
	// the overlay must read the same at every distance.
	const float dpr = float( devicePixelRatioF() );
	const float lineW = 1.6f * dpr;
	const float pointW = 5.0f * dpr;

	QHash<int, Vector3> head;
	head.reserve( skelOverlayBones.size() );
	for ( int b : skelOverlayBones )
		if ( Node * n = scene->getNode( model, model->getBlockIndex( b ) ) )
			head.insert( b, n->worldTrans().translation );

	// Which bones have a drawn child: a bone with none gets a stub instead of a
	// body, so no bone in the list is invisible.
	QSet<int> hasDrawnChild;
	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p >= 0 && head.contains( p ) )
			hasDrawnChild.insert( p );
	}

	scene->setGLLineWidth( lineW );
	const float cap = skelOverlaySize * 2.0f;

	// Pass 1: one body per parent -> child pair, in the CHILD's class colour --
	// the segment belongs to the bone it names, not to its parent.
	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p < 0 || !head.contains( p ) || !head.contains( b ) )
			continue;
		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( p ), head.value( b ) );
		c.segments++;
	}

	// Pass 2: a stub for every bone with no drawn child, so a leaf still shows
	// which way it points.
	for ( int b : skelOverlayBones ) {
		if ( hasDrawnChild.contains( b ) || !head.contains( b ) )
			continue;
		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( b ), boneTailIn( b, skelOverlayBones, cap ) );
		c.stubs++;
	}

	// Pass 3: the joint markers, one per bone, and the census of what was drawn.
	scene->setGLPointSize( pointW );
	for ( int b : skelOverlayBones ) {
		if ( !head.contains( b ) )
			continue;
		const int cls = qBound( 0, skelOverlayClass.value( b, 2 ), 2 );
		scene->setGLColor( colClass[cls] );
		const Vector3 at = head.value( b );
		scene->drawPoints( &at, 1 );
		skelOverlayDrawnAt.insert( b, at );
		c.nodes++;
		if ( cls == SkelDeforming )
			c.deforming++;
		else if ( cls == SkelUnused )
			c.unused++;
		else
			c.notABone++;
	}
	c.bones = c.deforming + c.unused;

	glDepthMask( GL_TRUE );
	glEnable( GL_DEPTH_TEST );

	c.names = skelOverlayCensus.names;	// written by the QPainter pass below
	skelOverlayCensus = c;
}

/*! Bone names, behind the Overlays menu's EXISTING "Show Nodes" entry.
 *
 * Not a row of its own. "Show Nodes" is already the toggle that labels what the
 * viewport draws over the model, and the Skeleton Manager's names are the same
 * node names; a second checkbox for the same idea is the kind of row bungo's
 * END-menu rule exists to stop. Blender does have a separate Names checkbox --
 * stated as a divergence in the block above.
 */
void GLView::paintSkeletonOverlayNames( QPainter & painter )
{
	skelOverlayCensus.names = 0;
	if ( !skeletonOverlay || !model || !scene || skelOverlayDrawnAt.isEmpty() )
		return;
	if ( !scene->hasOption( Scene::ShowNodes ) )
		return;

	painter.setRenderHint( QPainter::Antialiasing, true );
	QFont f = painter.font();
	f.setPointSizeF( 8.0 );
	painter.setFont( f );
	int painted = 0;
	for ( auto it = skelOverlayDrawnAt.constBegin(); it != skelOverlayDrawnAt.constEnd(); ++it ) {
		const QString name = model->get<QString>( model->getBlockIndex( it.key() ), "Name" );
		if ( name.isEmpty() )
			continue;
		QPointF sp;
		if ( !worldToScreen( it.value(), sp ) )
			continue;
		const QPointF at = sp + QPointF( 6, 3 );
		painter.setPen( QColor( 0, 0, 0, 200 ) );
		painter.drawText( at + QPointF( 1, 1 ), name );
		const int cls = qBound( 0, skelOverlayClass.value( it.key(), 2 ), 2 );
		painter.setPen( QColor::fromString( wwSkinColor(
			cls == SkelDeforming ? "text" : ( cls == SkelUnused ? "accent" : "textMuted" ) ) ) );
		painter.drawText( at, name );
		painted++;
	}
	skelOverlayCensus.names = painted;
}

void GLView::setVertexPaintPreviewColors( int targetBlock, const QVector<Color4> & colors )
''')

# ------------------------------------------------------- 5. call it in paintGL
repl('paintGL-call',
'''	if ( poseMode || skeletonView ) {
		if ( poseMode )
			drawPoseWeights();
		drawPoseSkeleton();
	}
''',
'''	if ( poseMode || skeletonView ) {
		if ( poseMode )
			drawPoseWeights();
		drawPoseSkeleton();
	}

	// Overlays > Show Skeleton. AFTER the pose armature deliberately: when both
	// are on the overlay's class colours are what should be read, and it is the
	// overlay whose census the harness reads back.
	drawSkeletonOverlay();
''')

# ------------------------------------------- 6. names in the QPainter pass
repl('painter-call',
'''			if ( poseHoverBone >= 0 )
				label( poseHoverBone, true );   // always, and under the cursor
		}
''',
'''			if ( poseHoverBone >= 0 )
				label( poseHoverBone, true );   // always, and under the cursor
		}
		paintSkeletonOverlayNames( painter );
''')

def main():
    check = '--check' in sys.argv
    data = open(SRC, 'rb').read()
    cr0, lf0 = data.count(b'\r'), data.count(b'\n')
    out = data
    problems = []
    for name, old, new in EDITS:
        n = out.count(old)
        if n != 1:
            if out.count(new) >= 1 and n == 0:
                problems.append('%s: ALREADY APPLIED' % name)
            else:
                problems.append('%s: anchor found %d times, need exactly 1' % (name, n))
            continue
        out = out.replace(old, new, 1)
    if problems:
        print('REFUSED, nothing written:')
        for p in problems:
            print('  ' + p)
        return 2
    cr1, lf1 = out.count(b'\r'), out.count(b'\n')
    added = lf1 - lf0
    print('lines added: %d   dCR %+d   dLF %+d' % (added, cr1 - cr0, added))
    if cr1 - cr0 != added:
        print('REFUSED: CR and LF did not move together -- a bare LF got in')
        return 3
    if check:
        print('--check: nothing written')
        return 0
    open(SRC, 'wb').write(out)
    print('written %s  (%d bytes)' % (SRC, len(out)))
    return 0

sys.exit(main())
