/* The FILES tab — what the left column's third page lists, and what happens
   when a non-NIF row in it is opened.

   bungo's ruling, 2026-09-10, verbatim: "add hkx files to the NIFs tab, search
   for them in already set game folders ... rename 'available NIFs' to
   'available files', and rename NIFs tab to 'Files', then also rename Loaded
   NIFs to 'Loaded Files', Basically replace mentions about nifs to generic
   'files', because now we'll be able to browse and open not just nifs, well, we
   already can, with stuff like bto or btr".

   The RENAMES are string edits in src/nifskope.cpp, src/nifskope_ui.cpp and
   src/ui/nifskope.ui and live nowhere else. What needs code, and therefore
   lives here, is the other half of his sentence:

     * WHICH EXTENSIONS the browser indexes. The tree is filled by feeding every
       configured resource root (folder or .ba2 alike) to BA2File with ONE
       predicate, so archives and loose folders are the same code path and the
       extension list is the whole policy -- see wwFilesTabAccepts().

     * WHAT AN .hkx ROW DOES when it is opened. A Havok clip is not a document:
       it has no scene graph of its own and replacing the open model with it
       would be meaningless. It is loaded as an ANIMATION onto the model that is
       already open, through the same HkxPlayback::load() call the render
       toolbar's "Load Animation (.hkx)..." button makes, and it answers with the
       same summary sentence. With nothing open to play it on, it REFUSES IN
       WORDS and loads nothing (CONSTITUTION rule 10: a refusal states its
       reason).

   Nothing here owns a widget. The two callers are src/nifskope.cpp (the tree
   and the Loaded-files list) and src/filestabtest.cpp (the gates), and both are
   hooked up by scratchpad/filestab_20260910/hookup.py.

   Lane FILESTAB, 2026-09-10. */

#ifndef FILESTAB_H
#define FILESTAB_H

#include <QByteArray>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <string_view>

class GLView;

/*! Every extension the Files tab indexes, lower case, with the leading dot.
 *
 *  `.nif` `.bto` `.btr` were the three the NIF browser had. `.hkx` is bungo's
 *  ruling; `.gltf` is the interchange format lane HKX4b writes and HKX5b reads;
 *  `.lodl` and `.lodt` are our own land and terrain-texture containers.
 *
 *  NOTE, measured and stated rather than assumed: the tree additionally keeps
 *  only paths that start with `meshes/` (src/nifskope.cpp, the folder walk needs
 *  a common root), so `.hkx` and `.gltf` are reachable -- FO4 keeps every clip
 *  under `meshes/actors/<actor>/animations/` -- while `.lodl` and `.lodt` are
 *  accepted by this predicate but are not shipped under `meshes/` by anything,
 *  and are opened from disk instead.
 */
const QStringList & wwFilesTabExtensions();

//! The BA2File predicate: does the Files tab index this archive/loose path?
bool wwFilesTabAccepts( const std::string_view & path );

//! Is this path a Havok animation clip -- a row that plays rather than opens?
bool wwFilesTabIsAnimation( const QString & path );

/*! The mark a loaded ANIMATION carries in the Loaded-files list.
 *
 *  A play triangle, so a clip cannot be mistaken for a document at a glance.
 *  Painted from wwSkinColor("toggle") through the shared skin table, never a
 *  colour literal (nifskope-ww-panel-style). Built once and cached.
 */
QIcon wwFilesTabClipIcon();

//! What wwFilesTabOpenAnimation() did, and the one sentence that says so.
struct WwAnimOpen
{
	//! true only when a clip was decoded AND added to the animations list
	bool loaded = false;
	//! its entry name in Scene::animGroups (empty unless `loaded`)
	QString name;
	//! ALWAYS non-empty: HkxPlayback::summary(), or the refusal, in words
	QString sentence;
	//! named nodes the open model offered -- 0 is the refusal's reason
	int nodesInNif = 0;
};

/*! Load one .hkx as an animation onto whatever model is open.
 *
 *  `diskPath` is used when the row is a loose file; for a row inside a BA2,
 *  pass the extracted `bytes` and a `label` (the archive-relative path) and the
 *  clip is staged into a temporary file, because lane HKX1's reader takes a
 *  path. `label` is only ever used for the message.
 *
 *  Refuses, without loading anything, when the scene offers no named nodes --
 *  which is exactly "no model is loaded" measured rather than guessed, through
 *  HkxPlayback::mapNames()'s own node count.
 */
WwAnimOpen wwFilesTabOpenAnimation( GLView * ogl, const QString & diskPath,
									const QByteArray & bytes = QByteArray(),
									const QString & label = QString() );

/*! Drop one loaded clip and restore every node it posed.
 *
 *  HkxPlayback::unload() has existed since lane HKX2 and nothing called it
 *  (that lane's report, section 6 item 2). This is the Loaded-files row action.
 *  Returns true when the clip was there and is gone.
 */
bool wwFilesTabUnloadAnimation( GLView * ogl, const QString & clipName );

/*! WW_FILESTAB_TEST: this lane's gates, run inside the real application.
 *  Defined in src/filestabtest.cpp; called once from the UI setup, so this
 *  lane's footprint in nifskope_ui.cpp is one line. Does nothing unless
 *  WW_FILESTAB_TEST is set. */
void wwFilesTabHarness( class NifSkope * skope );

#endif
