/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/math.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <base/system.h>

#include <engine/console.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/localization.h>

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif

#include "graphics_threaded.h"

class CSemaphore;

static CVideoMode g_aFakeModes[] = {
	{8192, 4320, 8192, 4320, 0, 8, 8, 8, 0}, {7680, 4320, 7680, 4320, 0, 8, 8, 8, 0}, {5120, 2880, 5120, 2880, 0, 8, 8, 8, 0},
	{4096, 2160, 4096, 2160, 0, 8, 8, 8, 0}, {3840, 2160, 3840, 2160, 0, 8, 8, 8, 0}, {2560, 1440, 2560, 1440, 0, 8, 8, 8, 0},
	{2048, 1536, 2048, 1536, 0, 8, 8, 8, 0}, {1920, 2400, 1920, 2400, 0, 8, 8, 8, 0}, {1920, 1440, 1920, 1440, 0, 8, 8, 8, 0},
	{1920, 1200, 1920, 1200, 0, 8, 8, 8, 0}, {1920, 1080, 1920, 1080, 0, 8, 8, 8, 0}, {1856, 1392, 1856, 1392, 0, 8, 8, 8, 0},
	{1800, 1440, 1800, 1440, 0, 8, 8, 8, 0}, {1792, 1344, 1792, 1344, 0, 8, 8, 8, 0}, {1680, 1050, 1680, 1050, 0, 8, 8, 8, 0},
	{1600, 1200, 1600, 1200, 0, 8, 8, 8, 0}, {1600, 1000, 1600, 1000, 0, 8, 8, 8, 0}, {1440, 1050, 1440, 1050, 0, 8, 8, 8, 0},
	{1440, 900, 1440, 900, 0, 8, 8, 8, 0}, {1400, 1050, 1400, 1050, 0, 8, 8, 8, 0}, {1368, 768, 1368, 768, 0, 8, 8, 8, 0},
	{1280, 1024, 1280, 1024, 0, 8, 8, 8, 0}, {1280, 960, 1280, 960, 0, 8, 8, 8, 0}, {1280, 800, 1280, 800, 0, 8, 8, 8, 0},
	{1280, 768, 1280, 768, 0, 8, 8, 8, 0}, {1152, 864, 1152, 864, 0, 8, 8, 8, 0}, {1024, 768, 1024, 768, 0, 8, 8, 8, 0},
	{1024, 600, 1024, 600, 0, 8, 8, 8, 0}, {800, 600, 800, 600, 0, 8, 8, 8, 0}, {768, 576, 768, 576, 0, 8, 8, 8, 0},
	{720, 400, 720, 400, 0, 8, 8, 8, 0}, {640, 480, 640, 480, 0, 8, 8, 8, 0}, {400, 300, 400, 300, 0, 8, 8, 8, 0},
	{320, 240, 320, 240, 0, 8, 8, 8, 0},

	{8192, 4320, 8192, 4320, 0, 5, 6, 5, 0}, {7680, 4320, 7680, 4320, 0, 5, 6, 5, 0}, {5120, 2880, 5120, 2880, 0, 5, 6, 5, 0},
	{4096, 2160, 4096, 2160, 0, 5, 6, 5, 0}, {3840, 2160, 3840, 2160, 0, 5, 6, 5, 0}, {2560, 1440, 2560, 1440, 0, 5, 6, 5, 0},
	{2048, 1536, 2048, 1536, 0, 5, 6, 5, 0}, {1920, 2400, 1920, 2400, 0, 5, 6, 5, 0}, {1920, 1440, 1920, 1440, 0, 5, 6, 5, 0},
	{1920, 1200, 1920, 1200, 0, 5, 6, 5, 0}, {1920, 1080, 1920, 1080, 0, 5, 6, 5, 0}, {1856, 1392, 1856, 1392, 0, 5, 6, 5, 0},
	{1800, 1440, 1800, 1440, 0, 5, 6, 5, 0}, {1792, 1344, 1792, 1344, 0, 5, 6, 5, 0}, {1680, 1050, 1680, 1050, 0, 5, 6, 5, 0},
	{1600, 1200, 1600, 1200, 0, 5, 6, 5, 0}, {1600, 1000, 1600, 1000, 0, 5, 6, 5, 0}, {1440, 1050, 1440, 1050, 0, 5, 6, 5, 0},
	{1440, 900, 1440, 900, 0, 5, 6, 5, 0}, {1400, 1050, 1400, 1050, 0, 5, 6, 5, 0}, {1368, 768, 1368, 768, 0, 5, 6, 5, 0},
	{1280, 1024, 1280, 1024, 0, 5, 6, 5, 0}, {1280, 960, 1280, 960, 0, 5, 6, 5, 0}, {1280, 800, 1280, 800, 0, 5, 6, 5, 0},
	{1280, 768, 1280, 768, 0, 5, 6, 5, 0}, {1152, 864, 1152, 864, 0, 5, 6, 5, 0}, {1024, 768, 1024, 768, 0, 5, 6, 5, 0},
	{1024, 600, 1024, 600, 0, 5, 6, 5, 0}, {800, 600, 800, 600, 0, 5, 6, 5, 0}, {768, 576, 768, 576, 0, 5, 6, 5, 0},
	{720, 400, 720, 400, 0, 5, 6, 5, 0}, {640, 480, 640, 480, 0, 5, 6, 5, 0}, {400, 300, 400, 300, 0, 5, 6, 5, 0},
	{320, 240, 320, 240, 0, 5, 6, 5, 0}};

void CGraphics_Threaded::FlushVertices(bool KeepVertices)
{
	CCommandBuffer::SCommand_Render Cmd;
	int PrimType, PrimCount, NumVerts;
	size_t VertSize = sizeof(CCommandBuffer::SVertex);
	FlushVerticesImpl(KeepVertices, PrimType, PrimCount, NumVerts, Cmd, VertSize);

	if(Cmd.m_pVertices != NULL)
	{
		mem_copy(Cmd.m_pVertices, m_aVertices, VertSize * NumVerts);
	}
}

void CGraphics_Threaded::FlushVerticesTex3D()
{
	CCommandBuffer::SCommand_RenderTex3D Cmd;
	int PrimType, PrimCount, NumVerts;
	size_t VertSize = sizeof(CCommandBuffer::SVertexTex3DStream);
	FlushVerticesImpl(false, PrimType, PrimCount, NumVerts, Cmd, VertSize);

	if(Cmd.m_pVertices != NULL)
	{
		mem_copy(Cmd.m_pVertices, m_aVerticesTex3D, VertSize * NumVerts);
	}
}

void CGraphics_Threaded::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= CCommandBuffer::MAX_VERTICES)
		FlushVertices();
}

void CGraphics_Threaded::AddVertices(int Count, CCommandBuffer::SVertex *pVertices)
{
	AddVertices(Count);
}

void CGraphics_Threaded::AddVertices(int Count, CCommandBuffer::SVertexTex3DStream *pVertices)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= CCommandBuffer::MAX_VERTICES)
		FlushVerticesTex3D();
}

CGraphics_Threaded::CGraphics_Threaded()
{
	m_State.m_ScreenTL.x = 0;
	m_State.m_ScreenTL.y = 0;
	m_State.m_ScreenBR.x = 0;
	m_State.m_ScreenBR.y = 0;
	m_State.m_ClipEnable = false;
	m_State.m_ClipX = 0;
	m_State.m_ClipY = 0;
	m_State.m_ClipW = 0;
	m_State.m_ClipH = 0;
	m_State.m_Texture = -1;
	m_State.m_BlendMode = CCommandBuffer::BLEND_NONE;
	m_State.m_WrapMode = CCommandBuffer::WRAP_REPEAT;

	m_CurrentCommandBuffer = 0;
	m_pCommandBuffer = 0x0;
	m_apCommandBuffers[0] = 0x0;
	m_apCommandBuffers[1] = 0x0;

	m_NumVertices = 0;

	m_ScreenWidth = -1;
	m_ScreenHeight = -1;
	m_ScreenRefreshRate = -1;

	m_Rotation = 0;
	m_Drawing = 0;

	m_TextureMemoryUsage = 0;

	m_RenderEnable = true;
	m_DoScreenshot = false;
}

void CGraphics_Threaded::ClipEnable(int x, int y, int w, int h)
{
	if(x < 0)
		w += x;
	if(y < 0)
		h += y;

	x = clamp(x, 0, ScreenWidth());
	y = clamp(y, 0, ScreenHeight());
	w = clamp(w, 0, ScreenWidth() - x);
	h = clamp(h, 0, ScreenHeight() - y);

	m_State.m_ClipEnable = true;
	m_State.m_ClipX = x;
	m_State.m_ClipY = ScreenHeight() - (y + h);
	m_State.m_ClipW = w;
	m_State.m_ClipH = h;
}

void CGraphics_Threaded::ClipDisable()
{
	m_State.m_ClipEnable = false;
}

void CGraphics_Threaded::BlendNone()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_NONE;
}

void CGraphics_Threaded::BlendNormal()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_ALPHA;
}

void CGraphics_Threaded::BlendAdditive()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_ADDITIVE;
}

void CGraphics_Threaded::WrapNormal()
{
	m_State.m_WrapMode = CCommandBuffer::WRAP_REPEAT;
}

void CGraphics_Threaded::WrapClamp()
{
	m_State.m_WrapMode = CCommandBuffer::WRAP_CLAMP;
}

uint64_t CGraphics_Threaded::TextureMemoryUsage() const
{
	return m_pBackend->TextureMemoryUsage();
}

uint64_t CGraphics_Threaded::BufferMemoryUsage() const
{
	return m_pBackend->BufferMemoryUsage();
}

uint64_t CGraphics_Threaded::StreamedMemoryUsage() const
{
	return m_pBackend->StreamedMemoryUsage();
}

uint64_t CGraphics_Threaded::StagingMemoryUsage() const
{
	return m_pBackend->StagingMemoryUsage();
}

const TTWGraphicsGPUList &CGraphics_Threaded::GetGPUs() const
{
	return m_pBackend->GetGPUs();
}

void CGraphics_Threaded::MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY)
{
	m_State.m_ScreenTL.x = TopLeftX;
	m_State.m_ScreenTL.y = TopLeftY;
	m_State.m_ScreenBR.x = BottomRightX;
	m_State.m_ScreenBR.y = BottomRightY;
}

void CGraphics_Threaded::GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY)
{
	*pTopLeftX = m_State.m_ScreenTL.x;
	*pTopLeftY = m_State.m_ScreenTL.y;
	*pBottomRightX = m_State.m_ScreenBR.x;
	*pBottomRightY = m_State.m_ScreenBR.y;
}

void CGraphics_Threaded::LinesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->LinesBegin twice");
	m_Drawing = DRAWING_LINES;
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::LinesEnd()
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::LinesDraw(const CLineItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesDraw without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 2 * i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 2 * i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 2 * i].m_Tex = m_aTexture[0];
		SetColor(&m_aVertices[m_NumVertices + 2 * i], 0);

		m_aVertices[m_NumVertices + 2 * i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 2 * i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 2 * i + 1].m_Tex = m_aTexture[1];
		SetColor(&m_aVertices[m_NumVertices + 2 * i + 1], 1);
	}

	AddVertices(2 * Num);
}

int CGraphics_Threaded::UnloadTexture(CTextureHandle *pIndex)
{
	if(pIndex->Id() == m_InvalidTexture.Id())
		return 0;

	if(!pIndex->IsValid())
		return 0;

	CCommandBuffer::SCommand_Texture_Destroy Cmd;
	Cmd.m_Slot = pIndex->Id();
	AddCmd(
		Cmd, [] { return true; }, "failed to unload texture.");

	m_vTextureIndices[pIndex->Id()] = m_FirstFreeTexture;
	m_FirstFreeTexture = pIndex->Id();

	pIndex->Invalidate();
	return 0;
}

static int ImageFormatToPixelSize(int Format)
{
	switch(Format)
	{
	case CImageInfo::FORMAT_RGB: return 3;
	case CImageInfo::FORMAT_SINGLE_COMPONENT: return 1;
	default: return 4;
	}
}

static bool ConvertToRGBA(uint8_t *pDest, const uint8_t *pSrc, size_t SrcWidth, size_t SrcHeight, int SrcFormat)
{
	if(SrcFormat == CImageInfo::FORMAT_RGBA)
	{
		mem_copy(pDest, pSrc, SrcWidth * SrcHeight * 4);
		return true;
	}
	else
	{
		size_t SrcChannelCount = ImageFormatToPixelSize(SrcFormat);
		size_t DstChannelCount = 4;
		for(size_t Y = 0; Y < SrcHeight; ++Y)
		{
			for(size_t X = 0; X < SrcWidth; ++X)
			{
				size_t ImgOffsetSrc = (Y * SrcWidth * SrcChannelCount) + (X * SrcChannelCount);
				size_t ImgOffsetDest = (Y * SrcWidth * DstChannelCount) + (X * DstChannelCount);
				size_t CopySize = SrcChannelCount;
				if(SrcChannelCount == 3)
				{
					mem_copy(&pDest[ImgOffsetDest], &pSrc[ImgOffsetSrc], CopySize);
					pDest[ImgOffsetDest + 3] = 255;
				}
				else if(SrcChannelCount == 1)
				{
					pDest[ImgOffsetDest + 0] = 255;
					pDest[ImgOffsetDest + 1] = 255;
					pDest[ImgOffsetDest + 2] = 255;
					pDest[ImgOffsetDest + 3] = pSrc[ImgOffsetSrc];
				}
			}
		}
		return false;
	}
}

