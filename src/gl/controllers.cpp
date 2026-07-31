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
#include "gl/renderer.h"
#include "glview.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
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
	hasColorGradient = false;
	iColorGradKeys = QModelIndex();
	scaleKeys.clear();
	hasRotation = false;
	rotSpeed = rotSpeedVar = rotAngle = rotAngleVar = 0;
	rotRandomSign = false;

	QModelIndex iPSys = nif->getBlockIndex( nif->getLink( iBlock, "Target" ) );
	if ( !iPSys.isValid() )
		return true;
	iExtras.append( iPSys );

	maxParticles = nif->get<int>( iPSys, "Num Vertices" );
	if ( maxParticles < 1 )
		maxParticles = 512;
	maxParticles = std::min( maxParticles, 4096 );

	// flipbook cells from the particle data block (each particle gets a random
	// cell of the sprite sheet, e.g. 16 for a 4x4 lightning atlas)
	subtexOffsets.clear();
	{
		QModelIndex iPD = nif->getBlockIndex( nif->getLink( iPSys, "Data" ) );
		if ( iPD.isValid() ) {
			iExtras.append( iPD );
			subtexOffsets = nif->getArray<Vector4>( nif->getIndex( iPD, "Subtexture Offsets" ) );
		}
	}

	// gather BSPositionData spawn points (object-local space) from a block's
	// extra data list. Layout (same as the Generate BSPositionData spell and
	// vanilla edison_pa_vfx.nif): numVerts*3 positions, numVerts*3 normals,
	// numTris*3 values, 2 zeros - ONLY the leading positions are spawn points;
	// reading the whole array as positions scatters particles around origin
	auto posDataPoints = [nif, this]( const QModelIndex & iObj ) {
		QVector<Vector3> pts;
		QModelIndex iExtraList = nif->getIndex( iObj, "Extra Data List" );
		for ( int r = 0; r < nif->rowCount( iExtraList ); r++ ) {
			QModelIndex iED = nif->getBlockIndex( nif->getLink( nif->getIndex( iExtraList, r ) ) );
			if ( !nif->blockInherits( iED, "BSPositionData" ) )
				continue;
			iExtras.append( iED );
			QVector<float> raw = nif->getArray<float>( nif->getIndex( iED, "Data" ) );

			// vertex count from the owning mesh bounds the position region
			int nv = nif->get<int>( iObj, "Num Vertices" );
			if ( nv <= 0 ) {
				QModelIndex iMeshData = nif->getBlockIndex( nif->getLink( iObj, "Data" ) );
				if ( iMeshData.isValid() )
					nv = nif->get<int>( iMeshData, "Num Vertices" );
			}
			int numPos;
			if ( nv > 0 && nv * 3 <= raw.size() )
				numPos = nv;
			else
				numPos = raw.size() / 6;	// unknown owner: positions + normals halves

			for ( int i = 0; i < numPos; i++ )
				pts.append( Vector3( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2] ) );
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

			// the emitter object's Node is resolved lazily in updateTime():
			// during update() the scene graph may not contain it yet, and its
			// world transform would come back as identity (spawning particles
			// at the wrong place)
			QModelIndex iObj = nif->getBlockIndex( nif->getLink( iMod, "Emitter Object" ) );
			if ( iObj.isValid() )
				e.iEmitObj = iObj;

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
				if ( iObj.isValid() )
					e.points = posDataPoints( iObj );
				if ( e.points.isEmpty() ) {
					e.points = posDataPoints( iPSys );
					e.iEmitObj = iPSys;
				}
			} else if ( mtype == QLatin1String( "NiPSysMeshEmitter" ) ) {
				e.shape = 4;
				e.emitFrom = nif->get<int>( iMod, "Emission Type" );
				QModelIndex iMeshes = nif->getIndex( iMod, "Emitter Meshes" );
				for ( int m = 0; m < nif->rowCount( iMeshes ); m++ ) {
					QModelIndex iMesh = nif->getBlockIndex( nif->getLink( nif->getIndex( iMeshes, m ) ) );
					if ( !iMesh.isValid() )
						continue;
					// points stay in the first mesh's local space; its world
					// transform (full parent chain, e.g. BSTriShape under the
					// pauldron NiNode) is applied fresh on each emission
					if ( !e.iEmitObj.isValid() )
						e.iEmitObj = iMesh;
					else if ( e.iEmitObj != iMesh )
						continue;	// preview: one reference frame per emitter

					// the game emits from the mesh's precomputed BSPositionData
					// when present (positions match the mesh vertex order, so
					// the triangles below stay valid for face sampling)
					e.points = posDataPoints( iMesh );

					QModelIndex iVD = nif->getIndex( iMesh, "Vertex Data" );
					if ( iVD.isValid() ) {
						if ( e.points.isEmpty() ) {
							for ( int v = 0; v < nif->rowCount( iVD ) && e.points.size() < 4096; v++ )
								e.points.append( nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) );
						}
						e.tris = nif->getArray<Triangle>( nif->getIndex( iMesh, "Triangles" ) );
					} else {
						QModelIndex iMeshData = nif->getBlockIndex( nif->getLink( iMesh, "Data" ) );
						if ( e.points.isEmpty() ) {
							QVector<Vector3> vv = nif->getArray<Vector3>( nif->getIndex( iMeshData, "Vertices" ) );
							for ( const Vector3 & v : vv ) {
								if ( e.points.size() >= 4096 )
									break;
								e.points.append( v );
							}
						}
						e.tris = nif->getArray<Triangle>( nif->getIndex( iMeshData, "Triangles" ) );
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
		} else if ( mtype == QLatin1String( "NiPSysRotationModifier" ) ) {
			hasRotation = true;
			rotSpeed = nif->get<float>( iMod, "Rotation Speed" );
			rotSpeedVar = nif->get<float>( iMod, "Rotation Speed Variation" );
			rotAngle = nif->get<float>( iMod, "Rotation Angle" );
			rotAngleVar = nif->get<float>( iMod, "Rotation Angle Variation" );
			rotRandomSign = nif->get<bool>( iMod, "Random Rot Speed Sign" );
		} else if ( mtype == QLatin1String( "NiPSysColorModifier" ) ) {
			// RGBA gradient over the particle's normalised age (age/lifespan);
			// the NiColorData "Data" key group holds Color4 keys with times
			QModelIndex iCData = nif->getBlockIndex( nif->getLink( iMod, "Data" ), "NiColorData" );
			if ( iCData.isValid() ) {
				iExtras.append( iCData );
				QModelIndex iKeys = nif->getIndex( iCData, "Data" );
				if ( iKeys.isValid() && nif->get<int>( iKeys, "Num Keys" ) > 0 ) {
					iColorGradKeys = iKeys;
					hasColorGradient = true;
				}
			}
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
					// only float interpolators carry a numeric birth-rate Value
					if ( nif->blockInherits( iInterp, "NiFloatInterpolator" )
						|| nif->blockInherits( iInterp, "NiBlendFloatInterpolator" ) ) {
						float v = nif->get<float>( iInterp, "Value" );
						// blend interpolators carry a -FLT_MAX pose sentinel
						if ( std::isfinite( v ) && std::fabs( v ) < 1.0e8f )
							e.birthRate = v;
					}
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
				if ( interpId == QLatin1String( "EmitterActive" ) ) {
					// bool interpolator: no float constant to read
					e.seqVis.append( sk );
				} else {
					// float interpolator: read the constant birth rate (guard sentinels)
					if ( nif->blockInherits( iInterp, "NiFloatInterpolator" )
						|| nif->blockInherits( iInterp, "NiBlendFloatInterpolator" ) ) {
						float v = nif->get<float>( iInterp, "Value" );
						if ( std::isfinite( v ) && std::fabs( v ) < 1.0e8f )
							sk.constVal = v;
					}
					e.seqBirth.append( sk );
				}
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
	if ( hasColorGradient && iColorGradKeys.isValid() ) {
		// NiPSysColorModifier: sample the RGBA gradient at the normalised age.
		// The gradient carries its own alpha keys, so the BSPSysSimpleColor
		// fadeIn/fadeOut below is skipped for it.
		int i = 0;
		interpolate( c, iColorGradKeys, std::min( std::max( u, 0.0f ), 1.0f ), i );
	} else if ( hasColorMod ) {
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
	if ( !hasColorGradient ) {
		if ( fadeIn > 0.0f && u < fadeIn )
			a *= u / fadeIn;
		if ( fadeOut < 1.0f && u > fadeOut )
			a *= ( 1.0f - u ) / ( 1.0f - fadeOut );
	}
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
		if ( !e.tris.isEmpty() && e.emitFrom >= 1 && e.emitFrom <= 4 ) {
			// face / edge emission: sample the triangle, not just its corners
			const Triangle & t = e.tris.at( std::rand() % e.tris.size() );
			int nv = e.points.size();
			if ( t[0] < nv && t[1] < nv && t[2] < nv ) {
				const Vector3 & a = e.points.at( t[0] );
				const Vector3 & b = e.points.at( t[1] );
				const Vector3 & c = e.points.at( t[2] );
				switch ( e.emitFrom ) {
				case 1:	// face center
					local = ( a + b + c ) / 3.0f;
					break;
				case 2:	// edge center
				case 4:	// edge surface
					{
						int k = std::rand() % 3;
						const Vector3 & ea = ( k == 0 ) ? a : ( k == 1 ? b : c );
						const Vector3 & eb = ( k == 0 ) ? b : ( k == 1 ? c : a );
						float u = ( e.emitFrom == 2 ) ? 0.5f : random( 1.0f );
						local = ea + ( eb - ea ) * u;
					}
					break;
				default:	// face surface: uniform barycentric sample
					{
						float u = random( 1.0f ), v = random( 1.0f );
						if ( u + v > 1.0f ) {
							u = 1.0f - u;
							v = 1.0f - v;
						}
						local = a + ( b - a ) * u + ( c - a ) * v;
					}
					break;
				}
			}
		} else if ( !e.points.isEmpty() ) {
			local = e.points.at( std::rand() % e.points.size() );
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

	if ( e.emitNode ) {
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

	if ( !subtexOffsets.isEmpty() ) {
		const Vector4 & s = subtexOffsets.at( std::rand() % subtexOffsets.size() );
		p.uvOff = Vector2( s[0], s[2] );
	}

	if ( hasRotation ) {
		p.angle = rotAngle + random( rotAngleVar ) - rotAngleVar * 0.5f;
		p.angVel = rotSpeed + random( rotSpeedVar ) - rotSpeedVar * 0.5f;
		if ( rotRandomSign && ( std::rand() & 1 ) )
			p.angVel = -p.angVel;
	}

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
		p.angle += p.angVel * dt;
		n++;
	}

	// emit
	const QString & curSeq = target->scene->animGroup;
	for ( Emitter & e : emitters ) {
		// resolve the emitter object's node now: by simulation time the scene
		// graph is complete, so the world transform chain is trustworthy
		if ( !e.emitNode && e.iEmitObj.isValid() )
			e.emitNode = target->scene->getNode( target->scene->nifModel, QModelIndex( e.iEmitObj ) );

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
	bool flipbook = !subtexOffsets.isEmpty();
	target->verts.resize( count );
	target->colors.resize( count );
	target->sizes.resize( count );
	target->uvOffsets.resize( flipbook ? count : 0 );
	target->angles.resize( hasRotation ? count : 0 );
	for ( int i = 0; i < count; i++ ) {
		const SimParticle & p = parts.at( i );
		float u = p.age / p.lifespan;
		target->verts[i] = p.pos;
		Emitter dummy;
		target->colors[i] = ( hasColorMod || hasColorGradient ) ? particleColor( dummy, u ) : p.color;
		target->sizes[i] = p.radius * particleScale( u );
		if ( flipbook )
			target->uvOffsets[i] = p.uvOff;
		if ( hasRotation )
			target->angles[i] = p.angle;
	}
	target->uvCell = flipbook
		? Vector2( subtexOffsets.first()[1], subtexOffsets.first()[3] )
		: Vector2( 1.0f, 1.0f );
	target->active = count;
	target->size = 1.0f;
}


/*
 *  ProcLightningController - preview for BSProceduralLightningController
 */

//! xorshift32 in [0, range). Deterministic given the state, unlike random().
static float tlBoltRandom( quint32 & state, float range )
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return float( double( state ) * ( 1.0 / 4294967296.0 ) ) * range;
}

//! Midpoint-displacement jitter on the v/w components of a (t,v,w) polyline
static void tlMidpointJag( QVector<Vector3> & pts, int a, int c, float amp, quint32 & rng )
{
	int m = ( a + c ) / 2;
	if ( m == a || m == c )
		return;
	pts[m][1] = ( pts[a][1] + pts[c][1] ) * 0.5f + ( tlBoltRandom( rng, 2.0f ) - 1.0f ) * amp;
	pts[m][2] = ( pts[a][2] + pts[c][2] ) * 0.5f + ( tlBoltRandom( rng, 2.0f ) - 1.0f ) * amp;
	// Classic midpoint displacement halves the amplitude per level. 0.55 was a
	// guess that barely showed at 3 levels; at the correct 7 it compounds into a
	// visibly too-noisy bolt.
	tlMidpointJag( pts, a, m, amp * 0.5f, rng );
	tlMidpointJag( pts, m, c, amp * 0.5f, rng );
}

ProcLightningController::ProcLightningController( Node * node, const QModelIndex & index )
	: Controller( index ), target( node )
{
}

bool ProcLightningController::update( const NifModel * nif, const QModelIndex & index )
{
	if ( !Controller::update( nif, index ) )
		return false;

	subdivisions = std::min( std::max( nif->get<int>( iBlock, "Subdivisions" ), 1 ), 12 );
	numBranches = std::min( std::max( nif->get<int>( iBlock, "Num Branches" ), 0 ), 10 );
	numBranchesVar = std::min( std::max( nif->get<int>( iBlock, "Num Branches Variation" ), 0 ), 10 );
	boltLength = nif->get<float>( iBlock, "Length" );
	boltLengthVar = nif->get<float>( iBlock, "Length Variation" );
	width = nif->get<float>( iBlock, "Width" );
	childWidthMult = nif->get<float>( iBlock, "Child Width Mult" );
	arcOffset = nif->get<float>( iBlock, "Arc Offset" );
	fadeMain = nif->get<bool>( iBlock, "Fade Main Bolt" );
	fadeChild = nif->get<bool>( iBlock, "Fade Child Bolts" );
	animateArc = nif->get<bool>( iBlock, "Animate Arc Offset" );
	iShaderProp = nif->getBlockIndex( nif->getLink( iBlock, "Shader Property" ) );

	// Interpolators 3-9 animate the generation parameters. They are -1 on the
	// shieldtesla bolts, but were previously not read at all, so any asset that
	// animates width/branching/arc offset rendered with static values.
	// NiBlendFloatInterpolator stubs are skipped deliberately: like
	// Generation/Mutation their real keys live in the sequences, and there is no
	// asset here to verify that path against — a wrong guess would animate the
	// bolt incorrectly rather than leave it static.
	auto paramCurve = [nif, this]( const char * field ) {
		ParamCurve pc;
		QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iBlock, field ), "NiFloatInterpolator" );
		if ( iInterp.isValid() ) {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, "Data" ), "NiFloatData" );
			if ( iData.isValid() ) {
				pc.keys = nif->getIndex( iData, "Data" );
				pc.valid = pc.keys.isValid();
			}
		}
		return pc;
	};
	cSubdiv    = paramCurve( "Interpolator 3: Subdivision" );
	cBranches  = paramCurve( "Interpolator 4: Num Branches" );
	cBranchVar = paramCurve( "Interpolator 5: Num Branches Var" );
	cLength    = paramCurve( "Interpolator 6: Length" );
	cLengthVar = paramCurve( "Interpolator 7: Length Var" );
	cWidth     = paramCurve( "Interpolator 8: Width" );
	cArc       = paramCurve( "Interpolator 9: Arc Offset" );

	// the Generation / Mutation bool keys live in the controller sequences
	// (the controller itself only holds blend interpolators)
	genKeys.clear();
	mutKeys.clear();
	for ( int k = 0; k < PCount; k++ )
		paramKeys[k].clear();
	int myBlock = nif->getBlockNumber( iBlock );
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iSeq = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
			continue;
		QString seqName = nif->resolveString( iSeq, "Name" );
		QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
			QModelIndex iRow = nif->getIndex( iCB, r );
			if ( nif->getLink( iRow, "Controller" ) != myBlock )
				continue;
			QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ) );
			QModelIndex iBD = nif->getBlockIndex( nif->getLink( iInterp, "Data" ) );
			if ( !iBD.isValid() )
				continue;
			SeqKeys sk;
			sk.seq = seqName;
			sk.keys = nif->getIndex( iBD, "Data" );
			// Partition by Interpolator ID. Anything that is neither Mutation nor
			// Generation is a sequence-driven PARAMETER curve (Width, Arc
			// Offset, ...); it used to fall into genKeys, where an unrelated
			// float curve would switch the whole bolt on and off as if it were
			// the Generation flag. An empty ID is Generation — that is how the
			// shieldtesla sequences author it.
			const QString interpId = nif->resolveString( iRow, "Interpolator ID" );
			if ( interpId == QLatin1String( "Mutation" ) ) {
				mutKeys.append( sk );
			} else if ( interpId.isEmpty() || interpId == QLatin1String( "Generation" ) ) {
				genKeys.append( sk );
			} else {
				// A named parameter curve. Match loosely on the controller's own
				// field names — authoring tools spell these with and without
				// spaces — and drop anything unrecognised rather than guessing.
				static const char * const paramIds[PCount] = {
					"subdivision", "numbranches", "numbranchesvar",
					"length", "lengthvar", "width", "arcoffset"
				};
				QString norm = interpId;
				norm.remove( QLatin1Char( ' ' ) ).remove( QLatin1Char( '_' ) );
				norm = norm.toLower();
				for ( int k = 0; k < PCount; k++ ) {
					if ( norm == QLatin1String( paramIds[k] ) ) {
						paramKeys[k].append( sk );
						break;
					}
				}
			}
		}
	}

	/* Seeded from the TARGET'S NAME, not from the block number.
	 *
	 * The block number is a fact about the file's layout, and every operation that
	 * rewrites layout — a merge, the loading-screen convert, deleting a block —
	 * silently reshapes every bolt in the file. Measured: the same rig converted to
	 * a loading screen produced bolts with 40, 48 and 56 triangles where the source
	 * had 48, 40 and 48, spanning the same nodes with a different jag. Nothing was
	 * wrong with them, and there was no way to tell that by looking.
	 *
	 * A name is what the file itself uses to identify the thing, it survives every
	 * renumbering, and it is already unique per limb — the merge qualifies effect
	 * names precisely so a sequence can address them. Falls back to the block
	 * number when there is no name to hash.
	 */
	const QString seedName = nif->get<QString>( nif->getBlockIndex( nif->getLink( iBlock, "Target" ) ), "Name" );
	// Seed 0 explicitly: qHash's default is deterministic, but the one-argument
	// form is the one a Qt release could decide to salt, and a bolt that changed
	// shape from run to run would take the bake's reproducibility with it.
	rngSeed = seedName.isEmpty() ? quint32( nif->getBlockNumber( iBlock ) + 1 )
	                             : quint32( qHash( seedName, 0 ) ) | 1u;

	// seed the per-frame values so frame 1 is right even without curves
	effSubdiv = subdivisions;
	effBranches = numBranches;
	effBranchVar = numBranchesVar;
	effLength = boltLength;
	effLengthVar = boltLengthVar;
	effArc = arcOffset;
	effWidth = width;

	nodesResolved = false;	// the scene graph may be mid-rebuild
	bolts.clear();
	visible = false;

	return true;
}

