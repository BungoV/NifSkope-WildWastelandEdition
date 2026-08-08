#ifndef SP_BLOCKS_H
#define SP_BLOCKS_H

#include "spellbook.h"

#include <functional>	// the Block List hover probe


// Brief description is deliberately not autolinked to class Spell
/*! \file blocks.h
 * \brief Block spells header
 *
 * All classes here inherit from the Spell class.
 */

typedef enum {
	// "nifskope"
	MIME_IDX_APP = 0,
	// "nibranch" or "niblock"
	MIME_IDX_STREAM,
	// "version"
	MIME_IDX_VER,
	// "type"
	MIME_IDX_TYPE
} CopyPasteMimeTypes;

//! Copy a branch (a block and its descendents) to the clipboard
class spCopyBranch final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Copy Branch" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	bool constant() const override final { return true; }
	QKeySequence hotkey() const override final { return QKeySequence( QKeySequence::Copy ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final;
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
};

//! Paste a branch from the clipboard
class spPasteBranch final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Paste Branch" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	// Doesn't work unless the menu entry is unique
	QKeySequence hotkey() const override final { return QKeySequence( QKeySequence::Paste ); }

	QString acceptFormat( const QString & format, const NifModel * nif );

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final;

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
};

class spDuplicateBranch final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Duplicate Branch" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return{ QKeySequence( int( Qt::CTRL ) + int( Qt::Key_D ) ) }; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final;

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
};

//! Remove a branch (a block and its descendents)
class spRemoveBranch final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove Branch" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return{ QKeySequence( int( Qt::CTRL ) + int( Qt::Key_Delete ) ) }; }

	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final;

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final;
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
};


//! Block List multi-selection copy: put the union of the given roots' branches
//! onto the clipboard in the branch format that spPasteBranch consumes.
bool copyBlockBranchesToClipboard( NifModel * nif, const QList<qint32> & roots );

//! Publish the Block List's current multi-selection (block numbers) so the branch
//! spells can act on every selected block. Call on selection change.
void setBlockListSelection( const QList<qint32> & blocks );

/*! What setBlockListSelection last published.
 *
 *  For a caller that must run a branch spell against exactly one block: the
 *  branch spells consult this selection through spellSelectionRoots, which is
 *  right for Ctrl+Delete and wrong for an operation that was asked about one
 *  thing. Save, clear, cast, restore.
 */
QList<qint32> blockListSelectionForSpells();

//! BSXFlags bits, named as the flag editor names them (spells/flags.cpp).
enum BSXFlagBits : quint32
{
	BSXF_Animated = 0x001,
	BSXF_Havok    = 0x002,		//!< "this mesh has collision at all"
	BSXF_Ragdoll  = 0x004,
	BSXF_Complex  = 0x008,
};

/*! Ensure the file's root carries a BSXFlags with at least \a bits set.
 *
 *  Measured over the stock Fallout 4 mesh tree: of the 22,496 meshes that carry
 *  a collision object, 22,496 have a BSXFlags on the ROOT's Extra Data List with
 *  bit 1 set. No exceptions, in any directory. Bit 1 is what tells the engine the
 *  mesh has collision, and nothing in NifSkope wrote it — so collision added here
 *  produced meshes the engine ignores, with no warning anywhere.
 *
 *  ORs, never assigns: the other bits are the author's. Idempotent, so a call
 *  site that fires twice costs nothing.
 *
 *  Deliberately does NOT clear the bit when collision is removed. The converse
 *  does not hold in vanilla — 71 stock meshes ship bit 1 with no collision block
 *  at all — so a spurious bit is demonstrably tolerated, while clearing one the
 *  user set by hand loses intent that cannot be recovered.
 *
 *  \return true if the model changed.
 */
bool wwEnsureRootBSXFlags( NifModel * nif, quint32 bits,
	quint32 * wasValue = nullptr, quint32 * nowValue = nullptr );

//! The blocks a branch spell should act on: the multi-selection when the clicked
//! block is part of it, otherwise just the clicked block.
QList<qint32> spellSelectionRoots( const NifModel * nif, const QModelIndex & index );

/*! Where the pointer is over the Block List, asked at the moment a spell runs.
 *
 *  Paste follows the POINTER rather than the selection: over a row it pastes into
 *  that row, over the blank space below the rows it pastes with no parent at all.
 *  Asked on demand instead of tracked, so nothing has to watch the mouse.
 *
 *  The probe returns false when the pointer is not over the block list — from a
 *  context menu, from the menu bar, from another window — and the spell then
 *  falls back to the index it was handed.
 *
 *  \param block receives the block under the pointer, or -1 for blank space.
 *  \return whether the pointer is over the block list at all.
 */
