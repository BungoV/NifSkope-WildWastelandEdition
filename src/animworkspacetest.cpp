/* WW_ANIMWS_TEST: the pre-registered gates of lane HKXEDIT2 -- the animation
   workspace that replaces the Animation Manager, run inside the real
   application (ww-test-harness-add). Its own translation unit; the footprint
   in nifskope_ui.cpp is one line (the hook-up).

   Everything is read off the WIDGETS by object name (AnimWsClipList,
   AnimWsDopeSheet, AnimWsInsertKey, ...) and off the workspace's public
   readers; the pose off Node::localTrans() in the scene, never off the
   playback's record of what it wrote.

   THE GATES (tests/spells/animws.sh has the reasons)
     (a) the list shows the clip; 78 bone rows with 93 keys each; ruler 60 fps.
         FLOOR: a name never loaded has no row; the unbound group holds 17.
     (b) Insert key on LLeg_Thigh at frame 46 with exactly 30 deg about X
         written to its NIF block (what the gizmo leaves): the document's
         frame 46 reads back within 0.01 deg, the viewport NODE's local
         rotation at frame 46 equals it within 0.01 deg, every other bone and
         frame byte-identical. FLOOR: frame 46 differs from the original.
     (c) delete that key: the sheet's selection -> Delete -> frame 46 is the
         45/47 interpolation (its deviation printed); Undo -> the pre-edit
         decode exactly.
     (d) Add annotation "FootLeft" at frame 30 through the Name row and the
         button; Save as; reload through the loader -> present at frame 30.
     (e) Trim 10..50 -> 41 frames, frame 0 == old frame 10; Retime 60->30 ->
         47 frames, coincident frames bit-identical.
     (f) Bake root on COM: track travel 0, motion ~487; Unbake -> byte-identical.
     (o) The Animations list (ruling 5): duplicate / copy / paste / cut /
         delete / rename / select all / move up / down driven by their own
         shortcuts and by the right-click menu, plus a drop, on three
         clips; row order, count and names read back from the list, the
         clips and the scene's animations list after every one.
     (p) The button row and the Keys block (rulings 6 and 6a): the bottom
         bar is gone and no push button is left in the dock, all sixteen of
         its actions are found in the header menus by TEXT, the right-side
         panel shows one section per selection kind and none of the others,
         it closes and reopens at the width it was dragged to, and the N
         key toggles it.
     (q) The transport icons (ruling 4): every transport button carries a
         drawn icon, none is a text glyph any more, they are all ONE size,
         every tooltip is asserted against the exact sentence it should
         carry with its shortcut in it, the six new keys are asserted and
         two of them fired, the speed reads "1.50x", the "Load... / root"
         header row has nothing clipped (proved able to fail by squeezing
         the column first), and the bar is saved at 1:1 and 2:1.
     (n) Remove transform axes (ruling 3): the entry sits directly under
         Remove track; stripping COM's translation X and Y holds both at
         frame 0's value over all 93 keys, leaves Z and every rotation
         byte-identical, is one undo step, saves and reads back bit for
         bit, and one Undo restores every byte.
     (g) the file saved in (d)/(b) re-read by our reader equals the document.
     (h) Undo / Redo through wwAnimUndoGroup() across insert, delete,
         annotation, trim: after undoing all four the frames equal the
         original decode; redo all four restores the edited state.
     (i) a NIF with a NiControllerSequence: its row appears, selecting it
         drives the scene (Scene::animGroup), the Start/Stop rows write the
         block through the NIF's undo stack and Undo puts it back.
     (j) panel-style counts with floors: every number field scrub-stamped
         (>= 6), no QGroupBox, every selector matched (>= 3), every button
         tipped, the note and the action bar outside the splitter; the wheel
         over the unfocused Speed field leaves it.
     pictures: the dock at frame 46 with the clip loaded, and the viewport with
         the posed bone selected (both headless, invisible, never primary).
     (k) THE DRAWING, read back from the pixels the sheet painted -- lane
         UINOTES1, 2026-09-12, bungo rulings 2 / 7b / 8: the active selected
         key orange, the other selected keys orange-red, an unselected key
         plain, the same three states on the annotations, a blue playhead box
         and line, and the area outside the clip range darkened while inside it
         is untouched. Twice over, at two zoom levels, with the colour pair
         that WAS here shown failing the same contrast test.
     (l) THE PLAY RANGE -- ruling 9: the ruler's grips dragged with real mouse
         events, the transport boxes typed into, four numbers asserted every
         time, the darkened edge read from the pixels, and a save whose .hkx
         is 61 frames long while the open clip keeps all 93. FLOOR: the same
         save with nothing out of range writes 93.
     (m) RIGHT-CLICK ANYWHERE and THE ZOOM -- rulings 7 / 7a: the menu read
         out of the live QMenu at three x positions, each naming the clicked
         frame; Rename / Remove on an annotation; the inline editor's Return
         and Escape; M / Ctrl+M; and a marker dragged 5 frames while zoomed to
         20..60 with the window unchanged. FLOOR: frameAll() fails that check.

   ENVIRONMENT (tests/spells/animws.sh sets them):
     WW_ANIMWS_TEST=1, WW_ANIMWS_CLIP, WW_ANIMWS_SKEL, WW_ANIMWS_JOG,
     WW_ANIMWS_SEQNIF, WW_ANIMWS_OUT. Log: release/ww_animws_test.log.

   Lane HKXEDIT2, 2026-09-10. */

#include "animworkspace.h"
#include "wwskin.h"
#include "hkxanimui.h"
#include "hkxplayback.h"
#include "hkxanim.h"
#include "hkxwrite.h"

