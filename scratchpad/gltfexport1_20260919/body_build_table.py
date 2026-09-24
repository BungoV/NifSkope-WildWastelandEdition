# The FO4 body-build triangle evaluated in Python, from the RACE table.
# This is the INDEPENDENT arm of tests/spells/body_build.sh: the panel's C++
# answer is compared against this file's, and this file is compared against
# eight files BETHESDA SHIPPED.
#
#     s = a*thin + m*musc + f*fat  -  k * ( (thin+musc+fat)/3 - 1 )
#     k = ( R - |P - centroid| ) / R ,  R = 1/sqrt(3) (the circumradius)
#     P = a*(0,h) + m*(0.5,0) + f*(1,h) ,  h = sqrt(3)/2
#
# so k = 0 at every corner and k = 1 at the centre.
#
# THE CONTROL. Bethesda ships the game's own answer as loose JSON beside the
# meshes: Data/Meshes/Actors/Character/CharacterAssets/
#   HumanRaceBoneScales{Male,Female}{Thin,Muscular,Fat,Default}.txt
# -- what TESRace::ImportBodyMorphBoneBaseScales reads at CK time to FILL the
# RACE record. So the question has a written answer on this machine.
#
# WHAT IT SHOWS (run 2026-09-19, 48 bones per gender):
#   * at a corner the result IS the raw stored corner vector, on Y and Z, to
#     double epsilon (1.11e-16). k = 0 there.
#   * at the centre it is exactly (1,1,1). k = 1 there.
#   * X is a DATA divergence, not an arithmetic one: the RACE record stores
#     X = 1.0 everywhere, the .txt carries an authored X on about ten bones
#     (RBreast_skin 1.0278, female fat LArm_Collarbone_skin 1.1840). The .txt
#     is the import SOURCE; the engine reads the record. So X is compared and
#     reported but does not fail the control.
#
# WHAT WOULD REFUTE THE PART THAT IS STILL OPEN: the SHAPE of k between the
# centre and a corner is not pinned by any of these files. k = 3*min(w) fits
# the same two ends. At the midpoint of an edge, w = (0.5,0,0.5), this file
# says k = 0.5 and that rival says k = 0 -- one in-game reading there decides.
#
# Lane GLTFEXPORT1, 2026-09-19.
import json
import math
import os
import sys

H = math.sqrt(3.0) * 0.5
R = 1.0 / math.sqrt(3.0)
CX, CY = 0.5, 2.0 * H / 3.0

CORNERS = [("thin", (1.0, 0.0, 0.0)),
           ("muscular", (0.0, 1.0, 0.0)),
           ("fat", (0.0, 0.0, 1.0)),
           ("centroid", (1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0))]

ASSETS = "E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets"
EPS = 1e-5


def k_of(a, m, f):
    s = a + m + f
    if not s > 0.0:
        return 1.0
    a, m, f = a / s, m / s, f / s
    px = m * 0.5 + f * 1.0
    py = a * H + f * H
    d = math.hypot(px - CX, py - CY)
    return max(0.0, min(1.0, (R - d) / R))


def scale_of(bone, a, m, f):
    s = a + m + f
    if not s > 0.0:
        a = m = f = 1.0 / 3.0
    else:
        a, m, f = a / s, m / s, f / s
    k = k_of(a, m, f)
    out = []
    for i in range(3):
        v0, v1, v2 = bone["thin"][i], bone["muscular"][i], bone["fat"][i]
        mean = (v0 + v1 + v2) / 3.0
        out.append(a * v0 + m * v1 + f * v2 - k * (mean - 1.0))
    return out


def load_table(path):
    """race_bone_data.json as dump_race_bones.py writes it:
         {"form","edid","sets":[{"gender",0|1,"gender_name","weight_scales":[
              {"bone","thin","muscular","fat"}...],"range_modifiers":[...]}]}"""
    raw = json.load(open(path))
    if not isinstance(raw, dict) or "sets" not in raw:
        raise SystemExit("race_bone_data.json: no 'sets'; keys are %s"
                         % (list(raw)[:6] if isinstance(raw, dict) else type(raw)))
    out = {}
    for s in raw["sets"]:
        gender = str(s.get("gender_name", "male")).lower()
        out[gender] = [{
            "name": r.get("bone") or r.get("name"),
            "thin": [float(x) for x in r["thin"]],
            "muscular": [float(x) for x in r["muscular"]],
            "fat": [float(x) for x in r["fat"]],
        } for r in s.get("weight_scales", [])]
    return out


