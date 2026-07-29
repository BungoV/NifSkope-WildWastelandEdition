#ifndef WWBLOCKSUMMARY_H
#define WWBLOCKSUMMARY_H

#include <QString>

class NifModel;

//! One-line, per-type summary of a block for the Block List's Summary column —
//! what the block IS, not what it is called: counts for geometry, the target for
//! a controller, the diffuse name for a texture set, bone counts for a skin.
//! Empty for types with nothing worth saying.
//!
//! `status` is set to a short defect marker when the block has one and cleared
//! otherwise; the column draws it in red after the summary. Only conditions that
//! can be decided exactly are reported — a missing texture, geometry with no
//! triangles, a block nothing links to — because a badge that is sometimes wrong
//! is worse than no badge.
//!
//! Implemented in spells/blocks.cpp, next to the block operations, so the
//! per-type knowledge stays in one place (mirrors wwFlagFieldSummary).
QString wwBlockSummary( const NifModel * nif, int block, QString & status );

#endif
