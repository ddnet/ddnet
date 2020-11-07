#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
// For FlashWindowEx, FLASHWINFO, FLASHW_TRAY
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#endif

#include <GL/glew.h>
#include <engine/storage.h>

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_syswm.h"
#include <base/detect.h>
#include <base/math.h>
#include <cmath>
#include <stdlib.h>

#if defined(SDL_VIDEO_DRIVER_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <engine/shared/config.h>

#include <base/tl/threading.h>

#if defined(CONF_VIDEORECORDER)
#include "video.h"
#endif

#include "backend_sdl.h"
#include "graphics_threaded.h"

#include "opengl_sl.h"
#include "opengl_sl_program.h"

#ifdef __MINGW32__
extern "C" {
int putenv(const char *);
}
#endif

/*
	sync_barrier - creates a full hardware fence
*/
#if defined(__GNUC__)
inline void sync_barrier()
{
	__sync_synchronize();
}
#elif defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
inline void sync_barrier()
{
	MemoryBarrier();
}
#else
#error missing atomic implementation for this compiler
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
#if defined(CONF_VIDEORECORDER)
		if(IVideo::Current())
			IVideo::Current()->NextVideoFrameThread();
#endif
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
	m_pThread = thread_init(ThreadFunc, this, "CGraphicsBackend_Threaded");
	m_BufferDone.signal();
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	m_Activity.signal();
	if(m_pThread)
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

static bool Texture2DTo3D(void *pImageBuffer, int ImageWidth, int ImageHeight, int ImageColorChannelCount, int SplitCountWidth, int SplitCountHeight, void *pTarget3DImageData, int &Target3DImageWidth, int &Target3DImageHeight)
{
	Target3DImageWidth = ImageWidth / SplitCountWidth;
	Target3DImageHeight = ImageHeight / SplitCountHeight;

	size_t FullImageWidth = (size_t)ImageWidth * ImageColorChannelCount;

	for(int Y = 0; Y < SplitCountHeight; ++Y)
	{
		for(int X = 0; X < SplitCountWidth; ++X)
		{
			for(int Y3D = 0; Y3D < Target3DImageHeight; ++Y3D)
			{
				int DepthIndex = X + Y * SplitCountWidth;

				size_t TargetImageFullWidth = (size_t)Target3DImageWidth * ImageColorChannelCount;
				size_t TargetImageFullSize = (size_t)TargetImageFullWidth * Target3DImageHeight;
				ptrdiff_t ImageOffset = (ptrdiff_t)(((size_t)Y * FullImageWidth * (size_t)Target3DImageHeight) + ((size_t)Y3D * FullImageWidth) + ((size_t)X * TargetImageFullWidth));
				ptrdiff_t TargetImageOffset = (ptrdiff_t)(TargetImageFullSize * (size_t)DepthIndex + ((size_t)Y3D * TargetImageFullWidth));
				mem_copy(((uint8_t *)pTarget3DImageData) + TargetImageOffset, ((uint8_t *)pImageBuffer) + (ptrdiff_t)(ImageOffset), TargetImageFullWidth);
			}
		}
	}

	return true;
}

// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand)
{
	pCommand->m_pSemaphore->signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
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

static int HighestBit(int OfVar)
{
	if(!OfVar)
		return 0;

	int RetV = 1;

	while(OfVar >>= 1)
		RetV <<= 1;

	return RetV;
}

int CCommandProcessorFragment_OpenGL::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return GL_ALPHA;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return GL_RGBA;
	return GL_RGBA;
}

int CCommandProcessorFragment_OpenGL::TexFormatToImageColorChannelCount(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return 3;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return 1;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return 4;
	return 4;
}

static float CubicHermite(float A, float B, float C, float D, float t)
{
	float a = -A / 2.0f + (3.0f * B) / 2.0f - (3.0f * C) / 2.0f + D / 2.0f;
	float b = A - (5.0f * B) / 2.0f + 2.0f * C - D / 2.0f;
	float c = -A / 2.0f + C / 2.0f;
	float d = B;

	return (a * t * t * t) + (b * t * t) + (c * t) + d;
}

static void GetPixelClamped(uint8_t *pSourceImage, int x, int y, uint32_t W, uint32_t H, size_t BPP, uint8_t aTmp[])
{
	x = clamp<int>(x, 0, (int)W - 1);
	y = clamp<int>(y, 0, (int)H - 1);

	for(size_t i = 0; i < BPP; i++)
	{
		aTmp[i] = pSourceImage[x * BPP + (W * BPP * y) + i];
	}
}

static void SampleBicubic(uint8_t *pSourceImage, float u, float v, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[])
{
	float X = (u * W) - 0.5f;
	int xInt = (int)X;
	float xFract = X - floorf(X);

	float Y = (v * H) - 0.5f;
	int yInt = (int)Y;
	float yFract = Y - floorf(Y);

	uint8_t PX00[4];
	uint8_t PX10[4];
	uint8_t PX20[4];
	uint8_t PX30[4];

	uint8_t PX01[4];
	uint8_t PX11[4];
	uint8_t PX21[4];
	uint8_t PX31[4];

	uint8_t PX02[4];
	uint8_t PX12[4];
	uint8_t PX22[4];
	uint8_t PX32[4];

	uint8_t PX03[4];
	uint8_t PX13[4];
	uint8_t PX23[4];
	uint8_t PX33[4];

	GetPixelClamped(pSourceImage, xInt - 1, yInt - 1, W, H, BPP, PX00);
	GetPixelClamped(pSourceImage, xInt + 0, yInt - 1, W, H, BPP, PX10);
	GetPixelClamped(pSourceImage, xInt + 1, yInt - 1, W, H, BPP, PX20);
	GetPixelClamped(pSourceImage, xInt + 2, yInt - 1, W, H, BPP, PX30);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 0, W, H, BPP, PX01);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 0, W, H, BPP, PX11);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 0, W, H, BPP, PX21);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 0, W, H, BPP, PX31);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 1, W, H, BPP, PX02);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 1, W, H, BPP, PX12);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 1, W, H, BPP, PX22);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 1, W, H, BPP, PX32);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 2, W, H, BPP, PX03);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 2, W, H, BPP, PX13);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 2, W, H, BPP, PX23);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 2, W, H, BPP, PX33);

	for(size_t i = 0; i < BPP; i++)
	{
		float Clmn0 = CubicHermite(PX00[i], PX10[i], PX20[i], PX30[i], xFract);
		float Clmn1 = CubicHermite(PX01[i], PX11[i], PX21[i], PX31[i], xFract);
		float Clmn2 = CubicHermite(PX02[i], PX12[i], PX22[i], PX32[i], xFract);
		float Clmn3 = CubicHermite(PX03[i], PX13[i], PX23[i], PX33[i], xFract);

		float Valuef = CubicHermite(Clmn0, Clmn1, Clmn2, Clmn3, yFract);

		Valuef = clamp<float>(Valuef, 0.0f, 255.0f);

		aSample[i] = (uint8_t)Valuef;
	}
}

static void ResizeImage(uint8_t *pSourceImage, uint32_t SW, uint32_t SH, uint8_t *pDestinationImage, uint32_t W, uint32_t H, size_t BPP)
{
	uint8_t aSample[4];
	int y, x;

	for(y = 0; y < (int)H; ++y)
	{
		float v = (float)y / (float)(H - 1);

		for(x = 0; x < (int)W; ++x)
		{
			float u = (float)x / (float)(W - 1);
			SampleBicubic(pSourceImage, u, v, SW, SH, BPP, aSample);

			for(size_t i = 0; i < BPP; ++i)
			{
				pDestinationImage[x * BPP + ((W * BPP) * y) + i] = aSample[i];
			}
		}
	}
}

void *CCommandProcessorFragment_OpenGL::Resize(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;

	int Bpp = TexFormatToImageColorChannelCount(Format);

	// All calls to Resize() ensure width & height are > 0, Bpp is always > 0,
	// thus no allocation size 0 possible.
	pTmpData = (unsigned char *)malloc((size_t)NewWidth * NewHeight * Bpp); // NOLINT(clang-analyzer-optin.portability.UnixAPI)

	ResizeImage((uint8_t *)pData, Width, Height, (uint8_t *)pTmpData, NewWidth, NewHeight, Bpp);

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::SState &State, bool Use2DArrayTextures)
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
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		if(!Use2DArrayTextures)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);

			if(m_aTextures[State.m_Texture].m_LastWrapMode != State.m_WrapMode)
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
				m_aTextures[State.m_Texture].m_LastWrapMode = State.m_WrapMode;
			}
		}
		else
		{
			if(m_Has2DArrayTextures)
			{
				if(!m_HasShaders)
					glEnable(m_2DArrayTarget);
				glBindTexture(m_2DArrayTarget, m_aTextures[State.m_Texture].m_Tex2DArray);
			}
			else if(m_Has3DTextures)
			{
				if(!m_HasShaders)
					glEnable(GL_TEXTURE_3D);
				glBindTexture(GL_TEXTURE_3D, m_aTextures[State.m_Texture].m_Tex2DArray);
			}
			else
			{
				dbg_msg("opengl", "Error: this call should not happen.");
			}
		}
	}

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, -10.0f, 10.f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const SCommand_Init *pCommand)
{
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
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	void *pTexData = pCommand->m_pData;
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int X = pCommand->m_X;
	int Y = pCommand->m_Y;

	if(!m_HasNPOTTextures)
	{
		float ResizeW = m_aTextures[pCommand->m_Slot].m_ResizeWidth;
		float ResizeH = m_aTextures[pCommand->m_Slot].m_ResizeHeight;
		if(ResizeW > 0 && ResizeH > 0)
		{
			int ResizedW = (int)(Width * ResizeW);
			int ResizedH = (int)(Height * ResizeH);

			void *pTmpData = Resize(Width, Height, ResizedW, ResizedH, pCommand->m_Format, static_cast<const unsigned char *>(pTexData));
			free(pTexData);
			pTexData = pTmpData;

			Width = ResizedW;
			Height = ResizedH;
		}
	}

	if(m_aTextures[pCommand->m_Slot].m_RescaleCount > 0)
	{
		int OldWidth = Width;
		int OldHeight = Height;
		for(int i = 0; i < m_aTextures[pCommand->m_Slot].m_RescaleCount; ++i)
		{
			Width >>= 1;
			Height >>= 1;

			X /= 2;
			Y /= 2;
		}

		void *pTmpData = Resize(OldWidth, OldHeight, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pTexData));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height,
		TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
	free(pTexData);
}

