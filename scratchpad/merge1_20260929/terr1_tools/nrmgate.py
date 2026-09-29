"""TERR1 normal-stamp gates on the Boston VT.2 sheets (offline decode, vtmosaic.py).

usage: python nrmgate.py <on VT.2.lodt> <off VT.2.lodt> <noroads VT.2.lodt> <out.json>

  G1 share / angle: on the STAMPED texels (colour sheet differs between the roads bake and the --no-roads bake,
     decoded, any channel > 2 levels), the share whose msn normal moved > 1 degree against the stamp-off bake,
     and the mean angle there.
  G2 off-mask identity, BC1 block level, mip 0, every tile's stored sheet incl. border: every msn block whose bytes
     differ ON vs OFF lies in a block whose COLOUR bytes differ roads vs no-roads. Refuter: the same test against
     an OFF msn with one planted change in an unstamped block must report exactly 1 violation.
  G3 orientation (maps1's refuter): east-facing texels (height sheet falls eastward, slope > 0.15) have R above
     west-facing ones; north-facing have B above south-facing -- on the whole box and on the stamped texels alone.
  G4 the other sheets: colour and height byte-identical ON vs OFF.
"""
import sys, json
import numpy as np
import vtmosaic as VM


def ang(a, b):
    a = a / np.maximum(np.linalg.norm(a, axis=-1, keepdims=True), 1e-9)
    b = b / np.maximum(np.linalg.norm(b, axis=-1, keepdims=True), 1e-9)
    return np.degrees(np.arccos(np.clip((a * b).sum(-1), -1, 1)))


def sheet_blocks(S, tx, ty, role):
    p, o = S.raw_sheet_bytes(tx, ty, role)
    v = S.v
    e = v.table[ty * v.tilesX + tx]
    si = S.role[role]
    sd = v.sheets[si]
    cover = bool(e['flags'] & 2)
    fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
    bb = 16 if fmt in (77, 78) else 8
    nb = S.D // 4
    return np.frombuffer(p, dtype=np.uint8, count=nb * nb * bb, offset=o).reshape(nb, nb, bb)


def main():
    on, off, nr = VM.Sheets(sys.argv[1]), VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3])
    out = {}
    # ---- G1
    col_on = on.mosaic(1)[..., :3]
    col_nr = nr.mosaic(1)[..., :3]
    stamped = (np.abs(col_on - col_nr) > 2).any(-1)
    n_on = VM.msn_world(on.mosaic(2))
    n_off = VM.msn_world(off.mosaic(2))
    a = ang(n_on, n_off)
    moved = a > 1.0
    out['G1'] = dict(texels=int(stamped.size), stamped=int(stamped.sum()),
                     stamped_moved_share=float(moved[stamped].mean()),
                     stamped_mean_angle_deg=float(a[stamped].mean()),
                     stamped_moved_mean_angle_deg=float(a[stamped & moved].mean()) if (stamped & moved).any() else 0.0,
                     unstamped_moved_share=float(moved[~stamped].mean()),
                     moved_total=int(moved.sum()))
    # ---- G2
    viol = checked = differ = 0
    first_clean = None
    for ty in range(on.v.tilesY):
        for tx in range(on.v.tilesX):
            m_on, m_off = sheet_blocks(on, tx, ty, 2), sheet_blocks(off, tx, ty, 2)
            c_on, c_nr = sheet_blocks(on, tx, ty, 1), sheet_blocks(nr, tx, ty, 1)
            md = (m_on != m_off).any(-1)
            cd = (c_on != c_nr).any(-1)
            checked += md.size
            differ += int(md.sum())
            viol += int((md & ~cd).sum())
            if first_clean is None:
                cl = np.argwhere(~cd & ~md)
                if len(cl):
                    first_clean = (tx, ty, int(cl[0][0]), int(cl[0][1]))
    out['G2'] = dict(blocks=checked, msn_blocks_differ=differ, violations=viol)
    # refuter: plant one change in an unstamped, unchanged block of the OFF msn and rerun that tile's test
    tx, ty, bj, bi = first_clean
    m_on, m_off = sheet_blocks(on, tx, ty, 2), sheet_blocks(off, tx, ty, 2).copy()
    c_on, c_nr = sheet_blocks(on, tx, ty, 1), sheet_blocks(nr, tx, ty, 1)
    m_off[bj, bi, 0] ^= 0x01
    md = (m_on != m_off).any(-1)
    cd = (c_on != c_nr).any(-1)
    out['G2_refuter'] = dict(tile=[tx, ty], block=[bj, bi], violations_with_plant=int((md & ~cd).sum()))
    # ---- G3
    h = VM.height_units(on.mosaic(4))
    gx = np.zeros_like(h); gy = np.zeros_like(h)
    gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) / 32.0          # d h / d east
    gy[1:-1, :] = (h[:-2, :] - h[2:, :]) / 32.0          # d h / d north (row 0 north)
    msn_on = on.mosaic(2)
    east = gx < -0.15; west = gx > 0.15; north = gy < -0.15; south = gy > 0.15
    g3 = {}
    for name, sel in (('all', np.ones_like(stamped)), ('stamped', stamped)):
        g3[name] = dict(R_east=float(msn_on[..., 0][east & sel].mean()), R_west=float(msn_on[..., 0][west & sel].mean()),
                        B_north=float(msn_on[..., 2][north & sel].mean()), B_south=float(msn_on[..., 2][south & sel].mean()),
                        n=[int((east & sel).sum()), int((west & sel).sum()), int((north & sel).sum()), int((south & sel).sum())])
    # refuter: the same test on a sheet with R and B swapped for 255-R / 255-B must invert
    g3['refuter_flipped'] = dict(R_east=float(255 - msn_on[..., 0][east].mean()), R_west=float(255 - msn_on[..., 0][west].mean()))
    out['G3'] = g3
    # ---- G4
    same = {}
    for role in (1, 4):
        ok = True
        for ty in range(on.v.tilesY):
            for tx in range(on.v.tilesX):
                if role == 4:
                    ok &= bool((on.tile(tx, ty, 4, True) == off.tile(tx, ty, 4, True)).all())
                else:
                    ok &= bool((sheet_blocks(on, tx, ty, 1) == sheet_blocks(off, tx, ty, 1)).all())
        same[str(role)] = ok
    out['G4_identical_roles'] = same
    json.dump(out, open(sys.argv[4], 'w'), indent=1)
    print(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
