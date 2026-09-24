/* The one loader for .hkx animation clips, and the refusals the animations
   list would otherwise have nowhere to put. Header carries the rationale.

   Lane HKX3, 2026-09-10. */

#include "hkxanimui.h"

#include "hkxplayback.h"
#include "glview.h"
#include "gl/glscene.h"
#include "gl/glnode.h"

#include <QFileInfo>

#include <cmath>
#include <utility>

/*
 *  WwHkxListEntry
 */

QString WwHkxListEntry::label() const
{
	if ( refused )
		return QObject::tr( "%1 — refused" ).arg( name );

	// Frame count and rate, because they are the two numbers that decide
	// whether the timeline's ticks mean anything: the Mixamo fixture is 60 fps
	// and every vanilla Fallout 4 clip is 30, and a ruler ticking at the wrong
	// one is wrong in a way nobody notices.
	if ( numFrames > 0 && fps > 0.0f )
		return QObject::tr( "%1 — %2 frames @ %3 fps" )
			.arg( name ).arg( numFrames ).arg( qRound( fps ) );
	if ( numFrames > 0 )
		return QObject::tr( "%1 — %2 frames" ).arg( name ).arg( numFrames );
	return name;
}

/*
 *  WwHkxAnimHub
 */

WwHkxAnimHub * WwHkxAnimHub::instance()
{
	static WwHkxAnimHub hub;
	return &hub;
}

bool WwHkxAnimHub::isAnimationFile( const QString & path )
{
	const QString suffix = QFileInfo( path ).suffix().toLower();
	// .xml is HKXPACK's text form of the same packfile, which the reader takes
	// through the same API (lane HKX1). It is not a NifSkope document type, so
	// there is no ambiguity with File > Open.
	return suffix == QLatin1String( "hkx" ) || suffix == QLatin1String( "xml" );
}

QString WwHkxAnimHub::fileDialogFilter()
{
	return QObject::tr( "Havok animation (*.hkx *.xml);;All files (*)" );
}

QStringList WwHkxAnimHub::animationFilesIn( const QStringList & paths )
{
	QStringList out;
	for ( const QString & p : paths ) {
		const QFileInfo info( p );
		if ( info.exists() && info.isFile() && isAnimationFile( p ) )
			out.append( info.absoluteFilePath() );
	}
	return out;
}

Scene * WwHkxAnimHub::sceneOf( GLView * ogl )
{
	return ogl ? ogl->getScene() : nullptr;
}

void WwHkxAnimHub::say( const QString & text, bool refusal, const QString & shortText )
{
	lastSentence = text;
	lastShort = shortText;
	lastWasRefusal = refusal;
	emit sentenceChanged( text, refusal );
}