void CCommandProcessorFragment_OpenGL::DestroyTexture(int Slot)
{
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) - m_aTextures[Slot].m_MemSize, std::memory_order_relaxed);

	if(m_aTextures[Slot].m_Tex != 0)
	{
		glDeleteTextures(1, &m_aTextures[Slot].m_Tex);
	}

	if(m_aTextures[Slot].m_Tex2DArray != 0)
	{
		glDeleteTextures(1, &m_aTextures[Slot].m_Tex2DArray);
	}

	if(IsNewApi())
	{
		if(m_aTextures[Slot].m_Sampler != 0)
		{
			glDeleteSamplers(1, &m_aTextures[Slot].m_Sampler);
		}
		if(m_aTextures[Slot].m_Sampler2DArray != 0)
		{
			glDeleteSamplers(1, &m_aTextures[Slot].m_Sampler2DArray);
		}
	}

	m_aTextures[Slot].m_Tex = 0;
	m_aTextures[Slot].m_Sampler = 0;
	m_aTextures[Slot].m_Tex2DArray = 0;
	m_aTextures[Slot].m_Sampler2DArray = 0;
	m_aTextures[Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	DestroyTexture(pCommand->m_Slot);
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

	m_aTextures[pCommand->m_Slot].m_ResizeWidth = -1.f;
	m_aTextures[pCommand->m_Slot].m_ResizeHeight = -1.f;

	if(!m_HasNPOTTextures)
	{
		int PowerOfTwoWidth = HighestBit(Width);
		int PowerOfTwoHeight = HighestBit(Height);
		if(Width != PowerOfTwoWidth || Height != PowerOfTwoHeight)
		{
			void *pTmpData = Resize(Width, Height, PowerOfTwoWidth, PowerOfTwoHeight, pCommand->m_Format, static_cast<const unsigned char *>(pTexData));
			free(pTexData);
			pTexData = pTmpData;

			m_aTextures[pCommand->m_Slot].m_ResizeWidth = (float)PowerOfTwoWidth / (float)Width;
			m_aTextures[pCommand->m_Slot].m_ResizeHeight = (float)PowerOfTwoHeight / (float)Height;

			Width = PowerOfTwoWidth;
			Height = PowerOfTwoHeight;
		}
	}

	int RescaleCount = 0;
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB || pCommand->m_Format == CCommandBuffer::TEXFORMAT_ALPHA)
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
		else if(pCommand->m_Format != CCommandBuffer::TEXFORMAT_ALPHA && (Width > 16 && Height > 16 && (pCommand->m_Flags & CCommandBuffer::TEXFLAG_QUALITY) == 0))
		{
			Width >>= 1;
			Height >>= 1;
			++RescaleCount;
			NeedsResize = true;
		}

		if(NeedsResize)
		{
			void *pTmpData = Resize(OldWidth, OldHeight, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pTexData));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_aTextures[pCommand->m_Slot].m_Width = Width;
	m_aTextures[pCommand->m_Slot].m_Height = Height;
	m_aTextures[pCommand->m_Slot].m_RescaleCount = RescaleCount;

	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
		case GL_RGB: StoreOglformat = GL_COMPRESSED_RGB_ARB; break;
		case GL_ALPHA: StoreOglformat = GL_COMPRESSED_ALPHA_ARB; break;
		case GL_RGBA: StoreOglformat = GL_COMPRESSED_RGBA_ARB; break;
		default: StoreOglformat = GL_COMPRESSED_RGBA_ARB;
		}
	}

	if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
	{
		glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);
	}

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS || !m_HasMipMaps)
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}
	}
	else
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			if(m_OpenGLTextureLodBIAS != 0)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}

		int Flag2DArrayTexture = (CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE | CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER);
		int Flag3DTexture = (CCommandBuffer::TEXFLAG_TO_3D_TEXTURE | CCommandBuffer::TEXFLAG_TO_3D_TEXTURE_SINGLE_LAYER);
		if((pCommand->m_Flags & (Flag2DArrayTexture | Flag3DTexture)) != 0)
		{
			bool Is3DTexture = (pCommand->m_Flags & Flag3DTexture) != 0;

			glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2DArray);

			GLenum Target = GL_TEXTURE_3D;

			if(Is3DTexture)
			{
				Target = GL_TEXTURE_3D;
			}
			else
			{
				Target = m_2DArrayTarget;
			}

			glBindTexture(Target, m_aTextures[pCommand->m_Slot].m_Tex2DArray);

			if(IsNewApi())
			{
				glGenSamplers(1, &m_aTextures[pCommand->m_Slot].m_Sampler2DArray);
				glBindSampler(0, m_aTextures[pCommand->m_Slot].m_Sampler2DArray);
			}

			glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if(Is3DTexture)
			{
				glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				if(IsNewApi())
					glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(Target, GL_GENERATE_MIPMAP, GL_TRUE);
				if(IsNewApi())
					glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}

			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
			if(m_OpenGLTextureLodBIAS != 0)
				glTexParameterf(Target, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));

			if(IsNewApi())
			{
				glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
				if(m_OpenGLTextureLodBIAS != 0)
					glSamplerParameterf(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));

				glBindSampler(0, 0);
			}

			int ImageColorChannels = TexFormatToImageColorChannelCount(pCommand->m_Format);

			uint8_t *p3DImageData = NULL;

			bool IsSingleLayer = (pCommand->m_Flags & (CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER | CCommandBuffer::TEXFLAG_TO_3D_TEXTURE_SINGLE_LAYER)) != 0;

			if(!IsSingleLayer)
				p3DImageData = (uint8_t *)malloc((size_t)ImageColorChannels * Width * Height);
			int Image3DWidth, Image3DHeight;

			int ConvertWidth = Width;
			int ConvertHeight = Height;

			if(!IsSingleLayer)
			{
				if(ConvertWidth == 0 || (ConvertWidth % 16) != 0 || ConvertHeight == 0 || (ConvertHeight % 16) != 0)
				{
					dbg_msg("gfx", "3D/2D array texture was resized");
					int NewWidth = maximum<int>(HighestBit(ConvertWidth), 16);
					int NewHeight = maximum<int>(HighestBit(ConvertHeight), 16);
					uint8_t *pNewTexData = (uint8_t *)Resize(ConvertWidth, ConvertHeight, NewWidth, NewHeight, pCommand->m_Format, (const uint8_t *)pTexData);

					ConvertWidth = NewWidth;
					ConvertHeight = NewHeight;

					free(pTexData);
					pTexData = pNewTexData;
				}
			}

			if(IsSingleLayer || (Texture2DTo3D(pTexData, ConvertWidth, ConvertHeight, ImageColorChannels, 16, 16, p3DImageData, Image3DWidth, Image3DHeight)))
			{
				if(IsSingleLayer)
				{
					glTexImage3D(Target, 0, StoreOglformat, ConvertWidth, ConvertHeight, 1, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
				}
				else
				{
					glTexImage3D(Target, 0, StoreOglformat, Image3DWidth, Image3DHeight, 256, 0, Oglformat, GL_UNSIGNED_BYTE, p3DImageData);
				}

				/*if(StoreOglformat == GL_R8)
				{
					//Bind the texture 2D.
					GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
					glTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
				}*/
			}

			if(!IsSingleLayer)
				free(p3DImageData);
		}
	}

	// This is the initial value for the wrap modes
	m_aTextures[pCommand->m_Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_aTextures[pCommand->m_Slot].m_MemSize = Width * Height * pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_aTextures[pCommand->m_Slot].m_MemSize += Width * Height * pCommand->m_PixelSize;
	}
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) + m_aTextures[pCommand->m_Slot].m_MemSize, std::memory_order_relaxed);

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

	glVertexPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 2);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 4);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount * 4);
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
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)malloc((size_t)w * (h + 1) * 3);
	unsigned char *pTempRow = pPixelData + w * h * 3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h / 2; y++)
	{
		mem_copy(pTempRow, pPixelData + y * w * 3, w * 3);
		mem_copy(pPixelData + y * w * 3, pPixelData + (h - y - 1) * w * 3, w * 3);
		mem_copy(pPixelData + (h - y - 1) * w * 3, pTempRow, w * 3);
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
	m_HasShaders = false;
}

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
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
	case CCommandBuffer::CMD_TEXTURE_UPDATE:
		Cmd_Texture_Update(static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand));
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
	case CCommandBuffer::CMD_SCREENSHOT:
		Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_Screenshot *>(pBaseCommand));
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
	case CCommandBuffer::CMD_RENDER_BORDER_TILE_LINE: Cmd_RenderBorderTileLine(static_cast<const CCommandBuffer::SCommand_RenderBorderTileLine *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_LAYER: Cmd_RenderQuadLayer(static_cast<const CCommandBuffer::SCommand_RenderQuadLayer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_TEXT: Cmd_RenderText(static_cast<const CCommandBuffer::SCommand_RenderText *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_TEXT_STREAM: Cmd_RenderTextStream(static_cast<const CCommandBuffer::SCommand_RenderTextStream *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER: Cmd_RenderQuadContainer(static_cast<const CCommandBuffer::SCommand_RenderQuadContainer *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_EX: Cmd_RenderQuadContainerEx(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerEx *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE: Cmd_RenderQuadContainerAsSpriteMultiple(static_cast<const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_OpenGL2

void CCommandProcessorFragment_OpenGL2::UseProgram(CGLSLTWProgram *pProgram)
{
	pProgram->UseProgram();
}

bool CCommandProcessorFragment_OpenGL2::IsAndUpdateTextureSlotBound(int IDX, int Slot, bool Is2DArray)
{
	if(m_TextureSlotBoundToUnit[IDX].m_TextureSlot == Slot && m_TextureSlotBoundToUnit[IDX].m_Is2DArray == Is2DArray)
		return true;
	else
	{
		//the texture slot uses this index now
		m_TextureSlotBoundToUnit[IDX].m_TextureSlot = Slot;
		m_TextureSlotBoundToUnit[IDX].m_Is2DArray = Is2DArray;
		return false;
	}
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
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		int Slot = 0;
		if(m_UseMultipleTextureUnits)
		{
			Slot = State.m_Texture % m_MaxTextureUnits;
			if(!IsAndUpdateTextureSlotBound(Slot, State.m_Texture, Use2DArrayTextures))
			{
				glActiveTexture(GL_TEXTURE0 + Slot);
				if(!Use2DArrayTextures)
				{
					glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
					if(IsNewApi())
						glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler);
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D_ARRAY, m_aTextures[State.m_Texture].m_Tex2DArray);
					if(IsNewApi())
						glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler2DArray);
				}
			}
		}
		else
		{
			Slot = 0;
			if(!Use2DArrayTextures)
			{
				if(!IsNewApi() && !m_HasShaders)
					glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
				if(IsNewApi())
					glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler);
			}
			else
			{
				if(!m_Has2DArrayTextures)
				{
					if(!IsNewApi() && !m_HasShaders)
						glEnable(GL_TEXTURE_3D);
					glBindTexture(GL_TEXTURE_3D, m_aTextures[State.m_Texture].m_Tex2DArray);
					if(IsNewApi())
						glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler2DArray);
				}
				else
				{
					if(!IsNewApi() && !m_HasShaders)
						glEnable(m_2DArrayTarget);
					glBindTexture(m_2DArrayTarget, m_aTextures[State.m_Texture].m_Tex2DArray);
					if(IsNewApi())
						glBindSampler(Slot, m_aTextures[State.m_Texture].m_Sampler2DArray);
				}
			}
		}
		if(pProgram->m_LocIsTextured != -1)
		{
			if(pProgram->m_LastIsTextured != 1)
			{
				pProgram->SetUniform(pProgram->m_LocIsTextured, 1);
				pProgram->m_LastIsTextured = 1;
			}
		}

		if(pProgram->m_LastTextureSampler != Slot)
		{
			pProgram->SetUniform(pProgram->m_LocTextureSampler, Slot);
			pProgram->m_LastTextureSampler = Slot;
		}

		if(m_aTextures[State.m_Texture].m_LastWrapMode != State.m_WrapMode && !Use2DArrayTextures)
		{
			switch(State.m_WrapMode)
			{
			case CCommandBuffer::WRAP_REPEAT:
				if(IsNewApi())
				{
					glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
				}
				break;
			case CCommandBuffer::WRAP_CLAMP:
				if(IsNewApi())
				{
					glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glSamplerParameteri(m_aTextures[State.m_Texture].m_Sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
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
				pProgram->SetUniform(pProgram->m_LocIsTextured, 0);
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
		glUniformMatrix4x2fv(pProgram->m_LocPos, 1, true, (float *)&m);
	}
}

bool CCommandProcessorFragment_OpenGL2::DoAnalyzeStep(size_t StepN, size_t CheckCount, size_t VerticesCount, uint8_t aFakeTexture[], size_t SingleImageSize)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int Slot = 0;
	if(m_HasShaders)
	{
		CGLSLTWProgram *pProgram = m_pPrimitive3DProgramTextured;
		if(StepN == 1)
			pProgram = m_pTileProgramTextured;
		UseProgram(pProgram);

		pProgram->SetUniform(pProgram->m_LocTextureSampler, Slot);

		if(StepN == 1)
		{
			float aColor[4] = {1.f, 1.f, 1.f, 1.f};
			pProgram->SetUniformVec4(((CGLSLTileProgram *)pProgram)->m_LocColor, 1, aColor);
		}

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

	GLuint BufferID = 0;
	if(StepN == 1 && m_HasShaders)
	{
		glGenBuffers(1, &BufferID);
		glBindBuffer(GL_ARRAY_BUFFER, BufferID);
		glBufferData(GL_ARRAY_BUFFER, VerticesCount * sizeof((m_aStreamVertices[0])), m_aStreamVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof((m_aStreamVertices[0])), 0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof((m_aStreamVertices[0])), (GLvoid *)(sizeof(vec4) + sizeof(vec2)));
	}
	else
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glVertexPointer(2, GL_FLOAT, sizeof(m_aStreamVertices[0]), m_aStreamVertices);
		glColorPointer(4, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2)));
		glTexCoordPointer(3, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(vec4)));
	}

	glDrawArrays(GL_QUADS, 0, VerticesCount);

	if(StepN == 1 && m_HasShaders)
	{
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &BufferID);
	}
	else
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

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

	//everything build up, now do the analyze steps
	bool NoError = DoAnalyzeStep(0, CheckCount, VertexCount, pFakeTexture, SingleImageSize);
	if(NoError && m_HasShaders)
		NoError &= DoAnalyzeStep(1, CheckCount, VertexCount, pFakeTexture, SingleImageSize);

	glDeleteTextures(1, &FakeTexture);
	free(pFakeTexture);

	return NoError;
}

