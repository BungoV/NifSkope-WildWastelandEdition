#!/usr/bin/env python3
"""CARDFIT3 fix 01 -- two defects in the render hook, found while building the
transition gate:

 1. WW_RENDER_DIST had NO effect. setDistance() sets Dist, and the
    orthographic half-height is Dist/Zoom; the auto-fit that setOrientation(
    recenter ) performs leaves Zoom at whatever framed the bound sphere, so the
    two renders of one model at half-heights 1874 and 7496 came out BYTE
    IDENTICAL. The impostor bake already knew this and corrects by reading
    orthographicHalfHeight() back; the render hook did not. Same correction here.

 2. No way to photograph a model WITHOUT the viewport grid, the two axis lines
    and the node markers, which land in any silhouette a script measures off the
    picture. WW_RENDER_CLEAN=1 clears Scene::ShowGrid | ShowAxes | ShowNodes.
"""
import sys

P = 'src/nifskope_ui.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'nifskope_ui.cpp is LF-only'
s = b.decode('utf-8')
n = 0

old = """						const QString dist = qEnvironmentVariable( "WW_RENDER_DIST" );
						if ( !dist.isEmpty() )
							skope->ogl->setDistance( dist.toFloat() );
"""
new = """						const QString dist = qEnvironmentVariable( "WW_RENDER_DIST" );
						if ( !dist.isEmpty() ) {
							/* setDistance sets Dist, and the ORTHOGRAPHIC HALF-HEIGHT
							 * is Dist / Zoom. setOrientation( recenter ) above leaves
							 * Zoom at whatever framed the bound sphere, so asking for
							 * a half-height here used to change nothing at all:
							 * measured 2026-09-09, one tree photographed at 1874 and
							 * at 7496 produced two BYTE-IDENTICAL PNGs, and a camera
							 * pin that cannot move is a harness that guards nothing.
							 * The impostor bake has always read the value back and
							 * corrected; this does the same. */
							const float want = dist.toFloat();
							skope->ogl->setDistance( want );
							qApp->processEvents();
							const float got = skope->ogl->orthographicHalfHeight();
							if ( want > 0.0f && got > 0.0f
								&& std::fabs( got - want ) > 1.0e-3f * want )
								skope->ogl->setDistance( want * want / got );
						}
"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new); n += 1

old = """						sc->showParticles = true;
"""
new = """						sc->showParticles = true;

						/* WW_RENDER_CLEAN=1: the MODEL and nothing else -- no viewport
						 * grid, no axis lines, no node markers. A script that measures
						 * a silhouette off the picture cannot tell a grid line from a
						 * twig, and the grid alone put the measured bounding box at
						 * the full width of the window (2026-09-09). It changes only
						 * these three overlays; lighting, texturing and the camera are
						 * untouched, so a CLEAN render and a normal one differ by
						 * exactly the overlays. */
						if ( qEnvironmentVariableIntValue( "WW_RENDER_CLEAN" ) != 0 )
							sc->options &= ~( Scene::ShowGrid | Scene::ShowAxes | Scene::ShowNodes );
"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new); n += 1

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('fix01: %d edits, %d bytes' % (n, len(out)))
