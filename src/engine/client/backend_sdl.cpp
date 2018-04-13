#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
	// For FlashWindowEx, FLASHWINFO, FLASHW_TRAY
	#define _WIN32_WINNT 0x0501
	#define WINVER 0x0501
#endif

#include <GL/glew.h>
#include <engine/storage.h>

#include <base/detect.h>
#include <base/math.h>
#include <stdlib.h>
#include <cmath>
#include "SDL.h"
#include "SDL_syswm.h"
#if defined(__ANDROID__)
	#define GL_GLEXT_PROTOTYPES
	#include <GLES/gl.h>
	#include <GLES/glext.h>
	#include <GL/glu.h>
	#define glOrtho glOrthof
#else

	#if defined(CONF_PLATFORM_MACOSX)
	#include "OpenGL/glu.h"
	#else
	#include "SDL_opengl.h"
	#include "GL/glu.h"
	#endif
#endif

#if defined(SDL_VIDEO_DRIVER_X11)
	#include <X11/Xutil.h>
	#include <X11/Xlib.h>
#endif

#include <engine/shared/config.h>
#include <base/tl/threading.h>

#include "graphics_threaded.h"
#include "backend_sdl.h"

#include "opengl_sl_program.h"
#include "opengl_sl.h"

#ifdef __MINGW32__
extern "C"
{
	int putenv(const char *);
}
#endif

// ------------ CGraphicsBackend_Threaded

void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	CGraphicsBackend_Threaded *pThis = (CGraphicsBackend_Threaded *)pUser;

	while(!pThis->m_Shutdown)
	{
		pThis->m_Activity.wait();
		if(pThis->m_pBuffer)
		{
			#ifdef CONF_PLATFORM_MACOSX
				CAutoreleasePool AutoreleasePool;
			#endif
			pThis->m_pProcessor->RunBuffer(pThis->m_pBuffer);
			sync_barrier();
			pThis->m_pBuffer = 0x0;
			pThis->m_BufferDone.signal();
		}
	}
}

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded()
{
	m_pBuffer = 0x0;
	m_pProcessor = 0x0;
	m_pThread = 0x0;
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	m_Shutdown = false;
	m_pProcessor = pProcessor;
	m_pThread = thread_init(ThreadFunc, this);
	m_BufferDone.signal();
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	m_Activity.signal();
	thread_wait(m_pThread);
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	WaitForIdle();
	m_pBuffer = pBuffer;
	m_Activity.signal();
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
	return m_pBuffer == 0x0;
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
	while(m_pBuffer != 0x0)
		m_BufferDone.wait();
}


// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand)
{
	pCommand->m_pSemaphore->signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::SCommand  *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_NOP: break;
	case CCommandBuffer::CMD_SIGNAL: Cmd_Signal(static_cast<const CCommandBuffer::SCommand_Signal *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_OpenGL

int CCommandProcessorFragment_OpenGL::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB) return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA) return GL_ALPHA;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA) return GL_RGBA;
	return GL_RGBA;
}

unsigned char CCommandProcessorFragment_OpenGL::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

void *CCommandProcessorFragment_OpenGL::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{

	unsigned char *pTmpData;
	int ScaleW = Width / NewWidth;
	int ScaleH = Height / NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;
	else if(Format == CCommandBuffer::TEXFORMAT_ALPHA)
		Bpp = 1;

	pTmpData = (unsigned char *)malloc(NewWidth * NewHeight * Bpp);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			for(int i = 0; i < Bpp; ++i) {
				pTmpData[c*Bpp + i] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, i, ScaleW, ScaleH, Bpp);
			}
		}

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::SState &State)
{
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

	// clip
	if(State.m_ClipEnable)
	{
		glScissor(State.m_ClipX, State.m_ClipY, State.m_ClipW, State.m_ClipH);
		glEnable(GL_SCISSOR_TEST);
	}
	else
		glDisable(GL_SCISSOR_TEST);

	// texture
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
	}
	else
		glDisable(GL_TEXTURE_2D);

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

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, -10.0f, 10.f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_MaxTexSize = -1;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	void* pTexData = pCommand->m_pData;
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int X = pCommand->m_X;
	int Y = pCommand->m_Y;
	if(m_aTextures[pCommand->m_Slot].m_RescaleCount > 0)
	{
		for(int i = 0; i < m_aTextures[pCommand->m_Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height,
		TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	m_aTextures[pCommand->m_Slot].m_Tex = 0;
	*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	if(m_MaxTexSize == -1)
	{
		// fix the alignment to allow even 1byte changes, e.g. for alpha components
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
	}

	int RescaleCount = 0;
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB || pCommand->m_Format == CCommandBuffer::TEXFORMAT_ALPHA)
	{
		if(Width > m_MaxTexSize || Height > m_MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
				++RescaleCount;
			} while(Width > m_MaxTexSize || Height > m_MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
		else if(pCommand->m_Format != CCommandBuffer::TEXFORMAT_ALPHA && (Width > 16 && Height > 16 && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0))
		{
			Width >>= 1;
			Height >>= 1;
			++RescaleCount;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_aTextures[pCommand->m_Slot].m_Width = Width;
	m_aTextures[pCommand->m_Slot].m_Height = Height;
	m_aTextures[pCommand->m_Slot].m_RescaleCount = RescaleCount;

	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

#if defined(__ANDROID__)
	StoreOglformat = Oglformat;
#else
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
			case GL_RGB: StoreOglformat = GL_COMPRESSED_RGB_ARB; break;
			case GL_ALPHA: StoreOglformat = GL_COMPRESSED_ALPHA_ARB; break;
			case GL_RGBA: StoreOglformat = GL_COMPRESSED_RGBA_ARB; break;
			default: StoreOglformat = GL_COMPRESSED_RGBA_ARB;
		}
	}
#endif
	glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, StoreOglformat, Width, Height, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}
	
	// calculate memory usage
	m_aTextures[pCommand->m_Slot].m_MemSize = Width*Height*pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width>>=1;
		Height>>=1;
		m_aTextures[pCommand->m_Slot].m_MemSize += Width*Height*pCommand->m_PixelSize;
	}
	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;

	free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	SetState(pCommand->m_State);

	glVertexPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices + sizeof(float)*2);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices + sizeof(float)*4);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
#if defined(__ANDROID__)
		for( unsigned i = 0, j = pCommand->m_PrimCount; i < j; i++ )
			glDrawArrays(GL_TRIANGLE_FAN, i*4, 4);
#else
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount*4);
#endif
		break;
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount*2);
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, 0, pCommand->m_PrimCount*3);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0,0,0,0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)malloc(w*(h+1)*3);
	unsigned char *pTempRow = pPixelData+w*h*3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0,0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h/2; y++)
	{
		mem_copy(pTempRow, pPixelData+y*w*3, w*3);
		mem_copy(pPixelData+y*w*3, pPixelData+(h-y-1)*w*3, w*3);
		mem_copy(pPixelData+(h-y-1)*w*3, pTempRow,w*3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL::CCommandProcessorFragment_OpenGL()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	m_pTextureMemoryUsage = 0;
}

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::SCommand  *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CMD_INIT: Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_CREATE: Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY: Cmd_Texture_Destroy(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_UPDATE: Cmd_Texture_Update(static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CLEAR: Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER: Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SCREENSHOT: Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_Screenshot *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_OpenGL3_3

int CCommandProcessorFragment_OpenGL3_3::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB) return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA) return GL_RED;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA) return GL_RGBA;
	return GL_RGBA;
}

unsigned char CCommandProcessorFragment_OpenGL3_3::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

