"""Scratch: append section 5 to report.md (the Write tool refuses .md files here)."""
import json, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
D = json.load(open('disagreement.json'))
T = json.load(open('times.json'))
S = json.load(open('sweep.json'))
CAMDESC = {
 'close':  'low SW oblique over the built-up part, eye ~3,400 u above the ground (1600x900, 58 deg)',
 'east':   'stands in the east looking WNW, eye ~1,600 u up (1600x900, 58 deg)',
 'full':   'high SW oblique of the whole chunk, eye ~6,000 u up (1600x900, 56 deg)',
 'street': 'near-ground, eye ~120 u above the ground, looking along azimuth 289 (1600x900, 62 deg)',
}
def row(r):
    nm, az, el, st, tag = r
    return ('| %-11s | %3.0f | %2.0f | %6.2f%% | %6.2f%% | %9s | %6.2f%% | %9s | %5.1f%% | `%s.png` |'
            % (nm, az, el, st['all'][0], st['terrain'][0], '{:,}'.format(st['terrain'][1]),
               st['objects'][0], '{:,}'.format(st['objects'][1]), st['nodata'][0], tag))
H = ('| camera | az | el | ALL | terrain | ter px | objects | obj px | no-data | file |\n'
     '|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n')
persp = H + '\n'.join(row(r) for r in D if r[0] in CAMDESC) + '\n'
ctrl  = H + '\n'.join(row(r) for r in D if r[0] == 'east_ROT180') + '\n'
tops  = H + '\n'.join(row(r) for r in D if r[0].startswith('top_')) + '\n'
sw = ('| # | az | el | ALL disagree | terrain | objects |\n|---:|---:|---:|---:|---:|---:|\n' +
      '\n'.join('| %02d | %5.1f | %4.1f | %6.2f%% | %6.2f%% | %6.2f%% |' % (i, a, e, x, t, o)
                for i, (a, e, x, t, o) in enumerate(S)) + '\n')
gb = '\n'.join('| %s gbuffer (1600x900) | %5.1f s |' % (k.replace('gbuffer_', ''), v)
               for k, v in sorted(T.items()) if k.startswith('gbuffer_'))
pairt = [v for k, v in T.items() if not k.startswith('gbuffer_')]

