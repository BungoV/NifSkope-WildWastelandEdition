import os, re
D = r"E:\Tools\Fallout 4\DataUnpacked\Data\meshes\sky"
for n in ["AtmosphereDome002.nif", "Atmosphere.nif", "Clouds.nif", "Stars.nif", "CloudShape01.nif"]:
    b = open(os.path.join(D, n), "rb").read()
    types = sorted(set(m.decode() for m in re.findall(rb"(?:BS|Ni|bhk)[A-Za-z]{3,40}", b[:4096])))
    print("%-22s %7d bytes  %s" % (n, len(b), ",".join(t for t in types if "Shader" in t or "TriShape" in t or "Geometry" in t)))
