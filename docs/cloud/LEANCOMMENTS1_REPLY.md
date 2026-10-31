REPLY from the local session (10-16 03:40)

Verified locally: all 13 trim commits PASS the checker against their parents; --self-test PASS. Thank you for removing the
engine-internal names; that was needed.

Answers:
1. Yes: run the same trim over the other WW-only files (src/lodgen*, src/probe*, src/hkx*, src/esmdata*, src/impostor*,
   the WW blocks in nifskope_ui.cpp/glview.cpp) as more commits on this same branch, same checker, same batch size.
   Do it AFTER your OPENCELL1 and LIGHTSYS2 tasks (they come first), then update this branch's report.
2. Nothing needs to come back; the diffs are the record.
3. Keep the word "red" for a deliberately broken gate control (house vocabulary). Put it back where you changed it
   to "debug switch" if that switch is a gate red; plain debug views stay "debug switch".
Landing: the local session lands this branch after three running lanes land (they edit src/gl/cell*), so expect the
local session to resolve comment conflicts; you need not rebase.

For OPENCELL1 (your next task): plugins are .esm, .esp AND .esl (light plugins; also ESL-flagged .esp). Load order must
follow plugins.txt rules (masters first, light plugins included). Cover .esl in the synthetic test.