void CCommandProcessorFragment_OpenGL2::Cmd_Init(const SCommand_Init *pCommand)
{
	CCommandProcessorFragment_OpenGL::Cmd_Init(pCommand);

	m_OpenGLTextureLodBIAS = g_Config.m_GfxOpenGLTextureLODBIAS;

	m_HasShaders = pCommand->m_pCapabilities->m_ShaderSupport;

	bool HasAllFunc = true;
	if(m_HasShaders)
	{
		HasAllFunc &= (glUniformMatrix4x2fv != NULL) && (glGenBuffers != NULL);
		HasAllFunc &= (glBindBuffer != NULL) && (glBufferData != NULL);
		HasAllFunc &= (glEnableVertexAttribArray != NULL) && (glVertexAttribPointer != NULL);
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

	bool AnalysisCorrect = true;
	if(HasAllFunc)
	{
		if(m_HasShaders)
		{
			m_pTileProgram = new CGLSLTileProgram;
			m_pTileProgramTextured = new CGLSLTileProgram;
			m_pPrimitive3DProgram = new CGLSLPrimitiveProgram;
			m_pPrimitive3DProgramTextured = new CGLSLPrimitiveProgram;

			CGLSLCompiler ShaderCompiler(g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch);
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

				glBindAttribLocation(m_pTileProgram->GetProgramID(), 0, "inVertex");

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

				glBindAttribLocation(m_pTileProgram->GetProgramID(), 0, "inVertex");
				glBindAttribLocation(m_pTileProgram->GetProgramID(), 1, "inVertexTexCoord");

				m_pTileProgramTextured->LinkProgram();

				UseProgram(m_pTileProgramTextured);

				m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("gPos");
				m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("gTextureSampler");
				m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("gVertColor");
			}

			glUseProgram(0);
		}

		if(g_Config.m_Gfx3DTextureAnalysisDone == 0)
		{
			AnalysisCorrect = IsTileMapAnalysisSucceeded();
			if(AnalysisCorrect)
			{
				g_Config.m_Gfx3DTextureAnalysisDone = 1;
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
	}
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand)
{
	if(m_HasShaders)
	{
		CGLSLPrimitiveProgram *pProgram = NULL;
		if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
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
	//create necessary space
	if((size_t)Index >= m_BufferObjectIndices.size())
	{
		for(int i = m_BufferObjectIndices.size(); i < Index + 1; ++i)
		{
			m_BufferObjectIndices.push_back(SBufferObject(0));
		}
	}

	GLuint VertBufferID = 0;

	if(m_HasShaders)
	{
		glGenBuffers(1, &VertBufferID);
		glBindBuffer(GL_COPY_WRITE_BUFFER, VertBufferID);
		glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	SBufferObject &BufferObject = m_BufferObjectIndices[Index];
	BufferObject.m_BufferObjectID = VertBufferID;
	BufferObject.m_DataSize = pCommand->m_DataSize;
	BufferObject.m_pData = malloc(pCommand->m_DataSize);
	if(pUploadData)
		mem_copy(BufferObject.m_pData, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_BufferObjectIndices[Index];

	if(m_HasShaders)
	{
		glBindBuffer(GL_COPY_WRITE_BUFFER, BufferObject.m_BufferObjectID);
		glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	BufferObject.m_DataSize = pCommand->m_DataSize;
	if(BufferObject.m_pData)
		free(BufferObject.m_pData);
	BufferObject.m_pData = malloc(pCommand->m_DataSize);
	if(pUploadData)
		mem_copy(BufferObject.m_pData, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_BufferObjectIndices[Index];

	if(m_HasShaders)
	{
		glBindBuffer(GL_COPY_WRITE_BUFFER, BufferObject.m_BufferObjectID);
		glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pUploadData);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	if(pUploadData)
		mem_copy(((uint8_t *)BufferObject.m_pData) + (ptrdiff_t)pCommand->m_pOffset, pUploadData, pCommand->m_DataSize);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL2::Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand)
{
	int WriteIndex = pCommand->m_WriteBufferIndex;
	int ReadIndex = pCommand->m_ReadBufferIndex;

	SBufferObject &ReadBufferObject = m_BufferObjectIndices[ReadIndex];
	SBufferObject &WriteBufferObject = m_BufferObjectIndices[WriteIndex];

	mem_copy(((uint8_t *)WriteBufferObject.m_pData) + (ptrdiff_t)pCommand->m_pWriteOffset, ((uint8_t *)ReadBufferObject.m_pData) + (ptrdiff_t)pCommand->m_pReadOffset, pCommand->m_CopySize);

	if(m_HasShaders)
	{
		glBindBuffer(GL_COPY_WRITE_BUFFER, WriteBufferObject.m_BufferObjectID);
		glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)(pCommand->m_pWriteOffset), (GLsizeiptr)(pCommand->m_CopySize), ((uint8_t *)WriteBufferObject.m_pData) + (ptrdiff_t)pCommand->m_pWriteOffset);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}
}

void CCommandProcessorFragment_OpenGL2::Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand)
{
	int Index = pCommand->m_BufferIndex;
	SBufferObject &BufferObject = m_BufferObjectIndices[Index];

	if(m_HasShaders)
	{
		glDeleteBuffers(1, &BufferObject.m_BufferObjectID);
	}

	if(BufferObject.m_pData)
	{
		free(BufferObject.m_pData);
		BufferObject.m_pData = NULL;
	}
}

void CCommandProcessorFragment_OpenGL2::Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//create necessary space
	if((size_t)Index >= m_BufferContainers.size())
	{
		for(int i = m_BufferContainers.size(); i < Index + 1; ++i)
		{
			SBufferContainer Container;
			Container.m_ContainerInfo.m_Stride = 0;
			m_BufferContainers.push_back(Container);
		}
	}

	SBufferContainer &BufferContainer = m_BufferContainers[Index];

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];
		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL2::Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_BufferContainers[pCommand->m_BufferContainerIndex];

	BufferContainer.m_ContainerInfo.m_Attributes.clear();

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];
		BufferContainer.m_ContainerInfo.m_Attributes.push_back(Attr);
	}

	BufferContainer.m_ContainerInfo.m_Stride = pCommand->m_Stride;
}

void CCommandProcessorFragment_OpenGL2::Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand)
{
	SBufferContainer &BufferContainer = m_BufferContainers[pCommand->m_BufferContainerIndex];

	if(pCommand->m_DestroyAllBO)
	{
		for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i)
		{
			int VertBufferID = BufferContainer.m_ContainerInfo.m_Attributes[i].m_VertBufferBindingIndex;
			if(VertBufferID != -1)
			{
				for(auto &Attribute : BufferContainer.m_ContainerInfo.m_Attributes)
				{
					// set all equal ids to zero to not double delete
					if(VertBufferID == Attribute.m_VertBufferBindingIndex)
					{
						Attribute.m_VertBufferBindingIndex = -1;
					}
				}

				if(m_HasShaders)
				{
					glDeleteBuffers(1, &m_BufferObjectIndices[VertBufferID].m_BufferObjectID);
				}

				if(m_BufferObjectIndices[VertBufferID].m_pData)
				{
					free(m_BufferObjectIndices[VertBufferID].m_pData);
					m_BufferObjectIndices[VertBufferID].m_pData = NULL;
				}
			}
		}
	}

	BufferContainer.m_ContainerInfo.m_Attributes.clear();
}

void CCommandProcessorFragment_OpenGL2::Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand)
{
}

void CCommandProcessorFragment_OpenGL2::RenderBorderTileEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int DrawNum, const float *pOffset, const float *pDir, int JumpIndex)
{
	if(m_HasShaders)
	{
		CGLSLPrimitiveProgram *pProgram = NULL;
		if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
		{
			pProgram = m_pPrimitive3DProgramTextured;
		}
		else
			pProgram = m_pPrimitive3DProgram;

		UseProgram(pProgram);

		SetState(State, pProgram, true);
	}
	else
	{
		CCommandProcessorFragment_OpenGL::SetState(State, true);
	}

	bool IsTextured = BufferContainer.m_ContainerInfo.m_Attributes.size() == 2;

	SBufferObject &BufferObject = m_BufferObjectIndices[(size_t)BufferContainer.m_ContainerInfo.m_Attributes[0].m_VertBufferBindingIndex];

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	if(IsTextured)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(m_aStreamVertices[0]), m_aStreamVertices);
	glColorPointer(4, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2)));
	if(IsTextured)
		glTexCoordPointer(3, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(vec4)));

	size_t VertexCount = 0;
	for(size_t i = 0; i < DrawNum; ++i)
	{
		GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pBuffOffset)) / (6 * sizeof(unsigned int))) * 4);
		size_t SingleVertSize = (sizeof(vec2) + (IsTextured ? sizeof(vec3) : 0));
		size_t CurBufferOffset = (RealOffset)*SingleVertSize;

		for(size_t n = 0; n < 4; ++n)
		{
			int XCount = i - (int(i / JumpIndex) * JumpIndex);
			int YCount = (int(i / JumpIndex));

			ptrdiff_t VertOffset = (ptrdiff_t)(CurBufferOffset + (n * SingleVertSize));
			vec2 *pPos = (vec2 *)((uint8_t *)BufferObject.m_pData + VertOffset);

			GL_SVertexTex3D &Vertex = m_aStreamVertices[VertexCount++];
			mem_copy(&Vertex.m_Pos, pPos, sizeof(vec2));
			mem_copy(&Vertex.m_Color, pColor, sizeof(vec4));
			if(IsTextured)
			{
				vec3 *pTex = (vec3 *)((uint8_t *)BufferObject.m_pData + VertOffset + (ptrdiff_t)sizeof(vec2));
				mem_copy(&Vertex.m_Tex, pTex, sizeof(vec3));
			}

			Vertex.m_Pos.x += pOffset[0] + pDir[0] * XCount;
			Vertex.m_Pos.y += pOffset[1] + pDir[1] * YCount;

			if(VertexCount >= sizeof(m_aStreamVertices) / sizeof(m_aStreamVertices[0]))
			{
				glDrawArrays(GL_QUADS, 0, VertexCount);
				VertexCount = 0;
			}
		}
	}
	if(VertexCount > 0)
		glDrawArrays(GL_QUADS, 0, VertexCount);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if(IsTextured)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(m_HasShaders)
	{
		glUseProgram(0);
	}
}

void CCommandProcessorFragment_OpenGL2::RenderBorderTileLineEmulation(SBufferContainer &BufferContainer, const CCommandBuffer::SState &State, const float *pColor, const char *pBuffOffset, unsigned int IndexDrawNum, unsigned int DrawNum, const float *pOffset, const float *pDir)
{
	if(m_HasShaders)
	{
		CGLSLPrimitiveProgram *pProgram = NULL;
		if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
		{
			pProgram = m_pPrimitive3DProgramTextured;
		}
		else
			pProgram = m_pPrimitive3DProgram;

		UseProgram(pProgram);

		SetState(State, pProgram, true);
	}
	else
	{
		CCommandProcessorFragment_OpenGL::SetState(State, true);
	}

	bool IsTextured = BufferContainer.m_ContainerInfo.m_Attributes.size() == 2;

	SBufferObject &BufferObject = m_BufferObjectIndices[(size_t)BufferContainer.m_ContainerInfo.m_Attributes[0].m_VertBufferBindingIndex];

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	if(IsTextured)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(m_aStreamVertices[0]), m_aStreamVertices);
	glColorPointer(4, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2)));
	if(IsTextured)
		glTexCoordPointer(3, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(vec4)));

	size_t VertexCount = 0;
	for(size_t i = 0; i < DrawNum; ++i)
	{
		GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pBuffOffset)) / (6 * sizeof(unsigned int))) * 4);
		size_t SingleVertSize = (sizeof(vec2) + (IsTextured ? sizeof(vec3) : 0));
		size_t CurBufferOffset = (RealOffset)*SingleVertSize;
		size_t VerticesPerLine = (size_t)IndexDrawNum / 6;

		for(size_t n = 0; n < 4 * (size_t)VerticesPerLine; ++n)
		{
			ptrdiff_t VertOffset = (ptrdiff_t)(CurBufferOffset + (n * SingleVertSize));
			vec2 *pPos = (vec2 *)((uint8_t *)BufferObject.m_pData + VertOffset);

			GL_SVertexTex3D &Vertex = m_aStreamVertices[VertexCount++];
			mem_copy(&Vertex.m_Pos, pPos, sizeof(vec2));
			mem_copy(&Vertex.m_Color, pColor, sizeof(vec4));
			if(IsTextured)
			{
				vec3 *pTex = (vec3 *)((uint8_t *)BufferObject.m_pData + VertOffset + (ptrdiff_t)sizeof(vec2));
				mem_copy(&Vertex.m_Tex, pTex, sizeof(vec3));
			}

			Vertex.m_Pos.x += pOffset[0] + pDir[0] * i;
			Vertex.m_Pos.y += pOffset[1] + pDir[1] * i;

			if(VertexCount >= sizeof(m_aStreamVertices) / sizeof(m_aStreamVertices[0]))
			{
				glDrawArrays(GL_QUADS, 0, VertexCount);
				VertexCount = 0;
			}
		}
	}
	if(VertexCount > 0)
		glDrawArrays(GL_QUADS, 0, VertexCount);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if(IsTextured)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if(m_HasShaders)
	{
		glUseProgram(0);
	}
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];

	RenderBorderTileEmulation(BufferContainer, pCommand->m_State, (float *)&pCommand->m_Color, pCommand->m_pIndicesOffset, pCommand->m_DrawNum, pCommand->m_Offset, pCommand->m_Dir, pCommand->m_JumpIndex);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];

	RenderBorderTileLineEmulation(BufferContainer, pCommand->m_State, (float *)&pCommand->m_Color, pCommand->m_pIndicesOffset, pCommand->m_IndexDrawNum, pCommand->m_DrawNum, pCommand->m_Offset, pCommand->m_Dir);
}

