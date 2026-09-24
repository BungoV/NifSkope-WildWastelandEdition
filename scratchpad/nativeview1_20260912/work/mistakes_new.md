
6. **I assumed the `.BTR` and the `.BTO` of one chunk share a coordinate space,
   and photographed the `.BTO` at the wrong camera.** The legacy control frame
   came back as an empty 5,179-byte PNG and gate (c) read IoU 0.0000, which I
   first read as "the native scene is wrong". It was the camera. Read from the
   files: the `.BTR`'s shape sits at translation 0,0,0 over chunk-local vertices
   (0..4096 with a shape scale of 4), so it needs a CHUNK-LOCAL camera
   (`8192,8192,0`); the `.BTO`'s `BSSubIndexTriShape` carries Translation
   (-81920, 98304, 0), so it is in WORLD space and takes the same camera as the
   `.lodi`. Two files of one bake, two spaces. THE RULE: before blaming the
   thing under test for an empty frame, read the translation of the block you
   are photographing. A camera is an input, not a background fact.

7. **`grep -c` prints `0` AND exits 1, so `$(grep -c ... || echo 0)` produced
   `0` twice** and a census capture became the two-line string `0\n0`. The
   comparison then failed with a message that looked like a real count mismatch
   ("0 != 676") in a gate that was otherwise correct. Fixed with
   `X=$(grep -cv '^#' "$F" 2>/dev/null | head -1); X="${X:-0}"` in all three
   captures. THE RULE: `|| echo` after a command that already printed its answer
   appends a second answer. Default an EMPTY capture, do not fall back over a
   non-zero exit.

8. **The heredoc-backslash trap a THIRD time, and this one cost a render run.**
   A patch script for `shots.sh` was passed through an inline `python -c`; the
   continuation backslashes were eaten, the script's asserts never ran on the
   text I thought I had written, the file was left unpatched, and ten frames
   were re-rendered at the old camera before the diff showed it. Entry 2 of this
   same lane is the same mistake. THE RULE, now with a cost attached: patch
   scripts are written with the Write tool, always, and a patch is verified by
   re-reading the target's changed line, not by the patch script's own exit code.

9. **I built a shader block that pointed at a model-space normal map without
   declaring it one, and the land came back 40 percent too dark.** The sheet
   beside a `.lodl` is an `_msn`; read as a tangent-space map it lights wrong
   (mean luma 70.6 against the `.BTR`'s 121.1, MAD 50.457). The answer was in
   the bake's own `.BTR` the whole time: Shader Flags 1 = 0x80401000
   (`Model_Space_Normals` set, `Specular` clear) and Shader Flags 2 = 3
   (`ZBuffer_Write` | `LOD_Landscape`), with the bit meanings read from
   `build/nif.xml`. After setting those three bits: MAD 35.821, mean luma 92.5.
   THE RULE: when a viewer route reproduces a file the bake already writes, read
   that file's OWN flags and copy them; do not hand-write a shader block from
   what the texture slots seem to need.
