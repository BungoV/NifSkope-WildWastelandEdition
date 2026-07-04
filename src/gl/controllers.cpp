/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "controllers.h"

#include "gl/glshape.h"
#include "gl/glparticles.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <cmath>

// `NiControllerManager` blocks

ControllerManager::ControllerManager( Node * node, const QModelIndex & index )
	: Controller( index ), target( node )
{
}

bool ControllerManager::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		if ( target ) {
			Scene * scene = target->scene;
			QVector<qint32> lSequences = nif->getLinkArray( index, "Controller Sequences" );
			for ( const auto l : lSequences ) {
				QModelIndex iSeq = nif->getBlockIndex( l, "NiControllerSequence" );

				if ( iSeq.isValid() ) {
					QString name = nif->get<QString>( iSeq, "Name" );

					if ( !scene->animGroups.contains( name ) ) {
						scene->animGroups.append( name );

						QMap<QString, float> tags = scene->animTags[name];

						QModelIndex iKeys = nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ), "NiTextKeyExtraData" );
						QModelIndex iTags = nif->getIndex( iKeys, "Text Keys" );

						for ( int r = 0; r < nif->rowCount( iTags ); r++ ) {
							tags.insert( nif->get<QString>( nif->getIndex( iTags, r ), "Value" ), nif->get<float>( nif->getIndex( iTags, r ), "Time" ) );
						}

						scene->animTags[name] = tags;
					}
				}
			}
		}

		return true;
	}

	return false;
}

void ControllerManager::setSequence( const QString & seqname )
{
	auto nif = NifModel::fromValidIndex(iBlock);
	if ( nif && target ) {
		MultiTargetTransformController * multiTargetTransformer = 0;
		for ( Controller * c : target->controllers ) {
			if ( c->typeId() == "NiMultiTargetTransformController" ) {
				multiTargetTransformer = static_cast<MultiTargetTransformController *>(c);
				break;
			}
		}

		QVector<qint32> lSequences = nif->getLinkArray( iBlock, "Controller Sequences" );
		for ( const auto l : lSequences ) {
			QModelIndex iSeq = nif->getBlockIndex( l, "NiControllerSequence" );

			if ( iSeq.isValid() && nif->get<QString>( iSeq, "Name" ) == seqname ) {
				start = nif->get<float>( iSeq, "Start Time" );
				stop = nif->get<float>( iSeq, "Stop Time" );
				phase = nif->get<float>( iSeq, "Phase" );
				frequency = nif->get<float>( iSeq, "Frequency" );

				QModelIndex iCtrlBlcks = nif->getIndex( iSeq, "Controlled Blocks" );

				for ( int r = 0; r < nif->rowCount( iCtrlBlcks ); r++ ) {
					QModelIndex iCB = nif->getIndex( iCtrlBlcks, r );

					QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iCB, "Interpolator" ), "NiInterpolator" );

					QModelIndex iController = nif->getBlockIndex( nif->getLink( iCB, "Controller" ), "NiTimeController" );

					QString nodename = nif->get<QString>( iCB, "Node Name" );

					if ( nodename.isEmpty() ) {
						QModelIndex idx = nif->getIndex( iCB, "Node Name Offset" );
						nodename = idx.sibling( idx.row(), NifModel::ValueCol ).data( NifSkopeDisplayRole ).toString();

						if ( nodename.isEmpty() )
							nodename = nif->get<QString>( iCB, "Target Name" );
					}

					QString proptype = nif->get<QString>( iCB, "Property Type" );

					if ( proptype.isEmpty() ) {
						QModelIndex idx = nif->getIndex( iCB, "Property Type Offset" );
						proptype = idx.sibling( idx.row(), NifModel::ValueCol ).data( NifSkopeDisplayRole ).toString();
					}

					QString ctrltype = nif->get<QString>( iCB, "Controller Type" );

					if ( ctrltype.isEmpty() ) {
						QModelIndex idx = nif->getIndex( iCB, "Controller Type Offset" );
						ctrltype = idx.sibling( idx.row(), NifModel::ValueCol ).data( NifSkopeDisplayRole ).toString();

						if ( ctrltype.isEmpty() && iController.isValid() )
							ctrltype = nif->itemName( iController );
					}

					QString var1 = nif->get<QString>( iCB, "Controller ID" );

					if ( var1.isEmpty() ) {
						QModelIndex idx = nif->getIndex( iCB, "Controller ID Offset" );
						var1 = idx.sibling( idx.row(), NifModel::ValueCol ).data( NifSkopeDisplayRole ).toString();
					}

					QString var2 = nif->get<QString>( iCB, "Interpolator ID" );

					if ( var2.isEmpty() ) {
						QModelIndex idx = nif->getIndex( iCB, "Interpolator ID Offset" );
						var2 = idx.sibling( idx.row(), NifModel::ValueCol ).data( NifSkopeDisplayRole ).toString();
					}

					Node * node = target->findChild( nodename );

					if ( !node )
						continue;

					if ( ctrltype == "NiTransformController" && multiTargetTransformer ) {
						if ( multiTargetTransformer->setInterpolatorNode( node, iInterp ) ) {
							multiTargetTransformer->start = start;
							multiTargetTransformer->stop = stop;
							multiTargetTransformer->phase = phase;
							multiTargetTransformer->frequency = frequency;
							continue;
						}
					}

					if ( ctrltype == "BSLightingShaderPropertyFloatController"
						|| ctrltype == "BSLightingShaderPropertyColorController"
						|| ctrltype == "BSEffectShaderPropertyFloatController"
						|| ctrltype == "BSEffectShaderPropertyColorController"
						|| ctrltype == "BSNiAlphaPropertyTestRefController" )
					{
						//qDebug() << node->name;

						auto ctrl = node->findController( proptype, iController );
						if ( ctrl ) {
							ctrl->setInterpolator( iInterp );
						}
						continue;
					}

					Controller * ctrl = node->findController( proptype, ctrltype, var1, var2 );

					if ( ctrl ) {
						ctrl->start = start;
						ctrl->stop = stop;
						ctrl->phase = phase;
						ctrl->frequency = frequency;

						ctrl->setInterpolator( iInterp );
					}
				}
			}
		}
	}
}


// `NiKeyframeController` blocks

