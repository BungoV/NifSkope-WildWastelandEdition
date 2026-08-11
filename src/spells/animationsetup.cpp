/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellbook.h"
#include "spells/animationsetup.h"
#include "spells/blocks.h"
#include "nifsnapshot.h"

#include "gl/glcontroller.h"
#include "data/niftypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QFloat16>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <bit>

#include <algorithm>
#include <cmath>

/*! \file animationsetup.cpp
 * \brief Animation rigging spells: attach controllers, wire sequences,
 *  palette registration, removal, sequence duplication/scaling, B-spline baking.
 */

// FO4 EffectShaderControlledVariable enum
static const char * effectFloatVars[10] = {
	"EmissiveMultiple", "Falloff Start Angle", "Falloff Stop Angle",
	"Falloff Start Opacity", "Falloff Stop Opacity", "Alpha Transparency",
	"U Offset", "U Scale", "V Offset", "V Scale"
};

//! What kind of value a controller animates
// CtlrKind / CtlrOption now live in animationsetup.h so the headless CLI can
// name the same controller set; pull them into this file's scope unchanged.
using AnimSetup::CtlrKind;
using AnimSetup::CtlrOption;
using AnimSetup::KindFloat;
using AnimSetup::KindColor;
using AnimSetup::KindBool;
using AnimSetup::KindTransform;

//! Find the AV object that owns a property block (or the block itself if it is one)
static QModelIndex ownerAVObject( const NifModel * nif, const QModelIndex & iBlock )
{
	if ( nif->blockInherits( iBlock, "NiAVObject" ) )
		return iBlock;

	int b = nif->getBlockNumber( iBlock );
	while ( b >= 0 ) {
		b = nif->getParent( b );
		QModelIndex i = nif->getBlockIndex( b );
		if ( nif->blockInherits( i, "NiAVObject" ) )
			return i;
	}

	return QModelIndex();
}

//! Append a controller block to a controllable's Next Controller chain
static void attachControllerToChain( NifModel * nif, const QModelIndex & iTarget, int ctlrNum )
{
	qint32 first = nif->getLink( iTarget, "Controller" );
	if ( first < 0 ) {
		nif->setLink( iTarget, "Controller", ctlrNum );
		return;
	}

	QModelIndex iCur = nif->getBlockIndex( first );
	while ( nif->getLink( iCur, "Next Controller" ) >= 0 )
		iCur = nif->getBlockIndex( nif->getLink( iCur, "Next Controller" ) );

	nif->setLink( iCur, "Next Controller", ctlrNum );
}

//! Create an interpolator (+ data block) for the given kind. Returns interpolator block number.
static int createInterpolator( NifModel * nif, CtlrKind kind, float start, float stop, const QModelIndex & iNode )
{
	QModelIndex iInterp, iData;

	switch ( kind ) {
	case KindFloat:
		iInterp = nif->insertNiBlock( "NiFloatInterpolator" );
		iData = nif->insertNiBlock( "NiFloatData" );
		{
			QModelIndex iGroup = nif->getIndex( iData, "Data" );
			nif->set<int>( iGroup, "Num Keys", 2 );
			nif->set<int>( iGroup, "Interpolation", 1 );
			nif->updateArraySize( nif->getIndex( iGroup, "Keys" ) );
			QModelIndex iKeys = nif->getIndex( iGroup, "Keys" );
			nif->set<float>( nif->getIndex( iKeys, 0 ), "Time", start );
			nif->set<float>( nif->getIndex( iKeys, 0 ), "Value", 0.0f );
			nif->set<float>( nif->getIndex( iKeys, 1 ), "Time", stop );
			nif->set<float>( nif->getIndex( iKeys, 1 ), "Value", 1.0f );
		}
		break;

	case KindColor:
		iInterp = nif->insertNiBlock( "NiPoint3Interpolator" );
		iData = nif->insertNiBlock( "NiPosData" );
		{
			QModelIndex iGroup = nif->getIndex( iData, "Data" );
			nif->set<int>( iGroup, "Num Keys", 2 );
			nif->set<int>( iGroup, "Interpolation", 1 );
			nif->updateArraySize( nif->getIndex( iGroup, "Keys" ) );
			QModelIndex iKeys = nif->getIndex( iGroup, "Keys" );
			nif->set<float>( nif->getIndex( iKeys, 0 ), "Time", start );
			nif->set<Vector3>( nif->getIndex( iKeys, 0 ), "Value", Vector3( 1, 1, 1 ) );
			nif->set<float>( nif->getIndex( iKeys, 1 ), "Time", stop );
			nif->set<Vector3>( nif->getIndex( iKeys, 1 ), "Value", Vector3( 1, 1, 1 ) );
		}
		break;

	case KindBool:
		iInterp = nif->insertNiBlock( "NiBoolInterpolator" );
		iData = nif->insertNiBlock( "NiBoolData" );
		{
			QModelIndex iGroup = nif->getIndex( iData, "Data" );
			nif->set<int>( iGroup, "Num Keys", 2 );
			nif->set<int>( iGroup, "Interpolation", 1 );
			nif->updateArraySize( nif->getIndex( iGroup, "Keys" ) );
			QModelIndex iKeys = nif->getIndex( iGroup, "Keys" );
			nif->set<float>( nif->getIndex( iKeys, 0 ), "Time", start );
			nif->set<int>( nif->getIndex( iKeys, 0 ), "Value", 1 );
			nif->set<float>( nif->getIndex( iKeys, 1 ), "Time", stop );
			nif->set<int>( nif->getIndex( iKeys, 1 ), "Value", 1 );
		}
		break;

	case KindTransform:
		iInterp = nif->insertNiBlock( "NiTransformInterpolator" );
		iData = nif->insertNiBlock( "NiTransformData" );
		{
			// pose = node's current transform
			QModelIndex iTM = nif->getIndex( iInterp, "Transform" );
			Vector3 trans = nif->get<Vector3>( iNode, "Translation" );
			if ( iTM.isValid() ) {
				nif->set<Vector3>( iTM, "Translation", trans );
				nif->set<float>( iTM, "Scale", nif->get<float>( iNode, "Scale" ) );
				QModelIndex iRot = nif->getIndex( iTM, "Rotation" );
				if ( iRot.isValid() ) {
					Quat q;  // identity
					nif->set<Quat>( iRot, q );
				}
			}

			// two flat translation keys so the lane is immediately editable
			QModelIndex iGroup = nif->getIndex( iData, "Translations" );
			if ( iGroup.isValid() ) {
				nif->set<int>( iGroup, "Num Keys", 2 );
				nif->set<int>( iGroup, "Interpolation", 1 );
				nif->updateArraySize( nif->getIndex( iGroup, "Keys" ) );
				QModelIndex iKeys = nif->getIndex( iGroup, "Keys" );
				nif->set<float>( nif->getIndex( iKeys, 0 ), "Time", start );
				nif->set<Vector3>( nif->getIndex( iKeys, 0 ), "Value", trans );
				nif->set<float>( nif->getIndex( iKeys, 1 ), "Time", stop );
				nif->set<Vector3>( nif->getIndex( iKeys, 1 ), "Value", trans );
			}
		}
		break;
	}

	if ( iData.isValid() )
		nif->setLink( iInterp, "Data", nif->getBlockNumber( iData ) );

	return nif->getBlockNumber( iInterp );
}

//! Ensure a NiControllerManager (+ multi target controller + palette) exists; returns manager index
static QModelIndex ensureControllerManager( NifModel * nif )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( nif->blockInherits( i, "NiControllerManager" ) )
			return i;
	}

	// need the scene root
	auto roots = nif->getRootLinks();
	if ( roots.isEmpty() )
		return QModelIndex();

	QModelIndex iRoot = nif->getBlockIndex( roots.first(), "NiAVObject" );
	if ( !iRoot.isValid() )
		return QModelIndex();

	QModelIndex iManager = nif->insertNiBlock( "NiControllerManager" );
	nif->set<int>( iManager, "Flags", 0x000C );
	nif->set<float>( iManager, "Frequency", 1.0f );
	nif->setLink( iManager, "Target", nif->getBlockNumber( iRoot ) );

	QModelIndex iMulti = nif->insertNiBlock( "NiMultiTargetTransformController" );
	nif->set<int>( iMulti, "Flags", 0x000C );
	nif->set<float>( iMulti, "Frequency", 1.0f );
	nif->setLink( iMulti, "Target", nif->getBlockNumber( iRoot ) );
	nif->setLink( iManager, "Next Controller", nif->getBlockNumber( iMulti ) );

	QModelIndex iPalette = nif->insertNiBlock( "NiDefaultAVObjectPalette" );
	nif->setLink( iPalette, "Scene", nif->getBlockNumber( iRoot ) );
	nif->setLink( iManager, "Object Palette", nif->getBlockNumber( iPalette ) );

	attachControllerToChain( nif, iRoot, nif->getBlockNumber( iManager ) );

	return iManager;
}

//! Add a name/object pair to the manager's object palette if not present
static void ensurePaletteEntry( NifModel * nif, const QModelIndex & iManager, const QString & name, int avBlock )
{
	QModelIndex iPalette = nif->getBlockIndex( nif->getLink( iManager, "Object Palette" ), "NiDefaultAVObjectPalette" );
	if ( !iPalette.isValid() )
		return;

	QModelIndex iObjs = nif->getIndex( iPalette, "Objs" );
	for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
		if ( nif->resolveString( nif->getIndex( iObjs, r ), "Name" ) == name )
			return;
	}

	int n = nif->get<int>( iPalette, "Num Objs" );
	nif->set<int>( iPalette, "Num Objs", n + 1 );
	nif->updateArraySize( iObjs );
	QModelIndex iRow = nif->getIndex( iObjs, n );
	nif->assignString( iRow, QStringLiteral( "Name" ), name, false );
	nif->setLink( iRow, "AV Object", avBlock );
}

//! Ensure a node is among the multi target controller's extra targets
static void ensureExtraTarget( NifModel * nif, const QModelIndex & iManager, int avBlock )
{
	QModelIndex iMulti = nif->getBlockIndex( nif->getLink( iManager, "Next Controller" ), "NiMultiTargetTransformController" );
	if ( !iMulti.isValid() ) {
		// search the whole chain
		qint32 next = nif->getLink( iManager, "Next Controller" );
		while ( next >= 0 ) {
			QModelIndex i = nif->getBlockIndex( next );
			if ( nif->blockInherits( i, "NiMultiTargetTransformController" ) ) {
				iMulti = i;
				break;
			}
			next = nif->getLink( i, "Next Controller" );
		}
	}
	if ( !iMulti.isValid() )
		return;

	QModelIndex iTargets = nif->getIndex( iMulti, "Extra Targets" );
	for ( int r = 0; r < nif->rowCount( iTargets ); r++ ) {
		if ( nif->getLink( nif->getIndex( iTargets, r ) ) == avBlock )
			return;
	}

	int n = nif->get<int>( iMulti, "Num Extra Targets" );
	nif->set<int>( iMulti, "Num Extra Targets", n + 1 );
	nif->updateArraySize( iTargets );
	nif->setLink( nif->getIndex( iTargets, n ), avBlock );
}

//! Append a row to a sequence's Controlled Blocks
static void appendControlledBlock( NifModel * nif, const QModelIndex & iSeq, int interpNum, int ctlrNum,
                                   const QString & nodeName, const QString & propType, const QString & ctlrType )
{
	QModelIndex iArr = nif->getIndex( iSeq, "Controlled Blocks" );
	int n = nif->get<int>( iSeq, "Num Controlled Blocks" );
	nif->set<int>( iSeq, "Num Controlled Blocks", n + 1 );
	nif->updateArraySize( iArr );

	QModelIndex iRow = nif->getIndex( iArr, n );
	nif->setLink( iRow, "Interpolator", interpNum );
	if ( ctlrNum >= 0 )
		nif->setLink( iRow, "Controller", ctlrNum );
	if ( nif->getIndex( iRow, "Priority" ).isValid() )
		nif->set<int>( iRow, "Priority", 0 );

	nif->assignString( iRow, QStringLiteral( "Node Name" ), nodeName, false );
	if ( !propType.isEmpty() )
		nif->assignString( iRow, QStringLiteral( "Property Type" ), propType, false );
	nif->assignString( iRow, QStringLiteral( "Controller Type" ), ctlrType, false );
}

