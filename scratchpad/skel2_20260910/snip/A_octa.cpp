/*! Blender's octahedral bone, as a wireframe from head to tail.
 *
 * This is the shape that makes an armature readable: a square "collar" a short
 * way down from the head, with four edges fanning back to the head and four
 * converging on the tail. Because the wide end is at the head and it tapers to a
 * point at the tail, the bone's DIRECTION is visible at a glance — which a plain
 * head-to-tail line cannot show.
 *
 * \a ringAt is the collar's position along the bone, as a fraction of its
 * length. 0.15 is what both views drew before lane SKEL2 and is what the `Wire`
 * display keeps, byte for byte; 0.10 is Blender's own, and bungo's reference
 * picture (Blender 4.5.3, a default bone in Object Mode) shows the ring "about a
 * tenth of the length from the head", so the solid octahedron uses that.
 *
 * Drawn as 12 line segments rather than solid geometry, so it works through the
 * existing streaming line path and needs no new shader or render state. The
 * SOLID facets are armatureOctahedronFaces() below, which is a different pass.
 */
void GLView::drawOctahedralBone( const Vector3 & head, const Vector3 & tail, float ringAt )
{
	const Vector3 axis = tail - head;
	const float len = axis.length();
	if ( len < 1.0e-5f )
		return;
	const Vector3 dir = Vector3( axis ).normalize();

	// Any vector not parallel to dir gives a stable perpendicular frame. Picking
	// the world axis dir is LEAST aligned with avoids the degenerate cross
	// product — the same guard the procedural-lightning frames needed.
	Vector3 up( 0.0f, 0.0f, 1.0f );
	if ( fabsf( dir[2] ) > 0.9f )
		up = Vector3( 1.0f, 0.0f, 0.0f );
	Vector3 x = Vector3::crossproduct( dir, up ).normalize();
	Vector3 y = Vector3::crossproduct( dir, x ).normalize();

	const float r = len * 0.10f;
	const Vector3 collar = head + dir * ( len * qBound( 0.01f, ringAt, 0.9f ) );
	const Vector3 p[4] = { collar + x * r, collar + y * r, collar - x * r, collar - y * r };

	for ( int i = 0; i < 4; i++ ) {
		scene->drawLine( head, p[i] );				// fan back to the head
		scene->drawLine( p[i], p[( i + 1 ) & 3] );	// the collar square
		scene->drawLine( p[i], tail );				// taper to the tail
	}
}

/*! The SOLID octahedron of bungo's reference picture: eight flat-lit facets.
 *
 * His words, 2026-09-11: "what about the bone shape? Shouldn't it be something
 * like in Blender?", over a screenshot of Blender 4.5.3's default armature bone
 * in Object Mode — a solid-shaded octahedron whose four faces each take a
 * different light, which is what makes the twist of a bone readable and what a
 * wireframe of the same shape cannot show.
 *
 * Two decisions worth stating, because both are measurable rather than taste:
 *
 *  - BACK FACES ARE DROPPED HERE, on the CPU, by the sign of the facet normal in
 *    camera space. The armature draws with the depth test OFF (it is a diagram
 *    read through the mesh), so the GPU cannot resolve which facet is in front
 *    and the far side of each bone would blend over the near side. Four facets
 *    survive per bone, which is exactly what Blender shows;
 *  - the light is FIXED IN CAMERA SPACE, not in the world. A world light makes
 *    the same bone change shade as the user orbits, which reads as the bone
 *    changing colour rather than the camera moving.
 *
 * Appends into a shared triangle soup with per-vertex colours so the whole
 * armature is ONE draw call (`selection.prog` reads attribute 1 as the vertex
 * colour when `vertexColorOverride` is zero, which is what passing a colour
 * array to Scene::drawTriangles does).
 */
