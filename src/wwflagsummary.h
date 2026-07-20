#ifndef WWFLAGSUMMARY_H
#define WWFLAGSUMMARY_H

#include <QString>

class NifModel;
class QModelIndex;

//! Decoded one-line summary of a flag field's set bits/modes, for the grey
//! inline suffix in Block Details (WwFlagSummaryRole). Returns an empty
//! string when the field is not a recognized flag field or nothing is worth
//! saying. Implemented in spells/flags.cpp next to the flag dialogs so the
//! bit interpretations stay in one place.
QString wwFlagFieldSummary( const NifModel * nif, const QModelIndex & index );

#endif
