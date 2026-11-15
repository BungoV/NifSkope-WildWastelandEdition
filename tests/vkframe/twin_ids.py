"""Twin ids across lane branches: one id per Vulkan object twin (ObjBlDraw.twin), never two twins on one id.

Usage: python -I tests/vkframe/twin_ids.py [--red] REF [REF ...]   (git refs read with `git show`, `.` = the working tree; run from the repo)
Per ref it reads the declarations:
  - vkobjpipe.h `int twin = N; //!< N name, M name ...` comments (ObjBlShade, ObjCellShade)
  - vkobjpipe.h `constexpr int kObjTwin<X> = N; //!< twin N name`
  - cellbindless.cpp `pn == "name.prog" ? N`
  - vkobjpipe.inc `d.twin = N;` literals: a literal id no declaration names is named by its function
The union over the refs must map every id to one name: else FAIL (exit 1).
--red: HEAD's own text with every kObjTwin constant set to 7 (a forced duplicate); it must FAIL.
"""
import re
import subprocess
import sys

H = "src/features/vkframe/vkobjpipe.h"
INC = "src/features/vkframe/vkobjpipe.inc"
BL = "src/features/near/cellbindless.cpp"


def show(ref, path):
	if ref == ".":  # the working tree
		try:
			with open(path, encoding="utf-8", errors="replace") as f:
				return f.read()
		except OSError:
			return ""
	r = subprocess.run(["git", "show", f"{ref}:{path}"], capture_output=True, text=True, encoding="utf-8", errors="replace")
	return r.stdout if r.returncode == 0 else ""


def decls(h, inc, bl):
	out = []  # (id, name, where)
	for m in re.finditer(r"int twin = \d+;\s*//!<([^\n]*)", h):
		for i, n in re.findall(r"\b(\d+) ([A-Za-z_][\w]*)", m.group(1)):
			out.append((int(i), n, "vkobjpipe.h comment"))
	for m in re.finditer(r"constexpr int (kObjTwin\w*) = (\d+);\s*//!< twin (\d+) ([\w]+)", h):
		if m.group(2) != m.group(3):
			out.append((int(m.group(2)), m.group(1) + "<comment says " + m.group(3) + ">", "vkobjpipe.h const"))
		out.append((int(m.group(2)), m.group(4), "vkobjpipe.h " + m.group(1)))
	for n, i in re.findall(r'pn == "([\w]+)\.prog" \? (\d+)', bl):
		out.append((int(i), n, "cellbindless.cpp"))
	named = {i for i, _, _ in out}
	fn = None
	for line in inc.splitlines():
		f = re.match(r"^\w[\w:<>\s\*&]*?\b(\w+)\s*\(", line)
		if f and not line.startswith((" ", "\t")):
			fn = f.group(1)
		m = re.search(r"\bd\.twin = (\d+);", line)
		if m and int(m.group(1)) not in named:
			out.append((int(m.group(1)), fn or "?", "vkobjpipe.inc literal"))
	return out


def main():
	args = sys.argv[1:]
	red = "--red" in args
	refs = [a for a in args if a != "--red"]
	if not refs:
		print("TWIN IDS: no refs")
		return 2
	ids = {}
	for ref in refs:
		h, inc, bl = show(ref, H), show(ref, INC), show(ref, BL)
		if not h:
			print(f"TWIN IDS: {ref}: no {H}")
			return 2
		if red and ref == refs[0]:
			h = re.sub(r"(constexpr int kObjTwin\w* = )\d+(;\s*//!< twin )\d+", r"\g<1>7\g<2>7", h)
		for i, n, w in decls(h, inc, bl):
			ids.setdefault(i, {}).setdefault(n, []).append(f"{ref} {w}")
	bad = 0
	for i in sorted(ids):
		names = ids[i]
		print(f"twin {i}: " + " | ".join(f"{n} ({len(w)})" for n, w in names.items()))
		if len(names) > 1:
			bad += 1
			for n, w in names.items():
				print(f"  DUP twin {i} {n}: " + "; ".join(sorted(set(w))))
	print(f"TWIN IDS {'RED ' if red else ''}{'FAIL' if bad else 'OK'} ids={len(ids)} dups={bad} refs={' '.join(refs)}")
	return 1 if bad else 0


if __name__ == "__main__":
	sys.exit(main())
