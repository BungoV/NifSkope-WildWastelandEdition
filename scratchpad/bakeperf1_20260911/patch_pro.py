"""BAKEPERF1: the two new translation units into NifSkope.pro, and -lpsapi for
GetProcessMemoryInfo (the bake census line's peak working set).

Backslashes are built from chr(92): a qmake continuation is a backslash, and a
backslash does not survive a shell heredoc (lodgen skill, editing traps)."""
BS = chr(92)
P = 'NifSkope.pro'
s = open(P, encoding='utf-8', newline='').read()

oldh = '\tsrc/lodgen.h ' + BS + '\n'
assert s.count(oldh) == 1, ('headers', s.count(oldh))
s = s.replace(oldh, oldh + '\tsrc/lodgenchunkpass.h ' + BS + '\n\tsrc/lodgenparallel.h ' + BS + '\n')

oldc = '\tsrc/lodgen.cpp \tsrc/lodgenmanager.cpp \tsrc/lodtfile.cpp ' + BS + '\n'
assert s.count(oldc) == 1, ('sources', s.count(oldc))
s = s.replace(oldc, oldc + '\tsrc/lodgenchunkpass.cpp ' + BS + '\n\tsrc/lodgenparallel.cpp ' + BS + '\n')

oldl = '    LIBS += -lopengl32 -lgdi32\n'
assert s.count(oldl) == 1, ('libs', s.count(oldl))
s = s.replace(oldl,
    "    # -lpsapi: GetProcessMemoryInfo, behind the bake census line's peak\n"
    "    # working set (lane BAKEPERF1, 2026-09-11)\n"
    '    LIBS += -lopengl32 -lgdi32 -lpsapi\n')

open(P, 'w', encoding='utf-8', newline='').write(s)
print('pro patched')
