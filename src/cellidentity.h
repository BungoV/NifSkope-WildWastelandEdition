/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLIDENTITY_H
#define CELLIDENTITY_H

#include <QHash>
#include <QString>
#include <QStringList>

/* Reads a `.lodi` identity group table (v7+) and joins it to placed REFRs for the cell view's `identity`
 * overlay: refFormId -> { file-wide group id, group size, is it a tree }. A SCOL's parts fold into the
 * first row for the form; a reference the file does not name gets no group and draws grey. */

//! What the `.lodi` says about one placed reference.
struct CellIdentity
{
	quint32 group = 0;      //!< the file-wide group id
	int size = 0;           //!< placements sharing that group over the whole file
	bool tree = false;      //!< the instance carries a tree seed (LodiInstance::seed != 0)
	int parts = 1;          //!< `.lodi` instances that shared this refFormId
};

//! Census of one loaded `.lodi`, for the notes line.
struct CellIdentityCensus
{
	QString path;
	quint32 version = 0;
	quint32 instances = 0;      //!< rows in the file
	quint32 refs = 0;           //!< distinct refFormIds indexed
	quint32 groups = 0;         //!< distinct file-wide group ids
	quint32 singletons = 0;     //!< groups of exactly one placement
	quint32 largest = 0;        //!< members of the biggest group
	quint32 trees = 0;          //!< instances with a non-zero seed
	quint32 treeSingletons = 0; //!< of those, the ones alone in their group
	bool hasGroups = false;     //!< the file is version 7 or later AND carries the table
};

/*! Read `lodiPath` and build the index. False, with `*error` naming the field or file, on anything that
 *  does not read (never a silent empty index). A file below version 7 opens with `hasGroups = false`. */
bool cellIdentityLoad( const QString & lodiPath, QHash<quint32, CellIdentity> & index,
	CellIdentityCensus * census, QString * error );

/*! The legend line for the notes: groups, singletons, the biggest, and the tree statement, built from
 *  the same census as the picture. */
QString cellIdentityLegend( const CellIdentityCensus & c );

#endif // CELLIDENTITY_H
