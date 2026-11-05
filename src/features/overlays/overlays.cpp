// MODULES1 overlays: the editor overlays GLView::paintGL draws after the frame (the PRTP / volume / air-zone /
// place-mode / RTDDGI views, paint helpers, previews, gizmo, wire and edit overlays, skeletons, origins).
// Moved from src/glview.cpp paintGL as one member, no behaviour change.
#include "glview.h"
#include "wwswitch.h"	// the WW_* switch store
#include "wwflyfix.h"	// FLYFIX1's free-camera rules (their old way is a test twin now)
#include "cellspeed.h"
#include "cellcambounds.h"	// the camera never shows an out-of-bounds or under-terrain view
#include "cellstream.h"		// the cells round the camera
#include "celltexahead.h"
#include "cellstreamtex.h"	// lane STREAMTEX1: the stream texture pool

#include "rdccapture.h"

#include "message.h"
#include "nifskope.h"
#include "cellclick.h"		// one call, in mouseReleaseEvent
#include "gl/controllers.h"
#include "gl/lookdevstage.h"
#include "features/shadows/sunshadow.h"
#include "features/shadows/shadowset.h"	// contact shadows
#include "features/near/cellcull.h"
#include "features/particles/glparticles.h"
#include "gl/renderer.h"
#include "features/exposure/cellhdr.h"
#include "features/post/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1)
#include "features/weather/cellprecip.h"	// lane WEATHERLOOK1 B
#include "features/post/gametaa.h"	// lane MOTION1: the game's temporal AA
#include "features/post/taainputs.h"	// lane TAAINPUTS1: velocity + reactive mask for a temporal resolve
#include "features/post/cellpost.h"	// lane GRASSMB1: the game's depth of field + motion blur
#include "gl/cellgrass.h"	// lane GRASSMB1: the grass wind's clock
#include "features/far-lod/impostorchunk.h"
#include "features/far-lod/cellfarlod.h"		// the far field's planes, sway and cards
#include "features/water/cellwater.h"	// far water held until its dry mask's lookup lands
#include "features/far-ground/landlodspec.h"		// the land-LOD blend centre's glide
#include "cellmodpick.h"	// "Mods differ from the far LOD bake" over the view
#include "features/far-lod/impostorpreviewtest.h"
#include "gl/glshape.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"
#include "model/undocommands.h"
#include "data/nifitem.h"
#include "nifsnapshot.h"
#include "features/gi/probesky.h"	// one call a frame
#include "features/air/cellairzone.h"	// the hand-placed volumes' overlay
#include "celledit.h"	// the cell view's place mode
#include "cellsurface.h"	// the surface pick's planes
#include "features/gi/rtddgicell.h"	// the ray-traced GI volume, one call a frame
#include "celldoorlive.h"	// one call a frame
#include "cellworldanim.h"	// the cell's world clock
#include "shortcutregistry.h"
#include "wwskin.h"
#include "features/near/cellqueue.h"
#include "spells/animationsetup.h"
#include "skeletontools.h"	// skeletonAnalyse: the Skeleton Manager's own classes
#include "ui/settingsdialog.h"
#include "ui/widgets/fileselect.h"
#include "fp32vec4.hpp"
#include "ui/widgets/filebrowser.h"
#include "qt5compat.hpp"
#include "spells/blocks.h"	// blockLink for Separate / Duplicate
#include "spells/simplify.h"	// tlSimplifyShapeToRatio for the Decimate operator

#include <memory>

#include <QApplication>
#include <QFontMetrics>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QInputDialog>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QElapsedTimer>
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QSurface>
#include <QTimer>
#include <QToolBar>
#include <QWindow>

#include <QBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>	// the GPU timer
#include "gl/celllights.h"	// the imagespace measure
#include "features/sky/atmosphere.h"	// the sky in the exposure's mean
#include "features/gi/cellprobeview.h"	// the Pass overlays
#include "features/gi/cellvolumeview.h"	// the Space stage's view
#include "features/reflections/cellssr.h"
#include "features/gi/cellssgi.h"
#include "features/air/cellvolair.h"
#include "features/reflections/cellreflprobe.h"
#include "features/shared/cellpasstime.h"
#include "gl/geokeepclock.h"
#include "features/far-ground/cellfarground.h"
#include "features/water/cellocean.h"
#include "features/water/cellunderwater.h"
#include "features/clouds/cloudvol.h"	// the volume clouds' depth test

