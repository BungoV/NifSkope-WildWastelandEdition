/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "freezeanim.h"

#include "gl/glcontroller.h"
#include "gl/controllers.h"
#include "model/nifmodel.h"
#include "nifsnapshot.h"

#include <QHash>
#include <QSet>

#include <cmath>
#include <limits>

/*! \file freezeanim.cpp
 * \brief Bake a named animation sequence to a still.
 *
 * The rule this follows throughout: **what freezes is what the viewport shows.**
 * Every evaluation here mirrors the matching class in gl/controllers.cpp — same
 * interpolation, same ctrlTime mapping, same Controlled Variable switch — and
 * writes the result into the NIF field that the runtime object's member came
 * from. A freeze that disagreed with the viewport would be worse than none,
 * because the whole point is picking a moment by looking at it.
 */

namespace
{

// ---------------------------------------------------------------------------
// lookups
// ---------------------------------------------------------------------------

//! Which block owns each controller, by walking every block's Controller chain.
/*!
 * NiTimeController carries its own `Target` back-pointer, and it would be the
 * obvious thing to use — but FO4 leaves it at -1 for the shader property
 * controllers (measured: X01_Torso_Tesla_VFX block 26 `Target = -1`), so the
 * back-pointer is not dependable and the forward chain is the only direction
 * that always holds.
 */
QHash<int, int> controllerOwners( const NifModel * nif )
{
	QHash<int, int> owner;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() )
			continue;
		int c = nif->getLink( idx, "Controller" );
		QSet<int> guard;
		while ( c >= 0 && !guard.contains( c ) ) {
			guard << c;
			if ( !owner.contains( c ) )
				owner.insert( c, b );
			c = nif->getLink( nif->getBlockIndex( c ), "Next Controller" );
		}
	}
	return owner;
}

//! Does this controller run a simulation rather than set a field?
/*! A particle system's state at time t is a cloud of generated vertices and a
 *  procedural arc is a generated ribbon; neither is a value that can be written
 *  back into the file. These survive a freeze untouched — turning them into a
 *  still means snapshotting their geometry, which belongs to the loading-screen
 *  convert. */
bool drivesSimulation( const NifModel * nif, const QModelIndex & idx )
{
	return nif->blockInherits( idx, "NiPSysModifierCtlr" )
	       || nif->blockInherits( idx, "NiParticleSystemController" )
	       || nif->blockInherits( idx, { "NiPSysUpdateCtlr", "NiPSysEmitterCtlr",
	                                     "BSProceduralLightningController", "NiPSysResetOnLoopCtlr" } );
}

//! Which property a controller type must be attached to, when the row does not say.
/*! Empty means "the node itself" — NiVisController and NiLightDimmerController
 *  drive the NiAVObject, not a property on it. */
QString impliedPropertyType( const QString & controllerType )
{
	if ( controllerType.startsWith( QLatin1String( "BSEffectShaderProperty" ) ) )
		return QStringLiteral( "BSEffectShaderProperty" );
	if ( controllerType.startsWith( QLatin1String( "BSLightingShaderProperty" ) ) )
		return QStringLiteral( "BSLightingShaderProperty" );
	if ( controllerType == QLatin1String( "BSNiAlphaPropertyTestRefController" ) )
		return QStringLiteral( "NiAlphaProperty" );
	if ( controllerType == QLatin1String( "NiAlphaController" ) )
		return QStringLiteral( "NiMaterialProperty" );
	return QString();
}

//! The property of RTTI type `propType` hanging off a node; -1 when there is none.
/*! FO4 shapes name their two properties with dedicated refs; everything older
 *  uses the Properties link array. Both are searched. */
int propertyOnNode( const NifModel * nif, int node, const QString & propType )
{
	QModelIndex iNode = nif->getBlockIndex( node );
	if ( !iNode.isValid() || propType.isEmpty() )
		return -1;

	for ( const char * field : { "Shader Property", "Alpha Property" } ) {
		const int l = nif->getLink( iNode, field );
		if ( l >= 0 && nif->blockInherits( nif->getBlockIndex( l ), propType ) )
			return l;
	}
	for ( const qint32 l : nif->getLinkArray( iNode, "Properties" ) ) {
		if ( l >= 0 && nif->blockInherits( nif->getBlockIndex( l ), propType ) )
			return l;
	}
	return -1;
}

