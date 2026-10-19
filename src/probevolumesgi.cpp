#include "probevolumesgi.h"

#include <QMap>

#include <algorithm>
#include <cmath>

// lane VOLFIT1: see probevolumesgi.h
ProbeVolumesGi probeVolumesToGi( const ProbeVolumes & v, const ProbeRooms & gi, float splitShare, const QString & red )
{
	ProbeVolumesGi g;
	const double off = red == QLatin1String( "nogimap" ) ? 100000.0 : 0.0;
	QMap<int, int> roomsPerGi;
	int firstId = 0;   // red "gione": every room takes the first mapped room's id
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		ProbeVolumesGiRoom m;
		if ( gi.ok && !r.shapes.empty() && gi.cell > 0 ) {
			// the shapes' box, conservative (a rotated shape's half diagonal)
			double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 };
			for ( const ProbeVolumeShape & s : r.shapes ) {
				const double h = 0.5 * std::sqrt( double( s.size[0] ) * s.size[0] + double( s.size[1] ) * s.size[1] + double( s.size[2] ) * s.size[2] );
				for ( int c = 0; c < 3; c++ ) {
					lo[c] = std::min( lo[c], s.center[c] - h );
					hi[c] = std::max( hi[c], s.center[c] + h );
				}
			}
			int i0[3], i1[3];
			qint64 n = 1;
			for ( int c = 0; c < 3; c++ ) {
				i0[c] = std::max( 0, int( std::floor( ( lo[c] - gi.origin[c] ) / gi.cell ) ) );
				i1[c] = std::min( gi.dims[c] - 1, int( std::floor( ( hi[c] - gi.origin[c] ) / gi.cell ) ) );
				n *= std::max( 0, i1[c] - i0[c] + 1 );
			}
			// a stride keeps a huge room's vote under ~2M cell reads
			const int step = n > 2000000 ? int( std::ceil( std::cbrt( double( n ) / 2000000.0 ) ) ) : 1;
			QMap<int, qint64> votes;
			for ( int z = i0[2]; z <= i1[2]; z += step )
				for ( int y = i0[1]; y <= i1[1]; y += step )
					for ( int x = i0[0]; x <= i1[0]; x += step ) {
						const float p[3] = { float( gi.origin[0] + ( x + 0.5 ) * gi.cell ), float( gi.origin[1] + ( y + 0.5 ) * gi.cell ),
							float( gi.origin[2] + ( z + 0.5 ) * gi.cell ) };
						bool in = false;
						for ( const ProbeVolumeShape & s : r.shapes )
							if ( s.contains( p ) ) {
								in = true;
								break;
							}
						if ( !in )
							continue;
						m.samples++;
						const double q[3] = { p[0] + off, p[1], p[2] };
						int ab[2];
						if ( !gi.at( q, ab ) )
							continue;
						if ( ab[0] == 0 )
							m.outdoors++;
						else if ( ab[0] > 0 )
							votes[ab[0]]++;
					}
			std::vector<std::pair<qint64, int>> ranked;
			for ( auto it = votes.constBegin(); it != votes.constEnd(); ++it )
				ranked.push_back( { it.value(), it.key() } );
			std::sort( ranked.begin(), ranked.end(), []( const auto & a, const auto & b ) {
				return a.first != b.first ? a.first > b.first : a.second < b.second;
			} );
			for ( const auto & rk : ranked )
				m.split << QStringLiteral( "%1:%2" ).arg( rk.second ).arg( rk.first );
			if ( !ranked.empty() ) {
				m.giRoom = red == QLatin1String( "gione" ) && firstId > 0 ? firstId : ranked[0].second;
				if ( firstId < 1 )
					firstId = m.giRoom;
				m.votes = ranked[0].first;
				qint64 all = 0;
				for ( const auto & rk : ranked )
					all += rk.first;
				if ( ranked.size() > 1 && double( ranked[1].first ) >= double( splitShare ) * double( all ) ) {
					g.several++;
					g.log << QStringLiteral( "room %1 (%2) maps to several GI rooms: %3; takes %4" ).arg( r.id ).arg( r.cellKey )
						.arg( m.split.join( ' ' ) ).arg( m.giRoom );
				}
			}
		}
		if ( m.giRoom < 1 ) {
			g.none++;
			g.log << QStringLiteral( "room %1 (%2) maps to no GI room (samples %3, outdoors %4); left out" ).arg( r.id ).arg( r.cellKey )
				.arg( m.samples ).arg( m.outdoors );
		} else {
			g.mapped++;
			roomsPerGi[m.giRoom]++;
			for ( const ProbeVolumeShape & s : r.shapes ) {
				ProbeVolumesGiShape d;
				d.type = int( s.type );
				for ( int c = 0; c < 3; c++ ) {
					d.center[c] = s.center[c];
					d.size[c] = s.size[c];
					d.rotation[c] = s.rotation[c];
				}
				d.room = m.giRoom;
				g.shapes.push_back( d );
			}
		}
		g.rooms.push_back( m );
	}
	for ( int k : roomsPerGi )
		if ( k > 1 )
			g.shared++;
	// thresholds: a and b are 1-based Space rooms (0 outside); a dropped room's or a self-joining one is left out
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		int ids[2];
		const int ends[2] = { t.a, t.b };
		bool keep = true;
		for ( int e = 0; e < 2; e++ ) {
			if ( ends[e] == 0 )
				ids[e] = 0;
			else if ( ends[e] >= 1 && ends[e] <= int( g.rooms.size() ) && g.rooms[ends[e] - 1].giRoom >= 1 )
				ids[e] = g.rooms[ends[e] - 1].giRoom;
			else
				keep = false;
		}
		if ( !keep || ids[0] == ids[1] ) {
			g.thresholdsDropped++;
			continue;
		}
		ProbeVolumesGiThreshold d;
		d.shape.type = int( t.shape.type );
		for ( int c = 0; c < 3; c++ ) {
			d.shape.center[c] = t.shape.center[c];
			d.shape.size[c] = t.shape.size[c];
			d.shape.rotation[c] = t.shape.rotation[c];
		}
		d.rooms[0] = ids[0];
		d.rooms[1] = ids[1];
		g.thresholds.push_back( d );
	}
	g.census = QStringLiteral( "space to GI: rooms %1, mapped %2, none %3, several %4, GI rooms holding more than one %5; "
		"shapes %6, thresholds %7 (left out %8)" ).arg( v.rooms.size() ).arg( g.mapped ).arg( g.none ).arg( g.several )
		.arg( g.shared ).arg( g.shapes.size() ).arg( g.thresholds.size() ).arg( g.thresholdsDropped );
	return g;
}

