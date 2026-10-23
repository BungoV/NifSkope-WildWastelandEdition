#ifndef CELLWEAPON_H
#define CELLWEAPON_H

/* lane MISS1 (GAPLOOK2, 2026-10-23): a placed weapon drawn with its default mod parts.
 *
 * A WEAP's MODL is only its base mesh; the parts it shows come from its default object template (OBTE/OBTS with
 * Default set, else the first): each included OMOD's MODL, and the OMODs those include. A part declares the point
 * it plugs into ("C-<Slot>" in BSConnectPoint::Children); a mesh already in the assembly publishes it ("P-<Slot>"
 * in BSConnectPoint::Parents), the same rule as src/nifmerge.h nifMergeWeaponPart. Built once per base on the CPU.
 * Row "Weapon mod parts" (WW_CELL_WEAP_PARTS), read when the cell is built; red WW_CELL_WEAP_PARTS_RED=root puts
 * every part at the base's root (the connect points ignored). */

#include <QHash>
#include <QString>
#include <QVector>

#include <vector>

class EsmWorld;
struct NativeSrcShape;

class CellWeapons
{
public:
	CellWeapons( const EsmWorld & world, const QString & dataRoot );
	~CellWeapons();

	//! True when the row is on and `base` is a WEAP whose default template names a part with a mesh.
	bool wants( quint32 base );
	//! The assembled weapon, the reference's space (the base mesh's root transform left out, as a placed load).
	bool shapes( quint32 base, std::vector<NativeSrcShape> * out );
	//! `  weapon parts: W weapons, A assembled, parts P placed (R at the root: no point), U unresolved, M no mesh`
	QString censusLine() const;

private:
	struct Impl;
	Impl * d;
};

#endif // CELLWEAPON_H
