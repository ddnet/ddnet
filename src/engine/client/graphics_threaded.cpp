/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/log.h>
#include <base/math.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <base/system.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
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
	int PrimType;
	size_t PrimCount, NumVerts;
	FlushVerticesImpl(KeepVertices, PrimType, PrimCount, NumVerts, Cmd, sizeof(CCommandBuffer::SVertex));

	if(Cmd.m_pVertices != nullptr)
	{
		mem_copy(Cmd.m_pVertices, m_aVertices, sizeof(CCommandBuffer::SVertex) * NumVerts);
	}
}

void CGraphics_Threaded::FlushVerticesTex3D()
{
	CCommandBuffer::SCommand_RenderTex3D Cmd;
	int PrimType;
	size_t PrimCount, NumVerts;
	FlushVerticesImpl(false, PrimType, PrimCount, NumVerts, Cmd, sizeof(CCommandBuffer::SVertexTex3DStream));

	if(Cmd.m_pVertices != nullptr)
	{
		mem_copy(Cmd.m_pVertices, m_aVerticesTex3D, sizeof(CCommandBuffer::SVertexTex3DStream) * NumVerts);
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
	m_pCommandBuffer = nullptr;
	m_apCommandBuffers[0] = nullptr;
	m_apCommandBuffers[1] = nullptr;

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

const TTwGraphicsGpuList &CGraphics_Threaded::GetGpus() const
{
	return m_pBackend->GetGpus();
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

IGraphics::CTextureHandle CGraphics_Threaded::FindFreeTextureIndex()
{
	const size_t CurSize = m_vTextureIndices.size();
	if(m_FirstFreeTexture == CurSize)
	{
		m_vTextureIndices.resize(CurSize * 2);
		for(size_t i = 0; i < CurSize; ++i)
			m_vTextureIndices[CurSize + i] = CurSize + i + 1;
	}
	const size_t Tex = m_FirstFreeTexture;
	m_FirstFreeTexture = m_vTextureIndices[Tex];
	m_vTextureIndices[Tex] = -1;
	return CreateTextureHandle(Tex);
}

void CGraphics_Threaded::FreeTextureIndex(CTextureHandle *pIndex)
{
	dbg_assert(pIndex->IsValid(), "Cannot free invalid texture index");
	dbg_assert(m_vTextureIndices[pIndex->Id()] == -1, "Cannot free already freed texture index");

	m_vTextureIndices[pIndex->Id()] = m_FirstFreeTexture;
	m_FirstFreeTexture = pIndex->Id();
	pIndex->Invalidate();
}

void CGraphics_Threaded::UnloadTexture(CTextureHandle *pIndex)
{
	if(pIndex->IsNullTexture() || !pIndex->IsValid())
		return;

	CCommandBuffer::SCommand_Texture_Destroy Cmd;
	Cmd.m_Slot = pIndex->Id();
	AddCmd(Cmd);

	FreeTextureIndex(pIndex);
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadSpriteTexture(const CImageInfo &FromImageInfo, const CDataSprite *pSprite)
{
	int ImageGridX = FromImageInfo.m_Width / pSprite->m_pSet->m_Gridx;
	int ImageGridY = FromImageInfo.m_Height / pSprite->m_pSet->m_Gridy;
	int x = pSprite->m_X * ImageGridX;
	int y = pSprite->m_Y * ImageGridY;
	int w = pSprite->m_W * ImageGridX;
	int h = pSprite->m_H * ImageGridY;

	CImageInfo SpriteInfo;
	SpriteInfo.m_Width = w;
	SpriteInfo.m_Height = h;
	SpriteInfo.m_Format = FromImageInfo.m_Format;
	SpriteInfo.m_pData = static_cast<uint8_t *>(malloc(SpriteInfo.DataSize()));
	SpriteInfo.CopyRectFrom(FromImageInfo, x, y, w, h, 0, 0);
	return LoadTextureRawMove(SpriteInfo, 0, pSprite->m_pName);
}

bool CGraphics_Threaded::IsImageSubFullyTransparent(const CImageInfo &FromImageInfo, int x, int y, int w, int h)
{
	if(FromImageInfo.m_Format == CImageInfo::FORMAT_R || FromImageInfo.m_Format == CImageInfo::FORMAT_RA || FromImageInfo.m_Format == CImageInfo::FORMAT_RGBA)
	{
		const uint8_t *pImgData = FromImageInfo.m_pData;
		const size_t PixelSize = FromImageInfo.PixelSize();
		for(int iy = 0; iy < h; ++iy)
		{
			for(int ix = 0; ix < w; ++ix)
			{
				const size_t RealOffset = (x + ix) * PixelSize + (y + iy) * PixelSize * FromImageInfo.m_Width;
				if(pImgData[RealOffset + (PixelSize - 1)] > 0)
					return false;
			}
		}

		return true;
	}
	return false;
}

bool CGraphics_Threaded::IsSpriteTextureFullyTransparent(const CImageInfo &FromImageInfo, const CDataSprite *pSprite)
{
	int ImageGridX = FromImageInfo.m_Width / pSprite->m_pSet->m_Gridx;
	int ImageGridY = FromImageInfo.m_Height / pSprite->m_pSet->m_Gridy;
	int x = pSprite->m_X * ImageGridX;
	int y = pSprite->m_Y * ImageGridY;
	int w = pSprite->m_W * ImageGridX;
	int h = pSprite->m_H * ImageGridY;
	return IsImageSubFullyTransparent(FromImageInfo, x, y, w, h);
}

void CGraphics_Threaded::LoadTextureAddWarning(size_t Width, size_t Height, int Flags, const char *pTexName)
{
	if((Flags & IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE) != 0 || (Flags & IGraphics::TEXLOAD_TO_3D_TEXTURE) != 0)
	{
		if(Width == 0 || (Width % 16) != 0 || Height == 0 || (Height % 16) != 0)
		{
			SWarning NewWarning;
			char aText[128];
			str_format(aText, sizeof(aText), "\"%s\"", pTexName ? pTexName : "(no name)");
			str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), Localize("The width of texture %s is not divisible by %d, or the height is not divisible by %d, which might cause visual bugs."), aText, 16, 16);
			AddWarning(NewWarning);
		}
	}
}

static CCommandBuffer::SCommand_Texture_Create LoadTextureCreateCommand(int TextureId, size_t Width, size_t Height, int Flags)
{
	CCommandBuffer::SCommand_Texture_Create Cmd;
	Cmd.m_Slot = TextureId;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;

	Cmd.m_Flags = 0;
	if((Flags & IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_2D_ARRAY_TEXTURE;
	if((Flags & IGraphics::TEXLOAD_TO_3D_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_TO_3D_TEXTURE;
	if((Flags & IGraphics::TEXLOAD_NO_2D_TEXTURE) != 0)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_NO_2D_TEXTURE;

	return Cmd;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadTextureRaw(const CImageInfo &Image, int Flags, const char *pTexName)
{
	LoadTextureAddWarning(Image.m_Width, Image.m_Height, Flags, pTexName);

	if(Image.m_Width == 0 || Image.m_Height == 0)
		return IGraphics::CTextureHandle();

	IGraphics::CTextureHandle TextureHandle = FindFreeTextureIndex();
	CCommandBuffer::SCommand_Texture_Create Cmd = LoadTextureCreateCommand(TextureHandle.Id(), Image.m_Width, Image.m_Height, Flags);

	// Copy texture data and convert if necessary
	uint8_t *pTmpData;
	if(!ConvertToRgbaAlloc(pTmpData, Image))
	{
		dbg_msg("graphics", "converted image '%s' to RGBA, consider making its file format RGBA", pTexName ? pTexName : "(no name)");
	}
	Cmd.m_pData = pTmpData;

	AddCmd(Cmd);

	return TextureHandle;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadTextureRawMove(CImageInfo &Image, int Flags, const char *pTexName)
{
	if(Image.m_Format != CImageInfo::FORMAT_RGBA)
	{
		// Moving not possible, texture needs to be converted
		IGraphics::CTextureHandle TextureHandle = LoadTextureRaw(Image, Flags, pTexName);
		Image.Free();
		return TextureHandle;
	}

	LoadTextureAddWarning(Image.m_Width, Image.m_Height, Flags, pTexName);

	if(Image.m_Width == 0 || Image.m_Height == 0)
		return IGraphics::CTextureHandle();

	IGraphics::CTextureHandle TextureHandle = FindFreeTextureIndex();
	CCommandBuffer::SCommand_Texture_Create Cmd = LoadTextureCreateCommand(TextureHandle.Id(), Image.m_Width, Image.m_Height, Flags);
	Cmd.m_pData = Image.m_pData;
	Image.m_pData = nullptr;
	Image.Free();
	AddCmd(Cmd);

	return TextureHandle;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadTexture(const char *pFilename, int StorageType, int Flags)
{
	dbg_assert(pFilename[0] != '\0', "Cannot load texture from file with empty filename"); // would cause Valgrind to crash otherwise

	CImageInfo Image;
	if(LoadPng(Image, pFilename, StorageType))
	{
		CTextureHandle Id = LoadTextureRawMove(Image, Flags, pFilename);
		if(Id.IsValid())
		{
			if(g_Config.m_Debug)
				dbg_msg("graphics/texture", "loaded %s", pFilename);
			return Id;
		}
	}

	return m_NullTexture;
}

bool CGraphics_Threaded::LoadTextTextures(size_t Width, size_t Height, CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture, uint8_t *pTextData, uint8_t *pTextOutlineData)
{
	if(Width == 0 || Height == 0)
		return false;

	TextTexture = FindFreeTextureIndex();
	TextOutlineTexture = FindFreeTextureIndex();

	CCommandBuffer::SCommand_TextTextures_Create Cmd;
	Cmd.m_Slot = TextTexture.Id();
	Cmd.m_SlotOutline = TextOutlineTexture.Id();
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_pTextData = pTextData;
	Cmd.m_pTextOutlineData = pTextOutlineData;
	AddCmd(Cmd);

	return true;
}

bool CGraphics_Threaded::UnloadTextTextures(CTextureHandle &TextTexture, CTextureHandle &TextOutlineTexture)
{
	CCommandBuffer::SCommand_TextTextures_Destroy Cmd;
	Cmd.m_Slot = TextTexture.Id();
	Cmd.m_SlotOutline = TextOutlineTexture.Id();
	AddCmd(Cmd);

	if(TextTexture.IsValid())
		FreeTextureIndex(&TextTexture);
	if(TextOutlineTexture.IsValid())
		FreeTextureIndex(&TextOutlineTexture);
	return true;
}

bool CGraphics_Threaded::UpdateTextTexture(CTextureHandle TextureId, int x, int y, size_t Width, size_t Height, uint8_t *pData, bool IsMovedPointer)
{
	CCommandBuffer::SCommand_TextTexture_Update Cmd;
	Cmd.m_Slot = TextureId.Id();
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;

	if(IsMovedPointer)
	{
		Cmd.m_pData = pData;
	}
	else
	{
		const size_t MemSize = Width * Height;
		uint8_t *pTmpData = static_cast<uint8_t *>(malloc(MemSize));
		mem_copy(pTmpData, pData, MemSize);
		Cmd.m_pData = pTmpData;
	}
	AddCmd(Cmd);

	return true;
}

static SWarning FormatPngliteIncompatibilityWarning(int PngliteIncompatible, const char *pContextName)
{
	SWarning Warning;
	str_format(Warning.m_aWarningMsg, sizeof(Warning.m_aWarningMsg), Localize("\"%s\" is not compatible with pnglite and cannot be loaded by old DDNet versions:"), pContextName);
	str_append(Warning.m_aWarningMsg, " ");
	static const int FLAGS[] = {CImageLoader::PNGLITE_COLOR_TYPE, CImageLoader::PNGLITE_BIT_DEPTH, CImageLoader::PNGLITE_INTERLACE_TYPE, CImageLoader::PNGLITE_COMPRESSION_TYPE, CImageLoader::PNGLITE_FILTER_TYPE};
	static const char *const EXPLANATION[] = {"color type", "bit depth", "interlace type", "compression type", "filter type"};

	bool First = true;
	for(size_t i = 0; i < std::size(FLAGS); ++i)
	{
		if((PngliteIncompatible & FLAGS[i]) != 0)
		{
			if(!First)
			{
				str_append(Warning.m_aWarningMsg, ", ");
			}
			str_append(Warning.m_aWarningMsg, EXPLANATION[i]);
			First = false;
		}
	}
	str_append(Warning.m_aWarningMsg, " unsupported");
	return Warning;
}

bool CGraphics_Threaded::LoadPng(CImageInfo &Image, const char *pFilename, int StorageType)
{
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);

	int PngliteIncompatible;
	if(!CImageLoader::LoadPng(File, pFilename, Image, PngliteIncompatible))
		return false;

	if(m_WarnPngliteIncompatibleImages && PngliteIncompatible != 0)
	{
		AddWarning(FormatPngliteIncompatibilityWarning(PngliteIncompatible, pFilename));
	}

	return true;
}

bool CGraphics_Threaded::LoadPng(CImageInfo &Image, const uint8_t *pData, size_t DataSize, const char *pContextName)
{
	CByteBufferReader Reader(pData, DataSize);
	int PngliteIncompatible;
	if(!CImageLoader::LoadPng(Reader, pContextName, Image, PngliteIncompatible))
		return false;

	if(m_WarnPngliteIncompatibleImages && PngliteIncompatible != 0)
	{
		AddWarning(FormatPngliteIncompatibilityWarning(PngliteIncompatible, pContextName));
	}

	return true;
}

bool CGraphics_Threaded::CheckImageDivisibility(const char *pContextName, CImageInfo &Image, int DivX, int DivY, bool AllowResize)
{
	dbg_assert(DivX != 0 && DivY != 0, "Passing 0 to this function is not allowed.");
	bool ImageIsValid = true;
	bool WidthBroken = Image.m_Width == 0 || (Image.m_Width % DivX) != 0;
	bool HeightBroken = Image.m_Height == 0 || (Image.m_Height % DivY) != 0;
	if(WidthBroken || HeightBroken)
	{
		SWarning NewWarning;
		char aContextNameQuoted[128];
		str_format(aContextNameQuoted, sizeof(aContextNameQuoted), "\"%s\"", pContextName);
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg),
			Localize("The width of texture %s is not divisible by %d, or the height is not divisible by %d, which might cause visual bugs."), aContextNameQuoted, DivX, DivY);
		AddWarning(NewWarning);
		ImageIsValid = false;
	}

	if(AllowResize && !ImageIsValid && Image.m_Width > 0 && Image.m_Height > 0)
	{
		int NewWidth = DivX;
		int NewHeight = DivY;
		if(WidthBroken)
		{
			NewWidth = maximum<int>(HighestBit(Image.m_Width), DivX);
			NewHeight = (NewWidth / DivX) * DivY;
		}
		else
		{
			NewHeight = maximum<int>(HighestBit(Image.m_Height), DivY);
			NewWidth = (NewHeight / DivY) * DivX;
		}
		ResizeImage(Image, NewWidth, NewHeight);
		ImageIsValid = true;
	}

	return ImageIsValid;
}

bool CGraphics_Threaded::IsImageFormatRgba(const char *pContextName, const CImageInfo &Image)
{
	if(Image.m_Format != CImageInfo::FORMAT_RGBA)
	{
		SWarning NewWarning;
		char aContextNameQuoted[128];
		str_format(aContextNameQuoted, sizeof(aContextNameQuoted), "\"%s\"", pContextName);
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg),
			Localize("The format of texture %s is not RGBA which will cause visual bugs."), aContextNameQuoted);
		AddWarning(NewWarning);
		return false;
	}
	return true;
}

void CGraphics_Threaded::KickCommandBuffer()
{
	m_pBackend->RunBuffer(m_pCommandBuffer);

	std::vector<std::string> WarningStrings;
	if(m_pBackend->GetWarning(WarningStrings))
	{
		SWarning NewWarning;
		std::string WarningStr;
		for(const auto &WarnStr : WarningStrings)
			WarningStr.append((WarnStr + "\n"));
		str_copy(NewWarning.m_aWarningMsg, WarningStr.c_str());
		AddWarning(NewWarning);
	}

	// swap buffer
	m_CurrentCommandBuffer ^= 1;
	m_pCommandBuffer = m_apCommandBuffers[m_CurrentCommandBuffer];
	m_pCommandBuffer->Reset();
}

class CScreenshotSaveJob : public IJob
{
	IStorage *m_pStorage;
	IConsole *m_pConsole;
	char m_aName[IO_MAX_PATH_LENGTH];
	CImageInfo m_Image;

	void Run() override
	{
		char aWholePath[IO_MAX_PATH_LENGTH];
		char aBuf[64 + IO_MAX_PATH_LENGTH];
		if(CImageLoader::SavePng(m_pStorage->OpenFile(m_aName, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath)), m_aName, m_Image))
		{
			str_format(aBuf, sizeof(aBuf), "saved screenshot to '%s'", aWholePath);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "failed to save screenshot to '%s'", aWholePath);
		}
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, ColorRGBA(1.0f, 0.6f, 0.3f, 1.0f));
	}

public:
	CScreenshotSaveJob(IStorage *pStorage, IConsole *pConsole, const char *pName, CImageInfo Image) :
		m_pStorage(pStorage),
		m_pConsole(pConsole),
		m_Image(Image)
	{
		str_copy(m_aName, pName);
	}

	~CScreenshotSaveJob() override
	{
		m_Image.Free();
	}
};

void CGraphics_Threaded::ScreenshotDirect(bool *pSwapped)
{
	if(!m_DoScreenshot)
		return;
	m_DoScreenshot = false;
	if(!WindowActive())
		return;

	CImageInfo Image;

	CCommandBuffer::SCommand_TrySwapAndScreenshot Cmd;
	Cmd.m_pImage = &Image;
	Cmd.m_pSwapped = pSwapped;
	AddCmd(Cmd);

	KickCommandBuffer();
	WaitForIdle();

	if(Image.m_pData)
	{
		m_pEngine->AddJob(std::make_shared<CScreenshotSaveJob>(m_pStorage, m_pConsole, m_aScreenshotName, Image));
	}
}

void CGraphics_Threaded::TextureSet(CTextureHandle TextureId)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");
	dbg_assert(!TextureId.IsValid() || m_vTextureIndices[TextureId.Id()] == -1, "Texture handle was not invalid, but also did not correlate to an existing texture.");
	m_State.m_Texture = TextureId.Id();
}

