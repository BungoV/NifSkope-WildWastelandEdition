#include "cellworldanim.h"

#include "harnesswindow.h"
#include "wwswitch.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/glnode.h"
#include "gl/glscene.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>

#include <algorithm>

namespace {

struct Doc
{
	QVector<WwAnimObjRoot> roots;
	QVector<QPointer<Node>> bound;	// per root; null = bind again (a scene rebuild made new nodes)
	QString boundGroup;				// the scene's sequence when bound: a transport change rebinds every node
};

QHash<const void *, Doc> & docs()
{
	static QHash<const void *, Doc> d;
	return d;
}

// the scene's animate flag before the clock took it, given back when the row goes off
QHash<Scene *, bool> & held()
{
	static QHash<Scene *, bool> h;
	return h;
}

bool isCell( Scene * scene )
{
	return scene && scene->nifModel && wwCellLightsFor( scene->nifModel );
}

double pinnedSeconds()
{
	WW_SWITCH_STATIC( double, pin, wwEnvIsSet( "WW_CELL_WORLDANIM_TIME" )
		? std::max( wwEnvStr( "WW_CELL_WORLDANIM_TIME" ).toDouble(), 0.0 ) : -1.0 );
	return pin;
}

bool redStill()
{
	WW_SWITCH_STATIC( bool, still, wwEnvStr( "WW_CELL_WORLDANIM_RED" ).trimmed() == QLatin1String( "still" ) );
	return still;
}

// each copy plays its own looping sequence (Scene::make bound the document's first group on every node)
void bindRoots( Scene * scene )
{
	auto it = docs().find( scene->nifModel );
	if ( it == docs().end() || it->roots.isEmpty() )
		return;
	Doc & d = it.value();
	if ( d.bound.size() != d.roots.size() || d.boundGroup != scene->animGroup ) {
		d.bound = QVector<QPointer<Node>>( d.roots.size() );
		d.boundGroup = scene->animGroup;
	}
	QHash<int, Node *> byId;
	for ( int i = 0; i < d.roots.size(); i++ ) {
		if ( d.bound[i] || d.roots[i].loopSeq.isEmpty() )
			continue;
		if ( byId.isEmpty() )
			for ( Node * n : scene->nodes.list() )
				if ( n )
					byId.insert( n->id(), n );
		Node * n = byId.value( d.roots[i].root );
		if ( !n )
			continue;
		n->setSequence( d.roots[i].loopSeq );
		d.bound[i] = n;
		scene->transformDirty = true;
	}
}

}	// namespace

bool wwCellWorldAnimOn()
{
	return wwLookOn( WwLook::WorldAnim );
}

double wwCellWorldSeconds()
{
	if ( redStill() )
		return 0.0;
	if ( pinnedSeconds() >= 0.0 )
		return pinnedSeconds();
	if ( !wwWorldClockLive() )
		return 0.0;
	static QElapsedTimer clock;
	if ( !clock.isValid() )
		clock.start();
	return double( clock.nsecsElapsed() ) * 1e-9;
}

bool wwCellWorldAnimFrame( Scene * scene, float & time )
{
	if ( !scene )
		return false;
	if ( !isCell( scene ) || !wwCellWorldAnimOn() ) {
		auto h = held().find( scene );
		if ( h != held().end() ) {
			scene->animate = h.value();
			scene->transformDirty = true;
			held().erase( h );
		}
		return false;
	}
	if ( !held().contains( scene ) )
		held().insert( scene, scene->animate );
	scene->animate = true;
	bindRoots( scene );
	time = float( wwCellWorldSeconds() );
	return true;
}

bool wwCellWorldAnimWantsFrames( Scene * scene )
{
	return isCell( scene ) && wwCellWorldAnimOn() && wwWorldClockLive() && pinnedSeconds() < 0.0 && !redStill();
}

void wwCellAnimObjSet( const NifModel * nif, const QVector<WwAnimObjRoot> & roots )
{
	Doc d;
	d.roots = roots;
	docs().insert( nif, d );
}
