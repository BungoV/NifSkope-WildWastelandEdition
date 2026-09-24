# THE DECISIVE CONTROL for the body-build combination, run against the files
# BETHESDA SHIPPED. Lane GLTFEXPORT1, 2026-09-19.
#
# Data/Meshes/Actors/Character/CharacterAssets/HumanRaceBoneScales<G><C>.txt is
# what TESRace::ImportBodyMorphBoneBaseScales reads: the game's own answer for
# one gender at one corner, written down by the game's own tools. So the
# question "what does the engine do with the three stored corners" has a
# written answer on this machine, and it does not need the disassembly at all.
#
# This compares, bone for bone and axis for axis:
#   RAW      the corner vector stored in the RACE record
#   FORMULA  the k-term combination of src/bodybuild.h evaluated at that corner
# against the shipped file, for every gender x corner pairing, so a gender
# mix-up cannot be mistaken for a formula being right.
import json
import math
import os
import sys

H = math.sqrt(3.0) * 0.5
R = 1.0 / math.sqrt(3.0)
CX, CY = 0.5, 2.0 * H / 3.0
ASSETS = "E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets"
W = {"thin": (1.0, 0.0, 0.0), "muscular": (0.0, 1.0, 0.0), "fat": (0.0, 0.0, 1.0)}


def k_of(a, m, f):
    px = m * 0.5 + f * 1.0
    py = a * H + f * H
    d = math.hypot(px - CX, py - CY)
    return max(0.0, min(1.0, (R - d) / R))


def formula(b, a, m, f):
    k = k_of(a, m, f)
    out = []
    for i in range(3):
        v0, v1, v2 = b["thin"][i], b["muscular"][i], b["fat"][i]
        mean = (v0 + v1 + v2) / 3.0
        out.append(a * v0 + m * v1 + f * v2 - k * (mean - 1.0))
    return out


def load_race(path):
    raw = json.load(open(path))
    out = {}
    for s in raw["sets"]:
        g = str(s["gender_name"]).lower()
        out[g] = {r["bone"]: {"thin": r["thin"], "muscular": r["muscular"], "fat": r["fat"]}
                  for r in s["weight_scales"]}
    return out


def load_file(gender, which):
    p = os.path.join(ASSETS, "HumanRaceBoneScales%s%s.txt" % (gender.capitalize(), which.capitalize()))
    if not os.path.isfile(p):
        return None
    doc = json.loads(open(p, "r", errors="replace").read())
    return {o["Name"]: [float(o["Scale"]["x"]), float(o["Scale"]["y"]), float(o["Scale"]["z"])]
            for o in doc if "Name" in o and isinstance(o.get("Scale"), dict)}


def cmp(a, b):
    """max abs diff over the shared bones, and the bone it happened on."""
    worst, wb, n = 0.0, "", 0
    for name, v in a.items():
        r = b.get(name)
        if r is None:
            continue
        n += 1
        for i in range(3):
            if abs(v[i] - r[i]) > worst:
                worst, wb = abs(v[i] - r[i]), name
    return worst, wb, n


def main():
    race = load_race(sys.argv[1] if len(sys.argv) > 1
                     else "E:/Projects/Claude/tools/body_build_anim/race_bone_data.json")
    for gender in sorted(race):
        bones = race[gender]
        for which in ("thin", "muscular", "fat"):
            ref = load_file(gender, which)
            if ref is None:
                print("%s %s: file missing" % (gender, which))
                continue
            raw = {n: b[which] for n, b in bones.items()}
            fml = {n: formula(b, *W[which]) for n, b in bones.items()}
            r1 = cmp(raw, ref)
            r2 = cmp(fml, ref)
            print("%-7s %-9s  RAW vs file: %.3g (%s, %d bones)   FORMULA vs file: %.3g (%s)"
                  % (gender, which, r1[0], r1[1], r1[2], r2[0], r2[1]))
            # every bone where RAW is not the file, so "a few outliers" cannot
            # be confused with "the whole table is wrong"
            off = []
            for n, v in raw.items():
                r = ref.get(n)
                if r is None:
                    continue
                d = max(abs(v[i] - r[i]) for i in range(3))
                if d > 1e-5:
                    off.append((d, n, v, r))
            off.sort(reverse=True)
            print("        bones where RAW != file by more than 1e-5: %d of %d" % (len(off), r1[2]))
            for d, n, v, r in off[:8]:
                print("          %-24s raw %.6f %.6f %.6f   file %.6f %.6f %.6f   (%.4g)"
                      % (n, v[0], v[1], v[2], r[0], r[1], r[2], d))
        # the cross pairing, so a gender mix-up is visible
        other = "female" if gender == "male" else "male"
        if other in race:
            ref = load_file(gender, "fat")
            if ref:
                raw_other = {n: b["fat"] for n, b in race[other].items()}
                c = cmp(raw_other, ref)
                print("        CROSS FLOOR: the %s raw fat vs the %s fat file: %.3g (%s)"
                      % (other, gender, c[0], c[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
