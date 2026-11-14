"""Lane VKFAR3 (2026-11-14): cut the far ground's upload code out of cellfarground.cpp, the rung's and this tree's, for
fargroundoff.cpp. Text is taken as it stands in each file (no retyped copy): the constants, struct Gpu / Doc and the
functions by name, in source order. The rung has no uploadInputs: its pass() lines from `uploadBase( fn, d );` to
`uploadDecals( fn, d );` are wrapped into one, verbatim. python -I fargroundoff_prep.py <rung.cpp> <head.cpp> <out dir>
"""
import os
import re
import sys

FUNCS = ["delTex", "vkKeyOf", "delShared", "delIn", "sharedIn", "mipLevels", "freeDecals", "freeKit", "freePools",
         "freeAll", "tex2D", "mipBytes", "uploadBase", "uploadAtlas", "uploadDecals", "reshare", "uploadInputs"]
RUNG_NEED = ["delTex", "freeDecals", "freeKit", "freePools", "freeAll", "tex2D", "mipBytes", "uploadBase", "uploadDecals"]


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8").replace("\r\n", "\n").split("\n")


def block_end(lines, i, end):
    for j in range(i, len(lines)):
        if lines[j] == end:
            return j
    raise SystemExit("no end %r after line %d" % (end, i + 1))


def cut(lines, rung):
    first_fn = next(i for i, l in enumerate(lines) if l.startswith("void delTex("))
    parts = []  # (line index, text)
    for i, l in enumerate(lines[:first_fn + 40]):
        if l.startswith("constexpr ") or re.match(r"^(char|bool) g_vk", l):
            parts.append((i, l))
    for name in ("Gpu", "Doc"):
        i = lines.index("struct %s" % name)
        parts.append((i, "\n".join(lines[i:block_end(lines, i, "};") + 1])))
    found = []
    for name in FUNCS:
        pat = re.compile(r"^[A-Za-z][\w:<> ]*[ *&]%s\( " % name)
        hits = [i for i, l in enumerate(lines) if pat.match(l) and not l.rstrip().endswith(";")]
        if not hits:
            continue
        if len(hits) > 1:
            raise SystemExit("%s: %d definitions" % (name, len(hits)))
        i = hits[0]
        parts.append((i, "\n".join(lines[i:block_end(lines, i, "}") + 1])))
        found.append(name)
    if rung:
        missing = [n for n in RUNG_NEED if n not in found]
        if missing:
            raise SystemExit("rung lacks " + ", ".join(missing))
        p = next(i for i, l in enumerate(lines) if l.startswith("void pass( Scene * scene )"))
        a = next(i for i in range(p, len(lines)) if lines[i] == "\tuploadBase( fn, d );")
        b = next(i for i in range(a, len(lines)) if lines[i].strip() == "uploadDecals( fn, d );")
        if b - a != 4:
            raise SystemExit("rung pass block is %d lines, not the 5 expected" % (b - a + 1))
        body = "\n".join(lines[a:b + 1])
        parts.append((len(lines), "void uploadInputs( NifSkopeOpenGLContext::GLFunctions * fn, Doc & d, bool lookup, bool, bool )\n"
                      "{\n\tconst int want = lookup ? 1 : 2;\n" + body + "\n\t}\n}"))
        found.append("uploadInputs(pass)")
    elif "uploadInputs" not in found:
        raise SystemExit("head lacks uploadInputs")
    parts.sort(key=lambda p: p[0])
    return "\n\n".join(t for _, t in parts) + "\n", found


def main():
    rung, head, out = sys.argv[1], sys.argv[2], sys.argv[3]
    for src, name, is_rung in ((rung, "rung_tu.inc", True), (head, "head_tu.inc", False)):
        text, found = cut(read(src), is_rung)
        with open(os.path.join(out, name), "w", newline="\n") as f:
            f.write(text)
        print("FARGROUNDOFF prep %s: %d functions (%s), %d lines" % (name, len(found), " ".join(found), text.count("\n")))


main()
