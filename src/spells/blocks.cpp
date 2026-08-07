#include "blocks.h"
#include "mesh.h"
#include "wwblocksummary.h"
#include "nifsnapshot.h"	// wwReparentBlocks is one undo step

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm> // std::stable_sort

// Brief description is deliberately not autolinked to class Spell
/*! \file blocks.cpp
 * \brief Block manipulation spells
 *
 * All classes here inherit from the Spell class.
 *
 * spRemoveBranch is declared in \link spells/blocks.h \endlink so that it is accessible to spCombiTris.
 */

const char * B_ERR = QT_TR_NOOP( "%1 failed with errors." );
const char * REF_MSG = QT_TR_NOOP( "Found %1 References" );

// Clipboard MIME names must remain ASCII on Windows. Unicode separators were
// not reliably advertised to another open NifSkope window, causing Paste
// Branch to fail isApplicable() and disappear from the context menu.
const char * NAME_SEP = "˃"; // Existing serialized parent-name separator
const char * STR_BR = "nifskope/nibranch/%1";
const char * STR_BL = "nifskope/niblock/%1/%2";

static QStringList splitCopyPasteMime( const QString & format )
{
	// Read the restored ASCII format plus the format emitted by interim builds,
	// so a branch already on the clipboard can still be pasted after upgrade.
	for ( const QString & separator : { QStringLiteral( "/" ), QStringLiteral( "\u02C2" ) } ) {
		QStringList split = format.split( separator );
		if ( split.value( MIME_IDX_APP ) == QLatin1String( "nifskope" )
			&& ( split.value( MIME_IDX_STREAM ) == QLatin1String( "nibranch" )
				|| split.value( MIME_IDX_STREAM ) == QLatin1String( "niblock" ) ) )
			return split;
	}
	return {};
}


// Since nifxml doesn't track any of this data...

//! The valid controller types for each block
QMultiMap<QString, QString> ctlrMapping = {
	{ "NiObjectNET", "NiExtraDataController" },
	{ "NiAVObject", "NiControllerManager" },
	{ "NiAVObject", "NiVisController" },
	{ "NiAVObject", "NiTransformController" },
	{ "NiAVObject", "NiMultiTargetTransformController" },
	{ "NiParticles", "NiPSysModifierCtlr" },
	{ "NiParticles", "NiPSysUpdateCtlr" },
	{ "NiParticles", "NiPSysResetOnLoopCtlr" },
	{ "NiGeometry", "NiGeomMorpherController" },
	{ "NiLight", "NiLightColorController" },
	{ "NiLight", "NiLightDimmerController" },
	{ "NiMaterialProperty", "NiAlphaController" },
	{ "NiMaterialProperty", "NiMaterialColorController" },
	{ "NiTexturingProperty", "NiFlipController" },
	{ "NiTexturingProperty", "NiTextureTransformController" },
	// New Particles
	{ "NiPSParticleSystem", "NiPSEmitterCtlr" },
	{ "NiPSParticleSystem", "NiPSForceCtlr" },
	{ "NiPSParticleSystem", "NiPSResetOnLoopCtlr" },
	// New Geometry
	{ "NiMesh", "NiMorphWeightsController" },
};

//! The valid controller types for each block, Bethesda-only
QMultiMap<QString, QString> ctlrMappingBS = {
	// OB+
	{ "NiAVObject", "BSProceduralLightningController" },
	{ "NiAlphaProperty", "BSNiAlphaPropertyTestRefController" },
	{ "NiCamera", "BSFrustumFOVController" },
	{ "NiNode", "bhkBlendController" },
	{ "NiNode", "NiBSBoneLODController" },
	// FO3
	{ "BSShaderPPLightingProperty", "BSRefractionFirePeriodController" },
	{ "BSShaderPPLightingProperty", "BSRefractionStrengthController" },
	{ "NiMaterialProperty", "BSMaterialEmittanceMultController" },
	// SK+
	{ "NiNode", "BSLagBoneController" },
	{ "BSEffectShaderProperty", "BSEffectShaderPropertyColorController" },
	{ "BSEffectShaderProperty", "BSEffectShaderPropertyFloatController" },
	{ "BSLightingShaderProperty", "BSLightingShaderPropertyColorController" },
	{ "BSLightingShaderProperty", "BSLightingShaderPropertyFloatController" },
	// FO4
	{ "NiLight", "NiLightRadiusController" },
};

//! Blocks that are never used beyond 10.1.0.0
QStringList legacyOnlyBlocks = {
	"NiBone",
	"NiImage",
	"NiRawImageData",
	"NiParticleModifier",
	"NiParticleSystemController",
	"NiTriShapeSkinController",
	"NiEnvMappedTriShape",
	"NiEnvMappedTriShapeData",
	"NiTextureProperty",
	"NiMultiTextureProperty",
	"NiTransparentProperty",
	// Morrowind
	"AvoidNode",
	"RootCollisionNode",
	"NiBSAnimationNode",
	"NiBSParticleNode"
};

//! Blocks that store data for NiTimeControllers
QStringList animationData = {
	"NiPosData",
	"NiRotData",
	"NiBoolData",
	"NiFloatData",
	"NiColorData",
	"NiTransformData",
	"NiKeyframeData",
	"NiMorphData",
	"NiUVData",
	"NiVisData",
	"NiBSplineData",
	"NiBSplineBasisData",
	"NiDefaultAVObjectPalette"
};

//! The interpolators that return true for NiInterpolator::IsBoolValueSupported()
QStringList boolValue = {
	"NiBoolInterpolator",
	"NiBlendBoolInterpolator"
};
//! The interpolators that return true for NiInterpolator::IsFloatValueSupported()
QStringList floatValue = {
	"NiFloatInterpolator",
	"NiBlendFloatInterpolator",
	"NiBSplineFloatInterpolator"
};
//! The interpolators that return true for NiInterpolator::IsPoint3ValueSupported()
QStringList point3Value = {
	"NiPoint3Interpolator",
	"NiBlendPoint3Interpolator",
	"NiBSplinePoint3Interpolator"
};
//! The interpolators that return true for NiInterpolator::IsTransformValueSupported()
QStringList transformValue = {
	"NiTransformInterpolator",
	"NiBlendTransformInterpolator",
	"NiBlendAccumTransformInterpolator",
	"NiBSplineTransformInterpolator",
	"NiPathInterpolator",
	"NiLookAtInterpolator"
};

//! The kind of interpolator values supported on each controller
QMultiMap<QString, QStringList> interpMapping =
{
	{ "NiBoolInterpController", boolValue },
	{ "NiFloatInterpController", floatValue },
	{ "NiPoint3InterpController", point3Value },
	{ "NiFloatExtraDataController", floatValue },
	{ "NiFloatsExtraDataController", floatValue },
	{ "NiFloatsExtraDataPoint3Controller", point3Value },
	{ "NiTransformController", transformValue },
	{ "NiMultiTargetTransformController", transformValue },
	{ "NiPSysEmitterCtlr", floatValue },      // Interpolator
	{ "NiPSysEmitterCtlr", boolValue },       // Visibility Interpolator
	{ "NiPSysModifierBoolCtlr", boolValue },
	{ "NiPSEmitterFloatCtlr", floatValue },
	{ "NiPSForceFloatCtlr", floatValue },
	{ "NiPSForceBoolCtlr", boolValue },
	{ "NiMorphWeightsController", floatValue },
	{ "NiGeomMorpherController", floatValue },
};

//! The string names which can appear in the block root
QStringList rootStringList =
{
	"Name",
	"Modifier Name",   // NiPSysModifierCtlr
	"File Name",       // NiSourceTexture
	"String Data",     // NiStringExtraData
	"Extra Data Name", // NiExtraDataController
	"Accum Root Name", // NiSequence
	"Look At Name",    // NiLookAtInterpolator
	"Driven Name",     // NiLookAtEvaluator
	"Emitter Name",    // NiPSEmitterCtlr
	"Force Name",      // NiPSForceCtlr
	"Mesh Name",       // NiPhysXMeshDesc
	"Shape Name",      // NiPhysXShapeDesc
	"Actor Name",      // NiPhysXActorDesc
	"Joint Name",      // NiPhysXJointDesc
	"Root Material",    // BSLightingShaderProperty FO4+
	"Behaviour Graph File", // BSBehaviorGraphExtraData
};

//! Get strings array
QStringList getStringsArray( NifModel * nif, const QModelIndex & parent,
							 const QString & arr, const QString & name = {} )
{
	QStringList strings;
	auto iArr = nif->getIndex( parent, arr );
	if ( !iArr.isValid() )
		return {};

	if ( name.isEmpty() ) {
		for ( int i = 0; i < nif->rowCount( iArr ); i++ )
			strings << nif->resolveString( nif->getIndex( iArr, i ) );
	} else {
		for ( int i = 0; i < nif->rowCount( iArr ); i++ )
			strings << nif->resolveString( nif->getIndex( iArr, i ), name );
	}

	return strings;
}
//! Set strings array
void setStringsArray( NifModel * nif, const QModelIndex & parent, QStringList & strings,
					  const QString & arr, const QString & name = {} )
{
	auto iArr = nif->getIndex( parent, arr );
	if ( !iArr.isValid() )
		return;

	if ( name.isEmpty() ) {
		for ( int i = 0; i < nif->rowCount( iArr ); i++ )
			nif->set<QString>( nif->getIndex( iArr, i ), strings.takeFirst() );
	} else {
		for ( int i = 0; i < nif->rowCount( iArr ); i++ )
			nif->set<QString>( nif->getIndex( iArr, i ), name, strings.takeFirst() );
	}
}
//! Get "Name" et al. for NiObjectNET, NiExtraData, NiPSysModifier, etc.
QStringList getNiObjectRootStrings( NifModel * nif, const QModelIndex & iBlock )
{
	QStringList strings;
	for ( int i = 0; i < nif->rowCount( iBlock ); i++ ) {
		auto iString = nif->getIndex( iBlock, i );
		if ( rootStringList.contains( nif->itemName( iString ) ) )
			strings << nif->resolveString( iString );
	}

	return strings;
}
//! Set "Name" et al. for NiObjectNET, NiExtraData, NiPSysModifier, etc.
void setNiObjectRootStrings( NifModel * nif, const QModelIndex & iBlock, QStringList & strings )
{
	for ( int i = 0; i < nif->rowCount( iBlock ); i++ ) {
		auto iString = nif->getIndex( iBlock, i );
		if ( rootStringList.contains( nif->itemName( iString ) ) )
			nif->set<QString>( iString, strings.takeFirst() );
	}
}
//! Get strings for NiMesh
QStringList getStringsNiMesh( NifModel * nif, const QModelIndex & iBlock )
{
	// "Datastreams/Component Semantics/Name" * "Num Datastreams"
	QStringList strings;
	auto iData = nif->getIndex( iBlock, "Datastreams" );
	if ( !iData.isValid() )
		return {};

	for ( int i = 0; i < nif->rowCount( iData ); i++ )
		strings << getStringsArray( nif, nif->getIndex( iData, i ), "Component Semantics", "Name" );

	return strings;
}
//! Set strings for NiMesh
void setStringsNiMesh( NifModel * nif, const QModelIndex & iBlock, QStringList & strings )
{
	auto iData = nif->getIndex( iBlock, "Datastreams" );
	if ( !iData.isValid() )
		return;

	for ( int i = 0; i < nif->rowCount( iData ); i++ )
		setStringsArray( nif, nif->getIndex( iData, i ), strings, "Component Semantics", "Name" );
}
//! Get strings for NiSequence
static const char * controlledBlockStringNames[7] = {
	"Target Name",
	"Node Name",
	"Property Type",
	"Controller Type",
	"Controller ID",
	"Interpolator ID",
	nullptr
};
QStringList getStringsNiSequence( NifModel * nif, const QModelIndex & iBlock )
{
	QStringList strings;
	auto iControlledBlocks = nif->getIndex( iBlock, "Controlled Blocks" );
	if ( !iControlledBlocks.isValid() )
		return {};

	for ( int i = 0; i < nif->rowCount( iControlledBlocks ); i++ ) {
		auto iChild = nif->getIndex( iControlledBlocks, i );
		for ( int j = 0; controlledBlockStringNames[j]; j++ ) {
			auto iString = nif->getIndex( iChild, controlledBlockStringNames[j] );
			QString s;
			if ( iString.isValid() )
				s = nif->resolveString( iString );
			strings << s;
		}
	}

	return strings;
}
//! Set strings for NiSequence
void setStringsNiSequence( NifModel * nif, const QModelIndex & iBlock, QStringList & strings )
{
	auto iControlledBlocks = nif->getIndex( iBlock, "Controlled Blocks" );
	if ( !iControlledBlocks.isValid() )
		return;

	for ( int i = 0; i < nif->rowCount( iControlledBlocks ); i++ ) {
		auto iChild = nif->getIndex( iControlledBlocks, i );
		for ( int j = 0; controlledBlockStringNames[j]; j++ ) {
			QString s = strings.takeFirst();
			auto iString = nif->getIndex( iChild, controlledBlockStringNames[j] );
			if ( iString.isValid() )
				nif->set<QString>( iString, s );
		}
	}
}

//! Builds string list for datastream
QStringList serializeStrings( NifModel * nif, const QModelIndex & iBlock, const QString & type )
{
	auto strings = getNiObjectRootStrings( nif, iBlock );
	if ( nif->inherits( type, "NiSequence" ) )
		strings << getStringsNiSequence( nif, iBlock );
	else if ( type == "NiTextKeyExtraData" )
		strings << getStringsArray( nif, iBlock, "Text Keys", "Value" );
	else if ( type == "NiMesh" )
		strings << getStringsNiMesh( nif, iBlock );
	else if ( type == "NiStringsExtraData" )
		strings << getStringsArray( nif, iBlock, "Data" );
	else if ( type == "NiMorphWeightsController" )
		strings << getStringsArray( nif, iBlock, "Target Names" );

	if ( type == "NiMesh" || nif->inherits( type, "NiGeometry" ) )
		strings << getStringsArray( nif, nif->getIndex( iBlock, "Material Data" ), "Material Name" );;

	return strings;
}

//! Consumes string list from datastream
void deserializeStrings( NifModel * nif, const QModelIndex & iBlock, const QString & type, QStringList & strings )
{
	setNiObjectRootStrings( nif, iBlock, strings );
	if ( nif->inherits( type, "NiSequence" ) )
		setStringsNiSequence( nif, iBlock, strings );
	else if ( type == "NiTextKeyExtraData" )
		setStringsArray( nif, iBlock, strings, "Text Keys", "Value" );
	else if ( type == "NiMesh" )
		setStringsNiMesh( nif, iBlock, strings );
	else if ( type == "NiStringsExtraData" )
		setStringsArray( nif, iBlock, strings, "Data" );
	else if ( type == "NiMorphWeightsController" )
		setStringsArray( nif, iBlock, strings, "Target Names" );

	if ( type == "NiMesh" || nif->inherits( type, "NiGeometry" ) )
		setStringsArray( nif, nif->getIndex( iBlock, "Material Data" ), strings, "Material Name" );
}

