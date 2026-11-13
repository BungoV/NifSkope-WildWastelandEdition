"""Lane VKWATER1 (row G5): the water twins' sampled inputs, from spirv-cross --reflect JSON of the shipped HLSL ports.
Usage: waterview_prep.py <out slots.txt> <tag>=<reflect.json> ...
Writes one line per input name (union over the twins, first binding seen): `<name> <type> <binding> <tags>`, where
type is spirv-cross's image type (texture2D, utexture2D, texture3D, texture2DArray, textureCubeArray, *Buffer ...)."""
import json
import sys

out = sys.argv[1]
rows = {}
order = []
for arg in sys.argv[2:]:
    tag, path = arg.split("=", 1)
    j = json.load(open(path))
    imgs = j.get("separate_images", []) + j.get("textures", [])
    if not imgs:
        print(f"waterview_prep: {tag} has no sampled images")
        sys.exit(1)
    for r in imgs:
        n = r["name"]
        if n not in rows:
            rows[n] = [r.get("type", "?"), r.get("binding", -1), []]
            order.append(n)
        rows[n][2].append(tag)
with open(out, "w", newline="\n") as f:
    for n in order:
        t, b, tags = rows[n]
        f.write(f"{n} {t} {b} {','.join(tags)}\n")
print(f"waterview_prep: {len(order)} inputs over {len(sys.argv) - 2} twins")
