import sys, os, numpy as np
H = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, H + '/splat1_20260911')
import splatlib as S
from PIL import Image, ImageDraw, ImageFont
def load(p): return S.Dds(p).level(0)[:, :, :3].astype(np.float64)
cx, cy = -20, 20
V = load(S.van_sheet(cx, cy))
def ours(v): return load(H + '/tiling4_20260912/out/%s/r_-20_20_-17_23/tex/Commonwealth.4.%d.%d.DDS' % (v, cx, cy))
cols = [(V, 'VANILLA  chunk (-20,20)', (120,220,120)),
        (ours('fs_rung'), 'ours, default (plain footprint sampling)', (255,255,255)),
        (ours('fs_warp'), 'ours, --land-sample warp', (120,180,255)),
        (ours('fs_stoch'), 'ours, --land-sample stochastic (hex tiling)', (255,190,80))]
x0, y0, s, zf = 300, 40, 128, 3
try: font = ImageFont.truetype('consola.ttf', 15)
except Exception: font = ImageFont.load_default()
def u8(a): return np.clip(a, 0, 255).astype(np.uint8)
pw, gap, capH = 512, 12, 30
out = Image.new('RGB', (4*pw + 5*gap, 40 + s*zf + capH + pw + capH + gap), (16,16,16))
d = ImageDraw.Draw(out)
d.text((gap, 10), 'Repeat-hiding tricks, chunk (-20,20) Sanctuary, real bakes from lane TILING4 (exe 02:08:57). Top: a 128x128 open-ground window at 3:1. Bottom: the whole 512 sheet at 1:1.', font=font, fill=(200,200,200))
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
p = H + '/tiling_look.png'; out.save(p); print(p, out.size)
