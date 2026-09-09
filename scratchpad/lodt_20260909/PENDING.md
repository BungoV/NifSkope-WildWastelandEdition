# `.lodt` open — BUILD PENDING (second time)

State at 2026-09-09 14:49, lane LODTOPEN2.

`Fallout4.exe` is UP (pid 16884, started 14:34:38, 14.0 GB and climbing — bungo
is playing). CONSTITUTION rule 6: game up = the lane ends BUILD PENDING, never
builds beside it, never polls for it. Nothing in this folder yet.

`release/NifSkope.exe` is **13:42:03**; the newest changed source is
`src/btdterrain.cpp` at **14:40:29**. A render taken now would photograph the
OLD code, so no picture is better than a wrong picture.

**No NifSkope process is running** (checked 14:47) — the exe is NOT held, so the
rename-aside step will be a no-op unless bungo opens a window before the build.
Check again anyway; that is what the wrapper does.

## What is already proven, without a build

* `g++ -fsyntax-only` on all four changed translation units — `lodtfile.cpp`,
  `btdterrain.cpp`, `nifcli.cpp`, `nifskope.cpp` — **RC=0 each**, re-run by the
  director 14:47. Only the known pre-existing Qt/libstdc++ `-Wsfinae-incomplete`
  noise. It proves they COMPILE; it proves nothing about linking, about moc, or
  about behaviour.
* Line endings, by Python byte count: every changed file matches its
  neighbours. `src/nifskope.cpp` is a MIXED file (9234 CRLF, 1123 bare LF) and
  its CR count went **9191 → 9234, delta +43** = exactly the 44 CRLF lines added
  minus the 1 CRLF line removed. The 5 added lines that are bare LF sit inside
  `validExternalNifPaths`, which was **already an LF-only block at HEAD** — they
  match their neighbours, which is the rule. `git diff --numstat` on
  `src/nifskope.cpp` is **49 / 2**, small as required.

## The resume, in order. Re-check the game before EVERY step below.

```bash
tasklist | grep -i fallout4        # must be empty, or STOP and end BUILD PENDING again
cd /e/Projects/NifskopeWildWastelandEdition
bash tools/ww_build.sh src/btdterrain.cpp src/lodtfile.cpp src/nifskope.cpp src/nifcli.cpp
```

That wrapper is the whole gated chain: game-down check, the exe renamed aside if
a window holds it, `make -j2` on **its own exit code**, exe-newer-than-sources,
the link-time `style.qss` copy. Read `BUILD-RC=` and the exe timestamp; a green
harness on a stale exe is the failure this wrapper exists to prevent.

Then, with the exe proven newer than all four:

```bash
export WW_WINDOW_AT=1960,40
L='E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodt'
D=E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodt_20260909

# G1 writer byte identity — the writer was not touched, only the reader's
# accessors and its cache, and a cache changes speed, never values.
bash tests/spells/lodt_write.sh 2>&1 | tail -6

# G3 the new harness, 23 checks, right-hand side = lodt_open_authority.py
bash tests/spells/lodt_open.sh 2>&1 | tail -20

# G4 the suite the change reaches, and why: lodt_btd.sh because the READER is
# shared, btd_terrain.sh because the tile mesher MOVED under it (this is the
# gate that proves the .btd scene is unchanged, byte for byte).
bash tests/spells/lodt_btd.sh 2>&1 | tail -6
bash tests/spells/btd_terrain.sh 2>&1 | tail -6
```

Skipped in G4, and say so: the collision, block-list, impostor, atlas, array,
merge and VT harnesses read no `.lodt` and build no terrain surface.

### Timing and memory (brief step 3 — MEASURED, not computed)

The report's §4 memory table is arithmetic off the file header. Replace it with
a measurement: wall time and peak working set of the bare whole-worldspace open.

```bash
python - <<'PY'
import subprocess,time,os
exe=r'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe'
L=r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodt'
D=r'E:\Projects\NifskopeWildWastelandEdition\scratchpad\lodt_20260909'
env=dict(os.environ, WW_WINDOW_AT='1960,40',
         WW_RENDER_SHOT=D+r'\lodt_open_commonwealth.png',
         WW_RENDER_VIEW='1', WW_RENDER_SIZE='1400x1400')
t=time.time(); p=subprocess.Popen([exe,'--port','42331',L],env=env)
peak=0
try:
    import psutil; pr=psutil.Process(p.pid)
    while p.poll() is None:
        try: peak=max(peak,pr.memory_info().peak_wset)
        except Exception: break
        time.sleep(0.25)
except ImportError:
    pass
p.wait(); print('wall %.1f s  peak working set %.1f MB' % (time.time()-t, peak/1048576))
PY
```

