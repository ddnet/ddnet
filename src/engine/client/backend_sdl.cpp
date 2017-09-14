#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
	// For FlashWindowEx, FLASHWINFO, FLASHW_TRAY
	#define _WIN32_WINNT 0x0501
	#define WINVER 0x0501
#endif

#include "GL/glew.h"
#include <base/detect.h>
#include <base/math.h>
#include <stdlib.h>
#include "SDL.h"
#include "SDL_syswm.h"
#if defined(__ANDROID__)
	#define GL_GLEXT_PROTOTYPES
	#include <GLES/gl.h>
	#include <GLES/glext.h>
	#include <GL/glu.h>
	#define glOrtho glOrthof
#else
	#include "SDL_opengl.h"

	#if defined(CONF_PLATFORM_MACOSX)
	#include "OpenGL/glu.h"
	#else
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

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::SCommand * pBaseCommand)
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
	int ScaleW = Width/NewWidth;
	int ScaleH = Height/NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;

	pTmpData = (unsigned char *)mem_alloc(NewWidth*NewHeight*Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c*Bpp] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+1] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+2] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c*Bpp+3] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 3, ScaleW, ScaleH, Bpp);
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
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, 1.0f, 10.f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height,
		TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pCommand->m_pData);
	mem_free(pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTexSize);
		if(Width > MaxTexSize || Height > MaxTexSize)
		{
			do
			{
				Width>>=1;
				Height>>=1;
			}
			while(Width > MaxTexSize || Height > MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > 16 && Height > 16 && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width>>=1;
			Height>>=1;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}

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

	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	SetState(pCommand->m_State);

	glVertexPointer(3, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices + sizeof(float)*3);
	glColorPointer(4, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char*)pCommand->m_pVertices + sizeof(float)*5);
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
	unsigned char *pPixelData = (unsigned char *)mem_alloc(w*(h+1)*3, 1);
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

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::SCommand * pBaseCommand)
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

	pTmpData = (unsigned char *)mem_alloc(NewWidth*NewHeight*Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c*Bpp] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+1] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+2] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c*Bpp+3] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 3, ScaleW, ScaleH, Bpp);
		}

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL3_3::SetState(const CCommandBuffer::SState &State, CGLSLTWProgram* pProgram)
{
	if(State.m_BlendMode != m_LastBlendMode && State.m_BlendMode != CCommandBuffer::BLEND_NONE)
	{
		// blend
		switch(State.m_BlendMode)
		{
		case CCommandBuffer::BLEND_NONE:
			//we don't really need this anymore
			//glDisable(GL_BLEND);
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
		//dont disable it always
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
		} else {
			Slot = 0;
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
			glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler);
		}
		if(pProgram->m_LocIsTextured != -1) pProgram->SetUniform(pProgram->m_LocIsTextured, (int)1);
		pProgram->SetUniform(pProgram->m_LocTextureSampler, (int)Slot);

		if(m_TextureSlotBoundToUnit[Slot].m_LastWrapMode != State.m_WrapMode)
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
			m_TextureSlotBoundToUnit[Slot].m_LastWrapMode = State.m_WrapMode;
		}
	}
	else {
		if(pProgram->m_LocIsTextured != -1) pProgram->SetUniform(pProgram->m_LocIsTextured, (int)0);
	}

	// screen mapping
	//ortho matrix... we only need this projection... just made all structs here..
	struct vec4{
		vec4() {}
		vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {} 
		vec4(vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {} 
		float x;
		float y;
		float z;
		float w;
	};
	
	struct mat4{
		mat4(vec4& r1, vec4& r2, vec4& r3, vec4& r4) { r[0] = r1; r[1] = r2; r[2] = r3; r[3] = r4; }
		vec4 r[4];
	};
	
	vec4 r1( (2.f/(State.m_ScreenBR.x - State.m_ScreenTL.x)), 0, 0, -((State.m_ScreenBR.x + State.m_ScreenTL.x)/(State.m_ScreenBR.x - State.m_ScreenTL.x)));
	vec4 r2( 0, (2.f/(State.m_ScreenTL.y - State.m_ScreenBR.y)), 0, -((State.m_ScreenTL.y + State.m_ScreenBR.y)/(State.m_ScreenTL.y - State.m_ScreenBR.y)));
	vec4 r3( 0, 0, -(2.f/(9.f)), -((11.f)/(9.f)));
	vec4 r4( 0, 0, 0, 1.0f);
	
	mat4 m(r1,r2,r3,r4);
	
	//transpose bcs of column-major order of opengl
	glUniformMatrix4fv(pProgram->m_LocPos, 1, true, (float*)&m);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Init(const SCommand_Init *pCommand)
{
	m_UseMultipleTextureUnits = g_Config.m_GfxEnableTextureUnitOptimization;
	if(!m_UseMultipleTextureUnits)
	{
		glActiveTexture(GL_TEXTURE0);
	}
	
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_LastBlendMode = -1;
	m_LastClipEnable = false;
	m_pPrimitiveProgram = new CGLSLPrimitiveProgram;
	m_pTileProgram = new CGLSLTileProgram;
	m_pTileProgramTextured = new CGLSLTileProgram;
	m_pBorderTileProgram = new CGLSLBorderTileProgram;
	m_pBorderTileProgramTextured = new CGLSLBorderTileProgram;
	m_pBorderTileLineProgram = new CGLSLBorderTileLineProgram;
	m_pBorderTileLineProgramTextured = new CGLSLBorderTileLineProgram;
	
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader("./shader/prim.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader("./shader/prim.frag", GL_FRAGMENT_SHADER);
		
		m_pPrimitiveProgram->CreateProgram();
		m_pPrimitiveProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveProgram->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pPrimitiveProgram->DetachShader(&PrimitiveVertexShader);
		m_pPrimitiveProgram->DetachShader(&PrimitiveFragmentShader);
		
		m_pPrimitiveProgram->UseProgram();
		
		m_pPrimitiveProgram->m_LocPos = m_pPrimitiveProgram->GetUniformLoc("Pos");
		m_pPrimitiveProgram->m_LocIsTextured = m_pPrimitiveProgram->GetUniformLoc("isTextured");
		m_pPrimitiveProgram->m_LocTextureSampler = m_pPrimitiveProgram->GetUniformLoc("textureSampler");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/tile.frag", GL_FRAGMENT_SHADER);
		
		m_pTileProgram->CreateProgram();
		m_pTileProgram->AddShader(&VertexShader);
		m_pTileProgram->AddShader(&FragmentShader);
		m_pTileProgram->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pTileProgram->DetachShader(&VertexShader);
		m_pTileProgram->DetachShader(&FragmentShader);
		
		m_pTileProgram->UseProgram();
		
		m_pTileProgram->m_LocPos = m_pTileProgram->GetUniformLoc("Pos");
		m_pTileProgram->m_LocIsTextured = -1;
		m_pTileProgram->m_LocTextureSampler = -1;
		m_pTileProgram->m_LocColor = m_pTileProgram->GetUniformLoc("vertColor");
		m_pTileProgram->m_LocZoomFactor = -1;
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/tiletex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/tiletex.frag", GL_FRAGMENT_SHADER);
		
		m_pTileProgramTextured->CreateProgram();
		m_pTileProgramTextured->AddShader(&VertexShader);
		m_pTileProgramTextured->AddShader(&FragmentShader);
		m_pTileProgramTextured->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pTileProgramTextured->DetachShader(&VertexShader);
		m_pTileProgramTextured->DetachShader(&FragmentShader);
		
		m_pTileProgramTextured->UseProgram();
		
		m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("Pos");
		m_pTileProgramTextured->m_LocIsTextured = -1;
		m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("textureSampler");
		m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("vertColor");
		m_pTileProgramTextured->m_LocZoomFactor = m_pTileProgramTextured->GetUniformLoc("ZoomFactor");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/bordertile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/bordertile.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileProgram->CreateProgram();
		m_pBorderTileProgram->AddShader(&VertexShader);
		m_pBorderTileProgram->AddShader(&FragmentShader);
		m_pBorderTileProgram->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pBorderTileProgram->DetachShader(&VertexShader);
		m_pBorderTileProgram->DetachShader(&FragmentShader);
		
		m_pBorderTileProgram->UseProgram();
		
		m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("Pos");
		m_pBorderTileProgram->m_LocIsTextured = -1;
		m_pBorderTileProgram->m_LocTextureSampler = -1;
		m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("vertColor");
		m_pBorderTileProgram->m_LocZoomFactor = -1;
		m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("Offset");
		m_pBorderTileProgram->m_LocDir = m_pBorderTileProgram->GetUniformLoc("Dir");
		m_pBorderTileProgram->m_LocJumpIndex = m_pBorderTileProgram->GetUniformLoc("JumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/bordertiletex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/bordertiletex.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileProgramTextured->CreateProgram();
		m_pBorderTileProgramTextured->AddShader(&VertexShader);
		m_pBorderTileProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileProgramTextured->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pBorderTileProgramTextured->DetachShader(&VertexShader);
		m_pBorderTileProgramTextured->DetachShader(&FragmentShader);
		
		m_pBorderTileProgramTextured->UseProgram();
		
		m_pBorderTileProgramTextured->m_LocPos = m_pBorderTileProgramTextured->GetUniformLoc("Pos");
		m_pBorderTileProgramTextured->m_LocIsTextured = -1;
		m_pBorderTileProgramTextured->m_LocTextureSampler = m_pBorderTileProgramTextured->GetUniformLoc("textureSampler");
		m_pBorderTileProgramTextured->m_LocColor = m_pBorderTileProgramTextured->GetUniformLoc("vertColor");
		m_pBorderTileProgramTextured->m_LocZoomFactor = m_pBorderTileProgramTextured->GetUniformLoc("ZoomFactor");
		m_pBorderTileProgramTextured->m_LocOffset = m_pBorderTileProgramTextured->GetUniformLoc("Offset");
		m_pBorderTileProgramTextured->m_LocDir = m_pBorderTileProgramTextured->GetUniformLoc("Dir");
		m_pBorderTileProgramTextured->m_LocJumpIndex = m_pBorderTileProgramTextured->GetUniformLoc("JumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/bordertileline.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/bordertileline.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileLineProgram->CreateProgram();
		m_pBorderTileLineProgram->AddShader(&VertexShader);
		m_pBorderTileLineProgram->AddShader(&FragmentShader);
		m_pBorderTileLineProgram->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pBorderTileLineProgram->DetachShader(&VertexShader);
		m_pBorderTileLineProgram->DetachShader(&FragmentShader);
		
		m_pBorderTileLineProgram->UseProgram();
		
		m_pBorderTileLineProgram->m_LocPos = m_pBorderTileLineProgram->GetUniformLoc("Pos");
		m_pBorderTileLineProgram->m_LocIsTextured = -1;
		m_pBorderTileLineProgram->m_LocTextureSampler = -1;
		m_pBorderTileLineProgram->m_LocColor = m_pBorderTileLineProgram->GetUniformLoc("vertColor");
		m_pBorderTileLineProgram->m_LocZoomFactor = -1;
		m_pBorderTileLineProgram->m_LocDir = m_pBorderTileLineProgram->GetUniformLoc("Dir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader("./shader/bordertilelinetex.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader("./shader/bordertilelinetex.frag", GL_FRAGMENT_SHADER);
		
		m_pBorderTileLineProgramTextured->CreateProgram();
		m_pBorderTileLineProgramTextured->AddShader(&VertexShader);
		m_pBorderTileLineProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileLineProgramTextured->LinkProgram();
		
		//detach shader, they are not needed anymore after linking
		m_pBorderTileLineProgramTextured->DetachShader(&VertexShader);
		m_pBorderTileLineProgramTextured->DetachShader(&FragmentShader);
		
		m_pBorderTileLineProgramTextured->UseProgram();
		
		m_pBorderTileLineProgramTextured->m_LocPos = m_pBorderTileLineProgramTextured->GetUniformLoc("Pos");
		m_pBorderTileLineProgramTextured->m_LocIsTextured = -1;
		m_pBorderTileLineProgramTextured->m_LocTextureSampler = m_pBorderTileLineProgramTextured->GetUniformLoc("textureSampler");
		m_pBorderTileLineProgramTextured->m_LocColor = m_pBorderTileLineProgramTextured->GetUniformLoc("vertColor");
		m_pBorderTileLineProgramTextured->m_LocZoomFactor = m_pBorderTileLineProgramTextured->GetUniformLoc("ZoomFactor");
		m_pBorderTileLineProgramTextured->m_LocDir = m_pBorderTileLineProgramTextured->GetUniformLoc("Dir");
	}
	
	glGenBuffers(1, &m_PrimitiveDrawBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID);
	glGenVertexArrays(1, &m_PrimitiveDrawVertexID);
	glBindVertexArray(m_PrimitiveDrawVertexID);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	
	//query maximum of allowed textures
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_MaxTextureUnits);
	m_TextureSlotBoundToUnit.resize(m_MaxTextureUnits);
	for(int i = 0; i < m_MaxTextureUnits; ++i)
	{
		m_TextureSlotBoundToUnit[i].m_TextureSlot = -1;
		m_TextureSlotBoundToUnit[i].m_LastWrapMode = -1;
	}
	
	glGenBuffers(1, &m_PrimitiveDrawIndexBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_PrimitiveDrawIndexBufferID);
	
	unsigned int Indices[CCommandBuffer::MAX_VERTICES/4 * 6];
	int Primq = 0;
	for(int i = 0; i < CCommandBuffer::MAX_VERTICES/4 * 6; i+=6)
	{
		Indices[i] = Primq;
		Indices[i+1] = Primq + 1;
		Indices[i+2] = Primq + 3;
		Indices[i+3] = Primq + 1;
		Indices[i+4] = Primq + 2;
		Indices[i+5] = Primq + 3;
		Primq+=4;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * CCommandBuffer::MAX_VERTICES/4 * 6, Indices, GL_STATIC_DRAW);
	
	mem_zero(m_aTextures, sizeof(m_aTextures));
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	//clean up everything
	m_pPrimitiveProgram->DeleteProgram();
	//m_QuadProgram->DeleteProgram();
	m_pTileProgram->DeleteProgram();
	m_pTileProgramTextured->DeleteProgram();
	m_pBorderTileProgram->DeleteProgram();
	m_pBorderTileProgramTextured->DeleteProgram();
	m_pBorderTileLineProgram->DeleteProgram();
	m_pBorderTileLineProgramTextured->DeleteProgram();
	
	delete m_pPrimitiveProgram;
	//delete m_QuadProgram;
	delete m_pTileProgram;
	delete m_pTileProgramTextured;
	delete m_pBorderTileProgram;
	delete m_pBorderTileProgramTextured;
	delete m_pBorderTileLineProgram;
	delete m_pBorderTileLineProgramTextured;
	
	glBindVertexArray(0);
	glDeleteBuffers(1, &m_PrimitiveDrawBufferID);
	glDeleteVertexArrays(1, &m_PrimitiveDrawVertexID);
	
	for(int i = 0; i < CCommandBuffer::MAX_TEXTURES; ++i)
	{
		DestroyTexture(i);
	}
	
	for(int i = 0; i < m_VisualObjects.size(); ++i)
	{
		DestroyVisualObjects(i);
	}
	m_VisualObjects.clear();
	
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	if(m_UseMultipleTextureUnits)
	{
		int Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
	}
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height,
		TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pCommand->m_pData);
	mem_free(pCommand->m_pData);
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
	m_TextureSlotBoundToUnit[Slot].m_LastWrapMode = -1;
	DestroyTexture(pCommand->m_Slot);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTexSize);
		if(Width > MaxTexSize || Height > MaxTexSize)
		{
			do
			{
				Width>>=1;
				Height>>=1;
			}
			while(Width > MaxTexSize || Height > MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > 16 && Height > 16 && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width>>=1;
			Height>>=1;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}

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
			//this needs further checks. it seems on some gpus COMPRESSED_ALPHA isnt in the core profile
			case GL_RED: StoreOglformat = GL_COMPRESSED_RGBA; break;
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
		StoreOglformat = GL_RGBA;
	}
	
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}
	else
	{
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
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
	
	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	m_pPrimitiveProgram->UseProgram();
	SetState(pCommand->m_State, m_pPrimitiveProgram);
	
	int Count = 0;
	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_LINES:
		Count = pCommand->m_PrimCount*2;
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		Count = pCommand->m_PrimCount*4;
		break;
	default:
		return;
	};
	
	glBindVertexArray(m_PrimitiveDrawVertexID);
	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertex) * Count, (char*)pCommand->m_pVertices, GL_STREAM_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void*)(sizeof(float) * 2));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void*)(sizeof(float) * 4));

	switch(pCommand->m_PrimType)
	{
	//we dont support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount*2);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_PrimitiveDrawIndexBufferID);
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount*6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0,0,0,0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)mem_alloc(w*(h+1)*3, 1);
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

