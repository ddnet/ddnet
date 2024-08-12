#include "backend_opengl.h"

#include <engine/graphics.h>

#include <engine/client/backend_sdl.h>

#include <base/detect.h>
#include <base/system.h>

#if defined(BACKEND_AS_OPENGL_ES) || !defined(CONF_BACKEND_OPENGL_ES)

#include <engine/client/backend/opengl/opengl_sl.h>
#include <engine/client/backend/opengl/opengl_sl_program.h>
#include <engine/client/blocklist_driver.h>

#include <engine/client/backend/glsl_shader_compiler.h>

#include <engine/gfx/image_manipulation.h>

#ifndef BACKEND_AS_OPENGL_ES
#include <GL/glew.h>
#else
#include <GLES3/gl3.h>
#define GL_TEXTURE_2D_ARRAY_EXT GL_TEXTURE_2D_ARRAY
// GLES doesn't support GL_QUADS, but the code is also never executed
#define GL_QUADS GL_TRIANGLES
#ifndef CONF_BACKEND_OPENGL_ES3
#include <GLES/gl.h>
#define glOrtho glOrthof
#else
#define BACKEND_GL_MODERN_API 1
#endif
#endif

// ------------ CCommandProcessorFragment_OpenGL
void CCommandProcessorFragment_OpenGL::Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand)
{
	if(pCommand->m_ByResize)
	{
		m_CanvasWidth = (uint32_t)pCommand->m_Width;
		m_CanvasHeight = (uint32_t)pCommand->m_Height;
	}
	glViewport(pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height);
}

size_t CCommandProcessorFragment_OpenGL::GLFormatToPixelSize(int GLFormat)
{
	switch(GLFormat)
	{
	case GL_RGBA: return 4;
	case GL_RGB: return 3;
	case GL_RED: return 1;
	case GL_ALPHA: return 1;
	default: return 4;
	}
}

bool CCommandProcessorFragment_OpenGL::IsTexturedState(const CCommandBuffer::SState &State)
{
	return State.m_Texture >= 0 && State.m_Texture < (int)m_vTextures.size();
}

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::SState &State, bool Use2DArrayTextures)
{
#ifndef BACKEND_GL_MODERN_API
	// blend
	switch(State.m_BlendMode)
	{
	case CCommandBuffer::BLEND_NONE:
		glDisable(GL_BLEND);
		break;
	case CCommandBuffer::BLEND_ALPHA:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case CCommandBuffer::BLEND_ADDITIVE:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	default:
		dbg_msg("render", "unknown blendmode %d\n", State.m_BlendMode);
	};
	m_LastBlendMode = State.m_BlendMode;

	// clip
	if(State.m_ClipEnable)
	{
		glScissor(State.m_ClipX, State.m_ClipY, State.m_ClipW, State.m_ClipH);
		glEnable(GL_SCISSOR_TEST);
		m_LastClipEnable = true;
	}
	else if(m_LastClipEnable)
	{
		// Don't disable it always
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}

	glDisable(GL_TEXTURE_2D);
	if(!m_HasShaders)
	{
		if(m_Has3DTextures)
			glDisable(GL_TEXTURE_3D);
		if(m_Has2DArrayTextures)
		{
			glDisable(m_2DArrayTarget);
		}
	}

	if(m_HasShaders && IsNewApi())
	{
		glBindSampler(0, 0);
	}

	// texture
	if(IsTexturedState(State))
	{
		if(!Use2DArrayTextures)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_vTextures[State.m_Texture].m_Tex);

			if(m_vTextures[State.m_Texture].m_LastWrapMode != State.m_WrapMode)
			{
				switch(State.m_WrapMode)
				{
				case CCommandBuffer::WRAP_REPEAT:
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
					break;
				case CCommandBuffer::WRAP_CLAMP:
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					break;
				default:
					dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapMode);
				};
				m_vTextures[State.m_Texture].m_LastWrapMode = State.m_WrapMode;
			}
		}
		else
		{
			if(m_Has2DArrayTextures)
			{
				if(!m_HasShaders)
					glEnable(m_2DArrayTarget);
				glBindTexture(m_2DArrayTarget, m_vTextures[State.m_Texture].m_Tex2DArray);
			}
			else if(m_Has3DTextures)
			{
				if(!m_HasShaders)
					glEnable(GL_TEXTURE_3D);
				glBindTexture(GL_TEXTURE_3D, m_vTextures[State.m_Texture].m_Tex2DArray);
			}
			else
			{
				dbg_msg("opengl", "ERROR: this call should not happen.");
			}
		}
	}

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, -10.0f, 10.f);
#endif
}

static void ParseVersionString(EBackendType BackendType, const char *pStr, int &VersionMajor, int &VersionMinor, int &VersionPatch)
{
	if(pStr)
	{
		// if backend is GLES, it starts with "OpenGL ES " or OpenGL ES-CM for older contexts, rest is the same
		if(BackendType == BACKEND_TYPE_OPENGL_ES)
		{
			int StrLenGLES = str_length("OpenGL ES ");
			int StrLenGLESCM = str_length("OpenGL ES-CM ");
			if(str_comp_num(pStr, "OpenGL ES ", StrLenGLES) == 0)
				pStr += StrLenGLES;
			else if(str_comp_num(pStr, "OpenGL ES-CM ", StrLenGLESCM) == 0)
				pStr += StrLenGLESCM;
		}

		char aCurNumberStr[32];
		size_t CurNumberStrLen = 0;
		size_t TotalNumbersPassed = 0;
		int aNumbers[3] = {0};
		bool LastWasNumber = false;
		while(*pStr && TotalNumbersPassed < 3)
		{
			if(str_isnum(*pStr))
			{
				aCurNumberStr[CurNumberStrLen++] = (char)*pStr;
				LastWasNumber = true;
			}
			else if(LastWasNumber && (*pStr == '.' || *pStr == ' '))
			{
				if(CurNumberStrLen > 0)
				{
					aCurNumberStr[CurNumberStrLen] = 0;
					aNumbers[TotalNumbersPassed++] = str_toint(aCurNumberStr);
					CurNumberStrLen = 0;
				}

				LastWasNumber = false;

				if(*pStr != '.')
					break;
			}
			else
			{
				break;
			}

			++pStr;
		}

		VersionMajor = aNumbers[0];
		VersionMinor = aNumbers[1];
		VersionPatch = aNumbers[2];
	}
}

#ifndef BACKEND_AS_OPENGL_ES
static const char *GetGLErrorName(GLenum Type)
{
	if(Type == GL_DEBUG_TYPE_ERROR)
		return "ERROR";
	else if(Type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR)
		return "DEPRECATED BEHAVIOR";
	else if(Type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
		return "UNDEFINED BEHAVIOR";
	else if(Type == GL_DEBUG_TYPE_PORTABILITY)
		return "PORTABILITY";
	else if(Type == GL_DEBUG_TYPE_PERFORMANCE)
		return "PERFORMANCE";
	else if(Type == GL_DEBUG_TYPE_OTHER)
		return "OTHER";
	else if(Type == GL_DEBUG_TYPE_MARKER)
		return "MARKER";
	else if(Type == GL_DEBUG_TYPE_PUSH_GROUP)
		return "PUSH_GROUP";
	else if(Type == GL_DEBUG_TYPE_POP_GROUP)
		return "POP_GROUP";
	return "UNKNOWN";
};

static const char *GetGLSeverity(GLenum Type)
{
	if(Type == GL_DEBUG_SEVERITY_HIGH)
		return "high"; // All OpenGL Errors, shader compilation/linking errors, or highly-dangerous undefined behavior
	else if(Type == GL_DEBUG_SEVERITY_MEDIUM)
		return "medium"; // Major performance warnings, shader compilation/linking warnings, or the use of deprecated functionality
	else if(Type == GL_DEBUG_SEVERITY_LOW)
		return "low"; // Redundant state change performance warning, or unimportant undefined behavior
	else if(Type == GL_DEBUG_SEVERITY_NOTIFICATION)
		return "notification"; // Anything that isn't an error or performance issue.

	return "unknown";
}

static void GLAPIENTRY
GfxOpenGLMessageCallback(GLenum Source,
	GLenum Type,
	GLuint Id,
	GLenum Severity,
	GLsizei Length,
	const GLchar *pMsg,
	const void *pUserParam)
{
	dbg_msg("gfx", "[%s] (importance: %s) %s", GetGLErrorName(Type), GetGLSeverity(Severity), pMsg);
}
#endif

bool CCommandProcessorFragment_OpenGL::GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData)
{
	if(m_CanvasWidth == 0 || m_CanvasHeight == 0)
	{
		return false;
	}
	else
	{
		Width = m_CanvasWidth;
		Height = m_CanvasHeight;
		Format = CImageInfo::FORMAT_RGBA;
		vDstData.resize((size_t)Width * (Height + 1) * 4); // +1 for flipping image
		glReadBuffer(GL_FRONT);
		GLint Alignment;
		glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, m_CanvasWidth, m_CanvasHeight, GL_RGBA, GL_UNSIGNED_BYTE, vDstData.data());
		glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

		uint8_t *pTempRow = vDstData.data() + Width * Height * 4;
		for(uint32_t Y = 0; Y < Height / 2; ++Y)
		{
			mem_copy(pTempRow, vDstData.data() + Y * Width * 4, Width * 4);
			mem_copy(vDstData.data() + Y * Width * 4, vDstData.data() + ((Height - Y) - 1) * Width * 4, Width * 4);
			mem_copy(vDstData.data() + ((Height - Y) - 1) * Width * 4, pTempRow, Width * 4);
		}

		return true;
	}
}