void CGraphics_Threaded::Clear(float r, float g, float b, bool ForceClearNow)
{
	CCommandBuffer::SCommand_Clear Cmd;
	Cmd.m_Color.r = r;
	Cmd.m_Color.g = g;
	Cmd.m_Color.b = b;
	Cmd.m_Color.a = 0;
	Cmd.m_ForceClear = ForceClearNow;
	AddCmd(Cmd);
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

static unsigned char NormalizeColorComponent(float ColorComponent)
{
	return (unsigned char)(clamp(ColorComponent, 0.0f, 1.0f) * 255.0f + 0.5f); // +0.5f to round to nearest
}

void CGraphics_Threaded::SetColorVertex(const CColorVertex *pArray, size_t Num)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColorVertex without begin");

	for(size_t i = 0; i < Num; ++i)
	{
		const CColorVertex &Vertex = pArray[i];
		CCommandBuffer::SColor &Color = m_aColor[Vertex.m_Index];
		Color.r = NormalizeColorComponent(Vertex.m_R);
		Color.g = NormalizeColorComponent(Vertex.m_G);
		Color.b = NormalizeColorComponent(Vertex.m_B);
		Color.a = NormalizeColorComponent(Vertex.m_A);
	}
}

void CGraphics_Threaded::SetColor(float r, float g, float b, float a)
{
	CCommandBuffer::SColor NewColor;
	NewColor.r = NormalizeColorComponent(r);
	NewColor.g = NormalizeColorComponent(g);
	NewColor.b = NormalizeColorComponent(b);
	NewColor.a = NormalizeColorComponent(a);
	for(CCommandBuffer::SColor &Color : m_aColor)
	{
		Color = NewColor;
	}
}

