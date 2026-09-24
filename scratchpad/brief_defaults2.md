# Lane DEFAULTS2 -- two of bungo's rulings become what a bare bake does

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED behind IMPOSTORDEPTH1 (build slot). Folder scratchpad/defaults2_<date>/.

RULINGS (bungo 2026-09-23, AskUserQuestion answers):
1. "Turn the terrain edge blend on by default?" -> "Yes, default on". `--blend-edges quadrant` becomes the default
   (src/lodgen.cpp:6294 g_blendEdges = 0 -> 1; CLI help, panel row default, docs); `--blend-edges off` stays the way
   back. BLENDEDGES1 measured it: colour sheets only, seam 1.236 -> 0.977 (chunk), 1.250 -> 0.992 (pyramid).
2. "Default octahedral grid for tree cards?" -> "8x8 at 2k". Find where the card grid N and sheet size default live
   (CLI, panel, spec docs/LODGEN_IMPOSTOR_SPEC.md frame law); set N = 8, largest base 2048 (frame law ladders
   unchanged). Say what the old default was.
Also recorded (no code): shrub cards KEEP baked AO ("Keep AO").

Jobs: rung first (release/NifSkope.before_defaults2.exe). Byte gates: a bare bake on the new exe == the rung run with
the new flags typed out (both rulings), and != the rung's bare bake only in the files each ruling should move (name
them). Gates the change reaches: lodgen_octahedral.sh, impostor_draw.sh, terrain_vt, the tiling gate; re-rung any
gate whose pinned bytes move BY THE RULING and say so. Game down before build. Do not commit; text into
DELIVERABLE_TEXT.md; DONE marker; report under 300 words.
