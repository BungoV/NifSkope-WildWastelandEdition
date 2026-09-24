/* WW_SKELOVERLAY_TEST: the pre-registered gates of lane SKELOVERLAY
   (Overlays > Show Skeleton), run inside the real application against the real
   scene graph and the real Skeleton Manager dock.

   bungo's ruling, 2026-09-10, verbatim: "Add to the overlays: View skeleton,
   shows you the bones, basically the same view as in the skeleton manager".
   "The same view as in the skeleton manager" is the whole gate: it is not
   enough that something draws, it has to draw THE SAME BONES, in the same
   classes, as the dock does.

   It lives in its own translation unit rather than beside the ninety other
   WW_*_TEST harnesses in nifskope_ui.cpp because that file is 31,000 lines and
   two other lanes are writing into it this session; the whole of this lane's
   footprint there is the one line that calls wwSkelOverlayHarness().

   WHAT IT MEASURES (the letters are the brief's)

     (a) the overlay's bone count is the SKELETON MANAGER'S. Not "equals
         skeletonAnalyse()" -- the overlay is built from skeletonAnalyse(), so
         that would be our own output judging our own output (CONSTITUTION rule
         4). The number is read off the DOCK: the rows its tree actually
         contains under the All filter, which is what bungo is comparing
         against when he says "the same view".
         FLOOR: with the overlay OFF the census must be all zeros, so a census
         that is never written cannot pass by being right by accident.
     (b) the per-class colour counts are the dock's FILTER counts: the
         deforming-coloured bones = the rows the Deforming filter shows, the
         unused-coloured = the Unused filter, the bone-coloured (deforming +
         unused) = the Bones filter, and the muted ones = All minus Bones.
     (c) a render with the overlay ON differs from the same render with it OFF
         ONLY in the overlay's own pixels: every differing pixel lies within
         the mask rasterised from the segments the overlay REPORTS having drawn
         (readback, not a re-derivation) plus its joint markers.
         FLOOR: some pixels must differ -- an overlay that draws nothing would
         otherwise pass this gate perfectly.
     (d) with a clip loaded and the scene scrubbed to frame N, every joint the
         overlay drew is within 1e-3 of the ANIMATED NiNode's world position,
         read back off Node::worldTrans() at that moment.
         FLOOR: those same drawn positions held against the BIND pose must
         fail on at least one bone, or "it follows the animation" is vacuous.
     (e) toggling the overlay back off restores the off-render BYTE for byte.

   ADDED BY LANE SKELFIX, 2026-09-10, after the director saw long segments
   fanning out of the character in on_frame46.png. Gates (a)-(e) all still
   hold: the rule below changes only which pairs get a BODY, never which nodes
   get a joint marker, so the census still equals the dock's.

     (f) at the animated frame, no drawn segment is longer than 1.5x the
         longest BIND-pose segment between two nodes the Skeleton Manager calls
         bones. The limit is measured on the bind pose and on the dock's own
         classes, so it is not derived from the rule under test.
         FLOOR: the OLD rule -- every parent -> child pair, from the same
         readback -- must FAIL that same comparison.
     (g) every drawn segment endpoint lies inside the bones' own bounding box
         at that frame, grown by 5% of its diagonal.
         FLOOR: the old rule must put endpoints outside it.
     (h) the new census field `skipped` is WRITTEN and MOVES (the three rules
         of 2026-09-04 21:33): zero with the overlay off, the count of refused
         bodies with it on, and the joint-marker count unchanged by the rule.
     (i) the overlay states the rule in words, and the armature it built has
         both members and non-members.

   THE LINE-PATH WARM-UP is not optional here. Streaming line geometry draws
   nothing until a pick render has run (the open 07-17 defect); without
   indexAt() first, the bones are missing from the grab and only the joint dots
   survive -- which would quietly turn gate (c) into a test of three hundred
   points. WW_SKELETON_TEST carries the same note for the same reason.

   ADDED BY LANE SKEL2, 2026-09-11, after bungo's ruling that the overlay is to
   MIRROR the Skeleton Manager, that both views are to share one renderer, and
   that the bone is to be Blender's octahedron in blue.

     (c)/(e) NOW CARRY A MEASURED TOLERANCE. Both demanded EXACT framebuffer
         equality on a grabFramebuffer() result that is not bit-stable between
         repaints. Five identical runs on the rung (release/NifSkope.exe
         07:06:04) measured (c) at 16 / 13 / 14 / 9 / 1 pixels and (e) at
         12 / 22 / 19 / 15 / 21; BUILD11's four runs of the previous exe
         measured 10 / 0 / 4 / 0 and 17 / 0 / 37 / 2. The worst value either
         check has ever shown is 37, out of ~1.2 million pixels. The bar is
         64 px, fixed, written into the check's own sentence -- and it carries
         its own floor: the SAME bar is asserted to refuse the overlay's real
         ON-vs-OFF difference, which is tens of thousands of pixels, so a
         tolerance that swallowed everything would be seen doing it.
     (k) THE MIRROR. Under each of the dock's four chips, and under a search,
         the set of blocks the overlay LISTS equals the set of blocks the dock's
         tree lists -- by count and by name, not by count alone.
         FLOOR: a chip pushed to the viewport that the dock is NOT showing must
         make the two disagree.
     (l) SELECTION IS TWO-WAY. A synthetic click at a drawn bone's screen
         position selects that block in the viewport AND makes its row current
         in the dock; selecting a row selects the block in the viewport; a
         double-click on a row moves the camera (Frame Selected over a bone).
         FLOOR: a click on empty space must not select a bone.
     (m) THE BONE COLUMN. Every row at depth 6 or deeper carries a non-empty
         name that is NOT elided at the dock's own width.
         FLOOR: the same rows measured under the shipped column law
         (WW_SKELETON_LEGACY_COLUMNS=1, the exact way back) have no room left.
     (n) the census field `filtered` is WRITTEN and MOVES with the chip.
     (o) the three display modes are three different pictures, and `Wire` is
         the way back: it draws the SAME segment endpoints as before.
     (p) X-ray off changes the picture (the bones are hidden by the mesh).

   ENVIRONMENT (tests/spells/skeleton_overlay.sh sets all of them):
     WW_SKELOVERLAY_TEST=1        arm the harness
     WW_SKELOVERLAY_CLIP=<file>   a .hkx for gate (d); absent = (d) refuses
     WW_SKELOVERLAY_FRAME=46      the frame to scrub to
     WW_SKELOVERLAY_FPS=60        that clip's rate
     WW_SKELOVERLAY_SHOTDIR=<dir> where the gate's own evidence images go
     WW_SKELOVERLAY_DOCKSHOT=<png> grab the Skeleton Manager dock at 400 px
   Log: release/ww_skeloverlay_test.log

   Lane SKELOVERLAY, 2026-09-10; lane SKEL2, 2026-09-11. */