void CGraphics_Threaded::SetColor(ColorRGBA Color)
{
	SetColor(Color.r, Color.g, Color.b, Color.a);
}

void CGraphics_Threaded::SetColor4(ColorRGBA TopLeft, ColorRGBA TopRight, ColorRGBA BottomLeft, ColorRGBA BottomRight)
{
	CColorVertex aArray[] = {
		CColorVertex(0, TopLeft),
		CColorVertex(1, TopRight),
		CColorVertex(2, BottomRight),
		CColorVertex(3, BottomLeft)};
	SetColorVertex(aArray, std::size(aArray));
}

void CGraphics_Threaded::ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a)
{
	m_aColor[0].r = NormalizeColorComponent(r);
	m_aColor[0].g = NormalizeColorComponent(g);
	m_aColor[0].b = NormalizeColorComponent(b);
	m_aColor[0].a = NormalizeColorComponent(a);

	for(int i = 0; i < m_NumVertices; ++i)
	{
		SetColor(&m_aVertices[i], 0);
	}
}

void CGraphics_Threaded::ChangeColorOfQuadVertices(size_t QuadOffset, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	const CCommandBuffer::SColor Color(r, g, b, a);
	const size_t VertNum = g_Config.m_GfxQuadAsTriangle && !m_GLUseTrianglesAsQuad ? 6 : 4;
	for(size_t i = 0; i < VertNum; ++i)
	{
		m_aVertices[QuadOffset * VertNum + i].m_Color = Color;
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
	const int VertNum = g_Config.m_GfxQuadAsTriangle && !m_GLUseTrianglesAsQuad ? 6 : 4;
	const float CurIndex = Uses2DTextureArrays() ? m_CurIndex : (m_CurIndex + 0.5f) / 256.0f;

	for(int i = 0; i < Num; ++i)
	{
		for(int n = 0; n < VertNum; ++n)
		{
			m_aVerticesTex3D[m_NumVertices + VertNum * i + n].m_Tex.w = CurIndex;
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
		float Ca1 = std::cos(a1);
		float Ca2 = std::cos(a2);
		float Ca3 = std::cos(a3);
		float Sa1 = std::sin(a1);
		float Sa2 = std::sin(a2);
		float Sa3 = std::sin(a3);

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
		float Ca1 = std::cos(a1);
		float Ca2 = std::cos(a2);
		float Ca3 = std::cos(a3);
		float Sa1 = std::sin(a1);
		float Sa2 = std::sin(a2);
		float Sa3 = std::sin(a3);

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
		float Ca1 = std::cos(a1);
		float Ca2 = std::cos(a2);
		float Ca3 = std::cos(a3);
		float Sa1 = std::sin(a1);
		float Sa2 = std::sin(a2);
		float Sa3 = std::sin(a3);

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
			CenterX + std::cos(a1) * Radius, CenterY + std::sin(a1) * Radius,
			CenterX + std::cos(a3) * Radius, CenterY + std::sin(a3) * Radius,
			CenterX + std::cos(a2) * Radius, CenterY + std::sin(a2) * Radius);
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
	if(pData == nullptr)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		pData = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffset);
		if(pData == nullptr)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}
	Cmd.m_pIndicesOffsets = (char **)pData;
	Cmd.m_pDrawCount = (unsigned int *)(((char *)pData) + (sizeof(char *) * NumIndicesOffset));

	AddCmd(Cmd, [&] {
		pData = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffset);
		if(pData == nullptr)
			return false;
		Cmd.m_pIndicesOffsets = (char **)pData;
		Cmd.m_pDrawCount = (unsigned int *)(((char *)pData) + (sizeof(char *) * NumIndicesOffset));
		return true;
	});

	mem_copy(Cmd.m_pIndicesOffsets, pOffsets, sizeof(char *) * NumIndicesOffset);
	mem_copy(Cmd.m_pDrawCount, pIndicedVertexDrawNum, sizeof(unsigned int) * NumIndicesOffset);

	m_pCommandBuffer->AddRenderCalls(NumIndicesOffset);
	// todo max indices group check!!
}

