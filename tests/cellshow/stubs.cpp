// lane OPENCELLFIX2: the dropdown's boxed sheet lives in src/nifskope_ui.cpp; a stand-in string the look check compares.
#include <QString>

QString wwBoxedButtonQss( const QString & padding )
{
	return QStringLiteral( "QToolButton { padding: %1; }" ).arg( padding );
}
