#include "cellmodelahead.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "wwhwuse.h"	// lane EMPTY240 R3-1: the stream pool's busy count

#include "cellspeed.h"
#include "data/nifitemcache.h"
#include "esmdata.h"
#include "features/far-lod/lodgenparallel.h"

#include <QSet>
#include <QThread>

#include <atomic>
#include <cstdio>

// see cellmodelahead.h.

namespace
{

/* How many workers (WW_CELL_SPEED_THREADS=<n> overrides). Not capped at four like the generator's fan-out:
 * these workers do not queue on the item pool's mutex. Two cores are left to the window and the driver. */
int workerCap()
{
	bool ok = false;
	const int asked = wwEnvInt( "WW_CELL_SPEED_THREADS", &ok );
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
	const QByteArray red = wwEnv( "WW_CELL_SPEED_RED" );
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
	/* Must match the builder's MSWP rule (cellview.cpp, "THE MATERIAL SWAP"): first row per key wins, a
	 * self-swap is dropped. If they drift, take() misses and the builder loads the model itself. */
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

void CellModelAhead::wantModel( const QString & model )
{
	if ( !on_ || model.isEmpty() )
		return;
	const QString key = keyOf( model, nullptr );
	if ( byKey_.contains( key ) )
		return;
	byKey_.insert( key, int( jobs_.size() ) );
	Job j;
	j.model = model;
	jobs_.push_back( std::move( j ) );
}

void CellModelAhead::load()
{
	if ( !on_ || jobs_.empty() )
		return;
	CellSpeed::Acc speedAcc( "models read ahead on worker threads (wall)" );
	lodgenWarmSharedIndices();   // the lazy indices and the first document, built on this thread
	void * user = const_cast<QString *>( &dataRoot_ );
	/* lane EMPTY240 R4-4: a streamed cell asks for fewer than 32 models, so the generator's size floor parsed and inflated
	 * every one of them on the one builder thread (build 76: stream workers 0.5 of 14 busy). A model read is
	 * milliseconds of archive inflate + NIF parse, far over a pooled hand-off: two models are enough to fan out.
	 * Red WW_MODELPOOL_RED=1: the floor of 32 (build 76). */
	const bool poolRed = wwEnvInt( "WW_MODELPOOL_RED" ) == 1;
	const int floor = poolRed ? 32 : 2;
	{
		static std::atomic<int> said{ 0 };
		static std::atomic<long long> fanned{ 0 }, serial{ 0 };
		const bool fan = int( jobs_.size() ) >= floor && workerCap() > 1;
		( fan ? fanned : serial ).fetch_add( 1, std::memory_order_relaxed );
		const int s = said.fetch_add( 1, std::memory_order_relaxed );
		if ( s == 0 || ( s & 255 ) == 0 )
			std::fprintf( stderr, "stream model pool: %lld model sets parsed on up to %d workers, %lld on the asking thread; "
				"this set %d models (red WW_MODELPOOL_RED=1: under 32 models on the asking thread, build 76)\n",
				(long long) fanned.load(), workerCap(), (long long) serial.load(), int( jobs_.size() ) );
	}
	lodgenParallelFor( int( jobs_.size() ), [&]( int i ) {
		Job & j = jobs_[size_t( i )];
		NifItemThreadCache ownItems;
		WwHw::Busy wwBusy;	// lane EMPTY240 R3-1
		j.ok = lodgenNativeLoadModelPlacedOnce( user, j.model, j.subst.isEmpty() ? nullptr : &j.subst, &j.shapes );
		j.done = true;
	}, workerCap(), floor );
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
