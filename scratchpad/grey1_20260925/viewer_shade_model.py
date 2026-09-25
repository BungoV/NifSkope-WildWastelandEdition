"""GREY1 candidate 2, numeric half: what the viewer's Legacy lighting (the mode every session starts in) does to an
atlas texel's saturation, straight from res/shaders/fo4_default.{vert,frag} and src/glview.cpp at f506a0cc.
Defaults (no persisted sliders in his registry): ambient 1, brightnessL 1, lightColor 0 -> white, toneMapping
0.23641851, brightnessScale 1, frontalLight (headlight, L = V). So A.rgb = sqrt(1)*0.375, A.a = 0.2364,
D.rgb = 1, D.a = 1.
  color = (A + D*NdotL)*albedo*D*(1-F) + A*albedo + spec ;  out = tonemap(color)  (per channel)
Albedo = the raw UNORM texel (the shader's 'sqrt-linear' space; tonemap squares it). F ~ 0.04 at normal
incidence. Specular is left out here (it is white; the render half measures it)."""
import numpy as np
A_rgb, A_a, D_a = 0.375, 0.23641851, 1.0


def film(z):
    a, b, c, d, e, f = .15, .5, .1, .2, .02, .3
    return (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f


def tonemap(x):
    z = x * x * D_a * (A_a * 4.22978723)
    return np.sqrt(np.maximum(film(z), 0) / (A_a * 0.93333333))


def S(c):
    return (c.max() - c.min()) / max(c.max(), 1e-9)


def Y(c):
    return float(np.dot(c, (0.299, 0.587, 0.114)))


albedos = {'LOD atlas mean': (0.54, 0.50, 0.45), 'warm brick': (0.557, 0.460, 0.367), 'tinted green (swap)': (0.40, 0.52, 0.42),
           'dark grey': (0.25, 0.24, 0.23)}
print('%-22s %-6s | ' % ('albedo (sRGB)', 'S') + ' | '.join('N.L=%.2f out S (Y)' % n for n in (1.0, 0.7, 0.4, 0.0)))
for nm, a in albedos.items():
    a = np.array(a)
    row = '%-22s %.3f  | ' % (nm, S(a))
    for ndl in (1.0, 0.7, 0.4, 0.0):
        col = (A_rgb + ndl) * a * (1 - 0.04) + A_rgb * a
        o = np.clip(tonemap(col), 0, 1)
        row += '     %.3f (%.2f)    | ' % (S(o), Y(o))
    print(row)
# tone curve's own slope in log-log (a slope < 1 compresses channel ratios = desaturates)
for x in (0.3, 0.6, 0.9, 1.2, 1.6):
    e = 1e-3
    s = (np.log(tonemap(np.array(x * (1 + e)))) - np.log(tonemap(np.array(x)))) / np.log(1 + e)
    print('tonemap log-slope at input %.1f = %.3f  (out %.3f)' % (x, s, tonemap(np.array(x))))
