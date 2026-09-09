import sys
p = 'tests/spells/render_shot.sh'
b = open(p, 'rb').read()
cr = b.count(b'\r')
t = b.decode()

subs = [
    # THIS COMMENT WAS WRONG AND IS TAKEN BACK OUT. It said the primary-monitor
    # window was an artefact of the .NET heuristic. It is a real window.
    ("""# EnumWindows, not Get-Process().MainWindowHandle. The .NET property picks ONE
# window per process by a heuristic and gets it wrong during startup: on
# 2026-09-09 it reported a 426x306 window on the primary monitor, once per bake
# run, at a moment when an EnumWindows sweep of the same process over 574
# samples saw exactly one visible window -- the main one, on the second monitor,
# at layered alpha 0. Enumerating is also strictly stronger: it can only see
# MORE windows, so a transient dialog on the primary cannot hide behind the
# heuristic, and each line carries the class and title so a hit is named rather
# than guessed at.""",
     """# EnumWindows, not Get-Process().MainWindowHandle, and the reason is worth
# keeping. The .NET property returns ONE handle per process by a heuristic, so
# it can never support "there was no other window". When it reported a 426x306
# opaque window on the PRIMARY monitor once per run, that was first written off
# as its error -- and it was not: enumerating found the SAME window, in every
# hidden run, and named it (class Qt6111QWindowIcon, the application title, no
# filename). A 25 ms probe put it at t=371 ms, gone by t=1403 ms, replaced by a
# different HWND of class Qt6111QWindowOwnDCIcon at the asked-for place with
# layered alpha 0: Qt's Windows plugin picks the window class by whether the
# surface needs its own DC, so realising the GL container destroys the first
# native window and creates a second -- and the first was created while the
# widget still had Qt's default geometry. The cure is in the NifSkope
# constructor, before any native window can exist; this instrument is what
# proves it. Enumerating is also strictly stronger -- it can only see MORE
# windows -- and each line carries the class and title so a hit is NAMED."""),
]

for old, new in subs:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('anchor %d matches\n' % n)
        sys.exit(1)
    t = t.replace(old, new)

out = t.encode()
assert out.count(b'\r') == cr
open(p, 'wb').write(out)
print('CR', out.count(b'\r'), 'LF', out.count(b'\n'), 'bytes', len(out))