//! Add a link to the specified block to a link array
/*!
 * @param nif The model
 * @param iParent The block containing the link array
 * @param array The name of the link array
 * @param link A reference to the block to insert into the link array
 */
bool addLink( NifModel * nif, const QModelIndex & iParent, const QString & array, int link )
{
	QModelIndex iSize  = nif->getIndex( iParent, QString( "Num %1" ).arg( array ) );
	QModelIndex iArray = nif->getIndex( iParent, array );

	if ( iSize.isValid() && (iSize.flags() & Qt::ItemIsEnabled) ) {
		// size is valid: dynamically sized array?
		if ( iArray.isValid() && ( iArray.flags() & Qt::ItemIsEnabled ) ) {
			int numlinks = nif->get<int>( iSize );
			nif->set<int>( iSize, numlinks + 1 );
			nif->updateArraySize( iArray );
			nif->setLink( nif->getIndex( iArray, numlinks ), link );
			return true;
		}

	} else if ( iArray.isValid() && (iArray.flags() & Qt::ItemIsEnabled) ) {
		// static array, find a empty entry and insert link there
		NifItem * item = static_cast<NifItem *>( iArray.internalPointer() );

		if ( nif->isArray( iArray ) && item ) {
			for ( int c = 0; c < item->childCount(); c++ ) {
				if ( item->child( c )->getLinkValue() == -1 ) {
					nif->setLink( nif->getIndex( iArray, c ), link );
					return true;
				}
			}
		}
	}

	return false;
}

//! Remove a link to a block from the specified link array
/*!
 * @param nif The model
 * @param iParent The block containing the link array
 * @param array The name of the link array
 * @param link A reference to the block to remove from the link array
 */
void delLink( NifModel * nif, const QModelIndex & iParent, QString array, int link )
{
	QModelIndex iSize   = nif->getIndex( iParent, QString( "Num %1" ).arg( array ) );
	QModelIndex iArray  = nif->getIndex( iParent, array );
	QList<qint32> links = nif->getLinkArray( iArray ).toList();

	if ( iSize.isValid() && iArray.isValid() && links.contains( link ) ) {
		links.removeAll( link );
		nif->set<int>( iSize, links.count() );
		nif->updateArraySize( iArray );
		nif->setLinkArray( iArray, links.toVector() );
	}
}


//! Link one block to another
/*!
* @param nif The model
* @param index The block to link to (becomes parent)
* @param iBlock The block to link (becomes child)
*/
void blockLink( NifModel * nif, const QModelIndex & index, const QModelIndex & iBlock )
{
	if ( nif->isLink( index ) && nif->blockInherits( iBlock, nif->itemTempl( index ) ) ) {
		nif->setLink( index, nif->getBlockNumber( iBlock ) );
	}

	if ( nif->blockInherits( index, "NiNode" ) && nif->blockInherits( iBlock, "NiAVObject" ) ) {
		addLink( nif, index, "Children", nif->getBlockNumber( iBlock ) );

		if ( nif->blockInherits( iBlock, "NiDynamicEffect" ) ) {
			addLink( nif, index, "Effects", nif->getBlockNumber( iBlock ) );
		}
	} else if ( nif->blockInherits( index, "NiAVObject" ) && nif->blockInherits( iBlock, "NiProperty" ) ) {
		if ( !addLink( nif, index, "Properties", nif->getBlockNumber( iBlock ) ) ) {
			// Absent in Bethesda 20.2.0.7 stream version > 34
			if ( nif->inherits( nif->itemName( iBlock ), "BSShaderProperty" ) ) {
				nif->setLink( index, "Shader Property", nif->getBlockNumber( iBlock ) );
			} else if ( nif->itemName( iBlock ) == "NiAlphaProperty" ) {
				nif->setLink( index, "Alpha Property", nif->getBlockNumber( iBlock ) );
			}
		}
	} else if ( nif->blockInherits( index, "NiAVObject" ) && nif->blockInherits( iBlock, "NiExtraData" ) ) {
		addLink( nif, index, "Extra Data List", nif->getBlockNumber( iBlock ) );
	} else if ( nif->blockInherits( index, "NiObjectNET" ) && nif->blockInherits( iBlock, "NiTimeController" ) ) {
		if ( nif->getLink( index, "Controller" ) > 0 ) {
			blockLink( nif, nif->getBlockIndex( nif->getLink( index, "Controller" ) ), iBlock );
		} else {
			nif->setLink( index, "Controller", nif->getBlockNumber( iBlock ) );
			nif->setLink( iBlock, "Target", nif->getBlockNumber( index ) );
		}
	} else if ( nif->blockInherits( index, "NiTimeController" ) && nif->blockInherits( iBlock, "NiTimeController" ) ) {
		if ( nif->getLink( index, "Next Controller" ) > 0 ) {
			blockLink( nif, nif->getBlockIndex( nif->getLink( index, "Next Controller" ) ), iBlock );
		} else {
			nif->setLink( index, "Next Controller", nif->getBlockNumber( iBlock ) );
			nif->setLink( iBlock, "Target", nif->getLink( index, "Target" ) );
		}
	} else if ( nif->blockInherits( index, "NiAVObject" ) && nif->blockInherits( iBlock, "NiCollisionObject" ) ) {
		nif->setLink( index, "Collision Object", nif->getBlockNumber( iBlock ) );
	}
}

/*! A block's world transform, from the model alone.
 *
 *  Deliberately NOT read off the Scene: Node::nodeId is assigned when the Scene
 *  builds it and does not survive a renumber, so a scene lookup after a
 *  structural edit reads through a stale id. The model is the thing being
 *  written, so it is also the thing to measure against.
 *
 *  `guard` bounds the walk. getParent() searches childLinks, and a file that
 *  already contains a cycle would otherwise hang here rather than being reported.
 */
static Transform wwWorldTransform( const NifModel * nif, qint32 block, int guard = 256 )
{
	if ( !nif || block < 0 || guard <= 0 )
		return Transform();
	const QModelIndex iBlock = nif->getBlockIndex( block );
	if ( !iBlock.isValid() )
		return Transform();
	const Transform local( nif, iBlock );
	const int parent = nif->getParent( block );
	return parent < 0 ? local : wwWorldTransform( nif, parent, guard - 1 ) * local;
}

/*! EVERY block whose Children array holds this one, not just the first.
 *
 *  NifModel::getParent stops at the lowest-numbered match, which is all a tree
 *  normally needs — but Link mode deliberately gives a block two parents, and a
 *  later move that unlinked only one of them left the block still parented in
 *  two places while reporting that it had moved.
 */
static QList<qint32> wwParentsOf( const NifModel * nif, qint32 block )
{
	QList<qint32> parents;
	if ( !nif )
		return parents;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		if ( nif->getLinkArray( nif->getBlockIndex( b ), "Children" ).contains( block ) )
			parents.append( b );
	return parents;
}

/*! Every LINK FIELD anywhere in the file that points at this block.
 *
 *  `Children` is how a NiNode owns a scene object, and it is the only ownership
 *  the drag understood — so a BSLightingShaderProperty, which its shape owns
 *  through `Shader Property`, was reported as having no parent to leave and
 *  could not be dragged out. It has an owner; the owner just does not keep it in
 *  an array called Children. Same for a BSShaderTextureSet under `Texture Set`,
 *  a controller under `Controller`, extra data in `Extra Data List`.
 *
 *  Found by walking the owner's fields rather than from a table of type-to-field
 *  names: the format has hundreds of these and a table would be wrong the first
 *  time a version differs. What the file says holds it, holds it.
 */
struct WwHolder
{
	qint32 owner = -1;
	QPersistentModelIndex field;	//!< the link cell itself
	QString array;					//!< the array's name when the cell is inside one
};

static void wwFindLinkCells( const NifModel * nif, const QModelIndex & parent, qint32 target,
	qint32 owner, const QString & inArray, QList<WwHolder> & out )
{
	for ( int r = 0; r < nif->rowCount( parent ); r++ ) {
		const QModelIndex cell = nif->index( r, 0, parent );
		if ( !cell.isValid() )
			continue;
		if ( nif->isLink( cell ) && nif->getLink( cell ) == target )
			out.append( WwHolder{ owner, QPersistentModelIndex( cell ), inArray } );
		// an array's elements are its children, so remember the array's name on
		// the way down: detaching from one is a rebuild, not a blanking
		const QString name = nif->itemName( cell );
		wwFindLinkCells( nif, cell, target, owner,
			nif->isArray( cell ) ? name : inArray, out );
	}
}

static QList<WwHolder> wwHoldersOf( const NifModel * nif, qint32 block )
{
	QList<WwHolder> holders;
	if ( !nif || !nif->isValidBlockNumber( block ) )
		return holders;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		if ( b == block )
			continue;
		wwFindLinkCells( nif, nif->getBlockIndex( b ), block, b, QString(), holders );
	}
	return holders;
}

/*! Is ANYTHING holding this block? Stops at the first one.
 *
 *  THE REFUSAL RUNS ON EVERY DRAG-MOVE, and it only ever asked whether the list
 *  was empty — but it built the whole list to find out, which walks every field
 *  of every block in the file. For a block with no Children parent, which is
 *  precisely the case this was added for, that was a full-file scan per mouse
 *  movement. NifModel::getChildLinks answers the same question per block without
 *  descending item by item, so the scan stops on the first owner and usually
 *  never starts.
 */
static bool wwHasHolder( const NifModel * nif, qint32 block )
{
	if ( !nif || !nif->isValidBlockNumber( block ) )
		return false;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		if ( b == block )
			continue;
		if ( nif->getChildLinks( b ).contains( block ) )
			return true;
	}
	return false;
}

/*! Detach \a block from everything that points at it, and say how many.
 *
 *  A single Ref is set to none. A cell inside an array is REBUILT out of it,
 *  never blanked: a blanked entry leaves the count still claiming it and the
 *  entry pointing at nothing, which is precisely the dangling link the Issue
 *  Manager reports — the same reason the Children path rebuilds.
 */
static int wwDetachHolders( NifModel * nif, qint32 block )
{
	int detached = 0;
	for ( const WwHolder & holder : wwHoldersOf( nif, block ) ) {
		if ( !holder.field.isValid() )
			continue;
		const QModelIndex cell( holder.field );
		if ( holder.array.isEmpty() ) {
			nif->setLink( cell, -1 );
			detached++;
			continue;
		}
		const QModelIndex owner = nif->getBlockIndex( holder.owner );
		const QModelIndex array = nif->getIndex( owner, holder.array );
		if ( !array.isValid() )
			continue;
		QVector<qint32> kept;
		for ( const qint32 link : nif->getLinkArray( owner, holder.array ) )
			if ( link != block && nif->isValidBlockNumber( link ) )
				kept.append( link );
		const QString counter = QStringLiteral( "Num " ) + holder.array;
		if ( nif->getIndex( owner, counter ).isValid() )
			nif->set<uint>( owner, counter, uint( kept.size() ) );
		nif->updateArraySize( owner, holder.array );
		nif->setLinkArray( owner, holder.array, kept );
		detached++;
	}
	return detached;
}

/*! The link field on \a owner that would take \a block, or an invalid index.
 *
 *  Asked of the FORMAT, not of a table: every link cell declares the type it
 *  accepts (`itemTempl`), and the model knows what inherits what. So a
 *  BSLightingShaderProperty finds `Shader Property` on a BSTriShape because that
 *  cell says BSShaderProperty and the block inherits it — and it finds nothing on
 *  an NiNode, which is the honest answer rather than a refusal that has to name
 *  every case in advance.
 *
 *  A single Ref wins over an array cell when both would take it: dropping a
 *  shader property on a shape means THAT property, replacing what is there,
 *  which is what a single-valued field means. \a arrayName comes back set when
 *  the only home is an array, because joining one is an append and a rebuild
 *  rather than a write.
 */
static QModelIndex wwFieldAccepting( const NifModel * nif, qint32 owner, qint32 block,
	QString * arrayName )
{
	if ( arrayName )
		arrayName->clear();
	if ( !nif || !nif->isValidBlockNumber( owner ) || !nif->isValidBlockNumber( block ) )
		return QModelIndex();
	const QString type = nif->itemName( nif->getBlockIndex( block ) );

	/* THE MOST SPECIFIC FIELD, NOT THE FIRST ONE THAT WOULD TAKE IT.
	 *
	 * Taking the first match put a BSLightingShaderProperty into a BSTriShape's
	 * `Skin`, because that Ref's declared type is broad enough to accept anything
	 * and it comes earlier in the block than `Shader Property` does. Measured, in
	 * exactly those words, by asking the program which field it had chosen.
	 *
	 * So candidates are scored and the best wins:
	 *   0  the field declares this exact type
	 *   1  the field is HOLDING one of these already — the replace case, and the
	 *      strongest evidence a field takes something short of naming it
	 *   2  the field declares an ancestor of it, specific enough to mean something
	 *   3  the field declares NiObject, which every block inherits and which
	 *      therefore says nothing at all
	 * A single-valued field beats an array at the same score: dropping a property
	 * on a shape means THAT property, not another entry in a list.
	 */
	QModelIndex single, inArray;
	QString arrayFound;
	int singleScore = 99, arrayScore = 99;
	std::function<void( const QModelIndex &, const QString & )> walk =
		[&]( const QModelIndex & parent, const QString & array ) {
		for ( int r = 0; r < nif->rowCount( parent ); r++ ) {
			const QModelIndex cell = nif->index( r, 0, parent );
			if ( !cell.isValid() )
				continue;
			if ( nif->isLink( cell ) ) {
				const QString takes = nif->itemTempl( cell );
				const qint32 held = nif->getLink( cell );
				const QString holds = ( held >= 0 && nif->isValidBlockNumber( held ) )
					? nif->itemName( nif->getBlockIndex( held ) ) : QString();
				int score = 99;
				if ( takes == type )
					score = 0;
				else if ( !holds.isEmpty() && ( holds == type || nif->inherits( type, holds ) ) )
					score = 1;
				else if ( !takes.isEmpty() && takes != QLatin1String( "NiObject" )
					&& nif->inherits( type, takes ) )
					score = 2;
				else if ( !takes.isEmpty() && nif->inherits( type, takes ) )
					score = 3;

				if ( score < 99 ) {
					if ( array.isEmpty() ) {
						if ( score < singleScore ) {
							singleScore = score;
							single = cell;
						}
					} else if ( score < arrayScore ) {
						arrayScore = score;
						inArray = cell;
						arrayFound = array;
					}
				}
			}
			walk( cell, nif->isArray( cell ) ? nif->itemName( cell ) : array );
		}
	};
	walk( nif->getBlockIndex( owner ), QString() );

	if ( single.isValid() && singleScore <= arrayScore )
		return single;
	if ( inArray.isValid() && arrayName )
		*arrayName = arrayFound;
	return inArray.isValid() ? inArray : single;
}

/*! WW_BLOCKDND_TEST: which field a typed drop would write, as text.
 *
 *  So a harness can see the choice rather than only its effect — "the shape's
 *  Shader Property did not change" does not say whether the wrong field was
 *  picked, the right one was picked and the write failed, or nothing was found.
 */