void *CCommandProcessorFragment_OpenGL3_3::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;
	int ScaleW = Width/NewWidth;
	int ScaleH = Height/NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;
	else if(Format == CCommandBuffer::TEXFORMAT_ALPHA)
		Bpp = 1;

	pTmpData = (unsigned char *)malloc(NewWidth*NewHeight*Bpp);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			for(int i = 0; i < Bpp; ++i) {
				pTmpData[c*Bpp + i] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, i, ScaleW, ScaleH, Bpp);
			}
		}

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL3_3::SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram)
{
	if(State.m_BlendMode != m_LastBlendMode && State.m_BlendMode != CCommandBuffer::BLEND_NONE)
	{
		// blend
		switch(State.m_BlendMode)
		{
		case CCommandBuffer::BLEND_NONE:
			// We don't really need this anymore
			//glDisable(GL_BLEND);
			break;
		case CCommandBuffer::BLEND_ALPHA:
			//glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case CCommandBuffer::BLEND_ADDITIVE:
			//glEnable(GL_BLEND);
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

	// texture
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		int Slot = State.m_Texture % m_MaxTextureUnits;
		
		if(m_UseMultipleTextureUnits)
		{
			if(!IsAndUpdateTextureSlotBound(Slot, State.m_Texture))
			{
				glActiveTexture(GL_TEXTURE0 + Slot);
				glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
				glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler);
			}
		} else
		{
			Slot = 0;
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
			glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler);
		}
		if(pProgram->m_LocIsTextured != -1)
		{
			if(pProgram->m_LastIsTextured != 1)
			{
				pProgram->SetUniform(pProgram->m_LocIsTextured, (int)1);
				pProgram->m_LastIsTextured = 1;
			}
		}

		if(pProgram->m_LastTextureSampler != Slot)
		{
			pProgram->SetUniform(pProgram->m_LocTextureSampler, (int)Slot);
			pProgram->m_LastTextureSampler = Slot;
		}

		if(m_aTextures[State.m_Texture].m_LastWrapMode != State.m_WrapMode)
		{
			switch (State.m_WrapMode)
			{
			case CCommandBuffer::WRAP_REPEAT:
				glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
				break;
			case CCommandBuffer::WRAP_CLAMP:
				glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				break;
			default:
				dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapMode);
			};
			m_aTextures[State.m_Texture].m_LastWrapMode = State.m_WrapMode;
		}
	}
	else 
	{
		if(pProgram->m_LocIsTextured != -1)
		{
			if(pProgram->m_LastIsTextured != 0)
			{
				pProgram->SetUniform(pProgram->m_LocIsTextured, (int)0);
				pProgram->m_LastIsTextured = 0;
			}
		}
	}

	if(pProgram->m_LastScreen[0] != State.m_ScreenTL.x || pProgram->m_LastScreen[1] != State.m_ScreenTL.y || pProgram->m_LastScreen[2] != State.m_ScreenBR.x || pProgram->m_LastScreen[3] != State.m_ScreenBR.y)
	{
		pProgram->m_LastScreen[0] = State.m_ScreenTL.x;
		pProgram->m_LastScreen[1] = State.m_ScreenTL.y;
		pProgram->m_LastScreen[2] = State.m_ScreenBR.x;
		pProgram->m_LastScreen[3] = State.m_ScreenBR.y;
		// screen mapping
		// orthographic projection matrix
		// the z coordinate is the same for every vertex, so just ignore the z coordinate and set it in the shaders
		float m[2 * 4] = {
			2.f / (State.m_ScreenBR.x - State.m_ScreenTL.x), 0, 0, -((State.m_ScreenBR.x + State.m_ScreenTL.x) / (State.m_ScreenBR.x - State.m_ScreenTL.x)),
			0, (2.f / (State.m_ScreenTL.y - State.m_ScreenBR.y)), 0, -((State.m_ScreenTL.y + State.m_ScreenBR.y) / (State.m_ScreenTL.y - State.m_ScreenBR.y)),
			//0, 0, -(2.f/(9.f)), -((11.f)/(9.f)),
			//0, 0, 0, 1.0f
		};

		// transpose bcs of column-major order of opengl
		glUniformMatrix4x2fv(pProgram->m_LocPos, 1, true, (float*)&m);
	}
}

