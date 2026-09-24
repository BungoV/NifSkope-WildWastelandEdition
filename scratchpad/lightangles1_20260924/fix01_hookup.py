"""LIGHTANGLES1 hook-up: one harness call in nifskope_ui.cpp, one SOURCES line in NifSkope.pro.
Refuses unless each anchor occurs exactly once; asserts CR counts unchanged (both LF-only)."""
import sys

def patch(path, anchor, new):
    b = open(path, 'rb').read()
    cr0 = b.count(b'\r')
    if new in b:
        print(f'{path}: already applied'); return
    n = b.count(anchor)
    if n != 1:
        sys.exit(f'{path}: anchor count {n} != 1')
    b2 = b.replace(anchor, new)
    assert b2.count(b'\r') == cr0, 'CR count moved'
    open(path, 'wb').write(b2)
    print(f'{path}: applied (+{len(b2)-len(b)} B, CR {cr0})')

patch('src/nifskope_ui.cpp',
      b'\t\textern void wwHkxModelHarness( NifSkope * );\t// (lane HKXEDIT1) WW_HKXMODEL_TEST\n'
      b'\t\twwHkxModelHarness( skope );\n\t}\n',
      b'\t\textern void wwHkxModelHarness( NifSkope * );\t// (lane HKXEDIT1) WW_HKXMODEL_TEST\n'
      b'\t\twwHkxModelHarness( skope );\n\t}\n'
      b'\t{\n'
      b'\t\t// lane LIGHTANGLES1: WW_LIGHTANGLES_TEST, src/lightanglestest.cpp\n'
      b'\t\textern void wwLightAnglesHarness( NifSkope * );\n'
      b'\t\twwLightAnglesHarness( skope );\n'
      b'\t}\n')

patch('NifSkope.pro',
      b'\tsrc/impostorpreviewtest.cpp \\\n',
      b'\tsrc/impostorpreviewtest.cpp \\\n\tsrc/lightanglestest.cpp \\\n')
