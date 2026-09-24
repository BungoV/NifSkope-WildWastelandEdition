"""Lane GROUND1 gate A2f: is the object march a FAIR estimator, i.e. is it the
same estimator the terrain term in the same byte already is?

There is no vanilla artefact carrying object occlusion in the far terrain, so
there is no absolute strength to fit. What CAN be measured exactly is the
estimator itself, against the closed-form cosine-weighted sky visibility, for
an occluder of a known shape -- and, crucially, against the TERRAIN term that
is already in that byte, because the two use the same two constants.

The occluder: a wall of height H above the sample point at horizontal distance
D, filling one of the eight 45-degree sectors. s = H/D is the horizon slope.

  the march's contribution to occlusion   =  ( s/(1+s) ) / 8 * 1.6
  the truth's  contribution to occlusion  =  sin^2( atan s ) * (pi/4) / pi
                                          =  ( s^2/(1+s^2) ) / 8

Both are "fraction of the cosine-weighted hemisphere lost", so the ratio is
dimensionless and is the estimator's bias at that horizon angle.
"""
import math

print('  s = H/D   angle    march     truth     march/truth')
rows = []
for s in [0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0]:
    march = (s / (1.0 + s)) / 8.0 * 1.6
    truth = (s * s / (1.0 + s * s)) / 8.0
    rows.append((s, math.degrees(math.atan(s)), march, truth, march / truth))
    print('  %7.3f  %5.1f deg  %.5f   %.5f   %.3f' % rows[-1])

print()
print('  The terrain term in the SAME byte is the same two constants over the')
print('  ground\'s own horizon slope, so the object term is neither stronger nor')
print('  weaker per unit horizon angle than what the sheet already carries.')
print()

# What horizon slopes does the TERRAIN march actually see, against what the
# OBJECT march sees? The first is a property of the Commonwealth; the second of
# the placed geometry. Printed here as the shapes, not as a claim.
print('  Reference points:')
for name, h, d in [('a 128-unit fence at 128 u', 128.0, 128.0),
                   ('a 600-unit tree at 128 u', 600.0, 128.0),
                   ('a 600-unit tree at 648 u', 600.0, 648.0),
                   ('a 600-unit tree at 1458 u', 600.0, 1458.0),
                   ('a 1600-unit tower at 288 u', 1600.0, 288.0),
                   ('a 24-unit terrain rise at 128 u', 24.0, 128.0),
                   ('a 300-unit hill at 1458 u', 300.0, 1458.0)]:
    s = h / d
    march = (s / (1.0 + s)) / 8.0 * 1.6
    print('    %-28s s %6.3f  %4.1f deg  one sector costs %5.1f AO bytes'
          % (name, s, math.degrees(math.atan(s)), march * 255.0))