void ProcLightningController::regenerate( float time )
{
	bolts.clear();

	// Subdivisions is a SEGMENT COUNT, not a recursion depth. Reading it as a
	// depth (2^7 = 128 segments) was tested and is wrong: these bolts are ~25
	// units end to end and 4 units wide, so 128 segments makes each quad 0.2
	// units long by 4 wide — 20x wider than long. Consecutive quads then fan out
	// as separate blades instead of reading as a beam, which is exactly what
	// "the bolts do not connect to each other" looked like, and it got worse the
	// wider the bolt. Round up to a power of two for the midpoint subdivision.
	int nSeg = 4;
	while ( nSeg < effSubdiv + 1 && nSeg < 32 )
		nSeg *= 2;

	// Arc Offset is the bolt's maximum excursion from the straight line, not the
	// FIRST level's amplitude. Feeding it in raw compounded it down the
	// recursion (12.5 + 6.25 + 3.125 + ... ~= 2x Arc Offset), so on these bolts
	// -- 25.3 units end to end with Arc Offset 12.5 -- the lateral wander could
	// equal the whole bolt length. The path doubled back on itself constantly,
	// which both stops it reading as a bolt between two points and flips the
	// ribbon's perpendicular at every reversal. Normalise so the accumulated
	// displacement sums to Arc Offset instead.
	// Re-seed from (controller, mutation tick): the tick is the time quantised to
	// the mutation cadence, so scrubbing back to a time redraws exactly what was
	// there before and two runs of the same frame agree.
	const quint32 tick = quint32( std::max( time, 0.0f ) * 24.0f );
	rngState = ( rngSeed * 2654435761u ) ^ ( tick * 2246822519u );
	if ( !rngState )
		rngState = 1;	// xorshift cannot escape zero

	auto makeBolt = [this]( Bolt & b, int segs, float amp ) {
		b.pts.resize( segs + 1 );
		for ( int i = 0; i <= segs; i++ )
			b.pts[i] = Vector3( float( i ) / float( segs ), 0.0f, 0.0f );

		int levels = 0;
		for ( int s = segs; s > 1; s >>= 1 )
			levels++;
		// sum of amp * 0.5^k for k in [0, levels) -> normalise to reach `amp`
		float series = 2.0f * ( 1.0f - std::pow( 0.5f, float( std::max( levels, 1 ) ) ) );
		tlMidpointJag( b.pts, 0, segs, amp / std::max( series, 1.0e-3f ), rngState );
	};

	Bolt main;
	makeBolt( main, nSeg, effArc );
	bolts.append( main );

	// Num Branches Variation is a +/- spread around Num Branches, re-rolled on
	// every mutation (was ignored: the count was pinned to Num Branches).
	int nBranch = effBranches;
	if ( effBranchVar > 0 )
		nBranch += int( tlBoltRandom( rngState, float( effBranchVar * 2 + 1 ) ) ) - effBranchVar;
	nBranch = std::min( std::max( nBranch, 0 ), 20 );

	const int firstChild = bolts.size();
	for ( int k = 0; k < nBranch; k++ ) {
		Bolt br;
		br.parent = 0;
		br.rootT = 0.2f + tlBoltRandom( rngState, 0.6f );
		// branch direction in the (axis, v, w) frame: forward-biased
		br.dir = Vector3( 0.5f + tlBoltRandom( rngState, 0.5f ),
		                  tlBoltRandom( rngState, 2.0f ) - 1.0f,
		                  tlBoltRandom( rngState, 2.0f ) - 1.0f );
		br.dir.normalize();
		// Branch length stays a FRACTION of the main bolt. Using the authored
		// Length here was tried and is wrong: Length is 32 on bolts that span
		// 25.3 units, so branches came out longer than the bolt itself and shot
		// off in directions with nothing to do with the end node. Whatever
		// Length means (most likely the main bolt length for controllers with no
		// _Start/_End pair, where there is no node distance to span), it is not
		// this. Child Width Mult 0.5 agrees that children are subordinate.
		br.lenMul = 0.2f + tlBoltRandom( rngState, 0.25f );
		br.widthMul = childWidthMult;
		makeBolt( br, std::max( nSeg / 2, 4 ), effArc * 0.6f );
		bolts.append( br );
	}

	// Second level: forks off the branches, with Child Width Mult compounding
	// per level. That compounding is what makes it read as forking lightning
	// rather than a fan of equal-weight strands, and it is presumably why the
	// field is a MULTIPLIER rather than an absolute child width. Only about half
	// the branches fork, and depth stops at 2 — a third level costs segments for
	// detail invisible at bolt scale.
	const int lastChild = bolts.size();
	for ( int k = firstChild; k < lastChild; k++ ) {
		if ( tlBoltRandom( rngState, 1.0f ) < 0.5f )
			continue;
		Bolt gr;
		gr.parent = k;
		gr.rootT = 0.3f + tlBoltRandom( rngState, 0.5f );
		gr.dir = Vector3( 0.5f + tlBoltRandom( rngState, 0.5f ),
		                  tlBoltRandom( rngState, 2.0f ) - 1.0f,
		                  tlBoltRandom( rngState, 2.0f ) - 1.0f );
		gr.dir.normalize();
		gr.lenMul = 0.3f + tlBoltRandom( rngState, 0.3f );
		gr.widthMul = childWidthMult * childWidthMult;
		makeBolt( gr, std::max( nSeg / 4, 4 ), effArc * 0.35f );
		bolts.append( gr );
	}
}