void CCommandProcessorFragment_OpenGL2::Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];

	if(pCommand->m_IndicesDrawNum == 0)
	{
		return; //nothing to draw
	}

	if(m_HasShaders)
	{
		CGLSLTileProgram *pProgram = NULL;
		if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
		{
			pProgram = m_pTileProgramTextured;
		}
		else
			pProgram = m_pTileProgram;

		UseProgram(pProgram);

		SetState(pCommand->m_State, pProgram, true);
		pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);
	}
	else
	{
		CCommandProcessorFragment_OpenGL::SetState(pCommand->m_State, true);
	}

	bool IsTextured = BufferContainer.m_ContainerInfo.m_Attributes.size() == 2;

	SBufferObject &BufferObject = m_BufferObjectIndices[(size_t)BufferContainer.m_ContainerInfo.m_Attributes[0].m_VertBufferBindingIndex];
	if(m_HasShaders)
		glBindBuffer(GL_ARRAY_BUFFER, BufferObject.m_BufferObjectID);

	if(!m_HasShaders)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		if(IsTextured)
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if(m_HasShaders)
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_Attributes[0].m_pOffset);
		if(IsTextured)
		{
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, false, BufferContainer.m_ContainerInfo.m_Stride, BufferContainer.m_ContainerInfo.m_Attributes[1].m_pOffset);
		}

		for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
		{
			size_t RealDrawCount = (pCommand->m_pDrawCount[i] / 6) * 4;
			GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pCommand->m_pIndicesOffsets[i])) / (6 * sizeof(unsigned int))) * 4);
			glDrawArrays(GL_QUADS, RealOffset, RealDrawCount);
		}
	}
	else
	{
		glVertexPointer(2, GL_FLOAT, sizeof(m_aStreamVertices[0]), m_aStreamVertices);
		glColorPointer(4, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2)));
		if(IsTextured)
			glTexCoordPointer(3, GL_FLOAT, sizeof(m_aStreamVertices[0]), (uint8_t *)m_aStreamVertices + (ptrdiff_t)(sizeof(vec2) + sizeof(vec4)));

		size_t VertexCount = 0;
		for(int i = 0; i < pCommand->m_IndicesDrawNum; ++i)
		{
			size_t RealDrawCount = (pCommand->m_pDrawCount[i] / 6) * 4;
			GLint RealOffset = (GLint)((((size_t)(uintptr_t)(pCommand->m_pIndicesOffsets[i])) / (6 * sizeof(unsigned int))) * 4);
			size_t SingleVertSize = (sizeof(vec2) + (IsTextured ? sizeof(vec3) : 0));
			size_t CurBufferOffset = RealOffset * SingleVertSize;

			for(size_t n = 0; n < RealDrawCount; ++n)
			{
				ptrdiff_t VertOffset = (ptrdiff_t)(CurBufferOffset + (n * SingleVertSize));
				vec2 *pPos = (vec2 *)((uint8_t *)BufferObject.m_pData + VertOffset);
				GL_SVertexTex3D &Vertex = m_aStreamVertices[VertexCount++];
				mem_copy(&Vertex.m_Pos, pPos, sizeof(vec2));
				mem_copy(&Vertex.m_Color, &pCommand->m_Color, sizeof(vec4));
				if(IsTextured)
				{
					vec3 *pTex = (vec3 *)((uint8_t *)BufferObject.m_pData + VertOffset + (ptrdiff_t)sizeof(vec2));
					mem_copy(&Vertex.m_Tex, pTex, sizeof(vec3));
				}

				if(VertexCount >= sizeof(m_aStreamVertices) / sizeof(m_aStreamVertices[0]))
				{
					glDrawArrays(GL_QUADS, 0, VertexCount);
					VertexCount = 0;
				}
			}
		}
		if(VertexCount > 0)
			glDrawArrays(GL_QUADS, 0, VertexCount);
	}

	if(!m_HasShaders)
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		if(IsTextured)
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		glDisableVertexAttribArray(0);
		if(IsTextured)
			glDisableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
	}
}

// ------------ CCommandProcessorFragment_OpenGL3_3
int CCommandProcessorFragment_OpenGL3_3::TexFormatToNewOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return GL_RED;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return GL_RGBA;
	return GL_RGBA;
}

