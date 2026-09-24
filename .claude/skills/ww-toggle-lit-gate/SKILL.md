---
name: ww-toggle-lit-gate
description: Give a mode toggle in the NifSkope Wild Wasteland UI a LIT state and gate it by the pixels rather than by the code -- a QIcon On/Off pair in the palette's brightest ink, the mean ink measured off the icon and off the harness's own 2:1 grab, a floor on a button that has no lit state, and the check that finds out who really owns a shared QAction's icon. Use whenever a button says "this mode is on" and whenever a report is about to claim that it lights up.
---

# WW: a toggle that lights up, and the gate that proves it

## The icon

One function states the rule for every mode toggle, so the lit ink lives in one
place:

```cpp
QIcon wwTransportToggleIcon( int glyph, int px = 16 )
{
	QIcon ic;
	ic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( "text" ) ) ),        QIcon::Normal,   QIcon::Off );
	ic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( "textBright" ) ) ),  QIcon::Normal,   QIcon::On  );
	ic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( "textMuted" ) ) ),   QIcon::Disabled, QIcon::Off );
	ic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( "text" ) ) ),        QIcon::Disabled, QIcon::On  );
	return ic;
}
```

* Qt picks the `On` pixmap because `QStyle` sets `State_On` for a checked
  `QToolButton` and for a checked `QAction` in a menu. Nothing else is needed.
* **ASK WHICH INK, do not assume the accent.** The obvious choice is the
  palette's accent -- it is even named "selection accent, active toggles" --
  and in this tree it was overruled the same day it shipped (bungo, 2026-09-12
  07:3x, over the before/after picture: "Why do these turn yellow when
  selected? the buttons, keep them white"). The lit ink here is `textBright`,
  the brightest ink token, and the unlit one is `text`; Disabled/On is `text`,
  brighter than the `textMuted` of a disabled OFF toggle so it does not read as
  OFF, dimmer than the enabled white so it still reads as disabled.
* **A small ink step is fine when the PLATE carries the state.** `text` and
  `textBright` are 33 apart in summed RGB out of 765. The signal the eye reads
  is the QSS `:checked` background (`bgBtnDown`); the ink only has to stop
  contradicting it. Size the gate's thresholds to the tokens you chose -- never
  leave a floor sized for a colour you no longer use.
* **A button that already changes its glyph does not also light.** Play swaps to
  the pause bars; two signals for one state is one more than the button needs.

## WHO OWNS THE ICON -- ask before you promise a lit state

A `QToolButton` with `setDefaultAction( a )` shows `a`'s icon, and any other code
that touches `a` wins. In this tree the render toolbar re-skins `aAnimLoop` on
every refresh of its popup with a single-state icon, so a lit icon put on that
action from the dock is silently replaced. Before claiming a button lights:

```bash
grep -n "aTheAction->setIcon\|->setIcon( *tlMakeIcon" src/*.cpp
```

If anything else sets that action's icon, the lit state is not yours to give,
and making it light means changing THAT surface -- a separate decision.

## The gate: measure the ink, not the call

In the harness, mean the icon's own pixels per state and compare:

```cpp
auto inkMean = []( const QIcon & ic, const QSize & sz, QIcon::State s ) {
	const QImage im = ic.pixmap( sz, QIcon::Normal, s ).toImage()
	                    .convertToFormat( QImage::Format_ARGB32 );
	qint64 r = 0, g = 0, b = 0, n = 0;
	for ( int y = 0; y < im.height(); y++ )
		for ( int x = 0; x < im.width(); x++ ) {
			const QColor c = im.pixelColor( x, y );
			if ( c.alpha() > 128 ) { r += c.red(); g += c.green(); b += c.blue(); n++; }
		}
	return n ? QColor( int( r / n ), int( g / n ), int( b / n ) ) : QColor();
};
auto dist = []( const QColor & a, const QColor & b ) {
	return qAbs( a.red() - b.red() ) + qAbs( a.green() - b.green() ) + qAbs( a.blue() - b.blue() );
};
```

Four checks and a floor:

1. the button is a drawing with no text (`toolButtonStyle() == IconOnly`, icon not null);
2. its `iconSize()` equals the reference button's (Play), so the set stays one size;
3. `dist( off, on ) >= floor` -- it actually moves. The floor is a fraction of
   the distance between the two tokens (20 for a 33-apart pair), NOT a constant
   carried over from whatever colour the toggle used to light in;
4. both means are pinned to the token they are meant to be -- within 2 ON EVERY
   CHANNEL, `max(|dr|,|dg|,|db|) <= 2`, never a summed distance. The mean is
   taken over pixels whose alpha is merely over half, and un-premultiplying one
   of those costs up to 1 per channel, so a glyph that is nearly all antialiased
   edge (a ring, a thin arrow) reads 3 away from its own token in summed RGB
   while a solid one reads 0. Pinning both ends is what lets check 3's floor be
   small; pinning them per channel is what lets the tolerance stay at 2;
5. **FLOOR, on the same run**: the same arithmetic on a button with no lit state
   (Stop) must report `moved == 0`. Without it, checks 3 and 4 are only measuring
   that a `QIcon` exists.

A toggle whose lit state is NOT yours to give gets a `say()` line with both inks
and its checked state -- measured, never asserted. "It must not light" is the
wrong thing to own the day someone decides it should.

## And then the picture, because the gate cannot see the button

The icon can be right while the BUTTON is wrong (a stylesheet's `:checked`
background, a text label still drawn beside it, an icon the window never asks
for). Grab the row at 2:1 with both switches off and again with both on, put the
switches back to the state you found them in, and measure the ink inside each
checked button's box off the PNG:

```python
blue = (abs(a - bgBtnDown).sum(axis=2) < 40)      # the :checked background
ink  = (abs(sub - bgBtnDown).sum(axis=2) > 60)    # what is drawn on it
print(sub[ink].mean(axis=0))                      # the lit glyph's ink, over the blue plate
```

That measurement is what caught a lit state that the gate had passed and the
report had already claimed (2026-09-12, lane UINOTES2): the icon carried the
lit pixmap, the button never asked for it.

## The other thing the same picture settles

A WORD among drawings sits on the font's baseline while a drawing is centred in
the button, so a text button's ink lands lower than every glyph beside it -- 9
pixels at 2:1, 4.5 logical px, in the transport row. If someone says a control
looks "not centered", find every lump of ink in the row (column runs separated by
>= 6 blank columns), print each bounding box's y centre, and let the numbers say
which control is the odd one out. It is usually not the one they pointed at.
