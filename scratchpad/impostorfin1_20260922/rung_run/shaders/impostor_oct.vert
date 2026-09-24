#version 410 core

/* The octahedral impostor card, vertex stage. Lane IMPOSTORSHOW, 2026-09-19.
 *
 * The quad is ONE camera-facing rectangle per card, `card.half` x 2 in size
 * and centred on `card.center` in the base's model space -- spec 239: "the
 * recorded halfW/halfH span the full frame, gutter included -- the quad is the
 * frame", and spec 259: "halfW/halfH then carry the frame's aspect exactly --
 * a card quad is its frame, undistorted". So the quad is not fitted to the
 * silhouette and is not squared off: it IS the frame's rectangle.
 *
 * Everything else -- which frames, what weights, where the surface really is
 * -- is per PIXEL and lives in the fragment stage, because the three frames
 * disagree about where the surface is and that disagreement is what the height
 * channel exists to settle. */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

// The card, from its .lodm (docs/LODGEN_LODM_FORMAT.md `card` block).
uniform vec3  cardCenter;      // model space
uniform vec2  cardHalf;        // halfW, halfH -- the frame's own extents
uniform vec3  cardRight;       // the billboard's axes in model space, built
uniform vec3  cardUp;          // CPU-side from the camera direction so every
uniform vec3  cardFwd;         // fragment of one card agrees on them
uniform vec3  cardEyeModel;    // the camera's position in model space
uniform bool  cardOrtho;       // true: no eye point, the ray is cardFwd

layout ( location = 0 ) in vec3 vertexPosition;   // the unit quad, -1..1 in xy
layout ( location = 7 ) in vec2 multiTexCoord0;   // 0..1 across the quad

out vec3 cardPosModel;   // this fragment's point on the card plane, model space
out vec2 cardQuadUv;     // 0..1 across the quad, for the debug channels
out vec3 cardViewRay;    // model-space direction from the camera to this point

void main()
{
	/* The quad's own plane. `vertexPosition.xy` is the unit square, so the
	 * card is exactly 2*halfW by 2*halfH and undistorted. */
	vec3 p = cardCenter
		+ cardRight * ( vertexPosition.x * cardHalf.x )
		+ cardUp    * ( vertexPosition.y * cardHalf.y );

	cardPosModel = p;
	cardQuadUv   = multiTexCoord0;

	/* The view ray, in the same model space the card lives in, pointing FROM
	 * the camera TOWARDS this point. `cardFwd` points at the camera (it is the
	 * direction the frames were chosen for), so under an orthographic
	 * projection -- which has no eye point at all, and which is the projection
	 * the BAKE used -- every ray is its negative. Under a perspective camera
	 * the rays fan out across the card's own width, and a card wide enough to
	 * matter (a 25 m tree at the loaded-cell edge, spec 219) shows it: the
	 * parallax below has to follow the real ray or the near edge of the card
	 * parallaxes the wrong way. */
	/* ...and the projection says which of the two it is. `cardOrtho` arrives
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

	gl_Position = projectionMatrix * ( modelViewMatrix * vec4( p, 1.0 ) );
}
