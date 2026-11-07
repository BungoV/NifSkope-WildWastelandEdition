#pragma once
/* lane PERFFIX6 (F1): the wwEnv* accessors (wwswitch.h) cache per name and generation; an environment write outside
 * wwSwitchSet goes through these (qputenv / qunsetenv + a generation move), or the cached readers would keep the old
 * value. Kept out of wwswitch.h so its 160+ includers do not rebuild for three declarations. */
#include <QByteArray>

bool wwEnvPut( const char * name, const QByteArray & value );
bool wwEnvUnset( const char * name );
//! moves the accessors' generation (after a write that could not go through wwEnvPut)
void wwEnvWritten();
