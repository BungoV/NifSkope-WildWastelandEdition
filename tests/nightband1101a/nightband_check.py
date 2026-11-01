"""Lane FIX1101A (atmo): the night horizon stripe (SHOTS1101 sheet_lod_proof.png, spot 3 22:00: a flat 18,19,21 band
between a 25,40,60 sky and a 36,59,90 far land, hard edges). Display-space twin of cell_volsky.frag's horizon band
(HORIZONBAND1's method, tests/horizonband1) on the frame's own column (night_column.tsv, tools/mk_night_fixture.py):
  the band weight per row w = 1 - smoothstep(0, 0.03, tSky) (atmo_skyband.glsl wwSkyBandW, the sky's T from
  atmo_model.py with the frame's haze); the cover = the band colour x w over the backdrop x (1 - w);
  rule 1 (today, red WW_ATMO_AP_RED=apband): the band colour = the aerial perspective's far end = the plateau the frame
  shows where w = 1 (MEASURED); the backdrop per row solved from today's picture;
  rule 2 (shipped): the band colour = what a black surface at the far plane gets through the surfaces' fog path = the
  far land's colour at the silhouette (PROXY, REASONED: the land at the far plane is fog-dominated).
Gates (stated before the first run): the largest one-row change (max channel, 8-bit levels) from +2 deg down into the
land <= 8; the band no darker than the darker of its neighbours (the sky at +1 deg, the land 2 rows under the
silhouette) by more than 2 levels (no dark stripe). The red must fail both.
METHOD MOVED after the first run (bars unchanged): the silhouette's anti-aliased rows are remixed with the rule's band
colour by their land coverage (they held today's band; the first run kept them and failed green at the AA row, 29.1).
Measured 11-01: green step 6.4 / band +2.8 over its darker neighbour; red apband step 15.7 / 15.3 under: FAIL.
usage: python -I nightband_check.py [--red apband]"""
import sys, os
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
RED = '--red' in sys.argv
rows = []
for line in open(os.path.join(HERE, 'night_column.tsv')):
    if line.startswith('#'):
        continue
    f = line.split('\t')
    rows.append((int(f[0]), float(f[1]), float(f[2]), np.array([float(x) for x in f[3:6]])))
y = np.array([r[0] for r in rows])
e = np.array([r[1] for r in rows])
tS = np.array([r[2] for r in rows])
C = np.array([r[3] for r in rows])
t = np.clip(tS / 0.03, 0, 1)
w = 1 - t * t * (3 - 2 * t)
# the silhouette: the first row (from the top, below the horizon) whose blue jumps by > 20 levels over the plateau
plateau = C[(w > 0.999) & (e > -0.9)].mean(0)
land0 = int(np.nonzero((e < 0) & (C[:, 2] > plateau[2] + 20))[0][0])
sil = land0   # rows >= sil are the far land (unchanged by the rule: geometry, not the sky pass)
hz_ap = plateau
hz_far = C[min(sil + 3, len(C) - 1)]
base = np.where((w < 0.999)[:, None], (C - hz_ap * w[:, None]) / np.maximum(1 - w, 1e-6)[:, None], 0.0)
hz = hz_ap if RED else hz_far
out = C.copy()
for i in range(sil):
    out[i] = hz * w[i] + base[i] * (1 - w[i]) if w[i] < 0.999 else hz
# the silhouette's anti-aliased rows hold today's band mixed with the land: remix them with the rule's band colour by
# their land coverage a = (C - plateau) / (land - plateau) (luminance), as tests/horizonband1 treats its AA row
landf = C[min(sil + 4, len(C) - 1)]
lumv = lambda c: float(np.dot(c, [0.2126, 0.7152, 0.0722]))
for i in range(max(sil - 3, 0), min(sil + 4, len(C))):
    a = (lumv(C[i]) - lumv(plateau)) / max(lumv(landf) - lumv(plateau), 1e-6)
    if 0.0 < a < 0.98:
        out[i] = landf * a + hz * (1 - a)
steps = np.abs(np.diff(out, axis=0)).max(1)
m = (e[:-1] <= 2.0)
worst = float(steps[m].max())
iw = int(np.argmax(np.where(m, steps, -1)))
band = out[(w > 0.999) & (np.arange(len(out)) < sil)]
lum = lambda c: float(np.dot(c, [0.2126, 0.7152, 0.0722]))
sky1 = out[int(np.argmin(np.abs(e - 1.0)))]
land = out[min(sil + 2, len(out) - 1)]
dark = min(lum(sky1), lum(land)) - min(lum(c) for c in band)
fails = 0
for name, ok, msg in (
        ('step', worst <= 8.0, 'largest one-row change %.1f levels at row %d (bar 8)' % (worst, y[iw])),
        ('nodark', dark <= 2.0, 'the band is %.1f levels under the darker neighbour (sky +1 deg %.1f, land %.1f; bar 2)' % (
            dark, lum(sky1), lum(land)))):
    print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, msg))
    fails += 0 if ok else 1
print('band colour: %s (%s); silhouette row %d' % (np.round(hz, 1), 'rule 1: the AP far end' if RED else 'rule 2: the far-plane fog colour (proxy)', y[sil]))
print('%s: %d gate(s) failed%s' % ('GATE OK' if not fails else 'GATE FAIL', fails, ' (red apband)' if RED else ''))
sys.exit(1 if fails else 0)
