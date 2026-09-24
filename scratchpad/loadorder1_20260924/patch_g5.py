p = 'E:/Projects/NifskopeWWE-loadorder1/tests/spells/lodgen_loadorder.sh'
s = open(p, encoding='utf-8').read()
cr = s.count('\r')
old = '''mkdir -p "$W/g5/stock" "$W/g5/tex"
t0=$(date +%s)
lg --mo2-profile "$PROFILE" --worldspace 3C --terrain-region -20 24 -20 24 --dim 4 \\
	--out-dir "$WA/g5/stock" --tex-dir "$WA/g5/tex" --native "$WA/g5" > "$W/g5.log"; rc=$?
'''
new = '''# His live load order first, as information: the ESM reader (libfo76utils) refuses a plugin whose raw form
# IDs sit above 0x0FFFFFFF (TestWorldspace.esp: 483 records under top byte FF on 2026-09-24); the error
# names the plugin. The gate bake then runs on a COPY of his profile with exactly those plugins unticked.
mkdir -p "$W/g5live/stock" "$W/g5live/tex" "$W/prof_g5"
lg --mo2-profile "$PROFILE" --worldspace 3C --terrain-region -20 24 -20 24 --dim 4 \\
	--out-dir "$WA/g5live/stock" --tex-dir "$WA/g5live/tex" --native "$WA/g5live" > "$W/g5live.log"; lrc=$?
echo "  his live profile: bake rc=$lrc; $(grep -i -m1 -E 'error|refused' "$W/g5live.log" | cut -c1-200)"
lg --mo2-profile "$PROFILE" --print-source > "$W/g5_src.txt"
cp "$PROFILE/modlist.txt" "$W/prof_g5/"
"$PY" "$CHK" untick "$W/g5_src.txt" "$PROFILE/plugins.txt" "$W/prof_g5/plugins.txt"
mkdir -p "$W/g5/stock" "$W/g5/tex"
t0=$(date +%s)
lg --mo2-profile "$WA/prof_g5" --mo2-mods "$MODS" --data-root "$DATA" --worldspace 3C \\
	--terrain-region -20 24 -20 24 --dim 4 \\
	--out-dir "$WA/g5/stock" --tex-dir "$WA/g5/tex" --native "$WA/g5" > "$W/g5.log"; rc=$?
'''
assert s.count(old) == 1, 'bake'
s = s.replace(old, new)
old2 = '"$PY" "$CHK" record "$W/g5_rec.txt" "$PROFILE" "$MODS" "$DATA" > "$W/g5.chk"'
assert s.count(old2) == 1, 'record'
s = s.replace(old2, '"$PY" "$CHK" record "$W/g5_rec.txt" "$W/prof_g5" "$MODS" "$DATA" > "$W/g5.chk"')
old3 = '"$RUNG" -no-gui lodgen "$DATA/Fallout4.esm" --plugins-txt "$PROFILE/plugins.txt"'
assert s.count(old3) == 1, 'rung'
s = s.replace(old3, '"$RUNG" -no-gui lodgen "$DATA/Fallout4.esm" --plugins-txt "$WA/prof_g5/plugins.txt"')
assert s.count('\r') == cr
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('ok')
