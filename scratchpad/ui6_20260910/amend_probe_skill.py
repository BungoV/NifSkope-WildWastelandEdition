#!/usr/bin/env python3
"""Lane UI6 -- amend ww-qss-geometry-probe with the two traps this lane hit,
in BOTH skill trees (CONSTITUTION 1a: the two trees drift, and a lane's
amendment lands in the repo tree while the director's lands in the live one)."""

SECTION = '''
## 3c. A widget with no background of its own grabs onto WHITE (lane UI6, 2026-09-11)

Sections 3a and 3b both say "grab the widget and read the pixels". On an
**auto-raise QToolButton** that produces a picture with nothing in it.

`QWidget::grab()` allocates a QPixmap and renders the widget into it. A button
that paints no background of its own -- `autoRaise`, or any QSS rule whose
`background` is `transparent` -- leaves whatever the pixmap was filled with,
and Qt fills it **white**. Measured, lane UI6's probe: background `#efefef`
(luminance 239), the theme's glyph colour `#e6e8eb` (luminance 232), **seven
levels apart**. Every ink scan in 3b reported "found nothing" on four buttons
that were drawn perfectly, while the button beside them with a dark label read
fine -- so the failure looks like a per-widget bug rather than an instrument.

Render over a fill you chose instead:

```cpp
QPixmap pm( w, h );
pm.fill( QColor( skin( "bgBar" ) ) );          // the bar's own colour
b->render( &pm, QPoint(), QRegion(), QWidget::DrawChildren );
const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
```

`DrawChildren` without `DrawWindowBackground` keeps the widget's own painting
and does not erase the fill. The same call works in the in-application harness,
so the probe's number and the gate's number stay one number.

**And the same trap in the probe's own fixtures**: an icon built as
`QPixmap pm; QPainter p(&pm); ...; return QIcon(pm);` copies the pixmap while
the painter is still attached to it, and the icon comes out empty. `p.end()`
before the copy. A rig whose fixture is invisible reports the widget as
correct-and-empty, which is indistinguishable from the defect under test.

## 3d. Isolating a SUBCONTROL: two renders, one pinned geometry (lane UI6)

When the thing under test is a subcontrol the style draws -- a
`menu-indicator`, a `down-arrow`, a `::handle` -- no rect says where it went
(3a), and its ink runs into the widget's own ink, so a single scan cannot tell
the two apart. Take **two renders of the same widget** and diff the columns:

1. pin the geometry (`setFixedSize` at the current size), so nothing can move;
2. render once as it stands;
3. take the thing that makes the style draw the subcontrol away --
   `setMenu(nullptr)` for a menu indicator, and its default action's menu too;
4. render again, restore the menu and the size hints;
5. the columns that DIFFER are the subcontrol; the last INK column of the
   second render is the glyph's; the gap between them is the number.

Its own floor comes free: if no column differs, the subcontrol was never drawn
and the read is refused by name rather than reported as a huge gap.

**Pin the whole ROW, not one widget at a time.** Lane UI6's first version
pinned and unpinned each button inside its own measurement, so a toolbar of
thirteen buttons re-laid out thirteen times per sweep; the third sweep of a
CORRECT window read one button a pixel narrow and the gate's restore half went
red. One layout state for the whole sweep: pin every widget, measure them all,
unpin them all.

'''

for root in (r'E:\Projects\NifskopeWildWastelandEdition\.claude\skills',
             r'E:\Projects\Claude\.claude\skills'):
    p = root + r'\ww-qss-geometry-probe\SKILL.md'
    try:
        b = open(p, 'rb').read().decode('utf-8')
    except OSError as e:
        print('SKIPPED %s (%s)' % (p, e))
        continue
    if '## 3c. A widget with no background' in b:
        print('already amended: %s' % p)
        continue
    anchor = '## 3b. When the thing that moved is TEXT'
    if anchor not in b:
        print('ANCHOR MISSING in %s -- not amended' % p)
        continue
    before = len(b.encode('utf-8'))
    b = b.replace(anchor, SECTION.lstrip('\n') + anchor, 1)
    out = b.encode('utf-8')
    open(p, 'wb').write(out)
    print('amended %s (%d -> %d bytes, CR %d)' % (p, before, len(out), out.count(b'\r')))
