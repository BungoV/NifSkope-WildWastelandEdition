						/* ===== LANE LODUI1: THE PANEL ACTUALLY RUNS (WW_LODGEN_RUN=1) =====
						 *
						 * The structural checks above read widgets. These two drive the
						 * panel the way a person does -- set the rows, press Generate,
						 * wait for it -- because two of bungo's asks cannot be answered
						 * by reading a widget:
						 *
						 *   * the native row must WIRE the emitter, not merely exist:
						 *     the `.lodo`/`.lodi` pair has to be on disk after a
						 *     GUI-driven region run;
						 *   * the four stage times have to be WRITTEN and to MOVE. Run 1
						 *     is meshes-and-textures with no landscape file, run 2 is the
						 *     shadow heightmap and nothing else, so each of the two
						 *     stages is seen at zero in one run and above zero in the
						 *     other. That pairing is the floor.
						 *
						 * It is opt-in because it writes files and takes seconds, and
						 * `lod_generation.sh` says in its own header that it is structure
						 * only. `tests/spells/lodgen_panel_run.sh` is the spell for it.
						 *
						 * THE PANEL'S SETTINGS ARE PUT BACK. Pressing Generate calls
						 * saveSettings(), which writes the whole LodGeneration group --
						 * so the group is snapshotted here and restored at the end,
						 * keys that did not exist removed. A harness forces the state it
						 * measures and leaves nobody's panel rearranged. */
						if ( qEnvironmentVariableIntValue( "WW_LODGEN_RUN" ) == 1 ) {
							QMap<QString, QVariant> savedGroup;
							{
								QSettings s;
								s.beginGroup( QStringLiteral( "LodGeneration" ) );
								for ( const QString & k : s.allKeys() )
									savedGroup.insert( k, s.value( k ) );
								s.endGroup();
							}
							auto * gen2 = findChild<QPushButton *>( QStringLiteral( "LodgenGenerateButton" ) );
							auto * cancel2 = findChild<QPushButton *>( QStringLiteral( "LodgenCancelButton" ) );
							auto * resL2 = findChild<QLabel *>( QStringLiteral( "LodgenResultLabel" ) );
							auto * target2 = findChild<QComboBox *>( QStringLiteral( "LodgenTargetBox" ) );
							auto * ws2 = findChild<QComboBox *>( QStringLiteral( "LodgenWorldspaceBox" ) );
							auto * out2 = findChild<QLineEdit *>( QStringLiteral( "LodgenOutputEdit" ) );
							auto * nat2 = findChild<QCheckBox *>( QStringLiteral( "LodgenNativeCheck" ) );
							auto * lodt2 = findChild<QCheckBox *>( QStringLiteral( "LodgenLodtCheck" ) );
							auto * hm2 = findChild<QCheckBox *>( QStringLiteral( "LodgenHeightmapCheck" ) );
							auto * hmSize2 = findChild<QComboBox *>( QStringLiteral( "LodgenHeightmapSizeBox" ) );
							auto * vt2 = findChild<QCheckBox *>( QStringLiteral( "LodgenVtCheck" ) );
							auto * w2 = findChild<QSpinBox *>( QStringLiteral( "LodgenWestSpin" ) );
							auto * e2 = findChild<QSpinBox *>( QStringLiteral( "LodgenEastSpin" ) );
							auto * s2 = findChild<QSpinBox *>( QStringLiteral( "LodgenSouthSpin" ) );
							auto * n2 = findChild<QSpinBox *>( QStringLiteral( "LodgenNorthSpin" ) );
							const QString runOut = QDir::tempPath() + QStringLiteral( "/Lodgen_LODUI1_run" );
							/* A clean folder, so "the pair exists" cannot be answered by
							 * a pair some earlier run left behind. */
							QDir( runOut ).removeRecursively();
							QDir().mkpath( runOut );
							// the chunk-size selector: index 0 is every ring, 1 is dim 4
							QComboBox * dimBox2 = nullptr;
							if ( panel )
								for ( QComboBox * c : panel->findChildren<QComboBox *>() )
									if ( c->count() == 5 && c->itemText( 1 ) == QLatin1String( "4" ) )
										dimBox2 = c;
							/* Pump the event loop until the run is over. The chunk loop
							 * is driven by QTimer::singleShot( 0 ), so processEvents()
							 * is what advances it; Cancel is enabled for exactly as long
							 * as a run is live, which is the only public "still running"
							 * this panel has. */
							auto waitForRun = [&]( int budgetMs ) {
								QElapsedTimer t;
								t.start();
								while ( cancel2 && cancel2->isEnabled() && t.elapsed() < budgetMs ) {
									QApplication::processEvents( QEventLoop::AllEvents, 20 );
									QThread::msleep( 5 );
								}
								return t.elapsed();
							};
							auto stageOf = []( const QString & line, const QString & name ) {
								// "landscape 1.2 s, meshes 3.4 s, ..." -> the number after `name`
								const int i = line.indexOf( name );
								if ( i < 0 )
									return -1.0;
								return line.mid( i + name.size() ).trimmed()
									.section( QLatin1Char( ' ' ), 0, 0 ).toDouble();
							};
							if ( gen2 && cancel2 && resL2 && target2 && ws2 && out2 && nat2 && lodt2
								&& hm2 && hmSize2 && vt2 && w2 && e2 && s2 && n2 && dimBox2 ) {
								// ---- RUN 1: one object chunk, the native pair, no landscape ----
								target2->setCurrentIndex( 0 );		// FO4 Community Shaders
								QApplication::processEvents();
								ws2->setCurrentIndex( 0 );
								out2->setText( runOut );
								lodt2->setChecked( false );
								hm2->setChecked( false );
								vt2->setChecked( false );
								nat2->setChecked( true );
								w2->setValue( -20 ); e2->setValue( -20 );
								s2->setValue( 24 ); n2->setValue( 24 );
								dimBox2->setCurrentIndex( 1 );		// dim 4
								/* The viewport preview off: it splices every finished
								 * chunk into the workspace and reframes, which is a
								 * person's feature and a harness's noise. Found by its
								 * text -- it carries no object name. */
								if ( panel )
									for ( QCheckBox * c : panel->findChildren<QCheckBox *>() )
										if ( c->text().startsWith( QLatin1String( "Show chunks" ) ) )
											c->setChecked( false );
								QApplication::processEvents();
								log << "  run 1 armed, Generate enabled: "
									<< ( gen2->isEnabled() ? "yes" : "no" ) << "\n";
								check( "with the native row ticked the panel will run", gen2->isEnabled() );
								gen2->click();
								const qint64 ms1 = waitForRun( 600000 );
								const QString line1 = resL2->text();
								log << "  run 1 finished in " << ms1 << " ms\n";
								log << "  run 1 result line: '" << line1 << "'\n";
								const QString lodo = runOut + QStringLiteral( "/Terrain/Commonwealth.lodo" );
								const QString lodi = runOut + QStringLiteral( "/Terrain/Commonwealth.lodi" );
								const qint64 lodoBytes = QFileInfo( lodo ).size();
								const qint64 lodiBytes = QFileInfo( lodi ).size();
								log << "  Commonwealth.lodo " << lodoBytes << " bytes, .lodi "
									<< lodiBytes << " bytes\n";
								check( "a GUI run with the native row on writes the .lodo/.lodi pair",
									QFileInfo::exists( lodo ) && QFileInfo::exists( lodi )
									&& lodoBytes > 4096 && lodiBytes > 64 );
								check( "the result line names all four stages",
									line1.contains( QLatin1String( "landscape" ) )
									&& line1.contains( QLatin1String( "meshes" ) )
									&& line1.contains( QLatin1String( "textures" ) )
									&& line1.contains( QLatin1String( "impostors" ) ) );
								const double land1 = stageOf( line1, QStringLiteral( "landscape" ) );
								const double mesh1 = stageOf( line1, QStringLiteral( "meshes" ) );
								const double imp1 = stageOf( line1, QStringLiteral( "impostors" ) );
								log << "  run 1 stages: landscape " << land1 << ", meshes " << mesh1
									<< ", impostors " << imp1 << "\n";
								check( "the meshes stage is above zero when meshes were built", mesh1 > 0.0 );
								check( "and the landscape stage is exactly zero when no landscape file was written",
									land1 == 0.0 );
								check( "and the impostors stage is zero with no card library",
									imp1 == 0.0 );

								// ---- RUN 2: the shadow heightmap and nothing else ----
								nat2->setChecked( false );
								lodt2->setChecked( false );
								vt2->setChecked( false );
								hm2->setChecked( true );
								hmSize2->setCurrentIndex( hmSize2->findData( 4096 ) );
								QApplication::processEvents();
								log << "  run 2 armed, Generate enabled: "
									<< ( gen2->isEnabled() ? "yes" : "no" ) << "\n";
								gen2->click();
								const qint64 ms2 = waitForRun( 600000 );
								const QString line2 = resL2->text();
								log << "  run 2 finished in " << ms2 << " ms\n";
								log << "  run 2 result line: '" << line2 << "'\n";
								const double land2 = stageOf( line2, QStringLiteral( "landscape" ) );
								const double mesh2 = stageOf( line2, QStringLiteral( "meshes" ) );
								const double tex2 = stageOf( line2, QStringLiteral( "textures" ) );
								log << "  run 2 stages: landscape " << land2 << ", meshes " << mesh2
									<< ", textures " << tex2 << "\n";
								check( "the landscape stage is above zero when the heightmap was baked",
									land2 > 0.0 );
								check( "and the meshes and textures stages fall back to zero",
									mesh2 == 0.0 && tex2 == 0.0 );
								check( "so the line is not the last run's, reprinted",
									line1 != line2 );
								/* THE RESULT-LINE PICTURE: the action bar on its own,
								 * with the four times in it. */
								const QByteArray rshot = qgetenv( "WW_LODGEN_SHOT_RESULT" );
								if ( !rshot.isEmpty() ) {
									if ( auto * bar = findChild<QWidget *>( QStringLiteral( "LodgenActionBar" ) ) ) {
										const bool sv = bar->grab().save( QString::fromLocal8Bit( rshot ) );
										log << "  result-line shot " << ( sv ? "saved: " : "NOT saved: " )
											<< QString::fromLocal8Bit( rshot ) << "\n";
									}
								}
							} else {
								for ( const char * w : { "a GUI run writes the pair",
										"the four stage times are written", "and they move" } )
									check( QString( "LODUI1 run leg: %1" ).arg( QLatin1String( w ) ), false );
							}
							// put the panel's settings back exactly as they were found
							{
								QSettings s;
								s.beginGroup( QStringLiteral( "LodGeneration" ) );
								for ( const QString & k : s.allKeys() )
									if ( !savedGroup.contains( k ) )
										s.remove( k );
								for ( auto it = savedGroup.constBegin(); it != savedGroup.constEnd(); ++it )
									s.setValue( it.key(), it.value() );
								s.endGroup();
								s.sync();
							}
						}
