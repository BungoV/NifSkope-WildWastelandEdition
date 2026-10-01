# PRTP2 -- how the stock game lights a pixel

Lane PRTP2, research, 2026-09-30. What PRTP3 (viewport lights) draws and PRTP6 (the bake) sums.
Sources: Todd's treat (engine read, notes kept private), the stock shaders as FO4CS reproduces them, and
the record layouts (xEdit). Every term names its refuter: the PRTP4 capture that would prove it wrong.

## 0. Units and color space
- Record colors are 8-bit sRGB-ish bytes, / 255. The shaders take them to linear with pow(x, 2.2)
  (not the sRGB curve). A light's shader color = pow(diffuse, 2.2) x fade x dimmer.
- The Negative flag (LIGH) flips the color's sign: the light subtracts.

## 1. Point light (LIGH, omni)
Record: DATA Radius (ref XRDS overrides), Color, Falloff Exponent, Constant, Scalar, Exponent; FNAM fade.
Constant/Scalar/Exponent reach the shader as (bias, scale, exponent). With d = distance, r = radius:

    x     = saturate(d / r)
    atten = pow(1 - saturate(scale * x^exponent + bias), 2.2)
    L     = color * atten * max(N.L, 0)          (plus the stock specular)

Vanilla lights mostly carry (bias 0, scale 1, exponent 2): atten = (1 - x^2)^2.2, zero at the radius.
An exponent of 0 means no distance term (constant). The engine's CPU copy of the same curve (used for
light ranking) is 1 - saturate(bias + scale * x^exponent), without the 2.2.
Refuter: a capture's cb2[3] per light against its record's three floats.

## 2. Spot light
Flags Shadow Spotlight / NonShadow Spotlight. FOV (degrees, ref override first) -> outer cone;
Falloff Exponent shapes the cone edge; Near Clip for its shadow.

    cosOuter = cos(FOV / 2)                      (half angle: to confirm)
    base     = saturate(1 - (1 - dot(-L, spotDir)) / (1 - cosOuter))
    cone     = min(pow(base, falloffExponent), 1)
    atten    = radial (section 1) * cone

Hemisphere and omni shadow lights use fixed FOVs; they are points for lighting.
Refuter: the spot shader's cone row against the record FOV.

## 3. Sun (exteriors)
Direction and color from the weather's time-of-day (WTHR), times the light's dimmer; shadowed by the
cascades and cloud shadows. Radiance = diffuse x dimmer; L = radiance x max(N.L, 0).
Interiors have no sun; an interior's XCLL "Directional" color and rotation act as one unshadowed
directional light.

## 4. Ambient: directional ambient (DALC)
Six colors, one per world axis (+X, -X, +Y, -Y, +Z, -Z): from the weather (exterior, time-of-day
blended) or from the cell's XCLL / lighting template (interior). Reduced to three affine rows,
per channel c:

    row_c = ( (px - nx)/2, (py - ny)/2, (pz - nz)/2, mean of the six )
    ambient(n) = pow(max(dot(row, (n, 1)), 0), 2.2)

evaluated along the surface normal (world space), times the material's albedo and AO.
Refuter: a capture's ambient rows against the weather's six colors at that hour.

## 5. Fog
Exterior: the weather's fog near/far distance, near/far color, power, max (clamp), plus height fog
(high near/far colors and density). Interior: XCLL fog near/far color, near/far, power, max.

    f     = min(max, pow(saturate(dist / (far - near) - near / (far - near)), power))
    color = lerp(nearColor, farColor, f)
    out   = lerp(lit, color, f)

The engine packs 0x60 bytes of fog state per frame. The (near, far) -> shader packing above is the
Skyrim family's form; it is NOT yet confirmed for Fallout 4. Refuter: the FO4 vertex shader's fog
row in a PRTP4 capture.

## 6. Interior light fade
XCLL Light Fade Begin/End: placed lights dim from begin to end distance from the camera (a draw
cost cut, not physics). The bake ignores it (every light at full strength).

## 7. Summation
    lit = albedo * (ambient(n) * AO + sum_lights(color_i * atten_i * N.L_i) + sun)
          + specular terms
    out = fog(lit)

For the bake (PRTP6), only the diffuse part matters: probe irradiance = sum of point/spot/sun as
above (shadowed by the bake's own rays) + sky from the DALC/sky model, with no fog and no fade.

## 8. Still open
- Fog packing (section 5): capture.
- The LIGH color's exact 1/255 constant and the Negative branch: read once more with a PE reader.
- Specular: stock Blinn-Phong vs FO4CS PBR; the bake does not need it.

## 9. Measured while building PRTP3 (2026-10-01)
- XRDS is a DELTA added to the base radius, not an override: 2,080 of the 3,853 XRDS in the PRTP1
  gate's ten interiors are negative; as base + XRDS only 9 reach zero (those draw nothing).
- A spot shines along its ref's local +X under the engine euler (-x, -y, -z): of 2,396 spots,
  1,177 aim down and 581 up along +X; along -Z the split is 608/131 (scratchpad/prtp1_20260930/spot_axis.py).
- FO4's LIGH flag list has no Negative flag (section 0's note is from older games); not drawn.
- DALC orientation: section 4 lights up-facing normals with the +Z color. The Lookdev stage assumed
  the opposite; the two disagree until the PRTP4 capture rules.
- Interior directional direction: dirTo = (cos el sin az, cos el cos az, sin el), el = Rotation XY,
  az = Rotation Z, degrees. ASSUMED; refuter = the PRTP4 capture.
