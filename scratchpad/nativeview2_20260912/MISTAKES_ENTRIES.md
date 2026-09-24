## 2026-09-16 -- lane NATIVEVIEW2 (the model-space normal path)

1. **A new gate that built `WW_LODGEN_RESOURCES` out of MSYS paths and read the
   resulting flat render as a failure of the code under test.**
   `tests/spells/native_lighting.sh` set `ROOT="$(cd "$(dirname "$0")/../.." &&
   pwd)"`, which is `/e/Projects/...`, and then joined two of those with a
   semicolon into `WW_LODGEN_RESOURCES`. MSYS2 converts a single argv or env
   path to `E:/...` automatically but never a semicolon-joined LIST -- which is
   written out in full, with the reason, at the top of
   `tests/spells/_harness.sh`, a file this very spell sources on its first line.
   The exe therefore opened neither resource root, the object textures never
   loaded, and gate (a) reported all four legacy frames as differing from their
   baselines: `legacy_bto_top.png` came out 390,854 B against the baseline's
   537,794 B, mean |dColour| 30.93 over 1,012,736 pixels. Found by rendering the
   same frame three times and getting the same wrong bytes every time -- the
   render was deterministic, so the difference was in the INPUT, not the
   renderer; the census file confirmed the same two shapes on the same program
   in both runs, which ruled out the change under test. After converting each
   half with `_harness.sh`'s own `winpath`, 14 checks, 0 failures. THE RULE: a
   harness that fails on its FIRST run has two suspects, and the harness is the
   one you wrote five minutes ago. Render it twice before blaming the exe; if
   the bytes repeat, the defect is upstream of the renderer. And when a file you
   source carries a warning about the exact shape of value you are building,
   that warning is about you.

2. **A file-scope helper was written as if it were a member.** The first build
   of `src/gl/renderer.cpp` failed with
   `error: 'BSLightingShaderProperty* Shape::bslsp' is protected within this
   context` at lines 136-138: `wwProgramCensus` was a file-scope `static`, and
   `Shape` grants friendship to `Renderer`'s members, not to every function in
   the file. Cost one build. THE RULE: before reaching for a member of another
   class from a free function, check the friend list, not the header's
   indentation.