void CGraphics_Threaded::RenderBorderTiles(int BufferContainerIndex, const ColorRGBA &Color, char *pIndexBufferOffset, const vec2 &Offset, const vec2 &Scale, uint32_t DrawNum)
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

	Cmd.m_Offset = Offset;
	Cmd.m_Scale = Scale;

	AddCmd(Cmd);

	m_pCommandBuffer->AddRenderCalls(1);
}

void CGraphics_Threaded::RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, size_t QuadNum, int QuadOffset)
{
	if(QuadNum == 0)
		return;

	// add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderQuadLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_QuadNum = QuadNum;
	Cmd.m_QuadOffset = QuadOffset;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	Cmd.m_pQuadInfo = (SQuadRenderInfo *)AllocCommandBufferData(Cmd.m_QuadNum * sizeof(SQuadRenderInfo));

	AddCmd(Cmd, [&] {
		Cmd.m_pQuadInfo = (SQuadRenderInfo *)m_pCommandBuffer->AllocData(QuadNum * sizeof(SQuadRenderInfo));
		return Cmd.m_pQuadInfo != nullptr;
	});

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

	AddCmd(Cmd);

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
				pAttr->m_pOffset = nullptr;
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
	if(ContainerIndex == -1)
		return;

	SQuadContainer &Container = m_vQuadContainers[ContainerIndex];
	if(IsQuadContainerBufferingEnabled())
		DeleteBufferContainer(Container.m_QuadBufferContainerIndex, true);
	Container.m_vQuads.clear();
	Container.m_QuadBufferObjectIndex = -1;
}