int CGraphics_Threaded::LoadTextureRawSub(CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
	CCommandBuffer::SCommand_Texture_Update Cmd;
	Cmd.m_Slot = TextureID.Id();
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_Format = CCommandBuffer::TEXFORMAT_RGBA;

	// calculate memory usage
	int MemSize = Width * Height * 4;

	// copy texture data
	void *pTmpData = malloc(MemSize);
	ConvertToRGBA((uint8_t *)pTmpData, (const uint8_t *)pData, Width, Height, Format);
	Cmd.m_pData = pTmpData;

	AddCmd(
		Cmd, [] { return true; }, "failed to load raw sub texture.");
	return 0;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadSpriteTextureImpl(CImageInfo &FromImageInfo, int x, int y, int w, int h)
{
	int bpp = ImageFormatToPixelSize(FromImageInfo.m_Format);

	m_vSpriteHelper.resize((size_t)w * h * bpp);

	CopyTextureFromTextureBufferSub(m_vSpriteHelper.data(), w, h, (uint8_t *)FromImageInfo.m_pData, FromImageInfo.m_Width, FromImageInfo.m_Height, bpp, x, y, w, h);

	IGraphics::CTextureHandle RetHandle = LoadTextureRaw(w, h, FromImageInfo.m_Format, m_vSpriteHelper.data(), FromImageInfo.m_Format, 0);

	return RetHandle;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadSpriteTexture(CImageInfo &FromImageInfo, CDataSprite *pSprite)
{
	int imggx = FromImageInfo.m_Width / pSprite->m_pSet->m_Gridx;
	int imggy = FromImageInfo.m_Height / pSprite->m_pSet->m_Gridy;
	int x = pSprite->m_X * imggx;
	int y = pSprite->m_Y * imggy;
	int w = pSprite->m_W * imggx;
	int h = pSprite->m_H * imggy;
	return LoadSpriteTextureImpl(FromImageInfo, x, y, w, h);
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadSpriteTexture(CImageInfo &FromImageInfo, client_data7::CDataSprite *pSprite)
{
	int imggx = FromImageInfo.m_Width / pSprite->m_pSet->m_Gridx;
	int imggy = FromImageInfo.m_Height / pSprite->m_pSet->m_Gridy;
	int x = pSprite->m_X * imggx;
	int y = pSprite->m_Y * imggy;
	int w = pSprite->m_W * imggx;
	int h = pSprite->m_H * imggy;
	return LoadSpriteTextureImpl(FromImageInfo, x, y, w, h);
}

bool CGraphics_Threaded::IsImageSubFullyTransparent(CImageInfo &FromImageInfo, int x, int y, int w, int h)
{
	if(FromImageInfo.m_Format == CImageInfo::FORMAT_SINGLE_COMPONENT || FromImageInfo.m_Format == CImageInfo::FORMAT_RGBA)
	{
		uint8_t *pImgData = (uint8_t *)FromImageInfo.m_pData;
		int bpp = ImageFormatToPixelSize(FromImageInfo.m_Format);
		for(int iy = 0; iy < h; ++iy)
		{
			for(int ix = 0; ix < w; ++ix)
			{
				int RealOffset = (x + ix) * bpp + (y + iy) * bpp * FromImageInfo.m_Width;
				if(pImgData[RealOffset + (bpp - 1)] > 0)
					return false;
			}
		}

		return true;
	}
	return false;
}

bool CGraphics_Threaded::IsSpriteTextureFullyTransparent(CImageInfo &FromImageInfo, client_data7::CDataSprite *pSprite)
{
	int imggx = FromImageInfo.m_Width / pSprite->m_pSet->m_Gridx;
	int imggy = FromImageInfo.m_Height / pSprite->m_pSet->m_Gridy;
	int x = pSprite->m_X * imggx;
	int y = pSprite->m_Y * imggy;
	int w = pSprite->m_W * imggx;
	int h = pSprite->m_H * imggy;

	return IsImageSubFullyTransparent(FromImageInfo, x, y, w, h);
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags, const char *pTexName)
{
	// don't waste memory on texture if we are stress testing
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
		return m_InvalidTexture;
#endif

	if((Flags & IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE) != 0 || (Flags & IGraphics::TEXLOAD_TO_3D_TEXTURE) != 0)
	{
		if(Width == 0 || (Width % 16) != 0 || Height == 0 || (Height % 16) != 0)
		{
			SWarning NewWarning;
			char aText[128];
			aText[0] = '\0';
			if(pTexName)
			{
				str_format(aText, sizeof(aText), "\"%s\"", pTexName);
			}
			str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), Localize("The width of texture %s is not divisible by %d, or the height is not divisible by %d, which might cause visual bugs."), aText, 16, 16);

			m_vWarnings.emplace_back(NewWarning);
		}
	}

	if(Width == 0 || Height == 0)
		return IGraphics::CTextureHandle();

	// grab texture
	int Tex = m_FirstFreeTexture;
	if(Tex == -1)
	{
		size_t CurSize = m_vTextureIndices.size();
		m_vTextureIndices.resize(CurSize * 2);
		for(size_t i = 0; i < CurSize - 1; ++i)
		{
			m_vTextureIndices[CurSize + i] = CurSize + i + 1;
		}
		m_vTextureIndices.back() = -1;

		Tex = CurSize;
	}
	m_FirstFreeTexture = m_vTextureIndices[Tex];
	m_vTextureIndices[Tex] = -1;

	CCommandBuffer::SCommand_Texture_Create Cmd;
	Cmd.m_Slot = Tex;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_PixelSize = 4;
	Cmd.m_Format = CCommandBuffer::TEXFORMAT_RGBA;
	Cmd.m_StoreFormat = CCommandBuffer::TEXFORMAT_RGBA;

	// flags
	Cmd.m_Flags = 0;
	if(Flags & IGraphics::TEXLOAD_NOMIPMAPS)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_NOMIPMAPS;
	if((Flags & IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE;
	if((Flags & IGraphics::TEXLOAD_TO_3D_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_3D_TEXTURE;
	if((Flags & IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER;
	if((Flags & IGraphics::TEXLOAD_TO_3D_TEXTURE_SINGLE_LAYER) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_3D_TEXTURE_SINGLE_LAYER;
	if((Flags & IGraphics::TEXLOAD_NO_2D_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_NO_2D_TEXTURE;

	// copy texture data
	int MemSize = Width * Height * Cmd.m_PixelSize;
	void *pTmpData = malloc(MemSize);
	if(!ConvertToRGBA((uint8_t *)pTmpData, (const uint8_t *)pData, Width, Height, Format))
	{
		dbg_msg("graphics", "converted image %s to RGBA, consider making its file format RGBA", pTexName ? pTexName : "(no name)");
	}
	Cmd.m_pData = pTmpData;

	AddCmd(
		Cmd, [] { return true; }, "failed to load raw texture.");

	return CreateTextureHandle(Tex);
}

// simple uncompressed RGBA loaders
IGraphics::CTextureHandle CGraphics_Threaded::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	IGraphics::CTextureHandle ID;
	CImageInfo Img;

	if(l < 3)
		return CTextureHandle();
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		if(StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags, pFilename);
		free(Img.m_pData);
		if(ID.Id() != m_InvalidTexture.Id() && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return ID;
	}

	return m_InvalidTexture;
}

bool CGraphics_Threaded::LoadTextTextures(int Width, int Height, CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture, void *pTextData, void *pTextOutlineData)
{
	if(Width == 0 || Height == 0)
		return false;

	// grab texture
	int Tex = m_FirstFreeTexture;
	if(Tex == -1)
	{
		size_t CurSize = m_vTextureIndices.size();
		m_vTextureIndices.resize(CurSize * 2);
		for(size_t i = 0; i < CurSize - 1; ++i)
		{
			m_vTextureIndices[CurSize + i] = CurSize + i + 1;
		}
		m_vTextureIndices.back() = -1;

		Tex = CurSize;
	}
	m_FirstFreeTexture = m_vTextureIndices[Tex];
	m_vTextureIndices[Tex] = -1;

	int Tex2 = m_FirstFreeTexture;
	if(Tex2 == -1)
	{
		size_t CurSize = m_vTextureIndices.size();
		m_vTextureIndices.resize(CurSize * 2);
		for(size_t i = 0; i < CurSize - 1; ++i)
		{
			m_vTextureIndices[CurSize + i] = CurSize + i + 1;
		}
		m_vTextureIndices.back() = -1;

		Tex2 = CurSize;
	}
	m_FirstFreeTexture = m_vTextureIndices[Tex2];
	m_vTextureIndices[Tex2] = -1;

	CCommandBuffer::SCommand_TextTextures_Create Cmd;
	Cmd.m_Slot = Tex;
	Cmd.m_SlotOutline = Tex2;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;

	Cmd.m_pTextData = pTextData;
	Cmd.m_pTextOutlineData = pTextOutlineData;

	AddCmd(
		Cmd, [] { return true; }, "failed to load text textures.");

	TextTexture = CreateTextureHandle(Tex);
	TextOutlineTexture = CreateTextureHandle(Tex2);

	return true;
}

bool CGraphics_Threaded::UnloadTextTextures(CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture)
{
	CCommandBuffer::SCommand_TextTextures_Destroy Cmd;
	Cmd.m_Slot = TextTexture.Id();
	Cmd.m_SlotOutline = TextOutlineTexture.Id();
	AddCmd(
		Cmd, [] { return true; }, "failed to unload text textures.");

	m_vTextureIndices[TextTexture.Id()] = m_FirstFreeTexture;
	m_FirstFreeTexture = TextTexture.Id();

	m_vTextureIndices[TextOutlineTexture.Id()] = m_FirstFreeTexture;
	m_FirstFreeTexture = TextOutlineTexture.Id();

	TextTexture.Invalidate();
	TextOutlineTexture.Invalidate();
	return true;
}

bool CGraphics_Threaded::UpdateTextTexture(CTextureHandle TextureID, int x, int y, int Width, int Height, const void *pData)
{
	CCommandBuffer::SCommand_TextTexture_Update Cmd;
	Cmd.m_Slot = TextureID.Id();
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;

	// calculate memory usage
	int MemSize = Width * Height;

	// copy texture data
	void *pTmpData = malloc(MemSize);
	mem_copy(pTmpData, pData, MemSize);
	Cmd.m_pData = pTmpData;

	AddCmd(
		Cmd, [] { return true; }, "failed to update text texture.");
	return true;
}

int CGraphics_Threaded::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[IO_MAX_PATH_LENGTH];
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		unsigned int FileSize = io_tell(File);
		io_seek(File, 0, IOSEEK_START);

		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		uint8_t *pImgBuffer = NULL;
		EImageFormat ImageFormat;
		int PngliteIncompatible;
		if(::LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, pImg->m_Width, pImg->m_Height, pImgBuffer, ImageFormat))
		{
			pImg->m_pData = pImgBuffer;

			if(ImageFormat == IMAGE_FORMAT_RGB) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGB;
			else if(ImageFormat == IMAGE_FORMAT_RGBA) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGBA;
			else
			{
				free(pImgBuffer);
				return 0;
			}

			if(m_WarnPngliteIncompatibleImages && PngliteIncompatible != 0)
			{
				SWarning Warning;
				str_format(Warning.m_aWarningMsg, sizeof(Warning.m_aWarningMsg), Localize("\"%s\" is not compatible with pnglite and cannot be loaded by old DDNet versions: "), pFilename);
				static const int FLAGS[] = {PNGLITE_COLOR_TYPE, PNGLITE_BIT_DEPTH, PNGLITE_INTERLACE_TYPE, PNGLITE_COMPRESSION_TYPE, PNGLITE_FILTER_TYPE};
				static const char *EXPLANATION[] = {"color type", "bit depth", "interlace type", "compression type", "filter type"};

				bool First = true;
				for(int i = 0; i < (int)std::size(FLAGS); i++)
				{
					if((PngliteIncompatible & FLAGS[i]) != 0)
					{
						if(!First)
						{
							str_append(Warning.m_aWarningMsg, ", ", sizeof(Warning.m_aWarningMsg));
						}
						str_append(Warning.m_aWarningMsg, EXPLANATION[i], sizeof(Warning.m_aWarningMsg));
						First = false;
					}
				}
				str_append(Warning.m_aWarningMsg, " unsupported", sizeof(Warning.m_aWarningMsg));
				m_vWarnings.emplace_back(Warning);
			}
		}
		else
		{
			dbg_msg("game/png", "image had unsupported image format. filename='%s'", pFilename);
			return 0;
		}
	}
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	return 1;
}

void CGraphics_Threaded::FreePNG(CImageInfo *pImg)
{
	free(pImg->m_pData);
	pImg->m_pData = NULL;
}

bool CGraphics_Threaded::CheckImageDivisibility(const char *pFileName, CImageInfo &Img, int DivX, int DivY, bool AllowResize)
{
	dbg_assert(DivX != 0 && DivY != 0, "Passing 0 to this function is not allowed.");
	bool ImageIsValid = true;
	bool WidthBroken = Img.m_Width == 0 || (Img.m_Width % DivX) != 0;
	bool HeightBroken = Img.m_Height == 0 || (Img.m_Height % DivY) != 0;
	if(WidthBroken || HeightBroken)
	{
		SWarning NewWarning;
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), Localize("The width of texture %s is not divisible by %d, or the height is not divisible by %d, which might cause visual bugs."), pFileName, DivX, DivY);

		m_vWarnings.emplace_back(NewWarning);

		ImageIsValid = false;
	}

	if(AllowResize && !ImageIsValid && Img.m_Width > 0 && Img.m_Height > 0)
	{
		int NewWidth = DivX;
		int NewHeight = DivY;
		if(WidthBroken)
		{
			NewWidth = maximum<int>(HighestBit(Img.m_Width), DivX);
			NewHeight = (NewWidth / DivX) * DivY;
		}
		else
		{
			NewHeight = maximum<int>(HighestBit(Img.m_Height), DivY);
			NewWidth = (NewHeight / DivY) * DivX;
		}

		int ColorChannelCount = 4;
		if(Img.m_Format == CImageInfo::FORMAT_SINGLE_COMPONENT)
			ColorChannelCount = 1;
		else if(Img.m_Format == CImageInfo::FORMAT_RGB)
			ColorChannelCount = 3;
		else if(Img.m_Format == CImageInfo::FORMAT_RGBA)
			ColorChannelCount = 4;

		uint8_t *pNewImg = ResizeImage((uint8_t *)Img.m_pData, Img.m_Width, Img.m_Height, NewWidth, NewHeight, ColorChannelCount);
		free(Img.m_pData);
		Img.m_pData = pNewImg;
		Img.m_Width = NewWidth;
		Img.m_Height = NewHeight;
		ImageIsValid = true;
	}

	return ImageIsValid;
}

bool CGraphics_Threaded::IsImageFormatRGBA(const char *pFileName, CImageInfo &Img)
{
	if(Img.m_Format != CImageInfo::FORMAT_RGBA)
	{
		SWarning NewWarning;
		char aText[128];
		aText[0] = '\0';
		if(pFileName)
		{
			str_format(aText, sizeof(aText), "\"%s\"", pFileName);
		}
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg),
			Localize("The format of texture %s is not RGBA which will cause visual bugs."), aText);
		m_vWarnings.emplace_back(NewWarning);
		return false;
	}
	return true;
}

void CGraphics_Threaded::CopyTextureBufferSub(uint8_t *pDestBuffer, uint8_t *pSourceBuffer, int FullWidth, int FullHeight, int ColorChannelCount, int SubOffsetX, int SubOffsetY, int SubCopyWidth, int SubCopyHeight)
{
	for(int Y = 0; Y < SubCopyHeight; ++Y)
	{
		int ImgOffset = ((SubOffsetY + Y) * FullWidth * ColorChannelCount) + (SubOffsetX * ColorChannelCount);
		int CopySize = SubCopyWidth * ColorChannelCount;
		mem_copy(&pDestBuffer[ImgOffset], &pSourceBuffer[ImgOffset], CopySize);
	}
}

void CGraphics_Threaded::CopyTextureFromTextureBufferSub(uint8_t *pDestBuffer, int DestWidth, int DestHeight, uint8_t *pSourceBuffer, int SrcWidth, int SrcHeight, int ColorChannelCount, int SrcSubOffsetX, int SrcSubOffsetY, int SrcSubCopyWidth, int SrcSubCopyHeight)
{
	for(int Y = 0; Y < SrcSubCopyHeight; ++Y)
	{
		int SrcImgOffset = ((SrcSubOffsetY + Y) * SrcWidth * ColorChannelCount) + (SrcSubOffsetX * ColorChannelCount);
		int DstImgOffset = (Y * DestWidth * ColorChannelCount);
		int CopySize = SrcSubCopyWidth * ColorChannelCount;
		mem_copy(&pDestBuffer[DstImgOffset], &pSourceBuffer[SrcImgOffset], CopySize);
	}
}

void CGraphics_Threaded::KickCommandBuffer()
{
	m_pBackend->RunBuffer(m_pCommandBuffer);

	// swap buffer
	m_CurrentCommandBuffer ^= 1;
	m_pCommandBuffer = m_apCommandBuffers[m_CurrentCommandBuffer];
	m_pCommandBuffer->Reset();
}

bool CGraphics_Threaded::ScreenshotDirect()
{
	// add swap command
	CImageInfo Image;
	mem_zero(&Image, sizeof(Image));

	bool DidSwap = false;

	CCommandBuffer::SCommand_TrySwapAndScreenshot Cmd;
	Cmd.m_pImage = &Image;
	Cmd.m_pSwapped = &DidSwap;
	AddCmd(
		Cmd, [] { return true; }, "failed to take screenshot.");

	// kick the buffer and wait for the result
	KickCommandBuffer();
	WaitForIdle();

	if(Image.m_pData)
	{
		// find filename
		char aWholePath[1024];

		IOHANDLE File = m_pStorage->OpenFile(m_aScreenshotName, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));
		if(File)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "saved screenshot to '%s'", aWholePath);

			// save png
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, ColorRGBA{1.0f, 0.6f, 0.3f, 1.0f});

			TImageByteBuffer ByteBuffer;
			SImageByteBuffer ImageByteBuffer(&ByteBuffer);

			if(SavePNG(IMAGE_FORMAT_RGBA, (const uint8_t *)Image.m_pData, ImageByteBuffer, Image.m_Width, Image.m_Height))
				io_write(File, &ByteBuffer.front(), ByteBuffer.size());
			io_close(File);
		}

		free(Image.m_pData);
	}

	return DidSwap;
}