void ProcLightningController::updateTime( float time )
{
	visible = false;
	if ( !( active && target && target->scene ) )
		return;

	if ( !nodesResolved ) {
		nodesResolved = true;
		startNode = nullptr;
		endNode = nullptr;
		// rig convention (edison_pa / shieldtesla): the controller target sits
		// under "<name>_Start", with a matching "<name>_End" node
		Node * n = target;
		while ( n && !n->name.endsWith( QLatin1String( "_Start" ) ) )
			n = n->parentNode();
		if ( n ) {
			startNode = n;
			QString endName = n->name;
			endName.chop( 5 );
			endName += QLatin1String( "End" );
			for ( Node * cand : target->scene->getNodes() ) {
				if ( cand && cand->name == endName ) {
					endNode = cand;
					break;
				}
			}
		}
	}
	// A rig that does not follow the *_Start / *_End convention used to render
	// NOTHING at all. Fall back to emitting along the target's own axis for
	// Length — which is very likely what Length is for, since it is meaningless
	// when two nodes already define the span.
	spanNodes = ( startNode && endNode );
	if ( !spanReported && qEnvironmentVariableIsSet( "WW_BOLT_DEBUG" ) ) {
		spanReported = true;
		QFile f( QApplication::applicationDirPath() + "/ww_bolt_debug.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
			QTextStream( &f ) << "target " << ( target ? target->name : QStringLiteral( "-" ) )
				<< "  start " << ( startNode ? startNode->name : QStringLiteral( "NOT FOUND" ) )
				<< "  end " << ( endNode ? endNode->name : QStringLiteral( "NOT FOUND" ) )
				<< "  scene nodes " << ( target && target->scene ? target->scene->getNodes().size() : -1 )
				<< "\n";
		}
	}
	if ( !spanNodes && !( target && boltLength > 0.0f ) )
		return;

	auto evalKeys = [this, time]( QVector<SeqKeys> & list, bool dflt ) {
		if ( list.isEmpty() )
			return dflt;
		SeqKeys * sk = &list[0];
		const QString & curSeq = target->scene->animGroup;
		for ( auto & c : list ) {
			if ( c.seq == curSeq ) {
				sk = &c;
				break;
			}
		}
		bool v = dflt;
		if ( sk->keys.isValid() )
			interpolate( v, sk->keys, time, sk->idx );
		return v;
	};

	if ( !evalKeys( genKeys, true ) ) {
		bolts.clear();
		return;
	}

	// Interpolators 3-9 for this frame; a change in a shape parameter forces a
	// rebuild even when the bolt is not mutating.
	// A direct NiFloatInterpolator wins; otherwise fall back to the sequence's
	// keys for that parameter (the NiBlendFloatInterpolator-stub case).
	auto evalCurve = [this, time]( ParamCurve & pc, ParamId pid, float dflt ) {
		if ( pc.valid ) {
			float v = dflt;
			interpolate( v, pc.keys, time, pc.idx );
			return v;
		}
		QVector<SeqKeys> & seq = paramKeys[pid];
		if ( !seq.isEmpty() ) {
			SeqKeys * sk = &seq[0];
			const QString & curSeq = target->scene->animGroup;
			for ( auto & c : seq ) {
				if ( c.seq == curSeq ) {
					sk = &c;
					break;
				}
			}
			if ( sk->keys.isValid() ) {
				float v = dflt;
				interpolate( v, sk->keys, time, sk->idx );
				return v;
			}
		}
		return dflt;
	};
	auto asInt = []( float f, int lo, int hi ) {
		return std::min( std::max( int( f + 0.5f ), lo ), hi );
	};
	int pSubdiv = asInt( evalCurve( cSubdiv, PSubdiv, float( subdivisions ) ), 1, 12 );
	int pBranches = asInt( evalCurve( cBranches, PBranches, float( numBranches ) ), 0, 10 );
	int pBranchVar = asInt( evalCurve( cBranchVar, PBranchVar, float( numBranchesVar ) ), 0, 10 );
	float pLength = evalCurve( cLength, PLength, boltLength );
	float pLengthVar = evalCurve( cLengthVar, PLengthVar, boltLengthVar );
	float pArc = evalCurve( cArc, PArc, arcOffset );
	effWidth = evalCurve( cWidth, PWidth, width );

	const bool paramsChanged = pSubdiv != effSubdiv || pBranches != effBranches
		|| pBranchVar != effBranchVar || pLength != effLength
		|| pLengthVar != effLengthVar || pArc != effArc;
	effSubdiv = pSubdiv; effBranches = pBranches; effBranchVar = pBranchVar;
	effLength = pLength; effLengthVar = pLengthVar; effArc = pArc;

	bool mut = evalKeys( mutKeys, true ) && animateArc;
	if ( bolts.isEmpty() || paramsChanged
		|| ( mut && std::fabs( time - lastMutation ) >= ( 1.0f / 24.0f ) ) ) {
		lastMutation = time;
		regenerate( time );
	}

	visible = true;
}

