/* THE ONE LOADER for Havok animation clips (.hkx), and the list the Animation
   Manager dock shows them in.

   bungo's rulings, 2026-09-10, verbatim:
     "in animation workspace, add an option to load a hkx file with animation,
      then they get added to the animations list, and if there's rigged geometry
      with nodes / bone names that match, they play"
     "So either win exporer pick or drag and drop"
     "Also, add support of these to the timeline workspace"

   WHY A HUB AND NOT THREE CALL SITES. There are three ways to ask for the same
   thing -- the button in the render toolbar's Animation panel, the button in
   the Animation Manager dock, and dropping a file on the window -- and lane
   HKX2 shipped the first of them with the load, the activation, the summary
   line and the signal spelled out inline. Two more copies of that is two more
   places for the three to drift, and drift here is invisible: a drop that
   loads but does not activate looks exactly like a clip that refused. Every
   entry point calls loadFiles() and shows the sentence it returns.

   WHAT THE LIST HOLDS. `Scene::animGroups` holds the names of clips that
   LOADED; it cannot hold the ones that did not, and a file that vanishes
   without a trace is the worst answer a drop can give. So the hub keeps, per
   scene, two small registers beside the playback's own clips:

     * files that could not be read at all, or that carry no animation
       (skeleton.hkx is the common one: it is a skeleton, not a clip), and
     * clips that loaded and then REFUSED TO BIND, because no bone they name is
       a node in the open NIF.

   Both appear in the dock's list marked refused, with the reason in words.
   The second register is written only after `setActive` has been called and
   has come back with the clip not active -- a measurement, never a guess made
   before anything was tried (CONSTITUTION rule 4).

   Lane HKX3, 2026-09-10. */

#ifndef HKXANIMUI_H
#define HKXANIMUI_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class GLView;
class Scene;
struct HkxClipEntry;   //!< src/hkxplayback.h -- one loaded clip (lane UINOTES1's copy/paste)

//! One row of the animations list that came from a file rather than a block.
struct WwHkxListEntry
{
	QString name;			//!< the entry name (also the Scene::animGroups name)
	QString path;			//!< the file it came from
	int numFrames = 0;		//!< 0 when the file never produced a clip
	float fps = 0.0f;		//!< 1 / frameDuration; 60 for the Mixamo fixture, 30 vanilla
	float duration = 0.0f;	//!< seconds
	bool refused = false;	//!< shown marked, with `reason` in the summary line
	QString reason;			//!< the refusal, in words; empty on a good clip

	//! What the list shows: "Jog · 93 frames @ 60 fps" or "skeleton · refused".
	QString label() const;
};

/*! Every entry point that loads a .hkx as an animation, and the refusals.
 *
 *  One instance for the whole application (the clips themselves live in each
 *  Scene's own HkxPlayback; this only holds what that cannot hold). It owns no
 *  widgets and no OpenGL state.
 */
class WwHkxAnimHub final : public QObject
{
	Q_OBJECT

public:
	static WwHkxAnimHub * instance();

	//! Is this a file we would try to load as an animation? Extension only --
	//! the real answer is the reader's, and a wrong guess here is a refusal
	//! with a reason rather than a silently ignored drop.
	static bool isAnimationFile( const QString & path );
	//! The one file-dialog filter, so the button and the dock cannot drift.
	static QString fileDialogFilter();
	//! Of a list of dropped paths, the ones that are animation files.
	static QStringList animationFilesIn( const QStringList & paths );

	/*! THE loader. Every button and the drop path go through here.
	 *
	 *  Returns ONE SENTENCE for the panel's summary line: the mapping summary
	 *  when something loaded, or the refusal when nothing did. Never empty.
	 *  `refusal` (optional) says which of the two it is, so the label can be
	 *  painted in the danger colour without parsing English.
	 */
	QString loadFiles( GLView * ogl, const QStringList & paths, bool activateFirst,
					   bool * refusal = nullptr );

	/*! Select a clip and REPORT whether it actually plays.
	 *
	 *  Returns true when the clip is now posing the scene. False marks it
	 *  refused in the list with the playback's own sentence as the reason --
	 *  which is a measurement of setActive, not a prediction.
	 */
	bool activate( GLView * ogl, const QString & entryName );