QString WwHkxAnimHub::loadFiles( GLView * ogl, const QStringList & paths,
								 bool activateFirst, bool * refusal )
{
	auto answer = [this, refusal]( const QString & text, bool bad,
								   const QString & shortText = QString() ) {
		say( text, bad, shortText );
		if ( refusal )
			*refusal = bad;
		return text;
	};

	if ( paths.isEmpty() )
		return answer( tr( "No file was given to load." ), true );

	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return answer( tr( "There is no 3D scene open, so there is nothing for an "
						   "animation to play on." ), true );

	/* NO MODEL -> REFUSE IN WORDS (bungo's ruling for the drop path).
	 *
	 * "No model" is measured as an empty node list, not as an empty filename:
	 * NifSkope always has a document open, and the starter cube is a model. A
	 * clip loaded against an empty scene would sit in the list posing nothing,
	 * which is the silent failure this refusal exists to prevent.
	 */
	if ( sc->getNodes().isEmpty() )
		return answer( tr( "Nothing is open to animate. Open a rigged NIF first, "
						   "then load %1 onto it." )
					   .arg( QFileInfo( paths.first() ).fileName() ), true );

	SceneState & st = states[sc];

	QString firstAdded;
	int loaded = 0, refused = 0;
	QString lastRefusal;

	for ( const QString & path : paths ) {
		const QString shown = QFileInfo( path ).completeBaseName();

		auto refuse = [&]( const QString & why ) {
			// One row per file, replaced rather than duplicated when the same
			// file is dropped twice.
			for ( int i = st.refusedFiles.count() - 1; i >= 0; i-- ) {
				if ( st.refusedFiles.at( i ).path == path )
					st.refusedFiles.removeAt( i );
			}
			WwHkxListEntry e;
			e.name = shown;
			e.path = path;
			e.refused = true;
			e.reason = why;
			st.refusedFiles.append( e );
			lastRefusal = why;
			refused++;
		};

		if ( !isAnimationFile( path ) ) {
			refuse( tr( "%1 is not a Havok animation file (.hkx, or HKXPACK's .xml)." )
					.arg( QFileInfo( path ).fileName() ) );
			continue;
		}
		if ( !QFileInfo::exists( path ) ) {
			refuse( tr( "%1 is not there any more." ).arg( path ) );
			continue;
		}

		QStringList added;
		const QString err = sc->hkx->load( path, &added );
		if ( !err.isEmpty() ) {
			refuse( err );
			continue;
		}
		if ( added.isEmpty() ) {
			/* Read, but it carried no animation. skeleton.hkx is the usual
			 * one: its bones ARE kept, and clips loaded later use them for
			 * names, so this is a refusal of the REQUEST and not a wasted
			 * read -- which is what the sentence has to say (rule 10). */
			refuse( tr( "%1 carries no animation — %2" )
					.arg( QFileInfo( path ).fileName(),
						  sc->hkx->summary().isEmpty()
						  ? tr( "nothing in it is a clip." )
						  : sc->hkx->summary() ) );
			continue;
		}

		for ( const QString & name : added ) {
			// A name that loaded is no longer a refusal, whatever it was before.
			st.wontBind.remove( name );
			for ( int i = st.refusedFiles.count() - 1; i >= 0; i-- ) {
				if ( st.refusedFiles.at( i ).name == name )
					st.refusedFiles.removeAt( i );
			}
		}
		loaded += added.count();
		if ( firstAdded.isEmpty() )
			firstAdded = added.first();
	}

	// Root motion is a scene-wide switch: a clip loaded now must obey the
	// setting the user already made, not the playback's constructor default.
	sc->hkx->setRootMotion( st.rootMotion );

	bool bad = ( loaded == 0 );
	QString text;
	if ( loaded > 0 ) {
		if ( activateFirst && !firstAdded.isEmpty() ) {
			// activate() writes the wontBind register and emits for us.
			const bool plays = activate( ogl, firstAdded );
			text = plays ? sc->hkx->summary() : st.wontBind.value( firstAdded );
			bad = !plays;
		} else {
			text = sc->hkx->summary();
		}
		if ( refused > 0 )
			text += tr( "  %1 other file(s) refused: %2" ).arg( refused ).arg( lastRefusal );
	} else {
		text = lastRefusal.isEmpty() ? tr( "Nothing was loaded." ) : lastRefusal;
	}

	emit ogl->sequencesUpdated();
	emit clipsChanged();
	/* The LABEL gets the playback's few-word form; the full sentence -- which
	 * names every bone that did not bind -- goes to the tooltip through
	 * sentence() (lane UI6). */
	return answer( text, bad, loaded > 0 ? sc->hkx->summaryShort() : QString() );
}

