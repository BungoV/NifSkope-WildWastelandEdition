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

***** END LICENSE BLOCK *****/

#include "cellaodecal.h"

#include "nativeemit.h"
#include "probeplace.h"
#include "probebvh.h"
#include "data/niftypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <thread>

namespace {

constexpr float kMinSide = 64.0f;	// small clutter: a longest side under this is skipped (his call)
constexpr float kMaxSide = 1600.0f;	// past this (or kMaxHeight tall) it is a building, not a movable static
constexpr float kMaxHeight = 600.0f;

QString redOf()
{
	return QString::fromLatin1( qgetenv( "WW_CELL_AODECAL_RED" ) ).trimmed();
}

}	// namespace

bool aoDecalOn()
{
	const QByteArray pin = qgetenv( "WW_CELL_AODECAL" ).trimmed();
	if ( !pin.isEmpty() )
		return pin != "0";
	return QSettings().value( QStringLiteral( "WW/CellAoDecal" ), false ).toBool();
}

void aoDecalSetOn( bool on )
{
	QSettings().setValue( QStringLiteral( "WW/CellAoDecal" ), on );
}

QString aoDecalKind( const QString & model )
{
	const QString m = aovol::normPath( model );
	const QString file = m.section( '\\', -1 );
	auto any = [&]( const QString & s, std::initializer_list<const char *> words ) {
		for ( const char * w : words )
			if ( s.contains( QLatin1String( w ) ) )
				return true;
		return false;
	};
	/* refined on Concord's census (2026-10-04): set pieces that never move (a crashed vertibird, the molerat
	 * mounds, the power armor stand) and flat things that cast nothing (a sleeping bag) are not his kind */
	if ( any( m, { "vertibird", "molerat", "powerarmorfurniture", "sleepingbag", "landscape\\", "architecture\\" } ) )
		return QString();
	if ( any( m, { "dumpster", "trashbin\\" } ) )	// FO4's dumpsters are SetDressing\TrashBin\TrashBin*
		return QStringLiteral( "dumpster" );
	if ( m.contains( QLatin1String( "crate" ) ) )
		return QStringLiteral( "crate" );
	if ( m.contains( QLatin1String( "vehicles\\" ) ) || any( file, { "carhulk", "carframe" } )
		|| file.startsWith( QLatin1String( "car0" ) ) || file.startsWith( QLatin1String( "truck" ) )
		|| file.startsWith( QLatin1String( "van0" ) ) )
		return QStringLiteral( "car" );
	if ( m.contains( QLatin1String( "furniture\\" ) )
		|| any( file, { "desk", "cabinet", "locker", "bookcase", "shelf", "fridge", "workbench" } ) )
		return QStringLiteral( "furniture" );
	return QString();
}

void AoDecalSet::toModel( const Copy & c, const double w[3], double m[3] )
{
	const double d[3] = { w[0] - c.t[0], w[1] - c.t[1], w[2] - c.t[2] };
	for ( int a = 0; a < 3; a++ )
		m[a] = ( c.R[0 * 3 + a] * d[0] + c.R[1 * 3 + a] * d[1] + c.R[2 * 3 + a] * d[2] ) / c.s;
}