If `psutil` is absent, take the peak from
`Get-Process NifSkope | Select PeakWorkingSet64` in a parallel PowerShell poll,
or run the CLI twin `-no-gui lodt "$L" --region -96 -96 95 95 --lod 2 -o out.nif`
under the same timer. **State which was measured and which is arithmetic.**

### G5 — the eleven pictures

```bash
# 1. the whole worldspace, heights, top-down (also the timing run above)
WW_RENDER_SHOT="$D/lodt_open_commonwealth.png" WW_RENDER_VIEW=1 WW_RENDER_SIZE=1400x1400 \
  timeout 900 release/NifSkope.exe --port 42331 "$L"

# 2. Boston, zoomed: cells [-8,-8]..[7,7] at the file's own rate
WW_LODT_REGION="-8,-8,7,7,0" WW_LODT_PLANE=height \
WW_RENDER_SHOT="$D/lodt_open_closeup.png" WW_RENDER_SIZE=1200x1200 WW_RENDER_VIEW=1 \
  timeout 600 release/NifSkope.exe --port 42332 "$L"

# 3-11. one per plane, SAME camera and region for all nine
for p in height ao blend colour waterheight watertype cellflags cellrange overview; do
  WW_LODT_REGION="-96,-96,95,95,2" WW_LODT_PLANE=$p WW_RENDER_FLAT=1 \
  WW_RENDER_SHOT="$D/lodt_open_$p.png" WW_RENDER_SIZE=1400x1400 WW_RENDER_VIEW=1 \
    timeout 900 release/NifSkope.exe --port 42333 "$L"
done
```

One NifSkope instance at a time (the loop is sequential on purpose); second
monitor; each run needs an unused `--port`; the app exits by itself after the
grab; **absolute paths on every exe argument** — a relative one resolves against
the exe's folder and lands in `release/` (lane IMAGES lost eight files that way).

**OPEN ALL ELEVEN PNGs AND LOOK AT THEM before delivering.** A caption or label
off the edge is a mistake, and counts do not see it (MISTAKES.md, lane IMAGES2).
Two planes that render the same picture mean the selector ignored its argument —
`lodt_open.sh` has a floor for exactly that, but the eye is the second check.

`groundcover` is deliberately absent from the nine: Commonwealth.lodt carries
none (section flags `0xd`), and asking for it must REFUSE in words. Capture that
refusal as text beside the pictures — a refusal that names its reason is the
MODULES-AND-FALLBACKS rule, so it is a deliverable, not an error:

```bash
release/NifSkope.exe -no-gui lodt "$L" --plane groundcover --region -20 24 -19 25 --lod 2 \
  > "$D/lodt_open_groundcover_refusal.txt" 2>&1; echo "rc=$?" >> "$D/lodt_open_groundcover_refusal.txt"
```

### G2 parity, same session

```bash
B='E:/SteamLibrary/steamapps/common/Fallout 76 Playtest/Data/Terrain/EXM1PittWorldspace.btd'
release/NifSkope.exe -no-gui lodgen --from-btd "$B" --lodt "$D/pitt"
```

Then render the `.btd` and the converted `.lodt` from ONE pinned camera, **LIT**
(a flat render has no shading to differ), and report max |dz| beside the pixel
diff. Expect a bounded, EXPLAINED difference, not zero: `.btd` heights are
range-normalised and `.lodt` heights are quantised on a fixed 32767 bias
(`docs/LODGEN_BTD_FORMAT.md`, Height encoding), so the round trip costs half a
quantum — 0.32 units on this worldspace. Verify the camera pin actually took
(two distances must give two different files) before trusting the diff.

Fixtures confirmed on disk 14:47:
`Commonwealth.lodt` 35,953,286 B (09-05 03:14) · `EXM1PittWorldspace.btd`
1,581,073 B (07-12 01:04).

### Last

Append the results to `scratchpad/lane_lodt_open_report.md` as a new numbered
section with the exe mtime beside the newest source mtime; replace the
"NOT MEASURED AND OWED" paragraph at the top of `WW_CHANGES.md` with the
measured numbers; add any new mistakes to `MISTAKES.md` at the repo root.
**Commit nothing** — bungo's "Not yet" stands (78 uncommitted paths).
Tell him his open window needs a restart to get the new exe.
