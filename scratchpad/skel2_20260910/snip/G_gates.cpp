/* ================= LANE SKEL2's GATES, 2026-09-11 =================
 *
 * bungo's three rulings, in order: the bone view is to MIRROR the Skeleton
 * Manager; both views are to share one renderer and both are to be improved;
 * the bone is to be Blender's octahedron, in blue.
 *
 * These run on the BIND pose, before the clip section, so a missing clip
 * cannot skip them -- the old file's `break` on a missing clip took gates
 * (f)-(i) with it, and there is no reason for the mirror to share that fate.
 */
{
    ogl->setSkeletonOverlay( true );
    wwGrab( ogl );

    auto * searchBox = dock->findChild<QLineEdit *>( QStringLiteral( "SkeletonSearch" ) );
    auto nameOf = [&]( int b ) {
        return QString( "%1:%2" ).arg( b )
            .arg( nif->get<QString>( nif->getBlockIndex( b ), "Name" ) );
    };
    auto listedNames = [&]() {
        QStringList out;
        for ( int b : ogl->skeletonOverlayListed() )
            out << nameOf( b );
        out.sort();
        return out;
    };
    auto treeNames = [&]() {
        QStringList out;
        std::function<void( QTreeWidgetItem * )> walk = [&]( QTreeWidgetItem * it ) {
            out << nameOf( it->data( 0, Qt::UserRole ).toInt() );
            for ( int i = 0; i < it->childCount(); i++ )
                walk( it->child( i ) );
        };
        for ( int i = 0; i < tree->topLevelItemCount(); i++ )
            walk( tree->topLevelItem( i ) );
        out.sort();
        return out;
    };

    // ---- (k) THE OVERLAY DRAWS WHAT THE DOCK LISTS -------------------
    // By NAME, not by count: two sets of 93 that are not the same 93 would
    // pass a count comparison and be exactly the defect worth catching.
    const QStringList chipLabel = { QStringLiteral( "All" ), QStringLiteral( "Bones" ),
        QStringLiteral( "Deforming" ), QStringLiteral( "Unused" ) };
    for ( int chip = 0; chip < 4; chip++ ) {
        clickFilter( chip );
        wwGrab( ogl );
        const QStringList dockRows = treeNames();
        const QStringList drawn = listedNames();
        log << "(k) chip " << chipLabel.at( chip ) << ": dock " << dockRows.size()
            << " row(s), overlay " << drawn.size() << " listed, census filtered "
            << ogl->skeletonOverlayCensus().filtered << "\n";
        wwCheck( *st, QString( "(k) the %1 chip: the overlay lists exactly the dock's %2 row(s), by name" )
                .arg( chipLabel.at( chip ) ).arg( dockRows.size() ),
            dockRows == drawn );
    }

    // ---- (n) the census field `filtered` is WRITTEN and MOVES ---------
    clickFilter( 0 );
    wwGrab( ogl );
    const int filteredAll = ogl->skeletonOverlayCensus().filtered;
    clickFilter( 1 );
    wwGrab( ogl );
    const int filteredBones = ogl->skeletonOverlayCensus().filtered;
    clickFilter( 0 );
    wwGrab( ogl );
    log << "(n) census filtered: All " << filteredAll << ", Bones " << filteredBones << "\n";
    wwCheck( *st, QString( "(n) the census field `filtered` is written and MOVES with the chip (All %1, Bones %2)" )
            .arg( filteredAll ).arg( filteredBones ),
        filteredAll == 0 && filteredBones > 0 );

    // ---- (k) with a search, and (k') its floor ------------------------
    if ( searchBox ) {
        searchBox->setText( QStringLiteral( "Finger" ) );
        qApp->processEvents();
        wwGrab( ogl );
        const QStringList dockRows = treeNames();
        const QStringList drawn = listedNames();
        log << "(k) search 'Finger': dock " << dockRows.size()
            << " row(s), overlay " << drawn.size() << " listed\n";
        wwCheck( *st, QString( "(k) a search: the overlay lists exactly the dock's %1 row(s), by name" )
                .arg( dockRows.size() ), !dockRows.isEmpty() && dockRows == drawn );

        // FLOOR: push a chip the dock is NOT showing and the two must part.
        ogl->setSkeletonOverlayFilter( 3, QString() );
        wwGrab( ogl );
        const QStringList wrong = listedNames();
        log << "(k') FLOOR: with the Unused chip pushed behind the dock's back, overlay "
            << wrong.size() << " vs dock " << dockRows.size() << "\n";
        wwCheck( *st, QString( "(k') FLOOR: a chip the dock is not showing makes the two disagree (%1 vs %2)" )
                .arg( wrong.size() ).arg( dockRows.size() ), wrong != dockRows );

        searchBox->clear();
        qApp->processEvents();
        clickFilter( 0 );
        wwGrab( ogl );
    } else {
        wwSay( *st, QStringLiteral( "SKIP (k) search: the dock has no SkeletonSearch box" ) );
    }

    // ---- (l) SELECTION IS TWO-WAY -------------------------------------
    {
        // A finger bone: it is never on top of another joint, which the root
        // and the pelvis are, so a pick radius of 12 px means what it says.
        int probe = -1;
        QPointF probeAt;
        const QHash<int, Vector3> joints = ogl->skeletonOverlayJoints();
        for ( int b : ogl->skeletonOverlayListed() ) {
            if ( !ogl->skeletonOverlayInArmature( b ) )
                continue;
            if ( !nif->get<QString>( nif->getBlockIndex( b ), "Name" )
                    .contains( QStringLiteral( "Finger" ), Qt::CaseInsensitive ) )
                continue;
            QPointF sp;
            if ( !joints.contains( b ) || !ogl->worldToScreenForTest( joints.value( b ), sp ) )
                continue;
            if ( sp.x() < 8 || sp.y() < 8 || sp.x() > ogl->width() - 8 || sp.y() > ogl->height() - 8 )
                continue;
            probe = b;
            probeAt = sp;
            break;
        }
        log << "(l) probe bone " << probe << " at screen " << probeAt.x() << "," << probeAt.y() << "\n";
        wwCheck( *st, QStringLiteral( "(l) a drawn bone could be found on screen to click" ), probe >= 0 );

        if ( probe >= 0 ) {
            auto clickViewport = [&]( const QPointF & at ) {
                const QPoint lp( int( at.x() ), int( at.y() ) );
                QMouseEvent pr( QEvent::MouseButtonPress, lp, ogl->mapToGlobal( lp ),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
                QMouseEvent rl( QEvent::MouseButtonRelease, lp, ogl->mapToGlobal( lp ),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
                qApp->sendEvent( ogl, &pr );
                qApp->sendEvent( ogl, &rl );
                qApp->processEvents();
            };
            clickViewport( probeAt );
            const int got = ogl->activeObjectBlock();
            QTreeWidgetItem * cur = tree->currentItem();
            const int rowBlock = cur ? cur->data( 0, Qt::UserRole ).toInt() : -1;
            log << "(l) click at the bone -> viewport active " << got
                << ", dock current row " << rowBlock << "\n";
            wwCheck( *st, QString( "(l) clicking a bone in the viewport selects it (%1, wanted %2)" )
                    .arg( got ).arg( probe ), got == probe );
            wwCheck( *st, QString( "(l) and its row is the dock's current row (%1, wanted %2)" )
                    .arg( rowBlock ).arg( probe ), rowBlock == probe );

            // FLOOR: a click far from every bone must not select one. The top
            // left corner of the viewport is background on this fixture, which
            // the off-render's own background measurement confirms.
            ogl->objectSelectClick( -1, false );
            qApp->processEvents();
            clickViewport( QPointF( 12, 12 ) );
            log << "(l') FLOOR: click on empty space -> active " << ogl->activeObjectBlock() << "\n";
            wwCheck( *st, QString( "(l') FLOOR: a click away from every bone selects none (%1)" )
                    .arg( ogl->activeObjectBlock() ), ogl->activeObjectBlock() != probe );

            // THE OTHER DIRECTION: pick a row, and the viewport follows.
            QTreeWidgetItem * want = nullptr;
            std::function<QTreeWidgetItem *( QTreeWidgetItem * )> find =
                [&]( QTreeWidgetItem * it ) -> QTreeWidgetItem * {
                if ( it->data( 0, Qt::UserRole ).toInt() == probe )
                    return it;
                for ( int i = 0; i < it->childCount(); i++ )
                    if ( QTreeWidgetItem * hit = find( it->child( i ) ) )
                        return hit;
                return nullptr;
            };
            for ( int i = 0; i < tree->topLevelItemCount() && !want; i++ )
                want = find( tree->topLevelItem( i ) );
            if ( want ) {
                tree->clearSelection();
                tree->setCurrentItem( want );
                want->setSelected( true );
                qApp->processEvents();
                log << "(l) row -> viewport active " << ogl->activeObjectBlock() << "\n";
                wwCheck( *st, QString( "(l) selecting the row selects the bone in the viewport (%1, wanted %2)" )
                        .arg( ogl->activeObjectBlock() ).arg( probe ),
                    ogl->activeObjectBlock() == probe );

                // DOUBLE-CLICK FRAMES IT. A real gesture on the viewport of the
                // tree, not an invoked signal: QAbstractItemView is what turns
                // the third event into itemDoubleClicked.
                const float dist0 = ogl->cameraDistance();
                const Vector3 pos0 = ogl->cameraPosition();
                tree->scrollToItem( want, QAbstractItemView::PositionAtCenter );
                qApp->processEvents();
                const QRect r = tree->visualItemRect( want );
                const QPoint at = r.center();
                for ( QEvent::Type t : { QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                        QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease } ) {
                    QMouseEvent ev( t, at, tree->viewport()->mapToGlobal( at ),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
                    qApp->sendEvent( tree->viewport(), &ev );
                }
                qApp->processEvents();
                const float moved = ( ogl->cameraPosition() - pos0 ).length();
                log << "(l) double-click: camera distance " << dist0 << " -> "
                    << ogl->cameraDistance() << ", look-at moved " << moved << "\n";
                wwCheck( *st, QString( "(l) double-clicking the row frames the bone (distance %1 -> %2, moved %3)" )
                        .arg( dist0, 0, 'f', 2 ).arg( ogl->cameraDistance(), 0, 'f', 2 )
                        .arg( moved, 0, 'f', 2 ),
                    qAbs( ogl->cameraDistance() - dist0 ) > 1e-3f || moved > 1e-3f );
            } else {
                wwSay( *st, QStringLiteral( "SKIP (l) row->viewport: the probe bone has no row" ) );
            }
        }
        ogl->objectSelectClick( -1, false );
        qApp->processEvents();
    }

    // ---- (m) THE BONE COLUMN NEVER ELIDES TO NOTHING -------------------
    {
        clickFilter( 0 );
        qApp->processEvents();
        const int nameCol = tree->header()->sectionSize( 0 );
        const QFontMetrics fm( tree->font() );
        int deep = 0, empty = 0, elided = 0, worstDepth = 0, worstRoom = 1 << 29;
        QString worstName;
        std::function<void( QTreeWidgetItem *, int )> walk =
            [&]( QTreeWidgetItem * it, int d ) {
            if ( d >= 6 ) {
                deep++;
                const QString nm = it->text( 0 );
                if ( nm.isEmpty() )
                    empty++;
                const int room = nameCol - tree->indentation() * ( d + 1 );
                const int need = fm.horizontalAdvance( nm );
                if ( room < need )
                    elided++;
                if ( room - need < worstRoom ) {
                    worstRoom = room - need;
                    worstName = nm;
                    worstDepth = d;
                }
            }
            for ( int i = 0; i < it->childCount(); i++ )
                walk( it->child( i ), d + 1 );
        };
        for ( int i = 0; i < tree->topLevelItemCount(); i++ )
            walk( tree->topLevelItem( i ), 0 );
        log << "(m) dock width " << dock->width() << ", tree viewport "
            << tree->viewport()->width() << ", name column " << nameCol
            << ", indent " << tree->indentation() << "\n";
        log << "(m) rows at depth >= 6: " << deep << "; empty names " << empty
            << "; elided " << elided << "; tightest '" << worstName << "' at depth "
            << worstDepth << " with " << worstRoom << " px to spare\n";
        wwCheck( *st, QString( "(m) FLOOR: the fixture actually has deep rows to measure (%1 at depth >= 6)" )
                .arg( deep ), deep > 0 );
        wwCheck( *st, QString( "(m) every row at depth 6+ shows its whole name (%1 empty, %2 elided)" )
                .arg( empty ).arg( elided ), deep > 0 && empty == 0 && elided == 0 );

        // FLOOR, in arithmetic: the SHIPPED column law on these same rows.
        // Column 0 stretched into what the three ResizeToContents columns left,
        // and Qt's default 20 px of indentation ate it. The spell runs the real
        // thing with WW_SKELETON_LEGACY_COLUMNS=1 and photographs it.
        const int numeric = tree->sizeHintForColumn( 1 ) + tree->sizeHintForColumn( 2 )
            + tree->sizeHintForColumn( 3 );
        const int legacyRoom = tree->viewport()->width() - numeric
            - 20 * ( worstDepth + 1 ) - fm.horizontalAdvance( worstName );
        log << "(m') FLOOR arithmetic: viewport " << tree->viewport()->width()
            << " - numeric " << numeric << " - 20*" << ( worstDepth + 1 )
            << " - name " << fm.horizontalAdvance( worstName )
            << " = " << legacyRoom << " px for '" << worstName << "'\n";
        wwCheck( *st, QString( "(m') FLOOR: under the shipped column law that row had %1 px, i.e. none" )
                .arg( legacyRoom ), legacyRoom <= 0 );
    }

    // ---- (o) THE THREE DISPLAY MODES ARE THREE PICTURES ---------------
    {
        ogl->setArmatureDisplay( GLView::ArmOctahedral );
        const QImage octa = wwGrab( ogl );
        ogl->setArmatureDisplay( GLView::ArmStick );
        const QImage stick = wwGrab( ogl );
        ogl->setArmatureDisplay( GLView::ArmWire );
        const QImage wire = wwGrab( ogl );
        const int dOS = wwDiffPixels( octa, stick, nullptr );
        const int dOW = wwDiffPixels( octa, wire, nullptr );
        const int dSW = wwDiffPixels( stick, wire, nullptr );
        log << "(o) display modes: Octahedral vs Stick " << dOS
            << ", Octahedral vs Wire " << dOW << ", Stick vs Wire " << dSW << "\n";
        wwCheck( *st, QString( "(o) Octahedral, Stick and Wire are three different pictures (%1 / %2 / %3 px, bar 2000)" )
                .arg( dOS ).arg( dOW ).arg( dSW ),
            dOS > 2000 && dOW > 2000 && dSW > 2000 );
        ogl->setArmatureDisplay( GLView::ArmOctahedral );
        wwGrab( ogl );
    }

    // ---- (p) X-RAY -----------------------------------------------------
    {
        const QImage through = wwGrab( ogl );
        ogl->setArmatureXray( false );
        const QImage behind = wwGrab( ogl );
        const int d = wwDiffPixels( through, behind, nullptr );
        log << "(p) X-ray on vs off: " << d << " pixels\n";
        wwCheck( *st, QString( "(p) X-ray off hides the bones inside the mesh (%1 pixels differ, bar 2000)" )
                .arg( d ), d > 2000 );
        ogl->setArmatureXray( true );
        wwGrab( ogl );
    }
}

