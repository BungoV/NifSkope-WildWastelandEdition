#ifndef CELLSPEED_H
#define CELLSPEED_H

/* Lane SPEED1 (2026-10-02): WHERE OPENING A CELL SPENDS ITS TIME AND MEMORY.
 *
 * WW_CELL_SPEED_DUMP=<file> appends one table per cell opened: each stage of the
 * build in order (wall ms, processor ms, working set after it), the repeated
 * stages inside them (model file read, parse, texture load, frames), and the
 * counts that say what was shared (placements, distinct models, model loads,
 * vertices welded). Unset, every call here is one bool test.
 *
 * A measuring instrument, not a feature: no menu row, no INI key. */

#include <QString>
#include <QtGlobal>

namespace CellSpeed
{

bool on();
//! A cell open starts: the table is cleared and the clock set.
void begin();
//! Wall time since the previous mark (or begin) is this stage's.
void mark( const char * stage );
//! A repeated stage inside the marked ones ("of which"): nanoseconds and one call.
void add( const char * stage, qint64 ns );
void count( const char * what, qint64 n );
//! Appends the table; also runs by itself when the process ends with a table open.
void end();
qint64 nowNs();
/*! WW_CELL_SPEED_REOPEN=<spec>[;<spec>...] (with the dump on): once a cell has opened and drawn for five
 *  seconds, the next spec is opened in the same window, and after the last the window quits -- one table
 *  per cell, so the tables say what a closed cell leaves behind. 1 = open `fname` again now (WW_CELL_OPEN
 *  has been set to the next spec), 0 = quit, -1 = not asked. */
int reopenStep( const QString & fname );

//! Scope timer for add(). Costs one bool test when the dump is off.
class Acc
{
public:
	explicit Acc( const char * stage ) : stage_( on() ? stage : nullptr ), t0_( stage_ ? nowNs() : 0 ) {}
	~Acc() { if ( stage_ ) add( stage_, nowNs() - t0_ ); }
private:
	const char * stage_;
	qint64 t0_;
	Q_DISABLE_COPY( Acc )
};

} // namespace CellSpeed

#endif