void CCommandProcessorFragment_OpenGL3_3::UseProgram(CGLSLTWProgram *pProgram)
{
	if(m_LastProgramID != pProgram->GetProgramID()) {
		pProgram->UseProgram();
		m_LastProgramID = pProgram->GetProgramID();
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Init(const SCommand_Init *pCommand)
{
	m_UseMultipleTextureUnits = g_Config.m_GfxEnableTextureUnitOptimization;
	if(!m_UseMultipleTextureUnits)
	{
		glActiveTexture(GL_TEXTURE0);
	}
	
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_LastBlendMode = CCommandBuffer::BLEND_ALPHA;
	m_LastClipEnable = false;
	m_pPrimitiveProgram = new CGLSLPrimitiveProgram;
	m_pTileProgram = new CGLSLTileProgram;
	m_pTileProgramTextured = new CGLSLTileProgram;
	m_pBorderTileProgram = new CGLSLBorderTileProgram;
	m_pBorderTileProgramTextured = new CGLSLBorderTileProgram;
	m_pBorderTileLineProgram = new CGLSLBorderTileLineProgram;
	m_pBorderTileLineProgramTextured = new CGLSLBorderTileLineProgram;
	m_pQuadProgram = new CGLSLQuadProgram;
	m_pQuadProgramTextured = new CGLSLQuadProgram;
	m_pTextProgram = new CGLSLTextProgram;
	m_pSpriteProgram = new CGLSLSpriteProgram;
	m_pSpriteProgramMultiple = new CGLSLSpriteMultipleProgram;
	
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(pCommand->m_pStorage, "shader/prim.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(pCommand->m_pStorage, "shader/prim.frag", GL_FRAGMENT_SHADER);
		
		m_pPrimitiveProgram->CreateProgram();
		m_pPrimitiveProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveProgram->LinkProgram();
		
		UseProgram(m_pPrimitiveProgram);
		
		m_pPrimitiveProgram->m_LocPos = m_pPrimitiveProgram->GetUniformLoc("Pos");
		m_pPrimitiveProgram->m_LocIsTextured = m_pPrimitiveProgram->GetUniformLoc("isTextured");
		m_pPrimitiveProgram->m_LocTextureSampler = m_pPrimitiveProgram->GetUniformLoc("textureSampler");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		
		m_pTileProgram->CreateProgram();
		m_pTileProgram->AddShader(&VertexShader);
		m_pTileProgram->AddShader(&FragmentShader);
		m_pTileProgram->LinkProgram();
		
		UseProgram(m_pTileProgram);
		
		m_pTileProgram->m_LocPos = m_pTileProgram->GetUniformLoc("Pos");
		m_pTileProgram->m_LocIsTextured = -1;
		m_pTileProgram->m_LocTextureSampler = -1;
		m_pTileProgram->m_LocColor = m_pTileProgram->GetUniformLoc("vertColor");
		m_pTileProgram->m_LocLOD = -1;
		m_pTileProgram->m_LocTexelOffset = -1;
		m_pTileProgram->m_LastLOD = -1;
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/tiletex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/tiletex.frag", GL_FRAGMENT_SHADER);
		
		m_pTileProgramTextured->CreateProgram();
		m_pTileProgramTextured->AddShader(&VertexShader);
		m_pTileProgramTextured->AddShader(&FragmentShader);
		m_pTileProgramTextured->LinkProgram();
		
		UseProgram(m_pTileProgramTextured);
		
		m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("Pos");
		m_pTileProgramTextured->m_LocIsTextured = -1;
		m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("textureSampler");
		m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("vertColor");
		m_pTileProgramTextured->m_LocLOD = m_pTileProgramTextured->GetUniformLoc("LOD");
		m_pTileProgramTextured->m_LocTexelOffset = m_pTileProgramTextured->GetUniformLoc("TexelOffset");
		m_pTileProgramTextured->m_LastLOD = -1;
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/bordertile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/bordertile.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileProgram->CreateProgram();
		m_pBorderTileProgram->AddShader(&VertexShader);
		m_pBorderTileProgram->AddShader(&FragmentShader);
		m_pBorderTileProgram->LinkProgram();
		
		UseProgram(m_pBorderTileProgram);
		
		m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("Pos");
		m_pBorderTileProgram->m_LocIsTextured = -1;
		m_pBorderTileProgram->m_LocTextureSampler = -1;
		m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("vertColor");
		m_pBorderTileProgram->m_LocLOD = -1;
		m_pBorderTileProgram->m_LocTexelOffset = -1;
		m_pBorderTileProgram->m_LastLOD = -1;
		m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("Offset");
		m_pBorderTileProgram->m_LocDir = m_pBorderTileProgram->GetUniformLoc("Dir");
		m_pBorderTileProgram->m_LocJumpIndex = m_pBorderTileProgram->GetUniformLoc("JumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/bordertiletex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/bordertiletex.frag", GL_FRAGMENT_SHADER);

		m_pBorderTileProgramTextured->CreateProgram();
		m_pBorderTileProgramTextured->AddShader(&VertexShader);
		m_pBorderTileProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileProgramTextured->LinkProgram();
		
		UseProgram(m_pBorderTileProgramTextured);
		
		m_pBorderTileProgramTextured->m_LocPos = m_pBorderTileProgramTextured->GetUniformLoc("Pos");
		m_pBorderTileProgramTextured->m_LocIsTextured = -1;
		m_pBorderTileProgramTextured->m_LocTextureSampler = m_pBorderTileProgramTextured->GetUniformLoc("textureSampler");
		m_pBorderTileProgramTextured->m_LocColor = m_pBorderTileProgramTextured->GetUniformLoc("vertColor");
		m_pBorderTileProgramTextured->m_LocLOD = m_pBorderTileProgramTextured->GetUniformLoc("LOD");
		m_pBorderTileProgramTextured->m_LocTexelOffset = m_pBorderTileProgramTextured->GetUniformLoc("TexelOffset");
		m_pBorderTileProgramTextured->m_LastLOD = -1;
		m_pBorderTileProgramTextured->m_LocOffset = m_pBorderTileProgramTextured->GetUniformLoc("Offset");
		m_pBorderTileProgramTextured->m_LocDir = m_pBorderTileProgramTextured->GetUniformLoc("Dir");
		m_pBorderTileProgramTextured->m_LocJumpIndex = m_pBorderTileProgramTextured->GetUniformLoc("JumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/bordertileline.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/bordertileline.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileLineProgram->CreateProgram();
		m_pBorderTileLineProgram->AddShader(&VertexShader);
		m_pBorderTileLineProgram->AddShader(&FragmentShader);
		m_pBorderTileLineProgram->LinkProgram();
		
		UseProgram(m_pBorderTileLineProgram);
		
		m_pBorderTileLineProgram->m_LocPos = m_pBorderTileLineProgram->GetUniformLoc("Pos");
		m_pBorderTileLineProgram->m_LocIsTextured = -1;
		m_pBorderTileLineProgram->m_LocTextureSampler = -1;
		m_pBorderTileLineProgram->m_LocColor = m_pBorderTileLineProgram->GetUniformLoc("vertColor");
		m_pBorderTileLineProgram->m_LocLOD = -1;
		m_pBorderTileLineProgram->m_LocTexelOffset = -1;
		m_pBorderTileLineProgram->m_LastLOD = -1;
		m_pBorderTileLineProgram->m_LocDir = m_pBorderTileLineProgram->GetUniformLoc("Dir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/bordertilelinetex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/bordertilelinetex.frag", GL_FRAGMENT_SHADER);

		m_pBorderTileLineProgramTextured->CreateProgram();
		m_pBorderTileLineProgramTextured->AddShader(&VertexShader);
		m_pBorderTileLineProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileLineProgramTextured->LinkProgram();

		UseProgram(m_pBorderTileLineProgramTextured);

		m_pBorderTileLineProgramTextured->m_LocPos = m_pBorderTileLineProgramTextured->GetUniformLoc("Pos");
		m_pBorderTileLineProgramTextured->m_LocIsTextured = -1;
		m_pBorderTileLineProgramTextured->m_LocTextureSampler = m_pBorderTileLineProgramTextured->GetUniformLoc("textureSampler");
		m_pBorderTileLineProgramTextured->m_LocColor = m_pBorderTileLineProgramTextured->GetUniformLoc("vertColor");
		m_pBorderTileLineProgramTextured->m_LocLOD = m_pBorderTileLineProgramTextured->GetUniformLoc("LOD");
		m_pBorderTileLineProgramTextured->m_LocTexelOffset = m_pBorderTileLineProgramTextured->GetUniformLoc("TexelOffset");
		m_pBorderTileLineProgramTextured->m_LastLOD = -1;
		m_pBorderTileLineProgramTextured->m_LocDir = m_pBorderTileLineProgramTextured->GetUniformLoc("Dir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/quad.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/quad.frag", GL_FRAGMENT_SHADER);

		m_pQuadProgram->CreateProgram();
		m_pQuadProgram->AddShader(&VertexShader);
		m_pQuadProgram->AddShader(&FragmentShader);
		m_pQuadProgram->LinkProgram();

		UseProgram(m_pQuadProgram);

		m_pQuadProgram->m_LocPos = m_pQuadProgram->GetUniformLoc("Pos");
		m_pQuadProgram->m_LocIsTextured = -1;
		m_pQuadProgram->m_LocTextureSampler = -1;
		m_pQuadProgram->m_LocColor = m_pQuadProgram->GetUniformLoc("vertColor");
		m_pQuadProgram->m_LocRotation = m_pQuadProgram->GetUniformLoc("Rotation");
		m_pQuadProgram->m_LocOffset = m_pQuadProgram->GetUniformLoc("Offset");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/quadtex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/quadtex.frag", GL_FRAGMENT_SHADER);

		m_pQuadProgramTextured->CreateProgram();
		m_pQuadProgramTextured->AddShader(&VertexShader);
		m_pQuadProgramTextured->AddShader(&FragmentShader);
		m_pQuadProgramTextured->LinkProgram();

		UseProgram(m_pQuadProgramTextured);

		m_pQuadProgramTextured->m_LocPos = m_pQuadProgramTextured->GetUniformLoc("Pos");
		m_pQuadProgramTextured->m_LocIsTextured = -1;
		m_pQuadProgramTextured->m_LocTextureSampler = m_pQuadProgramTextured->GetUniformLoc("textureSampler");
		m_pQuadProgramTextured->m_LocColor = m_pQuadProgramTextured->GetUniformLoc("vertColor");
		m_pQuadProgramTextured->m_LocRotation = m_pQuadProgramTextured->GetUniformLoc("Rotation");
		m_pQuadProgramTextured->m_LocOffset = m_pQuadProgramTextured->GetUniformLoc("Offset");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(pCommand->m_pStorage, "shader/text.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(pCommand->m_pStorage, "shader/text.frag", GL_FRAGMENT_SHADER);

		m_pTextProgram->CreateProgram();
		m_pTextProgram->AddShader(&VertexShader);
		m_pTextProgram->AddShader(&FragmentShader);
		m_pTextProgram->LinkProgram();

		UseProgram(m_pTextProgram);

		m_pTextProgram->m_LocPos = m_pTextProgram->GetUniformLoc("Pos");
		m_pTextProgram->m_LocIsTextured = -1;
		m_pTextProgram->m_LocTextureSampler = -1;
		m_pTextProgram->m_LocTextSampler = m_pTextProgram->GetUniformLoc("textSampler");
		m_pTextProgram->m_LocTextOutlineSampler = m_pTextProgram->GetUniformLoc("textOutlineSampler");
		m_pTextProgram->m_LocColor = m_pTextProgram->GetUniformLoc("vertColor");
		m_pTextProgram->m_LocOutlineColor = m_pTextProgram->GetUniformLoc("vertOutlineColor");
		m_pTextProgram->m_LocTextureSize = m_pTextProgram->GetUniformLoc("textureSize");
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(pCommand->m_pStorage, "shader/sprite.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(pCommand->m_pStorage, "shader/sprite.frag", GL_FRAGMENT_SHADER);

		m_pSpriteProgram->CreateProgram();
		m_pSpriteProgram->AddShader(&PrimitiveVertexShader);
		m_pSpriteProgram->AddShader(&PrimitiveFragmentShader);
		m_pSpriteProgram->LinkProgram();

		UseProgram(m_pSpriteProgram);

		m_pSpriteProgram->m_LocPos = m_pSpriteProgram->GetUniformLoc("Pos");
		m_pSpriteProgram->m_LocIsTextured = -1;
		m_pSpriteProgram->m_LocTextureSampler = m_pSpriteProgram->GetUniformLoc("textureSampler");
		m_pSpriteProgram->m_LocRotation = m_pSpriteProgram->GetUniformLoc("Rotation");
		m_pSpriteProgram->m_LocCenter = m_pSpriteProgram->GetUniformLoc("Center");
		m_pSpriteProgram->m_LocVertciesColor = m_pSpriteProgram->GetUniformLoc("VerticesColor");

		m_pSpriteProgram->SetUniform(m_pSpriteProgram->m_LocRotation, 0);
		float Center[2] = { 0.f, 0.f };
		m_pSpriteProgram->SetUniformVec2(m_pSpriteProgram->m_LocCenter, 1, Center);
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(pCommand->m_pStorage, "shader/spritemulti.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(pCommand->m_pStorage, "shader/spritemulti.frag", GL_FRAGMENT_SHADER);

		m_pSpriteProgramMultiple->CreateProgram();
		m_pSpriteProgramMultiple->AddShader(&PrimitiveVertexShader);
		m_pSpriteProgramMultiple->AddShader(&PrimitiveFragmentShader);
		m_pSpriteProgramMultiple->LinkProgram();

		UseProgram(m_pSpriteProgramMultiple);

		m_pSpriteProgramMultiple->m_LocPos = m_pSpriteProgramMultiple->GetUniformLoc("Pos");
		m_pSpriteProgramMultiple->m_LocIsTextured = -1;
		m_pSpriteProgramMultiple->m_LocTextureSampler = m_pSpriteProgramMultiple->GetUniformLoc("textureSampler");
		m_pSpriteProgramMultiple->m_LocRSP = m_pSpriteProgramMultiple->GetUniformLoc("RSP[0]");
		m_pSpriteProgramMultiple->m_LocCenter = m_pSpriteProgramMultiple->GetUniformLoc("Center");
		m_pSpriteProgramMultiple->m_LocVertciesColor = m_pSpriteProgramMultiple->GetUniformLoc("VerticesColor");

		float Center[2] = { 0.f, 0.f };
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, Center);
	}

	m_LastStreamBuffer = 0;

	glGenBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glGenVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);

	m_UsePreinitializedVertexBuffer = g_Config.m_GfxUsePreinitBuffer;
	
	for(int i = 0; i < MAX_STREAM_BUFFER_COUNT; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[i]);
		glBindVertexArray(m_PrimitiveDrawVertexID[i]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void*)(sizeof(float) * 2));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertex), (void*)(sizeof(float) * 4));

		if(m_UsePreinitializedVertexBuffer)
			glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertex) * CCommandBuffer::MAX_VERTICES, NULL, GL_STREAM_DRAW);

		m_LastIndexBufferBound[i] = 0;
	}
	
	//query the image max size only once
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);

	//query maximum of allowed textures
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_MaxTextureUnits);
	m_TextureSlotBoundToUnit.resize(m_MaxTextureUnits);
	for(int i = 0; i < m_MaxTextureUnits; ++i)
	{
		m_TextureSlotBoundToUnit[i].m_TextureSlot = -1;
	}

	glBindVertexArray(0);
	glGenBuffers(1, &m_QuadDrawIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, m_QuadDrawIndexBufferID);
	
	unsigned int Indices[CCommandBuffer::MAX_VERTICES/4 * 6];
	int Primq = 0;
	for(int i = 0; i < CCommandBuffer::MAX_VERTICES/4 * 6; i+=6)
	{
		Indices[i] = Primq;
		Indices[i+1] = Primq + 1;
		Indices[i+2] = Primq + 2;
		Indices[i+3] = Primq;
		Indices[i+4] = Primq + 2;
		Indices[i+5] = Primq + 3;
		Primq+=4;
	}
	glBufferData(GL_COPY_WRITE_BUFFER, sizeof(unsigned int) * CCommandBuffer::MAX_VERTICES/4 * 6, Indices, GL_STATIC_DRAW);
	
	m_CurrentIndicesInBuffer = CCommandBuffer::MAX_VERTICES/4 * 6;
	
	mem_zero(m_aTextures, sizeof(m_aTextures));
	
	m_ClearColor.r = m_ClearColor.g = m_ClearColor.b = -1.f;
	
	// fix the alignment to allow even 1byte changes, e.g. for alpha components
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	//clean up everything	
	delete m_pPrimitiveProgram;
	//delete m_QuadProgram;
	delete m_pTileProgram;
	delete m_pTileProgramTextured;
	delete m_pBorderTileProgram;
	delete m_pBorderTileProgramTextured;
	delete m_pBorderTileLineProgram;
	delete m_pBorderTileLineProgramTextured;

	glBindVertexArray(0);
	glDeleteBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glDeleteBuffers(1, &m_QuadDrawIndexBufferID);
	glDeleteVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);
	
	for(int i = 0; i < CCommandBuffer::MAX_TEXTURES; ++i)
	{
		DestroyTexture(i);
	}
	
	for(size_t i = 0; i < m_BufferContainers.size(); ++i)
	{
		DestroyBufferContainer(i);
	}

	m_BufferContainers.clear();	
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	if(m_UseMultipleTextureUnits)
	{
		int Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
		glBindSampler(Slot, m_aTextures[pCommand->m_Slot].m_Sampler);
	}

	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	void* pTexData = pCommand->m_pData;
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int X = pCommand->m_X;
	int Y = pCommand->m_Y;
	if(m_aTextures[pCommand->m_Slot].m_RescaleCount > 0)
	{
		for(int i = 0; i < m_aTextures[pCommand->m_Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height,
		TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	int Slot = 0;
	if(m_UseMultipleTextureUnits)
	{
		Slot = pCommand->m_Slot % m_MaxTextureUnits;
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindSampler(Slot, 0);
	m_TextureSlotBoundToUnit[Slot].m_TextureSlot = -1;
	DestroyTexture(pCommand->m_Slot);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	int RescaleCount = 0;
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB || pCommand->m_Format == CCommandBuffer::TEXFORMAT_ALPHA)
	{
		if(Width > m_MaxTexSize || Height > m_MaxTexSize)
		{
			do
			{
				Width>>=1;
				Height>>=1;
				++RescaleCount;
			}
			while(Width > m_MaxTexSize || Height > m_MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
		else if(pCommand->m_Format != CCommandBuffer::TEXFORMAT_ALPHA && (Width > 16 && Height > 16 && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0))
		{
			Width>>=1;
			Height>>=1;
			++RescaleCount;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_aTextures[pCommand->m_Slot].m_Width = Width;
	m_aTextures[pCommand->m_Slot].m_Height = Height; 
	m_aTextures[pCommand->m_Slot].m_RescaleCount = RescaleCount;

	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

#if defined(__ANDROID__)
	StoreOglformat = Oglformat;
#else
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
			case GL_RGB: StoreOglformat = GL_COMPRESSED_RGB; break;
			// COMPRESSED_ALPHA is deprecated, so use different single channel format.
			case GL_RED: StoreOglformat = GL_COMPRESSED_RED; break;
			case GL_RGBA: StoreOglformat = GL_COMPRESSED_RGBA; break;
			default: StoreOglformat = GL_COMPRESSED_RGBA;
		}
	}
#endif
	int Slot = 0;
	if(m_UseMultipleTextureUnits)
	{
		Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
	}
	glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	glGenSamplers(1, &m_aTextures[pCommand->m_Slot].m_Sampler);
	glBindSampler(Slot, m_aTextures[pCommand->m_Slot].m_Sampler);
	
	if(Oglformat == GL_RED)
	{
		//Bind the texture 2D.
		GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
		StoreOglformat = GL_R8;
	}
	
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}
	else
	{
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		//prevent mipmap display bugs, when zooming out far
		if(Width >= 1024 && Height >= 1024)
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5.f);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5);
		}
		glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	
	// This is the initial value for the wrap modes
	m_aTextures[pCommand->m_Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_aTextures[pCommand->m_Slot].m_MemSize = Width*Height*pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width>>=1;
		Height>>=1;
		m_aTextures[pCommand->m_Slot].m_MemSize += Width*Height*pCommand->m_PixelSize;
	}
	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;
	
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	if(pCommand->m_Color.r != m_ClearColor.r || pCommand->m_Color.g != m_ClearColor.g || pCommand->m_Color.b != m_ClearColor.b)
	{
		glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
		m_ClearColor = pCommand->m_Color;
	}
	glClear(GL_COLOR_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL3_3::UploadStreamBufferData(unsigned int PrimitiveType, const void* pVertices, unsigned int PrimitiveCount)
{
	int Count = 0;
	switch (PrimitiveType)
	{
	case CCommandBuffer::PRIMTYPE_LINES:
		Count = PrimitiveCount * 2;
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		Count = PrimitiveCount * 4;
		break;
	default:
		return;
	};

	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[m_LastStreamBuffer]);

	if(!m_UsePreinitializedVertexBuffer)
		glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertex) * Count, (const void*)pVertices, GL_STREAM_DRAW);
	else
	{
		// This is better for some iGPUs. Probably due to not initializing a new buffer in the system memory again and again...(driver dependend)
		void *pData = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(CCommandBuffer::SVertex) * Count, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		mem_copy(pData, pVertices, sizeof(CCommandBuffer::SVertex) * Count);

		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	UseProgram(m_pPrimitiveProgram);
	SetState(pCommand->m_State, m_pPrimitiveProgram);
	
	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, pCommand->m_PrimCount);

	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount*2);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
			m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
		}
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount*6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};

	m_LastStreamBuffer = (m_LastStreamBuffer + 1 >= MAX_STREAM_BUFFER_COUNT ? 0 : m_LastStreamBuffer + 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0,0,0,0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)malloc(w*(h+1)*3);
	unsigned char *pTempRow = pPixelData+w*h*3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0,0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h/2; y++)
	{
		mem_copy(pTempRow, pPixelData+y*w*3, w*3);
		mem_copy(pPixelData+y*w*3, pPixelData+(h-y-1)*w*3, w*3);
		mem_copy(pPixelData+(h-y-1)*w*3, pTempRow,w*3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL3_3::CCommandProcessorFragment_OpenGL3_3()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	m_pTextureMemoryUsage = 0;
}

bool CCommandProcessorFragment_OpenGL3_3::RunCommand(const CCommandBuffer::SCommand  *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CMD_INIT: Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand)); break;
	case CMD_SHUTDOWN: Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_CREATE: Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY: Cmd_Texture_Destroy(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_UPDATE: Cmd_Texture_Update(static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CLEAR: Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER: Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SCREENSHOT: Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_Screenshot *>(pBaseCommand)); break;

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
	case CCommandBuffer::CMD_RENDER_BORDER_TILE_LINE: Cmd_RenderBorderTileLine(static_cast<const CCommandBuffer::SCommand_RenderBorderTileLine *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_LAYER: Cmd_RenderQuadLayer(static_cast<const CCommandBuffer::SCommand_RenderQuadLayer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_TEXT: Cmd_RenderText(static_cast<const CCommandBuffer::SCommand_RenderText *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_TEXT_STREAM: Cmd_RenderTextStream(static_cast<const CCommandBuffer::SCommand_RenderTextStream *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER: Cmd_RenderQuadContainer(static_cast<const CCommandBuffer::SCommand_RenderQuadContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_SPRITE: Cmd_RenderQuadContainerAsSprite(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerAsSprite *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE: Cmd_RenderQuadContainerAsSpriteMultiple(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

bool CCommandProcessorFragment_OpenGL3_3::IsAndUpdateTextureSlotBound(int IDX, int Slot)
{
	if(m_TextureSlotBoundToUnit[IDX].m_TextureSlot == Slot) return true;
	else
	{
		//the texture slot uses this index now
		m_TextureSlotBoundToUnit[IDX].m_TextureSlot = Slot;
		return false;
	}
}

void CCommandProcessorFragment_OpenGL3_3::DestroyTexture(int Slot)
{
	glDeleteTextures(1, &m_aTextures[Slot].m_Tex);
	glDeleteSamplers(1, &m_aTextures[Slot].m_Sampler);
	*m_pTextureMemoryUsage -= m_aTextures[Slot].m_MemSize;
	
	m_aTextures[Slot].m_Tex = 0;
	m_aTextures[Slot].m_Sampler = 0;
}

void CCommandProcessorFragment_OpenGL3_3::DestroyBufferContainer(int Index, bool DeleteBOs)
{
	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID != 0)
		glDeleteVertexArrays(1, &BufferContainer.m_VertArrayID);

	// all buffer objects can deleted automatically, so the program doesn't need to deal with them (e.g. causing crashes because of driver bugs)
	if(DeleteBOs)
	{
		for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i)
		{
			int VertBufferID = BufferContainer.m_ContainerInfo.m_Attributes[i].m_VertBufferBindingIndex;
			if(VertBufferID != -1)
			{
				for(size_t j = 0; j < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++j)
				{
					// set all equal ids to zero to not double delete
					if(VertBufferID == BufferContainer.m_ContainerInfo.m_Attributes[j].m_VertBufferBindingIndex) {
						BufferContainer.m_ContainerInfo.m_Attributes[j].m_VertBufferBindingIndex = -1;
					}
				}

				glDeleteBuffers(1, &m_BufferObjectIndices[VertBufferID]);
			}
		}
	}
	
	BufferContainer.m_LastIndexBufferBound = 0;
	BufferContainer.m_ContainerInfo.m_Attributes.clear();
}

void CCommandProcessorFragment_OpenGL3_3::AppendIndices(unsigned int NewIndicesCount)
{
	if(NewIndicesCount <= m_CurrentIndicesInBuffer) return;
	unsigned int AddCount = NewIndicesCount - m_CurrentIndicesInBuffer;
	unsigned int *Indices = new unsigned int[AddCount];
	int Primq = (m_CurrentIndicesInBuffer/6) * 4;
	for(unsigned int i = 0; i < AddCount; i+=6)
	{
		Indices[i] = Primq;
		Indices[i+1] = Primq + 1;
		Indices[i+2] = Primq + 2;
		Indices[i+3] = Primq;
		Indices[i+4] = Primq + 2;
		Indices[i+5] = Primq + 3;
		Primq+=4;
	}
	
	glBindBuffer(GL_COPY_READ_BUFFER, m_QuadDrawIndexBufferID);
	GLuint NewIndexBufferID;
	glGenBuffers(1, &NewIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, NewIndexBufferID);
	GLsizeiptr size = sizeof(unsigned int);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)NewIndicesCount * size, NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)m_CurrentIndicesInBuffer * size);
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)m_CurrentIndicesInBuffer * size, (GLsizeiptr)AddCount * size, Indices);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	
	glDeleteBuffers(1, &m_QuadDrawIndexBufferID);	
	m_QuadDrawIndexBufferID = NewIndexBufferID;

	for(int i = 0; i < MAX_STREAM_BUFFER_COUNT; ++i)
		m_LastIndexBufferBound[i] = 0;
	for(size_t i = 0; i < m_BufferContainers.size(); ++i)
	{
		m_BufferContainers[i].m_LastIndexBufferBound = 0;
	}

	m_CurrentIndicesInBuffer = NewIndicesCount;
	delete[] Indices;	
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;
	//create necessary space
	if((size_t)Index >= m_BufferObjectIndices.size())
	{
		for(int i = m_BufferObjectIndices.size(); i < Index + 1; ++i)
		{
			m_BufferObjectIndices.push_back(0);
		}
	}

	GLuint VertBufferID = 0;

	glGenBuffers(1, &VertBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, VertBufferID);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pCommand->m_pUploadData, GL_STATIC_DRAW);

	m_BufferObjectIndices[Index] = VertBufferID;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pCommand->m_pUploadData, GL_STATIC_DRAW);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pCommand->m_pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand)
{
	int WriteIndex = pCommand->m_WriteBufferIndex;
	int ReadIndex = pCommand->m_ReadBufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[WriteIndex]);
	glBindBuffer(GL_COPY_READ_BUFFER, m_BufferObjectIndices[ReadIndex]);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_pReadOffset), (GLsizeiptr)(pCommand->m_pWriteOffset), (GLsizeiptr)pCommand->m_CopySize);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;

	glDeleteBuffers(1, &m_BufferObjectIndices[Index]);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//create necessary space
	if((size_t)Index >= m_BufferContainers.size())
	{
		for(int i = m_BufferContainers.size(); i < Index + 1; ++i)
		{
			m_BufferContainers.push_back(SBufferContainer());
		}
	}

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	glGenVertexArrays(1, &BufferContainer.m_VertArrayID);
	glBindVertexArray(BufferContainer.m_VertArrayID);

	BufferContainer.m_LastIndexBufferBound = 0;

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);

		SBufferContainerInfo::SAttribute& Attr = pCommand->m_Attributes[i];

		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, (GLboolean)Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand)
{
	SBufferContainer& BufferContainer = m_BufferContainers[pCommand->m_BufferContainerIndex];

	glBindVertexArray(BufferContainer.m_VertArrayID);

	//disable all old attributes
	for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i) {
		glDisableVertexAttribArray((GLuint)i);
	}
	BufferContainer.m_ContainerInfo.m_Attributes.clear();

	for(int i = 0; i < pCommand->m_AttrCount; ++i) {
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);
		SBufferContainerInfo::SAttribute& Attr = pCommand->m_Attributes[i];
		if(Attr.m_FuncType == 0)
			glVertexAttribPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, Attr.m_Normalized, pCommand->m_Stride, Attr.m_pOffset);
		else if(Attr.m_FuncType == 1)
			glVertexAttribIPointer((GLuint)i, Attr.m_DataTypeCount, Attr.m_Type, pCommand->m_Stride, Attr.m_pOffset);

		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand)
{
	DestroyBufferContainer(pCommand->m_BufferContainerIndex, pCommand->m_DestroyAllBO);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand)
{
	if(pCommand->m_RequiredIndicesNum > m_CurrentIndicesInBuffer)
		AppendIndices(pCommand->m_RequiredIndicesNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0) return;

	CGLSLBorderTileProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pBorderTileProgramTextured;
	}
	else pProgram = m_pBorderTileProgram;
	UseProgram(pProgram);
	if(pProgram->m_LocLOD != -1)
	{
		if(pCommand->m_LOD != pProgram->m_LastLOD)
		{
			pProgram->SetUniform(pProgram->m_LocLOD, (float)(pCommand->m_LOD));

			// calculate the texel offset for the current LOD
			float TexelOffset = (0.5f / (1024.0f * powf(0.5f, (float)pCommand->m_LOD)));
			pProgram->SetUniform(pProgram->m_LocTexelOffset, TexelOffset);

			pProgram->m_LastLOD = pCommand->m_LOD;
		}
	}

	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);

	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float*)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float*)&pCommand->m_Dir);
	pProgram->SetUniform(pProgram->m_LocJumpIndex, (int)pCommand->m_JumpIndex);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	CGLSLBorderTileLineProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pBorderTileLineProgramTextured;
	}
	else pProgram = m_pBorderTileLineProgram;
	UseProgram(pProgram);
	if(pProgram->m_LocLOD != -1)
	{
		if(pCommand->m_LOD != pProgram->m_LastLOD)
		{
			pProgram->SetUniform(pProgram->m_LocLOD, (float)(pCommand->m_LOD));

			// calculate the texel offset for the current LOD
			float TexelOffset = (0.5f / (1024.0f * powf(0.5f, (float)pCommand->m_LOD)));
			pProgram->SetUniform(pProgram->m_LocTexelOffset, TexelOffset);

			pProgram->m_LastLOD = pCommand->m_LOD;
		}
	}

	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float*)&pCommand->m_Dir);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_IndexDrawNum, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	if(pCommand->m_IndicesDrawNum == 0)
	{
		return; //nothing to draw
	}

	CGLSLTileProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pTileProgramTextured;
	}
	else pProgram = m_pTileProgram;

	UseProgram(pProgram);
	if(pProgram->m_LocLOD != -1)
	{
		if(pCommand->m_LOD != pProgram->m_LastLOD)
		{
			pProgram->SetUniform(pProgram->m_LocLOD, (float)(pCommand->m_LOD));

			// calculate the texel offset for the current LOD
			float TexelOffset = (0.5f / (1024.0f * powf(0.5f, (float)pCommand->m_LOD)));
			pProgram->SetUniform(pProgram->m_LocTexelOffset, TexelOffset);

			pProgram->m_LastLOD = pCommand->m_LOD;
		}
	}

	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}
	for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
	{
		glDrawElements(GL_TRIANGLES, pCommand->m_pDrawCount[i], GL_UNSIGNED_INT, pCommand->m_pIndicesOffsets[i]);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	if(pCommand->m_QuadNum == 0)
	{
		return; //nothing to draw
	}

	CGLSLQuadProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pQuadProgramTextured;
	}
	else 
		pProgram = m_pQuadProgram;

	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	// colors, offsets and rotation probably rarely change
	float aColor[4];
	mem_copy(aColor, pCommand->m_pQuadInfo[0].m_aColor, sizeof(aColor));
	float aOffset[2];
	mem_copy(aOffset, pCommand->m_pQuadInfo[0].m_aOffsets, sizeof(aOffset));
	float Rotation = pCommand->m_pQuadInfo[0].m_Rotation;
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)aColor);
	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float*)aOffset);
	pProgram->SetUniform(pProgram->m_LocRotation, (float)Rotation);

	for(int i = 0; i < pCommand->m_QuadNum; ++i)
	{
		if(aColor[0] != pCommand->m_pQuadInfo[i].m_aColor[0] || aColor[1] != pCommand->m_pQuadInfo[i].m_aColor[1] || aColor[2] != pCommand->m_pQuadInfo[i].m_aColor[2] || aColor[3] != pCommand->m_pQuadInfo[i].m_aColor[3])
		{
			mem_copy(aColor, pCommand->m_pQuadInfo[i].m_aColor, sizeof(aColor));
			pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)aColor);
		}
		if(aOffset[0] != pCommand->m_pQuadInfo[i].m_aOffsets[0] || aOffset[1] != pCommand->m_pQuadInfo[i].m_aOffsets[1])
		{
			mem_copy(aOffset, pCommand->m_pQuadInfo[i].m_aOffsets, sizeof(aOffset));
			pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float*)aOffset);
		}
		if(Rotation != pCommand->m_pQuadInfo[i].m_Rotation)
		{
			Rotation = pCommand->m_pQuadInfo[i].m_Rotation;
			pProgram->SetUniform(pProgram->m_LocRotation, (float)Rotation);
		}
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(i * 6 * sizeof(unsigned int)));
	}
}

