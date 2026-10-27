// lane OPENCELLFIX2: the dropdown's boxed sheet lives in src/nifskope_ui.cpp; a stand-in string the look check compares.
#include <QString>

QString wwBoxedButtonQss( const QString & padding )
{
	return QStringLiteral( "QToolButton { padding: %1; }" ).arg( padding );
}

QString wwSegmentedToolButtonQss()
{
	return QStringLiteral( "QToolButton { padding: 3px 10px; border: 1px solid gray; border-left: 0; }"
		"QToolButton[wwSegmentFirst=\"true\"] { border-left: 1px solid gray; }"
		"QToolButton:checked { background: blue; }" );
}
