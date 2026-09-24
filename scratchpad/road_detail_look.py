import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/roads3_20260911')
os.chdir(os.path.dirname(os.path.abspath(__file__)) + '/roads3_20260911')
import r3lib as R3
from PIL import Image, ImageDraw, ImageFont
t = 't2020'
V = R3.vanilla(t); D0 = R3.ours('new_default', t); D1 = R3.ours('rung_detail1', t)
mask = R3.road_mask(R3.ours('rung_roads', t), R3.ours('rung_noroads', t))
m = mask.astype(np.int64); c = np.pad(m.cumsum(0).cumsum(1), ((1,0),(1,0)))
s = 96; H, W = m.shape
win = c[s:, s:] - c[:-s, s:] - c[s:, :-s] + c[:-s, :-s]
y0, x0 = np.unravel_index(win.argmax(), win.shape); y1, x1 = y0+s, x0+s
zf = 4
def u8(a): return np.clip(a, 0, 255).astype(np.uint8)
try: font = ImageFont.truetype('consola.ttf', 15)
except Exception: font = ImageFont.load_default()
cols = [(V, 'VANILLA  chunk (-20,20)', (120,220,120)),
        (D0, 'ours, --road-detail 0  (current default)', (255,255,255)),
        (D1, 'ours, --road-detail 1  (the old look)', (255,190,80))]
pw = 512; gap = 12; capH = 44
out = Image.new('RGB', (3*pw + 4*gap, 40 + s*zf + capH + pw + capH + gap), (16,16,16))
d = ImageDraw.Draw(out)
d.text((gap, 10), 'The far road, chunk (-20,20) Sanctuary: --road-detail 0 (flat colour per material) vs 1 (the diffuse printed). Top 96x96 at 4:1, bottom the whole sheet. Both --road-opacity 1.', font=font, fill=(200,200,200))
for i, (a, title, tint) in enumerate(cols):
    x = gap + i*(pw+gap)
    crop = Image.fromarray(u8(a)[y0:y1, x0:x1]).resize((s*zf, s*zf), Image.NEAREST)
    out.paste(crop, (x + (pw - s*zf)//2, 40))
    d.text((x, 40 + s*zf + 6), title[:70], font=font, fill=tint)
    full = Image.fromarray(u8(a))
    yb = 40 + s*zf + capH
    out.paste(full, (x, yb))
    d.rectangle([x+x0, yb+y0, x+x1, yb+y1], outline=tint)
    L = R3.L(a); d.text((x, yb + pw + 6), 'road L %.2f   local 5x5 SD on road %.2f' % (L[mask].mean(), R3.local_sd(L)[mask].mean()), font=font, fill=tint)
p = os.path.abspath('../road_detail_look.png'); out.save(p); print(p, out.size, 'window', (x0, y0))
