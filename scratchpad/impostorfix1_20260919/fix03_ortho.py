#!/usr/bin/env python
"""IMPOSTORFIX1 fix03: the card must know when the camera is ORTHOGRAPHIC.

`impostor_oct.vert` already has the clause and says why it matters -- under an
orthographic projection there is no eye point and every view ray is `-cardFwd`
-- but `cardOrtho` is written `false` unconditionally from
src/gl/impostordraw.cpp:464, so the ray is always the perspective fan
`normalize( p - cardEyeModel )`.

That is measurable, and it is the residual on the known-answer control. At a
BAKE DIRECTION under an orthographic camera the parallax step is a
mathematical no-op whatever the height channel says: it moves the sample along
`ray`, `frameUvOf` projects onto `frameRight`/`frameUp`, and if `ray` is
antiparallel to `frameFwd` the projection is unchanged. With the fan it is not
antiparallel -- at the measuring camera's pin (Dist = 4 x 361, half 135 x 361)
the ray leans up to 14 degrees at the frame's edge -- so a height of a few
hundred units drags the sample sideways at exactly the directions where the
card is supposed to be a photograph.

Measured, blast_n4, its own 16 bake directions, repaired sheets:
    parallax off 0.8823 / on 0.7545.

The decision is made in the vertex shader from the projection matrix itself
rather than from a new uniform: an orthographic matrix has a w-row of
(0,0,0,1), a perspective one has -1 in it, and the shader already has the
matrix because it needs it for `gl_Position`. The C++ `cardOrtho` uniform is
left exactly as it is, so nothing that sets it true changes meaning.
"""
import io

P = 'res/shaders/impostor_oct.vert'
b = io.open(P, 'rb').read()
cr = b.count(b'\r')

OLD = b"""	cardViewRay = cardOrtho ? -cardFwd : normalize( p - cardEyeModel );
"""
NEW = b"""	/* ...and the projection says which of the two it is. `cardOrtho` arrives
	 * false from every caller (src/gl/impostordraw.cpp), which was fine while
	 * nothing drew a card under an orthographic camera and wrong the moment
	 * something did: an orthographic projection matrix has a w-row of
	 * (0,0,0,1) and a perspective one has -1 in it, and this shader already
	 * holds the matrix for `gl_Position`. At a bake direction under an
	 * orthographic camera the parallax step below is a mathematical no-op --
	 * it moves the sample along `ray`, and `frameUvOf` projects onto axes
	 * perpendicular to `frameFwd` -- so with the fan the card stopped being a
	 * photograph of the mesh at the one set of directions where it must be
	 * one. blast_n4 at its own sixteen bake directions, repaired sheets:
	 * parallax off 0.8823, parallax on with the fan 0.7545. */
	bool orthoCam = cardOrtho || abs( projectionMatrix[2][3] ) < 1.0e-6;
	cardViewRay = orthoCam ? -cardFwd : normalize( p - cardEyeModel );
"""
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)
assert b.count(b'\r') == cr
io.open(P, 'wb').write(b)
print('fix03 applied, %d bytes, CR=%d' % (len(b), b.count(b'\r')))
