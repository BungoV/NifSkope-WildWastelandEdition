#!/usr/bin/env python3
"""Lane FARWATER1 (part B): how much sky the far water reflects at the far rings' grazing angles.

A twin of res/shaders/water_refl.glsl wwWaterFresnel (Frostbite's rough Schlick, F0 0.0204) and of the GGX alpha
floor of wwWaterAlpha2 (a0 = 2 / (4 n + 2), n the WATR's sun specular power). The far rings' distances are the
cell view's (skill ww-far-water-shore-census: band 0, rings 1-4 out to ~95 cells); the eye heights are a shore
walker (128 u over the water) and a hill (2048 u). The shader's per-pixel slope variance and footprint only RAISE
alpha, so alpha = sqrt(a0) is the upper bound of F; F at alpha 0.3 shows the cap a rough far sheet reaches.
It asserts the twin against the GLSL's text (the formula lines verbatim), so a law change makes it fail."""
import math, re, sys

GLSL = 'res/shaders/water_refl.glsl'


def fresnel(nov, alpha, f0=0.0204):
    c = 1.0 - min(max(nov, 0.0), 1.0)
    c2 = c * c
    return f0 + (max(1.0 - alpha, f0) - f0) * c2 * c2 * c


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    src = open(f'{root}/{GLSL}', encoding='utf-8').read()
    need = ['float c = 1.0 - clamp( NoV, 0.0, 1.0 );', 'return F0 + ( max( 1.0 - alpha, F0 ) - F0 ) * c2 * c2 * c;',
            'float a0 = 2.0 / ( 4.0 * max( specPow, 1.0 ) + 2.0 );']
    miss = [n for n in need if n not in src]
    print('twin lines found in the GLSL:', 'all' if not miss else f'MISSING {miss}')
    rings = [('band 0', 8 * 4096), ('ring 1', 20 * 4096), ('ring 2', 32 * 4096), ('ring 4', 80 * 4096)]
    print(f'{"eye":>6} {"where":>8} {"dist u":>8} {"angle deg":>9} {"F a=0.022":>9} {"F a=0.1":>8} {"F a=0.3":>8}')
    for h in (128.0, 2048.0):
        for name, d in rings:
            nov = h / math.hypot(h, d)
            a_floor = math.sqrt(2.0 / (4.0 * 1000.0 + 2.0))
            print(f'{h:6.0f} {name:>8} {d:8.0f} {math.degrees(math.asin(nov)):9.3f} {fresnel(nov, a_floor):9.3f} '
                  f'{fresnel(nov, 0.1):8.3f} {fresnel(nov, 0.3):8.3f}')
    print('(a=0.022: the alpha floor at specular power 1000; the per-pixel terms only raise alpha, so lower F)')
    return 1 if miss else 0


if __name__ == '__main__':
    sys.exit(main())