QString wwFieldAcceptingName( const NifModel * nif, qint32 owner, qint32 block )
{
	QString array;
	const QModelIndex cell = wwFieldAccepting( nif, owner, block, &array );
	if ( !cell.isValid() )
		return QStringLiteral( "<none>" );
	return array.isEmpty()
		? nif->itemName( cell )
		: QStringLiteral( "%1[] (array)" ).arg( array );
}

QString wwReparentRefusal( const NifModel * nif, qint32 block, qint32 newParent, WwReparentMode mode,
	int position )
{
	if ( !nif )
		return QCoreApplication::translate( "Reparent", "No file is open." );

	const QModelIndex iBlock = nif->getBlockIndex( block );
	const QModelIndex iParent = nif->getBlockIndex( newParent );

	if ( !iBlock.isValid() )
		return QCoreApplication::translate( "Reparent", "That row has no block to move." );

	/* NO PARENT AT ALL is a legal destination. Dragging a block OUT of its parent
	 * had nowhere to go: every drop resolved to some node to go into, so a
	 * top-level child could not be lifted to a root of its own — and the empty
	 * space meant "the end of the root's children", which is still inside it.
	 */
	if ( newParent < 0 ) {
		if ( mode == WwReparentMode::Link )
			return QCoreApplication::translate( "Reparent",
				"Linking needs a parent to link into." );
		// held by a typed link counts as held: a shader property's owner points at
		// it through `Shader Property`, and answering "already a root" to that was
		// simply false
		if ( wwParentsOf( nif, block ).isEmpty() && !wwHasHolder( nif, block ) )
			return QCoreApplication::translate( "Reparent",
				"%1 is already a root — it has no parent to leave." ).arg( nif->itemName( iBlock ) );
		return QString();
	}

	/* NOT A SCENE OBJECT? THEN IT GOES IN A FIELD, NOT IN Children.
	 *
	 * A BSLightingShaderProperty belongs to its shape through `Shader Property`, a
	 * BSShaderTextureSet to its property through `Texture Set`. Those are as much
	 * ownership as an array called Children is, and dropping one on a block that
	 * has a field for it means exactly what dropping a shape on a node means.
	 *
	 * Checked before the NiNode rules below, because those are about Children and
	 * would refuse every one of these on the way past.
	 */
	if ( !nif->blockInherits( iBlock, "NiAVObject" ) ) {
		if ( block == newParent )
			return QCoreApplication::translate( "Reparent", "A block cannot be its own parent." );
		QString array;
		if ( !wwFieldAccepting( nif, newParent, block, &array ).isValid() )
			return QCoreApplication::translate( "Reparent",
				"%1 has no field that takes a %2." )
				.arg( nif->itemName( iParent.isValid() ? iParent : iBlock ),
					nif->itemName( iBlock ) );
		return QString();
	}

	/* ONLY A NiNode CARRIES CHILDREN, and only a NiAVObject can be one. Both
	 * halves are checked against the file rather than assumed: "Children" is
	 * looked up as well as the type, because a version without the array would
	 * otherwise take the link silently into nothing.
	 */
	if ( !iParent.isValid() || !nif->blockInherits( iParent, "NiNode" )
		|| !nif->getIndex( iParent, "Children" ).isValid() )
		return QCoreApplication::translate( "Reparent",
			"%1 cannot take children — only a NiNode can." )
			.arg( nif->itemName( iParent.isValid() ? iParent : iBlock ) );

	if ( !nif->blockInherits( iBlock, "NiAVObject" ) )
		return QCoreApplication::translate( "Reparent",
			"%1 is not a scene object, so it cannot be a child of a node." )
			.arg( nif->itemName( iBlock ) );

	if ( block == newParent )
		return QCoreApplication::translate( "Reparent", "A block cannot be its own parent." );

	/* A descendant taking its own ancestor as parent cuts the branch out of the
	 * file and leaves a cycle behind. Walk up from the target, not down from the
	 * block: the way up is much narrower than the way down.
	 *
	 * EVERY parent on the way, not the one getParent() answers with. A block may
	 * legally have more than one — Ctrl-drop makes exactly that, and it is a real
	 * NIF capability rather than a corruption — and getParent() reports the
	 * LOWEST-NUMBERED of them. Give a child a second parent that sorts before its
	 * first, and a walk up through getParent() climbs the wrong chain, finds no
	 * cycle, and writes one into the file: measured, with the drop ALLOWED.
	 *
	 * The seen set is also what makes this terminate on a file that already
	 * contains a cycle, which is what the old fixed guard of 256 was for.
	 */
	/* ONE PASS FOR THE MAP, then the walk is free. This runs on every DragMove —
	 * the drag card asks it for what to say — so calling wwParentsOf per level
	 * would re-read every block's Children once for each step of the way up.
	 */
	QHash<qint32, QList<qint32>> parentsOf;
	const int blockCount = nif->getBlockCount();
	for ( int b = 0; b < blockCount; b++ )
		for ( const qint32 child : nif->getLinkArray( nif->getBlockIndex( b ), "Children" ) )
			parentsOf[child].append( b );

	QList<qint32> up{ newParent };
	QSet<qint32> seen;
	while ( !up.isEmpty() ) {
		const qint32 p = up.takeLast();
		if ( p < 0 || seen.contains( p ) )
			continue;
		seen.insert( p );
		if ( p == block )
			return QCoreApplication::translate( "Reparent",
				"Block %1 already sits under this one — re-parenting onto it would cut the "
				"branch out of the file." ).arg( newParent );
		up.append( parentsOf.value( p ) );
	}

	/* ALREADY A CHILD is only a refusal when nothing was asked for beyond that.
	 * With a position — a drop BETWEEN two rows — the same parent is the ordinary
	 * case: it is how a block is moved up or down among its siblings. What is
	 * refused there is landing back where it already is, because a drag that
	 * changes nothing should say so rather than push an empty undo step.
	 */
	const QVector<qint32> kids = nif->getLinkArray( iParent, "Children" );
	if ( kids.contains( block ) ) {
		if ( position < 0 )
			return QCoreApplication::translate( "Reparent", "Already a child of block %1." ).arg( newParent );
		const int was = int( kids.indexOf( block ) );
		if ( position == was || position == was + 1 )
			return QCoreApplication::translate( "Reparent", "Already in that position." );
	}

	Q_UNUSED( mode );
	return QString();
}

int wwReparentBlocks( NifModel * nif, const QList<qint32> & blocks, qint32 newParent,
	WwReparentMode mode, QStringList * refusals, int position,
	const QList<qint32> & fromParents )
{
	if ( !nif )
		return 0;

	struct Move { qint32 block; Transform world; qint32 from; };
	QVector<Move> moves;
	for ( int i = 0; i < blocks.size(); i++ ) {
		const qint32 block = blocks.at( i );
		const QString refusal = wwReparentRefusal( nif, block, newParent, mode, position );
		if ( !refusal.isEmpty() ) {
			if ( refusals )
				refusals->append( refusal );
			continue;
		}
		// which of the block's placings this is, when the caller had a row to
		// read it off; -1 means "the block itself", i.e. all of them
		moves.append( { block, wwWorldTransform( nif, block ), fromParents.value( i, -1 ) } );
	}
	if ( moves.isEmpty() )
		return 0;

	/* SIBLING ORDER, NOT SELECTION ORDER. The blocks land in the order given, and
	 * the block list gathers them from selectedIndexes(), which reports the order
	 * the selection was BUILT in — so three siblings dragged together could come
	 * out shuffled among themselves, each one individually correct. Sorted by
	 * where they actually sit: parent first, then position in that parent's
	 * Children.
	 */
	std::stable_sort( moves.begin(), moves.end(), [nif]( const Move & a, const Move & b ) {
		const int pa = nif->getParent( a.block );
		const int pb = nif->getParent( b.block );
		if ( pa != pb )
			return pa < pb;
		const QVector<qint32> kids = nif->getLinkArray( nif->getBlockIndex( pa ), "Children" );
		return kids.indexOf( a.block ) < kids.indexOf( b.block );
	} );

	// The new parent's world is read up front too, and for the same reason: if the
	// selection contains one of its ancestors, moving that ancestor first would
	// change what "the parent's space" means half-way through the loop.
	const Transform parentWorld = wwWorldTransform( nif, newParent );
	const Transform parentInv = parentWorld.inverted();

	const QString what = mode == WwReparentMode::Link
		? QCoreApplication::translate( "Reparent", "Link to parent" )
		: QCoreApplication::translate( "Reparent", "Re-parent" );

	// where the next block lands. Walks forward so a multi-block drop keeps the
	// order it was dragged in.
	int cursor = position;

	nifSnapshotOp( nif, what, [&]() {
		for ( const Move & move : std::as_const( moves ) ) {
			/* A BLOCK HELD BY A FIELD RATHER THAN BY Children.
			 *
			 * Properties, texture sets, controllers, extra data: their owner points
			 * at them through a named Ref, so leaving one means clearing that Ref
			 * and joining a new one means writing it. Neither is a Children rebuild,
			 * and none of the transform work below applies — a property has no
			 * place in the world to preserve.
			 *
			 * Dropped on a block with a single-valued field, it REPLACES what was
			 * there, which is what a single-valued field means and what dropping a
			 * shader property on a shape looks like it should do.
			 */
			if ( !nif->blockInherits( nif->getBlockIndex( move.block ), "NiAVObject" ) ) {
				if ( mode != WwReparentMode::Link )
					wwDetachHolders( nif, move.block );
				if ( newParent >= 0 ) {
					QString array;
					const QModelIndex cell = wwFieldAccepting( nif, newParent, move.block, &array );
					if ( cell.isValid() ) {
						if ( array.isEmpty() )
							nif->setLink( cell, move.block );
						else
							addLink( nif, nif->getBlockIndex( newParent ), array, move.block );
					}
				}
				continue;
			}

			/* REBUILT, not blanked. Dropping a link leaves Num Children still
			 * claiming the entry and the entry pointing at nothing, which is
			 * exactly the dangling child link the Issue Manager reports.
			 *
			 * EVERY old parent, not the first one getParent() happens to find:
			 * Link mode gives a block two parents on purpose, and unlinking only
			 * one of them left it parented in two places while reporting a move.
			 *
			 * Link mode keeps them all — that is the whole point of it.
			 */
			if ( mode != WwReparentMode::Link ) {
				/* THE INSTANCE THAT WAS PICKED UP, not every place the block sits.
				 *
				 * A block under several parents appears in the tree once per parent,
				 * and dragging one of those rows moves THAT one — the others are
				 * other placings of the same block and nothing asked about them.
				 * `from` is the row's own parent, read off the proxy row at drag
				 * start and carried in the payload.
				 *
				 * When nothing said which instance — a spell, or the Collision
				 * Manager's Set Parent, neither of which has a row — every parent is
				 * taken, because a caller with no row means the block itself.
				 */
				const QList<qint32> leaving = move.from >= 0
					? QList<qint32>{ move.from } : wwParentsOf( nif, move.block );
				for ( const qint32 oldParent : leaving ) {
					const QModelIndex from = nif->getBlockIndex( oldParent );
					QVector<qint32> kept;
					for ( const qint32 child : nif->getLinkArray( from, "Children" ) )
						if ( child != move.block && nif->isValidBlockNumber( child ) )
							kept.append( child );
					// a sibling reorder removes from the same array it is about to
					// insert into, so everything after the hole shifts back one
					if ( oldParent == newParent && cursor > 0 ) {
						const int was = int( nif->getLinkArray( from, "Children" ).indexOf( move.block ) );
						if ( was >= 0 && was < cursor )
							cursor--;
					}
					nif->set<uint>( from, "Num Children", uint( kept.size() ) );
					nif->updateArraySize( from, "Children" );
					nif->setLinkArray( from, "Children", kept );
				}
			}

			/* Unparented on purpose: the links are gone and there is nowhere to
			 * put it back, which is the whole request. NOT a `continue` — the
			 * transform still has to be written below, and with no parent the
			 * compensation is the identity, so PreserveWorld writes the block's
			 * world transform into its local and it stays exactly where it was.
			 */
			const QModelIndex iNew = nif->getBlockIndex( newParent );
			if ( newParent < 0 ) {
				// nothing to link into
			} else if ( cursor < 0 ) {
				addLink( nif, iNew, "Children", move.block );
			} else {
				QVector<qint32> kids = nif->getLinkArray( iNew, "Children" );
				kids.insert( qBound( 0, cursor, int( kids.size() ) ), move.block );
				nif->set<uint>( iNew, "Num Children", uint( kids.size() ) );
				nif->updateArraySize( iNew, "Children" );
				nif->setLinkArray( iNew, "Children", kids );
				cursor++;
			}

			/* newLocal = inverse(newParentWorld) * oldWorld. Link mode does not
			 * touch the transform: a block with two parents has no single world
			 * position to preserve, so silently rewriting its local would move it
			 * under the parent it already had.
			 */
			if ( mode == WwReparentMode::PreserveWorld ) {
				const QModelIndex iBlock = nif->getBlockIndex( move.block );
				if ( Transform::canConstruct( nif, iBlock ) )
					( parentInv * move.world ).writeBack( nif, iBlock );
			}
		}
	} );

	return moves.size();
}

//! Helper function for branch paste
static qint32 getBlockByName( NifModel * nif, const QString & tn )
{
	QStringList ls = tn.split( NAME_SEP );
	QString type = ls.value( 0 );
	QString name = ls.value( 1 );

	if ( type.isEmpty() || name.isEmpty() )
		return -1;

	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );

		if ( nif->itemName( iBlock ) == type && nif->get<QString>( iBlock, "Name" ) == name )
			return b;
	}

	return -1;
}

//! Helper function for branch copy
static void populateBlocks( QList<qint32> & blocks, NifModel * nif, qint32 block )
{
	if ( !blocks.contains( block ) )
		blocks.append( block );

	for ( const auto link : nif->getChildLinks( block ) ) {
		populateBlocks( blocks, nif, link );
	}
}

//! Remove the children from the specified block
static void removeChildren( NifModel * nif, const QPersistentModelIndex & iBlock )
{
	// Build list of child links
	QVector<QPersistentModelIndex> iChildren;
	for ( const auto link : nif->getChildLinks( nif->getBlockNumber( iBlock ) ) ) {
		iChildren.append( nif->getBlockIndex( link ) );
	}

	// Remove children of child links
	for ( const QPersistentModelIndex& iChild : iChildren ) {
		if ( iChild.isValid() && nif->getBlockNumber( iBlock ) == nif->getParent( nif->getBlockNumber( iChild ) ) ) {
			removeChildren( nif, iChild );
		}
	}

	// Remove children
	for ( const QPersistentModelIndex& iChild : iChildren ) {
		if ( iChild.isValid() && nif->getBlockNumber( iBlock ) == nif->getParent( nif->getBlockNumber( iChild ) ) ) {
			nif->removeNiBlock( nif->getBlockNumber( iChild ) );
		}
	}
}

/*! How many blocks a Remove Branch actually takes with it.
 *
 *  Mirrors removeChildren's ownership test above rather than counting child
 *  links: a child whose parent is some other block is shared, survives the
 *  removal, and counting it would overstate the loss in exactly the files where
 *  the number matters — a texture set hung off two shapes.
 */