namespace {

// every cell of `r` whose centre `s` holds: f( cell index )
template <class F>
void forShapeCells( const ProbeRooms & r, const ProbeVolumeShape & s, F f )
{
	const double h = 0.5 * std::sqrt( double( s.size[0] ) * s.size[0] + double( s.size[1] ) * s.size[1] + double( s.size[2] ) * s.size[2] );
	int i0[3], i1[3];
	for ( int c = 0; c < 3; c++ ) {
		i0[c] = std::max( 0, int( std::floor( ( s.center[c] - h - r.origin[c] ) / r.cell ) ) );
		i1[c] = std::min( r.dims[c] - 1, int( std::floor( ( s.center[c] + h - r.origin[c] ) / r.cell ) ) );
	}
	for ( int z = i0[2]; z <= i1[2]; z++ )
		for ( int y = i0[1]; y <= i1[1]; y++ )
			for ( int x = i0[0]; x <= i1[0]; x++ ) {
				const float p[3] = { r.origin[0] + ( x + 0.5f ) * r.cell, r.origin[1] + ( y + 0.5f ) * r.cell, r.origin[2] + ( z + 0.5f ) * r.cell };
				if ( s.contains( p ) )
					f( r.index( x, y, z ) );
			}
}

}

QString probeRoomsFromSpace( const ProbeVolumes & v, ProbeRooms * r, const QString & red )
{
	const bool shapesOn = red != QLatin1String( "spaceshapes" );
	const size_t N = r->a.size();
	const int nRooms = std::min( int( v.rooms.size() ), 4094 );   // the pack holds ids up to 4094
	std::vector<qint16> own( N, 0 );
	std::vector<double> vol( size_t( nRooms ) + 1, 0.0 );
	for ( int k = 1; k <= nRooms; k++ )
		for ( const ProbeVolumeShape & s : v.rooms[size_t( k - 1 )].shapes )
			vol[size_t( k )] += s.volume();
	// 1. the shapes: a cell takes the smallest room holding its centre (a room inside a room keeps its own)
	std::vector<qint64> cellsOf( size_t( nRooms ) + 1, 0 ), wasOutdoors( size_t( nRooms ) + 1, 0 );
	for ( int k = 1; k <= nRooms; k++ )
		for ( const ProbeVolumeShape & s : v.rooms[size_t( k - 1 )].shapes )
			forShapeCells( *r, s, [&]( size_t i ) {
				if ( r->a[i] < 0 || !shapesOn )
					return;   // solid stays solid (a shape's overshoot into the wall)
				if ( own[i] == 0 || vol[size_t( k )] < vol[size_t( own[i] )] )
					own[i] = qint16( k );
			} );
	// 2. each ROOMCLAMP1 room's cells: the Space room most of its shape-held cells took (the fit's gaps)
	QMap<int, QMap<int, qint64>> votes;
	for ( size_t i = 0; i < N; i++ )
		if ( own[i] > 0 ) {
			cellsOf[size_t( own[i] )]++;
			if ( r->a[i] == 0 )
				wasOutdoors[size_t( own[i] )]++;
			else if ( r->a[i] > 0 )
				votes[r->a[i]][own[i]]++;
		}
	QMap<int, int> rcTo;
	for ( auto it = votes.constBegin(); it != votes.constEnd(); ++it ) {
		int best = 0;
		qint64 bv = 0;
		for ( auto jt = it.value().constBegin(); jt != it.value().constEnd(); ++jt )
			if ( jt.value() > bv ) {
				bv = jt.value();
				best = jt.key();
			}
		rcTo[it.key()] = best;
	}
	qint64 gapCells = 0, rcOutdoors = 0;
	std::vector<qint16> a( N ), b( N, -1 );
	for ( size_t i = 0; i < N; i++ ) {
		const int o = r->a[i];
		if ( own[i] > 0 )
			a[i] = own[i];
		else if ( o > 0 && rcTo.value( o, 0 ) > 0 ) {
			a[i] = qint16( rcTo.value( o ) );
			gapCells++;
		} else {
			a[i] = o < 0 ? qint16( -1 ) : qint16( 0 );
			if ( o > 0 )
				rcOutdoors++;   // a ROOMCLAMP1 room the Space stage did not keep (open, sky, too small)
		}
	}
	// 3. the second names: the Space thresholds only (a box grown 1.5 cells each way across, half a cell along)
	qint64 bCells = 0;
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		if ( t.a > nRooms || t.b > nRooms )
			continue;
		ProbeVolumeShape s = t.shape;
		s.size[0] += r->cell;
		s.size[1] += 3.0f * r->cell;
		s.size[2] += r->cell;
		forShapeCells( *r, s, [&]( size_t i ) {
			const int here = a[i];
			const int other = here == t.a ? t.b : here == t.b ? t.a : -1;
			if ( here >= 0 && other >= 0 && other != here && b[i] < 0 ) {
				b[i] = qint16( other );
				bCells++;
			}
		} );
	}
	int inGrid = 0, interiorNow = 0;
	for ( int k = 1; k <= nRooms; k++ ) {
		if ( cellsOf[size_t( k )] > 0 )
			inGrid++;
		if ( cellsOf[size_t( k )] > 0 && 2 * wasOutdoors[size_t( k )] > cellsOf[size_t( k )] )
			interiorNow++;
	}
	r->a.swap( a );
	r->b.swap( b );
	r->rooms = nRooms;
	return QStringLiteral( "space rooms into GI: rooms %1, mapped %2, unmapped %3, interior where the room grid had outdoors %4; "
		"room-grid rooms taken over %5, room-grid room cells now outdoors %6, gap cells %7, threshold cells %8 (grid %9x%10x%11 cell %12)" )
		.arg( nRooms ).arg( inGrid ).arg( nRooms - inGrid ).arg( interiorNow ).arg( rcTo.size() ).arg( rcOutdoors ).arg( gapCells )
		.arg( bCells ).arg( r->dims[0] ).arg( r->dims[1] ).arg( r->dims[2] ).arg( double( r->cell ), 0, 'f', 1 );
}