void CCommandProcessorFragment_OpenGL3_3::RenderText(const CCommandBuffer::SState& State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float* pTextColor, const float* pTextOutlineColor)
{
	if(DrawNum == 0)
	{
		return; //nothing to draw
	}
	
	UseProgram(m_pTextProgram);

	int SlotText = 0;
	int SlotTextOutline = 0;

	if(m_UseMultipleTextureUnits)
	{
		SlotText = TextTextureIndex % m_MaxTextureUnits;
		SlotTextOutline = TextOutlineTextureIndex % m_MaxTextureUnits;
		if(SlotText == SlotTextOutline)
			SlotTextOutline = (TextOutlineTextureIndex + 1) % m_MaxTextureUnits;

		if(!IsAndUpdateTextureSlotBound(SlotText, TextTextureIndex))
		{
			glActiveTexture(GL_TEXTURE0 + SlotText);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[TextTextureIndex].m_Tex);
			glBindSampler(SlotText, m_aTextures[TextTextureIndex].m_Sampler);
		}
		if(!IsAndUpdateTextureSlotBound(SlotTextOutline, TextOutlineTextureIndex))
		{
			glActiveTexture(GL_TEXTURE0 + SlotTextOutline);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[TextOutlineTextureIndex].m_Tex);
			glBindSampler(SlotTextOutline, m_aTextures[TextOutlineTextureIndex].m_Sampler);
		}
	}
	else
	{
		SlotText = 0;
		SlotTextOutline = 1;
		glBindTexture(GL_TEXTURE_2D, m_aTextures[TextTextureIndex].m_Tex);
		glBindSampler(SlotText, m_aTextures[TextTextureIndex].m_Sampler);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[TextOutlineTextureIndex].m_Tex);
		glBindSampler(SlotTextOutline, m_aTextures[TextOutlineTextureIndex].m_Sampler);
		glActiveTexture(GL_TEXTURE0);
	}
	
	if(m_pTextProgram->m_LastTextSampler != SlotText)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextSampler, (int)SlotText);
		m_pTextProgram->m_LastTextSampler = SlotText;
	}

	if(m_pTextProgram->m_LastTextOutlineSampler != SlotTextOutline)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextOutlineSampler, (int)SlotTextOutline);
		m_pTextProgram->m_LastTextOutlineSampler = SlotTextOutline;
	}

	SetState(State, m_pTextProgram);

	if(m_pTextProgram->m_LastTextureSize != TextureSize)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextureSize, (float)TextureSize);
		m_pTextProgram->m_LastTextureSize = TextureSize;
	}

	if(m_pTextProgram->m_LastOutlineColor[0] != pTextOutlineColor[0] || m_pTextProgram->m_LastOutlineColor[1] != pTextOutlineColor[1] || m_pTextProgram->m_LastOutlineColor[2] != pTextOutlineColor[2] || m_pTextProgram->m_LastOutlineColor[3] != pTextOutlineColor[3])
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocOutlineColor, 1, (float*)pTextOutlineColor);
		m_pTextProgram->m_LastOutlineColor[0] = pTextOutlineColor[0];
		m_pTextProgram->m_LastOutlineColor[1] = pTextOutlineColor[1];
		m_pTextProgram->m_LastOutlineColor[2] = pTextOutlineColor[2];
		m_pTextProgram->m_LastOutlineColor[3] = pTextOutlineColor[3];
	}

	if(m_pTextProgram->m_LastColor[0] != pTextColor[0] || m_pTextProgram->m_LastColor[1] != pTextColor[1] || m_pTextProgram->m_LastColor[2] != pTextColor[2] || m_pTextProgram->m_LastColor[3] != pTextColor[3])
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocColor, 1, (float*)pTextColor);
		m_pTextProgram->m_LastColor[0] = pTextColor[0];
		m_pTextProgram->m_LastColor[1] = pTextColor[1];
		m_pTextProgram->m_LastColor[2] = pTextColor[2];
		m_pTextProgram->m_LastColor[3] = pTextColor[3];
	}


	glDrawElements(GL_TRIANGLES, DrawNum, GL_UNSIGNED_INT, (void*)(0));
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	RenderText(pCommand->m_State, pCommand->m_DrawNum, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, pCommand->m_aTextColor, pCommand->m_aTextOutlineColor);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand)
{
	if(pCommand->m_QuadNum == 0)
	{
		return; //nothing to draw
	}

	UploadStreamBufferData(CCommandBuffer::PRIMTYPE_QUADS, pCommand->m_pVertices, pCommand->m_QuadNum);
	
	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);	
	if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
	}

	float aTextColor[4] = { 1.f, 1.f, 1.f, 1.f };

	RenderText(pCommand->m_State, pCommand->m_QuadNum * 6, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, aTextColor, pCommand->m_aTextOutlineColor);

	m_LastStreamBuffer = (m_LastStreamBuffer + 1 >= MAX_STREAM_BUFFER_COUNT ? 0 : m_LastStreamBuffer + 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	UseProgram(m_pPrimitiveProgram);
	SetState(pCommand->m_State, m_pPrimitiveProgram);

	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerAsSprite(const CCommandBuffer::SCommand_RenderQuadContainerAsSprite *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	UseProgram(m_pSpriteProgram);
	SetState(pCommand->m_State, m_pSpriteProgram);

	if(pCommand->m_Rotation != 0.0f && (m_pSpriteProgram->m_LastCenter[0] != pCommand->m_Center.x || m_pSpriteProgram->m_LastCenter[1] != pCommand->m_Center.y))
	{
		m_pSpriteProgram->SetUniformVec2(m_pSpriteProgram->m_LocCenter, 1, (float*)&pCommand->m_Center);
		m_pSpriteProgram->m_LastCenter[0] = pCommand->m_Center.x;
		m_pSpriteProgram->m_LastCenter[1] = pCommand->m_Center.y;
	}

	if(m_pSpriteProgram->m_LastRotation != pCommand->m_Rotation)
	{
		m_pSpriteProgram->SetUniform(m_pSpriteProgram->m_LocRotation, pCommand->m_Rotation);
		m_pSpriteProgram->m_LastRotation = pCommand->m_Rotation;
	}

	if(m_pSpriteProgram->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || m_pSpriteProgram->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || m_pSpriteProgram->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || m_pSpriteProgram->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		m_pSpriteProgram->SetUniformVec4(m_pSpriteProgram->m_LocVertciesColor, 1, (float*)&pCommand->m_VertexColor);
		m_pSpriteProgram->m_LastVertciesColor[0] = pCommand->m_VertexColor.r;
		m_pSpriteProgram->m_LastVertciesColor[1] = pCommand->m_VertexColor.g;
		m_pSpriteProgram->m_LastVertciesColor[2] = pCommand->m_VertexColor.b;
		m_pSpriteProgram->m_LastVertciesColor[3] = pCommand->m_VertexColor.a;
	}

	glDrawElements(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand)
{
	if(pCommand->m_DrawNum == 0 || pCommand->m_DrawCount == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer& BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	UseProgram(m_pSpriteProgramMultiple);
	SetState(pCommand->m_State, m_pSpriteProgramMultiple);

	if((m_pSpriteProgramMultiple->m_LastCenter[0] != pCommand->m_Center.x || m_pSpriteProgramMultiple->m_LastCenter[1] != pCommand->m_Center.y))
	{
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, (float*)&pCommand->m_Center);
		m_pSpriteProgramMultiple->m_LastCenter[0] = pCommand->m_Center.x;
		m_pSpriteProgramMultiple->m_LastCenter[1] = pCommand->m_Center.y;
	}
	
	if(m_pSpriteProgramMultiple->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || m_pSpriteProgramMultiple->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || m_pSpriteProgramMultiple->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || m_pSpriteProgramMultiple->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocVertciesColor, 1, (float*)&pCommand->m_VertexColor);
		m_pSpriteProgramMultiple->m_LastVertciesColor[0] = pCommand->m_VertexColor.r;
		m_pSpriteProgramMultiple->m_LastVertciesColor[1] = pCommand->m_VertexColor.g;
		m_pSpriteProgramMultiple->m_LastVertciesColor[2] = pCommand->m_VertexColor.b;
		m_pSpriteProgramMultiple->m_LastVertciesColor[3] = pCommand->m_VertexColor.a;
	}

	int DrawCount = pCommand->m_DrawCount;
	size_t RenderOffset = 0;
	
	// 4 for the center (always use vec4) and 16 for the matrix(just to be sure), 4 for the sampler and vertex color
	const int RSPCount = 256 - 4 - 16 - 8;

	while(DrawCount > 0)
	{
		int UniformCount = (DrawCount > RSPCount ? RSPCount : DrawCount);

		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocRSP, UniformCount, (float*)(pCommand->m_pRenderInfo + RenderOffset));

		glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset, UniformCount);

		RenderOffset += RSPCount;
		DrawCount -= RSPCount;
	}
}