void GLView::armatureOctahedronFaces( const Vector3 & head, const Vector3 & tail, float ringAt,
	FloatVector4 base, QVector<Vector3> & tri, QVector<FloatVector4> & col ) const
{
	const Vector3 axis = tail - head;
	const float len = axis.length();
	if ( len < 1.0e-5f )
		return;
	const Vector3 dir = Vector3( axis ).normalize();
	Vector3 up( 0.0f, 0.0f, 1.0f );
	if ( fabsf( dir[2] ) > 0.9f )
		up = Vector3( 1.0f, 0.0f, 0.0f );
	Vector3 x = Vector3::crossproduct( dir, up ).normalize();
	Vector3 y = Vector3::crossproduct( dir, x ).normalize();

	const float r = len * 0.10f;
	const Vector3 collar = head + dir * ( len * qBound( 0.01f, ringAt, 0.9f ) );
	const Vector3 p[4] = { collar + x * r, collar + y * r, collar - x * r, collar - y * r };

	const Transform vt = viewTransform();
	// Camera space: the eye looks down -Z, so a facet faces us when its normal
	// has a POSITIVE z there. The light sits up and to the left of the eye.
	const Vector3 lightCam = Vector3( 0.35f, 0.45f, 0.82f ).normalize();

	auto face = [&]( const Vector3 & a, const Vector3 & b, const Vector3 & c ) {
		Vector3 n = Vector3::crossproduct( b - a, c - a );
		if ( n.length() < 1.0e-9f )
			return;
		n.normalize();
		/* Point the normal OUTWARD without trusting the winding.
		 *
		 * A cross product only gives an outward normal if the three points are
		 * wound the right way round, and getting that wrong here does not look
		 * wrong -- it drops every facet and the bone silently draws as a
		 * wireframe. The octahedron's own axis settles it instead: the outward
		 * direction at a facet is the one that leads AWAY from the head->tail
		 * line, which is true whichever way the triangle was listed.
		 */
		const Vector3 cen = ( a + b + c ) / 3.0f;
		const Vector3 outward = cen - ( head + dir * Vector3::dotproduct( cen - head, dir ) );
		if ( outward.length() > 1.0e-9f && Vector3::dotproduct( n, outward ) < 0.0f )
			n = -n;
		Vector3 nc = vt.rotation * n;
		if ( nc[2] <= 0.0f )
			return;						// facing away from the eye: drop it
		const float lambert = qMax( 0.0f, Vector3::dotproduct( nc, lightCam ) );
		const float shade = 0.42f + 0.58f * lambert;
		const FloatVector4 c4( base[0] * shade, base[1] * shade, base[2] * shade, base[3] );
		tri.append( a ); col.append( c4 );
		tri.append( b ); col.append( c4 );
		tri.append( c ); col.append( c4 );
	};

	for ( int i = 0; i < 4; i++ ) {
		const Vector3 & q0 = p[i];
		const Vector3 & q1 = p[( i + 1 ) & 3];
		face( head, q0, q1 );		// the four facets back to the head ball
		face( q0, tail, q1 );		// the four facets tapering to the tail
	}
}

/*! THE ONE ARMATURE RENDERER (lane SKEL2, bungo 2026-09-11).
 *
 * His words: "shouldn't we improve both views (that will now be shared)?".
 * Before this lane there were two routines drawing the same rig --
 * drawPoseSkeleton() for Pose Mode and the Skeleton Manager, and
 * drawSkeletonOverlay() for the Overlays tick -- with two colour laws, two
 * shape rules and two sets of widths. They now both build a list of
 * ArmatureBone and hand it here, which is CONSTITUTION rule 10 taken literally:
 * what is shared lives in the shared code, and a feature owns only its own
 * selection and gating.
 *
 * The passes are in the order the old overlay drew them -- every BODY, then
 * every STUB, then the joint balls -- because the armature draws blended with
 * no depth test, so the order decides which line wins a crossing pixel, and the
 * `Wire` display has to reproduce the old picture.
 */
void GLView::drawArmature( const QVector<ArmatureBone> & bones, const ArmatureStyle & style )
{
	if ( !scene || bones.isEmpty() )
		return;

	const bool xray = style.xray;
	if ( xray )
		glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	scene->loadModelViewMatrix( viewTransform() );
	const float dpr = float( devicePixelRatioF() );

	/* THE COLOUR LAW, once, for both callers.
	 *
	 * skeletonKindColor() is the single palette function the Skeleton Manager's
	 * rows also call, so a bone and its row cannot be two different colours.
	 * bungo, verbatim: "Just keep the color of the bones blue".
	 */
	auto colourOf = [&]( const ArmatureBone & b, float alpha ) {
		const QColor c = skeletonKindColor( b.kind,
			b.active ? 2 : ( b.selected ? 1 : ( b.hovered ? 3 : 0 ) ) );
		float f = 1.0f;
		if ( style.depthFade && !b.active && !b.selected && !b.hovered )
			f = 0.45f + 0.55f * qBound( 0.0f, b.fade, 1.0f );
		if ( b.pinned && !b.active && !b.selected )
			f *= 0.70f;				// a locked bone reads as held, not as chosen
		return FloatVector4( float( c.redF() ) * f, float( c.greenF() ) * f,
							 float( c.blueF() ) * f, alpha );
	};

	// ---- pass 1: the solid facets, all of them in ONE draw ----------------
	if ( style.display == ArmOctahedral ) {
		QVector<Vector3> tri;
		QVector<FloatVector4> col;
		tri.reserve( bones.size() * 24 );
		col.reserve( bones.size() * 24 );
		for ( const ArmatureBone & b : bones ) {
			if ( !b.body )
				continue;
			armatureOctahedronFaces( b.head, b.tail, 0.10f, colourOf( b, 0.88f ), tri, col );
		}
		if ( !tri.isEmpty() )
			scene->drawTriangles( tri.constData(), size_t( tri.size() ), col.constData(), true );
	}

	// ---- pass 2: the edges (and the whole shape in Stick / Wire) ----------
	// Bodies first, then stubs: the old overlay's order exactly.
	for ( int phase = 0; phase < 2; phase++ ) {
		for ( const ArmatureBone & b : bones ) {
			if ( !b.body || ( b.stub ? 0 : 1 ) != phase )
				continue;
			// 1.55 keeps Pose Mode's active bone at the 2.8 px it has always been
			// (1.8 * 1.55 = 2.79) without a second width in the style.
			scene->setGLLineWidth( ( b.active ? style.lineWidth * 1.55f : style.lineWidth ) * dpr );
			scene->setGLColor( colourOf( b, 0.95f ) );
			if ( style.display == ArmStick )
				scene->drawLine( b.head, b.tail );
			else if ( style.display == ArmWire )
				drawOctahedralBone( b.head, b.tail, 0.15f );
			else
				drawOctahedralBone( b.head, b.tail, 0.10f );
		}
	}

	// ---- pass 3: the balls. Blender draws one at the head (the joint) and a
	// smaller one at the tail; `Wire` draws only the head, which is what the
	// old overlay and the old pose armature both did.
	for ( const ArmatureBone & b : bones ) {
		if ( !b.ball )
			continue;
		scene->setGLColor( colourOf( b, 0.95f ) );
		const float ps = b.active ? style.pointSize * 1.8f
			: ( b.hovered ? style.pointSize * 1.4f : style.pointSize );
		scene->setGLPointSize( ps * dpr );
		scene->drawPoints( &b.head, 1 );
	}
	if ( style.display != ArmWire ) {
		for ( const ArmatureBone & b : bones ) {
			if ( !b.body )
				continue;
			scene->setGLColor( colourOf( b, 0.95f ) );
			const float ps = b.active ? style.pointSize * 1.8f
				: ( b.hovered ? style.pointSize * 1.4f : style.pointSize );
			scene->setGLPointSize( qMax( 1.0f, ps * 0.55f ) * dpr );
			scene->drawPoints( &b.tail, 1 );
		}
	}

	glDepthMask( GL_TRUE );
	if ( xray )
		glEnable( GL_DEPTH_TEST );
}