void GLView::paintEditorOverlays( const Transform & viewTrans )
{
	// the PRTP band's Pass overlays (surfel tiles, lit probes, the picked probe's links)
	wwCellProbeViewDraw( scene, viewTrans );
	wwCellVolumeViewDraw( scene, viewTrans );   // rooms, shapes, thresholds; VOLVIEW2: in the PRTP Debug View
	wwAirZoneViewDraw( scene, viewTrans );	// the hand-placed volumes while the panel shows them
	cellEditFrame( this, scene, viewTrans );	// the place mode's probe, ghost and placed edits
	rtddgiCellViewDraw( scene, viewTrans );	// the Ray-traced GI probes row (states / irradiance)

	if ( !cleanView ) {	// Clean View hides the paint helpers (block kept at its old indent)
	// Selected-bone weight heatmap. Per-corner colours are supplied by the
	// Rigging Manager, while the viewport owns only an ephemeral triangle soup.
	if ( !riggingWeightPreviewSoup.isEmpty()
		&& riggingWeightPreviewSoup.size() == riggingWeightPreviewColors.size()
		&& !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		scene->loadModelViewMatrix( viewTrans );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
		scene->drawTriangles( riggingWeightPreviewSoup.constData(),
			size_t( riggingWeightPreviewSoup.size() ), riggingWeightPreviewColors.constData(), true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDisable( GL_BLEND );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// Blender-style Weight Paint selection mask. Unlike Edit Mode, Weight Paint must not cover the useful heat
	// colours with orange selection fill.
	const bool elementPaintMode = riggingWeightPaintMode || vertexPaintMode || segmentPaintMode;
	const int elementPaintTarget = riggingWeightPaintMode ? riggingWeightPaintTarget
		: vertexPaintMode ? vertexPaintTarget : segmentPaintTarget;
	if ( model && elementPaintMode && elementPaintTarget >= 0
		&& !pickedElems.isEmpty() && !scene->selecting ) {
		Shape * s = shapeForBlock( elementPaintTarget );
		QSet<int> selectedVerts = pickedVertexRefs().value( elementPaintTarget );
		if ( s && !s->isHidden() && !selectedVerts.isEmpty()
			&& !s->verts.isEmpty() && !s->triangles.isEmpty() ) {
			const float dpr = float( devicePixelRatioF() );
			const int nv = s->verts.size();
			const bool faceMask = bool( pickMode & 4 );
			const bool vertexMask = bool( pickMode & 1 );
			const bool edgeMask = bool( pickMode & 2 );
			const QSet<int> hiddenT = editHiddenTris.value( elementPaintTarget );

			QSet<int> selectedFaces;
			QSet<quint64> selectedEdges;
			auto edgeKey = []( int a, int b ) {
				return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
			};
			for ( const PickedElement & pe : pickedElems ) {
				if ( pe.shapeBlock != elementPaintTarget )
					continue;
				if ( pe.type == 3 )
					selectedFaces.insert( pe.e0 );
				else if ( pe.type == 2 )
					selectedEdges.insert( edgeKey( pe.e0, pe.e1 ) );
			}

			Transform wt = shapeRenderTrans( s );
			QVector<Vector3> worldVerts( nv );
			for ( int i = 0; i < nv; i++ )
				worldVerts[i] = wt * editVertexLocal( s, i );

			// In face mode a triangle is selected explicitly. Vertex and edge modes
			// use the actual paintable vertex set, allowing Blender's characteristic
			// soft grey transition across a triangle with a partly selected corner set.
			QVector<Vector3> maskSoup;
			QVector<FloatVector4> maskColors;
			maskSoup.reserve( s->triangles.size() * 3 );
			maskColors.reserve( s->triangles.size() * 3 );
			const FloatVector4 clearMask( 0.55f, 0.55f, 0.55f, 0.0f );
			const FloatVector4 greyMask( 0.55f, 0.55f, 0.55f, 0.68f );
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				const bool faceSelected = faceMask && selectedFaces.contains( ti );
				for ( int corner = 0; corner < 3; corner++ ) {
					const int vi = t[corner];
					maskSoup.append( worldVerts.at( vi ) );
					maskColors.append( faceMask
						? ( faceSelected ? clearMask : greyMask )
						: ( selectedVerts.contains( vi ) ? clearMask : greyMask ) );
				}
			}

			if ( !maskSoup.isEmpty() ) {
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				glDepthMask( GL_FALSE );
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				scene->loadModelViewMatrix( viewTrans );
				glEnable( GL_POLYGON_OFFSET_FILL );
				glPolygonOffset( -2.0f, -2.0f );
				scene->drawTriangles( maskSoup.constData(), size_t( maskSoup.size() ),
					maskColors.constData(), true );
				glDisable( GL_POLYGON_OFFSET_FILL );
				glDisable( GL_BLEND );
				glDepthMask( GL_TRUE );
				glDepthFunc( GL_LESS );
			}

			// Build the visible cage once. Face mode uses only the boundary between
			// selected and protected faces; vertex/edge modes retain Blender's
			// black cage with selected endpoints/edges changed to white.
			struct MaskEdge {
				int a = -1;
				int b = -1;
				int adjacent = 0;
				bool selectedSide = false;
				bool protectedSide = false;
			};
			QHash<quint64, MaskEdge> maskEdges;
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				const bool selected = selectedFaces.contains( ti );
				for ( int e = 0; e < 3; e++ ) {
					const int a = t[e], b = t[( e + 1 ) % 3];
					const quint64 key = edgeKey( a, b );
					MaskEdge & edge = maskEdges[key];
					edge.a = std::min( a, b );
					edge.b = std::max( a, b );
					edge.adjacent++;
					edge.selectedSide = edge.selectedSide || selected;
					edge.protectedSide = edge.protectedSide || !selected;
				}
			}

			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			scene->loadModelViewMatrix( viewTrans );
			if ( faceMask ) {
				QVector<Vector3> boundary;
				for ( auto it = maskEdges.constBegin(); it != maskEdges.constEnd(); ++it ) {
					const MaskEdge & edge = it.value();
					if ( edge.selectedSide && ( edge.protectedSide || edge.adjacent == 1 ) )
						boundary << worldVerts.at( edge.a ) << worldVerts.at( edge.b );
				}
				if ( !boundary.isEmpty() ) {
					glDisable( GL_DEPTH_TEST );
					scene->setGLLineWidth( 1.35f * dpr * wireWidthMul );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.95f );
					scene->drawLines( boundary.constData(), size_t( boundary.size() ), nullptr );
					glEnable( GL_DEPTH_TEST );
				}
			} else if ( vertexMask || edgeMask ) {
				const FloatVector4 black( 0.0f, 0.0f, 0.0f, 1.0f );
				const FloatVector4 white( 1.0f, 1.0f, 1.0f, 1.0f );
				QVector<Vector3> lines;
				QVector<FloatVector4> lineColors;
				lines.reserve( maskEdges.size() * 2 );
				lineColors.reserve( maskEdges.size() * 2 );
				for ( auto it = maskEdges.constBegin(); it != maskEdges.constEnd(); ++it ) {
					const MaskEdge & edge = it.value();
					lines << worldVerts.at( edge.a ) << worldVerts.at( edge.b );
					if ( edgeMask && selectedEdges.contains( it.key() ) )
						lineColors << white << white;
					else
						lineColors << ( selectedVerts.contains( edge.a ) ? white : black )
							<< ( selectedVerts.contains( edge.b ) ? white : black );
				}
				if ( !lines.isEmpty() ) {
					scene->setGLLineWidth( 1.0f * dpr * wireWidthMul );
					scene->drawLines( lines.constData(), size_t( lines.size() ), lineColors.constData() );
				}
				if ( vertexMask ) {
					QVector<Vector3> unselectedPoints, selectedPoints;
					for ( int vi = 0; vi < nv; vi++ )
						( selectedVerts.contains( vi ) ? selectedPoints : unselectedPoints )
							.append( worldVerts.at( vi ) );
					scene->setGLPointSize( vertexPointSize * dpr );
					if ( !unselectedPoints.isEmpty() ) {
						scene->setGLColor( black );
						scene->drawPoints( unselectedPoints.constData(), size_t( unselectedPoints.size() ) );
					}
					if ( !selectedPoints.isEmpty() ) {
						glDisable( GL_DEPTH_TEST );
						scene->setGLColor( white );
						scene->drawPoints( selectedPoints.constData(), size_t( selectedPoints.size() ) );
						glEnable( GL_DEPTH_TEST );
					}
				}
			}
			glDepthMask( GL_TRUE );
			glDepthFunc( GL_LESS );
		}
	}
	}	// end of the Clean View paint helpers

	// Other open NIF documents form a neutral, read-only scene assembly around the active document. They do
	// not participate in picking or saving.
	if ( !sessionDocumentPreviewSoup.isEmpty() && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		scene->loadModelViewMatrix( viewTrans );
		scene->setGLColor( 0.46f, 0.54f, 0.62f, 1.0f );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( 1.0f, 1.0f );
		const bool shaded =
			sessionDocumentPreviewColors.size() == sessionDocumentPreviewSoup.size();
		scene->drawTriangles( sessionDocumentPreviewSoup.constData(),
			size_t( sessionDocumentPreviewSoup.size() ),
			shaded ? sessionDocumentPreviewColors.constData() : nullptr, true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDepthFunc( GL_LESS );
	}

	// Ghosted documents, in a second pass after the opaque one.
	if ( !sessionDocumentGhostSoup.isEmpty() && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		scene->loadModelViewMatrix( viewTrans );
		const bool shaded =
			sessionDocumentGhostColors.size() == sessionDocumentGhostSoup.size();
		scene->drawTriangles( sessionDocumentGhostSoup.constData(),
			size_t( sessionDocumentGhostSoup.size() ),
			shaded ? sessionDocumentGhostColors.constData() : nullptr, true );
		glDisable( GL_BLEND );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// Rigging donor overlay: copied, read-only donor geometry in target model space. It is deliberately
	// outside Scene, so it cannot be picked, saved, skinned, or confused with target blocks.
	if ( !riggingDonorPreviewSoup.isEmpty() && !scene->selecting && !cleanView
		&& ( riggingDonorPreviewFilled || riggingDonorPreviewWireframe ) ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		if ( riggingDonorPreviewFilled ) {
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			scene->setGLColor( 0.08f, 0.66f, 1.0f, riggingDonorPreviewOpacity );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -2.0f, -2.0f );
			scene->drawTriangles( riggingDonorPreviewSoup.constData(),
				size_t( riggingDonorPreviewSoup.size() ), nullptr, true );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDisable( GL_BLEND );
		}
		if ( riggingDonorPreviewWireframe ) {
			scene->setGLColor( 0.18f, 0.82f, 1.0f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthHighlight * 0.8f );
			scene->drawTriangles( riggingDonorPreviewSoup.constData(),
				size_t( riggingDonorPreviewSoup.size() ), nullptr, false );
		}
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// Collision Manager live preview: translucent amber fill plus a bright
	// wire overlay. This is a world-space triangle soup owned by the viewport,
	// not a model block, so cancelling the operator is a zero-cost clear.
	if ( !collisionPreviewSoup.isEmpty() && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		// While the sim runs the preview is the SCENE, not a proposal, so it is drawn like unselected geometry
		// in Edit Mode -- near-black -- and orange is reserved for the bone actually being held.
		const bool simulating = physicsPreview.active();
		if ( simulating )
			scene->setGLColor( 0.09f, 0.09f, 0.10f, 0.55f );
		else
			scene->setGLColor( 1.0f, 0.58f, 0.08f, 0.28f );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
		scene->drawTriangles( collisionPreviewSoup.constData(), size_t( collisionPreviewSoup.size() ), nullptr, true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDepthMask( GL_TRUE );
		if ( simulating )
			scene->setGLColor( 0.16f, 0.16f, 0.18f, 1.0f );
		else
			scene->setGLColor( 1.0f, 0.72f, 0.16f, 1.0f );
		scene->setGLLineWidth( Settings::lineWidthHighlight );
		scene->drawTriangles( collisionPreviewSoup.constData(), size_t( collisionPreviewSoup.size() ), nullptr, false );
		glDisable( GL_BLEND );
	}

	// Physics Sim extras: the floor, and whatever has been shot at it.
	if ( physicsPreview.active() && !scene->selecting ) {
		/* Pinned bodies in a red-orange, the way a secondary selection reads in
		 * Edit Mode: related to the thing in hand, distinct from it.
		 */
		const QVector<Vector3> nailed = physicsPreview.pinnedSoup();
		if ( !nailed.isEmpty() ) {
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glDepthMask( GL_FALSE );
			scene->loadModelViewMatrix( viewTrans );
			scene->setGLColor( 0.85f, 0.28f, 0.10f, 0.35f );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -1.5f, -1.5f );
			scene->drawTriangles( nailed.constData(), size_t( nailed.size() ), nullptr, true );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDepthMask( GL_TRUE );
			scene->setGLColor( 0.95f, 0.40f, 0.14f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthHighlight );
			scene->drawTriangles( nailed.constData(), size_t( nailed.size() ), nullptr, false );
			glDisable( GL_BLEND );
		}

		// the bone in hand, in orange over the black rest
		const QVector<Vector3> held = physicsPreview.grabbedSoup();
		if ( !held.isEmpty() ) {
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glDepthMask( GL_FALSE );
			scene->loadModelViewMatrix( viewTrans );
			scene->setGLColor( 1.0f, 0.55f, 0.08f, 0.40f );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -2.0f, -2.0f );
			scene->drawTriangles( held.constData(), size_t( held.size() ), nullptr, true );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDepthMask( GL_TRUE );
			scene->setGLColor( 1.0f, 0.72f, 0.16f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthHighlight );
			scene->drawTriangles( held.constData(), size_t( held.size() ), nullptr, false );
			glDisable( GL_BLEND );
		}

		// The beam, and the point on the shape the hand actually has hold of.
		Vector3 grip, hand;
		if ( physicsPreview.grabBeam( grip, hand ) ) {
			const float pxScale = float( devicePixelRatioF() );
			glDisable( GL_DEPTH_TEST );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			scene->loadModelViewMatrix( viewTrans );

			const QVector<Vector3> beam { grip, hand };
			// the same orange as the bone in hand, so the two read as one object
			scene->setGLColor( 1.0f, 0.72f, 0.16f, 0.75f );
			scene->setGLLineWidth( 1.6f * pxScale );
			scene->drawLines( beam.constData(), size_t( beam.size() ), nullptr );

			// A cross at the grip rather than a dot: it is a POINT on a surface, and a filled marker large
			// enough to see would hide the very detail you are aiming at.
			const float s = std::max( 0.4f, cameraDistance() * 0.012f );
			QVector<Vector3> mark;
			mark << grip - Vector3( s, 0, 0 ) << grip + Vector3( s, 0, 0 )
				 << grip - Vector3( 0, s, 0 ) << grip + Vector3( 0, s, 0 )
				 << grip - Vector3( 0, 0, s ) << grip + Vector3( 0, 0, s );
			scene->setGLColor( 1.0f, 0.85f, 0.35f, 0.95f );
			scene->setGLLineWidth( 2.0f * pxScale );
			scene->drawLines( mark.constData(), size_t( mark.size() ), nullptr );

			// and where the hand is asking it to be, which is not the same place
			// whenever the rig cannot reach -- that gap IS the chain going taut
			QVector<Vector3> target;
			const float t = s * 0.7f;
			target << hand - Vector3( t, t, 0 ) << hand + Vector3( t, t, 0 )
				   << hand - Vector3( t, -t, 0 ) << hand + Vector3( t, -t, 0 );
			scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.6f );
			scene->setGLLineWidth( 1.2f * pxScale );
			scene->drawLines( target.constData(), size_t( target.size() ), nullptr );

			glDisable( GL_BLEND );
			glEnable( GL_DEPTH_TEST );
		}

		// bodies outside their joint limits, drawn over the rest in red
		const QVector<Vector3> bad = physicsPreview.limitSoup();
		if ( !bad.isEmpty() ) {
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glDepthMask( GL_FALSE );
			scene->loadModelViewMatrix( viewTrans );
			scene->setGLColor( 1.0f, 0.22f, 0.18f, 0.45f );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -2.0f, -2.0f );
			scene->drawTriangles( bad.constData(), size_t( bad.size() ), nullptr, true );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDepthMask( GL_TRUE );
			glDisable( GL_BLEND );
		}
		const QVector<Vector3> ground = physicsPreview.groundSoup();
		const QVector<PhysicsPreview::Shot> & shots = physicsPreview.shots();
		if ( !ground.isEmpty() || !shots.isEmpty() ) {
			const float pxScale = float( devicePixelRatioF() );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			scene->loadModelViewMatrix( viewTrans );

			if ( !ground.isEmpty() ) {
				glDepthMask( GL_FALSE );
				scene->setGLColor( 0.55f, 0.58f, 0.62f, 0.35f );
				scene->drawTriangles( ground.constData(), size_t( ground.size() ), nullptr, true );
				glDepthMask( GL_TRUE );
				scene->setGLColor( 0.75f, 0.78f, 0.82f, 0.8f );
				scene->setGLLineWidth( 1.2f * pxScale );
				scene->drawTriangles( ground.constData(), size_t( ground.size() ), nullptr, false );
			}

			for ( const PhysicsPreview::Shot & sh : shots ) {
				// a landed trace fades out; one still in flight stays solid
				const float fade = sh.flying ? 1.0f
					: std::max( 0.0f, 1.0f - sh.age / PhysicsPreview::TRACE_FADE );
				if ( fade <= 0.0f )
					continue;
				glDisable( GL_DEPTH_TEST );
				const QVector<Vector3> line { sh.from, sh.to };
				scene->setGLColor( 1.0f, 0.95f, 0.55f, 0.85f * fade );
				scene->setGLLineWidth( 2.0f * pxScale );
				scene->drawLines( line.constData(), size_t( line.size() ), nullptr );
				glEnable( GL_DEPTH_TEST );

				// the round itself, or the mark it left
				const float r = sh.radius * PhysicsPreview::SCALE * ( sh.flying ? 1.0f : 1.6f );
				const Vector3 c = sh.flying ? ( sh.pos * PhysicsPreview::SCALE ) : sh.to;
				QVector<Vector3> ball;
				const int SEG = 10;
				for ( int a = 0; a < SEG; a++ ) {
					const float t0 = float( a ) / SEG * 2.0f * float( M_PI );
					const float t1 = float( a + 1 ) / SEG * 2.0f * float( M_PI );
					// three rings, so it reads as a ball from any angle without a mesh
					ball << c + Vector3( std::cos( t0 ) * r, std::sin( t0 ) * r, 0 )
						 << c + Vector3( std::cos( t1 ) * r, std::sin( t1 ) * r, 0 )
						 << c + Vector3( std::cos( t0 ) * r, 0, std::sin( t0 ) * r )
						 << c + Vector3( std::cos( t1 ) * r, 0, std::sin( t1 ) * r )
						 << c + Vector3( 0, std::cos( t0 ) * r, std::sin( t0 ) * r )
						 << c + Vector3( 0, std::cos( t1 ) * r, std::sin( t1 ) * r );
				}
				if ( sh.flying )
					scene->setGLColor( 1.0f, 0.9f, 0.4f, 1.0f );
				else
					scene->setGLColor( 1.0f, 0.42f, 0.2f, fade );
				scene->setGLLineWidth( 2.0f * pxScale );
				scene->drawLines( ball.constData(), size_t( ball.size() ), nullptr );
			}
			glDisable( GL_BLEND );
		}
	}

	// Flat shading: the textured shapes were skipped, so fill every visible
	// mesh with a uniform dark grey (Blender wireframe-mode faces). X-ray
	// makes the fill half-transparent so geometry behind shows through.
	if ( model && scene->flatGrey && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( scene->xRay ? GL_FALSE : GL_TRUE );
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			const QSet<int> hidT = editMode ? editHiddenTris.value( s->id() ) : QSet<int>();
			int nv = s->verts.size();
			QVector<Vector3> soup;
			soup.reserve( s->triangles.size() * 3 );
			auto displayLocal = [this, s]( int vi ) {
				return scene->hasOption( Scene::DoSkinning )
					? s->skinVertex( vi, s->verts[vi] ) : s->verts[vi];
			};
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hidT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				soup << displayLocal( t[0] ) << displayLocal( t[1] ) << displayLocal( t[2] );
			}
			if ( soup.isEmpty() )
				continue;
			scene->loadModelViewMatrix( s->viewTrans() );
			scene->setGLColor( 0.20f, 0.20f, 0.21f, scene->xRay ? 0.45f : 1.0f );
			scene->drawTriangles( soup.constData(), size_t( soup.size() ), nullptr, true );
		}
		glDepthMask( GL_TRUE );
		glDisable( GL_BLEND );
	}

	if ( !cleanView ) {	// Clean View hides outlines, gizmos, wire/edit overlays, skeletons, origins
	// Blender-style selection outlines around the object-mode selection
	if ( model && !editMode && !objSelection.isEmpty() && !scene->selecting )
		drawObjectOutlines();

	// Pivot gizmo: RGB axes at the selected node's origin (oriented to the node),
	// plus the active constraint axis while a modal G/R/S transform is running.
	// In edit mode it follows the picked elements rather than the tree selection.
	bool showGizmo = editMode ? ( !pickedElems.isEmpty() || elemTransform )
	                          : ( objActive >= 0 || scene->currentBlock.isValid() );
	if ( model && showGizmo ) {
		int gb;
		if ( editMode && editShapeBlock >= 0 ) {
			gb = editShapeBlock;
		} else if ( !editMode && objActive >= 0 ) {
			gb = objActive;	// follow the active object/node (incl. block-list picks)
		} else {
			gb = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
			while ( gb >= 0 && !model->blockInherits( model->getBlockIndex( gb ), "NiAVObject" ) )
				gb = model->getParent( gb );
		}
		Node * gizmoNode = ( gb >= 0 ) ? scene->getNode( model, model->getBlockIndex( gb ) ) : nullptr;

		if ( gizmoNode ) {
			glDisable( GL_DEPTH_TEST );
			glDepthMask( GL_FALSE );

			QModelIndex iGb = model->getBlockIndex( gb );
			// during a modal gesture keep the frozen frame; otherwise follow the settings
			Matrix basis = gizmoMode ? gizmoBasisM : gizmoBasis( iGb );
			Vector3 pivot = gizmoMode ? gizmoPivotWorld : gizmoPivotPoint( iGb );
			if ( elemTransform )
				pivot = elemPivot;	// element transforms orbit the element median / cursor
			else if ( editMode && !pickedElems.isEmpty() )
				pivot = ( gizmoPivot == 3 ) ? cursorPos : pickedMedian();	// edit-mode gizmo sits on the selection

			Transform nt;
			nt.rotation = basis;
			nt.translation = pivot;
			nt.scale = 1.0f;
			float gs = gizmoScale( pivot );

			scene->setGLLineWidth( Settings::lineWidthAxes * ( gizmoMode ? 1.6f : 1.0f ) );
			scene->loadModelViewMatrix( viewTrans * nt );

			// object origin dot (Blender): always shown for the selected object /
			// node so geometry-less nodes (NiNode, lights) are visible and can be
			// grabbed, even with the gizmo handles toggled off
			if ( !editMode ) {
				scene->setGLPointSize( 8.0f * float( devicePixelRatioF() ) );
				if ( gb == objActive )
					scene->setGLColor( 1.0f, 0.616f, 0.0f, 1.0f );	// active #FF9D00
				else
					scene->setGLColor( 1.0f, 0.447f, 0.0f, 1.0f );	// secondary #FF7200
				Vector3 o;
				scene->drawPoints( &o, 1 );
			}

			if ( gizmoHandlesOn ) {
				// Blender-style handles: arrows with solid cone tips (move), rings (rotate), solid boxes
				// (scale), center circle (view- plane move); modelview is already the gizmo basis at the
				// pivot.
				const float L = gs;
				const float axR[3][4] = {
					{ 1.000f, 0.200f, 0.322f, 1.0f },	// X
					{ 0.545f, 0.863f, 0.000f, 1.0f },	// Y
					{ 0.157f, 0.565f, 1.000f, 1.0f }	// Z
				};
				const float hw = Settings::lineWidthAxes * ( gizmoMode ? 1.5f : 1.1f );

				// solid cone: apex at base+dir*len, 12-segment fan + base disc
				auto solidCone = [this]( const Vector3 & base, const Vector3 & dir, float radius, float len ) {
					Vector3 n( dir );
					n.normalize();
					Vector3 u = Vector3::crossproduct( n, ( std::fabs( n[2] ) < 0.9f )
						? Vector3( 0.0f, 0.0f, 1.0f ) : Vector3( 1.0f, 0.0f, 0.0f ) );
					u.normalize();
					Vector3 v = Vector3::crossproduct( n, u );
					Vector3 apex = base + n * len;
					const int sd = 12;
					QVector<Vector3> tris;
					tris.reserve( sd * 6 );
					for ( int i = 0; i < sd; i++ ) {
						float a0 = float( i ) * float( M_PI * 2.0 / sd );
						float a1 = float( i + 1 ) * float( M_PI * 2.0 / sd );
						Vector3 p0 = base + ( u * std::cos( a0 ) + v * std::sin( a0 ) ) * radius;
						Vector3 p1 = base + ( u * std::cos( a1 ) + v * std::sin( a1 ) ) * radius;
						tris << apex << p0 << p1;		// side
						tris << base << p1 << p0;		// base disc
					}
					scene->drawTriangles( tris.constData(), size_t( tris.size() ), nullptr, true );
				};
				// solid axis-aligned cube (gizmo-local space)
				auto solidBox = [this]( const Vector3 & c, float h ) {
					Vector3 p[8];
					for ( int i = 0; i < 8; i++ )
						p[i] = c + Vector3( ( i & 1 ) ? h : -h, ( i & 2 ) ? h : -h, ( i & 4 ) ? h : -h );
					static const int F[12][3] = {
						{ 0, 2, 3 }, { 0, 3, 1 }, { 4, 5, 7 }, { 4, 7, 6 },
						{ 0, 1, 5 }, { 0, 5, 4 }, { 2, 6, 7 }, { 2, 7, 3 },
						{ 0, 4, 6 }, { 0, 6, 2 }, { 1, 3, 7 }, { 1, 7, 5 }
					};
					QVector<Vector3> tris;
					tris.reserve( 36 );
					for ( auto & f : F )
						tris << p[f[0]] << p[f[1]] << p[f[2]];
					scene->drawTriangles( tris.constData(), size_t( tris.size() ), nullptr, true );
				};
				auto axisColor = [this, &axR]( int i, bool hov ) {
					if ( hov )
						scene->setGLColor( 1.0f, 1.0f, 1.0f, 1.0f );
					else
						scene->setGLColor( axR[i][0], axR[i][1], axR[i][2], axR[i][3] );
				};
				auto drawArrow = [&]( int i, bool hov ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					scene->setGLLineWidth( hw );
					scene->drawLine( u * ( L * 0.2f ), u * ( L * 0.92f ) );
					solidCone( u * ( L * 0.92f ), u, L * 0.045f, L * 0.18f );
				};
				auto drawRing = [&]( int i, bool hov ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					scene->setGLLineWidth( hw );
					scene->drawCircle( Vector3(), u, L * 0.85f, 64 );
				};
				auto drawScaleHandle = [&]( int i, bool hov, bool withShaft ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					if ( withShaft ) {
						// modal scale: Blender look, shaft ending in a box
						scene->setGLLineWidth( hw );
						scene->drawLine( u * ( L * 0.2f ), u * ( L * 0.85f ) );
						solidBox( u * ( L * 0.9f ), L * 0.05f );
					} else {
						solidBox( u * ( L * 0.62f ), L * 0.045f );
					}
				};

				if ( gizmoMode ) {
					// modal G/R/S: only the relevant sub-gizmo, only the locked
					// axis once constrained, and for plane moves the two
					// in-plane arrows (the excluded axis is hidden)
					for ( int i = 0; i < 3; i++ ) {
						if ( gizmoAxis > 0 && gizmoAxis != 1 + i )
							continue;
						if ( gizmoMode == 1 && gizmoPlane == 1 + i )
							continue;
						if ( gizmoMode == 1 )
							drawArrow( i, false );
						else if ( gizmoMode == 2 )
							drawRing( i, false );
						else
							drawScaleHandle( i, false, true );
					}
				} else {
					for ( int i = 0; i < 3; i++ ) {
						drawArrow( i, gizmoHover == 1 + i );
						drawScaleHandle( i, gizmoHover == 8 + i, false );
						drawRing( i, gizmoHover == 5 + i );
					}
					// plane-move handles: small quads between axis pairs,
					// coloured by the plane's normal axis (Blender)
					for ( int i = 0; i < 3; i++ ) {
						int j = ( i + 1 ) % 3, k = ( i + 2 ) % 3;
						Vector3 a, b;
						a[j] = 1.0f;
						b[k] = 1.0f;
						bool hov = ( gizmoHover == 12 + i );
						scene->setGLColor( axR[i][0], axR[i][1], axR[i][2], hov ? 0.95f : 0.5f );
						Vector3 q[6] = {
							a * ( L * 0.3f ) + b * ( L * 0.3f ), a * ( L * 0.5f ) + b * ( L * 0.3f ),
							a * ( L * 0.5f ) + b * ( L * 0.5f ),
							a * ( L * 0.3f ) + b * ( L * 0.3f ), a * ( L * 0.5f ) + b * ( L * 0.5f ),
							a * ( L * 0.3f ) + b * ( L * 0.5f )
						};
						scene->drawTriangles( q, 6, nullptr, true );
					}
					// center circle (view-plane move) + white view-rotate ring,
					// both billboarded to the camera
					Matrix vtInv = viewTrans.rotation.inverted();
					Matrix bInv = basis.inverted();
					Vector3 camN = bInv * ( vtInv * Vector3( 0.0f, 0.0f, 1.0f ) );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, gizmoHover == 4 ? 1.0f : 0.6f );
					scene->setGLLineWidth( Settings::lineWidthAxes * 1.2f );
					scene->drawCircle( Vector3(), camN, L * 0.12f, 32 );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, gizmoHover == 11 ? 0.95f : 0.4f );
					scene->setGLLineWidth( Settings::lineWidthAxes * 1.1f );
					scene->drawCircle( Vector3(), camN, L * 1.02f, 64 );
				}
			}

			if ( gizmoMode && ( gizmoAxis > 0 || gizmoPlane > 0 ) ) {
				// constraint guide lines through the pivot, in the same Blender
				// axis colours as the gizmo (plane constraints show both axes)
				static const float gc[3][3] = {
					{ 1.000f, 0.200f, 0.322f },	// X #FF3352
					{ 0.545f, 0.863f, 0.000f },	// Y #8BDC00
					{ 0.157f, 0.565f, 1.000f }	// Z #2890FF
				};
				scene->loadModelViewMatrix( viewTrans );
				scene->setGLLineWidth( Settings::lineWidthAxes * 0.8f );
				for ( int i = 0; i < 3; i++ ) {
					bool show = ( gizmoAxis == 1 + i )
					            || ( gizmoMode == 1 && gizmoPlane > 0 && gizmoPlane != 1 + i );
					if ( !show )
						continue;
					Vector3 unit;
					unit[i] = 1.0f;
					Vector3 a = basis * unit;
					scene->setGLColor( gc[i][0], gc[i][1], gc[i][2], 0.9f );
					scene->drawLine( pivot - a * ( gs * 40.0f ), pivot + a * ( gs * 40.0f ) );
				}
			}

			glDepthMask( GL_TRUE );
			glEnable( GL_DEPTH_TEST );
		}
	}

	// Blender-style wireframe overlay: a black wireframe drawn on top of the solid/shaded render, so the
	// texture shows through the faces.
	if ( model && wireframeOverlay && !scene->selecting ) {
		auto edgeKey = []( int a, int b ) {
			return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
		};

		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		if ( scene->xRay )
			glDisable( GL_DEPTH_TEST );	// X-ray: every edge shows through
		scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );	// Blender wireframe black
		scene->setGLLineWidth( Settings::lineWidthWireframe * wireWidthMul );

		// unique edge lists are cached across frames (the QSet dedup pass over
		// every triangle was running per repaint); only the camera-dependent
		// positions are rebuilt each frame
		if ( !wireEdgeCacheValid ) {
			wireEdgeCache.clear();
			wireEdgeCacheKey.clear();
			wireEdgeCacheValid = true;
		}
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			// edited meshes are wired by the edit overlay (keeps selection colours)
			if ( editMode && editShapeBlocks.contains( s->id() ) )
				continue;
			const QSet<int> hidT = editMode ? editHiddenTris.value( s->id() ) : QSet<int>();
			int nv = s->verts.size();

			// pull the wire toward the camera (local space) so it sits just in
			// front of the textured faces instead of z-fighting them
			Transform mv = s->viewTrans();
			Vector3 eyeL = mv.inverted() * Vector3( 0.0f, 0.0f, 0.0f );

			const quint64 wkey = quint64( quint32( s->triangles.size() ) )
				| ( quint64( quint32( nv ) ) << 24 ) | ( quint64( quint32( hidT.size() ) ) << 48 );
			auto itW = wireEdgeCache.constFind( s->id() );
			if ( itW == wireEdgeCache.constEnd() || wireEdgeCacheKey.value( s->id() ) != wkey ) {
				QVector<QPair<int, int>> built;
				QSet<quint64> eset;
				for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
					if ( hidT.contains( ti ) )
						continue;
					const Triangle & t = s->triangles.at( ti );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					for ( int e = 0; e < 3; e++ ) {
						int a = t[e], b = t[( e + 1 ) % 3];
						quint64 k = edgeKey( a, b );
						if ( !eset.contains( k ) ) {
							eset.insert( k );
							built.append( qMakePair( a, b ) );
						}
					}
				}
				itW = wireEdgeCache.insert( s->id(), built );
				wireEdgeCacheKey.insert( s->id(), wkey );
			}
			const QVector<QPair<int, int>> & wedges = itW.value();
			if ( wedges.isEmpty() )
				continue;
			QVector<Vector3> lines;
			lines.reserve( wedges.size() * 2 );
			for ( const auto & e : wedges ) {
				lines << ( eyeL + ( s->verts[e.first] - eyeL ) * 0.998f )
				      << ( eyeL + ( s->verts[e.second] - eyeL ) * 0.998f );
			}
			scene->loadModelViewMatrix( mv );
			scene->drawLines( lines.constData(), size_t( lines.size() ), nullptr );
		}
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
	}

	// object-mode selection is indicated by the Blender-style silhouette
	// outline (drawObjectOutlines, right after the scene) - the old coloured
	// wireframe overlay is gone

	// Blender-style edit-mode overlay: black wireframe + black vertex dots, selected elements orange
	// (#FF8500), active element white, translucent orange fill on selected faces, and an orange->black
	// gradient on edges running into selected vertices (vertex mode only).
	if ( model && editMode && !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode && !editShapeBlocks.isEmpty() ) {
		const float dpr = float( devicePixelRatioF() );
		const FloatVector4 colWire( 0.0f, 0.0f, 0.0f, 1.0f );
		const FloatVector4 colSel( 1.0f, 0.522f, 0.0f, 1.0f );		// Blender select #FF8500
		const FloatVector4 colActive( 1.0f, 1.0f, 1.0f, 1.0f );	// active element

		// gather the selection per shape
		QHash<int, QSet<int>> selVerts;
		QHash<int, QSet<quint64>> selEdges;
		QHash<int, QSet<int>> selFaces;
		auto edgeKey = []( int a, int b ) {
			return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
		};
		for ( const auto & pe : pickedElems ) {
			if ( pe.type == 1 )
				selVerts[pe.shapeBlock].insert( pe.e0 );
			else if ( pe.type == 2 )
				selEdges[pe.shapeBlock].insert( edgeKey( pe.e0, pe.e1 ) );
			else if ( pe.type == 3 )
				selFaces[pe.shapeBlock].insert( pe.e0 );
		}
		const PickedElement * activeElem = pickedElems.isEmpty() ? nullptr : &pickedElems.constLast();

		// Selection fingerprint: the cached filledTris must follow every
		// pickedElems mutation, and there are many mutation sites — an FNV
		// over the elements is O(selection) per frame and needs no hooks.
		quint64 selHash = 0xcbf29ce484222325ULL;
		for ( const auto & pe : pickedElems ) {
			selHash = ( selHash ^ ( quint64( quint32( pe.shapeBlock ) )
				| ( quint64( quint32( pe.type ) ) << 24 ) ) ) * 1099511628211ULL;
			selHash = ( selHash ^ ( quint64( quint32( pe.e0 ) )
				| ( quint64( quint32( pe.e1 ) ) << 32 ) ) ) * 1099511628211ULL;
		}
		if ( !editOverlaySetsValid ) {
			editOverlaySets.clear();
			editOverlaySetsValid = true;
		}

		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		Vector3 eye = viewTrans.inverted() * Vector3( 0.0f, 0.0f, 0.0f );

		for ( int wb : editShapeBlocks ) {
			Shape * s = shapeForBlock( wb );
			if ( !s || s->verts.isEmpty() )
				continue;
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();

			// world-space vertices, pulled a hair toward the camera
			QVector<Vector3> wv( nv );
			for ( int i = 0; i < nv; i++ ) {
				Vector3 w = wt * editVertexLocal( s, i );
				wv[i] = eye + ( w - eye ) * 0.997f;
			}

			// unique edge list (hidden triangles excluded, Blender H).
			const QSet<int> hiddenT = editHiddenTris.value( wb );
			const QSet<quint64> qmarks = quadMarksFor( wb );
			EditOverlaySets & es = editOverlaySets[wb];
			const bool structuralValid = ( es.nTris == s->triangles.size() && es.nVerts == nv
				&& es.nHidden == hiddenT.size() && es.nMarks == qmarks.size() );
			if ( !structuralValid ) {
				es.markAdj.clear();
				if ( !qmarks.isEmpty() ) {
					for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
						if ( hiddenT.contains( ti ) )
							continue;
						const Triangle & t = s->triangles.at( ti );
						if ( t[0] >= nv || t[1] >= nv || t[2] >= nv
							|| t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
							continue;
						for ( int e = 0; e < 3; e++ ) {
							quint64 k = edgeKey( t[e], t[( e + 1 ) % 3] );
							if ( qmarks.contains( k ) )
								es.markAdj[k]++;
						}
					}
				}
				es.visVerts.clear();
				es.edges.clear();
				QSet<int> visSet;
				QSet<quint64> eset;
				for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
					if ( hiddenT.contains( ti ) )
						continue;
					const Triangle & t = s->triangles.at( ti );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					for ( int e = 0; e < 3; e++ ) {
						int a = t[e], b = t[( e + 1 ) % 3];
						visSet.insert( a );
						quint64 k = edgeKey( a, b );
						if ( !eset.contains( k ) ) {
							eset.insert( k );
							if ( qmarks.contains( k ) && es.markAdj.value( k ) == 2 )
								continue;	// interior quad diagonal: not drawn
							es.edges.append( qMakePair( a, b ) );
						}
					}
				}
				es.visVerts.reserve( visSet.size() );
				for ( int vi : std::as_const( visSet ) )
					es.visVerts.append( vi );
				es.nTris = s->triangles.size();
				es.nVerts = nv;
				es.nHidden = hiddenT.size();
				es.nMarks = qmarks.size();
				es.selHash = selHash + 1;	// force the filled-tris rebuild below
			}

			const bool vertMode = bool( pickMode & 1 );
			const QSet<int> & sv = selVerts[wb];
			const QSet<quint64> & se = selEdges[wb];

			// Faces to fill: a face is filled when it is face-selected, OR (only in the matching select mode)
			// all of its verts / all of its edges are selected.
			if ( es.selHash != selHash || es.pickModeUsed != pickMode ) {
				const bool fillByVerts = bool( pickMode & 1 ) && !sv.isEmpty();
				const bool fillByEdges = bool( pickMode & 2 ) && !se.isEmpty();
				es.filledTris.clear();
				if ( selFaces.contains( wb ) )
					es.filledTris = selFaces.value( wb );
				if ( fillByVerts || fillByEdges ) {
					for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
						if ( hiddenT.contains( ti ) )
							continue;
						const Triangle & t = s->triangles.at( ti );
						if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
							continue;
						bool allV = fillByVerts && sv.contains( t[0] ) && sv.contains( t[1] ) && sv.contains( t[2] );
						bool allE = fillByEdges && se.contains( edgeKey( t[0], t[1] ) )
							&& se.contains( edgeKey( t[1], t[2] ) ) && se.contains( edgeKey( t[2], t[0] ) );
						if ( allV || allE )
							es.filledTris.insert( ti );
					}
				}
				es.selHash = selHash;
				es.pickModeUsed = pickMode;
			}
			const QVector<QPair<int, int>> & edges = es.edges;
			const QHash<quint64, int> & markAdj = es.markAdj;
			const QSet<int> & filledTris = es.filledTris;
			const QVector<int> & visVerts = es.visVerts;

			// Selection (fills, selected edges/verts, outlines) is drawn with the depth test OFF so nearby
			// unconnected geometry can never occlude it - the edit cage stays on top, Blender-style.
			QVector<Vector3> foutline;	// non-active filled faces (orange outline)
			QVector<Vector3> aoutline;	// the active/primary face (white outline)
			if ( !filledTris.isEmpty() ) {
				// The FILL is depth-tested unless X-ray is on:
				// 753af78 turned the depth test off for the fills, the selected edges/verts and the outlines
				// together, so nearby unconnected geometry could not occlude the selection.
				if ( scene->xRay )
					glDisable( GL_DEPTH_TEST );
				// the active face's quad partner counts as active too: a marked
				// quad must read as ONE uniformly-lit face, not a light half and
				// a dark half split along its hidden diagonal (the active fill
				// is lighter than the selected fill, so the tone step showed)
				int actPartner = -1;
				if ( activeElem && activeElem->type == 3 && activeElem->shapeBlock == wb
					&& !qmarks.isEmpty() )
					actPartner = quadPartnerTri( wb, activeElem->e0 );
				QVector<Vector3> ftris, atris;
				for ( int fi : filledTris ) {
					if ( fi < 0 || fi >= s->triangles.size() )
						continue;
					const Triangle & t = s->triangles.at( fi );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					bool act = ( activeElem && activeElem->type == 3
					             && activeElem->shapeBlock == wb
					             && ( activeElem->e0 == fi || ( actPartner >= 0 && fi == actPartner ) ) );
					QVector<Vector3> & dst = act ? atris : ftris;
					dst << wv[t[0]] << wv[t[1]] << wv[t[2]];
					// only the active face gets the white outline (Blender); the rest of the selection stays
					// orange.
					QVector<Vector3> & odst = act ? aoutline : foutline;
					for ( int e = 0; e < 3; e++ ) {
						const int a = t[e], b = t[( e + 1 ) % 3];
						const quint64 k = edgeKey( a, b );
						if ( !qmarks.isEmpty() && qmarks.contains( k ) && markAdj.value( k ) == 2 )
							continue;
						odst << wv[a] << wv[b];
					}
				}
				if ( !ftris.isEmpty() ) {
					scene->setGLColor( 1.0f, 0.522f, 0.0f, 0.30f );
					scene->drawTriangles( ftris.constData(), size_t( ftris.size() ), nullptr, true );
				}
				if ( !atris.isEmpty() ) {
					scene->setGLColor( 1.0f, 0.72f, 0.35f, 0.36f );	// active face: lighter
					scene->drawTriangles( atris.constData(), size_t( atris.size() ), nullptr, true );
				}
				glEnable( GL_DEPTH_TEST );
			}

			// base wireframe with per-endpoint colours (gradient in vertex mode)
			QVector<Vector3> lineVerts;
			QVector<FloatVector4> lineCols;
			lineVerts.reserve( edges.size() * 2 );
			lineCols.reserve( edges.size() * 2 );
			for ( const auto & e : edges ) {
				lineVerts << wv[e.first] << wv[e.second];
				FloatVector4 ca = colWire, cb = colWire;
				if ( vertMode ) {
					if ( sv.contains( e.first ) )
						ca = colSel;
					if ( sv.contains( e.second ) )
						cb = colSel;
				}
				lineCols << ca << cb;
			}
			if ( !lineVerts.isEmpty() ) {
				scene->setGLLineWidth( 1.0f * dpr * wireWidthMul );
				scene->drawLines( lineVerts.constData(), size_t( lineVerts.size() ), lineCols.constData() );
			}

			// unselected vertex dots (depth-tested)
			if ( vertMode ) {
				scene->setGLPointSize( vertexPointSize * dpr );
				scene->setGLColor( colWire );
				if ( hiddenT.isEmpty() ) {
					scene->drawPoints( wv.constData(), size_t( nv ) );
				} else {
					QVector<Vector3> visPts;
					visPts.reserve( visVerts.size() );
					for ( int vi : visVerts )
						visPts.append( wv[vi] );
					if ( !visPts.isEmpty() )
						scene->drawPoints( visPts.constData(), size_t( visPts.size() ) );
				}
			}

			// selected elements, on top of their own surface:
			// Depth-tested unless X-ray is on, same as the fill above.
			if ( scene->xRay )
				glDisable( GL_DEPTH_TEST );

			// outline around every filled face: white in edge/face select mode
			// (Blender), orange in pure vertex mode so the selection still reads
			// as orange instead of white
			if ( !foutline.isEmpty() ) {
				scene->setGLLineWidth( 1.7f * dpr * wireWidthMul );
				scene->setGLColor( colSel );	// non-primary selected faces: orange
				scene->drawLines( foutline.constData(), size_t( foutline.size() ), nullptr );
			}
			if ( !aoutline.isEmpty() ) {
				scene->setGLLineWidth( 1.7f * dpr * wireWidthMul );
				scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.95f );	// active/primary face: white (Blender)
				scene->drawLines( aoutline.constData(), size_t( aoutline.size() ), nullptr );
			}

			// selected edges (any mode), slightly wider like Blender
			if ( selEdges.contains( wb ) ) {
				QVector<Vector3> selLines, actLines;
				for ( const auto & e : edges ) {
					quint64 k = edgeKey( e.first, e.second );
					if ( !se.contains( k ) )
						continue;
					bool act = ( activeElem && activeElem->type == 2 && activeElem->shapeBlock == wb
					             && edgeKey( activeElem->e0, activeElem->e1 ) == k );
					QVector<Vector3> & dst = act ? actLines : selLines;
					dst << wv[e.first] << wv[e.second];
				}
				scene->setGLLineWidth( selLineWidth * dpr );
				if ( !selLines.isEmpty() ) {
					scene->setGLColor( colSel );
					scene->drawLines( selLines.constData(), size_t( selLines.size() ), nullptr );
				}
				if ( !actLines.isEmpty() ) {
					scene->setGLColor( colActive );
					scene->drawLines( actLines.constData(), size_t( actLines.size() ), nullptr );
				}
			}

			// selected / active vertex dots
			if ( vertMode && !sv.isEmpty() ) {
				scene->setGLPointSize( vertexPointSize * dpr );
				QVector<Vector3> selPts, actPts;
				for ( int vi : sv ) {
					if ( vi < 0 || vi >= nv )
						continue;
					bool act = ( activeElem && activeElem->type == 1
					             && activeElem->shapeBlock == wb && activeElem->e0 == vi );
					( act ? actPts : selPts ).append( wv[vi] );
				}
				if ( !selPts.isEmpty() ) {
					scene->setGLColor( colSel );
					scene->drawPoints( selPts.constData(), size_t( selPts.size() ) );
				}
				if ( !actPts.isEmpty() ) {
					scene->setGLColor( colActive );
					scene->drawPoints( actPts.constData(), size_t( actPts.size() ) );
				}
			}

			glEnable( GL_DEPTH_TEST );
		}

		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// picked reference elements outside edit mode (snap targets etc.)
	if ( model && !editMode && !pickedElems.isEmpty() ) {
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		float ms = std::max( float( Dist ) / 100.0f, 0.005f ) * gizmoSizeMul;

		for ( const auto & pe : pickedElems ) {
			Shape * s = shapeForBlock( pe.shapeBlock );
			if ( !s )
				continue;
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();

			if ( pe.type == 1 ) {
				if ( pe.e0 >= nv )
					continue;
				Vector3 v = wt * editVertexLocal( s, pe.e0 );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );	// contrast halo
				scene->drawSphereSimple( v, ms * 0.62f, 16, 2 );
				scene->setGLColor( 1.0f, 0.5f, 0.0f, 1.0f );	// orange verts
				scene->drawSphereSimple( v, ms * 0.45f, 16, 2 );
			} else if ( pe.type == 2 ) {
				if ( pe.e0 >= nv || pe.e1 >= nv )
					continue;
				Vector3 a = wt * editVertexLocal( s, pe.e0 ), b = wt * editVertexLocal( s, pe.e1 );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 3.4f );
				scene->drawLine( a, b );
				scene->setGLColor( 1.0f, 0.9f, 0.2f, 1.0f );	// yellow edges
				scene->setGLLineWidth( Settings::lineWidthAxes * 1.6f );
				scene->drawLine( a, b );
			} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < s->triangles.size() ) {
				const Triangle & t = s->triangles.at( pe.e0 );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				Vector3 tri[3] = { wt * editVertexLocal( s, t[0] ),
					wt * editVertexLocal( s, t[1] ), wt * editVertexLocal( s, t[2] ) };
				// filled translucent face + solid outline
				scene->setGLColor( 1.0f, 0.4f, 0.85f, 0.45f );	// magenta fill
				scene->drawTriangles( tri, 3, nullptr, true );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 3.2f );
				scene->drawLine( tri[0], tri[1] );
				scene->drawLine( tri[1], tri[2] );
				scene->drawLine( tri[2], tri[0] );
				scene->setGLColor( 1.0f, 0.55f, 0.95f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 1.5f );
				scene->drawLine( tri[0], tri[1] );
				scene->drawLine( tri[1], tri[2] );
				scene->drawLine( tri[2], tri[0] );
			}
		}

		if ( pickedElems.size() > 1 ) {
			// median marker
			Vector3 m = pickedMedian();
			scene->setGLColor( 1.0f, 1.0f, 1.0f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthAxes );
			for ( int c = 0; c < 3; c++ ) {
				Vector3 d;
				d[c] = ms * 1.5f;
				scene->drawLine( m - d, m + d );
			}
		}

		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
	}

	// Pose Mode: bone-influence overlay under the skeleton, then the skeleton.
	// The Skeleton Manager draws the armature too, but not the weight overlay —
	// that one belongs to the bone you are actively posing.
	if ( poseMode || skeletonView ) {
		if ( poseMode )
			drawPoseWeights();
		drawPoseSkeleton();
	}

	// Overlays > Show Skeleton. AFTER the pose armature deliberately: when both
	// are on the overlay's class colours are what should be read, and it is the
	// overlay whose census the harness reads back.
	drawSkeletonOverlay();

	// Blender-style origin dots + parent relationship lines for the selection
	if ( model && showOrigins && ( !objSelection.isEmpty() || ( editMode && !editShapeBlocks.isEmpty() ) ) ) {
		const QSet<int> & selN = editMode ? editShapeBlocks : objSelection;
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		float dpr = float( devicePixelRatioF() );
		for ( int ob : selN ) {
			Node * n = scene->getNode( model, model->getBlockIndex( ob ) );
			if ( !n )
				continue;
			Vector3 o = n->worldTrans().translation;
			scene->setGLPointSize( 9.0f * dpr );
			scene->setGLColor( 0.05f, 0.05f, 0.05f, 1.0f );
			scene->drawPoints( &o, 1 );
			scene->setGLPointSize( 6.0f * dpr );
			// selection colours matching the block list / wireframe:
			// active (last-selected) #FF9D00, other selected #FF7200
			bool isActive = editMode ? ( ob == editShapeBlock ) : ( ob == objActive );
			if ( isActive )
				scene->setGLColor( 1.0f, 0.616f, 0.0f, 1.0f );
			else
				scene->setGLColor( 1.0f, 0.447f, 0.0f, 1.0f );
			scene->drawPoints( &o, 1 );
			// dashed relationship line to the parent's origin
			int pb = model->getParent( ob );
			if ( pb >= 0 && model->blockInherits( model->getBlockIndex( pb ), "NiAVObject" ) ) {
				Node * pn = scene->getNode( model, model->getBlockIndex( pb ) );
				if ( pn ) {
					scene->setGLLineWidth( 1.0f * dpr );
					scene->setGLColor( 0.75f, 0.75f, 0.75f, 0.7f );
					scene->drawDashLine( o, pn->worldTrans().translation, 24 );
				}
			}
		}
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
	}
	}	// end of the Clean View helpers
}
