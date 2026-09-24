import sys, os, numpy as np
H = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, H + '/splat1_20260911')
import splatlib as S
from PIL import Image, ImageDraw, ImageFont
def load(p): return S.Dds(p).level(0)[:, :, :3].astype(np.float64)
cx, cy = -20, 20
V = load(S.van_sheet(cx, cy))
def ours(v): return load(H + '/tiling4_20260912/out/%s/t2020/tex/Commonwealth.4.%d.%d.DDS' % (v, cx, cy))
cols = [(V, 'VANILLA  chunk (-20,20)', (120,220,120)),
        (ours('dw_0'), 'ours, no warp (--road-detail 1)', (255,255,255)),
        (ours('dw_170'), 'ours, --land-warp 170 (half a repeat)', (150,200,255)),
        (ours('dw_340'), 'ours, --land-warp 340 (one repeat)', (120,180,255)),
        (ours('dw_hex'), 'ours, hex tiling (--land-sample stochastic)', (255,190,80))]
x0, y0, s, zf = 300, 40, 128, 3
try: font = ImageFont.truetype('consola.ttf', 15)
except Exception: font = ImageFont.load_default()
def u8(a): return np.clip(a, 0, 255).astype(np.uint8)
pw, gap, capH = 512, 12, 30
out = Image.new('RGB', (5*pw + 6*gap, 40 + s*zf + capH + pw + capH + gap), (16,16,16))
d = ImageDraw.Draw(out)
d.text((gap, 10), 'Warp strength sweep, chunk (-20,20) Sanctuary, baked 05:14 on the 04:10:38 exe with --road-detail 1 everywhere (the shipped warp preset is 683). Top: 128x128 open ground at 3:1. Bottom: the whole sheet.', font=font, fill=(200,200,200))
for i, (a, title, tint) in enumerate(cols):
    x = gap + i*(pw+gap)
    crop = Image.fromarray(u8(a)[y0:y0+s, x0:x0+s]).resize((s*zf, s*zf), Image.NEAREST)
    out.paste(crop, (x + (pw - s*zf)//2, 40))
    d.text((x, 40 + s*zf + 6), title, font=font, fill=tint)
    yb = 40 + s*zf + capH
    out.paste(Image.fromarray(u8(a)), (x, yb))
    d.rectangle([x+x0, yb+y0, x+x0+s, yb+y0+s], outline=tint)
    L = 0.299*a[:,:,0]+0.587*a[:,:,1]+0.114*a[:,:,2]
    d.text((x, yb + pw + 6), 'mean L %.1f   local 5x5 SD %.2f' % (L.mean(), np.std(np.lib.stride_tricks.sliding_window_view(L,(5,5)),axis=(2,3)).mean()), font=font, fill=tint)
p = H + '/warp_sweep.png'; out.save(p); print(p, out.size)
