# VTFIX patch 2 -- scratchpad/vtfix_20260909/bake4.sh
# A `cmp -s` on a file that is not there reports DIFFERS, so a run that produced
# nothing (rc=127, a missing DLL) printed exactly what a real difference prints.
# Recorded in MISTAKES.md.
import io, os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "scratchpad/vtfix_20260909/bake4.sh"
s = io.open(P, encoding="utf-8", newline="").read()
assert "\r" not in s

i = s.index('echo "== the two comparisons =="')
j = s.index("echo BAKE4-DONE")
old = s[i:j]
assert old.count("cmp -s") == 2, old

new = (
    'echo "== the two comparisons =="\n'
    "# never let cmp speak for a file that is not there: a missing input reported\n"
    '# as "DIFFERS" is how a failed run looks exactly like a real result\n'
    "pair() {  # pair <label> <a> <b>\n"
    '\tif [ ! -f "$2" ] || [ ! -f "$3" ]; then\n'
    '\t\techo "$1: MISSING INPUT (a=$([ -f "$2" ] && echo y || echo n)'
    ' b=$([ -f "$3" ] && echo y || echo n))"\n'
    "\t\treturn 2\n"
    "\tfi\n"
    '\tif cmp -s "$2" "$3"; then echo "$1: IDENTICAL"; else echo "$1: DIFFERS"; fi\n'
    "}\n"
    'pair cover   "$W/vt_cover/tex/Commonwealth.4.-24.24.DDS" "$W/dir_cover/tex/Commonwealth.4.-24.24.DDS"\n'
    'pair nocover "$W/vt_nc/tex/Commonwealth.4.-24.24.DDS" "$W/dir_nc/tex/Commonwealth.4.-24.24.DDS"\n'
)
out = s[:i] + new + s[j:]
assert out.count("\r") == 0
io.open(P, "w", encoding="utf-8", newline="").write(out)
print("OK", P, len(out.encode("utf-8")), "bytes, LF", out.count("\n"))