void CCommandProcessorFragment_OpenGL3_3::UseProgram(CGLSLTWProgram *pProgram)
{
	if(m_LastProgramID != pProgram->GetProgramID())
	{
		pProgram->UseProgram();
		m_LastProgramID = pProgram->GetProgramID();
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Init(const SCommand_Init *pCommand)
{
	m_OpenGLTextureLodBIAS = g_Config.m_GfxOpenGLTextureLODBIAS;

	m_UseMultipleTextureUnits = g_Config.m_GfxEnableTextureUnitOptimization;
	if(!m_UseMultipleTextureUnits)
	{
		glActiveTexture(GL_TEXTURE0);
	}

	m_Has2DArrayTextures = true;
	m_Has2DArrayTexturesAsExtension = false;
	m_2DArrayTarget = GL_TEXTURE_2D_ARRAY;
	m_Has3DTextures = false;
	m_HasMipMaps = true;
	m_HasNPOTTextures = true;
	m_HasShaders = true;

	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	m_pTextureMemoryUsage->store(0, std::memory_order_relaxed);
	m_LastBlendMode = CCommandBuffer::BLEND_ALPHA;
	m_LastClipEnable = false;
	m_pPrimitiveProgram = new CGLSLPrimitiveProgram;
	m_pTileProgram = new CGLSLTileProgram;
	m_pTileProgramTextured = new CGLSLTileProgram;
	m_pPrimitive3DProgram = new CGLSLPrimitiveProgram;
	m_pPrimitive3DProgramTextured = new CGLSLPrimitiveProgram;
	m_pBorderTileProgram = new CGLSLTileProgram;
	m_pBorderTileProgramTextured = new CGLSLTileProgram;
	m_pBorderTileLineProgram = new CGLSLTileProgram;
	m_pBorderTileLineProgramTextured = new CGLSLTileProgram;
	m_pQuadProgram = new CGLSLQuadProgram;
	m_pQuadProgramTextured = new CGLSLQuadProgram;
	m_pTextProgram = new CGLSLTextProgram;
	m_pPrimitiveExProgram = new CGLSLPrimitiveExProgram;
	m_pPrimitiveExProgramTextured = new CGLSLPrimitiveExProgram;
	m_pSpriteProgramMultiple = new CGLSLSpriteMultipleProgram;
	m_LastProgramID = 0;

	CGLSLCompiler ShaderCompiler(g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch);

	GLint CapVal;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &CapVal);
	;
	m_MaxQuadsAtOnce = minimum<int>(((CapVal - 20) / (3 * 4)), m_MaxQuadsPossible);

	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/prim.frag", GL_FRAGMENT_SHADER);

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
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_MODERN_GL", "");
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/pipeline.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pPrimitive3DProgram->CreateProgram();
		m_pPrimitive3DProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitive3DProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitive3DProgram->LinkProgram();

		UseProgram(m_pPrimitive3DProgram);

		m_pPrimitive3DProgram->m_LocPos = m_pPrimitive3DProgram->GetUniformLoc("gPos");
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_MODERN_GL", "");
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

	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);

		m_pTileProgram->CreateProgram();
		m_pTileProgram->AddShader(&VertexShader);
		m_pTileProgram->AddShader(&FragmentShader);
		m_pTileProgram->LinkProgram();

		UseProgram(m_pTileProgram);

		m_pTileProgram->m_LocPos = m_pTileProgram->GetUniformLoc("gPos");
		m_pTileProgram->m_LocColor = m_pTileProgram->GetUniformLoc("gVertColor");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pTileProgramTextured->CreateProgram();
		m_pTileProgramTextured->AddShader(&VertexShader);
		m_pTileProgramTextured->AddShader(&FragmentShader);
		m_pTileProgramTextured->LinkProgram();

		UseProgram(m_pTileProgramTextured);

		m_pTileProgramTextured->m_LocPos = m_pTileProgramTextured->GetUniformLoc("gPos");
		m_pTileProgramTextured->m_LocTextureSampler = m_pTileProgramTextured->GetUniformLoc("gTextureSampler");
		m_pTileProgramTextured->m_LocColor = m_pTileProgramTextured->GetUniformLoc("gVertColor");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileProgram->CreateProgram();
		m_pBorderTileProgram->AddShader(&VertexShader);
		m_pBorderTileProgram->AddShader(&FragmentShader);
		m_pBorderTileProgram->LinkProgram();

		UseProgram(m_pBorderTileProgram);

		m_pBorderTileProgram->m_LocPos = m_pBorderTileProgram->GetUniformLoc("gPos");
		m_pBorderTileProgram->m_LocColor = m_pBorderTileProgram->GetUniformLoc("gVertColor");
		m_pBorderTileProgram->m_LocOffset = m_pBorderTileProgram->GetUniformLoc("gOffset");
		m_pBorderTileProgram->m_LocDir = m_pBorderTileProgram->GetUniformLoc("gDir");
		m_pBorderTileProgram->m_LocJumpIndex = m_pBorderTileProgram->GetUniformLoc("gJumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER", "");
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileProgramTextured->CreateProgram();
		m_pBorderTileProgramTextured->AddShader(&VertexShader);
		m_pBorderTileProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileProgramTextured->LinkProgram();

		UseProgram(m_pBorderTileProgramTextured);

		m_pBorderTileProgramTextured->m_LocPos = m_pBorderTileProgramTextured->GetUniformLoc("gPos");
		m_pBorderTileProgramTextured->m_LocTextureSampler = m_pBorderTileProgramTextured->GetUniformLoc("gTextureSampler");
		m_pBorderTileProgramTextured->m_LocColor = m_pBorderTileProgramTextured->GetUniformLoc("gVertColor");
		m_pBorderTileProgramTextured->m_LocOffset = m_pBorderTileProgramTextured->GetUniformLoc("gOffset");
		m_pBorderTileProgramTextured->m_LocDir = m_pBorderTileProgramTextured->GetUniformLoc("gDir");
		m_pBorderTileProgramTextured->m_LocJumpIndex = m_pBorderTileProgramTextured->GetUniformLoc("gJumpIndex");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER_LINE", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileLineProgram->CreateProgram();
		m_pBorderTileLineProgram->AddShader(&VertexShader);
		m_pBorderTileLineProgram->AddShader(&FragmentShader);
		m_pBorderTileLineProgram->LinkProgram();

		UseProgram(m_pBorderTileLineProgram);

		m_pBorderTileLineProgram->m_LocPos = m_pBorderTileLineProgram->GetUniformLoc("gPos");
		m_pBorderTileLineProgram->m_LocColor = m_pBorderTileLineProgram->GetUniformLoc("gVertColor");
		m_pBorderTileLineProgram->m_LocOffset = m_pBorderTileLineProgram->GetUniformLoc("gOffset");
		m_pBorderTileLineProgram->m_LocDir = m_pBorderTileLineProgram->GetUniformLoc("gDir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_TILE_BORDER_LINE", "");
		ShaderCompiler.AddDefine("TW_TILE_TEXTURED", "");
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/tile.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pBorderTileLineProgramTextured->CreateProgram();
		m_pBorderTileLineProgramTextured->AddShader(&VertexShader);
		m_pBorderTileLineProgramTextured->AddShader(&FragmentShader);
		m_pBorderTileLineProgramTextured->LinkProgram();

		UseProgram(m_pBorderTileLineProgramTextured);

		m_pBorderTileLineProgramTextured->m_LocPos = m_pBorderTileLineProgramTextured->GetUniformLoc("gPos");
		m_pBorderTileLineProgramTextured->m_LocTextureSampler = m_pBorderTileLineProgramTextured->GetUniformLoc("gTextureSampler");
		m_pBorderTileLineProgramTextured->m_LocColor = m_pBorderTileLineProgramTextured->GetUniformLoc("gVertColor");
		m_pBorderTileLineProgramTextured->m_LocOffset = m_pBorderTileLineProgramTextured->GetUniformLoc("gOffset");
		m_pBorderTileLineProgramTextured->m_LocDir = m_pBorderTileLineProgramTextured->GetUniformLoc("gDir");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_MAX_QUADS", std::to_string(m_MaxQuadsAtOnce).c_str());
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pQuadProgram->CreateProgram();
		m_pQuadProgram->AddShader(&VertexShader);
		m_pQuadProgram->AddShader(&FragmentShader);
		m_pQuadProgram->LinkProgram();

		UseProgram(m_pQuadProgram);

		m_pQuadProgram->m_LocPos = m_pQuadProgram->GetUniformLoc("gPos");
		m_pQuadProgram->m_LocColors = m_pQuadProgram->GetUniformLoc("gVertColors");
		m_pQuadProgram->m_LocRotations = m_pQuadProgram->GetUniformLoc("gRotations");
		m_pQuadProgram->m_LocOffsets = m_pQuadProgram->GetUniformLoc("gOffsets");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		ShaderCompiler.AddDefine("TW_QUAD_TEXTURED", "");
		ShaderCompiler.AddDefine("TW_MAX_QUADS", std::to_string(m_MaxQuadsAtOnce).c_str());
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/quad.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pQuadProgramTextured->CreateProgram();
		m_pQuadProgramTextured->AddShader(&VertexShader);
		m_pQuadProgramTextured->AddShader(&FragmentShader);
		m_pQuadProgramTextured->LinkProgram();

		UseProgram(m_pQuadProgramTextured);

		m_pQuadProgramTextured->m_LocPos = m_pQuadProgramTextured->GetUniformLoc("gPos");
		m_pQuadProgramTextured->m_LocTextureSampler = m_pQuadProgramTextured->GetUniformLoc("gTextureSampler");
		m_pQuadProgramTextured->m_LocColors = m_pQuadProgramTextured->GetUniformLoc("gVertColors");
		m_pQuadProgramTextured->m_LocRotations = m_pQuadProgramTextured->GetUniformLoc("gRotations");
		m_pQuadProgramTextured->m_LocOffsets = m_pQuadProgramTextured->GetUniformLoc("gOffsets");
	}
	{
		CGLSL VertexShader;
		CGLSL FragmentShader;
		VertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/text.vert", GL_VERTEX_SHADER);
		FragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/text.frag", GL_FRAGMENT_SHADER);

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
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/primex.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/primex.frag", GL_FRAGMENT_SHADER);

		m_pPrimitiveExProgram->CreateProgram();
		m_pPrimitiveExProgram->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveExProgram->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveExProgram->LinkProgram();

		UseProgram(m_pPrimitiveExProgram);

		m_pPrimitiveExProgram->m_LocPos = m_pPrimitiveExProgram->GetUniformLoc("gPos");
		m_pPrimitiveExProgram->m_LocIsTextured = -1;
		m_pPrimitiveExProgram->m_LocTextureSampler = -1;
		m_pPrimitiveExProgram->m_LocRotation = m_pPrimitiveExProgram->GetUniformLoc("gRotation");
		m_pPrimitiveExProgram->m_LocCenter = m_pPrimitiveExProgram->GetUniformLoc("gCenter");
		m_pPrimitiveExProgram->m_LocVertciesColor = m_pPrimitiveExProgram->GetUniformLoc("gVerticesColor");

		m_pPrimitiveExProgram->SetUniform(m_pPrimitiveExProgram->m_LocRotation, 0.0f);
		float Center[2] = {0.f, 0.f};
		m_pPrimitiveExProgram->SetUniformVec2(m_pPrimitiveExProgram->m_LocCenter, 1, Center);
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		ShaderCompiler.AddDefine("TW_TEXTURED", "");
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/primex.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/primex.frag", GL_FRAGMENT_SHADER);
		ShaderCompiler.ClearDefines();

		m_pPrimitiveExProgramTextured->CreateProgram();
		m_pPrimitiveExProgramTextured->AddShader(&PrimitiveVertexShader);
		m_pPrimitiveExProgramTextured->AddShader(&PrimitiveFragmentShader);
		m_pPrimitiveExProgramTextured->LinkProgram();

		UseProgram(m_pPrimitiveExProgramTextured);

		m_pPrimitiveExProgramTextured->m_LocPos = m_pPrimitiveExProgramTextured->GetUniformLoc("gPos");
		m_pPrimitiveExProgramTextured->m_LocIsTextured = -1;
		m_pPrimitiveExProgramTextured->m_LocTextureSampler = m_pPrimitiveExProgramTextured->GetUniformLoc("gTextureSampler");
		m_pPrimitiveExProgramTextured->m_LocRotation = m_pPrimitiveExProgramTextured->GetUniformLoc("gRotation");
		m_pPrimitiveExProgramTextured->m_LocCenter = m_pPrimitiveExProgramTextured->GetUniformLoc("gCenter");
		m_pPrimitiveExProgramTextured->m_LocVertciesColor = m_pPrimitiveExProgramTextured->GetUniformLoc("gVerticesColor");

		m_pPrimitiveExProgramTextured->SetUniform(m_pPrimitiveExProgramTextured->m_LocRotation, 0.0f);
		float Center[2] = {0.f, 0.f};
		m_pPrimitiveExProgramTextured->SetUniformVec2(m_pPrimitiveExProgramTextured->m_LocCenter, 1, Center);
	}
	{
		CGLSL PrimitiveVertexShader;
		CGLSL PrimitiveFragmentShader;
		PrimitiveVertexShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/spritemulti.vert", GL_VERTEX_SHADER);
		PrimitiveFragmentShader.LoadShader(&ShaderCompiler, pCommand->m_pStorage, "shader/spritemulti.frag", GL_FRAGMENT_SHADER);

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

		float Center[2] = {0.f, 0.f};
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, Center);
	}

	m_LastStreamBuffer = 0;

	glGenBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glGenVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);
	glGenBuffers(1, &m_PrimitiveDrawBufferIDTex3D);
	glGenVertexArrays(1, &m_PrimitiveDrawVertexIDTex3D);

	m_UsePreinitializedVertexBuffer = g_Config.m_GfxUsePreinitBuffer;

	for(int i = 0; i < MAX_STREAM_BUFFER_COUNT; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[i]);
		glBindVertexArray(m_PrimitiveDrawVertexID[i]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 2));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertex), (void *)(sizeof(float) * 4));

		if(m_UsePreinitializedVertexBuffer)
			glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertex) * CCommandBuffer::MAX_VERTICES, NULL, GL_STREAM_DRAW);

		m_LastIndexBufferBound[i] = 0;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIDTex3D);
	glBindVertexArray(m_PrimitiveDrawVertexIDTex3D);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), 0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(CCommandBuffer::SVertexTex3DStream), (void *)(sizeof(float) * 2 + sizeof(unsigned char) * 4));

	if(m_UsePreinitializedVertexBuffer)
		glBufferData(GL_ARRAY_BUFFER, sizeof(CCommandBuffer::SVertexTex3DStream) * CCommandBuffer::MAX_VERTICES, NULL, GL_STREAM_DRAW);

	//query the image max size only once
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);

	//query maximum of allowed textures
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_MaxTextureUnits);
	m_TextureSlotBoundToUnit.resize(m_MaxTextureUnits);
	for(int i = 0; i < m_MaxTextureUnits; ++i)
	{
		m_TextureSlotBoundToUnit[i].m_TextureSlot = -1;
		m_TextureSlotBoundToUnit[i].m_Is2DArray = false;
	}

	glBindVertexArray(0);
	glGenBuffers(1, &m_QuadDrawIndexBufferID);
	glBindBuffer(GL_COPY_WRITE_BUFFER, m_QuadDrawIndexBufferID);

	unsigned int Indices[CCommandBuffer::MAX_VERTICES / 4 * 6];
	int Primq = 0;
	for(int i = 0; i < CCommandBuffer::MAX_VERTICES / 4 * 6; i += 6)
	{
		Indices[i] = Primq;
		Indices[i + 1] = Primq + 1;
		Indices[i + 2] = Primq + 2;
		Indices[i + 3] = Primq;
		Indices[i + 4] = Primq + 2;
		Indices[i + 5] = Primq + 3;
		Primq += 4;
	}
	glBufferData(GL_COPY_WRITE_BUFFER, sizeof(unsigned int) * CCommandBuffer::MAX_VERTICES / 4 * 6, Indices, GL_STATIC_DRAW);

	m_CurrentIndicesInBuffer = CCommandBuffer::MAX_VERTICES / 4 * 6;

	mem_zero(m_aTextures, sizeof(m_aTextures));

	m_ClearColor.r = m_ClearColor.g = m_ClearColor.b = -1.f;

	// fix the alignment to allow even 1byte changes, e.g. for alpha components
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	glUseProgram(0);

	m_pPrimitiveProgram->DeleteProgram();
	m_pBorderTileProgram->DeleteProgram();
	m_pBorderTileProgramTextured->DeleteProgram();
	m_pBorderTileLineProgram->DeleteProgram();
	m_pBorderTileLineProgramTextured->DeleteProgram();
	m_pQuadProgram->DeleteProgram();
	m_pQuadProgramTextured->DeleteProgram();
	m_pTileProgram->DeleteProgram();
	m_pTileProgramTextured->DeleteProgram();
	m_pPrimitive3DProgram->DeleteProgram();
	m_pPrimitive3DProgramTextured->DeleteProgram();
	m_pTextProgram->DeleteProgram();
	m_pPrimitiveExProgram->DeleteProgram();
	m_pPrimitiveExProgramTextured->DeleteProgram();
	m_pSpriteProgramMultiple->DeleteProgram();

	//clean up everything
	delete m_pPrimitiveProgram;
	delete m_pBorderTileProgram;
	delete m_pBorderTileProgramTextured;
	delete m_pBorderTileLineProgram;
	delete m_pBorderTileLineProgramTextured;
	delete m_pQuadProgram;
	delete m_pQuadProgramTextured;
	delete m_pTileProgram;
	delete m_pTileProgramTextured;
	delete m_pPrimitive3DProgram;
	delete m_pPrimitive3DProgramTextured;
	delete m_pTextProgram;
	delete m_pPrimitiveExProgram;
	delete m_pPrimitiveExProgramTextured;
	delete m_pSpriteProgramMultiple;

	glBindVertexArray(0);
	glDeleteBuffers(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawBufferID);
	glDeleteBuffers(1, &m_QuadDrawIndexBufferID);
	glDeleteVertexArrays(MAX_STREAM_BUFFER_COUNT, m_PrimitiveDrawVertexID);
	glDeleteBuffers(1, &m_PrimitiveDrawBufferIDTex3D);
	glDeleteVertexArrays(1, &m_PrimitiveDrawVertexIDTex3D);

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

	void *pTexData = pCommand->m_pData;
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

		void *pTmpData = Resize(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
		free(pTexData);
		pTexData = pTmpData;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, Width, Height,
		TexFormatToNewOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
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
	m_TextureSlotBoundToUnit[Slot].m_Is2DArray = false;
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
				Width >>= 1;
				Height >>= 1;
				++RescaleCount;
			} while(Width > m_MaxTexSize || Height > m_MaxTexSize);

			void *pTmpData = Resize(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
		else if(pCommand->m_Format != CCommandBuffer::TEXFORMAT_ALPHA && (Width > 16 && Height > 16 && (pCommand->m_Flags & CCommandBuffer::TEXFLAG_QUALITY) == 0))
		{
			Width >>= 1;
			Height >>= 1;
			++RescaleCount;

			void *pTmpData = Resize(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			free(pTexData);
			pTexData = pTmpData;
		}
	}
	m_aTextures[pCommand->m_Slot].m_Width = Width;
	m_aTextures[pCommand->m_Slot].m_Height = Height;
	m_aTextures[pCommand->m_Slot].m_RescaleCount = RescaleCount;

	int Oglformat = TexFormatToNewOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToNewOpenGLFormat(pCommand->m_StoreFormat);

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_COMPRESSED)
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
	int Slot = 0;
	if(m_UseMultipleTextureUnits)
	{
		Slot = pCommand->m_Slot % m_MaxTextureUnits;
		//just tell, that we using this texture now
		IsAndUpdateTextureSlotBound(Slot, pCommand->m_Slot);
		glActiveTexture(GL_TEXTURE0 + Slot);
		m_TextureSlotBoundToUnit[Slot].m_TextureSlot = -1;
		m_TextureSlotBoundToUnit[Slot].m_Is2DArray = false;
	}

	if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
	{
		glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

		glGenSamplers(1, &m_aTextures[pCommand->m_Slot].m_Sampler);
		glBindSampler(Slot, m_aTextures[pCommand->m_Slot].m_Sampler);
	}

	if(Oglformat == GL_RED)
	{
		//Bind the texture 2D.
		GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
		StoreOglformat = GL_R8;
	}

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}
	}
	else
	{
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_NO_2D_TEXTURE) == 0)
		{
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			if(m_OpenGLTextureLodBIAS != 0)
				glSamplerParameterf(m_aTextures[pCommand->m_Slot].m_Sampler, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));
			//prevent mipmap display bugs, when zooming out far
			if(Width >= 1024 && Height >= 1024)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5.f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5);
			}
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		if((pCommand->m_Flags & (CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE | CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER)) != 0)
		{
			glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2DArray);
			glBindTexture(GL_TEXTURE_2D_ARRAY, m_aTextures[pCommand->m_Slot].m_Tex2DArray);

			glGenSamplers(1, &m_aTextures[pCommand->m_Slot].m_Sampler2DArray);
			glBindSampler(Slot, m_aTextures[pCommand->m_Slot].m_Sampler2DArray);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
			if(m_OpenGLTextureLodBIAS != 0)
				glSamplerParameterf(m_aTextures[pCommand->m_Slot].m_Sampler2DArray, GL_TEXTURE_LOD_BIAS, ((GLfloat)m_OpenGLTextureLodBIAS / 1000.0f));

			int ImageColorChannels = TexFormatToImageColorChannelCount(pCommand->m_Format);

			uint8_t *p3DImageData = NULL;

			bool IsSingleLayer = (pCommand->m_Flags & CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER) != 0;

			if(!IsSingleLayer)
				p3DImageData = (uint8_t *)malloc((size_t)ImageColorChannels * Width * Height);
			int Image3DWidth, Image3DHeight;

			int ConvertWidth = Width;
			int ConvertHeight = Height;

			if(!IsSingleLayer)
			{
				if(ConvertWidth == 0 || (ConvertWidth % 16) != 0 || ConvertHeight == 0 || (ConvertHeight % 16) != 0)
				{
					dbg_msg("gfx", "3D/2D array texture was resized");
					int NewWidth = maximum<int>(HighestBit(ConvertWidth), 16);
					int NewHeight = maximum<int>(HighestBit(ConvertHeight), 16);
					uint8_t *pNewTexData = (uint8_t *)Resize(ConvertWidth, ConvertHeight, NewWidth, NewHeight, pCommand->m_Format, (const uint8_t *)pTexData);

					ConvertWidth = NewWidth;
					ConvertHeight = NewHeight;

					free(pTexData);
					pTexData = pNewTexData;
				}
			}

			if(IsSingleLayer || (Texture2DTo3D(pTexData, ConvertWidth, ConvertHeight, ImageColorChannels, 16, 16, p3DImageData, Image3DWidth, Image3DHeight)))
			{
				if(IsSingleLayer)
				{
					glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, StoreOglformat, ConvertWidth, ConvertHeight, 1, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
				}
				else
				{
					glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, StoreOglformat, Image3DWidth, Image3DHeight, 256, 0, Oglformat, GL_UNSIGNED_BYTE, p3DImageData);
				}
				glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

				if(StoreOglformat == GL_R8)
				{
					//Bind the texture 2D.
					GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
					glTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
				}
			}

			if(!IsSingleLayer)
				free(p3DImageData);
		}
	}

	// This is the initial value for the wrap modes
	m_aTextures[pCommand->m_Slot].m_LastWrapMode = CCommandBuffer::WRAP_REPEAT;

	// calculate memory usage
	m_aTextures[pCommand->m_Slot].m_MemSize = Width * Height * pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_aTextures[pCommand->m_Slot].m_MemSize += Width * Height * pCommand->m_PixelSize;
	}
	m_pTextureMemoryUsage->store(m_pTextureMemoryUsage->load(std::memory_order_relaxed) + m_aTextures[pCommand->m_Slot].m_MemSize, std::memory_order_relaxed);

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

