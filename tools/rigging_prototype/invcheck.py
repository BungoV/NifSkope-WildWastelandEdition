import sys, numpy as np, importlib
def check(path):
    import nifparse
    # fresh parse via invbind's logic by reloading with argv
    sys.argv=['x',path]
    import invbind
    importlib.reload(invbind)
    ib=invbind
    Ms=[ib.world(ib.bonerefs[k],False)@ib.stored[k] for k in range(ib.nb)]
    mean=np.mean(Ms,0); spread=np.abs(np.array(Ms)-mean).max()
    errs=[np.abs(np.linalg.inv(ib.world(ib.bonerefs[k],False))@mean - ib.stored[k]).max() for k in range(ib.nb)]
    import os
    print(f"{os.path.basename(path):42s} bones={ib.nb:3d} meshBindWorld spread={spread:.4g} formula maxerr={max(errs):.3g} bindTrans={mean[:3,3].round(2)}")
base='E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/'
for f in ['BaseFemaleHead_faceBones.nif','BaseMaleHead_faceBones.nif','Beards/BigBeard01_faceBones.nif','Beards/BigBeard01_Hairline_faceBones.nif']:
    try: check(base+f)
    except Exception as e: print(f, 'FAIL', e)
