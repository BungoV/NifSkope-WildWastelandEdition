#ifndef SP_TRANSFORM_H
#define SP_TRANSFORM_H

#include "spellbook.h"

class spApplyTransformation final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Apply" ); }
	QString page() const override final { return Spell::tr( "Transform" ); }

	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final;

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final;
	static void cast_Starfield( NifModel * nif, const QModelIndex & index );
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
};

#endif
