p=r'E:\Projects\NifskopeWildWastelandEdition\tests\spells\pbr_r4_gates.py'
s=open(p,'rb').read().decode()
old="""def uv_region(img, u0, u1, v0, v1, margin=0.04):
    \"\"\"pixels whose uv lies inside [u0,u1] x [v0,v1] shrunk by `margin` (filter-safe)\"\"\"
    O, U, V = UVMAP"""
new="""def uv_of(img):
    O, U, V = UVMAP
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    inv = np.linalg.inv(np.array([[U[0], V[0]], [U[1], V[1]]]))
    dx, dy = xx - O[0], yy - O[1]
    return inv[0, 0] * dx + inv[0, 1] * dy, inv[1, 0] * dx + inv[1, 1] * dy


def uv_region(img, u0, u1, v0, v1, margin=0.04):
    \"\"\"pixels whose uv lies inside [u0,u1] x [v0,v1] shrunk by `margin` (filter-safe)\"\"\"
    O, U, V = UVMAP"""
assert s.count(old)==1; s=s.replace(old,new)
old="""    outside = ~PLANE                                     # the background around the plane"""
new="""    uu, vv = uv_of(src)                                  # the background: clear of the plane's
    outside = ~PLANE & ((uu < -0.05) | (uu > 1.05) | (vv < -0.05) | (vv > 1.05))   # filtered edge"""
assert s.count(old)==1; s=s.replace(old,new)
open(p,'wb').write(s.encode())
