# NifSkope - Wild Wasteland Edition 0.3

A NIF editor for **Fallout 4**.

NifSkope opens and edits the NetImmerse file format (`.nif`) — the meshes,
skeletons, collision, materials, particle effects and animation that Fallout 4
loads at runtime. This edition is a fork aimed squarely at Fallout 4 modding:
everything added here was built against Fallout 4 files, measured against
Fallout 4 files, and is regression-tested against Fallout 4 files.

The headline difference from stock NifSkope is that the things you previously
had to leave NifSkope to do — rig a mesh to a skeleton, edit topology, unwrap
UVs, pose a skeleton, author collision, keyframe an animation, find out why a
file is broken — are in the program.

**[What this fork adds, in full →](WW_FEATURES.md)** · **[Change history →](WW_CHANGES.md)** · **[Handoff / project state →](HANDOFF.md)**

---

### What it does

Ten task **workspaces**, switched from one button, each with its own dock:

| Workspace | What it is for |
|---|---|
| **Animation** | Keyframe timeline: lanes per controlled block, value graph, sequence picker, transport |
| **Materials** | BGSM/BGEM material tree, texture preview and slot-correct labelling |
| **Collision** | Read, edit and **write back** compiled Havok (`hknp`) collision; ragdoll simulation |
| **Rigging** | Bone and weight transfer donor → receiver, segment editing, weight painting |
| **Vertex Paint** | Vertex colour and vertex alpha, with a brush |
| **UV Editing** | A UV editor: unwrap, project, pin, mirror, snap, multi-mesh |
| **Pose** | Pose a skeleton bone by bone; pose library; Outfit Studio pose XML |
| **Skeleton** | Skeleton inspection with Blender-style armature drawing |
| **Issue Manager** | Scans the open file for real defects and offers a one-click fix for each |
| **Default** | The classic Block List / Block Details layout |

Plus a Blender-shaped editing layer over the viewport: object and edit modes,
the modeling operators (extrude, bevel, knife, loop cut, inset, bridge,
subdivide, dissolve, symmetrize, merge, smooth, quads), modal transforms with
snapping, operator redo panels, and a shortcut set that will not surprise you if
you came from Blender. Shortcuts are rebindable.

And a **headless CLI** (`NifSkope -no-gui`) that can inspect, query, edit, merge,
pose and animation-rig NIFs from a script without opening a window.

See **[WW_FEATURES.md](WW_FEATURES.md)** for the itemised list.

### Download

Builds are published under [Releases](https://github.com/bungov/NifSkope-WildWastelandEdition/releases).
Windows x64.

### Building from source

Requires Qt 6.4+ (Qt 5.15 also works). On Windows, build in
[MSYS2](https://www.msys2.org/). From the MSYS2-UCRT64 shell:

```
pacman -S base-devel mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-qt6-base
pacman -S mingw-w64-ucrt-x86_64-qt6-imageformats mingw-w64-ucrt-x86_64-qt6-tools
pacman -S git
```

Then:

```
git clone https://github.com/bungov/NifSkope-WildWastelandEdition.git
cd NifSkope-WildWastelandEdition
qmake6 NifSkope.pro
make -j 8
```

Output lands in `release/`. There are no submodules — every dependency
(`qhull`, `gli`, `meshoptimizer`, `libfo76utils`, the XML definitions) is
vendored in the tree, so a plain `git clone` is complete.

By default the compiler targets Intel Haswell / AMD Zen or newer. `qmake6
noavx2=1` drops that to Ivy Bridge, `nof16c=1` to Sandy Bridge, `noavx=1`
lower still. `debug=1` builds a debug binary.

Using MSYS2-CLANG64 instead of UCRT64 works; replace **ucrt** with **clang** in
the package names. For Qt 5, replace **qt6** with **qt5** and run `qmake-qt5`.

**Note:** `README.md` is generated at link time from `build/README.md.in` — edit
the `.in` file, not this one.

#### Tests

The regression harnesses are shell scripts that drive the real binary with an
environment flag and assert on what it writes:

```
tests/spells/top_bar.sh
tests/spells/collision_undo.sh
tests/spells/scrub_uniform.sh
```

`tests/spells/_harness.sh` holds the shared setup. Each script prints a
pass/fail count and exits non-zero on any failure.

### Lineage and license

This is a fork of [fo76utils/nifskope](https://github.com/fo76utils/nifskope),
which is itself a fork of
[niftools/nifskope](https://github.com/niftools/nifskope). The upstream history
is preserved as a squashed baseline commit; everything after it is this fork's
work.

NifSkope is free software under a BSD 3-clause license — see
[LICENSE.md](LICENSE.md) and [CONTRIBUTORS.md](CONTRIBUTORS.md). That license
carries forward here unchanged, and the upstream copyright notices are intact.
Credit for everything this fork did not write belongs to the NIF File Format
Library and Tools project and to fo76utils.

Vendored third-party code keeps its own license, in its own directory:
`lib/qhull` (Qhull), `lib/gli` (MIT), `lib/meshoptimizer` (MIT),
`lib/libfo76utils`.

### Documentation

| | |
|---|---|
| [WW_FEATURES.md](WW_FEATURES.md) | Everything this fork adds, vs. upstream |
| [HANDOFF.md](HANDOFF.md) | Project state, build commands, where things live |
| [WW_CHANGES.md](WW_CHANGES.md) | Dated change history, in detail |
| [docs/CLI.md](docs/CLI.md) | The headless command line |
| [docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) | The backlog |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Inherited troubleshooting notes |
| [docs/README_GLTF.md](docs/README_GLTF.md) | glTF import/export |
| [CHANGELOG.md](CHANGELOG.md) | Upstream's changelog, up to the fork point |

Internal version: NifSkope 2.0.dev11.
