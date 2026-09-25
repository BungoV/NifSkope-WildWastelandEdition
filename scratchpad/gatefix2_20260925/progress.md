# GATEFIX2 progress
- 02:53 worktree built (main objects, make -n 0 in main at 3489f4b; REVISION objects deleted); rung release/NifSkope.before_gatefix2.exe
- 02:58 native_lighting.sh reproduced 21/2 (gate (a) btr_top/obl) on 3a4d1e5d
- 03:0x NOT an exe move: before_vt1 (the exe the baselines were measured on) and before_cellview4 give the same 93,893-pixel diff under a COPY of bungo's profile
- 03:05 named: his profile GLView/Display/Contributions/2 = 0x00184b00; bit 0x80 (Vertex Color) alone -> BTR water white
- 03:1x empty scope = first install -> settings dialog writes Background 46,46,46 -> terrain gates red; seed Settings/Version=1
- 03:4x harness fixed (per-window scope + Version seed): 21/0 twice, baselines untouched; reds: his Contributions seed 21/2, corrupted baseline 21/1
- 04:2x kept green: lodgen_native 32/0, native_baseline PASS, btofree 30/0; DONE