KeyframeController::KeyframeController( Node * node, const QModelIndex & index )
	: Controller( index ), target( node ), lTrans( 0 ), lRotate( 0 ), lScale( 0 )
{
}

void KeyframeController::updateTime( float time )
{
	if ( !(active && target) )
		return;

	time = ctrlTime( time );

	interpolate( target->local.rotation, iRotations, time, lRotate );
	interpolate( target->local.translation, iTranslations, time, lTrans );
	interpolate( target->local.scale, iScales, time, lScale );
}

bool KeyframeController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		iTranslations = nif->getIndex( iData, "Translations" );
		iRotations = nif->getIndex( iData, "Rotations" );

		if ( !iRotations.isValid() )
			iRotations = iData;

		iScales = nif->getIndex( iData, "Scales" );
		return true;
	}

	return false;
}


// `NiTransformController` blocks

TransformController::TransformController( Node * node, const QModelIndex & index )
	: Controller( index ), target( node )
{
}

void TransformController::updateTime( float time )
{
	if ( !(active && target) )
		return;

	time = ctrlTime( time );

	if ( interpolator ) {
		interpolator->updateTransform( target->local, time );
	}
}

void TransformController::setInterpolator( const QModelIndex & idx )
{
	auto nif = NifModel::fromValidIndex(idx);
	if ( nif ) {
		if ( interpolator ) {
			delete interpolator;
			interpolator = 0;
		}

		if ( nif->isNiBlock( idx, "NiBSplineCompTransformInterpolator" ) ) {
			iInterpolator = idx;
			interpolator = new BSplineTransformInterpolator( this );
		} else if ( nif->isNiBlock( idx, "NiTransformInterpolator" ) ) {
			iInterpolator = idx;
			interpolator = new TransformInterpolator( this );
		}

		if ( interpolator ) {
			interpolator->update( nif, iInterpolator );
		}
	}
}


// `NiMultiTargetTransformController` blocks

MultiTargetTransformController::MultiTargetTransformController( Node * node, const QModelIndex & index )
	: Controller( index ), target( node )
{
}

void MultiTargetTransformController::updateTime( float time )
{
	if ( !(active && target) )
		return;

	time = ctrlTime( time );

	for ( const TransformTarget& tt : extraTargets ) {
		if ( tt.first && tt.second ) {
			tt.second->updateTransform( tt.first->local, time );
		}
	}
}

bool MultiTargetTransformController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		if ( target ) {
			Scene * scene = target->scene;
			extraTargets.clear();

			QVector<qint32> lTargets = nif->getLinkArray( index, "Extra Targets" );
			for ( const auto l : lTargets ) {
				Node * node = scene->getNode( nif, nif->getBlockIndex( l ) );

				if ( node ) {
					extraTargets.append( TransformTarget( node, 0 ) );
				}
			}
		}

		return true;
	}

#if 0
	for ( const TransformTarget& tt : extraTargets ) {
		// TODO: update the interpolators
	}
#endif

	return false;
}

bool MultiTargetTransformController::setInterpolatorNode( Node * node, const QModelIndex & idx )
{
	auto nif = NifModel::fromValidIndex(idx);
	if ( !nif )
		return false;

	QMutableListIterator<TransformTarget> it( extraTargets );

	while ( it.hasNext() ) {
		it.next();

		auto& val = it.value();
		if ( val.first == node ) {
			if ( val.second ) {
				delete val.second;
				val.second = 0;
			}

			if ( nif->isNiBlock( idx, "NiBSplineCompTransformInterpolator" ) ) {
				val.second = new BSplineTransformInterpolator( this );
			} else if ( nif->isNiBlock( idx, "NiTransformInterpolator" ) ) {
				val.second = new TransformInterpolator( this );
			}

			if ( val.second ) {
				val.second->update( nif, idx );
			}

			return true;
		}
	}

	return false;
}


// `NiVisController` blocks

VisibilityController::VisibilityController( Node * node, const QModelIndex & index )
	: Controller( index ), target( node ), visLast( 0 )
{
}

void VisibilityController::updateTime( float time )
{
	if ( !(active && target) )
		return;

	time = ctrlTime( time );

	bool isVisible;

	if ( interpolate( isVisible, iData, "Data", time, visLast ) ) {
		target->flags.node.hidden = !isVisible;
	}
}

bool VisibilityController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		return true;
	}

	return false;
}


// `NiGeomMorpherController` blocks

MorphController::MorphController( Shape * mesh, const QModelIndex & index )
	: Controller( index ), target( mesh )
{
}

MorphController::~MorphController()
{
	qDeleteAll( morph );
}

void MorphController::updateTime( float time )
{
	if ( !(target && iData.isValid() && active && morph.count() > 1) )
		return;

	time = ctrlTime( time );

	if ( target->verts.count() != morph[0]->verts.count() )
		return;

	target->clearHash();
	target->verts = morph[0]->verts;

	float x;

	for ( int i = 1; i < morph.count(); i++ ) {
		MorphKey * key = morph[i];

		if ( interpolate( x, key->iFrames, time, key->index ) ) {
			if ( x < 0 )
				x = 0;
			if ( x > 1 )
				x = 1;

			if ( x != 0 && target->verts.count() == key->verts.count() ) {
				for ( int v = 0; v < target->verts.count(); v++ )
					target->verts[v] += key->verts[v] * x;
			}
		}
	}

	target->needUpdateBounds = true;
}

bool MorphController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		qDeleteAll( morph );
		morph.clear();

		QModelIndex midx = nif->getIndex( iData, "Morphs" );

		for ( int r = 0; r < nif->rowCount( midx ); r++ ) {
			QModelIndex iInterpolators, iInterpolatorWeights;

			if ( nif->checkVersion( 0, 0x14000005 ) ) {
				iInterpolators = nif->getIndex( iBlock, "Interpolators" );
			} else if ( nif->checkVersion( 0x14010003, 0 ) ) {
				iInterpolatorWeights = nif->getIndex( iBlock, "Interpolator Weights" );
			}

			QModelIndex iKey = nif->getIndex( midx, r );

			MorphKey * key = new MorphKey;
			key->index = 0;

			// this is ugly...
			if ( iInterpolators.isValid() ) {
				key->iFrames = nif->getIndex( nif->getBlockIndex( nif->getLink( nif->getBlockIndex( nif->getLink( nif->getIndex( iInterpolators, r ) ), "NiFloatInterpolator" ), "Data" ), "NiFloatData" ), "Data" );
			} else if ( iInterpolatorWeights.isValid() ) {
				key->iFrames = nif->getIndex( nif->getBlockIndex( nif->getLink( nif->getBlockIndex( nif->getLink( nif->getIndex( iInterpolatorWeights, r ), "Interpolator" ), "NiFloatInterpolator" ), "Data" ), "NiFloatData" ), "Data" );
			} else {
				key->iFrames = iKey;
			}

			key->verts = nif->getArray<Vector3>( nif->getIndex( iKey, "Vectors" ) );

			morph.append( key );
		}

		return true;
	}

	return false;
}