/* The ribbon geometry, with the billboard axis as a PARAMETER.
 *
 * Everything in here is camera-independent except that one axis: regenerate()
 * produces the bolt polylines in a normalised frame and boltPoint() puts them in
 * world space using the Start/End nodes. Only the width expansion needs to know
 * where the viewer is.
 *
 * That is what makes a bake possible at all. drawPreview() passes the scene
 * camera; a bake passes a fixed axis, because static geometry cannot turn to face
 * anyone.
 */
bool ProcLightningController::buildRibbon( const Vector3 & viewAxis, QVector<Vector3> & tris,
                                           QVector<FloatVector4> & cols, QVector<Vector2> & uvs,
                                           Color4 & tintOut )
{
	tris.clear();
	cols.clear();
	uvs.clear();

	if ( !( visible && target && target->scene ) )
		return false;
	Scene * sc = target->scene;
	if ( bolts.isEmpty() )
		return false;

	Vector3 A, B;
	if ( spanNodes ) {
		A = startNode->worldTrans().translation;
		B = endNode->worldTrans().translation;
	} else {
		// No end node to reach: emit along the target's local +Y for Length.
		// +Y is the authored bolt axis on the rigs that DO have a pair — the
		// shieldtesla End sits at local (0, +25, 0) from its Start — so the same
		// convention is the best available guess for rigs without one.
		const Transform & tt = target->worldTrans();
		A = tt.translation;
		B = A + tt.rotation * Vector3( 0.0f, effLength, 0.0f );
	}
	if ( !ribbonReported && qEnvironmentVariableIsSet( "WW_BOLT_DEBUG" ) ) {
		ribbonReported = true;
		QFile f( QApplication::applicationDirPath() + "/ww_bolt_debug.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
			QTextStream( &f ) << "  ribbon " << ( target ? target->name : QStringLiteral( "-" ) )
				<< ( spanNodes ? "  span" : "  FALLBACK" )
				<< "  A (" << A[0] << ", " << A[1] << ", " << A[2] << ")"
				<< "  B (" << B[0] << ", " << B[1] << ", " << B[2] << ")\n";
		}
	}

	Vector3 axis = B - A;
	float len = axis.length();
	if ( len < 1.0e-4f )
		return false;
	axis = axis * ( 1.0f / len );
	Vector3 up = ( std::fabs( axis[2] ) < 0.9f ) ? Vector3( 0.0f, 0.0f, 1.0f ) : Vector3( 1.0f, 0.0f, 0.0f );
	Vector3 v = Vector3::crossproduct( axis, up );
	v.normalize();
	Vector3 w = Vector3::crossproduct( axis, v );

	// billboard: expand the strip perpendicular to the viewer's forward axis —
	// the caller's, so a bake can pin it instead of following the camera
	const Vector3 & camZ = viewAxis;

	// texture + tint from the controller's effect shader; the BGEM material
	// (e.g. shieldtesla_lightning_beam_blue.bgem) carries both
	BSShaderLightingProperty * shaderProp = nullptr;
	if ( iShaderProp.isValid() )
		shaderProp = dynamic_cast<BSShaderLightingProperty *>(
			sc->getProperty( sc->nifModel, QModelIndex( iShaderProp ) ) );
	Color4 tint( 0.45f, 0.7f, 1.0f, 1.0f );
	if ( auto esp = dynamic_cast<BSEffectShaderProperty *>( shaderProp ) ) {
		Color4 ec = esp->emissiveColor;
		float m = std::min( std::max( esp->emissiveMult, 0.0f ), 2.0f );
		if ( ( ec[0] + ec[1] + ec[2] ) * m > 0.05f )
			tint = Color4( std::min( ec[0] * m, 1.0f ), std::min( ec[1] * m, 1.0f ),
			               std::min( ec[2] * m, 1.0f ), 1.0f );
	}

	// The jag lives in the plane PERPENDICULAR to the bolt's own direction, so
	// each bolt needs its own frame. Branches used to be displaced along the
	// main bolt's v/w: a branch heading along v had its "lateral" offset pushed
	// down its own axis, so it folded back on itself and read as disconnected
	// spikes instead of a continuous bolt.
	auto boltPoint = [&]( const Bolt & b, int i, const Vector3 & root, const Vector3 & bDir,
	                      const Vector3 & bv, const Vector3 & bw, float bLen ) {
		const Vector3 & p = b.pts.at( i );
		return root + bDir * ( p[0] * bLen ) + bv * p[1] + bw * p[2];
	};

	//! orthonormal (v, w) pair perpendicular to dir
	auto frameFor = []( const Vector3 & dir, Vector3 & fv, Vector3 & fw ) {
		Vector3 up = ( std::fabs( dir[2] ) < 0.9f ) ? Vector3( 0.0f, 0.0f, 1.0f ) : Vector3( 1.0f, 0.0f, 0.0f );
		fv = Vector3::crossproduct( dir, up );
		if ( fv.length() < 1.0e-6f )
			fv = Vector3( 1.0f, 0.0f, 0.0f );
		fv.normalize();
		fw = Vector3::crossproduct( dir, fv );
		fw.normalize();
	};

	tintOut = tint;

	// beam textures run ALONG V (e.g. shieldtesla_lightning_beam is 256x2048,
	// tile-V) with the sequences scrolling the shader's V offset; U spans the
	// width. The shader property carries the (BGEM-aware, animated) UV
	// offset/scale, so the scrolling shows in the preview too.
	float uvOffU = 0.0f, uvOffV = 0.0f, uvSclU = 1.0f, uvSclV = 1.0f;
	if ( shaderProp ) {
		uvOffU = shaderProp->uvOffset.x;
		uvOffV = shaderProp->uvOffset.y;
		uvSclU = shaderProp->uvScale.x;
		uvSclV = shaderProp->uvScale.y;
	}
	// Beam sheets are tall and narrow, and the ratio sets how often the texture
	// repeats along the bolt. Take it from the ACTUAL texture rather than
	// assuming 8:1 — a wrong aspect is invisible as "wrong tiling density",
	// which is impossible to guess at from a screenshot.
	float texAspect = 8.0f;
	if ( shaderProp && sc->textures ) {
		QString texName = shaderProp->fileName( 0 );
		if ( !texName.isEmpty() ) {
			if ( const TexCache::Tex::ImageInfo * ti = sc->textures->getTextureInfo( QStringView( texName ) ) ) {
				if ( ti->width > 0 && ti->height > 0 )
					texAspect = float( ti->height ) / float( ti->width );
			}
		}
	}

	// a connected ribbon per bolt: joints share mitered vertices so the
	// segments connect geometrically instead of leaving notches at bends
	auto addBolt = [&]( const QVector<Vector3> & wpts, const QVector<float> & tvals,
	                    float halfWidth, float bLen, const Color4 & col, bool fade ) {
		int n = wpts.size();
		if ( n < 2 )
			return;
		float u0 = uvOffU, u1 = uvOffU + uvSclU;

		// V follows ARC LENGTH along the ribbon, not the straight-axis parameter
		// t. Driving V from t only looked right while the bolt was nearly
		// straight (the old 8-segment version): at the correct subdivision the
		// jagged path is much longer than the axis, so a t-based V smeared one
		// span of texture over the whole zigzag — and because each segment took
		// its V straight from t, lateral jumps stretched V unevenly and the
		// tiles never lined up.
		QVector<float> arc( n );
		arc[0] = 0.0f;
		for ( int i = 1; i < n; i++ )
			arc[i] = arc[i - 1] + ( wpts.at( i ) - wpts.at( i - 1 ) ).length();
		float totalLen = arc.at( n - 1 );
		if ( totalLen < 1.0e-4f )
			totalLen = std::max( bLen, 1.0e-4f );	// degenerate: fall back to the axis

		// One tile every (width * aspect) world units, rounded to a WHOLE number
		// of tiles so the strip wraps seamlessly instead of ending mid-texture.
		float period = std::max( halfWidth * 2.0f * texAspect, 0.1f );
		float vTiles = std::max( std::round( totalLen / period ), 1.0f );

		// averaged (mitered) billboard perpendicular per point
		QVector<Vector3> perps( n );
		for ( int i = 0; i < n; i++ ) {
			Vector3 d = wpts.at( std::min( i + 1, n - 1 ) ) - wpts.at( std::max( i - 1, 0 ) );
			// |cross(camZ, d)| is sin(angle between the segment and the view
			// axis). Near zero the segment points AT the camera and its width
			// direction is undefined — normalising it there amplifies noise into
			// a random direction, so the strip spins and tears at exactly those
			// segments. That is the "bolts do not connect on some angles" break:
			// it depends on the view, not on the bolt. Below ~15 degrees, hold
			// the previous segment's side instead of recomputing.
			Vector3 dn = d;
			dn.normalize();
			Vector3 perp = Vector3::crossproduct( camZ, dn );
			float sinA = perp.length();
			if ( sinA < 0.25f )
				perp = ( i > 0 ) ? perps.at( i - 1 ) : Vector3( 0, 0, 1 ) * halfWidth;
			else
				perp = perp * ( halfWidth / sinA );
			// Keep the ribbon's winding consistent. cross(camZ, d) flips sign
			// wherever the jagged path doubles back, which twists the strip into
			// a bowtie and pinches it to nothing at the crossing — reads as the
			// geometry coming apart. Carry the previous side forward instead.
			if ( i > 0 && Vector3::dotproduct( perp, perps.at( i - 1 ) ) < 0.0f )
				perp = -perp;
			perps[i] = perp;
		}

		for ( int i = 0; i + 1 < n; i++ ) {
			const Vector3 & p0 = wpts.at( i );
			const Vector3 & p1 = wpts.at( i + 1 );
			const Vector3 & q0 = perps.at( i );
			const Vector3 & q1 = perps.at( i + 1 );
			// normalized position along the DRAWN ribbon: drives both the tiling
			// and the tip fade, so neither depends on how jagged the bolt is
			float s0 = arc.at( i ) / totalLen, s1 = arc.at( i + 1 ) / totalLen;
			float a0 = fade ? ( 1.0f - s0 ) : 1.0f;
			float a1 = fade ? ( 1.0f - s1 ) : 1.0f;
			float v0 = uvOffV + uvSclV * ( s0 * vTiles );
			float v1 = uvOffV + uvSclV * ( s1 * vTiles );
			FloatVector4 c0( col[0], col[1], col[2], col[3] * a0 );
			FloatVector4 c1( col[0], col[1], col[2], col[3] * a1 );
			tris << ( p0 + q0 ) << ( p0 - q0 ) << ( p1 + q1 );
			cols << c0 << c0 << c1;
			uvs << Vector2( u0, v0 ) << Vector2( u1, v0 ) << Vector2( u0, v1 );
			tris << ( p1 + q1 ) << ( p0 - q0 ) << ( p1 - q1 );
			cols << c1 << c0 << c1;
			uvs << Vector2( u0, v1 ) << Vector2( u1, v0 ) << Vector2( u1, v1 );
		}
	};

	// Every bolt's world polyline, parents before children (regenerate() appends
	// in that order). A child roots on its parent's JITTERED path and takes its
	// frame from the parent's TANGENT there, so a branch-of-a-branch nests off
	// the strand it grew from instead of being laid out in the main bolt's frame.
	QVector<QVector<Vector3>> polys( bolts.size() );
	QVector<float> boltLens( bolts.size(), 0.0f );

	auto sampleAt = []( const QVector<Vector3> & pts, float t, Vector3 & pos, Vector3 & tangent ) {
		int nSeg = pts.size() - 1;
		float f = std::min( std::max( t, 0.0f ), 1.0f ) * float( nSeg );
		int i = std::min( int( f ), nSeg - 1 );
		float u = f - float( i );
		pos = pts.at( i ) + ( pts.at( i + 1 ) - pts.at( i ) ) * u;
		tangent = pts.at( i + 1 ) - pts.at( i );
		if ( tangent.length() < 1.0e-6f )
			tangent = Vector3( 0.0f, 0.0f, 1.0f );
		tangent.normalize();
	};

	for ( int bi = 0; bi < bolts.size(); bi++ ) {
		const Bolt & b = bolts.at( bi );
		Vector3 root, bDir, bv, bw;
		float bLen;

		if ( b.parent < 0 ) {
			root = A;
			bDir = axis;
			bv = v;
			bw = w;
			bLen = len;
		} else {
			const QVector<Vector3> & pp = polys.at( b.parent );
			if ( pp.size() < 2 )
				continue;	// parent was skipped; orphan a child rather than crash
			Vector3 ptan;
			sampleAt( pp, b.rootT, root, ptan );
			Vector3 pv, pw;
			frameFor( ptan, pv, pw );
			bDir = ptan * b.dir[0] + pv * b.dir[1] + pw * b.dir[2];
			bDir.normalize();
			frameFor( bDir, bv, bw );
			bLen = boltLens.at( b.parent ) * b.lenMul;
		}
		boltLens[bi] = bLen;

		QVector<Vector3> & wpts = polys[bi];
		QVector<float> tvals;
		wpts.reserve( b.pts.size() );
		tvals.reserve( b.pts.size() );
		for ( int i = 0; i < b.pts.size(); i++ ) {
			wpts.append( boltPoint( b, i, root, bDir, bv, bw, bLen ) );
			tvals.append( b.pts.at( i )[0] );
		}

		float hw = std::max( effWidth * b.widthMul * 0.5f, 0.1f );
		addBolt( wpts, tvals, hw, bLen, tint, ( b.parent < 0 ) ? fadeMain : fadeChild );
	}

	return !tris.isEmpty();
}

