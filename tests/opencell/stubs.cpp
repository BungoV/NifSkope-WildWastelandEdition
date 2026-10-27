// lane CELLSPAWN1: the dialog styles its selectors with src/ui/widgets/wwnumberfield.cpp, which pulls in the skin.
// The test needs no look, so the call does nothing here.
// lane OPENCELLFIX1: the panel headings come from the skin too; a plain label stands in.
#include <QLabel>
#include <QString>

void wwMatchFieldStyle( QWidget * ) {}
QLabel * wwHeading( const QString & text, QWidget * parent ) { return new QLabel( text, parent ); }
// lane OPENCELLFIX2: the dropdown's boxed sheet (src/nifskope_ui.cpp); a stand-in string the look check compares.
QString wwBoxedButtonQss( const QString & padding ) { return QStringLiteral( "QToolButton { padding: %1; }" ).arg( padding ); }
