"""GREY1 candidate 3: what the vanilla CommonwealthClear midday light does to a building's colour, vs the viewer.
Inputs: weathers.json (esm_weather.py, vanilla Fallout4.esm read only). Light in linear units: NAM0 Sunlight x IMGS
HNAM Sunlight Scale x max(N.L, 0) + DALC ambient-cube(N). DALC axis sense = src/esmweather.cpp's (the Z- colour lights
UP-facing normals; that file flags it an assumption). Sun at midday: elevation 60 deg from the south (assumption,
the range 40..75 is printed too). Albedo = the LOD atlas surface mean measured by census2.py (REFR-weighted), plus a
neutral grey and a warm brick so the effect on each is visible.
Output: per face, the light's own colour (normalised), the lit colour's HSV saturation vs the albedo's, and the
viewer's (white headlight, white ambient: S unchanged before its tone map)."""
import json, math
import numpy as np
import sys
W = json.load(open(sys.argv[1] if len(sys.argv) > 1 else 'weathers.json'))
w = [x for x in W if x['edid'] == 'CommonwealthClear'][0]
lin = lambda c: (np.asarray(c, float) / 255.0) ** 2.2
SCALE = w['imgs']['HNAM'][6] if w['imgs'].get('HNAM') else 4.5   # unresolved IMGS -> vanilla Clear's 4.5, ASSUMED
sun = lin(w['sunlight']) * SCALE
D = [lin(a) for a in w['dalc']]          # X+ X- Y+ Y- Z+ Z-
# ambient cube: the colour of axis k lights a normal pointing OPPOSITE the axis' travel direction (esmweather)
axes = [np.array(v, float) for v in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))]


def amb(n):
    e = np.zeros(3)
    for a, c in zip(axes, D):
        d = max(float(np.dot(n, -a)), 0.0)
        e += d * d * c
    return e


def s2l(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def l2s(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(np.maximum(c, 0), 1 / 2.4) - 0.055)


def S(rgb):
    rgb = np.asarray(rgb, float)
    return (rgb.max() - rgb.min()) / max(rgb.max(), 1e-9)


albedos = {'LOD atlas mean (census2)': (0.54, 0.50, 0.45), 'neutral grey': (0.5, 0.5, 0.5), 'warm brick': (0.557, 0.460, 0.367)}
faces = {'roof (up)': (0, 0, 1), 'wall facing sun (south)': (0, -1, 0), 'wall east': (1, 0, 0), 'wall facing away (north)': (0, 1, 0)}
print('CommonwealthClear Day: sun %s x sunlightScale %.2f -> linear %s; DALC up-lighting colour %s' % (
    w['sunlight'], SCALE, np.round(sun, 3), w['dalc'][5]))
for el in (60, 40, 75):
    sd = np.array((0, -math.cos(math.radians(el)), math.sin(math.radians(el))))
    print('\n-- sun elevation %d deg --' % el)
    for fn, n in faces.items():
        n = np.array(n, float)
        E = sun * max(float(np.dot(n, sd)), 0) + amb(n)
        tint = E / E.max()
        row = '%-26s light rgb %s (S %.2f) |' % (fn, np.round(tint, 2), S(l2s(tint)))
        for an, a in albedos.items():
            out = l2s(s2l(a) * E / (0.2126 * E[0] + 0.7152 * E[1] + 0.0722 * E[2]))   # exposure-normalised, hue kept
            row += ' %s S %.2f->%.2f' % (an.split()[0], S(a), S(np.clip(out, 0, None)))
        print(row)
