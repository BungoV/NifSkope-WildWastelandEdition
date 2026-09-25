#!/usr/bin/env bash
# btr_pair.sh <exe> <outdir> [scope|-]  -- render only gate (a)'s two legacy .BTR frames the way
# native_lighting.sh does, optionally in a wiped WW_SETTINGS_SCOPE, and print pixel sha vs baseline.
EXE="$1"; OUT="$2"; SCOPE="${3:--}"; PORT="${PORT:-43613}"
ROOT=/e/Projects/NifskopeWWE-gatefix2
OBJ="$ROOT/scratchpad/showcase1_20260912/out/look/obj"
R1="$ROOT/scratchpad/nativeview1_20260912"
RES="E:${R1:2}/resroot;E:${OBJ:2}"
mkdir -p "$OUT"; rm -f "$OUT"/legacy_btr_*.png
wipe() { [ "$SCOPE" = - ] || reg delete "HKCU\Software\NifTools\NifSkope 2.0 $SCOPE" //f >/dev/null 2>&1; }
prep() { wipe; [ -n "${PROFREG:-}" ] && reg import "$(cygpath -w "$PROFREG")" >/dev/null 2>&1; [ -n "${MASK2:-}" ] && reg add "HKCU\Software\NifTools\NifSkope 2.0 $SCOPE\GLView\Display\Contributions" //v 2 //t REG_DWORD //d $MASK2 //f >/dev/null; }
for v in 1 8; do
  if [ $v = 1 ]; then s=top; lc="8192,8192,0"; else s=obl; lc="8192,8192,8500"; fi
  prep
  sc=(); [ "$SCOPE" = - ] || sc=(WW_SETTINGS_SCOPE="$SCOPE")
  env "${sc[@]}" WW_WINDOW_AT=1960,40 WW_RENDER_SHOT="$OUT/legacy_btr_$s.png" WW_RENDER_SIZE=1024x1024 \
    WW_RENDER_VIEW=$v WW_RENDER_CLEAN=1 WW_RENDER_CENTER="$lc" WW_RENDER_ORTHO=8192 \
    WW_LODGEN_RESOURCES="$RES" timeout 300 "$EXE" --port "$PORT" "$OBJ/Commonwealth.4.-20.24.BTR" >/dev/null 2>&1
done
wipe
python - "$(cygpath -m "$OUT")" <<'PY'
import sys
from PIL import Image
import numpy as np
o=sys.argv[1]
for s in ('top','obl'):
    try:
        b=np.asarray(Image.open(o+'/legacy_btr_%s.png'%s).convert('RGBA')).astype(int)
    except Exception as e:
        print(s,'NO RENDER',e); continue
    a=np.asarray(Image.open('E:/Projects/NifskopeWWE-gatefix2/tests/baselines/native_lighting/legacy_btr_%s.png'%s).convert('RGBA')).astype(int)
    same=open(o+'/legacy_btr_%s.png'%s,'rb').read()==open('E:/Projects/NifskopeWWE-gatefix2/tests/baselines/native_lighting/legacy_btr_%s.png'%s,'rb').read()
    d=(np.abs(a-b).max(axis=2)>0).sum() if a.shape==b.shape else -1
    print(s,'bytes-identical' if same else 'DIFFERS','pixels_differ',d,'luma',round(b[...,:3].mean(),2))
PY
