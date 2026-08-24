"""Build the hybrid: OUR NIF wrapper carrying THE GAME'S OWN collision blobs.

Static analysis is saturated -- five levels of comparison and our own ragdoll
solver all say our file is equivalent to the game's, and the game disagrees. So
stop reasoning about which differences are inert and SPLIT THE FILE instead.

This file is our decompile/recompile output in every respect except the two
packfile blobs, which are the game's bytes verbatim. Blob lengths are equal, so
nothing in the NIF moves.

  behaves correctly -> the fault is IN the blobs, i.e. one of the differences I
                       proved inert is not (capsule core-box roll, shape-list
                       order, or the hkVector4 w lanes)
  still misbehaves  -> the fault is in the NIF around them, which every block
                       comparison says is identical -- so it would be the byte
                       layout or the header, and I would go looking there
"""
import subprocess
import sys

CLI = r"E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
SP = r"C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\11afc0a0-b384-4dca-98e5-6ab1294f48ec\scratchpad"
MOD = r"E:\Projects\Fallout 4 Mods\mods\WW Ragdoll Test"
OUT = r"E:\Projects\Fallout 4 Mods\mods\WW Ragdoll Hybrid"


def blocks(path, kind):
    p = subprocess.run([CLI, "-no-gui", "list", path], capture_output=True, text=True,
                       errors="replace")
    out = []
    for line in p.stdout.splitlines():
        if line.strip().endswith(kind):
            out.append(int(line.split("]")[0].lstrip("[")))
    return out


def extract(path, blk, dst):
    subprocess.run([CLI, "-no-gui", "collision", path, "--extract", "-b", str(blk),
                    "-o", dst], capture_output=True)


for game_src, our_rel, tag in (
        (SP + r"\ba2_human.nif", r"Meshes\Actors\Character\CharacterAssets\skeleton.nif", "hum"),
        (SP + r"\ba2_brahmin.nif", r"Meshes\Actors\Brahmin\CharacterAssets\Skeleton.nif", "bra")):
    ours_path = MOD + "\\" + our_rel
    data = open(ours_path, "rb").read()
    replaced = 0
    for kind in ("bhkRagdollSystem", "bhkPhysicsSystem"):
        gb = blocks(game_src, kind)
        ob = blocks(ours_path, kind)
        if len(gb) != len(ob):
            sys.exit("%s: %s count %d vs %d" % (tag, kind, len(gb), len(ob)))
        for i, (g, o) in enumerate(zip(gb, ob)):
            gf = "%s\\%s_g_%s%d.bin" % (SP, tag, kind[3:6], i)
            of = "%s\\%s_o_%s%d.bin" % (SP, tag, kind[3:6], i)
            extract(game_src, g, gf)
            extract(ours_path, o, of)
            gblob = open(gf, "rb").read()
            oblob = open(of, "rb").read()
            if len(gblob) != len(oblob):
                sys.exit("%s %s[%d]: blob lengths %d vs %d" % (tag, kind, i, len(gblob), len(oblob)))
            if data.count(oblob) != 1:
                sys.exit("%s %s[%d]: our blob appears %d times" % (tag, kind, i, data.count(oblob)))
            data = data.replace(oblob, gblob, 1)
            replaced += 1
    dst = OUT + "\\" + our_rel
    import os
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "wb").write(data)
    print("%-4s %s  %d blob(s) swapped to the game's bytes, %d bytes total"
          % (tag, our_rel.rsplit("\\", 1)[-1], replaced, len(data)))
