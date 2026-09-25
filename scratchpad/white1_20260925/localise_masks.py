"""the core mask of localise.py (western block samples more than 8 cells from the land edge), recomputed from blk/ring"""
import numpy as np
def core():
    h = np.load('h.npy'); blk = np.load('blk.npy'); land = h != -352.0
    ed = np.full(h.shape, 9999, np.int32)
    for axis in (0, 1):
        a = land if axis == 1 else land.T
        d = np.zeros(a.shape, np.int32); run = np.zeros(a.shape[0], np.int32)
        for j in range(a.shape[1]):
            run = np.where(a[:, j], run + 1, 0); d[:, j] = run
        d2 = np.zeros(a.shape, np.int32); run = np.zeros(a.shape[0], np.int32)
        for j in range(a.shape[1] - 1, -1, -1):
            run = np.where(a[:, j], run + 1, 0); d2[:, j] = run
        dd = np.minimum(d, d2); ed = np.minimum(ed, dd if axis == 1 else dd.T)
    return blk & (ed > 8 * 32)