void ProcLightningController::drawPreview()
{
	if ( !( visible && target && target->scene ) )
		return;
	Scene * sc = target->scene;
	if ( sc->selecting )
		return;

	QVector<Vector3> tris;
	QVector<FloatVector4> cols;
	QVector<Vector2> uvs;
	Color4 tint;
	// the scene camera's forward axis: the preview billboards, a bake does not
	if ( !buildRibbon( sc->view.rotation.inverted() * Vector3( 0.0f, 0.0f, 1.0f ),
	                   tris, cols, uvs, tint ) )
		return;

	BSShaderLightingProperty * shaderProp = nullptr;
	if ( iShaderProp.isValid() )
		shaderProp = dynamic_cast<BSShaderLightingProperty *>(
			sc->getProperty( sc->nifModel, QModelIndex( iShaderProp ) ) );

	if ( !sc->renderer )
		return;

	auto prog = sc->renderer->useProgram( "boltstrip.prog" );
	if ( !prog )
		return;
	prog->uni4m( "modelViewMatrix", sc->view.toMatrix4() );
	prog->uni1i( "boltTexture", 0 );

	// beam texture from the BGEM material, else the raw source texture path,
	// else flat white (strip shows the plain tint)
	sc->textures->activateTextureUnit( 0 );
	bool texOk = shaderProp && shaderProp->bind( 0 );
	if ( !texOk ) {
		static const QString defaultTexture = QStringLiteral( "#FFFFFFFF" );
		sc->textures->bind( defaultTexture, sc->nifModel );
	}

	// lightning beams are additive
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );

	const float * attrData[3] = { &( tris.constFirst()[0] ), &( cols.constFirst()[0] ), &( uvs.constFirst()[0] ) };
	sc->renderer->bindShape( (unsigned int) tris.size(), 0x0243ULL, 0, attrData, nullptr );
	sc->renderer->fn->glDrawArrays( GL_TRIANGLES, 0, GLsizei( tris.size() ) );

	glDepthMask( GL_TRUE );
	glDisable( GL_BLEND );
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