static int countOwnedBranch( const NifModel * nif, qint32 block )
{
	int n = 1;
	for ( const auto link : nif->getChildLinks( block ) ) {
		if ( nif->getParent( link ) == block )
			n += countOwnedBranch( nif, link );
	}
	return n;
}

//! A block as the destructive warnings name it: "[12] NiNode 'Tesla_Light_03'".
static QString describeBlock( const NifModel * nif, const QModelIndex & index )
{
	const QString name = nif->get<QString>( index, "Name" );
	QString s = QString( "[%1] %2" ).arg( nif->getBlockNumber( index ) ).arg( nif->itemName( index ) );
	if ( !name.isEmpty() )
		s += QString( " '%1'" ).arg( name );
	return s;
}

//! Set values in blocks that cannot be handled in nif.xml such as inherited values
void blockDefaults( NifModel * nif, const QString & type, const QModelIndex & index )
{
	// Set Bethesda NiExtraData names to their required strings
	static QMap<QString, QString> nameMap = {
		{"BSBehaviorGraphExtraData", "BGED"},
		{"BSBoneLODExtraData", "BSBoneLOD"},
		{"BSBound", "BBX"},
		{"BSClothExtraData", "CED"},
		{"BSConnectPoint::Children", "CPT"},
		{"BSConnectPoint::Parents", "CPA"},
		{"BSDecalPlacementVectorExtraData", "DVPG"},
		{"BSDistantObjectLargeRefExtraData", "DOLRED"},
		{"BSEyeCenterExtraData", "ECED"},
		{"BSFurnitureMarker", "FRN"},
		{"BSFurnitureMarkerNode", "FRN"},
		{"BSInvMarker", "INV"},
		{"BSPositionData", "BSPosData"},
		{"BSWArray", "BSW"},
		{"BSXFlags", "BSX"},
	};

	auto iterName = nameMap.find( type );
	if ( iterName != nameMap.end() )
		nif->set<QString>( nif->getIndex( index, "Name" ), iterName.value() );
}

//! Filters a list of blocks based on version (since nif.xml lacks this data)
void blockFilter( NifModel * nif, std::list<QString>& blocks, const QString & type = {} )
{
	blocks.erase( std::remove_if( blocks.begin(), blocks.end(),
		[nif, type] ( const QString& s ) { return !nif->inherits( s, type )
			// Obsolete/Undecoded
			|| s.startsWith( QLatin1StringView("NiClod") )
			|| s.startsWith( QLatin1StringView("NiArk") )
			|| s.startsWith( QLatin1StringView("NiBez") )
			|| s.startsWith( QLatin1StringView("Ni3ds") )
			|| s.startsWith( QLatin1StringView("NiBinaryVox") )
			// Legacy
			|| ( ( (nif->inherits( s, "NiParticles" ) && !nif->inherits( s, "NiParticleSystem" ))
				   || (nif->inherits( s, "NiParticlesData" ) && !s.startsWith( QLatin1StringView("NiP") )) // NiRotating, NiAutoNormal, etc.
				   || nif->inherits( s, legacyOnlyBlocks ) )
				 && nif->getVersionNumber() > 0x0a010000 )
			// Bethesda
			|| ( (s.startsWith( QLatin1StringView("bhk") ) || s.startsWith( QLatin1StringView("hk") )
					|| s.startsWith( QLatin1StringView("BS") )
					|| s.endsWith( QLatin1StringView("ShaderProperty") )) && nif->getBSVersion() == 0 )
			// Introduced in 20.2.0.8
			|| (( s.startsWith( QLatin1StringView("NiPhysX") ) && nif->getVersionNumber() < 0x14020008 ))
			// Introduced in 20.5
			|| ( ((s.startsWith( QLatin1StringView("NiPS") ) && !s.contains( QLatin1StringView("PSys") ))
					|| (s.startsWith( QLatin1StringView("NiMesh") ) && !s.startsWith( QLatin1StringView("NiMeshP") ))
					|| s.contains( QLatin1StringView("Evaluator") )
				   ) && nif->getVersionNumber() < 0x14050000 )
			// Deprecated in 20.5
			|| ( (s.startsWith( QLatin1StringView("NiParticle") ) || s.contains( QLatin1StringView("PSys") )
					|| s.startsWith( QLatin1StringView("NiTri") ) || s.contains( QLatin1StringView("Interpolator") )
				   ) && nif->getVersionNumber() >= 0x14050000 );
		} ),
		blocks.end()
	);
}

//! Creates a menu structure for a list of blocks
QMap<QString, QMenu *> blockMenu( NifModel * nif, const std::list<QString> & blocks, bool categorize = false, bool filter = false )
{
	QMap<QString, QMenu *> map;
	auto ids = blocks;
	ids.sort();
	if ( filter )
		blockFilter( nif, ids );

	bool firstCat = false;
	for ( const QString& id : ids ) {
		QString alph( "Other" );
		QString beth = (nif->getBSVersion() == 0) ? alph : "Bethesda";
		QString hk = (nif->getBSVersion() == 0) ? alph : "Havok";

		bool alphabetized = false;
		// Group Old Particles
		if ( id.contains( "PSys" ) )
			alph = QString( "Ni&P(Sys)..." );
		// Group New Particles
		else if ( id.startsWith( "NiPS" ) )
			alph = QString( "Ni&P(S)..." );
		// Group Havok
		else if ( id.startsWith( "bhk" ) || id.startsWith( "hk" ) )
			alph = hk;
		// Group PhysX
		else if ( id.startsWith( "NiPhysX" ) )
			alph = "PhysX";
		// Group Bethesda
		else if ( id.startsWith( "BS" ) || id.endsWith( "ShaderProperty" ) )
			alph = beth;
		// Group Custom
		else if ( !id.startsWith( "Ni" )
				  || (id.startsWith( "NiBS" ) && !id.startsWith( "NiBSp" )) // Bethesda but not NiBSpline
				  || id.startsWith( "NiDeferred" ) )
			alph = "Other";
		// Alphabetize Everything else Ni
		else if ( id.startsWith( "Ni" ) ) {
			alph = QString( "Ni&" ) + id.mid( 2, 1 ) + "...";
			alphabetized = true;
		}


		// Categories
		QString cat;
		if ( !alphabetized )
			cat = ""; // Already grouped well above
		else if ( nif->inherits( id, "NiInterpolator" ) || nif->inherits( id, "NiEvaluator" )
				  || nif->inherits( id, "NiTimeController" )
				  || id.contains( "Sequence" )
				  || animationData.contains( id ) )
			cat = "NiAnimation...";
		else if ( nif->inherits( id, "NiNode" ) )
			cat = "NiNode...";
		else if ( nif->inherits( id, "NiGeometry" ) || nif->inherits( id, "NiGeometryData" )
				  || id.contains( "Skin" ) )
			cat = "NiGeometry...";
		else if ( nif->inherits( id, "NiAVObject" ) )
			cat = "NiAVObject...";
		else if ( nif->inherits( id, "NiExtraData" ) )
			cat = "NiExtraData...";
		else if ( nif->inherits( id, "NiProperty" ) )
			cat = "NiProperty...";
		else if ( nif->inherits( id, "NiObject" ) )
			cat = "NiObject...";

		if ( !map.contains( alph ) )
			map[alph] = new QMenu( alph );

		map[alph]->addAction( id );

		if ( categorize && !cat.isEmpty() ) {
			if ( !firstCat ) {
				// Use NiAAA to place it alphabetically between NiZBu and NiA[a-z]
				// which will split the alphabetization and categorization.
				map["NiAAA"] = new QMenu( "" );
				firstCat = true;
			}

			if ( !map.contains( cat ) )
				map[cat] = new QMenu( cat );

			map[cat]->addAction( id );
		}
	}

	return map;
}

static std::list< QString > qStringListToStdList( const QStringList & v )
{
	std::list< QString >	tmp;
	for ( const auto & i : v )
		tmp.push_back( i );
	return tmp;
}

//! Insert an unattached block
class spInsertBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Insert" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		Q_UNUSED( nif );
		return ( !index.isValid() || !index.parent().isValid() );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMenu menu;
		menu.addSection( tr( "Alphabetical" ) );
		for ( QMenu * m : blockMenu( nif, qStringListToStdList( NifModel::allNiBlocks() ), true, true ) ) {
			if ( m->title().isEmpty() )
				menu.addSection( tr( "Categories" ) );
			else if ( m->actions().size() == 1 )
				menu.addAction( m->actions().at( 0 ) );
			else
				menu.addMenu( m );
		}

		QAction * act = menu.exec( QCursor::pos() );

		if ( act ) {
			// insert block
			QModelIndex newindex = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );

			// Set values that can't be handled by defaults in nif.xml
			blockDefaults( nif, act->text(), newindex );

			// return index to new block
			return newindex;
		}

		return index;
	}
};

REGISTER_SPELL( spInsertBlock )

//! Attach a Property to a block
class spAttachProperty final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach Property" ); }
	QString group() const override { return Spell::tr( "Add" ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( nif->itemStrType( index ) != "NiBlock" )
			return false;

		if ( nif->getUserVersion() < 12 )
			return nif->blockInherits( index, "NiAVObject" ); // Not Skyrim

		// Skyrim and later
		return nif->blockInherits( index, "NiGeometry" ) || nif->blockInherits( index, "BSTriShape" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMenu menu;
		QStringList ids = nif->allNiBlocks();
		ids.sort();
		for ( const QString& id : ids ) {
			if ( (nif->blockInherits(index, "NiGeometry") || nif->blockInherits(index, "BSTriShape")) && nif->getBSVersion() > 34 ) {
				if ( !(id == "BSLightingShaderProperty" || id == "BSEffectShaderProperty" || id == "NiAlphaProperty") )
					continue;
			}

			if ( nif->inherits( id, "NiProperty" ) )
				menu.addAction( id );
		}

		if ( menu.actions().isEmpty() )
			return index;

		QAction * act = menu.exec( QCursor::pos() );

		if ( act ) {
			QPersistentModelIndex iParent = index;
			QModelIndex iProperty = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );

			if ( !addLink( nif, iParent, "Properties", nif->getBlockNumber( iProperty ) ) ) {
				// Skyrim and later
				auto name = nif->itemName( iProperty );
				if ( name == "BSLightingShaderProperty" || name == "BSEffectShaderProperty" ) {
					if ( !nif->setLink( iParent, "Shader Property", nif->getBlockNumber( iProperty ) ) ) {
						qCWarning( nsSpell ) << Spell::tr( "Failed to attach property." );
					}
				} else if ( name == "NiAlphaProperty" ) {
					if ( !nif->setLink( iParent, "Alpha Property", nif->getBlockNumber( iProperty ) ) ) {
						qCWarning( nsSpell ) << Spell::tr( "Failed to attach property." );
					}
				}
			}

			return iProperty;
		}

		return index;
	}
};

REGISTER_SPELL( spAttachProperty )

//! Attach a Node to a block
class spAttachNode final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach Node" ); }
	QString group() const override { return Spell::tr( "Add" ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->blockInherits( index, "NiNode" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMenu menu;
		QStringList ids = nif->allNiBlocks();
		ids.sort();
		for ( const QString& id : ids ) {
			if ( nif->inherits( id, "NiAVObject" ) && !nif->inherits( id, "NiDynamicEffect" ) )
				menu.addAction( id );
		}

		QAction * act = menu.exec( QCursor::pos() );

		if ( act ) {
			QPersistentModelIndex iParent = index;
			QModelIndex iNode = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );
			addLink( nif, iParent, "Children", nif->getBlockNumber( iNode ) );
			return iNode;
		}

		return index;
	}
};

REGISTER_SPELL( spAttachNode )


//! Attach a new block to an empty Ref link
class spAddNewRef final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach" ); }
	bool instant() const override { return true; }
	QIcon icon() const override { return QIcon( ":img/add" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		auto val = nif->getValue( index );
		if ( val.type() == NifValue::tLink )
			return nif->isLink( index ) && nif->getLink( index ) == -1;
		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		NifItem * item = static_cast<NifItem *>(index.internalPointer());
		auto type = item->templ();

		std::list<QString> allIds = qStringListToStdList( nif->allNiBlocks() );
		blockFilter( nif, allIds, type );

		auto iBlock = nif->getBlockIndex( index );

		std::list<QString> ids;
		auto ctlrFilter = [nif, &ids, &allIds, &iBlock] ( QMultiMap<QString, QString> m ) {
			auto i = m.begin();
			while ( i != m.end() ) {
				if ( nif->inherits( nif->itemName( iBlock ), i.key() ) )
					for ( const auto & id : allIds )
						if ( nif->inherits( id, i.value() ) )
							ids.push_back( id );
				++i;
			}
		};

		auto interpFilter = [nif, &ids, &allIds, &iBlock]( QMultiMap<QString, QStringList> m ) {
			auto i = m.begin();
			while ( i != m.end() ) {
				if ( nif->inherits( nif->itemName( iBlock ), i.key() ) )
					for ( const auto & id : allIds )
						for ( const auto & s : i.value() )
							if ( nif->inherits( id, s ) )
								ids.push_back( id );
				++i;
			}
		};

		if ( nif->inherits( type, "NiTimeController" ) ) {
			// Show only applicable types for controller links for the given block
			if ( nif->blockInherits( iBlock, "NiTimeController" ) && item->hasName("Next Controller") )
				iBlock = nif->getBlockIndex( nif->getLink( index.parent(), "Target" ) );

			if ( nif->getVersionNumber() > 0x14050000 ) {
				ctlrMapping.insert( "NiNode", "NiSkinningLODController" );
			}
			// Block-to-Controller Mapping
			ctlrFilter( ctlrMapping );
			// Bethesda Controllers
			if ( nif->getBSVersion() > 0 )
				ctlrFilter( ctlrMappingBS );

		} else if ( nif->blockInherits( iBlock, "NiTimeController" )
					&& nif->inherits( type, "NiInterpolator" ) ) {
			// Show only applicable types for interpolator links for the given block
			interpFilter( interpMapping );
		} else {
			ids = allIds;
		}

		ids.sort();
		ids.unique();

		QMenu menu;
		if ( ids.size() < 8 ) {
			for ( const QString& id : ids )
				menu.addAction( id );
		} else {
			for ( QMenu * m : blockMenu( nif, ids ) ) {
				if ( m->actions().size() == 1 )
					menu.addAction( m->actions().at(0) );
				else
					menu.addMenu( m );
			}
		}

		QAction * act;
		if ( ids.size() == 1 )
			act = menu.actions().at(0);
		else
			act = menu.exec( QCursor::pos() );

		if ( act ) {
			// insert block
			QModelIndex newindex = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );

			if ( !nif->setLink( index, nif->getBlockNumber( newindex ) ) ) {
				qCWarning( nsSpell ) << tr( "Failed to attach link." );
			}

			if ( nif->inherits( nif->itemName( newindex ), "NiTimeController" ) ) {
				auto blk = nif->getBlockNumber( iBlock );
				nif->setLink( newindex, "Target", blk );
			}

			// Set values that can't be handled by defaults in nif.xml
			blockDefaults( nif, act->text(), newindex );

			// return index to new block
			return newindex;
		}

		return index;
	}
};

