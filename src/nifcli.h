/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

See the LICENSE.md file for the full license text.

***** END LICENCE BLOCK *****/

#ifndef NIFCLI_H
#define NIFCLI_H

#include <QStringList>

//! Headless batch entry point (`NifSkope -no-gui <command> ...`).
/*!
 * Runs entirely on QCoreApplication: no widgets, no GL context, no scene.
 * That bounds what it can do — spells and direct model edits are available,
 * viewport modelling operations (which live on GLView and need picked-element
 * state) are not.
 *
 * \param args The full argument list, args[0] being the program name.
 * \return Process exit code; 0 on success.
 */
int nifskopeCliMain( const QStringList & args );

#endif // NIFCLI_H
