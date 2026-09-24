"""CSM1: hook the cascade caster pass into GLView (src/glview.cpp is mixed CRLF/LF; splice CRLF lines)."""
p = r"E:\Projects\NifskopeWildWastelandEdition\src\glview.cpp"
with open(p, "rb") as f:
    d = f.read()
cr0 = d.count(b"\r")

a1 = b'#include "gl/lookdevstage.h"\r\n'
assert d.count(a1) == 1, "include anchor"
d = d.replace(a1, a1 + b'#include "gl/sunshadow.h"\r\n')

a2 = b"\tif ( perspectiveMode ) {\r\n\t\t// Lookdev: the lookdev cube"
assert d.count(a2) == 1, "pass anchor"
ins = (b"\t/* Lookdev (lane CSM1): the three sun-shadow cascade maps, rendered before\r\n"
       b"\t * anything else this frame so the ground and the PBR shapes can receive.\r\n"
       b"\t * A no-op with the Shadows row off (the default): no map, no program swap. */\r\n"
       b"\tif ( wwLookdevActive() )\r\n"
       b"\t\twwSunShadowPass( scene );\r\n"
       b"\r\n")
d = d.replace(a2, ins + a2)

with open(p + ".tmp", "wb") as f:
    f.write(d)
import os
os.replace(p + ".tmp", p)
with open(p, "rb") as f:
    d2 = f.read()
print("CR", cr0, "->", d2.count(b"\r"), "added", d2.count(b"\r") - cr0)