	//! Drop one entry: a loaded clip (HkxPlayback::unload) or a refused row.
	bool unload( GLView * ogl, const QString & entryName );
	//! Drop every clip and every refused row. Returns how many rows went.
	int unloadAll( GLView * ogl );

	/* ---- lane UINOTES1, bungo's ruling 5 of 2026-09-12: "For these
	   animations, I should be able to use a shortcut to delete, copy, paste,
	   etc. them, same goes with reordering with a drag and drop, same goes
	   with right clicking and selecting each such option."

	   The list widget, its shortcuts, its right-click menu and its drop all
	   end HERE, so the three ways in cannot drift apart, and the scene's own
	   animations list is kept in the same order by HkxPlayback. Each returns
	   a refusal sentence, or "" when it was done. */
	QString duplicateEntry( GLView * ogl, const QString & entryName, QString * newName = nullptr );
	QString renameEntry( GLView * ogl, const QString & entryName, const QString & newName );
	//! Paste: a copy of an entry taken earlier, placed at `atIndex`.
	QString pasteEntry( GLView * ogl, const HkxClipEntry & e, int atIndex, QString * newName = nullptr );
	//! The loaded clips in this order (a drag-and-drop reorder). True when it moved.
	bool setEntryOrder( GLView * ogl, const QStringList & entryNames );
	//! The clip behind a row, for a copy; null for a refused row or a sequence.
	const HkxClipEntry * clipEntry( GLView * ogl, const QString & entryName ) const;
	//! Where a loaded clip sits among the loaded clips, or -1.
	int clipIndex( GLView * ogl, const QString & entryName ) const;

	//! Root motion, off by default, shared by every control that shows it.
	void setRootMotion( GLView * ogl, bool on );
	bool rootMotion( GLView * ogl ) const;

	//! Every file-borne row the animations list must show, load order first.
	QVector<WwHkxListEntry> entries( GLView * ogl ) const;
	//! The same, by name (invalid name -> a default-constructed entry).
	WwHkxListEntry entry( GLView * ogl, const QString & entryName ) const;
	//! Is this name one of ours rather than a NiControllerSequence's?
	bool has( GLView * ogl, const QString & entryName ) const;

	//! The last sentence produced, and whether it was a refusal.
	const QString & sentence() const { return lastSentence; }
	/*! THE SAME THING IN A FEW WORDS -- what a pinned LABEL shows (lane UI6,
	 *  2026-09-11; bungo, over the old dock's binding paragraph: "look at all
	 *  this text clutter"). sentence() is what its tooltip carries, so the
	 *  names of the bones that did not bind are folded, never lost. Falls back
	 *  to the full sentence when the last thing that happened had no short form
	 *  -- a named fallback, never an empty label. */
	const QString & sentenceShort() const
	{
		return lastShort.isEmpty() ? lastSentence : lastShort;
	}
	bool sentenceIsRefusal() const { return lastWasRefusal; }

signals:
	//! A clip was loaded, refused, activated or unloaded: rebuild the lists.
	void clipsChanged();
	//! The summary-or-refusal line changed.
	void sentenceChanged( const QString & text, bool refusal );

private:
	explicit WwHkxAnimHub( QObject * parent = nullptr ) : QObject( parent ) {}

	//! What one scene's registers hold. Keyed by Scene *, which lives as long
	//! as its GLView does -- Scene::clear() empties a scene, it does not
	//! destroy it, so a clip loaded before a NIF survives the NIF opening.
	struct SceneState
	{
		//! files that never produced a clip, in the order they were dropped
		QVector<WwHkxListEntry> refusedFiles;
		//! clip name -> the sentence setActive refused with
		QHash<QString, QString> wontBind;
		bool rootMotion = false;
	};

	static Scene * sceneOf( GLView * ogl );
	void say( const QString & text, bool refusal, const QString & shortText = QString() );

	QHash<const Scene *, SceneState> states;
	QString lastSentence;
	QString lastShort;		//!< the few-word form of lastSentence, "" when there is none
	bool lastWasRefusal = false;
};

/*! WW_HKXANIM_UI_TEST: lane HKX3's gates, run inside the real application.
 *  Defined in src/hkxanimuitest.cpp so this lane's footprint in
 *  nifskope_ui.cpp is one line. Does nothing unless WW_HKXANIM_UI_TEST is set.
 */
void wwHkxAnimUiHarness( class NifSkope * skope );

#endif // HKXANIMUI_H
