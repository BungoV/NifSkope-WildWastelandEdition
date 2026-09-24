#!/usr/bin/env python3
"""Lane UI6, the third link -- what the FIRST EXECUTION of three gates found.

  F1 src/animworkspace.cpp  THE GATE FOUND A REAL ONE. Two of the dock's eleven
                            number fields -- Speed and Frame -- still carry Qt's
                            own up/down arrows. wwMakeScrubField only removes
                            them when its `chrome` flag is on (wwnumberfield.cpp
                            :959), and the transport row asks for no chrome to
                            save width, so the two fields were stamped as scrub
                            fields and kept the native buttons anyway. That is
                            half of bungo's ruling unmet: "we have a new
                            standard for those sliders". Application code.
  F2 src/hkxanimuitest.cpp  the DROP path needs the same list settle the load
                            path got (the dock rebuilds on a 50 ms timer).
  F3 src/wateruitest.cpp    group A's restore half measures a widget whose
                            stylesheet has been swapped twice; Qt keeps the
                            sabotage's box until the widget is re-polished, so
                            the third sweep read the arrows 2 px left of where
                            they are drawn in the shipped window.
  F4 two user-facing strings still named the retired dock.
"""


def ed(path, pairs):
    b = open(path, 'rb').read().decode('utf-8')
    cr = b.count('\r')
    for a, t in pairs:
        c = b.count(a)
        assert c == 1, (path, c, a[:90])
        b = b.replace(a, t, 1)
    out = b.encode('utf-8')
    assert out.count(b'\r') == cr
    open(path, 'wb').write(out)
    print('ok %s %d bytes CR %d' % (path, len(out), cr))


# ---- F1: no native spin-box arrows anywhere in the dock
ed('src/animworkspace.cpp', [
("""	wwMakeScrubField( speedBox, WwScrubSpec{ 0.05, WwScrubSpec::No, false } );""",
 """	wwMakeScrubField( speedBox, WwScrubSpec{ 0.05, WwScrubSpec::No, false } );
	/* NO NATIVE SPIN ARROWS (bungo, 2026-09-10 21:1x, over the old dock's two
	 * fields whose steppers were squeezed to a sliver: "Do you see it?", then
	 * "we have a new standard for those sliders, don't you remember?").
	 *
	 * wwMakeScrubField only takes Qt's up/down buttons away when its `chrome`
	 * flag is on (src/ui/widgets/wwnumberfield.cpp:959), and this row asks for
	 * no chrome because the transport is tight. So these two were STAMPED as
	 * scrub fields and kept the native buttons anyway -- which lane UI6's new
	 * stepper gate found on its first run. Drag or type; there is nothing to
	 * squeeze. */
	speedBox->setButtonSymbols( QAbstractSpinBox::NoButtons );"""),
("""	wwMakeScrubField( frameBox, WwScrubSpec{ 0.0, WwScrubSpec::Yes, false } );""",
 """	wwMakeScrubField( frameBox, WwScrubSpec{ 0.0, WwScrubSpec::Yes, false } );
	frameBox->setButtonSymbols( QAbstractSpinBox::NoButtons );	// see Speed above"""),
])

# ---- F2: the drop path settles the list too
ed('src/hkxanimuitest.cpp', [
("""		const bool acc = wwSimulateDrop( skope, { st.clipPath }, &enterOk );
		const int r = wwRowOf( box, clipName );""",
 """		const bool acc = wwSimulateDrop( skope, { st.clipPath }, &enterOk );
		wwSettle( tl );		// the drop rebuilds the list on the same 50 ms timer
		const int r = wwRowOf( box, clipName );"""),
])

# ---- F3: re-polish before the restored sweep
ed('src/wateruitest.cpp', [
("""				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( victimSheets.at( i ) );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int rWorst = 0, rRead = 0;""",
 """				/* PUTTING THE STRING BACK IS NOT PUTTING THE BOX BACK. Qt keeps
				 * the sabotage's computed box until the widget is re-polished,
				 * so the third sweep of a CORRECT window read every arrow two
				 * pixels left of where it is drawn -- and this floor's restore
				 * half went red on the state the picture below shows. Unpolish
				 * and polish each victim, which is what forces the stylesheet's
				 * geometry to be worked out again. */
				for ( int i = 0; i < victims.size(); i++ ) {
					QWidget * w = victims.at( i );
					w->setStyleSheet( victimSheets.at( i ) );
					w->style()->unpolish( w );
					w->style()->polish( w );
					w->updateGeometry();
				}
				for ( int i = 0; i < 6; i++ )
					QApplication::processEvents();
				int rWorst = 0, rRead = 0;"""),
])

# ---- F4: the last two user-facing strings that named the retired dock
ed('src/spells/animationsetup.cpp', [
("""			"Use the Animation Manager to edit them, or its channel copy/paste to clone keys from another lane." ), &dlg ) );""",
 """			"Edit them in the Blocks tab: the Animation Manager that used to edit them was retired on 2026-09-11 (lane UI6) and the Animation dock shows a NIF sequence read-only." ), &dlg ) );"""),
])
ed('src/ui/widgets/physicspanel.cpp', [
("""							"Animation Manager's timeline. Capped at 20 seconds, \"""",
 """							"Animation dock's timeline. Capped at 20 seconds, \""""),
])