bool CCommandProcessorFragment_OpenGL::InitOpenGL(const SCommand_Init *pCommand)
{
	m_IsOpenGLES = pCommand->m_RequestedBackend == BACKEND_TYPE_OPENGL_ES;

	*pCommand->m_pReadPresentedImageDataFunc = [this](uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) {
		return GetPresentedImageData(Width, Height, Format, vDstData);
	};

	const char *pVendorString = (const char *)glGetString(GL_VENDOR);
	dbg_msg("opengl", "Vendor string: %s", pVendorString);

	// check what this context can do
	const char *pVersionString = (const char *)glGetString(GL_VERSION);
	dbg_msg("opengl", "Version string: %s", pVersionString);

	const char *pRendererString = (const char *)glGetString(GL_RENDERER);

	str_copy(pCommand->m_pVendorString, pVendorString, gs_GpuInfoStringSize);
	str_copy(pCommand->m_pVersionString, pVersionString, gs_GpuInfoStringSize);
	str_copy(pCommand->m_pRendererString, pRendererString, gs_GpuInfoStringSize);

	// parse version string
	ParseVersionString(pCommand->m_RequestedBackend, pVersionString, pCommand->m_pCapabilities->m_ContextMajor, pCommand->m_pCapabilities->m_ContextMinor, pCommand->m_pCapabilities->m_ContextPatch);

	*pCommand->m_pInitError = 0;

	int BlocklistMajor = -1, BlocklistMinor = -1, BlocklistPatch = -1;
	bool RequiresWarning = false;
	const char *pErrString = ParseBlocklistDriverVersions(pVendorString, pVersionString, BlocklistMajor, BlocklistMinor, BlocklistPatch, RequiresWarning);
	// if the driver is buggy, and the requested GL version is the default, fallback
	if(pErrString != NULL && pCommand->m_RequestedMajor == 3 && pCommand->m_RequestedMinor == 0 && pCommand->m_RequestedPatch == 0)
	{
		// if not already in the error state, set the GL version
		if(g_Config.m_GfxDriverIsBlocked == 0)
		{
			// fallback to known good GL version
			pCommand->m_pCapabilities->m_ContextMajor = BlocklistMajor;
			pCommand->m_pCapabilities->m_ContextMinor = BlocklistMinor;
			pCommand->m_pCapabilities->m_ContextPatch = BlocklistPatch;

			// set backend error string
			if(RequiresWarning)
				*pCommand->m_pErrStringPtr = pErrString;
			*pCommand->m_pInitError = -2;

			g_Config.m_GfxDriverIsBlocked = 1;
		}
	}
	// if the driver was in a blocked error state, but is not anymore, reset all config variables
	else if(pErrString == NULL && g_Config.m_GfxDriverIsBlocked == 1)
	{
		pCommand->m_pCapabilities->m_ContextMajor = 3;
		pCommand->m_pCapabilities->m_ContextMinor = 0;
		pCommand->m_pCapabilities->m_ContextPatch = 0;

		// tell the caller to reinitialize the context
		*pCommand->m_pInitError = -2;

		g_Config.m_GfxDriverIsBlocked = 0;
	}

	int MajorV = pCommand->m_pCapabilities->m_ContextMajor;

	if(pCommand->m_RequestedBackend == BACKEND_TYPE_OPENGL)
	{
#ifndef BACKEND_AS_OPENGL_ES
		int MinorV = pCommand->m_pCapabilities->m_ContextMinor;
		if(*pCommand->m_pInitError == 0)
		{
			if(MajorV < pCommand->m_RequestedMajor)
			{
				*pCommand->m_pInitError = -2;
			}
			else if(MajorV == pCommand->m_RequestedMajor)
			{
				if(MinorV < pCommand->m_RequestedMinor)
				{
					*pCommand->m_pInitError = -2;
				}
				else if(MinorV == pCommand->m_RequestedMinor)
				{
					int PatchV = pCommand->m_pCapabilities->m_ContextPatch;
					if(PatchV < pCommand->m_RequestedPatch)
					{
						*pCommand->m_pInitError = -2;
					}
				}
			}
		}

		if(*pCommand->m_pInitError == 0)
		{
			MajorV = pCommand->m_RequestedMajor;
			MinorV = pCommand->m_RequestedMinor;

			pCommand->m_pCapabilities->m_2DArrayTexturesAsExtension = false;
			pCommand->m_pCapabilities->m_NPOTTextures = true;
			pCommand->m_pCapabilities->m_TrianglesAsQuads = false;

			if(MajorV >= 4 || (MajorV == 3 && MinorV == 3))
			{
				pCommand->m_pCapabilities->m_TileBuffering = true;
				pCommand->m_pCapabilities->m_QuadBuffering = true;
				pCommand->m_pCapabilities->m_TextBuffering = true;
				pCommand->m_pCapabilities->m_QuadContainerBuffering = true;
				pCommand->m_pCapabilities->m_ShaderSupport = true;

				pCommand->m_pCapabilities->m_MipMapping = true;
				pCommand->m_pCapabilities->m_3DTextures = true;
				pCommand->m_pCapabilities->m_2DArrayTextures = true;

				pCommand->m_pCapabilities->m_TrianglesAsQuads = true;
			}
			else if(MajorV == 3)
			{
				pCommand->m_pCapabilities->m_MipMapping = true;
				// check for context native 2D array texture size
				pCommand->m_pCapabilities->m_3DTextures = false;
				pCommand->m_pCapabilities->m_2DArrayTextures = false;
				pCommand->m_pCapabilities->m_ShaderSupport = true;

				int TextureLayers = 0;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &TextureLayers);
				if(TextureLayers >= 256)
				{
					pCommand->m_pCapabilities->m_2DArrayTextures = true;
				}

				int Texture3DSize = 0;
				glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &Texture3DSize);
				if(Texture3DSize >= 256)
				{
					pCommand->m_pCapabilities->m_3DTextures = true;
				}

				if(!pCommand->m_pCapabilities->m_3DTextures && !pCommand->m_pCapabilities->m_2DArrayTextures)
				{
					*pCommand->m_pInitError = -2;
					pCommand->m_pCapabilities->m_ContextMajor = 1;
					pCommand->m_pCapabilities->m_ContextMinor = 5;
					pCommand->m_pCapabilities->m_ContextPatch = 0;
				}

				pCommand->m_pCapabilities->m_TileBuffering = pCommand->m_pCapabilities->m_2DArrayTextures;
				pCommand->m_pCapabilities->m_QuadBuffering = false;
				pCommand->m_pCapabilities->m_TextBuffering = false;
				pCommand->m_pCapabilities->m_QuadContainerBuffering = false;
			}
			else if(MajorV == 2)
			{
				pCommand->m_pCapabilities->m_MipMapping = true;
				// check for context extension: 2D array texture and its max size
				pCommand->m_pCapabilities->m_3DTextures = false;
				pCommand->m_pCapabilities->m_2DArrayTextures = false;

				pCommand->m_pCapabilities->m_ShaderSupport = false;

				int Texture3DSize = 0;
				glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &Texture3DSize);
				if(Texture3DSize >= 256)
				{
					pCommand->m_pCapabilities->m_3DTextures = true;
				}

				pCommand->m_pCapabilities->m_TileBuffering = false;
				pCommand->m_pCapabilities->m_QuadBuffering = false;
				pCommand->m_pCapabilities->m_TextBuffering = false;
				pCommand->m_pCapabilities->m_QuadContainerBuffering = false;

				pCommand->m_pCapabilities->m_NPOTTextures = GLEW_ARB_texture_non_power_of_two || pCommand->m_GlewMajor > 2;

				if(!pCommand->m_pCapabilities->m_NPOTTextures || (!pCommand->m_pCapabilities->m_3DTextures && !pCommand->m_pCapabilities->m_2DArrayTextures))
				{
					*pCommand->m_pInitError = -2;
					pCommand->m_pCapabilities->m_ContextMajor = 1;
					pCommand->m_pCapabilities->m_ContextMinor = 5;
					pCommand->m_pCapabilities->m_ContextPatch = 0;
				}
			}
			else if(MajorV < 2)
			{
				pCommand->m_pCapabilities->m_TileBuffering = false;
				pCommand->m_pCapabilities->m_QuadBuffering = false;
				pCommand->m_pCapabilities->m_TextBuffering = false;
				pCommand->m_pCapabilities->m_QuadContainerBuffering = false;
				pCommand->m_pCapabilities->m_ShaderSupport = false;

				pCommand->m_pCapabilities->m_MipMapping = false;
				pCommand->m_pCapabilities->m_3DTextures = false;
				pCommand->m_pCapabilities->m_2DArrayTextures = false;
				pCommand->m_pCapabilities->m_NPOTTextures = false;
			}
		}
