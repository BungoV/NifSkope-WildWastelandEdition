"""FARRING1 step 4: src/lodgenmanager.cpp -- the Far-ring simplification rows,
their settings, the atlas format off the Target, and the pass in the run."""

P = 'src/lodgenmanager.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'lodgenmanager.cpp must be LF-only'
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:240])


# ---- 1. members ------------------------------------------------------------
A = ("\tQSpinBox * aoSkirtSpin = nullptr, * cullMarginSpin = nullptr;\n"
     "\tQCheckBox * swayCheck = nullptr, * channelsCheck = nullptr, * cullCheck = nullptr, * slotFallbackCheck = nullptr;\n")
once(s, A)
s = s.replace(A, A +
    "\tQCheckBox * simplifyCheck = nullptr;\n"
    "\tQDoubleSpinBox * simplify8Spin = nullptr, * simplify16Spin = nullptr,\n"
    "\t\t* simplify32Spin = nullptr, * simplifyErrorSpin = nullptr;\n")

# ---- 2. the rows -----------------------------------------------------------
B = ("\t\t\tf.span( slotFallbackCheck );\n"
     "\t\t\tf.span( atlasCheck );\n")
once(s, B)
ROWS = open('scratchpad/snip_panel_rows.cpp', encoding='utf-8').read()
assert ROWS.count('\r') == 0
_UNUSED = """\t\t\tf.span( slotFallbackCheck );
\t\t\t/* Far-ring proxies. bungo, 2026-09-06: "Proxy meshes for the far rings.
\t\t\t * Every engine since 2017 replaces far clusters with one simplified mesh
\t\t\t * per cell ... ring 2 and 3 chunks could ship at a quarter of their
\t\t\t * triangles with the same textures." It is geometry, not a channel, so
\t\t\t * it stays visible under BOTH targets: the stock engine draws the
\t\t\t * smaller mesh for the same picture exactly as FO4CS does. */
\t\t\tsimplifyCheck = new QCheckBox( tr( "Far-ring simplification" ), page );
\t\t\tsimplifyCheck->setObjectName( QStringLiteral( "LodgenSimplifyCheck" ) );
\t\t\tsimplifyCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/simplify" ), true ).toBool() );
\t\t\tsimplifyCheck->setToolTip( tr( "After the shapes merge, each far ring's meshes are decimated to the fraction\\n"
\\t\\t\\t\\t"of their triangles set below. Alpha-tested shapes and impostor cards keep every\\n"
\\t\\t\\t\\t"triangle - a cut-out is a silhouette, not a surface - and ring 0, the one you\\n"
\\t\\t\\t\\t"walk up to, is never touched." ) );
\t\t\tf.span( simplifyCheck );
\t\t\tauto ratioField = [this, page, &f]( QDoubleSpinBox *& field, const char * objName,
\t\t\t\tconst QString & label, const QString & key, double dflt, const QString & tip ) {
\t\t\t\tfield = new QDoubleSpinBox( page );
\t\t\t\tfield->setObjectName( QLatin1String( objName ) );
\t\t\t\tfield->setRange( 0.05, 1.00 );
\t\t\t\tfield->setSingleStep( 0.05 );
\t\t\t\tfield->setDecimals( 2 );
\t\t\t\tfield->setValue( settings.value( key, dflt ).toDouble() );
\t\t\t\tfield->setToolTip( tip );
\t\t\t\twwMakeScrubField( field );
\t\t\t\treturn f.add( page, label, field );
\t\t\t};
\t\t\tQLabel * s8Label = ratioField( simplify8Spin, "LodgenSimplify8Spin",
\t\t\t\ttr( "Ring 1 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing8" ), 1.00,
\t\t\t\ttr( "The fraction of ring 1 (dim 8) triangles that survive. 1.00 leaves the ring\\n"
\\t\\t\\t\\t\\t"alone, which is the default: ring 1 is still close enough to read as geometry." ) );
\t\t\tQLabel * s16Label = ratioField( simplify16Spin, "LodgenSimplify16Spin",
\t\t\t\ttr( "Ring 2 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing16" ), 0.35,
\t\t\t\ttr( "The fraction of ring 2 (dim 16) triangles that survive." ) );
\t\t\tQLabel * s32Label = ratioField( simplify32Spin, "LodgenSimplify32Spin",
\t\t\t\ttr( "Ring 3 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing32" ), 0.20,
\t\t\t\ttr( "The fraction of ring 3 (dim 32) triangles that survive." ) );
\t\t\tsimplifyErrorSpin = new QDoubleSpinBox( page );
\t\t\tsimplifyErrorSpin->setObjectName( QStringLiteral( "LodgenSimplifyErrorSpin" ) );
\t\t\tsimplifyErrorSpin->setRange( 1.0, 1024.0 );
\t\t\tsimplifyErrorSpin->setSingleStep( 4.0 );
\t\t\tsimplifyErrorSpin->setDecimals( 1 );
\t\t\tsimplifyErrorSpin->setValue( settings.value( QStringLiteral( "LodGeneration/simplifyError" ), 32.0 ).toDouble() );
\t\t\tsimplifyErrorSpin->setToolTip( tr( "How far a simplified surface may move, in world units at ring 0, scaled by each\\n"
\\t\\t\\t\\t"ring's own size. The decimator stops early rather than exceed it, so the\\n"
\\t\\t\\t\\t"fractions above are targets and this is the rail." ) );
\t\t\twwMakeScrubField( simplifyErrorSpin );
\t\t\tQLabel * sErrLabel = f.add( page, tr( "Simplification error" ), simplifyErrorSpin );
\t\t\tf.span( atlasCheck );
"""
s = s.replace(B, ROWS)