//! Create a new NiControllerSequence with text keys, registered on the manager
static QModelIndex createSequence( NifModel * nif, const QModelIndex & iManager, const QString & name,
                                   float start, float stop )
{
	QModelIndex iSeq = nif->insertNiBlock( "NiControllerSequence" );
	nif->assignString( iSeq, QStringLiteral( "Name" ), name, false );
	nif->set<float>( iSeq, "Frequency", 1.0f );
	nif->set<float>( iSeq, "Start Time", start );
	nif->set<float>( iSeq, "Stop Time", stop );
	if ( nif->getIndex( iSeq, "Cycle Type" ).isValid() )
		nif->set<int>( iSeq, "Cycle Type", 0 );
	if ( nif->getIndex( iSeq, "Weight" ).isValid() )
		nif->set<float>( iSeq, "Weight", 1.0f );
	nif->setLink( iSeq, "Manager", nif->getBlockNumber( iManager ) );

	// accum root name = scene root name
	auto roots = nif->getRootLinks();
	if ( !roots.isEmpty() ) {
		QString rootName = nif->resolveString( nif->getBlockIndex( roots.first() ), "Name" );
		nif->assignString( iSeq, QStringLiteral( "Accum Root Name" ), rootName, false );
	}

	// start/end text keys
	QModelIndex iText = nif->insertNiBlock( "NiTextKeyExtraData" );
	nif->set<int>( iText, "Num Text Keys", 2 );
	nif->updateArraySize( nif->getIndex( iText, "Text Keys" ) );
	QModelIndex iKeys = nif->getIndex( iText, "Text Keys" );
	nif->set<float>( nif->getIndex( iKeys, 0 ), "Time", start );
	nif->assignString( nif->getIndex( iKeys, 0 ), QStringLiteral( "Value" ), QStringLiteral( "start" ), false );
	nif->set<float>( nif->getIndex( iKeys, 1 ), "Time", stop );
	nif->assignString( nif->getIndex( iKeys, 1 ), QStringLiteral( "Value" ), QStringLiteral( "end" ), false );
	nif->setLink( iSeq, "Text Keys", nif->getBlockNumber( iText ) );

	addLink( nif, iManager, "Controller Sequences", nif->getBlockNumber( iSeq ) );

	return iSeq;
}


//! Attach controllers to a block and optionally wire it into a controller sequence
// ---- parameterised core (shared by the dialog and the headless CLI) -------

namespace AnimSetup
{

QVector<CtlrOption> controllerOptions( const NifModel * nif, const QModelIndex & iBlock )
{
	QVector<CtlrOption> options;
	if ( !nif || !iBlock.isValid() )
		return options;

	if ( nif->blockInherits( iBlock, "BSEffectShaderProperty" ) ) {
		options.append( { "BSEffectShaderPropertyFloatController", Spell::tr( "Float variable" ), KindFloat, true, false } );
		options.append( { "BSEffectShaderPropertyColorController", Spell::tr( "Emissive color" ), KindColor, false, false } );
	} else if ( nif->blockInherits( iBlock, "BSLightingShaderProperty" ) ) {
		options.append( { "BSLightingShaderPropertyFloatController", Spell::tr( "Float variable (numeric id)" ), KindFloat, false, true } );
		options.append( { "BSLightingShaderPropertyColorController", Spell::tr( "Color variable" ), KindColor, false, false } );
	} else if ( nif->blockInherits( iBlock, "NiAlphaProperty" ) ) {
		options.append( { "BSNiAlphaPropertyTestRefController", Spell::tr( "Alpha test threshold" ), KindFloat, false, false } );
	} else {
		if ( nif->blockInherits( iBlock, "NiLight" ) ) {
			options.append( { "NiLightDimmerController", Spell::tr( "Light dimmer" ), KindFloat, false, false } );
			options.append( { "NiLightColorController", Spell::tr( "Light color" ), KindColor, false, false } );
			/* Radius was the one of the three this list left out, while
			 * spells/blocks.cpp has always been willing to attach it. It cannot
			 * be FROZEN the way the dimmer can, though: freeze bakes a
			 * controller by writing its value into the field it drives, and
			 * nif.xml gives NiPointLight no radius row — the number lives in the
			 * LIGH form, outside the mesh.
			 */
			options.append( { "NiLightRadiusController", Spell::tr( "Light radius" ), KindFloat, false, false } );
		}
		options.append( { "NiTransformController", Spell::tr( "Transform (position/rotation/scale)" ), KindTransform, false, false } );
		options.append( { "NiVisController", Spell::tr( "Visibility (on/off)" ), KindBool, false, false } );
	}
	return options;
}

QStringList sequenceNames( const NifModel * nif )
{
	QStringList names;
	if ( !nif )
		return names;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( nif->blockInherits( i, "NiControllerSequence" ) )
			names << nif->resolveString( i, "Name" );
	}
	return names;
}

bool setupControllers( NifModel * nif, const QModelIndex & iBlock,
                       const Params & p, QString * error )
{
	auto fail = [error]( const QString & msg ) {
		if ( error )
			*error = msg;
		return false;
	};

	if ( !nif || !iBlock.isValid() )
		return fail( QStringLiteral( "invalid target block" ) );

	const QVector<CtlrOption> options = controllerOptions( nif, iBlock );
	if ( options.isEmpty() )
		return fail( QStringLiteral( "no controllers apply to this block type" ) );
	if ( p.chosen.isEmpty() )
		return fail( QStringLiteral( "no controller selected" ) );
	for ( int i : p.chosen ) {
		if ( i < 0 || i >= options.size() )
			return fail( QStringLiteral( "controller index %1 out of range" ).arg( i ) );
	}

	const QModelIndex iAV = ownerAVObject( nif, iBlock );
	const QString nodeName = nif->resolveString( iAV, "Name" );
	const bool isProperty = ( iAV != iBlock );

	// Resolve the target sequence BY NAME (the dialog used a combo index,
	// which is meaningless to a caller that never saw the combo).
	QPersistentModelIndex pExistingSeq;
	if ( p.useSequence && !p.newSequence ) {
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( !nif->blockInherits( i, "NiControllerSequence" ) )
				continue;
			if ( p.sequenceName.isEmpty() || nif->resolveString( i, "Name" ) == p.sequenceName ) {
				pExistingSeq = QPersistentModelIndex( i );
				break;
			}
		}
		if ( !pExistingSeq.isValid() )
			return fail( p.sequenceName.isEmpty()
				? QStringLiteral( "the file has no NiControllerSequence; pass a new sequence name" )
				: QStringLiteral( "no sequence named '%1'" ).arg( p.sequenceName ) );
	}
	if ( p.useSequence && p.newSequence && p.sequenceName.trimmed().isEmpty() )
		return fail( QStringLiteral( "sequence name must not be empty" ) );

	QPersistentModelIndex pBlock( iBlock );
	QPersistentModelIndex pAV( iAV );
	bool created = false;

	nifSnapshotOp( nif, Spell::tr( "Setup controllers on %1" ).arg( nodeName ), [&]() {
		QModelIndex block( pBlock );
		QModelIndex av( pAV );
		float start = 0.0f, stop = 1.0f;

		QModelIndex iManager, iSeq;
		if ( p.useSequence ) {
			iManager = ensureControllerManager( nif );
			if ( !iManager.isValid() )
				return;

			if ( p.newSequence ) {
				iSeq = createSequence( nif, iManager, p.sequenceName.trimmed(), start, stop );
			} else {
				iSeq = QModelIndex( pExistingSeq );
				start = nif->get<float>( iSeq, "Start Time" );
				stop  = nif->get<float>( iSeq, "Stop Time" );
			}
		}

		for ( int optIdx : p.chosen ) {
			const CtlrOption & opt = options[optIdx];

			QModelIndex iCtlr;
			int ctlrNum = -1;

			if ( opt.kind == KindTransform && p.useSequence ) {
				// sequence-driven transforms ride the manager's multi-target controller
				ensureExtraTarget( nif, iManager, nif->getBlockNumber( av ) );
				qint32 next = nif->getLink( iManager, "Next Controller" );
				while ( next >= 0 ) {
					QModelIndex i = nif->getBlockIndex( next );
					if ( nif->blockInherits( i, "NiMultiTargetTransformController" ) ) {
						ctlrNum = next;
						break;
					}
					next = nif->getLink( i, "Next Controller" );
				}
			} else {
				iCtlr = nif->insertNiBlock( opt.type );
				ctlrNum = nif->getBlockNumber( iCtlr );
				nif->set<int>( iCtlr, "Flags", p.useSequence ? 0x004C : 0x0008 );
				nif->set<float>( iCtlr, "Frequency", 1.0f );
				nif->set<float>( iCtlr, "Start Time", start );
				nif->set<float>( iCtlr, "Stop Time", stop );
				nif->setLink( iCtlr, "Target", nif->getBlockNumber( block ) );

				if ( opt.hasEffectVar )
					nif->set<int>( iCtlr, "Controlled Variable", p.effectVar );
				if ( opt.hasIntVar && nif->getIndex( iCtlr, "Controlled Variable" ).isValid() )
					nif->set<int>( iCtlr, "Controlled Variable", p.intVar );

				int poseInterp = createInterpolator( nif, opt.kind, start, stop, av );
				nif->setLink( iCtlr, "Interpolator", poseInterp );

				attachControllerToChain( nif, block, ctlrNum );
			}

			if ( p.useSequence && iSeq.isValid() ) {
				int seqInterp = createInterpolator( nif, opt.kind, start, stop, av );
				QString propType = isProperty ? nif->itemName( block ) : QString();
				QString ctype = ( opt.kind == KindTransform ) ? QStringLiteral( "NiTransformController" ) : opt.type;
				appendControlledBlock( nif, iSeq, seqInterp, ctlrNum, nodeName, propType, ctype );
				ensurePaletteEntry( nif, iManager, nodeName, nif->getBlockNumber( av ) );
			}
			created = true;
		}
	} );

	if ( !created )
		return fail( QStringLiteral( "nothing was created" ) );
	return true;
}

// ---- poses ----------------------------------------------------------------

QVector<int> poseBoneNodes( const NifModel * nif )
{
	QVector<int> bones;
	if ( !nif )
		return bones;
	QSet<int> seen;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iShape = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iShape, "NiAVObject" ) )
			continue;
		const int skin = nif->getLink( iShape, "Skin" );
		if ( skin < 0 )
			continue;
		QModelIndex iBones = nif->getIndex( nif->getBlockIndex( skin ), "Bones" );
		for ( int r = 0; r < nif->rowCount( iBones ); r++ ) {
			const int node = nif->getLink( nif->getIndex( iBones, r ) );
			if ( node >= 0 && !seen.contains( node ) ) {
				seen.insert( node );
				bones.append( node );
			}
		}
	}
	return bones;
}

namespace
{

//! The manager's NiMultiTargetTransformController, which drives posed bones.
int multiTargetController( NifModel * nif, const QModelIndex & iManager )
{
	qint32 next = nif->getLink( iManager, "Next Controller" );
	while ( next >= 0 ) {
		QModelIndex i = nif->getBlockIndex( next );
		if ( nif->blockInherits( i, "NiMultiTargetTransformController" ) )
			return next;
		next = nif->getLink( i, "Next Controller" );
	}
	return -1;
}

//! One key at t=0 in each channel, holding the node's current transform.
int createPoseInterpolator( NifModel * nif, const QModelIndex & iNode )
{
	const Vector3 trans = nif->get<Vector3>( iNode, "Translation" );
	const Matrix  rotM  = nif->get<Matrix>( iNode, "Rotation" );
	const float   scale = nif->get<float>( iNode, "Scale" );
	const Quat    rot   = rotM.toQuat();

	QModelIndex iInterp = nif->insertNiBlock( "NiTransformInterpolator" );
	QModelIndex iData   = nif->insertNiBlock( "NiTransformData" );

	// The interpolator's own Transform is what a reader falls back to, so fill
	// it as well as the keys — a one-key pose should be unambiguous.
	if ( QModelIndex iTM = nif->getIndex( iInterp, "Transform" ); iTM.isValid() ) {
		nif->set<Vector3>( iTM, "Translation", trans );
		nif->set<float>( iTM, "Scale", scale );
		if ( QModelIndex iRot = nif->getIndex( iTM, "Rotation" ); iRot.isValid() )
			nif->set<Quat>( iRot, rot );
	}

	if ( QModelIndex iG = nif->getIndex( iData, "Translations" ); iG.isValid() ) {
		nif->set<int>( iG, "Num Keys", 1 );
		nif->set<int>( iG, "Interpolation", 1 );
		nif->updateArraySize( nif->getIndex( iG, "Keys" ) );
		QModelIndex iK = nif->getIndex( nif->getIndex( iG, "Keys" ), 0 );
		nif->set<float>( iK, "Time", 0.0f );
		nif->set<Vector3>( iK, "Value", trans );
	}

	if ( QModelIndex iG = nif->getIndex( iData, "Scales" ); iG.isValid() ) {
		nif->set<int>( iG, "Num Keys", 1 );
		nif->set<int>( iG, "Interpolation", 1 );
		nif->updateArraySize( nif->getIndex( iG, "Keys" ) );
		QModelIndex iK = nif->getIndex( nif->getIndex( iG, "Keys" ), 0 );
		nif->set<float>( iK, "Time", 0.0f );
		nif->set<float>( iK, "Value", scale );
	}

	// Rotation keys are not in a KeyGroup: Num Rotation Keys / Rotation Type
	// gate a Quaternion Keys array (see NiKeyframeData in nif.xml). Type must
	// not be 4 (XYZ) or the quaternion array is conditioned out.
	nif->set<int>( iData, "Num Rotation Keys", 1 );
	nif->set<int>( iData, "Rotation Type", 1 );	// LINEAR
	if ( QModelIndex iQ = nif->getIndex( iData, "Quaternion Keys" ); iQ.isValid() ) {
		nif->updateArraySize( iQ );
		QModelIndex iK = nif->getIndex( iQ, 0 );
		nif->set<float>( iK, "Time", 0.0f );
		nif->set<Quat>( iK, "Value", rot );
	}

	nif->setLink( iInterp, "Data", nif->getBlockNumber( iData ) );
	return nif->getBlockNumber( iInterp );
}

} // namespace

