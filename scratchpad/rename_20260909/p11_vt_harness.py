# Patch 11 -- the terrain virtual-texture harnesses follow the container to
# .lodt and to the LDTX magic, and lodgen_terrain_vt.sh gains the OTHER half of
# the refusal gate: a REAL baked .lodt handed to the landscape route.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


class F:
    def __init__(self, p):
        self.p = p
        b = open(p, "rb").read()
        assert b.count(b"\r") == 0, p
        self.s = b.decode("utf-8")

    def sub(self, old, new, n=1):
        c = self.s.count(old)
        assert c == n, "%s: anchor count %d != %d for %r" % (self.p, c, n, old[:80])
        self.s = self.s.replace(old, new)

    def all(self, old, new, atleast=1):
        c = self.s.count(old)
        assert c >= atleast, "%s: %r appears %d times" % (self.p, old, c)
        self.s = self.s.replace(old, new)
        return c

    def save(self):
        out = self.s.encode("utf-8")
        assert out.count(b"\r") == 0
        open(self.p, "wb").write(out)
        print("OK", self.p, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))


v = F("tests/spells/lodgen_terrain_vt.sh")
v.sub("# 256-texel tiles with an 8-texel border, one .lodv container per level under",
      "# 256-texel tiles with an 8-texel border, one .lodt container per level under")
# every container path and every CLI/keyword use. src/io/lodvfile.cpp is a
# SOURCE path and must not move.
print("  .lodv paths ->", v.all(".lodv", ".lodt"))
print("  --lodv-check ->", v.all("--lodv-check", "--lodt-check"))
print("  ^lodv keyword ->", v.all("'^lodv", "'^lodt"))
# the log file names moved with the keyword; keep them legible
print("  $W/lodv.txt ->", v.all('"$W/lodv.txt"', '"$W/lodt.txt"'))

# --- the other half of the refusal gate, on a REAL baked container ---------
v.sub("""ROOTC="$DIR/Commonwealth.VT.8.lodt\"""",
      """ROOTC="$DIR/Commonwealth.VT.8.lodt"

# The landscape route must NAME this file rather than misparse it: `.lodt` was
# the landscape file's own extension until 2026-09-09. This is the real-file
# half of the pair; tests/spells/lodl_write.sh does the other direction and a
# synthetic header.
if "$NS" -no-gui lodl "$ROOTC" --info > "$W/land_on_tex.txt" 2>&1; then
	bad "the landscape route refuses a real terrain texture container"
else
	if grep -qai "TEXTURE file" "$W/land_on_tex.txt"; then
		ok "the landscape route names the .lodt texture container it was handed"
	else
		bad "the landscape route's refusal names the texture container"
		sed 's/^/       /' "$W/land_on_tex.txt"
	fi
fi""")
v.save()

c = F("tests/spells/lodgen_vt_check.py")
c.sub("This re-implements the .lodv LAYOUT from docs/LODGEN_TERRAIN_VT.md -- header,",
      "This re-implements the .lodt terrain-texture LAYOUT from\n"
      "docs/LODGEN_TERRAIN_VT.md (the container was .lodv until 2026-09-09) -- header,")
c.sub("\t\t\t  v.magic == b'LODV' and v.version == 1 and v.headerBytes == HDR)",
      "\t\t\t  v.magic == b'LDTX' and v.version == 1 and v.headerBytes == HDR)")
c.save()

for p in ("tests/spells/lodgen_terrain_vt.sh", "tests/spells/lodgen_vt_check.py"):
    for i, ln in enumerate(open(p, "rb").read().decode("utf-8").split("\n"), 1):
        if "lodv" in ln.lower():
            print("  check %s @%d: %s" % (p, i, ln.strip()[:100]))