REGISTER_SPELL( spAddNewRef )


//! Attach a dynamic effect (4/5 are lights) to a block
class spAttachLight final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach Effect" ); }
	QString group() const override { return Spell::tr( "Add" ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->blockInherits( index, "NiNode" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMenu menu;
		QStringList ids = nif->allNiBlocks();
		ids.sort();
		for ( const QString& id : ids ) {
			if ( nif->inherits( id, "NiDynamicEffect" ) )
				menu.addAction( id );
		}


		QAction * act = menu.exec( QCursor::pos() );

		if ( act ) {
			QPersistentModelIndex iParent = index;
			QModelIndex iLight = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );
			addLink( nif, iParent, "Children", nif->getBlockNumber( iLight ) );
			addLink( nif, iParent, "Effects", nif->getBlockNumber( iLight ) );

			if ( nif->checkVersion( 0, 0x04000002 ) ) {
				nif->set<int>( iLight, "Num Affected Nodes", 1 );
				nif->updateArraySize( iLight, "Affected Nodes" );
				nif->updateArraySize( iLight, "Affected Node Pointers" );
			}

			if ( act->text() == "NiTextureEffect" ) {
				nif->set<int>( iLight, "Flags", 4 );
				QModelIndex iSrcTex = nif->insertNiBlock( "NiSourceTexture", nif->getBlockNumber( iLight ) + 1 );
				nif->setLink( iLight, "Source Texture", nif->getBlockNumber( iSrcTex ) );
			}

			return iLight;
		}

		return index;
	}
};

REGISTER_SPELL( spAttachLight )

//! Attach extra data to a block
class spAttachExtraData final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach Extra Data" ); }
	QString group() const override { return Spell::tr( "Add" ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->blockInherits( index, "NiObjectNET" ) && nif->checkVersion( 0x0a000100, 0 );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMenu menu;
		QStringList ids = nif->allNiBlocks();
		ids.sort();
		for ( const QString& id : ids ) {
			if ( nif->inherits( id, "NiExtraData" ) )
				menu.addAction( id );
		}

		QAction * act = menu.exec( QCursor::pos() );

		if ( act ) {
			QPersistentModelIndex iParent = index;
			QModelIndex iExtra = nif->insertNiBlock( act->text(), nif->getBlockNumber( index ) + 1 );

			// Set values that can't be handled by defaults in nif.xml
			blockDefaults( nif,  act->text(), iExtra );

			addLink( nif, iParent, "Extra Data List", nif->getBlockNumber( iExtra ) );
			return iExtra;
		}

		return index;
	}
};

REGISTER_SPELL( spAttachExtraData )

//! Remove a block
class spRemoveBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final
	{
		/* Unlike Remove Branch this leaves the children behind, orphaned — which
		 * is the part worth saying, because the block list will still show them
		 * and nothing else will hint that they are now unreachable.
		 */
		const int kids = countOwnedBranch( nif, nif->getBlockNumber( index ) ) - 1;
		QString s = Spell::tr( "Remove %1?" ).arg( describeBlock( nif, index ) );
		if ( kids > 0 )
			s += Spell::tr( "\n\nIts %1 child blocks stay in the file with nothing pointing at them. "
				"Remove Branch deletes them too." ).arg( kids );
		return s;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->getBlockNumber( index ) >= 0;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		nif->removeNiBlock( nif->getBlockNumber( index ) );
		return QModelIndex();
	}
};

REGISTER_SPELL( spRemoveBlock )

//! Copy a block to the clipboard
class spCopyBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Copy" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	bool constant() const override final { return true; }
	QKeySequence hotkey() const override final { return{ Qt::CTRL | Qt::SHIFT | Qt::Key_C }; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QByteArray data;
		QBuffer buffer( &data );
		if ( !buffer.open( QIODevice::WriteOnly ) )
			return {};

		QDataStream ds( &buffer );

		auto bType = nif->createRTTIName( index );

		if ( nif->checkVersion( 0x14010001, 0 ) )
			ds << serializeStrings( nif, index, bType );

		if ( nif->saveIndex( buffer, index ) ) {
			QMimeData * mime = new QMimeData;
			mime->setData( QString( STR_BL ).arg( nif->getVersion(), bType ), data );
			QApplication::clipboard()->setMimeData( mime );
		}

		return index;
	}
};

REGISTER_SPELL( spCopyBlock )

//! Paste a block from the clipboard
class spPasteBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Paste" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	QPair<QString, QString> acceptFormat( const QString & format, const NifModel * nif )
	{
		QStringList split = splitCopyPasteMime( format );

		NiMesh::DataStreamMetadata metadata = {};
		auto bType = nif->extractRTTIArgs( split.value( MIME_IDX_TYPE ), metadata );
		if ( !NifModel::isNiBlock( bType ) )
			return {};

		if ( split.value( MIME_IDX_APP ) == "nifskope" && split.value( MIME_IDX_STREAM ) == "niblock" )
			return {split.value( MIME_IDX_VER ), bType};

		return {};
	}

	static void fixFO76ShaderPropertyName( NifModel * nif, char * blockData, const QModelIndex & iBlock,
											const QString & blockType, const QStringList & strings );

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		Q_UNUSED( index );
		const QMimeData * mime = QApplication::clipboard()->mimeData();

		if ( mime ) {
			for ( const QString& form : mime->formats() ) {
				if ( !acceptFormat( form, nif ).first.isEmpty() )
					return true;
			}
		}

		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QMimeData * mime = QApplication::clipboard()->mimeData();

		if ( mime ) {
			for ( const QString& form : mime->formats() ) {
				auto result = acceptFormat( form, nif );
				auto version = result.first;

				NiMesh::DataStreamMetadata metadata = {};
				auto bType = nif->extractRTTIArgs( result.second, metadata );

				if ( !version.isEmpty()
					 && (version == nif->getVersion() || QMessageBox::question( nullptr,
							tr( "Paste Block" ),
							tr( "Nif versions differ!<br><br>Current File Version: %1<br>Clipboard Data Version: %2<br><br>The results will be unpredictable..." )
								.arg( nif->getVersion() )
								.arg( version ) ) == QMessageBox::Yes )
				) {
					QByteArray data = mime->data( form );
					QBuffer buffer( &data );

					if ( buffer.open( QIODevice::ReadOnly ) ) {
						QDataStream ds( &buffer );
						QStringList strings;
						if ( nif->checkVersion( 0x14010001, 0 ) )
							ds >> strings;

						QModelIndex block = nif->insertNiBlock( bType, nif->getBlockCount() );
						if ( buffer.pos() <= ( data.size() - 4 ) )
							fixFO76ShaderPropertyName( nif, data.data() + buffer.pos(), block, bType, strings );
						nif->loadIndex( buffer, block );
						blockLink( nif, index, block );

						// Post-Load corrections

						// NiDataStream RTTI arg values
						if ( nif->checkVersion( 0x14050000, 0 ) && bType == QLatin1String( "NiDataStream" ) ) {
							nif->set<quint32>( block, "Usage", metadata.usage );
							nif->set<quint32>( block, "Access", metadata.access );
						}

						// Set strings
						if ( nif->checkVersion( 0x14010001, 0 ) )
							deserializeStrings( nif, block, bType, strings );

						return block;
					}
				}
			}
		}

		return QModelIndex();
	}
};

void spPasteBlock::fixFO76ShaderPropertyName( NifModel * nif, char * blockData, const QModelIndex & iBlock,
												const QString & blockType, const QStringList & strings )
{
	if ( nif->getBSVersion() < 151 )
		return;
	if ( !( blockType == "BSLightingShaderProperty" || blockType == "BSEffectShaderProperty" ) )
		return;
	// hack to work around issues with pasting Fallout 76 and Starfield shader property blocks
	// where the data is conditional based on the block name being empty
	QModelIndex	iName = nif->getIndex( iBlock, "Name" );
	if ( iName.isValid() && !strings.isEmpty() ) {
		nif->set<QString>( iName, strings.first() );
		// write the new string index to the buffer (FIXME: this may be non-portable)
		*( reinterpret_cast< std::int32_t * >( blockData ) ) = nif->get<qint32>( iName );
	}
}

REGISTER_SPELL( spPasteBlock )

//! Paste a block from the clipboard over another
class spPasteOverBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Paste Over" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return{ Qt::CTRL | Qt::SHIFT | Qt::Key_V }; }

	QPair<QString, QString> acceptFormat( const QString & format, const NifModel * nif, const QModelIndex & iBlock )
	{
		QStringList split = splitCopyPasteMime( format );

		NiMesh::DataStreamMetadata metadata = {};
		auto bType = nif->extractRTTIArgs( split.value( MIME_IDX_TYPE ), metadata );
		if ( !nif->isNiBlock( iBlock, bType ) )
			return {};

		if ( split.value( MIME_IDX_APP ) == "nifskope"
			 && split.value( MIME_IDX_STREAM ) == "niblock" )
			return {split.value( MIME_IDX_VER ), bType};

		return {};
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		const QMimeData * mime = QApplication::clipboard()->mimeData();

		if ( mime ) {
			for ( const QString& form : mime->formats() ) {
				if ( !acceptFormat( form, nif, index ).first.isEmpty() )
					return true;
			}
		}

		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QMimeData * mime = QApplication::clipboard()->mimeData();

		if ( mime ) {
			for ( const QString& form : mime->formats() ) {
				auto result = acceptFormat( form, nif, index );
				auto version = result.first;

				NiMesh::DataStreamMetadata metadata = {};
				auto bType = nif->extractRTTIArgs( result.second, metadata );

				if ( !version.isEmpty()
					 && (version == nif->getVersion() || QMessageBox::question( nullptr,
							tr( "Paste Over" ),
							tr( "Nif versions differ!<br><br>Current File Version: %1<br>Clipboard Data Version: %2<br><br>The results will be unpredictable..." )
								.arg( nif->getVersion() )
							    .arg( version ) ) == QMessageBox::Yes) )
				{
					QByteArray data = mime->data( form );
					QBuffer buffer( &data );
					if ( buffer.open( QIODevice::ReadOnly ) ) {
						QDataStream ds( &buffer );

						QStringList strings;
						if ( nif->checkVersion( 0x14010001, 0 ) )
							ds >> strings;

						if ( buffer.pos() <= ( data.size() - 4 ) )
							spPasteBlock::fixFO76ShaderPropertyName( nif, data.data() + buffer.pos(), index, bType, strings );
						nif->loadIndex( buffer, index );

						// NiDataStream RTTI arg values
						if ( nif->checkVersion( 0x14050000, 0 ) && bType == QLatin1String( "NiDataStream" ) ) {
							nif->set<quint32>( index, "Usage", metadata.usage );
							nif->set<quint32>( index, "Access", metadata.access );
						}

						// Set strings
						if ( nif->checkVersion( 0x14010001, 0 ) )
							deserializeStrings( nif, index, bType, strings );

						return index;
					}
				}
			}
		}

		return QModelIndex();
	}
};

REGISTER_SPELL( spPasteOverBlock )

//! Copy a branch (a block and its descendents) to the clipboard

bool spCopyBranch::isApplicable( const NifModel * nif, const QModelIndex & index )
{
	return nif->isNiBlock( index );
}

//! Serialize a set of blocks as a branch payload onto the clipboard, remapping
//! internal links (shared by single-branch Copy Branch and the Block List's
//! multi-selection copy). Returns false and reports on failure.
static bool serializeBranchToClipboard( NifModel * nif, const QList<qint32> & blocks )
{
	if ( blocks.isEmpty() )
		return false;

	QMap<qint32, qint32> blockMap;
	for ( int b = 0; b < blocks.count(); b++ )
		blockMap.insert( blocks[b], b );

	QMap<qint32, QString> parentMap;
	for ( const auto block : blocks )
	{
		for ( const auto link : nif->getParentLinks( block ) ) {
			if ( !blocks.contains( link ) && !parentMap.contains( link ) ) {
				QString failMessage = Spell::tr( "parent link invalid" );
				QModelIndex iParent = nif->getBlockIndex( link );

				if ( iParent.isValid() ) {
					failMessage = Spell::tr( "parent unnamed" );
					QString name = nif->get<QString>( iParent, "Name" );

					if ( !name.isEmpty() ) {
						parentMap.insert( link, nif->itemName( iParent ) + NAME_SEP + name );
						continue;
					}
				}

				Message::append( Spell::tr( B_ERR ).arg( Spell::tr( "Copy Branch" ) ),
								 Spell::tr( "failed to map parent link %1 %2 for block %3 %4; %5." )
									.arg( link )
									.arg( nif->itemName( nif->getBlockIndex( link ) ) )
									.arg( block )
									.arg( nif->itemName( nif->getBlockIndex( block ) ) )
									.arg( failMessage ),
								 QMessageBox::Critical
				);
				return false;
			}
		}
	}

	QByteArray data;
	QBuffer buffer( &data );

	if ( buffer.open( QIODevice::WriteOnly ) ) {
		QDataStream ds( &buffer );
		ds << int( blocks.count() );
		ds << blockMap;
		ds << parentMap;

		for ( const auto block : blocks ) {
			auto iBlock = nif->getBlockIndex( block );
			auto bType = nif->createRTTIName( iBlock );

			ds << bType;

			if ( nif->checkVersion( 0x14010001, 0 ) )
				ds << serializeStrings( nif, iBlock, bType );

			if ( !nif->saveIndex( buffer, iBlock ) ) {
				Message::append( Spell::tr( B_ERR ).arg( Spell::tr( "Copy Branch" ) ),
								 Spell::tr( "failed to save block %1 %2." ).arg( block ).arg( bType ),
								 QMessageBox::Critical
				);
				return false;
			}
		}

		QMimeData * mime = new QMimeData;
		mime->setData( QString( STR_BR ).arg( nif->getVersion() ), data );
		QApplication::clipboard()->setMimeData( mime );
		return true;
	}

	return false;
}

//! Block List multi-selection copy: the union of every selected root's branch
//! goes onto the clipboard in the branch format, so Paste Branch recreates
//! them all (with internal links remapped) in one step.
bool copyBlockBranchesToClipboard( NifModel * nif, const QList<qint32> & roots )
{
	QList<qint32> blocks;
	for ( qint32 r : roots )
		populateBlocks( blocks, nif, r );	// dedups; unions overlapping branches
	return serializeBranchToClipboard( nif, blocks );
}

// The Block List's current multi-selection (block numbers), published by the UI.
// Copy Branch is reached through a shortcut/menu that only hands the spell one
// index, so this is how the spell learns the user picked several blocks.
static QList<qint32> blockListSelection;

void setBlockListSelection( const QList<qint32> & blocks )
{
	blockListSelection = blocks;
}

QList<qint32> blockListSelectionForSpells()
{
	return blockListSelection;
}

// Asked at the moment a spell runs, rather than tracked, so nothing has to watch
// the mouse to keep it current.
static std::function<bool( qint32 & )> blockListHoverProbe;

void setBlockListHoverProbe( std::function<bool( qint32 & )> probe )
{
	blockListHoverProbe = std::move( probe );
}

bool blockListHoverTarget( qint32 & block )
{
	block = -1;
	return blockListHoverProbe && blockListHoverProbe( block );
}