void setBlockListHoverProbe( std::function<bool( qint32 & block )> probe );
bool blockListHoverTarget( qint32 & block );

//! Add a link to the specified block to a link array
/*!
* @param nif The model
* @param iParent The block containing the link array
* @param array The name of the link array
* @param link A reference to the block to insert into the link array
*/
bool addLink( NifModel * nif, const QModelIndex & iParent, const QString & array, int link );

//! Remove a link to a block from the specified link array
/*!
* @param nif The model
* @param iParent The block containing the link array
* @param array The name of the link array
* @param link A reference to the block to remove from the link array
*/
void delLink( NifModel * nif, const QModelIndex & iParent, QString array, int link );

//! Link one block to another
/*!
 * @param nif The model
 * @param index The block to link to (becomes parent)
 * @param iBlock The block to link (becomes child)
 */
void blockLink( NifModel * nif, const QModelIndex & index, const QModelIndex & iBlock );

/*! Follow a renamed NiAVObject through everything that refers to it BY NAME.
 *
 *  A scene object's name is not just a label: NiDefaultAVObjectPalette entries
 *  and every NiControllerSequence's Controlled Blocks address nodes by string.
 *  Rename the node alone and the animation silently stops binding to it.
 *
 *  Palette entries are matched by LINK first and only then by old name, because
 *  the link is authoritative and still works on a palette whose names have
 *  already drifted. Controlled blocks carry no link, so name is all there is.
 *
 *  There were two copies of this — one behind the F2 / double-click editor, one
 *  behind the Rename (sync animation)… spell — and they were identical
 *  line-for-line, which is the only reason merging them is a no-op rather than a
 *  behaviour change. Two identical copies is one copy that has not drifted yet.
 *
 *  \return how many references were rewritten.
 */
int wwPropagateNodeName( NifModel * nif, int nodeNumber, const QString & oldName, const QString & newName );

/*! What a re-parent does to the moved block's transform.
 *
 *  A NIF has only NiNode children, so Blender's two separate gestures —
 *  move-to-collection (organisational, nothing appears to move) and parenting
 *  (transform-level) — collapse into one operation here, and the distinction has
 *  to be re-cast as what happens to the transform. That is what these are.
 */
enum class WwReparentMode
{
	//! Compensate the local transform, so the block stays where it is in the
	//! world. Blender's move-to-collection semantic.
	PreserveWorld,
	//! Leave the local transform alone, so the block snaps into the new parent's
	//! space. Right for attaching collision to a bone.
	KeepLocal,
	//! Add the child link and KEEP the old one, so the block has two parents.
	//! A real NIF capability, and the closest thing to Blender's link.
	Link
};

/*! Why re-parenting `block` under `newParent` is refused — empty if it is legal.
 *
 *  Separate from the operation so a drag can ask before the drop, and phrased for
 *  display: a silent no-op is what this kept getting reported for.
 */
QString wwReparentRefusal( const NifModel * nif, qint32 block, qint32 newParent, WwReparentMode mode,
	int position = -1 );

//! WW_BLOCKDND_TEST: which field a typed drop would write, as text, or "<none>".
//! So a harness can see the CHOICE and not only its effect.
QString wwFieldAcceptingName( const NifModel * nif, qint32 owner, qint32 block );

//! Can `owner` hold `block` through a named field rather than in Children? What
//! gives a row an inside to aim at while a non-scene-object is being dragged.
bool wwCanTakeTypedChild( const NifModel * nif, qint32 owner, qint32 block );

/*! Re-parent blocks under `newParent`, as ONE undo step.
 *
 *  Every world transform is read BEFORE anything is written. Node transforms are
 *  stored as locals, so writing one parent moves its children — reading up front
 *  places each block against the hierarchy as it was, not as the loop has left it
 *  half-way through. That is what makes dragging a parent and its own child in the
 *  same selection come out right.
 *
 *  Refused blocks are skipped, not fatal: a mixed multi-selection moves what it
 *  can and reports the rest.
 *
 *  `position` is where in the new parent's `Children` the blocks land, which is
 *  how dropping BETWEEN two rows reorders: -1 appends, which is what dropping on
 *  a row does. It is resolved against the array as the user saw it, so removing
 *  a block that sat above the insertion point shifts it back by one, and several
 *  blocks land consecutively in the order given.
 *
 *  \param refusals if given, receives one line per skipped block.
 *  \return how many blocks moved.
 */
int wwReparentBlocks( NifModel * nif, const QList<qint32> & blocks, qint32 newParent,
	WwReparentMode mode, QStringList * refusals = nullptr, int position = -1,
	const QList<qint32> & fromParents = QList<qint32>() );

#endif // SP_BLOCKS_H
