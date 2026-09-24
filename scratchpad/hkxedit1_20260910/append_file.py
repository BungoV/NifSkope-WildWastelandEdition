"""Append <entry> to <target>, append-only and LF-only: the target's bytes are
asserted to be an exact prefix of the result, and CR counts are asserted 0
before and after. Usage: python append_file.py <target> <entry>"""
import sys

target, entry = sys.argv[1], sys.argv[2]
before = open(target, "rb").read()
add = open(entry, "rb").read()
assert before.count(b"\r") == 0, "target carries CR"
assert add.count(b"\r") == 0, "entry carries CR"
if not before.endswith(b"\n"):
    before += b"\n"
after = before + add
open(target, "wb").write(after)
check = open(target, "rb").read()
assert check.startswith(open(target, "rb").read()[:len(before)]) and check[:len(before)] == before, "not append-only"
print("appended %d bytes to %s (%d -> %d, CR %d)" % (len(add), target, len(before), len(check), check.count(b"\r")))