void CGraphics_Threaded::TextureSet(CTextureHandle TextureID)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");
	dbg_assert(!TextureID.IsValid() || m_vTextureIndices[TextureID.Id()] == -1, "Texture handle was not invalid, but also did not correlate to an existing texture.");
	m_State.m_Texture = TextureID.Id();
}

void CGraphics_Threaded::Clear(float r, float g, float b, bool ForceClearNow)
{
	CCommandBuffer::SCommand_Clear Cmd;
	Cmd.m_Color.r = r;
	Cmd.m_Color.g = g;
	Cmd.m_Color.b = b;
	Cmd.m_Color.a = 0;
	Cmd.m_ForceClear = ForceClearNow;
	AddCmd(
		Cmd, [] { return true; }, "failed to clear graphics.");
}

void CGraphics_Threaded::QuadsBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->QuadsBegin twice");
	m_Drawing = DRAWING_QUADS;

	QuadsSetSubset(0, 0, 1, 1);
	QuadsSetRotation(0);
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::QuadsEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsTex3DBegin()
{
	QuadsBegin();
}

void CGraphics_Threaded::QuadsTex3DEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	FlushVerticesTex3D();
	m_Drawing = 0;
}

void CGraphics_Threaded::TrianglesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TrianglesBegin twice");
	m_Drawing = DRAWING_TRIANGLES;

	QuadsSetSubset(0, 0, 1, 1);
	QuadsSetRotation(0);
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::TrianglesEnd()
{
	dbg_assert(m_Drawing == DRAWING_TRIANGLES, "called Graphics()->TrianglesEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsEndKeepVertices()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEndKeepVertices without begin");
	FlushVertices(true);
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsDrawCurrentVertices(bool KeepVertices)
{
	m_Drawing = DRAWING_QUADS;
	FlushVertices(KeepVertices);
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsSetRotation(float Angle)
{
	m_Rotation = Angle;
}

inline void clampf(float &Value, float Min, float Max)
{
	if(Value > Max)
		Value = Max;
	else if(Value < Min)
		Value = Min;
}

void CGraphics_Threaded::SetColorVertex(const CColorVertex *pArray, int Num)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColorVertex without begin");

	for(int i = 0; i < Num; ++i)
	{
		float r = pArray[i].m_R, g = pArray[i].m_G, b = pArray[i].m_B, a = pArray[i].m_A;
		clampf(r, 0.f, 1.f);
		clampf(g, 0.f, 1.f);
		clampf(b, 0.f, 1.f);
		clampf(a, 0.f, 1.f);
		m_aColor[pArray[i].m_Index].r = (unsigned char)(r * 255.f);
		m_aColor[pArray[i].m_Index].g = (unsigned char)(g * 255.f);
		m_aColor[pArray[i].m_Index].b = (unsigned char)(b * 255.f);
		m_aColor[pArray[i].m_Index].a = (unsigned char)(a * 255.f);
	}
}

void CGraphics_Threaded::SetColor(float r, float g, float b, float a)
{
	clampf(r, 0.f, 1.f);
	clampf(g, 0.f, 1.f);
	clampf(b, 0.f, 1.f);
	clampf(a, 0.f, 1.f);
	r *= 255.f;
	g *= 255.f;
	b *= 255.f;
	a *= 255.f;

	for(auto &Color : m_aColor)
	{
		Color.r = (unsigned char)(r);
		Color.g = (unsigned char)(g);
		Color.b = (unsigned char)(b);
		Color.a = (unsigned char)(a);
	}
}

void CGraphics_Threaded::SetColor(ColorRGBA Color)
{
	SetColor(Color.r, Color.g, Color.b, Color.a);
}

void CGraphics_Threaded::SetColor4(ColorRGBA TopLeft, ColorRGBA TopRight, ColorRGBA BottomLeft, ColorRGBA BottomRight)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColor without begin");
	CColorVertex Array[4] = {
		CColorVertex(0, TopLeft.r, TopLeft.g, TopLeft.b, TopLeft.a),
		CColorVertex(1, TopRight.r, TopRight.g, TopRight.b, TopRight.a),
		CColorVertex(2, BottomRight.r, BottomRight.g, BottomRight.b, BottomRight.a),
		CColorVertex(3, BottomLeft.r, BottomLeft.g, BottomLeft.b, BottomLeft.a)};
	SetColorVertex(Array, 4);
}

void CGraphics_Threaded::ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a)
{
	clampf(r, 0.f, 1.f);
	clampf(g, 0.f, 1.f);
	clampf(b, 0.f, 1.f);
	clampf(a, 0.f, 1.f);
	m_aColor[0].r = (unsigned char)(r * 255.f);
	m_aColor[0].g = (unsigned char)(g * 255.f);
	m_aColor[0].b = (unsigned char)(b * 255.f);
	m_aColor[0].a = (unsigned char)(a * 255.f);

	for(int i = 0; i < m_NumVertices; ++i)
	{
		SetColor(&m_aVertices[i], 0);
	}
}

void CGraphics_Threaded::ChangeColorOfQuadVertices(int QuadOffset, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	if(g_Config.m_GfxQuadAsTriangle && !m_GLUseTrianglesAsQuad)
	{
		m_aVertices[QuadOffset * 6].m_Color.r = r;
		m_aVertices[QuadOffset * 6].m_Color.g = g;
		m_aVertices[QuadOffset * 6].m_Color.b = b;
		m_aVertices[QuadOffset * 6].m_Color.a = a;

		m_aVertices[QuadOffset * 6 + 1].m_Color.r = r;
		m_aVertices[QuadOffset * 6 + 1].m_Color.g = g;
		m_aVertices[QuadOffset * 6 + 1].m_Color.b = b;
		m_aVertices[QuadOffset * 6 + 1].m_Color.a = a;

		m_aVertices[QuadOffset * 6 + 2].m_Color.r = r;
		m_aVertices[QuadOffset * 6 + 2].m_Color.g = g;
		m_aVertices[QuadOffset * 6 + 2].m_Color.b = b;
		m_aVertices[QuadOffset * 6 + 2].m_Color.a = a;

		m_aVertices[QuadOffset * 6 + 3].m_Color.r = r;
		m_aVertices[QuadOffset * 6 + 3].m_Color.g = g;
		m_aVertices[QuadOffset * 6 + 3].m_Color.b = b;
		m_aVertices[QuadOffset * 6 + 3].m_Color.a = a;

		m_aVertices[QuadOffset * 6 + 4].m_Color.r = r;
		m_aVertices[QuadOffset * 6 + 4].m_Color.g = g;
		m_aVertices[QuadOffset * 6 + 4].m_Color.b = b;
		m_aVertices[QuadOffset * 6 + 4].m_Color.a = a;

		m_aVertices[QuadOffset * 6 + 5].m_Color.r = r;
		m_aVertices[QuadOffset * 6 + 5].m_Color.g = g;
		m_aVertices[QuadOffset * 6 + 5].m_Color.b = b;
		m_aVertices[QuadOffset * 6 + 5].m_Color.a = a;
	}
	else
	{
		m_aVertices[QuadOffset * 4].m_Color.r = r;
		m_aVertices[QuadOffset * 4].m_Color.g = g;
		m_aVertices[QuadOffset * 4].m_Color.b = b;
		m_aVertices[QuadOffset * 4].m_Color.a = a;

		m_aVertices[QuadOffset * 4 + 1].m_Color.r = r;
		m_aVertices[QuadOffset * 4 + 1].m_Color.g = g;
		m_aVertices[QuadOffset * 4 + 1].m_Color.b = b;
		m_aVertices[QuadOffset * 4 + 1].m_Color.a = a;

		m_aVertices[QuadOffset * 4 + 2].m_Color.r = r;
		m_aVertices[QuadOffset * 4 + 2].m_Color.g = g;
		m_aVertices[QuadOffset * 4 + 2].m_Color.b = b;
		m_aVertices[QuadOffset * 4 + 2].m_Color.a = a;

		m_aVertices[QuadOffset * 4 + 3].m_Color.r = r;
		m_aVertices[QuadOffset * 4 + 3].m_Color.g = g;
		m_aVertices[QuadOffset * 4 + 3].m_Color.b = b;
		m_aVertices[QuadOffset * 4 + 3].m_Color.a = a;
	}
}

void CGraphics_Threaded::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV)
{
	m_aTexture[0].u = TlU;
	m_aTexture[1].u = BrU;
	m_aTexture[0].v = TlV;
	m_aTexture[1].v = TlV;

	m_aTexture[3].u = TlU;
	m_aTexture[2].u = BrU;
	m_aTexture[3].v = BrV;
	m_aTexture[2].v = BrV;
}

void CGraphics_Threaded::QuadsSetSubsetFree(
	float x0, float y0, float x1, float y1,
	float x2, float y2, float x3, float y3, int Index)
{
	m_aTexture[0].u = x0;
	m_aTexture[0].v = y0;
	m_aTexture[1].u = x1;
	m_aTexture[1].v = y1;
	m_aTexture[2].u = x2;
	m_aTexture[2].v = y2;
	m_aTexture[3].u = x3;
	m_aTexture[3].v = y3;
	m_CurIndex = Index;
}

void CGraphics_Threaded::QuadsDraw(CQuadItem *pArray, int Num)
{
	for(int i = 0; i < Num; ++i)
	{
		pArray[i].m_X -= pArray[i].m_Width / 2;
		pArray[i].m_Y -= pArray[i].m_Height / 2;
	}

	QuadsDrawTL(pArray, Num);
}

