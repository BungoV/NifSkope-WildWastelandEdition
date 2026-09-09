			f.span( slotFallbackCheck );
			/* Far-ring proxies. bungo, 2026-09-06: "Proxy meshes for the far
			 * rings. Every engine since 2017 replaces far clusters with one
			 * simplified mesh per cell ... ring 2 and 3 chunks could ship at a
			 * quarter of their triangles with the same textures." It is
			 * geometry, not a channel, so it stays visible under BOTH targets:
			 * the stock engine draws the smaller mesh for the same picture
			 * exactly as FO4CS does. */
			simplifyCheck = new QCheckBox( tr( "Far-ring simplification" ), page );
			simplifyCheck->setObjectName( QStringLiteral( "LodgenSimplifyCheck" ) );
			simplifyCheck->setChecked( settings.value( QStringLiteral( "LodGeneration/simplify" ), true ).toBool() );
			simplifyCheck->setToolTip( tr( "After the shapes merge, each far ring's meshes are decimated to the fraction\n"
				"of their triangles set below. Alpha-tested shapes and impostor cards keep every\n"
				"triangle - a cut-out is a silhouette, not a surface - and ring 0, the one you\n"
				"walk up to, is never touched." ) );
			f.span( simplifyCheck );
			auto ratioField = [this, page, &f, &settings]( QDoubleSpinBox *& field, const char * objName,
				const QString & label, const QString & key, double dflt, const QString & tip ) {
				field = new QDoubleSpinBox( page );
				field->setObjectName( QLatin1String( objName ) );
				field->setRange( 0.05, 1.00 );
				field->setSingleStep( 0.05 );
				field->setDecimals( 2 );
				field->setValue( settings.value( key, dflt ).toDouble() );
				field->setToolTip( tip );
				wwMakeScrubField( field );
				return f.add( page, label, field );
			};
			QLabel * s8Label = ratioField( simplify8Spin, "LodgenSimplify8Spin",
				tr( "Ring 1 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing8" ), 1.00,
				tr( "The fraction of ring 1 (dim 8) triangles that survive. 1.00 leaves the ring\n"
					"alone, which is the default: ring 1 is still close enough to read as geometry." ) );
			QLabel * s16Label = ratioField( simplify16Spin, "LodgenSimplify16Spin",
				tr( "Ring 2 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing16" ), 0.35,
				tr( "The fraction of ring 2 (dim 16) triangles that survive." ) );
			QLabel * s32Label = ratioField( simplify32Spin, "LodgenSimplify32Spin",
				tr( "Ring 3 triangles kept" ), QStringLiteral( "LodGeneration/simplifyRing32" ), 0.20,
				tr( "The fraction of ring 3 (dim 32) triangles that survive." ) );
			simplifyErrorSpin = new QDoubleSpinBox( page );
			simplifyErrorSpin->setObjectName( QStringLiteral( "LodgenSimplifyErrorSpin" ) );
			simplifyErrorSpin->setRange( 1.0, 1024.0 );
			simplifyErrorSpin->setSingleStep( 4.0 );
			simplifyErrorSpin->setDecimals( 1 );
			simplifyErrorSpin->setValue( settings.value( QStringLiteral( "LodGeneration/simplifyError" ), 32.0 ).toDouble() );
			simplifyErrorSpin->setToolTip( tr( "How far a simplified surface may move, in world units at ring 0, scaled by\n"
				"each ring's own size. The decimator stops early rather than exceed it, so the\n"
				"fractions above are targets and this is the rail." ) );
			wwMakeScrubField( simplifyErrorSpin );
			QLabel * sErrLabel = f.add( page, tr( "Simplification error" ), simplifyErrorSpin );
			f.span( atlasCheck );
