"""Whole-packfile reassembly across the mesh tree: decode a file, rebuild it, diff.

The per-shape checks in rt_corpus prove individual objects. This proves the file
they sit in -- the class-name table and its order, where every object lands, all
three fixup tables and their orderings, and the section headers. Nothing short of
a byte comparison against the original tests any of that.

Reports three outcomes, which mean different things:

  byte-exact      the rebuild is indistinguishable from what Havok wrote
  leaf-only       differs only INSIDE a shape or mass-properties object, i.e. in
                  bytes an encoder derives rather than stores -- the assembly is
                  still proven right
  structural      differs anywhere else: a real fault

A clean refusal ("not assembled") is counted apart. It is not a wrong answer.
"""
import os
import re
import subprocess
import sys
from collections import Counter

EXE = os.environ.get("NIFSKOPE_EXE",
	os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
		os.path.abspath(__file__)))), "release", "NifSkope.exe"))
ROOT = os.environ.get("FO4_MESHES", os.path.join(
	"E:" + os.sep, "Tools", "Fallout 4", "DataUnpacked", "Data", "meshes"))
WANT = int(sys.argv[1]) if len(sys.argv) > 1 else 500

PACK = re.compile(r"^packfile\s+(\d+)\s+byte-exact (\d+) / (\d+)", re.M)
LEAF = re.compile(r"^\s+\+(\d+) differing only inside leaf", re.M)
STRUCT = re.compile(r"^\s+first structural difference", re.M)
SKIP = re.compile(r"not assembled: (.*)$", re.M)
DERIV = re.compile(r"re-derived\): byte-exact (\d+) / (\d+)")


def main():
	files = []
	for dp, _, fn in os.walk(ROOT):
		for f in fn:
			if f.lower().endswith(".nif"):
				files.append(os.path.join(dp, f))
	files.sort()
	stride = max(1, len(files) // WANT)
	sample = files[::stride]

	agg = Counter()
	reasons = Counter()
	bad = []
	for i, nif in enumerate(sample):
		if i and i % 100 == 0:
			print("  ... %d/%d" % (i, len(sample)))
		try:
			r = subprocess.run([EXE, "-no-gui", "collision", nif, "--roundtrip"],
							   capture_output=True, text=True, timeout=180).stdout
		except subprocess.TimeoutExpired:
			agg["timeouts"] += 1
			continue
		m = PACK.search(r)
		if not m:
			for s in SKIP.findall(r):
				reasons[s.strip()] += 1
				agg["refused"] += 1
			continue
		total, exact = int(m.group(3)), int(m.group(2))
		agg["assembled"] += total
		agg["byte-exact"] += exact
		leaf = LEAF.search(r)
		agg["leaf-only"] += int(leaf.group(1)) if leaf else 0
		d = DERIV.search(r)
		if d:
			agg["from-model exact"] += int(d.group(1))
			agg["from-model total"] += int(d.group(2))
		for s in SKIP.findall(r):
			reasons[s.strip()] += 1
			agg["refused"] += 1
		if STRUCT.search(r):
			agg["STRUCTURAL"] += 1
			bad.append((nif, [l for l in r.splitlines()
							  if "structural" in l or "differing bytes" in l]))
		agg["files with a packfile"] += 1

	print("\nsampled %d of %d nif files (stride %d)\n" % (len(sample), len(files), stride))
	for k in ("files with a packfile", "assembled", "byte-exact", "leaf-only",
			  "STRUCTURAL", "refused", "timeouts", "from-model exact", "from-model total"):
		if agg[k] or k in ("assembled", "byte-exact", "STRUCTURAL"):
			print("  %-22s %d" % (k, agg[k]))
	if reasons:
		print("\nrefusals:")
		for k, v in reasons.most_common():
			print("  x%-4d %s" % (v, k))
	if bad:
		print("\nSTRUCTURAL FAILURES:")
		for nif, lines in bad[:10]:
			print("  %s" % nif)
			for l in lines:
				print("   %s" % l.strip())
	else:
		print("\nno structural failures")


if __name__ == "__main__":
	main()