// ------------ CCommandProcessorFragment_SDL

void CCommandProcessorFragment_SDL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);

	// set some default settings
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);
}

void CCommandProcessorFragment_SDL::Cmd_Update_Viewport(const SCommand_Update_Viewport *pCommand)
{
	glViewport(pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height);
}

void CCommandProcessorFragment_SDL::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	SDL_GL_MakeCurrent(NULL, NULL);
}

void CCommandProcessorFragment_SDL::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
	SDL_GL_SwapWindow(m_pWindow);

	if(pCommand->m_Finish)
		glFinish();
}

void CCommandProcessorFragment_SDL::Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand)
{
	*pCommand->m_pRetOk = SDL_GL_SetSwapInterval(pCommand->m_VSync) == 0;
}

void CCommandProcessorFragment_SDL::Cmd_Resize(const CCommandBuffer::SCommand_Resize *pCommand)
{
	SDL_SetWindowSize(m_pWindow, pCommand->m_Width, pCommand->m_Height);
	glViewport(0, 0, pCommand->m_Width, pCommand->m_Height);
}

void CCommandProcessorFragment_SDL::Cmd_VideoModes(const CCommandBuffer::SCommand_VideoModes *pCommand)
{
	SDL_DisplayMode mode;
	int maxModes = SDL_GetNumDisplayModes(pCommand->m_Screen),
		numModes = 0;

	for(int i = 0; i < maxModes; i++)
	{
		if(SDL_GetDisplayMode(pCommand->m_Screen, i, &mode) < 0)
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			continue;
		}

		bool AlreadyFound = false;
		for(int j = 0; j < numModes; j++)
		{
			if(pCommand->m_pModes[j].m_Width == mode.w && pCommand->m_pModes[j].m_Height == mode.h)
			{
				AlreadyFound = true;
				break;
			}
		}
		if(AlreadyFound)
			continue;

		pCommand->m_pModes[numModes].m_Width = mode.w;
		pCommand->m_pModes[numModes].m_Height = mode.h;
		pCommand->m_pModes[numModes].m_Red = 8;
		pCommand->m_pModes[numModes].m_Green = 8;
		pCommand->m_pModes[numModes].m_Blue = 8;
		numModes++;
	}
	*pCommand->m_pNumModes = numModes;
}

