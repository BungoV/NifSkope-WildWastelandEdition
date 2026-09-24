# Lane BTOFREE1, 2026-09-16 -- fills report section 4.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

S4 = '''## 4. Owed / red / bungo's calls

**Red that is NOT mine, untouched, and still red.** I ran neither of these into the ground and
changed no line either of them reads:

* `tests/spells/lodgen_ground_cover.sh` -- 4 grass-feature failures (C2 x3, C6a, C9, C16). Owner:
  whoever the director assigns; not this lane. Nothing in this lane reaches grass features: the
  `.BTO` move changes *where a chunk file is written*, not what goes into it, and the gate proves
  that by byte-comparing the chunk the way back produces against the rung's.
* the stock `.BTO` silent ~6 percent drop on dense chunks (measured by GENSMALL1). **bungo's call**,
  and explicitly not something to "fix" from inside a lane. My leg (c) makes the stronger statement
  that matters here: the stock target's whole output tree is byte-identical to the rung's, so
  whatever that drop is, this lane did not move it by a byte.

**Owed to bungo, as decisions rather than work.**

1. **Should `--keep-bto` / "Keep legacy .BTO chunks" exist at all?** His words on 2026-09-12 were
   "no legacy vanilla file types are now used by us or baked in the FO4CS lod bake", qualified with
   "Except the data we're reading from for the bakes". A chunk that is built, read back five times
   and deleted inside one bake *is* data we read from, so the default obeys him. The switch is the
   way back for anyone who wants to inspect a chunk, and it ships OFF; if he would rather it did not
   exist, deleting the row is a five-line change and the gate's leg (b) is what would go with it.
2. **The scratch folder lives inside the mod folder** (`<mod folder>/lodgen_bto_scratch`), because
   that is the one writable path the panel is certain of -- `outputDir()` is the only route to a
   Data path and there is deliberately no second field. It is created, used and removed inside one
   bake, and the next bake clears a leftover from an interrupted run before it starts. If he would
   rather it sat under the system temp folder, that is one line in each front end; I did not choose
   it for him, because a temp folder on a different volume turns the sidecar move into a copy.
3. **The census wording** is mine, not his: `bto built in scratch <path>, N chunk(s), N dropped,
   N bytes freed`, and in the panel summary "built in a scratch folder and dropped (the manifest
   sidecars stay under meshes\\terrain\\<worldspace>)". It is written to be read by someone who has
   never heard the word chunk; if it still reads like lane jargon to him, the strings are in
   `src/lodgenchunkpass.cpp` and `src/lodgenmanager.cpp` and nothing else depends on their wording
   except the self-test, which looks only for the words "scratch" and "dropped".

**Owed as work, and small.**

* `tests/spells/native_open.sh` still skips two checks, and both are skips the tree has always had:
  the byte-identity leg of (a) wants `release/NifSkope.before_nativeview1.exe`, which is not on
  disk, and the manifest leg of (b) wants `GBAKE`. Neither is this lane's to supply.
* The new gate bakes ONE chunk. That is what makes it a five-bake gate that finishes in minutes and
  can therefore be run on every build; the multi-chunk statement is `lodgen_native.sh`'s, which I
  extended rather than duplicated. A lane that wants the drop measured across a region should widen
  `REGION=` -- the gate takes it from the environment.

'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = u"## 4. Owed / red / bungo's calls\n\n(in progress)\n\n"
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, S4)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('report 4 written: %d -> %d bytes, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))