void CGraphics_Threaded::QuadsDrawTL(const CQuadItem *pArray, int Num)
{
	QuadsDrawTLImpl(m_aVertices, pArray, Num);
}

void CGraphics_Threaded::QuadsTex3DDrawTL(const CQuadItem *pArray, int Num)
{
	int CurNumVert = m_NumVertices;

	int VertNum = 0;
	if(g_Config.m_GfxQuadAsTriangle && !m_GLUseTrianglesAsQuad)
	{
		VertNum = 6;
	}
	else
	{
		VertNum = 4;
	}

	for(int i = 0; i < Num; ++i)
	{
		for(int n = 0; n < VertNum; ++n)
		{
			if(HasTextureArrays())
				m_aVerticesTex3D[CurNumVert + VertNum * i + n].m_Tex.w = (float)m_CurIndex;
			else
				m_aVerticesTex3D[CurNumVert + VertNum * i + n].m_Tex.w = ((float)m_CurIndex + 0.5f) / 256.f;
		}
	}

	QuadsDrawTLImpl(m_aVerticesTex3D, pArray, Num);
}

void CGraphics_Threaded::QuadsDrawFreeform(const CFreeformItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_QUADS || m_Drawing == DRAWING_TRIANGLES, "called Graphics()->QuadsDrawFreeform without begin");

	if((g_Config.m_GfxQuadAsTriangle && !m_GLUseTrianglesAsQuad) || m_Drawing == DRAWING_TRIANGLES)
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 6 * i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6 * i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6 * i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6 * i], 0);

			m_aVertices[m_NumVertices + 6 * i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 6 * i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 6 * i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 6 * i + 1], 1);

			m_aVertices[m_NumVertices + 6 * i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6 * i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6 * i + 2].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 6 * i + 2], 3);

			m_aVertices[m_NumVertices + 6 * i + 3].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6 * i + 3].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6 * i + 3].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6 * i + 3], 0);

			m_aVertices[m_NumVertices + 6 * i + 4].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6 * i + 4].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6 * i + 4].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 6 * i + 4], 3);

			m_aVertices[m_NumVertices + 6 * i + 5].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 6 * i + 5].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 6 * i + 5].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 6 * i + 5], 2);
		}

		AddVertices(3 * 2 * Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4 * i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 4 * i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 4 * i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 4 * i], 0);

			m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 4 * i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 4 * i + 1], 1);

			m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 4 * i + 2].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 4 * i + 2], 3);

			m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 4 * i + 3].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 4 * i + 3], 2);
		}

		AddVertices(4 * Num);
	}
}

void CGraphics_Threaded::QuadsText(float x, float y, float Size, const char *pText)
{
	float StartX = x;

	while(*pText)
	{
		char c = *pText;
		pText++;

		if(c == '\n')
		{
			x = StartX;
			y += Size;
		}
		else
		{
			QuadsSetSubset(
				(c % 16) / 16.0f,
				(c / 16) / 16.0f,
				(c % 16) / 16.0f + 1.0f / 16.0f,
				(c / 16) / 16.0f + 1.0f / 16.0f);

			CQuadItem QuadItem(x, y, Size, Size);
			QuadsDrawTL(&QuadItem, 1);
			x += Size / 2;
		}
	}
}

void CGraphics_Threaded::DrawRectExt(float x, float y, float w, float h, float r, int Corners)
{
	const int NumSegments = 8;
	const float SegmentsAngle = pi / 2 / NumSegments;
	IGraphics::CFreeformItem aFreeform[NumSegments * 4];
	size_t NumItems = 0;

	for(int i = 0; i < NumSegments; i += 2)
	{
		float a1 = i * SegmentsAngle;
		float a2 = (i + 1) * SegmentsAngle;
		float a3 = (i + 2) * SegmentsAngle;
		float Ca1 = cosf(a1);
		float Ca2 = cosf(a2);
		float Ca3 = cosf(a3);
		float Sa1 = sinf(a1);
		float Sa2 = sinf(a2);
		float Sa3 = sinf(a3);

		if(Corners & CORNER_TL)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + r, y + r,
				x + (1 - Ca1) * r, y + (1 - Sa1) * r,
				x + (1 - Ca3) * r, y + (1 - Sa3) * r,
				x + (1 - Ca2) * r, y + (1 - Sa2) * r);

		if(Corners & CORNER_TR)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + w - r, y + r,
				x + w - r + Ca1 * r, y + (1 - Sa1) * r,
				x + w - r + Ca3 * r, y + (1 - Sa3) * r,
				x + w - r + Ca2 * r, y + (1 - Sa2) * r);

		if(Corners & CORNER_BL)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + r, y + h - r,
				x + (1 - Ca1) * r, y + h - r + Sa1 * r,
				x + (1 - Ca3) * r, y + h - r + Sa3 * r,
				x + (1 - Ca2) * r, y + h - r + Sa2 * r);

		if(Corners & CORNER_BR)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + w - r, y + h - r,
				x + w - r + Ca1 * r, y + h - r + Sa1 * r,
				x + w - r + Ca3 * r, y + h - r + Sa3 * r,
				x + w - r + Ca2 * r, y + h - r + Sa2 * r);
	}
	QuadsDrawFreeform(aFreeform, NumItems);

	CQuadItem aQuads[9];
	NumItems = 0;
	aQuads[NumItems++] = CQuadItem(x + r, y + r, w - r * 2, h - r * 2); // center
	aQuads[NumItems++] = CQuadItem(x + r, y, w - r * 2, r); // top
	aQuads[NumItems++] = CQuadItem(x + r, y + h - r, w - r * 2, r); // bottom
	aQuads[NumItems++] = CQuadItem(x, y + r, r, h - r * 2); // left
	aQuads[NumItems++] = CQuadItem(x + w - r, y + r, r, h - r * 2); // right

	if(!(Corners & CORNER_TL))
		aQuads[NumItems++] = CQuadItem(x, y, r, r);
	if(!(Corners & CORNER_TR))
		aQuads[NumItems++] = CQuadItem(x + w, y, -r, r);
	if(!(Corners & CORNER_BL))
		aQuads[NumItems++] = CQuadItem(x, y + h, r, -r);
	if(!(Corners & CORNER_BR))
		aQuads[NumItems++] = CQuadItem(x + w, y + h, -r, -r);

	QuadsDrawTL(aQuads, NumItems);
}

void CGraphics_Threaded::DrawRectExt4(float x, float y, float w, float h, ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, float r, int Corners)
{
	if(Corners == 0 || r == 0.0f)
	{
		SetColor4(ColorTopLeft, ColorTopRight, ColorBottomLeft, ColorBottomRight);
		CQuadItem ItemQ = CQuadItem(x, y, w, h);
		QuadsDrawTL(&ItemQ, 1);
		return;
	}

	const int NumSegments = 8;
	const float SegmentsAngle = pi / 2 / NumSegments;
	for(int i = 0; i < NumSegments; i += 2)
	{
		float a1 = i * SegmentsAngle;
		float a2 = (i + 1) * SegmentsAngle;
		float a3 = (i + 2) * SegmentsAngle;
		float Ca1 = cosf(a1);
		float Ca2 = cosf(a2);
		float Ca3 = cosf(a3);
		float Sa1 = sinf(a1);
		float Sa2 = sinf(a2);
		float Sa3 = sinf(a3);

		if(Corners & CORNER_TL)
		{
			SetColor(ColorTopLeft);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + r, y + r,
				x + (1 - Ca1) * r, y + (1 - Sa1) * r,
				x + (1 - Ca3) * r, y + (1 - Sa3) * r,
				x + (1 - Ca2) * r, y + (1 - Sa2) * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_TR)
		{
			SetColor(ColorTopRight);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + w - r, y + r,
				x + w - r + Ca1 * r, y + (1 - Sa1) * r,
				x + w - r + Ca3 * r, y + (1 - Sa3) * r,
				x + w - r + Ca2 * r, y + (1 - Sa2) * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_BL)
		{
			SetColor(ColorBottomLeft);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + r, y + h - r,
				x + (1 - Ca1) * r, y + h - r + Sa1 * r,
				x + (1 - Ca3) * r, y + h - r + Sa3 * r,
				x + (1 - Ca2) * r, y + h - r + Sa2 * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_BR)
		{
			SetColor(ColorBottomRight);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + w - r, y + h - r,
				x + w - r + Ca1 * r, y + h - r + Sa1 * r,
				x + w - r + Ca3 * r, y + h - r + Sa3 * r,
				x + w - r + Ca2 * r, y + h - r + Sa2 * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_ITL)
		{
			SetColor(ColorTopLeft);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x, y,
				x + (1 - Ca1) * r, y - r + Sa1 * r,
				x + (1 - Ca3) * r, y - r + Sa3 * r,
				x + (1 - Ca2) * r, y - r + Sa2 * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_ITR)
		{
			SetColor(ColorTopRight);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + w, y,
				x + w - r + Ca1 * r, y - r + Sa1 * r,
				x + w - r + Ca3 * r, y - r + Sa3 * r,
				x + w - r + Ca2 * r, y - r + Sa2 * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_IBL)
		{
			SetColor(ColorBottomLeft);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x, y + h,
				x + (1 - Ca1) * r, y + h + (1 - Sa1) * r,
				x + (1 - Ca3) * r, y + h + (1 - Sa3) * r,
				x + (1 - Ca2) * r, y + h + (1 - Sa2) * r);
			QuadsDrawFreeform(&ItemF, 1);
		}

		if(Corners & CORNER_IBR)
		{
			SetColor(ColorBottomRight);
			IGraphics::CFreeformItem ItemF = IGraphics::CFreeformItem(
				x + w, y + h,
				x + w - r + Ca1 * r, y + h + (1 - Sa1) * r,
				x + w - r + Ca3 * r, y + h + (1 - Sa3) * r,
				x + w - r + Ca2 * r, y + h + (1 - Sa2) * r);
			QuadsDrawFreeform(&ItemF, 1);
		}
	}

	SetColor4(ColorTopLeft, ColorTopRight, ColorBottomLeft, ColorBottomRight);
	CQuadItem ItemQ = CQuadItem(x + r, y + r, w - r * 2, h - r * 2); // center
	QuadsDrawTL(&ItemQ, 1);

	SetColor4(ColorTopLeft, ColorTopRight, ColorTopLeft, ColorTopRight);
	ItemQ = CQuadItem(x + r, y, w - r * 2, r); // top
	QuadsDrawTL(&ItemQ, 1);

	SetColor4(ColorBottomLeft, ColorBottomRight, ColorBottomLeft, ColorBottomRight);
	ItemQ = CQuadItem(x + r, y + h - r, w - r * 2, r); // bottom
	QuadsDrawTL(&ItemQ, 1);

	SetColor4(ColorTopLeft, ColorTopLeft, ColorBottomLeft, ColorBottomLeft);
	ItemQ = CQuadItem(x, y + r, r, h - r * 2); // left
	QuadsDrawTL(&ItemQ, 1);

	SetColor4(ColorTopRight, ColorTopRight, ColorBottomRight, ColorBottomRight);
	ItemQ = CQuadItem(x + w - r, y + r, r, h - r * 2); // right
	QuadsDrawTL(&ItemQ, 1);

	if(!(Corners & CORNER_TL))
	{
		SetColor(ColorTopLeft);
		ItemQ = CQuadItem(x, y, r, r);
		QuadsDrawTL(&ItemQ, 1);
	}

	if(!(Corners & CORNER_TR))
	{
		SetColor(ColorTopRight);
		ItemQ = CQuadItem(x + w, y, -r, r);
		QuadsDrawTL(&ItemQ, 1);
	}

	if(!(Corners & CORNER_BL))
	{
		SetColor(ColorBottomLeft);
		ItemQ = CQuadItem(x, y + h, r, -r);
		QuadsDrawTL(&ItemQ, 1);
	}

	if(!(Corners & CORNER_BR))
	{
		SetColor(ColorBottomRight);
		ItemQ = CQuadItem(x + w, y + h, -r, -r);
		QuadsDrawTL(&ItemQ, 1);
	}
}

