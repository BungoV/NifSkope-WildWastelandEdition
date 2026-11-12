#pragma once
// Lane VKCOMPUTE1 (2026-11-12): shaders that were C++ string literals live in shaders/ (or a module folder,
// shaders/features/<m>/) beside the exe, so modders can read them. Read byte for byte: the program is the same.
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QString>

QString wwShaderPath( const QString & filepath );	// gl/glcontext.cpp: the module-folder lookup

//! The file's bytes; empty and *why set when it is missing.
inline QByteArray wwShaderText( const char * name, QString * why = nullptr )
{
	QFile	f( wwShaderPath( QCoreApplication::applicationDirPath() + QStringLiteral( "/shaders/" ) + QLatin1String( name ) ) );
	if ( f.open( QIODevice::ReadOnly ) )
		return f.readAll();
	if ( why )
		*why = QStringLiteral( "no shaders/%1 (or a module folder) beside the exe" ).arg( QLatin1String( name ) );
	return QByteArray();
}
