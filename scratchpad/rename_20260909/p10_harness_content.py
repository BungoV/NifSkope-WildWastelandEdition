# Patch 10 -- the renamed harnesses' contents, and the NEW refusal gate.
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


# ------------------------------------------------------------- lodl_write.sh
w = F("tests/spells/lodl_write.sh")
w.sub("# The .lodt whole-worldspace landscape writer.",
      "# The .lodl whole-worldspace landscape writer.\n"
      "#\n"
      "# Was lodt_write.sh until bungo's 2026-09-09 ruling: the landscape file is\n"
      "# .lodl now and .lodt names the terrain texture sheets. The BYTES did not\n"
      "# change with the name -- the magic still reads LODT on disk -- and check 1\n"
      "# below is what pins that.")
print("  --lodt ->", w.all("--lodt ", "--lodl "))
print("  Commonwealth.lodt ->", w.all("Commonwealth.lodt", "Commonwealth.lodl"))
print("  NukaWorldAmphitheater.lodt ->",
      w.all("NukaWorldAmphitheater.lodt", "NukaWorldAmphitheater.lodl"))
w.sub("WW_LODT_VERSION=1", "WW_LODL_VERSION=1")
w.sub('echo "FAIL: no .lodt written"', 'echo "FAIL: no .lodl written"')
w.sub("""check("magic is LODT", b[0:4] == b'LODT')""",
      """# The extension moved on 2026-09-09; the MAGIC deliberately did not, because
# the gate on the rename is that a .lodl is byte-identical to the .lodt the same
# worldspace wrote the day before. What tells the two formats apart is the
# TEXTURE file's own magic, LDTX -- see the refusal section at the end.
check("magic is still LODT after the .lodl rename", b[0:4] == b'LODT')""")
w.sub("# The .lodt and the HeightMap.dds are read by FO4CS as ONE surface -- terrain",
      "# The .lodl and the HeightMap.dds are read by FO4CS as ONE surface -- terrain")
w.sub('check("the .lodt grid and the heightmap are the same size (%dx%d)" % (w, h),',
      'check("the .lodl grid and the heightmap are the same size (%dx%d)" % (w, h),')
w.sub("""    print("  first difference: cell (%d,%d) col %d row %d  lodt %d  heightmap %d\"""",
      """    print("  first difference: cell (%d,%d) col %d row %d  lodl %d  heightmap %d\"""")
w.sub('bad2 "both the .lodt and the heightmap were produced"',
      'bad2 "both the .lodl and the heightmap were produced"')
w.sub('bad2 "the NukaWorldAmphitheater .lodt writes"',
      'bad2 "the NukaWorldAmphitheater .lodl writes"')

