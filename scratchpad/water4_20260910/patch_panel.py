# -*- coding: utf-8 -*-
"""patch_panel.py -- src/watermarkpanel.cpp: the Dye pin tool, the Dye colour
and Dye fade rows (Marking), the Dye at mouth tick (Selected body), the Dye
plane in Show, and the canvas paint.  Rows only, through the house helpers."""
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

P = 'src/watermarkpanel.cpp'
splice(P, [
    # ---- Show: the dye plane ----
    ('\t\tshowBox->addItem( tr( "Shore distance" ), 2 );\n',
     '\t\tshowBox->addItem( tr( "Dye" ), 3 );\n', 'after'),
    # ---- Tool: the dye pin ----
    ('\t\ttoolBox->addItem( tr( "Outlet pin" ), int( WaterStroke::OutletPin ) );\n',
     '\t\ttoolBox->addItem( tr( "Dye pin" ), int( WaterStroke::DyePin ) );\n', 'after'),
    ('\t\t\t"is the path between them.\\n"\n\t\t\t"Erase: click a stroke to remove it." ) );\n',
     '\t\t\t"is the path between them.\\n"\n'
     '\t\t\t"Dye pin: click where something enters the water; its colour is carried\\n"\n'
     '\t\t\t"downstream and fades over the Dye fade distance.\\n"\n'
     '\t\t\t"Erase: click a stroke to remove it." ) );\n', 'replace'),
    # ---- Marking: Dye colour and Dye fade rows, after Width ----
    ('\t\twwMakeScrubField( widthSpin );\n\t\tmk.add( page, tr( "Width" ), widthSpin );\n\t\tlayout->addLayout( mk.g );\n',
     '''\t\twwMakeScrubField( widthSpin );
\t\tmk.add( page, tr( "Width" ), widthSpin );

\t\tdyeColourButton = new QPushButton( tr( "Choose" ), page );
\t\tdyeColourButton->setObjectName( QStringLiteral( "WaterMarkDyeColourButton" ) );
\t\tdyeColourButton->setToolTip( tr( "The colour a Dye pin releases into the water." ) );
\t\tmk.add( page, tr( "Dye colour" ), dyeColourButton );

\t\tdyeFadeSpin = new QDoubleSpinBox( page );
\t\tdyeFadeSpin->setObjectName( QStringLiteral( "WaterMarkDyeFadeSpin" ) );
\t\tdyeFadeSpin->setRange( 256.0, 262144.0 );
\t\tdyeFadeSpin->setDecimals( 0 );
\t\tdyeFadeSpin->setSingleStep( 512.0 );
\t\tdyeFadeSpin->setValue( WaterMarkDoc::kDyeHalfDistanceDefault );
\t\tdyeFadeSpin->setToolTip( tr( "How far along the flow a dye travels before half of it is gone,\\n"
\t\t\t"in world units; one cell is 4096. The same distance serves every dye\\n"
\t\t\t"in the file, and it is stored in the file." ) );
\t\twwMakeScrubField( dyeFadeSpin );
\t\tmk.add( page, tr( "Dye fade" ), dyeFadeSpin );
\t\tlayout->addLayout( mk.g );
''', 'replace'),
    # ---- Selected body: the Dye at mouth tick, after Still water ----
    ('\t\tsel.add( page, tr( "Flow" ), stillCheck );\n',
     '''\t\tsel.add( page, tr( "Flow" ), stillCheck );

\t\tdyeMouthCheck = new QCheckBox( tr( "Dye at mouth" ), page );
\t\tdyeMouthCheck->setObjectName( QStringLiteral( "WaterMarkDyeMouthCheck" ) );
\t\tdyeMouthCheck->setToolTip( tr( "This body's water keeps its own colour past its mouth, as a\\n"
\t\t\t"plume into the body it drains into that fades over the Dye fade\\n"
\t\t\t"distance. A river into the sea, or a creek into a lake." ) );
\t\tsel.add( page, tr( "Dye" ), dyeMouthCheck );
''', 'replace'),
    # ---- the wiring ----
    ('\t\tconnect( colourCheck, &QCheckBox::toggled, this, [this]( bool on ) {\n',
     '''\t\tconnect( dyeMouthCheck, &QCheckBox::toggled, this, [this]( bool on ) {
\t\t\tif ( doc && selected && !loadingBody ) {
\t\t\t\tdoc->setBodyDyeMouth( selected, on, 1.0f );
\t\t\t\tsolve();
\t\t\t}
\t\t\trefreshSummary();
\t\t} );
\t\tconnect( dyeColourButton, &QPushButton::clicked, this, [this]() {
\t\t\tconst QColor c = QColorDialog::getColor( dyePick, this, tr( "Dye colour" ) );
\t\t\tif ( c.isValid() )
\t\t\t\tdyePick = c;
\t\t} );
\t\tconnect( dyeFadeSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
\t\t\tif ( doc && !loadingBody ) {
\t\t\t\tdoc->setDyeHalfDistance( dyeFadeSpin->value() );
\t\t\t\tsolve();
\t\t\t}
\t\t} );
''', 'before'),
    # ---- accessors ----
    ('\tdouble strokeWidth() const { return widthSpin->value(); }\n',
     '\tQColor dyeColour() const { return dyePick; }\n', 'after'),
    # ---- openFile: the knob out of the file ----
    ('\t\tcanvas->setDoc( doc );\n\t\tselected = 0;\n\t\tsolve();\n\t\tselectBody( 0 );\n\t\treturn true;\n',
     '\t\tcanvas->setDoc( doc );\n\t\tselected = 0;\n'
     '\t\tloadingBody = true;\n\t\tdyeFadeSpin->setValue( doc->dyeHalfDistance() );\n\t\tloadingBody = false;\n'
     '\t\tsolve();\n\t\tselectBody( 0 );\n\t\treturn true;\n', 'replace'),
    # ---- selectBody: the tick out of the store ----
    ('\t\t\tstillCheck->setChecked( doc->bodyLockZero( id ) );\n',
     '\t\t\tstillCheck->setChecked( doc->bodyLockZero( id ) );\n'
     '\t\t\tdyeMouthCheck->setChecked( doc->bodyDyeMouth( id ) );\n', 'replace'),
    # ---- refreshSummary: enable with a selection ----
    ('\t\tstillCheck->setEnabled( doc && selected );\n',
     '\t\tstillCheck->setEnabled( doc && selected );\n\t\tdyeMouthCheck->setEnabled( doc && selected );\n'
     '\t\tdyeFadeSpin->setEnabled( doc != nullptr );\n', 'replace'),
    # ---- members ----
    ('\tQCheckBox * stillCheck = nullptr;\n',
     '\tQCheckBox * stillCheck = nullptr;\n\tQCheckBox * dyeMouthCheck = nullptr;\n'
     '\tQPushButton * dyeColourButton = nullptr;\n\tQDoubleSpinBox * dyeFadeSpin = nullptr;\n'
     '\tQColor dyePick = QColor( 110, 170, 40 );   //!< a sludge green, until chosen\n', 'replace'),
    # ---- the canvas: paint the dye plane ----
    ('\t\t\t} else if ( plane == 2 ) {\n\t\t\t\tint px = 0, py = 0;\n\t\t\t\tdoc->worldToTexel( wx, wy, px, py );\n\t\t\t\tconst int s = id ? int( doc->file()->shoreAt( px, py ) ) : 255;\n',
     '''\t\t\t} else if ( plane == 3 ) {
\t\t\t\t/* the dye plane: the source's colour -- a body's hash, or the pin's
\t\t\t\t * own -- blended by its weight over the body's dark base */
\t\t\t\tint px = 0, py = 0;
\t\t\t\tdoc->worldToTexel( wx, wy, px, py );
\t\t\t\tconst quint32 d = id ? doc->dyeWordAt( px, py ) : 0;
\t\t\t\tQColor base = id ? QColor( 34, 48, 70 ) : QColor( 24, 26, 30 );
\t\t\t\tif ( d ) {
\t\t\t\t\tconst quint32 src = d & 0xFFFF;
\t\t\t\t\tconst double wgt = double( ( d >> 16 ) & 0xFF ) / 255.0;
\t\t\t\t\tQColor dye = bodyColour( quint16( src & 0x7FFF ) );
\t\t\t\t\tif ( src & 0x8000 ) {
\t\t\t\t\t\tint k = 0;
\t\t\t\t\t\tfor ( const WaterStroke & s : doc->strokes() )
\t\t\t\t\t\t\tif ( s.kind == WaterStroke::DyePin && s.enabled() && k++ == int( src & 0x7FFF ) )
\t\t\t\t\t\t\t\tdye = QColor( s.colour[0], s.colour[1], s.colour[2] );
\t\t\t\t\t}
\t\t\t\t\tbase = QColor( int( base.red() + ( dye.red() - base.red() ) * wgt ),
\t\t\t\t\t\tint( base.green() + ( dye.green() - base.green() ) * wgt ),
\t\t\t\t\t\tint( base.blue() + ( dye.blue() - base.blue() ) * wgt ) );
\t\t\t\t}
\t\t\t\tc = base;
\t\t\t} else if ( plane == 2 ) {
\t\t\t\tint px = 0, py = 0;
\t\t\t\tdoc->worldToTexel( wx, wy, px, py );
\t\t\t\tconst int s = id ? int( doc->file()->shoreAt( px, py ) ) : 255;
''', 'replace'),
    # ---- the canvas: a dye pin is drawn in its own colour ----
    ('\t\telse if ( s.kind == WaterStroke::ZeroFlow )\n\t\t\tcol = QColor( wwSkinColor( "textMuted" ) );\n',
     '\t\telse if ( s.kind == WaterStroke::ZeroFlow || s.kind == WaterStroke::DyeMouth )\n'
     '\t\t\tcol = QColor( wwSkinColor( "textMuted" ) );\n'
     '\t\telse if ( s.kind == WaterStroke::DyePin )\n'
     '\t\t\tcol = QColor( s.colour[0], s.colour[1], s.colour[2] );\n'
     '\t\telse if ( s.kind == WaterStroke::DyeKnob )\n'
     '\t\t\tcontinue;\n', 'replace'),
    ('\t\tdrawPoly( pts, col, s.kind != WaterStroke::ZeroFlow );\n',
     '\t\tdrawPoly( pts, col, s.kind != WaterStroke::ZeroFlow && s.kind != WaterStroke::DyeMouth );\n', 'replace'),
    # ---- mouse release: a dye pin is one point with a colour ----
    ('\tif ( tool == WaterStroke::SourcePin || tool == WaterStroke::OutletPin ) {\n\t\tWaterStrokePoint p;\n\t\tp.x = float( current.first().x() );\n\t\tp.y = float( current.first().y() );\n\t\ts.pts << p;\n\t} else {\n',
     '''\tif ( tool == WaterStroke::SourcePin || tool == WaterStroke::OutletPin
\t\t|| tool == WaterStroke::DyePin ) {
\t\tWaterStrokePoint p;
\t\tp.x = float( current.first().x() );
\t\tp.y = float( current.first().y() );
\t\ts.pts << p;
\t\tif ( tool == WaterStroke::DyePin ) {
\t\t\t// strength 1 at the pin; the Width row is its radius
\t\t\ts.speed = 1.0f;
\t\t\tconst QColor c = panel->dyeColour();
\t\t\ts.colour[0] = quint8( c.red() );
\t\t\ts.colour[1] = quint8( c.green() );
\t\t\ts.colour[2] = quint8( c.blue() );
\t\t\ts.colour[3] = 255;
\t\t}
\t} else {
''', 'replace'),
])
