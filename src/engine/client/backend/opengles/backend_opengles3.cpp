#include <base/detect.h>

#if defined(CONF_BACKEND_OPENGL_ES) || defined(CONF_BACKEND_OPENGL_ES3)

#define GLES_CLASS_DEFINES_DO_DEFINE
#include "gles_class_defines.h"
#undef GLES_CLASS_DEFINES_DO_DEFINE

#define BACKEND_AS_OPENGL_ES 1

#include <engine/client/backend/opengl/backend_opengl3.cpp>

#undef BACKEND_AS_OPENGL_ES

#include "gles_class_defines.h"

#endif