bool CCommandProcessorFragment_OpenGL3_3::RunCommand(const CCommandBuffer::SCommand * pBaseCommand)
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
	
	case CCommandBuffer::CMD_CREATE_VERTEX_BUFFER_OBJECT: Cmd_CreateVertBuffer(static_cast<const CCommandBuffer::SCommand_CreateVertexBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_APPEND_VERTEX_BUFFER_OBJECT: Cmd_AppendVertBuffer(static_cast<const CCommandBuffer::SCommand_AppendVertexBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CREATE_VERTEX_ARRAY_OBJECT: Cmd_CreateVertArray(static_cast<const CCommandBuffer::SCommand_CreateVertexArrayObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CREATE_INDEX_BUFFER_OBJECT: Cmd_CreateIndexBuffer(static_cast<const CCommandBuffer::SCommand_CreateIndexBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_APPEND_INDEX_BUFFER_OBJECT: Cmd_AppendIndexBuffer(static_cast<const CCommandBuffer::SCommand_AppendIndexBufferObject *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_IBO_VERTEX_ARRAY: Cmd_RenderVertexArray(static_cast<const CCommandBuffer::SCommand_RenderVertexArray *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_DESTROY_VISUAL: Cmd_DestroyVertexArray(static_cast<const CCommandBuffer::SCommand_DestroyVisual *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_BORDER_TILE: Cmd_RenderBorderTile(static_cast<const CCommandBuffer::SCommand_RenderBorderTile *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_BORDER_TILE_LINE: Cmd_RenderBorderTileLine(static_cast<const CCommandBuffer::SCommand_RenderBorderTileLine *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

bool CCommandProcessorFragment_OpenGL3_3::IsAndUpdateTextureSlotBound(int IDX, int Slot)
{
	if(m_TextureSlotBoundToUnit[IDX].m_TextureSlot == Slot) return true;
	else {
		//the texture slot uses this index now
		m_TextureSlotBoundToUnit[IDX].m_TextureSlot = Slot;
		m_TextureSlotBoundToUnit[IDX].m_LastWrapMode = -1;
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

void CCommandProcessorFragment_OpenGL3_3::DestroyVisualObjects(int Index)
{
	SVisualObject& VisualObject = m_VisualObjects[Index];
	if(VisualObject.m_VertArrayID != 0) glDeleteBuffers(1, &VisualObject.m_VertArrayID);
	if(VisualObject.m_VertBufferID != 0) glDeleteBuffers(1, &VisualObject.m_VertBufferID);//this line should never be called
	if(VisualObject.m_IndexBufferID != 0) glDeleteBuffers(1, &VisualObject.m_IndexBufferID);

	VisualObject.m_NumElements = 0;
	VisualObject.m_IsTextured = false;
	VisualObject.m_NumIndices = 0;
	VisualObject.m_IndexBufferID = VisualObject.m_VertBufferID = VisualObject.m_VertArrayID = 0;
}
	
void CCommandProcessorFragment_OpenGL3_3::Cmd_DestroyVertexArray(const CCommandBuffer::SCommand_DestroyVisual *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	if(Index >= m_VisualObjects.size())
		return;
	
	DestroyVisualObjects(Index);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	//if space not there return
	if(Index >= m_VisualObjects.size())
		return;
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	if(VisualObject.m_VertArrayID == 0 || VisualObject.m_IndexBufferID == 0) return;
	
	CGLSLBorderTileProgram* pProgram = NULL;
	if(VisualObject.m_IsTextured)
	{
		pProgram = m_pBorderTileProgramTextured;
	}
	else pProgram = m_pBorderTileProgram;
	pProgram->UseProgram();
	if(pProgram->m_LocZoomFactor != -1) pProgram->SetUniform(pProgram->m_LocZoomFactor, (float)(pCommand->m_ZoomScreenRatio < .5f ? .5f : pCommand->m_ZoomScreenRatio));
	
	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);
	
	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float*)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float*)&pCommand->m_Dir);
	pProgram->SetUniform(pProgram->m_LocJumpIndex, (int)pCommand->m_JumpIndex);
	
	glBindVertexArray(VisualObject.m_VertArrayID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VisualObject.m_IndexBufferID);
		
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	//if space not there return
	if(Index >= m_VisualObjects.size())
		return;
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	if(VisualObject.m_VertArrayID == 0 || VisualObject.m_IndexBufferID == 0) return;
	
	CGLSLBorderTileLineProgram* pProgram = NULL;
	if(VisualObject.m_IsTextured)
	{
		pProgram = m_pBorderTileLineProgramTextured;
	}
	else pProgram = m_pBorderTileLineProgram;
	pProgram->UseProgram();
	if(pProgram->m_LocZoomFactor != -1) pProgram->SetUniform(pProgram->m_LocZoomFactor, (float)(pCommand->m_ZoomScreenRatio < .5f ? .5f : pCommand->m_ZoomScreenRatio));
	
	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);	
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float*)&pCommand->m_Dir);
	
	glBindVertexArray(VisualObject.m_VertArrayID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VisualObject.m_IndexBufferID);
		
	glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_IndexDrawNum, GL_UNSIGNED_INT, pCommand->m_pIndicesOffset, pCommand->m_DrawNum);
}
	
void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderVertexArray(const CCommandBuffer::SCommand_RenderVertexArray *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	//if space not there return
	if(Index >= m_VisualObjects.size())
		return;
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	if(VisualObject.m_VertArrayID == 0 || VisualObject.m_IndexBufferID == 0) return;

	GLsizei* DrawCount = new GLsizei[pCommand->m_IndicesDrawNum];
	GLvoid** Offsets = new GLvoid*[pCommand->m_IndicesDrawNum];
	int c = 0;
	for (int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
	{
		if (pCommand->m_pIndicesOffsets[i].m_DrawCount == 0) continue;
		DrawCount[c] = pCommand->m_pIndicesOffsets[i].m_DrawCount;
		Offsets[c++] = pCommand->m_pIndicesOffsets[i].m_Offset;
	}
	if (c == 0)
	{
		delete[] DrawCount;
		delete[] Offsets;
		return; //nothing to draw
	}
	
	CGLSLTileProgram* pProgram = NULL;
	if(VisualObject.m_IsTextured)
	{
		pProgram = m_pTileProgramTextured;
	}
	else pProgram = m_pTileProgram;
	
	pProgram->UseProgram();
	if(pProgram->m_LocZoomFactor != -1) pProgram->SetUniform(pProgram->m_LocZoomFactor, (float)(pCommand->m_ZoomScreenRatio < .5f ? .5f : pCommand->m_ZoomScreenRatio));
	
	SetState(pCommand->m_State, pProgram);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float*)&pCommand->m_Color);
		
	glBindVertexArray(VisualObject.m_VertArrayID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VisualObject.m_IndexBufferID);
	
	//for some reasons this function seems not to be in the coreprofile for quite some gpus
	//glMultiDrawElements(GL_TRIANGLES, DrawCount, GL_UNSIGNED_INT, Offsets, c);
	for (int i = 0; i < c; ++i)
	{
		glDrawElements(GL_TRIANGLES, DrawCount[i], GL_UNSIGNED_INT, Offsets[i]);
	}

	delete[] DrawCount;
	delete[] Offsets;
}
	
void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateVertBuffer(const CCommandBuffer::SCommand_CreateVertexBufferObject *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	//create necessary space
	if(Index >= m_VisualObjects.size())
	{
		for(int i = m_VisualObjects.size(); i < Index + 1; ++i)
		{
			m_VisualObjects.push_back(SVisualObject());
		}
	}
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	VisualObject.m_NumElements = pCommand->m_NumVertices;
	VisualObject.m_IsTextured = pCommand->m_IsTextured;
	
	glGenBuffers(1, &VisualObject.m_VertBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, VisualObject.m_VertBufferID);
	
	GLsizeiptr size = (VisualObject.m_IsTextured ? (sizeof(float) + sizeof(unsigned char) * 2) : (sizeof(float)));	
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(VisualObject.m_NumElements * size), pCommand->m_Elements, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_AppendVertBuffer(const CCommandBuffer::SCommand_AppendVertexBufferObject *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	
	//if space not there return
	if(Index >= m_VisualObjects.size())
	{
		return;
	}
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	
	glBindBuffer(GL_COPY_READ_BUFFER, VisualObject.m_VertBufferID);
	GLuint NewVerBufferID;
	glGenBuffers(1, &NewVerBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, NewVerBufferID);
	GLsizeiptr size = (VisualObject.m_IsTextured ? (sizeof(float) + sizeof(unsigned char) * 2) : (sizeof(float)));
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)((VisualObject.m_NumElements + pCommand->m_NumVertices) * size), NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)(VisualObject.m_NumElements * size));
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(VisualObject.m_NumElements * size), (GLsizeiptr)(pCommand->m_NumVertices * size), pCommand->m_Elements);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	
	glDeleteBuffers(1, &VisualObject.m_VertBufferID);	
	VisualObject.m_VertBufferID = NewVerBufferID;

	VisualObject.m_NumElements += pCommand->m_NumVertices;	
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateVertArray(const CCommandBuffer::SCommand_CreateVertexArrayObject *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	//if space not there return
	if(Index >= m_VisualObjects.size())
		return;
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	
	glGenVertexArrays(1, &VisualObject.m_VertArrayID);
	glBindVertexArray(VisualObject.m_VertArrayID);
	
	glEnableVertexAttribArray(0);
	if(VisualObject.m_IsTextured)
	{
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, VisualObject.m_VertBufferID);	
	GLsizei stride = sizeof(float) * 2 + sizeof(unsigned char) * 2 * 2;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)(VisualObject.m_IsTextured ? stride : 0) , 0);
	if(VisualObject.m_IsTextured)
	{
		glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, (GLsizei)(VisualObject.m_IsTextured ? stride : 0), (void*)(sizeof(float)*2));
		glVertexAttribIPointer(2, 2, GL_UNSIGNED_BYTE, (GLsizei)(VisualObject.m_IsTextured ? stride : 0), (void*)(sizeof(float)*2 + sizeof(unsigned char)*2));
	}
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	//TODO: destroy the VBO here? it shouldn't be needed anymore	
	glDeleteBuffers(1, &VisualObject.m_VertBufferID);
	VisualObject.m_VertBufferID = 0;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateIndexBuffer(const CCommandBuffer::SCommand_CreateIndexBufferObject *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	
	//if space not there return
	if(Index >= m_VisualObjects.size())
	{
		return;
	}
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	VisualObject.m_NumIndices = pCommand->m_NumIndices;
	
	glGenBuffers(1, &VisualObject.m_IndexBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VisualObject.m_IndexBufferID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(VisualObject.m_NumIndices * sizeof(unsigned int)), pCommand->m_Indices, GL_STATIC_DRAW);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_AppendIndexBuffer(const CCommandBuffer::SCommand_AppendIndexBufferObject *pCommand)
{
	int Index = pCommand->m_VisualObjectIDX;
	
	//if space not there return
	if(Index >= m_VisualObjects.size())
	{
		return;
	}
	
	SVisualObject& VisualObject = m_VisualObjects[Index];
	
	glBindBuffer(GL_COPY_READ_BUFFER, VisualObject.m_IndexBufferID);
	GLuint NewIndexBufferID;
	glGenBuffers(1, &NewIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, NewIndexBufferID);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)((VisualObject.m_NumIndices + pCommand->m_NumIndices) * sizeof(unsigned int)), NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)(VisualObject.m_NumIndices * sizeof(unsigned int)));
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(VisualObject.m_NumIndices * sizeof(unsigned int)), (GLsizeiptr)(pCommand->m_NumIndices * sizeof(unsigned int)), pCommand->m_Indices);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	
	glDeleteBuffers(1, &VisualObject.m_IndexBufferID);	
	VisualObject.m_IndexBufferID = NewIndexBufferID;
	
	VisualObject.m_NumIndices += pCommand->m_NumIndices;	
}

