// Lane ESPWRITE1 gate tool, thin since ESPWRITE2: the body is src/io/espwritegates.cpp (also `-no-gui espwrite`).
#include <QCoreApplication>
#include <QStringList>

int espwriteGatesMain( const QStringList & a );

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	return espwriteGatesMain( app.arguments() );
}
