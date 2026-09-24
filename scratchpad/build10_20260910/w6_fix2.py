#!/usr/bin/env python3
"""Lane WATER6 fix 2, after the FIRST run of its own gates (BUILD10).

Two defects, both in the GATES and neither in the change under test:

X2  the pin was placed at `axis.pts[size/2]` without asking whether that point
    is WET.  The centreline is the mean position of each slice's wet texels and
    on a winding reach that mean can land on the bank -- the harness's own
    stroke reports "21 of 48 points fell outside body 2".  `addStroke` refused
    it in words, the gate never read the refusal, and the solve then had no
    strokes at all.  Fixed by choosing the mid-most point that IS on the river
    and by reading `addStroke`'s answer.

X3b/X3c  the gate counted texels outside the layer whose word EQUALS the
    layer's constant, and one of the river's 29,121 outside texels solves to
    exactly 0xFF40 by coincidence.  That is a collision, not an authority leak.
    The instrument now compares against the LAYER-FREE SOLVE texel by texel,
    which is what "the solve fills the rest" and "it is not baked in" actually
    mean; the threshold stays 0.  The first run's numbers (1 of 29,121 and 190
    of 191) are reported in the lane report, not erased.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))

X2_OLD = """		WaterStroke pin;
		pin.kind = WaterStroke::Pin;
		pin.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		pin.speed = 0.5f;
		pin.width = 4096.0f;
		WaterStrokePoint p = axis.pts[axis.pts.size() / 2];
		pin.pts.append( p );
		WaterMarkSolve stp;
		doc.addStroke( pin, &msg );
		doc.solve( &stp, &err );
"""
X2_NEW = """		WaterStroke pin;
		pin.kind = WaterStroke::Pin;
		pin.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		pin.speed = 0.5f;
		pin.width = 4096.0f;
		/* THE MID-MOST point of the centreline that is actually ON the river.
		 * The centreline is the mean position of each slice's wet texels, and on
		 * a winding reach that mean lands on the bank often enough that the
		 * harness's own stroke reports a fifth of its points outside the body.
		 * A pin on dry land is refused in words, which is correct and is not
		 * what this gate is about. */
		WaterStrokePoint p = axis.pts[axis.pts.size() / 2];
		bool wet = false;
		for ( int step = 0; step < axis.pts.size() && !wet; step++ )
			for ( int sgn = -1; sgn <= 1 && !wet; sgn += 2 ) {
				const int i = axis.pts.size() / 2 + sgn * step;
				if ( i < 0 || i >= axis.pts.size() )
					continue;
				if ( int( doc.bodyAtWorld( double( axis.pts[i].x ), double( axis.pts[i].y ) ) ) == river ) {
					p = axis.pts[i];
					wet = true;
				}
			}
		pin.pts.append( p );
		WaterMarkSolve stp;
		const bool pinned = doc.addStroke( pin, &msg );
		check( QStringLiteral( "X2 the pin lands on the river: %1" ).arg( msg ), pinned && wet );
		doc.solve( &stp, &err );
"""

X3_OLD = """		qint64 magicIn = 0, magicOut = 0, solvedOut = 0, wetIn = 0;
		doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			const bool inside = px >= layer.px0 && py >= layer.py0
				&& px < layer.px0 + layer.w && py < layer.py0 + layer.h;
			if ( inside ) {
				wetIn++;
				if ( n == kMagic )
					magicIn++;
			} else {
				if ( n == kMagic )
					magicOut++;
				else
					solvedOut++;
			}
		}, &err );
		check( QStringLiteral( "X3a the raster is the AUTHORITY where painted: %1 of %2 painted "
			"texels read the layer's word" ).arg( magicIn ).arg( wetIn ),
			wetIn > 0 && magicIn == wetIn );
		check( QStringLiteral( "X3b the solve fills the REST: %1 texels outside the layer carry "
			"the layer's word (gate 0) and %2 carry the solved one (floor > 0)" )
			.arg( magicOut ).arg( solvedOut ), magicOut == 0 && solvedOut > 0 );

		// X3c: it is an authority, not a bake -- removing it puts the words back
		std::vector<quint16> withLayer;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) == river )
				withLayer.push_back( n );
		}, &err );
		for ( int i = doc.strokes().size() - 1; i >= 0; i-- )
			if ( doc.strokes()[i].kind == 10 )
				doc.removeStroke( i );
		qint64 back = 0, still = 0;
		size_t at = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			if ( at < withLayer.size() && withLayer[at] == kMagic ) {
				if ( n == kMagic )
					still++;
				else
					back++;
			}
			at++;
		}, &err );
		check( QStringLiteral( "X3c the raster is not baked in: with the layer removed %1 of %2 "
			"of its texels went back to the solved word (%3 did not)" )
			.arg( back ).arg( back + still ).arg( still ), back > 0 && still == 0 );