// ------------ CCommandProcessorFragment_SDL

void CCommandProcessorFragment_SDL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);

	// set some default settings
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);
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

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight)
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

	m_UseOpenGL3_3 = false;
	if (!g_Config.m_GfxForceOldOpenGL && SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0)
	{
		if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) == 0 && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) == 0)
		{
			m_UseOpenGL3_3 = true;
			int vMaj, vMin;
			SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &vMaj);
			SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &vMin);
			dbg_msg("gfx", "Using OpenGL version %d.%d.", vMaj, vMin);
		}
		else {
			dbg_msg("gfx", "Couldn't create OpenGL 3.3 context.");
		}
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
#ifdef __ANDROID__
	*pWidth = *pDesktopWidth;
	*pHeight = *pDesktopHeight;
/*
#elif defined(CONF_FAMILY_WINDOWS)
	if(*pWidth == 0 || *pHeight == 0)
	{
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
	}
	else
	{
		float dpi = -1;
		SDL_GetDisplayDPI(0, NULL, &dpi, NULL);
		if(dpi > 0)
		{
			*pWidth = *pWidth * 96 / dpi;
			*pHeight = *pHeight * 96 / dpi;
		}
	}
*/
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
#if defined(CONF_PLATFORM_MACOSX)	// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
		SdlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;	// always use "fake" fullscreen
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
#else
		SdlFlags |= SDL_WINDOW_FULLSCREEN;
#endif
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
	
	//support graphic cards that are pretty old(and linux)
	glewExperimental = GL_TRUE;
	if (GLEW_OK != glewInit())
		return -1;

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
		CmdBuffer.AddCommand(CmdOpenGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
	} else {
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

	SDL_ShowWindow(m_pWindow);
	SetWindowScreen(g_Config.m_GfxScreen);

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
