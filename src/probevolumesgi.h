#ifndef PROBEVOLUMESGI_H
#define PROBEVOLUMESGI_H

/* lane VOLFIT1: the Space stage's rooms (src/probevolumes.h) as the GI's volume input. The GI reads a voxel only from
 * the slots of its room, and those carry the room grid's ids (ProbeRooms::a), so each Space room takes the GI room id
 * its shapes overlap most (a vote over the GI grid's cells inside the shapes). A room that overlaps no GI room is left
 * out; one whose votes split (the runner-up holds >= splitShare of them) keeps the winner. Both are logged.
 * Thresholds keep their shape and take their rooms' ids (0 = outdoors); one joining a room to itself is left out. */

#include "probevolumes.h"
#include "proberooms.h"

#include <QString>
#include <QStringList>

#include <vector>

struct ProbeVolumesGiShape
{
	int type = 0;                       //!< ProbeShapeType: box 0, cylinder 1, wedge 2, capsule 3, sphere 4
	float center[3] = { 0, 0, 0 };
	float size[3] = { 0, 0, 0 };
	float rotation[3] = { 0, 0, 0 };    //!< degrees, R = Rz Ry Rx
	int room = 0;                       //!< the GI room grid's id
};

struct ProbeVolumesGiThreshold
{
	ProbeVolumesGiShape shape;
	int rooms[2] = { 0, 0 };
};

struct ProbeVolumesGiRoom
{
	int giRoom = -1;                    //!< -1: maps to none
	qint64 votes = 0, samples = 0, outdoors = 0;
	QStringList split;                  //!< "id:votes" of every GI room it overlaps, most first
};

struct ProbeVolumesGi
{
	std::vector<ProbeVolumesGiRoom> rooms;  //!< one per ProbeVolumes::rooms
	std::vector<ProbeVolumesGiShape> shapes;
	std::vector<ProbeVolumesGiThreshold> thresholds;
	int mapped = 0, none = 0, several = 0, shared = 0, thresholdsDropped = 0;
	QStringList log;                    //!< one line per room that maps to none or to several
	QString census;
};

/*! Map `v` onto the GI room grid `gi`. splitShare: the runner-up's share of the votes that logs "several".
 *  red "nogimap": the votes are read 100000 units off (every room maps to none); "gione": every room takes the first
 *  mapped room's id (twins collapse). */
ProbeVolumesGi probeVolumesToGi( const ProbeVolumes & v, const ProbeRooms & gi, float splitShare = 0.2f,
	const QString & red = QString() );

/*! The Space stage as the GI's room source (coordinator ruling 10-19): relabels the GI room grid `r` (built by
 *  ROOMCLAMP1 over the GI's box and cell, which stay) with the Space rooms' 1-based indexes. A cell takes the
 *  smallest room whose shapes hold its centre; an air cell in no shape takes the Space room that holds most of its
 *  ROOMCLAMP1 room (the fit's coverage gaps); a ROOMCLAMP1 room the Space stage did not keep stays a room of its
 *  own (ids after Space's, with ROOMCLAMP1's openings); solid stays -1. Between Space rooms the second name (b)
 *  comes only from the Space thresholds. A wide-doorway room ROOMCLAMP1 called outdoors is interior here. Returns
 *  the census line ("space rooms into GI: ..."). Reds: "spaceshapes" no cell is taken by a shape (wide-door rooms
 *  stay outdoors); "spaceleftover" the rooms Space did not keep become outdoors. */
QString probeRoomsFromSpace( const ProbeVolumes & v, ProbeRooms * r, const QString & red = QString() );

/*! Fill the GI's volume set (lane PERF1's GiVolumeSet: shapes{type, center, size, rotation, room},
 *  thresholds{shape, rooms[2], blendWidth}, source, census). A template so this branch builds before that merge. */
template <class Set>
void probeVolumesGiFill( const ProbeVolumesGi & g, Set & out )
{
	using Shape = typename decltype( out.shapes )::value_type;
	using Thr = typename decltype( out.thresholds )::value_type;
	auto conv = []( const ProbeVolumesGiShape & s ) {
		Shape d;
		d.type = s.type;
		for ( int c = 0; c < 3; c++ ) {
			d.center[c] = s.center[c];
			d.size[c] = s.size[c];
			d.rotation[c] = s.rotation[c];
		}
		d.room = s.room;
		return d;
	};
	out.shapes.clear();
	out.thresholds.clear();
	for ( const ProbeVolumesGiShape & s : g.shapes )
		out.shapes.push_back( conv( s ) );
	for ( const ProbeVolumesGiThreshold & t : g.thresholds ) {
		Thr d;
		d.shape = conv( t.shape );
		d.rooms[0] = t.rooms[0];
		d.rooms[1] = t.rooms[1];
		out.thresholds.push_back( d );
	}
	out.source = QStringLiteral( "VOLFIT1 Space stage" );
	out.census = g.census;
}

#endif // PROBEVOLUMESGI_H