// `NiUVController` blocks

UVController::UVController( Shape * mesh, const QModelIndex & index )
	: Controller( index ), target( mesh )
{
}

UVController::~UVController()
{
}

void UVController::updateTime( float time )
{
	auto nif = NifModel::fromIndex( iData );
	QModelIndex uvGroups = nif->getIndex( iData, "UV Groups" );

	// U trans, V trans, U scale, V scale
	// see NiUVData compound in nif.xml
	float val[4] = { 0.0, 0.0, 1.0, 1.0 };

	if ( uvGroups.isValid() ) {
		for ( int i = 0; i < 4 && i < nif->rowCount( uvGroups ); i++ ) {
			interpolate( val[i], nif->getIndex( uvGroups, i ), ctrlTime( time ), luv );
		}

		target->clearHash();

		// adjust coords; verified in SceneImmerse
		for ( int i = 0; i < target->coords[0].size(); i++ ) {
			// operating on pointers makes this too complicated, so we don't
			Vector2 current = target->coords[0][i];
			// scaling/tiling applied before translation
			// Note that scaling is relative to center!
			current += Vector2( -0.5, -0.5 );
			current = Vector2( current[0] * val[2], current[1] * val[3] );
			current += Vector2( -val[0], val[1] );
			current += Vector2( 0.5, 0.5 );
			target->coords[0][i] = current;
		}
	}
}

bool UVController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		// do stuff here
		return true;
	}

	return false;
}


static float random( float r )
{
	return r * ( float(std::rand()) / float(RAND_MAX) );
}

static Vector3 random( Vector3 v )
{
	v[0] *= random( 1.0f );
	v[1] *= random( 1.0f );
	v[2] *= random( 1.0f );
	return v;
}


// `NiParticleSystemController` and other blocks

ParticleController::ParticleController( Particles * particles, const QModelIndex & index )
	: Controller( index ), target( particles )
{
}

bool ParticleController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( !target )
		return false;

	if ( Controller::update( nif, index ) || (index.isValid() && iExtras.contains( index )) ) {
		emitNode = target->scene->getNode( nif, nif->getBlockIndex( nif->getLink( iBlock, "Emitter" ) ) );
		emitStart = nif->get<float>( iBlock, "Emit Start Time" );
		emitStop = nif->get<float>( iBlock, "Emit Stop Time" );
		emitRate = nif->get<float>( iBlock, "Birth Rate" );
		emitRadius = nif->get<Vector3>( iBlock, "Emitter Dimensions" );
		emitAccu = 0;
		emitLast = emitStart;

		spd = nif->get<float>( iBlock, "Speed" );
		spdRnd = nif->get<float>( iBlock, "Speed Variation" );

		ttl = nif->get<float>( iBlock, "Lifetime" );
		ttlRnd = nif->get<float>( iBlock, "Lifetime Variation" );

		inc = nif->get<float>( iBlock, "Declination" );
		incRnd = nif->get<float>( iBlock, "Declination Variation" );

		dec = nif->get<float>( iBlock, "Planar Angle" );
		decRnd = nif->get<float>( iBlock, "Planar Angle Variation" );

		size = nif->get<float>( iBlock, "Initial Size" );
		grow = 0.0;
		fade = 0.0;

		list.clear();

		QModelIndex iParticles = nif->getIndex( iBlock, "Particles" );

		if ( iParticles.isValid() ) {
			emitMax = nif->get<int>( iBlock, "Num Particles" );
			int numValid = nif->get<int>( iBlock, "Num Valid" );

			//iParticles = nif->getIndex( iParticles, "Particles" );
			//if ( iParticles.isValid() )
			//{
			for ( int p = 0; p < numValid && p < nif->rowCount( iParticles ); p++ ) {
				Particle particle;
				particle.velocity = nif->get<Vector3>( nif->getIndex( iParticles, p ), "Velocity" );
				particle.lifetime = nif->get<float>( nif->getIndex( iParticles, p ), "Age" );
				particle.lifespan = nif->get<float>( nif->getIndex( iParticles, p ), "Life Span" );
				particle.lasttime = nif->get<float>( nif->getIndex( iParticles, p ), "Last Update" );
				particle.vertex = nif->get<int>( nif->getIndex( iParticles, p ), "Code" );
				// Display saved particle start on initial load
				list.append( particle );
			}

			//}
		}

		if ( nif->get<bool>( iBlock, "Use Birth Rate" ) == 0 ) {
			emitRate = emitMax / (ttl + ttlRnd / 2);
		}

		iExtras.clear();
		grav.clear();
		iColorKeys = QModelIndex();
		QModelIndex iExtra = nif->getBlockIndex( nif->getLink( iBlock, "Particle Modifier" ) );

		while ( iExtra.isValid() ) {
			iExtras.append( iExtra );

			QString name = nif->itemName( iExtra );

			if ( name == "NiParticleGrowFade" ) {
				grow = nif->get<float>( iExtra, "Grow" );
				fade = nif->get<float>( iExtra, "Fade" );
			} else if ( name == "NiParticleColorModifier" ) {
				iColorKeys = nif->getIndex( nif->getBlockIndex( nif->getLink( iExtra, "Color Data" ), "NiColorData" ), "Data" );
			} else if ( name == "NiGravity" ) {
				Gravity g;
				g.force = nif->get<float>( iExtra, "Force" );
				g.type = nif->get<int>( iExtra, "Type" );
				g.position = nif->get<Vector3>( iExtra, "Position" );
				g.direction = nif->get<Vector3>( iExtra, "Direction" );
				grav.append( g );
			}

			iExtra = nif->getBlockIndex( nif->getLink( iExtra, "Next Modifier" ) );
		}

		return true;
	}

	return false;
}

