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

/*! lane CELLUNI6: as wwGlContextSerial(), and new again when the same QOpenGLContext object is made again: create() on a
 *  made context destroys the old one first, and its aboutToBeDestroyed drops this property (wwGlContextSerial()'s
 *  survives that, so a cache keyed on it would answer for the old context). */
inline std::uint64_t wwGlContextLife()
{
	QOpenGLContext * c = QOpenGLContext::currentContext();
	if ( !c )
		return 0;
	QVariant v = c->property( "wwCtxLife" );
	if ( !v.isValid() ) {
		static std::uint64_t next = 0;
		v = QVariant::fromValue<qulonglong>( ++next );
		c->setProperty( "wwCtxLife", v );
		if ( !c->property( "wwCtxLifeHook" ).isValid() ) {	// once per object: the hook outlives a re-create
			c->setProperty( "wwCtxLifeHook", true );
			QObject::connect( c, &QOpenGLContext::aboutToBeDestroyed, c, [c]() { c->setProperty( "wwCtxLife", QVariant() ); } );
		}
	}
	return std::uint64_t( v.toULongLong() );
}

#endif
