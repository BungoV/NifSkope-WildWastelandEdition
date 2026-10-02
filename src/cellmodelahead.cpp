#include "cellmodelahead.h"

#include "cellspeed.h"
#include "data/nifitemcache.h"
#include "esmdata.h"
#include "lodgenparallel.h"

#include <QSet>
#include <QThread>

#include <cstdio>

// lane SPEED1 (2026-10-02): see cellmodelahead.h.

namespace
{

/* How many workers. Measured on bungo's machine (16 logical cores), WW_CELL_SPEED_THREADS=<n> for the ladder;
 * the generator's own model fan-out is capped at four for the item pool's mutex, which the workers here do
 * not queue on (data/nifitemcache.h). Two cores are left to the window and the graphics driver. */
int workerCap()
{
	bool ok = false;
	const int asked = qEnvironmentVariableIntValue( "WW_CELL_SPEED_THREADS", &ok );
	if ( ok && asked > 0 )
		return asked;
	return qBound( 1, QThread::idealThreadCount() - 2, 8 );
}

// the loader's own inputs, spelled out: the model as named and the swap rows as given
QString keyOf( const QString & model, const LodgenMaterialSubst * subst )
{
	QString k = model;
	if ( subst )
		for ( const auto & row : *subst )
			k += QChar( '\n' ) + row.first + QChar( '>' ) + row.second;
	return k;
}

} // namespace

CellModelAhead::CellModelAhead( const QString & dataRoot, bool on )
	: dataRoot_( dataRoot ), on_( on )
{
	const QByteArray red = qgetenv( "WW_CELL_SPEED_RED" );
	if ( red == "slow" || red == "nomodels" )
		on_ = false;
}

CellModelAhead::~CellModelAhead()
{
	if ( on_ && !jobs_.empty() ) {
		CellSpeed::count( "models read ahead", qint64( jobs_.size() ) );
		CellSpeed::count( "models read ahead and taken", taken_ );
	}
}

void CellModelAhead::want( const EsmWorld & world, quint32 base, quint32 swap )
{
	if ( !on_ )
		return;
	const QString model = world.lodBase( base ).model;
	if ( model.isEmpty() )
		return;
	// the builder leaves the sky's distant cards out (WW_CELL_SKY draws them: then they are loaded late)
	if ( QString( model ).replace( '/', '\\' ).startsWith( QLatin1String( "sky\\" ), Qt::CaseInsensitive ) )
		return;
	/* The swap rows the builder makes of an MSWP (cellview.cpp, "THE MATERIAL SWAP"): first row per key wins,
	 * a self-swap is dropped. If that rule changes there and not here, take() finds no answer under the
	 * builder's rows and the builder loads the model itself. */
	const LodgenMaterialSubst * subst = nullptr;
	if ( swap ) {
		auto it = swaps_.constFind( swap );
		if ( it == swaps_.constEnd() ) {
			LodgenMaterialSubst sub;
			const EsmMaterialSwap & mw = world.materialSwap( swap );
			if ( mw.exists ) {
				QSet<QString> seen;
				for ( const EsmMaterialSubst & row : mw.rows ) {
					const QString k = lodgenMaterialSwapKey( row.original );
					if ( k.isEmpty() || row.replacement.isEmpty() || seen.contains( k ) )
						continue;
					seen.insert( k );
					if ( lodgenMaterialSwapKey( row.replacement ) != k )
						sub.append( qMakePair( k, row.replacement ) );
				}
			}
			it = swaps_.insert( swap, sub );
		}
		if ( !it->isEmpty() )
			subst = &it.value();
	}
	const QString key = keyOf( model, subst );
	if ( byKey_.contains( key ) )
		return;
	byKey_.insert( key, int( jobs_.size() ) );
	Job j;
	j.model = model;
	if ( subst )
		j.subst = *subst;
	jobs_.push_back( std::move( j ) );
}

void CellModelAhead::load()
{
	if ( !on_ || jobs_.empty() )
		return;
	CellSpeed::Acc speedAcc( "models read ahead on worker threads (wall)" );
	lodgenWarmSharedIndices();   // the lazy indices and the first document, built on this thread
	void * user = const_cast<QString *>( &dataRoot_ );
	lodgenParallelFor( int( jobs_.size() ), [&]( int i ) {
		Job & j = jobs_[size_t( i )];
		NifItemThreadCache ownItems;
		j.ok = lodgenNativeLoadModelPlacedOnce( user, j.model, j.subst.isEmpty() ? nullptr : &j.subst, &j.shapes );
		j.done = true;
	}, workerCap() );
}

bool CellModelAhead::take( const QString & model, const LodgenMaterialSubst * subst,
	std::vector<NativeSrcShape> * out, bool * ok )
{
	if ( !on_ || jobs_.empty() )
		return false;
	if ( subst && subst->isEmpty() )
		subst = nullptr;
	auto it = byKey_.constFind( keyOf( model, subst ) );
	if ( it == byKey_.constEnd() )
		return false;
	Job & j = jobs_[size_t( it.value() )];
	if ( !j.done || j.taken )
		return false;
	j.taken = true;
	taken_++;
	*out = std::move( j.shapes );
	*ok = j.ok;
	return true;
}