void ParticleController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	localtime = ctrlTime( time );

	int n = 0;

	while ( n < list.count() ) {
		Particle & p = list[n];

		float deltaTime = (localtime > p.lasttime ? localtime - p.lasttime : 0); //( stop - start ) - p.lasttime + localtime );

		p.lifetime += deltaTime;

		if ( p.lifetime < p.lifespan && p.vertex < target->verts.count() ) {
			p.position = target->verts[p.vertex];

			for ( int i = 0; i < 4; i++ )
				moveParticle( p, deltaTime / 4 );

			p.lasttime = localtime;
			n++;
		} else {
			list.remove( n );
		}
	}

	if ( emitNode && emitNode->isVisible() && localtime >= emitStart && localtime <= emitStop ) {
		float emitDelta = (localtime > emitLast ? localtime - emitLast : 0);
		emitLast = localtime;

		emitAccu += emitDelta * emitRate;

		int num = int( emitAccu );

		if ( num > 0 ) {
			emitAccu -= num;

			while ( num-- > 0 && list.count() < target->verts.count() ) {
				Particle p;
				startParticle( p );
				list.append( p );
			}
		}
	}

	n = 0;

	while ( n < list.count() ) {
		Particle & p = list[n];
		p.vertex = n;
		target->verts[n] = p.position;

		if ( n < target->sizes.count() )
			sizeParticle( p, target->sizes[n] );

		if ( n < target->colors.count() )
			colorParticle( p, target->colors[n] );

		n++;
	}

	target->active = list.count();
	target->size = size;
}

void ParticleController::startParticle( Particle & p )
{
	p.position = random( emitRadius * 2 ) - emitRadius;
	p.position += target->worldTrans().rotation.inverted() * (emitNode->worldTrans().translation - target->worldTrans().translation);

	float i = inc + random( incRnd );
	float d = dec + random( decRnd );

	p.velocity = Vector3( rand() & 1 ? sin( i ) : -sin( i ), 0, cos( i ) );

	Matrix m; m.fromEuler( 0, 0, rand() & 1 ? d : -d );
	p.velocity = m * p.velocity;

	p.velocity = p.velocity * (spd + random( spdRnd ));
	p.velocity = target->worldTrans().rotation.inverted() * emitNode->worldTrans().rotation * p.velocity;

	p.lifetime = 0;
	p.lifespan = ttl + random( ttlRnd );
	p.lasttime = localtime;
}

void ParticleController::moveParticle( Particle & p, float deltaTime )
{
	for ( Gravity g : grav ) {
		switch ( g.type ) {
		case 0:
			p.velocity += g.direction * (g.force * deltaTime);
			break;
		case 1:
		{
			Vector3 dir = (g.position - p.position);
			dir.normalize();
			p.velocity += dir * (g.force * deltaTime);
		}
		break;
		}
	}
	p.position += p.velocity * deltaTime;
}

void ParticleController::sizeParticle( Particle & p, float & sz )
{
	sz = 1.0;

	if ( grow > 0 && p.lifetime < grow )
		sz *= p.lifetime / grow;

	if ( fade > 0 && p.lifespan - p.lifetime < fade )
		sz *= (p.lifespan - p.lifetime) / fade;
}

void ParticleController::colorParticle( Particle & p, Color4 & color )
{
	if ( iColorKeys.isValid() ) {
		int i = 0;
		interpolate( color, iColorKeys, p.lifetime / p.lifespan, i );
	}
}


/*
 *  PSysSimController - preview simulation of modern NiPSys particle systems
 */

static Color4 tlLerpColor( const Color4 & a, const Color4 & b, float t )
{
	return Color4( a[0] + ( b[0] - a[0] ) * t, a[1] + ( b[1] - a[1] ) * t,
	               a[2] + ( b[2] - a[2] ) * t, a[3] + ( b[3] - a[3] ) * t );
}

PSysSimController::PSysSimController( Particles * particles, const QModelIndex & index )
	: Controller( index ), target( particles )
{
}

