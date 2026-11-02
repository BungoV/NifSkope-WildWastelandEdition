#pragma once
// Lane TOOLS1101 (REFLPROBECLI1): `nifskope -no-gui reflcensus`, the reflection probes' capture census without a
// window (src/reflcensus.h; the arguments are in reflcensuscli.cpp's usage text).
#include <QStringList>

int reflCensusCli( const QStringList & args );
