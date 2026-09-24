/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SUNSHADOW_H
#define WW_SUNSHADOW_H

/* Cascaded sun shadows (lane CSM1; the law is
 * scratchpad/pbrprep1_20260924/spec_cascaded_shadows.md, read from Todd's treat).
 *
 * Lookdev only, perspective only, one Shadows row in the Scene popup (ships OFF,
 * persisted, harness-isolated). The light is the lookdev sun as it is SHADED: the
 * same floored direction the lighting uses (spec 2.7 -- at night the same light,
 * the moon is visual only), so the hour row moves the shadows live.
 *
 * Fit (spec 2.1-2.3): three cascades, far = 800 / 3000 / D (D = the shadow
 * distance, code default 3000); slice i spans [i ? far[i-1] - 100 : camNear,
 * far[i] + 100] of planar view depth. Light basis right = normalize(L x Y),
 * up = normalize(right x L) with L = the light's TRAVEL direction. The slice
 * corners go to light space relative to the camera (z + 15000); the texel is
 * ceil(extent / map) above half a unit per texel, else 1/n with n = floor(map /
 * extent); the light origin and the box's low corner are floored onto that texel;
 * the viewport is floor(extent / texel) texels; near 150, far = max z + 150.
 * Caster pass (spec 2.6): D16 texture array, 3 layers, glPolygonOffset(6, 12),
 * back faces culled (double-sided shapes drawn unculled), the ground quad included.
 * Receiver (spec 2.4/2.5, res/shaders/ww_sunshadow.glsl): the 16-tap Poisson
 * kernel, radius 3 texels, bilinear PCF per tap; cascade pair by view depth, a
 * smoothstep blend over 100 units past each split, the distance fade s^4; the
 * factor multiplies the SUN's diffuse and specular only.
 *
 * Byte identity: the receiving code lives only in the variant programs
 * pbrm_csm.prog and lookdev_ground_csm.prog (WW_SUNSHADOW defined), swapped in by
 * name while a draw receives. Shadows OFF never builds a map and never swaps.
 *
 * Divergences, stated: legacy (non-PBR) shapes cast but do not receive; alpha-test
 * cutouts cast solid; GPU-skinned casters are skinned on the CPU for the map; an
 * orthographic view draws no shadows; the right/top edge of the light window is
 * l' + viewport x texel (INFERRED; the spec floors it separately, <= 1 quantum).
 *
 * Pins: WW_LOOKDEV_SHADOWS=0|1 (the row), WW_CSM_DISTANCE=<D>, WW_CSM_MAP=<px>,
 * WW_CSM_PROBE=1 (hard single tap of the selected cascade, raw grey) | 2 (cascade
 * colour debug: red / green / blue over the shaded picture) | 3 (the filtered
 * factor with the seam blend, no fade, raw grey) | 4 (the final factor, raw grey) |
 * 5 (the selection as numbers: R = cascade A / 2, G = cascade B / 2, B = blend).
 * Reds (WW_CSM_RED=<a,b>): flipsun, nofloor, nosnap, onecascade, wrongsplit,
 * nobias, bigbias (600-unit receiver offset), noblend, nofade, diffonly, factorhalf (Shadows OFF still swaps in the
 * variant and halves the sun: the OFF-identity gate must refuse it).
 * Echo: wwSunShadowSummary() -> "csm=on map=.. D=.. L=.. c0=(zn,zf,texel,n,vw,vh,
 * pb,l,b,far) ... camrows=<camera right>|<camera up> m0..m2=<the 16 floats uploaded
 * as csmMat[i], column-major>" in the PBRM census line (written at a shape's first
 * draw), and WW_CSM_ECHO=<absolute path> holds the same line for the LAST pass run,
 * i.e. the grabbed frame once the render hook's camera pin has taken. */

#include <QString>

class Scene;

//! true while this draw receives: the map was built this frame for this view
bool wwSunShadowWanted( Scene * scene );
//! the caster pass: renders the three cascade maps (GLView calls it before the background)
void wwSunShadowPass( Scene * scene );
//! the receiver uniforms of the renderer's CURRENT program; a no-op without csmMap
void wwSunShadowUniforms( Scene * scene );
//! one line for the census: the fit of the last pass, or why none was built
QString wwSunShadowSummary();

#endif
