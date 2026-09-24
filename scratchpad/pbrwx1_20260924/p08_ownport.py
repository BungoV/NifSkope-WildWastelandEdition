# every PBR gate's "harness still running" guard looks at OUR port only: another lane's harness
# (a different --port) is not ours and must not refuse our shots
import sys
root = r"E:\Projects\NifskopeWildWastelandEdition\tests\spells"
old = b"CommandLine -match '--port' }"
new = b"CommandLine -match '--port $PORT' }"
for f in ["pbr_shade_ab", "pbr_r1_gates", "pbr_r2a_gates", "pbr_r2b_gates", "pbr_r3_gates", "pbr_r4_gates"]:
    p = root + "\\" + f + ".sh"
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    assert b.count(old) == 1, f
    b = b.replace(old, new)
    assert b.count(b"\r") == cr
    open(p, "wb").write(b)
    print("patched", f)
