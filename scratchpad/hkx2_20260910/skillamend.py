import io

section = """

## 9. Settled by lane HKX2 (playback into the scene graph), 2026-09-10

The reader hands you `HkxAnimClip`. Putting it on a rig in NifSkope is these
five facts; none of them needs deriving again.

* **The pose goes into `Node::local`, written from `Node::transform()`
  immediately after `IControllable::transform()`** (`src/gl/glnode.cpp`). That
  is one step after the node's own controllers, so a clip WINS over a
  `NiTransformController` naming the same node; and it is before this node's
  collision body is the first thing in the frame to read a world transform, and
  before any child is walked. Do NOT do it in `Scene::transform` after the roots
  walk: that needs a second `transformCache.clear()`, which throws away the
  `bhkBodyTransKey` entries `Node::drawHvkConstraint` reads.
* **`Node::local`, `Node::parent`, `Node::children` and `Node::nodeId` are
  PROTECTED.** Everything that writes them is a `friend` at the top of
  `class Node` (`ControllerManager`, `TransformController`,
  `MultiTargetTransformController`, `KeyframeController`,
  `ProcLightningController`, and now `HkxPlayback`). Read a member's access from
  the LAST specifier above it.
* **A clip becomes an animations-list entry for free.** Put the name in
  `Scene::animGroups`, `{"start","end"}` in `Scene::animTags`, and
  `Scene::CycleLoop` in `Scene::animCycle`. `Scene::timeMin`/`timeMax`
  short-circuit on `animTags`, so play/pause/loop/reverse/speed/scrub/cycle and
  the Timeline dock's ruler all drive it with NO new transport code.
  `Scene::setSequence` is then the bind/unbind hook. The Animation Manager
  dock's own `seqBox` does NOT follow: it is built from `NiControllerSequence`
  BLOCKS (`QPersistentModelIndex`), and a loaded clip has no model index.
* **Two arithmetic traps in `src/data/niftypes.h`.** `Quat::normalize()` divides
  by the SQUARED magnitude, so it only normalises quaternions that are already
  unit -- write your own. `Quat::slerp` is Blow's approximation, not slerp; it
  does return `p` exactly at t=0. And a `Transform` carries ONE scale where a
  Havok transform carries three: take x and say you dropped y/z.
* **A frame time needs an epsilon.** `N * frameDuration` divided back by
  `frameDuration` is not `N` in float (5/30 comes back as 4.99999952), so a
  frame-exact read has to snap to the nearest frame within about 1e-4 of a
  frame or it silently interpolates 99.99997% of the next one.

And one correction to section 8: **an empty
`transformTrackToBoneIndices` is the IDENTITY map, not a missing one** -- so a
consumer sizes its "does this skeleton fit" test on `numberOfTransformTracks`,
never on the length of the binding vector.

### Bone names are not in a clip

A clip stores track -> bone INDEX against a skeleton it only NAMES, so any
playback needs an `hkaSkeleton` first. FO4's layout is
`<actor>/Animations/<group>/<clip>.hkx` with the skeleton at
`<actor>/CharacterAssets/skeleton.hkx`, so the search walks UP from the clip and
looks SIDEWAYS into `CharacterAssets` at each level, then does the same from the
open NIF's folder, then asks the game archives
(`Game::GameManager::get_file(blob, game, "meshes/.../skeleton.hkx")`, with
`Game::GameManager::get_game(nif)` for the mode). Match names
CASE-INSENSITIVELY and expect a partial match; on skeleton.nif the answer is
**78 matched, 17 unmatched, 4 matched only by case**, and those three numbers
are the gate.

### The in-app gate

`src/hkxplaybacktest.cpp` + `tests/spells/hkxanim_play.sh` (`WW_HKXANIM_TEST`).
It reads the pose back off `Node::localTrans()`, never off the playback's own
record, and every check has a floor beside it: the wrong frame must FAIL the
same comparison, invented bone names must match nothing, and the clip must
actually have moved the rig before "unload restored it" means anything.
"""

p = '.claude/skills/ww-hkx-animation/SKILL.md'
s = io.open(p, encoding='utf-8', newline='').read()
before = open(p, 'rb').read()
assert '## 9. Settled by lane HKX2' not in s
if not s.endswith('\n'):
    s += '\n'
s = s + section.lstrip('\n')
io.open(p, 'w', encoding='utf-8', newline='').write(s)
after = open(p, 'rb').read()
print('CR before', before.count(b'\r'), 'after', after.count(b'\r'))
print('bytes', len(before), '->', len(after))