# ---- the NEW gate: each reader refuses the other's file BY NAME -----------
w.sub("""[ "$rc" = 0 ] && echo PASS || echo FAIL
exit $rc""",
      """# --- the two readers refuse each other's file, BY NAME -----------------
#
# `.lodt` named THIS format until 2026-09-09 and names the terrain texture
# sheets now, so the one mistake a user or a script will really make is handing
# one route the other's file. Neither may say only "bad magic", and neither may
# parse it: the landscape reader must NAME the texture file and the texture
# validator must NAME the landscape file.
#
# The controls are on both sides of each check. A file that IS the right format
# must still open (or the refusal proves nothing but that the route is broken),
# and the refusal text must contain the OTHER format's name (or "refused"
# alone would pass on any error at all).
echo "== each reader refuses the other's file by name =="

# a real landscape file, under its new name and unchanged in content
cp "$F" "$W/Commonwealth_probe.lodl"
# the smallest thing that is unmistakably a terrain TEXTURE file: 0x98 bytes
# whose first four are the LDTX magic. The land reader reads the header before
# anything else, so this reaches exactly the branch under test.
"$PY" -c "import sys; open(sys.argv[1],'wb').write(b'LDTX'+bytes(0x94))" \\
	"$W/fake_texture.lodt"

# CONTROL: the landscape route opens a real .lodl
if "$NS" -no-gui lodl "$W/Commonwealth_probe.lodl" --info > "$W/ctl_land.txt" 2>&1; then
	ok2 "control: the landscape route opens a .lodl"
else
	bad2 "control: the landscape route opens a .lodl"; cat "$W/ctl_land.txt"
fi

# the landscape route, handed a terrain texture file
if "$NS" -no-gui lodl "$W/fake_texture.lodt" --info > "$W/ref_land.txt" 2>&1; then
	bad2 "the landscape route REFUSES a .lodt terrain texture file"
else
	if grep -qi "TEXTURE file" "$W/ref_land.txt"; then
		ok2 "the landscape route names the .lodt texture file it was handed"
		say "$(head -1 "$W/ref_land.txt")"
	else
		bad2 "the landscape route's refusal NAMES the texture file"
		cat "$W/ref_land.txt"
	fi
fi

# the texture validator, handed the landscape file (the old .lodt bytes)
cp "$F" "$W/old_meaning.lodt"
if "$NS" -no-gui lodgen --lodt-check "$W/old_meaning.lodt" > "$W/ref_tex.txt" 2>&1; then
	bad2 "the texture route REFUSES an old-meaning .lodt (the landscape file)"
else
	if grep -qi "lodl" "$W/ref_tex.txt"; then
		ok2 "the texture route names the landscape file it was handed"
		say "$(grep -i refused "$W/ref_tex.txt" | head -1)"
	else
		bad2 "the texture route's refusal NAMES the landscape file"
		cat "$W/ref_tex.txt"
	fi
fi

# the retired spellings fail LOUDLY and name their replacement
if "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodt "$W/retired" \\
	> "$W/retired.txt" 2>&1; then
	bad2 "--lodt is refused"
else
	grep -qi -- "--lodl" "$W/retired.txt" \\
		&& ok2 "--lodt is refused and names --lodl" \\
		|| { bad2 "--lodt's refusal names --lodl"; cat "$W/retired.txt"; }
fi
if "$NS" -no-gui lodt "$W/Commonwealth_probe.lodl" --info > "$W/retired2.txt" 2>&1; then
	bad2 "the 'lodt' command is refused"
else
	grep -qi "lodl" "$W/retired2.txt" \\
		&& ok2 "the 'lodt' command is refused and names 'lodl'" \\
		|| { bad2 "the 'lodt' command's refusal names 'lodl'"; cat "$W/retired2.txt"; }
fi
if WW_LODT_VERSION=1 "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/oldenv" \\
	> "$W/oldenv.txt" 2>&1; then
	bad2 "WW_LODT_VERSION is refused"
else
	grep -qi "WW_LODL_VERSION" "$W/oldenv.txt" \\
		&& ok2 "WW_LODT_VERSION is refused and names WW_LODL_VERSION" \\
		|| { bad2 "WW_LODT_VERSION's refusal names WW_LODL_VERSION"; cat "$W/oldenv.txt"; }
fi

# and the renamed file still VERIFIES against its own source: the rename moved
# the name, not a byte.
mkdir -p "$W/verify/Terrain"
cp "$F" "$W/verify/Terrain/Commonwealth.lodl"
if "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/verify" --verify-only \\
	> "$W/verify.txt" 2>&1; then
	MM="$(grep -o '[0-9]* mismatched' "$W/verify.txt" | head -1)"
	say "verify-only under the new name: ${MM:-no mismatch line}"
	echo "$MM" | grep -q '^0 mismatched' \\
		&& ok2 "the file verifies under its new name with 0 mismatches" \\
		|| ok2 "the file verifies under its new name (rc 0)"
else
	bad2 "the file verifies under its new name"; tail -5 "$W/verify.txt"
fi

[ "$rc" = 0 ] && echo PASS || echo FAIL
exit $rc""")
w.save()

# -------------------------------------------------------------- lodl_open.sh
o = F("tests/spells/lodl_open.sh")
o.sub("# Opening a `.lodt` — the geometry carries the file's own heights, and EVERY",
      "# Opening a `.lodl` — the geometry carries the file's own heights, and EVERY")
o.sub("# A `.lodt` stores no triangles: it is one whole worldspace's landscape —",
      "# A `.lodl` stores no triangles: it is one whole worldspace's landscape —")
o.sub("# THE AUTHORITY IS NOT OUR OWN READER. `lodt_open_authority.py` decodes the",
      "# THE AUTHORITY IS NOT OUR OWN READER. `lodl_open_authority.py` decodes the")
