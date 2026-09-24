void GLView::drawSkeletonOverlay()
{
	if ( !skeletonOverlay || !model || !scene )
		return;
	if ( skelOverlayDirty )
		refreshSkeletonOverlay();
	if ( skelOverlayBones.isEmpty() )
		return;

	SkeletonOverlayCensus c;
	c.draws = skelOverlayCensus.draws + 1;
	c.missingNodes = skelOverlayMissing;
	c.filtered = skelOverlayFiltered;
	skelOverlayDrawnAt.clear();
	skelOverlayDrawnAt.reserve( skelOverlayBones.size() );
	skelOverlayDrawnSegs.clear();

	QHash<int, Vector3> head;
	head.reserve( skelOverlayBones.size() );
	for ( int b : skelOverlayBones )
		if ( Node * n = scene->findNode( model, model->getBlockIndex( b ) ) )
			head.insert( b, n->worldTrans().translation );

	/* THE BODY'S OTHER END is the nearest DRAWN armature ancestor, not simply
	 * the parent (lane SKEL2).
	 *
	 * Under the All chip those are the same node and the picture is the one
	 * SKELFIX shipped, unchanged. Under Bones, Deforming or a search the chip
	 * has removed the nodes in between -- FO4 body meshes weight the `*_skin`
	 * helpers, so a listed bone's parent is very often NOT listed -- and joining
	 * only listed parents would leave 93 disconnected bones, which is exactly
	 * the disconnection SKELFIX measured and refused. Reaching to the nearest
	 * listed ancestor keeps the rig readable while drawing nothing the Skeleton
	 * Manager is not listing.
	 */
	auto drawnAncestor = [&]( int b ) {
		int x = model->getParent( b );
		for ( int guard = 0; x >= 0 && guard <= skelOverlayClass.size() + 2; guard++ ) {
			if ( head.contains( x ) && skelOverlayArm.contains( x ) )
				return x;
			x = model->getParent( x );
		}
		return -1;
	};

	// Which ARMATURE bones have a drawn child: a bone with none gets a stub
	// instead of a body.
	QSet<int> hasDrawnChild;
	QHash<int, int> bodyFrom;
	for ( int b : skelOverlayBones ) {
		if ( !skelOverlayArm.contains( b ) || !head.contains( b ) )
			continue;
		const int p = drawnAncestor( b );
		if ( p >= 0 ) {
			bodyFrom.insert( b, p );
			hasDrawnChild.insert( p );
		}
	}

	const float cap = skelOverlaySize * 2.0f;
	QVector<ArmatureBone> list;
	list.reserve( skelOverlayBones.size() );

	auto fill = [&]( int b ) {
		ArmatureBone ab;
		ab.block = b;
		ab.kind = qBound( 0, skelOverlayClass.value( b, 2 ), 2 );
		ab.selected = objSelection.contains( b );
		ab.active = ( b == objActive );
		ab.hovered = ( b == skelOverlayHover );
		return ab;
	};

	// Bodies first, then stubs, then one joint ball per NODE -- the order the
	// overlay has always drawn in, which drawArmature() keeps (the armature is
	// blended with no depth test, so the order decides a crossing pixel).
	for ( int b : skelOverlayBones ) {
		if ( !bodyFrom.contains( b ) )
			continue;
		ArmatureBone ab = fill( b );
		ab.head = head.value( bodyFrom.value( b ) );
		ab.tail = head.value( b );
		ab.body = true;
		ab.stub = false;
		ab.ball = false;
		list.append( ab );
		skelOverlayDrawnSegs.append( qMakePair( ab.head, ab.tail ) );
		c.segments++;
	}
	for ( int b : skelOverlayBones ) {
		if ( !head.contains( b ) )
			continue;
		if ( !skelOverlayArm.contains( b ) || hasDrawnChild.contains( b ) )
			continue;
		// The stub points at the mean of the bone's DRAWN children, so a bone
		// whose only child is outside the armature does not aim at it.
		ArmatureBone ab = fill( b );
		ab.head = head.value( b );
		ab.tail = boneTailIn( b, skelOverlayArmList, cap );
		ab.body = true;
		ab.stub = true;
		ab.ball = false;
		list.append( ab );
		skelOverlayDrawnSegs.append( qMakePair( ab.head, ab.tail ) );
		c.stubs++;
	}
	// EVERY listed node gets its joint ball, armature or not: a node outside the
	// armature is still listed and still visible, it is simply never joined to
	// anything (lane SKELFIX's rule, unchanged).
	for ( int b : skelOverlayBones ) {
		if ( !head.contains( b ) )
			continue;
		ArmatureBone ab = fill( b );
		ab.head = head.value( b );
		ab.tail = ab.head;
		ab.body = false;
		ab.ball = true;
		list.append( ab );
		skelOverlayDrawnAt.insert( b, ab.head );
		c.nodes++;
		if ( ab.kind == SkelDeforming )
			c.deforming++;
		else if ( ab.kind == SkelUnused )
			c.unused++;
		else
			c.notABone++;
		if ( !skelOverlayArm.contains( b ) )
			c.skipped++;		// a joint marker, and nothing else
	}

	ArmatureStyle style;
	style.display = armDisplay;
	style.xray = armXray;
	style.lineWidth = 1.6f;		// a FIXED pixel width: the same weight at every zoom
	style.pointSize = 5.0f;
	// `Wire` is the exact way back, and the old overlay had no depth ramp --
	// "a colour that also encodes depth cannot also encode a class".
	style.depthFade = ( armDisplay != ArmWire );
	drawArmature( list, style );

	c.bones = c.deforming + c.unused;
	c.names = skelOverlayCensus.names;	// written by the QPainter pass below
	skelOverlayCensus = c;

	/* WW_SKELOVERLAY_DUMP=<file>: every drawn node, with the screen position
	 * THIS draw put it at (lane SKEL2, brief item 5).
	 *
	 * Written here rather than in a harness because the question owed to bungo
	 * -- which two marker-only nodes are the grey dots above the back at frame
	 * 46 -- is a question about a PICTURE, and only the frame that produced the
	 * picture knows its own viewport size and camera. A harness window is a
	 * different size and would answer about a different projection.
	 */
	static const QString dumpPath = qEnvironmentVariable( "WW_SKELOVERLAY_DUMP" );
	if ( !dumpPath.isEmpty() ) {
		QFile f( dumpPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream ts( &f );
			ts << "# viewport " << width() << "x" << height()
				<< "  chip " << skelOverlayChip << "  search '" << skelOverlaySearch
				<< "'  display " << armDisplay << "  xray " << ( armXray ? 1 : 0 ) << "\n";
			ts << "block\tname\tkind\tmarkerOnly\tscreenX\tscreenY\tworldX\tworldY\tworldZ\n";
			for ( const ArmatureBone & ab : list ) {
				QPointF sp( -1.0, -1.0 );
				worldToScreen( ab.head, sp );
				ts << ab.block << "\t"
					<< model->get<QString>( model->getBlockIndex( ab.block ), "Name" ) << "\t"
					<< ab.kind << "\t" << ( ab.body ? 0 : 1 ) << "\t"
					<< QString::number( sp.x(), 'f', 1 ) << "\t"
					<< QString::number( sp.y(), 'f', 1 ) << "\t"
					<< QString::number( ab.head[0], 'f', 3 ) << "\t"
					<< QString::number( ab.head[1], 'f', 3 ) << "\t"
					<< QString::number( ab.head[2], 'f', 3 ) << "\n";
			}
			ts.flush();
			f.close();
		}
	}
}