int CGraphics_Threaded::CreateRectQuadContainer(float x, float y, float w, float h, float r, int Corners)
{
	int ContainerIndex = CreateQuadContainer(false);

	if(Corners == 0 || r == 0.0f)
	{
		CQuadItem ItemQ = CQuadItem(x, y, w, h);
		QuadContainerAddQuads(ContainerIndex, &ItemQ, 1);
		QuadContainerUpload(ContainerIndex);
		QuadContainerChangeAutomaticUpload(ContainerIndex, true);
		return ContainerIndex;
	}

	const int NumSegments = 8;
	const float SegmentsAngle = pi / 2 / NumSegments;
	IGraphics::CFreeformItem aFreeform[NumSegments * 4];
	size_t NumItems = 0;

	for(int i = 0; i < NumSegments; i += 2)
	{
		float a1 = i * SegmentsAngle;
		float a2 = (i + 1) * SegmentsAngle;
		float a3 = (i + 2) * SegmentsAngle;
		float Ca1 = cosf(a1);
		float Ca2 = cosf(a2);
		float Ca3 = cosf(a3);
		float Sa1 = sinf(a1);
		float Sa2 = sinf(a2);
		float Sa3 = sinf(a3);

		if(Corners & CORNER_TL)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + r, y + r,
				x + (1 - Ca1) * r, y + (1 - Sa1) * r,
				x + (1 - Ca3) * r, y + (1 - Sa3) * r,
				x + (1 - Ca2) * r, y + (1 - Sa2) * r);

		if(Corners & CORNER_TR)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + w - r, y + r,
				x + w - r + Ca1 * r, y + (1 - Sa1) * r,
				x + w - r + Ca3 * r, y + (1 - Sa3) * r,
				x + w - r + Ca2 * r, y + (1 - Sa2) * r);

		if(Corners & CORNER_BL)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + r, y + h - r,
				x + (1 - Ca1) * r, y + h - r + Sa1 * r,
				x + (1 - Ca3) * r, y + h - r + Sa3 * r,
				x + (1 - Ca2) * r, y + h - r + Sa2 * r);

		if(Corners & CORNER_BR)
			aFreeform[NumItems++] = IGraphics::CFreeformItem(
				x + w - r, y + h - r,
				x + w - r + Ca1 * r, y + h - r + Sa1 * r,
				x + w - r + Ca3 * r, y + h - r + Sa3 * r,
				x + w - r + Ca2 * r, y + h - r + Sa2 * r);
	}

	if(NumItems > 0)
		QuadContainerAddQuads(ContainerIndex, aFreeform, NumItems);

	CQuadItem aQuads[9];
	NumItems = 0;
	aQuads[NumItems++] = CQuadItem(x + r, y + r, w - r * 2, h - r * 2); // center
	aQuads[NumItems++] = CQuadItem(x + r, y, w - r * 2, r); // top
	aQuads[NumItems++] = CQuadItem(x + r, y + h - r, w - r * 2, r); // bottom
	aQuads[NumItems++] = CQuadItem(x, y + r, r, h - r * 2); // left
	aQuads[NumItems++] = CQuadItem(x + w - r, y + r, r, h - r * 2); // right

	if(!(Corners & CORNER_TL))
		aQuads[NumItems++] = CQuadItem(x, y, r, r);
	if(!(Corners & CORNER_TR))
		aQuads[NumItems++] = CQuadItem(x + w, y, -r, r);
	if(!(Corners & CORNER_BL))
		aQuads[NumItems++] = CQuadItem(x, y + h, r, -r);
	if(!(Corners & CORNER_BR))
		aQuads[NumItems++] = CQuadItem(x + w, y + h, -r, -r);

	if(NumItems > 0)
		QuadContainerAddQuads(ContainerIndex, aQuads, NumItems);

	QuadContainerUpload(ContainerIndex);
	QuadContainerChangeAutomaticUpload(ContainerIndex, true);

	return ContainerIndex;
}

void CGraphics_Threaded::DrawRect(float x, float y, float w, float h, ColorRGBA Color, int Corners, float Rounding)
{
	TextureClear();
	QuadsBegin();
	SetColor(Color);
	DrawRectExt(x, y, w, h, Rounding, Corners);
	QuadsEnd();
}

void CGraphics_Threaded::DrawRect4(float x, float y, float w, float h, ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, int Corners, float Rounding)
{
	TextureClear();
	QuadsBegin();
	DrawRectExt4(x, y, w, h, ColorTopLeft, ColorTopRight, ColorBottomLeft, ColorBottomRight, Rounding, Corners);
	QuadsEnd();
}

void CGraphics_Threaded::DrawCircle(float CenterX, float CenterY, float Radius, int Segments)
{
	IGraphics::CFreeformItem aItems[32];
	size_t NumItems = 0;
	const float SegmentsAngle = 2 * pi / Segments;
	for(int i = 0; i < Segments; i += 2)
	{
		const float a1 = i * SegmentsAngle;
		const float a2 = (i + 1) * SegmentsAngle;
		const float a3 = (i + 2) * SegmentsAngle;
		aItems[NumItems++] = IGraphics::CFreeformItem(
			CenterX, CenterY,
			CenterX + cosf(a1) * Radius, CenterY + sinf(a1) * Radius,
			CenterX + cosf(a3) * Radius, CenterY + sinf(a3) * Radius,
			CenterX + cosf(a2) * Radius, CenterY + sinf(a2) * Radius);
		if(NumItems == std::size(aItems))
		{
			QuadsDrawFreeform(aItems, std::size(aItems));
			NumItems = 0;
		}
	}
	if(NumItems)
		QuadsDrawFreeform(aItems, NumItems);
}

void CGraphics_Threaded::RenderTileLayer(int BufferContainerIndex, const ColorRGBA &Color, char **pOffsets, unsigned int *pIndicedVertexDrawNum, size_t NumIndicesOffset)
{
	if(NumIndicesOffset == 0)
		return;

	// add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderTileLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndicesDrawNum = NumIndicesOffset;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	Cmd.m_Color = Color;

	void *pData = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffset);
	if(pData == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		pData = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffset);
		if(pData == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}
	Cmd.m_pIndicesOffsets = (char **)pData;
	Cmd.m_pDrawCount = (unsigned int *)(((char *)pData) + (sizeof(char *) * NumIndicesOffset));

	if(!AddCmd(
		   Cmd, [&] {
			   pData = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffset);
			   if(pData == 0x0)
			   {
				   dbg_msg("graphics", "failed to allocate data for vertices");
				   return false;
			   }
			   Cmd.m_pIndicesOffsets = (char **)pData;
			   Cmd.m_pDrawCount = (unsigned int *)(((char *)pData) + (sizeof(char *) * NumIndicesOffset));
			   return true;
		   },
		   "failed to allocate memory for render command"))
	{
		return;
	}

	mem_copy(Cmd.m_pIndicesOffsets, pOffsets, sizeof(char *) * NumIndicesOffset);
	mem_copy(Cmd.m_pDrawCount, pIndicedVertexDrawNum, sizeof(unsigned int) * NumIndicesOffset);

	m_pCommandBuffer->AddRenderCalls(NumIndicesOffset);
	// todo max indices group check!!
}

void CGraphics_Threaded::RenderBorderTiles(int BufferContainerIndex, const ColorRGBA &Color, char *pIndexBufferOffset, const vec2 &Offset, const vec2 &Dir, int JumpIndex, unsigned int DrawNum)
{
	if(DrawNum == 0)
		return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTile Cmd;
	Cmd.m_State = m_State;
	Cmd.m_DrawNum = DrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	Cmd.m_Color = Color;

	Cmd.m_pIndicesOffset = pIndexBufferOffset;
	Cmd.m_JumpIndex = JumpIndex;

	Cmd.m_Offset = Offset;
	Cmd.m_Dir = Dir;

	// check if we have enough free memory in the commandbuffer
	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for render command"))
	{
		return;
	}

	m_pCommandBuffer->AddRenderCalls(1);
}

void CGraphics_Threaded::RenderBorderTileLines(int BufferContainerIndex, const ColorRGBA &Color, char *pIndexBufferOffset, const vec2 &Offset, const vec2 &Dir, unsigned int IndexDrawNum, unsigned int RedrawNum)
{
	if(IndexDrawNum == 0 || RedrawNum == 0)
		return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTileLine Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndexDrawNum = IndexDrawNum;
	Cmd.m_DrawNum = RedrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	Cmd.m_Color = Color;

	Cmd.m_pIndicesOffset = pIndexBufferOffset;

	Cmd.m_Offset = Offset;
	Cmd.m_Dir = Dir;

	// check if we have enough free memory in the commandbuffer
	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for render command"))
	{
		return;
	}

	m_pCommandBuffer->AddRenderCalls(1);
}

void CGraphics_Threaded::RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, int QuadNum, int QuadOffset)
{
	if(QuadNum == 0)
		return;

	// add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderQuadLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_QuadNum = QuadNum;
	Cmd.m_QuadOffset = QuadOffset;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;

	Cmd.m_pQuadInfo = (SQuadRenderInfo *)AllocCommandBufferData(QuadNum * sizeof(SQuadRenderInfo));
	if(Cmd.m_pQuadInfo == 0x0)
		return;

	// check if we have enough free memory in the commandbuffer
	if(!AddCmd(
		   Cmd, [&] {
			   Cmd.m_pQuadInfo = (SQuadRenderInfo *)m_pCommandBuffer->AllocData(QuadNum * sizeof(SQuadRenderInfo));
			   if(Cmd.m_pQuadInfo == 0x0)
			   {
				   dbg_msg("graphics", "failed to allocate data for the quad info");
				   return false;
			   }
			   return true;
		   },
		   "failed to allocate memory for render quad command"))
	{
		return;
	}

	mem_copy(Cmd.m_pQuadInfo, pQuadInfo, sizeof(SQuadRenderInfo) * QuadNum);

	m_pCommandBuffer->AddRenderCalls(((QuadNum - 1) / gs_GraphicsMaxQuadsRenderCount) + 1);
}

void CGraphics_Threaded::RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor)
{
	if(BufferContainerIndex == -1)
		return;

	CCommandBuffer::SCommand_RenderText Cmd;
	Cmd.m_State = m_State;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	Cmd.m_DrawNum = TextQuadNum * 6;
	Cmd.m_TextureSize = TextureSize;
	Cmd.m_TextTextureIndex = TextureTextIndex;
	Cmd.m_TextOutlineTextureIndex = TextureTextOutlineIndex;
	Cmd.m_TextColor = TextColor;
	Cmd.m_TextOutlineColor = TextOutlineColor;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for render text command"))
	{
		return;
	}

	m_pCommandBuffer->AddRenderCalls(1);
}

int CGraphics_Threaded::CreateQuadContainer(bool AutomaticUpload)
{
	int Index = -1;
	if(m_FirstFreeQuadContainer == -1)
	{
		Index = m_vQuadContainers.size();
		m_vQuadContainers.emplace_back(AutomaticUpload);
	}
	else
	{
		Index = m_FirstFreeQuadContainer;
		m_FirstFreeQuadContainer = m_vQuadContainers[Index].m_FreeIndex;
		m_vQuadContainers[Index].m_FreeIndex = Index;
	}

	return Index;
}

void CGraphics_Threaded::QuadContainerChangeAutomaticUpload(int ContainerIndex, bool AutomaticUpload)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];
	Container.m_AutomaticUpload = AutomaticUpload;
}

void CGraphics_Threaded::QuadContainerUpload(int ContainerIndex)
{
	if(IsQuadContainerBufferingEnabled())
	{
		SQuadContainer &Container = m_vQuadContainers[ContainerIndex];
		if(!Container.m_vQuads.empty())
		{
			if(Container.m_QuadBufferObjectIndex == -1)
			{
				size_t UploadDataSize = Container.m_vQuads.size() * sizeof(SQuadContainer::SQuad);
				Container.m_QuadBufferObjectIndex = CreateBufferObject(UploadDataSize, Container.m_vQuads.data(), 0);
			}
			else
			{
				size_t UploadDataSize = Container.m_vQuads.size() * sizeof(SQuadContainer::SQuad);
				RecreateBufferObject(Container.m_QuadBufferObjectIndex, UploadDataSize, Container.m_vQuads.data(), 0);
			}

			if(Container.m_QuadBufferContainerIndex == -1)
			{
				SBufferContainerInfo Info;
				Info.m_Stride = sizeof(CCommandBuffer::SVertex);
				Info.m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;

				Info.m_vAttributes.emplace_back();
				SBufferContainerInfo::SAttribute *pAttr = &Info.m_vAttributes.back();
				pAttr->m_DataTypeCount = 2;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = false;
				pAttr->m_pOffset = 0;
				pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
				Info.m_vAttributes.emplace_back();
				pAttr = &Info.m_vAttributes.back();
				pAttr->m_DataTypeCount = 2;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = false;
				pAttr->m_pOffset = (void *)(sizeof(float) * 2);
				pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
				Info.m_vAttributes.emplace_back();
				pAttr = &Info.m_vAttributes.back();
				pAttr->m_DataTypeCount = 4;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = true;
				pAttr->m_pOffset = (void *)(sizeof(float) * 2 + sizeof(float) * 2);
				pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;

				Container.m_QuadBufferContainerIndex = CreateBufferContainer(&Info);
			}
		}
	}
}

int CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];

	if((int)Container.m_vQuads.size() > Num + CCommandBuffer::CCommandBuffer::MAX_VERTICES)
		return -1;

	int RetOff = (int)Container.m_vQuads.size();

	for(int i = 0; i < Num; ++i)
	{
		Container.m_vQuads.emplace_back();
		SQuadContainer::SQuad &Quad = Container.m_vQuads.back();

		Quad.m_aVertices[0].m_Pos.x = pArray[i].m_X;
		Quad.m_aVertices[0].m_Pos.y = pArray[i].m_Y;
		Quad.m_aVertices[0].m_Tex = m_aTexture[0];
		SetColor(&Quad.m_aVertices[0], 0);

		Quad.m_aVertices[1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
		Quad.m_aVertices[1].m_Pos.y = pArray[i].m_Y;
		Quad.m_aVertices[1].m_Tex = m_aTexture[1];
		SetColor(&Quad.m_aVertices[1], 1);

		Quad.m_aVertices[2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
		Quad.m_aVertices[2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
		Quad.m_aVertices[2].m_Tex = m_aTexture[2];
		SetColor(&Quad.m_aVertices[2], 2);

		Quad.m_aVertices[3].m_Pos.x = pArray[i].m_X;
		Quad.m_aVertices[3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
		Quad.m_aVertices[3].m_Tex = m_aTexture[3];
		SetColor(&Quad.m_aVertices[3], 3);

		if(m_Rotation != 0)
		{
			CCommandBuffer::SPoint Center;
			Center.x = pArray[i].m_X + pArray[i].m_Width / 2;
			Center.y = pArray[i].m_Y + pArray[i].m_Height / 2;

			Rotate(Center, Quad.m_aVertices, 4);
		}
	}

	if(Container.m_AutomaticUpload)
		QuadContainerUpload(ContainerIndex);

	return RetOff;
}

int CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];

	if((int)Container.m_vQuads.size() > Num + CCommandBuffer::CCommandBuffer::MAX_VERTICES)
		return -1;

	int RetOff = (int)Container.m_vQuads.size();

	for(int i = 0; i < Num; ++i)
	{
		Container.m_vQuads.emplace_back();
		SQuadContainer::SQuad &Quad = Container.m_vQuads.back();

		Quad.m_aVertices[0].m_Pos.x = pArray[i].m_X0;
		Quad.m_aVertices[0].m_Pos.y = pArray[i].m_Y0;
		Quad.m_aVertices[0].m_Tex = m_aTexture[0];
		SetColor(&Quad.m_aVertices[0], 0);

		Quad.m_aVertices[1].m_Pos.x = pArray[i].m_X1;
		Quad.m_aVertices[1].m_Pos.y = pArray[i].m_Y1;
		Quad.m_aVertices[1].m_Tex = m_aTexture[1];
		SetColor(&Quad.m_aVertices[1], 1);

		Quad.m_aVertices[2].m_Pos.x = pArray[i].m_X3;
		Quad.m_aVertices[2].m_Pos.y = pArray[i].m_Y3;
		Quad.m_aVertices[2].m_Tex = m_aTexture[3];
		SetColor(&Quad.m_aVertices[2], 3);

		Quad.m_aVertices[3].m_Pos.x = pArray[i].m_X2;
		Quad.m_aVertices[3].m_Pos.y = pArray[i].m_Y2;
		Quad.m_aVertices[3].m_Tex = m_aTexture[2];
		SetColor(&Quad.m_aVertices[3], 2);
	}

	if(Container.m_AutomaticUpload)
		QuadContainerUpload(ContainerIndex);

	return RetOff;
}

void CGraphics_Threaded::QuadContainerReset(int ContainerIndex)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];
	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex != -1)
			DeleteBufferContainer(Container.m_QuadBufferContainerIndex, true);
	}
	Container.m_vQuads.clear();
	Container.m_QuadBufferContainerIndex = Container.m_QuadBufferObjectIndex = -1;
}