bool WwHkxAnimHub::activate( GLView * ogl, const QString & entryName )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return false;

	SceneState & st = states[sc];

	// A refused FILE has no clip behind it; selecting its row must not silently
	// clear the sequence, it must say why the row is there.
	for ( const WwHkxListEntry & e : std::as_const( st.refusedFiles ) ) {
		if ( e.name == entryName ) {
			say( e.reason, true );
			emit clipsChanged();
			return false;
		}
	}

	ogl->setSceneSequence( entryName );

	/* THE MEASUREMENT. Not "will it bind" -- "did it". Scene::setSequence has
	 * already been through HkxPlayback::setActive by the time this line runs,
	 * so activeName() is the answer the playback itself gives.
	 */
	const bool plays = ( sc->hkx->activeName() == entryName );
	if ( plays )
		st.wontBind.remove( entryName );
	else if ( sc->hkx->has( entryName ) )
		st.wontBind.insert( entryName, sc->hkx->summary() );

	say( sc->hkx->summary(), !plays, sc->hkx->summaryShort() );
	emit clipsChanged();
	return plays;
}

bool WwHkxAnimHub::unload( GLView * ogl, const QString & entryName )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return false;

	SceneState & st = states[sc];
	bool went = false;

	for ( int i = st.refusedFiles.count() - 1; i >= 0; i-- ) {
		if ( st.refusedFiles.at( i ).name == entryName ) {
			st.refusedFiles.removeAt( i );
			went = true;
		}
	}
	st.wontBind.remove( entryName );

	if ( sc->hkx->has( entryName ) ) {
		// Clearing the sequence FIRST would restore the rig twice; unload()
		// restores it itself (lane HKX2's gate (b)), so the only thing left to
		// do is stop the scene naming a sequence that is gone.
		const bool wasActive = ( sc->hkx->activeName() == entryName );
		went = sc->hkx->unload( entryName ) || went;
		if ( wasActive )
			ogl->clearSceneSequence();
	}

	if ( went ) {
		say( tr( "Unloaded %1." ).arg( entryName ), false );
		emit ogl->sequencesUpdated();
		emit clipsChanged();
		ogl->update();
	}
	return went;
}

/* ---- lane UINOTES1 (ruling 5): the list edits itself.
   Every one of these is the SAME call the shortcut, the menu entry and the
   drop make; the workspace only decides which one and on which row. */

QString WwHkxAnimHub::duplicateEntry( GLView * ogl, const QString & entryName, QString * newName )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return tr( "There is no scene to copy an animation in." );
	if ( !sc->hkx->has( entryName ) )
		return tr( "%1 is one of the NIF's own sequences, not a loaded animation; it is copied in the Blocks tab." ).arg( entryName );
	QString made;
	const QString why = sc->hkx->duplicateClip( entryName, &made );
	if ( !why.isEmpty() ) {
		say( why, true );
		return why;
	}
	if ( newName )
		*newName = made;
	say( tr( "%1 copied as %2." ).arg( entryName, made ), false, made );
	emit ogl->sequencesUpdated();
	emit clipsChanged();
	ogl->update();
	return QString();
}

QString WwHkxAnimHub::pasteEntry( GLView * ogl, const HkxClipEntry & e, int atIndex, QString * newName )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return tr( "There is no scene to paste an animation into." );
	QString made;
	const QString why = sc->hkx->insertClip( e, atIndex, &made );
	if ( !why.isEmpty() ) {
		say( why, true );
		return why;
	}
	if ( newName )
		*newName = made;
	say( tr( "%1 pasted as %2." ).arg( e.name, made ), false, made );
	emit ogl->sequencesUpdated();
	emit clipsChanged();
	ogl->update();
	return QString();
}

QString WwHkxAnimHub::renameEntry( GLView * ogl, const QString & entryName, const QString & newName )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return tr( "There is no scene to rename an animation in." );
	if ( !sc->hkx->has( entryName ) )
		return tr( "%1 is one of the NIF's own sequences; its name lives in the NIF block, so it is renamed in the Blocks tab." ).arg( entryName );
	const QString why = sc->hkx->renameClip( entryName, newName );
	if ( !why.isEmpty() ) {
		say( why, true );
		return why;
	}
	// the refusal a row carries follows its name
	SceneState & st = states[sc];
	if ( st.wontBind.contains( entryName ) )
		st.wontBind.insert( newName.trimmed(), st.wontBind.take( entryName ) );
	say( tr( "%1 is now %2." ).arg( entryName, newName.trimmed() ), false, newName.trimmed() );
	emit ogl->sequencesUpdated();
	emit clipsChanged();
	ogl->update();
	return QString();
}

