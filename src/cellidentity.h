/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLIDENTITY_H
#define CELLIDENTITY_H

#include <QHash>
#include <QString>
#include <QStringList>

/* ---------------------------------------------------------------------------
 * THE `.lodi` IDENTITY GROUP, JOINED TO A PLACED REFR.
 *
 * bungo's ruling of 2026-09-18 -- "The houses should be one object each though,
 * for identity" -- made the native object LOD carry a GROUP table: one u16 per
 * instance, dense from 0 INSIDE EACH CHUNK, written at `.lodi` version 7
 * (src/lodifile.h, LODI_VERSION_GROUP_SKY).  The proximity join that builds
 * those groups has a ruled 64-unit radius, and the only way to judge a join is
 * to LOOK at which placements came out in the same group.
 *
 * This is the read side of that, for the cell view's `identity` overlay:
 *
 *      refFormId  ->  { group id, how many placements share it, is it a tree }
 *
 * WHY THE GROUP ID IS NOT `table.group[i]`.  The group ids are dense per CHUNK,
 * so chunk (0,0) and chunk (1,0) both have a group 0 and they are different
 * objects.  A file-wide id is therefore `(chunkIndex << 16) | group`, and
 * `LODI_MAX_CHUNKS` is 65,536 while a chunk's group count cannot exceed its
 * instance count, so the pair fits a u32 exactly as long as no chunk holds more
 * than 65,536 groups -- which the writer already refuses (LODI_BASE_MAX and the
 * per-chunk density rule).  `cellIdentityLoad` CHECKS that rather than assuming
 * it, and refuses by name if a chunk ever breaks it.
 *
 * WHY A REFR CAN HAVE SEVERAL INSTANCES.  A SCOL part is its own `.lodi`
 * instance with the collection's refFormId and its own `scolPart`, so a single
 * REFR maps to several rows.  The index keeps the FIRST row for the form and
 * counts the rest, because the cell view colours a placement and a SCOL part is
 * a placement of the collection; `parts` says how many were folded so a picture
 * that looks wrong can be checked against a number.
 *
 * WHAT IS NOT DONE HERE.  Nothing writes a `.lodi`.  No group is INVENTED for a
 * placement the bake never saw: a reference the file does not name comes back
 * `false` and the overlay draws it GREY and counts it, which is what makes the
 * picture an answer about the bake instead of about this reader.
 * --------------------------------------------------------------------------- */

//! What the `.lodi` says about one placed reference.
struct CellIdentity
{
	quint32 group = 0;      //!< `(chunkIndex << 16) | groupId`, unique over the FILE
	int size = 0;           //!< placements sharing that group over the whole file
	bool tree = false;      //!< the instance carries a tree seed (LodiInstance::seed != 0)
	int parts = 1;          //!< `.lodi` instances that shared this refFormId
};

//! Census of one loaded `.lodi`, for the notes line and for a gate.
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

/*! Read `lodiPath` and build the index.  False, with `*error` NAMING the field
 *  or the file, on anything that does not read -- never a silent empty index,
 *  because an empty index and a missing bake would then draw the same picture.
 *
 *  A file BELOW version 7 opens and reports `hasGroups = false`: it is a valid
 *  `.lodi` that simply predates the group table, and the caller must say so by
 *  name rather than colouring every placement grey in silence. */
bool cellIdentityLoad( const QString & lodiPath, QHash<quint32, CellIdentity> & index,
	CellIdentityCensus * census, QString * error );

/*! The legend line for the notes: how many groups, how many singletons, the
 *  biggest, and the tree statement -- one string, so the census and the picture
 *  cannot disagree about what was drawn. */
QString cellIdentityLegend( const CellIdentityCensus & c );

#endif // CELLIDENTITY_H