void CGraphics_Threaded::DeleteQuadContainer(int ContainerIndex)
{
	QuadContainerReset(ContainerIndex);

	// also clear the container index
	m_vQuadContainers[ContainerIndex].m_FreeIndex = m_FirstFreeQuadContainer;
	m_FirstFreeQuadContainer = ContainerIndex;
}

void CGraphics_Threaded::RenderQuadContainer(int ContainerIndex, int QuadDrawNum)
{
	RenderQuadContainer(ContainerIndex, 0, QuadDrawNum);
}

void CGraphics_Threaded::RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum, bool ChangeWrapMode)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];

	if(QuadDrawNum == -1)
		QuadDrawNum = (int)Container.m_vQuads.size() - QuadOffset;

	if((int)Container.m_vQuads.size() < QuadOffset + QuadDrawNum || QuadDrawNum == 0)
		return;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		if(ChangeWrapMode)
			WrapClamp();

		CCommandBuffer::SCommand_RenderQuadContainer Cmd;
		Cmd.m_State = m_State;
		Cmd.m_DrawNum = (unsigned int)QuadDrawNum * 6;
		Cmd.m_pOffset = (void *)(QuadOffset * 6 * sizeof(unsigned int));
		Cmd.m_BufferContainerIndex = Container.m_QuadBufferContainerIndex;

		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to allocate memory for render quad container"))
		{
			return;
		}

		m_pCommandBuffer->AddRenderCalls(1);
	}
	else
	{
		if(g_Config.m_GfxQuadAsTriangle)
		{
			for(int i = 0; i < QuadDrawNum; ++i)
			{
				SQuadContainer::SQuad &Quad = Container.m_vQuads[QuadOffset + i];
				m_aVertices[i * 6] = Quad.m_aVertices[0];
				m_aVertices[i * 6 + 1] = Quad.m_aVertices[1];
				m_aVertices[i * 6 + 2] = Quad.m_aVertices[2];
				m_aVertices[i * 6 + 3] = Quad.m_aVertices[0];
				m_aVertices[i * 6 + 4] = Quad.m_aVertices[2];
				m_aVertices[i * 6 + 5] = Quad.m_aVertices[3];
				m_NumVertices += 6;
			}
		}
		else
		{
			mem_copy(m_aVertices, &Container.m_vQuads[QuadOffset], sizeof(CCommandBuffer::SVertex) * 4 * QuadDrawNum);
			m_NumVertices += 4 * QuadDrawNum;
		}
		m_Drawing = DRAWING_QUADS;
		if(ChangeWrapMode)
			WrapClamp();
		FlushVertices(false);
		m_Drawing = 0;
	}
	WrapNormal();
}

void CGraphics_Threaded::RenderQuadContainerEx(int ContainerIndex, int QuadOffset, int QuadDrawNum, float X, float Y, float ScaleX, float ScaleY)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];

	if((int)Container.m_vQuads.size() < QuadOffset + 1)
		return;

	if(QuadDrawNum == -1)
		QuadDrawNum = (int)Container.m_vQuads.size() - QuadOffset;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		SQuadContainer::SQuad &Quad = Container.m_vQuads[QuadOffset];
		CCommandBuffer::SCommand_RenderQuadContainerEx Cmd;

		WrapClamp();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		MapScreen((ScreenX0 - X) / ScaleX, (ScreenY0 - Y) / ScaleY, (ScreenX1 - X) / ScaleX, (ScreenY1 - Y) / ScaleY);
		Cmd.m_State = m_State;
		MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);

		Cmd.m_DrawNum = QuadDrawNum * 6;
		Cmd.m_pOffset = (void *)(QuadOffset * 6 * sizeof(unsigned int));
		Cmd.m_BufferContainerIndex = Container.m_QuadBufferContainerIndex;

		Cmd.m_VertexColor.r = (float)m_aColor[0].r / 255.f;
		Cmd.m_VertexColor.g = (float)m_aColor[0].g / 255.f;
		Cmd.m_VertexColor.b = (float)m_aColor[0].b / 255.f;
		Cmd.m_VertexColor.a = (float)m_aColor[0].a / 255.f;

		Cmd.m_Rotation = m_Rotation;

		// rotate before positioning
		Cmd.m_Center.x = Quad.m_aVertices[0].m_Pos.x + (Quad.m_aVertices[1].m_Pos.x - Quad.m_aVertices[0].m_Pos.x) / 2.f;
		Cmd.m_Center.y = Quad.m_aVertices[0].m_Pos.y + (Quad.m_aVertices[2].m_Pos.y - Quad.m_aVertices[0].m_Pos.y) / 2.f;

		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to allocate memory for render quad container extended"))
		{
			return;
		}

		m_pCommandBuffer->AddRenderCalls(1);
	}
	else
	{
		if(g_Config.m_GfxQuadAsTriangle)
		{
			for(int i = 0; i < QuadDrawNum; ++i)
			{
				SQuadContainer::SQuad &Quad = Container.m_vQuads[QuadOffset + i];
				m_aVertices[i * 6 + 0] = Quad.m_aVertices[0];
				m_aVertices[i * 6 + 1] = Quad.m_aVertices[1];
				m_aVertices[i * 6 + 2] = Quad.m_aVertices[2];
				m_aVertices[i * 6 + 3] = Quad.m_aVertices[0];
				m_aVertices[i * 6 + 4] = Quad.m_aVertices[2];
				m_aVertices[i * 6 + 5] = Quad.m_aVertices[3];

				for(int n = 0; n < 6; ++n)
				{
					m_aVertices[i * 6 + n].m_Pos.x *= ScaleX;
					m_aVertices[i * 6 + n].m_Pos.y *= ScaleY;

					SetColor(&m_aVertices[i * 6 + n], 0);
				}

				if(m_Rotation != 0)
				{
					CCommandBuffer::SPoint Center;
					Center.x = m_aVertices[i * 6 + 0].m_Pos.x + (m_aVertices[i * 6 + 1].m_Pos.x - m_aVertices[i * 6 + 0].m_Pos.x) / 2.f;
					Center.y = m_aVertices[i * 6 + 0].m_Pos.y + (m_aVertices[i * 6 + 2].m_Pos.y - m_aVertices[i * 6 + 0].m_Pos.y) / 2.f;

					Rotate(Center, &m_aVertices[i * 6 + 0], 6);
				}

				for(int n = 0; n < 6; ++n)
				{
					m_aVertices[i * 6 + n].m_Pos.x += X;
					m_aVertices[i * 6 + n].m_Pos.y += Y;
				}
				m_NumVertices += 6;
			}
		}
		else
		{
			mem_copy(m_aVertices, &Container.m_vQuads[QuadOffset], sizeof(CCommandBuffer::SVertex) * 4 * QuadDrawNum);
			for(int i = 0; i < QuadDrawNum; ++i)
			{
				for(int n = 0; n < 4; ++n)
				{
					m_aVertices[i * 4 + n].m_Pos.x *= ScaleX;
					m_aVertices[i * 4 + n].m_Pos.y *= ScaleY;
					SetColor(&m_aVertices[i * 4 + n], 0);
				}

				if(m_Rotation != 0)
				{
					CCommandBuffer::SPoint Center;
					Center.x = m_aVertices[i * 4 + 0].m_Pos.x + (m_aVertices[i * 4 + 1].m_Pos.x - m_aVertices[i * 4 + 0].m_Pos.x) / 2.f;
					Center.y = m_aVertices[i * 4 + 0].m_Pos.y + (m_aVertices[i * 4 + 2].m_Pos.y - m_aVertices[i * 4 + 0].m_Pos.y) / 2.f;

					Rotate(Center, &m_aVertices[i * 4 + 0], 4);
				}

				for(int n = 0; n < 4; ++n)
				{
					m_aVertices[i * 4 + n].m_Pos.x += X;
					m_aVertices[i * 4 + n].m_Pos.y += Y;
				}
				m_NumVertices += 4;
			}
		}
		m_Drawing = DRAWING_QUADS;
		WrapClamp();
		FlushVertices(false);
		m_Drawing = 0;
	}
	WrapNormal();
}

void CGraphics_Threaded::RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX, float ScaleY)
{
	RenderQuadContainerEx(ContainerIndex, QuadOffset, 1, X, Y, ScaleX, ScaleY);
}

void CGraphics_Threaded::RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo)
{
	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];

	if(DrawCount == 0)
		return;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		WrapClamp();
		SQuadContainer::SQuad &Quad = Container.m_vQuads[0];
		CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple Cmd;

		Cmd.m_State = m_State;

		Cmd.m_DrawNum = 1 * 6;
		Cmd.m_DrawCount = DrawCount;
		Cmd.m_pOffset = (void *)(QuadOffset * 6 * sizeof(unsigned int));
		Cmd.m_BufferContainerIndex = Container.m_QuadBufferContainerIndex;

		Cmd.m_VertexColor.r = (float)m_aColor[0].r / 255.f;
		Cmd.m_VertexColor.g = (float)m_aColor[0].g / 255.f;
		Cmd.m_VertexColor.b = (float)m_aColor[0].b / 255.f;
		Cmd.m_VertexColor.a = (float)m_aColor[0].a / 255.f;

		// rotate before positioning
		Cmd.m_Center.x = Quad.m_aVertices[0].m_Pos.x + (Quad.m_aVertices[1].m_Pos.x - Quad.m_aVertices[0].m_Pos.x) / 2.f;
		Cmd.m_Center.y = Quad.m_aVertices[0].m_Pos.y + (Quad.m_aVertices[2].m_Pos.y - Quad.m_aVertices[0].m_Pos.y) / 2.f;

		Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
		if(Cmd.m_pRenderInfo == 0x0)
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
			if(Cmd.m_pRenderInfo == 0x0)
			{
				dbg_msg("graphics", "failed to allocate data for render info");
				return;
			}
		}

		if(!AddCmd(
			   Cmd, [&] {
				   Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
				   if(Cmd.m_pRenderInfo == 0x0)
				   {
					   dbg_msg("graphics", "failed to allocate data for render info");
					   return false;
				   }
				   return true;
			   },
			   "failed to allocate memory for render quad container sprite"))
		{
			return;
		}

		mem_copy(Cmd.m_pRenderInfo, pRenderInfo, sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);

		m_pCommandBuffer->AddRenderCalls(((DrawCount - 1) / gs_GraphicsMaxParticlesRenderCount) + 1);

		WrapNormal();
	}
	else
	{
		for(int i = 0; i < DrawCount; ++i)
		{
			QuadsSetRotation(pRenderInfo[i].m_Rotation);
			RenderQuadContainerAsSprite(ContainerIndex, QuadOffset, pRenderInfo[i].m_Pos.x, pRenderInfo[i].m_Pos.y, pRenderInfo[i].m_Scale, pRenderInfo[i].m_Scale);
		}
	}
}

void *CGraphics_Threaded::AllocCommandBufferData(unsigned AllocSize)
{
	void *pData = m_pCommandBuffer->AllocData(AllocSize);
	if(pData == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		pData = m_pCommandBuffer->AllocData(AllocSize);
		if(pData == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for command buffer");
			return NULL;
		}
	}
	return pData;
}

int CGraphics_Threaded::CreateBufferObject(size_t UploadDataSize, void *pUploadData, int CreateFlags, bool IsMovedPointer)
{
	int Index = -1;
	if(m_FirstFreeBufferObjectIndex == -1)
	{
		Index = m_vBufferObjectIndices.size();
		m_vBufferObjectIndices.push_back(Index);
	}
	else
	{
		Index = m_FirstFreeBufferObjectIndex;
		m_FirstFreeBufferObjectIndex = m_vBufferObjectIndices[Index];
		m_vBufferObjectIndices[Index] = Index;
	}

	CCommandBuffer::SCommand_CreateBufferObject Cmd;
	Cmd.m_BufferIndex = Index;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_DeletePointer = IsMovedPointer;
	Cmd.m_Flags = CreateFlags;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to allocate memory for update buffer object command"))
		{
			return -1;
		}
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
			if(Cmd.m_pUploadData == NULL)
				return -1;

			if(!AddCmd(
				   Cmd, [&] {
					   Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
					   if(Cmd.m_pUploadData == 0x0)
					   {
						   dbg_msg("graphics", "failed to allocate data for upload data");
						   return false;
					   }
					   return true;
				   },
				   "failed to allocate memory for create buffer object command"))
			{
				return -1;
			}

			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = NULL;

			if(!AddCmd(
				   Cmd, [] { return true; }, "failed to allocate memory for create buffer object command"))
			{
				return -1;
			}

			// update the buffer instead
			size_t UploadDataOffset = 0;
			while(UploadDataSize > 0)
			{
				size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

				UpdateBufferObjectInternal(Index, UpdateSize, (((char *)pUploadData) + UploadDataOffset), (void *)UploadDataOffset);

				UploadDataOffset += UpdateSize;
				UploadDataSize -= UpdateSize;
			}
		}
	}

	return Index;
}