//! First NiAVObject called `name`; -1 when there is none.
int nodeByName( const NifModel * nif, const QString & name, int * duplicates = nullptr )
{
	int found = -1;
	int count = 0;
	if ( name.isEmpty() )
		return -1;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		if ( nif->get<QString>( idx, "Name" ) != name )
			continue;
		count++;
		if ( found < 0 )
			found = b;
	}
	if ( duplicates )
		*duplicates = count;
	return found;
}

// ---------------------------------------------------------------------------
// time
// ---------------------------------------------------------------------------

//! How a controller reads its clock — Controller::ctrlTime, without needing an
//! instance of the abstract Controller to own it.
float ctrlTime( float time, float start, float stop, float phase, float frequency, int extrapolation )
{
	time = frequency * time + phase;

	if ( time >= start && time <= stop )
		return time;

	const float delta = stop - start;
	if ( delta <= 0 )
		return start;

	switch ( extrapolation ) {
	case 0: // Cyclic
		{
			const float x = ( time - start ) / delta;
			return start + ( x - std::floor( x ) ) * delta;
		}
	case 1: // Reverse
		{
			const float x = ( time - start ) / delta;
			const float y = ( x - std::floor( x ) ) * delta;
			if ( ( ( (int)std::fabs( std::floor( x ) ) ) & 1 ) == 0 )
				return start + y;
			return stop - y;
		}
	default: // Constant
		return time < start ? start : stop;
	}
}

//! A controller's clock parameters, as Controller::update() reads them.
struct Clock
{
	float start = 0, stop = 0, phase = 0, frequency = 1;
	int extrapolation = 0;
	bool active = false;

	float at( float time ) const { return ctrlTime( time, start, stop, phase, frequency, extrapolation ); }
};

Clock clockOf( const NifModel * nif, const QModelIndex & iCtrl )
{
	Clock c;
	c.start = nif->get<float>( iCtrl, "Start Time" );
	c.stop = nif->get<float>( iCtrl, "Stop Time" );
	c.phase = nif->get<float>( iCtrl, "Phase" );
	c.frequency = nif->get<float>( iCtrl, "Frequency" );
	const int flags = nif->get<int>( iCtrl, "Flags" );
	c.active = flags & 0x08;
	c.extrapolation = ( flags & 0x06 ) >> 1;
	return c;
}

// ---------------------------------------------------------------------------
// evaluation
// ---------------------------------------------------------------------------

bool isSentinel( float v )
{
	return !( v == v ) || std::fabs( v ) >= std::numeric_limits<float>::max();
}

//! Keys first, then the interpolator's own constant Value.
/*! A NiFloatInterpolator with Data set parks Value at FLT_MAX to say "read the
 *  keys"; one without Data carries the value itself. NiBlendFloatInterpolator
 *  has only the Value. All three land here. */
bool evalFloat( const NifModel * nif, const QModelIndex & iInterp, float t, float * out )
{
	if ( !iInterp.isValid() )
		return false;

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
	int last = 0;
	if ( iData.isValid() && Controller::interpolate( *out, iData, "Data", t, last ) )
		return true;

	if ( nif->getIndex( iInterp, "Value" ).isValid() ) {
		const float v = nif->get<float>( iInterp, "Value" );
		if ( !isSentinel( v ) ) {
			*out = v;
			return true;
		}
	}
	return false;
}

bool evalVector3( const NifModel * nif, const QModelIndex & iInterp, float t, Vector3 * out )
{
	if ( !iInterp.isValid() )
		return false;

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
	int last = 0;
	if ( iData.isValid() && Controller::interpolate( *out, iData, "Data", t, last ) )
		return true;

	if ( nif->getIndex( iInterp, "Value" ).isValid() ) {
		const Vector3 v = nif->get<Vector3>( iInterp, "Value" );
		if ( !isSentinel( v[0] ) ) {
			*out = v;
			return true;
		}
	}
	return false;
}