bool PSysSimController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( !target )
		return false;

	if ( !( Controller::update( nif, index ) || ( index.isValid() && iExtras.contains( index ) ) ) )
		return false;

	iExtras.clear();
	emitters.clear();
	hasGravity = false;
	gravityStrength = 0;
	dragPct = 0;
	hasColorMod = false;
	scaleKeys.clear();

	QModelIndex iPSys = nif->getBlockIndex( nif->getLink( iBlock, "Target" ) );
	if ( !iPSys.isValid() )
		return true;
	iExtras.append( iPSys );

	maxParticles = nif->get<int>( iPSys, "Num Vertices" );
	if ( maxParticles < 1 )
		maxParticles = 512;
	maxParticles = std::min( maxParticles, 4096 );

	// gather BSPositionData spawn points from a block's extra data list
	auto posDataPoints = [nif, this]( const QModelIndex & iObj, const Transform & wt ) {
		QVector<Vector3> pts;
		QModelIndex iExtraList = nif->getIndex( iObj, "Extra Data List" );
		for ( int r = 0; r < nif->rowCount( iExtraList ); r++ ) {
			QModelIndex iED = nif->getBlockIndex( nif->getLink( nif->getIndex( iExtraList, r ) ) );
			if ( !nif->blockInherits( iED, "BSPositionData" ) )
				continue;
			iExtras.append( iED );
			QVector<float> raw = nif->getArray<float>( nif->getIndex( iED, "Data" ) );
			for ( int i = 0; i + 2 < raw.size(); i += 3 )
				pts.append( wt * Vector3( raw[i], raw[i + 1], raw[i + 2] ) );
			break;
		}
		return pts;
	};

	// modifiers
	QModelIndex iMods = nif->getIndex( iPSys, "Modifiers" );
	for ( int r = 0; r < nif->rowCount( iMods ); r++ ) {
		QModelIndex iMod = nif->getBlockIndex( nif->getLink( nif->getIndex( iMods, r ) ) );
		if ( !iMod.isValid() )
			continue;
		iExtras.append( iMod );
		QString mtype = nif->itemName( iMod );

		if ( nif->blockInherits( iMod, "NiPSysEmitter" ) ) {
			Emitter e;
			e.iBlock = iMod;
			e.name = nif->resolveString( iMod, "Name" );
			e.speed = nif->get<float>( iMod, "Speed" );
			e.speedVar = nif->get<float>( iMod, "Speed Variation" );
			e.declination = nif->get<float>( iMod, "Declination" );
			e.declinationVar = nif->get<float>( iMod, "Declination Variation" );
			e.planar = nif->get<float>( iMod, "Planar Angle" );
			e.planarVar = nif->get<float>( iMod, "Planar Angle Variation" );
			e.color = nif->get<Color4>( iMod, "Initial Color" );
			e.radius = nif->get<float>( iMod, "Initial Radius" );
			e.radiusVar = nif->get<float>( iMod, "Radius Variation" );
			e.lifeSpan = nif->get<float>( iMod, "Life Span" );
			e.lifeSpanVar = nif->get<float>( iMod, "Life Span Variation" );

			QModelIndex iObj = nif->getBlockIndex( nif->getLink( iMod, "Emitter Object" ) );
			if ( iObj.isValid() )
				e.emitNode = target->scene->getNode( nif, iObj );

			if ( mtype == QLatin1String( "NiPSysBoxEmitter" ) ) {
				e.shape = 1;
				e.dims[0] = nif->get<float>( iMod, "Width" );
				e.dims[1] = nif->get<float>( iMod, "Height" );
				e.dims[2] = nif->get<float>( iMod, "Depth" );
			} else if ( mtype == QLatin1String( "NiPSysCylinderEmitter" ) ) {
				e.shape = 2;
				e.dims[0] = nif->get<float>( iMod, "Radius" );
				e.dims[1] = nif->get<float>( iMod, "Height" );
			} else if ( mtype == QLatin1String( "NiPSysSphereEmitter" ) ) {
				e.shape = 3;
				e.dims[0] = nif->get<float>( iMod, "Radius" );
			} else if ( mtype == QLatin1String( "BSPSysArrayEmitter" ) ) {
				e.shape = 4;
				// spawn points come from BSPositionData on the emitter object
				// (falling back to the particle system itself)
				if ( iObj.isValid() ) {
					Node * n = target->scene->getNode( nif, iObj );
					e.points = posDataPoints( iObj, n ? n->worldTrans() : Transform() );
				}
				if ( e.points.isEmpty() )
					e.points = posDataPoints( iPSys, target->worldTrans() );
			} else if ( mtype == QLatin1String( "NiPSysMeshEmitter" ) ) {
				e.shape = 4;
				QModelIndex iMeshes = nif->getIndex( iMod, "Emitter Meshes" );
				for ( int m = 0; m < nif->rowCount( iMeshes ); m++ ) {
					QModelIndex iMesh = nif->getBlockIndex( nif->getLink( nif->getIndex( iMeshes, m ) ) );
					if ( !iMesh.isValid() )
						continue;
					Node * n = target->scene->getNode( nif, iMesh );
					Transform wt = n ? n->worldTrans() : Transform();
					if ( !e.emitNode )
						e.emitNode = n;
					QModelIndex iVD = nif->getIndex( iMesh, "Vertex Data" );
					if ( iVD.isValid() ) {
						for ( int v = 0; v < nif->rowCount( iVD ) && e.points.size() < 4096; v++ )
							e.points.append( wt * nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) );
					} else {
						QModelIndex iVerts = nif->getIndex(
							nif->getBlockIndex( nif->getLink( iMesh, "Data" ) ), "Vertices" );
						QVector<Vector3> vv = nif->getArray<Vector3>( iVerts );
						for ( const Vector3 & v : vv ) {
							if ( e.points.size() >= 4096 )
								break;
							e.points.append( wt * v );
						}
					}
				}
			}

			emitters.append( e );
		} else if ( mtype == QLatin1String( "NiPSysGravityModifier" ) ) {
			hasGravity = true;
			gravityDir = nif->get<Vector3>( iMod, "Gravity Axis" );
			gravityDir.normalize();
			gravityStrength = nif->get<float>( iMod, "Strength" );
			QModelIndex iGObj = nif->getBlockIndex( nif->getLink( iMod, "Gravity Object" ) );
			if ( iGObj.isValid() ) {
				Node * n = target->scene->getNode( nif, iGObj );
				if ( n )
					gravityDir = n->worldTrans().rotation * gravityDir;
			}
		} else if ( mtype == QLatin1String( "NiPSysDragModifier" ) ) {
			dragPct = nif->get<float>( iMod, "Percentage" );
		} else if ( mtype == QLatin1String( "BSPSysSimpleColorModifier" ) ) {
			hasColorMod = true;
			fadeIn = nif->get<float>( iMod, "Fade In Percent" );
			fadeOut = nif->get<float>( iMod, "Fade Out Percent" );
			c1End = nif->get<float>( iMod, "Color 1 End Percent" );
			c2Start = nif->get<float>( iMod, "Color 1 Start Percent" );
			c2End = nif->get<float>( iMod, "Color 2 End Percent" );
			c3Start = nif->get<float>( iMod, "Color 2 Start Percent" );
			QModelIndex iCols = nif->getIndex( iMod, "Colors" );
			for ( int c = 0; c < 3 && c < nif->rowCount( iCols ); c++ )
				modColors[c] = nif->get<Color4>( nif->getIndex( iCols, c ) );
		} else if ( mtype == QLatin1String( "BSPSysScaleModifier" ) ) {
			scaleKeys = nif->getArray<float>( nif->getIndex( iMod, "Scales" ) );
		}
	}

	// emitter controllers on the particle system's controller chain
	QModelIndex iCtlr = nif->getBlockIndex( nif->getLink( iPSys, "Controller" ) );
	int guard = 0;
	while ( iCtlr.isValid() && guard++ < 64 ) {
		iExtras.append( iCtlr );
		if ( nif->itemName( iCtlr ) == QLatin1String( "NiPSysEmitterCtlr" ) ) {
			QString modName = nif->resolveString( iCtlr, "Modifier Name" );
			for ( Emitter & e : emitters ) {
				if ( e.name != modName )
					continue;
				e.ctlrBlock = nif->getBlockNumber( iCtlr );
				QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iCtlr, "Interpolator" ) );
				if ( iInterp.isValid() ) {
					iExtras.append( iInterp );
					float v = nif->get<float>( iInterp, "Value" );
					// blend interpolators carry a -FLT_MAX pose sentinel
					if ( std::isfinite( v ) && std::fabs( v ) < 1.0e8f )
						e.birthRate = v;
					QModelIndex iFD = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
					if ( iFD.isValid() ) {
						iExtras.append( iFD );
						e.iBirthKeys = nif->getIndex( iFD, "Data" );
					}
				}
				QModelIndex iVis = nif->getBlockIndex( nif->getLink( iCtlr, "Visibility Interpolator" ) );
				if ( iVis.isValid() ) {
					iExtras.append( iVis );
					QModelIndex iBD = nif->getBlockIndex( nif->getLink( iVis, "Data" ) );
					if ( iBD.isValid() ) {
						iExtras.append( iBD );
						e.iVisKeys = nif->getIndex( iBD, "Data" );
					}
				}
			}
		}
		iCtlr = nif->getBlockIndex( nif->getLink( iCtlr, "Next Controller" ) );
	}

	// manager rigs: the sequences own the real BirthRate / EmitterActive keys
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iSeq = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
			continue;
		QString seqName = nif->resolveString( iSeq, "Name" );
		QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
			QModelIndex iRow = nif->getIndex( iCB, r );
			qint32 rowCtlr = nif->getLink( iRow, "Controller" );
			for ( Emitter & e : emitters ) {
				if ( e.ctlrBlock < 0 || rowCtlr != e.ctlrBlock )
					continue;
				QString interpId = nif->resolveString( iRow, "Interpolator ID" );
				QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ) );
				if ( !iInterp.isValid() )
					continue;
				iExtras.append( iSeq );
				iExtras.append( iInterp );
				QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
				if ( iData.isValid() )
					iExtras.append( iData );
				Emitter::SeqKeys sk;
				sk.seq = seqName;
				if ( iData.isValid() )
					sk.keys = nif->getIndex( iData, "Data" );
				float v = nif->get<float>( iInterp, "Value" );
				if ( std::isfinite( v ) && std::fabs( v ) < 1.0e8f )
					sk.constVal = v;
				if ( interpId == QLatin1String( "EmitterActive" ) )
					e.seqVis.append( sk );
				else
					e.seqBirth.append( sk );
			}
		}
	}

	parts.clear();
	lastTime = -1.0e30f;

	return true;
}

