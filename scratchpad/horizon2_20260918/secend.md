
## DONE

`horizon2` -- lane complete at 2026-09-19 01:28 (`date`). `BUILDING` removed,
`DONE` written. Nothing committed, nothing stashed, `WW_CHANGES.md` and
`HANDOFF.md` untouched (their text is section 9). Exe on disk:
`release/NifSkope.exe` 2026-09-18 23:47:33, 22,949,376 B, sha1
`b349f807426be700ed2ff9b54ee23e4fab3ba037`; the rung
`release/NifSkope.before_horizon2.exe` is the 21:59:46 exe, untouched.

### Five plain sentences for bungo

1. The far-LOD terrain shadow map was smearing every building across a
   22-and-a-half-degree wedge of sky, so the ground thought it had a tower in
   every direction and a whole downtown chunk rendered with **nothing lit at a
   low sun**; the march now looks along the actual direction it is storing, and
   that chunk goes from 0.0% lit to 5.3% at a 15-degree sun.
2. The reason nobody caught it is that the checker that was supposed to catch it
   calls the same function as the thing it checks -- so the two agreed with each
   other while both stood about eight and a half degrees too high -- and that is
   now written down in `MISTAKES.md` in plain words.
3. There is a new check, G6, that measures the sheet against the raw heightmap
   and the actual building placements instead, it shares no code with the bake,
   and it is shipped with the old broken sheet wired in beside it as proof that
   it can fail.
4. The way out is unchanged and free: `--no-terrain-horizon` / `--lodi-v7`
   produce byte-identical files to before, every other harness in the tree
   reports exactly the counts it reported yesterday, and the bake costs three
   seconds more a chunk.
5. **His open window needs a restart.**
