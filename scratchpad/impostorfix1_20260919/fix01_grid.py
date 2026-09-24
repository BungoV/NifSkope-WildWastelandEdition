#!/usr/bin/env python
"""IMPOSTORFIX1 fix01: the measuring modes must also switch the GRID off.

Found by the known-answer control, 2026-09-19 13:50. At the four bake
directions that are axis-aligned views (azim 0/90/180/270 at elev 0) the
mesh grab scored 0.31-0.35 while every other bake direction scored 0.86-0.91.
The cause is not the card: `Scene::drawGrid` returns early in orthographic
mode unless `GLView::axisAlignedViewState()` names a face view, so at exactly
those four azimuths a full screen-plane lattice is painted -- into the MESH
grab only, because the card grab runs with the whole scene suppressed. Mesh
coverage there is 0.0746 against 0.025 at every other direction. The lattice
enters the union and never the intersection, so it can only push IoU down.

`ShowAxes` was already cleared here for exactly this class of reason.
"""
import io, sys

P = 'src/impostorpreviewtest.cpp'
b = io.open(P, 'rb').read()
cr = b.count(b'\r')
assert cr == 0, cr

OLD = b"""		if ( scene )
			scene->options &= ~Scene::SceneOptions( Scene::ShowAxes );
		ogl->showCursor = false;
		st.log << QStringLiteral( "chrome off: nav gizmo and 3D cursor suppressed"
				" (they were counted in both silhouettes and flattered every IoU)" );
"""

NEW = b"""		if ( scene )
			scene->options &= ~Scene::SceneOptions( Scene::ShowAxes | Scene::ShowGrid );
		ogl->showCursor = false;
		st.log << QStringLiteral( "chrome off: nav gizmo, 3D cursor and GRID suppressed" );
		/* THE GRID IS THE WORSE HALF OF THE SAME FAULT, and it biases the
		 * other way -- downwards, at four directions only, which is how it
		 * survived: it looks like a defect in the card.
		 *
		 * `Scene::drawGrid` (glscene.cpp:632) returns early in orthographic
		 * mode unless `GLView::axisAlignedViewState()` names a face view. So
		 * at azim 0/90/180/270 with elev 0 -- and NOWHERE else -- a full
		 * screen-plane lattice is painted across the frame. The card grab
		 * runs with the whole scene suppressed and never gets it, so the
		 * lattice lands in the MESH mask alone: it joins the union, never the
		 * intersection, and those four views can only score low.
		 *
		 * Measured, lane IMPOSTORFIX1 2026-09-19, blast_n4 at its own 16 bake
		 * directions: mesh coverage 0.0746 at the four axis-aligned views
		 * against 0.0250 at the other twelve, IoU 0.31-0.35 against
		 * 0.86-0.91. The card was the same card in both groups.
		 */
"""

assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)
assert b.count(b'\r') == cr
io.open(P, 'wb').write(b)
print('fix01 applied, %d bytes, CR=%d' % (len(b), b.count(b'\r')))