Color4 PSysSimController::particleColor( const Emitter & e, float u ) const
{
	Color4 c = e.color;
	if ( hasColorMod ) {
		if ( u < c1End || c2Start <= c1End )
			c = modColors[0];
		else if ( u < c2Start )
			c = tlLerpColor( modColors[0], modColors[1], ( u - c1End ) / ( c2Start - c1End ) );
		else if ( u < c2End || c3Start <= c2End )
			c = modColors[1];
		else if ( u < c3Start )
			c = tlLerpColor( modColors[1], modColors[2], ( u - c2End ) / ( c3Start - c2End ) );
		else
			c = modColors[2];
	}
	float a = c[3];
	if ( fadeIn > 0.0f && u < fadeIn )
		a *= u / fadeIn;
	if ( fadeOut < 1.0f && u > fadeOut )
		a *= ( 1.0f - u ) / ( 1.0f - fadeOut );
	return Color4( c[0], c[1], c[2], std::max( a, 0.0f ) );
}

float PSysSimController::particleScale( float u ) const
{
	if ( scaleKeys.size() < 2 )
		return scaleKeys.isEmpty() ? 1.0f : scaleKeys.first();
	float f = u * float( scaleKeys.size() - 1 );
	int i = std::min( int( f ), int( scaleKeys.size() ) - 2 );
	float t = f - float( i );
	return scaleKeys[i] + ( scaleKeys[i + 1] - scaleKeys[i] ) * t;
}

void PSysSimController::emitParticle( Emitter & e )
{
	SimParticle p;

	Vector3 local;
	bool worldPos = false;
	switch ( e.shape ) {
	case 1:
		local = Vector3( random( e.dims[0] ) - e.dims[0] * 0.5f,
		                 random( e.dims[1] ) - e.dims[1] * 0.5f,
		                 random( e.dims[2] ) - e.dims[2] * 0.5f );
		break;
	case 2:
		{
			float ang = random( 2.0f * float( M_PI ) );
			float rr = e.dims[0] * std::sqrt( random( 1.0f ) );
			local = Vector3( rr * std::cos( ang ), rr * std::sin( ang ), random( e.dims[1] ) - e.dims[1] * 0.5f );
		}
		break;
	case 3:
		{
			float ang = random( 2.0f * float( M_PI ) );
			float z = random( 2.0f ) - 1.0f;
			float rxy = std::sqrt( std::max( 1.0f - z * z, 0.0f ) );
			float rr = e.dims[0] * std::cbrt( std::max( random( 1.0f ), 1.0e-6f ) );
			local = Vector3( rxy * std::cos( ang ), rxy * std::sin( ang ), z ) * rr;
		}
		break;
	case 4:
		if ( !e.points.isEmpty() ) {
			local = e.points.at( std::rand() % e.points.size() );
			worldPos = true;	// stored pre-transformed to world space
		}
		break;
	default:
		break;
	}

	Transform pw = target->worldTrans();
	float psc = ( pw.scale != 0.0f ) ? pw.scale : 1.0f;
	auto worldToPSys = [&pw, psc]( const Vector3 & w ) {
		return pw.rotation.inverted() * ( ( w - pw.translation ) * ( 1.0f / psc ) );
	};

	if ( worldPos ) {
		p.pos = worldToPSys( local );
	} else if ( e.emitNode ) {
		p.pos = worldToPSys( e.emitNode->worldTrans() * local );
	} else {
		p.pos = local;
	}

	// direction from declination (from +Z) and planar angle
	float di = e.declination + random( e.declinationVar ) - e.declinationVar * 0.5f;
	float pa = e.planar + random( e.planarVar ) - e.planarVar * 0.5f;
	Vector3 dir( std::sin( di ) * std::cos( pa ), std::sin( di ) * std::sin( pa ), std::cos( di ) );
	if ( e.emitNode )
		dir = pw.rotation.inverted() * ( e.emitNode->worldTrans().rotation * dir );
	p.vel = dir * ( e.speed + random( e.speedVar ) );

	p.age = 0;
	p.lifespan = std::max( e.lifeSpan + random( e.lifeSpanVar ), 0.05f );
	p.radius = std::max( e.radius + random( e.radiusVar ), 0.01f );
	p.color = particleColor( e, 0.0f );

	parts.append( p );
}