bool evalBool( const NifModel * nif, const QModelIndex & iInterp, float t, bool * out )
{
	if ( !iInterp.isValid() )
		return false;

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
	int last = 0;
	if ( iData.isValid() && Controller::interpolate( *out, iData, "Data", t, last ) )
		return true;

	if ( nif->getIndex( iInterp, "Value" ).isValid() ) {
		*out = nif->get<int>( iInterp, "Value" ) != 0;
		return true;
	}
	return false;
}

//! One posed transform channel set. A channel with no keys stays untouched, so
//! the node keeps whatever it already had — which is what
//! TransformInterpolator::updateTransform does, since it is handed the node's
//! own local transform to overwrite in place.
struct PosedTransform
{
	bool hasTrans = false, hasRot = false, hasScale = false;
	Vector3 translation;
	Matrix rotation;
	float scale = 1.0f;
};

PosedTransform evalTransform( const NifModel * nif, const QModelIndex & iInterp, float t )
{
	PosedTransform p;
	if ( !iInterp.isValid() )
		return p;

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ), "NiKeyframeData" );
	if ( iData.isValid() ) {
		QModelIndex iRot = nif->getIndex( iData, "Rotations" );
		if ( !iRot.isValid() )
			iRot = iData;   // NiKeyframeData holds the rotation keys itself

		int l = 0;
		p.hasRot = Controller::interpolate( p.rotation, iRot, t, l );
		l = 0;
		p.hasTrans = Controller::interpolate( p.translation, nif->getIndex( iData, "Translations" ), t, l );
		l = 0;
		p.hasScale = Controller::interpolate( p.scale, nif->getIndex( iData, "Scales" ), t, l );
	}

	// A data-less NiTransformInterpolator carries the pose in its own Transform.
	// The renderer ignores that case (it only ever overwrites from keys), but our
	// own pose library writes exactly this shape, and AnimSetup::readPose reads
	// it, so freezing has to as well or freezing a pose would be a no-op.
	QModelIndex iTM = nif->getIndex( iInterp, "Transform" );
	if ( iTM.isValid() ) {
		if ( !p.hasTrans ) {
			const Vector3 v = nif->get<Vector3>( iTM, "Translation" );
			if ( !isSentinel( v[0] ) ) {
				p.translation = v;
				p.hasTrans = true;
			}
		}
		if ( !p.hasScale ) {
			const float s = nif->get<float>( iTM, "Scale" );
			if ( !isSentinel( s ) ) {
				p.scale = s;
				p.hasScale = true;
			}
		}
		if ( !p.hasRot ) {
			const Quat q = nif->get<Quat>( nif->getIndex( iTM, "Rotation" ) );
			if ( !isSentinel( q[0] ) ) {
				p.rotation.fromQuat( q );
				p.hasRot = true;
			}
		}
	}

	return p;
}

// ---------------------------------------------------------------------------
// writing
// ---------------------------------------------------------------------------

//! Set one component of a TexCoord field.
/*! nif.xml declares TexCoord as a struct of u and v, but NifValue maps the whole
 *  type to tVector2 — so it is one leaf value, and there are no child rows to
 *  address. It has to be read, edited and written whole. */
bool setUV( NifModel * nif, const QModelIndex & iProp, const char * field, int comp, float v )
{
	QModelIndex i = nif->getIndex( iProp, field );
	if ( !i.isValid() )
		return false;
	Vector2 uv = nif->get<Vector2>( i );
	uv[comp] = v;
	return nif->set<Vector2>( i, uv );
}

//! Does this shader property take its parameters from a .bgem/.bgsm?
/*! When it does, the NIF's own UV Offset / Base Color Scale / Glossiness rows are
 *  dead weight: BSShaderLightingProperty::setMaterial builds a Material from the
 *  file and updateParams reads every parameter off THAT, ignoring the block.
 *  Baking into the block would write a number nothing reads. */
QString materialBacking( const NifModel * nif, const QModelIndex & iProp )
{
	const QString name = nif->get<QString>( iProp, "Name" );
	if ( name.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive )
	     || name.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive ) )
		return name;
	return QString();
}

