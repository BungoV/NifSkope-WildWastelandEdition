#ifndef SANITIZE_H
#define SANITIZE_H

#include "spellbook.h"

//! Reorders blocks
class spSanitizeBlockOrder final : public Spell
{
public:
    QString name() const override final { return Spell::tr( "Reorder Blocks" ); }
    QString hint() const override { return Spell::tr( "Renumbers every block in the file. Upstream keeps this out of auto-sanitize: it can break CK texture-set overrides." ); }
    QString page() const override final { return Spell::tr( "Sanitize" ); }
    // Prevent this from running during auto-sanitize for the time being
    //	Can really only cause issues with rendering and textureset overrides via the CK
    bool sanity() const override { return false; }

    bool isApplicable( const NifModel *, const QModelIndex & index ) override final;

    // check whether the block is of a type that comes before the parent or not
    bool childBeforeParent( NifModel * nif, qint32 block );

    // build the nif tree at node block; the block itself and its children are recursively added to
    // the newblocks list
    void addTree( NifModel * nif, qint32 block, QList<qint32> & newblocks );

    QModelIndex cast( NifModel * nif, const QModelIndex & ) override final;

	static QModelIndex cast_Static( NifModel * nif, const QModelIndex & index );
};

// Base class for all warning and error checkers
class spChecker : public Spell
{
public:
	QString name() const override { return {}; }
	QString page() const override { return {}; }
	bool sanity() const override { return true; }
	bool constant() const override { return true; }
	bool checker() const override { return true; }

	bool isApplicable(const NifModel *, const QModelIndex &) override
	{
		return false;
	}

	QModelIndex cast(NifModel *, const QModelIndex &) override { return {};	}

	static QString message() { return {}; }
};

class spErrorNoneRefs : public spChecker
{
public:
	QString name() const override { return Spell::tr( "None Refs" ); }
	QString hint() const override { return Spell::tr( "Reports references that are None where the format requires a block." ); }
	QString page() const override final { return Spell::tr( "Error Checking" ); }

	bool isApplicable( const NifModel *, const QModelIndex & index ) override;

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final;

	static QString message()
	{
		return tr("One or more blocks contain None Refs.");
	}

	void checkArray( NifModel * nif, const QModelIndex & idx, QVector<QString> rows );
	void checkRef( NifModel * nif, const QModelIndex & idx, const QString & name );
};


class spErrorInvalidPaths : public spChecker
{
public:
	QString name() const override { return Spell::tr( "Invalid Paths" ); }
	QString hint() const override { return Spell::tr( "Reports texture paths that are absolute, empty, or missing a file extension." ); }
	QString page() const override final { return Spell::tr( "Error Checking" ); }

	bool isApplicable( const NifModel *, const QModelIndex & index ) override;

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final;

	/*! Which faults to report. These are tested with `invalid & P_x`, so they
	 *  have to be BITS — and they were not.
	 *
	 *  Declared without values they came out 0, 1, 2, which makes
	 *  `invalid & P_EMPTY` equal to `invalid & 0`: always false, for every
	 *  caller, forever. The empty-path message below has never been reachable.
	 *  P_ABSOLUTE and P_NO_EXT happened to work because 1 and 2 are distinct
	 *  bits and P_DEFAULT is their OR.
	 */
	typedef enum
	{
		P_EMPTY    = 1 << 0,
		P_ABSOLUTE = 1 << 1,
		P_NO_EXT   = 1 << 2,
		P_DEFAULT  = (P_ABSOLUTE | P_NO_EXT)
	} InvalidPath;

	static QString message()
	{
		return tr("One or more asset path strings contain an invalid path.");
	}

	void checkPath( NifModel * nif, const QModelIndex & idx, const QString & name, InvalidPath invalid = P_DEFAULT );
};


class spWarningEnvironmentMapping : public spChecker
{
public:
	QString name() const override { return Spell::tr("Environment Mapping Flags"); }
	QString hint() const override { return Spell::tr( "Reports shader flags that disagree with the environment map actually assigned." ); }
	QString page() const override final { return Spell::tr("Error Checking"); }

	bool isApplicable(const NifModel *, const QModelIndex & index) override;

	QModelIndex cast(NifModel * nif, const QModelIndex &) override final;

	static QString message()
	{
		return tr("Potential problems detected with Shader Flags.");
	}
};

#endif // SANITIZE_H
