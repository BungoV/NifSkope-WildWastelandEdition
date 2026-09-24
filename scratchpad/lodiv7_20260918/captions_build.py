"""Build images/captions.md from the pictures that were actually taken.

Every number in a caption is a number in the report (s3, s5, s6, s7, s8, s9) or a
number this script read back out of the run's own log. The note line under each
picture is the viewer's OWN sentence, copied from that picture's log, so a
caption can never claim a channel the run did not draw.
"""
import glob
import os
import re

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'images')

CAP = {
 '1_v7_identity': (
  "`identity` on the v7 file: one colour a house",
  "The channel that used to paint every placement its own colour now paints the GROUP. "
  "588 groups over 2,449 placements on this chunk (report s3); 2,446 of them are in the "
  "chunk this camera sees. A kit house that was a confetti of walls is one flat colour."),
 '2_v7_placement': (
  "`placement` on the v7 file: the per-placement identity, which did not go away",
  "The new channel draws exactly what `identity` drew before v7 -- one colour per "
  "placement, 2,446 of them read here, ids 0..2,448 (report s9). Nothing was taken away "
  "by the change; it was given a name of its own."),
 '3_v6_identity': (
  "`identity` on a version-6 file: the fallback, and it SAYS it is the fallback",
  "The same channel, the same camera, a `.lodi` with no group table. It falls back to the "
  "per-placement identity -- and the note line below is the viewer's own, printed by the run "
  "that made this picture. A viewer that fell back silently would make a v6 file "
  "indistinguishable from a v7 one, which is the defect class the root MISTAKES entry of "
  "05:1x records."),
 '4_v7_sky': (
  "`sky` on the v7 file: the PER-VERTEX stream",
  "53,349 bytes over 2,446 slices in this view (53,396 over 2,449 in the whole file, report "
  "s6). The gradient WITHIN a single building -- dark at the base, open at the roof -- is the "
  "thing one byte a building could not say."),
 '5_v6_sky': (
  "`sky` on a version-6 file: the flat placement byte",
  "The same channel and camera on the v6 pair: one value for the whole placement, so every "
  "building is a single flat tone. Held beside picture 4 this is the whole argument for the "
  "stream. G4 asserts the two are not the same picture -- a per-vertex channel that rendered "
  "byte-identically to the flat one would not be wired."),
 '6_v7_ao': (
  "`ao` on the v7 file: the control, the channel this lane did not touch",
  "AO was already per-vertex before this lane and is unchanged by it. It is here so that a "
  "reader can see what a working per-vertex channel looks like on this same scene, and judge "
  "picture 4 against it rather than against a memory. AO's stream agrees with its own "
  "placement byte within 2 on 91.63% of placements where sky manages 72.44% (report s6); the "
  "correlations are 0.9878 and 0.9854."),
 '7_v7_group_largest': (
  "The largest group: 205 placements under one id",
  "Group (chunk 1, id 277), 205 placements drawn from 205 distinct refFormIds and 29 distinct "
  "meshes -- garage floors, shack roofs, lobby walls, a church end cap -- standing in a box "
  "2,712 x 1,697 units (report s5). It is ONE colour here. "
  "**What else is in frame, stated rather than cropped out:** no shipped knob draws a single "
  "group on its own, so this narrows the objects to the one CELL the group stands in "
  "(`WW_LODI_REGION=\"5,-11,5,-11\"`, src/lodinative.cpp). 200 further placements share that "
  "cell and belong to 26 other groups; they are the other colours. The alternative was a "
  "doctored `.lodi`, and a picture of bytes nobody shipped is not evidence."),
}
ORDER = ['1_v7_identity', '2_v7_placement', '3_v6_identity', '4_v7_sky',
         '5_v6_sky', '6_v7_ao', '7_v7_group_largest']

lines = ["# Lane LODIV7 -- the pictures, and what each one is evidence of",
         "",
         "CHANVIEW1 framing throughout (`WW_RENDER_CENTER=24900,-41300,450`, "
         "`WW_RENDER_ORTHO=2600`, `WW_RENDER_VIEW=8`, 1400x1091), which is the camera "
         "`lodl_channels.sh` uses, so a picture here and a picture there are the same view. "
         "Picture 7 reframes and says so.",
         "",
         "Taken by `scratchpad/lodiv7_20260918/pictures.sh`. The note line under each picture "
         "is the viewer's own sentence, read back out of that picture's log.",
         ""]

missing = []
for k in ORDER:
    png = os.path.join(OUT, k + '.png')
    log = os.path.join(OUT, k + '.log')
    title, body = CAP[k]
    note = ''
    if os.path.exists(log):
        for ln in open(log, encoding='utf-8', errors='replace'):
            m = re.search(r'WW_LODL_(?:CHANNEL=|AO:).*', ln)
            if m:
                note = m.group(0).strip()
                break
    if not os.path.exists(png):
        missing.append(k)
    size = os.path.getsize(png) if os.path.exists(png) else 0
    lines.append('## %s. %s' % (k.split('_')[0], title))
    lines.append('')
    lines.append('![%s](%s.png)' % (title, k))
    lines.append('')
    lines.append(body)
    lines.append('')
    if note:
        lines.append('> the run\'s own note line: `%s`' % note)
    else:
        lines.append('> NO NOTE LINE IN THE LOG -- this picture is not evidence of a channel.')
    lines.append('')
    lines.append('`%s.png`, %d bytes.' % (k, size))
    lines.append('')

open(os.path.join(OUT, 'captions.md'), 'w', encoding='utf-8', newline='').write('\n'.join(lines))
print('captions.md written, %d pictures, missing: %s'
      % (len(glob.glob(os.path.join(OUT, '*.png'))), missing or 'none'))
