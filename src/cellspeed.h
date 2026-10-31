#ifndef CELLSPEED_H
#define CELLSPEED_H

/* Where opening a cell spends its time and memory: WW_CELL_SPEED_DUMP=<file> appends one table per cell
 * (stage wall/processor ms, working set, repeated sub-stages, counts). Unset, every call is one bool test. */

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
/*! WW_CELL_SPEED_REOPEN=<spec>[;<spec>...]: after a cell has drawn for five seconds, open the next spec in the
 *  same window, then quit. 1 = open `fname` again now (WW_CELL_OPEN set to the next spec), 0 = quit, -1 = not asked. */
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
