"""LIGHTANGLES1 fix: the load reads the key family the save writes, and wraps like rotateLight.
Refuses unless the anchor occurs exactly once; asserts the file stays LF-only."""
import sys

path = 'src/ui/widgets/lightingwidget.cpp'
old = (b'\ttmp = settings.value( "Lighting/Declination", 0 ).toInt();\n'
       b'\togl->declination = float( tmp % int(POS) ) * ( 180.0f / float(POS) );\n'
       b'\ttmp = settings.value( "Lighting/Planar Angle", 0 ).toInt();\n'
       b'\togl->planarAngle = float( tmp % int(POS) ) * ( 180.0f / float(POS) );\n')
new = (b'\t/* The light\'s two angles, stored by saveSettings() as quarter-degree\n'
       b'\t * integers (POS = 180 degrees) under Settings/Render/Lighting/, and read\n'
       b'\t * back from the SAME keys. The load used to read "Lighting/Declination" and\n'
       b'\t * "Lighting/Planar Angle", which nothing writes, so the angles never came\n'
       b'\t * back after a restart (lane LIGHTANGLES1, 2026-09-24). It also folded the\n'
       b'\t * value with % POS, turning a legal +-180 degrees (+-POS stored, the light\n'
       b'\t * from the opposite side) into 0. The wrap is now GLView::rotateLight\'s own,\n'
       b'\t * so the loaded range is exactly the range the view can hold: [-180, 180],\n'
       b'\t * both ends kept (roundFloat rounds half to even).\n'
       b'\t */\n'
       b'\tauto loadAngle = [&settings]( const char * key ) {\n'
       b'\t\tconst float a = float( settings.value( key, 0 ).toInt() ) * ( 180.0f / float(POS) );\n'
       b'\t\treturn a - float( roundFloat( a / 360.0f ) ) * 360.0f;\n'
       b'\t};\n'
       b'\togl->declination = loadAngle( "Settings/Render/Lighting/Declination" );\n'
       b'\togl->planarAngle = loadAngle( "Settings/Render/Lighting/Planar Angle" );\n')

b = open(path, 'rb').read()
if new in b:
    print('already applied'); sys.exit(0)
n = b.count(old)
if n != 1:
    sys.exit(f'anchor count {n} != 1')
b2 = b.replace(old, new)
assert b2.count(b'\r') == 0 == b.count(b'\r')
open(path, 'wb').write(b2)
print(f'applied, {len(b2) - len(b):+d} B, CR 0')