void CGraphics_Threaded::DeleteQuadContainer(int &ContainerIndex)
{
	if(ContainerIndex == -1)
		return;

	QuadContainerReset(ContainerIndex);

	// also clear the container index
	m_vQuadContainers[ContainerIndex].m_FreeIndex = m_FirstFreeQuadContainer;
	m_FirstFreeQuadContainer = ContainerIndex;
	ContainerIndex = -1;
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

		AddCmd(Cmd);
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

		AddCmd(Cmd);
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
		if(Cmd.m_pRenderInfo == nullptr)
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
			if(Cmd.m_pRenderInfo == nullptr)
			{
				dbg_msg("graphics", "failed to allocate data for render info");
				return;
			}
		}

		AddCmd(Cmd, [&] {
			Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
			return Cmd.m_pRenderInfo != nullptr;
		});

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

void *CGraphics_Threaded::AllocCommandBufferData(size_t AllocSize)
{
	void *pData = m_pCommandBuffer->AllocData(AllocSize);
	if(pData == nullptr)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		pData = m_pCommandBuffer->AllocData(AllocSize);
		if(pData == nullptr)
		{
			char aError[256];
			str_format(aError, sizeof(aError), "graphics: failed to allocate data (size %" PRIzu ") for command buffer", AllocSize);
			dbg_assert(false, aError);
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

	dbg_assert((CreateFlags & EBufferObjectCreateFlags::BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT) == 0 || (UploadDataSize <= CCommandBuffer::MAX_VERTICES * maximum(sizeof(CCommandBuffer::SVertexTex3DStream), sizeof(CCommandBuffer::SVertexTex3D))),
		"If BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT is used, then the buffer size must not exceed max(sizeof(CCommandBuffer::SVertexTex3DStream), sizeof(CCommandBuffer::SVertexTex3D)) * CCommandBuffer::MAX_VERTICES");

	CCommandBuffer::SCommand_CreateBufferObject Cmd;
	Cmd.m_BufferIndex = Index;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_DeletePointer = IsMovedPointer;
	Cmd.m_Flags = CreateFlags;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;
		AddCmd(Cmd);
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);

			AddCmd(Cmd, [&] {
				Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
				return Cmd.m_pUploadData != nullptr;
			});
			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = nullptr;
			AddCmd(Cmd);

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

	dbg_assert((CreateFlags & EBufferObjectCreateFlags::BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT) == 0 || (UploadDataSize <= CCommandBuffer::MAX_VERTICES * maximum(sizeof(CCommandBuffer::SVertexTex3DStream), sizeof(CCommandBuffer::SVertexTex3D))),
		"If BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT is used, then the buffer size must not exceed max(sizeof(CCommandBuffer::SVertexTex3DStream), sizeof(CCommandBuffer::SVertexTex3D)) * CCommandBuffer::MAX_VERTICES");

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;
		AddCmd(Cmd);
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);

			AddCmd(Cmd, [&] {
				Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
				return Cmd.m_pUploadData != nullptr;
			});

			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = nullptr;
			AddCmd(Cmd);

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
		AddCmd(Cmd);
	}
	else
	{
		Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);

		AddCmd(Cmd, [&] {
			Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
			return Cmd.m_pUploadData != nullptr;
		});

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
	AddCmd(Cmd);
}

