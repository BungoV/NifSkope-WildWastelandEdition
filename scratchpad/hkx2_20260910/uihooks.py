import io
p = 'src/nifskope_ui.cpp'
s = io.open(p, encoding='utf-8', newline='').read()
b = s

# ---- 0. the include -------------------------------------------------------
anchor = '#include "gl/glscene.h"'
if anchor not in s:
    raise SystemExit('include anchor missing')
if '#include "hkxplayback.h"' not in s:
    s = s.replace(anchor, anchor + '\n#include "hkxplayback.h"', 1)

# ---- 1. install the harness (one line) ------------------------------------
h_anchor = "\t/* TEST HARNESS (WW_CYCLETYPE_TEST=1): does the preview do what the SEQUENCE"
if h_anchor not in s:
    raise SystemExit('cycletype harness anchor missing')
h_new = ("\t// TEST HARNESS (WW_HKXANIM_TEST=1): lane HKX2's playback and mapping gates.\n"
         "\t// The whole harness is src/hkxplaybacktest.cpp; this is its only line here.\n"
         "\twwHkxAnimHarness( skope );\n\n" + h_anchor)
s = s.replace(h_anchor, h_new, 1)

# ---- 2. the Animation panel's Load row ------------------------------------
panel_anchor = """		grid->addWidget( timelineBtn, 7, 0, 1, 2 );

		QWidgetAction * animWa = new QWidgetAction( animMenu );"""
if panel_anchor not in s:
    raise SystemExit('animation panel anchor missing')
panel_new = """		grid->addWidget( timelineBtn, 7, 0, 1, 2 );

		/* LOAD ANIMATION (.hkx) -- bungo 2026-09-10, lane HKX2/HKX3.
		 *
		 * His words: "in animation workspace, add an option to load a hkx file
		 * with animation, then they get added to the animations list, and if
		 * there's rigged geometry with nodes / bone names that match, they play".
		 *
		 * The loaded clip goes into Scene::animGroups and Scene::animTags, so the
		 * Sequence row above, the scrub bar, Loop, Speed and Cycle are already
		 * its transport and nothing else here changes. The label under the button
		 * is the summary line: how many bones play, which ones do not, and where
		 * the bone names came from.
		 */
		auto * hkxBtn = new QPushButton( tr( "Load Animation (.hkx)…" ), animPanel );
		hkxBtn->setStyleSheet( boxQss );
		hkxBtn->setToolTip( tr( "Load a Havok animation and add it to the animations "
								"list. It plays on the bones of the open NIF whose "
								"names match its tracks." ) );
		auto * hkxNote = new QLabel( animPanel );
		hkxNote->setWordWrap( true );
		hkxNote->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( "textMuted" ) ) );
		connect( hkxBtn, &QPushButton::clicked, this, [this, hkxNote, refreshAnimPanel]() {
			Scene * sc = ogl->getScene();
			if ( !sc || !sc->hkx )
				return;
			const QString f = QFileDialog::getOpenFileName( this, tr( "Load Animation" ),
				QString(), tr( "Havok animation (*.hkx *.xml);;All files (*)" ) );
			if ( f.isEmpty() )
				return;
			QStringList added;
			const QString err = sc->hkx->load( f, &added );
			if ( !err.isEmpty() ) {
				hkxNote->setText( err );
				return;
			}
			if ( !added.isEmpty() )
				ogl->setSceneSequence( added.first() );
			hkxNote->setText( sc->hkx->summary() );
			refreshAnimPanel();
		} );
		grid->addWidget( hkxBtn, 8, 0, 1, 2 );
		grid->addWidget( hkxNote, 9, 0, 1, 2 );

		// Root motion is a switch of its own and starts OFF (bungo's ruling): a
		// clip that walks forward would otherwise carry the rig out of frame the
		// moment it is selected.
		auto * rootChk = new QCheckBox( tr( "Root motion" ), animPanel );
		rootChk->setToolTip( tr( "Apply the loaded animation's extracted root motion "
								 "to the skeleton root" ) );
		connect( rootChk, &QCheckBox::toggled, this, [this, hkxNote]( bool on ) {
			Scene * sc = ogl->getScene();
			if ( !sc || !sc->hkx )
				return;
			sc->hkx->setRootMotion( on );
			if ( !sc->hkx->activeName().isEmpty() )
				hkxNote->setText( sc->hkx->summary() );
			ogl->update();
		} );
		grid->addWidget( rootChk, 10, 0, 1, 2 );

		QWidgetAction * animWa = new QWidgetAction( animMenu );"""
s = s.replace(panel_anchor, panel_new, 1)

# ---- 3. the render hook, so a picture can be taken of a clip ---------------
shot_anchor = """				// WW_RENDER_SEQ=<name> selects a sequence. Without it the"""
if shot_anchor not in s:
    raise SystemExit('render seq anchor missing')
shot_new = """				/* WW_HKXANIM_CLIP=<file.hkx>: load a Havok animation into the
				 * scene's playback BEFORE the sequence is chosen, so a picture
				 * of a loaded clip is WW_RENDER_SEQ naming it and WW_RENDER_TIME
				 * picking the frame. Lane HKX2's gate (e) is three of these.
				 * WW_HKXANIM_ROOTMOTION=1 turns the root motion on for the shot.
				 */
				if ( Scene * hsc = skope->ogl->getScene(); hsc && hsc->hkx ) {
					const QString hkxPath = qEnvironmentVariable( "WW_HKXANIM_CLIP" );
					if ( !hkxPath.isEmpty() ) {
						QStringList hkxAdded;
						const QString hkxErr = hkxPath.isEmpty()
							? QString() : hsc->hkx->load( hkxPath, &hkxAdded );
						hsc->hkx->setRootMotion(
							qEnvironmentVariableIntValue( "WW_HKXANIM_ROOTMOTION" ) != 0 );
						if ( hkxErr.isEmpty() && !hkxAdded.isEmpty() )
							skope->ogl->setSceneSequence( hkxAdded.first() );
					}
				}

				// WW_RENDER_SEQ=<name> selects a sequence. Without it the"""
s = s.replace(shot_anchor, shot_new, 1)

if s == b:
    raise SystemExit('nothing changed')
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('cr=', open(p, 'rb').read().count(b'\r'))
