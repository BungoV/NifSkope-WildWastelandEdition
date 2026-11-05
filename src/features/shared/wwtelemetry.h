#ifndef WWTELEMETRY_H
#define WWTELEMETRY_H

/* lane MODULES1: the shared telemetry module. One JSON-lines file per run, every feature reports through it.
 * WW_TELEMETRY=<file> turns it on at launch (unset: off, nothing is built or written). WW_TELEMETRY_RATE=N writes the
 * per-frame cost row every N frames (default 1). Records: {"t","frame","row","gen","module","event", fields...}.
 * Events: start (the launch switches), switch (a live set: name, value, stage, the generation it made), row (the live
 * driver's row step), cost (frame + per-zone + per-module GPU/CPU ms). A cost row whose "gen" equals a switch record's
 * "gen" is drawn with that set in effect. Reader: tools/telemetry.py. It never changes a picture. */

#include <QByteArray>

namespace WwTelemetry
{
//! WW_TELEMETRY set at launch (read once)
bool on();
//! One record; `fields` is the body of a JSON object without braces (may be empty)
void record( const char * module, const char * event, const QByteArray & fields );
//! The frame the next records belong to (the pass-time frame counter)
void setFrame( qint64 frame );
//! The live driver's current row (written into every later record)
void setRow( const QByteArray & row );
//! A cost row is due this frame (WW_TELEMETRY_RATE)
bool costDue( qint64 frame );
//! The feature module that owns a pass-time zone or sub-stage ("" = an inclusive CPU timer, kept out of module sums)
const char * zoneModule( const char * zone );
//! A JSON string literal (quoted, escaped)
QByteArray str( const QByteArray & s );
//! A JSON number with 3 decimals
QByteArray num( double v );
} // namespace WwTelemetry

#endif // WWTELEMETRY_H