void CGraphics_Threaded::DeleteBufferObject(int BufferIndex)
{
	if(BufferIndex == -1)
		return;

	CCommandBuffer::SCommand_DeleteBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	AddCmd(Cmd);

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
	Cmd.m_AttrCount = pContainerInfo->m_vAttributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;
	Cmd.m_VertBufferBindingIndex = pContainerInfo->m_VertBufferBindingIndex;
	Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	AddCmd(Cmd, [&] {
		Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
		return Cmd.m_pAttributes != nullptr;
	});

	mem_copy(Cmd.m_pAttributes, pContainerInfo->m_vAttributes.data(), Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	m_vVertexArrayInfo[Index].m_AssociatedBufferObjectIndex = pContainerInfo->m_VertBufferBindingIndex;

	return Index;
}

void CGraphics_Threaded::DeleteBufferContainer(int &ContainerIndex, bool DestroyAllBO)
{
	if(ContainerIndex == -1)
		return;

	CCommandBuffer::SCommand_DeleteBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_DestroyAllBO = DestroyAllBO;
	AddCmd(Cmd);

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
	ContainerIndex = -1;
}

void CGraphics_Threaded::UpdateBufferContainerInternal(int ContainerIndex, SBufferContainerInfo *pContainerInfo)
{
	CCommandBuffer::SCommand_UpdateBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_AttrCount = pContainerInfo->m_vAttributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;
	Cmd.m_VertBufferBindingIndex = pContainerInfo->m_VertBufferBindingIndex;
	Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	AddCmd(Cmd, [&] {
		Cmd.m_pAttributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
		return Cmd.m_pAttributes != nullptr;
	});

	mem_copy(Cmd.m_pAttributes, pContainerInfo->m_vAttributes.data(), Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	m_vVertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndex = pContainerInfo->m_VertBufferBindingIndex;
}

void CGraphics_Threaded::IndicesNumRequiredNotify(unsigned int RequiredIndicesCount)
{
	CCommandBuffer::SCommand_IndicesRequiredNumNotify Cmd;
	Cmd.m_RequiredIndicesNum = RequiredIndicesCount;
	AddCmd(Cmd);
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
		m_GLUses2DTextureArrays = m_pBackend->Uses2DTextureArrays();
		m_GLHasTextureArraysSupport = m_pBackend->HasTextureArraysSupport();
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
	AddCmd(Cmd);
}

void CGraphics_Threaded::AddBackEndWarningIfExists()
{
	const char *pErrStr = m_pBackend->GetErrorString();
	if(pErrStr != nullptr)
	{
		SWarning NewWarning;
		str_copy(NewWarning.m_aWarningMsg, Localize(pErrStr));
		AddWarning(NewWarning);
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
	m_pEngine = Kernel()->RequestInterface<IEngine>();

	// init textures
	m_FirstFreeTexture = 0;
	m_vTextureIndices.resize(CCommandBuffer::MAX_TEXTURES);
	for(size_t i = 0; i < m_vTextureIndices.size(); ++i)
		m_vTextureIndices[i] = i + 1;

	m_FirstFreeVertexArrayInfo = -1;
	m_FirstFreeBufferObjectIndex = -1;
	m_FirstFreeQuadContainer = -1;

	m_pBackend = CreateGraphicsBackend(Localize);
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
	{
		const size_t PixelSize = 4;
		const unsigned char aRed[] = {0xff, 0x00, 0x00, 0xff};
		const unsigned char aGreen[] = {0x00, 0xff, 0x00, 0xff};
		const unsigned char aBlue[] = {0x00, 0x00, 0xff, 0xff};
		const unsigned char aYellow[] = {0xff, 0xff, 0x00, 0xff};
		constexpr size_t NullTextureDimension = 16;
		unsigned char aNullTextureData[NullTextureDimension * NullTextureDimension * PixelSize];
		for(size_t y = 0; y < NullTextureDimension; ++y)
		{
			for(size_t x = 0; x < NullTextureDimension; ++x)
			{
				const unsigned char *pColor;
				if(x < NullTextureDimension / 2 && y < NullTextureDimension / 2)
					pColor = aRed;
				else if(x >= NullTextureDimension / 2 && y < NullTextureDimension / 2)
					pColor = aGreen;
				else if(x < NullTextureDimension / 2 && y >= NullTextureDimension / 2)
					pColor = aBlue;
				else
					pColor = aYellow;
				mem_copy(&aNullTextureData[(y * NullTextureDimension + x) * PixelSize], pColor, PixelSize);
			}
		}
		CImageInfo NullTextureInfo;
		NullTextureInfo.m_Width = NullTextureDimension;
		NullTextureInfo.m_Height = NullTextureDimension;
		NullTextureInfo.m_Format = CImageInfo::FORMAT_RGBA;
		NullTextureInfo.m_pData = aNullTextureData;
		const int TextureLoadFlags = Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
		m_NullTexture.Invalidate();
		m_NullTexture = LoadTextureRaw(NullTextureInfo, TextureLoadFlags, "null-texture");
		dbg_assert(m_NullTexture.IsNullTexture(), "Null texture invalid");
	}

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
	m_pBackend = nullptr;

	// delete the command buffers
	for(auto &pCommandBuffer : m_apCommandBuffers)
		delete pCommandBuffer;
}

int CGraphics_Threaded::GetNumScreens() const
{
	return m_pBackend->GetNumScreens();
}

const char *CGraphics_Threaded::GetScreenName(int Screen) const
{
	return m_pBackend->GetScreenName(Screen);
}

void CGraphics_Threaded::Minimize()
{
	m_pBackend->Minimize();

	for(auto &PropChangedListener : m_vPropChangeListeners)
		PropChangedListener();
}

void CGraphics_Threaded::Maximize()
{
	// TODO: SDL
	m_pBackend->Maximize();

	for(auto &PropChangedListener : m_vPropChangeListeners)
		PropChangedListener();
}

void CGraphics_Threaded::WarnPngliteIncompatibleImages(bool Warn)
{
	m_WarnPngliteIncompatibleImages = Warn;
}

void CGraphics_Threaded::SetWindowParams(int FullscreenMode, bool IsBorderless)
{
	m_pBackend->SetWindowParams(FullscreenMode, IsBorderless);
	CVideoMode CurMode;
	m_pBackend->GetCurrentVideoMode(CurMode, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, g_Config.m_GfxScreen);
	GotResized(CurMode.m_WindowWidth, CurMode.m_WindowHeight, CurMode.m_RefreshRate);

	for(auto &PropChangedListener : m_vPropChangeListeners)
		PropChangedListener();
}

bool CGraphics_Threaded::SetWindowScreen(int Index)
{
	if(!m_pBackend->SetWindowScreen(Index))
	{
		return false;
	}

	// send a got resized event so that the current canvas size is requested
	GotResized(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight, g_Config.m_GfxScreenRefreshRate);

	for(auto &PropChangedListener : m_vPropChangeListeners)
		PropChangedListener();

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

	// send a got resized event so that the current canvas size is requested
	GotResized(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight, g_Config.m_GfxScreenRefreshRate);

	for(auto &PropChangedListener : m_vPropChangeListeners)
		PropChangedListener();
}

bool CGraphics_Threaded::Resize(int w, int h, int RefreshRate)
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && IVideo::Current()->IsRecording())
		return false;
#endif

	if(WindowWidth() == w && WindowHeight() == h && RefreshRate == m_ScreenRefreshRate)
		return false;

	// if the size is changed manually, only set the window resize, a window size changed event is triggered anyway
	if(m_pBackend->ResizeWindow(w, h, RefreshRate))
	{
		CVideoMode CurMode;
		m_pBackend->GetCurrentVideoMode(CurMode, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, g_Config.m_GfxScreen);
		GotResized(w, h, RefreshRate);
		return true;
	}
	return false;
}

void CGraphics_Threaded::ResizeToScreen()
{
	if(Resize(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight, g_Config.m_GfxScreenRefreshRate))
		return;

	// Revert config variables if the change was not accepted
	g_Config.m_GfxScreenWidth = ScreenWidth();
	g_Config.m_GfxScreenHeight = ScreenHeight();
	g_Config.m_GfxScreenRefreshRate = m_ScreenRefreshRate;
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
	auto PrevCanvasWidth = m_ScreenWidth;
	auto PrevCanvasHeight = m_ScreenHeight;
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

	if(PrevCanvasWidth != m_ScreenWidth || PrevCanvasHeight != m_ScreenHeight)
	{
		for(auto &ResizeListener : m_vResizeListeners)
			ResizeListener();
	}
}

void CGraphics_Threaded::AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc)
{
	m_vResizeListeners.emplace_back(pFunc);
}

