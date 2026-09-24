# Lane WATER2's entries for `MISTAKES.md` (the director splices these)

## 2026-09-10 — a decimated point cloud shipped as a distance RULE (lane WATER1, found by WATER2)

**What was done.** `scratchpad/water_20260909/final_census.py` states rule D's
bridge as "two components at the same height whose shores are within 2 texels",
and implements it by comparing point clouds decimated to at most 4,000 points a
body (`bx[::area // 4000]`). For the Commonwealth's sea that is 4,000 of
21,585,117 texels. The resulting counts — 590 bodies, 215 merges accepted, 3
refused — went into `scratchpad/specs_20260909/spec_water.md` as gate G5, to be
reproduced exactly.

**What was true instead.** The exact test finds **545** pairs within two texels
where the decimated one found 218, and **347** bodies where it reported 590. A
decimated distance can only ever be too LARGE, so every disagreement is a merge
the stated rule requires and the implementation missed.

**How it was found.** Lane WATER2 was about to reproduce the decimation in C++
to hit the gate, and wrote the control first instead:
`scratchpad/water2_20260909/bridge_exact.py` recomputes the same step with an
exact disc scan and prints both answers.

**The rule that prevents it.** A gate number is only as good as the
implementation that produced it. Before pinning a measurement as a gate, state
the approximation the measurement used, or run it once without one. An
approximation whose error is one-sided (a subsample can only lengthen a minimum
distance) must be named in the spec beside the number, or the number is not the
rule's.

## 2026-09-10 — a merge rule with no DIRECTION gave the ocean a marsh's name

**What was done.** Rule D's bridge accepts a merge when "their types are equal
or one inherits", with no statement of which side is absorbed. With the exact
shore test above, that let the 21.5 M-texel inheriting sea merge with painted
marsh bodies that pass within two texels of it; the merged body's form is "the
most common PAINTED type", so the whole Commonwealth ocean came out as
`ExtMarshScumWater` — 21,587,443 texels, assembled from 332 components across
five forms.

**What was true instead.** Rule C's merge already states the direction ("a
component whose type is the worldspace default is merged INTO the same-height
PAINTED component it touches"). The bridge is the same merge across a gap and
needs the same direction, plus the one clause that makes it safe: the
inheriting side must be the SMALLER of the two. With it: 347 bodies, 526
accepted, 14 refused, the sea keeps its own form, every large painted body
survives, and there is not one body where the two readings of the form rule
disagree.

**How it was found.** `scratchpad/water2_20260909/bridge_effect.py` printed the
resulting body table instead of only its size — the count 340 looked like an
improvement, and the table showed what it had cost.

**The rule that prevents it.** A count is not a result. When a rule changes how
things are GROUPED, print what the groups became, not how many there are.

## 2026-09-10 — a heredoc mangled `\n` into a real newline, and the build paid

**What was done.** The `--water-census` usage line was inserted into
`src/nifcli.cpp` through a `python - <<'PYEOF'` heredoc. The two-character
escape `\n` inside the C++ string literal arrived as an actual newline, so the
literal was unterminated and `nifcli.o` failed with six "missing terminating
character" errors. The insert also landed between the `--info` line and its own
continuation.

**What was true instead.** `nifskope-ww-build-verify` says it outright: *patch
with a script FILE, never a heredoc or `python -c`* — heredocs halve
backslashes. This was the third time the rule has been paid for, twice on
2026-09-06 and once here.

**How it was found.** `make`'s own exit code, which is why the chain gates on
it.

**The rule that prevents it.** The skill's rule, followed: every subsequent
patch in this lane went through a `fixNN.py` written with the Write tool
(`scratchpad/water2_20260909/fix01.py`, `fix02.py`).

## 2026-09-10 — six most-vexing parses in one file

**What was done.** `std::vector<T> v( size_t( n ) );` was written six times.
Every one declares a FUNCTION taking a `size_t`, because `size_t( n )` is a
valid parameter declaration; the errors then land on the first USE of the name,
tens of lines away, and all six arrived at once.

**What was true instead.** A one-argument vector construction whose argument is
a cast-looking expression needs a second argument (the fill value), a named
variable, or a `resize`.

**How it was found.** The build.

**The rule that prevents it.** Never spell a single-argument container
construction as `T v( U( x ) )`. Give it the fill value it is going to have
anyway — it also documents the initial value, which the one-argument form does
not.