void CCommandProcessorFragment_OpenGL3_3::UploadStreamBufferData(unsigned int PrimitiveType, const void *pVertices, size_t VertSize, unsigned int PrimitiveCount, bool AsTex3D)
{
	int Count = 0;
	switch(PrimitiveType)
	{
	case CCommandBuffer::PRIMTYPE_LINES:
		Count = PrimitiveCount * 2;
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		Count = PrimitiveCount * 3;
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		Count = PrimitiveCount * 4;
		break;
	default:
		return;
	};

	if(AsTex3D)
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferIDTex3D);
	else
		glBindBuffer(GL_ARRAY_BUFFER, m_PrimitiveDrawBufferID[m_LastStreamBuffer]);

	if(!m_UsePreinitializedVertexBuffer)
		glBufferData(GL_ARRAY_BUFFER, VertSize * Count, pVertices, GL_STREAM_DRAW);
	else
	{
		// This is better for some iGPUs. Probably due to not initializing a new buffer in the system memory again and again...(driver dependent)
		void *pData = glMapBufferRange(GL_ARRAY_BUFFER, 0, VertSize * Count, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		mem_copy(pData, pVertices, VertSize * Count);

		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	UseProgram(m_pPrimitiveProgram);
	SetState(pCommand->m_State, m_pPrimitiveProgram);

	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertex), pCommand->m_PrimCount);

	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, 0, pCommand->m_PrimCount * 3);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
			m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
		}
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount * 6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};

	m_LastStreamBuffer = (m_LastStreamBuffer + 1 >= MAX_STREAM_BUFFER_COUNT ? 0 : m_LastStreamBuffer + 1);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderTex3D(const CCommandBuffer::SCommand_RenderTex3D *pCommand)
{
	CGLSLPrimitiveProgram *pProg = m_pPrimitive3DProgram;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
		pProg = m_pPrimitive3DProgramTextured;
	UseProgram(pProg);
	SetState(pCommand->m_State, pProg, true);

	UploadStreamBufferData(pCommand->m_PrimType, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertexTex3DStream), pCommand->m_PrimCount, true);

	glBindVertexArray(m_PrimitiveDrawVertexIDTex3D);

	switch(pCommand->m_PrimType)
	{
	// We don't support GL_QUADS due to core profile
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	case CCommandBuffer::PRIMTYPE_QUADS:
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		glDrawElements(GL_TRIANGLES, pCommand->m_PrimCount * 6, GL_UNSIGNED_INT, 0);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_PrimType);
	};
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)malloc((size_t)w * (h + 1) * 3);
	unsigned char *pTempRow = pPixelData + w * h * 3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h / 2; y++)
	{
		mem_copy(pTempRow, pPixelData + y * w * 3, w * 3);
		mem_copy(pPixelData + y * w * 3, pPixelData + (h - y - 1) * w * 3, w * 3);
		mem_copy(pPixelData + (h - y - 1) * w * 3, pTempRow, w * 3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;
}

void CCommandProcessorFragment_OpenGL3_3::DestroyBufferContainer(int Index, bool DeleteBOs)
{
	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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
				for(auto &Attribute : BufferContainer.m_ContainerInfo.m_Attributes)
				{
					// set all equal ids to zero to not double delete
					if(VertBufferID == Attribute.m_VertBufferBindingIndex)
					{
						Attribute.m_VertBufferBindingIndex = -1;
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
	if(NewIndicesCount <= m_CurrentIndicesInBuffer)
		return;
	unsigned int AddCount = NewIndicesCount - m_CurrentIndicesInBuffer;
	unsigned int *Indices = new unsigned int[AddCount];
	int Primq = (m_CurrentIndicesInBuffer / 6) * 4;
	for(unsigned int i = 0; i < AddCount; i += 6)
	{
		Indices[i] = Primq;
		Indices[i + 1] = Primq + 1;
		Indices[i + 2] = Primq + 2;
		Indices[i + 3] = Primq;
		Indices[i + 4] = Primq + 2;
		Indices[i + 5] = Primq + 3;
		Primq += 4;
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

	for(unsigned int &i : m_LastIndexBufferBound)
		i = 0;
	for(auto &BufferContainer : m_BufferContainers)
	{
		BufferContainer.m_LastIndexBufferBound = 0;
	}

	m_CurrentIndicesInBuffer = NewIndicesCount;
	delete[] Indices;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
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
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	m_BufferObjectIndices[Index] = VertBufferID;

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)(pCommand->m_DataSize), pUploadData, GL_STATIC_DRAW);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand)
{
	void *pUploadData = pCommand->m_pUploadData;
	int Index = pCommand->m_BufferIndex;

	glBindBuffer(GL_COPY_WRITE_BUFFER, m_BufferObjectIndices[Index]);
	glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)(pCommand->m_pOffset), (GLsizeiptr)(pCommand->m_DataSize), pUploadData);

	if(pCommand->m_DeletePointer)
		free(pUploadData);
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
			SBufferContainer Container;
			Container.m_ContainerInfo.m_Stride = 0;
			m_BufferContainers.push_back(Container);
		}
	}

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	glGenVertexArrays(1, &BufferContainer.m_VertArrayID);
	glBindVertexArray(BufferContainer.m_VertArrayID);

	BufferContainer.m_LastIndexBufferBound = 0;

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);

		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];

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
	SBufferContainer &BufferContainer = m_BufferContainers[pCommand->m_BufferContainerIndex];

	glBindVertexArray(BufferContainer.m_VertArrayID);

	//disable all old attributes
	for(size_t i = 0; i < BufferContainer.m_ContainerInfo.m_Attributes.size(); ++i)
	{
		glDisableVertexAttribArray((GLuint)i);
	}
	BufferContainer.m_ContainerInfo.m_Attributes.clear();

	for(int i = 0; i < pCommand->m_AttrCount; ++i)
	{
		glEnableVertexAttribArray((GLuint)i);

		glBindBuffer(GL_ARRAY_BUFFER, m_BufferObjectIndices[pCommand->m_Attributes[i].m_VertBufferBindingIndex]);
		SBufferContainerInfo::SAttribute &Attr = pCommand->m_Attributes[i];
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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	CGLSLTileProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pBorderTileProgramTextured;
	}
	else
		pProgram = m_pBorderTileProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float *)&pCommand->m_Dir);
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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	CGLSLTileProgram *pProgram = NULL;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pBorderTileLineProgramTextured;
	}
	else
		pProgram = m_pBorderTileLineProgram;
	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);
	pProgram->SetUniformVec2(pProgram->m_LocOffset, 1, (float *)&pCommand->m_Offset);
	pProgram->SetUniformVec2(pProgram->m_LocDir, 1, (float *)&pCommand->m_Dir);

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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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
	else
		pProgram = m_pTileProgram;

	UseProgram(pProgram);

	SetState(pCommand->m_State, pProgram, true);
	pProgram->SetUniformVec4(pProgram->m_LocColor, 1, (float *)&pCommand->m_Color);

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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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

	int QuadsLeft = pCommand->m_QuadNum;
	size_t QuadOffset = 0;

	vec4 aColors[m_MaxQuadsPossible];
	vec2 aOffsets[m_MaxQuadsPossible];
	float aRotations[m_MaxQuadsPossible];

	while(QuadsLeft > 0)
	{
		int ActualQuadCount = minimum<int>(QuadsLeft, m_MaxQuadsAtOnce);

		for(size_t i = 0; i < (size_t)ActualQuadCount; ++i)
		{
			mem_copy(&aColors[i], pCommand->m_pQuadInfo[i + QuadOffset].m_aColor, sizeof(vec4));
			mem_copy(&aOffsets[i], pCommand->m_pQuadInfo[i + QuadOffset].m_aOffsets, sizeof(vec2));
			mem_copy(&aRotations[i], &pCommand->m_pQuadInfo[i + QuadOffset].m_Rotation, sizeof(float));
		}

		pProgram->SetUniformVec4(pProgram->m_LocColors, ActualQuadCount, (float *)aColors);
		pProgram->SetUniformVec2(pProgram->m_LocOffsets, ActualQuadCount, (float *)aOffsets);
		pProgram->SetUniform(pProgram->m_LocRotations, ActualQuadCount, (float *)aRotations);
		glDrawElements(GL_TRIANGLES, ActualQuadCount * 6, GL_UNSIGNED_INT, (void *)(QuadOffset * 6 * sizeof(unsigned int)));

		QuadsLeft -= ActualQuadCount;
		QuadOffset += (size_t)ActualQuadCount;
	}
}