//! BSEffectShaderProperty, by EffectShaderControlledVariable.
/*! Mirrors EffectFloatController::updateTime; each runtime member is written
 *  back to the NIF field it was loaded from. */
bool writeEffectFloat( NifModel * nif, const QModelIndex & iProp, int variable, float val )
{
	const bool f76 = nif->getBSVersion() >= 151;

	switch ( variable ) {
	case EffectFloat::Emissive_Multiple:
		return nif->set<float>( iProp, "Base Color Scale", val );
	case EffectFloat::Falloff_Start_Angle:
		return nif->set<float>( iProp, "Falloff Start Angle", val );
	case EffectFloat::Falloff_Stop_Angle:
		return nif->set<float>( iProp, "Falloff Stop Angle", val );
	case EffectFloat::Falloff_Start_Opacity:
		return nif->set<float>( iProp, "Falloff Start Opacity", val );
	case EffectFloat::Falloff_Stop_Opacity:
		return nif->set<float>( iProp, "Falloff Stop Opacity", val );
	case EffectFloat::Alpha:
		{
			Color4 c = nif->get<Color4>( iProp, "Base Color" );
			c.setAlpha( val );
			return nif->set<Color4>( iProp, "Base Color", c );
		}
	case EffectFloat::U_Offset_F76:
		if ( !f76 ) return false;
		[[fallthrough]];
	case EffectFloat::U_Offset:
		return setUV( nif, iProp, "UV Offset", 0, val );
	case EffectFloat::U_Scale_F76:
		if ( !f76 ) return false;
		[[fallthrough]];
	case EffectFloat::U_Scale:
		return setUV( nif, iProp, "UV Scale", 0, val );
	case EffectFloat::V_Offset_F76:
		if ( !f76 ) return false;
		[[fallthrough]];
	case EffectFloat::V_Offset:
		return setUV( nif, iProp, "UV Offset", 1, val );
	case EffectFloat::V_Scale_F76:
		if ( !f76 ) return false;
		[[fallthrough]];
	case EffectFloat::V_Scale:
		return setUV( nif, iProp, "UV Scale", 1, val );
	default:
		return false;
	}
}

//! BSLightingShaderProperty, by LightingShaderControlledVariable.
bool writeLightingFloat( NifModel * nif, const QModelIndex & iProp, int variable, float val )
{
	switch ( variable ) {
	case LightingFloat::Refraction_Strength:
		return nif->set<float>( iProp, "Refraction Strength", val );
	case LightingFloat::Reflection_Strength:
		return nif->set<float>( iProp, "Environment Map Scale", val );
	case LightingFloat::Glossiness:
		// FO76 renamed the field; both spellings are tried so this stays correct
		// either side of that split.
		if ( nif->getIndex( iProp, "Glossiness" ).isValid() )
			return nif->set<float>( iProp, "Glossiness", val );
		return nif->set<float>( iProp, "Smoothness", val );
	case LightingFloat::Specular_Strength:
		return nif->set<float>( iProp, "Specular Strength", val );
	case LightingFloat::Emissive_Multiple_F76:
		if ( nif->getBSVersion() < 151 )
			return false;
		[[fallthrough]];
	case LightingFloat::Emissive_Multiple:
		return nif->set<float>( iProp, "Emissive Multiple", val );
	case LightingFloat::Alpha:
		return nif->set<float>( iProp, "Alpha", val );
	case LightingFloat::U_Offset:
		return setUV( nif, iProp, "UV Offset", 0, val );
	case LightingFloat::U_Scale:
		return setUV( nif, iProp, "UV Scale", 0, val );
	case LightingFloat::V_Offset:
		return setUV( nif, iProp, "UV Offset", 1, val );
	case LightingFloat::V_Scale:
		return setUV( nif, iProp, "UV Scale", 1, val );
	default:
		return false;
	}
}

