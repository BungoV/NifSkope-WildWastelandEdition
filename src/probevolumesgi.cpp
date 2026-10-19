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
