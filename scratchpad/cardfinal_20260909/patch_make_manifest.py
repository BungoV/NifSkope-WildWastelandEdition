#!/usr/bin/env python3
"""Stop the sample-set MANIFEST's PROSE from being typed.

`make_manifest.py` reads every SIZE from disk and says so, but its opening
paragraph and its command block carried a typed exe timestamp ("19:35:14") and a
typed card-library path, both of which went stale the first time the sample set
was regenerated -- and the correction was then hand-spliced into MANIFEST.md,
which the file itself tells you not to do.

After this: the exe timestamp is read from the exe, the card library is an
argument (`python make_manifest.py <cards dir>`), and the lane line is one
constant at the top of the script rather than a sentence buried in the prose.
"""
import sys

P = 'scratchpad/handoff_fo4cs/samples/make_manifest.py'
s = open(P, encoding='utf-8').read()


def rep(old, new):
    global s
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


rep("""HERE = os.path.dirname(os.path.abspath(__file__))""",
    """HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
EXE = os.path.join(REPO, 'release', 'NifSkope.exe')

# WHO MADE THIS SET, and off which card library. Both used to be typed into the
# prose below and both went stale the first time the set was regenerated; the
# exe's timestamp is now read from the exe, and the card library is argv[1].
LANE = 'lane CARDFINAL, 2026-09-09'
CARDS = (sys.argv[1] if len(sys.argv) > 1
         else 'scratchpad/cardfinal_20260909/cards_perframe')""")

rep("""import datetime
import os
import struct""",
    """import datetime
import os
import struct
import sys""")

rep("""    w('Written by lane IMAGES5, 2026-09-09, on `release/NifSkope.exe` **19:35:14**')
    w('(the build carrying lane RENAME\\'s FINAL FILE NAMES and lane OFFSCREEN2\\'s')
    w('invisible headless window). **No build happened.** `Fallout4.exe` and any')
    w('other `NifSkope.exe` were checked absent before the run; the gate is inside')
    w('`make_samples.sh`.\\n')""",
    """    exemt = datetime.datetime.fromtimestamp(os.path.getmtime(EXE)).strftime('%Y-%m-%d %H:%M:%S')
    w('Written by %s, on `release/NifSkope.exe` **%s** (timestamp read from the'
      % (LANE, exemt))
    w('exe, not typed). `Fallout4.exe` and any other `NifSkope.exe` were checked')
    w('absent before the run; the gate is inside `make_samples.sh`.\\n')
    w('The card sets here carry **`card.gap`** (the distance in texels between two')
    w('neighbouring silhouettes across a frame border), **`card.pad`** (half of it,')
    w('the margin on each side), **`card.mips = log2(gap)`** -- so no shipped mip')
    w('lets a border tap pick up any of the neighbouring frame -- and')
    w('**`card.frameOffset`**, two numbers per frame, which is where that frame\\'s')
    w('quad sits relative to `card.center` (`docs/LODGEN_CARD_SHEETS.md` 3.6 and')
    w('`docs/LODGEN_LODM_FORMAT.md` 3.1). A `cardArray` layer carries the same')
    w('`frameOffset`. `--card-half-aux` was NOT used: all four sheets of every set')
    w('are full size, which is the 2026-09-06 default.\\n')""")

rep("""    w('`E:/Tools/Fallout 4/DataUnpacked/Data`, `CARDS` =')
    w('`<repo>/scratchpad/images_20260909/gen/cards_trees19` (the 19-tree octahedral')
    w('library this lane baked), `S` = this directory. Every path absolute.\\n')""",
    """    w('`E:/Tools/Fallout 4/DataUnpacked/Data`, `CARDS` =')
    w('`<repo>/%s` (the 19-tree octahedral library' % CARDS)
    w('this set stands on), `S` = this directory. Every path absolute.\\n')""")

open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