bool AoDecalSet::skyFree( const double p[3], const float vis[8], float out[8] ) const
{
	double prod[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	bool any = false;
	for ( const Copy & c : copies ) {
		if ( p[0] < c.wlo[0] || p[1] < c.wlo[1] || p[2] < c.wlo[2] || p[0] > c.whi[0] || p[1] > c.whi[1] || p[2] > c.whi[2] )
			continue;
		if ( c.model < 0 || !c.basis )
			continue;
		double m[3], V[8];
		toModel( c, p, m );
		aovol::octantOcclusion( models[size_t( c.model )].vol, *c.basis, m, V );
		for ( int o = 0; o < 8; o++ )
			prod[o] *= 1.0 - V[o];
		any = true;
	}
	for ( int o = 0; o < 8; o++ ) {
		if ( !any || red == QLatin1String( "nodivide" ) || !( vis[o] > 0.0f ) ) {
			out[o] = vis[o];
			continue;
		}
		const double occ = 1.0 - prod[o];
		out[o] = float( std::min( 1.0, double( vis[o] ) / std::max( 1.0 - occ, aovol::kGFloor ) ) );
	}
	return any;
}

AoDecalBuilder::AoDecalBuilder()
{
	on_ = aoDecalOn();
	if ( !on_ )
		return;
	set_ = std::make_shared<AoDecalSet>();
	set_->red = redOf();
	set_->dir = QString::fromLocal8Bit( qgetenv( "WW_CELL_AODECAL_DIR" ) ).trimmed();
	if ( set_->dir.isEmpty() )
		set_->dir = QCoreApplication::applicationDirPath() + QStringLiteral( "/prtp_bake/ao" );
	censusFile_ = QString::fromLocal8Bit( qgetenv( "WW_CELL_AODECAL_CENSUS" ) ).trimmed();
}

void AoDecalBuilder::consider( const QString & baseType, const QString & model, const std::vector<NativeSrcShape> & shapes,
	const Vector3 & pos, const Matrix & rot, float scale, unsigned ref, size_t tri0, size_t tri1 )
{
	if ( !on_ )
		return;
	considered_++;
	static const QStringList types { QStringLiteral( "STAT" ), QStringLiteral( "MSTT" ), QStringLiteral( "FURN" ),
		QStringLiteral( "CONT" ) };
	const bool redClutter = set_->red == QLatin1String( "clutter" );
	if ( !types.contains( baseType ) ) {
		skippedType_++;
		return;
	}
	const QString key = aovol::normPath( model );
	auto it = std::find_if( seenOrder_.begin(), seenOrder_.end(), [&]( const auto & e ) { return e.first == key; } );
	if ( it == seenOrder_.end() ) {
		Seen s;
		// the bake's triangles: the probe soup's per-shape rules (no effect, no blended, no decal, no refraction-only)
		std::vector<float> bake, mask;
		for ( const NativeSrcShape & sh : shapes ) {
			const size_t nv = sh.geom.pos.size() / 3;
			if ( !nv || sh.geom.tris.empty() || sh.nearFacts.effectShader )
				continue;
			const bool refractOnly = ( sh.shaderSF1 & ( 1U << 15 ) ) != 0;
			const bool soup = !refractOnly && sh.effectTex0.isEmpty() && !sh.nearFacts.alphaBlend && !sh.nearFacts.decal;
			for ( size_t t = 0; t + 2 < sh.geom.tris.size(); t += 3 ) {
				float w[9];
				bool ok = true;
				for ( int k = 0; k < 3 && ok; k++ ) {
					const size_t vi = size_t( sh.geom.tris[t + size_t( k )] );
					ok = vi < nv;
					if ( !ok )
						break;
					float v[3] = { sh.geom.pos[vi * 3], sh.geom.pos[vi * 3 + 1], sh.geom.pos[vi * 3 + 2] };
					if ( sh.billboard ) {	// lane GLOW1's node transform (model = R * local * scale + pos)
						float o[3];
						for ( int a = 0; a < 3; a++ )
							o[a] = sh.bbPos[a] + sh.bbScale * ( sh.bbRot[a * 3] * v[0] + sh.bbRot[a * 3 + 1] * v[1] + sh.bbRot[a * 3 + 2] * v[2] );
						std::copy( o, o + 3, v );
					}
					for ( int a = 0; a < 3; a++ )
						w[k * 3 + a] = v[a];
				}
				if ( !ok )
					continue;
				mask.insert( mask.end(), w, w + 9 );
				if ( soup )
					bake.insert( bake.end(), w, w + 9 );
			}
		}
		float lo[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, hi[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
		for ( size_t i = 0; i < bake.size(); i++ ) {
			lo[i % 3] = std::min( lo[i % 3], bake[i] );
			hi[i % 3] = std::max( hi[i % 3], bake[i] );
		}
		const float L = bake.empty() ? 0.0f : std::max( { hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] } );
		const float H = bake.empty() ? 0.0f : hi[2] - lo[2];
		const QString kind = aoDecalKind( model );
		if ( bake.empty() )
			s.why = QStringLiteral( "no soup triangles" );
		else if ( kind.isEmpty() && !redClutter )
			s.why = QStringLiteral( "not his kind" );
		else if ( L < kMinSide && !redClutter )
			s.why = QStringLiteral( "small clutter" );
		else if ( L > kMaxSide || H > kMaxHeight )
			s.why = QStringLiteral( "too big to move" );
		if ( s.why.isEmpty() ) {
			AoDecalSet::Model m;
			m.model = model;
			m.norm = aovol::normPath( QStringLiteral( "meshes\\" ) + model );
			m.maskTris = std::move( mask );
			s.model = int( set_->models.size() );
			set_->models.push_back( std::move( m ) );
			bakeTris_.push_back( std::move( bake ) );
		} else if ( s.why == QLatin1String( "not his kind" ) )
			skippedKind_++;
		else if ( s.why == QLatin1String( "small clutter" ) )
			skippedSmall_++;
		else if ( s.why == QLatin1String( "too big to move" ) )
			skippedBig_++;
		censusRows_ += QStringLiteral( "%1\t%2\t%3\t%4\t%5\t%6\n" ).arg( baseType, key ).arg( double( L ), 0, 'f', 1 )
			.arg( double( H ), 0, 'f', 1 ).arg( kind.isEmpty() ? QStringLiteral( "-" ) : kind )
			.arg( s.why.isEmpty() ? QStringLiteral( "decal" ) : s.why );
		seenOrder_.push_back( { key, s } );
		it = seenOrder_.end() - 1;
	}
	if ( it->second.model < 0 )
		return;
	AoDecalSet::Copy c;
	c.model = it->second.model;
	for ( int r = 0; r < 3; r++ ) {
		for ( int k = 0; k < 3; k++ )
			c.R[r * 3 + k] = double( rot( unsigned( r ), unsigned( k ) ) );
		c.t[r] = double( pos[r] );
	}
	c.s = scale > 0.0f ? double( scale ) : 1.0;
	std::copy( c.R, c.R + 9, c.placedR );
	std::copy( c.t, c.t + 3, c.placedT );
	c.placedS = c.s;
	c.ref = ref;
	c.soupTri0 = tri0;
	c.soupTri1 = tri1;
	set_->copies.push_back( c );
}

std::shared_ptr<const AoDecalSet> AoDecalBuilder::finish( const QString & dataRoot )
{
	if ( !on_ )
		return nullptr;
	AoDecalSet & S = *set_;
	QElapsedTimer clock;
	clock.start();
	QDir().mkpath( S.dir );
	const QString idxPath = S.dir + QStringLiteral( "/index.aoi" );
	QHash<QString, aovol::IndexRec> index;
	{
		QFile f( idxPath );
		QString why;
		if ( f.open( QIODevice::ReadOnly ) )
			aovol::readIndex( f.readAll(), &index, &why );
	}
	bool indexDirty = false;
	const quint64 salt = qgetenv( "WW_CELL_AODECAL_FPSALT" ).toULongLong();	// a gate's "the model changed"
	const bool redStale = S.red == QLatin1String( "stale" );
	for ( size_t mi = 0; mi < S.models.size(); mi++ ) {
		AoDecalSet::Model & M = S.models[mi];
		QByteArray bytes;
		{
			QFile f( dataRoot + QStringLiteral( "/meshes/" ) + QString( M.model ).replace( '\\', '/' ) );
			if ( f.open( QIODevice::ReadOnly ) )
				bytes = f.readAll();
		}
		if ( bytes.isEmpty() )	// archived: the fingerprint of the triangles the bake reads
			bytes = QByteArray( reinterpret_cast<const char *>( bakeTris_[mi].data() ), int( bakeTris_[mi].size() * sizeof( float ) ) );
		const std::uint64_t fp = aovol::fingerprint( bytes ) ^ salt;
		const QString file = S.dir + QLatin1Char( '/' ) + aovol::aoFileName( M.norm );
		bool have = false;
		auto rit = index.find( M.norm );
		if ( rit != index.end() ) {
			QFile f( file );
			if ( f.open( QIODevice::ReadOnly ) ) {
				const QByteArray b = f.readAll();
				QString why;
				const bool sizeOk = std::uint32_t( b.size() ) == rit->size && aovol::crc32( b.constData(), size_t( b.size() ) ) == rit->crc;
				const bool fpOk = redStale || ( rit->fp == fp );
				have = sizeOk && fpOk && aovol::readAo( b, redStale ? 0 : fp, &M.vol, &why );
				if ( !have )
					S.modelsRefused++;
			}
		}
		if ( have ) {
			S.modelsRead++;
			continue;
		}
		QString why;
		if ( !aovol::bake( bakeTris_[mi], &M.vol, &why ) ) {
			S.census += QStringLiteral( "  aodecal: %1 not baked (%2)\n" ).arg( M.model, why );
			continue;
		}
		M.vol.fp = fp;
		M.baked = true;
		S.modelsBaked++;
		S.msBake += M.vol.msBake;
		const QByteArray ao = aovol::aoBytes( M.vol );
		QFile f( file );
		if ( f.open( QIODevice::WriteOnly ) && f.write( ao ) == ao.size() ) {
			aovol::IndexRec r;
			r.hash = aovol::fnv1a64( M.norm.toUtf8() );
			r.fp = fp;
			r.path = M.norm;
			M.vol.footprint( r.lo, r.hi );
			r.size = std::uint32_t( ao.size() );
			r.crc = aovol::crc32( ao.constData(), size_t( ao.size() ) );
			index.insert( M.norm, r );
			indexDirty = true;
		}
	}
	if ( indexDirty ) {
		QVector<aovol::IndexRec> recs;
		for ( const auto & r : index )
			recs.append( r );
		const QByteArray b = aovol::indexBytes( recs );
		QFile f( idxPath );
		if ( f.open( QIODevice::WriteOnly ) )
			f.write( b );
	}
	// the copies whose model has a volume; the red "frozen" leaves every copy on its model's first copy
	std::vector<AoDecalSet::Copy> keep;
	QHash<int, int> firstOf;
	for ( AoDecalSet::Copy c : S.copies ) {
		if ( S.models[size_t( c.model )].vol.k.empty() )
			continue;
		if ( S.red == QLatin1String( "frozen" ) ) {
			auto f = firstOf.find( c.model );
			if ( f == firstOf.end() )
				firstOf.insert( c.model, int( keep.size() ) );
			else {
				const AoDecalSet::Copy & c0 = keep[size_t( f.value() )];
				std::copy( c0.R, c0.R + 9, c.R );
				std::copy( c0.t, c0.t + 3, c.t );
				c.s = c0.s;
			}
		}
		float flo[3], fhi[3];
		S.models[size_t( c.model )].vol.footprint( flo, fhi );
		for ( int a = 0; a < 3; a++ ) {
			c.wlo[a] = 3.4e38f;
			c.whi[a] = -3.4e38f;
		}
		for ( int k = 0; k < 8; k++ ) {
			const double m[3] = { ( k & 1 ) ? fhi[0] : flo[0], ( k & 2 ) ? fhi[1] : flo[1], ( k & 4 ) ? fhi[2] : flo[2] };
			for ( int a = 0; a < 3; a++ ) {
				const double w = c.t[a] + c.s * ( c.R[a * 3] * m[0] + c.R[a * 3 + 1] * m[1] + c.R[a * 3 + 2] * m[2] );
				c.wlo[a] = std::min( c.wlo[a], float( w ) );
				c.whi[a] = std::max( c.whi[a], float( w ) );
			}
		}
		c.basis = std::make_shared<aovol::OctantBasis>();
		aovol::octantBasis( c.R, c.basis.get() );
		keep.push_back( c );
	}
	S.copies.swap( keep );
	qint64 bytes = 0;
	for ( const auto & M : S.models )
		bytes += M.vol.k.empty() ? 0 : qint64( aovol::kAoHead + M.vol.payload.size() );
	S.census += QStringLiteral( "  aodecal: %1 placements considered, %2 models with a volume (%3 baked in %4 s, %5 read, %6 refused "
		"as stale or damaged; %7 KB), %8 copies; skipped models: %9 not his kind, %10 small clutter, %11 too big, %12 placements "
		"of other types%13\n" ).arg( considered_ ).arg( S.models.size() ).arg( S.modelsBaked ).arg( S.msBake / 1000.0, 0, 'f', 1 )
		.arg( S.modelsRead ).arg( S.modelsRefused ).arg( bytes / 1024 ).arg( S.copies.size() ).arg( skippedKind_ ).arg( skippedSmall_ )
		.arg( skippedBig_ ).arg( skippedType_ ).arg( S.red.isEmpty() ? QString() : QStringLiteral( " [red %1]" ).arg( S.red ) );
	if ( !censusFile_.isEmpty() ) {
		QFile f( censusFile_ );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream ts( &f );
			ts << "# type\tmodel\tlongest\theight\tkind\tverdict\n" << censusRows_;
			ts << "# copies: model\tref\tR(9)\tt(3)\ts\n";
			for ( const auto & c : S.copies ) {
				ts << S.models[size_t( c.model )].norm << '\t' << QString::number( c.ref, 16 );
				for ( double v : c.R )
					ts << '\t' << QString::number( v, 'g', 9 );
				for ( double v : c.t )
					ts << '\t' << QString::number( v, 'g', 9 );
				ts << '\t' << QString::number( c.s, 'g', 9 ) << '\n';
			}
			ts << "#" << S.census;
		}
	}
	bakeTris_.clear();
	return set_;
}

QString AoDecalBuilder::census() const
{
	return set_ ? set_->census : QString();
}

// ---------------------------------------------------------------- gate D on a real cell
bool aoDecalProbeGate( const AoDecalSet & set, const ProbeSoup & soup, const std::vector<float> & probePos,
	const std::vector<float> & probeVis, const QString & file, QString * summary )
{
	const size_t np = probePos.size() / 3;
	std::vector<size_t> near;
	for ( size_t i = 0; i < np; i++ ) {
		const float * p = &probePos[i * 3];
		for ( const auto & c : set.copies )
			if ( p[0] >= c.wlo[0] && p[1] >= c.wlo[1] && p[2] >= c.wlo[2] && p[0] <= c.whi[0] && p[1] <= c.whi[1] && p[2] <= c.whi[2] ) {
				near.push_back( i );
				break;
			}
	}
	// the soup twice (local origin: the probes' mean x, y), with the copies and without them
	double O[2] = { 0, 0 };
	for ( size_t i : near )
		for ( int a = 0; a < 2; a++ )
			O[a] += double( probePos[i * 3 + size_t( a )] ) / double( std::max<size_t>( near.size(), 1 ) );
	std::vector<char> isCopy( soup.tris.size() / 9, 0 );
	for ( const auto & c : set.copies )
		for ( size_t t = c.soupTri0; t < c.soupTri1 && t < isCopy.size(); t++ )
			isCopy[t] = 1;
	probebvh::Bvh withB, freeB;
	for ( size_t t = 0; t < isCopy.size(); t++ )
		for ( int k = 0; k < 9; k++ ) {
			const float v = float( double( soup.tris[t * 9 + size_t( k )] ) - ( k % 3 < 2 ? O[k % 3] : 0.0 ) );
			withB.t.push_back( v );
			if ( !isCopy[t] )
				freeB.t.push_back( v );
		}
	withB.build();
	freeB.build();
	const int N = 4096;
	std::vector<double> D( size_t( N ) * 3 );
	std::vector<int> oc( static_cast<size_t>( N ) );
	int cnt[8] = {};
	for ( int i = 0; i < N; i++ ) {
		aovol::fib( i, N, &D[size_t( i ) * 3] );
		const double * d = &D[size_t( i ) * 3];
		oc[size_t( i )] = ( d[0] < 0 ? 1 : 0 ) | ( d[1] < 0 ? 2 : 0 ) | ( d[2] < 0 ? 4 : 0 );
		cnt[oc[size_t( i )]]++;
	}
	struct Row { float with[8], free[8], rec[8], recBake[8]; };
	std::vector<Row> rows( near.size() );
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	const int nThreads = std::max( 1, int( std::thread::hardware_concurrency() ) );
	for ( int th = 0; th < nThreads; th++ )
		pool.emplace_back( [&]() {
			for ( size_t k; ( k = next.fetch_add( 1 ) ) < near.size(); ) {
				const float * p = &probePos[near[k] * 3];
				const double o[3] = { double( p[0] ) - O[0], double( p[1] ) - O[1], double( p[2] ) };
				int w[8] = {}, f[8] = {};
				for ( int i = 0; i < N; i++ ) {
					const double * d = &D[size_t( i ) * 3];
					double t;
					if ( !withB.ray( o, d, 131072.0, &t ) )
						w[oc[size_t( i )]]++;
					if ( !freeB.ray( o, d, 131072.0, &t ) )
						f[oc[size_t( i )]]++;
				}
				Row & r = rows[k];
				for ( int q = 0; q < 8; q++ ) {
					r.with[q] = float( w[q] ) / float( cnt[q] );
					r.free[q] = float( f[q] ) / float( cnt[q] );
				}
				const double pw[3] = { p[0], p[1], p[2] };
				set.skyFree( pw, r.with, r.rec );
				set.skyFree( pw, &probeVis[near[k] * 8], r.recBake );
			}
		} );
	for ( auto & t : pool )
		t.join();
	QFile f( file );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return false;
	QTextStream ts( &f );
	ts << "# x y z | bake skyVis[8] | traced with copies[8] | traced without[8] | rebuilt from traced-with[8] | rebuilt from bake[8]"
	   << ( set.red.isEmpty() ? QString() : QStringLiteral( " [red %1]" ).arg( set.red ) ) << "\n";
	for ( size_t k = 0; k < near.size(); k++ ) {
		const float * p = &probePos[near[k] * 3];
		ts << p[0] << ' ' << p[1] << ' ' << p[2];
		for ( const float * a : std::initializer_list<const float *> { &probeVis[near[k] * 8], rows[k].with, rows[k].free, rows[k].rec, rows[k].recBake } )
			for ( int q = 0; q < 8; q++ )
				ts << ' ' << QString::number( double( a[q] ), 'f', 5 );
		ts << '\n';
	}
	if ( summary )
		*summary = QStringLiteral( "aodecal gate: %1 of %2 probes within a copy's footprint traced (%3 soup triangles, %4 of them copies')" )
			.arg( near.size() ).arg( np ).arg( isCopy.size() ).arg( std::count( isCopy.begin(), isCopy.end(), char( 1 ) ) );
	return true;
}

// ---------------------------------------------------------------- gate A on a real model (nifcli aobake)
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out );

