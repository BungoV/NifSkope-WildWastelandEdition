# Lane LODIV7 -- the pictures, and what each one is evidence of

CHANVIEW1 framing throughout (`WW_RENDER_CENTER=24900,-41300,450`, `WW_RENDER_ORTHO=2600`, `WW_RENDER_VIEW=8`, 1400x1091), which is the camera `lodl_channels.sh` uses, so a picture here and a picture there are the same view. Picture 7 reframes and says so.

Taken by `scratchpad/lodiv7_20260918/pictures.sh`. The note line under each picture is the viewer's own sentence, read back out of that picture's log.

## 1. `identity` on the v7 file: one colour a house

![`identity` on the v7 file: one colour a house](1_v7_identity.png)

The channel that used to paint every placement its own colour now paints the GROUP. 588 groups over 2,449 placements on this chunk (report s3); 2,446 of them are in the chunk this camera sees. A kit house that was a confetti of walls is one flat colour.

> the run's own note line: `WW_LODL_CHANNEL=identity: the GROUP (.lodi v7 0x100) on 2446 placements, 588 groups in the file`

`1_v7_identity.png`, 74512 bytes.

## 2. `placement` on the v7 file: the per-placement identity, which did not go away

![`placement` on the v7 file: the per-placement identity, which did not go away](2_v7_placement.png)

The new channel draws exactly what `identity` drew before v7 -- one colour per placement, 2,446 of them read here, ids 0..2,448 (report s9). Nothing was taken away by the change; it was given a name of its own.

> the run's own note line: `WW_LODL_CHANNEL=placement: the placement identity, hashed to colour (the stock channel 1 palette) from Commonwealth.lodi, 2446 placements read; min 0, max 2448, mean 1224.894`

`2_v7_placement.png`, 95460 bytes.

## 3. `identity` on a version-6 file: the fallback, and it SAYS it is the fallback

![`identity` on a version-6 file: the fallback, and it SAYS it is the fallback](3_v6_identity.png)

The same channel, the same camera, a `.lodi` with no group table. It falls back to the per-placement identity -- and the note line below is the viewer's own, printed by the run that made this picture. A viewer that fell back silently would make a v6 file indistinguishable from a v7 one, which is the defect class the root MISTAKES entry of 05:1x records.

> the run's own note line: `WW_LODL_CHANNEL=identity: no group table in Commonwealth.lodi (a version-6 file), so the PLACEMENT IDENTITY served it on 2446 placements`

`3_v6_identity.png`, 95460 bytes.

## 4. `sky` on the v7 file: the PER-VERTEX stream

![`sky` on the v7 file: the PER-VERTEX stream](4_v7_sky.png)

53,349 bytes over 2,446 slices in this view (53,396 over 2,449 in the whole file, report s6). The gradient WITHIN a single building -- dark at the base, open at the roof -- is the thing one byte a building could not say.

> the run's own note line: `WW_LODL_CHANNEL=sky: the PER-VERTEX SKY STREAM (.lodi v7 0x110) from Commonwealth.lodi, 53349 bytes over 2446 slices, 53349 values read; min 0, max 255, mean 119.161`

`4_v7_sky.png`, 311486 bytes.

## 5. `sky` on a version-6 file: the flat placement byte

![`sky` on a version-6 file: the flat placement byte](5_v6_sky.png)

The same channel and camera on the v6 pair: one value for the whole placement, so every building is a single flat tone. Held beside picture 4 this is the whole argument for the stream. G4 asserts the two are not the same picture -- a per-vertex channel that rendered byte-identically to the flat one would not be wired.

> the run's own note line: `WW_LODL_CHANNEL=sky: no per-vertex stream in Commonwealth.lodi (a version-6 file), so the PLACEMENT BYTE (.lodi 0x11) served it`

`5_v6_sky.png`, 85012 bytes.

## 6. `ao` on the v7 file: the control, the channel this lane did not touch

![`ao` on the v7 file: the control, the channel this lane did not touch](6_v7_ao.png)

AO was already per-vertex before this lane and is unchanged by it. It is here so that a reader can see what a working per-vertex channel looks like on this same scene, and judge picture 4 against it rather than against a memory. AO's stream agrees with its own placement byte within 2 on 91.63% of placements where sky manages 72.44% (report s6); the correlations are 0.9878 and 0.9854.

> the run's own note line: `WW_LODL_AO: .lodi v6 scene vertex AO used on 2446 placements (53349 bytes, mean 177.0), 0 slices did not match the drawn mesh`

`6_v7_ao.png`, 351578 bytes.

## 7. The largest group: 205 placements under one id

![The largest group: 205 placements under one id](7_v7_group_largest.png)

Group (chunk 1, id 277), 205 placements drawn from 205 distinct refFormIds and 29 distinct meshes -- garage floors, shack roofs, lobby walls, a church end cap -- standing in a box 2,712 x 1,697 units (report s5). It is ONE colour here. **What else is in frame, stated rather than cropped out:** no shipped knob draws a single group on its own, so this narrows the objects to the one CELL the group stands in (`WW_LODI_REGION="5,-11,5,-11"`, src/lodinative.cpp). 200 further placements share that cell and belong to 26 other groups; they are the other colours. The alternative was a doctored `.lodi`, and a picture of bytes nobody shipped is not evidence.

> the run's own note line: `WW_LODL_CHANNEL=identity: the GROUP (.lodi v7 0x100) on 405 placements, 588 groups in the file`

`7_v7_group_largest.png`, 34933 bytes.
