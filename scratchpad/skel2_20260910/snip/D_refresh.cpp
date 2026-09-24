void GLView::refreshSkeletonOverlay()
{
	skelOverlayDirty = false;
	skelOverlayBones.clear();
	skelOverlayClass.clear();
	skelOverlayMissing = 0;
	skelOverlayFiltered = 0;
	if ( !model || !scene )
		return;

	const SkeletonReport report = skeletonAnalyse( model );

	/* THE OVERLAY LISTS EXACTLY WHAT THE SKELETON MANAGER LISTS (lane SKEL2).
	 *
	 * bungo, 2026-09-11, verbatim: "for that bone view toggle, shouldn't it
	 * mirror the skeleton manager view?".
	 *
	 * Not "the same numbers, arrived at the same way" -- the SAME FUNCTION.
	 * skeletonListedBlocks() is the one place the chip's four predicates, the
	 * search text and the dock's Isolate set are applied, and the dock's tree is
	 * built from its answer too. Two copies of that rule would be two views that
	 * agree until somebody edits one of them (CONSTITUTION rule 10).
	 */
	const QList<int> listed = skeletonListedBlocks( report, skelOverlayChip,
		skelOverlaySearch, skeletonIsolated );
	const QSet<int> listedSet( listed.begin(), listed.end() );

	for ( const SkeletonBoneInfo & b : report.bones ) {
		if ( b.block < 0 )
			continue;
		if ( !scene->findNode( model, model->getBlockIndex( b.block ) ) ) {
			// The analysis reads the FILE; the overlay draws the SCENE. A block
			// the scene never built has no world transform to draw at, and
			// silently dropping it would make the overlay's count quietly
			// smaller than the dock's. Counted instead, and the harness holds
			// the sum against the dock.
			skelOverlayMissing++;
			continue;
		}
		// The class is recorded for EVERY node the file has, not only the drawn
		// ones, because Pose Mode's own bone list is built by a different rule
		// and reads its colours out of this table.
		skelOverlayClass.insert( b.block, b.isNotABone() ? int( SkelNotABone )
			: ( b.isUnusedBone() ? int( SkelUnused ) : int( SkelDeforming ) ) );
		if ( !listedSet.contains( b.block ) ) {
			skelOverlayFiltered++;		// the chip or the search removed this row
			continue;
		}
		skelOverlayBones.append( b.block );
	}
	std::sort( skelOverlayBones.begin(), skelOverlayBones.end() );

	/* THE ARMATURE -- which pairs may be joined by a BODY (lane SKELFIX).
	 *
	 * The first cut of this overlay drew a body between every parent and child
	 * in the list, and at frame 46 of a running clip that produced segments
	 * 300.5 units long fanning out of the character: the clip carries the
	 * travel on the COM track (487 units), while `Root`, `Camera`, `CamTarget`,
	 * `CamTargetParent`, `Camera Control` and the eight `AnimObject*` nodes
	 * stay at the world origin, and each of them was being joined to a relative
	 * that had moved. `CharacterBumper` and `EyeLeftDummy001` did the same from
	 * the file's own root. Measured, all of them, in
	 * scratchpad/skelfix_20260910/segments_frame46.tsv.
	 *
	 * THE RULE, and it is a rule about the FILE, never a list of names: a body
	 * is drawn only between two ARMATURE nodes, where the armature is
	 *
	 *   every node a skin lists (the Skeleton Manager's Bones filter -- its
	 *   Deforming and Unused classes), CLOSED UPWARDS through the parent chain,
	 *   and then CUT at the deepest node that still has every one of those
	 *   bones at or beneath it.
	 *
	 * The upward closure is what keeps the rig intact: FO4 body meshes weight
	 * the `*_skin` helper bones, so `Pelvis`, `LLeg_Thigh`, `LLeg_Calf`,
	 * `LArm_UpperArm`, `COM` and the whole limb chain are NOT in the Bones
	 * filter at all -- taking the filter alone would have cut the skeleton into
	 * 60 disconnected pieces of the 129 it draws (40 with a track test on top).
	 * The cut at the common root is what removes `Root` and the file root above
	 * it, and with them the last long segment (`Root` -> `COM`, 300.5 units).
	 * Camera, weapon, anim-object, bumper and eye-dummy nodes are outside the
	 * armature because no skin bone is beneath them, not because of their names.
	 *
	 * A NODE IS NEVER HIDDEN by this: every row in the list still gets a joint
	 * marker, so the overlay's census still equals the Skeleton Manager's, which
	 * is the harness's gate (a).
	 *
	 * The closure is computed over the WHOLE file, not over the listed rows
	 * (lane SKEL2). Under the Bones chip the listed rows are the 93 skin bones
	 * and none of the `*_skin` helpers' parents is among them; closing over the
	 * listing alone would cut the rig into 93 pieces, which is the very defect
	 * SKELFIX measured. What the chip decides is which nodes are DRAWN; the
	 * armature stays a fact about the file.
	 */
	skelOverlayArm.clear();
	skelOverlayRule.clear();
	{
		QVector<int> boneRows;
		QSet<int> fileNodes;
		for ( const SkeletonBoneInfo & b : report.bones ) {
			if ( b.block < 0 )
				continue;
			fileNodes.insert( b.block );
			if ( !b.isNotABone() )
				boneRows.append( b.block );
		}
		const QSet<int> shown( skelOverlayBones.begin(), skelOverlayBones.end() );

		if ( boneRows.isEmpty() ) {
			/* FALLBACK, named in words (CONSTITUTION rule 10). A file with no
			 * skin at all -- an exported skeleton.nif -- gives the closure
			 * nothing to close over, and refusing every body there would leave
			 * a cloud of dots. Every node keeps its body, and the summary SAYS
			 * that is the arm that served.
			 */
			skelOverlayArm = shown;
			skelOverlayRule = tr( "No skin in this file, so there is no bone class to "
				"close over: every one of the %1 drawn nodes is drawn as a bone." ).arg( shown.count() );
		} else {
			// how many of the bones sit at or beneath each block
			QHash<int, int> below;
			for ( int b : boneRows ) {
				int x = b;
				for ( int guard = 0; x >= 0 && guard <= fileNodes.count() + 2; guard++ ) {
					below[x] = below.value( x, 0 ) + 1;
					x = model->getParent( x );
				}
			}
			// the DEEPEST block that still has all of them beneath it
			int common = -1, commonDepth = -1;
			for ( auto it = below.constBegin(); it != below.constEnd(); ++it ) {
				if ( it.value() != boneRows.count() )
					continue;
				int d = 0, x = model->getParent( it.key() );
				for ( ; x >= 0 && d <= fileNodes.count() + 2; d++ )
					x = model->getParent( x );
				if ( d > commonDepth ) {
					commonDepth = d;
					common = it.key();
				}
			}
			for ( int b : skelOverlayBones ) {
				bool inside = false;
				int x = b;
				for ( int guard = 0; x >= 0 && guard <= fileNodes.count() + 2; guard++ ) {
					if ( x == common ) {
						inside = true;
						break;
					}
					x = model->getParent( x );
				}
				if ( inside )
					skelOverlayArm.insert( b );
			}
			const QString rootName = ( common >= 0 )
				? model->get<QString>( model->getBlockIndex( common ), "Name" ) : QString();
			skelOverlayRule = tr( "Bones are drawn between armature nodes only: the %1 node(s) "
				"a skin lists, plus every node between them, rooted at %2. The other %3 drawn node(s) "
				"-- camera, anim-object, weapon, attach and root nodes, which no skin bone sits "
				"beneath -- get a joint marker and no bone.%4" )
				.arg( boneRows.count() )
				.arg( rootName.isEmpty() ? tr( "the file root" ) : rootName )
				.arg( skelOverlayBones.count() - skelOverlayArm.count() )
				.arg( skelOverlayFiltered > 0
					? tr( " The Skeleton Manager's filter is hiding %1 more." ).arg( skelOverlayFiltered )
					: QString() );
		}
		skelOverlayArmList.clear();
		skelOverlayArmList.reserve( skelOverlayArm.count() );
		for ( int b : skelOverlayBones ) {
			if ( skelOverlayArm.contains( b ) )
				skelOverlayArmList.append( b );
		}
	}

	const float s = characteristicBoneSize( skelOverlayBones );
	if ( s > 0.0f )
		skelOverlaySize = s;
}
