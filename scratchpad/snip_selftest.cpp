						/* 2026-09-06n: the far-ring proxy rows. Four numbers, one
						 * box, and two things a screenshot would not show: the
						 * ratios grey with the box that owns them, and the rows
						 * survive the STOCK target, because a smaller mesh is a
						 * smaller mesh for both readers. Each is measured
						 * against its opposite state, so a row that is always
						 * enabled or always hidden cannot pass. */
						{
							auto * simp = findChild<QCheckBox *>( QStringLiteral( "LodgenSimplifyCheck" ) );
							auto * r16 = findChild<QDoubleSpinBox *>( QStringLiteral( "LodgenSimplify16Spin" ) );
							auto * r32 = findChild<QDoubleSpinBox *>( QStringLiteral( "LodgenSimplify32Spin" ) );
							auto * sErr = findChild<QDoubleSpinBox *>( QStringLiteral( "LodgenSimplifyErrorSpin" ) );
							if ( simp && r16 && r32 && sErr && target ) {
								log << "far-ring defaults: ring 2 " << r16->value()
									<< ", ring 3 " << r32->value() << ", error " << sErr->value() << "\n";
								check( "the far rings default to fewer triangles the further out they are",
									r16->value() < 1.0 && r32->value() < r16->value() );
								simp->setChecked( false );
								QApplication::processEvents();
								const bool greyed = !r16->isEnabled() && !r32->isEnabled() && !sErr->isEnabled();
								simp->setChecked( true );
								QApplication::processEvents();
								const bool live = r16->isEnabled() && r32->isEnabled() && sErr->isEnabled();
								check( "the ratios grey with the far-ring box", greyed && live );
								const int keepTarget = target->currentIndex();
								target->setCurrentIndex( 1 );		// stock engine
								QApplication::processEvents();
								const bool stockKeeps = !simp->isHidden() && !r16->isHidden();
								target->setCurrentIndex( 0 );
								QApplication::processEvents();
								const bool csKeeps = !simp->isHidden() && !r16->isHidden();
								target->setCurrentIndex( keepTarget );
								QApplication::processEvents();
								check( "far-ring simplification is offered to BOTH targets",
									stockKeeps && csKeeps );
							} else {
								check( "the far-ring rows exist", false );
							}
						}