void GLView::setArmatureDisplay( int mode )
{
	const int m = qBound( 0, mode, int( ArmWire ) );
	if ( m == armDisplay )
		return;
	armDisplay = m;
	update();
}

void GLView::setArmatureXray( bool on )
{
	if ( on == armXray )
		return;
	armXray = on;
	update();
}

void GLView::setArmatureNames( int mode )
{
	const int m = qBound( 0, mode, 1 );
	if ( m == armNames )
		return;
	armNames = m;
	update();
}

void GLView::setSkeletonOverlayHover( int block )
{
	if ( block == skelOverlayHover )
		return;
	skelOverlayHover = block;
	emit skeletonOverlayHoverChanged( block );
	update();
}

/*! The chip and search text of the Skeleton Manager, mirrored into the overlay.
 *
 * bungo, verbatim: "for that bone view toggle, shouldn't it mirror the skeleton
 * manager view?". The dock pushes its state here at the end of every refresh();
 * the overlay applies the SAME four predicates to the SAME skeletonAnalyse()
 * report, so the two lists are equal by construction rather than by agreement.
 */
void GLView::setSkeletonOverlayFilter( int chip, const QString & search )
{
	const int c = qBound( 0, chip, 3 );
	if ( c == skelOverlayChip && search == skelOverlaySearch )
		return;
	skelOverlayChip = c;
	skelOverlaySearch = search;
	skelOverlayDirty = true;
	if ( skeletonOverlay ) {
		if ( model && scene )
			refreshSkeletonOverlay();
		update();
	}
}

/*! The viewport half of the two-way selection: which overlay bone is here?
 *
 * Deliberately NOT poseBoneAt() reached a second way. That one picks over
 * poseBones, which is built from the skinned shapes by a different rule and is
 * empty unless Pose Mode or the Skeleton Manager dock is up; this picks over the
 * list the overlay is actually drawing, under the chip the dock is showing.
 */
int GLView::skeletonOverlayBoneAt( const QPointF & pos ) const
{
	if ( !skeletonOverlay || !model || !scene || skelOverlayBones.isEmpty() )
		return -1;
	const float pickR = 12.0f;
	float best = pickR * pickR;
	int bestBone = -1;
	for ( int b : skelOverlayBones ) {
		Node * n = scene->findNode( model, model->getBlockIndex( b ) );
		if ( !n )
			continue;
		QPointF hs, ts;
		if ( !worldToScreen( n->worldTrans().translation, hs ) )
			continue;
		float d2 = float( QPointF::dotProduct( pos - hs, pos - hs ) );
		if ( skelOverlayArm.contains( b )
			 && worldToScreen( boneTailIn( b, skelOverlayArmList, skelOverlaySize * 2.0f ), ts ) ) {
			QPointF ab = ts - hs;
			float len2 = float( QPointF::dotProduct( ab, ab ) );
			if ( len2 > 1e-3f ) {
				float t = qBound( 0.0f, float( QPointF::dotProduct( pos - hs, ab ) ) / len2, 1.0f );
				QPointF proj = hs + ab * t;
				d2 = qMin( d2, float( QPointF::dotProduct( pos - proj, pos - proj ) ) );
			}
		}
		if ( d2 < best ) {
			best = d2;
			bestBone = b;
		}
	}
	return bestBone;
}
