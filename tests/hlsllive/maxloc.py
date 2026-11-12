"""prints GL_MAX_UNIFORM_LOCATIONS (0x826E) and GL_MAX_VERTEX_UNIFORM_COMPONENTS of the driver in the hidden 4.1 context"""
import ctypes, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glctx
glctx.context()
for name, e in (('GL_MAX_UNIFORM_LOCATIONS', 0x826E), ('GL_MAX_TEXTURE_IMAGE_UNITS', 0x8872)):
    v = ctypes.c_int(0)
    glctx.gl.glGetIntegerv(e, ctypes.byref(v))
    print(name, v.value)