bool wwEnsureRootBSXFlags( NifModel * nif, quint32 bits, quint32 * wasValue, quint32 * nowValue )
{
	if ( !nif || !bits )
		return false;
	// BSXFlags is a Bethesda block; a file with no BS stream header has no place
	// to put one and nothing that would read it
	if ( nif->getBSVersion() == 0 )
		return false;

	const QList<int> roots = nif->getRootLinks();
	if ( roots.size() != 1 )		// every stock FO4 mesh has exactly one
		return false;
	const QPersistentModelIndex iRoot = nif->getBlockIndex( roots.first() );
	if ( !iRoot.isValid() || !nif->blockInherits( QModelIndex( iRoot ), "NiAVObject" ) )
		return false;
	const QModelIndex iNum = nif->getIndex( QModelIndex( iRoot ), "Num Extra Data List" );
	const QModelIndex iArr = nif->getIndex( QModelIndex( iRoot ), "Extra Data List" );
	if ( !iNum.isValid() || !iArr.isValid() )
		return false;

	/* Only the ROOT's own list counts. SCOL files carry a second BSXFlags on the
	 * physics-merged sub-node, always valued exactly 2 — but never instead of the
	 * root's, so finding that one is not a reason to skip this one.
	 */
	for ( const qint32 l : nif->getLinkArray( iArr ) ) {
		const QModelIndex i = nif->getBlockIndex( l );
		if ( !nif->isNiBlock( i, "BSXFlags" ) )
			continue;
		const quint32 flags = nif->get<uint>( i, "Integer Data" );
		if ( wasValue ) *wasValue = flags;
		if ( nowValue ) *nowValue = flags | bits;
		if ( ( flags & bits ) == bits )
			return false;
		nif->set<uint>( i, "Integer Data", flags | bits );
		return true;
	}

	// Absent: create it directly after the root, where 23,408 of the 24,151
	// BSXFlags-bearing stock meshes put it.
	const QModelIndex iNew = nif->insertNiBlock( QStringLiteral( "BSXFlags" ),
		nif->getBlockNumber( QModelIndex( iRoot ) ) + 1 );
	if ( !iNew.isValid() )
		return false;
	// Sanitize renames any BSXFlags whose Name is not "BSX"; write it correctly
	// rather than leaving a fix-up for later
	// assignString, not set<QString>: from 20.2.0.7 the Name is an INDEX into the
	// header's string table, and setting it as a plain string writes the index
	// digits as the name — the block came out called "1"
	nif->assignString( iNew, "Name", QStringLiteral( "BSX" ) );
	nif->set<uint>( iNew, "Integer Data", bits );

	const int n = nif->get<int>( iNum );
	nif->set<int>( iNum, n + 1 );
	nif->updateArraySize( iArr );
	nif->setLink( nif->getIndex( iArr, n ), nif->getBlockNumber( iNew ) );

	if ( wasValue ) *wasValue = 0;
	if ( nowValue ) *nowValue = bits;
	return true;
}

/*! The blocks a branch spell should actually act on.
 *
 *  A spell is handed ONE index by the menu, so without this every branch
 *  operation silently ignored the rest of a multi-selection. Copy Branch already
 *  consulted the selection; Remove Branch and Duplicate Branch did not, so
 *  selecting five nodes and pressing Ctrl+Delete removed one of them and left
 *  the other four selected and untouched. The menu said "Remove Branch" either
 *  way, which is the menu lying about its scope.
 *
 *  The selection only counts when the clicked block is part of it. Right-clicking
 *  a block OUTSIDE the current selection is a fresh target, not an addition to
 *  it, which is how every file manager behaves.
 */
QList<qint32> spellSelectionRoots( const NifModel * nif, const QModelIndex & index )
{
	const qint32 cur = nif->getBlockNumber( index );
	if ( blockListSelection.size() > 1 && blockListSelection.contains( cur ) )
		return blockListSelection;
	return QList<qint32>{ cur };
}

QModelIndex spCopyBranch::cast( NifModel * nif, const QModelIndex & index )
{
	copyBlockBranchesToClipboard( nif, spellSelectionRoots( nif, index ) );
	return index;
}


REGISTER_SPELL( spCopyBranch )

//! Paste a branch from the clipboard

QString spPasteBranch::acceptFormat( const QString & format, const NifModel * nif )
{
	Q_UNUSED( nif );
	QStringList split = splitCopyPasteMime( format );

	if ( split.value( MIME_IDX_APP ) == "nifskope" && split.value( MIME_IDX_STREAM ) == "nibranch" )
		return split.value( MIME_IDX_VER );

	return QString();
}

bool spPasteBranch::isApplicable( const NifModel * nif, const QModelIndex & index )
{
	if ( index.isValid() && !nif->isNiBlock( index ) && !nif->isLink( index ) )
		return false;

	const QMimeData * mime = QApplication::clipboard()->mimeData();

	/* An INVALID index is now applicable: it means "paste with no parent", which
	 * is what the blank space below the block list asks for. It used to be the
	 * one state in which Ctrl+V was simply disabled, so clearing the selection
	 * and pasting did nothing at all.
	 */
	if ( mime ) {
		for ( const QString& form : mime->formats() ) {
			if ( nif->isVersionSupported( nif->version2number( acceptFormat( form, nif ) ) ) )
				return true;
		}
	}

	return false;
}

QModelIndex spPasteBranch::cast( NifModel * nif, const QModelIndex & index )
{
	const QMimeData * mime = QApplication::clipboard()->mimeData();

	if ( mime ) {
		for ( const QString& form : mime->formats() ) {
			QString v = acceptFormat( form, nif );

			if ( !v.isEmpty()
				&& ( v == nif->getVersion()
					|| QMessageBox::question( nullptr, tr( "Paste Branch" ),
					        tr( "Nif versions differ!<br><br>Current File Version: %1<br>Clipboard Data Version: %2<br><br>The results will be unpredictable..." )
					            .arg( nif->getVersion() ).arg( v )
						) == QMessageBox::Yes
					)
				)
			{
				QByteArray data = mime->data( form );
				QBuffer buffer( &data );

				if ( buffer.open( QIODevice::ReadOnly ) ) {
					QDataStream ds( &buffer );

					int count;
					ds >> count;

					QMap<qint32, qint32> blockMap;
					ds >> blockMap;
					QMutableMapIterator<qint32, qint32> ibm( blockMap );

					auto origBlockCount = nif->getBlockCount();
					while ( ibm.hasNext() ) {
						ibm.next();
						ibm.value() += origBlockCount;
					}

					QMap<qint32, QString> parentMap;
					ds >> parentMap;

					QMapIterator<qint32, QString> ipm( parentMap );

					while ( ipm.hasNext() ) {
						ipm.next();
						qint32 block = getBlockByName( nif, ipm.value() );

						if ( ipm.key() == 0 ) {
							// Ignore Root
							blockMap.insert( ipm.key(), 0 );
						// getBlockByName returns -1 for "not found", so 0 is a hit -
						// `> 0` rejected a valid mapping and aborted the whole paste
						} else if ( block >= 0 ) {
							blockMap.insert( ipm.key(), block );
						} else {
							Message::append( tr( B_ERR ).arg( name() ),
											 tr( "failed to map parent link %1" ).arg( ipm.value() ),
											 QMessageBox::Critical
							);
							return index;
						}
					}

					QModelIndex iRoot;
					QVector<int> pasted;

					nif->holdUpdates( true );
					for ( int c = 0; c < count; c++ ) {
						QString bType;
						QStringList strings;
						ds >> bType;
						if ( nif->checkVersion( 0x14010001, 0 ) )
							ds >> strings;

						NiMesh::DataStreamMetadata metadata = {};
						bType = nif->extractRTTIArgs( bType, metadata );

						QModelIndex block = nif->insertNiBlock( bType, -1 );
						if ( buffer.pos() <= ( data.size() - 4 ) )
							spPasteBlock::fixFO76ShaderPropertyName( nif, data.data() + buffer.pos(), block, bType, strings );
						if ( !nif->loadAndMapLinks( buffer, block, blockMap ) ) {
							// releasing the hold is not optional: it stays set for
							// the life of the model, and updateHeader/updateFooter
							// are no-ops while it is, so every later save would
							// write these blocks behind a stale header
							nif->holdUpdates( false );
							return index;
						}

						// NiDataStream RTTI arg values
						if ( nif->checkVersion( 0x14050000, 0 ) && bType == QLatin1String( "NiDataStream" ) ) {
							nif->set<quint32>( block, "Usage", metadata.usage );
							nif->set<quint32>( block, "Access", metadata.access );
						}

						// Set strings
						if ( nif->checkVersion( 0x14010001, 0 ) )
							deserializeStrings( nif, block, bType, strings );

						pasted.append( nif->getBlockNumber( block ) );
						if ( c == 0 )
							iRoot = block;
					}
					nif->holdUpdates( false );

					// Link every ROOT of the pasted set (a block not childed by
					// another pasted block) to the target. For a single branch
					// that is just iRoot (unchanged); for a multi-selection copy
					// each independent root is slotted in individually.
					//
					// blockLink picks the right slot for the pair (a NiAVObject
					// under a NiNode -> Children, a NiProperty under a shape ->
					// Properties, ...). If the chosen target can't hold a root —
					// e.g. Ctrl+V while a shape, not a node, is current — a scene
					// object would be left orphaned; fall back to the nearest
					// NiNode ancestor of the target so it still slots in.
					/* THE POINTER DECIDES, not the selection.
					 *
					 * Ctrl+V parented into whatever happened to be selected, wherever
					 * you were looking — so pasting next to a branch meant selecting
					 * that branch first, and pasting a free copy was not possible at
					 * all. Over a row it pastes into that row; over the BLANK SPACE
					 * below the rows it pastes with no parent, as a second root, to
					 * be dragged into place.
					 *
					 * The probe answers false when the pointer is not over the block
					 * list — a context menu, the menu bar, another window — and the
					 * index the spell was handed is used exactly as before.
					 */
					QModelIndex target = index;
					qint32 hovered = -1;
					if ( blockListHoverTarget( hovered ) )
						target = hovered >= 0 ? nif->getBlockIndex( hovered ) : QModelIndex();

					const QSet<int> pastedSet( pasted.constBegin(), pasted.constEnd() );
					QModelIndex iNode = target;
					while ( iNode.isValid() && !nif->blockInherits( iNode, "NiNode" ) )
						iNode = nif->getBlockIndex( nif->getParent( iNode ) );

					for ( int nb : std::as_const( pasted ) ) {
						if ( pastedSet.contains( nif->getParent( nb ) ) )
							continue;	// not a root — childed by another pasted block
						if ( !target.isValid() )
							continue;	// deliberately unparented; nothing to link into
						QModelIndex iBlock = nif->getBlockIndex( nb );
						blockLink( nif, target, iBlock );
						if ( nif->getParent( nb ) < 0 && iNode.isValid() && iNode != target
							&& nif->blockInherits( iBlock, "NiAVObject" ) )
							blockLink( nif, iNode, iBlock );	// still orphaned: attach to a node
					}

					return iRoot;
				}
			}
		}
	}

	return QModelIndex();
}


REGISTER_SPELL( spPasteBranch )

//! Paste branch without parenting; see spPasteBranch
/*!
 * This was originally a dodgy hack involving duplicating the contents of
 * spPasteBranch and neglecting to link the blocks; now it calls
 * spPasteBranch with a bogus index.
 */
class spPasteBranch2 final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Paste At End" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	// hotkey() won't work here, probably because the context menu is not available

	QString acceptFormat( const QString & format, const NifModel * nif )
	{
		Q_UNUSED( nif );
		QStringList split = splitCopyPasteMime( format );

		if ( split.value( 0 ) == "nifskope" && split.value( 1 ) == "nibranch" )
			return split.value( 2 );

		return QString();
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		//if ( index.isValid() && ! nif->isNiBlock( index ) && ! nif->isLink( index ) )
		//	return false;
		const QMimeData * mime = QApplication::clipboard()->mimeData();

		if ( mime && !index.isValid() ) {
			for ( const QString& form : mime->formats() ) {
				if ( nif->isVersionSupported( nif->version2number( acceptFormat( form, nif ) ) ) )
					return true;
			}
		}

		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		Q_UNUSED( index );
		spPasteBranch paster;
		paster.cast( nif, QModelIndex() );
		return QModelIndex();
	}
};

REGISTER_SPELL( spPasteBranch2 )

// definitions for spRemoveBranch moved to blocks.h
QString spRemoveBranch::destructiveWarning( NifModel * nif, const QModelIndex & index ) const
{
	const QList<qint32> roots = spellSelectionRoots( nif, index );
	if ( roots.size() > 1 ) {
		int total = 0;
		for ( qint32 r : roots )
			total += countOwnedBranch( nif, r );
		// the count is what makes this decidable, and it is not the number of
		// rows the user highlighted -- branches bring their children with them
		return Spell::tr( "Remove %1 selected branches — %2 blocks in all?" )
			.arg( roots.size() ).arg( total );
	}
	const int n = countOwnedBranch( nif, nif->getBlockNumber( index ) );
	return n > 1
		? Spell::tr( "Remove %1 and the %2 blocks below it?" )
			.arg( describeBlock( nif, index ) ).arg( n - 1 )
		: Spell::tr( "Remove %1?" ).arg( describeBlock( nif, index ) );
}

bool spRemoveBranch::isApplicable( const NifModel * nif, const QModelIndex & iBlock )
{
	int ix = nif->getBlockNumber( iBlock );
	return ( nif->isNiBlock( iBlock ) && ix >= 0 && ( nif->getRootLinks().contains( ix ) || nif->getParent( ix ) >= 0 ) );
}

QModelIndex spRemoveBranch::cast( NifModel * nif, const QModelIndex & index )
{
	/* Persistent indices for every root BEFORE removing any of them.
	 *
	 * Removing a block renumbers everything after it, so a list of block NUMBERS
	 * collected up front points at the wrong blocks by the second iteration.
	 * QPersistentModelIndex is what survives that.
	 */
	QVector<QPersistentModelIndex> targets;
	for ( qint32 r : spellSelectionRoots( nif, index ) )
		if ( nif->isValidBlockNumber( r ) )
			targets.append( QPersistentModelIndex( nif->getBlockIndex( r ) ) );

	for ( const QPersistentModelIndex & t : targets ) {
		if ( !t.isValid() )
			continue;			// already taken as a child of an earlier root
		QPersistentModelIndex iBlock( t );
		removeChildren( nif, iBlock );
		nif->removeNiBlock( nif->getBlockNumber( iBlock ) );
	}
	return QModelIndex();
}

REGISTER_SPELL( spRemoveBranch )

//! Convert descendents to siblings?
class spFlattenBranch : public Spell
{
public:
	QString name() const override { return Spell::tr( "Flatten Branch" ); }
	QString page() const override { return Spell::tr( "Block" ); }

