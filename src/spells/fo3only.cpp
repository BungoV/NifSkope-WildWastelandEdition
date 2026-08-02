#include "spellbook.h"


// Brief description is deliberately not autolinked to class Spell
/*! \file fo3only.cpp
 * \brief Fallout 3 specific spells (spFO3FixShapeDataName)
 *
 * All classes here inherit from the Spell class.
 */

//! Zero the Group ID of every NiGeometryData block (Fallout 3 only)
class spFO3FixShapeDataName final : public Spell
{
public:
	// was "Fix Geometry Data Names", which is what the doc comment claimed it did.
	// It sets Group ID to 0 and touches no name at all.
	QString name() const override final { return Spell::tr( "Zero Geometry Group ID" ); }
	QString hint() const override { return Spell::tr( "Zeroes the Group ID on every NiGeometryData block. Fallout 3 / New Vegas only." ); }
	QString page() const override final { return Spell::tr( "Sanitize" ); }
	bool sanity() const override final { return true; }

	//////////////////////////////////////////////////////////////////////////
	// Valid if nothing or NiGeometryData-based node is selected
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		//if ( !index.isValid() )
		//	return false;

		if ( !nif->checkVersion( 0x14020007, 0x14020007 ) || (nif->getUserVersion() != 11) )
			return false;

		return !index.isValid() || nif->getBlockIndex( index, "NiGeometryData" ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		if ( index.isValid() && nif->getBlockIndex( index, "NiGeometryData" ).isValid() ) {
			nif->set<int>( index, "Group ID", 0 );
		} else {
			// set all blocks
			for ( int n = 0; n < nif->getBlockCount(); n++ ) {
				QModelIndex iBlock = nif->getBlockIndex( n );

				if ( nif->getBlockIndex( iBlock, "NiGeometryData" ).isValid() ) {
					cast( nif, iBlock );
				}
			}
		}

		return index;
	}
};

REGISTER_SPELL( spFO3FixShapeDataName )