o.sub("#   bash tests/spells/lodt_open.sh\n#   LODT=<path> bash tests/spells/lodt_open.sh",
      "#   bash tests/spells/lodl_open.sh\n#   LODL=<path> bash tests/spells/lodl_open.sh")
o.sub('LODT="${LODT:-E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodt}"',
      'LODL="${LODL:-E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl}"')
o.sub('AUTH="$ROOT/tests/spells/lodt_open_authority.py"',
      'AUTH="$ROOT/tests/spells/lodl_open_authority.py"')
o.sub('[ -f "$LODT" ] || { echo "no fixture at $LODT — regenerate with: $NS -no-gui lodgen '
      '<Fallout4.esm> --worldspace 3C --lodt <dir>"; exit 2; }',
      '[ -f "$LODL" ] || { echo "no fixture at $LODL — regenerate with: $NS -no-gui lodgen '
      '<Fallout4.esm> --worldspace 3C --lodl <dir>"; exit 2; }')
print("  $LODT ->", o.all('"$LODT"', '"$LODL"'))
print("  -no-gui lodt ->", o.all("-no-gui lodt ", "-no-gui lodl "))
o.sub('WW_LODT_REGION="$CX0,$CY0,$CX1,$CY1,2" WW_LODT_PLANE="$1" \\',
      'WW_LODL_REGION="$CX0,$CY0,$CX1,$CY1,2" WW_LODL_PLANE="$1" \\')
o.save()

# --------------------------------------------------------------- lodl_btd.sh
d = F("tests/spells/lodl_btd.sh")
d.sub("# Converting a Fallout 76 .btd to .lodt.", "# Converting a Fallout 76 .btd to .lodl.")
print("  --lodt ->", d.all("--lodt ", "--lodl "))
d.sub('F="$(ls "$W"/Terrain/*.lodt 2>/dev/null | head -1)"',
      'F="$(ls "$W"/Terrain/*.lodl 2>/dev/null | head -1)"')
d.sub('echo "  ok   a .lodt was written', 'echo "  ok   a .lodl was written')
d.sub('echo "  FAIL no .lodt was written"', 'echo "  FAIL no .lodl was written"')
d.save()

# ------------------------------------------------------ lodl_open_authority.py
a = F("tests/spells/lodl_open_authority.py")
a.sub('"""The RIGHT-HAND SIDE of tests/spells/lodt_open.sh -- a .lodt decoder that',
      '"""The RIGHT-HAND SIDE of tests/spells/lodl_open.sh -- a .lodl decoder that')
a.sub("`lodt_open.sh` asks whether a scene NifSkope MESHED from a .lodt carries the",
      "`lodl_open.sh` asks whether a scene NifSkope MESHED from a .lodl carries the")
print("  usage lines ->", a.all("python lodt_open_authority.py <file.lodt>",
                                "python lodl_open_authority.py <file.lodl>"))
a.sub("""        if len(h) < 0x98 or h[0:4] != b'LODT':
            raise SystemExit('not a .lodt: %s' % path)""",
      """        # The magic still spells LODT: the 2026-09-09 rename moved the
        # extension, deliberately not a byte of the file.
        if len(h) < 0x98 or h[0:4] != b'LODT':
            raise SystemExit('not a .lodl: %s' % path)""")
a.sub("""        \"\"\"The plane list `lodt --info` must print, derived from the header""",
      """        \"\"\"The plane list `lodl --info` must print, derived from the header""")
a.save()

# ---------------------------------------------------------- lod_generation.sh
g = F("tests/spells/lod_generation.sh")
g.sub("# (lodt_write.sh, lodgen_terrain.sh, lodt_btd.sh) and a GUI harness that",
      "# (lodl_write.sh, lodgen_terrain.sh, lodl_btd.sh) and a GUI harness that")
g.save()

for p in ("tests/spells/lodl_write.sh", "tests/spells/lodl_open.sh",
          "tests/spells/lodl_btd.sh", "tests/spells/lodl_open_authority.py"):
    for i, ln in enumerate(open(p, "rb").read().decode("utf-8").split("\n"), 1):
        if "lodt" in ln.lower() and "LODT" != ln.strip():
            print("  check %s @%d: %s" % (p, i, ln.strip()[:100]))
