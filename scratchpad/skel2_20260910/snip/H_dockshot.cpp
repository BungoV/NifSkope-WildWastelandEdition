/* WW_SKELOVERLAY_DOCKSHOT=<png>: the Skeleton Manager dock, grabbed from
 * inside the application at a FIXED 400 px (lane SKEL2).
 *
 * The width is forced, not inherited, because the whole question -- does the
 * Bone column still show a name nine levels down -- is a question about a
 * width, and a grab at whatever width this machine's layout happened to give
 * would be a measurement of the machine. 400 px is the width bungo's
 * screenshot was taken at.
 *
 * Run the same spell with WW_SKELETON_LEGACY_COLUMNS=1 and the same file comes
 * out with the shipped column law, which is the before half of
 * cmp_manager_names.png.
 */
{
    const QString dockShot = qEnvironmentVariable( "WW_SKELOVERLAY_DOCKSHOT" );
    if ( !dockShot.isEmpty() ) {
        auto * dk = skope->findChild<QDockWidget *>( QStringLiteral( "SkeletonManagerDock" ) );
        if ( dk ) {
            dk->show();
            dk->raise();
            dk->setFixedWidth( 400 );
            qApp->processEvents();
            if ( auto * t2 = dk->findChild<QTreeWidget *>( QStringLiteral( "SkeletonTree" ) ) ) {
                // Put the deep arm rows in view: they are what the grab is of.
                std::function<QTreeWidgetItem *( QTreeWidgetItem *, int )> deepest =
                    [&]( QTreeWidgetItem * it, int d ) -> QTreeWidgetItem * {
                    QTreeWidgetItem * best = ( d >= 7 ) ? it : nullptr;
                    for ( int i = 0; i < it->childCount(); i++ )
                        if ( QTreeWidgetItem * h = deepest( it->child( i ), d + 1 ) )
                            best = h;
                    return best;
                };
                QTreeWidgetItem * target = nullptr;
                for ( int i = 0; i < t2->topLevelItemCount() && !target; i++ )
                    target = deepest( t2->topLevelItem( i ), 0 );
                if ( target )
                    t2->scrollToItem( target, QAbstractItemView::PositionAtCenter );
                qApp->processEvents();
            }
            dk->grab().save( dockShot );
            dk->setMinimumWidth( 0 );
            dk->setMaximumWidth( QWIDGETSIZE_MAX );
            log << "dock grab written to " << dockShot << "\n";
        } else {
            log << "dock grab NOT written: no SkeletonManagerDock\n";
        }
    }
}
