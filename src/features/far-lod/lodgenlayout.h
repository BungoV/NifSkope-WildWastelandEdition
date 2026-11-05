#ifndef LODGENLAYOUT_H
#define LODGENLAYOUT_H

#include <QString>


/*! THE ONE ROOT EVERY FO4CS-TARGET FILE GOES UNDER.
 *
 * bungo, 19:2x: "shouldn't all these files sit under a new single directory
 * then?" -- then 19:3x, and this is the ruling: "The folder should be called
 * FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?"
 *
 * Until today the FO4CS-target bake scattered its own file types across three
 * of the engine's folders -- `Terrain\` for the landscape file, the texture
 * pyramid and the native pair, `Textures\Lodgen\` for the aggregate sheets and
 * the impostor cards, `meshes\terrain\` for the manifest sidecars -- because
 * each writer grew out of the legacy pass that used to share those folders.
 * None of them is an engine path for OUR types: nothing in Fallout 4 reads a
 * `.lodl`, a `.lodt`, a `.lodo` or a `.lodi`, so nothing in Fallout 4 cares
 * where they sit. FO4CS does, and one folder is easier to ship, to diff and to
 * delete than three.
 *
 * THE LAYOUT, relative to the mod folder (which IS the `Data` folder):
 *
 *   FO4CSLOD/<ws>/<ws>.lodl              the landscape file
 *   FO4CSLOD/<ws>/<ws>.VT.<dim>.lodt     the terrain texture pyramid, one a level
 *   FO4CSLOD/<ws>/<ws>.VT.lodm           its index
 *   FO4CSLOD/<ws>/<ws>.lodo + .lodi      the native object library and instances
 *   FO4CSLOD/<ws>/Objects/...            the texture, atlas and card ARRAYS
 *   FO4CSLOD/<ws>/Aggregate/...          the ring-3 aggregate impostor sheets
 *   FO4CSLOD/<ws>/<chunk>.BTO.manifest.txt   the kept sidecars
 *   FO4CSLOD/Cards/<id>_oct.*            the impostor cards: PER TREE, so they
 *                                        sit beside the worldspaces, not inside
 *                                        one -- a maple is the same maple in
 *                                        the Commonwealth and on Far Harbour
 *   FO4CSLOD/<ws>/<ws>.lodb              the bake ledger (its spot;
 *                                        LAYOUT1 writes nothing there)
 *
 * WHAT DOES NOT MOVE. The stock-engine target is untouched -- its `.BTR`,
 * `.BTO`, atlas and chunk sheets are engine paths that the engine composes
 * itself, and their byte identity is a hard gate. So is the far shadow
 * heightmap `Textures\Terrain\<ws>\<ws>.HeightMap.*.dds`: a SHIPPED FO4CS
 * reader composes that path from loose files, and moving it is a change on
 * their side and bungo's call. Every READ path is untouched as well: vanilla's
 * `Textures\Terrain`, `Textures\LOD`, `meshes\LOD` and the MNAM models are
 * inputs, and bungo's standing ruling (2026-09-12 18:3x) is about what we
 * WRITE -- "Except the data we're reading from for the bakes".
 *
 * EVERY writer composes its directory HERE and nowhere else. A second spelling
 * of the folder name anywhere in `src/` is what `tests/features/far-lod/lodgen_layout.sh`
 * leg (e) greps for, so this file is the only place the string exists.
 */

//! The folder name itself, once. `FO4CSLOD`.
QString lodgenFo4csFolderName();

//! `<modFolder>/FO4CSLOD` -- the root, in the shell's own separator.
QString lodgenFo4csRoot( const QString & modFolder );

//! `<modFolder>/FO4CSLOD/<ws>` -- everything that belongs to one worldspace.
QString lodgenFo4csWorldDir( const QString & modFolder, const QString & ws );

//! `<modFolder>/FO4CSLOD/Cards` -- the impostor cards, shared by every worldspace.
QString lodgenFo4csCardDir( const QString & modFolder );

//! The GAME-RELATIVE spelling written INTO a file, with backslashes and no leading `Data\`: `FO4CSLOD\<ws>`.
QString lodgenFo4csGameWorldPath( const QString & ws );

//! The game-relative card folder: `FO4CSLOD\Cards`.
QString lodgenFo4csGameCardPath();


//! THE LAYOUT CENSUS CLAUSE -- read back from the paths actually written.
void lodgenClearLayoutCensus();
void lodgenNoteLayoutFile( const QString & path );
//! Every FILE directly inside `dir`, read off the disk after the pass that filled it.
void lodgenNoteLayoutDir( const QString & dir );
//! `layout <abs root>, N file(s), 0 outside` / `layout n/a (no FO4CS-target file written)`
QString lodgenLayoutCensusLine();
//! The absolute root the noted files landed under, or empty when none was noted.
QString lodgenLayoutCensusRoot();

#endif // LODGENLAYOUT_H