void CGraphics_Threaded::AddWindowPropChangeListener(WINDOW_PROPS_CHANGED_FUNC pFunc)
{
	m_vPropChangeListeners.emplace_back(pFunc);
}

int CGraphics_Threaded::GetWindowScreen()
{
	return m_pBackend->GetWindowScreen();
}

void CGraphics_Threaded::WindowDestroyNtf(uint32_t WindowId)
{
	m_pBackend->WindowDestroyNtf(WindowId);

	CCommandBuffer::SCommand_WindowDestroyNtf Cmd;
	Cmd.m_WindowId = WindowId;
	AddCmd(Cmd);

	// wait
	KickCommandBuffer();
	WaitForIdle();
}

void CGraphics_Threaded::WindowCreateNtf(uint32_t WindowId)
{
	m_pBackend->WindowCreateNtf(WindowId);

	CCommandBuffer::SCommand_WindowCreateNtf Cmd;
	Cmd.m_WindowId = WindowId;
	AddCmd(Cmd);

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
	m_pBackend->SetWindowGrab(Grab);
}

void CGraphics_Threaded::NotifyWindow()
{
	m_pBackend->NotifyWindow();
}

void CGraphics_Threaded::ReadPixel(ivec2 Position, ColorRGBA *pColor)
{
	dbg_assert(Position.x >= 0 && Position.x < ScreenWidth(), "ReadPixel position x out of range");
	dbg_assert(Position.y >= 0 && Position.y < ScreenHeight(), "ReadPixel position y out of range");

	m_ReadPixelPosition = Position;
	m_pReadPixelColor = pColor;
}