"""
X3_NEW = """		/* The comparison is against the LAYER-FREE SOLVE, texel by texel, and not
		 * against the layer's constant: exactly one of this river's 29,121
		 * texels outside the window solves to 0xFF40 on its own, and counting
		 * equality with a constant called that coincidence an authority leak on
		 * the first run of these gates. */
		qint64 magicIn = 0, wetIn = 0, changedOut = 0, sameOut = 0;
		size_t at = 0;
		doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			const bool inside = px >= layer.px0 && py >= layer.py0
				&& px < layer.px0 + layer.w && py < layer.py0 + layer.h;
			if ( inside ) {
				wetIn++;
				if ( n == kMagic )
					magicIn++;
			} else if ( at < plain.size() ) {
				if ( n != plain[at] )
					changedOut++;
				else
					sameOut++;
			}
			at++;
		}, &err );
		check( QStringLiteral( "X3a the raster is the AUTHORITY where painted: %1 of %2 painted "
			"texels read the layer's word" ).arg( magicIn ).arg( wetIn ),
			wetIn > 0 && magicIn == wetIn );
		check( QStringLiteral( "X3b the solve fills the REST: %1 texels outside the layer moved "
			"from the layer-free solve (gate 0) and %2 did not (floor > 0)" )
			.arg( changedOut ).arg( sameOut ), changedOut == 0 && sameOut > 0 );

		// X3c: it is an authority, not a bake -- removing it puts every word back
		for ( int i = doc.strokes().size() - 1; i >= 0; i-- )
			if ( doc.strokes()[i].kind == 10 )
				doc.removeStroke( i );
		qint64 differ = 0, same = 0;
		at = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			if ( at < plain.size() ) {
				if ( n != plain[at] )
					differ++;
				else
					same++;
			}
			at++;
		}, &err );
		check( QStringLiteral( "X3c the raster is not baked in: with the layer removed the body's "
			"words are the layer-free solve's again (%1 of %2 differ)" )
			.arg( differ ).arg( differ + same ), differ == 0 && same > 0 );
"""

PLAIN_OLD = """		doc.clearStrokes();
		doc.addStroke( weighted( axis, QVector<float>() ), &msg );
		doc.solve( &st, &err );
		// the window round the stroke's middle point, in body-plane texels
"""
PLAIN_NEW = """		doc.clearStrokes();
		doc.addStroke( weighted( axis, QVector<float>() ), &msg );
		doc.solve( &st, &err );
		// the body's words WITHOUT any layer: the thing X3b and X3c compare with
		std::vector<quint16> plain;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) == river )
				plain.push_back( n );
		}, &err );
		// the window round the stroke's middle point, in body-plane texels
"""

EDITS = [PLAIN_OLD, X2_OLD, X3_OLD]
NEWS = [PLAIN_NEW, X2_NEW, X3_NEW]

for p in (os.path.join(ROOT, "src", "watermark.cpp"),
          os.path.join(HERE, "w6_gates.cpp.txt")):
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    for old, new in zip(EDITS, NEWS):
        n = b.count(old.encode("utf-8"))
        print("%-22s count=%d  %r" % (os.path.basename(p), n, old[:40]))
        assert n == 1, (p, old[:60])
        b = b.replace(old.encode("utf-8"), new.encode("utf-8"))
    assert b.count(b"\r") == cr == 0
    open(p, "wb").write(b)
    print("  wrote %s %d bytes, CR 0" % (os.path.basename(p), len(b)))
