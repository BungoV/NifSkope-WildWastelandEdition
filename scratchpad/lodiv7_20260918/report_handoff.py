p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

A = "> **LANDED 2026-09-18 -- `.lodi` v7 (lane LODIV7), PENDING on one blocker.**"
B = "## 14. The five plain sentences for bungo"
i, j = s.index(A), s.index(B)

NEW = """> **LANDED 2026-09-18 -- `.lodi` v7 (lane LODIV7). One verification run owed.** Built into
> `release/NifSkope.exe` (**19:05:11, 22,764,544 B, `28ac412c6da96e0707e492c175822d2076016dc5`**; the
> 10:38 exe is kept as `release/NifSkope.before_btdterrain_rebuild.exe`): the v7 group table and per-vertex
> sky stream, both readers, the census clauses, the viewer's `identity` / `placement` / `sky` channels,
> `docs/LODGEN_NATIVE_LODO_LODI.md` s4.1c, s4.6.6, s4.9, s4.10, s5 and s8, `docs/LODGEN_CENSUS.md` 6.1 and
> `shadowIdentityUnique`, `tests/spells/lodi_v7.sh`, `tests/spells/lodi_v7_refuters.py` (12 refuters, 7 red
> controls, 0 failures), `tests/spells/lodi_v7_numbers.py`, `tests/spells/lodl_channels*` +1 channel +3
> checks, and two neighbour repairs the version bump owed (`lodgen_native_fields.py` j0 accepts version 7;
> `lodgen_native_mutate.py` signs a 512-byte v7 header, so the `lodi-wrap` control goes red for its own
> reason again). **PROVED:** G1 all three halves, G2, G3, G4 -- `lodi_v7: 12 ok, 0 failed, 0 skipped`, twice
> -- plus `lodl_channels.sh` 54/0, `native_open.sh`, `lodl_open.sh` and `lodgen_slab.sh` at their standing
> counts, and **the seven pictures** in `scratchpad/lodiv7_20260918/images/` with `captions.md`.
> **FOUND AND FIXED MID-GATE:** the 10:38 exe linked a stale `btdterrain.o` compiled before the channel enum
> gained a value, so the terrain viewer read every channel from `mask-r` on one late and G1..G4 were green on
> a wrong binary; rebuilt 19:05, re-proved, three rules in root `MISTAKES.md` and the check added to
> `nifskope-ww-build-verify`. **OWED, one command when the game is down:**
> `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the 19:05 exe -- it re-runs G1..G3 and the two neighbours
> not yet re-run (`lodgen_native.sh`, repaired but unrun; `render_shot.sh`, 2 luminance failures whose cause
> is not established and which last ran on the stale binary). The gate refuses to start while `Fallout4` is
> up, by its own rule. **The grouping rule remains a PROPOSAL awaiting bungo's ruling.** Report:
> `scratchpad/lodiv7_20260918/lane_lodiv7_report.md`.

"""
s = s[:i] + NEW + s[j:]
open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('handoff block rewritten; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