def shipped(gender, which):
    """The Scale of every bone in HumanRaceBoneScales<G><Which>.txt, or None.
    A JSON array of {Name, Position, Rotation, Scale}, CRLF, Scale an OBJECT."""
    p = os.path.join(ASSETS, "HumanRaceBoneScales%s%s.txt"
                     % (gender.capitalize(), which.capitalize()))
    if not os.path.isfile(p):
        return None
    try:
        doc = json.loads(open(p, "r", errors="replace").read())
    except Exception as e:
        print("    (%s did not parse: %s)" % (os.path.basename(p), e))
        return None
    out = {}
    for o in doc if isinstance(doc, list) else []:
        n, s = o.get("Name"), o.get("Scale")
        if n and isinstance(s, dict):
            out[str(n)] = [float(s["x"]), float(s["y"]), float(s["z"])]
    return out or None


def main():
    jpath = sys.argv[1] if len(sys.argv) > 1 else \
        "E:/Projects/Claude/tools/body_build_anim/race_bone_data.json"
    table = load_table(jpath)
    print("table: %s" % jpath)
    bad = 0
    for gender, bones in sorted(table.items()):
        print("")
        print("=== %s : %d bones" % (gender, len(bones)))
        xs = set((b["thin"][0], b["muscular"][0], b["fat"][0]) for b in bones)
        print("    X values across all corners: %s" % sorted(xs))

        # CONTROL 1: the centroid is (1,1,1)
        worst, worstb = 0.0, ""
        for b in bones:
            for v in scale_of(b, 1 / 3.0, 1 / 3.0, 1 / 3.0):
                if abs(v - 1.0) > worst:
                    worst, worstb = abs(v - 1.0), b["name"]
        print("    CONTROL centroid == (1,1,1): max |s-1| = %.3g on %s" % (worst, worstb))
        if worst > 1e-6:
            bad += 1
            print("    REFUTED: the centroid is not the identity")

        # CONTROL 2: the four SHIPPED files. Y and Z must match; X is reported.
        for which, w in (("Default", CORNERS[3][1]), ("Thin", CORNERS[0][1]),
                         ("Muscular", CORNERS[1][1]), ("Fat", CORNERS[2][1])):
            sd = shipped(gender, which)
            if sd is None:
                print("    shipped HumanRaceBoneScales%s%s.txt: NOT ON THIS MACHINE"
                      % (gender.capitalize(), which))
                continue
            wyz, wyzb, wx, wxb, nb = 0.0, "", 0.0, "", 0
            for b in bones:
                ref = sd.get(b["name"])
                if ref is None:
                    continue
                nb += 1
                s = scale_of(b, *w)
                if abs(s[0] - ref[0]) > wx:
                    wx, wxb = abs(s[0] - ref[0]), b["name"]
                for i in (1, 2):
                    if abs(s[i] - ref[i]) > wyz:
                        wyz, wyzb = abs(s[i] - ref[i]), b["name"]
            print("    CONTROL vs shipped %-8s (%d of %d bones): YZ %.3g on %-22s  X %.3g on %s"
                  % (which, nb, len(sd), wyz, wyzb, wx, wxb))
            if wyz > EPS:
                bad += 1
                print("        REFUTED: the formula does not reproduce the shipped file on Y/Z")

        # CONTROL 3 and its FLOOR: at a corner the answer IS the raw vector,
        # and at the centre it is NOT (that is what makes the k term real).
        b = next((x for x in bones if x["name"].lower() == "belly_skin"), bones[0])
        print("    on %s:" % b["name"])
        for nm, w in CORNERS:
            s = scale_of(b, *w)
            raw = b.get(nm)
            print("        %-9s k=%.6f  ->  %.6f %.6f %.6f%s"
                  % (nm, k_of(*w), s[0], s[1], s[2],
                     ("   raw %.6f %.6f %.6f" % tuple(raw)) if raw else ""))
        for nm, w in CORNERS[:3]:
            s, raw = scale_of(b, *w), b[nm]
            if max(abs(s[i] - raw[i]) for i in range(3)) > EPS:
                bad += 1
                print("        REFUTED: the %s corner is not the raw stored vector" % nm)
        mean = [(b["thin"][i] + b["muscular"][i] + b["fat"][i]) / 3.0 for i in range(3)]
        cen = scale_of(b, 1 / 3.0, 1 / 3.0, 1 / 3.0)
        print("        FLOOR the centre is NOT the plain weighted sum: mean would be "
              "%.6f %.6f %.6f, the answer is %.6f %.6f %.6f"
              % (mean[0], mean[1], mean[2], cen[0], cen[1], cen[2]))
        if max(abs(cen[i] - mean[i]) for i in range(3)) < EPS:
            bad += 1
            print("        the k term does nothing on this bone; pick another floor bone")

        print("    EXPECTED (the gate's table), all %d bones x 4 states:" % len(bones))
        for nm, w in CORNERS:
            for b in bones:
                s = scale_of(b, *w)
                print("ROW %s %s %s %.6f %.6f %.6f" % (gender, nm, b["name"], s[0], s[1], s[2]))

    print("")
    print("controls failed: %d" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
