#ifndef ENGINE_CLIENT_BACKEND_OPENGLES_BACKEND_OPENGLES3_H
#define ENGINE_CLIENT_BACKEND_OPENGLES_BACKEND_OPENGLES3_H

#include <base/detect.h>

#if defined(CONF_BACKEND_OPENGL_ES) || defined(CONF_BACKEND_OPENGL_ES3)
#include <engine/client/backend_sdl.h>

#define GLES_CLASS_DEFINES_DO_DEFINE
#include "gles_class_defines.h"
#undef GLES_CLASS_DEFINES_DO_DEFINE

#define BACKEND_AS_OPENGL_ES 1

#include <engine/client/backend/opengl/backend_opengl3.h>

#undef BACKEND_AS_OPENGL_ES

#include "gles_class_defines.h"

#endif

#endif
