"""Five MISTAKES.md entries (newest at the top, after the header), and the
doc's section 8 gains gate (b)'s findings + the reader files' hashes."""
import hashlib

def patch(path, pairs):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for a, c in pairs:
        assert s.count(a) == 1, (path, a[:70], s.count(a))
        s = s.replace(a, c)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr
    open(path, 'wb').write(out)
    print('patched', path, 'CR', cr)

entries = '''## 2026-09-10 -- the exe was launched once by a lane told not to (lane HKX1)

**What was done.** `release/nifskope-cli.cmd skeleton skeleton.nif` was run
to list the NIF's bones. The wrapper starts `NifSkope.exe` headlessly.

**What was true.** The brief said the exe must not be launched by the lane
while bungo bakes in the GUI. The `tasklist` check had returned rc=1 (no
NifSkope, no Fallout4) a minute earlier and nothing of his existed or was
touched -- the letter of the rule was still broken.

**How it was found.** Re-reading the brief after the command returned.

**The rule.** "Do not launch the exe" covers the CLI wrapper too; a headless
run is still the exe. When a lane needs a NIF fact under that rule it writes
a Python reader (gate (b) was), or reads the NIF with `tools/`.

## 2026-09-10 -- a header constant copied from another walker refused every animation file (lane HKX1)

**What was done.** The census's packfile walker took the section-header
start, 0x40, from `tools/hkparse.py` and `src/gl/hknpdecode.cpp`.

**What was true.** A Havok packfile header ends with
`predicateArraySizePlusPadding` (u16 at 0x3e); the section headers start at
`0x40 + that`. It is 0 in a collision blob and 0x10 in every FO4 animation
file, so the walker read `__classnames__` from the predicate bytes and all
15,320 files failed to parse.

**How it was found.** A hexdump of three headers side by side. The C++
collision walker has the same constant and would refuse every animation
file; it was not touched (one lane per file) and is noted in the contract.

**The rule.** A header field is READ, not copied from a sibling reader that
happened to work on a file family where it was zero. Every constant taken
from another walker gets one hexdump on the new family before use.

## 2026-09-10 -- an angle metric with no resolution below the gate it served (lane HKX1)

**What was done.** Gate (a) measured the rotation difference between the two
decoders as `2 * acos(|q1 . q2|)` and reported 0.0325 degrees, over its
0.01-degree line.

**What was true.** The quaternions differed by float rounding (1e-7 in a
component). `acos` near 1 has no resolution: `acos(1 - 1e-7)` is already
0.026 degrees, so the metric could not report anything smaller than ~0.03
degrees and the gate could never have passed on identical data.

**How it was found.** The translation difference on the same rows was 4e-6
-- rounding -- while the rotation claimed 0.03 degrees; the metric was
suspected before the decoders. Replaced by `2 * asin(min(|q1 - q2|, |q1 + q2|) / 2)`,
which reads 1e-5 degrees on the same rows.

**The rule.** A gate metric is run on two copies of the SAME data first and
must read ~0; a metric that cannot reach zero cannot hold a threshold
(CONSTITUTION 4: prove the check can fail, and prove it can pass).

## 2026-09-10 -- the mutation gate found two decoder holes: one blind, one crashing (lane HKX1)

**What was done.** The C++ XML route read `data`'s bytes and never compared
their count with the element's `numelements`; the Python decoder let a data
byte of 999 and a malformed XML raise tracebacks (exit 1) instead of a
refusal (exit 2).

**What was true.** Two of the twenty pre-registered corruptions decoded (one
route) or crashed (the other). Both fixed; the gate then read 20/20.

**How it was found.** By the gate, which is what it is for. Recorded because
the decoders were called finished before the gate ran.

**The rule.** A refusal path is a path: it gets the same run as the decode
path, and "finished" is after the mutation run, not before.

## 2026-09-10 -- a heredoc halved the backslashes again (lane HKX1)

**What was done.** A Python one-liner in a Bash heredoc split a path on
`'\\\\'`; the shell delivered one backslash and Python raised a syntax error.
The same trap `nifskope-ww-build-verify` and four earlier MISTAKES entries
name.

**What was true.** Scripts go through the Write tool; heredocs carry no
backslash.

**How it was found.** The traceback.

**The rule.** The existing one, repeated: a script with a backslash is a
file, never a heredoc. Repeating an entry already in the ledger is its own
entry (CONSTITUTION 2).

'''

patch('MISTAKES.md', [
    ('Newest at the top.\n\n## 2026-09-10 -- two undo conventions in one stack (lane WATER5)\n',
     'Newest at the top.\n\n' + entries + '## 2026-09-10 -- two undo conventions in one stack (lane WATER5)\n'),
])

# the doc: gate (b) findings into section 8, and the reader files' hashes into the provenance table
rows = ''
for f in ('src/hkxanim.h', 'src/hkxanim.cpp', 'tests/spells/hkxanim_decode.py'):
    b = open(f, 'rb').read()
    rows += '| `%s` | `%s` | %s | %s |\n' % (f, hashlib.sha256(b).hexdigest()[:16], '{:,}'.format(len(b)),
                                             'the reader (this contract implemented)' if f.startswith('src') else 'the independent decoder, written from this page')
patch('docs/HKX_ANIMATION_FORMAT.md', [
    ('names. The bone NAMES a clip\'s tracks map to come from the skeleton the clip was\nauthored against (`originalSkeletonName`), so a clip alone gives INDICES;\nHKX2 needs `skeleton.hkx` (or the NIF\'s own bone order, if it matches\nskeleton.nif -- gate (b) says it does, see the lane report) to turn them into\nnames.\n',
     'names. The bone NAMES a clip\'s tracks map to come from the skeleton the clip was\nauthored against (`originalSkeletonName`), so a clip alone gives INDICES;\nHKX2 needs `skeleton.hkx` (this reader returns it) to turn them into names.\n\n**Measured against `skeleton.nif` (gate (b), `tests/spells/hkxanim_gates.py`):**\nof the 95 animation bones, 78 have a NiNode of the same name in\n`meshes\\actors\\character\\CharacterAssets\\skeleton.nif` -- four of them only\ncase-insensitively (`Head`/`HEAD`, `Spine1`/`SPINE1`, `Spine2`/`SPINE2`,\n`Weapon`/`WEAPON`) -- and the 17 `Weapon*` bones (WeaponBolt, WeaponExtra1-3,\nWeaponIKTarget{L,R}{,Mirror}, WeaponMagazine + Child1-5, WeaponOptics1-2,\nWeaponTrigger) have NO node on the body skeleton (they live on the weapon\nNIF). 55 NiNodes (`*_skin`, CamTargetParent, CharacterBumper, ...) are not\nanimation bones; the NIF interposes `CamTargetParent` between Root and\nCamTarget. On the shared bones the reference pose and the NiNode bind pose\nagree: parents by name on all 78, rotation to 2.7e-4 per matrix element --\nthe hkx quaternion (x,y,z,w) converted to a rotation matrix by the standard\nformula equals the NiNode matrix DIRECTLY, not its transpose -- scale to\n1.8e-4, translation to 1e-3 on 77 and 1.48e-3 on `Weapon`. So: match names\ncase-insensitively, expect partial matches, and hand the decoded `Quat` to\nthe NiNode as-is.\n'),
    ('| `scratchpad/hkx1_20260910/disasm/*.txt` | -- | -- | DISASM, one file per function named above |\n',
     '| `scratchpad/hkx1_20260910/disasm/*.txt` | -- | -- | DISASM, one file per function named above |\n' + rows),
])
