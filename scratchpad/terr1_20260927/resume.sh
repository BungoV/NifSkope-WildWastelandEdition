#!/bin/bash
# TERR1 resume (continuation 2026-09-27): every step that needs a build or a headless NifSkope. In the
# continuation session the harness refused to run build.sh at all, so all of this is still owed.
# Each bake.sh / shot.sh call takes the turn lock itself (turn.sh acquire terr1) and drops it on exit; this
# script never touches the lock. build.sh waits for any other lane's build first.
#   usage: bash resume.sh [step ...]
#   steps: build diag on off noroads noflat gates phys pics clean   (default: all but diag and clean)
# Exe for the bakes: runs/sky3 = commit 6eb5954f (the cosine sky law). Renders use runs/base (renderer untouched).
# A fresh run copy can be exec-blocked by the antivirus for minutes (skill ww-gui-launch-silent-exit,
# nifskope-ww-worktree-build section 8): retry, never kill.
set -u
T=/e/Projects/NifskopeWWE-terr1/scratchpad/terr1_20260927
W=/e/Projects/NifskopeWWE-terr1
cd $T || exit 1
C=mod/FO4CSLOD/Commonwealth
V=$C/Commonwealth.VT.2.lodt
# the objects the pictures draw: the ON bake's own .lodo/.lodi (the ao2/reg_x7 folder of the first session is gone)
OD=$T/bakes/on/$C
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
F=$SP/ao2/terr_x1
STEPS=${*:-build on off noroads noflat gates phys pics}
game() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }

# 0. the exe of the committed law (src must be at 6eb5954f or later, with no TERR1-DIAG line in it)
if has build; then
	grep -q "TERR1-DIAG" $W/src/lodgen.cpp && { echo "src carries the diagnostic patch; reverse it first"; exit 1; }
	bash build.sh sky3 src/lodgen.cpp || exit 1
