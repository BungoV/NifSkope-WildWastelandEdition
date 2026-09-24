"""Deliverable stamps for the lane report: sha256 (16), bytes, CR count, mtime.
Every text file must read CR 0 (src/, tests/, res/, scratchpad/ are LF-only)."""
import hashlib, os, time
R = r"E:\Projects\NifskopeWildWastelandEdition"
files = [
    "src/hkxclipedit.h", "src/hkxclipedit.cpp", "src/animdopesheet.h", "src/animdopesheet.cpp",
    "src/animworkspace.h", "src/animworkspace.cpp", "src/animworkspacetest.cpp",
    "src/hkxplayback.h", "src/hkxplayback.cpp",
    "tests/hkxclipedit_gate.cpp", "tests/spells/animws.sh", "res/hkx_annotation_vocabulary.txt",
    "sx_HKXEDIT2.sh",
    "scratchpad/hkxedit2_20260910/hookup.py", "scratchpad/hkxedit2_20260910/build_gate.sh",
    "scratchpad/hkxedit2_20260910/annot_vocab.py", "scratchpad/hkxedit2_20260910/PENDING.md",
    "scratchpad/hkxedit2_20260910/CHANGE_NEEDED.md", "scratchpad/hkxedit2_20260910/WW_CHANGES_ENTRY.md",
    "scratchpad/hkxedit2_20260910/MISTAKES_ENTRIES.md", "scratchpad/lane_hkxedit2_report.md",
    "release/hkxclipedit_gate.exe",
]
print("| file | sha256 (16) | bytes | CR | mtime |")
print("|---|---|---|---|---|")
for f in files:
    p = os.path.join(R, f)
    if not os.path.exists(p):
        print("| `%s` | MISSING | | | |" % f)
        continue
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    print("| `%s` | `%s` | %s | %d | %s |" % (f, hashlib.sha256(b).hexdigest()[:16], format(len(b), ","), cr,
          time.strftime("%H:%M:%S", time.localtime(os.path.getmtime(p)))))