#endif
	}
	else if(pCommand->m_RequestedBackend == BACKEND_TYPE_OPENGL_ES)
	{
		if(MajorV < 3)
		{
			pCommand->m_pCapabilities->m_TileBuffering = false;
			pCommand->m_pCapabilities->m_QuadBuffering = false;
			pCommand->m_pCapabilities->m_TextBuffering = false;
			pCommand->m_pCapabilities->m_QuadContainerBuffering = false;
			pCommand->m_pCapabilities->m_ShaderSupport = false;

			pCommand->m_pCapabilities->m_MipMapping = false;
			pCommand->m_pCapabilities->m_3DTextures = false;
			pCommand->m_pCapabilities->m_2DArrayTextures = false;
			pCommand->m_pCapabilities->m_NPOTTextures = false;

			pCommand->m_pCapabilities->m_TrianglesAsQuads = false;
		}
		else
		{
			pCommand->m_pCapabilities->m_TileBuffering = true;
			pCommand->m_pCapabilities->m_QuadBuffering = true;
			pCommand->m_pCapabilities->m_TextBuffering = true;
			pCommand->m_pCapabilities->m_QuadContainerBuffering = true;
			pCommand->m_pCapabilities->m_ShaderSupport = true;

			pCommand->m_pCapabilities->m_MipMapping = true;
			pCommand->m_pCapabilities->m_3DTextures = true;
			pCommand->m_pCapabilities->m_2DArrayTextures = true;
			pCommand->m_pCapabilities->m_NPOTTextures = true;

			pCommand->m_pCapabilities->m_TrianglesAsQuads = true;
		}
	}

	if(*pCommand->m_pInitError != -2)
	{
		// set some default settings
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

#ifndef BACKEND_GL_MODERN_API
		if(!IsNewApi())
		{
			glAlphaFunc(GL_GREATER, 0);
			glEnable(GL_ALPHA_TEST);
		}
#endif

		glDepthMask(0);

#ifndef BACKEND_AS_OPENGL_ES
		if(g_Config.m_DbgGfx != DEBUG_GFX_MODE_NONE)
		{
			if(GLEW_KHR_debug || GLEW_ARB_debug_output)
			{
				// During init, enable debug output
				if(GLEW_KHR_debug)
				{
					glEnable(GL_DEBUG_OUTPUT);
					glDebugMessageCallback((GLDEBUGPROC)GfxOpenGLMessageCallback, 0);
				}
				else if(GLEW_ARB_debug_output)
				{
					glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
					glDebugMessageCallbackARB((GLDEBUGPROC)GfxOpenGLMessageCallback, 0);
				}
				dbg_msg("gfx", "Enabled OpenGL debug mode");
			}
			else
				dbg_msg("gfx", "Requested OpenGL debug mode, but the driver does not support the required extension");
		}
#endif

		return true;
	}
	else
		return false;
}

bool CCommandProcessorFragment_OpenGL::Cmd_Init(const SCommand_Init *pCommand)
{
	if(!InitOpenGL(pCommand))
		return false;

	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_pTextureMemoryUsage->store(0, std::memory_order_relaxed);
	m_MaxTexSize = -1;

	m_OpenGLTextureLodBIAS = 0;

	m_Has2DArrayTextures = pCommand->m_pCapabilities->m_2DArrayTextures;
	if(pCommand->m_pCapabilities->m_2DArrayTexturesAsExtension)
	{
		m_Has2DArrayTexturesAsExtension = true;
		m_2DArrayTarget = GL_TEXTURE_2D_ARRAY_EXT;
	}
	else
	{
		m_Has2DArrayTexturesAsExtension = false;
		m_2DArrayTarget = GL_TEXTURE_2D_ARRAY;
	}

	m_Has3DTextures = pCommand->m_pCapabilities->m_3DTextures;
	m_HasMipMaps = pCommand->m_pCapabilities->m_MipMapping;
	m_HasNPOTTextures = pCommand->m_pCapabilities->m_NPOTTextures;

	m_LastBlendMode = CCommandBuffer::BLEND_ALPHA;
	m_LastClipEnable = false;

	return true;
}