bool savePose( NifModel * nif, const QString & name, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );
	if ( name.trimmed().isEmpty() )
		return fail( QStringLiteral( "pose name must not be empty" ) );
	if ( sequenceNames( nif ).contains( name ) )
		return fail( QStringLiteral( "a sequence named '%1' already exists" ).arg( name ) );

	const QVector<int> bones = poseBoneNodes( nif );
	if ( bones.isEmpty() )
		return fail( QStringLiteral( "no skinned shape, so there are no bones to pose" ) );

	bool ok = false;
	nifSnapshotOp( nif, Spell::tr( "Save pose %1" ).arg( name ), [&]() {
		QModelIndex iManager = ensureControllerManager( nif );
		if ( !iManager.isValid() )
			return;
		QModelIndex iSeq = createSequence( nif, iManager, name.trimmed(), 0.0f, 0.0f );
		if ( !iSeq.isValid() )
			return;

		for ( int b : bones ) {
			QModelIndex iNode = nif->getBlockIndex( b );
			const QString nodeName = nif->resolveString( iNode, "Name" );
			if ( nodeName.isEmpty() )
				continue;
			ensureExtraTarget( nif, iManager, b );
			const int ctlr = multiTargetController( nif, iManager );
			const int interp = createPoseInterpolator( nif, iNode );
			appendControlledBlock( nif, iSeq, interp, ctlr, nodeName,
								   QString(), QStringLiteral( "NiTransformController" ) );
			ensurePaletteEntry( nif, iManager, nodeName, b );
		}
		ok = true;
	} );

	if ( !ok )
		return fail( QStringLiteral( "could not create the pose sequence" ) );
	return true;
}

namespace
{

//! Find a NiControllerSequence by name; invalid index when absent.
QModelIndex findSequence( const NifModel * nif, const QString & name )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( nif->blockInherits( i, "NiControllerSequence" )
			 && nif->resolveString( i, "Name" ) == name )
			return i;
	}
	return QModelIndex();
}

//! The t=0 transform a ControlledBlock's interpolator holds: first key,
//! falling back to the interpolator's own Transform.
BoneTransform transformOf( const NifModel * nif, const QModelIndex & iInterp )
{
	BoneTransform bt;
	QModelIndex iTM = nif->getIndex( iInterp, "Transform" );
	bt.translation = nif->get<Vector3>( iTM, "Translation" );
	bt.scale       = nif->get<float>( iTM, "Scale" );
	bt.rotation    = nif->get<Quat>( nif->getIndex( iTM, "Rotation" ) );

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
	if ( iData.isValid() ) {
		QModelIndex iK = nif->getIndex( nif->getIndex( nif->getIndex( iData, "Translations" ), "Keys" ), 0 );
		if ( iK.isValid() )
			bt.translation = nif->get<Vector3>( iK, "Value" );
		QModelIndex iS = nif->getIndex( nif->getIndex( nif->getIndex( iData, "Scales" ), "Keys" ), 0 );
		if ( iS.isValid() )
			bt.scale = nif->get<float>( iS, "Value" );
		QModelIndex iQ = nif->getIndex( nif->getIndex( iData, "Quaternion Keys" ), 0 );
		if ( iQ.isValid() )
			bt.rotation = nif->get<Quat>( iQ, "Value" );
	}
	return bt;
}

} // namespace

QHash<QString, BoneTransform> readPose( const NifModel * nif, const QString & name )
{
	QHash<QString, BoneTransform> pose;
	if ( !nif )
		return pose;
	QModelIndex iSeq = findSequence( nif, name );
	if ( !iSeq.isValid() )
		return pose;

	QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
	for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
		QModelIndex iRow = nif->getIndex( iCB, r );
		QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ) );
		if ( !nif->blockInherits( iInterp, "NiTransformInterpolator" ) )
			continue;
		const QString nodeName = nif->resolveString( iRow, "Node Name" );
		if ( !nodeName.isEmpty() )
			pose.insert( nodeName, transformOf( nif, iInterp ) );
	}
	return pose;
}

// ---- Outfit Studio pose XML ----------------------------------------------

namespace
{

// Verbatim port of nifly's RotVecToMat / RotMatToVec (ousnius/nifly, used by
// BodySlide/Outfit Studio). An OS pose's rotX/rotY/rotZ is a ROTATION VECTOR
// (axis * angle, Rodrigues) — NOT three Euler angles. nifly's Matrix3 is
// row-major m[row][col] applied as M*v, identical to NifSkope's Matrix, so
// these produce bit-identical rotations to Outfit Studio. (Using Euler here was
// wrong for any multi-axis bone; it only happened to match on single-axis
// finger curls, where a rotation vector equals the same-axis Euler angle.)
Matrix osRotVecToMat( const Vector3 & v )
{
	const double angle = std::sqrt( double( v[0] ) * v[0] + double( v[1] ) * v[1] + double( v[2] ) * v[2] );
	const double cosang = std::cos( angle );
	const double sinang = std::sin( angle );
	const double onemcosang = ( cosang > 0.5 ) ? ( sinang * sinang / ( 1 + cosang ) ) : ( 1 - cosang );
	const Vector3 n = ( angle != 0.0 ) ? ( v / float( angle ) ) : Vector3( 1.0f, 0.0f, 0.0f );
	Matrix m;
	m( 0, 0 ) = float( n[0] * n[0] * onemcosang + cosang );
	m( 1, 1 ) = float( n[1] * n[1] * onemcosang + cosang );
	m( 2, 2 ) = float( n[2] * n[2] * onemcosang + cosang );
	m( 0, 1 ) = float( n[0] * n[1] * onemcosang + n[2] * sinang );
	m( 1, 0 ) = float( n[0] * n[1] * onemcosang - n[2] * sinang );
	m( 1, 2 ) = float( n[1] * n[2] * onemcosang + n[0] * sinang );
	m( 2, 1 ) = float( n[1] * n[2] * onemcosang - n[0] * sinang );
	m( 2, 0 ) = float( n[2] * n[0] * onemcosang + n[1] * sinang );
	m( 0, 2 ) = float( n[2] * n[0] * onemcosang - n[1] * sinang );
	return m;
}

Vector3 osRotMatToVec( const Matrix & m )
{
	const double cosang = ( double( m( 0, 0 ) ) + m( 1, 1 ) + m( 2, 2 ) - 1 ) * 0.5;
	if ( cosang > 0.5 ) {
		Vector3 v( m( 1, 2 ) - m( 2, 1 ), m( 2, 0 ) - m( 0, 2 ), m( 0, 1 ) - m( 1, 0 ) );
		const double s = v.length();
		if ( s == 0.0 )
			return Vector3();
		return v * float( std::asin( s * 0.5 ) / s );
	}
	if ( cosang > -1 ) {
		Vector3 v( m( 1, 2 ) - m( 2, 1 ), m( 2, 0 ) - m( 0, 2 ), m( 0, 1 ) - m( 1, 0 ) );
		v.normalize();
		return v * float( std::acos( cosang ) );
	}
	double x = ( m( 0, 0 ) - cosang ) * 0.5, y = ( m( 1, 1 ) - cosang ) * 0.5, z = ( m( 2, 2 ) - cosang ) * 0.5;
	if ( x < 0.0 ) x = 0.0;
	if ( y < 0.0 ) y = 0.0;
	if ( z < 0.0 ) z = 0.0;
	Vector3 v( float( std::sqrt( x ) ), float( std::sqrt( y ) ), float( std::sqrt( z ) ) );
	v.normalize();
	if ( m( 1, 2 ) < m( 2, 1 ) ) v[0] = -v[0];
	if ( m( 2, 0 ) < m( 0, 2 ) ) v[1] = -v[1];
	if ( m( 0, 1 ) < m( 1, 0 ) ) v[2] = -v[2];
	return v * float( PI );
}

//! block number of every named NiAVObject, keyed by name.
QHash<QString, int> namedAVObjects( const NifModel * nif )
{
	QHash<QString, int> byName;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->blockInherits( i, "NiAVObject" ) )
			continue;
		const QString n = nif->resolveString( i, "Name" );
		if ( !n.isEmpty() && !byName.contains( n ) )
			byName.insert( n, b );
	}
	return byName;
}

} // namespace

/*! Does this file carry a real bone hierarchy? (See the header for why.)
 *
 * Moved here from nifskope.cpp's hasWorkspaceBoneHierarchy, which the Loaded
 * NIFs rig merge has used since the skeleton-target work to refuse a flat
 * "skeleton" marker. It is the same rule, unchanged: a NiNode whose parent is a
 * NiNode that is not the file root. The only addition is onlyBlocks, so a caller
 * can ask about the bones it is about to write rather than about the file.
 */
bool hasBoneHierarchy( const NifModel * nif, const QSet<int> * onlyBlocks )
{
	if ( !nif )
		return false;
	const QList<int> roots = nif->getRootLinks();
	for ( int block = 0; block < nif->getBlockCount(); block++ ) {
		if ( onlyBlocks && !onlyBlocks->contains( block ) )
			continue;
		const QModelIndex child = nif->getBlockIndex( block );
		if ( !child.isValid() || !nif->isNiBlock( child, "NiNode" ) )
			continue;
		const int parentBlock = nif->getParent( block );
		if ( parentBlock < 0 || roots.contains( parentBlock ) )
			continue;
		const QModelIndex parent = nif->getBlockIndex( parentBlock );
		if ( parent.isValid() && nif->isNiBlock( parent, "NiNode" ) )
			return true;
	}
	return false;
}

bool applyOutfitStudioPose( NifModel * nif, const QString & path,
                            const QHash<QString, Transform> & restByName,
                            float blend, int * appliedOut, int * missingOut, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
		return fail( QStringLiteral( "cannot open %1" ).arg( path ) );

	// parse: bone name -> (euler radians, translation delta)
	struct Delta { Vector3 euler; Vector3 trans; };
	QHash<QString, Delta> deltas;
	QXmlStreamReader xml( &f );
	while ( !xml.atEnd() ) {
		if ( xml.readNext() == QXmlStreamReader::StartElement
			 && xml.name() == QLatin1String( "Bone" ) ) {
			const QXmlStreamAttributes a = xml.attributes();
			const QString bn = a.value( QLatin1String( "name" ) ).toString();
			if ( bn.isEmpty() )
				continue;
			Delta d;
			d.euler = Vector3( a.value( QLatin1String( "rotX" ) ).toFloat(),
			                   a.value( QLatin1String( "rotY" ) ).toFloat(),
			                   a.value( QLatin1String( "rotZ" ) ).toFloat() );
			d.trans = Vector3( a.value( QLatin1String( "transX" ) ).toFloat(),
			                   a.value( QLatin1String( "transY" ) ).toFloat(),
			                   a.value( QLatin1String( "transZ" ) ).toFloat() );
			deltas.insert( bn, d );
		}
	}
	if ( xml.hasError() )
		return fail( QStringLiteral( "XML parse error: %1" ).arg( xml.errorString() ) );
	if ( deltas.isEmpty() )
		return fail( QStringLiteral( "%1 has no <Bone> entries" ).arg( QFileInfo( path ).fileName() ) );

	const QHash<QString, int> nodes = namedAVObjects( nif );
	int applied = 0, missing = 0;

	nifSnapshotOp( nif, Spell::tr( "Import Outfit Studio pose" ), [&]() {
		for ( auto it = deltas.constBegin(); it != deltas.constEnd(); ++it ) {
			auto node = nodes.constFind( it.key() );
			if ( node == nodes.constEnd() ) {
				missing++;
				continue;
			}
			QModelIndex iNode = nif->getBlockIndex( *node );

			// base = rest for this bone (mode-entry snapshot, or the current
			// bind pose when standalone / not captured)
			Transform base;
			auto r = restByName.constFind( it.key() );
			if ( r != restByName.constEnd() )
				base = *r;
			else
				base = Transform( nif, iNode );

			// delta as a Transform, then target = base * delta (rotate in the
			// bone's own local frame; OS deltas are rest-relative). The pose's
			// rot* is a rotation VECTOR, so use nifly's Rodrigues conversion.
			Transform delta;
			delta.rotation = osRotVecToMat( it->euler );
			delta.translation = it->trans;
			delta.scale = 1.0f;
			Transform target = base * delta;

			if ( std::fabs( blend - 1.0f ) > 1e-6f ) {
				Transform cur( nif, iNode );
				target.translation = cur.translation * ( 1.0f - blend ) + target.translation * blend;
				Quat q = Quat::slerp( blend, cur.rotation.toQuat(), target.rotation.toQuat() );
				target.rotation.fromQuat( q );
			}

			nif->set<Vector3>( iNode, "Translation", target.translation );
			nif->set<Matrix>( iNode, "Rotation", target.rotation );
			applied++;
		}
	} );

	if ( appliedOut ) *appliedOut = applied;
	if ( missingOut ) *missingOut = missing;
	if ( applied == 0 )
		return fail( QStringLiteral( "no bone names in the pose matched this skeleton (%1 missing)" ).arg( missing ) );
	return true;
}

