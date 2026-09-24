"""LOADORDER1 panel leg self-test (WW_LODGEN_MO2DISK=1) inside WW_LODGEN_TEST, before the PANEL1 byte gate.
nifskope_ui.cpp is LF-only: assert the anchor once and the CR count unchanged."""
import sys
P = 'E:/Projects/NifskopeWWE-loadorder1/src/nifskope_ui.cpp'
s = open(P, encoding='utf-8', newline='').read()
cr0 = s.count('\r')
anchor = '''						/* ===== LANE PANEL1: THE BYTE GATE (WW_LODGEN_GATE=1) =====
'''
block = r'''						/* ===== LANE LOADORDER1: THE MO2 PROFILE OFF DISK (WW_LODGEN_MO2DISK=1) =====
						 *
						 * The Source row's third choice reads a Mod Organizer 2 profile
						 * with no Mod Organizer running. The harness points it at a
						 * profile (WW_LODGEN_MO2_PROFILE, optional WW_LODGEN_MO2_MODS),
						 * reads the resolved plugin list and mod order OFF THE WIDGETS,
						 * shows a bad profile refusing by name, and -- with
						 * WW_LODGEN_MO2_OUT set -- presses Generate for one FO4CS chunk
						 * into that stand-in mod folder and walks what landed: the
						 * FO4CSLOD root, and no legacy .BTO / .BTR anywhere (bungo's
						 * R2). Settings are put back as the run leg puts them back.
						 * Every line carries "MO2DISK"; tests/spells/lodgen_panel_mo2.sh
						 * counts its own. */
						if ( qEnvironmentVariableIntValue( "WW_LODGEN_MO2DISK" ) == 1 ) {
							QMap<QString, QVariant> savedGroup3;
							{
								QSettings s;
								s.beginGroup( QStringLiteral( "LodGeneration" ) );
								for ( const QString & k : s.allKeys() )
									savedGroup3.insert( k, s.value( k ) );
								s.endGroup();
							}
							auto * src3 = findChild<QComboBox *>( QStringLiteral( "LodgenSourceBox" ) );
							auto * prof3 = findChild<QLineEdit *>( QStringLiteral( "LodgenMo2ProfileEdit" ) );
							auto * mods3 = findChild<QLineEdit *>( QStringLiteral( "LodgenMo2ModsEdit" ) );
							auto * stack3 = findChild<QListWidget *>( QStringLiteral( "LodgenMo2StackList" ) );
							auto * plugins3 = findChild<QListWidget *>( QStringLiteral( "LodgenPluginList" ) );
							auto * status3 = findChild<QLabel *>( QStringLiteral( "LodgenSourceStatus" ) );
							auto * resList3 = findChild<QListWidget *>( QStringLiteral( "LodgenResourceList" ) );
							auto * ws3 = findChild<QComboBox *>( QStringLiteral( "LodgenWorldspaceBox" ) );
							auto * gen3 = findChild<QPushButton *>( QStringLiteral( "LodgenGenerateButton" ) );
							auto * cancel3 = findChild<QPushButton *>( QStringLiteral( "LodgenCancelButton" ) );
							auto * resL3 = findChild<QLabel *>( QStringLiteral( "LodgenResultLabel" ) );
							const int idx3 = src3 ? src3->findData( 2 ) : -1;
							check( "MO2DISK the Source row offers Mod Organizer 2 profile", idx3 >= 0 );
							if ( src3 && prof3 && mods3 && stack3 && plugins3 && status3 && resList3 && ws3
								&& gen3 && cancel3 && resL3 && idx3 >= 0 ) {
								// the worldspace list refreshes on a timer: pump until Commonwealth is offered
								auto pumpWs = [&]( int budgetMs ) {
									QElapsedTimer t;
									t.start();
									QApplication::processEvents();
									while ( t.elapsed() < budgetMs && ws3->findData( 0x3Cu ) < 0 ) {
										QApplication::processEvents( QEventLoop::AllEvents, 20 );
										QThread::msleep( 20 );
									}
									QApplication::processEvents();
									return t.elapsed();
								};
								// a profile folder that is not one: the refusal names it, Generate refuses
								prof3->setText( QStringLiteral( "C:/no/such/profile" ) );
								mods3->setText( QString() );
								src3->setCurrentIndex( idx3 );
								QApplication::processEvents();
								log << "  MO2DISK bad profile status: '" << status3->text() << "'\n";
								check( "MO2DISK a folder with no modlist.txt is refused by name on the status line",
									status3->text().contains( QLatin1String( "modlist.txt" ) )
									&& status3->text().contains( QLatin1String( "C:/no/such/profile" ) ) );
								check( "MO2DISK and Generate is refused with it", !gen3->isEnabled() );
								// the profile under test
								mods3->setText( qEnvironmentVariable( "WW_LODGEN_MO2_MODS" ) );
								prof3->setText( qEnvironmentVariable( "WW_LODGEN_MO2_PROFILE" ) );
								const qint64 wsMs = pumpWs( 120000 );
								const int np = plugins3->count();
								log << "  MO2DISK status: '" << status3->text() << "'\n";
								log << "  MO2DISK plugins " << np << ", mod order " << stack3->count()
									<< " rows, worldspaces after " << wsMs << " ms\n";
								bool allFull = np > 0;
								for ( int i = 0; i < np; i++ ) {
									const QString p = plugins3->item( i )->text();
									log << "  MO2DISK plugin " << i << ": " << p << "\n";
									if ( !QFileInfo( p ).isAbsolute() || !QFileInfo( p ).isFile() )
										allFull = false;
								}
								for ( int i = 0; i < stack3->count(); i++ )
									log << "  MO2DISK stack " << i << ": " << stack3->item( i )->text() << " | "
										<< stack3->item( i )->toolTip() << "\n";
								check( "MO2DISK plugin 0 is Fallout4.esm by full path",
									np > 0 && plugins3->item( 0 )->text().endsWith( QLatin1String( "/Fallout4.esm" ), Qt::CaseInsensitive )
									&& QFileInfo( plugins3->item( 0 )->text() ).isAbsolute() );
								check( QString( "MO2DISK every listed plugin is an existing full path (%1)" ).arg( np ), allFull );
								const int wantP = qEnvironmentVariableIntValue( "WW_LODGEN_MO2_NPLUGINS" );
								check( QString( "MO2DISK the list holds the expected %1 plugins (%2)" ).arg( wantP ).arg( np ),
									wantP > 0 && np == wantP );
								const int wantS = qEnvironmentVariableIntValue( "WW_LODGEN_MO2_NSTACK" );
								check( QString( "MO2DISK the mod order shows the expected %1 rows, game Data first (%2)" )
									.arg( wantS ).arg( stack3->count() ),
									stack3->isVisible() && wantS > 0 && stack3->count() == wantS
									&& stack3->item( 0 )->toolTip().endsWith( QLatin1String( "/Data" ), Qt::CaseInsensitive ) );
								check( "MO2DISK the Resources list is hidden under this source", !resList3->isVisible() );
								check( "MO2DISK the plugin list is read only (no drag)",
									plugins3->dragDropMode() == QAbstractItemView::NoDragDrop );
								check( "MO2DISK the profile rows are shown", prof3->isVisible() && mods3->isVisible() );
								check( "MO2DISK the worldspace list filled from the resolved plugins (Commonwealth)",
									ws3->findData( 0x3Cu ) >= 0 );
								check( "MO2DISK the status line says what it read",
									status3->text().contains( QLatin1String( "plugins" ) )
									&& status3->text().contains( QLatin1String( "mods enabled" ) ) );
								const QByteArray shot3 = qgetenv( "WW_LODGEN_MO2_SHOT" );
								if ( !shot3.isEmpty() ) {
									if ( auto * dock3 = findChild<QDockWidget *>( QStringLiteral( "LodGenerationDock" ) ) ) {
										if ( auto * sa3 = dock3->findChild<QScrollArea *>() ) {
											sa3->verticalScrollBar()->setValue( 0 );
											sa3->ensureWidgetVisible( stack3, 0, 40 );
										}
										QApplication::processEvents();
										QApplication::processEvents();
										const bool sv = dock3->grab().save( QString::fromLocal8Bit( shot3 ) );
										log << "  MO2DISK shot " << ( sv ? "saved: " : "NOT saved: " )
											<< QString::fromLocal8Bit( shot3 ) << "\n";
									}
								}
								const QString out3 = qEnvironmentVariable( "WW_LODGEN_MO2_OUT" );
								if ( !out3.isEmpty() ) {
									auto * target3 = findChild<QComboBox *>( QStringLiteral( "LodgenTargetBox" ) );
									auto * outE3 = findChild<QLineEdit *>( QStringLiteral( "LodgenOutputEdit" ) );
									auto * nat3 = findChild<QCheckBox *>( QStringLiteral( "LodgenNativeCheck" ) );
									auto * lodt3 = findChild<QCheckBox *>( QStringLiteral( "LodgenLodtCheck" ) );
									auto * hm3 = findChild<QCheckBox *>( QStringLiteral( "LodgenHeightmapCheck" ) );
									auto * vt3 = findChild<QCheckBox *>( QStringLiteral( "LodgenVtCheck" ) );
									auto * w3 = findChild<QSpinBox *>( QStringLiteral( "LodgenWestSpin" ) );
									auto * e3 = findChild<QSpinBox *>( QStringLiteral( "LodgenEastSpin" ) );
									auto * s3 = findChild<QSpinBox *>( QStringLiteral( "LodgenSouthSpin" ) );
									auto * n3 = findChild<QSpinBox *>( QStringLiteral( "LodgenNorthSpin" ) );
									QComboBox * dim3 = nullptr;
									if ( panel )
										for ( QComboBox * c : panel->findChildren<QComboBox *>() )
											if ( c->count() == 5 && c->itemText( 1 ) == QLatin1String( "4" ) )
												dim3 = c;
									if ( target3 && outE3 && nat3 && lodt3 && hm3 && vt3 && w3 && e3 && s3 && n3 && dim3 ) {
										QDir( out3 ).removeRecursively();
										target3->setCurrentIndex( 0 );		// FO4 Community Shaders
										QApplication::processEvents();
										ws3->setCurrentIndex( ws3->findData( 0x3Cu ) );
										outE3->setText( out3 );
										lodt3->setChecked( false );
										hm3->setChecked( false );
										vt3->setChecked( false );
										nat3->setChecked( true );
										w3->setValue( -20 ); e3->setValue( -20 );
										s3->setValue( 24 ); n3->setValue( 24 );
										dim3->setCurrentIndex( 1 );		// dim 4
										if ( panel )
											for ( QCheckBox * c : panel->findChildren<QCheckBox *>() )
												if ( c->text().startsWith( QLatin1String( "Show chunks" ) ) )
													c->setChecked( false );
										QApplication::processEvents();
										log << "  MO2DISK run armed, Generate enabled: " << ( gen3->isEnabled() ? "yes" : "no" ) << "\n";
										gen3->click();
										QElapsedTimer t;
										t.start();
										while ( cancel3->isEnabled() && t.elapsed() < 600000 ) {
											QApplication::processEvents( QEventLoop::AllEvents, 20 );
											QThread::msleep( 5 );
										}
										log << "  MO2DISK run finished in " << t.elapsed() << " ms: '" << resL3->text() << "'\n";
										int legacy = 0, files = 0;
										QDirIterator di( out3, QDir::Files, QDirIterator::Subdirectories );
										while ( di.hasNext() ) {
											const QString f = di.next();
											files++;
											const QString rel = QDir( out3 ).relativeFilePath( f );
											if ( files <= 40 )
												log << "  MO2DISK wrote " << rel << "\n";
											if ( rel.endsWith( QLatin1String( ".bto" ), Qt::CaseInsensitive )
												|| rel.endsWith( QLatin1String( ".btr" ), Qt::CaseInsensitive ) )
												legacy++;
										}
										const QString nat = lodgenFo4csWorldDir( out3, QStringLiteral( "Commonwealth" ) );
										log << "  MO2DISK " << files << " files, " << legacy << " legacy .BTO/.BTR; FO4CS root "
											<< nat << "\n";
										check( QString( "MO2DISK a GUI bake from the profile writes the .lodo in %1" ).arg( nat ),
											QFileInfo( nat + QStringLiteral( "/Commonwealth.lodo" ) ).size() > 4096 );
										check( "MO2DISK no legacy .BTO or .BTR anywhere in the mod folder, no scratch folder left (R2)",
											files > 0 && legacy == 0
											&& !QFileInfo::exists( out3 + QStringLiteral( "/lodgen_bto_scratch" ) ) );
									} else {
										check( "MO2DISK the run rows are all found", false );
									}
								}
							}
							{
								QSettings s;
								s.beginGroup( QStringLiteral( "LodGeneration" ) );
								for ( const QString & k : s.allKeys() )
									if ( !savedGroup3.contains( k ) )
										s.remove( k );
								for ( auto it = savedGroup3.constBegin(); it != savedGroup3.constEnd(); ++it )
									s.setValue( it.key(), it.value() );
								s.endGroup();
								s.sync();
							}
						}
'''
assert s.count(anchor) == 1
s = s.replace(anchor, block + anchor)
assert s.count('\r') == cr0
if '--check' in sys.argv:
    print('check ok')
else:
    with open(P, 'w', encoding='utf-8', newline='') as f:
        f.write(s)
    print('applied')