int aoDecalCli( const QStringList & args )
{
	QString data, model, dir, recv, outFile, red, copyStr, trisFile;
	for ( int i = 0; i + 1 < args.size(); i += 2 ) {
		const QString & k = args[i];
		const QString & v = args[i + 1];
		if ( k == QLatin1String( "--data" ) ) data = v;
		else if ( k == QLatin1String( "--model" ) ) model = v;
		else if ( k == QLatin1String( "--dir" ) ) dir = v;
		else if ( k == QLatin1String( "--receivers" ) ) recv = v;
		else if ( k == QLatin1String( "--out" ) ) outFile = v;
		else if ( k == QLatin1String( "--red" ) ) red = v;
		else if ( k == QLatin1String( "--copy" ) ) copyStr = v;
		else if ( k == QLatin1String( "--tris" ) ) trisFile = v;
	}
	QTextStream so( stdout );
	if ( data.isEmpty() || model.isEmpty() || dir.isEmpty() ) {
		so << "usage: aobake --data <Data folder> --model <data-relative .nif, no meshes\> --dir <.ao folder> "
			"[--receivers <float32 P3 N3 each, world> --copy \"R9 t3 s\" --out <float32 lookup, brute each>] "
			"[--red worlddown|stale|clutter]\n";
		so.flush();
		return 2;
	}
	qputenv( "WW_CELL_AODECAL", "1" );
	qputenv( "WW_CELL_AODECAL_DIR", dir.toLocal8Bit() );
	if ( red == QLatin1String( "stale" ) || red == QLatin1String( "clutter" ) )
		qputenv( "WW_CELL_AODECAL_RED", red.toLatin1() );
	std::vector<NativeSrcShape> shapes;
	QString root = data;
	if ( !lodgenNativeLoadModel( &root, model, &shapes ) || shapes.empty() ) {
		so << "aobake: the model did not load\n";
		so.flush();
		return 1;
	}
	AoDecalBuilder b;
	Matrix I;
	b.consider( QStringLiteral( "STAT" ), model, shapes, Vector3( 0, 0, 0 ), I, 1.0f, 0, 0, 0 );
	std::shared_ptr<const AoDecalSet> S = b.finish( data );
	so << b.census();
	if ( !S || S->models.empty() || S->models[0].vol.k.empty() ) {
		so << "aobake: no volume\n";
		so.flush();
		return 1;
	}
	const aovol::Volume & v = S->models[0].vol;
	so << QStringLiteral( "aobake: %1 dims %2x%3x%4 box %5 %6 %7 .. %8 %9 %10 fade %11 rcut %12 tris %13 inside %14 baked %15 ms %16 fp %17\n" )
		.arg( S->models[0].norm ).arg( v.dims[0] ).arg( v.dims[1] ).arg( v.dims[2] )
		.arg( double( v.lo[0] ), 0, 'f', 2 ).arg( double( v.lo[1] ), 0, 'f', 2 ).arg( double( v.lo[2] ), 0, 'f', 2 )
		.arg( double( v.hi[0] ), 0, 'f', 2 ).arg( double( v.hi[1] ), 0, 'f', 2 ).arg( double( v.hi[2] ), 0, 'f', 2 )
		.arg( double( v.fade ), 0, 'f', 2 ).arg( double( v.rcut ), 0, 'f', 1 ).arg( v.tris ).arg( v.insideVoxels )
		.arg( S->models[0].baked ? 1 : 0 ).arg( v.msBake, 0, 'f', 0 ).arg( v.fp, 16, 16, QLatin1Char( '0' ) );
	so << "aobake: file " << aovol::aoFileName( S->models[0].norm ) << "\n";
	if ( recv.isEmpty() || outFile.isEmpty() ) {
		so.flush();
		return 0;
	}
	// the copy (model -> world) and the model's own bake triangles placed by it, for the brute force
	AoDecalSet::Copy c;
	c.model = 0;
	{
		const QStringList p = copyStr.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		if ( p.size() == 13 ) {
			for ( int k = 0; k < 9; k++ )
				c.R[k] = p[k].toDouble();
			for ( int k = 0; k < 3; k++ )
				c.t[k] = p[9 + k].toDouble();
			c.s = p[12].toDouble();
		}
	}
	// the bake's triangles again (consider() handed them to finish(), which let them go): the soup rules
	std::vector<float> tris;
	for ( const NativeSrcShape & sh : shapes ) {
		const size_t nv = sh.geom.pos.size() / 3;
		const bool refractOnly = ( sh.shaderSF1 & ( 1U << 15 ) ) != 0;
		if ( !nv || sh.nearFacts.effectShader || refractOnly || !sh.effectTex0.isEmpty() || sh.nearFacts.alphaBlend || sh.nearFacts.decal )
			continue;
		for ( size_t t = 0; t + 2 < sh.geom.tris.size(); t += 3 ) {
			float w[9];
			bool ok = true;
			for ( int k = 0; k < 3 && ok; k++ ) {
				const size_t vi = size_t( sh.geom.tris[t + size_t( k )] );
				ok = vi < nv;
				if ( !ok )
					break;
				float m[3] = { sh.geom.pos[vi * 3], sh.geom.pos[vi * 3 + 1], sh.geom.pos[vi * 3 + 2] };
				if ( sh.billboard ) {
					float o[3];
					for ( int a = 0; a < 3; a++ )
						o[a] = sh.bbPos[a] + sh.bbScale * ( sh.bbRot[a * 3] * m[0] + sh.bbRot[a * 3 + 1] * m[1] + sh.bbRot[a * 3 + 2] * m[2] );
					std::copy( o, o + 3, m );
				}
				for ( int a = 0; a < 3; a++ )	// world
					w[k * 3 + a] = float( c.t[a] + c.s * ( c.R[a * 3] * m[0] + c.R[a * 3 + 1] * m[1] + c.R[a * 3 + 2] * m[2] ) );
			}
			if ( ok )
				tris.insert( tris.end(), w, w + 9 );
		}
	}
	probebvh::Bvh bvh;
	bvh.t = tris;
	bvh.build();
	if ( !trisFile.isEmpty() ) {	// the placed bake triangles (world), for the gate's near / far split
		QFile tf( trisFile );
		if ( tf.open( QIODevice::WriteOnly ) )
			tf.write( reinterpret_cast<const char *>( tris.data() ), qint64( tris.size() * sizeof( float ) ) );
	}
	QFile rf( recv );
	if ( !rf.open( QIODevice::ReadOnly ) ) {
		so << "aobake: no receivers file\n";
		so.flush();
		return 1;
	}
	const QByteArray rb = rf.readAll();
	const size_t n = size_t( rb.size() ) / ( 6 * sizeof( float ) );
	const float * R6 = reinterpret_cast<const float *>( rb.constData() );
	std::vector<float> res( n * 3 );	// lookup, brute force, back-face share of the hits (inside > 0.5)
	const int NR = 4096;
	std::vector<double> D( size_t( NR ) * 3 );
	for ( int i = 0; i < NR; i++ )
		aovol::fib( i, NR, &D[size_t( i ) * 3] );
	// the red "worlddown": the projected decal (the copy's yaw only, every receiver facing world up)
	AoDecalSet::Copy cl = c;
	const bool worlddown = red == QLatin1String( "worlddown" );
	if ( worlddown ) {
		double ax[2] = { c.R[0], c.R[3] };
		const double l = std::hypot( ax[0], ax[1] );
		const double yaw = l > 1e-6 ? std::atan2( ax[1], ax[0] ) : 0.0;
		const double cy = std::cos( yaw ), sy = std::sin( yaw );
		const double Ry[9] = { cy, -sy, 0, sy, cy, 0, 0, 0, 1 };
		std::copy( Ry, Ry + 9, cl.R );
		cl.s = 1.0;
	}
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	const int nThreads = std::max( 1, int( std::thread::hardware_concurrency() ) );
	for ( int th = 0; th < nThreads; th++ )
		pool.emplace_back( [&]() {
			for ( size_t i; ( i = next.fetch_add( 1 ) ) < n; ) {
				const float * q = &R6[i * 6];
				const double P[3] = { q[0], q[1], q[2] }, N[3] = { q[3], q[4], q[5] };
				// the lookup: the receiver in the copy's model space (AoDecalSet::toModel), the normal by R^T
				double m[3], nm[3];
				AoDecalSet::toModel( cl, P, m );
				if ( worlddown ) {
					nm[0] = 0.0;
					nm[1] = 0.0;
					nm[2] = 1.0;
				} else {
					for ( int a = 0; a < 3; a++ )
						nm[a] = c.R[0 * 3 + a] * N[0] + c.R[1 * 3 + a] * N[1] + c.R[2 * 3 + a] * N[2];
				}
				res[i * 3] = float( aovol::lookup( v, m, nm ) );
				// the brute force: 4096 Fibonacci directions, cosine weighted, against the placed triangles
				double blk = 0.0, cw = 0.0;
				int hits = 0, back = 0;
				for ( int k = 0; k < NR; k++ ) {
					const double * d = &D[size_t( k ) * 3];
					const double cs = d[0] * N[0] + d[1] * N[1] + d[2] * N[2];
					double t;
					int tri = -1;
					const bool hit = bvh.ray( P, d, 1.0e7, &t, &tri );
					if ( hit && tri >= 0 ) {	// the inside test of the bake: a back face first
						const float * a = &tris[size_t( tri ) * 9];
						const double e1[3] = { a[3] - a[0], a[4] - a[1], a[5] - a[2] }, e2[3] = { a[6] - a[0], a[7] - a[1], a[8] - a[2] };
						const double fn[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
						hits++;
						back += ( fn[0] * d[0] + fn[1] * d[1] + fn[2] * d[2] ) > 0.0 ? 1 : 0;
					}
					if ( cs <= 0.0 )
						continue;
					cw += cs;
					if ( hit )
						blk += cs;
				}
				res[i * 3 + 1] = float( cw > 0.0 ? 1.0 - blk / cw : 1.0 );
				res[i * 3 + 2] = hits ? float( back ) / float( hits ) : 0.0f;
			}
		} );
	for ( auto & t : pool )
		t.join();
	QFile of( outFile );
	if ( !of.open( QIODevice::WriteOnly ) ) {
		so << "aobake: the output did not open\n";
		so.flush();
		return 1;
	}
	of.write( reinterpret_cast<const char *>( res.data() ), qint64( res.size() * sizeof( float ) ) );
	so << "aobake: " << n << " receivers looked up and brute-forced (4096 rays each)" << ( worlddown ? " [red worlddown]" : "" ) << "\n";
	so.flush();
	return 0;
}
