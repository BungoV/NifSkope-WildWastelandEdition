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

static SeqBind::Stats seqBindStats;
static bool seqBindMeasuring = false;

SeqBind::Stats SeqBind::stats()
{
	return seqBindStats;
}

void SeqBind::reset()
{
	seqBindStats = Stats();
	// Measuring starts the first time anyone asks, and stays on: the cost is one
	// extra name search per Controlled Block, paid only in a harness run.
	seqBindMeasuring = true;
}

/*! Name -> block number, read out of the manager's `NiDefaultAVObjectPalette`.
 *
 *  This is the map the game uses. `NiControllerSequence::StoreTargets`
 *  (1.10.155 `0x1c14ff0`) resolves every Controlled Block's Node Name with
 *  `NiDefaultAVObjectPalette::GetAVObject` (`0x1bc1d20`) — a CRC32 hash of the
 *  name into a table — and never searches the scene graph. The palette it uses
 *  is the one **in the file**: `NiControllerManager::LoadBinary` (`0x1c0c260`)
 *  resolves the link straight into the manager and nothing rebuilds it at load.
 *
 *  So the file, not the tree, decides which node a name means. That matters
 *  whenever two nodes share a name — which merged files produce constantly.
 *  `Node::findChild` returns the FIRST match in pre-order; when the engine does
 *  rebuild a palette it overwrites (`SetAVObject`, `0x1bc25b0`, calls `SetAt`),
 *  so the LAST match in the same pre-order wins. Same traversal, opposite
 *  answer, and nothing in a name search can tell which one the author meant.
 *
 *  Empty for a file with no palette, which then falls back to the name search.
 */
static QHash<QString, qint32> objectPalette( const NifModel * nif, const QModelIndex & iManager )
{
	QHash<QString, qint32> map;

	QModelIndex iPalette = nif->getBlockIndex( nif->getLink( iManager, "Object Palette" ),
	                                           "NiDefaultAVObjectPalette" );
	if ( !iPalette.isValid() )
		return map;

	QModelIndex iObjs = nif->getIndex( iPalette, "Objs" );
	for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
		QModelIndex iObj = nif->getIndex( iObjs, r );
		const QString name = nif->get<QString>( iObj, "Name" );
		if ( name.isEmpty() )
			continue;

		// A palette row whose Ptr is dead resolves to nothing in the engine too,
		// so it is not an entry — recording it would only mask the name search.
		const qint32 link = nif->getLink( iObj, "AV Object" );
		if ( link >= 0 )
			map.insert( name, link );
	}

	return map;
}