bool writeOutfitStudioPose( NifModel * nif, const QString & path, const QString & poseName,
                            const QHash<int, Transform> & restByBlock, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	// gather posed bones (delta from rest above a small threshold), sorted by
	// name to match Outfit Studio's output ordering. Rest is keyed by block, so
	// the delta is always diffed against the exact node it was captured from.
	struct Row { QString name; Vector3 euler; Vector3 trans; };
	QVector<Row> rows;
	for ( auto it = restByBlock.constBegin(); it != restByBlock.constEnd(); ++it ) {
		QModelIndex iNode = nif->getBlockIndex( it.key() );
		const QString name = nif->get<QString>( iNode, "Name" );
		if ( name.isEmpty() )
			continue;
		Transform cur( nif, iNode );
		Transform delta = it.value().inverted() * cur;   // rest-relative
		Row row;
		row.name = name;
		row.euler = osRotMatToVec( delta.rotation );      // rotation VECTOR, not Euler
		row.trans = delta.translation;
		const float rotMag = std::fabs( row.euler[0] ) + std::fabs( row.euler[1] ) + std::fabs( row.euler[2] );
		if ( rotMag < 1e-4f && row.trans.length() < 1e-4f )
			continue;   // unposed — omit, like OS
		rows.append( row );
	}
	std::sort( rows.begin(), rows.end(), []( const Row & a, const Row & b ) { return a.name < b.name; } );
	if ( rows.isEmpty() )
		return fail( QStringLiteral( "no bones are posed — nothing to export" ) );

	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return fail( QStringLiteral( "cannot write %1" ).arg( path ) );

	QXmlStreamWriter xml( &f );
	xml.setAutoFormatting( true );
	xml.setAutoFormattingIndent( 4 );
	xml.writeStartDocument( QStringLiteral( "1.0" ) );
	xml.writeStartElement( QStringLiteral( "PoseData" ) );
	xml.writeStartElement( QStringLiteral( "Pose" ) );
	QString pn = poseName.trimmed();
	if ( pn.isEmpty() )
		pn = QFileInfo( path ).completeBaseName();
	xml.writeAttribute( QStringLiteral( "name" ), pn );
	auto num = []( float v ) { return QString::number( v, 'g', 8 ); };
	for ( const Row & row : rows ) {
		xml.writeStartElement( QStringLiteral( "Bone" ) );
		xml.writeAttribute( QStringLiteral( "name" ), row.name );
		xml.writeAttribute( QStringLiteral( "rotX" ), num( row.euler[0] ) );
		xml.writeAttribute( QStringLiteral( "rotY" ), num( row.euler[1] ) );
		xml.writeAttribute( QStringLiteral( "rotZ" ), num( row.euler[2] ) );
		xml.writeAttribute( QStringLiteral( "transX" ), num( row.trans[0] ) );
		xml.writeAttribute( QStringLiteral( "transY" ), num( row.trans[1] ) );
		xml.writeAttribute( QStringLiteral( "transZ" ), num( row.trans[2] ) );
		xml.writeEndElement();
	}
	xml.writeEndElement();   // Pose
	xml.writeEndElement();   // PoseData
	xml.writeEndDocument();
	return true;
}

// ---- Screen Archer Menu pose JSON ---------------------------------------

bool applySamPose( NifModel * nif, const QString & path, float blend,
                   int * appliedOut, int * missingOut, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( appliedOut ) *appliedOut = 0;
	if ( missingOut ) *missingOut = 0;
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot open %1" ).arg( path ) );

	QJsonParseError perr {};
	const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &perr );
	f.close();
	if ( perr.error != QJsonParseError::NoError )
		return fail( QStringLiteral( "JSON parse error in %1 at offset %2: %3" )
			.arg( QFileInfo( path ).fileName() ).arg( perr.offset ).arg( perr.errorString() ) );
	if ( !doc.isObject() )
		return fail( QStringLiteral( "%1 is not a SAM pose (top level is not an object)" )
			.arg( QFileInfo( path ).fileName() ) );

	const QJsonObject root = doc.object();
	const QJsonValue tv = root.value( QStringLiteral( "transforms" ) );
	if ( !tv.isObject() )
		return fail( QStringLiteral( "%1 has no \"transforms\" object — not a SAM pose file" )
			.arg( QFileInfo( path ).fileName() ) );
	const QJsonObject transforms = tv.toObject();
	if ( transforms.isEmpty() )
		return fail( QStringLiteral( "%1 has an empty \"transforms\" object" )
			.arg( QFileInfo( path ).fileName() ) );

	/* SAM writes every channel as a JSON STRING ("90.000000"), never a number —
	 * see SAF/io.cpp's WriteJsonFloat. A bare number is accepted anyway; nothing
	 * in the format forbids one and a hand-edited pose is the likely source. */
	auto chan = []( const QJsonObject & o, const char * key, float dflt, bool & ok ) -> float {
		const QJsonValue v = o.value( QLatin1String( key ) );
		if ( v.isUndefined() || v.isNull() )
			return dflt;                    // absent channel keeps its default
		if ( v.isDouble() )
			return float( v.toDouble() );
		if ( v.isString() ) {
			bool good = false;
			const float r = v.toString().trimmed().toFloat( &good );
			if ( good )
				return r;
		}
		ok = false;
		return dflt;
	};

	/* Parse everything into absolute local transforms BEFORE touching the model,
	 * so a malformed file cannot leave the rig half-posed.
	 *
	 * A SAM entry REPLACES the node's local transform — it is not a delta from
	 * rest the way an Outfit Studio pose is (proven over 80 pose files: 5504
	 * (file,bone) pairs carry the skeleton's rest translation verbatim, 6 carry
	 * zero). So there is no rest map here and nothing to compose.
	 *
	 * Rotation is Rx(yaw)·Ry(pitch)·Rz(roll) with the angles in DEGREES and the
	 * counter-intuitive mapping yaw->X, pitch->Y, roll->Z. That product, written
	 * in NIF row order, is element-for-element what Matrix::fromEuler(x,y,z)
	 * produces (niftypes.cpp:215) and what SAM's own MatrixFromEulerYPR writes,
	 * so the call below is a direct translation of the format, not a conversion.
	 */
	QHash<QString, Transform> poseByName;
	int malformed = 0;
	const float toRad = float( PI / 180.0 );
	for ( auto it = transforms.constBegin(); it != transforms.constEnd(); ++it ) {
		if ( it.key().isEmpty() || !it.value().isObject() ) {
			malformed++;
			continue;
		}
		const QJsonObject o = it.value().toObject();
		bool ok = true;
		const float yaw   = chan( o, "yaw",   0.0f, ok );
		const float pitch = chan( o, "pitch", 0.0f, ok );
		const float roll  = chan( o, "roll",  0.0f, ok );
		const float x     = chan( o, "x",     0.0f, ok );
		const float y     = chan( o, "y",     0.0f, ok );
		const float z     = chan( o, "z",     0.0f, ok );
		const float scale = chan( o, "scale", 1.0f, ok );
		if ( !ok ) {
			malformed++;
			continue;
		}
		Transform t;
		t.translation = Vector3( x, y, z );
		t.scale = scale;
		t.rotation.fromEuler( yaw * toRad, pitch * toRad, roll * toRad );
		poseByName.insert( it.key(), t );
	}
	if ( poseByName.isEmpty() )
		return fail( QStringLiteral( "%1 has no usable bone entries (%2 malformed)" )
			.arg( QFileInfo( path ).fileName() ).arg( malformed ) );

	const QHash<QString, int> nodes = namedAVObjects( nif );
	int applied = 0, missing = 0, hidden = 0;

	/* Resolve the target bones BEFORE writing anything, because the answer
	 * decides whether this file may be posed at all.
	 *
	 * A SAM entry is an ABSOLUTE PARENT-SPACE transform. That is only meaningful
	 * where the bones are actually parented to each other. A skinned mesh — a
	 * Power Armor Frame.nif, a piece of clothing — carries a copy of the bone
	 * NAMES as a flat list of NiNodes hanging off Scene Root, so every one of
	 * these transforms lands in world space instead of in its parent's, and the
	 * mesh crumples. That looked like a working import for exactly as long as
	 * nobody put geometry in front of it: the bones moved, the pixels changed,
	 * and the result was garbage.
	 */
	QSet<int> targetBlocks;
	QHash<QString, int> matched;
	for ( auto it = poseByName.constBegin(); it != poseByName.constEnd(); ++it ) {
		auto node = nodes.constFind( it.key() );
		// 5 of 80 real poses omit the 17 armour-piece bones; the mirror case
		// (a pose naming bones this file lacks) is just as normal.
		if ( node == nodes.constEnd() || !nif->getBlockIndex( *node ).isValid() ) {
			missing++;
			continue;
		}
		matched.insert( it.key(), *node );
		targetBlocks.insert( *node );
	}
	if ( missingOut ) *missingOut = missing;
	if ( matched.isEmpty() )
		return fail( QStringLiteral( "no bone names in the pose matched this skeleton (%1 not found)" )
			.arg( missing ) );
	if ( !hasBoneHierarchy( nif, &targetBlocks ) )
		return fail( Spell::tr(
			"%1 has no bone hierarchy: its %2 matching node(s) are flat bone "
			"references hanging off the file root, not parented to each other.\n\n"
			"A SAM pose stores each bone as an absolute PARENT-SPACE transform, so "
			"applying it here would place every bone in world space and crumple the "
			"mesh. A skinned mesh (a Power Armor frame, an armour piece, clothing) "
			"only lists the bones its skin references; it is not a skeleton.\n\n"
			"Load the game's CharacterAssets/skeleton.nif, add this file under "
			"Loaded NIFs, mark the skeleton and merge onto it, then import the pose "
			"into the merged document." )
			.arg( nif->getFilename().isEmpty()
				? QStringLiteral( "This file" )
				: QFileInfo( nif->getFilename() ).fileName() )
			.arg( matched.size() ) );

	nifSnapshotOp( nif, Spell::tr( "Import SAM pose" ), [&]() {
		for ( auto it = poseByName.constBegin(); it != poseByName.constEnd(); ++it ) {
			auto node = matched.constFind( it.key() );
			if ( node == matched.constEnd() )
				continue;
			QModelIndex iNode = nif->getBlockIndex( *node );

			Transform target = it.value();
			// Blend, matching the dock's strength slider: SAM is absolute, so
			// the interpolation runs from where the bone is NOW toward the pose.
			if ( std::fabs( blend - 1.0f ) > 1e-6f ) {
				Transform cur( nif, iNode );
				target.translation = cur.translation * ( 1.0f - blend ) + target.translation * blend;
				target.scale = cur.scale * ( 1.0f - blend ) + target.scale * blend;
				Quat q = Quat::slerp( blend, cur.rotation.toQuat(), target.rotation.toQuat() );
				target.rotation.fromQuat( q );
			}

			// writeBack rather than three set<>() calls: it writes the quaternion
			// form for node types that store one, and it writes Scale, which SAM
			// carries and an OS pose does not.
			target.writeBack( nif, iNode );
			if ( std::fabs( target.scale ) < 1e-6f )
				hidden++;
			applied++;
		}
	} );

	if ( appliedOut ) *appliedOut = applied;
	if ( missingOut ) *missingOut = missing;
	if ( applied == 0 )
		return fail( QStringLiteral( "no bone names in the pose matched this skeleton (%1 not found)" )
			.arg( missing ) );

	// non-fatal notes, appended to the dock's status line the way the OS import does
	if ( error ) {
		QStringList notes;
		if ( missing > 0 )
			notes << Spell::tr( "%1 bone(s) not in this skeleton." ).arg( missing );
		if ( malformed > 0 )
			notes << Spell::tr( "%1 entr(y/ies) skipped as malformed." ).arg( malformed );
		if ( hidden > 0 )
			notes << Spell::tr( "%1 bone(s) have scale 0 and will not draw." ).arg( hidden );
		*error = notes.join( QStringLiteral( " " ) );
	}
	return true;
}