void CGraphics_Threaded::RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, int CreateFlags, bool IsMovedPointer)
{
	CCommandBuffer::SCommand_RecreateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_DeletePointer = IsMovedPointer;
	Cmd.m_Flags = CreateFlags;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to allocate memory for recreate buffer object command"))
		{
			return;
		}
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
			if(Cmd.m_pUploadData == NULL)
				return;

			if(!AddCmd(
				   Cmd, [&] {
					   Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
					   if(Cmd.m_pUploadData == 0x0)
					   {
						   dbg_msg("graphics", "failed to allocate data for upload data");
						   return false;
					   }
					   return true;
				   },
				   "failed to allocate memory for recreate buffer object command"))
			{
				return;
			}

			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = NULL;

			if(!AddCmd(
				   Cmd, [] { return true; }, "failed to allocate memory for update buffer object command"))
			{
				return;
			}

			// update the buffer instead
			size_t UploadDataOffset = 0;
			while(UploadDataSize > 0)
			{
				size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

				UpdateBufferObjectInternal(BufferIndex, UpdateSize, (((char *)pUploadData) + UploadDataOffset), (void *)UploadDataOffset);

				UploadDataOffset += UpdateSize;
				UploadDataSize -= UpdateSize;
			}
		}
	}
}

void CGraphics_Threaded::UpdateBufferObjectInternal(int BufferIndex, size_t UploadDataSize, void *pUploadData, void *pOffset, bool IsMovedPointer)
{
	CCommandBuffer::SCommand_UpdateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_pOffset = pOffset;
	Cmd.m_DeletePointer = IsMovedPointer;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to allocate memory for update buffer object command"))
		{
			return;
		}
	}
	else
	{
		Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
		if(Cmd.m_pUploadData == NULL)
			return;

		if(!AddCmd(
			   Cmd, [&] {
				   Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
				   if(Cmd.m_pUploadData == 0x0)
				   {
					   dbg_msg("graphics", "failed to allocate data for upload data");
					   return false;
				   }
				   return true;
			   },
			   "failed to allocate memory for update buffer object command"))
		{
			return;
		}

		mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
	}
}

void CGraphics_Threaded::CopyBufferObjectInternal(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize)
{
	CCommandBuffer::SCommand_CopyBufferObject Cmd;
	Cmd.m_WriteBufferIndex = WriteBufferIndex;
	Cmd.m_ReadBufferIndex = ReadBufferIndex;
	Cmd.m_WriteOffset = WriteOffset;
	Cmd.m_ReadOffset = ReadOffset;
	Cmd.m_CopySize = CopyDataSize;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for copy buffer object command"))
	{
		return;
	}
}

void CGraphics_Threaded::DeleteBufferObject(int BufferIndex)
{
	if(BufferIndex == -1)
		return;

	CCommandBuffer::SCommand_DeleteBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for delete buffer object command"))
	{
		return;
	}

	// also clear the buffer object index
	m_vBufferObjectIndices[BufferIndex] = m_FirstFreeBufferObjectIndex;
	m_FirstFreeBufferObjectIndex = BufferIndex;
}

int CGraphics_Threaded::CreateBufferContainer(SBufferContainerInfo *pContainerInfo)
{
	int Index = -1;
	if(m_FirstFreeVertexArrayInfo == -1)
	{
		Index = m_vVertexArrayInfo.size();
		m_vVertexArrayInfo.emplace_back();
	}
	else
	{
		Index = m_FirstFreeVertexArrayInfo;
		m_FirstFreeVertexArrayInfo = m_vVertexArrayInfo[Index].m_FreeIndex;
		m_vVertexArrayInfo[Index].m_FreeIndex = Index;
	}

	CCommandBuffer::SCommand_CreateBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = Index;
	Cmd.m_AttrCount = (int)pContainerInfo->m_vAttributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;
	Cmd.m_VertBufferBindingIndex = pContainerInfo->m_VertBufferBindingIndex;

	Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_pAttributes == nullptr)
		return -1;

	if(!AddCmd(
		   Cmd, [&] {
			   Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
			   if(Cmd.m_pAttributes == nullptr)
			   {
				   dbg_msg("graphics", "failed to allocate data for upload data");
				   return false;
			   }
			   return true;
		   },
		   "failed to allocate memory for create buffer container command"))
	{
		return -1;
	}

	mem_copy(Cmd.m_pAttributes, pContainerInfo->m_vAttributes.data(), Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	m_vVertexArrayInfo[Index].m_AssociatedBufferObjectIndex = pContainerInfo->m_VertBufferBindingIndex;

	return Index;
}

void CGraphics_Threaded::DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO)
{
	if(ContainerIndex == -1)
		return;
	CCommandBuffer::SCommand_DeleteBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_DestroyAllBO = DestroyAllBO;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for delete buffer container command"))
	{
		return;
	}

	if(DestroyAllBO)
	{
		// delete all associated references
		int BufferObjectIndex = m_vVertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndex;
		if(BufferObjectIndex != -1)
		{
			// clear the buffer object index
			m_vBufferObjectIndices[BufferObjectIndex] = m_FirstFreeBufferObjectIndex;
			m_FirstFreeBufferObjectIndex = BufferObjectIndex;
		}
	}
	m_vVertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndex = -1;

	// also clear the buffer object index
	m_vVertexArrayInfo[ContainerIndex].m_FreeIndex = m_FirstFreeVertexArrayInfo;
	m_FirstFreeVertexArrayInfo = ContainerIndex;
}

void CGraphics_Threaded::UpdateBufferContainerInternal(int ContainerIndex, SBufferContainerInfo *pContainerInfo)
{
	CCommandBuffer::SCommand_UpdateBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_AttrCount = (int)pContainerInfo->m_vAttributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;
	Cmd.m_VertBufferBindingIndex = pContainerInfo->m_VertBufferBindingIndex;

	Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_pAttributes == nullptr)
		return;

	if(!AddCmd(
		   Cmd, [&] {
			   Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
			   if(Cmd.m_pAttributes == nullptr)
			   {
				   dbg_msg("graphics", "failed to allocate data for upload data");
				   return false;
			   }
			   return true;
		   },
		   "failed to allocate memory for update buffer container command"))
	{
		return;
	}

	mem_copy(Cmd.m_pAttributes, pContainerInfo->m_vAttributes.data(), Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	m_vVertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndex = pContainerInfo->m_VertBufferBindingIndex;
}

void CGraphics_Threaded::IndicesNumRequiredNotify(unsigned int RequiredIndicesCount)
{
	CCommandBuffer::SCommand_IndicesRequiredNumNotify Cmd;
	Cmd.m_RequiredIndicesNum = RequiredIndicesCount;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to allocate memory for indcies required count notify command"))
	{
		return;
	}
}

int CGraphics_Threaded::IssueInit()
{
	int Flags = 0;

	bool IsPurlyWindowed = g_Config.m_GfxFullscreen == 0;
	bool IsExclusiveFullscreen = g_Config.m_GfxFullscreen == 1;
	bool IsDesktopFullscreen = g_Config.m_GfxFullscreen == 2;
#ifndef CONF_FAMILY_WINDOWS
	//  special mode for windows only
	IsDesktopFullscreen |= g_Config.m_GfxFullscreen == 3;
#endif

	if(g_Config.m_GfxBorderless)
		Flags |= IGraphicsBackend::INITFLAG_BORDERLESS;
	if(IsExclusiveFullscreen)
		Flags |= IGraphicsBackend::INITFLAG_FULLSCREEN;
	else if(IsDesktopFullscreen)
		Flags |= IGraphicsBackend::INITFLAG_DESKTOP_FULLSCREEN;
	if(IsPurlyWindowed || IsExclusiveFullscreen || IsDesktopFullscreen)
		Flags |= IGraphicsBackend::INITFLAG_RESIZABLE;
	if(g_Config.m_GfxVsync)
		Flags |= IGraphicsBackend::INITFLAG_VSYNC;
	if(g_Config.m_GfxHighdpi)
		Flags |= IGraphicsBackend::INITFLAG_HIGHDPI;

	int r = m_pBackend->Init("DDNet Client", &g_Config.m_GfxScreen, &g_Config.m_GfxScreenWidth, &g_Config.m_GfxScreenHeight, &g_Config.m_GfxScreenRefreshRate, &g_Config.m_GfxFsaaSamples, Flags, &g_Config.m_GfxDesktopWidth, &g_Config.m_GfxDesktopHeight, &m_ScreenWidth, &m_ScreenHeight, m_pStorage);
	AddBackEndWarningIfExists();
	if(r == 0)
	{
		m_GLUseTrianglesAsQuad = m_pBackend->UseTrianglesAsQuad();
		m_GLTileBufferingEnabled = m_pBackend->HasTileBuffering();
		m_GLQuadBufferingEnabled = m_pBackend->HasQuadBuffering();
		m_GLQuadContainerBufferingEnabled = m_pBackend->HasQuadContainerBuffering();
		m_GLTextBufferingEnabled = (m_GLQuadContainerBufferingEnabled && m_pBackend->HasTextBuffering());
		m_GLHasTextureArrays = m_pBackend->Has2DTextureArrays();
		m_ScreenHiDPIScale = m_ScreenWidth / (float)g_Config.m_GfxScreenWidth;
		m_ScreenRefreshRate = g_Config.m_GfxScreenRefreshRate;
	}
	return r;
}

void CGraphics_Threaded::AdjustViewport(bool SendViewportChangeToBackend)
{
	// adjust the viewport to only allow certain aspect ratios
	// keep this in sync with backend_vulkan GetSwapImageSize's check
	if(m_ScreenHeight > 4 * m_ScreenWidth / 5)
	{
		m_IsForcedViewport = true;
		m_ScreenHeight = 4 * m_ScreenWidth / 5;

		if(SendViewportChangeToBackend)
		{
			UpdateViewport(0, 0, m_ScreenWidth, m_ScreenHeight, true);
		}
	}
	else
	{
		m_IsForcedViewport = false;
	}
}

void CGraphics_Threaded::UpdateViewport(int X, int Y, int W, int H, bool ByResize)
{
	CCommandBuffer::SCommand_Update_Viewport Cmd;
	Cmd.m_X = X;
	Cmd.m_Y = Y;
	Cmd.m_Width = W;
	Cmd.m_Height = H;
	Cmd.m_ByResize = ByResize;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add resize command"))
	{
		return;
	}
}

void CGraphics_Threaded::AddBackEndWarningIfExists()
{
	const char *pErrStr = m_pBackend->GetErrorString();
	if(pErrStr != NULL)
	{
		SWarning NewWarning;
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), "%s", Localize(pErrStr));
		m_vWarnings.emplace_back(NewWarning);
	}
}

int CGraphics_Threaded::InitWindow()
{
	int ErrorCode = IssueInit();
	if(ErrorCode == 0)
		return 0;

	// try disabling fsaa
	while(g_Config.m_GfxFsaaSamples)
	{
		// 4 is the minimum required by OpenGL ES spec (GL_MAX_SAMPLES - https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml), so can probably also be assumed for OpenGL
		if(g_Config.m_GfxFsaaSamples > 4)
			g_Config.m_GfxFsaaSamples = 4;
		else
			g_Config.m_GfxFsaaSamples = 0;

		if(g_Config.m_GfxFsaaSamples)
			dbg_msg("gfx", "lowering FSAA to %d and trying again", g_Config.m_GfxFsaaSamples);
		else
			dbg_msg("gfx", "disabling FSAA and trying again");

		ErrorCode = IssueInit();
		if(ErrorCode == 0)
			return 0;
	}

	size_t GLInitTryCount = 0;
	while(ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_GL_CONTEXT_FAILED || ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_GL_VERSION_FAILED)
	{
		if(ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_GL_CONTEXT_FAILED)
		{
			// try next smaller major/minor or patch version
			if(g_Config.m_GfxGLMajor >= 4)
			{
				g_Config.m_GfxGLMajor = 3;
				g_Config.m_GfxGLMinor = 3;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 3 && g_Config.m_GfxGLMinor >= 1)
			{
				g_Config.m_GfxGLMajor = 3;
				g_Config.m_GfxGLMinor = 0;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 3 && g_Config.m_GfxGLMinor == 0)
			{
				g_Config.m_GfxGLMajor = 2;
				g_Config.m_GfxGLMinor = 1;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 2 && g_Config.m_GfxGLMinor >= 1)
			{
				g_Config.m_GfxGLMajor = 2;
				g_Config.m_GfxGLMinor = 0;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 2 && g_Config.m_GfxGLMinor == 0)
			{
				g_Config.m_GfxGLMajor = 1;
				g_Config.m_GfxGLMinor = 5;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 1 && g_Config.m_GfxGLMinor == 5)
			{
				g_Config.m_GfxGLMajor = 1;
				g_Config.m_GfxGLMinor = 4;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 1 && g_Config.m_GfxGLMinor == 4)
			{
				g_Config.m_GfxGLMajor = 1;
				g_Config.m_GfxGLMinor = 3;
				g_Config.m_GfxGLPatch = 0;
			}
			else if(g_Config.m_GfxGLMajor == 1 && g_Config.m_GfxGLMinor == 3)
			{
				g_Config.m_GfxGLMajor = 1;
				g_Config.m_GfxGLMinor = 2;
				g_Config.m_GfxGLPatch = 1;
			}
			else if(g_Config.m_GfxGLMajor == 1 && g_Config.m_GfxGLMinor == 2)
			{
				g_Config.m_GfxGLMajor = 1;
				g_Config.m_GfxGLMinor = 1;
				g_Config.m_GfxGLPatch = 0;
			}
		}

		// new gl version was set by backend, try again
		ErrorCode = IssueInit();
		if(ErrorCode == 0)
		{
			return 0;
		}

		if(++GLInitTryCount >= 9)
		{
			// try something else
			break;
		}
	}

	// try lowering the resolution
	if(g_Config.m_GfxScreenWidth != 640 || g_Config.m_GfxScreenHeight != 480)
	{
		dbg_msg("gfx", "setting resolution to 640x480 and trying again");
		g_Config.m_GfxScreenWidth = 640;
		g_Config.m_GfxScreenHeight = 480;

		if(IssueInit() == 0)
			return 0;
	}

	// at the very end, just try to set to gl 1.4
	{
		g_Config.m_GfxGLMajor = 1;
		g_Config.m_GfxGLMinor = 4;
		g_Config.m_GfxGLPatch = 0;

		if(IssueInit() == 0)
			return 0;
	}

	dbg_msg("gfx", "out of ideas. failed to init graphics");

	return -1;
}

