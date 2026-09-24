						/* ================= LANE LODUI1, 2026-09-11 =================
						 *
						 * bungo's four rulings of that morning, each counted with a
						 * floor on the other side, so a panel that hid everything and
						 * a panel that hid nothing both fail:
						 *
						 *   06:4x  "we should only have those 5 .lod types in fo4
						 *          community shaders target"
						 *   07:0x  "I've only wanted trees for the impostors",
						 *          amended 07:1x "Make trees only a toggle"
						 *   07:3x  the card resolution list gains 512 px
						 *   07:4x  "Cards from ring" IS the tree override he asked
						 *          for, and its label now says so
						 *
						 * Every row's hidden flag is PRINTED beside its name before it
						 * is counted, so a count that moves in a later lane is read
						 * against a list and not against a number nobody can place. */
						{
							auto * nativeChk = findChild<QCheckBox *>( QStringLiteral( "LodgenNativeCheck" ) );
							auto * objChk = findChild<QCheckBox *>( QStringLiteral( "LodgenObjectsCheck" ) );
							auto * btrSec = findChild<QWidget *>( QStringLiteral( "LodgenBtrSection" ) );
							auto * atlasChk2 = findChild<QCheckBox *>( QStringLiteral( "LodgenAtlasCheck" ) );
							auto * vtBtrChk2 = findChild<QCheckBox *>( QStringLiteral( "LodgenVtBtrCheck" ) );
							auto * treesChk = findChild<QCheckBox *>( QStringLiteral( "LodgenTreesOnlyCheck" ) );
							auto * ringBox = findChild<QComboBox *>( QStringLiteral( "LodgenImpostorLevelBox" ) );
							auto * sumL = findChild<QLabel *>( QStringLiteral( "LodgenSummaryLabel" ) );
							auto * outE2 = findChild<QLineEdit *>( QStringLiteral( "LodgenOutputEdit" ) );
							auto * resL = findChild<QLabel *>( QStringLiteral( "LodgenResultLabel" ) );
							if ( target && nativeChk && objChk && btrSec && atlasChk2 && vtBtrChk2
								&& treesChk && ringBox && sumL && outE2 && resL ) {
								const int keepTarget = target->currentIndex();
								const QString keepOut = outE2->text();
								/* An output folder, so the summary is the "Will write"
								 * sentence and not a refusal: the extension list is what
								 * is under test and a refusal carries none. */
								outE2->setText( QDir::tempPath() + QStringLiteral( "/Lodgen_LODUI1" ) );

								// ---- FO4 Community Shaders: the five .lod types ----
								target->setCurrentIndex( 0 );
								QApplication::processEvents();
								QVector<QPair<QString, QWidget *>> legacy;
								legacy << qMakePair( QStringLiteral( "the .btr section" ), (QWidget *) btrSec )
									   << qMakePair( QStringLiteral( "the .bto head" ), (QWidget *) objChk )
									   << qMakePair( QStringLiteral( "the object atlas" ), (QWidget *) atlasChk2 )
									   << qMakePair( QStringLiteral( "chunk textures from the pyramid" ), (QWidget *) vtBtrChk2 );
								int legacyHidden = 0;
								for ( const auto & row : legacy ) {
									log << "  FO4CS: " << row.first << " hidden: "
										<< ( row.second->isHidden() ? "yes" : "no" ) << "\n";
									if ( row.second->isHidden() )
										legacyHidden++;
								}
								check( "FO4 Community Shaders hides all four legacy object rows",
									legacyHidden == 4 );
								check( "and shows the native .lodo/.lodi row instead", !nativeChk->isHidden() );
								const QString sumCs = sumL->text();
								log << "  FO4CS summary: '" << sumCs << "'\n";
								check( "the FO4CS summary names .lodl, .lodt, .lodo and .lodi",
									sumCs.contains( QLatin1String( ".lodl" ) )
									&& sumCs.contains( QLatin1String( ".lodt" ) )
									&& sumCs.contains( QLatin1String( ".lodo" ) )
									&& sumCs.contains( QLatin1String( ".lodi" ) ) );
								check( "and never the legacy .btr chunks",
									!sumCs.contains( QLatin1String( ".btr" ), Qt::CaseInsensitive ) );

								// ---- the stock engine: the reverse, row for row ----
								target->setCurrentIndex( 1 );
								QApplication::processEvents();
								int legacyShown = 0;
								for ( const auto & row : legacy ) {
									log << "  Stock: " << row.first << " hidden: "
										<< ( row.second->isHidden() ? "yes" : "no" ) << "\n";
									if ( !row.second->isHidden() )
										legacyShown++;
								}
								check( "the stock engine shows all four of them again", legacyShown == 4 );
								check( "and hides the native row, which it cannot read", nativeChk->isHidden() );
								const QString sumStock = sumL->text();
								log << "  Stock summary: '" << sumStock << "'\n";
								check( "the stock summary names .btr and .bto",
									sumStock.contains( QLatin1String( ".btr" ), Qt::CaseInsensitive )
									&& sumStock.contains( QLatin1String( ".bto" ), Qt::CaseInsensitive ) );
								check( "and none of the four .lod types its reader has no use for",
									!sumStock.contains( QLatin1String( ".lodl" ) )
									&& !sumStock.contains( QLatin1String( ".lodt" ) )
									&& !sumStock.contains( QLatin1String( ".lodo" ) )
									&& !sumStock.contains( QLatin1String( ".lodi" ) ) );

								/* THE SAVED-TICK LAW this lane had to keep: restoring a
								 * target only HIDES; a tick under a hidden row is a
								 * saved setting, never a request. Ticked here while the
								 * stock engine owns the panel, and the run must not see
								 * it. */
								nativeChk->setChecked( true );
								QApplication::processEvents();
								check( "a native tick under the stock target does not reach the run",
									!sumL->text().contains( QLatin1String( ".lodo" ) ) );

								target->setCurrentIndex( 0 );
								QApplication::processEvents();

								// ---- Trees only, and the ring override it renamed ----
								log << "  Trees only default: " << ( treesChk->isChecked() ? "on" : "off" ) << "\n";
								check( "Trees only is ON by default", treesChk->isChecked() );
								check( "its label is a name and its tooltip is one sentence",
									treesChk->text() == QLatin1String( "Trees only" )
									&& !treesChk->toolTip().isEmpty()
									&& !treesChk->toolTip().contains( QLatin1Char( '\n' ) ) );
								int ringLabelled = 0;
								if ( panel )
									for ( QLabel * l : panel->findChildren<QLabel *>() )
										if ( l->text() == QLatin1String( "Tree cards from ring" ) )
											ringLabelled++;
								log << "  labels reading 'Tree cards from ring': " << ringLabelled << "\n";
								check( "the ring override says it is trees only", ringLabelled == 1 );
								check( "and its tooltip is one sentence",
									!ringBox->toolTip().isEmpty()
									&& !ringBox->toolTip().contains( QLatin1Char( '\n' ) ) );

								// ---- 512 px, and the cost line that moves with it ----
								{
									auto * cf2 = findChild<QComboBox *>( QStringLiteral( "LodgenCardFramesBox" ) );
									auto * cr3 = findChild<QComboBox *>( QStringLiteral( "LodgenCardResBox" ) );
									auto * ha2 = findChild<QCheckBox *>( QStringLiteral( "LodgenCardHalfAuxCheck" ) );
									auto * cl2 = findChild<QLabel *>( QStringLiteral( "LodgenCardCostLabel" ) );
									if ( cf2 && cr3 && ha2 && cl2 ) {
										const int keepF = cf2->currentIndex(), keepR = cr3->currentIndex();
										const bool keepH = ha2->isChecked();
										cf2->setCurrentIndex( cf2->findData( 8 ) );
										ha2->setChecked( false );
										cr3->setCurrentIndex( cr3->findData( 256 ) );
										qApp->processEvents();
										const QString at256 = cl2->text();
										cr3->setCurrentIndex( cr3->findData( 512 ) );
										qApp->processEvents();
										const QString at512 = cl2->text();
										log << "  cost at 8x8/256: " << at256 << "\n";
										log << "  cost at 8x8/512: " << at512 << "\n";
										check( "512 px is offered and reaches the sheet arithmetic",
											cr3->findData( 512 ) >= 0
											&& at512.contains( QLatin1String( "4096 x 4096" ) )
											&& at512.contains( QLatin1String( "512 x 512" ) ) );
										/* The floor: the same line at 256 must be a
										 * DIFFERENT sentence naming the 2048 sheet, so a
										 * fixed string cannot pass either half. */
										check( "and the cost line moves when the row moves",
											at256 != at512 && at256.contains( QLatin1String( "2048 x 2048" ) ) );
										cf2->setCurrentIndex( keepF );
										cr3->setCurrentIndex( keepR );
										ha2->setChecked( keepH );
										qApp->processEvents();
									} else {
										check( "512 px is offered and reaches the sheet arithmetic", false );
										check( "and the cost line moves when the row moves", false );
									}
								}

								// ---- the result line: the four stage times live here ----
								log << "  result line before a run: '" << resL->text() << "'\n";
								check( "the result line carries nothing until something has run",
									resL->text().isEmpty() );

								outE2->setText( keepOut );
								target->setCurrentIndex( keepTarget );
								QApplication::processEvents();
							} else {
								for ( const char * w : { "the five .lod types under FO4CS",
										"Trees only", "512 px", "the native row", "the result line" } )
									check( QString( "LODUI1 row missing: %1" ).arg( QLatin1String( w ) ), false );
							}
						}
