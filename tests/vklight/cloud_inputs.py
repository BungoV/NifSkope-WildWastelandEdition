"""Lane VKLIGHT1 round 2: the volume clouds' march inputs for tests/vklight/frag_jobs.py, from the tree's own producers
(no NifSkope): the noise volumes and the calibration table from cloudvol_dump.cpp (WwCloudNoise / WwCloudVolMath, the
bytes the exe uploads), the transmittance and sky-view LUTs drawn by the real atmo_*.prog (atmo_skyrender.Sky), the
uniforms as tests/features/clouds/cloudvol_check.py sets them, the blue noise sheet res/ssgi/fast_2uges.rg8.

    python -I tests/vklight/cloud_inputs.py <out dir>

Writes <out dir>/cloud_inputs.json (uniforms per job) + shape.bin / detail.bin (R8) + trans.npy / sunview.npy (float32).
"""
import ctypes, json, os, runpy, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.abspath(sys.argv[1])
os.makedirs(OUT, exist_ok=True)

# cloudvol_check with a gate letter it does not have runs none of its gates (cloudband_check does the same)
sys.argv = [os.path.join(TREE, 'tests', 'features', 'clouds', 'cloudvol_check.py'), OUT, '--only', 'Z']
K = runpy.run_path(sys.argv[0], run_name='vklight_cloud_inputs')
np, gl = K['np'], K['gl']


def read_tex(t):
    gl.glBindTexture(0x0DE1, t)
    w, h = ctypes.c_int(), ctypes.c_int()
    gl.glGetTexLevelParameteriv(0x0DE1, 0, 0x1000, ctypes.byref(w))
    gl.glGetTexLevelParameteriv(0x0DE1, 0, 0x1001, ctypes.byref(h))
    a = np.zeros((h.value, w.value, 4), np.float32)
    gl.glGetTexImage(0x0DE1, 0, 0x1908, 0x1406, a.ctypes.data_as(ctypes.c_void_p))
    return a


SUN = (30.0, 30.0)  # azimuth, elevation deg
trans = read_tex(K['trans'])
sv = read_tex(K['sky'].view_lut(192, 108, 6360.05, K['R'].sundir(*SUN)))
np.save(os.path.join(OUT, 'trans.npy'), trans)
np.save(os.path.join(OUT, 'sunview.npy'), sv)

HW, HH = 320, 180  # the history (1/4 of 1280 x 720, cloudvol.cpp Medium)
POS = (0.0, 0.0, 0.2)
CAM = K['camera'](20.0, 12.0)
ATMO = {'atRay': (5.802e-3, 13.558e-3, 33.1e-3), 'atMieS': (3.996e-3,), 'atMieE': (4.40e-3,), 'atHaze': (0.02, 1.2, 0.6),
        'atOzone': (0.65e-3, 1.881e-3, 0.085e-3), 'atG': (0.8,), 'atHazeAlbedo': (0.9, 0.9, 0.9), 'atHazeSpec': (1.0, 1.0, 1.0)}


def plain(u):
    out = {}
    for k, v in u.items():
        if k.endswith('[0]') or k.endswith('[1]') or k.endswith('[2]'):
            base = k[:-3]
            arr = out.setdefault(base, [0.0] * 9)
            i = int(k[-2])
            arr[3 * i:3 * i + 3] = [float(x) for x in v]
            continue
        if isinstance(v, int):
            out[k] = v
        else:
            out[k] = [float(x) for x in np.asarray(v, np.float64).reshape(-1)]
    return out


def march(stride, phase, frame, cam=CAM, pos=POS, cov=(0.6, 0.3, 0.0)):
    u = K['bands_uni'](K['LOW'], K['MID'], K['cal'], cov=cov)
    u.update(K['march_uni'](cam, pos, (HW, HH), stride, phase, SUN[1], sun_az=SUN[0], steps=48, taps=3, drop=K['drop'],
                            amb=0.05, ap=1.0, frame=float(frame), lightKm=1.0, maxKm=60.0))
    u.update(ATMO)
    return plain(u)


cam2 = K['camera'](20.4, 12.0)  # the last frame: 0.4 deg of turn, 0.5 m of move
pos2 = (0.0005, 0.0, 0.2)
J = {
    'march_full': march(1, (0.0, 0.0), 5),
    'march_s4': march(4, (1.0, 2.0), 6),
    'march_prev': march(1, (0.0, 0.0), 4, cam=cam2, pos=pos2),
    'cam': plain({'c': CAM})['c'], 'pos': list(POS), 'cam_prev': plain({'c': cam2})['c'], 'pos_prev': list(pos2),
    'hist': [HW, HH],
}
with open(os.path.join(OUT, 'cloud_inputs.json'), 'w') as fh:
    json.dump(J, fh, indent=0)
print('CLOUD INPUTS', OUT, 'trans', trans.shape, 'sunview', sv.shape, 'cal', len(J['march_full']['cvCal']))
