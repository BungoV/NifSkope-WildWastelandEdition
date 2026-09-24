import refcard as R, numpy as np, math, copy
R.USE_DDS=True
cs_dds=R.CardSet('blast_n4'); c12=R.CardSet('blast_n12')
R.USE_DDS=False
cs_png=R.CardSet('blast_n4')
def dirOf(az,el):
    a=math.radians(az); e=math.radians(el)
    return np.array([math.cos(e)*math.cos(a), math.cos(e)*math.sin(a), math.sin(e)])
views=[(az,el) for el in (15,45) for az in range(0,360,30)]
res=(192,512)
T={v:R.truth(c12,dirOf(*v),res,cs_dds.half)[0] for v in views}

def block5(h):
    """What BC1-colour blue does to a channel: 5 bits, 4 levels per 4x4 block
       interpolated between two block endpoints."""
    H,W=h.shape
    out=np.empty_like(h)
    for y in range(0,H,4):
        for x in range(0,W,4):
            b=h[y:y+4,x:x+4]
            lo=np.round(b.min()*31)/31; hi=np.round(b.max()*31)/31
            pal=np.array([lo,hi,(2*lo+hi)/3,(lo+2*hi)/3])
            out[y:y+4,x:x+4]=pal[np.abs(b[...,None]-pal).argmin(-1)]
    return out

def variant(name, span, hmap, tag):
    cs=copy.copy(cs_png); cs.span=span
    cs.nrm=cs_png.nrm.copy(); cs.nrm[...,2]=hmap
    a15=[];a45=[]
    for v in views:
        m,_=R.render(cs,dirOf(*v),res=res)
        val=R.iou(m,T[v]); (a15 if v[1]==15 else a45).append(val)
    print('%-58s %-12s %.3f  %.3f  %.3f'%(name,tag,np.mean(a15),np.mean(a45),np.mean(a15+a45)))

hpng=cs_png.nrm[...,2]
# the object's real depth, measured off the FULLY covered texels of the sheet
cov=cs_png.alb[...,3]; full=cov>=250/255.0
d=(hpng[full]-0.5)*3072.0
tight=2*np.percentile(np.abs(d),99.5)
k=3072.0/tight
print('measured object depth (99.5th pct of fully-covered texels): %.0f world units'%tight)
print('depthSpan declared 3072  ->  %.1fx larger than the object it encodes'%k)
print('usable 8-bit height levels over the object: %d   BC1-blue levels: %.1f'%(int(255/k), 31/k))
print()
print('%-58s %-12s IoU15  IoU45  mean'%('variant','height ch'))
print('-- controls -------------------------------------------------------------------')
variant('SPEC as shipped, sheets as the GPU reads them',3072.0, cs_dds.nrm[...,2],'BC3 DDS')
variant('SPEC as shipped, sheets before compression',   3072.0, hpng,             'PNG 8-bit')
print('-- counterfactual: the SAME picture, a depthSpan fitted to the object --------')
ht=np.clip(0.5+(hpng-0.5)*k,0,1)
variant('height rescaled to a fitted span, 8-bit',       tight, np.round(ht*255)/255,'8-bit')
variant('height rescaled to a fitted span, BC1-blue',    tight, block5(ht),          'BC1 5-bit')
variant('height rescaled to a fitted span, 4-bit',       tight, np.round(ht*15)/15,  '4-bit')
print('-- counterfactual: declared span kept, height moved to an 8-bit block chan ---')
variant('span 3072, height quantised as BC3 ALPHA would',3072.0, np.round(hpng*255)/255,'8-bit exact')
