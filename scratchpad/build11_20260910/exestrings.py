#!/usr/bin/env python
"""Lane BUILD11: prove the three lanes' code reached the LINKED BINARY, without
launching it (bungo's own window is open, so no exe launch is allowed).

QStringLiteral compiles to UTF-16, so an ASCII grep of the exe returns 0 on an
exe that contains the string twice (build-verify, lane BUILD9). Search both
encodings and say which one hit.
"""
import os

EXE = os.path.join(r"E:\Projects\NifskopeWildWastelandEdition", "release", "NifSkope.exe")
b = open(EXE, "rb").read()
print("exe %d bytes" % len(b))

WANT = [
    ("AnimWorkspaceDock", "HKXEDIT2: the new dock's objectName"),
    ("AnimWsRate", "HKXEDIT2: the dope sheet's rate widget"),
    ("WW_ANIMWS_TEST", "HKXEDIT2: the harness arming variable"),
    ("WW_HKXMODEL_TEST", "HKXEDIT1: the harness arming variable"),
    ("hkclasses_fo4.json", "HKXEDIT1: the class database beside the exe"),
    ("no class database found", "HKXEDIT1: HkxModel's refusal in words"),
    ("WW_SKELOVERLAY_TEST", "SKELFIX/SKELOVERLAY: the overlay harness"),
    ("Show Skeleton", "SKELFIX: the overlay action"),
    ("hkx_annotation_vocabulary", "HKXEDIT2: the vocabulary beside the exe"),
]

bad = 0
for s, why in WANT:
    a = b.count(s.encode("ascii"))
    u = b.count(s.encode("utf-16-le"))
    hit = "yes" if (a or u) else "NO"
    if not (a or u):
        bad += 1
    print("  %-4s %-28s ascii=%-3d utf16=%-3d  %s" % (hit, s, a, u, why))
print("EXESTRINGS missing=%d" % bad)