void CCommandProcessorFragment_OpenGL::TextureUpdate(int Slot, int X, int Y, int Width, int Height, int GLFormat, uint8_t *pTexData)
{
	glBindTexture(GL_TEXTURE_2D, m_vTextures[Slot].m_Tex);

	if(!m_HasNPOTTextures)
	{
		float ResizeW = m_vTextures[Slot].m_ResizeWidth;
		float ResizeH = m_vTextures[Slot].m_ResizeHeight;
		if(ResizeW > 0 && ResizeH > 0)
		{
			int ResizedW = (int)(Width * ResizeW);
			int ResizedH = (int)(Height * ResizeH);

			uint8_t *pTmpData = ResizeImage(pTexData, Width, Height, ResizedW, ResizedH, GLFormatToPixelSize(GLFormat));
			free(pTexData);
			pTexData = pTmpData;

			Width = ResizedW;
			Height = ResizedH;
		}
	}

	if(m_vTextures[Slot].m_RescaleCount > 0)
	{
		int OldWidth = Width;
		int OldHeight = Height;
		for(int i = 0; i < m_vTextures[Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		uint8_t *pTmpData = ResizeImage(pTexData, OldWidth, OldHeight, Width, Height, GLFormatToPixelSize(GLFormat));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height, GLFormat, GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL::DestroyTexture(int Slot)
{
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) - m_vTextures[Slot].m_MemSize, std::memory_order_relaxed);

	if(m_vTextures[Slot].m_Tex != 0)
	{
		glDeleteTextures(1, &m_vTextures[Slot].m_Tex);
	}

	if(m_vTextures[Slot].m_Tex2DArray != 0)
	{
		glDeleteTextures(1, &m_vTextures[Slot].m_Tex2DArray);
	}

	if(IsNewApi())
	{
		if(m_vTextures[Slot].m_Sampler != 0)
		{
			glDeleteSamplers(1, &m_vTextures[Slot].m_Sampler);
		}
		if(m_vTextures[Slot].m_Sampler2DArray != 0)
		{
			glDeleteSamplers(1, &m_vTextures[Slot].m_Sampler2DArray);
		}
	}

	m_vTextures[Slot].m_Tex = 0;
	m_vTextures[Slot].m_Sampler = 0;
	m_vTextures[Slot].m_Tex2DArray = 0;
	m_vTextures[Slot].m_Sampler2DArray = 0;
	m_vTextures[Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	DestroyTexture(pCommand->m_Slot);
}

void CCommandProcessorFragment_OpenGL::TextureCreate(int Slot, int Width, int Height, int GLFormat, int GLStoreFormat, int Flags, uint8_t *pTexData)
{
#ifndef BACKEND_GL_MODERN_API

	if(m_MaxTexSize == -1)
	{
		// fix the alignment to allow even 1byte changes, e.g. for alpha components
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
	}

	while(Slot >= (int)m_vTextures.size())
		m_vTextures.resize(m_vTextures.size() * 2);

	m_vTextures[Slot].m_ResizeWidth = -1.f;
	m_vTextures[Slot].m_ResizeHeight = -1.f;

	if(!m_HasNPOTTextures)
	{
		int PowerOfTwoWidth = HighestBit(Width);
		int PowerOfTwoHeight = HighestBit(Height);
		if(Width != PowerOfTwoWidth || Height != PowerOfTwoHeight)
		{
			uint8_t *pTmpData = ResizeImage(pTexData, Width, Height, PowerOfTwoWidth, PowerOfTwoHeight, GLFormatToPixelSize(GLFormat));
			free(pTexData);
			pTexData = pTmpData;

			m_vTextures[Slot].m_ResizeWidth = (float)PowerOfTwoWidth / (float)Width;
			m_vTextures[Slot].m_ResizeHeight = (float)PowerOfTwoHeight / (float)Height;

			Width = PowerOfTwoWidth;
			Height = PowerOfTwoHeight;
		}
	}

	int RescaleCount = 0;
	if(GLFormat == GL_RGBA)
	{
		int OldWidth = Width;
		int OldHeight = Height;
		bool NeedsResize = false;

		if(Width > m_MaxTexSize || Height > m_MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
				++RescaleCount;
			} while(Width > m_MaxTexSize || Height > m_MaxTexSize);
			NeedsResize = true;
		}

		if(NeedsResize)
		{
			uint8_t *pTmpData = ResizeImage(pTexData, OldWidth, OldHeight, Width, Height, GLFormatToPixelSize(GLFormat));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_vTextures[Slot].m_Width = Width;
	m_vTextures[Slot].m_Height = Height;
	m_vTextures[Slot].m_RescaleCount = RescaleCount;

	const size_t PixelSize = GLFormatToPixelSize(GLFormat);

	if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
	{
		glGenTextures(1, &m_vTextures[Slot].m_Tex);
		glBindTexture(GL_TEXTURE_2D, m_vTextures[Slot].m_Tex);
	}

	if(Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS || !m_HasMipMaps)
	{
		if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GLStoreFormat, Width, Height, 0, GLFormat, GL_UNSIGNED_BYTE, pTexData);
		}
	}
	else
	{
		if((Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			glTexImage2D(GL_TEXTURE_2D, 0, GLStoreFormat, Width, Height, 0, GLFormat, GL_UNSIGNED_BYTE, pTexData);
		}

		int Flag2DArrayTexture = CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE;
		int Flag3DTexture = CCommandBuffer::TEXFLAG_TO_3D_TEXTURE;
		if((Flags & (Flag2DArrayTexture | Flag3DTexture)) != 0)
		{
			bool Is3DTexture = (Flags & Flag3DTexture) != 0;

			glGenTextures(1, &m_vTextures[Slot].m_Tex2DArray);

			GLenum Target = GL_TEXTURE_3D;

			if(Is3DTexture)
			{
				Target = GL_TEXTURE_3D;
			}
			else
			{
				Target = m_2DArrayTarget;
			}

			glBindTexture(Target, m_vTextures[Slot].m_Tex2DArray);

			if(IsNewApi())
			{
				glGenSamplers(1, &m_vTextures[Slot].m_Sampler2DArray);
				glBindSampler(0, m_vTextures[Slot].m_Sampler2DArray);
			}

			glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if(Is3DTexture)
			{
				glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				if(IsNewApi())
					glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(Target, GL_GENERATE_MIPMAP, GL_TRUE);
				if(IsNewApi())
					glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}

			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

#ifndef BACKEND_AS_OPENGL_ES
			if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
				glTexParameterf(Target, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

			if(IsNewApi())
			{
				glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glSamplerParameteri(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

#ifndef BACKEND_AS_OPENGL_ES
				if(m_OpenGLTextureLodBIAS != 0 && !m_IsOpenGLES)
					glSamplerParameterf(m_vTextures[Slot].m_Sampler2DArray, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
#endif

				glBindSampler(0, 0);
			}

			uint8_t *p3DImageData = static_cast<uint8_t *>(malloc((size_t)Width * Height * PixelSize));
			int Image3DWidth, Image3DHeight;

			int ConvertWidth = Width;
			int ConvertHeight = Height;

			if(ConvertWidth == 0 || (ConvertWidth % 16) != 0 || ConvertHeight == 0 || (ConvertHeight % 16) != 0)
			{
				dbg_msg("gfx", "3D/2D array texture was resized");
				int NewWidth = maximum<int>(HighestBit(ConvertWidth), 16);
				int NewHeight = maximum<int>(HighestBit(ConvertHeight), 16);
				uint8_t *pNewTexData = ResizeImage(pTexData, ConvertWidth, ConvertHeight, NewWidth, NewHeight, GLFormatToPixelSize(GLFormat));

				ConvertWidth = NewWidth;
				ConvertHeight = NewHeight;

				free(pTexData);
				pTexData = pNewTexData;
			}

			if(Texture2DTo3D(pTexData, ConvertWidth, ConvertHeight, PixelSize, 16, 16, p3DImageData, Image3DWidth, Image3DHeight))
			{
				glTexImage3D(Target, 0, GLStoreFormat, Image3DWidth, Image3DHeight, 256, 0, GLFormat, GL_UNSIGNED_BYTE, p3DImageData);
			}

			free(p3DImageData);
		}
	}

	// This is the initial value for the wrap modes
	m_vTextures[Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_vTextures[Slot].m_MemSize = (size_t)Width * Height * PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_vTextures[Slot].m_MemSize += (size_t)Width * Height * PixelSize;
	}
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) + m_vTextures[Slot].m_MemSize, std::memory_order_relaxed);

	free(pTexData);
#endif
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	TextureCreate(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, GL_RGBA, GL_RGBA, pCommand->m_Flags, pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL::Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand)
{
	TextureUpdate(pCommand->m_Slot, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height, GL_ALPHA, pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL::Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand)
{
	DestroyTexture(pCommand->m_Slot);
	DestroyTexture(pCommand->m_SlotOutline);
}

void CCommandProcessorFragment_OpenGL::Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand)
{
	TextureCreate(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, GL_ALPHA, GL_ALPHA, CCommandBuffer::TEXFLAG_NOMIPMAPS, pCommand->m_pTextData);
	TextureCreate(pCommand->m_SlotOutline, pCommand->m_Width, pCommand->m_Height, GL_ALPHA, GL_ALPHA, CCommandBuffer::TEXFLAG_NOMIPMAPS, pCommand->m_pTextOutlineData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	// if clip is still active, force disable it for clearing, enable it again afterwards
	bool ClipWasEnabled = m_LastClipEnable;
	if(ClipWasEnabled)
	{
		glDisable(GL_SCISSOR_TEST);
	}
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(ClipWasEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
	}
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
#ifndef BACKEND_GL_MODERN_API
	SetState(pCommand->m_State);

	glVertexPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 2);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 4);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
#ifndef BACKEND_AS_OPENGL_ES
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount * 4);
#endif
		break;
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, 0, pCommand->m_PrimCount * 3);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};
#endif
}

void CCommandProcessorFragment_OpenGL::Cmd_ReadPixel(const CCommandBuffer::SCommand_TrySwapAndReadPixel *pCommand)
{
	// get size of viewport
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);
	const int h = aViewport[3];

	// fetch the pixel
	uint8_t aPixelData[3];
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(pCommand->m_Position.x, h - 1 - pCommand->m_Position.y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, aPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// fill in the information
	*pCommand->m_pColor = ColorRGBA(aPixelData[0] / 255.0f, aPixelData[1] / 255.0f, aPixelData[2] / 255.0f, 1.0f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::SCommand_TrySwapAndScreenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)malloc((size_t)w * (h + 1) * 4);
	unsigned char *pTempRow = pPixelData + w * h * 4;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h / 2; y++)
	{
		mem_copy(pTempRow, pPixelData + y * w * 4, w * 4);
		mem_copy(pPixelData + y * w * 4, pPixelData + (h - y - 1) * w * 4, w * 4);
		mem_copy(pPixelData + (h - y - 1) * w * 4, pTempRow, w * 4);
		for(int x = 0; x < w; x++)
		{
			pPixelData[y * w * 4 + x * 4 + 3] = 255;
			pPixelData[(h - y - 1) * w * 4 + x * 4 + 3] = 255;
		}
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGBA;
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL::CCommandProcessorFragment_OpenGL()
{
	m_vTextures.resize(CCommandBuffer::MAX_TEXTURES);
	m_HasShaders = false;
}

ERunCommandReturnTypes CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandProcessorFragment_OpenGL::CMD_INIT:
		Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
		break;
	case CCommandProcessorFragment_OpenGL::CMD_SHUTDOWN:
		Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXTURE_CREATE:
		Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY:
		Cmd_Texture_Destroy(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURES_CREATE:
		Cmd_TextTextures_Create(static_cast<const CCommandBuffer::SCommand_TextTextures_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURES_DESTROY:
		Cmd_TextTextures_Destroy(static_cast<const CCommandBuffer::SCommand_TextTextures_Destroy *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURE_UPDATE:
		Cmd_TextTexture_Update(static_cast<const CCommandBuffer::SCommand_TextTexture_Update *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_CLEAR:
		Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_RENDER:
		Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_RENDER_TEX3D:
		Cmd_RenderTex3D(static_cast<const CCommandBuffer::SCommand_RenderTex3D *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TRY_SWAP_AND_READ_PIXEL:
		Cmd_ReadPixel(static_cast<const CCommandBuffer::SCommand_TrySwapAndReadPixel *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TRY_SWAP_AND_SCREENSHOT:
		Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_TrySwapAndScreenshot *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_UPDATE_VIEWPORT:
		Cmd_Update_Viewport(static_cast<const CCommandBuffer::SCommand_Update_Viewport *>(pBaseCommand));
		break;

	case CCommandBuffer::CMD_CREATE_BUFFER_OBJECT: Cmd_CreateBufferObject(static_cast<const CCommandBuffer::SCommand_CreateBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_UPDATE_BUFFER_OBJECT: Cmd_UpdateBufferObject(static_cast<const CCommandBuffer::SCommand_UpdateBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RECREATE_BUFFER_OBJECT: Cmd_RecreateBufferObject(static_cast<const CCommandBuffer::SCommand_RecreateBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_COPY_BUFFER_OBJECT: Cmd_CopyBufferObject(static_cast<const CCommandBuffer::SCommand_CopyBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_DELETE_BUFFER_OBJECT: Cmd_DeleteBufferObject(static_cast<const CCommandBuffer::SCommand_DeleteBufferObject *>(pBaseCommand)); break;

	case CCommandBuffer::CMD_CREATE_BUFFER_CONTAINER: Cmd_CreateBufferContainer(static_cast<const CCommandBuffer::SCommand_CreateBufferContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_UPDATE_BUFFER_CONTAINER: Cmd_UpdateBufferContainer(static_cast<const CCommandBuffer::SCommand_UpdateBufferContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_DELETE_BUFFER_CONTAINER: Cmd_DeleteBufferContainer(static_cast<const CCommandBuffer::SCommand_DeleteBufferContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_INDICES_REQUIRED_NUM_NOTIFY: Cmd_IndicesRequiredNumNotify(static_cast<const CCommandBuffer::SCommand_IndicesRequiredNumNotify *>(pBaseCommand)); break;

	case CCommandBuffer::CMD_RENDER_TILE_LAYER: Cmd_RenderTileLayer(static_cast<const CCommandBuffer::SCommand_RenderTileLayer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_BORDER_TILE: Cmd_RenderBorderTile(static_cast<const CCommandBuffer::SCommand_RenderBorderTile *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_LAYER: Cmd_RenderQuadLayer(static_cast<const CCommandBuffer::SCommand_RenderQuadLayer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_TEXT: Cmd_RenderText(static_cast<const CCommandBuffer::SCommand_RenderText *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER: Cmd_RenderQuadContainer(static_cast<const CCommandBuffer::SCommand_RenderQuadContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_EX: Cmd_RenderQuadContainerEx(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerEx *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE: Cmd_RenderQuadContainerAsSpriteMultiple(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *>(pBaseCommand)); break;
	default: return ERunCommandReturnTypes::RUN_COMMAND_COMMAND_UNHANDLED;
	}

	return ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED;
}

// ------------ CCommandProcessorFragment_OpenGL2

void CCommandProcessorFragment_OpenGL2::UseProgram(CGLSLTWProgram *pProgram)
{
	pProgram->UseProgram();
}

void CCommandProcessorFragment_OpenGL2::SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram, bool Use2DArrayTextures)
{
	if(m_LastBlendMode == CCommandBuffer::BLEND_NONE)
	{
		m_LastBlendMode = CCommandBuffer::BLEND_ALPHA;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	if(State.m_BlendMode != m_LastBlendMode && State.m_BlendMode != CCommandBuffer::BLEND_NONE)
	{
		// blend
		switch(State.m_BlendMode)
		{
		case CCommandBuffer::BLEND_NONE:
			// We don't really need this anymore
			// glDisable(GL_BLEND);
			break;
		case CCommandBuffer::BLEND_ALPHA:
			// glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case CCommandBuffer::BLEND_ADDITIVE:
			// glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		default:
			dbg_msg("render", "unknown blendmode %d\n", State.m_BlendMode);
		};

		m_LastBlendMode = State.m_BlendMode;
	}

	// clip
	if(State.m_ClipEnable)
	{
		glScissor(State.m_ClipX, State.m_ClipY, State.m_ClipW, State.m_ClipH);
		glEnable(GL_SCISSOR_TEST);
		m_LastClipEnable = true;
	}
	else if(m_LastClipEnable)
	{
		// Don't disable it always
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}

	if(!IsNewApi())
	{
		glDisable(GL_TEXTURE_2D);
		if(!m_HasShaders)
		{
			if(m_Has3DTextures)
				glDisable(GL_TEXTURE_3D);
			if(m_Has2DArrayTextures)
			{
				glDisable(m_2DArrayTarget);
			}
		}
	}

	// texture
	if(IsTexturedState(State))
	{
		int Slot = 0;
		if(!Use2DArrayTextures)
		{
			if(!IsNewApi() && !m_HasShaders)
				glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_vTextures[State.m_Texture].m_Tex);
			if(IsNewApi())
				glBindSampler(Slot, m_vTextures[State.m_Texture].m_Sampler);
		}
		else
		{
			if(!m_Has2DArrayTextures)
			{
				if(!IsNewApi() && !m_HasShaders)
					glEnable(GL_TEXTURE_3D);
				glBindTexture(GL_TEXTURE_3D, m_vTextures[State.m_Texture].m_Tex2DArray);
				if(IsNewApi())
					glBindSampler(Slot, m_vTextures[State.m_Texture].m_Sampler2DArray);
			}
			else
			{
				if(!IsNewApi() && !m_HasShaders)
					glEnable(m_2DArrayTarget);
				glBindTexture(m_2DArrayTarget, m_vTextures[State.m_Texture].m_Tex2DArray);
				if(IsNewApi())
					glBindSampler(Slot, m_vTextures[State.m_Texture].m_Sampler2DArray);
			}
		}

		if(pProgram->m_LastTextureSampler != Slot)
		{
			pProgram->SetUniform(pProgram->m_LocTextureSampler, Slot);
			pProgram->m_LastTextureSampler = Slot;
		}

		if(m_vTextures[State.m_Texture].m_LastWrapMode != State.m_WrapMode && !Use2DArrayTextures)
		{
			switch(State.m_WrapMode)
			{
			case CCommandBuffer::WRAP_REPEAT:
				if(IsNewApi())
				{
					glSamplerParameteri(m_vTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glSamplerParameteri(m_vTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
				}
				break;
			case CCommandBuffer::WRAP_CLAMP:
				if(IsNewApi())
				{
					glSamplerParameteri(m_vTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glSamplerParameteri(m_vTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				break;
			default:
				dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapMode);
			};
			m_vTextures[State.m_Texture].m_LastWrapMode = State.m_WrapMode;
		}
	}

	if(pProgram->m_LastScreenTL != State.m_ScreenTL || pProgram->m_LastScreenBR != State.m_ScreenBR)
	{
		pProgram->m_LastScreenTL = State.m_ScreenTL;
		pProgram->m_LastScreenBR = State.m_ScreenBR;

		// screen mapping
		// orthographic projection matrix
		// the z coordinate is the same for every vertex, so just ignore the z coordinate and set it in the shaders
		float m[2 * 4] = {
			2.f / (State.m_ScreenBR.x - State.m_ScreenTL.x),
			0,
			0,
			-((State.m_ScreenBR.x + State.m_ScreenTL.x) / (State.m_ScreenBR.x - State.m_ScreenTL.x)),
			0,
			(2.f / (State.m_ScreenTL.y - State.m_ScreenBR.y)),
			0,
			-((State.m_ScreenTL.y + State.m_ScreenBR.y) / (State.m_ScreenTL.y - State.m_ScreenBR.y)),
		};

		// transpose bcs of column-major order of opengl
		glUniformMatrix4x2fv(pProgram->m_LocPos, 1, true, (float *)&m);
	}
}

#ifndef BACKEND_GL_MODERN_API
bool CCommandProcessorFragment_OpenGL2::DoAnalyzeStep(size_t CheckCount, size_t VerticesCount, uint8_t aFakeTexture[], size_t SingleImageSize)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int Slot = 0;
	if(m_HasShaders)
	{
		CGLSLTWProgram *pProgram = m_pPrimitive3DProgramTextured;
		UseProgram(pProgram);

		pProgram->SetUniform(pProgram->m_LocTextureSampler, Slot);

		float m[2 * 4] = {
			1, 0, 0, 0,
			0, 1, 0, 0};

		// transpose bcs of column-major order of opengl
		glUniformMatrix4x2fv(pProgram->m_LocPos, 1, true, (float *)&m);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-1, 1, -1, 1, -10.0f, 10.f);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(m_aStreamVertices[0]), m_aStreamVertices);
	glColorPointer(4, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2)));
	glTexCoordPointer(3, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(vec4)));

	glDrawArrays(GL_QUADS, 0, VerticesCount);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(m_HasShaders)
	{
		glUseProgram(0);
	}

	glFinish();

	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	size_t PixelDataSize = (size_t)w * h * 3;
	if(PixelDataSize == 0)
		return false;
	uint8_t *pPixelData = (uint8_t *)malloc(PixelDataSize);

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// now analyse the image data
	bool CheckFailed = false;
	int WidthTile = w / 16;
	int HeightTile = h / 16;
	int StartX = WidthTile / 2;
	int StartY = HeightTile / 2;
	for(size_t d = 0; d < CheckCount; ++d)
	{
		int CurX = (int)d % 16;
		int CurY = (int)d / 16;

		int CheckX = StartX + CurX * WidthTile;
		int CheckY = StartY + CurY * HeightTile;

		ptrdiff_t OffsetPixelData = (CheckY * (w * 3)) + (CheckX * 3);
		ptrdiff_t OffsetFakeTexture = SingleImageSize * d;
		OffsetPixelData = clamp<ptrdiff_t>(OffsetPixelData, 0, (ptrdiff_t)PixelDataSize);
		OffsetFakeTexture = clamp<ptrdiff_t>(OffsetFakeTexture, 0, (ptrdiff_t)(SingleImageSize * CheckCount));
		uint8_t *pPixel = pPixelData + OffsetPixelData;
		uint8_t *pPixelTex = aFakeTexture + OffsetFakeTexture;
		for(size_t i = 0; i < 3; ++i)
		{
			if((pPixel[i] < pPixelTex[i] - 25) || (pPixel[i] > pPixelTex[i] + 25))
			{
				CheckFailed = true;
				break;
			}
		}
	}

	free(pPixelData);
	return !CheckFailed;
}

bool CCommandProcessorFragment_OpenGL2::IsTileMapAnalysisSucceeded()
{
	glClearColor(0, 0, 0, 1);

	// create fake texture 1024x1024
	const size_t ImageWidth = 1024;
	const size_t ImageHeight = 1024;
	uint8_t *pFakeTexture = (uint8_t *)malloc(sizeof(uint8_t) * ImageWidth * ImageHeight * 4);
	// fill by colors stepping by 50 => (255 / 50 ~ 5) => 5 times 3(color channels) = 5 ^ 3 = 125 possibilities to check
	size_t CheckCount = 5 * 5 * 5;
	// always fill 4 pixels of the texture, so the sampling is accurate
	int aCurColor[4] = {25, 25, 25, 255};
	const size_t SingleImageWidth = 64;
	const size_t SingleImageHeight = 64;
	size_t SingleImageSize = SingleImageWidth * SingleImageHeight * 4;
	for(size_t d = 0; d < CheckCount; ++d)
	{
		uint8_t *pCurFakeTexture = pFakeTexture + (ptrdiff_t)(SingleImageSize * d);

		uint8_t aCurColorUint8[SingleImageWidth * SingleImageHeight * 4];
		for(size_t y = 0; y < SingleImageHeight; ++y)
		{
			for(size_t x = 0; x < SingleImageWidth; ++x)
			{
				for(size_t i = 0; i < 4; ++i)
				{
					aCurColorUint8[(y * SingleImageWidth * 4) + (x * 4) + i] = (uint8_t)aCurColor[i];
				}
			}
		}
		mem_copy(pCurFakeTexture, aCurColorUint8, sizeof(aCurColorUint8));

		aCurColor[2] += 50;
		if(aCurColor[2] > 225)
		{
			aCurColor[2] -= 250;
			aCurColor[1] += 50;
		}
		if(aCurColor[1] > 225)
		{
			aCurColor[1] -= 250;
			aCurColor[0] += 50;
		}
		if(aCurColor[0] > 225)
		{
			break;
		}
	}

	// upload the texture
	GLuint FakeTexture;
	glGenTextures(1, &FakeTexture);

	GLenum Target = GL_TEXTURE_3D;
	if(m_Has2DArrayTextures)
	{
		Target = m_2DArrayTarget;
	}

	glBindTexture(Target, FakeTexture);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if(!m_Has2DArrayTextures)
	{
		glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(Target, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

	glTexImage3D(Target, 0, GL_RGBA, ImageWidth / 16, ImageHeight / 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pFakeTexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_SCISSOR_TEST);

	if(!m_HasShaders)
	{
		glDisable(GL_TEXTURE_2D);
		if(m_Has3DTextures)
			glDisable(GL_TEXTURE_3D);
		if(m_Has2DArrayTextures)
		{
			glDisable(m_2DArrayTarget);
		}

		if(!m_Has2DArrayTextures)
		{
			glEnable(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, FakeTexture);
		}
		else
		{
			glEnable(m_2DArrayTarget);
			glBindTexture(m_2DArrayTarget, FakeTexture);
		}
	}

	static_assert(sizeof(m_aStreamVertices) / sizeof(m_aStreamVertices[0]) >= 256 * 4, "Keep the number of stream vertices >= 256 * 4.");

	size_t VertexCount = 0;
	for(size_t i = 0; i < CheckCount; ++i)
	{
		float XPos = (float)(i % 16);
		float YPos = (float)(i / 16);

		GL_SVertexTex3D *pVertex = &m_aStreamVertices[VertexCount++];
		GL_SVertexTex3D *pVertexBefore = pVertex;
		pVertex->m_Pos.x = XPos / 16.f;
		pVertex->m_Pos.y = YPos / 16.f;
		pVertex->m_Color.r = 1;
		pVertex->m_Color.g = 1;
		pVertex->m_Color.b = 1;
		pVertex->m_Color.a = 1;
		pVertex->m_Tex.u = 0;
		pVertex->m_Tex.v = 0;

		pVertex = &m_aStreamVertices[VertexCount++];
		pVertex->m_Pos.x = XPos / 16.f + 1.f / 16.f;
		pVertex->m_Pos.y = YPos / 16.f;
		pVertex->m_Color.r = 1;
		pVertex->m_Color.g = 1;
		pVertex->m_Color.b = 1;
		pVertex->m_Color.a = 1;
		pVertex->m_Tex.u = 1;
		pVertex->m_Tex.v = 0;

		pVertex = &m_aStreamVertices[VertexCount++];
		pVertex->m_Pos.x = XPos / 16.f + 1.f / 16.f;
		pVertex->m_Pos.y = YPos / 16.f + 1.f / 16.f;
		pVertex->m_Color.r = 1;
		pVertex->m_Color.g = 1;
		pVertex->m_Color.b = 1;
		pVertex->m_Color.a = 1;
		pVertex->m_Tex.u = 1;
		pVertex->m_Tex.v = 1;

		pVertex = &m_aStreamVertices[VertexCount++];
		pVertex->m_Pos.x = XPos / 16.f;
		pVertex->m_Pos.y = YPos / 16.f + 1.f / 16.f;
		pVertex->m_Color.r = 1;
		pVertex->m_Color.g = 1;
		pVertex->m_Color.b = 1;
		pVertex->m_Color.a = 1;
		pVertex->m_Tex.u = 0;
		pVertex->m_Tex.v = 1;

		for(size_t n = 0; n < 4; ++n)
		{
			pVertexBefore[n].m_Pos.x *= 2;
			pVertexBefore[n].m_Pos.x -= 1;
			pVertexBefore[n].m_Pos.y *= 2;
			pVertexBefore[n].m_Pos.y -= 1;
			if(m_Has2DArrayTextures)
			{
				pVertexBefore[n].m_Tex.w = i;
			}
			else
			{
				pVertexBefore[n].m_Tex.w = (i + 0.5f) / 256.f;
			}
		}
	}

	// everything build up, now do the analyze steps
	bool NoError = DoAnalyzeStep(CheckCount, VertexCount, pFakeTexture, SingleImageSize);

	glDeleteTextures(1, &FakeTexture);
	free(pFakeTexture);

	return NoError;
}

bool CCommandProcessorFragment_OpenGL2::Cmd_Init(const SCommand_Init *pCommand)
{
	if(!CCommandProcessorFragment_OpenGL::Cmd_Init(pCommand))
		return false;

	m_pTileProgram = nullptr;
	m_pTileProgramTextured = nullptr;
	m_pPrimitive3DProgram = nullptr;
	m_pPrimitive3DProgramTextured = nullptr;

	m_OpenGLTextureLodBIAS = g_Config.m_GfxGLTextureLODBIAS;

	m_HasShaders = pCommand->m_pCapabilities->m_ShaderSupport;

	bool HasAllFunc = true;
#ifndef BACKEND_AS_OPENGL_ES
	if(m_HasShaders)
	{
		HasAllFunc &= (glUniformMatrix4x2fv != NULL) && (glGenBuffers != NULL);
		HasAllFunc &= (glBindBuffer != NULL) && (glBufferData != NULL);
		HasAllFunc &= (glEnableVertexAttribArray != NULL) && (glVertexAttribPointer != NULL) && (glVertexAttribIPointer != NULL);
		HasAllFunc &= (glDisableVertexAttribArray != NULL) && (glDeleteBuffers != NULL);
		HasAllFunc &= (glUseProgram != NULL) && (glTexImage3D != NULL);
		HasAllFunc &= (glBindAttribLocation != NULL) && (glTexImage3D != NULL);
		HasAllFunc &= (glBufferSubData != NULL) && (glGetUniformLocation != NULL);
		HasAllFunc &= (glUniform1i != NULL) && (glUniform1f != NULL);
		HasAllFunc &= (glUniform1ui != NULL) && (glUniform1i != NULL);
		HasAllFunc &= (glUniform1fv != NULL) && (glUniform2fv != NULL);
		HasAllFunc &= (glUniform4fv != NULL) && (glGetAttachedShaders != NULL);
		HasAllFunc &= (glGetProgramInfoLog != NULL) && (glGetProgramiv != NULL);
		HasAllFunc &= (glLinkProgram != NULL) && (glDetachShader != NULL);
		HasAllFunc &= (glAttachShader != NULL) && (glDeleteProgram != NULL);
		HasAllFunc &= (glCreateProgram != NULL) && (glShaderSource != NULL);
		HasAllFunc &= (glCompileShader != NULL) && (glGetShaderiv != NULL);
		HasAllFunc &= (glGetShaderInfoLog != NULL) && (glDeleteShader != NULL);
		HasAllFunc &= (glCreateShader != NULL);
	}
#endif

	bool AnalysisCorrect = true;
	if(HasAllFunc)
	{
		if(m_HasShaders)
		{
			m_pTileProgram = new CGLSLTileProgram;
			m_pTileProgramTextured = new CGLSLTileProgram;
			m_pBorderTileProgram = new CGLSLTileProgram;
			m_pBorderTileProgramTextured = new CGLSLTileProgram;
			m_pPrimitive3DProgram = new CGLSLPrimitiveProgram;
			m_pPrimitive3DProgramTextured = new CGLSLPrimitiveProgram;

			CGLSLCompiler ShaderCompiler(g_Config.m_GfxGLMajor, g_Config.m_GfxGLMinor, g_Config.m_GfxGLPatch, m_IsOpenGLES, m_OpenGLTextureLodBIAS / 1000.0f);
			ShaderCompiler.SetHasTextureArray(pCommand->m_pCapabilities->m_2DArrayTextures);

			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL PrimitiveVertexShader;
				CGLSL PrimitiveFragmentShader;
				PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.vert", GL_VERTEX_SHADER);
				PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.frag", GL_FRAGMENT_SHADER);

				m_pPrimitive3DProgram->CreateProgram();
				m_pPrimitive3DProgram->AddShader(&PrimitiveVertexShader);
				m_pPrimitive3DProgram->AddShader(&PrimitiveFragmentShader);
				m_pPrimitive3DProgram->LinkProgram();

				UseProgram(m_pPrimitive3DProgram);

				m_pPrimitive3DProgram->m_LocPos = m_pPrimitive3DProgram->GetUniformLoc("gPos");
			}

			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL PrimitiveVertexShader;
				CGLSL PrimitiveFragmentShader;
				ShaderCompiler.AddDefine("TW_TEXTURED", "");
				if(!pCommand->m_pCapabilities->m_2DArrayTextures)
					ShaderCompiler.AddDefine("TW_3D_TEXTURED", "");
				PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.vert", GL_VERTEX_SHADER);
				PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.frag", GL_FRAGMENT_SHADER);
				ShaderCompiler.ClearDefines();

				m_pPrimitive3DProgramTextured->CreateProgram();
				m_pPrimitive3DProgramTextured->AddShader(&PrimitiveVertexShader);
				m_pPrimitive3DProgramTextured->AddShader(&PrimitiveFragmentShader);
				m_pPrimitive3DProgramTextured->LinkProgram();

				UseProgram(m_pPrimitive3DProgramTextured);

				m_pPrimitive3DProgramTextured->m_LocPos = m_pPrimitive3DProgramTextured->GetUniformLoc("gPos");
				m_pPrimitive3DProgramTextured->m_LocTextureSampler = m_pPrimitive3DProgramTextured->GetUniformLoc("gTextureSampler");
			}
			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL VertexShader;
				CGLSL FragmentShader;
				VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
				FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);

				m_pTileProgram->CreateProgram();
				m_pTileProgram->AddShader(&VertexShader);
				m_pTileProgram->AddShader(&FragmentShader);

				glBindAttribLocation(m_pTileProgram->GetProgramId(), 0, "inVertex");

				m_pTileProgram->LinkProgram();

				UseProgram(m_pTileProgram);

				m_pTileProgram->m_LocPos = m_pTileProgram->GetUniformLoc("gPos");
				m_pTileProgram->m_LocColor = m_pTileProgram->GetUniformLoc("gVertColor");
			}
			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL VertexShader;
				CGLSL FragmentShader;
				ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
				if(!pCommand->m_pCapabilities->m_2DArrayTextures)
					ShaderCompiler.AddDefine("TW_TILE_3D_TEXTURED", "");
				VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
				FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
				ShaderCompiler.ClearDefines();

				m_pTileProgramTextured->CreateProgram();
				m_pTileProgramTextured->AddShader(&VertexShader);
				m_pTileProgramTextured->AddShader(&FragmentShader);

				glBindAttribLocation(m_pTileProgram->GetProgramId(), 0, "inVertex");
				glBindAttribLocation(m_pTileProgram->GetProgramId(), 1, "inVertexTexCoord");

				m_pTileProgramTextured->LinkProgram();

				UseProgram(m_pTileProgramTextured);

				m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("gPos");
				m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("gTextureSampler");
				m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("gVertColor");
			}
			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL VertexShader;
				CGLSL FragmentShader;
				VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.vert", GL_VERTEX_SHADER);
				FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.frag", GL_FRAGMENT_SHADER);
				ShaderCompiler.ClearDefines();

				m_pBorderTileProgram->CreateProgram();
				m_pBorderTileProgram->AddShader(&VertexShader);
				m_pBorderTileProgram->AddShader(&FragmentShader);

				glBindAttribLocation(m_pBorderTileProgram->GetProgramId(), 0, "inVertex");

				m_pBorderTileProgram->LinkProgram();

				UseProgram(m_pBorderTileProgram);

				m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("gPos");
				m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("gVertColor");
				m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("gOffset");
				m_pBorderTileProgram->m_LocScale = m_pBorderTileProgram->GetUniformLoc("gScale");
			}
			if(pCommand->m_pCapabilities->m_2DArrayTextures)
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_2D_ARRAY);
			else
				ShaderCompiler.SetTextureReplaceType(CGLSLCompiler::GLSL_COMPILER_TEXTURE_REPLACE_TYPE_3D);
			{
				CGLSL VertexShader;
				CGLSL FragmentShader;
				ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
				if(!pCommand->m_pCapabilities->m_2DArrayTextures)
					ShaderCompiler.AddDefine("TW_TILE_3D_TEXTURED", "");
				VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.vert", GL_VERTEX_SHADER);
				FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile_border.frag", GL_FRAGMENT_SHADER);
				ShaderCompiler.ClearDefines();

				m_pBorderTileProgramTextured->CreateProgram();
				m_pBorderTileProgramTextured->AddShader(&VertexShader);
				m_pBorderTileProgramTextured->AddShader(&FragmentShader);

				glBindAttribLocation(m_pBorderTileProgramTextured->GetProgramId(), 0, "inVertex");
				glBindAttribLocation(m_pBorderTileProgramTextured->GetProgramId(), 1, "inVertexTexCoord");

				m_pBorderTileProgramTextured->LinkProgram();

				UseProgram(m_pBorderTileProgramTextured);

				m_pBorderTileProgramTextured->m_LocPos = m_pBorderTileProgramTextured->GetUniformLoc("gPos");
				m_pBorderTileProgramTextured->m_LocTextureSampler = m_pBorderTileProgramTextured->GetUniformLoc("gTextureSampler");
				m_pBorderTileProgramTextured->m_LocColor = m_pBorderTileProgramTextured->GetUniformLoc("gVertColor");
				m_pBorderTileProgramTextured->m_LocOffset = m_pBorderTileProgramTextured->GetUniformLoc("gOffset");
				m_pBorderTileProgramTextured->m_LocScale = m_pBorderTileProgramTextured->GetUniformLoc("gScale");
			}

			glUseProgram(0);
		}

		if(g_Config.m_Gfx3DTextureAnalysisRan == 0 || str_comp(g_Config.m_Gfx3DTextureAnalysisRenderer, pCommand->m_pRendererString) != 0 || str_comp(g_Config.m_Gfx3DTextureAnalysisVersion, pCommand->m_pVersionString) != 0)
		{
			AnalysisCorrect = IsTileMapAnalysisSucceeded();
			if(AnalysisCorrect)
			{
				g_Config.m_Gfx3DTextureAnalysisRan = 1;
				str_copy(g_Config.m_Gfx3DTextureAnalysisRenderer, pCommand->m_pRendererString);
				str_copy(g_Config.m_Gfx3DTextureAnalysisVersion, pCommand->m_pVersionString);
			}
		}
	}

	if(!AnalysisCorrect || !HasAllFunc)
	{
		// downgrade to opengl 1.5
		*pCommand->m_pInitError = -2;
		pCommand->m_pCapabilities->m_ContextMajor = 1;
		pCommand->m_pCapabilities->m_ContextMinor = 5;
		pCommand->m_pCapabilities->m_ContextPatch = 0;

		return false;
	}

	return true;
}

void CCommandProcessorFragment_OpenGL2::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	// TODO: cleanup the OpenGL context too
	delete m_pTileProgram;
	delete m_pTileProgramTextured;
	delete m_pPrimitive3DProgram;
	delete m_pPrimitive3DProgramTextured;
	for(auto &BufferObject : m_vBufferObjectIndices)
		free(BufferObject.m_pData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand)
{
	if(m_HasShaders)
	{
		CGLSLPrimitiveProgram *pProgram = NULL;
		if(IsTexturedState(pCommand->m_State))
		{
			pProgram = m_pPrimitive3DProgramTextured;
		}
		else
			pProgram = m_pPrimitive3DProgram;

		UseProgram(pProgram);

		SetState(pCommand->m_State, pProgram, true);
	}
	else
	{
		CCommandProcessorFragment_OpenGL::SetState(pCommand->m_State, true);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(pCommand->m_pVertices[0]), pCommand->m_pVertices);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(pCommand->m_pVertices[0]), (uint8_t *)pCommand->m_pVertices + (ptrdiff_t)(sizeof(vec2)));
	glTexCoordPointer(3, GL_FLOAT, sizeof(pCommand->m_pVertices[0]), (uint8_t *)pCommand->m_pVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(unsigned char) * 4));

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount * 4);
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, 0, pCommand->m_PrimCount * 3);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(m_HasShaders)
	{
		glUseProgram(0);
	}
}

void CCommandProcessorFragment_OpenGL2::Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	// create necessary space
	if((size_t)Index >= m_vBufferObjectIndices.size())
	{
		for(int i = m_vBufferObjectIndices.size(); i < Index + 1; ++i)
		{
			m_vBufferObjectIndices.emplace_back(0);
		}
	}

	GLuint VertBufferId = 0;

	glGenBuffers(1, &VertBufferId);
	glBindBuffer(GL_ARRAY_BUFFER, VertBufferId);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	SBufferObject &BufferObject = m_vBufferObjectIndices[Index];
	BufferObject.m_BufferObjectId = VertBufferId;
	BufferObject.m_DataSize = pCommand->m_DataSize;
	BufferObject.m_pData = static_cast<uint8_t *>(malloc(pCommand->m_DataSize));
	if(pUploadData)
		mem_copy(BufferObject.m_pData, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_vBufferObjectIndices[Index];

	glBindBuffer(GL_ARRAY_BUFFER, BufferObject.m_BufferObjectId);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	BufferObject.m_DataSize = pCommand->m_DataSize;
	free(BufferObject.m_pData);
	BufferObject.m_pData = static_cast<uint8_t *>(malloc(pCommand->m_DataSize));
	if(pUploadData)
		mem_copy(BufferObject.m_pData, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_vBufferObjectIndices[Index];

	glBindBuffer(GL_ARRAY_BUFFER, BufferObject.m_BufferObjectId);
	glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pUploadData);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if(pUploadData)
		mem_copy(BufferObject.m_pData + (ptrdiff_t)pCommand->m_pOffset, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand)
{
	int WriteIndex = pCommand->m_WriteBufferIndex;
	int ReadIndex = pCommand->m_ReadBufferIndex;

	SBufferObject &ReadBufferObject = m_vBufferObjectIndices[ReadIndex];
	SBufferObject &WriteBufferObject = m_vBufferObjectIndices[WriteIndex];

	mem_copy(WriteBufferObject.m_pData + (ptrdiff_t)pCommand->m_WriteOffset, ReadBufferObject.m_pData + (ptrdiff_t)pCommand->m_ReadOffset, pCommand->m_CopySize);

	glBindBuffer(GL_ARRAY_BUFFER, WriteBufferObject.m_BufferObjectId);
	glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(pCommand->m_WriteOffset), (GLsizeiptr)(pCommand->m_CopySize), WriteBufferObject.m_pData + (ptrdiff_t)pCommand->m_WriteOffset);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CCommandProcessorFragment_OpenGL2::Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_vBufferObjectIndices[Index];

	glDeleteBuffers(1, &BufferObject.m_BufferObjectId);

	free(BufferObject.m_pData);
	BufferObject.m_pData = NULL;
}

void CCommandProcessorFragment_OpenGL2::Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// create necessary space
	if((size_t)Index >= m_vBufferContainers.size())
	{
		for(int i = m_vBufferContainers.size(); i < Index + 1; ++i)
		{
			SBufferContainer Container;
			Container.m_ContainerInfo.m_Stride = 0;
			Container.m_ContainerInfo.m_VertBufferBindingIndex = -1;
			m_vBufferContainers.push_back(Container);
		}
	}

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];

	for(size_t i = 0; i < pCommand->m_AttrCount; ++i)
	{
		BufferContainer.m_ContainerInfo.m_vAttributes.push_back(pCommand->m_pAttributes[i]);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
	BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex = pCommand->m_VertBufferBindingIndex;
}

void CCommandProcessorFragment_OpenGL2::Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_vBufferContainers[pCommand->m_BufferContainerIndex];

	BufferContainer.m_ContainerInfo.m_vAttributes.clear();

	for(size_t i = 0; i < pCommand->m_AttrCount; ++i)
	{
		BufferContainer.m_ContainerInfo.m_vAttributes.push_back(pCommand->m_pAttributes[i]);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
	BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex = pCommand->m_VertBufferBindingIndex;
}

void CCommandProcessorFragment_OpenGL2::Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_vBufferContainers[pCommand->m_BufferContainerIndex];

	if(pCommand->m_DestroyAllBO)
	{
		int VertBufferId = BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex;
		if(VertBufferId != -1)
		{
			glDeleteBuffers(1, &m_vBufferObjectIndices[VertBufferId].m_BufferObjectId);

			free(m_vBufferObjectIndices[VertBufferId].m_pData);
			m_vBufferObjectIndices[VertBufferId].m_pData = NULL;
		}
	}

	BufferContainer.m_ContainerInfo.m_vAttributes.clear();
}

void CCommandProcessorFragment_OpenGL2::Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand)
{
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
		pProgram = m_pBorderTileProgramTextured;
	else
		pProgram = m_pBorderTileProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocScale, 1, (float *)&pCommand->m_Scale);

	bool IsTextured = BufferContainer.m_ContainerInfo.m_vAttributes.size() == 2;

	SBufferObject &BufferObject = m_vBufferObjectIndices[(size_t)BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex];

	glBindBuffer(GL_ARRAY_BUFFER, BufferObject.m_BufferObjectId);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, false, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_vAttributes[0].m_pOffset);
	if(IsTextured)
	{
		glEnableVertexAttribArray(1);
		glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_vAttributes[1].m_pOffset);
	}

	size_t RealDrawCount = pCommand->m_DrawNum * 4;
	GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pCommand->m_pIndicesOffset)) / (6 * sizeof(unsigned int))) * 4);
	glDrawArrays(GL_QUADS, RealOffset, RealDrawCount);

	glDisableVertexAttribArray(0);
	if(IsTextured)
		glDisableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	// if space not there return
	if((size_t)Index >= m_vBufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_vBufferContainers[Index];

	if(pCommand->m_IndicesDrawNum == 0)
	{
		return; // nothing to draw
	}

	CGLSLTileProgram *pProgram = NULL;
	if(IsTexturedState(pCommand->m_State))
	{
		pProgram = m_pTileProgramTextured;
	}
	else
		pProgram = m_pTileProgram;

	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	bool IsTextured = BufferContainer.m_ContainerInfo.m_vAttributes.size() == 2;

	SBufferObject &BufferObject = m_vBufferObjectIndices[(size_t)BufferContainer.m_ContainerInfo.m_VertBufferBindingIndex];

	glBindBuffer(GL_ARRAY_BUFFER, BufferObject.m_BufferObjectId);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, false, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_vAttributes[0].m_pOffset);
	if(IsTextured)
	{
		glEnableVertexAttribArray(1);
		glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_vAttributes[1].m_pOffset);
	}

	for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
	{
		size_t RealDrawCount = (pCommand->m_pDrawCount[i] / 6) * 4;
		GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pCommand->m_pIndicesOffsets[i])) / (6 * sizeof(unsigned int))) * 4);
		glDrawArrays(GL_QUADS, RealOffset, RealDrawCount);
	}

	glDisableVertexAttribArray(0);
	if(IsTextured)
		glDisableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

#undef BACKEND_GL_MODERN_API

#endif

#endif
