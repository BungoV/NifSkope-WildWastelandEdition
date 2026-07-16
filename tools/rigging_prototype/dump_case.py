import sys
sys.argv=['x','E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/BaseFemaleHead_faceBones.nif']
import numpy as np, nifshape, transfer

base='E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/'
donor=nifshape.load_shape(base+'BaseFemaleHead_faceBones.nif')
target=nifshape.load_shape(base+'Beards/BigBeard01_Hairline_faceBones.nif')

# global bone id map
allb=sorted(set().union(*[set(d) for d in donor['skin']]) | set().union(*[set(d) for d in target['skin']]))
bid={b:i for i,b in enumerate(allb)}

pred,_=transfer.transfer(donor,target,'face')

def skin_line(d):
    items=sorted(d.items(),key=lambda kv:-kv[1])[:4]
    while len(items)<4: items.append((None,0.0))
    return ' '.join(f'{bid.get(b,-1)} {w:.7f}' for b,w in items)

with open('case.txt','w') as f:
    f.write(f"DONOR {donor['nv']} {donor['ntri']} {len(allb)}\n")
    for p in donor['pos']: f.write(f"{p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n")
    for t in donor['tris']: f.write(f"{t[0]} {t[1]} {t[2]}\n")
    for d in donor['skin']: f.write(skin_line(d)+"\n")
    f.write(f"TARGET {target['nv']}\n")
    for p in target['pos']: f.write(f"{p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n")
    f.write("EXPECT\n")
    for d in pred: f.write(skin_line(d)+"\n")
print("wrote case.txt  donor nv",donor['nv'],"tris",donor['ntri'],"target nv",target['nv'],"bones",len(allb))