//! A colour controller's Controlled Color, for either shader property family.
bool writeShaderColor( NifModel * nif, const QModelIndex & iProp, bool lighting, int variable, const Vector3 & val )
{
	if ( lighting ) {
		switch ( variable ) {
		case 0:
			return nif->set<Color3>( iProp, "Specular Color", Color3( val[0], val[1], val[2] ) );
		case 1:
			return nif->set<Color3>( iProp, "Emissive Color", Color3( val[0], val[1], val[2] ) );
		default:
			return false;
		}
	}

	if ( variable != 0 )
		return false;
	// The effect property keeps colour and alpha in one field; only rgb is driven.
	Color4 c = nif->get<Color4>( iProp, "Base Color" );
	return nif->set<Color4>( iProp, "Base Color", Color4( val[0], val[1], val[2], c.alpha() ) );
}

} // namespace

// ---------------------------------------------------------------------------
// public
// ---------------------------------------------------------------------------

namespace FreezeAnim
{

bool sequenceRange( const NifModel * nif, const QString & sequence, float * start, float * stop )
{
	if ( !nif )
		return false;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b, "NiControllerSequence" );
		if ( !idx.isValid() || nif->get<QString>( idx, "Name" ) != sequence )
			continue;
		if ( start )
			*start = nif->get<float>( idx, "Start Time" );
		if ( stop )
			*stop = nif->get<float>( idx, "Stop Time" );
		return true;
	}
	return false;
}

