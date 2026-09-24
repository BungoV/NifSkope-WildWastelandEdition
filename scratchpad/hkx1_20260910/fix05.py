"""(1) the doc's section 8 gains gate (b)'s findings and the provenance table
the reader files' hashes (fix04's anchor missed by one wrap); (2) one more
MISTAKES entry: two repo-tree skills covering this lane's standalone gate and
its .pro hook-up were not loaded because the repo tree was never listed;
(3) the report's skill review says so."""
import hashlib
import re

def rw(path):
    b = open(path, 'rb').read()
    return b, b.count(b'\r'), b.decode('utf-8')

def wr(path, s, cr):
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, (path, cr, out.count(b'\r'))
    open(path, 'wb').write(out)
    print('patched', path)

# ---- the doc
p = 'docs/HKX_ANIMATION_FORMAT.md'
b, cr, s = rw(p)
pat = re.compile(r"The bone NAMES a clip's tracks map to come from the skeleton the clip was\n.*?to turn them into\nnames\.\n", re.S)
assert len(pat.findall(s)) == 1, len(pat.findall(s))
new = ("The bone NAMES a clip's tracks map to come from the skeleton the clip was\n"
       "authored against (`originalSkeletonName`), so a clip alone gives INDICES;\n"
       "HKX2 needs `skeleton.hkx` (this reader returns it) to turn them into names.\n\n"
       "**Measured against `skeleton.nif` (gate (b), `tests/spells/hkxanim_gates.py`):**\n"
       "of the 95 animation bones, 78 have a NiNode of the same name in\n"
       "`meshes\\actors\\character\\CharacterAssets\\skeleton.nif` -- four of them only\n"
       "case-insensitively (`Head`/`HEAD`, `Spine1`/`SPINE1`, `Spine2`/`SPINE2`,\n"
       "`Weapon`/`WEAPON`) -- and the 17 `Weapon*` bones (WeaponBolt, WeaponExtra1-3,\n"
       "WeaponIKTarget{L,R}{,Mirror}, WeaponMagazine + Child1-5, WeaponOptics1-2,\n"
       "WeaponTrigger) have NO node on the body skeleton (they live on the weapon\n"
       "NIF). 55 NiNodes (`*_skin`, CamTargetParent, CharacterBumper, ...) are not\n"
       "animation bones; the NIF interposes `CamTargetParent` between Root and\n"
       "CamTarget. On the shared bones the reference pose and the NiNode bind pose\n"
       "agree: parents by name on all 78, rotation to 2.7e-4 per matrix element --\n"
       "the hkx quaternion (x,y,z,w) converted to a rotation matrix by the standard\n"
       "formula equals the NiNode matrix DIRECTLY, not its transpose -- scale to\n"
       "1.8e-4, translation to 1e-3 on 77 and 1.48e-3 on `Weapon`. So: match names\n"
       "case-insensitively, expect partial matches, and hand the decoded `Quat` to\n"
       "the NiNode as-is.\n")
s = pat.sub(lambda m: new, s, count=1)
rows = ''
for f in ('src/hkxanim.h', 'src/hkxanim.cpp', 'tests/spells/hkxanim_decode.py'):
    fb = open(f, 'rb').read()
    rows += '| `%s` | `%s` | %s | %s |\n' % (f, hashlib.sha256(fb).hexdigest()[:16], '{:,}'.format(len(fb)),
                                             'the reader (this contract implemented)' if f.startswith('src') else 'the independent decoder, written from this page')
a = '| `scratchpad/hkx1_20260910/disasm/*.txt` | -- | -- | DISASM, one file per function named above |\n'
assert s.count(a) == 1
s = s.replace(a, a + rows)
wr(p, s, cr)

# ---- MISTAKES
p = 'MISTAKES.md'
b, cr, s = rw(p)
entry = '''## 2026-09-10 -- two repo-tree skills that covered this lane's work were never listed (lane HKX1)

**What was done.** The standalone Qt6Core test binary, the hand-written
known-answer clip, the independent decoder and the single-byte mutation
run were designed from scratch, and the two `NifSkope.pro` lines were
patched by an ad-hoc script.

**What was true.** `.claude/skills/ww-standalone-writer-gate` (lane NATIVE0b)
is that gate procedure, and `.claude/skills/ww-anchored-hookup` is the
refusing hook-up script. Both sit in the REPO tree, which the brief's skill
list (the live tree's names) did not carry; the lane never ran
`ls .claude/skills` in its own cwd.

**How it was found.** The two skills surfaced in the tool listing while the
finished-work review was being written.

**The rule.** CONSTITUTION 1a's "the two skill trees drift": a lane whose cwd
is the repo lists `<repo>/.claude/skills` FIRST, before the brief's names,
and loads every skill whose description covers the work. Doing by hand what
a skill covers is the process error this ledger exists for.

'''
a = 'Newest at the top.\n\n## 2026-09-10 -- the exe was launched once by a lane told not to (lane HKX1)\n'
assert s.count(a) == 1
s = s.replace(a, 'Newest at the top.\n\n' + entry + '## 2026-09-10 -- the exe was launched once by a lane told not to (lane HKX1)\n')
wr(p, s, cr)

# ---- the report
p = 'scratchpad/lane_hkx1_report.md'
b, cr, s = rw(p)
a = "5. A Bash heredoc halved the backslashes in a Python one-liner (the known\n   trap, `nifskope-ww-build-verify`); one turn lost, the script went into a\n   file via the Write tool.\n"
assert s.count(a) == 1
s = s.replace(a, a + "6. Two REPO-tree skills covered work this lane did by hand:\n   `ww-standalone-writer-gate` (the standalone Qt6Core binary + known answer +\n   independent decoder + mutations -- exactly this lane's gate design) and\n   `ww-anchored-hookup` (the refusing .pro hook-up script). The brief listed\n   the live tree's skills; the lane never listed `<repo>/.claude/skills`.\n   Process error under CONSTITUTION 1a, recorded.\n")
a = "Re-derived from memory / worked out again, now written as\n"
assert s.count(a) == 1
s = s.replace(a, "Should have been loaded and were not (repo tree, see Mistakes 6):\n`ww-standalone-writer-gate` and `ww-anchored-hookup`; the lane's gate design\nmatches the first almost step for step, which is the point of the entry.\n\nRe-derived from memory / worked out again, now written as\n")
wr(p, s, cr)