	bool destructive() const override { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override
	{
		/* No block is deleted, so this reads as harmless — and it is the one in
		 * the set most likely to be run by accident on the wrong node. What it
		 * destroys is the hierarchy: the transforms are multiplied into the
		 * children on the way out, and no operation puts the nesting back.
		 */
		return Spell::tr( "Move every child of %1 up to its parent?"
			"\n\nEach child keeps its world position, because %1's transform is multiplied "
			"into it on the way out. The nesting itself cannot be restored." )
			.arg( describeBlock( nif, index ) );
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override
	{
		return ( nif && nif->blockInherits( index, "NiNode" ) );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iNode ) override
	{
		QModelIndex iParent = nif->getBlockIndex( nif->getParent( nif->getBlockNumber( iNode ) ), "NiNode" );
		bool isRecursive = ( typeid( *this ) == typeid( spFlattenBranch ) );
		if ( !iParent.isValid() ) {
			for ( const auto l : nif->getLinkArray( iNode, "Children" ) ) {
				if ( auto iChild = nif->getBlockIndex( l, "NiNode" ); iChild.isValid() )
					doNode( nif, iChild, iNode, Transform(), isRecursive );
			}
		} else {
			doNode( nif, iNode, iParent, Transform(), isRecursive );
		}
		return iNode;
	}

	static void doNode( NifModel * nif, const QModelIndex & iNode, const QModelIndex & iParent, const Transform & tp,
						bool isRecursive = false )
	{
		if ( !nif->blockInherits( iNode, "NiNode" ) )
			return;

		Transform t = tp * Transform( nif, iNode );

		QList<qint32> links;

		for ( const auto l : nif->getLinkArray( iNode, "Children" ) ) {
			QModelIndex iChild = nif->getBlockIndex( l );

			if ( nif->getParent( nif->getBlockNumber( iChild ) ) == nif->getBlockNumber( iNode ) ) {
				Transform tc = t * Transform( nif, iChild );
				tc.writeBack( nif, iChild );
				addLink( nif, iParent, "Children", l );
				delLink( nif, iNode, "Children", l );
				links.append( l );
			}
		}

		if ( isRecursive ) {
			for ( const auto l : links )
				doNode( nif, nif->getBlockIndex( l, "NiNode" ), iParent, tp, true );
		}
	}
};

REGISTER_SPELL( spFlattenBranch )

class spFlattenBranchNR final : public spFlattenBranch
{
public:
	QString name() const override final { return Spell::tr( "Flatten Branch (non-recursive)" ); }
};

REGISTER_SPELL( spFlattenBranchNR )

//! Move a block up in the NIF
class spMoveBlockUp final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Move Up" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return { Qt::ControlModifier | Qt::Key_Up }; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->getBlockNumber( index ) > 0;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock ) override final
	{
		int ix = nif->getBlockNumber( iBlock );
		nif->moveNiBlock( ix, ix - 1 );
		return nif->getBlockIndex( ix - 1 );
	}
};

REGISTER_SPELL( spMoveBlockUp )

//! Move a block down in the NIF
class spMoveBlockDown final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Move Down" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return { Qt::ControlModifier | Qt::Key_Down }; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index ) && nif->getBlockNumber( index ) < nif->getBlockCount() - 1;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock ) override final
	{
		int ix = nif->getBlockNumber( iBlock );
		nif->moveNiBlock( ix, ix + 1 );
		return nif->getBlockIndex( ix + 1 );
	}
};

REGISTER_SPELL( spMoveBlockDown )

//! Remove blocks by regex
class spRemoveBlocksById final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove By Id" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		Q_UNUSED( nif );
		return !index.isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final
	{
		QSettings settings;
		QString key = QString( "%1/%2/%3/Match Expression" ).arg( "Spells", page(), name() );

		bool ok = true;
		QString match = QInputDialog::getText( 0, Spell::tr( "Remove Blocks by Id" ), Spell::tr( "Enter a regular expression:" ), QLineEdit::Normal,
			settings.value( key, "^BS|^NiBS|^bhk|^hk" ).toString(), &ok );

		if ( !ok )
			return QModelIndex();

		settings.setValue( key, match );

		QRegularExpression exp( match );

		int n = 0;

		while ( n < nif->getBlockCount() ) {
			QModelIndex iBlock = nif->getBlockIndex( n );

			if ( nif->itemName( iBlock ).indexOf( exp ) >= 0 )
				nif->removeNiBlock( n );
			else
				n++;
		}

		return QModelIndex();
	}
};

REGISTER_SPELL( spRemoveBlocksById )

//! Remove all blocks except a given branch
class spCropToBranch final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Crop To Branch" ); }
	QString label() const override { return Spell::tr( "Crop File To This Branch…" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final
	{
		/* The one that can empty a file. getBranch appends without deduplicating
		 * — harmless for the removal loop, which only asks `contains` — so the
		 * kept set has to be counted through a QSet or the number here is wrong
		 * on any file that shares a block between two parents.
		 */
		const QList<quint32> branch = getBranch( nif, nif->getBlockNumber( index ) );
		const int kept = QSet<quint32>( branch.cbegin(), branch.cend() ).size();
		const int total = nif->getBlockCount();
		return kept > 1
			? Spell::tr( "Delete %1 of the %2 blocks in this file, keeping only %3 and the %4 "
				"blocks in its branch?" )
				.arg( total - kept ).arg( total ).arg( describeBlock( nif, index ) ).arg( kept - 1 )
			: Spell::tr( "Delete %1 of the %2 blocks in this file, keeping only %3?" )
				.arg( total - kept ).arg( total ).arg( describeBlock( nif, index ) );
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index );
	}

	// construct list of block numbers of all blocks that are in the link's branch (including link itself)
	QList<quint32> getBranch( NifModel * nif, quint32 link ) const
	{
		QList<quint32> branch;
		// add the link itself
		branch << link;
		// add all its children, grandchildren, ...
		for ( const auto child : nif->getChildLinks( link ) ) {
			// check that child is not in branch to avoid infinite recursion
			if ( !branch.contains( quint32(child) ) )
				// it's not in there yet so add the child and grandchildren etc...
				branch << getBranch( nif, child );
		}

		// done, so return result
		return branch;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		// construct list of block numbers of all blocks in this branch of index
		QList<quint32> branch = getBranch( nif, nif->getBlockNumber( index ) );
		//qDebug() << branch;
		// remove non-branch blocks
		int n = 0; // tracks the current block number in the new system (after some blocks have been removed already)
		int m = 0; // tracks the block number in the old system i.e.  as they are numbered in the branch list

		while ( n < nif->getBlockCount() ) {
			if ( !branch.contains( quint32(m) ) )
				nif->removeNiBlock( n );
			else
				n++;

			m++;
		}

		// done
		return QModelIndex();
	}
};

REGISTER_SPELL( spCropToBranch )

//! Convert block types
class spConvertBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Convert" ); }
	QString label() const override { return Spell::tr( "Convert Block Type…" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		// was `index.isValid()`, which put Convert on every field row in Block
		// Details, where cast() reads itemName as a block type, finds no
		// inheritance chain, and offers an empty chooser
		return nif->isNiBlock( index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QStringList ids = nif->allNiBlocks();
		ids.sort();

		QString btype = nif->itemName( index );

		QMap<QString, QMenu *> map;
		for ( const QString& id : ids ) {
			QString x( "Other" );

			// Exclude siblings not in inheritance chain
			if ( btype == id || ( !nif->inherits( btype, id ) && !nif->inherits( id, btype ) ) )
				continue;

			if ( id.startsWith( "Ni" ) )
				x = QString( "Ni&" ) + id.mid( 2, 1 ) + "...";

			if ( id.startsWith( "bhk" ) || id.startsWith( "hk" ) )
				x = "Havok";

			if ( id.startsWith( "BS" ) || id == "AvoidNode" || id == "RootCollisionNode" )
				x = "Bethesda";

			if ( id.startsWith( "Fx" ) )
				x = "Firaxis";

			if ( !map.contains( x ) )
				map[ x ] = new QMenu( x );

			map[ x ]->addAction( id );
		}

		QString newType;
		{
			QMenu menu;
			for ( QMenu * m : map ) {
				menu.addMenu( m );
			}

			if ( QAction * act = menu.exec( QCursor::pos() ); act )
				newType = act->text();
		}

		if ( newType.isEmpty() )
			return index;

		QVector<Vector4> dynamicVertexData;
		if ( btype == "BSDynamicTriShape" ) {
			if ( auto i = nif->getIndex( index, "Vertices" ); i.isValid() )
				dynamicVertexData = nif->getArray<Vector4>( i );
		}

		nif->convertNiBlock( newType, index );

		if ( !dynamicVertexData.isEmpty() ) {
			auto vertexDesc = nif->get<BSVertexDesc>( index, "Vertex Desc" );
			vertexDesc.SetFlag( VF_VERTEX );
			vertexDesc.RemoveFlag( VF_FULLPREC );
			if ( auto i = nif->getItem( index ); i )
				i->invalidateCondition();
			nif->set<BSVertexDesc>( index, "Vertex Desc", vertexDesc );
		}

		if ( btype == "BSTriShape" || newType == "BSTriShape" )
			spRemoveWasteVertices::updateBSTriShape( nif, index );

		if ( !dynamicVertexData.isEmpty() ) {
			if ( auto iVertexData = nif->getIndex( index, "Vertex Data" ); iVertexData.isValid() ) {
				int n = nif->rowCount( iVertexData );
				for ( int i = 0; i < n; i++ ) {
					if ( auto iVertex = nif->getIndex( iVertexData, i ); iVertex.isValid() ) {
						if ( i < dynamicVertexData.size() ) {
							Vector4 v = dynamicVertexData.at( i );
							if ( auto j = nif->getItem( iVertex, "Vertex" ); j ) {
								if ( j->hasValueType( NifValue::tHalfVector3 ) )
									nif->set<HalfVector3>( j, HalfVector3( Vector3( v ) ) );
								else
									nif->set<Vector3>( j, Vector3( v ) );
							}
							if ( auto j = nif->getItem( iVertex, "Bitangent X" ); j )
								nif->set<float>( j, v[3] );
							else if ( auto j = nif->getItem( iVertex, "Unused W" ); j )
								nif->set<float>( j, v[3] );
						}
					}
				}
			}
		}

		if ( newType != "BSDismemberSkinInstance" )
			return index;

		// set the number of partitions on conversion from NiSkinInstance to BSDismemberSkinInstance
		if ( auto iSkinPart = nif->getBlockIndex( nif->getLink( index, "Skin Partition" ) ); iSkinPart.isValid() ) {
			if ( auto iNumParts = nif->getIndex( iSkinPart, "Num Partitions" ); iNumParts.isValid() ) {
				quint32 numParts = nif->get<quint32>( iNumParts );
				nif->set<quint32>( index, "Num Partitions", numParts );
				if ( auto iPartitions = nif->getIndex( index, "Partitions" ); iPartitions.isValid() )
					nif->updateArraySize( iPartitions );
			}
		}

		return index;
	}
};

REGISTER_SPELL( spConvertBlock )

//! Duplicate a block in place
class spDuplicateBlock final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Duplicate" ); }
	QString page() const override final { return Spell::tr( "Block" ); }
	QKeySequence hotkey() const override final { return{ Qt::CTRL | Qt::SHIFT | Qt::Key_D }; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		// from spCopyBlock
		QByteArray data;
		QBuffer buffer( &data );

		// Opening in ReadWrite doesn't work - race condition?
		if ( buffer.open( QIODevice::WriteOnly ) && nif->saveIndex( buffer, index ) ) {
			// from spPasteBlock
			if ( buffer.open( QIODevice::ReadOnly ) ) {
				QModelIndex block = nif->insertNiBlock( nif->itemName( index ), nif->getBlockCount() );
				nif->loadIndex( buffer, block );
				blockLink( nif, nif->getBlockIndex( nif->getParent( nif->getBlockNumber( index ) ) ), block );
				return block;
			}
		}

		return QModelIndex();
	}
};

REGISTER_SPELL( spDuplicateBlock )

//! Duplicate a branch in place

bool spDuplicateBranch::isApplicable( const NifModel * nif, const QModelIndex & index )
{
	return nif->isNiBlock( index );
}

QModelIndex spDuplicateBranch::cast( NifModel * nif, const QModelIndex & index )
{
	/* One branch at a time, but every SELECTED branch.
	 *
	 * The body below duplicates a single branch through a clipboard round-trip
	 * and a parent-name map, and generalising it to several roots at once would
	 * mean rewriting that map. Recursing per root is the same result with no new
	 * machinery: each duplicate is independent anyway. Persistent indices,
	 * because each duplication inserts blocks and renumbers what follows.
	 */
	const QList<qint32> roots = spellSelectionRoots( nif, index );
	if ( roots.size() > 1 ) {
		QVector<QPersistentModelIndex> targets;
		for ( qint32 r : roots )
			if ( nif->isValidBlockNumber( r ) )
				targets.append( QPersistentModelIndex( nif->getBlockIndex( r ) ) );
		QModelIndex last;
		for ( const QPersistentModelIndex & t : targets ) {
			if ( !t.isValid() )
				continue;
			// single-root path: spellSelectionRoots returns just this block
			// because the clicked index is no longer the multi-selection anchor
			blockListSelection.clear();
			last = cast( nif, QModelIndex( t ) );
		}
		return last;
	}

	// from spCopyBranch
	QList<qint32> blocks;
	populateBlocks( blocks, nif, nif->getBlockNumber( index ) );

	QMap<qint32, qint32> blockMap;

	for ( int b = 0; b < blocks.count(); b++ )
		blockMap.insert( blocks[b], b );

	QMap<qint32, QString> parentMap;
	for ( const auto block : blocks )
	{
		for ( const auto link : nif->getParentLinks( block ) ) {
			if ( !blocks.contains( link ) && !parentMap.contains( link ) ) {
				QString failMessage = Spell::tr( "parent link invalid" );
				QModelIndex iParent = nif->getBlockIndex( link );

				if ( iParent.isValid() ) {
					failMessage = Spell::tr( "parent unnamed" );
					QString name = nif->get<QString>( iParent, "Name" );

					if ( !name.isEmpty() ) {
						parentMap.insert( link, nif->itemName( iParent ) + NAME_SEP + name );
						continue;
					}
				}

				Message::append( tr( B_ERR ).arg( name() ),
								 tr( "failed to map parent link %1 %2 for block %3 %4; %5." )
									.arg( link )
									.arg( nif->itemName( nif->getBlockIndex( link ) ) )
									.arg( block )
									.arg( nif->itemName( nif->getBlockIndex( block ) ) )
									.arg( failMessage ),
								 QMessageBox::Critical
				);
				return index;
			}
		}
	}

	QByteArray data;
	QBuffer buffer( &data );

	if ( buffer.open( QIODevice::WriteOnly ) ) {
		QDataStream ds( &buffer );
		ds << int( blocks.count() );
		ds << blockMap;
		ds << parentMap;
		for ( const auto block : blocks ) {
			ds << nif->itemName( nif->getBlockIndex( block ) );

			if ( !nif->saveIndex( buffer, nif->getBlockIndex( block ) ) ) {
				Message::append( tr( B_ERR ).arg( name() ),
								 tr( "failed to save block %1 %2." ).arg( block )
									.arg( nif->itemName( nif->getBlockIndex( block ) ) ),
								 QMessageBox::Critical
				);
				return index;
			}
		}
	}

	// from spPasteBranch
	if ( buffer.open( QIODevice::ReadOnly ) ) {
		QDataStream ds( &buffer );

		int count;
		ds >> count;

		QMap<qint32, qint32> blockMap;
		ds >> blockMap;
		QMutableMapIterator<qint32, qint32> ibm( blockMap );

		while ( ibm.hasNext() ) {
			ibm.next();
			ibm.value() += nif->getBlockCount();
		}

		QMap<qint32, QString> parentMap;
		ds >> parentMap;

		QMapIterator<qint32, QString> ipm( parentMap );

		while ( ipm.hasNext() ) {
			ipm.next();
			qint32 block = getBlockByName( nif, ipm.value() );

			if ( block >= 0 ) {
				blockMap.insert( ipm.key(), block );
			} else {
				Message::append( tr( B_ERR ).arg( name() ),
								 tr( "failed to map parent link %1" ).arg( ipm.value() ),
								 QMessageBox::Critical
				);
				return index;
			}
		}

		QModelIndex iRoot;
		nif->holdUpdates( true );
		for ( int c = 0; c < count; c++ ) {
			QString type;
			ds >> type;

			QModelIndex block = nif->insertNiBlock( type, -1 );

			if ( !nif->loadAndMapLinks( buffer, block, blockMap ) ) {
				// same leak as spPasteBranch: a hold that outlives the spell
				// silences updateHeader/updateFooter for the rest of the session
				nif->holdUpdates( false );
				return index;
			}

			if ( c == 0 )
				iRoot = block;
		}
		nif->holdUpdates( false );
		blockLink( nif, nif->getBlockIndex( nif->getParent( nif->getBlockNumber( index ) ) ), iRoot );

		return iRoot;
	}

	return QModelIndex();
}

