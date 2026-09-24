/*! Pose Mode's armature, and the Skeleton Manager's.
 *
 * Everything that is PER-VIEW stays here -- the relationship lines, the depth
 * ramp, the pins, the hover, the selection -- and nothing that is per-view
 * draws a bone. The drawing itself is drawArmature(), which the Overlays
 * armature also calls, so the two cannot look different on the same rig
 * (bungo 2026-09-11: "shouldn't we improve both views (that will now be
 * shared)?").
 */
void GLView::drawPoseSkeleton()
{
	// Also runs for the Skeleton Manager, which wants the same armature drawing
	// without entering Pose Mode (Pose Mode additionally makes bones draggable).
	if ( ( !poseMode && !skeletonView ) || !model || !scene || poseBones.isEmpty() )
		return;

	// Pass 1: thin dashed relationship lines to each bone's parent (Blender's
	// bone relationship lines) — dim, so they read as structure, not clutter.
	if ( poseShowRelations ) {
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTransform() );
		scene->setGLLineWidth( 1.0f * float( devicePixelRatioF() ) );
		scene->setGLColor( 0.5f, 0.5f, 0.55f, 0.45f );
		for ( int b : poseBones ) {
			int p = model->getParent( b );
			if ( p < 0 || !poseBones.contains( p ) )
				continue;
			Node * n = scene->getNode( model, model->getBlockIndex( b ) );
			Node * pn = scene->getNode( model, model->getBlockIndex( p ) );
			if ( n && pn )
				scene->drawDashLine( pn->worldTrans().translation, n->worldTrans().translation, 8 );
		}
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
	}

	// Depth range across the drawn bones, so nearer bones can be drawn brighter
	// than farther ones — a strong cue that makes a dense cluster far easier to
	// read and pick (nearest = full brightness, farthest = dim).
	const Transform vt = viewTransform();
	float dMin = 1e30f, dMax = -1e30f;
	QHash<int, float> depth;
	for ( int b : poseBones ) {
		Node * n = scene->getNode( model, model->getBlockIndex( b ) );
		if ( !n )
			continue;
		const float d = -( vt * n->worldTrans().translation )[2];   // camera-space, +front
		depth.insert( b, d );
		dMin = qMin( dMin, d );
		dMax = qMax( dMax, d );
	}
	const float dRange = qMax( 1e-3f, dMax - dMin );

	/* The bone list, in the terms the shared renderer understands.
	 *
	 * The KIND is the Skeleton Manager's class, exactly as the Overlays armature
	 * reads it, so a deforming bone is the same blue in both views. Pose Mode
	 * builds poseBones from the skinned shapes rather than from
	 * skeletonAnalyse(), so a block it draws may have no class recorded; a
	 * missing class falls back to `deforming`, which is what every bone in
	 * poseBones is by that list's own construction.
	 */
	QVector<ArmatureBone> list;
	list.reserve( poseBones.size() );
	for ( int b : poseBones ) {
		Node * n = scene->getNode( model, model->getBlockIndex( b ) );
		if ( !n )
			continue;
		ArmatureBone ab;
		ab.block = b;
		ab.head = n->worldTrans().translation;
		ab.tail = poseBoneTail( b );
		ab.kind = skelOverlayClass.value( b, int( SkelDeforming ) );
		ab.body = true;
		ab.stub = false;
		ab.active = ( b == objActive );
		ab.selected = objSelection.contains( b );
		ab.hovered = ( b == poseHoverBone );
		ab.pinned = posePinned.contains( b );
		ab.fade = ( dMax - depth.value( b, dMax ) ) / dRange;   // 1 near, 0 far
		list.append( ab );
	}

	ArmatureStyle style;
	style.display = armDisplay;
	style.xray = armXray;
	style.lineWidth = 1.8f;
	style.pointSize = 5.0f;
	style.depthFade = true;
	drawArmature( list, style );
}
