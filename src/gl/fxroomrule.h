#ifndef FXROOMRULE_H
#define FXROOMRULE_H

/* Lane HAZECARD1: which exterior lit effect takes GIDARK1's room share (fxRoomLit) on its base.
 * The game's lit effect base is the weather's Effect Lighting x fade x Sunlight Scale everywhere outdoors; nothing
 * scales it by the spot. GIDARK1 scales it for the glass in an open shop. A soft card (soft depth > 0: the haze cards,
 * GlowFill / Mist / Steam) takes the game's base again; red WW_CELL_HAZECARD_RED=room gives it the room share as
 * before. WW_CELL_FXROOM_RED=game: every effect the game's base.
 * red: 1 = WW_CELL_FXROOM_RED=game, 2 = WW_CELL_HAZECARD_RED=room. No Qt: tests/hazecard1 builds it with g++. */
inline bool wwFxRoomLitRule( bool exterior, bool fxLit, bool soft, int red )
{
	if ( ( red & 1 ) || !exterior || !fxLit )
		return false;
	return !soft || ( red & 2 );
}

#endif