int CGraphics_Threaded::Init()
{
	// fetch pointers
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	// init textures
	m_FirstFreeTexture = 0;
	m_vTextureIndices.resize(CCommandBuffer::MAX_TEXTURES);
	for(int i = 0; i < (int)m_vTextureIndices.size() - 1; i++)
		m_vTextureIndices[i] = i + 1;
	m_vTextureIndices.back() = -1;

	m_FirstFreeVertexArrayInfo = -1;
	m_FirstFreeBufferObjectIndex = -1;
	m_FirstFreeQuadContainer = -1;

	m_pBackend = CreateGraphicsBackend();
	if(InitWindow() != 0)
		return -1;

	for(auto &FakeMode : g_aFakeModes)
	{
		FakeMode.m_WindowWidth = FakeMode.m_CanvasWidth / m_ScreenHiDPIScale;
		FakeMode.m_WindowHeight = FakeMode.m_CanvasHeight / m_ScreenHiDPIScale;
		FakeMode.m_RefreshRate = g_Config.m_GfxScreenRefreshRate;
	}

	// create command buffers
	for(auto &pCommandBuffer : m_apCommandBuffers)
		pCommandBuffer = new CCommandBuffer(CMD_BUFFER_CMD_BUFFER_SIZE, CMD_BUFFER_DATA_BUFFER_SIZE);
	m_pCommandBuffer = m_apCommandBuffers[0];

	// create null texture, will get id=0
	static const unsigned char s_aNullTextureData[] = {
		0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff,
		0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff};

	m_InvalidTexture = LoadTextureRaw(4, 4, CImageInfo::FORMAT_RGBA, s_aNullTextureData, CImageInfo::FORMAT_RGBA, 0);

	ColorRGBA GPUInfoPrintColor{0.6f, 0.5f, 1.0f, 1.0f};

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "GPU vendor: %s", GetVendorString());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gfx", aBuf, GPUInfoPrintColor);

	str_format(aBuf, sizeof(aBuf), "GPU renderer: %s", GetRendererString());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gfx", aBuf, GPUInfoPrintColor);

	str_format(aBuf, sizeof(aBuf), "GPU version: %s", GetVersionString());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gfx", aBuf, GPUInfoPrintColor);

	AdjustViewport(true);

	return 0;
}

void CGraphics_Threaded::Shutdown()
{
	// shutdown the backend
	m_pBackend->Shutdown();
	delete m_pBackend;
	m_pBackend = 0x0;

	// delete the command buffers
	for(auto &pCommandBuffer : m_apCommandBuffers)
		delete pCommandBuffer;
}

int CGraphics_Threaded::GetNumScreens() const
{
	return m_pBackend->GetNumScreens();
}

void CGraphics_Threaded::Minimize()
{
	m_pBackend->Minimize();
}

void CGraphics_Threaded::Maximize()
{
	// TODO: SDL
	m_pBackend->Maximize();
}

void CGraphics_Threaded::WarnPngliteIncompatibleImages(bool Warn)
{
	m_WarnPngliteIncompatibleImages = Warn;
}

void CGraphics_Threaded::SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing)
{
	m_pBackend->SetWindowParams(FullscreenMode, IsBorderless, AllowResizing);
	CVideoMode CurMode;
	m_pBackend->GetCurrentVideoMode(CurMode, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, g_Config.m_GfxScreen);
	GotResized(CurMode.m_WindowWidth, CurMode.m_WindowHeight, CurMode.m_RefreshRate);
}

bool CGraphics_Threaded::SetWindowScreen(int Index)
{
	if(!m_pBackend->SetWindowScreen(Index))
	{
		return false;
	}

	m_pBackend->GetViewportSize(m_ScreenWidth, m_ScreenHeight);
	AdjustViewport(true);
	m_ScreenHiDPIScale = m_ScreenWidth / (float)g_Config.m_GfxScreenWidth;
	return true;
}

void CGraphics_Threaded::Move(int x, int y)
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && IVideo::Current()->IsRecording())
		return;
#endif

	// Only handling CurScreen != m_GfxScreen doesn't work reliably
	const int CurScreen = m_pBackend->GetWindowScreen();
	m_pBackend->UpdateDisplayMode(CurScreen);
	m_pBackend->GetViewportSize(m_ScreenWidth, m_ScreenHeight);
	AdjustViewport(true);
	m_ScreenHiDPIScale = m_ScreenWidth / (float)g_Config.m_GfxScreenWidth;
}

void CGraphics_Threaded::Resize(int w, int h, int RefreshRate)
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && IVideo::Current()->IsRecording())
		return;
#endif

	if(WindowWidth() == w && WindowHeight() == h && RefreshRate == m_ScreenRefreshRate)
		return;

	// if the size is changed manually, only set the window resize, a window size changed event is triggered anyway
	if(m_pBackend->ResizeWindow(w, h, RefreshRate))
	{
		CVideoMode CurMode;
		m_pBackend->GetCurrentVideoMode(CurMode, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, g_Config.m_GfxScreen);
		GotResized(w, h, RefreshRate);
	}
}

void CGraphics_Threaded::GotResized(int w, int h, int RefreshRate)
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && IVideo::Current()->IsRecording())
		return;
#endif

	// if RefreshRate is -1 use the current config refresh rate
	if(RefreshRate == -1)
		RefreshRate = g_Config.m_GfxScreenRefreshRate;

	// if the size change event is triggered, set all parameters and change the viewport
	m_pBackend->GetViewportSize(m_ScreenWidth, m_ScreenHeight);

	AdjustViewport(false);

	m_ScreenRefreshRate = RefreshRate;

	g_Config.m_GfxScreenWidth = w;
	g_Config.m_GfxScreenHeight = h;
	g_Config.m_GfxScreenRefreshRate = m_ScreenRefreshRate;
	m_ScreenHiDPIScale = m_ScreenWidth / (float)g_Config.m_GfxScreenWidth;

	UpdateViewport(0, 0, m_ScreenWidth, m_ScreenHeight, true);

	// kick the command buffer and wait
	KickCommandBuffer();
	WaitForIdle();

	for(auto &ResizeListener : m_vResizeListeners)
		ResizeListener.m_pFunc(ResizeListener.m_pUser);
}

void CGraphics_Threaded::AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser)
{
	m_vResizeListeners.emplace_back(pFunc, pUser);
}

int CGraphics_Threaded::GetWindowScreen()
{
	return m_pBackend->GetWindowScreen();
}

void CGraphics_Threaded::WindowDestroyNtf(uint32_t WindowID)
{
	m_pBackend->WindowDestroyNtf(WindowID);

	CCommandBuffer::SCommand_WindowDestroyNtf Cmd;
	Cmd.m_WindowID = WindowID;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add window destroy notify command"))
	{
		return;
	}

	// wait
	KickCommandBuffer();
	WaitForIdle();
}

void CGraphics_Threaded::WindowCreateNtf(uint32_t WindowID)
{
	m_pBackend->WindowCreateNtf(WindowID);

	CCommandBuffer::SCommand_WindowCreateNtf Cmd;
	Cmd.m_WindowID = WindowID;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add window create notify command"))
	{
		return;
	}

	// wait
	KickCommandBuffer();
	WaitForIdle();
}

int CGraphics_Threaded::WindowActive()
{
	return m_pBackend->WindowActive();
}

int CGraphics_Threaded::WindowOpen()
{
	return m_pBackend->WindowOpen();
}

void CGraphics_Threaded::SetWindowGrab(bool Grab)
{
	return m_pBackend->SetWindowGrab(Grab);
}

void CGraphics_Threaded::NotifyWindow()
{
	return m_pBackend->NotifyWindow();
}

void CGraphics_Threaded::TakeScreenshot(const char *pFilename)
{
	// TODO: screenshot support
	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	str_format(m_aScreenshotName, sizeof(m_aScreenshotName), "screenshots/%s_%s.png", pFilename ? pFilename : "screenshot", aDate);
	m_DoScreenshot = true;
}

void CGraphics_Threaded::TakeCustomScreenshot(const char *pFilename)
{
	str_copy(m_aScreenshotName, pFilename);
	m_DoScreenshot = true;
}

void CGraphics_Threaded::Swap()
{
	if(!m_vWarnings.empty())
	{
		SWarning *pCurWarning = GetCurWarning();
		if(pCurWarning->m_WasShown)
		{
			m_vWarnings.erase(m_vWarnings.begin());
		}
	}

	bool TookScreenshotAndSwapped = false;

	if(m_DoScreenshot)
	{
		if(WindowActive())
			TookScreenshotAndSwapped = ScreenshotDirect();
		m_DoScreenshot = false;
	}

	if(!TookScreenshotAndSwapped)
	{
		// add swap command
		CCommandBuffer::SCommand_Swap Cmd;
		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to add swap command"))
		{
			return;
		}
	}

	if(g_Config.m_GfxFinish)
	{
		CCommandBuffer::SCommand_Finish Cmd;
		if(!AddCmd(
			   Cmd, [] { return true; }, "failed to add finish command"))
		{
			return;
		}
	}

	// kick the command buffer
	KickCommandBuffer();
	// TODO: Remove when https://github.com/libsdl-org/SDL/issues/5203 is fixed
#ifdef CONF_PLATFORM_MACOS
	if(str_find(GetVersionString(), "Metal"))
		WaitForIdle();
#endif
}

bool CGraphics_Threaded::SetVSync(bool State)
{
	if(!m_pCommandBuffer)
		return true;

	// add vsync command
	bool RetOk = false;
	CCommandBuffer::SCommand_VSync Cmd;
	Cmd.m_VSync = State ? 1 : 0;
	Cmd.m_pRetOk = &RetOk;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add vsync command"))
	{
		return false;
	}

	// kick the command buffer
	KickCommandBuffer();
	WaitForIdle();
	return RetOk;
}

bool CGraphics_Threaded::SetMultiSampling(uint32_t ReqMultiSamplingCount, uint32_t &MultiSamplingCountBackend)
{
	if(!m_pCommandBuffer)
		return true;

	// add multisampling command
	bool RetOk = false;
	CCommandBuffer::SCommand_MultiSampling Cmd;
	Cmd.m_RequestedMultiSamplingCount = ReqMultiSamplingCount;
	Cmd.m_pRetMultiSamplingCount = &MultiSamplingCountBackend;
	Cmd.m_pRetOk = &RetOk;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add multi sampling command"))
	{
		return false;
	}

	// kick the command buffer
	KickCommandBuffer();
	WaitForIdle();
	return RetOk;
}

// synchronization
void CGraphics_Threaded::InsertSignal(CSemaphore *pSemaphore)
{
	CCommandBuffer::SCommand_Signal Cmd;
	Cmd.m_pSemaphore = pSemaphore;

	if(!AddCmd(
		   Cmd, [] { return true; }, "failed to add signal command"))
	{
		return;
	}
}

bool CGraphics_Threaded::IsIdle() const
{
	return m_pBackend->IsIdle();
}

void CGraphics_Threaded::WaitForIdle()
{
	m_pBackend->WaitForIdle();
}

SWarning *CGraphics_Threaded::GetCurWarning()
{
	if(m_vWarnings.empty())
		return NULL;
	else
	{
		SWarning *pCurWarning = m_vWarnings.data();
		return pCurWarning;
	}
}

const char *CGraphics_Threaded::GetVendorString()
{
	return m_pBackend->GetVendorString();
}

const char *CGraphics_Threaded::GetVersionString()
{
	return m_pBackend->GetVersionString();
}

const char *CGraphics_Threaded::GetRendererString()
{
	return m_pBackend->GetRendererString();
}

TGLBackendReadPresentedImageData &CGraphics_Threaded::GetReadPresentedImageDataFuncUnsafe()
{
	return m_pBackend->GetReadPresentedImageDataFuncUnsafe();
}

int CGraphics_Threaded::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	if(g_Config.m_GfxDisplayAllVideoModes)
	{
		int Count = std::size(g_aFakeModes);
		mem_copy(pModes, g_aFakeModes, sizeof(g_aFakeModes));
		if(MaxModes < Count)
			Count = MaxModes;
		return Count;
	}

	// add videomodes command
	CImageInfo Image;
	mem_zero(&Image, sizeof(Image));

	int NumModes = 0;
	m_pBackend->GetVideoModes(pModes, MaxModes, &NumModes, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, Screen);

	return NumModes;
}

extern IEngineGraphics *CreateEngineGraphicsThreaded()
{
	return new CGraphics_Threaded();
}
