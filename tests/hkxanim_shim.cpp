/* The one static data/niftypes.cpp defines that the standalone hkxanim_dump
   needs (Quat's default constructor memcpy's it). niftypes.cpp itself drags
   in model/nifmodel.h and the whole application; this shim keeps the test
   binary at Qt6Core + Qt6Gui. */

#include "data/niftypes.h"

const float Quat::identity[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