# ---- 3. the sub-widget list and the greying --------------------------------
C = ("\t\t\tobjectsSub = { identityCheck, swayCheck, channelsCheck, arraysCheck, aoCheck, aoSkirtSpin, aoSkirtLabel, cullCheck,\n"
     "\t\t\t\tcullMarginSpin, cullMarginLabel, slotFallbackCheck, atlasCheck, impostorHost, impostorLabel,\n"
     "\t\t\t\timpostorLevelBox, impostorLevelLabel };\n"
     "\t\t\tauto sync = [this, aoSkirtLabel, cullMarginLabel]() {\n")
once(s, C)
s = s.replace(C,
    "\t\t\tobjectsSub = { identityCheck, swayCheck, channelsCheck, arraysCheck, aoCheck, aoSkirtSpin, aoSkirtLabel, cullCheck,\n"
    "\t\t\t\tcullMarginSpin, cullMarginLabel, slotFallbackCheck, atlasCheck, impostorHost, impostorLabel,\n"
    "\t\t\t\timpostorLevelBox, impostorLevelLabel, simplifyCheck, simplify8Spin, s8Label,\n"
    "\t\t\t\tsimplify16Spin, s16Label, simplify32Spin, s32Label, simplifyErrorSpin, sErrLabel };\n"
    "\t\t\tauto sync = [this, aoSkirtLabel, cullMarginLabel, s8Label, s16Label, s32Label, sErrLabel]() {\n")

D = ("\t\t\t\tcullMarginSpin->setEnabled( on && cullCheck->isChecked() );\n"
     "\t\t\t\tcullMarginLabel->setEnabled( on && cullCheck->isChecked() );\n"
     "\t\t\t};\n")
once(s, D)
s = s.replace(D,
    "\t\t\t\tcullMarginSpin->setEnabled( on && cullCheck->isChecked() );\n"
    "\t\t\t\tcullMarginLabel->setEnabled( on && cullCheck->isChecked() );\n"
    "\t\t\t\tconst bool simp = on && simplifyCheck->isChecked();\n"
    "\t\t\t\tfor ( QWidget * w : { (QWidget *) simplify8Spin, (QWidget *) s8Label,\n"
    "\t\t\t\t\t\t(QWidget *) simplify16Spin, (QWidget *) s16Label,\n"
    "\t\t\t\t\t\t(QWidget *) simplify32Spin, (QWidget *) s32Label,\n"
    "\t\t\t\t\t\t(QWidget *) simplifyErrorSpin, (QWidget *) sErrLabel } )\n"
    "\t\t\t\t\tw->setEnabled( simp );\n"
    "\t\t\t};\n")

E = ("\t\t\tconnect( cullCheck, &QCheckBox::toggled, this, sync );\n"
     "\t\t\tsync();\n")
once(s, E)
s = s.replace(E,
    "\t\t\tconnect( cullCheck, &QCheckBox::toggled, this, sync );\n"
    "\t\t\tconnect( simplifyCheck, &QCheckBox::toggled, this, sync );\n"
    "\t\t\tsync();\n")