#include "nifskope.h"
#include "glview.h"
#include "skeletontools.h"
#include "hkxplayback.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUndoStack>

#include <cmath>
#include <functional>

namespace
{

struct WwSkelState
{
	QTextStream * out = nullptr;
	int checks = 0;
	int fails = 0;
	QString clipPath;
	QString shotDir;
	int frame = 46;
	float fps = 60.0f;
};

void wwSay( WwSkelState & st, const QString & s )
{
	if ( st.out )
		( *st.out ) << s << "\n";
}

void wwCheck( WwSkelState & st, const QString & what, bool ok )
{
	st.checks++;
	if ( !ok )
		st.fails++;
	if ( st.out )
		( *st.out ) << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
}

//! Rows in a QTreeWidget, children included. The dock's own harness counts the
//! same way; a top-level count would miss every child bone.
int wwCountRows( QTreeWidget * t )
{
	int n = 0;
	std::function<void( QTreeWidgetItem * )> walk = [&]( QTreeWidgetItem * it ) {
		n++;
		for ( int i = 0; i < it->childCount(); i++ )
			walk( it->child( i ) );
	};
	for ( int i = 0; i < t->topLevelItemCount(); i++ )
		walk( t->topLevelItem( i ) );
	return n;
}

//! Pump the viewport until the grab is of the frame we just asked for.
//! grabFramebuffer() reads the CURRENT buffer without repainting.
QImage wwGrab( GLView * ogl )
{
	for ( int i = 0; i < 3; i++ ) {
		ogl->update();
		qApp->processEvents();
	}
	return ogl->grabFramebuffer();
}

//! Pixels that differ between two grabs of the same size, and where they are.
int wwDiffPixels( const QImage & a, const QImage & b, QVector<QPoint> * where )
{
	if ( a.size() != b.size() || a.isNull() || b.isNull() )
		return -1;
	const QImage x = a.convertToFormat( QImage::Format_ARGB32 );
	const QImage y = b.convertToFormat( QImage::Format_ARGB32 );
	int n = 0;
	for ( int j = 0; j < x.height(); j++ ) {
		const QRgb * rx = reinterpret_cast<const QRgb *>( x.constScanLine( j ) );
		const QRgb * ry = reinterpret_cast<const QRgb *>( y.constScanLine( j ) );
		for ( int i = 0; i < x.width(); i++ ) {
			if ( rx[i] != ry[i] ) {
				n++;
				if ( where )
					where->append( QPoint( i, j ) );
			}
		}
	}
	return n;
}

} // namespace

void wwSkelOverlayHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_SKELOVERLAY_TEST" ) )
		return;

	auto * st = new WwSkelState;
	st->clipPath = qEnvironmentVariable( "WW_SKELOVERLAY_CLIP" );
	st->shotDir = qEnvironmentVariable( "WW_SKELOVERLAY_SHOTDIR" );
	if ( qEnvironmentVariableIsSet( "WW_SKELOVERLAY_FRAME" ) )
		st->frame = qEnvironmentVariableIntValue( "WW_SKELOVERLAY_FRAME" );
	if ( qEnvironmentVariableIsSet( "WW_SKELOVERLAY_FPS" ) ) {
		const float f = qEnvironmentVariable( "WW_SKELOVERLAY_FPS" ).toFloat();
		if ( f > 0.0f )
			st->fps = f;
	}

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		// 1.5 s, like the other WW harnesses: the scene is built on the load
		// signal but the first paint, and so the first transform walk, is not.
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			// The log file is a STACK object and the stream is destroyed before
			// it: a QTextStream flushing into a device that has already been
			// deleted writes the last line into freed memory, and the last line
			// is PASS/FAIL.
			QFile logf( QApplication::applicationDirPath() + "/ww_skeloverlay_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			st->out = &log;

			NifModel * nif = skope->getNifModel();
			GLView * ogl = skope->getGLView();

			do {
				if ( !ok || !nif || !ogl ) {
					wwCheck( *st, QStringLiteral( "the file loaded" ), false );
					break;
				}
				log << "nodes in file: " << nif->getBlockCount() << "\n";

				// ---- the Skeleton Manager's own numbers, off the DOCK -------
				QDockWidget * dock = skope->findChild<QDockWidget *>(
					QStringLiteral( "SkeletonManagerDock" ) );
				if ( !dock ) {
					wwCheck( *st, QStringLiteral( "the Skeleton Manager dock exists" ), false );
					break;
				}
				dock->show();
				dock->raise();
				qApp->processEvents();
				auto * tree = dock->findChild<QTreeWidget *>( QStringLiteral( "SkeletonTree" ) );
				auto * footer = dock->findChild<QLabel *>( QStringLiteral( "SkeletonFooter" ) );
				if ( !tree || !footer ) {
					wwCheck( *st, QStringLiteral( "the dock's tree and footer exist" ), false );
					break;
				}
				auto clickFilter = [dock]( int i ) {
					auto * b = dock->findChild<QToolButton *>(
						QStringLiteral( "SkeletonFilter%1" ).arg( i ) );
					if ( b )
						b->click();
					qApp->processEvents();
					return b != nullptr;
				};
				clickFilter( 0 );
				const int dockAll = wwCountRows( tree );
				clickFilter( 1 );
				const int dockBones = wwCountRows( tree );
				clickFilter( 2 );
				const int dockDeforming = wwCountRows( tree );
				clickFilter( 3 );
				const int dockUnused = wwCountRows( tree );
				clickFilter( 0 );
				log << "dock: All " << dockAll << ", Bones " << dockBones
					<< ", Deforming " << dockDeforming << ", Unused " << dockUnused << "\n";
				log << "dock footer: " << footer->text() << "\n";

				// The shared analysis, printed for the record only. It is NOT
				// what gates (a) and (b) compare against -- see the file header.
				const SkeletonReport rep = skeletonAnalyse( nif );
				log << "analyse (record only): " << rep.bones.size() << " nodes, "
					<< ( rep.deformingCount() + rep.unusedCount() ) << " bones, "
					<< rep.deformingCount() << " deforming, " << rep.unusedCount()
					<< " unused, " << rep.skinnedShapes << " skinned shape(s)\n";

				/* FORCE EVERY STATE THIS GATE MEASURES (lane SKEL2).
				 *
				 * The Bone Display rows persist in QSettings, so without this
				 * the whole run would measure whatever bungo last ticked --
				 * ww-test-harness-add section 6, a harness that inherits its
				 * state is a measurement of the machine.
				 */
				ogl->setArmatureDisplay( GLView::ArmOctahedral );
				ogl->setArmatureXray( true );
				ogl->setArmatureNames( 0 );
				ogl->setSkeletonOverlayFilter( 0, QString() );

				// ---- (a) FLOOR: nothing is written while the overlay is off --
				ogl->setSkeletonOverlay( false );
				// Warm the line path. Without a pick render, streaming LINE
				// geometry draws nothing and only the joint dots would appear.
				ogl->indexAt( QPointF( ogl->width() * 0.5, ogl->height() * 0.5 ) );
				qApp->processEvents();
				const QImage imgOff = wwGrab( ogl );
				GLView::SkeletonOverlayCensus c0 = ogl->skeletonOverlayCensus();
				log << "overlay OFF census: nodes " << c0.nodes << ", segments "
					<< c0.segments << ", draws " << c0.draws << "\n";
				wwCheck( *st, QStringLiteral( "(a') FLOOR: the census is empty while the overlay is off" ),
					c0.nodes == 0 && c0.segments == 0 && c0.stubs == 0 && c0.skipped == 0
					&& c0.draws == 0 );
				wwCheck( *st, QStringLiteral( "the off-render is a real image" ),
					!imgOff.isNull() && imgOff.width() > 16 && imgOff.height() > 16 );
				log << "render size: " << imgOff.width() << "x" << imgOff.height() << "\n";

				// ---- turn it on --------------------------------------------
				ogl->setSkeletonOverlay( true );
				const QImage imgOn = wwGrab( ogl );
				const GLView::SkeletonOverlayCensus c = ogl->skeletonOverlayCensus();
				log << "overlay ON census: nodes " << c.nodes << ", bones " << c.bones
					<< ", deforming " << c.deforming << ", unused " << c.unused
					<< ", notABone " << c.notABone << ", segments " << c.segments
					<< ", stubs " << c.stubs << ", missingNodes " << c.missingNodes
					<< ", draws " << c.draws << "\n";

				// ---- (a) the count is the dock's ---------------------------
				wwCheck( *st, QString( "(a) joint markers %1 + blocks with no scene node %2 = the dock's All filter %3" )
						.arg( c.nodes ).arg( c.missingNodes ).arg( dockAll ),
					c.nodes + c.missingNodes == dockAll );
				wwCheck( *st, QString( "(a) every block the dock lists has a node to draw at (missing %1)" )
						.arg( c.missingNodes ), c.missingNodes == 0 );
				wwCheck( *st, QStringLiteral( "(a) the overlay drew something" ), c.nodes > 0 );
				wwCheck( *st, QStringLiteral( "(a) the census counts a draw" ), c.draws >= 1 );

				// ---- (b) the classes are the dock's filters -----------------
				wwCheck( *st, QString( "(b) bone-coloured %1 = the Bones filter %2" )
						.arg( c.bones ).arg( dockBones ), c.bones == dockBones );
				wwCheck( *st, QString( "(b) deforming-coloured %1 = the Deforming filter %2" )
						.arg( c.deforming ).arg( dockDeforming ), c.deforming == dockDeforming );
				wwCheck( *st, QString( "(b) unused-coloured %1 = the Unused filter %2" )
						.arg( c.unused ).arg( dockUnused ), c.unused == dockUnused );
				wwCheck( *st, QString( "(b) muted %1 = All %2 minus Bones %3" )
						.arg( c.notABone ).arg( dockAll ).arg( dockBones ),
					c.notABone == dockAll - dockBones );

				// ---- (c) the picture changes, and only where it may ---------
				QVector<QPoint> diffAt;
				const int nDiff = wwDiffPixels( imgOff, imgOn, &diffAt );
				const double pct = imgOff.isNull() ? 0.0
					: 100.0 * double( nDiff ) / double( imgOff.width() * imgOff.height() );
				log << "pixels changed by the overlay: " << nDiff
					<< QString( " (%1%)" ).arg( pct, 0, 'f', 3 ) << "\n";
				wwCheck( *st, QStringLiteral( "(c') FLOOR: turning the overlay on changes some pixels" ),
					nDiff > 0 );

				// The mask is rasterised from what the overlay REPORTS having
				// drawn -- its segment endpoints and its joint positions --
				// projected through the same worldToScreen the viewport uses.
				// Cell grid at 8 px, dilated by 3 cells, which covers the line
				// width, the point size and the octahedral collar.
				const int cell = 8, dil = 3;
				const int gw = ( imgOff.width() + cell - 1 ) / cell;
				const int gh = ( imgOff.height() + cell - 1 ) / cell;
				QVector<quint8> mask( gw * gh, 0 );
				auto markCell = [&]( int cx, int cy ) {
					for ( int dy = -dil; dy <= dil; dy++ )
						for ( int dx = -dil; dx <= dil; dx++ ) {
							const int x = cx + dx, y = cy + dy;
							if ( x >= 0 && y >= 0 && x < gw && y < gh )
								mask[y * gw + x] = 1;
						}
				};
				auto markPoint = [&]( const QPointF & p ) {
					markCell( int( p.x() ) / cell, int( p.y() ) / cell );
				};
				int marked = 0;
				const QHash<int, Vector3> joints = ogl->skeletonOverlayJoints();
				for ( auto it = joints.constBegin(); it != joints.constEnd(); ++it ) {
					QPointF p;
					if ( ogl->worldToScreenForTest( it.value(), p ) ) {
						markPoint( p );
						marked++;
					}
				}
				const QVector<QPair<Vector3, Vector3>> segs = ogl->skeletonOverlaySegments();
				log << "mask sources: " << joints.size() << " joints, " << segs.size()
					<< " segments; " << marked << " joints projected on screen\n";
				for ( const QPair<Vector3, Vector3> & s : segs ) {
					QPointF a, b;
					const bool oa = ogl->worldToScreenForTest( s.first, a );
					const bool ob = ogl->worldToScreenForTest( s.second, b );
					if ( !oa || !ob )
						continue;
					const double len = std::hypot( b.x() - a.x(), b.y() - a.y() );
					const int steps = qMax( 2, int( len ) + 2 );
					for ( int i = 0; i <= steps; i++ ) {
						const double t = double( i ) / double( steps );
						markPoint( QPointF( a.x() + ( b.x() - a.x() ) * t,
											a.y() + ( b.y() - a.y() ) * t ) );
					}
				}
				int outside = 0;
				QPoint firstOutside( -1, -1 );
				for ( const QPoint & p : diffAt ) {
					const int gx = p.x() / cell, gy = p.y() / cell;
					if ( gx < 0 || gy < 0 || gx >= gw || gy >= gh || !mask[gy * gw + gx] ) {
						if ( outside == 0 )
							firstOutside = p;
						outside++;
					}
				}
				log << "changed pixels outside the overlay's own mask: " << outside;
				if ( outside )
					log << "  (first at " << firstOutside.x() << "," << firstOutside.y() << ")";
				log << "\n";
				/* THE TOLERANCE, and where its number comes from (lane SKEL2).
				 *
				 * 64 pixels, fixed. Not a guess and not a percentage: five
				 * identical runs on the rung measured this check at
				 * 16 / 13 / 14 / 9 / 1 and BUILD11 measured the previous exe at
				 * 10 / 0 / 4 / 0, so the worst value on record is 37 out of
				 * ~1.2 million pixels. grabFramebuffer() is not bit-stable
				 * between repaints and this check demanded exact equality,
				 * which is why it has been red on a correct overlay for two
				 * builds (BUILD11 red 3).
				 */
				const int wwJitter = 64;
				wwCheck( *st, QString( "(c) the render differs ONLY in the overlay's pixels (%1 outside the mask, bar %2)" )
						.arg( outside ).arg( wwJitter ), outside <= wwJitter );
				// FLOOR: the SAME bar must still refuse a real difference, or a
				// tolerance that swallowed everything would pass unnoticed.
				wwCheck( *st, QString( "(c') FLOOR: the %1-pixel bar still refuses the overlay itself (%2 pixels changed)" )
						.arg( wwJitter ).arg( nDiff ), nDiff > wwJitter );
				// A mask that covered the whole frame would pass (c) for free.
				int cellsMarked = 0;
				for ( quint8 m : mask )
					cellsMarked += m;
				const double maskPct = 100.0 * double( cellsMarked ) / double( qMax( 1, gw * gh ) );
				log << QString( "mask covers %1% of the frame\n" ).arg( maskPct, 0, 'f', 2 );
				wwCheck( *st, QString( "(c') FLOOR: the mask is not the whole frame (%1%)" )
						.arg( maskPct, 0, 'f', 2 ), maskPct < 80.0 );

				// ---- (e) off restores the exact off-render ------------------
				ogl->setSkeletonOverlay( false );
				const QImage imgOff2 = wwGrab( ogl );
				const int backDiff = wwDiffPixels( imgOff, imgOff2, nullptr );
				log << "pixels differing after toggling back off: " << backDiff << "\n";
				wwCheck( *st, QString( "(e) toggling off restores the off-render (%1 pixels differ, bar %2)" )
						.arg( backDiff ).arg( wwJitter ), backDiff >= 0 && backDiff <= wwJitter );

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
						const int numeric = tree->header()->sectionSizeHint( 1 )
							+ tree->header()->sectionSizeHint( 2 )
							+ tree->header()->sectionSizeHint( 3 );
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

				// ---- (d) the overlay follows the animated pose --------------
				Scene * sc = ogl->getScene();
				if ( st->clipPath.isEmpty() || !QFileInfo::exists( st->clipPath ) ) {
					wwSay( *st, QStringLiteral( "no WW_SKELOVERLAY_CLIP on disk: gate (d) not run" ) );
					wwCheck( *st, QStringLiteral( "(d) a clip was supplied" ), false );
					break;
				}
				if ( !sc || !sc->hkx ) {
					wwCheck( *st, QStringLiteral( "(d) the scene has a playback" ), false );
					break;
				}
				ogl->setSkeletonOverlay( true );
				wwGrab( ogl );
				// The bind-pose positions, kept for (d)'s floor.
				const QHash<int, Vector3> bindJoints = ogl->skeletonOverlayJoints();

				QStringList added;
				const QString err = sc->hkx->load( st->clipPath, &added );
				log << "clip: " << QFileInfo( st->clipPath ).fileName()
					<< "  added " << added.count()
					<< ( err.isEmpty() ? QString() : QString( "  refusal: %1" ).arg( err ) ) << "\n";
				log << "mapping: " << sc->hkx->summary() << "\n";
				wwCheck( *st, QStringLiteral( "(d) the clip loaded and named a sequence" ),
					err.isEmpty() && !added.isEmpty() );
				if ( added.isEmpty() )
					break;
				ogl->setSceneSequence( added.first() );
				const float t = float( st->frame ) / st->fps;
				ogl->setSceneTime( t );
				const QImage imgFrame = wwGrab( ogl );
				log << "scrubbed to frame " << st->frame << " at " << st->fps
					<< " fps = t " << t << "\n";

				const QHash<int, Vector3> drawn = ogl->skeletonOverlayJoints();
				double worst = 0.0;
				int worstBlock = -1, compared = 0;
				for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
					Node * n = sc->getNode( nif, nif->getBlockIndex( it.key() ) );
					if ( !n )
						continue;
					const Vector3 w = n->worldTrans().translation;
					const double d = ( w - it.value() ).length();
					compared++;
					if ( d > worst ) {
						worst = d;
						worstBlock = it.key();
					}
				}
				log << "(d) " << compared << " joints compared against the animated nodes; worst "
					<< QString::number( worst, 'g', 6 ) << " units at block " << worstBlock << "\n";
				wwCheck( *st, QString( "(d) every drawn joint is the animated node's world position (worst %1 <= 1e-3)" )
						.arg( worst, 0, 'g', 4 ), compared > 0 && worst <= 1e-3 );

				// FLOOR: the same drawn positions against the BIND pose must
				// disagree, or nothing about the clip reached the rig.
				double moved = 0.0;
				int movedBlock = -1, movedCount = 0;
				for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
					if ( !bindJoints.contains( it.key() ) )
						continue;
					const double d = ( bindJoints.value( it.key() ) - it.value() ).length();
					if ( d > 1e-3 )
						movedCount++;
					if ( d > moved ) {
						moved = d;
						movedBlock = it.key();
					}
				}
				log << "(d') " << movedCount << " joints moved from the bind pose; largest "
					<< QString::number( moved, 'g', 6 ) << " units at block " << movedBlock << "\n";
				wwCheck( *st, QString( "(d') FLOOR: the clip actually moved the overlay (%1 joints, largest %2)" )
						.arg( movedCount ).arg( moved, 0, 'g', 4 ),
					movedCount > 0 && moved > 1e-3 );

				// ---- (f) NO SEGMENT REACHES OFF THE CHARACTER ---------------
				//
				// Lane SKELFIX. The defect this gate exists for: at this frame
				// the first cut of the overlay drew bodies 300 units long,
				// fanning out of the character to the world origin, because the
				// clip carries the travel on COM while Root, Camera, CamTarget
				// and the AnimObject nodes stay at the origin.
				//
				// The limit is measured on the BIND pose, from bones the
				// Skeleton Manager calls bones, so it is not derived from the
				// rule under test: 1.5x the longest bind-pose segment between
				// two such bones. Every check has the OLD rule beside it as the
				// floor -- the same readback, every parent -> child pair, which
				// must FAIL the same comparison.
				{
					auto boneRow = [&]( int blk ) {
						const int cl = ogl->skeletonOverlayClassOf( blk );
						return cl == int( GLView::SkelDeforming ) || cl == int( GLView::SkelUnused );
					};
					double longestBind = 0.0;
					int pairsBind = 0;
					for ( auto it = bindJoints.constBegin(); it != bindJoints.constEnd(); ++it ) {
						const int p = nif->getParent( it.key() );
						if ( p < 0 || !bindJoints.contains( p ) )
							continue;
						if ( !boneRow( it.key() ) || !boneRow( p ) )
							continue;
						pairsBind++;
						longestBind = qMax( longestBind,
							double( ( bindJoints.value( p ) - it.value() ).length() ) );
					}
					const double limit = 1.5 * longestBind;
					log << "(f) longest BIND-pose bone-to-bone segment " << longestBind
						<< " over " << pairsBind << " pairs; limit " << limit << "\n";
					wwCheck( *st, QString( "(f) the limit is a real measurement (%1 pairs, longest %2)" )
							.arg( pairsBind ).arg( longestBind, 0, 'f', 2 ),
						pairsBind > 0 && longestBind > 0.0 );

					const QVector<QPair<Vector3, Vector3>> segNow = ogl->skeletonOverlaySegments();
					double worstSeg = 0.0;
					for ( const QPair<Vector3, Vector3> & s : segNow )
						worstSeg = qMax( worstSeg, double( ( s.second - s.first ).length() ) );
					log << "(f) " << segNow.count() << " segments drawn at frame " << st->frame
						<< "; longest " << worstSeg << "\n";
					wwCheck( *st, QString( "(f) no drawn segment is longer than %1 (longest %2)" )
							.arg( limit, 0, 'f', 2 ).arg( worstSeg, 0, 'f', 2 ),
						!segNow.isEmpty() && worstSeg <= limit );

					// FLOOR: the rule this replaced, on the same numbers.
					double worstOld = 0.0;
					int oldPairs = 0;
					for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
						const int p = nif->getParent( it.key() );
						if ( p < 0 || !drawn.contains( p ) )
							continue;
						oldPairs++;
						worstOld = qMax( worstOld, double( ( drawn.value( p ) - it.value() ).length() ) );
					}
					log << "(f') FLOOR: the old rule (every parent -> child pair) would draw "
						<< oldPairs << " segments, longest " << worstOld << "\n";
					wwCheck( *st, QString( "(f') FLOOR: the old rule FAILS the same limit (longest %1 > %2)" )
							.arg( worstOld, 0, 'f', 2 ).arg( limit, 0, 'f', 2 ),
						worstOld > limit );

					// ---- (g) every endpoint is on the character ------------
					// The box is the bones' own extent at this frame, grown by
					// 5% of its diagonal -- room for a leaf's stub, which is
					// capped at twice the characteristic bone size and so is
					// always far smaller than that.
					Vector3 lo, hi;
					int nBox = 0;
					for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
						if ( !boneRow( it.key() ) )
							continue;
						const Vector3 v = it.value();
						if ( nBox++ == 0 ) {
							lo = v;
							hi = v;
							continue;
						}
						for ( int k = 0; k < 3; k++ ) {
							lo[k] = qMin( lo[k], v[k] );
							hi[k] = qMax( hi[k], v[k] );
						}
					}
					const double margin = 0.05 * double( ( hi - lo ).length() );
					auto outsideBy = [&]( const Vector3 & v ) {
						double d = 0.0;
						for ( int k = 0; k < 3; k++ )
							d = qMax( d, qMax( double( lo[k] - v[k] ), double( v[k] - hi[k] ) ) );
						return qMax( 0.0, d );
					};
					log << "(g) bone box over " << nBox << " bones: ("
						<< lo[0] << ", " << lo[1] << ", " << lo[2] << ") .. ("
						<< hi[0] << ", " << hi[1] << ", " << hi[2] << "), margin " << margin << "\n";
					double worstOut = 0.0;
					int nOut = 0;
					for ( const QPair<Vector3, Vector3> & s : segNow ) {
						for ( const Vector3 & v : { s.first, s.second } ) {
							const double d = outsideBy( v );
							worstOut = qMax( worstOut, d );
							if ( d > margin )
								nOut++;
						}
					}
					log << "(g) segment endpoints outside the box+margin: " << nOut
						<< "; worst overshoot " << worstOut << "\n";
					wwCheck( *st, QString( "(g) every drawn segment stays on the character (%1 outside, worst %2)" )
							.arg( nOut ).arg( worstOut, 0, 'f', 2 ),
						nBox > 0 && nOut == 0 );

					int nOutOld = 0;
					double worstOutOld = 0.0;
					for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
						const int p = nif->getParent( it.key() );
						if ( p < 0 || !drawn.contains( p ) )
							continue;
						for ( const Vector3 & v : { drawn.value( p ), it.value() } ) {
							const double d = outsideBy( v );
							worstOutOld = qMax( worstOutOld, d );
							if ( d > margin )
								nOutOld++;
						}
					}
					log << "(g') FLOOR: the old rule put " << nOutOld
						<< " endpoints outside, worst " << worstOutOld << "\n";
					wwCheck( *st, QString( "(g') FLOOR: the old rule FAILS the box (%1 outside, worst %2)" )
							.arg( nOutOld ).arg( worstOutOld, 0, 'f', 2 ), nOutOld > 0 );

					// ---- (h) the census field is WRITTEN and MOVES ---------
					// The three rules of 2026-09-04 21:33, rule 1: a new census
					// field ships with a test that it is written and that it
					// moves. `skipped` is zero with the overlay off (the (a')
					// floor above), and here it must be the bodies the rule
					// actually refused.
					const GLView::SkeletonOverlayCensus c2 = ogl->skeletonOverlayCensus();
					log << "(h) frame census: nodes " << c2.nodes << ", segments " << c2.segments
						<< ", stubs " << c2.stubs << ", skipped " << c2.skipped << "\n";
					wwCheck( *st, QString( "(h) the census counts the bodies the rule refused (skipped %1 > 0)" )
							.arg( c2.skipped ), c2.skipped > 0 );
					wwCheck( *st, QString( "(h) segments %1 = the segments reported for the mask %2" )
							.arg( c2.segments ).arg( segNow.count() ),
						c2.segments + c2.stubs == segNow.count() );
					wwCheck( *st, QString( "(h) the joint markers are untouched by the rule (%1 = the dock's All %2)" )
							.arg( c2.nodes ).arg( dockAll ), c2.nodes + c2.missingNodes == dockAll );

					// ---- (i) the rule says what it is ----------------------
					const QString rule = ogl->skeletonOverlayRule();
					log << "(i) rule: " << rule << "\n";
					wwCheck( *st, QStringLiteral( "(i) the overlay states its rule in words" ),
						rule.length() > 40 );
					int armature = 0, outsideArm = 0;
					for ( auto it = drawn.constBegin(); it != drawn.constEnd(); ++it ) {
						if ( ogl->skeletonOverlayInArmature( it.key() ) )
							armature++;
						else
							outsideArm++;
					}
					log << "(i) armature nodes " << armature << ", joint-marker-only "
						<< outsideArm << "\n";
					wwCheck( *st, QString( "(i) some nodes are marker-only, and most are armature (%1 / %2)" )
							.arg( outsideArm ).arg( armature ),
						outsideArm > 0 && armature > outsideArm );
				}

				// ---- the evidence images -----------------------------------
				if ( !st->shotDir.isEmpty() ) {
					QDir().mkpath( st->shotDir );
					const QString base = st->shotDir + QStringLiteral( "/gate_" );
					imgOff.save( base + QStringLiteral( "off.png" ) );
					imgOn.save( base + QStringLiteral( "on.png" ) );
					imgFrame.save( base + QStringLiteral( "on_frame%1.png" ).arg( st->frame ) );
					// The mask itself, so "only the overlay's pixels" can be
					// looked at rather than believed.
					QImage maskImg( imgOff.size(), QImage::Format_ARGB32 );
					maskImg.fill( QColor( 0, 0, 0, 255 ) );
					for ( int y = 0; y < maskImg.height(); y++ ) {
						QRgb * row = reinterpret_cast<QRgb *>( maskImg.scanLine( y ) );
						for ( int x = 0; x < maskImg.width(); x++ )
							if ( mask[( y / cell ) * gw + ( x / cell )] )
								row[x] = qRgb( 40, 40, 40 );
					}
					for ( const QPoint & p : diffAt )
						if ( p.x() < maskImg.width() && p.y() < maskImg.height() )
							reinterpret_cast<QRgb *>( maskImg.scanLine( p.y() ) )[p.x()]
								= qRgb( 255, 157, 0 );
					maskImg.save( base + QStringLiteral( "mask.png" ) );
					log << "images written under " << st->shotDir << "\n";
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
				}
			} while ( false );

			log << st->checks << " checks, " << st->fails << " failures\n";
			log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
			st->out = nullptr;
			delete st;
			log.flush();
			logf.close();
			if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
				n->undoStack->setClean();
			skope->setWindowModified( false );
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	} );
}