bool WwHkxAnimHub::setEntryOrder( GLView * ogl, const QStringList & entryNames )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return false;
	if ( !sc->hkx->setOrder( entryNames ) )
		return false;
	say( tr( "The animations are in a new order." ), false, tr( "reordered" ) );
	emit ogl->sequencesUpdated();
	emit clipsChanged();
	ogl->update();
	return true;
}

const HkxClipEntry * WwHkxAnimHub::clipEntry( GLView * ogl, const QString & entryName ) const
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return nullptr;
	return sc->hkx->find( entryName );
}

int WwHkxAnimHub::clipIndex( GLView * ogl, const QString & entryName ) const
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return -1;
	return sc->hkx->indexOfClip( entryName );
}

int WwHkxAnimHub::unloadAll( GLView * ogl )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return 0;

	SceneState & st = states[sc];
	const int n = sc->hkx->count() + st.refusedFiles.count();
	if ( n == 0 )
		return 0;

	const bool hadActive = !sc->hkx->activeName().isEmpty();
	sc->hkx->unloadAll();
	if ( hadActive )
		ogl->clearSceneSequence();
	st.refusedFiles.clear();
	st.wontBind.clear();

	say( tr( "Unloaded %1 animation(s)." ).arg( n ), false );
	emit ogl->sequencesUpdated();
	emit clipsChanged();
	ogl->update();
	return n;
}

void WwHkxAnimHub::setRootMotion( GLView * ogl, bool on )
{
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return;
	states[sc].rootMotion = on;
	sc->hkx->setRootMotion( on );
	if ( !sc->hkx->activeName().isEmpty() )
		say( sc->hkx->summary(), false, sc->hkx->summaryShort() );
	emit clipsChanged();
	ogl->update();
}

bool WwHkxAnimHub::rootMotion( GLView * ogl ) const
{
	const Scene * sc = ogl ? ogl->getScene() : nullptr;
	if ( !sc )
		return false;
	return states.value( sc ).rootMotion;
}

QVector<WwHkxListEntry> WwHkxAnimHub::entries( GLView * ogl ) const
{
	QVector<WwHkxListEntry> out;
	Scene * sc = sceneOf( ogl );
	if ( !sc || !sc->hkx )
		return out;

	const SceneState st = states.value( sc );

	const QStringList names = sc->hkx->names();
	for ( const QString & name : names ) {
		WwHkxListEntry e;
		e.name = name;
		if ( const HkxClipEntry * c = sc->hkx->find( name ) ) {
			e.path = c->path;
			e.numFrames = c->clip.numFrames;
			e.duration = c->clip.duration;
			if ( c->clip.frameDuration > 0.0f )
				e.fps = 1.0f / c->clip.frameDuration;
		}
		if ( st.wontBind.contains( name ) ) {
			e.refused = true;
			e.reason = st.wontBind.value( name );
		}
		out.append( e );
	}

	out += st.refusedFiles;
	return out;
}

WwHkxListEntry WwHkxAnimHub::entry( GLView * ogl, const QString & entryName ) const
{
	const QVector<WwHkxListEntry> all = entries( ogl );
	for ( const WwHkxListEntry & e : all ) {
		if ( e.name == entryName )
			return e;
	}
	return WwHkxListEntry();
}

bool WwHkxAnimHub::has( GLView * ogl, const QString & entryName ) const
{
	const QVector<WwHkxListEntry> all = entries( ogl );
	for ( const WwHkxListEntry & e : all ) {
		if ( e.name == entryName )
			return true;
	}
	return false;
}
