#ifndef WWSHAREDREAD_H
#define WWSHAREDREAD_H

#include <QByteArray>
#include <QString>

/*! lane FRAMESPEED1 (CONCORDQUIT1): a small text file read whole. A file whose name starts with a Windows device name
 *  (con.fly.path: QFile opens the console, and Concord's walk quit "cannot read") and any file QFile refuses are read
 *  through the \\?\ form with every share flag. *why says what happened (empty: QFile read it). ok=false only when
 *  both refuse. */
QByteArray wwReadFileShared( const QString & path, bool * ok, QString * why );

/*! The device name the file name starts with (con, prn, aux, nul, com1-9, lpt1-9; any case, before the first dot), or empty. */
QString wwDeviceNameStem( const QString & path );

#endif
