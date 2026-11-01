"""lane FIX1101A area T (LATE ITEM 26): a numpy port of Fusion Shaders' Physical tone map, line for line from FO4CS
res/Post/TonemapCS.hlsl (F4FX_PhysicalCurve, F4FX_PhysicalInversePq, F4FX_PhysicalPqEotf, the ICtCp pair,
F4FX_TonemapPhysical in its SDR output mode) and res/HDR/DisplayMap.hlsli (the Rec.709 <-> Rec.2020 matrices), with the
constants FO4CS feeds it (src/Post/Tonemap.cpp ResolveTonemapConstants + the defaults below). Adapted from Polyphony
Digital's MIT-licensed GT7 Tone Mapping sample (LICENSE.md). Independent of res/shaders/cell_tonemap.glsl: the gate
(gate.py) runs that file through the driver and compares.

Also the game's filmic curve as cell_lights.glsl had it (the red) and its inverse.
"""
import numpy as np

# src/Post/Tonemap.h PhysicalTonemapParams + res/FO4CS.ini [Post] (sdr paper white 250, blend 0.60, fade 0.98..1.16)
PAPER = 250.0           # fTonemapPhysicalSdrPaperWhiteNits
BLEND = 0.60            # fTonemapPhysicalBlendRatio
FADE0, FADE1 = 0.98, 1.16
# src/Post/Tonemap.cpp constants
REF = 100.0             # kPhysicalReferenceNits
ALPHA, MID, LIN, TOE = 0.25, 0.538, 0.444, 1.280
EPS = 0.0001            # res/Post/Common.hlsli F4FX_LUM_EPSILON
# ResolveTonemapConstants, SDR: outputParams = (250, blend, 100 / 250, paper); chromaParams = (fade0, fade1, 100, 0)
TARGET = max(PAPER, 250.0)
PEAK = TARGET / max(REF, 1.0)
CORR = REF / PAPER


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def curve(x, peak=PEAK):
    k = (LIN - 1.0) / (ALPHA - 1.0)
    kA = peak * (LIN + k)
    kB = -peak * k * np.exp(LIN / k)
    kC = -1.0 / (k * peak)
    x = np.maximum(x, 0.0)
    w = smoothstep(0.0, MID, x)
    toe = MID * np.power(np.maximum(x / MID, 0.0), TOE)
    lin = toe + (x - toe) * w
    sh = kA + kB * np.exp(x * kC)
    return np.where(x < LIN * peak, lin, sh)


def inv_pq(v):
    m1, m2, c1, c2, c3 = 0.1593017578125, 78.84375, 0.8359375, 18.8515625, 18.6875
    y = np.maximum(v, 0.0) * REF / 10000.0
    ym = np.power(np.abs(y), m1)
    return np.exp2(m2 * (np.log2(c1 + c2 * ym) - np.log2(1.0 + c3 * ym)))


def pq_eotf(v):
    m1, m2, c1, c2, c3 = 0.1593017578125, 78.84375, 0.8359375, 18.8515625, 18.6875
    npw = np.power(np.clip(v, 0.0, 1.0), 1.0 / m2)
    l = np.power(np.maximum(npw - c1, 0.0) / np.maximum(c2 - c3 * npw, EPS), 1.0 / m1)
    return l * 10000.0 / REF


def to_ictcp(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    l = (1688.0 * r + 2146.0 * g + 262.0 * b) / 4096.0
    m = (683.0 * r + 2951.0 * g + 462.0 * b) / 4096.0
    s = (99.0 * r + 309.0 * g + 3688.0 * b) / 4096.0
    L, M, S = inv_pq(l), inv_pq(m), inv_pq(s)
    return np.stack([0.5 * (L + M), (6610.0 * L - 13613.0 * M + 7003.0 * S) / 4096.0,
                     (17933.0 * L - 17390.0 * M - 543.0 * S) / 4096.0], -1)


def from_ictcp(ic):
    i, t, p = ic[..., 0], ic[..., 1], ic[..., 2]
    L = pq_eotf(i + 0.00860904 * t + 0.11103 * p)
    M = pq_eotf(i - 0.00860904 * t - 0.11103 * p)
    S = pq_eotf(i + 0.560031 * t - 0.320627 * p)
    return np.maximum(np.stack([3.43661 * L - 2.50645 * M + 0.0698454 * S,
                                -0.79133 * L + 1.9836 * M - 0.192271 * S,
                                -0.0259499 * L - 0.0989137 * M + 1.12486 * S], -1), 0.0)


M709_2020 = np.array([[0.6274040, 0.3292820, 0.0433136], [0.0690970, 0.9195400, 0.0113612],
                      [0.0163916, 0.0880132, 0.8955950]])
M2020_709 = np.array([[1.6604910, -0.5876411, -0.0728499], [-0.1245505, 1.1328999, -0.0083494],
                      [-0.0181508, -0.1005789, 1.1187297]])


def physical(c709):
    """F4FX_TonemapPhysical, SDR mode: exposed scene-linear Rec.709 (1.0 = paper white) -> display-linear 0..1"""
    c709 = np.asarray(c709, np.float64)
    rgb = np.maximum(np.maximum(c709, 0.0) @ M709_2020.T, 0.0)
    rgb = rgb * PEAK
    orig = to_ictcp(rgb)
    skew = curve(rgb)
    skewU = to_ictcp(skew)
    target = to_ictcp(np.array([PEAK, PEAK, PEAK]))[0]
    nl = orig[..., 0] / max(target, EPS)
    cs = 1.0 - smoothstep(FADE0, FADE1, nl)
    scaled = from_ictcp(np.stack([skewU[..., 0], orig[..., 1] * cs, orig[..., 2] * cs], -1))
    m = np.minimum(skew + (scaled - skew) * np.clip(BLEND, 0.0, 1.0), PEAK) * CORR
    return np.clip(np.maximum(m @ M2020_709.T, 0.0), 0.0, 1.0)


def vanilla(exposed, E=0.0):
    """cell_lights.glsl before this lane: the game's filmic curve at W 11.2 on x = 2 x the exposed light"""
    x = 2.0 * np.maximum(np.asarray(exposed, np.float64), 0.0)
    c = (x * (0.15 * x + 0.05) + 0.2 * E) / (x * (0.15 * x + 0.5) + 0.06) - E / 0.3
    return c / ((0.2 * E + 19.376) * 0.040856 - E / 0.3)


def vanilla_inv(y, E=0.0):
    """the exposed light the filmic curve shows as display-linear y (cell_hdr.frag cellCurveInv, / 2)"""
    y = np.asarray(y, np.float64)
    yp = y * ((0.2 * E + 19.376) * 0.040856 - E / 0.3) + E / 0.3
    a, b, c = 0.15 * (yp - 1.0), 0.5 * yp - 0.05, 0.06 * yp - 0.2 * E
    x = (-b - np.sqrt(np.maximum(b * b - 4.0 * a * c, 0.0))) / (2.0 * a)
    return np.maximum(x, 0.0) / 2.0