void CGraphics_Threaded::ReadPixelDirect(bool *pSwapped)
{
	if(m_pReadPixelColor == nullptr)
		return;

	CCommandBuffer::SCommand_TrySwapAndReadPixel Cmd;
	Cmd.m_Position = m_ReadPixelPosition;
	Cmd.m_pColor = m_pReadPixelColor;
	Cmd.m_pSwapped = pSwapped;
	AddCmd(Cmd);

	KickCommandBuffer();
	WaitForIdle();

	m_pReadPixelColor = nullptr;
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
	bool Swapped = false;
	ScreenshotDirect(&Swapped);
	ReadPixelDirect(&Swapped);

	if(!Swapped)
	{
		CCommandBuffer::SCommand_Swap Cmd;
		AddCmd(Cmd);
	}

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
	AddCmd(Cmd);

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
	AddCmd(Cmd);

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
	AddCmd(Cmd);
}

bool CGraphics_Threaded::IsIdle() const
{
	return m_pBackend->IsIdle();
}

void CGraphics_Threaded::WaitForIdle()
{
	m_pBackend->WaitForIdle();
}

void CGraphics_Threaded::AddWarning(const SWarning &Warning)
{
	const std::unique_lock<std::mutex> Lock(m_WarningsMutex);
	m_vWarnings.emplace_back(Warning);
}

std::optional<SWarning> CGraphics_Threaded::CurrentWarning()
{
	const std::unique_lock<std::mutex> Lock(m_WarningsMutex);
	if(m_vWarnings.empty())
	{
		return std::nullopt;
	}
	else
	{
		std::optional<SWarning> Result = std::make_optional(m_vWarnings[0]);
		m_vWarnings.erase(m_vWarnings.begin());
		return Result;
	}
}

bool CGraphics_Threaded::ShowMessageBox(unsigned Type, const char *pTitle, const char *pMsg)
{
	if(m_pBackend == nullptr)
		return false;
	m_pBackend->WaitForIdle();
	return m_pBackend->ShowMessageBox(Type, pTitle, pMsg);
}

bool CGraphics_Threaded::IsBackendInitialized()
{
	return m_pBackend != nullptr;
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
		const int Count = minimum<size_t>(std::size(g_aFakeModes), MaxModes);
		mem_copy(pModes, g_aFakeModes, Count * sizeof(CVideoMode));
		return Count;
	}

	int NumModes = 0;
	m_pBackend->GetVideoModes(pModes, MaxModes, &NumModes, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, Screen);
	return NumModes;
}

void CGraphics_Threaded::GetCurrentVideoMode(CVideoMode &CurMode, int Screen)
{
	m_pBackend->GetCurrentVideoMode(CurMode, m_ScreenHiDPIScale, g_Config.m_GfxDesktopWidth, g_Config.m_GfxDesktopHeight, Screen);
}

extern IEngineGraphics *CreateEngineGraphicsThreaded()
{
	return new CGraphics_Threaded();
}