void PSysSimController::updateTime( float time )
{
	if ( !( target && active ) )
		return;

	float dt = time - lastTime;
	if ( dt < 0.0f ) {
		// scrubbed backwards or the sequence looped: restart the simulation
		parts.clear();
		for ( Emitter & e : emitters )
			e.accum = 0;
		lastTime = time;
		return;
	}
	lastTime = time;
	if ( dt <= 0.0f )
		return;
	dt = std::min( dt, 0.25f );

	// advance
	Transform pw = target->worldTrans();
	Vector3 gLocal = hasGravity ? pw.rotation.inverted() * gravityDir : Vector3();
	float dragMul = ( dragPct > 0.0f ) ? std::exp( -dragPct * 30.0f * dt ) : 1.0f;

	int n = 0;
	while ( n < parts.size() ) {
		SimParticle & p = parts[n];
		p.age += dt;
		if ( p.age >= p.lifespan ) {
			parts.remove( n );
			continue;
		}
		if ( hasGravity )
			p.vel += gLocal * ( gravityStrength * dt );
		if ( dragPct > 0.0f )
			p.vel *= dragMul;
		p.pos += p.vel * dt;
		n++;
	}

	// emit
	const QString & curSeq = target->scene->animGroup;
	for ( Emitter & e : emitters ) {
		float rate = e.birthRate;
		if ( e.iBirthKeys.isValid() ) {
			interpolate( rate, e.iBirthKeys, time, e.birthIdx );
		} else if ( !e.seqBirth.isEmpty() ) {
			// manager rig: use the active sequence's keys (else the first set)
			Emitter::SeqKeys * sk = &e.seqBirth[0];
			for ( auto & c : e.seqBirth ) {
				if ( c.seq == curSeq ) {
					sk = &c;
					break;
				}
			}
			rate = sk->constVal;
			if ( sk->keys.isValid() )
				interpolate( rate, sk->keys, time, sk->idx );
		}

		bool vis = true;
		if ( e.iVisKeys.isValid() ) {
			interpolate( vis, e.iVisKeys, time, e.visIdx );
		} else if ( !e.seqVis.isEmpty() ) {
			Emitter::SeqKeys * sk = &e.seqVis[0];
			for ( auto & c : e.seqVis ) {
				if ( c.seq == curSeq ) {
					sk = &c;
					break;
				}
			}
			if ( sk->keys.isValid() )
				interpolate( vis, sk->keys, time, sk->idx );
		}

		if ( !( std::isfinite( rate ) && std::fabs( rate ) < 1.0e8f ) )
			rate = 0.0f;
		if ( !vis || rate <= 0.0f )
			continue;

		e.accum += rate * dt;
		int num = int( e.accum );
		if ( num > 0 ) {
			e.accum -= float( num );
			while ( num-- > 0 && parts.size() < maxParticles )
				emitParticle( e );
		}
	}

	// hand the state to the renderer
	int count = parts.size();
	target->verts.resize( count );
	target->colors.resize( count );
	target->sizes.resize( count );
	for ( int i = 0; i < count; i++ ) {
		const SimParticle & p = parts.at( i );
		float u = p.age / p.lifespan;
		target->verts[i] = p.pos;
		Emitter dummy;
		target->colors[i] = hasColorMod ? particleColor( dummy, u ) : p.color;
		target->sizes[i] = p.radius * particleScale( u );
	}
	target->active = count;
	target->size = 1.0f;
}


// `BSNiAlphaPropertyTestRefController`

AlphaController::AlphaController( AlphaProperty * prop, const QModelIndex & index )
	: Controller( index ), alphaProp( prop )
{
}

// `NiAlphaController`

AlphaController::AlphaController( MaterialProperty * prop, const QModelIndex & index )
	: Controller( index ), materialProp( prop )
{
}

void AlphaController::updateTime( float time )
{
	if ( !(active) )
		return;

	if ( materialProp ) {
		interpolate( materialProp->alpha, iData, "Data", ctrlTime( time ), lAlpha );

		if ( materialProp->alpha < 0 )
			materialProp->alpha = 0;

		if ( materialProp->alpha > 1 )
			materialProp->alpha = 1;
	} else if ( alphaProp ) {
		float threshold;

		if ( interpolate( threshold, iData, "Data", ctrlTime( time ), lAlpha ) )
			alphaProp->alphaThreshold = threshold / 255.0f;
	}
}


// `NiMaterialColorController`

MaterialColorController::MaterialColorController( MaterialProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void MaterialColorController::updateTime( float time )
{
	if ( !(active && target) )
		return;

	Vector3 v3;
	interpolate( v3, iData, "Data", ctrlTime( time ), lColor );

	Color4 color( Color3( v3 ), 1.0 );

	switch ( tColor ) {
	case tAmbient:
		target->ambient = color;
		break;
	case tDiffuse:
		target->diffuse = color;
		break;
	case tSpecular:
		target->specular = color;
		break;
	case tSelfIllum:
		target->emissive = color;
		break;
	}
}

bool MaterialColorController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		if ( nif->checkVersion( 0x0A010000, 0 ) ) {
			tColor = nif->get<int>( iBlock, "Target Color" );
		} else {
			tColor = ((nif->get<int>( iBlock, "Flags" ) >> 4) & 7);
		}

		return true;
	}

	return false;
}


// `NiFlipController`

TexFlipController::TexFlipController( TexturingProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

TexFlipController::TexFlipController( TextureProperty * prop, const QModelIndex & index )
	: Controller( index ), oldTarget( prop )
{
}

void TexFlipController::updateTime( float time )
{
	auto nif = NifModel::fromValidIndex(iSources);
	if ( !((target || oldTarget) && active && nif) )
		return;

	float r = 0;

	if ( iData.isValid() )
		interpolate( r, iData, "Data", ctrlTime( time ), flipLast );
	else if ( flipDelta > 0 )
		r = ctrlTime( time ) / flipDelta;

	// TexturingProperty
	if ( target ) {
		target->textures[flipSlot & 7].iSource = nif->getBlockIndex( nif->getLink( nif->getIndex( iSources, int(r) ) ), "NiSourceTexture" );
	} else if ( oldTarget ) {
		oldTarget->iImage = nif->getBlockIndex( nif->getLink( nif->getIndex( iSources, int(r) ) ), "NiImage" );
	}
}

bool TexFlipController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		flipDelta = nif->get<float>( iBlock, "Delta" );
		flipSlot = nif->get<int>( iBlock, "Texture Slot" );

		if ( nif->checkVersion( 0x04000000, 0 ) ) {
			iSources = nif->getIndex( iBlock, "Sources" );
		} else {
			iSources = nif->getIndex( iBlock, "Images" );
		}

		return true;
	}

	return false;
}