QVector<int> samPoseBones( const NifModel * nif )
{
	QVector<int> bones;
	if ( !nif )
		return bones;
	const QList<int> roots = nif->getRootLinks();
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->blockInherits( i, "NiAVObject" ) )
			continue;
		const QString name = nif->resolveString( i, "Name" );
		if ( name.isEmpty() )
			continue;
		// the skinning proxies, spelled both ways on the PA skeleton
		if ( name.endsWith( QLatin1String( "_skin" ), Qt::CaseInsensitive ) )
			continue;
		// depth below the nearest file root; a node not reachable from one at all
		// has no parent space to be expressed in and is skipped
		int depth = 0, at = b;
		while ( at >= 0 && !roots.contains( at ) && depth < 256 ) {
			at = nif->getParent( at );
			depth++;
		}
		if ( at < 0 || depth < 2 )
			continue;
		bones << b;
	}
	return bones;
}

QString samSkeletonName( const NifModel * nif )
{
	if ( !nif )
		return QStringLiteral( "Vanilla" );
	const QHash<QString, int> nodes = namedAVObjects( nif );
	if ( nodes.contains( QStringLiteral( "Pauldron_Armor" ) )
		 && nodes.contains( QStringLiteral( "Tank_Armor" ) ) )
		return QStringLiteral( "Power Armor" );
	if ( nodes.contains( QStringLiteral( "L_RibHelper" ) )
		 || nodes.contains( QStringLiteral( "R_RibHelper" ) ) )
		return QStringLiteral( "Human" );
	const QList<int> roots = nif->getRootLinks();
	if ( !roots.isEmpty() ) {
		const QString rootName = nif->resolveString( nif->getBlockIndex( roots.first() ), "Name" );
		if ( !rootName.isEmpty() )
			return rootName;
	}
	return QStringLiteral( "Vanilla" );		// SAM's own default
}

bool writeSamPose( const NifModel * nif, const QString & path, const QString & poseName,
                   int * writtenOut, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( writtenOut ) *writtenOut = 0;
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	const QVector<int> bones = samPoseBones( nif );
	if ( bones.isEmpty() )
		return fail( Spell::tr( "this file has no posable bone nodes — a SAM pose is a set of "
			"named NiNodes hanging inside a skeleton's hierarchy, and there are none here" ) );

	/* Six decimals for EVERYTHING, angles included. SAM's writer prints yaw/pitch/
	 * roll with "%.02f" (SAF/io.cpp, WriteTransformJson) and x/y/z/scale with
	 * "%.06f"; its reader is a plain float parse either way, so the two-decimal
	 * angles are a writer's habit rather than part of the format. Keeping them
	 * would throw away up to ~0.005 degrees per channel for nothing. */
	auto num = []( float v ) { return QString::number( double( v ), 'f', 6 ); };
	const float toDeg = float( 180.0 / PI );

	QJsonObject transforms;
	QStringList repeated;
	for ( int b : bones ) {
		const QModelIndex i = nif->getBlockIndex( b );
		const QString name = nif->resolveString( i, "Name" );
		const Transform t( nif, i );
		float yaw = 0, pitch = 0, roll = 0;
		/* Matrix::toEuler IS SAM's MatrixToEulerYPR, element for element and in
		 * both gimbal branches, once SAM's transposed NiMatrix43 storage is mapped
		 * onto NifSkope's row order — so this is the import read backwards, not a
		 * second convention that happens to agree. */
		t.rotation.toEuler( yaw, pitch, roll );
		QJsonObject o;
		o.insert( QStringLiteral( "yaw" ), num( yaw * toDeg ) );
		o.insert( QStringLiteral( "pitch" ), num( pitch * toDeg ) );
		o.insert( QStringLiteral( "roll" ), num( roll * toDeg ) );
		o.insert( QStringLiteral( "x" ), num( t.translation[0] ) );
		o.insert( QStringLiteral( "y" ), num( t.translation[1] ) );
		o.insert( QStringLiteral( "z" ), num( t.translation[2] ) );
		o.insert( QStringLiteral( "scale" ), num( t.scale ) );
		// a rig binds by name, so two nodes of one name would silently collapse
		// into one key here exactly as they collapse on import; say so
		if ( transforms.contains( name ) && !repeated.contains( name ) )
			repeated << name;
		transforms.insert( name, o );
	}

	QString pn = poseName.trimmed();
	if ( pn.isEmpty() )
		pn = QFileInfo( path ).completeBaseName();

	QJsonObject root;
	root.insert( QStringLiteral( "name" ), pn );
	root.insert( QStringLiteral( "skeleton" ), samSkeletonName( nif ) );
	root.insert( QStringLiteral( "transforms" ), transforms );
	root.insert( QStringLiteral( "version" ), 2 );		// a NUMBER, as SAM writes it

	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QStringLiteral( "cannot write %1" ).arg( path ) );
	const QByteArray bytes = QJsonDocument( root ).toJson( QJsonDocument::Indented );
	if ( f.write( bytes ) != bytes.size() ) {
		f.close();
		return fail( QStringLiteral( "could not write all of %1" ).arg( path ) );
	}
	f.close();

	if ( writtenOut ) *writtenOut = int( transforms.size() );
	if ( error ) {
		*error = repeated.isEmpty() ? QString()
			: Spell::tr( "%1 bone name(s) appear on more than one node and were written once "
				"(%2)." ).arg( repeated.size() )
				.arg( repeated.mid( 0, 8 ).join( QStringLiteral( ", " ) ) );
	}
	return true;
}

bool applyPose( NifModel * nif, const QString & name, float blend, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	const QHash<QString, BoneTransform> pose = readPose( nif, name );
	if ( pose.isEmpty() )
		return fail( findSequence( nif, name ).isValid()
			? QStringLiteral( "'%1' has no bone transforms" ).arg( name )
			: QStringLiteral( "no sequence named '%1'" ).arg( name ) );

	// node name -> block, so the pose resolves without a scene graph
	QHash<QString, int> nodes;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->blockInherits( i, "NiAVObject" ) )
			continue;
		const QString n = nif->resolveString( i, "Name" );
		if ( !n.isEmpty() && !nodes.contains( n ) )
			nodes.insert( n, b );
	}

	int applied = 0, missing = 0;
	nifSnapshotOp( nif, Spell::tr( "Apply pose %1" ).arg( name ), [&]() {
		for ( auto it = pose.constBegin(); it != pose.constEnd(); ++it ) {
			auto node = nodes.constFind( it.key() );
			if ( node == nodes.constEnd() ) {
				missing++;
				continue;
			}
			QModelIndex iNode = nif->getBlockIndex( *node );
			BoneTransform target = it.value();

			// blend < 1 interpolates from where the bone is now toward the pose
			// (Blender's pose-strength slider). blend == 1 is a plain replace.
			if ( std::fabs( blend - 1.0f ) > 1e-6f ) {
				BoneTransform cur;
				cur.translation = nif->get<Vector3>( iNode, "Translation" );
				cur.rotation    = nif->get<Matrix>( iNode, "Rotation" ).toQuat();
				cur.scale       = nif->get<float>( iNode, "Scale" );
				target.translation = cur.translation * ( 1.0f - blend ) + target.translation * blend;
				target.scale       = cur.scale * ( 1.0f - blend ) + target.scale * blend;
				target.rotation    = Quat::slerp( blend, cur.rotation, target.rotation );
			}

			Matrix m;
			m.fromQuat( target.rotation );
			nif->set<Vector3>( iNode, "Translation", target.translation );
			nif->set<Matrix>( iNode, "Rotation", m );
			if ( target.scale > 0.0f )
				nif->set<float>( iNode, "Scale", target.scale );
			applied++;
		}
	} );

	if ( applied == 0 )
		return fail( QStringLiteral( "'%1' posed no bones (%2 node name(s) not found)" )
			.arg( name ).arg( missing ) );
	if ( error && missing > 0 )
		*error = QStringLiteral( "%1 bone(s) in the pose are not in this file" ).arg( missing );
	return true;
}

} // namespace AnimSetup

class spSetupControllers final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Setup Controllers..." ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !nif || !index.isValid() )
			return false;
		QModelIndex iBlock = nif->getBlockIndex( index );
		return nif->blockInherits( iBlock, "NiAVObject" )
		       || nif->blockInherits( iBlock, "BSEffectShaderProperty" )
		       || nif->blockInherits( iBlock, "BSLightingShaderProperty" )
		       || nif->blockInherits( iBlock, "NiAlphaProperty" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		QModelIndex iAV = ownerAVObject( nif, iBlock );
		QString nodeName = nif->resolveString( iAV, "Name" );

		// available controller types for this block (shared with the CLI)
		const QVector<CtlrOption> options = AnimSetup::controllerOptions( nif, iBlock );

		// existing sequences (names for the combo, plus which already animate
		// this node — the core resolves the chosen sequence by name itself)
		QStringList seqNames;
		QStringList inSequences;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "NiControllerSequence" ) ) {
				QString sn = nif->resolveString( i, "Name" );
				seqNames << sn;

				QModelIndex iCtrl = nif->getIndex( i, "Controlled Blocks" );
				for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
					if ( nif->resolveString( nif->getIndex( iCtrl, r ), "Node Name" ) == nodeName ) {
						inSequences << sn;
						break;
					}
				}
			}
		}

		// ---- dialog
		QDialog dlg;
		dlg.setWindowTitle( Spell::tr( "Setup controllers for %1" ).arg( nodeName.isEmpty() ? nif->itemName( iBlock ) : nodeName ) );
		auto lay = new QVBoxLayout( &dlg );

		if ( !inSequences.isEmpty() )
			lay->addWidget( new QLabel( Spell::tr( "Already animated in: %1" ).arg( inSequences.join( QStringLiteral( ", " ) ) ), &dlg ) );

		auto grpCtlr = new QGroupBox( Spell::tr( "Controllers to add" ), &dlg );
		auto ctlrLay = new QVBoxLayout( grpCtlr );
		QVector<QCheckBox *> checks;
		QComboBox * effectVarBox = nullptr;
		QSpinBox * intVarBox = nullptr;

		for ( const auto & opt : options ) {
			auto cb = new QCheckBox( QString( "%1  (%2)" ).arg( opt.label, opt.type ), grpCtlr );
			ctlrLay->addWidget( cb );
			checks.append( cb );

			if ( opt.hasEffectVar && !effectVarBox ) {
				effectVarBox = new QComboBox( grpCtlr );
				for ( int i = 0; i < 10; i++ )
					effectVarBox->addItem( QString::fromLatin1( effectFloatVars[i] ), i );
				effectVarBox->setCurrentIndex( 6 ); // U Offset
				ctlrLay->addWidget( effectVarBox );
			}
			if ( opt.hasIntVar && !intVarBox ) {
				intVarBox = new QSpinBox( grpCtlr );
				intVarBox->setRange( 0, 40 );
				intVarBox->setToolTip( Spell::tr( "LightingShaderControlledVariable enum value" ) );
				ctlrLay->addWidget( intVarBox );
			}
		}
		lay->addWidget( grpCtlr );

		auto grpSeq = new QGroupBox( Spell::tr( "Controller sequence" ), &dlg );
		auto seqLay = new QVBoxLayout( grpSeq );
		auto rbNone = new QRadioButton( Spell::tr( "Standalone (always playing, no sequence)" ), grpSeq );
		auto rbExisting = new QRadioButton( Spell::tr( "Add to existing sequence:" ), grpSeq );
		auto seqCombo = new QComboBox( grpSeq );
		seqCombo->addItems( seqNames );
		auto rbNew = new QRadioButton( Spell::tr( "Create new sequence:" ), grpSeq );
		auto newNameEdit = new QLineEdit( QStringLiteral( "NewSequence" ), grpSeq );

		if ( seqNames.isEmpty() ) {
			rbExisting->setEnabled( false );
			seqCombo->setEnabled( false );
			rbNew->setChecked( true );
		} else {
			rbExisting->setChecked( true );
		}

		seqLay->addWidget( rbNone );
		seqLay->addWidget( rbExisting );
		seqLay->addWidget( seqCombo );
		seqLay->addWidget( rbNew );
		seqLay->addWidget( newNameEdit );
		lay->addWidget( grpSeq );

		lay->addWidget( new QLabel( Spell::tr( "New interpolators start with 2 default keys.\n"
			"Use the Animation Manager to edit them, or its channel copy/paste to clone keys from another lane." ), &dlg ) );

		auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
		QObject::connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
		QObject::connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
		lay->addWidget( buttons );

		if ( dlg.exec() != QDialog::Accepted )
			return index;

		QVector<int> chosen;
		for ( int i = 0; i < checks.count(); i++ ) {
			if ( checks[i]->isChecked() )
				chosen.append( i );
		}
		if ( chosen.isEmpty() )
			return index;

		// hand the dialog's answers to the shared core
		AnimSetup::Params p;
		p.chosen      = chosen;
		p.useSequence = !rbNone->isChecked();
		p.newSequence = rbNew->isChecked();
		p.sequenceName = p.newSequence ? newNameEdit->text().trimmed() : seqCombo->currentText();
		if ( effectVarBox )
			p.effectVar = effectVarBox->currentData().toInt();
		if ( intVarBox )
			p.intVar = intVarBox->value();

		QString error;
		if ( !AnimSetup::setupControllers( nif, iBlock, p, &error ) )
			QMessageBox::warning( nullptr, name(), error );

		return index;
	}
};