void CCommandProcessorFragment_OpenGL3_3::RenderText(const CCommandBuffer::SState &State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float *pTextColor, const float *pTextOutlineColor)
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
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextSampler, SlotText);
		m_pTextProgram->m_LastTextSampler = SlotText;
	}

	if(m_pTextProgram->m_LastTextOutlineSampler != SlotTextOutline)
	{
		m_pTextProgram->SetUniform(m_pTextProgram->m_LocTextOutlineSampler, SlotTextOutline);
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
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocOutlineColor, 1, (float *)pTextOutlineColor);
		m_pTextProgram->m_LastOutlineColor[0] = pTextOutlineColor[0];
		m_pTextProgram->m_LastOutlineColor[1] = pTextOutlineColor[1];
		m_pTextProgram->m_LastOutlineColor[2] = pTextOutlineColor[2];
		m_pTextProgram->m_LastOutlineColor[3] = pTextOutlineColor[3];
	}

	if(m_pTextProgram->m_LastColor[0] != pTextColor[0] || m_pTextProgram->m_LastColor[1] != pTextColor[1] || m_pTextProgram->m_LastColor[2] != pTextColor[2] || m_pTextProgram->m_LastColor[3] != pTextColor[3])
	{
		m_pTextProgram->SetUniformVec4(m_pTextProgram->m_LocColor, 1, (float *)pTextColor);
		m_pTextProgram->m_LastColor[0] = pTextColor[0];
		m_pTextProgram->m_LastColor[1] = pTextColor[1];
		m_pTextProgram->m_LastColor[2] = pTextColor[2];
		m_pTextProgram->m_LastColor[3] = pTextColor[3];
	}

	glDrawElements(GL_TRIANGLES, DrawNum, GL_UNSIGNED_INT, (void *)(0));
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand)
{
	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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
	if(pCommand->m_PrimCount == 0)
	{
		return; //nothing to draw
	}

	UploadStreamBufferData(CCommandBuffer::PRIMTYPE_QUADS, pCommand->m_pVertices, sizeof(CCommandBuffer::SVertex), pCommand->m_PrimCount);

	glBindVertexArray(m_PrimitiveDrawVertexID[m_LastStreamBuffer]);
	if(m_LastIndexBufferBound[m_LastStreamBuffer] != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		m_LastIndexBufferBound[m_LastStreamBuffer] = m_QuadDrawIndexBufferID;
	}

	float aTextColor[4] = {1.f, 1.f, 1.f, 1.f};

	RenderText(pCommand->m_State, pCommand->m_PrimCount * 6, pCommand->m_TextTextureIndex, pCommand->m_TextOutlineTextureIndex, pCommand->m_TextureSize, aTextColor, pCommand->m_aTextOutlineColor);

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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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

void CCommandProcessorFragment_OpenGL3_3::Cmd_RenderQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx *pCommand)
{
	if(pCommand->m_DrawNum == 0)
	{
		return; //nothing to draw
	}

	int Index = pCommand->m_BufferContainerIndex;
	//if space not there return
	if((size_t)Index >= m_BufferContainers.size())
		return;

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
	if(BufferContainer.m_VertArrayID == 0)
		return;

	glBindVertexArray(BufferContainer.m_VertArrayID);
	if(BufferContainer.m_LastIndexBufferBound != m_QuadDrawIndexBufferID)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadDrawIndexBufferID);
		BufferContainer.m_LastIndexBufferBound = m_QuadDrawIndexBufferID;
	}

	CGLSLPrimitiveExProgram *pProgram = m_pPrimitiveExProgram;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		pProgram = m_pPrimitiveExProgramTextured;
	}

	UseProgram(pProgram);
	SetState(pCommand->m_State, pProgram);

	if(pCommand->m_Rotation != 0.0f && (pProgram->m_LastCenter[0] != pCommand->m_Center.x || pProgram->m_LastCenter[1] != pCommand->m_Center.y))
	{
		pProgram->SetUniformVec2(pProgram->m_LocCenter, 1, (float *)&pCommand->m_Center);
		pProgram->m_LastCenter[0] = pCommand->m_Center.x;
		pProgram->m_LastCenter[1] = pCommand->m_Center.y;
	}

	if(pProgram->m_LastRotation != pCommand->m_Rotation)
	{
		pProgram->SetUniform(pProgram->m_LocRotation, pCommand->m_Rotation);
		pProgram->m_LastRotation = pCommand->m_Rotation;
	}

	if(pProgram->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || pProgram->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || pProgram->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || pProgram->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		pProgram->SetUniformVec4(pProgram->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
		pProgram->m_LastVertciesColor[0] = pCommand->m_VertexColor.r;
		pProgram->m_LastVertciesColor[1] = pCommand->m_VertexColor.g;
		pProgram->m_LastVertciesColor[2] = pCommand->m_VertexColor.b;
		pProgram->m_LastVertciesColor[3] = pCommand->m_VertexColor.a;
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

	SBufferContainer &BufferContainer = m_BufferContainers[Index];
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
		m_pSpriteProgramMultiple->SetUniformVec2(m_pSpriteProgramMultiple->m_LocCenter, 1, (float *)&pCommand->m_Center);
		m_pSpriteProgramMultiple->m_LastCenter[0] = pCommand->m_Center.x;
		m_pSpriteProgramMultiple->m_LastCenter[1] = pCommand->m_Center.y;
	}

	if(m_pSpriteProgramMultiple->m_LastVertciesColor[0] != pCommand->m_VertexColor.r || m_pSpriteProgramMultiple->m_LastVertciesColor[1] != pCommand->m_VertexColor.g || m_pSpriteProgramMultiple->m_LastVertciesColor[2] != pCommand->m_VertexColor.b || m_pSpriteProgramMultiple->m_LastVertciesColor[3] != pCommand->m_VertexColor.a)
	{
		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocVertciesColor, 1, (float *)&pCommand->m_VertexColor);
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

		m_pSpriteProgramMultiple->SetUniformVec4(m_pSpriteProgramMultiple->m_LocRSP, UniformCount, (float *)(pCommand->m_pRenderInfo + RenderOffset));

		glDrawElementsInstanced(GL_TRIANGLES, pCommand->m_DrawNum, GL_UNSIGNED_INT, pCommand->m_pOffset, UniformCount);

		RenderOffset += RSPCount;
		DrawCount -= RSPCount;
	}
}

// ------------ CCommandProcessorFragment_SDL