void ControllerManager::setSequence( const QString & seqname )
{
	auto nif = NifModel::fromValidIndex(iBlock);
	if ( nif && target ) {
		const QHash<QString, qint32> palette = objectPalette( nif, iBlock );

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

					/* The palette first, exactly as the engine does. Falling back
					 * to the name search when a name is absent is a deliberate
					 * divergence: the engine drops the block silently, but a
					 * half-written palette is a thing mod files really have, and
					 * an editor that shows nothing there is less use than one
					 * that shows the file's intent. `Animation ▸ Fix AV Object
					 * Palette` is what turns the fallback into a real binding.
					 */
					Node * node = nullptr;
					const auto hit = palette.constFind( nodename );

					if ( hit != palette.constEnd() ) {
						node = ( target->id() == *hit ) ? target.data()
						                                : target->findChild( int(*hit) );
					} else {
						node = target->findChild( nodename );
					}

					/* What the two routes would each have picked. `bySearch` is only
					 * computed to be compared against, and only when someone is
					 * measuring — a name search per Controlled Block is not free.
					 */
					if ( seqBindMeasuring ) {
						Node * bySearch = ( hit != palette.constEnd() )
							? target->findChild( nodename ) : node;
						seqBindStats.rows++;
						if ( hit != palette.constEnd() )
							seqBindStats.viaPalette++;
						if ( node != bySearch )
							seqBindStats.differs++;
						if ( !node )
							seqBindStats.unresolved++;

						if ( qEnvironmentVariableIsSet( "WW_SEQBIND_DEBUG" ) ) {
							QFile f( QApplication::applicationDirPath() + "/ww_seqbind_debug.log" );
							if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
								QTextStream( &f )
									<< seqname << " row " << r << " \"" << nodename << "\""
									<< " via " << ( hit != palette.constEnd() ? "palette" : "search" )
									<< " -> block " << ( node ? node->id() : -1 )
									<< ", search -> block " << ( bySearch ? bySearch->id() : -1 )
									<< ( node != bySearch ? "   DIFFERS" : "" ) << "\n";
							}
						}
					}

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
	modActive.clear();
	hasAgeDeath = false;
	hasPosition = false;
	QModelIndex iMods = nif->getIndex( iPSys, "Modifiers" );
	for ( int r = 0; r < nif->rowCount( iMods ); r++ ) {
		QModelIndex iMod = nif->getBlockIndex( nif->getLink( nif->getIndex( iMods, r ) ) );
		if ( !iMod.isValid() )
			continue;
		iExtras.append( iMod );
		QString mtype = nif->itemName( iMod );

		/* Every modifier's own Active flag. It was never read, so a modifier the
		 * author had switched off still ran. Recorded by name because that is how
		 * NiPSysModifierActiveCtlr addresses it, below.
		 */
		{
			ModActive ma;
			ma.name = nif->resolveString( iMod, "Name" );
			ma.active = nif->get<bool>( iMod, "Active" );
			modActive.append( ma );
		}

		// The two the engine will not do without the modifier present.
		if ( mtype == QLatin1String( "NiPSysAgeDeathModifier" ) )
			hasAgeDeath = true;
		else if ( mtype == QLatin1String( "NiPSysPositionModifier" ) )
			hasPosition = true;

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
			gravityName = modActive.last().name;
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
			dragName = modActive.last().name;
		} else if ( mtype == QLatin1String( "BSPSysSimpleColorModifier" ) ) {
			hasColorMod = true;
			colorName = modActive.last().name;
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
			scaleName = modActive.last().name;
		} else if ( mtype == QLatin1String( "NiPSysRotationModifier" ) ) {
			hasRotation = true;
			rotationName = modActive.last().name;
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
		} else if ( nif->blockInherits( iCtlr, "NiPSysModifierCtlr" ) ) {
			/* The rest of the NiPSysModifierCtlr family, which was ignored
			 * wholesale. They all name their target modifier the same way, so the
			 * only difference between them is which field they land on.
			 *
			 * Measured across FO4's 692 effect meshes, which is why these and not
			 * the others: ModifierActive 288, EmitterSpeed 51, InitialRadius 21,
			 * LifeSpan 6, Declination 6, PlanarAngle 1. The six field modifiers,
			 * GrowFade and MeshUpdate appear in ZERO of them, so carrying them
			 * would be dead code; Spawn is in 347 but 66 of the 67 sampled have
			 * Num Spawn Generations 0, so it spawns nothing.
			 */
			const QString ctype = nif->itemName( iCtlr );
			const QString modName = nif->resolveString( iCtlr, "Modifier Name" );
			QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iCtlr, "Interpolator" ) );
			QModelIndex iData = iInterp.isValid()
				? nif->getBlockIndex( nif->getLink( iInterp, "Data" ) ) : QModelIndex();
			QPersistentModelIndex keys;
			if ( iData.isValid() ) {
				iExtras.append( iInterp );
				iExtras.append( iData );
				keys = nif->getIndex( iData, "Data" );
			}

			if ( ctype == QLatin1String( "NiPSysModifierActiveCtlr" ) ) {
				for ( ModActive & ma : modActive ) {
					if ( ma.name != modName )
						continue;
					ma.keys = keys;
					ma.ctlrBlock = nif->getBlockNumber( iCtlr );
				}
			} else {
				static const struct { const char * type; int slot; } emitCurves[] = {
					{ "NiPSysEmitterSpeedCtlr",          Emitter::CSpeed },
					{ "NiPSysEmitterDeclinationCtlr",    Emitter::CDeclination },
					{ "NiPSysEmitterDeclinationVarCtlr", Emitter::CDeclinationVar },
					{ "NiPSysEmitterPlanarAngleCtlr",    Emitter::CPlanar },
					{ "NiPSysEmitterPlanarAngleVarCtlr", Emitter::CPlanarVar },
					{ "NiPSysEmitterLifeSpanCtlr",       Emitter::CLifeSpan },
					{ "NiPSysEmitterInitialRadiusCtlr",  Emitter::CRadius },
				};
				for ( const auto & ec : emitCurves ) {
					if ( ctype != QLatin1String( ec.type ) )
						continue;
					for ( Emitter & e : emitters ) {
						if ( e.name != modName )
							continue;
						e.curveKeys[ec.slot] = keys;
						// Remembered even when `keys` is valid: a manager rig gives
						// these controllers a NiBlendFloatInterpolator stub with no
						// data of its own, and the sequence walk below needs the
						// block number to find the row that really drives it.
						e.curveCtlr[ec.slot] = nif->getBlockNumber( iCtlr );
					}
					break;
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

			/* The same treatment for the parameter controllers and for the
			 * Active controllers, whose interpolators are blend stubs on a
			 * manager rig too. Without this an animated Speed silently fell back
			 * to the authored field on exactly the files that animate it, since
			 * an effect with sequences is where the stubs come from.
			 */
			for ( Emitter & e : emitters ) {
				for ( int c = 0; c < Emitter::CCount; c++ ) {
					if ( e.curveCtlr[c] <= 0 || rowCtlr != e.curveCtlr[c] )
						continue;
					QModelIndex iI = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ) );
					QModelIndex iD = iI.isValid()
						? nif->getBlockIndex( nif->getLink( iI, "Data" ) ) : QModelIndex();
					if ( !iD.isValid() )
						continue;
					iExtras.append( iSeq );
					iExtras.append( iI );
					iExtras.append( iD );
					Emitter::SeqKeys sk;
					sk.seq = seqName;
					sk.keys = nif->getIndex( iD, "Data" );
					e.seqCurve[c].append( sk );
				}
			}
			for ( ModActive & ma : modActive ) {
				if ( ma.ctlrBlock <= 0 || rowCtlr != ma.ctlrBlock )
					continue;
				QModelIndex iI = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ) );
				QModelIndex iD = iI.isValid()
					? nif->getBlockIndex( nif->getLink( iI, "Data" ) ) : QModelIndex();
				if ( !iD.isValid() )
					continue;
				iExtras.append( iSeq );
				iExtras.append( iI );
				iExtras.append( iD );
				ma.seqKeys.append( { seqName, nif->getIndex( iD, "Data" ), 0 } );
			}

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

	/* Direction from declination (from +Z) and planar angle. Read from `live`,
	 * not from the authored fields: the NiPSysEmitterSpeedCtlr family animates
	 * exactly these, and 51 of FO4's effect meshes drive Speed that way.
	 */
	const float * lv = e.live;
	float di = lv[Emitter::CDeclination] + random( lv[Emitter::CDeclinationVar] )
		- lv[Emitter::CDeclinationVar] * 0.5f;
	float pa = lv[Emitter::CPlanar] + random( lv[Emitter::CPlanarVar] )
		- lv[Emitter::CPlanarVar] * 0.5f;
	Vector3 dir( std::sin( di ) * std::cos( pa ), std::sin( di ) * std::sin( pa ), std::cos( di ) );
	if ( e.emitNode )
		dir = pw.rotation.inverted() * ( e.emitNode->worldTrans().rotation * dir );
	p.vel = dir * ( lv[Emitter::CSpeed] + random( lv[Emitter::CSpeedVar] ) );

	p.age = 0;
	p.lifespan = std::max( lv[Emitter::CLifeSpan] + random( lv[Emitter::CLifeSpanVar] ), 0.05f );
	p.radius = std::max( lv[Emitter::CRadius] + random( lv[Emitter::CRadiusVar] ), 0.01f );
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

	/* Which modifiers are switched on this frame.
	 *
	 * A modifier's own Active field, overridden by a NiPSysModifierActiveCtlr
	 * when one names it. 288 of FO4's 692 effect meshes carry one of those, and
	 * every modifier used to run regardless — so a gravity or drag that the
	 * author switches on partway through an effect was on from the first frame.
	 */
	const QString & activeSeq = target->scene->animGroup;
	auto modIsActive = [this, time, &activeSeq]( const QString & name ) {
		if ( name.isEmpty() )
			return true;
		for ( ModActive & ma : modActive ) {
			if ( ma.name != name )
				continue;
			bool on = ma.active;
			if ( ma.keys.isValid() ) {
				interpolate( on, ma.keys, time, ma.idx );
			} else if ( !ma.seqKeys.isEmpty() ) {
				ModActive::SeqKeys * sk = &ma.seqKeys[0];
				for ( auto & c : ma.seqKeys ) {
					if ( c.seq == activeSeq ) {
						sk = &c;
						break;
					}
				}
				if ( sk->keys.isValid() )
					interpolate( on, sk->keys, time, sk->idx );
			}
			return on;
		}
		return true;
	};
	const bool gravityOn = hasGravity && modIsActive( gravityName );
	const bool dragOn = dragPct > 0.0f && modIsActive( dragName );
	const bool rotationOn = hasRotation && modIsActive( rotationName );

	// advance
	Transform pw = target->worldTrans();
	Vector3 gLocal = gravityOn ? pw.rotation.inverted() * gravityDir : Vector3();
	float dragMul = dragOn ? std::exp( -dragPct * 30.0f * dt ) : 1.0f;

	int n = 0;
	while ( n < parts.size() ) {
		SimParticle & p = parts[n];
		// Ageing and movement are MODIFIERS in the engine, not laws of the
		// simulation: without a NiPSysAgeDeathModifier nothing dies, and without
		// a NiPSysPositionModifier nothing moves. Both were unconditional here.
		if ( hasAgeDeath ) {
			p.age += dt;
			if ( p.age >= p.lifespan ) {
				parts.remove( n );
				continue;
			}
		}
		if ( gravityOn )
			p.vel += gLocal * ( gravityStrength * dt );
		if ( dragOn )
			p.vel *= dragMul;
		if ( hasPosition )
			p.pos += p.vel * dt;
		if ( rotationOn )
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

		/* This frame's emitter parameters: the authored field, replaced by the
		 * NiPSysModifierFloatCtlr curve wherever one names this emitter. Kept
		 * beside the authored values rather than overwriting them, because an
		 * emitter is re-read from the model only when the model changes and a
		 * curve is evaluated on every frame.
		 */
		const float authored[Emitter::CCount] = {
			e.speed, e.speedVar, e.declination, e.declinationVar,
			e.planar, e.planarVar, e.lifeSpan, e.lifeSpanVar, e.radius, e.radiusVar
		};
		for ( int c = 0; c < Emitter::CCount; c++ ) {
			e.live[c] = authored[c];
			if ( e.curveKeys[c].isValid() ) {
				interpolate( e.live[c], e.curveKeys[c], time, e.curveIdx[c] );
			} else if ( !e.seqCurve[c].isEmpty() ) {
				Emitter::SeqKeys * sk = &e.seqCurve[c][0];
				for ( auto & k : e.seqCurve[c] ) {
					if ( k.seq == curSeq ) {
						sk = &k;
						break;
					}
				}
				if ( sk->keys.isValid() )
					interpolate( e.live[c], sk->keys, time, sk->idx );
			}
		}

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

		// An emitter is a modifier too, so its own Active field — and any
		// NiPSysModifierActiveCtlr that names it — gate emission alongside
		// EmitterActive, which is a different flag on a different controller.
		if ( !modIsActive( e.name ) )
			vis = false;

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
	const bool colourOn = ( hasColorMod || hasColorGradient ) && modIsActive( colorName );
	const bool scaleOn = !scaleKeys.isEmpty() && modIsActive( scaleName );
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
		target->colors[i] = colourOn ? particleColor( dummy, u ) : p.color;
		target->sizes[i] = scaleOn ? ( p.radius * particleScale( u ) ) : p.radius;
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

//! Uniform integer in [lo, hi), the shape of the engine's BSRandom::UnsignedInt
static int tlBoltRandInt( quint32 & state, int lo, int hi )
{
	if ( hi <= lo )
		return lo;
	return lo + int( tlBoltRandom( state, float( hi - lo ) ) );
}

/*! The engine's displacement, `BSProceduralGeometry::Lightning::OffsetHelper`
 *  (1.10.155 `0x1cdc820`), on the v/w components of an (axial, v, w) polyline.
 *
 *  It is NOT midpoint displacement. One random direction is drawn for the whole
 *  span — `BSRandom::FloatTwoPi` then cos/sin times the amplitude — and applied
 *  across it under a tent weight `1 - |t - 0.5| * 2`, which is zero at both ends
 *  and one in the middle. The span is then halved and each half redrawn with a
 *  fresh direction at half the amplitude. The three constants are read out of the
 *  exe at `0x2c48d60`, `0x2c4b1a0` and `0x2c49180`: 1.0, 0.5, 2.0.
 *
 *  The difference from per-vertex jitter is what it looks like: a tent bends the
 *  whole span one way, so the bolt is a chain of smooth arcs, where independent
 *  midpoint noise reads as static.
 *
 *  Amplitudes are used raw. The engine does not normalise the series, so the
 *  excursion sums toward roughly 2x Arc Offset over the levels — which is the
 *  authored intent, not the overshoot the old normalisation was written to fix.
 */
static void tlOffsetHelper( QVector<Vector3> & pts, int lo, int hi, float amp, quint32 & rng )
{
	while ( true ) {
		const int n = hi - lo;
		if ( n <= 1 || lo < 0 || hi > pts.size() )
			return;

		const float theta = tlBoltRandom( rng, 2.0f * float( M_PI ) );
		const float dv = std::cos( theta ) * amp;
		const float dw = std::sin( theta ) * amp;
		const float step = 1.0f / float( n );

		for ( int i = 1; i < n; i++ ) {
			const float t = float( i ) * step;
			const float weight = 1.0f - std::fabs( t - 0.5f ) * 2.0f;
			pts[lo + i][1] += dv * weight;
			pts[lo + i][2] += dw * weight;
		}

		const int mid = lo + n / 2;
		if ( mid + 1 >= hi )
			return;

		// Halve, recurse on the left, then loop on the right with a fresh
		// direction — exactly the engine's own recurse-once-then-tail shape.
		amp *= 0.5f;
		tlOffsetHelper( pts, lo, mid, amp, rng );
		lo = mid;
	}
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

	bolts.clear();
	visible = false;

	return true;
}

quint32 ProcLightningController::keyOrdinal( const QVector<SeqKeys> & list, float time ) const
{
	if ( list.isEmpty() )
		return 0;

	const SeqKeys * sk = &list.at( 0 );
	if ( target && target->scene ) {
		const QString & curSeq = target->scene->animGroup;
		for ( const auto & c : list ) {
			if ( c.seq == curSeq ) {
				sk = &c;
				break;
			}
		}
	}
	if ( !sk->keys.isValid() )
		return 0;

	auto nif = NifModel::fromValidIndex( sk->keys );
	if ( !nif )
		return 0;

	quint32 n = 0;
	const int rows = nif->rowCount( sk->keys );
	for ( int r = 0; r < rows; r++ ) {
		if ( nif->get<float>( nif->getIndex( sk->keys, r ), "Time" ) > time )
			break;
		n++;
	}
	return n;
}

void ProcLightningController::regenerate( quint32 tick )
{
	bolts.clear();

	/* Subdivisions is a RECURSION DEPTH: a branch has 2^Subdivisions segments.
	 *
	 * `GetBranchVerts(s) = (1 << s) * 4 + 4` and `GetBranchTris(s) = (1 << s) * 4`
	 * (1.10.155 `0x1cdc670`, `0x1cdc690`) — four verts a ring, four triangles a
	 * segment, so 2^s segments and 2^s + 1 rings. This used to be read as a
	 * segment count rounded to a power of two, on the evidence that a depth
	 * reading looked wrong on screen; it did, but that test predates the span fix
	 * (07-31i), so it was judging a bolt drawn between the wrong two points.
	 *
	 * nif.xml allows 0..12, and 2^12 segments per branch times a branch tree is
	 * more preview than anyone needs, so the depth is capped. Shipped assets sit
	 * at 3..6, well under it.
	 */
	const int rootSubdiv = std::min( std::max( effSubdiv, 0 ), 10 );

	// Re-seeded from (controller, tick). The tick is an ORDINAL, not a time — see
	// updateTime — so scrubbing back to a moment redraws exactly what was there.
	rngState = ( rngSeed * 2654435761u ) ^ ( tick * 2246822519u );
	if ( !rngState )
		rngState = 1;	// xorshift cannot escape zero

	/* The branch tree first, geometry after — the engine's own split, and the
	 * reason the two halves are separable at all: `CreateBranches` (`0x1cdc6d0`)
	 * only decides how many branches exist and how each is shaped.
	 *
	 *     count = rand[ max(A - B, 0), A + B + 1 )        A = Num Branches
	 *     A >>= 1;  B >>= 1                               B = Num Branches Variation
	 *     child subdivisions = s <= 1 ? 0 : s - 1
	 *
	 * The recursion terminates on its own: halving drives A and B to zero, which
	 * makes the count zero. So depth is not a constant anywhere — it falls out of
	 * how many branches were asked for. The old two-level tree with "about half
	 * of them fork" was a guess standing in for this.
	 */
	struct Info { int parent; int subdiv; int gen; };
	QVector<Info> tree;
	QVector<QPair<int, int>> pending;	// (index into tree, A<<16 | B)

	auto spawn = [&]( int parent, int subdiv, int A, int B, int gen ) {
		const int me = tree.size();
		tree.append( Info{ parent, subdiv, gen } );
		const int count = tlBoltRandInt( rngState, std::max( A - B, 0 ), A + B + 1 );
		if ( count > 0 )
			pending.append( qMakePair( me, ( count << 20 ) | ( ( A >> 1 ) << 10 ) | ( B >> 1 ) ) );
	};

	spawn( -1, rootSubdiv, effBranches, effBranchVar, 0 );
	for ( int q = 0; q < pending.size() && tree.size() < 512; q++ ) {
		const int at = pending.at( q ).first;
		const int packed = pending.at( q ).second;
		const int count = packed >> 20;
		const int A = ( packed >> 10 ) & 0x3ff;
		const int B = packed & 0x3ff;
		const Info parent = tree.at( at );
		const int childSubdiv = ( parent.subdiv <= 1 ) ? 0 : parent.subdiv - 1;
		for ( int k = 0; k < count && tree.size() < 512; k++ )
			spawn( at, childSubdiv, A, B, parent.gen + 1 );
	}

	/* Geometry, per branch, from `Lightning::Process` (`0x1cdb2c0`):
	 *
	 *     len       = Length * 0.5^gen + rand(-1,1) * LengthVar * 0.25^gen
	 *     amplitude = Arc Offset * 0.5^gen
	 *
	 * Length is the authored Length for every branch, scaled by generation. That
	 * too was tried once and reverted for looking wrong, against the same bolt
	 * drawn between the wrong two points.
	 */
	bolts.reserve( tree.size() );
	for ( int i = 0; i < tree.size(); i++ ) {
		const Info & info = tree.at( i );
		const int segs = 1 << info.subdiv;
		const float genHalf = std::pow( 0.5f, float( info.gen ) );
		const float genQuarter = std::pow( 0.25f, float( info.gen ) );

		Bolt b;
		b.parent = info.parent;
		b.gen = info.gen;
		b.length = effLength * genHalf
			+ ( tlBoltRandom( rngState, 2.0f ) - 1.0f ) * effLengthVar * genQuarter;

		/* Where a child starts: `GetRandomBranchPos` (`0x1cdc9c0`) picks a ring
		 * uniformly in [0, 2^(subdiv-1)) — the FIRST HALF of the parent's rings —
		 * and takes the midpoint of two of that ring's four verts, which is the
		 * ring's centre on the axis. Branches therefore leave the parent low down
		 * its length, never near the tip.
		 */
		Vector3 origin;
		if ( info.parent >= 0 && info.parent < bolts.size() ) {
			const Bolt & p = bolts.at( info.parent );
			if ( p.pts.isEmpty() )
				continue;
			const int half = std::max( 1, 1 << std::max( tree.at( info.parent ).subdiv - 1, 0 ) );
			const int ring = std::min( tlBoltRandInt( rngState, 0, half ), int( p.pts.size() ) - 1 );
			origin = p.pts.at( ring );
		}

		b.pts.resize( segs + 1 );
		const float step = b.length / float( segs );
		for ( int k = 0; k <= segs; k++ )
			b.pts[k] = Vector3( origin[0] + float( k ) * step, origin[1], origin[2] );

		// hi = segs, so the last ring keeps the axis: the tent already carries the
		// path back to it, and this is the index range the engine displaces.
		tlOffsetHelper( b.pts, 0, segs, effArc * genHalf, rngState );
		bolts.append( b );
	}

	/* WW_BOLT_DEBUG=1 -> release/ww_bolt_debug.log
	 *
	 * One line per branch: generation, segment count, length. Those three ARE the
	 * engine's shape rule, and each reads differently under the old generator —
	 * 2^s segments where it capped at 32, Length*0.5^gen where it took a random
	 * fraction of the parent — so the log says which generator drew a bolt
	 * without anyone having to judge a picture.
	 * tests/anim/lightning_shape.sh asserts them.
	 */
	if ( qEnvironmentVariableIsSet( "WW_BOLT_DEBUG" ) ) {
		QFile f( QApplication::applicationDirPath() + "/ww_bolt_debug.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
			QTextStream ts( &f );
			ts << "  tree " << ( target ? target->name : QStringLiteral( "-" ) )
			   << " subdiv=" << rootSubdiv << " branches=" << effBranches
			   << " var=" << effBranchVar << " -> " << bolts.size() << " branch(es)\n";
			for ( int i = 0; i < bolts.size(); i++ ) {
				const Bolt & b = bolts.at( i );
				ts << "    branch " << i << " gen=" << b.gen
				   << " segs=" << ( b.pts.size() - 1 )
				   << " len=" << b.length << "\n";
			}
		}
	}
}

void ProcLightningController::updateTime( float time )
{
	visible = false;
	if ( !( active && target && target->scene ) )
		return;

	/* The bolt runs from the TARGET'S OWN ORIGIN along its local +Y, for Length.
	 *
	 * This used to hunt for a `<name>_Start` ancestor and a `<name>_End` node of
	 * the same stem, and stretch the bolt between them — a convention read off the
	 * shieldtesla and edison_pa rigs, i.e. a guess. The engine does no such thing.
	 * Walked in the 1.10.155 PDB: BSProceduralLightningController::Update ->
	 * UpdateGenerationParams / UpdateProcessParams / AddTasklet ->
	 * BSProceduralGeometry::Lightning::CreateInstance / Process. Not one of them
	 * touches a BSFixedString or searches the scene; the whole path takes the NIF's
	 * own fields and writes into the target's geometry. Process starts at
	 * NiPoint3::ZERO and lays each ring at Y = segment * (length / segments), with
	 * the four verts of the ring at +-width in X and +-width in Z — so the axis is
	 * local +Y, the origin is the target, and Length is what Length says.
	 *
	 * Checked against the assets before believing it: on both X01_Torso_Tesla_VFX
	 * bolts the target's local +Y points along the Start->End line to within a
	 * degree, and the only disagreement is that the engine runs the full 32 units
	 * where the node pair spans 25.3. That is why the node convention looked right
	 * on the torso — and why it put the leg bolts somewhere else entirely, since
	 * nothing ties those nodes to the axis the engine actually uses.
	 */
	if ( !( target && boltLength > 0.0f ) )
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

	/* When the bolt re-rolls.
	 *
	 * There is no cadence in the engine. `Lightning::Process` holds three float
	 * constants — 1.0, 0.5, 0.25 — and none of them is a rate; the 1/24 s that
	 * used to be here was invented and only looked right because the shieldtesla
	 * Mutation keys happen to be dense. What actually happens
	 * (`BSProceduralLightningController::Update`, 1.10.155 `0x1cf5bea`) is that
	 * Generation and Mutation are BOOL curves whose values are cached at `+0x1a0`
	 * and `+0x1a1`, and a full regenerate is forced on the frame either one
	 * CHANGES. Between changes the branch structure is kept. So the rate a bolt
	 * reshapes at is authored, per asset, in the Mutation curve.
	 *
	 * The seed is the ORDINAL of the key interval `time` falls in, not a count of
	 * flips seen so far. A running count would depend on which frames happened to
	 * be rendered, so scrubbing backwards would return a different bolt; an
	 * ordinal is a pure function of time, which the bake and the render baselines
	 * both rely on.
	 */
	const bool mutNow = evalKeys( mutKeys, true );
	const quint32 tick = keyOrdinal( mutKeys, time ) * 977u + keyOrdinal( genKeys, time );

	if ( bolts.isEmpty() || paramsChanged || tick != lastTick || mutNow != lastMutation ) {
		lastTick = tick;
		lastMutation = mutNow;
		regenerate( tick );
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

	// Target origin, along its local +Y, for Length — the engine's own rule; see
	// updateTime for how it was read out of the 1.10.155 PDB and checked.
	const Transform & tt = target->worldTrans();
	const Vector3 A = tt.translation;
	const Vector3 B = A + tt.rotation * Vector3( 0.0f, effLength, 0.0f );
	if ( !ribbonReported && qEnvironmentVariableIsSet( "WW_BOLT_DEBUG" ) ) {
		ribbonReported = true;
		QFile f( QApplication::applicationDirPath() + "/ww_bolt_debug.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
			QTextStream( &f ) << "  ribbon " << ( target ? target->name : QStringLiteral( "-" ) )
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

	/* Every branch shares ONE frame. The engine has no per-branch direction:
	 * `Lightning::Process` lays each branch's rings along +Y from its own origin
	 * and the forking look comes entirely from the displacement. So a point is
	 * just the axis frame applied to the (axial, v, w) the generator produced,
	 * and the per-bolt frames, parent tangents and root parameters this used to
	 * carry are all gone with the directions they served.
	 */
	auto boltPoint = [&]( const Bolt & b, int i ) {
		const Vector3 & p = b.pts.at( i );
		return A + axis * p[0] + v * p[1] + w * p[2];
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
	// No t parameter: V follows ARC LENGTH along the drawn ribbon, so how far
	// along its own axis a point sits stopped mattering when the jag got deep.
	auto addBolt = [&]( const QVector<Vector3> & wpts,
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

	// Every branch's world polyline. regenerate() appends parents before
	// children and already folded each child's origin into its own points, so
	// nothing here has to look at a parent.
	for ( int bi = 0; bi < bolts.size(); bi++ ) {
		const Bolt & b = bolts.at( bi );
		if ( b.pts.size() < 2 )
			continue;

		QVector<Vector3> wpts;
		wpts.reserve( b.pts.size() );
		for ( int i = 0; i < b.pts.size(); i++ )
			wpts.append( boltPoint( b, i ) );

		/* halfWidth = Width * ChildWidthMult^gen * 0.5, straight out of Process
		 * (`0x1cdb46b`-`0x1cdb4ad`: powf(ChildWidthMult, gen), times params+0x8,
		 * times 0.5). The old two-level tree open-coded the same rule as
		 * childWidthMult and childWidthMult squared; this is it for any depth.
		 */
		const float hw = std::max(
			effWidth * std::pow( childWidthMult, float( b.gen ) ) * 0.5f, 0.1f );
		addBolt( wpts, hw, b.length, tint, ( b.parent < 0 ) ? fadeMain : fadeChild );
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
