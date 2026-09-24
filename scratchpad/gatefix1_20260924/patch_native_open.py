"""GATEFIX1: run native_open.sh's window shots in a scratch settings scope.

(d) inherited bungo's persisted QSettings (render options, game paths): the same
.BTR fixture rendered its WATER shape pure white under his profile and dark under
the defaults, so NCC read -0.21 on every exe back to before_btofree1 while the
same shots under WW_SETTINGS_SCOPE read 0.84. The harness now owns its settings.
"""
import sys

P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/native_open.sh'
with open(P, 'rb') as fh:
	src = fh.read()
cr0 = src.count(b'\r')


def rep(s, old, new):
	n = s.count(old)
	assert n == 1, (n, old[:60])
	return s.replace(old, new)


src = rep(src, b'''PORT="${PORT:-42931}"
WS="${WS:-Commonwealth}"
''', b'''PORT="${PORT:-42931}"
WS="${WS:-Commonwealth}"
# Every window this harness opens runs in its OWN settings scope
# (src/harnesswindow.cpp, WW_SETTINGS_SCOPE): the whole QSettings tree moves to
# HKCU\\Software\\NifTools\\NifSkope 2.0 <scope>, wiped before and after, so a
# picture depends on the fixtures and the exe and never on the profile of the
# person who last used the viewer. Measured 2026-09-24 (lane GATEFIX1): the
# (d) .BTR drew its water shape pure white under bungo's persisted profile and
# dark under the defaults, and NCC read -0.21 against 0.84 with the SAME exe
# and the SAME files -- on every exe back to before_btofree1, which passed
# 17/0 on 2026-09-16. A gate that reads the user's profile measures the profile.
SCOPE="${SCOPE:-nativeopen}"
REGKEY="HKCU\\\\Software\\\\NifTools\\\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1; }
''')

src = rep(src, b'''W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
''', b'''W="$(mktemp -d)"
wipe_scope
trap 'rm -rf "$W"; wipe_scope' EXIT
''')

src = rep(src, b'''	env "$@" \\
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \\''',
b'''	env "$@" WW_SETTINGS_SCOPE="$SCOPE" \\
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \\''')

assert src.count(b'\r') == cr0
with open(P + '.new', 'wb') as fh:
	fh.write(src)
print('patched; CR', cr0)