REGISTER_SPELL( spSetupControllers )


//! Remove a node's animation wiring (controlled blocks, controllers, palette, extra targets)
class spRemoveFromAnimation final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove From Animation..." ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !nif || !index.isValid() )
			return false;
		QModelIndex iBlock = nif->getBlockIndex( index );
		return nif->blockInherits( iBlock, "NiAVObject" )
		       || nif->blockInherits( iBlock, "NiProperty" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		QModelIndex iAV = ownerAVObject( nif, iBlock );
		QString nodeName = nif->resolveString( iAV, "Name" );
		int avNum = nif->getBlockNumber( iAV );

		struct Entry
		{
			QString text;
			int kind;          // 0 controlled block, 1 controller chain entry, 2 palette entry, 3 extra target
			int seqBlock = -1;
			int row = -1;
			int ctlrBlock = -1;
		};

		QVector<Entry> entries;

		// controlled blocks in all sequences
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iSeq = nif->getBlockIndex( b );
			if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
				continue;
			QString seqName = nif->resolveString( iSeq, "Name" );
			QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
				if ( nif->resolveString( nif->getIndex( iCtrl, r ), "Node Name" ) == nodeName ) {
					Entry e;
					e.text = Spell::tr( "Sequence \"%1\": controlled block %2 (%3)" )
						.arg( seqName ).arg( r ).arg( nif->resolveString( nif->getIndex( iCtrl, r ), "Controller Type" ) );
					e.kind = 0;
					e.seqBlock = b;
					e.row = r;
					entries.append( e );
				}
			}
		}

		// controllers attached to the block (and the AV object if different)
		for ( const QModelIndex & tgt : { iBlock, iAV } ) {
			if ( !tgt.isValid() )
				continue;
			qint32 c = nif->getLink( tgt, "Controller" );
			while ( c >= 0 ) {
				QModelIndex iCtlr = nif->getBlockIndex( c );
				if ( !nif->blockInherits( iCtlr, "NiControllerManager" )
				     && !nif->blockInherits( iCtlr, "NiMultiTargetTransformController" ) ) {
					Entry e;
					e.text = Spell::tr( "Controller %1 (%2) with its interpolator/data" ).arg( c ).arg( nif->itemName( iCtlr ) );
					e.kind = 1;
					e.ctlrBlock = c;
					// avoid duplicates when tgt==iAV==iBlock
					bool dup = false;
					for ( const auto & x : entries )
						dup = dup || ( x.kind == 1 && x.ctlrBlock == c );
					if ( !dup )
						entries.append( e );
				}
				c = nif->getLink( iCtlr, "Next Controller" );
			}
			if ( iBlock == iAV )
				break;
		}

		// palette + extra targets
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "NiDefaultAVObjectPalette" ) ) {
				QModelIndex iObjs = nif->getIndex( i, "Objs" );
				for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
					if ( nif->resolveString( nif->getIndex( iObjs, r ), "Name" ) == nodeName ) {
						Entry e;
						e.text = Spell::tr( "Palette entry in block %1" ).arg( b );
						e.kind = 2;
						e.seqBlock = b;
						e.row = r;
						entries.append( e );
					}
				}
			} else if ( nif->blockInherits( i, "NiMultiTargetTransformController" ) ) {
				QModelIndex iTargets = nif->getIndex( i, "Extra Targets" );
				for ( int r = 0; r < nif->rowCount( iTargets ); r++ ) {
					if ( nif->getLink( nif->getIndex( iTargets, r ) ) == avNum ) {
						Entry e;
						e.text = Spell::tr( "Extra target in NiMultiTargetTransformController %1" ).arg( b );
						e.kind = 3;
						e.seqBlock = b;
						e.row = r;
						entries.append( e );
					}
				}
			}
		}

		if ( entries.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "\"%1\" has no animation wiring." ).arg( nodeName ) );
			return index;
		}

		QDialog dlg;
		dlg.setWindowTitle( Spell::tr( "Remove animation of %1" ).arg( nodeName ) );
		auto lay = new QVBoxLayout( &dlg );
		lay->addWidget( new QLabel( Spell::tr( "Select what to remove:" ), &dlg ) );
		auto list = new QListWidget( &dlg );
		for ( const auto & e : entries ) {
			auto item = new QListWidgetItem( e.text, list );
			item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
			item->setCheckState( Qt::Checked );
		}
		lay->addWidget( list );
		auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
		QObject::connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
		QObject::connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
		lay->addWidget( buttons );

		if ( dlg.exec() != QDialog::Accepted )
			return index;

		QVector<Entry> selected;
		for ( int i = 0; i < entries.count(); i++ ) {
			if ( list->item( i )->checkState() == Qt::Checked )
				selected.append( entries[i] );
		}
		if ( selected.isEmpty() )
			return index;

		nifSnapshotOp( nif, Spell::tr( "Remove animation of %1" ).arg( nodeName ), [&]() {
			QSet<int> blocksToDelete;

			// 1. controlled block rows (collect interpolators for deletion), highest row first per sequence
			QVector<Entry> ctrlRows;
			for ( const auto & e : selected ) {
				if ( e.kind == 0 )
					ctrlRows.append( e );
			}
			std::sort( ctrlRows.begin(), ctrlRows.end(), []( const Entry & a, const Entry & b ) {
				return a.seqBlock == b.seqBlock ? a.row > b.row : a.seqBlock > b.seqBlock;
			} );

			for ( const auto & e : ctrlRows ) {
				QModelIndex iSeq = nif->getBlockIndex( e.seqBlock );
				QModelIndex iArr = nif->getIndex( iSeq, "Controlled Blocks" );
				int n = nif->rowCount( iArr );
				if ( e.row >= n )
					continue;

				qint32 interp = nif->getLink( nif->getIndex( iArr, e.row ), "Interpolator" );
				if ( interp >= 0 ) {
					blocksToDelete.insert( interp );
					qint32 data = nif->getLink( nif->getBlockIndex( interp ), "Data" );
					if ( data >= 0 )
						blocksToDelete.insert( data );
				}

				// shift rows up
				for ( int r = e.row; r < n - 1; r++ ) {
					QModelIndex src = nif->getIndex( iArr, r + 1 );
					QModelIndex dst = nif->getIndex( iArr, r );
					nif->setLink( dst, "Interpolator", nif->getLink( src, "Interpolator" ) );
					nif->setLink( dst, "Controller", nif->getLink( src, "Controller" ) );
					if ( nif->getIndex( dst, "Priority" ).isValid() )
						nif->set<int>( dst, "Priority", nif->get<int>( src, "Priority" ) );
					for ( const char * s : { "Node Name", "Property Type", "Controller Type", "Controller ID", "Interpolator ID" } ) {
						if ( nif->getIndex( dst, s ).isValid() )
							nif->assignString( dst, QString::fromLatin1( s ), nif->resolveString( src, s ), false );
					}
				}
				nif->set<int>( iSeq, "Num Controlled Blocks", n - 1 );
				nif->updateArraySize( iArr );
			}

			// 2. palette rows
			QVector<Entry> palRows;
			for ( const auto & e : selected ) {
				if ( e.kind == 2 )
					palRows.append( e );
			}
			std::sort( palRows.begin(), palRows.end(), []( const Entry & a, const Entry & b ) { return a.row > b.row; } );
			for ( const auto & e : palRows ) {
				QModelIndex iPal = nif->getBlockIndex( e.seqBlock );
				QModelIndex iObjs = nif->getIndex( iPal, "Objs" );
				int n = nif->rowCount( iObjs );
				for ( int r = e.row; r < n - 1; r++ ) {
					QModelIndex src = nif->getIndex( iObjs, r + 1 );
					QModelIndex dst = nif->getIndex( iObjs, r );
					nif->assignString( dst, QStringLiteral( "Name" ), nif->resolveString( src, "Name" ), false );
					nif->setLink( dst, "AV Object", nif->getLink( src, "AV Object" ) );
				}
				nif->set<int>( iPal, "Num Objs", n - 1 );
				nif->updateArraySize( iObjs );
			}

			// 3. extra targets
			for ( const auto & e : selected ) {
				if ( e.kind != 3 )
					continue;
				QModelIndex iMulti = nif->getBlockIndex( e.seqBlock );
				QModelIndex iTargets = nif->getIndex( iMulti, "Extra Targets" );
				int n = nif->rowCount( iTargets );
				int w = 0;
				for ( int r = 0; r < n; r++ ) {
					qint32 l = nif->getLink( nif->getIndex( iTargets, r ) );
					if ( l != avNum ) {
						nif->setLink( nif->getIndex( iTargets, w ), l );
						w++;
					}
				}
				nif->set<int>( iMulti, "Num Extra Targets", w );
				nif->updateArraySize( iTargets );
			}

			// 4. controllers: unlink from chain, then delete with their interpolator/data
			for ( const auto & e : selected ) {
				if ( e.kind != 1 )
					continue;

				QModelIndex iCtlr = nif->getBlockIndex( e.ctlrBlock );
				qint32 next = nif->getLink( iCtlr, "Next Controller" );

				// find who links to this controller
				for ( const QModelIndex & tgt : { iBlock, iAV } ) {
					if ( !tgt.isValid() )
						continue;
					if ( nif->getLink( tgt, "Controller" ) == e.ctlrBlock ) {
						nif->setLink( tgt, "Controller", next );
					} else {
						qint32 c = nif->getLink( tgt, "Controller" );
						while ( c >= 0 ) {
							QModelIndex iC = nif->getBlockIndex( c );
							if ( nif->getLink( iC, "Next Controller" ) == e.ctlrBlock ) {
								nif->setLink( iC, "Next Controller", next );
								break;
							}
							c = nif->getLink( iC, "Next Controller" );
						}
					}
					if ( iBlock == iAV )
						break;
				}

				blocksToDelete.insert( e.ctlrBlock );
				qint32 interp = nif->getLink( iCtlr, "Interpolator" );
				if ( interp >= 0 ) {
					blocksToDelete.insert( interp );
					qint32 data = nif->getLink( nif->getBlockIndex( interp ), "Data" );
					if ( data >= 0 )
						blocksToDelete.insert( data );
				}
			}

			// don't delete blocks still referenced by other sequences
			QSet<int> stillReferenced;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iSeq = nif->getBlockIndex( b );
				if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
					continue;
				QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
				for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
					qint32 l = nif->getLink( nif->getIndex( iCtrl, r ), "Interpolator" );
					if ( l >= 0 && blocksToDelete.contains( l ) ) {
						stillReferenced.insert( l );
						qint32 data = nif->getLink( nif->getBlockIndex( l ), "Data" );
						if ( data >= 0 )
							stillReferenced.insert( data );
					}
				}
			}
			blocksToDelete.subtract( stillReferenced );

			QList<int> del = blocksToDelete.values();
			std::sort( del.begin(), del.end(), std::greater<int>() );
			// batch the removals: every removeNiBlock otherwise runs a full-model
			// updateLinks/updateFooter — M removals = M full rebuilds (the
			// havok/optimize removal loops use this same Loading+updateModel pattern)
			nif->setState( BaseModel::Loading );
			for ( int b : del )
				nif->removeNiBlock( b );
			nif->restoreState();
			nif->updateModel();
		} );

		return QModelIndex();
	}
};

