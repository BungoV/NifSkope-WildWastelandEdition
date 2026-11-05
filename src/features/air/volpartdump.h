#ifndef WW_VOLPARTDUMP_H
#define WW_VOLPARTDUMP_H

#include "gl/glcontext.hpp"

#include <QString>

class Scene;

//! lane VOLPART1, gates only (WW_CELL_VOLPART_DUMP): a volumetric system drawn again into a one-row probe target,
//! one block appended to <path> (features/air/volpartdump.cpp)
void wwVolPartDumpSystem( Scene * scene, NifSkopeOpenGLContext::Program * prog, const QString & path,
	const QString & name, int block, int count );

#endif