Result freeze( NifModel * nif, const QString & sequence, float time, bool stripGraph, QString * error )
{
	Result res;
	auto fail = [&]( const QString & why ) {
		if ( error )
			*error = why;
		return res;
	};

	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	QModelIndex iSeq;
	for ( int b = 0; b < nif->getBlockCount() && !iSeq.isValid(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b, "NiControllerSequence" );
		if ( idx.isValid() && nif->get<QString>( idx, "Name" ) == sequence )
			iSeq = idx;
	}
	if ( !iSeq.isValid() )
		return fail( QStringLiteral( "no sequence named '%1'" ).arg( sequence ) );

	// The sequence's clock, which is what setSequence() pushes onto the
	// transform controllers it drives.
	Clock seqClock;
	seqClock.start = nif->get<float>( iSeq, "Start Time" );
	seqClock.stop = nif->get<float>( iSeq, "Stop Time" );
	seqClock.frequency = nif->get<float>( iSeq, "Frequency" );
	seqClock.active = true;
	if ( seqClock.frequency == 0.0f )
		seqClock.frequency = 1.0f;

	const QHash<int, int> owners = controllerOwners( nif );

	QSet<QString> unhandledSeen;
	QSet<int> bakedControllers;

	nifSnapshotOp( nif, QStringLiteral( "Freeze '%1' at %2s" ).arg( sequence ).arg( time, 0, 'f', 3 ), [&]() {

		QModelIndex iBlocks = nif->getIndex( iSeq, "Controlled Blocks" );
		const int rows = nif->rowCount( iBlocks );

		for ( int r = 0; r < rows; r++ ) {
			QModelIndex iCB = nif->getIndex( iBlocks, r );
			QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iCB, "Interpolator" ) );
			const int ctrlBlock = nif->getLink( iCB, "Controller" );
			QModelIndex iCtrl = nif->getBlockIndex( ctrlBlock );

			// The block type is the truth; the Controller Type string is the
			// fallback for the (rare) row with no Controller link.
			QString type = iCtrl.isValid() ? nif->itemName( iCtrl )
			                               : nif->resolveString( iCB, "Controller Type" );

			auto skip = [&]( const QString & why ) {
				res.skipped++;
				const QString line = QStringLiteral( "%1 — %2" ).arg( type.isEmpty() ? QStringLiteral( "?" ) : type, why );
				if ( !unhandledSeen.contains( line ) ) {
					unhandledSeen << line;
					res.unhandled << line;
				}
			};

			if ( !iInterp.isValid() ) {
				skip( QStringLiteral( "no interpolator" ) );
				continue;
			}

			// A NiBlendInterpolator is a runtime mixing slot, not a curve — it
			// holds whatever other sequences blended into it this frame, and at
			// rest carries the FLT_MAX sentinel. There is no value at time t to
			// bake, and the viewport shows none either.
			if ( nif->blockInherits( iInterp, "NiBlendInterpolator" ) ) {
				skip( QStringLiteral( "blend interpolator — mixes at runtime, holds no keys" ) );
				continue;
			}

			// --- transforms: the node named by the row, not the controller's owner.
			// One NiMultiTargetTransformController drives many nodes, and each
			// controlled block names which.
			if ( type == QLatin1String( "NiMultiTargetTransformController" )
			     || nif->blockInherits( iCtrl, "NiKeyframeController" )
			     || type == QLatin1String( "NiTransformController" ) )
			{
				const QString nodeName = nif->resolveString( iCB, "Node Name" );
				int dups = 0;
				const int node = nodeByName( nif, nodeName, &dups );
				if ( node < 0 ) {
					skip( QStringLiteral( "no node named '%1'" ).arg( nodeName ) );
					continue;
				}
				if ( dups > 1 )
					res.notes << QStringLiteral( "'%1' names %2 nodes; froze the first" ).arg( nodeName ).arg( dups );

				const PosedTransform p = evalTransform( nif, iInterp, seqClock.at( time ) );
				if ( !( p.hasTrans || p.hasRot || p.hasScale ) ) {
					skip( QStringLiteral( "interpolator holds no transform" ) );
					continue;
				}

				QModelIndex iNode = nif->getBlockIndex( node );
				if ( p.hasTrans )
					nif->set<Vector3>( iNode, "Translation", p.translation );
				if ( p.hasRot )
					nif->set<Matrix>( iNode, "Rotation", p.rotation );
				if ( p.hasScale )
					nif->set<float>( iNode, "Scale", p.scale );

				res.baked++;
				if ( ctrlBlock >= 0 )
					bakedControllers << ctrlBlock;
				continue;
			}

			if ( !iCtrl.isValid() ) {
				skip( QStringLiteral( "no controller block" ) );
				continue;
			}

			const Clock clock = clockOf( nif, iCtrl );
			if ( !clock.active ) {
				// An inactive controller does not animate in the viewport, so the
				// file already shows its frozen value. Leaving it alone is the
				// freeze.
				skip( QStringLiteral( "controller is not active" ) );
				continue;
			}
			const float t = clock.at( time );

			// Everything else drives a field on the block the controller is
			// attached to. Two idioms turn up, and X01_Torso_Tesla_VFX has BOTH:
			// `autoPlay` names the controller instance actually hanging off the
			// target (light 117 -> controller 1), while `autoLoop` carries its
			// own instances (controller 13) that are attached to nothing. So the
			// Controller chain is tried first, and the row's own Node Name +
			// Property Type — the addressing the format guarantees — is the
			// fallback that catches the second kind.
			int ownerBlock = owners.value( ctrlBlock, -1 );
			QString ownerWhy;
			if ( ownerBlock < 0 ) {
				const QString nodeName = nif->resolveString( iCB, "Node Name" );
				const int node = nodeByName( nif, nodeName );
				if ( node < 0 ) {
					ownerWhy = QStringLiteral( "this file has no node named '%1'" ).arg( nodeName );
				} else {
					QString propType = nif->resolveString( iCB, "Property Type" );
					if ( propType == QLatin1String( "<empty>" ) )
						propType.clear();
					// Property Type is optional and frequently blank, but the
					// controller type says which property it must be.
					if ( propType.isEmpty() )
						propType = impliedPropertyType( type );
					ownerBlock = propType.isEmpty() ? node : propertyOnNode( nif, node, propType );
					if ( ownerBlock < 0 )
						ownerWhy = QStringLiteral( "node '%1' has no %2" ).arg( nodeName, propType );
				}
			}
			QModelIndex iOwner = nif->getBlockIndex( ownerBlock );
			if ( ownerWhy.isEmpty() )
				ownerWhy = QStringLiteral( "not on any Controller chain" );

			if ( type == QLatin1String( "BSEffectShaderPropertyFloatController" )
			     || type == QLatin1String( "BSLightingShaderPropertyFloatController" ) )
			{
				if ( !iOwner.isValid() ) {
					skip( QStringLiteral( "nothing to write to — %1" ).arg( ownerWhy ) );
					continue;
				}
				if ( const QString mat = materialBacking( nif, iOwner ); !mat.isEmpty() ) {
					skip( QStringLiteral( "property reads its parameters from %1 — the NIF's own rows are ignored, "
					                      "so there is nowhere in this file to put the frozen value" ).arg( mat ) );
					continue;
				}
				float v = 0;
				if ( !evalFloat( nif, iInterp, t, &v ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				const int var = nif->get<int>( iCtrl, "Controlled Variable" );
				const bool lighting = type.startsWith( QLatin1String( "BSLighting" ) );
				const bool wrote = lighting ? writeLightingFloat( nif, iOwner, var, v )
				                            : writeEffectFloat( nif, iOwner, var, v );
				if ( !wrote ) {
					skip( QStringLiteral( "Controlled Variable %1 has no field in this file's version" ).arg( var ) );
					continue;
				}
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			if ( type == QLatin1String( "BSEffectShaderPropertyColorController" )
			     || type == QLatin1String( "BSLightingShaderPropertyColorController" ) )
			{
				if ( !iOwner.isValid() ) {
					skip( QStringLiteral( "nothing to write to — %1" ).arg( ownerWhy ) );
					continue;
				}
				if ( const QString mat = materialBacking( nif, iOwner ); !mat.isEmpty() ) {
					skip( QStringLiteral( "property reads its parameters from %1 — the NIF's own rows are ignored, "
					                      "so there is nowhere in this file to put the frozen value" ).arg( mat ) );
					continue;
				}
				Vector3 v;
				if ( !evalVector3( nif, iInterp, t, &v ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				const int var = nif->get<int>( iCtrl, "Controlled Color" );
				if ( !writeShaderColor( nif, iOwner, type.startsWith( QLatin1String( "BSLighting" ) ), var, v ) ) {
					skip( QStringLiteral( "Controlled Color %1 has no field" ).arg( var ) );
					continue;
				}
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			if ( type == QLatin1String( "NiLightDimmerController" ) ) {
				if ( !nif->blockInherits( iOwner, "NiLight" ) ) {
					skip( QStringLiteral( "controller does not own a light" ) );
					continue;
				}
				float v = 0;
				if ( !evalFloat( nif, iInterp, t, &v ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				nif->set<float>( iOwner, "Dimmer", v );
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			if ( type == QLatin1String( "BSNiAlphaPropertyTestRefController" ) ) {
				if ( !nif->blockInherits( iOwner, "NiAlphaProperty" ) ) {
					skip( QStringLiteral( "controller does not own an alpha property" ) );
					continue;
				}
				float v = 0;
				if ( !evalFloat( nif, iInterp, t, &v ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				nif->set<int>( iOwner, "Threshold", qBound( 0, int( v + 0.5f ), 255 ) );
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			if ( type == QLatin1String( "NiAlphaController" ) ) {
				if ( !nif->blockInherits( iOwner, "NiMaterialProperty" ) ) {
					skip( QStringLiteral( "controller does not own a material property" ) );
					continue;
				}
				float v = 0;
				if ( !evalFloat( nif, iInterp, t, &v ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				nif->set<float>( iOwner, "Alpha", v );
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			if ( type == QLatin1String( "NiVisController" ) ) {
				// Visibility is the node's flag bit 0, and the owner here IS the node.
				QModelIndex iNode = iOwner;
				if ( !nif->blockInherits( iNode, "NiAVObject" ) ) {
					const int node = nodeByName( nif, nif->resolveString( iCB, "Node Name" ) );
					iNode = nif->getBlockIndex( node );
				}
				if ( !iNode.isValid() ) {
					skip( QStringLiteral( "no node to hide or show" ) );
					continue;
				}
				bool visible = true;
				if ( !evalBool( nif, iInterp, t, &visible ) ) {
					skip( QStringLiteral( "interpolator holds no value" ) );
					continue;
				}
				int flags = nif->get<int>( iNode, "Flags" );
				flags = visible ? ( flags & ~1 ) : ( flags | 1 );
				nif->set<int>( iNode, "Flags", flags );
				res.baked++;
				bakedControllers << ctrlBlock;
				continue;
			}

			// Simulations, not values. A particle system's state at t is a cloud
			// of generated vertices and a procedural arc is a generated ribbon;
			// neither is a field that can be written back. Turning those into a
			// still means snapshotting their geometry, which belongs to the
			// loading-screen convert. Left running and reported.
			skip( QStringLiteral( "drives a simulation, not a value" ) );
		}

		if ( !stripGraph )
			return;

		// --- strip the controller graph -----------------------------------
		// The sequence machinery, and every controller that drives a VALUE.
		// Leaving a value controller behind would undo the freeze the moment the
		// file plays: the field we just baked would animate away from it. What
		// stays is the controllers that drive a SIMULATION — particles and
		// procedural lightning generate geometry rather than setting a field, so
		// they cannot be baked and they are not competing with anything we wrote.
		QSet<int> doomed;
		QSet<QString> strippedUnbaked;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex idx = nif->getBlockIndex( b );
			if ( !idx.isValid() )
				continue;

			if ( nif->blockInherits( idx, "NiControllerManager" )
			     || nif->blockInherits( idx, "NiControllerSequence" )
			     || nif->blockInherits( idx, "NiDefaultAVObjectPalette" ) ) {
				doomed << b;
				continue;
			}

			if ( !nif->blockInherits( idx, "NiTimeController" ) )
				continue;
			if ( drivesSimulation( nif, idx ) )
				continue;

			doomed << b;
			// Say so when we remove a controller we never baked — either it
			// belongs to a sequence other than the chosen one, or it is a runtime
			// blend slot. Its field keeps whatever the file already stores, which
			// is a real (if usually invisible) change, so it is not swallowed.
			if ( !bakedControllers.contains( b ) ) {
				const QString t = nif->itemName( idx );
				if ( !strippedUnbaked.contains( t ) ) {
					strippedUnbaked << t;
					res.notes << QStringLiteral( "removed %1 without baking it; its field keeps the value already in the file" ).arg( t );
				}
			}
		}

		// Text keys exist only to tag times inside a sequence. Collect first:
		// inserting into `doomed` while walking it re-buckets the QHash behind
		// the live iterator, which can skip or revisit entries.
		QList<int> textKeys;
		for ( int b : std::as_const( doomed ) ) {
			QModelIndex idx = nif->getBlockIndex( b );
			const int keys = nif->getLink( idx, "Text Keys" );
			if ( keys >= 0 )
				textKeys << keys;
		}
		for ( int k : std::as_const( textKeys ) )
			doomed << k;

		// Sweep anything that the removals leave with nothing pointing at it —
		// the interpolators and their key data. Repeated because a NiFloatData is
		// only orphaned once its NiFloatInterpolator has gone.
		auto isAnimLeaf = [&]( const QModelIndex & idx ) {
			return nif->blockInherits( idx, "NiInterpolator" )
			       || nif->blockInherits( idx, "NiAnimationKeyframeData" )
			       || nif->blockInherits( idx, "NiKeyframeData" )
			       || nif->blockInherits( idx, "NiFloatData" )
			       || nif->blockInherits( idx, "NiBoolData" )
			       || nif->blockInherits( idx, "NiPosData" )
			       || nif->blockInherits( idx, "NiColorData" );
		};

		for ( bool grew = true; grew; ) {
			grew = false;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				if ( doomed.contains( b ) )
					continue;
				QModelIndex idx = nif->getBlockIndex( b );
				if ( !idx.isValid() || !isAnimLeaf( idx ) )
					continue;

				bool referenced = false;
				for ( int c = 0; c < nif->getBlockCount() && !referenced; c++ ) {
					if ( c == b || doomed.contains( c ) )
						continue;
					if ( nif->getChildLinks( c ).contains( b ) || nif->getParentLinks( c ).contains( b ) )
						referenced = true;
				}
				if ( !referenced ) {
					doomed << b;
					grew = true;
				}
			}
		}

		// Descending, so the renumbering removeNiBlock does cannot invalidate a
		// number we have not used yet.
		QList<int> order = doomed.values();
		std::sort( order.begin(), order.end(), std::greater<int>() );
		for ( int b : order )
			nif->removeNiBlock( b );
		res.blocksRemoved = order.size();
	} );

	res.ok = true;
	return res;
}

} // namespace FreezeAnim