REGISTER_SPELL( spRemoveFromAnimation )


//! Deep-copy a controller sequence under a new name
class spDuplicateSequence final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Duplicate Sequence..." ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->blockInherits( nif->getBlockIndex( index ), "NiControllerSequence" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iSeq = nif->getBlockIndex( index );

		bool ok = false;
		QString newName = QInputDialog::getText( nullptr, name(), Spell::tr( "Name of the copy:" ),
			QLineEdit::Normal, nif->resolveString( iSeq, "Name" ) + QStringLiteral( "Copy" ), &ok );
		if ( !ok || newName.isEmpty() )
			return index;

		SpellPtr dup = SpellBook::lookup( Spell::tr( "Blocks" ) + QStringLiteral( "/" ) + Spell::tr( "Duplicate Branch" ) );
		if ( !dup )
			dup = SpellBook::lookup( QStringLiteral( "Duplicate Branch" ) );
		if ( !dup ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not find the Duplicate Branch spell." ) );
			return index;
		}

		QModelIndex result;
		nifSnapshotOp( nif, Spell::tr( "Duplicate sequence" ), [&]() {
			result = dup->cast( nif, iSeq );
			if ( result.isValid() ) {
				nif->assignString( result, QStringLiteral( "Name" ), newName, false );
				qint32 mgr = nif->getLink( result, "Manager" );
				if ( mgr >= 0 )
					addLink( nif, nif->getBlockIndex( mgr ), "Controller Sequences", nif->getBlockNumber( result ) );
			}
		} );

		return result.isValid() ? result : index;
	}
};

REGISTER_SPELL( spDuplicateSequence )


//! Scale all key times of a sequence (and its interpolators) by a factor
class spScaleSequence final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Scale Sequence Times..." ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->blockInherits( nif->getBlockIndex( index ), "NiControllerSequence" );
	}

	//! Multiply every "Time" leaf under idx by f (recursive)
	static void scaleTimes( NifModel * nif, const QModelIndex & idx, float f, int depth = 0 )
	{
		if ( depth > 6 || !idx.isValid() )
			return;

		for ( int r = 0; r < nif->rowCount( idx ); r++ ) {
			QModelIndex child = nif->getIndex( idx, r );
			if ( !child.isValid() )
				continue;

			QModelIndex iTime = nif->getIndex( child, "Time" );
			if ( iTime.isValid() )
				nif->set<float>( iTime, nif->get<float>( iTime ) * f );
			else if ( nif->rowCount( child ) > 0 )
				scaleTimes( nif, child, f, depth + 1 );
		}
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iSeq = nif->getBlockIndex( index );

		bool ok = false;
		double f = QInputDialog::getDouble( nullptr, name(),
			Spell::tr( "Scale factor (2.0 = twice as slow, 0.5 = twice as fast):" ), 1.0, 0.01, 100.0, 3, &ok );
		if ( !ok || f == 1.0 )
			return index;

		nifSnapshotOp( nif, Spell::tr( "Scale sequence times by %1" ).arg( f ), [&]() {
			float ff = (float)f;

			nif->set<float>( iSeq, "Start Time", nif->get<float>( iSeq, "Start Time" ) * ff );
			nif->set<float>( iSeq, "Stop Time", nif->get<float>( iSeq, "Stop Time" ) * ff );

			// text keys
			QModelIndex iText = nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ) );
			if ( iText.isValid() )
				scaleTimes( nif, iText, ff );

			// interpolator data of all controlled blocks
			QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
				QModelIndex iInterp = nif->getBlockIndex( nif->getLink( nif->getIndex( iCtrl, r ), "Interpolator" ) );
				if ( !iInterp.isValid() )
					continue;

				for ( const char * linkName : { "Data", "Path Data", "Percent Data" } ) {
					QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, linkName ) );
					if ( iData.isValid() )
						scaleTimes( nif, iData, ff );
				}

				// B-spline interpolators keep times on the block itself
				if ( nif->getIndex( iInterp, "Start Time" ).isValid() ) {
					nif->set<float>( iInterp, "Start Time", nif->get<float>( iInterp, "Start Time" ) * ff );
					nif->set<float>( iInterp, "Stop Time", nif->get<float>( iInterp, "Stop Time" ) * ff );
				}
			}
		} );

		return index;
	}
};

REGISTER_SPELL( spScaleSequence )


//! Sample a B-spline transform interpolator into an editable keyframe interpolator
class spBakeBSpline final : public Spell
{
	//! Concrete Controller shim (Controller is abstract; the interpolator only needs a parent pointer)
	class DummyController final : public Controller
	{
	public:
		DummyController() : Controller( QModelIndex() ) {}
		void updateTime( float ) override final {}
	};

public:
	QString name() const override final { return Spell::tr( "Bake B-Spline To Keys..." ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->blockInherits( nif->getBlockIndex( index ), "NiBSplineTransformInterpolator" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBSpline = nif->getBlockIndex( index );
		int bsplineNum = nif->getBlockNumber( iBSpline );

		float start = nif->get<float>( iBSpline, "Start Time" );
		float stop = nif->get<float>( iBSpline, "Stop Time" );

		if ( !( stop > start ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Invalid start/stop times on the interpolator." ) );
			return index;
		}

		bool ok = false;
		int fps = QInputDialog::getInt( nullptr, name(), Spell::tr( "Samples per second:" ), 30, 1, 240, 1, &ok );
		if ( !ok )
			return index;

		// sample using the same code the renderer uses
		DummyController dummy;
		BSplineTransformInterpolator interp( &dummy );
		if ( !interp.update( nif, iBSpline ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not read the B-spline data." ) );
			return index;
		}

		int numSamples = std::max( 2, (int)std::ceil( ( stop - start ) * fps ) + 1 );
		QVector<float> times;
		QVector<Transform> transforms;

		for ( int s = 0; s < numSamples; s++ ) {
			float t = start + ( stop - start ) * s / ( numSamples - 1 );
			Transform tm;
			interp.updateTransform( tm, t );
			times.append( t );
			transforms.append( tm );
		}

		QModelIndex result;

		nifSnapshotOp( nif, Spell::tr( "Bake B-spline to keys" ), [&]() {
			QModelIndex iInterp = nif->insertNiBlock( "NiTransformInterpolator" );
			QModelIndex iData = nif->insertNiBlock( "NiTransformData" );
			nif->setLink( iInterp, "Data", nif->getBlockNumber( iData ) );

			QModelIndex iTM = nif->getIndex( iInterp, "Transform" );
			if ( iTM.isValid() && !transforms.isEmpty() ) {
				nif->set<Vector3>( iTM, "Translation", transforms[0].translation );
				nif->set<float>( iTM, "Scale", transforms[0].scale );
				QModelIndex iRot = nif->getIndex( iTM, "Rotation" );
				if ( iRot.isValid() )
					nif->set<Quat>( iRot, transforms[0].rotation.toQuat() );
			}

			// rotations (quat keys, linear)
			nif->set<int>( iData, "Num Rotation Keys", times.count() );
			nif->set<int>( iData, "Rotation Type", 1 );
			QModelIndex iQuat = nif->getIndex( iData, "Quaternion Keys" );
			nif->updateArraySize( iQuat );
			for ( int k = 0; k < times.count(); k++ ) {
				QModelIndex iKey = nif->getIndex( iQuat, k );
				nif->set<float>( iKey, "Time", times[k] );
				nif->set<Quat>( nif->getIndex( iKey, "Value" ), transforms[k].rotation.toQuat() );
			}

			// translations
			QModelIndex iTrans = nif->getIndex( iData, "Translations" );
			nif->set<int>( iTrans, "Num Keys", times.count() );
			nif->set<int>( iTrans, "Interpolation", 1 );
			nif->updateArraySize( nif->getIndex( iTrans, "Keys" ) );
			QModelIndex iTKeys = nif->getIndex( iTrans, "Keys" );
			for ( int k = 0; k < times.count(); k++ ) {
				nif->set<float>( nif->getIndex( iTKeys, k ), "Time", times[k] );
				nif->set<Vector3>( nif->getIndex( iTKeys, k ), "Value", transforms[k].translation );
			}

			// scales
			QModelIndex iScales = nif->getIndex( iData, "Scales" );
			nif->set<int>( iScales, "Num Keys", times.count() );
			nif->set<int>( iScales, "Interpolation", 1 );
			nif->updateArraySize( nif->getIndex( iScales, "Keys" ) );
			QModelIndex iSKeys = nif->getIndex( iScales, "Keys" );
			for ( int k = 0; k < times.count(); k++ ) {
				nif->set<float>( nif->getIndex( iSKeys, k ), "Time", times[k] );
				nif->set<float>( nif->getIndex( iSKeys, k ), "Value", transforms[k].scale );
			}

			int newNum = nif->getBlockNumber( iInterp );

			// re-point every reference to the B-spline at the new interpolator
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iBlock = nif->getBlockIndex( b );

				if ( nif->blockInherits( iBlock, "NiTimeController" ) ) {
					if ( nif->getLink( iBlock, "Interpolator" ) == bsplineNum )
						nif->setLink( iBlock, "Interpolator", newNum );
				} else if ( nif->blockInherits( iBlock, "NiControllerSequence" ) ) {
					QModelIndex iCtrl = nif->getIndex( iBlock, "Controlled Blocks" );
					for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
						if ( nif->getLink( nif->getIndex( iCtrl, r ), "Interpolator" ) == bsplineNum )
							nif->setLink( nif->getIndex( iCtrl, r ), "Interpolator", newNum );
					}
				}
			}

			result = iInterp;
		} );

		if ( result.isValid() )
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Baked %1 samples. The B-spline blocks are now unreferenced;\nuse Spells > Optimize > Remove Unused Blocks to clean them up." ).arg( times.count() ) );

		return result.isValid() ? result : index;
	}
};

REGISTER_SPELL( spBakeBSpline )


//! Sort a sequence's controlled blocks alphabetically by node name
class spSortControlledBlocks final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Sort By Node Name" ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && index.isValid() && nif->itemName( index ) == QLatin1String( "Controlled Blocks" )
		       && nif->blockInherits( nif->getBlockIndex( index ), "NiControllerSequence" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		struct Row
		{
			qint32 interp, ctlr;
			int priority;
			QString strings[5];
		};
		static const char * strFields[5] = { "Node Name", "Property Type", "Controller Type", "Controller ID", "Interpolator ID" };

		QVector<Row> rows;
		int n = nif->rowCount( index );
		for ( int r = 0; r < n; r++ ) {
			QModelIndex iRow = nif->getIndex( index, r );
			Row row;
			row.interp = nif->getLink( iRow, "Interpolator" );
			row.ctlr = nif->getLink( iRow, "Controller" );
			row.priority = nif->get<int>( iRow, "Priority" );
			for ( int s = 0; s < 5; s++ )
				row.strings[s] = nif->resolveString( iRow, strFields[s] );
			rows.append( row );
		}

		std::stable_sort( rows.begin(), rows.end(), []( const Row & a, const Row & b ) {
			return QString::compare( a.strings[0], b.strings[0], Qt::CaseInsensitive ) < 0;
		} );

		QPersistentModelIndex pArr( index );
		nifSnapshotOp( nif, Spell::tr( "Sort controlled blocks" ), [&]() {
			QModelIndex iArr( pArr );
			for ( int r = 0; r < rows.count(); r++ ) {
				QModelIndex iRow = nif->getIndex( iArr, r );
				nif->setLink( iRow, "Interpolator", rows[r].interp );
				nif->setLink( iRow, "Controller", rows[r].ctlr );
				if ( nif->getIndex( iRow, "Priority" ).isValid() )
					nif->set<int>( iRow, "Priority", rows[r].priority );
				for ( int s = 0; s < 5; s++ ) {
					if ( nif->getIndex( iRow, strFields[s] ).isValid() )
						nif->assignString( iRow, QString::fromLatin1( strFields[s] ), rows[r].strings[s], false );
				}
			}
		} );

		return index;
	}
};

REGISTER_SPELL( spSortControlledBlocks )


//! Sort an object palette alphabetically by name
class spSortPaletteObjs final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Sort By Name" ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && index.isValid() && nif->itemName( index ) == QLatin1String( "Objs" )
		       && nif->blockInherits( nif->getBlockIndex( index ), "NiDefaultAVObjectPalette" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QVector<QPair<QString, qint32>> rows;
		int n = nif->rowCount( index );
		for ( int r = 0; r < n; r++ ) {
			QModelIndex iRow = nif->getIndex( index, r );
			rows.append( { nif->resolveString( iRow, "Name" ), nif->getLink( iRow, "AV Object" ) } );
		}

		std::stable_sort( rows.begin(), rows.end(), []( const QPair<QString, qint32> & a, const QPair<QString, qint32> & b ) {
			return QString::compare( a.first, b.first, Qt::CaseInsensitive ) < 0;
		} );

		QPersistentModelIndex pArr( index );
		nifSnapshotOp( nif, Spell::tr( "Sort palette objects" ), [&]() {
			QModelIndex iArr( pArr );
			for ( int r = 0; r < rows.count(); r++ ) {
				QModelIndex iRow = nif->getIndex( iArr, r );
				nif->assignString( iRow, QStringLiteral( "Name" ), rows[r].first, false );
				nif->setLink( iRow, "AV Object", rows[r].second );
			}
		} );

		return index;
	}
};