CCommandProcessorFragment_SDL::CCommandProcessorFragment_SDL()
{
}

bool CCommandProcessorFragment_SDL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_SWAP: Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_VSYNC: Cmd_VSync(static_cast<const CCommandBuffer::SCommand_VSync *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RESIZE: Cmd_Resize(static_cast<const CCommandBuffer::SCommand_Resize *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_VIDEOMODES: Cmd_VideoModes(static_cast<const CCommandBuffer::SCommand_VideoModes *>(pBaseCommand)); break;
	case CMD_INIT: Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand)); break;
	case CMD_SHUTDOWN: Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand)); break;
	case CMD_UPDATE_VIEWPORT: Cmd_Update_Viewport(static_cast<const SCommand_Update_Viewport *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessor_SDL_OpenGL

void CCommandProcessor_SDL_OpenGL::RunBuffer(CCommandBuffer *pBuffer)
{
	unsigned CmdIndex = 0;
	while(1)
	{
		const CCommandBuffer::SCommand *pBaseCommand = pBuffer->GetCommand(&CmdIndex);
		if(pBaseCommand == 0x0)
			break;
	
		if(m_UseOpenGL3_3)
		{
			if(m_OpenGL3_3.RunCommand(pBaseCommand))
				continue;
		}
		else
		{
			if(m_OpenGL.RunCommand(pBaseCommand))
				continue;
		}
		if(m_SDL.RunCommand(pBaseCommand))
			continue;

		if(m_General.RunCommand(pBaseCommand))
			continue;

		dbg_msg("graphics", "unknown command %d", pBaseCommand->m_Cmd);
	}
}

// ------------ CGraphicsBackend_SDL_OpenGL

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{	
	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return -1;
		}

		#ifdef CONF_FAMILY_WINDOWS
			if(!getenv("SDL_VIDEO_WINDOW_POS") && !getenv("SDL_VIDEO_CENTERED")) // ignore_convention
				putenv("SDL_VIDEO_WINDOW_POS=center"); // ignore_convention
		#endif
	}

	SDL_ClearError();
	const char *pErr = NULL;
	
	//query default values, since they are platform dependend
	static bool s_InitDefaultParams = false;
	static int s_SDLGLContextProfileMask, s_SDLGLContextMajorVersion, s_SDLGLContextMinorVersion;
	if(!s_InitDefaultParams)
	{
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &s_SDLGLContextProfileMask);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &s_SDLGLContextMajorVersion);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &s_SDLGLContextMinorVersion);
		s_InitDefaultParams = true;
	}
	
	m_UseOpenGL3_3 = false;
	if(g_Config.m_GfxOpenGL3 && SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0)
	{
		pErr = SDL_GetError();
		if(pErr[0] != '\0')
		{
			dbg_msg("gfx", "Using old OpenGL context, because an error occurred while trying to use OpenGL context 3.3: %s.", pErr);		
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, s_SDLGLContextProfileMask);
		}
		else
		{		
			if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) == 0 && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) == 0)
			{				
				pErr = SDL_GetError();
				if(pErr[0] != '\0')
				{					
					dbg_msg("gfx", "Using old OpenGL context, because an error occurred while trying to use OpenGL context 3.3: %s.", pErr);
					SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_SDLGLContextMajorVersion);
					SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_SDLGLContextMinorVersion);
				}
				else
				{
					m_UseOpenGL3_3 = true;
					int vMaj, vMin;
					SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &vMaj);
					SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &vMin);
					dbg_msg("gfx", "Using OpenGL version %d.%d.", vMaj, vMin);
				}
			}
			else 
			{
				dbg_msg("gfx", "Couldn't create OpenGL 3.3 context.");
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_SDLGLContextMajorVersion);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_SDLGLContextMinorVersion);
			}
		}
	}
	else 
	{
		//set default attributes
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, s_SDLGLContextProfileMask);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_SDLGLContextMajorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_SDLGLContextMinorVersion);
	}

	// set screen
	SDL_Rect ScreenPos;
	m_NumScreens = SDL_GetNumVideoDisplays();
	if(m_NumScreens > 0)
	{
		if(*Screen < 0 || *Screen >= m_NumScreens)
			*Screen = 0;
		if(SDL_GetDisplayBounds(*Screen, &ScreenPos) != 0)
		{
			dbg_msg("gfx", "unable to retrieve screen information: %s", SDL_GetError());
			return -1;
		}

	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		return -1;
	}

	// store desktop resolution for settings reset button
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(*Screen, &DisplayMode))
	{
		dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
		return -1;
	}
	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;

	// use desktop resolution as default resolution