fi
# 1. ON bake (defaults: stamp + sky union), with the object height field dumped for the sky gates
if has on; then game
	bash bake.sh sky3 bakes/on --dump-object-ao "E:/Projects/NifskopeWWE-terr1/scratchpad/terr1_20260927/bakes/objh_on.bin" || exit 1
	grep -E "^(stampNormal|skyObjects|objAo|terrainObjectAo)" bakes/on/$C/*.txt bakes/on/bake.log 2>/dev/null | head -20
fi
# 2. OFF bake: both TERR1 switches off -> must equal night-20260927 (bakes/base) byte for byte
if has off; then game
	bash bake.sh sky3 bakes/off --no-stamp-normals --no-sky-objects || exit 1
	bad=0
	for f in bakes/base/$C/*.lodt bakes/base/$C/*.lodm; do
		cmp -s "$f" "bakes/off/$C/$(basename "$f")" && echo "same $(basename "$f")" || { echo "DIFF $(basename "$f")"; bad=1; }
	done
	echo "OFF gate: $([ $bad = 0 ] && echo PASS || echo FAIL)"
	# refuter: one flipped byte in a copy must make cmp fail
	cp bakes/off/$V bakes/refuter.lodt
	python -c "import sys;p=sys.argv[1];b=bytearray(open(p,'rb').read());b[len(b)//2]^=1;open(p+'.x','wb').write(bytes(b))" bakes/refuter.lodt
	cmp -s bakes/base/$V bakes/refuter.lodt.x && echo "REFUTER BROKEN (cmp passed a flipped byte)" || echo "refuter ok: flipped byte detected"
	rm -f bakes/refuter.lodt bakes/refuter.lodt.x
fi
# 3./4. the mask bakes for the normal gates
if has noroads; then game; bash bake.sh sky3 bakes/noroads --no-roads || exit 1; fi
if has noflat; then game; bash bake.sh sky3 bakes/noflat --no-flat-objects || exit 1; fi
# 5. offline gates (no NifSkope)
if has gates; then
	python nrmgate.py bakes/on/$V bakes/off/$V bakes/noroads/$V nrm.json
	python g2diag.py bakes/on/$V bakes/off/$V bakes/noroads/$V g2diag.json > g2diag.log; head -30 g2diag.log
	python railprof.py bakes/on/$V bakes/off/$V bakes/noflat/$V bakes/noroads/$V rail.json
	python canyon.py bakes/off/$V bakes/on/$V bakes/objh_on.bin cellnames.json canyon.json 4
	for r in base on off noroads noflat; do echo "$r $(cat bakes/$r/rc.txt 2>/dev/null)"; done
fi
# 5a. G2 diagnostic (not in the default list; needs bakes/off and bakes/noroads): build a diagnostic exe with
#     stamp_diag.patch, put the source back, bake once with the per-texel stamp record, read it (g2stampdiag.py;
#     the pre-registered reading A/B/C is in its header). The diagnostic only adds a file write: its sheets must
#     equal bakes/on's (checked below), so the G2 numbers it reads are the shipped code's.
if has diag; then game
	patch --dry-run -p1 -d $W -i $T/stamp_diag.patch > /dev/null || { echo "stamp_diag.patch does not apply"; exit 1; }
	patch -p1 -d $W -i $T/stamp_diag.patch && bash build.sh diag1 src/lodgen.cpp; brc=$?
	patch -R -p1 -d $W -i $T/stamp_diag.patch
	grep -q "TERR1-DIAG" $W/src/lodgen.cpp && { echo "diagnostic NOT reversed -- fix src before anything else"; exit 1; }
	[ $brc = 0 ] || exit 1
	WW_TERR1_STAMP_DIAG="E:/Projects/NifskopeWWE-terr1/scratchpad/terr1_20260927/bakes/stamp_diag.bin" \
		bash bake.sh diag1 bakes/diag || exit 1
	cmp -s bakes/diag/$V bakes/on/$V && echo "diag sheets == on sheets" || echo "diag sheets DIFFER from on (read with care)"
	python g2diag.py bakes/diag/$V bakes/off/$V bakes/noroads/$V g2diag_diag.json > /dev/null
	python g2stampdiag.py bakes/stamp_diag.bin bakes/diag/$V bakes/off/$V g2diag_diag.json g2stamp.json
fi
# 5b. the sky law on the bake against the physical cosine-weighted cast (the reference), 312 texels, ~90 s
if has phys; then
	python physlaw.py bakes/on/$V bakes/off/$V bakes/objh_on.bin bakes/on/$C/Commonwealth sky_on.json physlaw_bake.json 12 120 60 > physlaw_bake.log 2>&1
	python lawfit.py physlaw_bake.json phys1458 | tee lawfit_bake.log
	python -c "
import json, numpy as np
R = json.load(open('physlaw_bake.json'))['rows']
for c in ('canyon', 'open', 'near', 'deck'):
    s = [r for r in R if r['cls'] == c]
    a = np.array([r['maskB_after'] for r in s]); b = np.array([r['maskB_before'] for r in s])
    p = np.array([r['phys1458'] for r in s]); q = np.array([r['phys10k'] for r in s])
    print('BAKED', c, len(s), 'maskB after %.1f before %.1f | phys 1458 %.1f 10k %.1f | MAE %.1f bias %+.1f' % (
        a.mean(), b.mean(), p.mean(), q.mean(), np.abs(a - p).mean(), (a - p).mean()))
for cell in sorted({r['cell'] for r in R if r['cls'] == 'canyon'}):
    s = [r for r in R if r['cell'] == cell]
    print('BAKED street', cell, s[0]['name'], 'maskB %.1f phys1458 %.1f' % (
        np.mean([r['maskB_after'] for r in s]), np.mean([r['phys1458'] for r in s])))
" | tee physlaw_bake_summary.log
fi
# 6. pictures (maps1 Boston camera, full size, then a 60 px title bar by label.py); ports 43761..
if has pics; then
	mkdir -p pics; P=43761
	# LODL: the ao2/terr_x1 folder the first session opened is gone and the Boston bakes write no .lodl, so the
	# terrain mesh comes from a copy of the installed Commonwealth.lodl (the one maps1 copied; read-only source).
	# The sheets drawn on it are the bake's own (SHEETS).
	shot() { game; env LODI_DIR=$OD LV=2 SLOT=0 SDIM=2 SHEETS=$T/bakes/$2/$C LODL=$T/bakes/Commonwealth.lodl "${@:3}" bash shot.sh pics/$1.png $F Commonwealth $X0 $Y0 $X1 $Y1 8 $HW 1600 1600 $P; P=$((P+1)); }
	lab() { [ -s pics/$1.png ] && python label.py pics/$1.png pics/$1_labeled.png "$2"; }
	X0=-5; Y0=-10; X1=2; Y1=-3; HW=16384
	shot sky_before base WW_RENDER_FLAT=1 WW_LODL_CHANNEL=mask-b
	lab sky_before "TERR1 Boston, ground sky byte (mask B) BEFORE: terrain only (night-20260927)"
	shot sky_after on WW_RENDER_FLAT=1 WW_LODL_CHANNEL=mask-b
	lab sky_after "TERR1 Boston, ground sky byte (mask B) AFTER: objects, cosine-weighted law (6eb5954f)"
	shot normal_before base WW_LODL_CHANNEL=normal WW_LOD_CHANNEL=12
	lab normal_before "TERR1 Boston, ground normal sheet BEFORE: heightmap only (night-20260927)"
	shot normal_after on WW_LODL_CHANNEL=normal WW_LOD_CHANNEL=12
	lab normal_after "TERR1 Boston, ground normal sheet AFTER: object normals stamped"
	# 4x: one cell, half-width 4096, the cells rail.json names (track / junction). Windows python prints CRLF:
	# the carriage return is stripped, or the cell reads "-10\r" and shot.sh's arithmetic fails.
	for k in track junction; do
		read cx cy < <(python -c "import json,sys;d=json.load(open('rail.json'));c=d.get(sys.argv[1]);print(*(c['cell'] if c else ['x','x']))" $k | tr -d '\r')
		[ "$cx" = x ] && { echo "no $k located, 4x crop skipped"; continue; }
		X0=$cx; Y0=$cy; X1=$cx; Y1=$cy; HW=4096
		shot ${k}_normal_before_4x base WW_LODL_CHANNEL=normal WW_LOD_CHANNEL=12
		lab ${k}_normal_before_4x "TERR1 $k cell ($cx,$cy), ground normal sheet BEFORE, 4x"
		shot ${k}_normal_after_4x on WW_LODL_CHANNEL=normal WW_LOD_CHANNEL=12
		lab ${k}_normal_after_4x "TERR1 $k cell ($cx,$cy), ground normal sheet AFTER (stamp on), 4x"
	done
	ls -la pics/*_labeled.png
fi
# 7. cleanup (night rule: delete own bake output and caches) -- only after DONE.md has the numbers
if has clean; then rm -rf bakes/base bakes/on bakes/off bakes/noroads bakes/noflat bakes/diag bakes/objh_on.bin bakes/stamp_diag.bin cache; fi