static void ParseVersionString(const char *pStr, int &VersionMajor, int &VersionMinor, int &VersionPatch)
{
	if(pStr)
	{
		char aCurNumberStr[32];
		size_t CurNumberStrLen = 0;
		size_t TotalNumbersPassed = 0;
		int aNumbers[3] = {0};
		bool LastWasNumber = false;
		while(*pStr && TotalNumbersPassed < 3)
		{
			if(*pStr >= '0' && *pStr <= '9')
			{
				aCurNumberStr[CurNumberStrLen++] = (char)*pStr;
				LastWasNumber = true;
			}
			else if(LastWasNumber && (*pStr == '.' || *pStr == ' ' || *pStr == '\0'))
			{
				int CurNumber = 0;
				if(CurNumberStrLen > 0)
				{
					aCurNumberStr[CurNumberStrLen] = 0;
					CurNumber = str_toint(aCurNumberStr);
					aNumbers[TotalNumbersPassed++] = CurNumber;
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
GfxOpenGLMessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar *message,
	const void *userParam)
{
	dbg_msg("gfx", "[%s] (importance: %s) %s", GetGLErrorName(type), GetGLSeverity(severity), message);
}

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

	if(g_Config.m_DbgGfx)
	{
		if(GLEW_KHR_debug || GLEW_ARB_debug_output)
		{
			// During init, enable debug output
			if(GLEW_KHR_debug)
			{
				glEnable(GL_DEBUG_OUTPUT);
				glDebugMessageCallback(GfxOpenGLMessageCallback, 0);
			}
			else if(GLEW_ARB_debug_output)
			{
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
				glDebugMessageCallbackARB(GfxOpenGLMessageCallback, 0);
			}
			dbg_msg("gfx", "Enabled OpenGL debug mode");
		}
		else
			dbg_msg("gfx", "Requested OpenGL debug mode, but the driver does not support the required extension");
	}

	const char *pVendorString = (const char *)glGetString(GL_VENDOR);
	dbg_msg("opengl", "Vendor string: %s", pVendorString);

	// check what this context can do
	const char *pVersionString = (const char *)glGetString(GL_VERSION);
	dbg_msg("opengl", "Version string: %s", pVersionString);
	// parse version string
	ParseVersionString(pVersionString, pCommand->m_pCapabilities->m_ContextMajor, pCommand->m_pCapabilities->m_ContextMinor, pCommand->m_pCapabilities->m_ContextPatch);

	*pCommand->m_pInitError = 0;

	int BlocklistMajor = -1, BlocklistMinor = -1, BlocklistPatch = -1;
	const char *pErrString = ParseBlocklistDriverVersions(pVendorString, pVersionString, BlocklistMajor, BlocklistMinor, BlocklistPatch);
	//if the driver is buggy, and the requested GL version is the default, fallback
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

			pCommand->m_pCapabilities->m_TileBuffering = pCommand->m_pCapabilities->m_2DArrayTextures || pCommand->m_pCapabilities->m_3DTextures;
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
			if(MinorV >= 1)
				pCommand->m_pCapabilities->m_ShaderSupport = true;

			int Texture3DSize = 0;
			glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &Texture3DSize);
			if(Texture3DSize >= 256)
			{
				pCommand->m_pCapabilities->m_3DTextures = true;
			}

			// check for array texture extension
			if(pCommand->m_pCapabilities->m_ShaderSupport && GLEW_EXT_texture_array)
			{
				int TextureLayers = 0;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS_EXT, &TextureLayers);
				if(TextureLayers >= 256)
				{
					pCommand->m_pCapabilities->m_2DArrayTextures = true;
					pCommand->m_pCapabilities->m_2DArrayTexturesAsExtension = true;
				}
			}

			pCommand->m_pCapabilities->m_TileBuffering = pCommand->m_pCapabilities->m_2DArrayTextures || pCommand->m_pCapabilities->m_3DTextures;
			pCommand->m_pCapabilities->m_QuadBuffering = false;
			pCommand->m_pCapabilities->m_TextBuffering = false;
			pCommand->m_pCapabilities->m_QuadContainerBuffering = false;

			if(GLEW_ARB_texture_non_power_of_two || pCommand->m_GlewMajor > 2)
				pCommand->m_pCapabilities->m_NPOTTextures = true;
			else
			{
				pCommand->m_pCapabilities->m_NPOTTextures = false;
			}

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
	for(CCommandBuffer::SCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		if(m_pOpenGL->RunCommand(pCommand))
			continue;

		if(m_SDL.RunCommand(pCommand))
			continue;

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_msg("gfx", "unknown command %d", pCommand->m_Cmd);
	}
}

CCommandProcessor_SDL_OpenGL::CCommandProcessor_SDL_OpenGL(int OpenGLMajor, int OpenGLMinor, int OpenGLPatch)
{
	if(OpenGLMajor < 2)
	{
		m_pOpenGL = new CCommandProcessorFragment_OpenGL();
	}
	if(OpenGLMajor == 2)
	{
		m_pOpenGL = new CCommandProcessorFragment_OpenGL2();
	}
	if(OpenGLMajor == 3 && OpenGLMinor == 0)
	{
		m_pOpenGL = new CCommandProcessorFragment_OpenGL3();
	}
	else if((OpenGLMajor == 3 && OpenGLMinor == 3) || OpenGLMajor >= 4)
	{
		m_pOpenGL = new CCommandProcessorFragment_OpenGL3_3();
	}
}

CCommandProcessor_SDL_OpenGL::~CCommandProcessor_SDL_OpenGL()
{
	delete m_pOpenGL;
}

// ------------ CGraphicsBackend_SDL_OpenGL

static void GetGlewVersion(int &GlewMajor, int &GlewMinor, int &GlewPatch)
{
#ifdef GLEW_VERSION_4_6
	if(GLEW_VERSION_4_6)
	{
		GlewMajor = 4;
		GlewMinor = 6;
		GlewPatch = 0;
		return;
	}
#endif
	if(GLEW_VERSION_4_5)
	{
		GlewMajor = 4;
		GlewMinor = 5;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_4_4)
	{
		GlewMajor = 4;
		GlewMinor = 4;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_4_3)
	{
		GlewMajor = 4;
		GlewMinor = 3;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_4_2)
	{
		GlewMajor = 4;
		GlewMinor = 2;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_4_1)
	{
		GlewMajor = 4;
		GlewMinor = 1;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_4_0)
	{
		GlewMajor = 4;
		GlewMinor = 0;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_3_3)
	{
		GlewMajor = 3;
		GlewMinor = 3;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_3_0)
	{
		GlewMajor = 3;
		GlewMinor = 0;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_2_1)
	{
		GlewMajor = 2;
		GlewMinor = 1;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_2_0)
	{
		GlewMajor = 2;
		GlewMinor = 0;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_1_5)
	{
		GlewMajor = 1;
		GlewMinor = 5;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_1_4)
	{
		GlewMajor = 1;
		GlewMinor = 4;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_1_3)
	{
		GlewMajor = 1;
		GlewMinor = 3;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_1_2_1)
	{
		GlewMajor = 1;
		GlewMinor = 2;
		GlewPatch = 1;
		return;
	}
	if(GLEW_VERSION_1_2)
	{
		GlewMajor = 1;
		GlewMinor = 2;
		GlewPatch = 0;
		return;
	}
	if(GLEW_VERSION_1_1)
	{
		GlewMajor = 1;
		GlewMinor = 1;
		GlewPatch = 0;
		return;
	}
}

static int IsVersionSupportedGlew(int VersionMajor, int VersionMinor, int VersionPatch, int GlewMajor, int GlewMinor, int GlewPatch)
{
	int InitError = 0;
	if(VersionMajor >= 4 && GlewMajor < 4)
	{
		InitError = -1;
	}
	else if(VersionMajor >= 3 && GlewMajor < 3)
	{
		InitError = -1;
	}
	else if(VersionMajor == 3 && GlewMajor == 3)
	{
		if(VersionMinor >= 3 && GlewMinor < 3)
		{
			InitError = -1;
		}
		if(VersionMinor >= 2 && GlewMinor < 2)
		{
			InitError = -1;
		}
		if(VersionMinor >= 1 && GlewMinor < 1)
		{
			InitError = -1;
		}
		if(VersionMinor >= 0 && GlewMinor < 0)
		{
			InitError = -1;
		}
	}
	else if(VersionMajor >= 2 && GlewMajor < 2)
	{
		InitError = -1;
	}
	else if(VersionMajor == 2 && GlewMajor == 2)
	{
		if(VersionMinor >= 1 && GlewMinor < 1)
		{
			InitError = -1;
		}
		if(VersionMinor >= 0 && GlewMinor < 0)
		{
			InitError = -1;
		}
	}
	else if(VersionMajor >= 1 && GlewMajor < 1)
	{
		InitError = -1;
	}
	else if(VersionMajor == 1 && GlewMajor == 1)
	{
		if(VersionMinor >= 5 && GlewMinor < 5)
		{
			InitError = -1;
		}
		if(VersionMinor >= 4 && GlewMinor < 4)
		{
			InitError = -1;
		}
		if(VersionMinor >= 3 && GlewMinor < 3)
		{
			InitError = -1;
		}
		if(VersionMinor >= 2 && GlewMinor < 2)
		{
			InitError = -1;
		}
		else if(VersionMinor == 2 && GlewMinor == 2)
		{
			if(VersionPatch >= 1 && GlewPatch < 1)
			{
				InitError = -1;
			}
			if(VersionPatch >= 0 && GlewPatch < 0)
			{
				InitError = -1;
			}
		}
		if(VersionMinor >= 1 && GlewMinor < 1)
		{
			InitError = -1;
		}
		if(VersionMinor >= 0 && GlewMinor < 0)
		{
			InitError = -1;
		}
	}

	return InitError;
}

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{
	// print sdl version
	{
		SDL_version Compiled;
		SDL_version Linked;

		SDL_VERSION(&Compiled);
		SDL_GetVersion(&Linked);
		dbg_msg("sdl", "SDL version %d.%d.%d (compiled = %d.%d.%d)", Linked.major, Linked.minor, Linked.patch,
			Compiled.major, Compiled.minor, Compiled.patch);
	}

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_INIT_FAILED;
		}

#ifdef CONF_FAMILY_WINDOWS
		if(!getenv("SDL_VIDEO_WINDOW_POS") && !getenv("SDL_VIDEO_CENTERED")) // ignore_convention
			putenv("SDL_VIDEO_WINDOW_POS=center"); // ignore_convention
#endif
	}

	SDL_ClearError();
	const char *pErr = NULL;

	// Query default values, since they are platform dependent
	static bool s_InitDefaultParams = false;
	static int s_SDLGLContextProfileMask, s_SDLGLContextMajorVersion, s_SDLGLContextMinorVersion;
	static bool s_TriedOpenGL3Context = false;

	if(!s_InitDefaultParams)
	{
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &s_SDLGLContextProfileMask);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &s_SDLGLContextMajorVersion);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &s_SDLGLContextMinorVersion);
		s_InitDefaultParams = true;
	}

	//clamp the versions to existing versions(only for OpenGL major <= 3)
	if(g_Config.m_GfxOpenGLMajor == 1)
	{
		g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 1, 5);
		if(g_Config.m_GfxOpenGLMinor == 2)
			g_Config.m_GfxOpenGLPatch = clamp(g_Config.m_GfxOpenGLPatch, 0, 1);
		else
			g_Config.m_GfxOpenGLPatch = 0;
	}
	else if(g_Config.m_GfxOpenGLMajor == 2)
	{
		g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 0, 1);
		g_Config.m_GfxOpenGLPatch = 0;
	}
	else if(g_Config.m_GfxOpenGLMajor == 3)
	{
		g_Config.m_GfxOpenGLMinor = clamp(g_Config.m_GfxOpenGLMinor, 0, 3);
		if(g_Config.m_GfxOpenGLMinor < 3)
			g_Config.m_GfxOpenGLMinor = 0;
		g_Config.m_GfxOpenGLPatch = 0;
	}

	// if OpenGL3 context was tried to be created, but failed, we have to restore the old context attributes
	bool IsNewOpenGL = (g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 3) || g_Config.m_GfxOpenGLMajor >= 4;
	if(s_TriedOpenGL3Context && !IsNewOpenGL)
	{
		s_TriedOpenGL3Context = false;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, s_SDLGLContextProfileMask);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_SDLGLContextMajorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_SDLGLContextMinorVersion);
	}

	m_UseNewOpenGL = false;
	if(IsNewOpenGL)
	{
		s_TriedOpenGL3Context = true;

		if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0)
		{
			pErr = SDL_GetError();
			if(pErr[0] != '\0')
			{
				dbg_msg("gfx", "Using old OpenGL context, because an error occurred while trying to use OpenGL context %zu.%zu: %s.", (size_t)g_Config.m_GfxOpenGLMajor, (size_t)g_Config.m_GfxOpenGLMinor, pErr);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, s_SDLGLContextProfileMask);
			}
			else
			{
				if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_Config.m_GfxOpenGLMajor) == 0 && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_Config.m_GfxOpenGLMinor) == 0)
				{
					pErr = SDL_GetError();
					if(pErr[0] != '\0')
					{
						dbg_msg("gfx", "Using old OpenGL context, because an error occurred while trying to use OpenGL context %zu.%zu: %s.", (size_t)g_Config.m_GfxOpenGLMajor, (size_t)g_Config.m_GfxOpenGLMinor, pErr);
						SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_SDLGLContextMajorVersion);
						SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_SDLGLContextMinorVersion);
					}
					else
					{
						m_UseNewOpenGL = true;
						int vMaj, vMin;
						SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &vMaj);
						SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &vMin);
						dbg_msg("gfx", "Using OpenGL version %d.%d.", vMaj, vMin);
					}
				}
				else
				{
					dbg_msg("gfx", "Couldn't create OpenGL %zu.%zu context.", (size_t)g_Config.m_GfxOpenGLMajor, (size_t)g_Config.m_GfxOpenGLMinor);
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
	}
	//if non standard opengl, set it
	else if(s_SDLGLContextMajorVersion != g_Config.m_GfxOpenGLMajor || s_SDLGLContextMinorVersion != g_Config.m_GfxOpenGLMinor)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_Config.m_GfxOpenGLMajor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_Config.m_GfxOpenGLMinor);
		dbg_msg("gfx", "Created OpenGL %zu.%zu context.", (size_t)g_Config.m_GfxOpenGLMajor, (size_t)g_Config.m_GfxOpenGLMinor);

		if(g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 0)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
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
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_INFO_REQUEST_FAILED;
		}
	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_REQUEST_FAILED;
	}

	// store desktop resolution for settings reset button
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(*Screen, &DisplayMode))
	{
		dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_RESOLUTION_REQUEST_FAILED;
	}
	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;

	// use desktop resolution as default resolution
	if(*pWidth == 0 || *pHeight == 0)
	{
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
	}

	// set flags
	int SdlFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
	if(Flags & IGraphicsBackend::INITFLAG_HIGHDPI)
		SdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
#if defined(SDL_VIDEO_DRIVER_X11)
	if(Flags & IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
#endif
	if(Flags & IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN)
	{
		// when we are at fullscreen, we really shouldn't allow window sizes, that aren't supported by the driver
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
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_WINDOW_CREATE_FAILED;
	}

	m_GLContext = SDL_GL_CreateContext(m_pWindow);

	if(m_GLContext == NULL)
	{
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_CONTEXT_FAILED;
	}

	//support graphic cards that are pretty old(and linux)
	glewExperimental = GL_TRUE;
	if(GLEW_OK != glewInit())
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_UNKNOWN;

	int GlewMajor = 0;
	int GlewMinor = 0;
	int GlewPatch = 0;

	GetGlewVersion(GlewMajor, GlewMinor, GlewPatch);

	int InitError = 0;
	const char *pErrorStr = NULL;

	InitError = IsVersionSupportedGlew(g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch, GlewMajor, GlewMinor, GlewPatch);

	SDL_GL_GetDrawableSize(m_pWindow, pCurrentWidth, pCurrentHeight);
	SDL_GL_SetSwapInterval(Flags & IGraphicsBackend::INITFLAG_VSYNC ? 1 : 0);
	SDL_GL_MakeCurrent(NULL, NULL);

	if(InitError != 0)
	{
		SDL_GL_DeleteContext(m_GLContext);
		SDL_DestroyWindow(m_pWindow);

		// try setting to glew supported version
		g_Config.m_GfxOpenGLMajor = GlewMajor;
		g_Config.m_GfxOpenGLMinor = GlewMinor;
		g_Config.m_GfxOpenGLPatch = GlewPatch;

		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED;
	}

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL(g_Config.m_GfxOpenGLMajor, g_Config.m_GfxOpenGLMinor, g_Config.m_GfxOpenGLPatch);
	StartProcessor(m_pProcessor);

	mem_zero(m_aErrorString, sizeof(m_aErrorString) / sizeof(m_aErrorString[0]));

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	//run sdl first to have the context in the thread
	CCommandProcessorFragment_SDL::SCommand_Init CmdSDL;
	CmdSDL.m_pWindow = m_pWindow;
	CmdSDL.m_GLContext = m_GLContext;
	CmdSDL.m_pCapabilities = &m_Capabilites;
	CmdSDL.m_RequestedMajor = g_Config.m_GfxOpenGLMajor;
	CmdSDL.m_RequestedMinor = g_Config.m_GfxOpenGLMinor;
	CmdSDL.m_RequestedPatch = g_Config.m_GfxOpenGLPatch;
	CmdSDL.m_GlewMajor = GlewMajor;
	CmdSDL.m_GlewMinor = GlewMinor;
	CmdSDL.m_GlewPatch = GlewPatch;
	CmdSDL.m_pInitError = &InitError;
	CmdSDL.m_pErrStringPtr = &pErrorStr;
	CmdBuffer.AddCommand(CmdSDL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

	if(InitError == 0)
	{
		CCommandProcessorFragment_OpenGL::SCommand_Init CmdOpenGL;
		CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdOpenGL.m_pStorage = pStorage;
		CmdOpenGL.m_pCapabilities = &m_Capabilites;
		CmdOpenGL.m_pInitError = &InitError;
		CmdBuffer.AddCommand(CmdOpenGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		if(InitError == -2)
		{
			CCommandProcessorFragment_OpenGL::SCommand_Shutdown CmdGL;
			CmdBuffer.AddCommand(CmdGL);
			RunBuffer(&CmdBuffer);
			WaitForIdle();
			CmdBuffer.Reset();

			g_Config.m_GfxOpenGLMajor = 1;
			g_Config.m_GfxOpenGLMinor = 5;
			g_Config.m_GfxOpenGLPatch = 0;
		}
	}

	if(InitError != 0)
	{
		CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
		CmdBuffer.AddCommand(Cmd);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		// stop and delete the processor
		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = 0;

		SDL_GL_DeleteContext(m_GLContext);
		SDL_DestroyWindow(m_pWindow);

		// try setting to version string's supported version
		if(InitError == -2)
		{
			g_Config.m_GfxOpenGLMajor = m_Capabilites.m_ContextMajor;
			g_Config.m_GfxOpenGLMinor = m_Capabilites.m_ContextMinor;
			g_Config.m_GfxOpenGLPatch = m_Capabilites.m_ContextPatch;
		}

		if(pErrorStr != NULL)
		{
			str_copy(m_aErrorString, pErrorStr, sizeof(m_aErrorString) / sizeof(m_aErrorString[0]));
		}

		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED;
	}

	if(SetWindowScreen(g_Config.m_GfxScreen))
	{
		// query the current displaymode, when running in fullscreen
		// this is required if DPI scaling is active
		if(SdlFlags & SDL_WINDOW_FULLSCREEN)
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
			CmdBuffer.Reset();
		}
	}

	// return
	return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_NONE;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_OpenGL::SCommand_Shutdown CmdGL;
	CmdBuffer.AddCommand(CmdGL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

	CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
	CmdBuffer.AddCommand(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();
	CmdBuffer.Reset();

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
#if defined(CONF_PLATFORM_MACOSX) // Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
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
	return SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_SHOWN;
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
		(unsigned char *)&demandsAttention, 1);
#endif
}

IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_OpenGL; }
