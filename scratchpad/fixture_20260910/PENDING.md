# PENDING — lane FIXTURE, the two bind-pose pictures

Everything else in lane FIXTURE is done and gated (see
`scratchpad/lane_fixture_report.md`). **Fallout4.exe came up at 05:32:15 on
2026-09-10**, so `release/NifSkope.exe` was not launched again and the two
render-hook pictures were not taken. Nothing else is owed.

No build is needed: `release/NifSkope.exe` (2026-09-10 03:57) is what took the
merge and the `segments` read-back, and this lane changed no source.

## Resume, paste-able

```bash
cd /e/Projects/NifskopeWildWastelandEdition
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # must print rc=1
NS=release/NifSkope.exe
OUT=E:/Projects/NifskopeWildWastelandEdition/fixtures
F="E:/Projects/NifskopeWildWastelandEdition/fixtures/human_male_vanilla.nif"

# ViewState: ViewDefault 0, ViewTop 1, ViewBottom 2, ViewLeft 3, ViewRight 4,
# ViewFront 5, ViewBack 6 (src/glview.h:291).  WW_RENDER_VIEW alone keeps the
# old auto-fit framing, which is what is wanted here (whole body in frame).
for v in 5 4; do
  case $v in 5) nm=front;; 4) nm=side;; esac
  WW_WINDOW_AT=1960,40 \
  WW_RENDER_SHOT="$OUT/human_male_vanilla_bindpose_$nm.png" \
  WW_RENDER_SIZE=900x1400 WW_RENDER_VIEW=$v WW_RENDER_CLEAN=1 \
    timeout 180 "$NS" --port 4233$v "$F" >/dev/null 2>&1
  echo "$nm rc=$?  $(ls -la "$OUT/human_male_vanilla_bindpose_$nm.png" 2>&1 | tail -1)"
done
python -c "from PIL import Image; import glob
for p in sorted(glob.glob(r'$OUT/human_male_vanilla_bindpose_*.png')): print(p, Image.open(p).size)"
grep -c . release/ww_headless_windows.log
```

Traps that apply here, all from `nifskope-ww-render-shot`:

* **every `WW_*` output path must be ABSOLUTE.** A relative `WW_RENDER_SHOT`
  writes nothing and says nothing. (The same trap bit this lane once already:
  `nifskope-cli merge -o scratchpad/…/trial1.nif` printed
  `error: failed to save` — see MISTAKES.md.)
* print the size of the file each run wrote, or `NO FILE`; `>/dev/null 2>&1`
  hides an exec failure otherwise.
* `WW_RENDER_SIZE` is clamped to one screen — read the real size back with PIL,
  never quote the requested one.
* `rc=124` with a PNG already on disk is the save-confirmation dialog, not a
  slow render; this exe has the headless guard, so check
  `release/ww_headless_close.log`.
* one instance at a time, each run its own unused `--port`, second monitor only.

## What the pictures are for

They are the bind-pose baseline the animation-preview work compares against:
the whole model in `skeleton.nif`'s reference pose, front and right side, so a
later playback frame can be put beside them from the same framing (CONSTITUTION
rule 5). Drop them in `fixtures/` and add the two file names to
`fixtures/README.md` under "What was measured".
