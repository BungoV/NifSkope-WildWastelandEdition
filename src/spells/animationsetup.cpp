/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellbook.h"
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
enum CtlrKind
{
	KindFloat = 0, KindColor, KindBool, KindTransform
};

struct CtlrOption
{
	QString type;      //!< Controller block type
	QString label;     //!< Dialog label
	CtlrKind kind;
	bool hasEffectVar = false;   //!< Offers the effect shader variable combo
	bool hasIntVar = false;      //!< Offers a numeric variable field (lighting shader)
};

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
		bool isProperty = ( iAV != iBlock );

		// available controller types for this block
		QVector<CtlrOption> options;
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
			}
			options.append( { "NiTransformController", Spell::tr( "Transform (position/rotation/scale)" ), KindTransform, false, false } );
			options.append( { "NiVisController", Spell::tr( "Visibility (on/off)" ), KindBool, false, false } );
		}

		// existing sequences
		QStringList seqNames;
		QVector<QPersistentModelIndex> seqIdxs;
		QStringList inSequences;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "NiControllerSequence" ) ) {
				QString sn = nif->resolveString( i, "Name" );
				seqNames << sn;
				seqIdxs.append( i );

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
			"Use the Timeline to edit them, or its channel copy/paste to clone keys from another lane." ), &dlg ) );

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

		bool useSequence = !rbNone->isChecked();
		bool newSeq = rbNew->isChecked();
		QString seqName = newSeq ? newNameEdit->text().trimmed() : seqCombo->currentText();
		int existingSeqPick = seqCombo->currentIndex();

		if ( useSequence && newSeq && seqName.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Sequence name must not be empty." ) );
			return index;
		}

		QPersistentModelIndex pBlock( iBlock );
		QPersistentModelIndex pAV( iAV );

		nifSnapshotOp( nif, Spell::tr( "Setup controllers on %1" ).arg( nodeName ), [&]() {
			QModelIndex block( pBlock );
			QModelIndex av( pAV );
			float start = 0.0f, stop = 1.0f;

			QModelIndex iManager, iSeq;
			if ( useSequence ) {
				iManager = ensureControllerManager( nif );
				if ( !iManager.isValid() )
					return;

				if ( newSeq ) {
					iSeq = createSequence( nif, iManager, seqName, start, stop );
				} else if ( existingSeqPick >= 0 && existingSeqPick < seqIdxs.count() ) {
					iSeq = QModelIndex( seqIdxs[existingSeqPick] );
					start = nif->get<float>( iSeq, "Start Time" );
					stop = nif->get<float>( iSeq, "Stop Time" );
				}
			}

			for ( int optIdx : chosen ) {
				const CtlrOption & opt = options[optIdx];

				// the controller attached to the target (with its own pose interpolator)
				QModelIndex iCtlr;
				int ctlrNum = -1;

				if ( opt.kind == KindTransform && useSequence ) {
					// sequence-driven transforms use the manager's multi target controller
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
					nif->set<int>( iCtlr, "Flags", useSequence ? 0x004C : 0x0008 );
					nif->set<float>( iCtlr, "Frequency", 1.0f );
					nif->set<float>( iCtlr, "Start Time", start );
					nif->set<float>( iCtlr, "Stop Time", stop );
					nif->setLink( iCtlr, "Target", nif->getBlockNumber( block ) );

					if ( opt.hasEffectVar && effectVarBox )
						nif->set<int>( iCtlr, "Controlled Variable", effectVarBox->currentData().toInt() );
					if ( opt.hasIntVar && intVarBox ) {
						if ( nif->getIndex( iCtlr, "Controlled Variable" ).isValid() )
							nif->set<int>( iCtlr, "Controlled Variable", intVarBox->value() );
					}

					int poseInterp = createInterpolator( nif, opt.kind, start, stop, av );
					nif->setLink( iCtlr, "Interpolator", poseInterp );

					attachControllerToChain( nif, block, ctlrNum );
				}

				if ( useSequence && iSeq.isValid() ) {
					int seqInterp = createInterpolator( nif, opt.kind, start, stop, av );
					QString propType = isProperty ? nif->itemName( block ) : QString();
					QString ctype = ( opt.kind == KindTransform ) ? QStringLiteral( "NiTransformController" ) : opt.type;
					appendControlledBlock( nif, iSeq, seqInterp, ctlrNum, nodeName, propType, ctype );
					ensurePaletteEntry( nif, iManager, nodeName, nif->getBlockNumber( av ) );
				}
			}
		} );

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
			for ( int b : del )
				nif->removeNiBlock( b );
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
