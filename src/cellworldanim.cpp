#include "cellworldanim.h"

#include "harnesswindow.h"
#include "wwswitch.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/glnode.h"
#include "gl/glparticles.h"
#include "gl/glscene.h"
#include "gl/glshape.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct Doc
{
	QVector<WwAnimObjRoot> roots;
	QVector<QPointer<Node>> bound;	// per root; null = bind again (a scene rebuild made new nodes)
	QString boundGroup;				// the scene's sequence when bound: a transport change rebinds every node
	int paints = 0;					// lane FIREFX1: the census's paint count (it prints on the third)
};

// lane FIREFX1: the shapes that got their program while the census listened (the second paint)
bool censusListening = false;
QSet<const void *> & censusDrawn()
{
	static QSet<const void *> s;
	return s;
}

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
	wwCellAnimObjCensus( scene );	// lane FIREFX1: once per document, the row on or off
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

void wwCellAnimObjNoteDraw( const void * shape )
{
	if ( censusListening )
		censusDrawn().insert( shape );
}

void wwCellAnimObjCensus( Scene * scene )
{
	if ( !scene || !isCell( scene ) )
		return;
	auto it = docs().find( scene->nifModel );
	if ( it == docs().end() || it->paints > 2 )
		return;	// a document without copies still prints its totals and its particles' places
	Doc & d = it.value();
	d.paints++;
	if ( d.paints == 2 ) {	// listen through this paint's draws
		censusDrawn().clear();
		censusListening = true;
		return;
	}
	if ( d.paints != 3 )
		return;
	censusListening = false;
	QHash<int, int> copyOf;	// a copy's root block -> its index in d.roots
	for ( int i = 0; i < d.roots.size(); i++ )
		copyOf.insert( d.roots[i].root, i );
	struct Row
	{
		int copies = 0, nodes = 0, shapes = 0, tree = 0, hidden = 0, drawn = 0, frame = 0;
		Vector3 at, wrapAt;
		bool atSet = false;
		QString probe;	// the first shape met: its rows and where its first vertex lands
	};
	QMap<QString, Row> rows;
	for ( const WwAnimObjRoot & r : d.roots )
		rows[r.model].copies++;
	const QList<Node *> & top = scene->roots.list();
	// a view-space point inside the picture's cone (fov 55 vertical, 16:9) and nearer than 20 km
	auto inFrame = []( const Vector3 & e ) {
		return e[2] < 0.0f && e[2] > -20000.0f && std::fabs( e[0] ) < -e[2] * 1.0f && std::fabs( e[1] ) < -e[2] * 0.55f;
	};
	int pfxSystems = 0, pfxFrame = 0, pfxLive = 0;
	QString pfxProbe;
	for ( Node * n : scene->nodes.list() ) {
		if ( !n )
			continue;
		if ( const Particles * ps = dynamic_cast<const Particles *>( n ) ) {	// the Particles row's copies
			const Vector3 e = n->viewTrans().translation;
			pfxSystems++;
			pfxFrame += inFrame( e ) ? 1 : 0;
			pfxLive += ps->liveCount();
			if ( pfxProbe.isEmpty() )
				pfxProbe = QStringLiteral( "first %1 world %2 %3 %4 view %5 %6 %7" ).arg( n->getName() )
					.arg( n->worldTrans().translation[0], 0, 'f', 0 ).arg( n->worldTrans().translation[1], 0, 'f', 0 )
					.arg( n->worldTrans().translation[2], 0, 'f', 0 ).arg( e[0], 0, 'f', 0 ).arg( e[1], 0, 'f', 0 ).arg( e[2], 0, 'f', 0 );
			continue;
		}
		// the copy this node belongs to: the nearest ancestor (or itself) that is a copy's root
		const Node * up = n;
		int copy = -1;
		while ( up && copy < 0 ) {
			copy = copyOf.value( up->id(), -1 );
			up = up->parentNode();
		}
		if ( copy < 0 )
			continue;
		Row & row = rows[d.roots[copy].model];
		if ( n->id() == d.roots[copy].root ) {
			row.nodes++;
			if ( !row.atSet ) {
				row.at = n->worldTrans().translation;
				if ( n->parentNode() )
					row.wrapAt = n->parentNode()->worldTrans().translation;
				row.atSet = true;
			}
		}
		const Shape * sh = dynamic_cast<const Shape *>( n );
		if ( !sh )
			continue;
		row.shapes++;
		row.frame += inFrame( n->viewTrans() * ( sh->verts.isEmpty() ? Vector3() : sh->verts.first() ) ) ? 1 : 0;
		if ( row.probe.isEmpty() ) {
			const Vector3 v0 = sh->verts.isEmpty() ? Vector3() : sh->verts.first();
			const Vector3 w0 = n->worldTrans() * v0, e0 = n->viewTrans() * v0;
			row.probe = QStringLiteral( "probe %1: verts %2 tris %3 v0 %4 %5 %6 world %7 %8 %9 view %10 %11 %12 scale %13" )
				.arg( n->getName() ).arg( sh->verts.size() ).arg( sh->triangles.size() )
				.arg( v0[0], 0, 'f', 1 ).arg( v0[1], 0, 'f', 1 ).arg( v0[2], 0, 'f', 1 )
				.arg( w0[0], 0, 'f', 0 ).arg( w0[1], 0, 'f', 0 ).arg( w0[2], 0, 'f', 0 )
				.arg( e0[0], 0, 'f', 0 ).arg( e0[1], 0, 'f', 0 ).arg( e0[2], 0, 'f', 0 )
				.arg( n->worldTrans().scale, 0, 'f', 3 );
		}
		const Node * t = n;
		while ( t->parentNode() )
			t = t->parentNode();
		row.tree += top.contains( const_cast<Node *>( t ) ) ? 1 : 0;
		row.hidden += n->isHidden() ? 1 : 0;
		row.drawn += censusDrawn().contains( static_cast<const Node *>( n ) ) ? 1 : 0;
	}
	Row all;
	for ( auto r = rows.constBegin(); r != rows.constEnd(); ++r ) {
		const Row & v = r.value();
		std::fprintf( stderr, "cell animated objects drawn: %s x%d: copies with a node %d, shapes %d, in the scene tree %d, "
			"hidden %d, reached a draw %d, in the frame %d; first at %.0f %.0f %.0f (wrapper %.0f %.0f %.0f); %s\n",
			qPrintable( r.key() ), v.copies, v.nodes, v.shapes, v.tree, v.hidden, v.drawn, v.frame, v.at[0], v.at[1], v.at[2],
			v.wrapAt[0], v.wrapAt[1], v.wrapAt[2], qPrintable( v.probe ) );
		all.copies += v.copies;
		all.nodes += v.nodes;
		all.shapes += v.shapes;
		all.tree += v.tree;
		all.hidden += v.hidden;
		all.drawn += v.drawn;
		all.frame += v.frame;
	}
	std::fprintf( stderr, "cell animated objects drawn: total %d copies of %d models: copies with a node %d, shapes %d, in the "
		"scene tree %d, hidden %d, reached a draw %d (second paint), in the frame %d (first vertex in the view cone, nearer "
		"than 20 km)\n", all.copies, int( rows.size() ), all.nodes, all.shapes, all.tree, all.hidden, all.drawn, all.frame );
	std::fprintf( stderr, "cell particles placed: %d systems, %d in the frame, %d live sprites; %s\n", pfxSystems, pfxFrame,
		pfxLive, qPrintable( pfxProbe ) );
	censusDrawn().clear();
}
