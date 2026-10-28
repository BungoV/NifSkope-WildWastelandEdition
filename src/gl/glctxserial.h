#ifndef GLCTXSERIAL_H
#define GLCTXSERIAL_H

#include <QOpenGLContext>
#include <QVariant>

#include <cstdint>

/*! lane FRAMESPEED1: the current GL context's serial, given on first sight and kept as a property of the context. A
 *  context made again at the same address has no property yet, so it gets a new serial (0: no current context). */
inline std::uint64_t wwGlContextSerial()
{
	QOpenGLContext * c = QOpenGLContext::currentContext();
	if ( !c )
		return 0;
	QVariant v = c->property( "wwCtxSerial" );
	if ( !v.isValid() ) {
		static std::uint64_t next = 0;
		v = QVariant::fromValue<qulonglong>( ++next );
		c->setProperty( "wwCtxSerial", v );
	}
	return std::uint64_t( v.toULongLong() );
}

#endif
