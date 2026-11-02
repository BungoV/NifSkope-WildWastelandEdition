// Lane ESPWRITE1 gate 2, thin since ESPWRITE2: the body is src/io/espwriteroundtrip.cpp (also `-no-gui espwrite roundtrip`).
#include <QCoreApplication>
#include <QStringList>

int espwriteRoundtripMain( const QStringList & a );

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	return espwriteRoundtripMain( app.arguments() );
}
