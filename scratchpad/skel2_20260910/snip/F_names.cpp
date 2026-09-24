/*! Bone names for the Overlays armature.
 *
 * bungo's screenshot that started this lane had all 130 node names painted at
 * once over the model, and his word for it was clutter. So the default is
 * Blender's: the bone under the cursor and the bones that are selected, and
 * nothing else. Overlays > Bone Display > Names turns every name on, and the
 * Overlays menu's existing Show Nodes still does too, so the picture that
 * shipped is still one tick away.
 */
void GLView::paintSkeletonOverlayNames( QPainter & painter )
{
	skelOverlayCensus.names = 0;
	if ( !skeletonOverlay || !model || !scene || skelOverlayDrawnAt.isEmpty() )
		return;

	const bool all = ( armNames == 1 ) || scene->hasOption( Scene::ShowNodes );
	QSet<int> want;
	if ( !all ) {
		if ( skelOverlayHover >= 0 )
			want.insert( skelOverlayHover );
		if ( objActive >= 0 )
			want.insert( objActive );
		for ( int s : objSelection )
			want.insert( s );
		if ( want.isEmpty() )
			return;
	}

	painter.setRenderHint( QPainter::Antialiasing, true );
	QFont f = painter.font();
	f.setPointSizeF( 8.0 );
	painter.setFont( f );
	int painted = 0;
	for ( auto it = skelOverlayDrawnAt.constBegin(); it != skelOverlayDrawnAt.constEnd(); ++it ) {
		if ( !all && !want.contains( it.key() ) )
			continue;
		const QString name = model->get<QString>( model->getBlockIndex( it.key() ), "Name" );
		if ( name.isEmpty() )
			continue;
		QPointF sp;
		if ( !worldToScreen( it.value(), sp ) )
			continue;
		const QPointF at = sp + QPointF( 6, 3 );
		painter.setPen( QColor( 0, 0, 0, 200 ) );
		painter.drawText( at + QPointF( 1, 1 ), name );
		// The SAME palette function the bone itself and the dock's row use.
		const int cls = qBound( 0, skelOverlayClass.value( it.key(), 2 ), 2 );
		const int state = ( it.key() == objActive ) ? 2
			: ( objSelection.contains( it.key() ) ? 1
			: ( it.key() == skelOverlayHover ? 3 : 0 ) );
		painter.setPen( skeletonKindColor( cls, state ) );
		painter.drawText( at, name );
		painted++;
	}
	skelOverlayCensus.names = painted;
}