#if defined(__ANDROID__) || defined(CONF_PLATFORM_MACOSX)
	*pWidth = *pDesktopWidth;
	*pHeight = *pDesktopHeight;
#else
	if(*pWidth == 0 || *pHeight == 0)
	{
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
	}
#endif

	// set flags
	int SdlFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI;
#if defined(SDL_VIDEO_DRIVER_X11)
	if(Flags&IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
#endif
	if(Flags&IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags&IGraphicsBackend::INITFLAG_FULLSCREEN)
	{
		//when we are at fullscreen, we really shouldn't allow window sizes, that aren't supported by the driver
		bool SupportedResolution = false;
		SDL_DisplayMode mode;
		int maxModes = SDL_GetNumDisplayModes(g_Config.m_GfxScreen);

		for(int i = 0; i < maxModes; i++)
		{
			if(SDL_GetDisplayMode(g_Config.m_GfxScreen, i, &mode) < 0)
			{
				dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
				continue;
			}

			if(*pWidth == mode.w && *pHeight == mode.h)
			{
				SupportedResolution = true;
				break;
			}
		}
		if(SupportedResolution)
			SdlFlags |= SDL_WINDOW_FULLSCREEN;
		else
			SdlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	// set gl attributes
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if(FsaaSamples)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, FsaaSamples);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	if(g_Config.m_InpMouseOld)
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");

	m_pWindow = SDL_CreateWindow(
		pName,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		*pWidth,
		*pHeight,
		SdlFlags);

	// set caption
	if(m_pWindow == NULL)
	{
		dbg_msg("gfx", "unable to create window: %s", SDL_GetError());
		return -1;
	}

	m_GLContext = SDL_GL_CreateContext(m_pWindow);

	if(m_GLContext == NULL)
	{
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		return -1;
	}

	if(m_UseOpenGL3_3)
	{
		//support graphic cards that are pretty old(and linux)
		glewExperimental = GL_TRUE;
		if(GLEW_OK != glewInit())
			return -1;
	}

	SDL_GL_GetDrawableSize(m_pWindow, pWidth, pHeight);
	SDL_GL_SetSwapInterval(Flags&IGraphicsBackend::INITFLAG_VSYNC ? 1 : 0);
	SDL_GL_MakeCurrent(NULL, NULL);

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL;
	((CCommandProcessor_SDL_OpenGL*)m_pProcessor)->UseOpenGL3_3(m_UseOpenGL3_3);
	StartProcessor(m_pProcessor);

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	if(m_UseOpenGL3_3)
	{
		//run sdl first to have the context in the thread
		CCommandProcessorFragment_SDL::SCommand_Init CmdSDL;
		CmdSDL.m_pWindow = m_pWindow;
		CmdSDL.m_GLContext = m_GLContext;
		CmdBuffer.AddCommand(CmdSDL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CCommandProcessorFragment_OpenGL3_3::SCommand_Init CmdOpenGL;
		CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdOpenGL.m_pStorage = pStorage;
		CmdBuffer.AddCommand(CmdOpenGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
	} 
	else
	{
		CCommandProcessorFragment_OpenGL::SCommand_Init CmdOpenGL;
		CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdBuffer.AddCommand(CmdOpenGL);
		CCommandProcessorFragment_SDL::SCommand_Init CmdSDL;
		CmdSDL.m_pWindow = m_pWindow;
		CmdSDL.m_GLContext = m_GLContext;
		CmdBuffer.AddCommand(CmdSDL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
	}

	*pCurrentWidth = *pWidth;
	*pCurrentHeight = *pHeight;

	SDL_ShowWindow(m_pWindow);
	if(SetWindowScreen(g_Config.m_GfxScreen))
	{
		// query the current displaymode, when running in fullscreen
		// this is required if DPI scaling is active
		if(SdlFlags&SDL_WINDOW_FULLSCREEN)
		{
			SDL_DisplayMode CurrentDisplayMode;
			SDL_GetCurrentDisplayMode(g_Config.m_GfxScreen, &CurrentDisplayMode);

			*pCurrentWidth = CurrentDisplayMode.w;
			*pCurrentHeight = CurrentDisplayMode.h;

			// since the window is centered, calculate how much the viewport has to be fixed
			//int XOverflow = (*pWidth > *pCurrentWidth ? (*pWidth - *pCurrentWidth) : 0);
			//int YOverflow = (*pHeight > *pCurrentHeight ? (*pHeight - *pCurrentHeight) : 0);
			//TODO: current problem is, that the opengl driver knows about the scaled display,
			//so the viewport cannot be adjusted for resolutions, that are higher than allowed by the display driver
			
			CCommandProcessorFragment_SDL::SCommand_Update_Viewport CmdSDL;
			CmdSDL.m_X = 0;
			CmdSDL.m_Y = 0;

			CmdSDL.m_Width = CurrentDisplayMode.w;
			CmdSDL.m_Height = CurrentDisplayMode.h;
			CmdBuffer.AddCommand(CmdSDL);
			RunBuffer(&CmdBuffer);
			WaitForIdle();			
		}
	}

	// return
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	if(m_UseOpenGL3_3)
	{
		CCommandProcessorFragment_OpenGL3_3::SCommand_Shutdown Cmd;
		CmdBuffer.AddCommand(Cmd);
	}
	CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
	CmdBuffer.AddCommand(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_GL_DeleteContext(m_GLContext);
	SDL_DestroyWindow(m_pWindow);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_OpenGL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::Maximize()
{
	// TODO: SDL
}

bool CGraphicsBackend_SDL_OpenGL::Fullscreen(bool State)
{
#if defined(CONF_PLATFORM_MACOSX)	// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
#else
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN : 0) == 0;
#endif
}

void CGraphicsBackend_SDL_OpenGL::SetWindowBordered(bool State)
{
	SDL_SetWindowBordered(m_pWindow, SDL_bool(State));
}

bool CGraphicsBackend_SDL_OpenGL::SetWindowScreen(int Index)
{
	if(Index >= 0 && Index < m_NumScreens)
	{
		SDL_Rect ScreenPos;
		if(SDL_GetDisplayBounds(Index, &ScreenPos) == 0)
		{
			SDL_SetWindowPosition(m_pWindow,
				SDL_WINDOWPOS_CENTERED_DISPLAY(Index),
				SDL_WINDOWPOS_CENTERED_DISPLAY(Index));
			return true;
		}
	}

	return false;
}

int CGraphicsBackend_SDL_OpenGL::GetWindowScreen()
{
	return SDL_GetWindowDisplayIndex(m_pWindow);
}

int CGraphicsBackend_SDL_OpenGL::WindowActive()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_SHOWN;
}

void CGraphicsBackend_SDL_OpenGL::SetWindowGrab(bool Grab)
{
	SDL_SetWindowGrab(m_pWindow, Grab ? SDL_TRUE : SDL_FALSE);
}

void CGraphicsBackend_SDL_OpenGL::NotifyWindow()
{
	if(!g_Config.m_ClShowNotifications)
		return;

	// get window handle
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(m_pWindow, &info))
	{
		dbg_msg("gfx", "unable to obtain window handle");
		return;
	}

	#if defined(CONF_FAMILY_WINDOWS)
		FLASHWINFO desc;
		desc.cbSize = sizeof(desc);
		desc.hwnd = info.info.win.window;
		desc.dwFlags = FLASHW_TRAY;
		desc.uCount = 3; // flash 3 times
		desc.dwTimeout = 0;

		FlashWindowEx(&desc);
	#elif defined(SDL_VIDEO_DRIVER_X11) && !defined(CONF_PLATFORM_MACOSX)
		Display *dpy = info.info.x11.display;
		Window win = info.info.x11.window;

		// Old hints
		XWMHints *wmhints;
		wmhints = XAllocWMHints();
		wmhints->flags = XUrgencyHint;
		XSetWMHints(dpy, win, wmhints);
		XFree(wmhints);

		// More modern way of notifying
		static Atom demandsAttention = XInternAtom(dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", true);
		static Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", true);
		XChangeProperty(dpy, win, wmState, XA_ATOM, 32, PropModeReplace,
			(unsigned char *) &demandsAttention, 1);
	#endif
}


IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_OpenGL; }
