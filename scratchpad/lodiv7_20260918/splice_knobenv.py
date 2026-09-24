p = 'src/nativeemit.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("""	//! Spatial-hash cell, world units. 33k instances makes the O(n^2) pair walk impossible.
	float gridCell = 1024.0f;
};
//! The one instance. Every knob turned here, nowhere else.
static const GroupKnobs KNOB;
""",
    """	//! Spatial-hash cell, world units. 33k instances makes the O(n^2) pair walk impossible.
	float gridCell = 1024.0f;

	/*! The knobs are also readable from the environment, and that is a
	 *  MEASURING surface, not a feature: the rule is a proposal, so the table
	 *  of what each knob does to a chunk has to come from bakes of the SHIPPED
	 *  code rather than from a re-implementation of it that could be wrong in
	 *  its own way. Nothing set here changes a default -- an unset variable
	 *  leaves the value above exactly as written. */
	GroupKnobs()
	{
		const QByteArray c = qgetenv( "WW_LODI_GROUP_COMPONENT" );
		if ( !c.isEmpty() ) {
			static QByteArray held;
			held = c;
			archComponent = held.constData();
		}
		bool okv = false;
		const float t = qgetenv( "WW_LODI_GROUP_TOLERANCE" ).toFloat( &okv );
		if ( okv )
			touchTolerance = t;
		const QByteArray b = qgetenv( "WW_LODI_GROUP_SHAPE" );
		if ( b == "sphere" )
			useBox = false;
		else if ( b == "box" )
			useBox = true;
		const float g = qgetenv( "WW_LODI_GROUP_GRID" ).toFloat( &okv );
		if ( okv && g > 0.0f )
			gridCell = g;
	}
};
//! The one instance. Every knob turned here, nowhere else.
static const GroupKnobs KNOB;
""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('knob env overrides spliced')