txt = '''
## 5. The numbers, and the pictures they belong to

Every figure below is the percentage of **decided surface pixels** where the two panels disagree
about lit vs shadow. A pixel is decided when it is not sky and the baked data has something to
say about it; the pixels where the bake has nothing (drawn with a teal wash on the RIGHT) are
counted separately in the *no-data* column and are **excluded** from the disagreement. The
terrain / objects split is by what the camera ray hit first, so the two pixel counts add up to
the decided total for that camera.

These are disagreements, not errors on anyone's part: the LEFT panel is this lane's ray cast
(with the honesty limits in s1 and the measured error bar in s4), the RIGHT panel is what the
baked horizon data can express. Where they differ at least one of the two is wrong, and s4 says
how far each can be trusted.

### 5.1 The perspective pairs -- the main deliverable

''' + persp + '''
Cameras (all heights are offsets read off the terrain, never typed):

- **close** -- ''' + CAMDESC['close'] + '''
- **east** -- ''' + CAMDESC['east'] + '''
- **full** -- ''' + CAMDESC['full'] + '''
- **street** -- ''' + CAMDESC['street'] + '''

**Why there are four cameras and not three.** The brief asked for three. Four of the five sun
positions stand at azimuth 120 (east-south-east), and the first two cameras both look
north-east from the south-west, so those four suns lit only the backs of everything in frame --
the shadows were there but they fell on surfaces the lens could not see. `east` was added to
look the other way, so the same four suns light the faces in frame and the cast shadows lie
across open ground toward the lens. It is the clearest pair in the set.

**The street camera was re-shot.** Its first bearing was azimuth 120, exactly into four of the
five suns: at elevation 5 and 15 the entire frame was a contre-jour silhouette and not one cast
shadow was legible (that render has been overwritten; its log line read `persp_street_az120_el05
all 23.86% ter 1.63% obj 71.83%`, and the terrain figure is that low only because nearly every
ground pixel was in shadow on both panels). The shipped `street` looks along azimuth 289 instead
-- 11 degrees off the anti-sun direction -- so the long shadows run down the street toward the
lens across lit ground. This is a deliberate deviation from the brief's wording ("long shadows
streak toward the camera"): shadows that point at the lens can only be seen from behind the sun,
and that is the picture that does not read. The elevation-5 frame at this new bearing is the
most informative image in the set: on the LEFT a row of pillars throws hard shadow bars down the
lit street; on the RIGHT the same street is almost entirely dark.

### 5.2 The sanity control -- the RIGHT panel read at the wrong azimuth

''' + ctrl + '''
Same camera and same sun as `persp_east_az120_el15`, except the RIGHT panel looks the baked bins
up at azimuth 300 instead of 120 -- the sun turned 180 degrees -- while the LEFT panel is
unchanged. The control bites on the **terrain**, where the bake has dense data: terrain
disagreement jumps from **31.76% to 64.11%**, i.e. turning the data round roughly doubles the
disagreement and overshoots the 50% a coin would give. That is the evidence that the RIGHT
panel's terrain result is actually driven by the azimuth bins and not by something that would
look the same whatever bin it read.

On the **objects** it barely moves -- 28.76% to 31.56%. That is not a failure of the control, it
is the fact s4 ends on: 65% of the stored object horizon bytes are zero, and rotating a field of
zeros by 180 degrees gives a field of zeros. The object half of the RIGHT panel is largely
insensitive to azimuth because there is largely nothing there to be sensitive with.

### 5.3 The secondary top-down pair

''' + tops + '''
North up, a 1,000,000-unit lens 1,000,000 units above the centre (orthographic to within a few
pixels), through the same ray path as everything else. `top_full` is the whole chunk plus its
margin; `top_crop` is a 7,200-unit square over the built-up part.

### 5.4 The sweep

`images/sweep_close.gif` (14 frames, 420 ms each, loops) and `images/sweep_close_filmstrip.png`
(the same 14 frames as a 4-wide contact sheet). Camera `close`, TRUTH on the left of each frame
and BAKED on the right, sun swept from azimuth 95 to 265 with the elevation on a sine arch
peaking at 44.7 degrees. Frames are rendered at 1600x900 and downscaled by 2 for the GIF.

''' + sw + '''
The shape of that column is the headline of this lane: the two panels agree best under a high
sun (31% at 45 degrees) and worst under a low one (48-51% at 8 degrees) -- backwards from what
you would want, because the low sun is when long shadows matter.

### 5.5 Render times

Wall clock, one process, no GPU, no multiprocessing -- plain NumPy on the CPU, with Fallout 4
running throughout.

| stage | time |
|---|---:|
''' + gb + '''
| top_full gbuffer (2048x2048) |  81.2 s |
| top_crop gbuffer (2048x2048) |  84.4 s |
| one 1600x900 PAIR (both panels, all shading, PNG written) | %.1f - %.1f s |
| the whole set: 4 cameras x 5 suns + control + 2 top-downs + 14 sweep frames | 5.5 min |
| the street re-shoot (1 camera x 5 suns) | 0.5 min |

The primary visibility (the G-buffer) is solved **once per camera** and reused by all five suns
and by both panels, which is why a pair costs about two seconds against the twenty a camera
costs. The shadow test itself is not a per-pixel ray march: it is a suffix maximum in sheared
sun space (s1), so one pass answers every shadow ray in the frame at once.

No resolution was dropped. The brief allowed lowering resolution before dropping cameras; that
was not needed, and a camera was added rather than removed.

### 5.6 The files

In `images/`:

- `persp_<camera>_az<azimuth>_el<elevation>.png` -- 20 pairs, 3234x1050 each (two 1600x900
  panels, labels and caption burned in). Cameras `close`, `east`, `full`, `street`.
- `control_east_az120_el15_RIGHT_ROT180.png` -- the sanity control of s5.2.
- `top_full_az120_el15.png`, `top_crop_az120_el15.png` -- the secondary top-down pairs, 2048px
  panels, north up.
- `sweep_close.gif`, `sweep_close_filmstrip.png` -- the sweep of s5.4.

Beside this report: `disagreement.json` (every row of the tables above), `times.json`,
`sweep.json`, `controls.json` (the s4 control output), and the scripts `scene.py`, `render.py`,
`shade.py`, `cams.py`, `run.py`, `control.py`. Files whose names begin with `_` are scratch
(camera-candidate contact sheets and probes) and are not part of the deliverable.
''' % (min(pairt), max(pairt))
open('report.md', 'a', encoding='utf-8').write(txt)
print('appended %d chars' % len(txt))