REGISTER_SPELL( spDuplicateBranch )

//! Sort blocks by name
class spSortBlockNames final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Sort By Name" ); }
	QString label() const override { return Spell::tr( "Sort Children By Name" ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		Q_UNUSED( nif );
		return ( !index.isValid() || !index.parent().isValid() );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		for ( int n = 0; n < nif->getBlockCount(); n++ ) {
			QModelIndex iBlock = nif->getBlockIndex( n );

			if ( index.isValid() ) {
				iBlock = index;
				n = nif->getBlockCount();
			}

			QModelIndex iNumChildren = nif->getIndex( iBlock, "Num Children" );
			QModelIndex iChildren = nif->getIndex( iBlock, "Children" );

			// NiNode children are NIAVObjects and have a Name
			if ( iNumChildren.isValid() && iChildren.isValid() ) {
				QList<QPair<QString, qint32> > links;

				for ( int r = 0; r < nif->rowCount( iChildren ); r++ ) {
					qint32 l = nif->getLink( nif->getIndex( iChildren, r ) );

					if ( l >= 0 )
						links.append( QPair<QString, qint32>( nif->get<QString>( nif->getBlockIndex( l ), "Name" ), l ) );
				}

				std::stable_sort( links.begin(), links.end() );

				for ( int r = 0; r < links.count(); r++ ) {
					if ( links[r].second != nif->getLink( nif->getIndex( iChildren, r ) ) )
						nif->setLink( nif->getIndex( iChildren, r ), links[r].second );

					nif->set<int>( iNumChildren, links.count() );
					nif->updateArraySize( iChildren );
				}
			}
		}

		if ( index.isValid() ) {
			return index;
		}

		return QModelIndex();
	}
};

REGISTER_SPELL( spSortBlockNames )

//! Attach a Node as a parent of the current block
class spAttachParentNode final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Attach Parent Node" ); }
	QString group() const override { return Spell::tr( "Add" ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		// find our current block number
		int thisBlockNumber = nif->getBlockNumber( index );
		// find our parent; most functions won't break if it doesn't exist,
		// so we don't care if it doesn't exist
		QModelIndex iParent = nif->getBlockIndex( nif->getParent( thisBlockNumber ) );

		// find our index into the parent children array
		QVector<int> parentChildLinks = nif->getLinkArray( iParent, "Children" );
		int thisBlockIndex = parentChildLinks.indexOf( thisBlockNumber );

		// attach a new node
		// basically spAttachNode limited to NiNode and without the auto-attachment
		QMenu menu;
		QStringList ids = nif->allNiBlocks();
		ids.sort();
		for ( const QString& id : ids ) {
			if ( nif->inherits( id, "NiNode" ) )
				menu.addAction( id );
		}

		QModelIndex attachedNode;

		QAction * act = menu.exec( QCursor::pos() );

		if ( !act )
			return index;

		attachedNode = nif->insertNiBlock( act->text(), thisBlockNumber );

		// the attached node pushes this block down one row
		int attachedNodeNumber = thisBlockNumber++;

		// replace this block with the attached node
		nif->setLink( nif->getIndex( nif->getIndex( iParent, "Children" ), thisBlockIndex ), attachedNodeNumber );

		// attach ourselves to the attached node
		addLink( nif, attachedNode, "Children", thisBlockNumber );

		return attachedNode;
	}
};

REGISTER_SPELL( spAttachParentNode )

//! List all blocks that reference this block
class spReferencedBy final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Referenced By" ); }
	QString group() const override { return Spell::tr( "Info" ); }
	QString page() const override final { return Spell::tr( "" ); }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif->isNiBlock( index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		int blockNum = nif->getBlockNumber( index );

		QVector<int> parents;
		QVector<int> children;
		for ( int i = 0; i < nif->getBlockCount(); i++ ) {
			if ( i == blockNum )
				continue;

			auto parentLinks = nif->getParentLinks( i );
			if ( parentLinks.contains( blockNum ) )
				children << i;

			auto childLinks = nif->getChildLinks( i );
			if ( childLinks.contains( blockNum ) )
				parents << i;
		}

		int refCount = parents.count() + children.count();

		for ( const int p : parents ) {
			Message::append( tr( REF_MSG ).arg( refCount ), tr( "Parent: %1" ).arg( p ),
							 QMessageBox::Information
			);
		}

		for ( const int c : children ) {
			Message::append( tr( REF_MSG ).arg( refCount ), tr( "Child: %1" ).arg( c ),
							 QMessageBox::Information
			);
		}
		return index;
	}
};

REGISTER_SPELL( spReferencedBy )


// ---- Block List Summary column (WW) --------------------------------------
// wwBlockSummary: see src/wwblocksummary.h. It lives here rather than in
// NifModel because every line of it is per-type block knowledge, which is what
// this file already is; the model only asks the question and paints the answer.

//! 1234 -> "1234", 12340 -> "12.3k". The exact number matters below ten
//! thousand (is this shape 8 triangles or 80?); above it, the magnitude does.
static QString wwCount( qint64 n )
{
	if ( n < 10000 )
		return QString::number( n );
	if ( n < 10000000 )
		return QString::asprintf( "%.1fk", double( n ) / 1000.0 );
	return QString::asprintf( "%.1fM", double( n ) / 1000000.0 );
}

//! Does any texture path this block OWNS resolve nowhere? Only direct children
//! and the entries of a "Textures" array are examined — a shader property's
//! slots and a texture set's array are all that ever hold one — because a full
//! subtree walk would visit every vertex of a BSTriShape on every repaint.
static bool wwHasMissingTexture( const NifModel * nif, const QModelIndex & block )
{
	auto broken = [nif]( const QModelIndex & idx ) {
		QString path, resolved;
		return nif->texturePathInfo( nif->getItem( idx ), path, resolved )
			&& !path.trimmed().isEmpty() && resolved.isEmpty();
	};
	const int n = nif->rowCount( block );
	for ( int r = 0; r < n; r++ ) {
		QModelIndex child = nif->getIndex( block, r );
		if ( !child.isValid() )
			continue;
		if ( broken( child ) )
			return true;
		if ( nif->itemName( child ) != QLatin1String( "Textures" ) )
			continue;
		const int m = nif->rowCount( child );
		for ( int s = 0; s < m; s++ )
			if ( broken( nif->getIndex( child, s ) ) )
				return true;
	}
	return false;
}

QString wwBlockSummary( const NifModel * nif, int block, QString & status )
{
	status.clear();
	if ( !nif || block < 0 )
		return QString();
	QModelIndex idx = nif->getBlockIndex( block );
	if ( !idx.isValid() )
		return QString();

	const QString type = nif->itemName( idx );
	QStringList bits;

	// ---- geometry ----
	if ( nif->blockInherits( idx, "NiGeometry" ) || nif->blockInherits( idx, "BSTriShape" ) ) {
		// BSTriShape carries its own counts; NiGeometry keeps them in its Data
		QModelIndex counts = idx;
		if ( !nif->getIndex( counts, "Num Vertices" ).isValid() ) {
			const int data = nif->getLink( idx, "Data" );
			if ( data >= 0 )
				counts = nif->getBlockIndex( data );
		}
		const int verts = nif->getIndex( counts, "Num Vertices" ).isValid()
			? nif->get<int>( counts, "Num Vertices" ) : 0;
		const int tris = nif->getIndex( counts, "Num Triangles" ).isValid()
			? nif->get<int>( counts, "Num Triangles" ) : 0;
		bits << QObject::tr( "%1 tris" ).arg( wwCount( tris ) )
			<< QObject::tr( "%1 verts" ).arg( wwCount( verts ) );
		if ( nif->getLink( idx, "Skin" ) >= 0 || nif->getLink( idx, "Skin Instance" ) >= 0 )
			bits << QObject::tr( "skinned" );
		QModelIndex iSeg = nif->getIndex( idx, "Segment" );
		if ( iSeg.isValid() ) {
			int nonEmpty = 0;
			for ( int s = 0; s < nif->rowCount( iSeg ); s++ )
				if ( nif->get<quint32>( nif->getIndex( iSeg, s ), "Num Primitives" ) > 0 )
					nonEmpty++;
			if ( nonEmpty > 0 )
				bits << QObject::tr( "%1 segments" ).arg( nonEmpty );
		}
		if ( tris == 0 || verts == 0 )
			status = QObject::tr( "no geometry" );

	// ---- texture set ----
	} else if ( nif->isNiBlock( idx, "BSShaderTextureSet" ) ) {
		QModelIndex iTex = nif->getIndex( idx, "Textures" );
		QString first;
		int filled = 0;
		for ( int s = 0; s < nif->rowCount( iTex ); s++ ) {
			const QString p = nif->get<QString>( nif->getIndex( iTex, s ) );
			if ( p.trimmed().isEmpty() )
				continue;
			filled++;
			if ( first.isEmpty() )
				first = p.section( QLatin1Char( '\\' ), -1 ).section( QLatin1Char( '/' ), -1 );
		}
		if ( !first.isEmpty() )
			bits << first;
		if ( filled > 1 )
			bits << QObject::tr( "+%1 more" ).arg( filled - 1 );
		if ( filled == 0 )
			bits << QObject::tr( "empty" );

	// ---- skin ----
	} else if ( nif->getIndex( idx, "Num Bones" ).isValid() ) {
		bits << QObject::tr( "%1 bones" ).arg( nif->get<int>( idx, "Num Bones" ) );

	// ---- controllers ----
	} else if ( nif->blockInherits( idx, "NiTimeController" ) ) {
		const int target = nif->getLink( idx, "Target" );
		if ( target >= 0 ) {
			const QString tn = nif->get<QString>( nif->getBlockIndex( target ), "Name" );
			bits << ( tn.isEmpty() ? QObject::tr( "→ #%1" ).arg( target )
				: QObject::tr( "→ %1" ).arg( tn ) );
		}
		if ( nif->getIndex( idx, "Stop Time" ).isValid() ) {
			const float t0 = nif->get<float>( idx, "Start Time" );
			const float t1 = nif->get<float>( idx, "Stop Time" );
			if ( t1 > t0 )
				bits << QObject::tr( "%1–%2 s" )
					.arg( QString::number( t0, 'g', 3 ), QString::number( t1, 'g', 3 ) );
		}

	// ---- animation data ----
	} else if ( nif->getIndex( idx, "Num Keys" ).isValid() ) {
		bits << QObject::tr( "%1 keys" ).arg( nif->get<int>( idx, "Num Keys" ) );

	// ---- nodes ----
	} else if ( nif->blockInherits( idx, "NiNode" ) ) {
		// zero is not worth a row. A skeleton is mostly leaf bones, and "0
		// children" down sixty rows is noise that buries the counts that matter.
		const int kids = nif->getIndex( idx, "Num Children" ).isValid()
			? nif->get<int>( idx, "Num Children" ) : 0;
		if ( kids > 0 )
			bits << QObject::tr( "%1 children" ).arg( kids );
		const int props = nif->getIndex( idx, "Num Properties" ).isValid()
			? nif->get<int>( idx, "Num Properties" ) : 0;
		if ( props > 0 )
			bits << QObject::tr( "%1 properties" ).arg( props );

	// ---- extra data ----
	} else if ( nif->getIndex( idx, "String Data" ).isValid() ) {
		bits << nif->get<QString>( idx, "String Data" );
	} else if ( nif->getIndex( idx, "Integer Data" ).isValid() ) {
		bits << QString::number( nif->get<int>( idx, "Integer Data" ) );

	// ---- binary blobs (bhkPhysicsSystem's Havok packfile, NiBinaryExtraData):
	//      the size is the only fact available without decoding the thing ----
	} else if ( nif->getIndex( idx, "Binary Data" ).isValid() ) {
		const NifItem * bin = nif->getItem( nif->getIndex( idx, "Binary Data" ) );
		if ( QByteArray * data = bin ? bin->get<QByteArray *>() : nullptr )
			bits << QObject::tr( "%1 KB" ).arg( QString::number( data->size() / 1024.0, 'f', 1 ) );

	// ---- shader properties: the material file, when there is one ----
	} else if ( type.contains( QLatin1String( "Shader" ) ) || type.contains( QLatin1String( "Material" ) ) ) {
		const QString mat = nif->getIndex( idx, "Name" ).isValid()
			? nif->get<QString>( idx, "Name" ) : QString();
		if ( !mat.isEmpty() )
			bits << mat.section( QLatin1Char( '\\' ), -1 ).section( QLatin1Char( '/' ), -1 );
	}

	/* ---- statuses, which apply to any block ----
	 *
	 * There is exactly one, plus "no geometry" above, and that is deliberate: a
	 * marker that is sometimes wrong is worse than no marker.
	 *
	 * An "unreferenced" marker was written and then removed, because this model
	 * cannot decide it. getParentLinks() holds the Ptr links a block OWNS, not the
	 * blocks that point AT it — reading it the other way marked 48 of a vanilla
	 * pistol's 57 blocks broken. The reverse relation is rootLinks, and NifModel
	 * builds that as "every block nothing refers to" (nifmodel.cpp:2838), then
	 * writes it back over the footer's Roots array: an orphan and a legitimate
	 * root are the same thing here, by construction. Deciding it needs a reverse
	 * index the model does not keep.
	 */
	if ( status.isEmpty() && wwHasMissingTexture( nif, idx ) )
		status = QObject::tr( "missing texture" );

	return bits.join( QStringLiteral( " · " ) );
}