# ---- 4. settings -----------------------------------------------------------
F = "\t\ts.setValue( QStringLiteral( \"LodGeneration/slotFallback\" ), slotFallbackCheck->isChecked() );\n"
once(s, F)
s = s.replace(F, F +
    "\t\ts.setValue( QStringLiteral( \"LodGeneration/simplify\" ), simplifyCheck->isChecked() );\n"
    "\t\ts.setValue( QStringLiteral( \"LodGeneration/simplifyRing8\" ), simplify8Spin->value() );\n"
    "\t\ts.setValue( QStringLiteral( \"LodGeneration/simplifyRing16\" ), simplify16Spin->value() );\n"
    "\t\ts.setValue( QStringLiteral( \"LodGeneration/simplifyRing32\" ), simplify32Spin->value() );\n"
    "\t\ts.setValue( QStringLiteral( \"LodGeneration/simplifyError\" ), simplifyErrorSpin->value() );\n")

# ---- 5. the atlas format follows the Target --------------------------------
G = ("\t\t\t\tif ( lodgenBuildAtlas( writtenBto, QString(),\n"
     "\t\t\t\t\tatlasDir + \"/\" + ws + QStringLiteral( \".LodgenObjects\" ),\n"
     "\t\t\t\t\tQString( \"data\\\\Textures\\\\Terrain\\\\%1\\\\Objects\\\\%1.LodgenObjects\" ).arg( ws ),\n"
     "\t\t\t\t\toutputDir(), &aerr ) )\n"
     "\t\t\t\t\ttail = tr( \", atlas written\" );\n")
once(s, G)
s = s.replace(G,
    "\t\t\t\t/* Vanilla's own sheet is DXT1 (measured), so the stock target gets\n"
    "\t\t\t\t * BC1 with one-bit alpha for the cut-outs - parity and half the\n"
    "\t\t\t\t * memory - and FO4CS keeps BC3's eight-bit alpha, which only a\n"
    "\t\t\t\t * consumer that soft-blends card edges can spend. */\n"
    "\t\t\t\tconst bool atlasBc1 = !fo4cs();\n"
    "\t\t\t\tif ( lodgenBuildAtlas( writtenBto, QString(),\n"
    "\t\t\t\t\tatlasDir + \"/\" + ws + QStringLiteral( \".LodgenObjects\" ),\n"
    "\t\t\t\t\tQString( \"data\\\\Textures\\\\Terrain\\\\%1\\\\Objects\\\\%1.LodgenObjects\" ).arg( ws ),\n"
    "\t\t\t\t\toutputDir(), atlasBc1, &aerr ) )\n"
    "\t\t\t\t\ttail = tr( \", atlas %1\" ).arg( atlasBc1 ? tr( \"written (BC1)\" ) : tr( \"written (BC3)\" ) );\n")

# ---- 6. the pass, after the merge ------------------------------------------
H = ("\t\t\t\tif ( lodgenMergeChunkShapes( writtenBto, &rep, &merr ) )\n"
     "\t\t\t\t\ttail += tr( \", merged %1\" ).arg( rep );\n"
     "\t\t\t\telse\n"
     "\t\t\t\t\ttail += tr( \", merge: %1\" ).arg( merr );\n"
     "\t\t\t}\n")
once(s, H)
s = s.replace(H, H +
    "\t\t\tif ( !cancelFlag && !writtenBto.isEmpty() && objectsCheck->isChecked()\n"
    "\t\t\t\t&& simplifyCheck->isChecked() ) {\n"
    "\t\t\t\t// the far rings, on the MERGED shapes: one proxy per cluster\n"
    "\t\t\t\tprogress->setFormat( tr( \"simplifying the far rings\\u2026\" ) );\n"
    "\t\t\t\tQCoreApplication::processEvents();\n"
    "\t\t\t\tLodgenSimplifyOptions sopts;\n"
    "\t\t\t\tsopts.ratio8 = float( simplify8Spin->value() );\n"
    "\t\t\t\tsopts.ratio16 = float( simplify16Spin->value() );\n"
    "\t\t\t\tsopts.ratio32 = float( simplify32Spin->value() );\n"
    "\t\t\t\tsopts.errorWorld = float( simplifyErrorSpin->value() );\n"
    "\t\t\t\tQString rep2, serr;\n"
    "\t\t\t\tif ( lodgenSimplifyFarRings( writtenBto, sopts, &rep2, &serr ) )\n"
    "\t\t\t\t\ttail += tr( \", far rings: %1\" ).arg( rep2 );\n"
    "\t\t\t\telse\n"
    "\t\t\t\t\ttail += tr( \", far rings: %1\" ).arg( serr );\n"
    "\t\t\t}\n")

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('lodgenmanager.cpp: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
