## 2026-09-06 — A comment that asserted someone else's file format, and was the argument for a decision

**What:** `lodgenBuildAtlas` wrote its diffuse sheet as BC3 under the comment
"BC3, like vanilla's sheet: 8-bit alpha (soft card edges survive)". Vanilla's
sheet is not BC3. Measured on the shipped file —
`Commonwealth.Objects.DDS`, 4096×2048, 13 mips, fourCC **DXT1**, 5,592,552
bytes — it is BC1, and its `_n` and `_s` are both `BC5U` where ours is BC3 for
the normal. So ours was twice the memory of vanilla's on the diffuse and twice
again on the normal, and the comment saying otherwise is what made that look
like parity rather than a cost.

**Why:** the comment was written from what the format *ought* to be for the
feature it was justifying (eight-bit alpha for soft card edges), and a
plausible sentence about another program's file never gets checked once it is
in the source. Reading a DDS header is thirty seconds of work; nobody spends it
on a line that already sounds settled.

**Solution:** the comment now carries the measurement, with the byte count, and
`tests/spells/lodgen_farring.sh` re-measures vanilla's own header every run, so
the claim the flag rests on cannot rot. Rule: a comment that states a fact
about a file we did not write is a MEASUREMENT, and it carries the numbers it
was measured from — a format, a size and a byte count — or it does not go in.

## 2026-09-06 — A mip chain that dropped the channel its top mip carried

**What:** `lodgenWriteDds` built its mip chain with
`( bc3 ? ( acc[3] / 4 ) : 0xFFU ) << 24` — BC3 averaged alpha down the chain,
BC1 forced it opaque. Every BC1 caller at the time was opaque anyway (the
terrain bakes, the emissive sheets), so the branch was invisible and correct.
The moment the atlas asked for BC1, mip 0 would have kept the cut-outs and
every mip below it would have been a solid rectangle — at exactly the distance
an atlas is looked at, which is the only distance it exists for.

**Why:** "BC1 has no alpha" is true of the common case and false of BC1's
punch-through mode, which the encoder in the same file already implements. A
default chosen for the callers that exist is a trap for the caller that
arrives; nothing about it reads as a decision when you find it.

**Solution:** the caller says (`bc1Alpha`), the default keeps every existing
byte identical, and the harness counts punch-through blocks PER MIP and
requires them past the top one — a check that fails on the old filter and
passes on the new. Rule: when a writer branches on a format flag to drop data,
the branch is a decision about the DATA, not about the format, and the caller
that has the data has to be able to say so.