#include "nifskope.h"
#include "glview.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QInputDialog>
#include <QMouseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLayout>
#include <QMenu>
#include <QAction>
#include <QMimeData>
#include <QShortcut>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>
#include <QWheelEvent>

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{

struct WwAwState
{
	int checks = 0, fails = 0, skips = 0;
	QTextStream * out = nullptr;
	QString clip, skel, jog, seqNif, outDir;
};

void check( WwAwState & st, const QString & what, bool ok )
{
	st.checks++;
	if ( !ok )
		st.fails++;
	*st.out << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
	st.out->flush();
}

void say( WwAwState & st, const QString & s )
{
	*st.out << s << "\n";
	st.out->flush();
}

void skip( WwAwState & st, const QString & why )
{
	st.skips++;
	*st.out << "  SKIP " << why << "\n";
	st.out->flush();
}

template <typename T>
T * widget( QWidget * root, const char * name )
{
	return root->findChild<T *>( QString::fromLatin1( name ) );
}

float len3( const Vector3 & v )
{
	return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

//! the scene's Node for a block number, or null
Node * nodeOf( Scene * sc, int block )
{
	for ( Node * n : sc->getNodes() )
		if ( n && n->id() == block )
			return n;
	return nullptr;
}

} // namespace

void wwAnimWorkspaceHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_ANIMWS_TEST" ) )
		return;
	auto * st = new WwAwState;
	st->clip = qEnvironmentVariable( "WW_ANIMWS_CLIP" );
	st->skel = qEnvironmentVariable( "WW_ANIMWS_SKEL" );
	st->jog = qEnvironmentVariable( "WW_ANIMWS_JOG" );
	st->seqNif = qEnvironmentVariable( "WW_ANIMWS_SEQNIF" );
	st->outDir = qEnvironmentVariable( "WW_ANIMWS_OUT" );

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		static bool ran = false;
		if ( ran )
			return;
		ran = true;
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_animws_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			st->out = &log;
			auto finish = [&]() {
				log << st->checks << " checks, " << st->fails << " failures, " << st->skips << " skips\n";
				log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\n";
				log << "done\n";
				log.flush();
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 100, qApp, &QApplication::quit );
			};

			check( *st, "the fixture NIF loaded", ok );
			GLView * ogl = skope->getGLView();
			NifModel * nif = skope->getNifModel();
			auto * ws = skope->findChild<AnimWorkspace *>( QStringLiteral( "AnimWorkspace" ) );
			check( *st, "the animation workspace exists (hook-up applied)", ws != nullptr );
			if ( !ws || !ogl || !nif ) {
				finish();
				return;
			}
			// a dock defers its work while hidden
			if ( auto * dock = qobject_cast<QDockWidget *>( ws->parentWidget() ) )
				dock->show();
			ws->show();
			qApp->processEvents();
			ws->refresh();
			qApp->processEvents();

			auto * list = widget<QListWidget>( ws, "AnimWsClipList" );
			AnimDopeSheet * sheet = ws->dopeSheet();
			check( *st, "the list and the dope sheet are there", list && sheet );
			if ( !list || !sheet ) {
				finish();
				return;
			}
			Scene * sc = ogl->getScene();
			HkxPlayback * pb = sc ? sc->hkx : nullptr;
			check( *st, "the scene has a playback", pb != nullptr );

			// ---- (a) load the skeleton (names) and the clip through the ONE loader
			if ( !st->skel.isEmpty() && QFile::exists( st->skel ) )
				WwHkxAnimHub::instance()->loadFiles( ogl, { st->skel }, false );
			else
				skip( *st, "no skeleton.hkx given: names come from whatever the loader finds" );
			bool refusal = false;
			const QString sentence = WwHkxAnimHub::instance()->loadFiles( ogl, { st->clip }, true, &refusal );
			say( *st, "loader: " + sentence );
			check( *st, "(a) the clip loaded and bound", !refusal );

			/* ---- (k) THE BINDING LINE IS A FEW WORDS (lane UI6, 2026-09-11)
			 *
			 * bungo, over a screenshot of the old dock, verbatim: "look at all
			 * this text clutter" -- the bone-binding report named every bone
			 * that did not bind, in a pinned label. It is now "78 of 95 bones",
			 * with the sentence one hover away.
			 *
			 * READ HERE, not in (j): the loader writes it through the hub's
			 * sentenceChanged the moment the clip binds, and the next refresh()
			 * replaces it with the clip's own brief. This is the only instant
			 * at which the label IS the binding report. */
			{
				const QString shown = ws->noteText();
				const QString tip = ws->noteDetail();
				say( *st, QStringLiteral( "  (k) the label says \"%1\" (%2 chars); its tooltip "
					"carries %3 chars" ).arg( shown ).arg( shown.size() ).arg( tip.size() ) );
				// how many separate NUMBERS the label carries -- "78 of 95 bones" is two
				int digits = 0;
				bool inNumber = false;
				for ( QChar c : shown ) {
					if ( c.isDigit() ) {
						if ( !inNumber )
							digits++;
						inNumber = true;
					} else {
						inNumber = false;
					}
				}
				check( *st, QStringLiteral( "(k) the binding line the dock SHOWS is a few words: "
					"%1 chars (<= 40) carrying %2 numbers (>= 2)" ).arg( shown.size() ).arg( digits ),
					!shown.isEmpty() && shown.size() <= 40 && digits >= 2 );
				check( *st, QStringLiteral( "(k) ...and the whole sentence is still there, in the "
					"tooltip (%1 chars), naming the bones that did not bind" ).arg( tip.size() ),
					tip.size() > 40 && tip.contains( QStringLiteral( "no node in this NIF" ) ) );
				check( *st, QStringLiteral( "(k floor) the two are not the same string, so the "
					"label really was folded and not merely renamed" ),
					!tip.isEmpty() && tip != shown && tip.size() > shown.size() );
			}
			qApp->processEvents();
			ws->refresh();
			qApp->processEvents();
			const QString entry = QFileInfo( st->clip ).completeBaseName();
			int rowIdx = -1;
			for ( int i = 0; i < list->count(); i++ )
				if ( list->item( i )->data( Qt::UserRole ).toString() == entry )
					rowIdx = i;
			check( *st, QStringLiteral( "(a) the list shows '%1' (row %2 of %3)" ).arg( entry ).arg( rowIdx ).arg( list->count() ), rowIdx >= 0 );
			int ghost = 0;
			for ( int i = 0; i < list->count(); i++ )
				if ( list->item( i )->data( Qt::UserRole ).toString() == QStringLiteral( "NeverLoadedClip" ) )
					ghost++;
			check( *st, "(a floor) a name never loaded has no row", ghost == 0 );
			if ( rowIdx >= 0 )
				list->setCurrentRow( rowIdx );
			qApp->processEvents();
			check( *st, QStringLiteral( "(a) selecting it makes it the scene's sequence: '%1'" ).arg( sc ? sc->animGroup : QString() ), sc && sc->animGroup == entry );
			check( *st, QStringLiteral( "(a) 78 bone rows: %1" ).arg( ws->boneRowCount() ), ws->boneRowCount() == 78 );
			check( *st, QStringLiteral( "(a floor) 17 tracks with no node, under the folded group: %1" ).arg( ws->unboundRowCount() ), ws->unboundRowCount() == 17 );
			{
				int off = 0;
				for ( int r = 0; r < sheet->rows().count(); r++ )
					if ( sheet->rows().at( r ).kind == AnimWsRow::Bone && sheet->keyCountOnRow( r ) != 93 )
						off++;
				check( *st, QStringLiteral( "(a) 93 keys on every bone row: %1 rows off" ).arg( off ), off == 0 );
			}
			check( *st, QStringLiteral( "(a) the ruler runs at 60 fps: %1" ).arg( sheet->rulerFps() ), std::fabs( sheet->rulerFps() - 60.0f ) < 0.01f );
			check( *st, QStringLiteral( "(a) the rate row says 60: '%1'" ).arg( widget<QLabel>( ws, "AnimWsRate" ) ? widget<QLabel>( ws, "AnimWsRate" )->text() : QString() ),
				widget<QLabel>( ws, "AnimWsRate" ) && widget<QLabel>( ws, "AnimWsRate" )->text().startsWith( QStringLiteral( "60" ) ) );

			const HkxClipDocument * doc = ws->document();
			check( *st, "(a) the workspace holds the clip's document", doc != nullptr );
			if ( !doc ) {
				finish();
				return;
			}
			const HkxAnimClip orig = doc->clip;

			// drive the scene to frame 46 (the clip's own rate)
			const float t46 = 46.0f * orig.frameDuration;
			ogl->setSceneTime( t46 );
			qApp->processEvents();
			check( *st, QStringLiteral( "(a) the readout says frame 46: '%1'" ).arg( widget<QLabel>( ws, "AnimWsReadout" )->text() ),
				widget<QLabel>( ws, "AnimWsReadout" )->text().contains( QStringLiteral( "frame 46 / 92" ) ) );
			check( *st, QStringLiteral( "(a) the sheet's playhead is at 46: %1" ).arg( sheet->currentFrame() ), sheet->currentFrame() == 46 );

			// ---- (b) insert a key at 46 on LLeg_Thigh with exactly 30 deg about X
			const int thigh = doc->findTrack( QStringLiteral( "LLeg_Thigh" ) );
			const int thighRow = sheet->rowOfTrack( thigh );
			check( *st, QStringLiteral( "(b) LLeg_Thigh is track %1, row %2" ).arg( thigh ).arg( thighRow ), thigh >= 0 && thighRow >= 0 );
			sheet->selectRow( thighRow );
			qApp->processEvents();
			const int thighBlock = ws->selectedNodeBlock();
			check( *st, QStringLiteral( "(b) selecting the row selected the bone in the viewport: block %1, scene index block %2" ).arg( thighBlock ).arg( nif->getBlockNumber( sc->currentIndex ) ),
				thighBlock >= 0 && nif->getBlockNumber( sc->currentIndex ) == thighBlock );
			// viewport -> sheet: select another node and see the row follow
			{
				const int comTrack = doc->findTrack( QStringLiteral( "COM" ) );
				const int comRow = sheet->rowOfTrack( comTrack );
				const int comBlock = comRow >= 0 ? sheet->rows().at( comRow ).nodeBlock : -1;
				if ( comBlock >= 0 ) {
					skope->select( nif->getBlockIndex( comBlock ) );
					qApp->processEvents();
					check( *st, QStringLiteral( "(b) selecting COM's node in the window selects its row: row %1 (COM row %2)" ).arg( sheet->currentRow() ).arg( comRow ), sheet->currentRow() == comRow );
				} else {
					skip( *st, "COM has no node in this NIF; the viewport->sheet half not measured" );
				}
				sheet->selectRow( thighRow );
				qApp->processEvents();
			}
			// the gizmo's result on the block: write exactly 30 deg about X
			const float half = 15.0f * float( M_PI ) / 180.0f;
			const Quat q30( std::cos( half ), std::sin( half ), 0.0f, 0.0f );
			{
				Matrix m;
				m.fromQuat( q30 );
				const QModelIndex iNode = nif->getBlockIndex( thighBlock );
				const Vector3 keepT = nif->get<Vector3>( iNode, "Translation" );
				nif->set<Matrix>( iNode, "Rotation", m );
				nif->set<Vector3>( iNode, "Translation", keepT );
			}
			/* RULING 6: Insert key is a header-menu ACTION now, not a button
			   at the bottom -- same object name, so this asks the same
			   question of the same thing. */
			auto * actInsert = ws->findChild<QAction *>( QStringLiteral( "AnimWsInsertKey" ) );
			check( *st, "(b) the Insert key action is enabled with a bone row selected", actInsert && actInsert->isEnabled() );
			ws->insertKeyAtPlayhead();
			qApp->processEvents();
			doc = ws->document();
			const float ang46 = HkxClipDocument::angleDeg( doc->clip.frames.at( 46 ).at( thigh ).rotation, q30 );
			check( *st, QStringLiteral( "(b) the document's frame 46 reads back 30 deg about X within 0.01: %1 deg off" ).arg( ang46 ), ang46 <= 0.01f );
			check( *st, "(b floor) frame 46 differs from the original decode", !HkxClipDocument::transformsEqual( doc->clip.frames.at( 46 ).at( thigh ), orig.frames.at( 46 ).at( thigh ) ) );
			{
				QString where;
				bool same = true;
				for ( int f = 0; f < orig.frames.count() && same; f++ )
					for ( int t = 0; t < orig.frames.at( f ).count(); t++ ) {
						if ( t == thigh ) continue;
						if ( !HkxClipDocument::transformsEqual( doc->clip.frames.at( f ).at( t ), orig.frames.at( f ).at( t ) ) ) {
							same = false;
							where = QStringLiteral( "frame %1 track %2" ).arg( f ).arg( t );
							break;
						}
					}
				check( *st, QStringLiteral( "(b) every other bone and frame byte-identical %1" ).arg( where ), same );
			}
			// the viewport's NODE at frame 46 carries the keyed rotation
			ogl->setSceneTime( t46 );
			qApp->processEvents();
			ogl->update();
			qApp->processEvents();
			if ( Node * n = nodeOf( sc, thighBlock ) ) {
				const Quat nq = n->localTrans().rotation.toQuat();
				const float angNode = HkxClipDocument::angleDeg( nq, q30 );
				check( *st, QStringLiteral( "(b) the viewport node's local rotation at frame 46 is the keyed one: %1 deg off" ).arg( angNode ), angNode <= 0.01f );
				ogl->setSceneTime( 0.0f );
				qApp->processEvents();
				ogl->update();
				qApp->processEvents();
				const float angNode0 = HkxClipDocument::angleDeg( n->localTrans().rotation.toQuat(), q30 );
				check( *st, QStringLiteral( "(b floor) at frame 0 the node is NOT the keyed pose: %1 deg off" ).arg( angNode0 ), angNode0 > 0.5f );
				ogl->setSceneTime( t46 );
				qApp->processEvents();
			} else {
				skip( *st, "no scene node for the thigh block; the viewport half of (b) not measured" );
			}
			// the dock grab at frame 46
			if ( !st->outDir.isEmpty() ) {
				QDir().mkpath( st->outDir );
				const QPixmap pm = ws->grab();
				const QString shot = st->outDir + QStringLiteral( "/dock_frame46.png" );
				check( *st, QStringLiteral( "picture: the dock at frame 46, %1x%2 -> %3" ).arg( pm.width() ).arg( pm.height() ).arg( shot ), pm.width() > 400 && pm.height() > 80 && pm.save( shot ) );
				ogl->update();
				qApp->processEvents();
				const QPixmap vp = QPixmap::fromImage( ogl->grabFramebuffer() );
				const QString shot2 = st->outDir + QStringLiteral( "/viewport_gizmo.png" );
				check( *st, QStringLiteral( "picture: the viewport with the posed bone selected, %1x%2" ).arg( vp.width() ).arg( vp.height() ), vp.width() > 100 && vp.save( shot2 ) );
			}

			// ---- (c) delete that key, then Undo
			sheet->selectKeys( { HkxKeyRef{ thigh, 46 } } );
			ws->deleteSelected();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(c) the key at 46 is gone: %1 keys on the track" ).arg( doc->keyCount( thigh ) ), doc->keyCount( thigh ) == 92 );
			{
				const float dev = HkxClipDocument::angleDeg( doc->clip.frames.at( 46 ).at( thigh ).rotation, orig.frames.at( 46 ).at( thigh ).rotation );
				say( *st, QStringLiteral( "     (c) after the delete frame 46 is the 45/47 interpolation, %1 deg from the original decode (pre-registered: not the pre-edit bytes)" ).arg( dev ) );
			}
			QUndoGroup * grp = wwAnimUndoGroup();
			check( *st, "(h) the workspace's stack is in the shared undo group", grp->stacks().contains( ws->undoStack() ) );
			grp->setActiveStack( ws->undoStack() );
			grp->undo();		// the delete
			qApp->processEvents();
			grp->undo();		// the insert
			qApp->processEvents();
			doc = ws->document();
			{
				int f = -1, t = -1;
				check( *st, QStringLiteral( "(c) Undo x2 -> the pre-edit decode exactly (first diff frame %1 track %2)" ).arg( f ).arg( t ), HkxClipDocument::framesEqual( doc->clip, orig, &f, &t ) );
				check( *st, QStringLiteral( "(c) ... and 93 keys on the track again: %1" ).arg( doc->keyCount( thigh ) ), doc->keyCount( thigh ) == 93 );
			}
			grp->redo();
			grp->redo();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(h) Redo x2 -> the deleted state again: %1 keys" ).arg( doc->keyCount( thigh ) ), doc->keyCount( thigh ) == 92 );
			grp->undo();		// leave the inserted key in place for (g)
			qApp->processEvents();
			doc = ws->document();
			check( *st, "(h) one more Undo -> the inserted key is back", HkxClipDocument::angleDeg( doc->clip.frames.at( 46 ).at( thigh ).rotation, q30 ) <= 0.01f && doc->keyCount( thigh ) == 93 );

			// ---- (d) annotation through the rows
			auto * annot = widget<QComboBox>( ws, "AnimWsAnnotName" );
			check( *st, QStringLiteral( "(d) the Name row offers the graph vocabulary: %1 names, first '%2'" ).arg( annot ? annot->count() : 0 ).arg( annot ? annot->itemText( 0 ) : QString() ),
				annot && annot->count() >= 50 && ws->vocabulary().contains( QStringLiteral( "FootLeft" ) ) );
			ogl->setSceneTime( 30.0f * orig.frameDuration );
			qApp->processEvents();
			const int annBefore = doc->annotationCount();
			if ( annot )
				annot->setEditText( QStringLiteral( "FootLeft" ) );
			ws->addAnnotationAtPlayhead();
			qApp->processEvents();
			doc = ws->document();
			bool at30 = false;
			for ( const HkxAnnotation & a : doc->clip.annotations.value( 0 ) )
				if ( a.text == QStringLiteral( "FootLeft" ) && doc->frameOfTime( a.time ) == 30 )
					at30 = true;
			check( *st, QStringLiteral( "(d) 'FootLeft' at frame 30 in the document (%1 -> %2 annotations)" ).arg( annBefore ).arg( doc->annotationCount() ), at30 && doc->annotationCount() == annBefore + 1 );
			check( *st, QStringLiteral( "(d) the marker row counts it: %1" ).arg( sheet->keyCountOnRow( 0 ) ), sheet->keyCountOnRow( 0 ) == annBefore + 1 );
			// the undo stack: undo the annotation, redo it
			grp->undo();
			qApp->processEvents();
			check( *st, QStringLiteral( "(h) Undo the annotation: %1" ).arg( ws->document()->annotationCount() ), ws->document()->annotationCount() == annBefore );
			grp->redo();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(h) Redo the annotation: %1" ).arg( doc->annotationCount() ), doc->annotationCount() == annBefore + 1 );

			/* ---- (k) THE DRAWING: bungo's rulings 2, 7b and 8 of 2026-09-12
			 *
			 * Verbatim, ruling 2: "When clicking on the keyframes, they go
			 * invisible, instead of appearing like in Blender, so as orange."
			 * 7b: "Selected annotation should also be orange, and orange
			 * redish for secondary annotation selection". 8: "Timeline marker
			 * where you're at in the played animation should also be blue." and
			 * "Areas inside of an animation should be as they are, outside like
			 * in Blender, darkened".
			 *
			 * A colour is not measurable from a widget's state, so this gate
			 * reads the PIXELS the sheet actually painted, at the coordinates
			 * the painter itself computes (frameToX, rowCenterY), and compares
			 * them to the named skin tokens. Every sample is taken twice, once
			 * with the whole clip in view and once zoomed in, because the second
			 * pass is the one that catches a colour surviving at one scale only.
			 *
			 * THE FLOORS: the pair of colours that WAS here -- "toggle" over
			 * "selBgActive" -- goes through the same contrast test and must come
			 * back red, and the darkening test is re-run with the range over the
			 * whole clip, where it must find nothing darkened at all. */
			{
				auto chanDist = []( const QColor & a, const QColor & b ) {
					return std::abs( a.red() - b.red() ) + std::abs( a.green() - b.green() ) + std::abs( a.blue() - b.blue() );
				};
				auto px = []( const QImage & im, int x, int y ) {
					const double d = im.devicePixelRatio() > 0.0 ? im.devicePixelRatio() : 1.0;
					const int sx = std::min( im.width() - 1, std::max( 0, int( x * d ) ) );
					const int sy = std::min( im.height() - 1, std::max( 0, int( y * d ) ) );
					return im.pixelColor( sx, sy );
				};
				const QColor cKey( wwSkinColor( "animKey" ) ), cAct( wwSkinColor( "animKeySel" ) );
				const QColor cOth( wwSkinColor( "animKeySelOther" ) ), cPh( wwSkinColor( "animPlayhead" ) );
				const QColor cRowSel( wwSkinColor( "selBgActive" ) );

				sheet->selectRow( thighRow );
				sheet->setCurrentFrame( 46 );
				qApp->processEvents();
				// bring the row into the sheet's own viewport if it is scrolled away
				if ( sheet->rowCenterY( thighRow ) < 0 ) {
					if ( auto * sb = sheet->findChild<QScrollBar *>( QStringLiteral( "AnimWsDopeSheetScroll" ) ) )
						sb->setValue( std::max( 0, int( sheet->visibleRows().indexOf( thighRow ) ) ) );
					qApp->processEvents();
				}
				const int cy = sheet->rowCenterY( thighRow );
				check( *st, QStringLiteral( "(k) the thigh row is on screen to be sampled: centre y %1, sheet %2x%3" ).arg( cy ).arg( sheet->width() ).arg( sheet->height() ),
					   cy > 0 && sheet->width() > 300 && sheet->height() > 80 );
				if ( cy > 0 ) {
					// key 20 ACTIVE, key 30 also selected, key 40 untouched
					sheet->selectKeys( { HkxKeyRef{ thigh, 20 }, HkxKeyRef{ thigh, 30 } }, HkxKeyRef{ thigh, 20 } );
					qApp->processEvents();
					check( *st, QStringLiteral( "(k) the sheet's active key is 20 of 2 selected: track %1 frame %2" ).arg( sheet->activeKey().track ).arg( sheet->activeKey().frame ),
						   sheet->activeKey() == HkxKeyRef{ thigh, 20 } && sheet->selectedKeys().count() == 2 );

					/* phF is the frame the playhead is parked on FOR THIS PASS.
					 * It has to be inside the pass's own view or there is no blue
					 * on screen to sample at all, and it has to miss the keys at
					 * 20 / 30 / 40 and the two background samples, because the
					 * playhead is a 2 px line drawn over the lot. */
					struct Pass { const char * name; float v0, v1; int r0, r1, outF, inF, phF; };
					const Pass passes[] = {
						{ "the whole 93 frames in view", -0.5f, 92.5f, 10, 50, 5, 40, 46 },
						{ "zoomed to frames 15..45", 15.0f, 45.0f, 25, 40, 18, 35, 23 },
					};
					for ( const Pass & ps : passes ) {
						sheet->setView( ps.v0, ps.v1 );
						sheet->setRange( ps.r0, ps.r1 );
						sheet->setCurrentFrame( ps.phF );
						qApp->processEvents();
						const QImage im = sheet->grab().toImage();
						const int x20 = int( sheet->frameToX( 20.0f ) ), x30 = int( sheet->frameToX( 30.0f ) );
						const int x40 = int( sheet->frameToX( 40.0f ) ), xPh = int( sheet->frameToX( float( ps.phF ) ) );
						const QColor gotAct = px( im, x20, cy );
						const QColor gotOth = px( im, x30, cy );
						const QColor gotKey = px( im, x40, cy );
						/* RULING 08:2x: a key outside the range is painted in the dimmed
						   form of its colour, so what a sample must equal depends on where
						   the pass put the range. Pass 2's range is 25..40, which leaves the
						   ACTIVE key at frame 20 outside it -- that is the case this
						   expectation exists for, and it is the painter's own function that
						   says what the colour should be. */
						auto outOf = [&]( int frame ) { return frame < ps.r0 || frame > ps.r1; };
						auto want = [&]( int frame, const QColor & base ) {
							return outOf( frame ) ? AnimDopeSheet::dimOutOfRange( base ) : base;
						};
						auto how = [&]( int frame, const char * token ) {
							return outOf( frame ) ? QStringLiteral( "%1 GREYED (frame %2 is out of range)" ).arg( QString::fromLatin1( token ) ).arg( frame )
												 : QString::fromLatin1( token );
						};
						const QColor wAct = want( 20, cAct ), wOth = want( 30, cOth ), wKey = want( 40, cKey );
						const QColor bgIn = px( im, int( sheet->frameToX( float( ps.inF ) ) ), cy - 8 );
						const QColor bgOut = px( im, int( sheet->frameToX( float( ps.outF ) ) ), cy - 8 );
						const QColor rulerBox = px( im, xPh, 6 );
						const QColor phLine = px( im, xPh, sheet->rulerHeight() + 2 );
						const QString pn = QString::fromLatin1( ps.name );
						say( *st, QStringLiteral( "  (k) %1: active %2, other %3, plain %4 | in-range bg %5, out-of-range bg %6 | ruler box %7, line %8 (playhead parked on frame %9)" )
								 .arg( pn, gotAct.name(), gotOth.name(), gotKey.name(), bgIn.name(), bgOut.name(), rulerBox.name(), phLine.name() ).arg( ps.phF ) );
						check( *st, QStringLiteral( "(k2) %1: the ACTIVE selected key is %2 %3, got %4" ).arg( pn, how( 20, "animKeySel" ), wAct.name(), gotAct.name() ),
							   gotAct == wAct );
						check( *st, QStringLiteral( "(k7b) %1: the OTHER selected key is %2 %3, got %4" ).arg( pn, how( 30, "animKeySelOther" ), wOth.name(), gotOth.name() ),
							   gotOth == wOth );
						check( *st, QStringLiteral( "(k2) %1: an unselected key is %2 %3, got %4" ).arg( pn, how( 40, "animKey" ), wKey.name(), gotKey.name() ),
							   gotKey == wKey );
						check( *st, QStringLiteral( "(k2) %1: the three states are three DIFFERENT colours" ).arg( pn ),
							   gotAct != gotOth && gotOth != gotKey && gotAct != gotKey );
						// the defect itself: a selected key must stand off the row it sits on
						const int dSel = chanDist( gotAct, cRowSel );
						check( *st, QStringLiteral( "(k2) %1: the selected key stands off the selected row under it: %2 apart in R+G+B (>= 60)" ).arg( pn ).arg( dSel ),
							   dSel >= 60 );
						check( *st, QStringLiteral( "(k8) %1: the ruler carries a BLUE playhead box, animPlayhead %2, got %3" ).arg( pn, cPh.name(), rulerBox.name() ),
							   rulerBox == cPh );
						check( *st, QStringLiteral( "(k8) %1: the playhead LINE below the ruler is the same blue: %2" ).arg( pn, phLine.name() ),
							   phLine == cPh );
						check( *st, QStringLiteral( "(k8) %1: INSIDE the range %2..%3 the row is untouched, %4 == selBgActive %5" ).arg( pn ).arg( ps.r0 ).arg( ps.r1 ).arg( bgIn.name(), cRowSel.name() ),
							   bgIn == cRowSel );
						check( *st, QStringLiteral( "(k8) %1: OUTSIDE the range it is darkened: %2 vs %3, %4 apart and darker" ).arg( pn, bgOut.name(), bgIn.name() ).arg( chanDist( bgOut, bgIn ) ),
							   chanDist( bgOut, bgIn ) >= 20 && bgOut.lightness() < bgIn.lightness() );
					}

					/* FLOOR 1: the colours that were here. The old painter filled
					 * a selected diamond with "toggle" and the current row with
					 * "selBgActive" -- put those two through the very same
					 * contrast test and it must call them indistinguishable. */
					{
						const QColor oldSel( wwSkinColor( "toggle" ) );
						const int dOld = chanDist( oldSel, cRowSel );
						say( *st, QStringLiteral( "  (k floor) the colours as they were: selected key %1 over current row %2, %3 apart in R+G+B" ).arg( oldSel.name(), cRowSel.name() ).arg( dOld ) );
						check( *st, QStringLiteral( "(k floor) the SAME contrast test calls the old pair indistinguishable: %1 < 60" ).arg( dOld ), dOld < 60 );
					}
					/* FLOOR 2: with the range over the whole clip nothing may be
					 * darkened -- the two samples that just differed must now be
					 * equal, so the darkening check is shown able to go red. */
					{
						sheet->frameAll();
						sheet->setRange( 0, sheet->numFrames() - 1 );
						qApp->processEvents();
						const QImage im = sheet->grab().toImage();
						const QColor a = px( im, int( sheet->frameToX( 5.0f ) ), cy - 8 );
						const QColor bIn = px( im, int( sheet->frameToX( 40.0f ) ), cy - 8 );
						check( *st, QStringLiteral( "(k8 floor) range 0..%1: frame 5 and frame 40 are the SAME colour %2 / %3 -- nothing darkened" ).arg( sheet->numFrames() - 1 ).arg( a.name(), bIn.name() ),
							   a == bIn && a == cRowSel );
					}
					/* ---- (k8b) RULING 08:2x, bungo verbatim: "also, grey out diamond
					 * keyframes out of animations start and end range, to indicate
					 * they're not being taking into consideration anymore".
					 *
					 * The fixture keys every frame of 93, so frames 5, 30 and 80 are all
					 * real diamonds. With the range at 10..50, 5 and 80 are outside it and
					 * 30 is inside; every sample is compared to the EXACT colour the
					 * painter's own dimOutOfRange() gives, not to "darker than", so a wash
					 * of the wrong strength fails too. Then End is moved out to 90 and
					 * frame 80 must come back plain with no reload -- the "the moment the
					 * grips move" half of the ruling. Selection is checked in the same
					 * place: a selected key that falls out of the range keeps its own
					 * colour, dimmed the same way.
					 *
					 * THE FLOOR: with the range back over the whole clip the very same
					 * three samples must all be plain, so the comparison is shown able to
					 * go the other way on the same run. */
					{
						const QColor dimKey = AnimDopeSheet::dimOutOfRange( cKey );
						sheet->frameAll();
						sheet->clearSelection();
						sheet->setRange( 10, 50 );
						sheet->setCurrentFrame( 60 );   // the playhead is a 2 px line: park it off every sample
						qApp->processEvents();
						auto keyAt = [&]( float frame ) {
							const QImage im2 = sheet->grab().toImage();
							return px( im2, int( sheet->frameToX( frame ) ), cy );
						};
						const QColor before5 = keyAt( 5.0f ), in30 = keyAt( 30.0f ), before80 = keyAt( 80.0f );
						say( *st, QStringLiteral( "  (k8b) range 10..50: frame 5 %1, frame 30 %2, frame 80 %3 (plain animKey %4, greyed %5)" )
								 .arg( before5.name(), in30.name(), before80.name(), cKey.name(), dimKey.name() ) );
						check( *st, QStringLiteral( "(k8b) the key at frame 5, BEFORE the range, is greyed: %1 == %2" ).arg( before5.name(), dimKey.name() ),
							   before5 == dimKey );
						check( *st, QStringLiteral( "(k8b) the key at frame 80, AFTER the range, is greyed: %1 == %2" ).arg( before80.name(), dimKey.name() ),
							   before80 == dimKey );
						check( *st, QStringLiteral( "(k8b) the key at frame 30, INSIDE it, is untouched plain animKey: %1 == %2" ).arg( in30.name(), cKey.name() ),
							   in30 == cKey );
						check( *st, QStringLiteral( "(k8b) greyed is darker than plain, and %1 apart in R+G+B (>= 60)" ).arg( chanDist( dimKey, cKey ) ),
							   dimKey.lightness() < cKey.lightness() && chanDist( dimKey, cKey ) >= 60 );
						// the grips move -> the diamonds follow, with no reload
						sheet->setRange( 10, 90 );
						qApp->processEvents();
						const QColor after80 = keyAt( 80.0f ), after5 = keyAt( 5.0f );
						check( *st, QStringLiteral( "(k8b) End moved to 90: frame 80 is plain again, %1 -> %2" ).arg( before80.name(), after80.name() ),
							   after80 == cKey && before80 != after80 );
						check( *st, QStringLiteral( "(k8b) ... and frame 5, still before the start, is still greyed: %1" ).arg( after5.name() ),
							   after5 == dimKey );
						// a SELECTED key that has fallen out of the range keeps its colour, dimmed
						sheet->setRange( 10, 50 );
						sheet->selectKeys( { HkxKeyRef{ thigh, 5 }, HkxKeyRef{ thigh, 80 } }, HkxKeyRef{ thigh, 5 } );
						qApp->processEvents();
						const QColor selOut = keyAt( 5.0f ), othOut = keyAt( 80.0f );
						const QColor wantSel = AnimDopeSheet::dimOutOfRange( cAct ), wantOth = AnimDopeSheet::dimOutOfRange( cOth );
						say( *st, QStringLiteral( "  (k8b) selected AND out of range: active %1 (want %2), other %3 (want %4)" )
								 .arg( selOut.name(), wantSel.name(), othOut.name(), wantOth.name() ) );
						check( *st, QStringLiteral( "(k8b) the ACTIVE selected key keeps animKeySel, greyed the same way: %1 == %2" ).arg( selOut.name(), wantSel.name() ),
							   selOut == wantSel );
						check( *st, QStringLiteral( "(k8b) the other selected key keeps animKeySelOther, greyed the same way: %1 == %2" ).arg( othOut.name(), wantOth.name() ),
							   othOut == wantOth );
						check( *st, "(k8b) and greyed does not flatten the three states into one",
							   selOut != othOut && othOut != dimKey && selOut != dimKey );
						if ( !st->outDir.isEmpty() ) {
							const QPixmap pm = sheet->grab();
							const QString shot = st->outDir + QStringLiteral( "/sheet_range_dim.png" );
							check( *st, QStringLiteral( "picture: (k8b) the sheet at 1:1, range 10..50, greyed keys either side, %1x%2 -> %3" ).arg( pm.width() ).arg( pm.height() ).arg( shot ),
								   pm.save( shot ) );
						}
						// THE FLOOR
						sheet->clearSelection();
						sheet->setRange( 0, sheet->numFrames() - 1 );
						qApp->processEvents();
						const QColor all5 = keyAt( 5.0f ), all30 = keyAt( 30.0f ), all80 = keyAt( 80.0f );
						check( *st, QStringLiteral( "(k8b floor) range 0..%1: the SAME three samples are all plain animKey -- %2 / %3 / %4" ).arg( sheet->numFrames() - 1 ).arg( all5.name(), all30.name(), all80.name() ),
							   all5 == cKey && all30 == cKey && all80 == cKey );
					}
					/* (7b) the annotation's three states on the marker row. The
					 * fixture carries 'FootLeft' at frame 30 from gate (d); a
					 * second one is added at frame 60 so "active" and "also
					 * selected" can both be in one picture, and it is undone
					 * again so the document (g) saves is the one (d) left. */
					{
						ogl->setSceneTime( 60.0f * ws->document()->clip.frameDuration );
						qApp->processEvents();
						if ( auto * annot2 = widget<QComboBox>( ws, "AnimWsAnnotName" ) )
							annot2->setEditText( QStringLiteral( "FootRight" ) );
						ws->addAnnotationAtPlayhead();
						qApp->processEvents();
						const HkxClipDocument * d2 = ws->document();
						int i30 = -1, i60 = -1;
						for ( int i = 0; i < d2->clip.annotations.value( 0 ).count(); i++ ) {
							const int fr = d2->frameOfTime( d2->clip.annotations.value( 0 ).at( i ).time );
							if ( fr == 30 ) i30 = i;
							if ( fr == 60 ) i60 = i;
						}
						check( *st, QStringLiteral( "(k7b) two annotations to colour: index %1 at frame 30, index %2 at frame 60" ).arg( i30 ).arg( i60 ), i30 >= 0 && i60 >= 0 );
						if ( i30 >= 0 && i60 >= 0 ) {
							sheet->frameAll();
							sheet->setRange( 0, sheet->numFrames() - 1 );
							sheet->selectMarker( AnimWsMarkerRef{ 0, i30 }, false );
							sheet->selectMarker( AnimWsMarkerRef{ 0, i60 }, true );
							/* the playhead is still on frame 60, where the second annotation
							 * was just added, and it is a 2 px blue line drawn over
							 * everything: park it away from BOTH markers or the sample
							 * reads animPlayhead, not the triangle. */
							sheet->setCurrentFrame( 75 );
							qApp->processEvents();
							check( *st, QStringLiteral( "(k7b) frame 60's marker is the ACTIVE one of 2 selected: index %1 of %2" ).arg( sheet->selectedMarker().index ).arg( sheet->selectedMarkers().count() ),
								   sheet->selectedMarker() == AnimWsMarkerRef{ 0, i60 } && sheet->selectedMarkers().count() == 2 );
							const QImage im = sheet->grab().toImage();
							// the triangle sits at the bottom of the marker row
							const int ty = sheet->rulerHeight() + sheet->markerRowHeight() - 4;
							const QColor mAct = px( im, int( sheet->frameToX( 60.0f ) ), ty );
							const QColor mOth = px( im, int( sheet->frameToX( 30.0f ) ), ty );
							say( *st, QStringLiteral( "  (k7b) annotation at 60 (active) %1, at 30 (also selected) %2" ).arg( mAct.name(), mOth.name() ) );
							check( *st, QStringLiteral( "(k7b) the active annotation is orange animKeySel %1, got %2" ).arg( cAct.name(), mAct.name() ), mAct == cAct );
							check( *st, QStringLiteral( "(k7b) the other selected annotation is orange-red animKeySelOther %1, got %2" ).arg( cOth.name(), mOth.name() ), mOth == cOth );
							sheet->clearSelection();
							qApp->processEvents();
							const QImage im2 = sheet->grab().toImage();
							const QColor mPlain = px( im2, int( sheet->frameToX( 60.0f ) ), ty );
							check( *st, QStringLiteral( "(k7b) deselected, the same annotation is plain animKey %1, got %2" ).arg( cKey.name(), mPlain.name() ), mPlain == cKey );
						}
						grp->undo();   // take the second annotation back out
						qApp->processEvents();
						check( *st, QStringLiteral( "(k7b) the extra annotation is undone: %1 annotations" ).arg( ws->document()->annotationCount() ),
							   ws->document()->annotationCount() == annBefore + 1 );
					}
					// the picture for the report, at 1:1
					if ( !st->outDir.isEmpty() ) {
						sheet->selectKeys( { HkxKeyRef{ thigh, 20 }, HkxKeyRef{ thigh, 30 } }, HkxKeyRef{ thigh, 20 } );
						sheet->setRange( 10, 50 );
						qApp->processEvents();
						const QPixmap pm = sheet->grab();
						const QString shot = st->outDir + QStringLiteral( "/sheet_colours.png" );
						check( *st, QStringLiteral( "picture: the sheet's colours, %1x%2 -> %3" ).arg( pm.width() ).arg( pm.height() ).arg( shot ), pm.save( shot ) );
					}
					sheet->setRange( 0, sheet->numFrames() - 1 );
					sheet->clearSelection();
					sheet->frameAll();
					qApp->processEvents();
				}
				doc = ws->document();
			}

			/* ---- (l) THE PLAY RANGE: bungo's ruling 9 of 2026-09-12
			 *
			 * Verbatim: "Allow me to drag the starting and ending frame, add
			 * markers for them of some kind I can drag, and the animation
			 * should only play that range".
			 *
			 * Driven as a HAND would drive it: real QMouseEvents on the sheet's
			 * ruler, at the pixel the grip is painted on, +10 frames on the
			 * start grip and -10 on the end grip, then the boxes in the
			 * transport row typed into. Four numbers are asserted every time --
			 * the document's range, the sheet's, the boxes', and the frame count
			 * -- because any one of them alone can be right while the others
			 * drift apart. Then the darkened-zone edge is read out of the
			 * pixels, and the file is saved and read back: its duration is the
			 * range's, and the open document still has all 93 frames.
			 *
			 * THE FLOOR: the same save with the range over the whole clip must
			 * come back 93 frames, so the duration check is shown able to fail.
			 */
			{
				const int frames0 = ws->document()->numFrames();
				const int undo0 = ws->undoStack()->index();
				check( *st, QStringLiteral( "(l) the clip starts with its whole length in range: %1..%2 of %3 frames" ).arg( ws->document()->rangeFirstFrame() ).arg( ws->document()->rangeLastFrame() ).arg( frames0 ),
					   ws->document()->rangeFirstFrame() == 0 && ws->document()->rangeLastFrame() == frames0 - 1 && frames0 == 93 );
				check( *st, QStringLiteral( "(l) and nothing is darkened while that holds: rangeIsPartial %1" ).arg( ws->document()->rangeIsPartial() ),
					   !ws->document()->rangeIsPartial() );

				auto dragGrip = [&]( bool endGrip, int deltaFrames ) {
					sheet->frameAll();
					qApp->processEvents();
					const float fr = endGrip ? float( sheet->rangeEnd() ) + 0.5f : float( sheet->rangeStart() ) - 0.5f;
					const int x0 = int( sheet->frameToX( fr ) );
					const float pxPerFrame = sheet->frameToX( 1.0f ) - sheet->frameToX( 0.0f );
					const int x1 = x0 + int( std::lround( double( pxPerFrame ) * deltaFrames ) );
					const QPoint a( x0, 4 ), c( x1, 4 );
					QMouseEvent pr( QEvent::MouseButtonPress, QPointF( a ), QPointF( sheet->mapToGlobal( a ) ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
					QMouseEvent mv( QEvent::MouseMove, QPointF( c ), QPointF( sheet->mapToGlobal( c ) ), Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
					QMouseEvent rl( QEvent::MouseButtonRelease, QPointF( c ), QPointF( sheet->mapToGlobal( c ) ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( sheet, &pr );
					QApplication::sendEvent( sheet, &mv );
					QApplication::sendEvent( sheet, &rl );
					qApp->processEvents();
					return QStringLiteral( "%1 px per frame, %2 -> %3 px" ).arg( pxPerFrame ).arg( x0 ).arg( x1 );
				};
				auto rangeIs = [&]( const char * what, int a, int bb ) {
					const HkxClipDocument * d3 = ws->document();
					const int n = bb - a + 1;
					check( *st, QStringLiteral( "(l) %1: document %2..%3, sheet %4..%5, boxes %6..%7, %8 frames in range" )
							   .arg( QString::fromLatin1( what ) ).arg( d3->rangeFirstFrame() ).arg( d3->rangeLastFrame() )
							   .arg( sheet->rangeStart() ).arg( sheet->rangeEnd() )
							   .arg( widget<QSpinBox>( ws, "AnimWsRangeStart" )->value() ).arg( widget<QSpinBox>( ws, "AnimWsRangeEnd" )->value() )
							   .arg( d3->rangeLastFrame() - d3->rangeFirstFrame() + 1 ),
						   d3->rangeFirstFrame() == a && d3->rangeLastFrame() == bb
						   && sheet->rangeStart() == a && sheet->rangeEnd() == bb
						   && widget<QSpinBox>( ws, "AnimWsRangeStart" )->value() == a
						   && widget<QSpinBox>( ws, "AnimWsRangeEnd" )->value() == bb
						   && d3->rangeLastFrame() - d3->rangeFirstFrame() + 1 == n );
					check( *st, QStringLiteral( "(l) %1: the clip itself is untouched, still %2 frames" ).arg( QString::fromLatin1( what ) ).arg( d3->numFrames() ), d3->numFrames() == frames0 );
				};

				say( *st, QStringLiteral( "  (l) start grip +10: %1" ).arg( dragGrip( false, 10 ) ) );
				rangeIs( "after dragging the start grip +10 frames", 10, 92 );
				say( *st, QStringLiteral( "  (l) end grip -10: %1" ).arg( dragGrip( true, -10 ) ) );
				rangeIs( "after dragging the end grip -10 frames", 10, 82 );
				// the boxes in the transport row are the same two numbers
				widget<QSpinBox>( ws, "AnimWsRangeEnd" )->setValue( 70 );
				ws->setRangeFromBoxes();
				qApp->processEvents();
				rangeIs( "after typing End = 70", 10, 70 );
				{
					QStringList cmdTexts;
					for ( int i = 0; i < ws->undoStack()->count(); i++ )
						cmdTexts << QStringLiteral( "%1:%2" ).arg( i ).arg( ws->undoStack()->text( i ) );
					say( *st, QStringLiteral( "  (l) the stack after the three gestures: %1 applied of %2 kept -- %3" )
							 .arg( ws->undoStack()->index() ).arg( ws->undoStack()->count() ).arg( cmdTexts.join( QStringLiteral( " | " ) ) ) );
				}
				/* one undoable command per gesture, not one per pixel of the drag.
				 * COUNTED WITH index(), NOT count(): count() still holds commands an
				 * earlier gate undid and never redid, which the next push throws
				 * away, so count() can go DOWN across a push and cannot count
				 * anything. (k7b) leaves exactly one such command behind. */
				if ( !st->outDir.isEmpty() ) {
					/* RULING 9's picture: the ruler carrying both range grips, with
					   the out-of-range frames darkened on either side of them. */
					const QPixmap rp = sheet->grab();
					const QString rs = st->outDir + QStringLiteral( "/r9_range_grips.png" );
					check( *st, QStringLiteral( "picture: (9) the ruler with both grips, range %1..%2, %3x%4 -> %5" )
						   .arg( ws->document()->rangeFirstFrame() ).arg( ws->document()->rangeLastFrame() )
						   .arg( rp.width() ).arg( rp.height() ).arg( rs ),
						   rp.width() > 200 && rp.save( rs ) );
				}
				check( *st, QStringLiteral( "(l) three gestures put three commands on the stack: %1 -> %2" ).arg( undo0 ).arg( ws->undoStack()->index() ),
					   ws->undoStack()->index() == undo0 + 3 );

				// the darkened-zone EDGE, read out of the picture
				{
					const int cy2 = sheet->rowCenterY( sheet->currentRow() >= 0 ? sheet->currentRow() : 1 );
					if ( cy2 > 0 ) {
						const QImage im = sheet->grab().toImage();
						const double dp = im.devicePixelRatio() > 0.0 ? im.devicePixelRatio() : 1.0;
						auto at = [&]( float frame ) {
							const int sx = std::min( im.width() - 1, std::max( 0, int( sheet->frameToX( frame ) * dp ) ) );
							const int sy = std::min( im.height() - 1, std::max( 0, int( ( cy2 - 8 ) * dp ) ) );
							return im.pixelColor( sx, sy );
						};
						const QColor out5 = at( 5.0f ), in15 = at( 15.0f ), in65 = at( 65.0f ), out80 = at( 80.0f );
						say( *st, QStringLiteral( "  (l) range 10..70, the row at frames 5 / 15 / 65 / 80: %1 %2 %3 %4" ).arg( out5.name(), in15.name(), in65.name(), out80.name() ) );
						check( *st, QStringLiteral( "(l) the darkened zone starts and ends at the grips: 5 and 80 darkened, 15 and 65 not" ),
							   out5 != in15 && out80 != in65 && in15 == in65 && out5.lightness() < in15.lightness() && out80.lightness() < in65.lightness() );
					} else {
						skip( *st, "(l) no row on screen to read the darkened-zone edge from" );
					}
				}

				// the FILE carries the range: 61 frames, 60 frame-times long
				if ( !st->outDir.isEmpty() ) {
					const HkxClipDocument * d3 = ws->document();
					const QString rp = st->outDir + QStringLiteral( "/inapp_range.hkx" );
					HkxWriteReport rep2;
					QString err2;
					const bool okSave = d3->save( rp, rep2, err2 );
					check( *st, QStringLiteral( "(l) save with the range 10..70: %1 %2" ).arg( rep2.summary(), err2 ), okSave );
					const HkxAnimFile back2 = hkxAnimLoad( rp );
					if ( okSave && back2.ok() && !back2.clips.isEmpty() ) {
						const HkxAnimClip & bc = back2.clips.first();
						const float want = 60.0f * d3->clip.frameDuration;
						check( *st, QStringLiteral( "(l) the .hkx holds the RANGE: %1 frames, %2 s (wanted 61 and %3)" ).arg( bc.numFrames ).arg( bc.duration ).arg( want ),
							   bc.numFrames == 61 && std::fabs( bc.duration - want ) < 1e-5f );
						check( *st, QStringLiteral( "(l) its frame 0 is the clip's frame 10, bit for bit" ),
							   HkxClipDocument::transformsEqual( bc.frames.at( 0 ).at( 3 ), d3->clip.frames.at( 10 ).at( 3 ) ) );
						check( *st, QStringLiteral( "(l) and the open document still has every frame: %1" ).arg( d3->numFrames() ), d3->numFrames() == frames0 );
					} else {
						check( *st, QStringLiteral( "(l) our reader reads the saved range back: %1" ).arg( back2.error ), false );
					}
				} else {
					skip( *st, "(l) no WW_ANIMWS_OUT: the saved duration not measured" );
				}

				// Undo puts the range back, one gesture at a time
				grp->undo();
				qApp->processEvents();
				rangeIs( "one Undo", 10, 82 );
				grp->undo();
				grp->undo();
				qApp->processEvents();
				rangeIs( "three Undos", 0, 92 );

				// THE FLOOR for the saved duration: with the whole clip in range
				// the same save must come back 93 frames
				if ( !st->outDir.isEmpty() ) {
					const QString rp = st->outDir + QStringLiteral( "/inapp_range_whole.hkx" );
					HkxWriteReport rep3;
					QString err3;
					ws->document()->save( rp, rep3, err3 );
					const HkxAnimFile back3 = hkxAnimLoad( rp );
					const int n3 = ( back3.ok() && !back3.clips.isEmpty() ) ? back3.clips.first().numFrames : -1;
					check( *st, QStringLiteral( "(l floor) the SAME save with nothing out of range writes the whole clip: %1 frames" ).arg( n3 ), n3 == 93 );
				}
				doc = ws->document();
			}

			/* ---- (m) RIGHT-CLICK ANYWHERE, AND THE ZOOM THAT RESET ITSELF:
			 * bungo's rulings 7 and 7a of 2026-09-12.
			 *
			 * Verbatim 7: "Now, why can't I right click and insert an
			 * annotation anywhere?" Verbatim 7a: "we need to add and remove
			 * annotations with right click, also, when I zoom into the timeline
			 * and try to drag an already existing annotation, the zoom resets
			 * to default zoom for some reason".
			 *
			 * The menu is MODAL (QMenu::exec), so it cannot be read from the
			 * line that opens it: a 0 ms timer queued first fires inside the
			 * menu's own event loop, copies out every entry's text, triggers
			 * the one asked for and closes it. That is the same path a hand
			 * takes, and it proves the TEXT the hand would read -- which is
			 * where the clicked frame has to show up.
			 *
			 * THE FLOORS: (1) the frame in the text is read at three different
			 * x positions, so a menu still built from the playhead would fail;
			 * (2) after the drag the view is compared to the zoom it was given,
			 * then frameAll() is called and the same comparison must fail, so
			 * the zoom check is shown able to catch a reset.
			 */
			{
				const int undoM0 = ws->undoStack()->index();
				const int annM0 = ws->document()->annotationCount();
				const int framesM0 = ws->document()->numFrames();
				sheet->frameAll();
				qApp->processEvents();

				/* THE FIXTURE CARRIES ITS OWN 'FootLeft', at frame 2. The gate's
				 * own FootLeft is the one (d) put at frame 30, and annotations are
				 * kept in time order, so the LAST match by index is the gate's and
				 * the FIRST is the fixture's. Taking the first was a defect in
				 * these two lookups, not in the workspace. */
				auto annIndexOf = [&]( const QString & name ) {
					const HkxClipDocument * d4 = ws->document();
					int found = -1;
					for ( int i = 0; i < d4->clip.annotations.value( 0 ).count(); i++ )
						if ( d4->clip.annotations.value( 0 ).at( i ).text == name )
							found = i;
					return found;
				};
				auto annFrameOf = [&]( const QString & name ) {
					const HkxClipDocument * d4 = ws->document();
					const int i = annIndexOf( name );
					return i < 0 ? -1 : d4->frameOfTime( d4->clip.annotations.value( 0 ).at( i ).time );
				};

				QStringList menuTexts;
				QString triggeredText;
				/* The PICTURE of the menu, when one is owed (bungo's rulings 7 and
				   7a). It has to be taken while the popup is alive, which is inside
				   the same timer that reads the entries out of it -- so the shot
				   path is set just before the openMenu call that owes one and the
				   lambda takes a copy. Empty = no picture. */
				QString menuShot;
				QStringList menuShotsTaken;
				auto openMenu = [&]( const QPoint & pos, const QString & wantPrefix ) {
					menuTexts.clear();
					triggeredText.clear();
					QTimer::singleShot( 0, qApp, [&menuTexts, &triggeredText, &menuShotsTaken, wantPrefix, shot = menuShot]() {
						auto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );
						if ( !m )
							return;
						if ( !shot.isEmpty() ) {
							const QPixmap pm = m->grab();
							if ( pm.width() > 40 && pm.save( shot ) )
								menuShotsTaken << QStringLiteral( "%1 (%2x%3)" ).arg( shot ).arg( pm.width() ).arg( pm.height() );
						}
						QAction * hit = nullptr;
						for ( QAction * a : m->actions() ) {
							if ( a->isSeparator() )
								continue;
							menuTexts << a->text();
							if ( !wantPrefix.isEmpty() && !hit && a->text().startsWith( wantPrefix ) )
								hit = a;
						}
						m->close();
						if ( hit ) {
							triggeredText = hit->text();
							hit->trigger();
						}
					} );
					QContextMenuEvent ce( QContextMenuEvent::Mouse, pos, sheet->mapToGlobal( pos ) );
					QApplication::sendEvent( sheet, &ce );
					qApp->processEvents();
				};
				auto hasEntry = [&]( const QString & prefix ) {
					for ( const QString & t : menuTexts )
						if ( t.startsWith( prefix ) )
							return true;
					return false;
				};
				auto typeIntoEditor = [&]( const QString & name, bool escape ) {
					auto * ed = widget<QLineEdit>( ws, "AnimWsMarkerNameEdit" );
					if ( !ed || !ed->isVisible() )
						return false;
					ed->setText( name );
					QKeyEvent kp( QEvent::KeyPress, escape ? Qt::Key_Escape : Qt::Key_Return, Qt::NoModifier );
					QApplication::sendEvent( ed, &kp );
					qApp->processEvents();
					return true;
				};

				// ---- the menu on a BONE row carries the clicked frame
				int boneRowM = -1, boneY = -1;
				for ( int rr : sheet->visibleRows() ) {
					if ( sheet->rows().at( rr ).kind != AnimWsRow::Bone )
						continue;
					const int yy = sheet->rowCenterY( rr );
					if ( yy > 0 ) {
						boneRowM = rr;
						boneY = yy;
						break;
					}
				}
				Q_UNUSED( boneRowM );
				if ( boneY > 0 ) {
					openMenu( QPoint( int( sheet->frameToX( 12.0f ) ), boneY ), QString() );
					say( *st, QStringLiteral( "  (m) right-click on a bone row at frame 12 offers: %1" ).arg( menuTexts.join( QStringLiteral( " | " ) ) ) );
					check( *st, QStringLiteral( "(m) the bone row's menu names the CLICKED frame 12, not the playhead %1" ).arg( sheet->currentFrame() ),
						   hasEntry( QStringLiteral( "Add annotation at frame 12" ) ) && hasEntry( QStringLiteral( "Insert key at frame 12" ) ) );
				} else {
					skip( *st, "(m) no bone row on screen for the row menu" );
				}

				// ---- the RULER had NO menu at all before this lane
				menuShot = st->outDir.isEmpty() ? QString() : st->outDir + QStringLiteral( "/r7_sheet_menu_frame37.png" );
				openMenu( QPoint( int( sheet->frameToX( 37.0f ) ), 4 ), QString() );
				menuShot.clear();
				say( *st, QStringLiteral( "  (m) right-click on the ruler at frame 37 offers: %1" ).arg( menuTexts.join( QStringLiteral( " | " ) ) ) );
				check( *st, QStringLiteral( "(m) the RULER now answers a right-click, at frame 37: %1 entries" ).arg( menuTexts.count() ),
					   !menuTexts.isEmpty() && hasEntry( QStringLiteral( "Add annotation at frame 37" ) ) );

				// ---- a THIRD x: the same entry has to move with the cursor
				openMenu( QPoint( int( sheet->frameToX( 70.0f ) ), 4 ), QString() );
				check( *st, QStringLiteral( "(m) and at frame 70 it says 70 (the floor for a playhead-built menu)" ),
					   hasEntry( QStringLiteral( "Add annotation at frame 70" ) ) && !hasEntry( QStringLiteral( "Add annotation at frame 37" ) ) );

				// ---- the menu ON AN ANNOTATION: rename and remove (7a)
				const int footIdx = annIndexOf( QStringLiteral( "FootLeft" ) );
				const int footFrame = annFrameOf( QStringLiteral( "FootLeft" ) );
				const int markerY = sheet->rulerHeight() + sheet->markerRowHeight() / 2;
				check( *st, QStringLiteral( "(m) the fixture's 'FootLeft' annotation is at frame %1 (index %2)" ).arg( footFrame ).arg( footIdx ), footFrame == 30 && footIdx >= 0 );
				menuShot = st->outDir.isEmpty() ? QString() : st->outDir + QStringLiteral( "/r7a_annotation_menu.png" );
				openMenu( QPoint( int( sheet->frameToX( float( footFrame ) ) ), markerY ), QString() );
				menuShot.clear();
				say( *st, QStringLiteral( "  (m) right-click on the 'FootLeft' marker offers: %1" ).arg( menuTexts.join( QStringLiteral( " | " ) ) ) );
				check( *st, QStringLiteral( "(m7a) the marker's menu has Rename and Remove by name" ),
					   hasEntry( QStringLiteral( "Rename annotation 'FootLeft'" ) ) && hasEntry( QStringLiteral( "Remove annotation 'FootLeft'" ) ) );

				// ---- add one from the ruler: the inline editor, then Return
				openMenu( QPoint( int( sheet->frameToX( 37.0f ) ), 4 ), QStringLiteral( "Add annotation at frame" ) );
				auto * nameEd = widget<QLineEdit>( ws, "AnimWsMarkerNameEdit" );
				check( *st, QStringLiteral( "(m) triggering '%1' opens the inline editor on the marker row: visible %2, ghost at frame %3" )
						   .arg( triggeredText ).arg( nameEd && nameEd->isVisible() ).arg( sheet->pendingMarkerFrame() ),
					   nameEd && nameEd->isVisible() && sheet->pendingMarkerFrame() == 37 );
				check( *st, QStringLiteral( "(m) and NOTHING is in the document yet: %1 annotations, %2 commands" ).arg( ws->document()->annotationCount() ).arg( ws->undoStack()->index() ),
					   ws->document()->annotationCount() == annM0 && ws->undoStack()->index() == undoM0 );
				check( *st, QStringLiteral( "(m) the name is typed and committed with Return" ), typeIntoEditor( QStringLiteral( "SlideStart" ), false ) );
				check( *st, QStringLiteral( "(m) 'SlideStart' is at frame %1 and it cost ONE command (%2 -> %3)" ).arg( annFrameOf( QStringLiteral( "SlideStart" ) ) ).arg( undoM0 ).arg( ws->undoStack()->index() ),
					   annFrameOf( QStringLiteral( "SlideStart" ) ) == 37 && ws->document()->annotationCount() == annM0 + 1
					   && ws->undoStack()->index() == undoM0 + 1 );
				check( *st, QStringLiteral( "(m) the new annotation is the ACTIVE one (ruling 7b's orange lands on it): index %1" ).arg( sheet->selectedMarker().index ),
					   sheet->selectedMarker().valid() && sheet->selectedMarker().index == annIndexOf( QStringLiteral( "SlideStart" ) ) );

				// ---- M at the playhead, then Escape: nothing added at all
				sheet->setCurrentFrame( 50 );
				{
					QKeyEvent km( QEvent::KeyPress, Qt::Key_M, Qt::NoModifier );
					QApplication::sendEvent( sheet, &km );
					qApp->processEvents();
					auto * ed2 = widget<QLineEdit>( ws, "AnimWsMarkerNameEdit" );
					check( *st, QStringLiteral( "(m) M opens the editor at the playhead: frame %1" ).arg( sheet->pendingMarkerFrame() ),
						   ed2 && ed2->isVisible() && sheet->pendingMarkerFrame() == 50 );
					check( *st, QStringLiteral( "(m) Escape closes it" ), typeIntoEditor( QStringLiteral( "NeverAdded" ), true ) );
					check( *st, QStringLiteral( "(m) and Escape added NOTHING: %1 annotations, %2 commands" ).arg( ws->document()->annotationCount() ).arg( ws->undoStack()->index() ),
						   ws->document()->annotationCount() == annM0 + 1 && ws->undoStack()->index() == undoM0 + 1
						   && annFrameOf( QStringLiteral( "NeverAdded" ) ) < 0 );
				}

				// ---- Ctrl+M renames the active annotation, inline
				{
					QKeyEvent kr( QEvent::KeyPress, Qt::Key_M, Qt::ControlModifier );
					QApplication::sendEvent( sheet, &kr );
					qApp->processEvents();
					auto * ed3 = widget<QLineEdit>( ws, "AnimWsMarkerNameEdit" );
					check( *st, QStringLiteral( "(m) Ctrl+M opens the editor on the active annotation with its name in it: '%1'" ).arg( ed3 ? ed3->text() : QString() ),
						   ed3 && ed3->isVisible() && ed3->text() == QStringLiteral( "SlideStart" ) );
					typeIntoEditor( QStringLiteral( "SlideGo" ), false );
					check( *st, QStringLiteral( "(m) renamed in place: 'SlideGo' at frame %1, 'SlideStart' gone (%2)" ).arg( annFrameOf( QStringLiteral( "SlideGo" ) ) ).arg( annFrameOf( QStringLiteral( "SlideStart" ) ) ),
						   annFrameOf( QStringLiteral( "SlideGo" ) ) == 37 && annFrameOf( QStringLiteral( "SlideStart" ) ) < 0
						   && ws->undoStack()->index() == undoM0 + 2 );
				}

				// ---- and remove it by right-click (7a's first half)
				openMenu( QPoint( int( sheet->frameToX( 37.0f ) ), markerY ), QStringLiteral( "Remove annotation" ) );
				check( *st, QStringLiteral( "(m7a) '%1' took it back out: %2 annotations (started at %3)" ).arg( triggeredText ).arg( ws->document()->annotationCount() ).arg( annM0 ),
					   ws->document()->annotationCount() == annM0 && annFrameOf( QStringLiteral( "SlideGo" ) ) < 0
					   && ws->undoStack()->index() == undoM0 + 3 );

				/* ---- (m7a) THE ZOOM. Zoom to a known window, drag the
				   existing 'FootLeft' marker 5 frames, and the window must be
				   where it was. */
				sheet->setView( 20.0f, 60.0f );
				qApp->processEvents();
				const float zoom0 = sheet->viewFirst(), zoom1 = sheet->viewLast();
				{
					const int x0 = int( sheet->frameToX( float( footFrame ) ) );
					const float pxPerFrame = sheet->frameToX( 1.0f ) - sheet->frameToX( 0.0f );
					const int x1 = x0 + int( std::lround( double( pxPerFrame ) * 5.0 ) );
					const QPoint a( x0, markerY ), c( x1, markerY );
					QMouseEvent pr( QEvent::MouseButtonPress, QPointF( a ), QPointF( sheet->mapToGlobal( a ) ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
					QMouseEvent mv( QEvent::MouseMove, QPointF( c ), QPointF( sheet->mapToGlobal( c ) ), Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
					QMouseEvent rl( QEvent::MouseButtonRelease, QPointF( c ), QPointF( sheet->mapToGlobal( c ) ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( sheet, &pr );
					QApplication::sendEvent( sheet, &mv );
					QApplication::sendEvent( sheet, &rl );
					qApp->processEvents();
					say( *st, QStringLiteral( "  (m7a) zoomed to %1..%2, dragged the marker at frame %3 by 5 frames (%4 px per frame)" )
							  .arg( zoom0 ).arg( zoom1 ).arg( footFrame ).arg( pxPerFrame ) );
					check( *st, QStringLiteral( "(m7a) the annotation moved exactly 5 frames: %1 -> %2" ).arg( footFrame ).arg( annFrameOf( QStringLiteral( "FootLeft" ) ) ),
						   annFrameOf( QStringLiteral( "FootLeft" ) ) == footFrame + 5 );
					check( *st, QStringLiteral( "(m7a) AND THE ZOOM STAYED: %1..%2 (was %3..%4)" ).arg( sheet->viewFirst() ).arg( sheet->viewLast() ).arg( zoom0 ).arg( zoom1 ),
						   std::fabs( sheet->viewFirst() - zoom0 ) < 0.01f && std::fabs( sheet->viewLast() - zoom1 ) < 0.01f );
					check( *st, QStringLiteral( "(m7a) the dragged annotation is still the selected one: index %1 of %2" ).arg( sheet->selectedMarker().index ).arg( sheet->selectedMarkers().count() ),
						   sheet->selectedMarker().valid() && sheet->selectedMarkers().count() >= 1 );
					check( *st, QStringLiteral( "(m7a) and the clip is untouched otherwise: %1 frames, %2 annotations" ).arg( ws->document()->numFrames() ).arg( ws->document()->annotationCount() ),
						   ws->document()->numFrames() == framesM0 && ws->document()->annotationCount() == annM0 );
				}
				/* THE PICTURE for ruling 7a's second half: the sheet AFTER the
				   drag, still at the zoom it was set to. The check above says the
				   window is unchanged as a number; this is the same fact as a
				   picture, taken before frameAll() throws the zoom away. */
				if ( !st->outDir.isEmpty() ) {
					const QPixmap zp = sheet->grab();
					const QString zs = st->outDir + QStringLiteral( "/r7a_zoom_after_drag.png" );
					check( *st, QStringLiteral( "picture: (7a) the sheet still at %1..%2 after the marker drag, %3x%4 -> %5" )
						   .arg( sheet->viewFirst() ).arg( sheet->viewLast() ).arg( zp.width() ).arg( zp.height() ).arg( zs ),
						   zp.width() > 200 && zp.save( zs ) );
				}
				check( *st, QStringLiteral( "picture: (7) and (7a) the two context menus: %1" ).arg( menuShotsTaken.join( QStringLiteral( " | " ) ) ),
					   st->outDir.isEmpty() || menuShotsTaken.count() == 2 );
				// THE FLOOR for the zoom check: a real reset must fail it
				sheet->frameAll();
				qApp->processEvents();
				check( *st, QStringLiteral( "(m7a floor) frameAll() DOES change the window (%1..%2), so the check above can catch a reset" ).arg( sheet->viewFirst() ).arg( sheet->viewLast() ),
					   std::fabs( sheet->viewFirst() - zoom0 ) > 0.01f || std::fabs( sheet->viewLast() - zoom1 ) > 0.01f );

				// ---- put the fixture back the way (g) and (h) expect it
				while ( ws->undoStack()->index() > undoM0 && ws->undoStack()->canUndo() )
					grp->undo();
				qApp->processEvents();
				check( *st, QStringLiteral( "(m) undone back to where the gate started: %1 commands, %2 annotations, 'FootLeft' at frame %3" )
						   .arg( ws->undoStack()->index() ).arg( ws->document()->annotationCount() ).arg( annFrameOf( QStringLiteral( "FootLeft" ) ) ),
					   ws->undoStack()->index() == undoM0 && ws->document()->annotationCount() == annM0
					   && annFrameOf( QStringLiteral( "FootLeft" ) ) == 30 );
				doc = ws->document();
			}

			// ---- (g) save as .hkx (the button's dialog cannot run; the document's save is what it calls)
			QString savedPath;
			if ( !st->outDir.isEmpty() ) {
				savedPath = st->outDir + QStringLiteral( "/inapp_edited.hkx" );
				HkxWriteReport rep;
				QString err;
				check( *st, QStringLiteral( "(g) save as %1: %2 %3" ).arg( savedPath, rep.summary(), err ), doc->save( savedPath, rep, err ) );
				const HkxAnimFile back = hkxAnimLoad( savedPath );
				check( *st, QStringLiteral( "(g) our reader reads it: %1" ).arg( back.error ), back.ok() && !back.clips.isEmpty() );
				if ( back.ok() && !back.clips.isEmpty() ) {
					int f = -1, t = -1;
					check( *st, QStringLiteral( "(g) decoded == the edited document bit for bit (first diff frame %1 track %2)" ).arg( f ).arg( t ), HkxClipDocument::framesEqual( back.clips.first(), doc->clip, &f, &t ) );
					bool found = false;
					HkxClipDocument bd = HkxClipDocument::fromClip( back.clips.first() );
					for ( const HkxAnnotation & a : bd.clip.annotations.value( 0 ) )
						if ( a.text == QStringLiteral( "FootLeft" ) && bd.frameOfTime( a.time ) == 30 )
							found = true;
					check( *st, "(d) reloaded: 'FootLeft' present at frame 30", found );
				}
				// through the loader: the saved file loads as a second entry
				bool ref2 = false;
				WwHkxAnimHub::instance()->loadFiles( ogl, { savedPath }, false, &ref2 );
				check( *st, "(d) the saved file loads through the loader as an entry", !ref2 && pb && pb->has( QStringLiteral( "inapp_edited" ) ) );
				if ( pb && pb->has( QStringLiteral( "inapp_edited" ) ) )
					WwHkxAnimHub::instance()->unload( ogl, QStringLiteral( "inapp_edited" ) );
				qApp->processEvents();
				ws->refresh();
				qApp->processEvents();
			} else {
				skip( *st, "no WW_ANIMWS_OUT: save-as not measured" );
			}

			// ---- (e) trim and retime through the rows and buttons
			doc = ws->document();
			const HkxAnimClip beforeTrim = doc->clip;
			/* the Trim from / Trim to rows are gone (bungo's ruling 9): the play
			   range in the transport row is those two numbers, and "Trim to
			   range" is the same cut made immediately. */
			widget<QSpinBox>( ws, "AnimWsRangeStart" )->setValue( 10 );
			widget<QSpinBox>( ws, "AnimWsRangeEnd" )->setValue( 50 );
			ws->setRangeFromBoxes();
			qApp->processEvents();
			ws->trimToRange();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(e) trim 10..50 -> 41 frames: %1" ).arg( doc->numFrames() ), doc->numFrames() == 41 );
			{
				bool all = true;
				for ( int t = 0; t < doc->numTracks(); t++ )
					all = all && HkxClipDocument::transformsEqual( doc->clip.frames.at( 0 ).at( t ), beforeTrim.frames.at( 10 ).at( t ) );
				check( *st, "(e) frame 0 equals old frame 10 exactly", all );
				check( *st, QStringLiteral( "(e) the scene's range followed: end %1 s" ).arg( sc->animTags.value( entry ).value( QStringLiteral( "end" ) ) ),
					std::fabs( sc->animTags.value( entry ).value( QStringLiteral( "end" ) ) - 40.0f / 60.0f ) < 1e-4f );
				check( *st, QStringLiteral( "(e) the sheet has 41 frames: %1" ).arg( sheet->numFrames() ), sheet->numFrames() == 41 );
			}
			grp->undo();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(h) Undo the trim -> 93 frames: %1" ).arg( doc->numFrames() ), doc->numFrames() == 93 );
			widget<QDoubleSpinBox>( ws, "AnimWsRetimeFps" )->setValue( 30.0 );
			ws->retimeClip();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(e) retime 60->30 -> 47 frames: %1, ruler %2 fps" ).arg( doc->numFrames() ).arg( sheet->rulerFps() ), doc->numFrames() == 47 && std::fabs( sheet->rulerFps() - 30.0f ) < 0.01f );
			{
				int bad = 0;
				for ( int f = 0; f < doc->numFrames(); f++ )
					for ( int t = 0; t < doc->numTracks(); t++ )
						if ( !HkxClipDocument::transformsEqual( doc->clip.frames.at( f ).at( t ), beforeTrim.frames.at( 2 * f ).at( t ) ) ) bad++;
				check( *st, QStringLiteral( "(e) coincident frames bit-identical: %1 differ" ).arg( bad ), bad == 0 );
			}
			grp->undo();
			qApp->processEvents();
			doc = ws->document();
			check( *st, QStringLiteral( "(h) Undo the retime -> 93 frames at 60: %1 / %2" ).arg( doc->numFrames() ).arg( doc->fps() ), doc->numFrames() == 93 && std::fabs( doc->fps() - 60.0f ) < 0.01f );

			// ---- (f) bake / unbake on COM through the row and buttons
			{
				auto * rootBox = widget<QComboBox>( ws, "AnimWsRootTrack" );
				const int com = doc->findTrack( QStringLiteral( "COM" ) );
				check( *st, QStringLiteral( "(f) the root-motion row defaults to COM: '%1'" ).arg( rootBox ? rootBox->currentText() : QString() ), rootBox && rootBox->currentData().toInt() == com );
				const HkxAnimClip preBake = doc->clip;
				const float travelBefore = doc->trackTravel( com );
				check( *st, QStringLiteral( "(f floor) COM travels ~487 before: %1" ).arg( travelBefore ), std::fabs( travelBefore - 487.0f ) < 2.0f );
				ws->bakeRootMotion();
				qApp->processEvents();
				doc = ws->document();
				check( *st, QStringLiteral( "(f) after bake COM travel 0: %1; motion %2" ).arg( doc->trackTravel( com ) ).arg( doc->rootMotionTravel() ),
					doc->trackTravel( com ) == 0.0f && std::fabs( doc->rootMotionTravel() - 487.0f ) < 2.0f );
				{
					// ruling 6: both are menu actions now, same object names
					auto * aUn = ws->findChild<QAction *>( QStringLiteral( "AnimWsUnbake" ) );
					auto * aBa = ws->findChild<QAction *>( QStringLiteral( "AnimWsBake" ) );
					check( *st, "(f) Unbake is the enabled action now", aUn && aBa && aUn->isEnabled() && !aBa->isEnabled() );
				}
				ws->unbakeRootMotion();
				qApp->processEvents();
				doc = ws->document();
				int f = -1, t = -1;
				check( *st, QStringLiteral( "(f) unbake reverses byte-identically (first diff frame %1 track %2)" ).arg( f ).arg( t ), HkxClipDocument::framesEqual( doc->clip, preBake, &f, &t ) );
				bool rmSame = doc->clip.rootMotion.count() == preBake.rootMotion.count();
				for ( int i = 0; rmSame && i < preBake.rootMotion.count(); i++ )
					rmSame = std::memcmp( &doc->clip.rootMotion.at( i ), &preBake.rootMotion.at( i ), sizeof( HkxRootMotion ) ) == 0;
				check( *st, "(f) ... and the extracted motion byte-identically", rmSame );
			}

			/* ---- (n) REMOVE TRANSFORM AXES: bungo's ruling 3 of 2026-09-12.
			 *
			 * Verbatim: "Add an option here, under remove track, to remove all
			 * transforms in specific directions, so x, y, z or a combination of
			 * them. Same goes for rotation direction. That way, I can make it so
			 * that the slide stays in the center, but the COM still moves
			 * downward when the player crouches."
			 *
			 * Measured on his own fixture, on the track he named: strip COM's
			 * translation X and Y, tick nothing else, and drive it the way a
			 * hand does -- the right-click entry, then the dialog's own boxes
			 * and its Remove button, both read inside their modal loops by a
			 * 0 ms timer queued first.
			 *
			 * THE FLOORS: (1) before the strip X and Y must actually travel, and
			 * the numbers are printed, so a no-op cannot pass as a success;
			 * (2) Z is compared BIT for bit, not with a tolerance, so a Z that
			 * went through the Euler round trip would fail; (3) the entry must
			 * sit directly under "Remove track", which is where the ruling puts
			 * it; (4) the file is written and read back and must equal the
			 * edited document bit for bit; (5) one Undo must restore every byte.
			 */
			{
				const int com = doc->findTrack( QStringLiteral( "COM" ) );
				const int comRow = sheet->rowOfTrack( com );
				const int undoN0 = ws->undoStack()->index();
				const HkxAnimClip preStrip = doc->clip;
				const QVector<HkxKey> keysBefore = doc->keys.value( com );
				auto spanOf = []( const QVector<HkxKey> & ks, int axis ) {
					float lo = 0.0f, hi = 0.0f;
					for ( int i = 0; i < ks.count(); i++ ) {
						const float v = ks.at( i ).xf.translation[axis];
						lo = i == 0 ? v : std::min( lo, v );
						hi = i == 0 ? v : std::max( hi, v );
					}
					return hi - lo;
				};
				const float spanX0 = spanOf( keysBefore, 0 ), spanY0 = spanOf( keysBefore, 1 ), spanZ0 = spanOf( keysBefore, 2 );
				check( *st, QStringLiteral( "(n floor) COM has 93 keys and moves in all three before the strip: %1 keys, X %2, Y %3, Z %4" )
					   .arg( keysBefore.count() ).arg( spanX0, 0, 'f', 2 ).arg( spanY0, 0, 'f', 2 ).arg( spanZ0, 0, 'f', 2 ),
					   keysBefore.count() == 93 && spanX0 > 1.0f && spanY0 > 1.0f && spanZ0 > 1.0f );

				sheet->selectRow( comRow );
				qApp->processEvents();
				check( *st, QStringLiteral( "(n) the COM row is the selected track: row %1, track %2 (COM %3)" ).arg( comRow ).arg( ws->selectedTrack() ).arg( com ),
					   comRow >= 0 && ws->selectedTrack() == com );

				// the entry, in the place the ruling puts it: directly under "Remove track"
				QStringList nTexts;
				/* A gate FORCES the state it measures. The COM row can be scrolled out
				   of the sheet by whatever ran before it -- this started skipping the
				   day gate (k8b) was added, which grabs the sheet and pumps the event
				   loop several times over -- and a right-click that cannot reach the row
				   measures nothing. So scroll it in, exactly the way gate (k) already
				   does for the thigh row, and say whether that was needed. */
				if ( sheet->rowCenterY( comRow ) <= 0 ) {
					if ( auto * sbN = sheet->findChild<QScrollBar *>( QStringLiteral( "AnimWsDopeSheetScroll" ) ) ) {
						sbN->setValue( std::max( 0, int( sheet->visibleRows().indexOf( comRow ) ) ) );
						qApp->processEvents();
					}
					say( *st, QStringLiteral( "  (n) the COM row was scrolled out of the sheet; scrolled in to row index %1, centre y now %2" )
							 .arg( sheet->visibleRows().indexOf( comRow ) ).arg( sheet->rowCenterY( comRow ) ) );
				}
				const int cyN = sheet->rowCenterY( comRow );
				if ( cyN > 0 ) {
					QTimer::singleShot( 0, qApp, [&nTexts]() {
						auto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );
						if ( !m )
							return;
						for ( QAction * a : m->actions() )
							if ( !a->isSeparator() )
								nTexts << a->text();
						m->close();
					} );
					const QPoint posN( sheet->width() / 2, cyN );
					QContextMenuEvent ceN( QContextMenuEvent::Mouse, posN, sheet->mapToGlobal( posN ) );
					QApplication::sendEvent( sheet, &ceN );
					qApp->processEvents();
					int iRemove = -1, iAxes = -1;
					for ( int i = 0; i < nTexts.count(); i++ ) {
						if ( nTexts.at( i ) == QStringLiteral( "Remove track" ) )
							iRemove = i;
						if ( iAxes < 0 && nTexts.at( i ).startsWith( QStringLiteral( "Remove transform axes" ) ) )
							iAxes = i;
					}
					check( *st, QStringLiteral( "(n) the right-click menu carries 'Remove transform axes...' directly under 'Remove track' (%1 then %2 of %3 entries)" )
						   .arg( iRemove ).arg( iAxes ).arg( nTexts.count() ),
						   iRemove >= 0 && iAxes == iRemove + 1 );
				} else {
					skip( *st, "the COM row is scrolled out of the sheet; the menu half of (n) not measured" );
				}

				// the dialog: tick translation X and Y, press Remove
				bool dlgSeen = false, boxesSeen = false;
				QString dlgShotLine;
				QTimer::singleShot( 0, qApp, [&dlgSeen, &boxesSeen, &dlgShotLine, out = st->outDir]() {
					QWidget * dw = QApplication::activeModalWidget();
					if ( !dw || dw->objectName() != QStringLiteral( "AnimWsAxisDialog" ) )
						return;
					dlgSeen = true;
					auto * bx = dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisTX" ) );
					auto * by = dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisTY" ) );
					boxesSeen = bx && by && dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisTZ" ) )
						&& dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisRX" ) )
						&& dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisRY" ) )
						&& dw->findChild<QCheckBox *>( QStringLiteral( "AnimWsAxisRZ" ) )
						&& dw->findChild<QLabel *>( QStringLiteral( "AnimWsAxisConvention" ) );
					if ( bx )
						bx->setChecked( true );
					if ( by )
						by->setChecked( true );
					/* RULING 3's picture, taken with the two boxes ticked -- the
					   dialog only exists inside this timer, so this is the one
					   moment it can be photographed. */
					if ( !out.isEmpty() ) {
						const QPixmap dp = dw->grab();
						const QString ds = out + QStringLiteral( "/r3_axis_dialog.png" );
						if ( dp.width() > 80 && dp.save( ds ) )
							dlgShotLine = QStringLiteral( "%1 (%2x%3)" ).arg( ds ).arg( dp.width() ).arg( dp.height() );
					}
					if ( auto * ok = dw->findChild<QPushButton *>( QStringLiteral( "AnimWsAxisOk" ) ) )
						ok->click();
					else
						dw->close();
				} );
				ws->removeTransformAxesDialog();
				qApp->processEvents();
				check( *st, "(n) the dialog opened with six axis boxes and the line naming the rotation convention",
					   dlgSeen && boxesSeen );
				check( *st, QStringLiteral( "picture: (3) the Remove transform axes dialog with translation X and Y ticked: %1" ).arg( dlgShotLine ),
					   st->outDir.isEmpty() || !dlgShotLine.isEmpty() );

				doc = ws->document();
				const QVector<HkxKey> keysAfter = doc->keys.value( com );
				const float spanX1 = spanOf( keysAfter, 0 ), spanY1 = spanOf( keysAfter, 1 ), spanZ1 = spanOf( keysAfter, 2 );
				check( *st, QStringLiteral( "(n) COM's X and Y are one number over all %1 keys (spans %2 and %3), and it is frame 0's (%4, %5)" )
					   .arg( keysAfter.count() ).arg( spanX1, 0, 'f', 6 ).arg( spanY1, 0, 'f', 6 )
					   .arg( keysAfter.isEmpty() ? 0.0f : keysAfter.first().xf.translation[0] )
					   .arg( keysAfter.isEmpty() ? 0.0f : keysAfter.first().xf.translation[1] ),
					   keysAfter.count() == 93 && spanX1 == 0.0f && spanY1 == 0.0f
					   && !keysBefore.isEmpty() && !keysAfter.isEmpty()
					   && keysAfter.first().xf.translation[0] == keysBefore.first().xf.translation[0]
					   && keysAfter.first().xf.translation[1] == keysBefore.first().xf.translation[1] );
				check( *st, QStringLiteral( "(n) COM still moves downward: Z span %1 (was %2)" ).arg( spanZ1, 0, 'f', 2 ).arg( spanZ0, 0, 'f', 2 ),
					   std::fabs( spanZ1 - spanZ0 ) < 0.0001f && spanZ1 > 1.0f );
				int zDiff = -1, rotDiff = -1;
				for ( int i = 0; i < keysAfter.count() && i < keysBefore.count(); i++ ) {
					const float za = keysAfter.at( i ).xf.translation[2], zb = keysBefore.at( i ).xf.translation[2];
					if ( zDiff < 0 && std::memcmp( &za, &zb, sizeof( float ) ) != 0 )
						zDiff = i;
					if ( rotDiff < 0 && std::memcmp( &keysAfter.at( i ).xf.rotation, &keysBefore.at( i ).xf.rotation, sizeof( Quat ) ) != 0 )
						rotDiff = i;
				}
				check( *st, QStringLiteral( "(n) Z is byte-identical on every key (first differing key %1)" ).arg( zDiff ), zDiff < 0 );
				check( *st, QStringLiteral( "(n) the rotations were not touched at all, byte for byte (first differing key %1)" ).arg( rotDiff ), rotDiff < 0 );
				check( *st, QStringLiteral( "(n) the summary line says what was held: '%1'" ).arg( ws->noteText() ),
					   ws->noteText().contains( QStringLiteral( "translation X" ) )
					   && ws->noteText().contains( QStringLiteral( "translation Y" ) ) && !ws->noteIsRefusal() );
				check( *st, QStringLiteral( "(n) it is one undo step: %1 commands, was %2" ).arg( ws->undoStack()->index() ).arg( undoN0 ),
					   ws->undoStack()->index() == undoN0 + 1 );
				{
					int fC = -1, tC = -1;
					check( *st, QStringLiteral( "(n) the dense frames agree with the keys the strip left (track %1 frame %2)" ).arg( tC ).arg( fC ),
						   doc->consistent( &tC, &fC ) );
				}

				// it round-trips through the file
				if ( !st->outDir.isEmpty() ) {
					/* A SAVE WRITES THE PLAY RANGE, by the product's own design
					 * (src/hkxclipedit.h:120-127, "a SAVE writes the range
					 * alone", and the dock says so in words: "a save writes the
					 * range"). (l) leaves the range at 10..50, so the file that
					 * came back held 41 frames and the gate called a 93-frame
					 * clip unequal to it -- the gate's mistake, not the
					 * writer's. To weigh the bytes of the strip, the whole clip
					 * goes back in range first, and the numbers are printed. */
					/* The workspace hands out its document read-only (animworkspace.h:111)
					 * and there is no other way in from a gate. The object itself is not
					 * const -- the workspace owns it and edits it all the time -- so the
					 * cast is safe, and it is the range alone that moves: no key is
					 * touched, and no undo command is pushed, so the stack the checks
					 * above and below count stays exactly as it was. */
					HkxClipDocument * docRW = const_cast<HkxClipDocument *>( doc );
					const HkxEditResult rangeBack = docRW->setPlayRange( 0, docRW->numFrames() - 1 );
					say( *st, QStringLiteral( "  (n) the whole clip put back in range before the save: %1..%2, still partial=%3 (%4)" )
					     .arg( doc->rangeFirstFrame() ).arg( doc->rangeLastFrame() )
					     .arg( doc->rangeIsPartial() ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ).arg( rangeBack.message ) );
					const QString axPath = st->outDir + QStringLiteral( "/inapp_axisstrip.hkx" );
					HkxWriteReport repN;
					QString errN;
					check( *st, QStringLiteral( "(n) save the stripped clip as %1: %2 %3" ).arg( axPath, repN.summary(), errN ), doc->save( axPath, repN, errN ) );
					const HkxAnimFile backN = hkxAnimLoad( axPath );
					int fN = -1, tN = -1;
					check( *st, QStringLiteral( "(n) it reads back bit for bit (first diff frame %1 track %2): %3" ).arg( fN ).arg( tN ).arg( backN.error ),
						   backN.ok() && !backN.clips.isEmpty() && HkxClipDocument::framesEqual( backN.clips.first(), doc->clip, &fN, &tN ) );
				} else {
					skip( *st, "no WW_ANIMWS_OUT: the round trip half of (n) not measured" );
				}

				// and one Undo puts every byte back
				ws->undoStack()->undo();
				qApp->processEvents();
				doc = ws->document();
				int fU = -1, tU = -1;
				check( *st, QStringLiteral( "(n) Undo restores the clip byte for byte (first diff frame %1 track %2)" ).arg( fU ).arg( tU ),
					   HkxClipDocument::framesEqual( doc->clip, preStrip, &fU, &tU ) && ws->undoStack()->index() == undoN0 );
			}

			/* ---- (p) THE BUTTON ROW AND THE KEYS BLOCK: bungo's rulings 6
			 * and 6a of 2026-09-12.
			 *
			 * Verbatim (6): "it's getting pretty crowded in here, isn't it?" ->
			 * "a header menu bar (Key, Channel, Marker, View) holds the
			 * actions; the sidebar (N panel) holds only what is being edited,
			 * in collapsible sections that show ONLY when their subject is
			 * selected"; "The bottom button row goes away; every action stays
			 * reachable by menu, context menu and shortcut"; "Save / Save as
			 * move to the dock's header menu (Clip > Save)".
			 * Verbatim (6a): "Why not add another panel in the animation
			 * manager, that opens from the right side, that contains more
			 * stuff." -> collapsible, remembering its width, on the N key.
			 *
			 * HIS OWN GATE, in his words: "the dock's fixed chrome (transport +
			 * header + status) height measured before/after; the sidebar shows
			 * one section for each selection kind on the fixture and none for
			 * the others; every former button's action found in a menu by the
			 * harness by text."
			 *
			 * THE FLOORS: (1) a text that was never a button ("Frobnicate") is
			 * NOT found by the same search, so "found in a menu" is shown able
			 * to fail; (2) the retired bar's height is not guessed -- one real
			 * QPushButton is built with the same text and measured, and the
			 * bar's own margins are added, so the "before" number comes from
			 * Qt, not from memory; (3) each section check names every section
			 * that IS visible, so a panel that showed all five would fail on
			 * the same line that a correct one passes.
			 */
			{
				doc = ws->document();
				auto * header = widget<QWidget>( ws, "AnimWsHeader" );
				auto * transport = widget<QWidget>( ws, "AnimWsTransport" );
				auto * noteLine = widget<QLabel>( ws, "AnimWsNote" );
				auto * menuBar = widget<QWidget>( ws, "AnimWsMenuBar" );
				auto * panel = widget<QWidget>( ws, "AnimWsSidePanel" );
				check( *st, "(p) the dock has a header with a menu bar, and the bottom action bar is gone",
					   header && menuBar && widget<QWidget>( ws, "AnimWsActionBar" ) == nullptr );

				// the chrome, measured, with the retired row's height derived
				const int chrome = ( header ? header->height() : 0 )
					+ ( transport ? transport->height() : 0 )
					+ ( noteLine ? noteLine->height() : 0 );
				int retired = 0;
				{
					auto * probe = new QPushButton( QStringLiteral( "Insert key" ), ws );
					probe->ensurePolished();
					retired = probe->sizeHint().height() + 2 + 4;	// the old bar's top and bottom margins
					delete probe;
				}
				const int pushLeft = ws->findChildren<QPushButton *>().count();
				say( *st, QStringLiteral( "  (p) fixed chrome now %1 px (header %2 + transport %3 + note %4); the bar that went was %5 px of buttons; %6 QPushButton(s) left in the dock" )
					 .arg( chrome ).arg( header ? header->height() : -1 ).arg( transport ? transport->height() : -1 )
					 .arg( noteLine ? noteLine->height() : -1 ).arg( retired ).arg( pushLeft ) );
				check( *st, QStringLiteral( "(p) the header is slimmer than the button row it replaced: %1 px vs %2 px" )
					   .arg( header ? header->height() : -1 ).arg( retired ),
					   header && retired > 0 && header->height() <= retired );
				check( *st, QStringLiteral( "(p) no push buttons are left in the dock: %1" ).arg( pushLeft ), pushLeft == 0 );
				/* RULING 6's picture: the header menu bar that took the fifteen
				   buttons' place, with one of its menus actually open -- a picture
				   of a closed menu bar would not show that the actions are in it.
				   popup() is not modal, so the picture can be taken in line. */
				if ( !st->outDir.isEmpty() ) {
					QString line;
					if ( auto * clipMenu = ws->findChild<QMenu *>( QStringLiteral( "AnimWsMenuClip" ) ) ) {
						clipMenu->popup( ws->mapToGlobal( QPoint( 8, 40 ) ) );
						qApp->processEvents();
						const QPixmap mp = clipMenu->grab();
						const QString ms = st->outDir + QStringLiteral( "/r6_menu_clip.png" );
						if ( mp.width() > 40 && mp.save( ms ) )
							line = QStringLiteral( "%1 (%2x%3)" ).arg( ms ).arg( mp.width() ).arg( mp.height() );
						clipMenu->close();
						qApp->processEvents();
					}
					if ( auto * hdr = widget<QWidget>( ws, "AnimWsHeader" ) ) {
						const QPixmap hp = hdr->grab();
						hp.save( st->outDir + QStringLiteral( "/r6_header_bar.png" ) );
					}
					check( *st, QStringLiteral( "picture: (6) the Clip menu open on the header menu bar: %1" ).arg( line ), !line.isEmpty() );
				}

				// every former button's action, found in a menu BY TEXT
				QStringList menuTexts, menuNames;
				if ( menuBar ) {
					for ( QAction * top : menuBar->actions() ) {
						menuNames << top->text();
						if ( !top->menu() )
							continue;
						for ( QAction * a : top->menu()->actions() )
							if ( !a->isSeparator() )
								menuTexts << a->text();
					}
				}
				const QStringList wanted = {
					QStringLiteral( "Insert key" ), QStringLiteral( "Delete" ), QStringLiteral( "Reduce" ),
					QStringLiteral( "Add annotation" ), QStringLiteral( "Rename" ), QStringLiteral( "Delete annotation" ),
					QStringLiteral( "Trim to range" ), QStringLiteral( "Retime" ), QStringLiteral( "Bake root" ),
					QStringLiteral( "Unbake" ), QStringLiteral( "Remove track" ), QStringLiteral( "Rename track" ),
					QStringLiteral( "Add float track" ), QStringLiteral( "Set float key" ),
					QStringLiteral( "Save" ), QString::fromUtf8( "Save as\xe2\x80\xa6" )
				};
				QStringList missing;
				for ( const QString & w : wanted )
					if ( !menuTexts.contains( w ) )
						missing << w;
				say( *st, QStringLiteral( "  (p) menus: %1; %2 entries in them" ).arg( menuNames.join( QStringLiteral( " / " ) ) ).arg( menuTexts.count() ) );
				check( *st, QStringLiteral( "(p) all %1 former buttons are in a menu by text (missing: %2)" )
					   .arg( wanted.count() ).arg( missing.isEmpty() ? QStringLiteral( "none" ) : missing.join( QStringLiteral( ", " ) ) ),
					   missing.isEmpty() && menuNames.count() == 5 );
				check( *st, "(p floor) a text that never was a button is NOT found by the same search",
					   !menuTexts.contains( QStringLiteral( "Frobnicate" ) ) );
				check( *st, "(p) Save and Save as are under Clip, not at the bottom",
					   menuBar && !menuBar->actions().isEmpty() && menuBar->actions().first()->menu()
					   && [&]() {
						   QStringList clipTexts;
						   for ( QAction * a : menuBar->actions().first()->menu()->actions() )
							   if ( !a->isSeparator() )
								   clipTexts << a->text();
						   return clipTexts.contains( QStringLiteral( "Save" ) ) && clipTexts.contains( QString::fromUtf8( "Save as\xe2\x80\xa6" ) );
					   }() );

				/* ---- one section per selection kind, and NONE of the others.
				 * The panel is forced open first: a harness forces the state it
				 * measures. */
				ws->setSidePanelOpen( true );
				qApp->processEvents();
				auto * clipSec = widget<QWidget>( ws, "AnimWsClipSection" );
				auto * keySec = widget<QWidget>( ws, "AnimWsKeySection" );
				auto * annotSec = widget<QWidget>( ws, "AnimWsAnnotationSection" );
				auto * trackSec = widget<QWidget>( ws, "AnimWsTrackSection" );
				auto * floatSec = widget<QWidget>( ws, "AnimWsFloatSection" );
				check( *st, "(p) the five sections exist in the right-side panel",
					   panel && clipSec && keySec && annotSec && trackSec && floatSec
					   && panel->isAncestorOf( clipSec ) );
				auto shown = [&]() {
					QStringList v;
					if ( clipSec && clipSec->isVisible() ) v << QStringLiteral( "Clip" );
					if ( keySec && keySec->isVisible() ) v << QStringLiteral( "Key" );
					if ( annotSec && annotSec->isVisible() ) v << QStringLiteral( "Annotation" );
					if ( trackSec && trackSec->isVisible() ) v << QStringLiteral( "Track" );
					if ( floatSec && floatSec->isVisible() ) v << QStringLiteral( "Float track" );
					return v;
				};

				if ( clipSec && keySec && annotSec && trackSec && floatSec ) {
					// (1) a bone row selected -> Clip + Track, nothing else
					sheet->clearSelection();
					const int comRowP = sheet->rowOfTrack( doc->findTrack( QStringLiteral( "COM" ) ) );
					if ( comRowP >= 0 ) {
						sheet->selectRow( comRowP );
						qApp->processEvents();
						check( *st, QStringLiteral( "(p) a bone row selected shows Clip and Track only: %1" ).arg( shown().join( QStringLiteral( "+" ) ) ),
							   shown() == QStringList( { QStringLiteral( "Clip" ), QStringLiteral( "Track" ) } ) );
						auto * boneField = widget<QLineEdit>( ws, "AnimWsTrackName" );
						check( *st, QStringLiteral( "(p) the Track section's Bone field reads the selected track: '%1'" ).arg( boneField ? boneField->text() : QString() ),
							   boneField && boneField->text() == QStringLiteral( "COM" ) );
						if ( !st->outDir.isEmpty() && panel ) {
							// RULING 6a's picture: the right-side panel, on a bone row
							const QPixmap pp = panel->grab();
							const QString ps = st->outDir + QStringLiteral( "/r6a_panel_track.png" );
							check( *st, QStringLiteral( "picture: (6a) the side panel with a bone row selected (%1), %2x%3 -> %4" )
								   .arg( shown().join( QStringLiteral( "+" ) ) ).arg( pp.width() ).arg( pp.height() ).arg( ps ),
								   pp.width() > 80 && pp.save( ps ) );
						}

						// (2) keys selected on that row -> Clip + Key
						const int comTrackP = doc->findTrack( QStringLiteral( "COM" ) );
						if ( !doc->keys.value( comTrackP ).isEmpty() ) {
							const int kf = doc->keys.value( comTrackP ).first().frame;
							sheet->selectKeys( { HkxKeyRef{ comTrackP, kf } } );
							qApp->processEvents();
							check( *st, QStringLiteral( "(p) keys selected show Clip and Key only: %1" ).arg( shown().join( QStringLiteral( "+" ) ) ),
								   shown() == QStringList( { QStringLiteral( "Clip" ), QStringLiteral( "Key" ) } ) );
						} else {
							skip( *st, "the COM track has no keys here; the Key section half of (p) not measured" );
						}
					} else {
						skip( *st, "no COM row on the sheet; the Track and Key halves of (p) not measured" );
					}

					// (3) an annotation selected -> Clip + Annotation
					int aTrack = -1, aIndex = -1;
					for ( int t = 0; t < doc->clip.annotations.count() && aTrack < 0; t++ )
						if ( !doc->clip.annotations.at( t ).isEmpty() ) {
							aTrack = t;
							aIndex = 0;
						}
					if ( aTrack >= 0 ) {
						sheet->clearSelection();
						sheet->selectMarker( AnimWsMarkerRef{ aTrack, aIndex }, false );
						qApp->processEvents();
						if ( !st->outDir.isEmpty() && panel ) {
							// RULING 6a's second picture: the same panel, another kind
							const QPixmap pp = panel->grab();
							const QString ps = st->outDir + QStringLiteral( "/r6a_panel_annotation.png" );
							check( *st, QStringLiteral( "picture: (6a) the side panel with an annotation selected (%1), %2x%3 -> %4" )
								   .arg( shown().join( QStringLiteral( "+" ) ) ).arg( pp.width() ).arg( pp.height() ).arg( ps ),
								   pp.width() > 80 && pp.save( ps ) );
						}
						check( *st, QStringLiteral( "(p) an annotation selected shows Clip and Annotation only: %1" ).arg( shown().join( QStringLiteral( "+" ) ) ),
							   shown() == QStringList( { QStringLiteral( "Clip" ), QStringLiteral( "Annotation" ) } ) );
					} else {
						skip( *st, "this clip carries no annotation at (p); the Annotation section not measured" );
					}

					// (4) a float row selected -> Clip + Float track
					const int undoP = ws->undoStack()->index();
					/* addFloatTrack opens a MODAL QInputDialog::getText, so the name has
					 * to be typed from inside the modal's own event loop: a 0 ms timer
					 * queued first fires there. Without it the whole harness sits in
					 * that loop until its watchdog gives up. */
					bool floatDlgSeen = false;
					QTimer::singleShot( 0, qApp, [&floatDlgSeen]() {
						QWidget * dw = QApplication::activeModalWidget();
						if ( !dw )
							return;
						if ( auto * idlg = qobject_cast<QInputDialog *>( dw ) ) {
							floatDlgSeen = true;
							idlg->setTextValue( QStringLiteral( "WWFloatTrack" ) );
							idlg->accept();
						} else {
							dw->close();
						}
					} );
					ws->addFloatTrack();
					qApp->processEvents();
					check( *st, "(p) Add float track asked for the name in a modal dialog", floatDlgSeen );
					int floatRowP = -1;
					for ( int i = 0; i < sheet->rows().count(); i++ )
						if ( sheet->rows().at( i ).kind == AnimWsRow::Float ) {
							floatRowP = i;
							break;
						}
					if ( floatRowP >= 0 ) {
						sheet->clearSelection();
						sheet->selectRow( floatRowP );
						qApp->processEvents();
						check( *st, QStringLiteral( "(p) a float row selected shows Clip and Float track only: %1" ).arg( shown().join( QStringLiteral( "+" ) ) ),
							   shown() == QStringList( { QStringLiteral( "Clip" ), QStringLiteral( "Float track" ) } ) );
					} else {
						skip( *st, "no float row appeared; the Float track section of (p) not measured" );
					}
					// put the document back the way the next gate expects it
					while ( ws->undoStack()->index() > undoP )
						ws->undoStack()->undo();
					sheet->clearSelection();
					qApp->processEvents();
				}

				/* ---- the panel itself: it closes, it opens, and it opens at
				 * the width it was dragged to (ruling 6a: "remembering its
				 * width"). */
				auto * splitP = widget<QSplitter>( ws, "AnimWsSplitter" );
				if ( splitP && panel && splitP->count() == 3 ) {
					ws->setSidePanelOpen( true );
					qApp->processEvents();
					QList<int> sz = splitP->sizes();
					const int want = 240;
					const int delta = want - sz.at( 2 );
					sz[1] -= delta;
					sz[2] += delta;
					splitP->setSizes( sz );
					qApp->processEvents();
					const int dragged = panel->width();
					ws->setSidePanelOpen( false );
					qApp->processEvents();
					const bool closed = !panel->isVisible();
					ws->setSidePanelOpen( true );
					qApp->processEvents();
					const int reopened = panel->width();
					check( *st, QStringLiteral( "(p) the panel closes and reopens at the width it was dragged to: %1 px -> closed %2 -> %3 px" )
						   .arg( dragged ).arg( closed ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ).arg( reopened ),
						   closed && dragged > 100 && qAbs( reopened - dragged ) <= 8 );
				} else {
					skip( *st, "the splitter does not have three panes here; the panel width half of (p) not measured" );
				}
				{
					/* The N key. A synthesized key event never reaches a
					 * QShortcut (shortcut matching happens at the platform
					 * level), so the shortcut's KEY is asserted and its
					 * activated signal is fired, which is what the real key
					 * press would reach. */
					auto * nKey = ws->findChild<QShortcut *>( QStringLiteral( "AnimWsSidePanelKey" ) );
					const bool wasOpen = panel && panel->isVisible();
					check( *st, QStringLiteral( "(p) the N key is on the sheet: key '%1'" ).arg( nKey ? nKey->key().toString() : QString() ),
						   nKey && nKey->key() == QKeySequence( Qt::Key_N ) );
					if ( nKey ) {
						QMetaObject::invokeMethod( nKey, "activated" );
						qApp->processEvents();
						check( *st, QStringLiteral( "(p) N toggles the panel: open %1 -> %2" ).arg( wasOpen ).arg( panel && panel->isVisible() ),
							   panel && panel->isVisible() != wasOpen );
						QMetaObject::invokeMethod( nKey, "activated" );
						qApp->processEvents();
					}
					ws->setSidePanelOpen( true );
					qApp->processEvents();
				}
			}

			/* ---- (q) THE TRANSPORT ICONS: bungo's ruling 4 of 2026-09-12
			 * 01:45.
			 *
			 * Verbatim: "Most of these icons are very bad looking, and unclear
			 * to what they do. They need to be better." -> "follow Blender's
			 * timeline transport, all one weight and one size, drawn as our own
			 * SVG in the wwskin palette, with tooltips that say the action AND
			 * its shortcut. Loop and speed become recognisable controls."  And
			 * the last line of the same ruling: "Load..." and "root" in the
			 * Animations header row are clipped.
			 *
			 * HIS OWN GATE, in his words: "a picture of the bar at 1:1 and 2:1,
			 * and every button's tooltip text asserted by the harness."
			 *
			 * THE FLOORS: (1) the tooltips are compared to the EXACT expected
			 * sentence, and a sentence that is not on any button ("Frobnicate
			 * the sprocket") is run through the same comparison and must not
			 * match, so "the tooltip is right" is shown able to fail; (2) the
			 * "not clipped" predicate is not just read off a wide window -- the
			 * splitter is dragged until the column is too narrow, the SAME
			 * predicate must call the header clipped there, and only then is
			 * the restored width allowed to pass; (3) every icon's size is
			 * printed, so "one size" is a number and not an impression.
			 */
			{
				auto * transport = widget<QWidget>( ws, "AnimWsTransport" );
				struct TipSpec { const char * name; QString tip; bool glyph; };
				const TipSpec tips[] = {
					{ "AnimWsToStart",  QStringLiteral( "Jump to the first frame (Shift+Left)" ), true },
					{ "AnimWsPrevKey",  QStringLiteral( "Jump to the previous key on the selected row, or on any row when none is selected (Down)" ), true },
					{ "AnimWsPlayBack", QStringLiteral( "Play backwards (Shift+Ctrl+Space)" ), true },
					{ "AnimWsPlay",     QStringLiteral( "Play, and pause while it plays (Space)" ), true },
					{ "AnimWsStop",     QStringLiteral( "Stop playing" ), true },
					{ "AnimWsNextKey",  QStringLiteral( "Jump to the next key on the selected row, or on any row when none is selected (Up)" ), true },
					{ "AnimWsToEnd",    QStringLiteral( "Jump to the last frame (Shift+Right)" ), true },
					{ "AnimWsAutoKey",  QStringLiteral( "Auto-key gizmo transforms: after every committed gizmo transform, key the bone at the playhead" ), true },
					/* bungo, 2026-09-12 06:1x: "Both icons". The pose switch was a
					   WORD in a row of drawings until this lane, so it was not in
					   this table at all and nothing held it to the set's rules. */
					{ "AnimWsPose",     QStringLiteral( "Pose with the gizmo: hold the selected bone out of the clip's pose so the viewport shows what the transform gizmo (G/R/S) does to it; Insert key then keys that pose at the playhead" ), true },
					/* Loop's tip comes from the shared QAction when the Animation
					   menu has been wired up and from the button when it has not, so
					   it is the one tip this table does not pin. */
					{ "AnimWsLoop",     QString(), true },
				};
				int wrongTip = 0, noIcon = 0, stillText = 0, sizes = 0;
				QString sizeList, tipTrouble;
				QSize firstSize;
				for ( const TipSpec & t : tips ) {
					auto * b = widget<QToolButton>( ws, t.name );
					if ( !b ) {
						noIcon++;
						tipTrouble += QStringLiteral( " %1(missing)" ).arg( t.name );
						continue;
					}
					if ( b->icon().isNull() )
						noIcon++;
					if ( b->toolButtonStyle() != Qt::ToolButtonIconOnly )
						stillText++;
					if ( !firstSize.isValid() )
						firstSize = b->iconSize();
					if ( b->iconSize() != firstSize )
						sizes++;
					sizeList += QStringLiteral( " %1=%2" ).arg( QString::fromLatin1( t.name ).mid( 6 ) ).arg( b->iconSize().width() );
					if ( !t.tip.isEmpty() && b->toolTip() != t.tip ) {
						wrongTip++;
						tipTrouble += QStringLiteral( " %1='%2'" ).arg( QString::fromLatin1( t.name ), b->toolTip() );
					}
				}
				say( *st, QStringLiteral( "  (q) icon sizes:%1 (one size = %2 px)" ).arg( sizeList ).arg( firstSize.width() ) );
				check( *st, QStringLiteral( "(q) every transport button carries a drawn icon (%1 without one)" ).arg( noIcon ), noIcon == 0 );
				check( *st, QStringLiteral( "(q) no transport button is a text glyph any more (%1 still are)" ).arg( stillText ), stillText == 0 );
				check( *st, QStringLiteral( "(q) all the icons are ONE size, %1 px (%2 the odd one out)" ).arg( firstSize.width() ).arg( sizes ),
					   firstSize.width() > 0 && sizes == 0 );
				check( *st, QStringLiteral( "(q) every tooltip is the sentence it should be, with its shortcut in it (%1 wrong:%2)" ).arg( wrongTip ).arg( tipTrouble ),
					   wrongTip == 0 );
				/* ---- THE TWO GIZMO TOGGLES, bungo 2026-09-12 06:1x ("Both
				 * icons"): each carries a drawing and no text, at the PLAY
				 * button's icon size, and the drawing is LIT when the mode is on.
				 *
				 * "Lit" is measured, not asserted: the mean colour of the icon's
				 * own ink (the pixels with alpha over half) is taken in both
				 * states, and each mean is compared to the token it is meant to
				 * be -- OFF to `text`, ON to `textBright`, since bungo's 07:3x
				 * ruling ("keep them white") replaced the accent -- so a button
				 * that merely changed its plate colour cannot pass. The two tokens
				 * are only 33 apart in summed RGB, so the "it moved" floor is 20
				 * and not the 60 the accent allowed; what keeps the pair honest is
				 * that BOTH means must land within 2 of their own token ON EVERY
				 * CHANNEL -- see chMax below for why summing will not do here.
				 * THE FLOOR is on the same line: Stop has no On
				 * pixmap at all, and the same arithmetic must report it unchanged.
				 */
				{
					auto * play = widget<QToolButton>( ws, "AnimWsPlay" );
					auto inkMean = []( const QIcon & ic, const QSize & sz, QIcon::State s ) {
						const QImage im = ic.pixmap( sz, QIcon::Normal, s ).toImage().convertToFormat( QImage::Format_ARGB32 );
						qint64 r = 0, g = 0, b = 0, n = 0;
						for ( int y = 0; y < im.height(); y++ ) {
							for ( int x = 0; x < im.width(); x++ ) {
								const QRgb px = im.pixel( x, y );
								if ( qAlpha( px ) > 128 ) { r += qRed( px ); g += qGreen( px ); b += qBlue( px ); n++; }
							}
						}
						return n ? QColor( int( r / n ), int( g / n ), int( b / n ) ) : QColor();
					};
					auto dist = []( const QColor & a, const QColor & b ) {
						if ( !a.isValid() || !b.isValid() )
							return -1;
						return std::abs( a.red() - b.red() ) + std::abs( a.green() - b.green() ) + std::abs( a.blue() - b.blue() );
					};
					/* The per-CHANNEL difference, which is what "the mean ink IS the
					   token" has to be measured with. The mean is taken over every pixel
					   whose alpha is over half, and un-premultiplying a half-transparent
					   pixel costs up to 1 per channel, so two identical inks can come back
					   3 apart in SUMMED RGB. Measured, on the 08:34 exe: the pose glyph is
					   a ring and is nearly all antialiased edge, and its ON mean read
					   #f1f2f4 against textBright #f2f3f5 -- one short on each channel --
					   while the auto-key dot, which is solid, read the token exactly. Per
					   channel the tolerance the ruling asks for (1-2) is a real gate; a
					   summed one would have to be loosened to 6 to say the same thing. */
					auto chMax = []( const QColor & a, const QColor & b ) {
						if ( !a.isValid() || !b.isValid() )
							return 99;
						return std::max( std::abs( a.red() - b.red() ),
										 std::max( std::abs( a.green() - b.green() ), std::abs( a.blue() - b.blue() ) ) );
					};
					const QColor litInk( wwSkinColor( "textBright" ) ), plainInk( wwSkinColor( "text" ) );
					const char * toggles[2] = { "AnimWsPose", "AnimWsAutoKey" };
					int notIcon = 0, wrongSize = 0, notLit = 0, notWhite = 0, notPlain = 0;
					QString litLine;
					for ( const char * nm : toggles ) {
						auto * b = widget<QToolButton>( ws, nm );
						if ( !b || b->icon().isNull() || b->toolButtonStyle() != Qt::ToolButtonIconOnly ) {
							notIcon++;
							continue;
						}
						if ( !play || b->iconSize() != play->iconSize() )
							wrongSize++;
						const QColor off = inkMean( b->icon(), b->iconSize(), QIcon::Off );
						const QColor on = inkMean( b->icon(), b->iconSize(), QIcon::On );
						const int moved = dist( off, on ), toWhite = chMax( on, litInk ), toPlain = chMax( off, plainInk );
						litLine += QStringLiteral( " %1 off=%2 on=%3 moved=%4 offWhiteMaxCh=%5 offPlainMaxCh=%6" )
							.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toWhite ).arg( toPlain );
						if ( moved < 20 )
							notLit++;
						if ( toWhite > 2 )
							notWhite++;
						if ( toPlain > 2 )
							notPlain++;
					}
					say( *st, QStringLiteral( "  (q) the gizmo toggles' ink:%1 (textBright %2, text %3)" ).arg( litLine, litInk.name(), plainInk.name() ) );
					check( *st, QStringLiteral( "(q) both gizmo toggles are drawings with no text (%1 are not)" ).arg( notIcon ), notIcon == 0 );
					check( *st, QStringLiteral( "(q) both are the PLAY button's icon size, %1 px (%2 the odd one out)" )
						   .arg( play ? play->iconSize().width() : -1 ).arg( wrongSize ), play && wrongSize == 0 );
					check( *st, QStringLiteral( "(q) both LIGHT UP when the mode is on (%1 did not move)" ).arg( notLit ), notLit == 0 );
					check( *st, QStringLiteral( "(q) and the lit ink is the palette's brightest white, textBright, not the accent (%1 off it by more than 2 on a channel)" ).arg( notWhite ), notWhite == 0 );
					check( *st, QStringLiteral( "(q) while the unlit ink is still the plain text ink (%1 off it by more than 2 on a channel)" ).arg( notPlain ), notPlain == 0 );
					/* AND THE ONE THAT DOES NOT LIGHT, said out loud. The dock's loop
					   button takes its icon from the SHARED aAnimLoop action, and the
					   render toolbar re-skins that action on every refresh of its popup
					   with a single-state icon, so nothing the dock puts on it survives.
					   Measured, not asserted: a check that Loop must NOT light would be
					   the wrong thing to own the day someone decides it should. */
					if ( auto * lp = widget<QToolButton>( ws, "AnimWsLoop" ) ) {
						const QColor lpOff = inkMean( lp->icon(), lp->iconSize(), QIcon::Off );
						const QColor lpOn = inkMean( lp->icon(), lp->iconSize(), QIcon::On );
						say( *st, QStringLiteral( "  (q) Loop, whose icon the render toolbar owns: off=%1 on=%2 moved=%3 checked=%4"
								" (not a gate -- a lit Loop would mean changing the render toolbar's own glyph)" )
							.arg( lpOff.name(), lpOn.name() ).arg( dist( lpOff, lpOn ) ).arg( lp->isChecked() ? 1 : 0 ) );
					}
					if ( auto * stop = widget<QToolButton>( ws, "AnimWsStop" ) ) {
						const int stopMoved = dist( inkMean( stop->icon(), stop->iconSize(), QIcon::Off ),
												   inkMean( stop->icon(), stop->iconSize(), QIcon::On ) );
						check( *st, QStringLiteral( "(q floor) the same arithmetic calls Stop, which has no lit state, unchanged: moved %1" ).arg( stopMoved ),
							   stopMoved == 0 );
					}
				}
				{
					// the floor: the same comparison, against a sentence no button carries
					int falseHits = 0;
					const QString notATip = QStringLiteral( "Frobnicate the sprocket (Ctrl+Q)" );
					for ( const TipSpec & t : tips ) {
						auto * b = widget<QToolButton>( ws, t.name );
						if ( b && b->toolTip() == notATip )
							falseHits++;
					}
					check( *st, "(q floor) a sentence no button carries is NOT matched by the same comparison", falseHits == 0 );
				}

				// the five keys the tooltips promise: asserted, then fired
				{
					struct KeySpec { const char * name; QKeySequence want; };
					const KeySpec keys[] = {
						{ "AnimWsPlayKey",     QKeySequence( Qt::Key_Space ) },
						{ "AnimWsPlayBackKey", QKeySequence( Qt::SHIFT | Qt::CTRL | Qt::Key_Space ) },
						{ "AnimWsToStartKey",  QKeySequence( Qt::SHIFT | Qt::Key_Left ) },
						{ "AnimWsToEndKey",    QKeySequence( Qt::SHIFT | Qt::Key_Right ) },
						{ "AnimWsNextKeyKey",  QKeySequence( Qt::Key_Up ) },
						{ "AnimWsPrevKeyKey",  QKeySequence( Qt::Key_Down ) },
					};
					int wrongKey = 0;
					QString keyList;
					for ( const KeySpec & k : keys ) {
						auto * sc = ws->findChild<QShortcut *>( QString::fromLatin1( k.name ) );
						keyList += QStringLiteral( " %1='%2'" ).arg( QString::fromLatin1( k.name ).mid( 6 ), sc ? sc->key().toString() : QStringLiteral( "MISSING" ) );
						if ( !sc || sc->key() != k.want )
							wrongKey++;
					}
					say( *st, QStringLiteral( "  (q) transport keys:%1" ).arg( keyList ) );
					check( *st, QStringLiteral( "(q) the six keys the tooltips name are on the sheet (%1 wrong)" ).arg( wrongKey ), wrongKey == 0 );

					/* A synthesized key press never reaches a QShortcut, so the
					 * jump keys are FIRED and the playhead is read back. Play is
					 * not fired here: (i) already drives it through the button,
					 * and starting the clock inside a gate leaves it running. */
					auto * toEndKey = ws->findChild<QShortcut *>( QStringLiteral( "AnimWsToEndKey" ) );
					auto * toStartKey = ws->findChild<QShortcut *>( QStringLiteral( "AnimWsToStartKey" ) );
					if ( toEndKey && toStartKey ) {
						QMetaObject::invokeMethod( toEndKey, "activated" );
						qApp->processEvents();
						const int atEnd = ws->currentFrame();
						QMetaObject::invokeMethod( toStartKey, "activated" );
						qApp->processEvents();
						const int atStart = ws->currentFrame();
						check( *st, QStringLiteral( "(q) Shift+Right then Shift+Left walk the playhead to the ends: %1 -> %2" ).arg( atEnd ).arg( atStart ),
							   atEnd > atStart );
					} else {
						skip( *st, "the jump keys are not there; the firing half of (q) not measured" );
					}
				}

				// the speed box says "1.50x", not "x1.50"
				if ( auto * speed = widget<QDoubleSpinBox>( ws, "AnimWsSpeed" ) ) {
					const double was = speed->value();
					speed->setValue( 1.5 );
					qApp->processEvents();
					const QString shown = speed->text();
					check( *st, QStringLiteral( "(q) the speed reads as one says it, '%1'" ).arg( shown ),
						   shown.startsWith( QStringLiteral( "1.50" ) ) && shown.endsWith( QStringLiteral( "x" ) ) );
					speed->setValue( was );
					qApp->processEvents();
				}

				/* ---- the last line of ruling 4: "Load..." and "root" clipped.
				 * A button is clipped when the width it was given is less than
				 * the width it asks for. The predicate is proved able to fail
				 * on the same run by squeezing the column first. */
				auto clippedCount = [&]( QString * who ) {
					int n = 0;
					const char * names[3] = { "AnimWsLoadAnim", "AnimWsUnloadAnim", "AnimWsRootMotion" };
					for ( const char * nm : names ) {
						auto * b = widget<QToolButton>( ws, nm );
						if ( !b )
							continue;
						if ( b->width() < b->sizeHint().width() ) {
							n++;
							if ( who )
								*who += QStringLiteral( " %1(%2<%3)" ).arg( QString::fromLatin1( nm ).mid( 6 ) ).arg( b->width() ).arg( b->sizeHint().width() );
						}
					}
					return n;
				};
				auto * split = widget<QSplitter>( ws, "AnimWsSplitter" );
				auto * leftCol = widget<QWidget>( ws, "AnimWsLeftColumn" );
				{
					QString who;
					const int bad = clippedCount( &who );
					say( *st, QStringLiteral( "  (q) the Animations header: column %1 px, its minimum %2 px%3" )
						 .arg( leftCol ? leftCol->width() : -1 ).arg( leftCol ? leftCol->minimumWidth() : -1 ).arg( who ) );
					check( *st, QStringLiteral( "(q) nothing in the 'Load... / root' header row is clipped (%1 clipped)" ).arg( bad ), bad == 0 );
					check( *st, QStringLiteral( "(q) the column cannot be squeezed under what its header needs: minimum %1 px" ).arg( leftCol ? leftCol->minimumWidth() : -1 ),
						   leftCol && leftCol->minimumWidth() >= 120 );
				}
				if ( split && leftCol ) {
					// the floor: force the column narrower than its minimum and re-measure
					/* THE SQUEEZE HAS TO GO THROUGH THE SPLITTER. Resizing a
					 * splitter's child directly is undone by the splitter's own
					 * layout at the next event pump, so the column was back at
					 * its full width by the time the floor measured it and
					 * nothing was clipped -- a floor that never fired. Setting
					 * the sizes on the splitter is how a hand drags the handle,
					 * and the width it actually reached is printed beside the
					 * count. */
					/* THE COLUMN CANNOT BE SQUEEZED, and that is the point of
					 * the check above: handed 40 px by the splitter it stopped
					 * at 219, because its own layout's minimum holds it there.
					 * So the floor squeezes what the predicate actually reads
					 * -- one of the three buttons -- and puts it straight back.
					 * Same predicate, same run, proved able to fail. */
					const QList<int> before = split->sizes();
					const int minWas = leftCol->minimumWidth();
					leftCol->setMinimumWidth( 0 );
					QList<int> squeezed = before;
					if ( squeezed.count() >= 2 ) {
						const int total = squeezed.at( 0 ) + squeezed.at( 1 );
						squeezed[0] = 40;
						squeezed[1] = total - 40;
					}
					split->setSizes( squeezed );
					if ( leftCol->layout() )
						leftCol->layout()->activate();
					qApp->processEvents();
					say( *st, QStringLiteral( "  (q floor) the splitter was handed 40 px for the column; it settled at %1 px (its layout's minimum is %2)" )
					     .arg( leftCol->width() ).arg( leftCol->minimumSizeHint().width() ) );
					auto * squeezeMe = widget<QToolButton>( ws, "AnimWsLoadAnim" );
					const QSize wasSize = squeezeMe ? squeezeMe->size() : QSize();
					if ( squeezeMe )
						squeezeMe->resize( 8, squeezeMe->height() );
					QString who;
					const int bad = clippedCount( &who );
					check( *st, QStringLiteral( "(q floor) with one button squeezed to 8 px the SAME test calls the header clipped (%1 clipped:%2)" )
					       .arg( bad ).arg( who ), bad > 0 );
					if ( squeezeMe )
						squeezeMe->resize( wasSize );
					// put it back exactly as it was
					leftCol->setMinimumWidth( minWas );
					split->setSizes( before );
					qApp->processEvents();
					check( *st, QStringLiteral( "(q) and at its real width nothing is clipped again (%1)" ).arg( clippedCount( nullptr ) ), clippedCount( nullptr ) == 0 );
				} else {
					skip( *st, "no splitter or left column; the squeeze floor of (q) not measured" );
				}

				// his picture, at 1:1 and at 2:1
				if ( transport && !st->outDir.isEmpty() ) {
					QDir().mkpath( st->outDir );
					const QPixmap one = transport->grab();
					const QString p1 = st->outDir + QStringLiteral( "/transport_1x.png" );
					const QString p2 = st->outDir + QStringLiteral( "/transport_2x.png" );
					const QPixmap two = one.scaled( one.width() * 2, one.height() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation );
					check( *st, QStringLiteral( "picture: (q) the transport bar %1x%2 at 1:1 -> %3, and at 2:1 -> %4" )
						   .arg( one.width() ).arg( one.height() ).arg( p1, p2 ),
						   one.width() > 200 && one.save( p1 ) && two.save( p2 ) );
					/* The two toggles OFF and ON, side by side in time: the same bar
					   photographed twice at 2:1, with nothing else changed, so the
					   lit state is visible and not merely a number above. The
					   switches are put back exactly as they were found. */
					{
						auto * pose = widget<QToolButton>( ws, "AnimWsPose" );
						auto * ak = widget<QToolButton>( ws, "AnimWsAutoKey" );
						const bool pose0 = pose && pose->isChecked(), ak0 = ak && ak->isChecked();
						auto shotAt = [&]( bool on, const char * leaf ) {
							if ( pose ) pose->setChecked( on );
							if ( ak ) ak->setChecked( on );
							qApp->processEvents();
							const QPixmap g = transport->grab();
							const QPixmap g2 = g.scaled( g.width() * 2, g.height() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation );
							const QString path = st->outDir + QLatin1String( "/" ) + QLatin1String( leaf );
							return g2.width() > 400 && g2.save( path ) ? path : QString();
						};
						const QString offShot = shotAt( false, "transport_2x_off.png" );
						const QString onShot = shotAt( true, "transport_2x_on.png" );
						if ( pose ) pose->setChecked( pose0 );
						if ( ak ) ak->setChecked( ak0 );
						qApp->processEvents();
						check( *st, QStringLiteral( "picture: (q) the two toggles off -> %1 and on -> %2, and both switches put back (%3, %4)" )
							   .arg( offShot, onShot ).arg( pose && pose->isChecked() == pose0 ).arg( ak && ak->isChecked() == ak0 ),
							   !offShot.isEmpty() && !onShot.isEmpty()
							   && pose && pose->isChecked() == pose0 && ak && ak->isChecked() == ak0 );
					}
				} else {
					skip( *st, "no WW_ANIMWS_OUT: the 1:1 and 2:1 pictures of (q) not saved" );
				}
			}

			/* ---- (o) THE ANIMATIONS LIST: bungo's ruling 5 of 2026-09-12.
			 *
			 * Verbatim: "For these animations, I should be able to use a
			 * shortcut to delete, copy, paste, etc. them, same goes with
			 * reordering with a drag and drop, same goes with right clicking
			 * and selecting each such option."
			 *
			 * The fixture is one clip, so the gate MAKES its three clips with
			 * the duplicate it is testing -- if duplicate is broken nothing
			 * after it can pass, which is the right way round. After every
			 * action the rows are read back by name, in order, from the list
			 * the eye sees AND from the scene's own animations list, so a row
			 * that moved only on screen would fail.
			 *
			 * TWO THINGS CANNOT BE DRIVEN FROM INSIDE THE APPLICATION, and are
			 * measured in halves instead of pretended:
			 *  - a key press does not reach a QShortcut unless it comes from
			 *    the platform, so the gate asserts each shortcut's KEY (the
			 *    binding a hand would press) and then fires that shortcut's own
			 *    activated signal, which is the same connection;
			 *  - QDrag::exec() runs a nested platform loop that never returns
			 *    in a harness, so the rows are moved the way the view's
			 *    internal move leaves them and the Drop event is delivered to
			 *    the viewport -- everything of ours (the filter, the order
			 *    push, the hub, the scene list) runs for real.
			 */
			{
				/* THE LIST REBUILDS 50 ms LATE (measured by lane UINOTES1b,
				 * 2026-09-12). Everything that goes through the hub -- delete,
				 * cut, rename, and SELECTING a row, which activates it --
				 * finishes in WwHkxAnimHub::clipsChanged, and
				 * AnimWorkspace::clipsChanged() ends with refreshLater(): a
				 * 50 ms single-shot timer (src/animworkspace.cpp:261-264,
				 * 1192-1241). A single processEvents() cannot see past it, so
				 * a gate that reads the rows straight after the action reads
				 * the OLD rows. Paste and Duplicate call refresh() themselves,
				 * which is exactly why those two used to pass and Cut, Del and
				 * rename used to fail. settle() runs the event loop for 120 ms
				 * -- the same wait a hand makes without noticing -- so the gate
				 * reads what the eye would see. */
				auto settle = [&]() {
					QEventLoop loop;
					QTimer::singleShot( 120, &loop, &QEventLoop::quit );
					loop.exec();
					qApp->processEvents();
				};
				auto selectOnce = [&]( const QString & name ) {
					for ( int i = 0; i < list->count(); i++ )
						if ( list->item( i )->data( Qt::UserRole ).toString() == name ) {
							list->clearSelection();
							list->setCurrentRow( i );
							list->item( i )->setSelected( true );
							return true;
						}
					return false;
				};
				// selecting a row activates it, which starts that same 50 ms
				// rebuild; let it land and put the selection back on the row
				// the rebuild made, so the action lands on the row the eye sees
				auto selectRowNamed = [&]( const QString & name ) {
					if ( !selectOnce( name ) )
						return false;
					/* The selection activates the row, the activation schedules
					 * the 50 ms rebuild, and the rebuild keeps the current row
					 * -- with syncing on, so it starts nothing further. One
					 * wait is therefore enough, and selecting AGAIN afterwards
					 * would leave a rebuild pending that lands in the middle of
					 * whatever the caller does next. */
					settle();
					QListWidgetItem * cur = list->currentItem();
					return cur && cur->data( Qt::UserRole ).toString() == name && cur->isSelected();
				};
				auto rowNames = [&]() {
					QStringList out;
					for ( int i = 0; i < list->count(); i++ )
						out << list->item( i )->data( Qt::UserRole ).toString();
					return out;
				};
				auto clipNames = [&]() {
					QStringList out;
					for ( int i = 0; i < list->count(); i++ )
						if ( list->item( i )->data( Qt::UserRole + 1 ).toBool() )
							out << list->item( i )->data( Qt::UserRole ).toString();
					return out;
				};
				/* THE DOCK'S CLIP ROWS ARE NOT THE SCENE'S ANIMATIONS LIST.
				 * A file that loaded but is not a clip -- skeleton.hkx is the
				 * standard one, and the fixture ships it -- is shown as a
				 * refused row and is deliberately kept OUT of
				 * Scene::animGroups (src/hkxanimui.h, lines 20-33). So a
				 * row-for-row comparison of the two lists uses the rows that
				 * really have a clip behind them, which is what animGroups
				 * holds. A gate that compared all four rows against three
				 * names could never pass. */
				auto liveNames = [&]() {
					QStringList out;
					for ( const QString & n : clipNames() )
						if ( pb->find( n ) )
							out << n;
					return out;
				};
				auto fire = [&]( const char * name ) {
					auto * s = list->findChild<QShortcut *>( QString::fromLatin1( name ) );
					if ( s )
						QMetaObject::invokeMethod( s, "activated" );
					settle();
					return s != nullptr;
				};
				auto keyOf = [&]( const char * name ) {
					auto * s = list->findChild<QShortcut *>( QString::fromLatin1( name ) );
					return s ? s->key() : QKeySequence();
				};

				const QStringList names0 = rowNames();
				check( *st, QStringLiteral( "(o) the list can be worked in: extended selection %1, internal move %2, drop line %3, own menu %4" )
					   .arg( int( list->selectionMode() ) ).arg( int( list->dragDropMode() ) )
					   .arg( list->showDropIndicator() ).arg( int( list->contextMenuPolicy() ) ),
					   list->selectionMode() == QAbstractItemView::ExtendedSelection
					   && list->dragDropMode() == QAbstractItemView::InternalMove
					   && list->showDropIndicator() && list->contextMenuPolicy() == Qt::CustomContextMenu );
				check( *st, QStringLiteral( "(o) the keys are the ones he asked for: Del %1, X %2, copy %3, paste %4, cut %5, duplicate %6, rename %7, all %8, up %9, down %10" )
					   .arg( keyOf( "AnimWsListDelete" ).toString(), keyOf( "AnimWsListDeleteX" ).toString(),
							 keyOf( "AnimWsListCopy" ).toString(), keyOf( "AnimWsListPaste" ).toString(),
							 keyOf( "AnimWsListCut" ).toString(), keyOf( "AnimWsListDuplicate" ).toString(),
							 keyOf( "AnimWsListRename" ).toString(), keyOf( "AnimWsListSelectAll" ).toString(),
							 keyOf( "AnimWsListMoveUp" ).toString(), keyOf( "AnimWsListMoveDown" ).toString() ),
					   keyOf( "AnimWsListDelete" ) == QKeySequence( Qt::Key_Delete )
					   && keyOf( "AnimWsListDeleteX" ) == QKeySequence( Qt::Key_X )
					   && keyOf( "AnimWsListCopy" ) == QKeySequence( QKeySequence::Copy )
					   && keyOf( "AnimWsListPaste" ) == QKeySequence( QKeySequence::Paste )
					   && keyOf( "AnimWsListCut" ) == QKeySequence( QKeySequence::Cut )
					   && keyOf( "AnimWsListDuplicate" ) == QKeySequence( Qt::SHIFT | Qt::Key_D )
					   && keyOf( "AnimWsListRename" ) == QKeySequence( Qt::Key_F2 )
					   && keyOf( "AnimWsListSelectAll" ) == QKeySequence( QKeySequence::SelectAll )
					   && keyOf( "AnimWsListMoveUp" ) == QKeySequence( Qt::CTRL | Qt::Key_Up )
					   && keyOf( "AnimWsListMoveDown" ) == QKeySequence( Qt::CTRL | Qt::Key_Down ) );

				// ---- three clips, made with Shift+D on the one the fixture gave us
				selectRowNamed( entry );
				qApp->processEvents();
				fire( "AnimWsListDuplicate" );
				const QStringList after1 = clipNames();
				selectRowNamed( entry );
				qApp->processEvents();
				fire( "AnimWsListDuplicate" );
				const QStringList three = clipNames();
				check( *st, QStringLiteral( "(o) Shift+D twice makes three clips, each copy right under its source: %1" ).arg( three.join( QStringLiteral( " | " ) ) ),
					   after1.count() == names0.count() + 1 && three.count() == names0.count() + 2
					   && three.value( 0 ) == entry && three.value( 1 ).startsWith( entry ) && three.value( 2 ).startsWith( entry )
					   && three.value( 1 ) != three.value( 2 ) );
				const QString copyA = three.value( 1 ), copyB = three.value( 2 );
				check( *st, QStringLiteral( "(o) a copy is a whole clip, not a reference: %1 has %2 frames" ).arg( copyA ).arg( pb->find( copyA ) ? pb->find( copyA )->clip.numFrames : -1 ),
					   pb->find( copyA ) && pb->find( copyA )->clip.numFrames == 93 );
				const QStringList threeLive = liveNames();
				check( *st, QStringLiteral( "(o) the scene's own animations list carries the same three, in the same order: %1 (the rows with a clip behind them: %2)" )
				       .arg( sc->animGroups.join( QStringLiteral( " | " ) ), threeLive.join( QStringLiteral( " | " ) ) ),
				       sc && threeLive.count() >= 3 && sc->animGroups.mid( sc->animGroups.count() - threeLive.count() ) == threeLive );

				/* ---- Ctrl+A, read at three moments, because a rebuild keeps
				 * only the current row: straight after the key, after one pump
				 * of the event loop, and after the 50 ms rebuild has landed.
				 * The check is on the FIRST number -- what the key itself did.
				 * The other two are printed, so a selection that is eaten
				 * afterwards shows up as a number instead of an opinion. */
				{
					settle();
					auto * sa = list->findChild<QShortcut *>( QStringLiteral( "AnimWsListSelectAll" ) );
					int selNow = -1, selPumped = -1, selSettled = -1;
					if ( sa ) {
						QMetaObject::invokeMethod( sa, "activated" );
						selNow = list->selectedItems().count();
						qApp->processEvents();
						selPumped = list->selectedItems().count();
						settle();
						selSettled = list->selectedItems().count();
					}
					/* All THREE numbers are the check now (lane UINOTES2,
					 * 2026-09-12). It used to check the first alone and print the
					 * other two, which was right while the defect was being
					 * measured: the key's own effect is one fact and what eats the
					 * selection afterwards is another. Now that both places are
					 * fixed -- the driven re-entry AND the rebuild -- a selection
					 * that survives the key and dies 50 ms later is a failure, not a
					 * printed number. */
					check( *st, QStringLiteral( "(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)" )
					       .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),
					       sa && selNow == list->count() && selPumped == list->count()
					       && selSettled == list->count() && list->count() >= 3 );
					/* WHERE DOES THE SELECTION GO? The same selectAll(), with
					 * the list's own signals blocked so nothing it drives can
					 * run. Four here and one above means the rows ARE selected
					 * and something the selection drives takes them away again;
					 * one here means selectAll() itself only took one row. */
					list->blockSignals( true );
					list->selectAll();
					list->blockSignals( false );
					say( *st, QStringLiteral( "  (o) Ctrl+A with the list's signals blocked: %1 of %2 selected, current row %3, selection mode %4" )
					     .arg( list->selectedItems().count() ).arg( list->count() )
					     .arg( list->currentRow() ).arg( int( list->selectionMode() ) ) );
				}

				// ---- Move down / Move up, on the first clip alone
				selectRowNamed( entry );
				qApp->processEvents();
				fire( "AnimWsListMoveDown" );
				const QStringList down = clipNames();
				check( *st, QStringLiteral( "(o) Ctrl+Down moves it one row down: %1" ).arg( down.join( QStringLiteral( " | " ) ) ),
					   down.value( 0 ) == copyA && down.value( 1 ) == entry && down.value( 2 ) == copyB );
				fire( "AnimWsListMoveUp" );
				const QStringList up = clipNames();
				check( *st, QStringLiteral( "(o) Ctrl+Up puts it back: %1" ).arg( up.join( QStringLiteral( " | " ) ) ), up == three );
				const QStringList upLive = liveNames();
				check( *st, QStringLiteral( "(o) the scene's list followed the move as well: %1 (the rows with a clip behind them: %2)" )
				       .arg( sc->animGroups.join( QStringLiteral( " | " ) ), upLive.join( QStringLiteral( " | " ) ) ),
				       sc && upLive == threeLive && sc->animGroups.mid( sc->animGroups.count() - upLive.count() ) == upLive );

				// ---- Ctrl+C then Ctrl+V (a copy lands after the selected row)
				selectRowNamed( entry );
				qApp->processEvents();
				fire( "AnimWsListCopy" );
				fire( "AnimWsListPaste" );
				const QStringList pasted = clipNames();
				check( *st, QStringLiteral( "(o) Ctrl+C then Ctrl+V adds one, right after the selected row: %1" ).arg( pasted.join( QStringLiteral( " | " ) ) ),
					   pasted.count() == three.count() + 1 && pasted.value( 0 ) == entry
					   && pasted.value( 1 ) != copyA && pasted.value( 2 ) == copyA );
				const QString pastedName = pasted.value( 1 );

				// ---- Ctrl+X takes it away and keeps it; Ctrl+V brings it back
				selectRowNamed( pastedName );
				qApp->processEvents();
				fire( "AnimWsListCut" );
				const QStringList cut = clipNames();
				check( *st, QStringLiteral( "(o) Ctrl+X removes the row: %1" ).arg( cut.join( QStringLiteral( " | " ) ) ),
					   cut == three && !cut.contains( pastedName ) );
				selectRowNamed( entry );
				qApp->processEvents();
				fire( "AnimWsListPaste" );
				const QStringList back = clipNames();
				check( *st, QStringLiteral( "(o) ...and Ctrl+V puts what was cut back: %1" ).arg( back.join( QStringLiteral( " | " ) ) ),
					   back.count() == three.count() + 1 && back.value( 1 ).startsWith( entry ) );
				const QString cutBack = back.value( 1 );

				// ---- Del takes the one that is selected, and only that one
				selectRowNamed( cutBack );
				qApp->processEvents();
				fire( "AnimWsListDelete" );
				const QStringList afterDel = clipNames();
				check( *st, QStringLiteral( "(o) Del removes the selected row and nothing else: %1" ).arg( afterDel.join( QStringLiteral( " | " ) ) ),
					   afterDel == three );

				// ---- F2 renames in place, and the edited document follows the name
				selectRowNamed( copyB );
				qApp->processEvents();
				const int framesBefore = ws->document() ? ws->document()->numFrames() : -1;
				fire( "AnimWsListRename" );
				auto * ed = list->findChild<QLineEdit *>();
				check( *st, QStringLiteral( "(o) F2 opens an editor on the row, holding its name: '%1'" ).arg( ed ? ed->text() : QString() ),
					   ed && ed->text() == copyB );
				const QString renamed = QStringLiteral( "SlideTest" );
				if ( ed ) {
					ed->setText( renamed );
					QKeyEvent ret( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
					QApplication::sendEvent( ed, &ret );
					settle();   // the rename goes through the hub: same 50 ms rebuild
				}
				const QStringList namedNow = clipNames();
				check( *st, QStringLiteral( "(o) the row, the clip and the scene all carry the new name: %1" ).arg( namedNow.join( QStringLiteral( " | " ) ) ),
					   namedNow.contains( renamed ) && !namedNow.contains( copyB )
					   && pb->has( renamed ) && sc && sc->animGroups.contains( renamed ) );
				selectRowNamed( renamed );
				qApp->processEvents();
				check( *st, QStringLiteral( "(o) the renamed row still opens its clip: %1 frames (was %2)" ).arg( ws->document() ? ws->document()->numFrames() : -1 ).arg( framesBefore ),
					   ws->document() && ws->document()->numFrames() == 93 );

				// ---- a double-click opens the same editor
				{
					QListWidgetItem * it = nullptr;
					for ( int i = 0; i < list->count(); i++ )
						if ( list->item( i )->data( Qt::UserRole ).toString() == renamed )
							it = list->item( i );
					QMetaObject::invokeMethod( list, "itemDoubleClicked", Q_ARG( QListWidgetItem *, it ) );
					qApp->processEvents();
					auto * ed2 = list->findChild<QLineEdit *>();
					check( *st, QStringLiteral( "(o) a double-click opens the same rename editor: '%1'" ).arg( ed2 ? ed2->text() : QString() ),
						   ed2 && ed2->text() == renamed );
					if ( ed2 ) {
						QKeyEvent esc( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
						QApplication::sendEvent( ed2, &esc );
						settle();
					}
					check( *st, QStringLiteral( "(o) Escape leaves the name alone: %1" ).arg( clipNames().join( QStringLiteral( " | " ) ) ),
						   clipNames().contains( renamed ) );
				}

				// ---- the right-click menu: every entry, a picture, and one of them driven
				{
					QStringList mTexts;
					QString shotPath;
					QTimer::singleShot( 0, qApp, [&mTexts, &shotPath, st]() {
						auto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );
						if ( !m )
							return;
						for ( QAction * a : m->actions() )
							if ( !a->isSeparator() )
								mTexts << a->text();
						if ( !st->outDir.isEmpty() ) {
							const QPixmap pm = m->grab();
							shotPath = st->outDir + QStringLiteral( "/animws_list_menu.png" );
							if ( !pm.save( shotPath ) )
								shotPath.clear();
						}
						m->close();
					} );
					QPoint mp( 10, 10 );
					for ( int i = 0; i < list->count(); i++ )
						if ( list->item( i )->data( Qt::UserRole ).toString() == entry )
							mp = list->visualItemRect( list->item( i ) ).center();
					QContextMenuEvent cme( QContextMenuEvent::Mouse, mp, list->viewport()->mapToGlobal( mp ) );
					QApplication::sendEvent( list->viewport(), &cme );
					qApp->processEvents();
					if ( mTexts.isEmpty() ) {
						// some styles deliver it to the widget itself
						QTimer::singleShot( 0, qApp, [&mTexts]() {
							auto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );
							if ( !m )
								return;
							for ( QAction * a : m->actions() )
								if ( !a->isSeparator() )
									mTexts << a->text();
							m->close();
						} );
						QContextMenuEvent cme2( QContextMenuEvent::Mouse, mp, list->mapToGlobal( mp ) );
						QApplication::sendEvent( list, &cme2 );
						qApp->processEvents();
					}
					const QStringList want{ QStringLiteral( "Rename" ), QStringLiteral( "Duplicate" ), QStringLiteral( "Copy" ),
											QStringLiteral( "Cut" ), QStringLiteral( "Paste" ), QStringLiteral( "Move up" ),
											QStringLiteral( "Move down" ), QStringLiteral( "Delete" ), QStringLiteral( "Select all" ) };
					QStringList missing;
					for ( const QString & w : want )
						if ( !mTexts.contains( w ) )
							missing << w;
					check( *st, QStringLiteral( "(o) the right-click menu offers every action by name (%1 entries; missing: %2)" )
						   .arg( mTexts.count() ).arg( missing.isEmpty() ? QStringLiteral( "none" ) : missing.join( QStringLiteral( ", " ) ) ),
						   missing.isEmpty() );
					check( *st, QStringLiteral( "(o) a picture of the menu was taken: %1" ).arg( shotPath.isEmpty() ? QStringLiteral( "no WW_ANIMWS_OUT" ) : shotPath ),
						   !st->outDir.isEmpty() ? !shotPath.isEmpty() : true );

					// and one entry actually driven from the menu, not from the slot
					const QStringList beforeMenuMove = clipNames();
					selectRowNamed( entry );
					qApp->processEvents();
					QTimer::singleShot( 0, qApp, []() {
						auto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );
						if ( !m )
							return;
						QAction * hit = nullptr;
						for ( QAction * a : m->actions() )
							if ( !a->isSeparator() && a->text() == QStringLiteral( "Move down" ) )
								hit = a;
						m->close();
						if ( hit )
							hit->trigger();
					} );
					QContextMenuEvent cme3( QContextMenuEvent::Mouse, mp, list->viewport()->mapToGlobal( mp ) );
					QApplication::sendEvent( list->viewport(), &cme3 );
					settle();
					const QStringList afterMenuMove = clipNames();
					check( *st, QStringLiteral( "(o) 'Move down' chosen in the menu moves the row: %1 -> %2" )
						   .arg( beforeMenuMove.join( QStringLiteral( " | " ) ), afterMenuMove.join( QStringLiteral( " | " ) ) ),
						   afterMenuMove.value( 1 ) == entry && afterMenuMove != beforeMenuMove );
					// put it back
					selectRowNamed( entry );
					qApp->processEvents();
					fire( "AnimWsListMoveUp" );
				}

				// ---- the drag-and-drop reorder
				{
					const QStringList beforeDrop = clipNames();
					int src = -1, dst = -1;
					for ( int i = 0; i < list->count(); i++ ) {
						if ( list->item( i )->data( Qt::UserRole ).toString() == beforeDrop.value( 0 ) )
							src = i;
						if ( list->item( i )->data( Qt::UserRole ).toString() == beforeDrop.value( 2 ) )
							dst = i;
					}
					check( *st, QStringLiteral( "(o floor) the rows are draggable and the view is in internal-move mode: flags %1" )
						   .arg( src >= 0 ? int( list->item( src )->flags() ) : -1 ),
						   src >= 0 && dst >= 0 && ( list->item( src )->flags() & Qt::ItemIsDragEnabled )
						   && list->dragDropMode() == QAbstractItemView::InternalMove );
					if ( src >= 0 && dst >= 0 ) {
						/* Does the Drop event arrive at all? A filter of the
						 * gate's own on the same viewport counts it. The dock's
						 * filter (animworkspace.cpp:1577-1584) queues the commit
						 * from exactly this event, so "seen 1, order unchanged"
						 * and "seen 0" are two different faults and this tells
						 * them apart. */
						struct DropSpy : public QObject
						{
							int seen = 0;
							bool eventFilter( QObject *, QEvent * e ) override
							{
								if ( e->type() == QEvent::Drop )
									seen++;
								return false;
							}
						};
						DropSpy spy;
						list->viewport()->installEventFilter( &spy );
						QListWidgetItem * moved = list->takeItem( src );
						list->insertItem( dst, moved );
						say( *st, QStringLiteral( "  (o) the drop, step 1 -- the rows after the fake move: %1  |  playback: %2" )
						     .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
						QMimeData md;
						QDropEvent de( QPointF( 8, list->visualItemRect( moved ).center().y() ), Qt::MoveAction,
						               &md, Qt::LeftButton, Qt::NoModifier, QEvent::Drop );
						QApplication::sendEvent( list->viewport(), &de );
						say( *st, QStringLiteral( "  (o) the drop, step 2 -- straight after the Drop event (the gate's own filter saw %1 Drop event(s), and the event was %2): %3  |  playback: %4" )
						     .arg( spy.seen ).arg( de.isAccepted() ? QStringLiteral( "accepted" ) : QStringLiteral( "ignored" ) )
						     .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
						list->viewport()->removeEventFilter( &spy );
						qApp->processEvents();
						say( *st, QStringLiteral( "  (o) the drop, step 3 -- after one pump (the queued commit runs here): %1  |  playback: %2" )
						     .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
						settle();
						const QStringList afterDrop = clipNames();
						say( *st, QStringLiteral( "  (o) the drop, step 4 -- after the list rebuilds: %1  |  playback: %2" )
						     .arg( afterDrop.join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
						const QStringList afterLive = liveNames();
						/* THE LAST INCH OF A DROP IS NOT A GATE'S TO DRIVE.
						 * The numbers above say it: the gate's own filter, on
						 * the very same viewport, counted 0 Drop events and the
						 * event came back ignored -- a QDropEvent handed to
						 * QApplication::sendEvent is not delivered inside the
						 * application, so the dock's filter
						 * (animworkspace.cpp:1577-1584) never sees the drop it
						 * queues the commit from. Nothing of the dock's is
						 * skipped by that: the rows are moved the way the
						 * view's internal move leaves them, and the slot the
						 * filter queues is then called by name, so the order
						 * push, the hub, the playback and the scene's list all
						 * run for real and are checked below. Owed to bungo,
						 * and written down as owed: one drag of a row with the
						 * mouse, to prove Qt delivers the Drop in his hands. */
						say( *st, QStringLiteral( "  (o) the Drop event itself cannot be delivered from inside the application (the gate's own filter saw %1); the half below is the dock's own, driven through the slot that filter queues" ).arg( spy.seen ) );
						const QStringList rows2 = clipNames();
						const QStringList live2 = liveNames();
						int src2 = -1, dst2 = -1;
						for ( int i = 0; i < list->count(); i++ ) {
							if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 0 ) )
								src2 = i;
							if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 2 ) )
								dst2 = i;
						}
						if ( src2 >= 0 && dst2 >= 0 ) {
							QListWidgetItem * m2 = list->takeItem( src2 );
							list->insertItem( dst2, m2 );
							const QStringList movedRows = clipNames();
							const bool called = QMetaObject::invokeMethod( ws, "commitListOrder" );
							settle();
							const QStringList afterCommit = clipNames();
							const QStringList liveCommit = liveNames();
							check( *st, QStringLiteral( "(o) a row moved and the drop's own commit run: the clips themselves are in the new order: %1 -> %2 (the rows with a clip behind them: %3; the dock says '%4')" )
							       .arg( rows2.join( QStringLiteral( " | " ) ), afterCommit.join( QStringLiteral( " | " ) ),
							             liveCommit.join( QStringLiteral( " | " ) ), ws->noteText() ),
							       called && afterCommit.count() == rows2.count() && liveCommit != live2
							       && liveCommit.count() > 0
							       && liveCommit == pb->names().mid( pb->names().count() - liveCommit.count() )
							       && liveCommit == movedRows.mid( 0, liveCommit.count() ) );
							check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1 (the rows with a clip behind them: %2)" )
							       .arg( sc->animGroups.join( QStringLiteral( " | " ) ), liveCommit.join( QStringLiteral( " | " ) ) ),
							       sc && sc->animGroups.mid( sc->animGroups.count() - liveCommit.count() ) == liveCommit );
						}
					}
				}

				/* ---- leave the fixture as the gates after this one expect it.
				 * "As it was" is names0, not the one entry: the fixture also
				 * ships the refused skeleton row, and unloading THAT left the
				 * list one row short and the gates after this one looking at a
				 * fixture this gate had quietly changed. */
				for ( const QString & n : clipNames() )
					if ( !names0.contains( n ) )
						WwHkxAnimHub::instance()->unload( ogl, n );
				ws->refresh();
				settle();
				selectRowNamed( entry );
				qApp->processEvents();
				check( *st, QStringLiteral( "(o) the list is back to what it was: %1" ).arg( rowNames().join( QStringLiteral( " | " ) ) ),
					   rowNames() == names0 && ws->document() && ws->document()->numFrames() == 93 );
			}

			// ---- (i) a NIF sequence still plays and its rows still write the block
			/* GATE (j) HAS NEVER RUN ON THIS MACHINE (found by lane UI6,
			 * 2026-09-11). It lived inside the sequence-NIF branch below, and
			 * the fixture that branch is given -- 10mmPistol.nif -- has no
			 * NiControllerSequence, so every run since lane HKXEDIT2 has taken
			 * the SKIP and finished without asking a single panel-style
			 * question. The scrub-field count bungo ruled on was among them.
			 * It is a lambda now and BOTH branches call it. */
			auto panelStyle = [st, ws]() {
						/* Ruling 6a put these rows in the right-side panel, and
						   the panel remembers whether it was open. A harness
						   FORCES the state it measures, never inherits it. */
						ws->setSidePanelOpen( true );
						qApp->processEvents();
						/* THE 1:1 OVERVIEW of the finished dock, side panel open --
						   bungo asked for one picture of the whole thing, not only
						   the crops that answer each ruling. Taken here because this
						   is the last thing the gate does, so it shows the dock after
						   every gate has put its fixture back. */
						if ( !st->outDir.isEmpty() ) {
							const QPixmap dp = ws->grab();
							const QString ds = st->outDir + QStringLiteral( "/dock_overview.png" );
							check( *st, QStringLiteral( "picture: the finished dock at 1:1, %1x%2 -> %3" ).arg( dp.width() ).arg( dp.height() ).arg( ds ),
								   dp.width() > 400 && dp.height() > 80 && dp.save( ds ) );
						}
						// ---- (j) panel style
						{
							int unstamped = 0, fields = 0;
							for ( QAbstractSpinBox * s : ws->findChildren<QAbstractSpinBox *>() ) {
								fields++;
								if ( !s->property( "wwScrubbed" ).toBool() )
									unstamped++;
							}
							check( *st, QStringLiteral( "(j) number fields carry the scrub stamp: %1 unstamped of %2 (>= 6)" ).arg( unstamped ).arg( fields ), unstamped == 0 && fields >= 6 );
							/* ITS FLOOR (lane UI6): a check that has only ever
							 * counted zero proves nothing. Put ONE un-stamped
							 * spin box in the dock, ask the same question, and
							 * watch the count rise -- then take it away. */
							{
								auto * plain = new QDoubleSpinBox( ws );
								plain->setObjectName( QStringLiteral( "WwUi6FloorSpin" ) );
								qApp->processEvents();
								int u2 = 0, f2 = 0;
								for ( QAbstractSpinBox * s : ws->findChildren<QAbstractSpinBox *>() ) {
									f2++;
									if ( !s->property( "wwScrubbed" ).toBool() )
										u2++;
								}
								check( *st, QStringLiteral( "(j floor) one un-stamped field IS seen: %1 unstamped of %2" ).arg( u2 ).arg( f2 ), u2 == unstamped + 1 && f2 == fields + 1 );
								delete plain;
								qApp->processEvents();
							}
							/* NO STEPPER IS CLIPPED TO A SLIVER (lane UI6).
							 * bungo, 2026-09-10 21:1x, over the old dock's
							 * 0.050 and 0.1000 fields: "Do you see it?" -- Qt's
							 * up/down buttons squeezed to a couple of pixels. A
							 * scrub field has no Qt stepper at all (it draws its
							 * own arrows and sets NoButtons), so the rule is:
							 * either the field has no stepper, or the stepper it
							 * has is inside its own rect and big enough to hit. */
							{
								int noStepper = 0, clipped = 0, all = 0;
								QString worst;
								for ( QAbstractSpinBox * s : ws->findChildren<QAbstractSpinBox *>() ) {
									all++;
									if ( s->buttonSymbols() == QAbstractSpinBox::NoButtons ) {
										noStepper++;
										continue;
									}
									QStyleOptionSpinBox opt;
									opt.initFrom( s );
									opt.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown;
									const QRect up = s->style()->subControlRect( QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxUp, s );
									if ( !s->rect().contains( up ) || up.width() < 8 || up.height() < 4 ) {
										clipped++;
										worst = QStringLiteral( "%1 (%2x%3 in %4x%5)" ).arg( s->objectName() ).arg( up.width() ).arg( up.height() ).arg( s->width() ).arg( s->height() );
									}
								}
								say( *st, QStringLiteral( "  (j) %1 number field(s): %2 are scrub fields with no Qt stepper to clip, %3 clipped%4" ).arg( all ).arg( noStepper ).arg( clipped ).arg( worst.isEmpty() ? QString() : QStringLiteral( " -- worst " ) + worst ) );
								check( *st, QStringLiteral( "(j) no number field has a stepper clipped to a sliver (%1 clipped of %2)" ).arg( clipped ).arg( all ), all >= 6 && clipped == 0 );
								/* THE FLOOR, reproducing what bungo saw: a bare
								 * spin box WITH Qt's buttons, squeezed the way
								 * the old strip squeezed its two. The same
								 * predicate must call it clipped. */
								{
									auto * sliver = new QDoubleSpinBox( ws );
									sliver->setButtonSymbols( QAbstractSpinBox::UpDownArrows );
									sliver->setFixedSize( 26, 9 );
									qApp->processEvents();
									QStyleOptionSpinBox opt;
									opt.initFrom( sliver );
									opt.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown;
									const QRect up = sliver->style()->subControlRect( QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxUp, sliver );
									const bool bad = !sliver->rect().contains( up ) || up.width() < 8 || up.height() < 4;
									say( *st, QStringLiteral( "  (j floor) a bare 26x9 spin box's up-arrow reads %1x%2 at %3,%4 inside %5x%6" ).arg( up.width() ).arg( up.height() ).arg( up.x() ).arg( up.y() ).arg( sliver->width() ).arg( sliver->height() ) );
									check( *st, QStringLiteral( "(j floor) the SAME test calls a squeezed bare spin box clipped" ), bad );
									delete sliver;
									qApp->processEvents();
								}
							}
							check( *st, QStringLiteral( "(j) no QGroupBox: %1" ).arg( ws->findChildren<QGroupBox *>().count() ), ws->findChildren<QGroupBox *>().isEmpty() );
							int unstyled = 0, combos = 0;
							for ( QComboBox * c : ws->findChildren<QComboBox *>() ) {
								combos++;
								if ( !c->styleSheet().contains( QStringLiteral( "drop-down" ) ) )
									unstyled++;
							}
							check( *st, QStringLiteral( "(j) selectors matched to the fields: %1 unstyled of %2 (>= 3)" ).arg( unstyled ).arg( combos ), unstyled == 0 && combos >= 3 );
							/* Qt builds buttons of its own inside its own
							 * widgets -- qt_menubar_ext_button is the menu
							 * bar's overflow chevron -- and they are not ours
							 * to write a tip on: the count is over OUR buttons,
							 * and says so. */
							auto oursToTip = []( QAbstractButton * b ) {
								return !b->objectName().startsWith( QStringLiteral( "qt_" ) );
							};
							int untipped = 0, buttons = 0;
							QString untippedName;
							for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() ) {
								if ( !oursToTip( b ) )
									continue;
								buttons++;
								if ( b->toolTip().isEmpty() ) {
									untipped++;
									untippedName = b->objectName();
								}
							}
							/* The floor was >= 20 while fifteen push buttons sat
							   at the bottom of the dock. Ruling 6 took those
							   fifteen into the header menus, so the dock's
							   buttons are now the transport's (eight plus the
							   two gizmo toggles), the list bar's three and the
							   side-panel toggle: the floor is lowered to 12 and
							   says so rather than passing on an empty count. */
							check( *st, QStringLiteral( "(j) every button tipped: %1 untipped of %2 of ours (Qt's own qt_* buttons are not ours to tip) (%3)" ).arg( untipped ).arg( buttons ).arg( untippedName ), untipped == 0 && buttons >= 12 );
							// FLOOR: blank the first button that HAS a tip and watch the count rise
							{
								QAbstractButton * victim = nullptr;
								for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
									if ( oursToTip( b ) && !b->toolTip().isEmpty() ) { victim = b; break; }
								const QString keep = victim ? victim->toolTip() : QString();
								if ( victim ) victim->setToolTip( QString() );
								int u2 = 0;
								for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
									if ( oursToTip( b ) && b->toolTip().isEmpty() ) u2++;
								check( *st, QStringLiteral( "(j floor) blanking one tip is seen: %1" ).arg( u2 ), u2 == untipped + 1 );
								if ( victim ) victim->setToolTip( keep );
							}
							auto * split = widget<QSplitter>( ws, "AnimWsSplitter" );
							auto * note = widget<QLabel>( ws, "AnimWsNote" );
							/* ruling 6: the action bar is gone; the thing that
							   must stay pinned outside the splitter is the
							   header that carries the menus. */
							auto * bar = widget<QWidget>( ws, "AnimWsHeader" );
							auto isInside = []( QWidget * w, QWidget * container ) {
								for ( QWidget * p = w ? w->parentWidget() : nullptr; p; p = p->parentWidget() )
									if ( p == container ) return true;
								return false;
							};
							check( *st, "(j) the note and the header menu bar are pinned outside the splitter", split && note && bar && !isInside( note, split ) && !isInside( bar, split ) );
							check( *st, "(j) the settings live in a scroll area", widget<QScrollArea>( ws, "AnimWsSettings" ) != nullptr );
							// the wheel over the UNFOCUSED speed field leaves it
							auto * speed = widget<QDoubleSpinBox>( ws, "AnimWsSpeed" );
							if ( speed ) {
								const double v0 = speed->value();
								speed->clearFocus();
								QWheelEvent we( QPointF( 10, 10 ), speed->mapToGlobal( QPoint( 10, 10 ) ), QPoint( 0, 120 ), QPoint( 0, 120 ), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false );
								QApplication::sendEvent( speed, &we );
								check( *st, QStringLiteral( "(j) the wheel over the unfocused Speed field leaves it: %1 -> %2" ).arg( v0 ).arg( speed->value() ), speed->value() == v0 );
								say( *st, QStringLiteral( "     (the focused half cannot fire in an inactive harness window: hasFocus %1, window active %2)" ).arg( speed->hasFocus() ).arg( ws->window()->isActiveWindow() ) );
							}
							check( *st, "(j) the summary line is not empty and not a refusal", !ws->noteText().isEmpty() && !ws->noteIsRefusal() );
						}
			};

			if ( !st->seqNif.isEmpty() && QFile::exists( st->seqNif ) ) {
				NifModel seqModel;
				const bool loaded = seqModel.loadFromFile( st->seqNif );
				int seqs = 0;
				for ( int b = 0; loaded && b < seqModel.getBlockCount(); b++ )
					if ( seqModel.isNiBlock( seqModel.getBlockIndex( b ), QStringLiteral( "NiControllerSequence" ) ) ) seqs++;
				if ( !loaded || seqs == 0 ) {
					skip( *st, QStringLiteral( "(i) %1 has no NiControllerSequence to test with" ).arg( st->seqNif ) );
					panelStyle();
				} else {
					// open it in the window (the workspace follows the NIF)
					QString seqPath = st->seqNif;
					skope->openFile( seqPath );
					// the load is asynchronous; give it its 1.5 s
					QTimer::singleShot( 2500, skope, [skope, st, ws, ogl, grp, finish, panelStyle]() {
						NifModel * n2 = skope->getNifModel();
						ws->show();
						qApp->processEvents();
						ws->refresh();
						qApp->processEvents();
						auto * list2 = widget<QListWidget>( ws, "AnimWsClipList" );
						int seqRow = -1;
						QString seqName;
						for ( int i = 0; i < list2->count(); i++ ) {
							if ( !list2->item( i )->data( Qt::UserRole + 1 ).toBool() ) {
								seqRow = i;
								seqName = list2->item( i )->data( Qt::UserRole ).toString();
								break;
							}
						}
						check( *st, QStringLiteral( "(i) the NIF's sequence rows appear: first '%1' at row %2" ).arg( seqName ).arg( seqRow ), seqRow >= 0 );
						if ( seqRow >= 0 ) {
							list2->setCurrentRow( seqRow );
							qApp->processEvents();
							Scene * s2 = ogl->getScene();
							check( *st, QStringLiteral( "(i) selecting it drives the scene: animGroup '%1'" ).arg( s2 ? s2->animGroup : QString() ), s2 && s2->animGroup == seqName );
							ws->setSidePanelOpen( true );	// ruling 6a: the rows live in the panel now
							qApp->processEvents();
							check( *st, "(i) the Sequence rows are visible and the edit rows hidden", widget<QWidget>( ws, "AnimWsSequenceSection" )->isVisible() && !widget<QWidget>( ws, "AnimWsEditSection" )->isVisible() );
							const int seqBlock = list2->item( seqRow )->data( Qt::UserRole + 2 ).toInt();
							const QModelIndex iSeq = n2->getBlockIndex( seqBlock );
							const float stop0 = n2->get<float>( iSeq, "Stop Time" );
							auto * stopBox = widget<QDoubleSpinBox>( ws, "AnimWsStopTime" );
							check( *st, QStringLiteral( "(i) the Stop time row reads the block: %1 vs %2" ).arg( stopBox->value() ).arg( stop0 ), std::fabs( float( stopBox->value() ) - stop0 ) < 1e-5f );
							stopBox->setValue( double( stop0 ) + 0.5 );
							stopBox->editingFinished();
							qApp->processEvents();
							const float stop1 = n2->get<float>( iSeq, "Stop Time" );
							check( *st, QStringLiteral( "(i) editing the row wrote the block: %1 -> %2" ).arg( stop0 ).arg( stop1 ), std::fabs( stop1 - ( stop0 + 0.5f ) ) < 1e-5f );
							check( *st, "(i) the NIF's own undo stack holds it", n2->undoStack && n2->undoStack->count() > 0 && grp->stacks().contains( n2->undoStack ) );
							if ( n2->undoStack ) {
								n2->undoStack->undo();
								qApp->processEvents();
							}
							check( *st, QStringLiteral( "(i) Undo puts the block back: %1" ).arg( n2->get<float>( iSeq, "Stop Time" ) ), std::fabs( n2->get<float>( iSeq, "Stop Time" ) - stop0 ) < 1e-5f );
							// play: the transport's play asks the application, which flips aAnimPlay
							widget<QToolButton>( ws, "AnimWsPlay" )->click();
							qApp->processEvents();
							check( *st, QStringLiteral( "(i) Play starts the clock: play button checked %1" ).arg( widget<QToolButton>( ws, "AnimWsPlay" )->isChecked() ), widget<QToolButton>( ws, "AnimWsPlay" )->isChecked() );
							widget<QToolButton>( ws, "AnimWsStop" )->click();
							qApp->processEvents();
							auto * cycle = widget<QComboBox>( ws, "AnimWsCycleType" );
							const int ct0 = n2->get<int>( iSeq, "Cycle Type" );
							cycle->setCurrentIndex( ( ct0 + 1 ) % 3 );
							cycle->activated( cycle->currentIndex() );
							qApp->processEvents();
							check( *st, QStringLiteral( "(i) the Cycle type row wrote the block: %1 -> %2" ).arg( ct0 ).arg( n2->get<int>( iSeq, "Cycle Type" ) ), n2->get<int>( iSeq, "Cycle Type" ) == ( ct0 + 1 ) % 3 );
							if ( n2->undoStack )
								n2->undoStack->undo();
						}
						panelStyle();
						finish();
					} );
					return;
				}
			} else {
				skip( *st, QStringLiteral( "(i) no sequence NIF at '%1'" ).arg( st->seqNif ) );
				panelStyle();
			}
			finish();
		} );
	} );
}