REGISTER_SPELL( spSortPaletteObjs )


//! Generate a BSPositionData extra data block from a geometry's vertex positions
class spGeneratePositionData final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Generate BSPositionData" ); }
	QString page() const override final { return Spell::tr( "Animation" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !nif || nif->getBSVersion() < 130 )
			return false;
		QModelIndex iBlock = nif->getBlockIndex( index );
		if ( !nif->blockInherits( iBlock, "NiAVObject" ) )
			return false;
		return nif->getIndex( iBlock, "Vertex Data" ).isValid()
		       || nif->getBlockIndex( nif->getLink( iBlock, "Data" ), "NiGeometryData" ).isValid();
	}

	//! A float whose IEEE half-precision encoding equals the raw bit pattern h.
	//! qfloat16 round-trips all finite bit patterns (including subnormals), so
	//! set<float> on the hfloat array stores exactly these 16 bits on save.
	static float halfBitsToFloat( quint16 h )
	{
		return float( std::bit_cast< qfloat16 >( h ) );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );

		// gather vertex positions + normals + triangles (BSTriShape style or legacy)
		// Vanilla layout (verified against edison_pa_vfx.nif, Edison_Torso_Lightning:0):
		//   numVerts*3 half-floats  - positions
		//   numVerts*3 half-floats  - normals
		//   numTris*3  raw uint16   - triangle indices (NOT half-floats!)
		//   2          raw uint16   - trailing (14, 0) as in vanilla
		// The engine samples emission points from the triangle list; writing
		// zeros there makes every particle spawn at the same spot in game.
		QVector<Vector3> verts;
		QVector<Vector3> norms;
		QVector<Triangle> tris;

		QModelIndex iVData = nif->getIndex( iBlock, "Vertex Data" );
		if ( iVData.isValid() ) {
			for ( int r = 0; r < nif->rowCount( iVData ); r++ ) {
				QModelIndex iRow = nif->getIndex( iVData, r );
				QModelIndex iV = nif->getIndex( iRow, "Vertex" );
				if ( iV.isValid() )
					verts.append( nif->get<Vector3>( iV ) );
				QModelIndex iN = nif->getIndex( iRow, "Normal" );
				if ( iN.isValid() )
					norms.append( nif->get<Vector3>( iN ) );
			}
			tris = nif->getArray<Triangle>( nif->getIndex( iBlock, "Triangles" ) );
		} else {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iBlock, "Data" ), "NiGeometryData" );
			QModelIndex iVerts = nif->getIndex( iData, "Vertices" );
			for ( int r = 0; r < nif->rowCount( iVerts ); r++ )
				verts.append( nif->get<Vector3>( nif->getIndex( iVerts, r ) ) );
			QModelIndex iNorms = nif->getIndex( iData, "Normals" );
			for ( int r = 0; r < nif->rowCount( iNorms ); r++ )
				norms.append( nif->get<Vector3>( nif->getIndex( iNorms, r ) ) );
			tris = nif->getArray<Triangle>( nif->getIndex( iData, "Triangles" ) );
		}
		int numTris = tris.size();

		if ( verts.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "No vertex positions found on this block." ) );
			return index;
		}

		// missing/degenerate normals: compute face-averaged ones from the triangles
		bool haveNorms = ( norms.size() == verts.size() );
		if ( haveNorms ) {
			bool allZero = true;
			for ( const Vector3 & n : norms ) {
				if ( n.squaredLength() > 1.0e-6f ) {
					allZero = false;
					break;
				}
			}
			haveNorms = !allZero;
		}
		if ( !haveNorms ) {
			norms.fill( Vector3(), verts.size() );
			for ( const Triangle & t : tris ) {
				if ( t[0] >= verts.size() || t[1] >= verts.size() || t[2] >= verts.size() )
					continue;
				Vector3 fn = Vector3::crossproduct( verts[t[1]] - verts[t[0]], verts[t[2]] - verts[t[0]] );
				norms[t[0]] += fn;
				norms[t[1]] += fn;
				norms[t[2]] += fn;
			}
			for ( Vector3 & n : norms ) {
				if ( n.squaredLength() > 1.0e-12f )
					n.normalize();
				else
					n = Vector3( 0, 0, 1 );
			}
		}

		int numData = verts.count() * 6 + numTris * 3 + 2;

		QModelIndex result;
		QPersistentModelIndex pBlock( iBlock );

		nifSnapshotOp( nif, Spell::tr( "Generate BSPositionData (%1 vertices)" ).arg( verts.count() ), [&]() {
			QModelIndex iPos = nif->insertNiBlock( "BSPositionData" );
			nif->assignString( iPos, QStringLiteral( "Name" ), QStringLiteral( "POS" ), false );

			nif->set<int>( iPos, "Num Data", numData );
			QModelIndex iArr = nif->getIndex( iPos, "Data" );
			nif->updateArraySize( iArr );

			int nv = verts.count();
			for ( int v = 0; v < nv; v++ ) {
				for ( int c = 0; c < 3; c++ ) {
					nif->set<float>( nif->getIndex( iArr, v * 3 + c ), verts[v][c] );
					nif->set<float>( nif->getIndex( iArr, nv * 3 + v * 3 + c ), norms[v][c] );
				}
			}
			// triangle indices as raw uint16 bit patterns inside the hfloat
			// array (this is what the engine actually reads for emission)
			int triBase = nv * 6;
			for ( int r = 0; r < numTris; r++ ) {
				for ( int c = 0; c < 3; c++ )
					nif->set<float>( nif->getIndex( iArr, triBase + r * 3 + c ),
						halfBitsToFloat( quint16( tris.at( r )[c] ) ) );
			}
			// trailing pair: (14, 0) in every vanilla sample seen so far
			nif->set<float>( nif->getIndex( iArr, triBase + numTris * 3 ), halfBitsToFloat( 14 ) );
			nif->set<float>( nif->getIndex( iArr, triBase + numTris * 3 + 1 ), 0.0f );

			addLink( nif, QModelIndex( pBlock ), "Extra Data List", nif->getBlockNumber( iPos ) );
			result = iPos;
		} );

		return result.isValid() ? result : index;
	}
};

REGISTER_SPELL( spGeneratePositionData )


//! Replace every reference to this block with a reference to another compatible block
class spSwapReferences final : public Spell
{
	//! Recursively rewrite links equal to oldNum with newNum
	static void rewriteLinks( NifModel * nif, const QModelIndex & iParent, qint32 oldNum, qint32 newNum, int depth = 0 )
	{
		if ( depth > 8 )
			return;

		for ( int r = 0; r < nif->rowCount( iParent ); r++ ) {
			QModelIndex iChild = nif->getIndex( iParent, r );
			if ( !iChild.isValid() )
				continue;

			if ( nif->isLink( iChild ) ) {
				if ( nif->getLink( iChild ) == oldNum )
					nif->setLink( iChild, newNum );
			} else if ( nif->rowCount( iChild ) > 0 ) {
				rewriteLinks( nif, iChild, oldNum, newNum, depth + 1 );
			}
		}
	}

public:
	QString name() const override final { return Spell::tr( "Replace References With..." ); }
	QString page() const override final { return Spell::tr( "Block" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && index.isValid() && nif->isNiBlock( nif->getBlockIndex( index ) );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		int oldNum = nif->getBlockNumber( iBlock );

		// base category compatibility filter
		static const char * bases[] = {
			"NiAVObject", "NiProperty", "NiInterpolator", "NiTimeController",
			"NiExtraData", "NiPSysModifier", "BSShaderTextureSet", "NiObject"
		};
		QString baseType;
		for ( const char * b : bases ) {
			if ( nif->blockInherits( iBlock, b ) ) {
				baseType = QString::fromLatin1( b );
				break;
			}
		}

		struct Candidate
		{
			int num;
			QString type;
			QString name;
			QString label;
		};
		QVector<Candidate> cands;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( b == oldNum )
				continue;
			QModelIndex i = nif->getBlockIndex( b );
			if ( !baseType.isEmpty() && !nif->blockInherits( i, baseType ) )
				continue;
			Candidate c;
			c.num = b;
			c.type = nif->itemName( i );
			c.name = nif->resolveString( i, "Name" );
			c.label = QString( "%1  %2" ).arg( b ).arg( c.type );
			if ( !c.name.isEmpty() )
				c.label += QStringLiteral( "  \"" ) + c.name + QStringLiteral( "\"" );
			cands.append( c );
		}

		if ( cands.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "No compatible blocks found." ) );
			return index;
		}

		QDialog dlg;
		dlg.setWindowTitle( name() );
		QVBoxLayout * lay = new QVBoxLayout( &dlg );
		lay->addWidget( new QLabel( Spell::tr( "Every reference to block %1 will instead point to:" ).arg( oldNum ) ) );

		QHBoxLayout * top = new QHBoxLayout;
		QLineEdit * edFilter = new QLineEdit;
		edFilter->setPlaceholderText( Spell::tr( "Filter by name/type, or type a block number" ) );
		QComboBox * cbSort = new QComboBox;
		cbSort->addItems( { Spell::tr( "Sort: Index" ), Spell::tr( "Sort: Name" ), Spell::tr( "Sort: Type" ) } );
		top->addWidget( edFilter, 1 );
		top->addWidget( cbSort );
		lay->addLayout( top );

		QListWidget * list = new QListWidget;
		lay->addWidget( list, 1 );

		QDialogButtonBox * bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
		lay->addWidget( bb );
		QObject::connect( bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
		QObject::connect( bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
		QObject::connect( list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept );

		auto repopulate = [&]() {
			QVector<Candidate> sorted = cands;
			int mode = cbSort->currentIndex();
			std::stable_sort( sorted.begin(), sorted.end(), [mode]( const Candidate & a, const Candidate & b ) {
				if ( mode == 1 )
					return a.name.compare( b.name, Qt::CaseInsensitive ) < 0;
				if ( mode == 2 )
					return a.type.compare( b.type, Qt::CaseInsensitive ) < 0;
				return a.num < b.num;
			} );
			QString f = edFilter->text().trimmed();
			list->clear();
			for ( const Candidate & c : sorted ) {
				if ( !f.isEmpty() && !c.label.contains( f, Qt::CaseInsensitive ) )
					continue;
				QListWidgetItem * it = new QListWidgetItem( c.label, list );
				it->setData( Qt::UserRole, c.num );
			}
			if ( list->count() > 0 )
				list->setCurrentRow( 0 );
		};
		QObject::connect( edFilter, &QLineEdit::textChanged, repopulate );
		QObject::connect( cbSort, QOverload<int>::of( &QComboBox::currentIndexChanged ), repopulate );
		QObject::connect( edFilter, &QLineEdit::returnPressed, &dlg, &QDialog::accept );
		repopulate();
		dlg.resize( 460, 420 );
		edFilter->setFocus();

		if ( dlg.exec() != QDialog::Accepted )
			return index;

		int newNum = -1;
		// a typed bare number wins, if it names a compatible block
		bool isNum = false;
		int typedNum = edFilter->text().trimmed().toInt( &isNum );
		if ( isNum ) {
			for ( const Candidate & c : cands ) {
				if ( c.num == typedNum ) {
					newNum = typedNum;
					break;
				}
			}
		}
		if ( newNum < 0 && list->currentItem() )
			newNum = list->currentItem()->data( Qt::UserRole ).toInt();
		if ( newNum < 0 )
			return index;

		nifSnapshotOp( nif, Spell::tr( "Replace references %1 -> %2" ).arg( oldNum ).arg( newNum ), [&]() {
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				if ( b == oldNum )
					continue;
				rewriteLinks( nif, nif->getBlockIndex( b ), oldNum, newNum );
			}
		} );

		return nif->getBlockIndex( newNum );
	}
};

REGISTER_SPELL( spSwapReferences )