// `NiTextureTransformController`

TexTransController::TexTransController( TexturingProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void TexTransController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	TexturingProperty::TexDesc * tex = &target->textures[texSlot & 7];

	float val;

	if ( interpolate( val, iData, "Data", ctrlTime( time ), lX ) ) {
		// If desired, we could force display even if texture transform was disabled:
		// tex->hasTransform = true;
		// however "Has Texture Transform" doesn't exist until 10.1.0.0, and neither does
		// NiTextureTransformController - so we won't bother
		switch ( texOP ) {
		case 0:
			tex->translation[0] = val;
			break;
		case 1:
			tex->translation[1] = val;
			break;
		case 2:
			tex->rotation = val;
			break;
		case 3:
			tex->tiling[0] = val;
			break;
		case 4:
			tex->tiling[1] = val;
			break;
		}
	}
}

bool TexTransController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		texSlot = nif->get<int>( iBlock, "Texture Slot" );
		texOP = nif->get<int>( iBlock, "Operation" );
		return true;
	}

	return false;
}



EffectFloatController::EffectFloatController( BSEffectShaderProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void EffectFloatController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	float val;

	int lIdx;

	if ( interpolate( val, iData, "Data", ctrlTime( time ), lIdx ) ) {
		switch ( variable ) {
		case EffectFloat::Emissive_Multiple:
			target->emissiveMult = val;
			break;
		case EffectFloat::Falloff_Start_Angle:
			target->falloff.startAngle = val;
			break;
		case EffectFloat::Falloff_Stop_Angle:
			target->falloff.stopAngle = val;
			break;
		case EffectFloat::Falloff_Start_Opacity:
			target->falloff.startOpacity = val;
			break;
		case EffectFloat::Falloff_Stop_Opacity:
			target->falloff.stopOpacity = val;
			break;
		case EffectFloat::Alpha:
			target->emissiveColor.setAlpha( val );
			break;
		case EffectFloat::U_Offset_F76:
			if ( target->bsVersion < 151 )
				break;
			[[fallthrough]];
		case EffectFloat::U_Offset:
			target->uvOffset.x = val;
			break;
		case EffectFloat::U_Scale_F76:
			if ( target->bsVersion < 151 )
				break;
			[[fallthrough]];
		case EffectFloat::U_Scale:
			target->uvScale.x = val;
			break;
		case EffectFloat::V_Offset_F76:
			if ( target->bsVersion < 151 )
				break;
			[[fallthrough]];
		case EffectFloat::V_Offset:
			target->uvOffset.y = val;
			break;
		case EffectFloat::V_Scale_F76:
			if ( target->bsVersion < 151 )
				break;
			[[fallthrough]];
		case EffectFloat::V_Scale:
			target->uvScale.y = val;
			break;
		default:
			break;
		}
	}
}

bool EffectFloatController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		variable = EffectFloat::Variable( nif->get<int>( iBlock, "Controlled Variable" ) );
		return true;
	}

	return false;
}


EffectColorController::EffectColorController( BSEffectShaderProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void EffectColorController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	Vector3 val;

	int lIdx;

	if ( interpolate( val, iData, "Data", ctrlTime( time ), lIdx ) ) {
		switch ( variable ) {
		case 0:
			target->emissiveColor = Color4( val[0], val[1], val[2], target->emissiveColor.alpha() );
			break;
		default:
			break;
		}
	}
}

bool EffectColorController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		variable = nif->get<int>( iBlock, "Controlled Color" );
		return true;
	}

	return false;
}


LightingFloatController::LightingFloatController( BSLightingShaderProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void LightingFloatController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	float val;

	int lIdx;

	if ( interpolate( val, iData, "Data", ctrlTime( time ), lIdx ) ) {
		switch ( variable ) {
		case LightingFloat::Refraction_Strength:
			break;
		case LightingFloat::Reflection_Strength:
			target->environmentReflection = val;
			break;
		case LightingFloat::Glossiness:
			target->specularGloss = val;
			break;
		case LightingFloat::Specular_Strength:
			target->specularStrength = val;
			break;
		case LightingFloat::Emissive_Multiple_F76:
			if ( target->bsVersion < 151 )
				break;
			[[fallthrough]];
		case LightingFloat::Emissive_Multiple:
			target->emissiveMult = val;
			break;
		case LightingFloat::Alpha:
			target->alpha = val;
			break;
		case LightingFloat::U_Offset:
			target->uvOffset.x = val;
			break;
		case LightingFloat::U_Scale:
			target->uvScale.x = val;
			break;
		case LightingFloat::V_Offset:
			target->uvOffset.y = val;
			break;
		case LightingFloat::V_Scale:
			target->uvScale.y = val;
			break;
		default:
			break;
		}
	}
}

bool LightingFloatController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		variable = LightingFloat::Variable(nif->get<int>( iBlock, "Controlled Variable" ));
		return true;
	}

	return false;
}


LightingColorController::LightingColorController( BSLightingShaderProperty * prop, const QModelIndex & index )
	: Controller( index ), target( prop )
{
}

void LightingColorController::updateTime( float time )
{
	if ( !(target && active) )
		return;

	Vector3 val;

	int lIdx;

	if ( interpolate( val, iData, "Data", ctrlTime( time ), lIdx ) ) {
		switch ( variable ) {
		case 0:
			target->specularColor = { val[0], val[1], val[2] };
			break;
		case 1:
			target->emissiveColor = { val[0], val[1], val[2] };
			break;
		default:
			break;
		}
	}
}

bool LightingColorController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( Controller::update( nif, index ) ) {
		variable = nif->get<int>( iBlock, "Controlled Color" );
		return true;
	}

	return false;
}
