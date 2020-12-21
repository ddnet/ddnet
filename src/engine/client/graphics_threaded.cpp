/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/math.h>
#include <base/tl/threading.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <base/system.h>

#include <pnglite.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/localization.h>

#include <engine/shared/image_manipulation.h>

#include <math.h> // cosf, sinf, log2f

#if defined(CONF_VIDEORECORDER)
#include "video.h"
#endif

#include "graphics_threaded.h"

static CVideoMode g_aFakeModes[] = {
	{8192, 4320, 8, 8, 8}, {7680, 4320, 8, 8, 8}, {5120, 2880, 8, 8, 8},
	{4096, 2160, 8, 8, 8}, {3840, 2160, 8, 8, 8}, {2560, 1440, 8, 8, 8},
	{2048, 1536, 8, 8, 8}, {1920, 2400, 8, 8, 8}, {1920, 1440, 8, 8, 8},
	{1920, 1200, 8, 8, 8}, {1920, 1080, 8, 8, 8}, {1856, 1392, 8, 8, 8},
	{1800, 1440, 8, 8, 8}, {1792, 1344, 8, 8, 8}, {1680, 1050, 8, 8, 8},
	{1600, 1200, 8, 8, 8}, {1600, 1000, 8, 8, 8}, {1440, 1050, 8, 8, 8},
	{1440, 900, 8, 8, 8}, {1400, 1050, 8, 8, 8}, {1368, 768, 8, 8, 8},
	{1280, 1024, 8, 8, 8}, {1280, 960, 8, 8, 8}, {1280, 800, 8, 8, 8},
	{1280, 768, 8, 8, 8}, {1152, 864, 8, 8, 8}, {1024, 768, 8, 8, 8},
	{1024, 600, 8, 8, 8}, {800, 600, 8, 8, 8}, {768, 576, 8, 8, 8},
	{720, 400, 8, 8, 8}, {640, 480, 8, 8, 8}, {400, 300, 8, 8, 8},
	{320, 240, 8, 8, 8},

	{8192, 4320, 5, 6, 5}, {7680, 4320, 5, 6, 5}, {5120, 2880, 5, 6, 5},
	{4096, 2160, 5, 6, 5}, {3840, 2160, 5, 6, 5}, {2560, 1440, 5, 6, 5},
	{2048, 1536, 5, 6, 5}, {1920, 2400, 5, 6, 5}, {1920, 1440, 5, 6, 5},
	{1920, 1200, 5, 6, 5}, {1920, 1080, 5, 6, 5}, {1856, 1392, 5, 6, 5},
	{1800, 1440, 5, 6, 5}, {1792, 1344, 5, 6, 5}, {1680, 1050, 5, 6, 5},
	{1600, 1200, 5, 6, 5}, {1600, 1000, 5, 6, 5}, {1440, 1050, 5, 6, 5},
	{1440, 900, 5, 6, 5}, {1400, 1050, 5, 6, 5}, {1368, 768, 5, 6, 5},
	{1280, 1024, 5, 6, 5}, {1280, 960, 5, 6, 5}, {1280, 800, 5, 6, 5},
	{1280, 768, 5, 6, 5}, {1152, 864, 5, 6, 5}, {1024, 768, 5, 6, 5},
	{1024, 600, 5, 6, 5}, {800, 600, 5, 6, 5}, {768, 576, 5, 6, 5},
	{720, 400, 5, 6, 5}, {640, 480, 5, 6, 5}, {400, 300, 5, 6, 5},
	{320, 240, 5, 6, 5}};

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

void CGraphics_Threaded::FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor)
{
	CCommandBuffer::SCommand_RenderTextStream Cmd;
	int PrimType, PrimCount, NumVerts;
	size_t VertSize = sizeof(CCommandBuffer::SVertex);

	Cmd.m_TextureSize = TextureSize;
	Cmd.m_TextTextureIndex = TextTextureIndex;
	Cmd.m_TextOutlineTextureIndex = TextOutlineTextureIndex;
	mem_copy(Cmd.m_aTextOutlineColor, pOutlineTextColor, sizeof(Cmd.m_aTextOutlineColor));

	FlushVerticesImpl(false, PrimType, PrimCount, NumVerts, Cmd, VertSize);
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

	m_Rotation = 0;
	m_Drawing = 0;

	m_TextureMemoryUsage = 0;

	m_RenderEnable = true;
	m_DoScreenshot = false;

	png_init(0, 0); // ignore_convention
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

int CGraphics_Threaded::MemoryUsage() const
{
	return m_pBackend->MemoryUsage();
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

int CGraphics_Threaded::UnloadTexture(CTextureHandle Index)
{
	if(Index == m_InvalidTexture)
		return 0;

	if(Index < 0)
		return 0;

	CCommandBuffer::SCommand_Texture_Destroy Cmd;
	Cmd.m_Slot = Index;
	m_pCommandBuffer->AddCommand(Cmd);

	m_TextureIndices[Index] = m_FirstFreeTexture;
	m_FirstFreeTexture = Index;
	return 0;
}

int CGraphics_Threaded::UnloadTextureNew(CTextureHandle &TextureHandle)
{
	int Ret = UnloadTexture(TextureHandle);
	TextureHandle = IGraphics::CTextureHandle();
	return Ret;
}

static int ImageFormatToTexFormat(int Format)
{
	if(Format == CImageInfo::FORMAT_RGB)
		return CCommandBuffer::TEXFORMAT_RGB;
	if(Format == CImageInfo::FORMAT_RGBA)
		return CCommandBuffer::TEXFORMAT_RGBA;
	if(Format == CImageInfo::FORMAT_ALPHA)
		return CCommandBuffer::TEXFORMAT_ALPHA;
	return CCommandBuffer::TEXFORMAT_RGBA;
}

static int ImageFormatToPixelSize(int Format)
{
	switch(Format)
	{
	case CImageInfo::FORMAT_RGB: return 3;
	case CImageInfo::FORMAT_ALPHA: return 1;
	default: return 4;
	}
}

int CGraphics_Threaded::LoadTextureRawSub(CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
	CCommandBuffer::SCommand_Texture_Update Cmd;
	Cmd.m_Slot = TextureID;
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_Format = ImageFormatToTexFormat(Format);

	// calculate memory usage
	int MemSize = Width * Height * ImageFormatToPixelSize(Format);

	// copy texture data
	void *pTmpData = malloc(MemSize);
	mem_copy(pTmpData, pData, MemSize);
	Cmd.m_pData = pTmpData;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();
		m_pCommandBuffer->AddCommand(Cmd);
	}
	return 0;
}

IGraphics::CTextureHandle CGraphics_Threaded::LoadSpriteTextureImpl(CImageInfo &FromImageInfo, int x, int y, int w, int h)
{
	int bpp = ImageFormatToPixelSize(FromImageInfo.m_Format);

	m_SpriteHelper.resize(w * h * bpp);

	CopyTextureFromTextureBufferSub(&m_SpriteHelper[0], w, h, (uint8_t *)FromImageInfo.m_pData, FromImageInfo.m_Width, FromImageInfo.m_Height, bpp, x, y, w, h);

	IGraphics::CTextureHandle RetHandle = LoadTextureRaw(w, h, FromImageInfo.m_Format, &m_SpriteHelper[0], FromImageInfo.m_Format, 0);

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
	if(FromImageInfo.m_Format == CImageInfo::FORMAT_ALPHA || FromImageInfo.m_Format == CImageInfo::FORMAT_RGBA)
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
			str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), Localize("The width or height of texture %s is not divisible by 16, which might cause visual bugs."), aText);

			m_Warnings.emplace_back(NewWarning);
		}
	}

	if(Width == 0 || Height == 0)
		return IGraphics::CTextureHandle();

	// grab texture
	int Tex = m_FirstFreeTexture;
	if(Tex == -1)
	{
		size_t CurSize = m_TextureIndices.size();
		m_TextureIndices.resize(CurSize * 2);
		for(size_t i = 0; i < CurSize - 1; ++i)
		{
			m_TextureIndices[CurSize + i] = CurSize + i + 1;
		}
		m_TextureIndices.back() = -1;

		Tex = CurSize;
	}
	m_FirstFreeTexture = m_TextureIndices[Tex];
	m_TextureIndices[Tex] = -1;

	CCommandBuffer::SCommand_Texture_Create Cmd;
	Cmd.m_Slot = Tex;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_PixelSize = ImageFormatToPixelSize(Format);
	Cmd.m_Format = ImageFormatToTexFormat(Format);
	Cmd.m_StoreFormat = ImageFormatToTexFormat(StoreFormat);

	// flags
	Cmd.m_Flags = 0;
	if(Flags & IGraphics::TEXLOAD_NOMIPMAPS)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_NOMIPMAPS;
	if(g_Config.m_GfxTextureCompressionOld && ((Flags & IGraphics::TEXLOAD_NO_COMPRESSION) == 0))
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_COMPRESSED;
	if(g_Config.m_GfxTextureQualityOld || Flags & TEXLOAD_NORESAMPLE)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_QUALITY;
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
	mem_copy(pTmpData, pData, MemSize);
	Cmd.m_pData = pTmpData;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();
		m_pCommandBuffer->AddCommand(Cmd);
	}

	return CreateTextureHandle(Tex);
}

// simple uncompressed RGBA loaders
IGraphics::CTextureHandle CGraphics_Threaded::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	int ID;
	CImageInfo Img;

	if(l < 3)
		return CTextureHandle();
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		if(StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags, pFilename);
		free(Img.m_pData);
		if(ID != m_InvalidTexture && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return CreateTextureHandle(ID);
	}

	return m_InvalidTexture;
}

int CGraphics_Threaded::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s', pnglite: %s", aCompleteFilename, png_error_string(Error));
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)malloc((size_t)Png.width * Png.height * Png.bpp); // ignore_convention
	Error = png_get_data(&Png, pBuffer); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to read image. filename='%s', pnglite: %s", aCompleteFilename, png_error_string(Error));
		free(pBuffer);
		png_close_file(&Png); // ignore_convention
		return 0;
	}
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	else
	{
		free(pBuffer);
		return 0;
	}
	pImg->m_pData = pBuffer;
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

		m_Warnings.emplace_back(NewWarning);

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
		if(Img.m_Format == CImageInfo::FORMAT_ALPHA)
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

void CGraphics_Threaded::ScreenshotDirect()
{
	// add swap command
	CImageInfo Image;
	mem_zero(&Image, sizeof(Image));

	CCommandBuffer::SCommand_Screenshot Cmd;
	Cmd.m_pImage = &Image;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the buffer and wait for the result
	KickCommandBuffer();
	WaitForIdle();

	if(Image.m_pData)
	{
		// find filename
		char aWholePath[1024];
		png_t Png; // ignore_convention

		IOHANDLE File = m_pStorage->OpenFile(m_aScreenshotName, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));
		if(File)
			io_close(File);

		// save png
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "saved screenshot to '%s'", aWholePath);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		png_open_file_write(&Png, aWholePath); // ignore_convention
		png_set_data(&Png, Image.m_Width, Image.m_Height, 8, PNG_TRUECOLOR, (unsigned char *)Image.m_pData); // ignore_convention
		png_close_file(&Png); // ignore_convention

		free(Image.m_pData);
	}
}

void CGraphics_Threaded::TextureSet(CTextureHandle TextureID)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");
	m_State.m_Texture = TextureID;
}

void CGraphics_Threaded::Clear(float r, float g, float b)
{
	CCommandBuffer::SCommand_Clear Cmd;
	Cmd.m_Color.r = r;
	Cmd.m_Color.g = g;
	Cmd.m_Color.b = b;
	Cmd.m_Color.a = 0;
	m_pCommandBuffer->AddCommand(Cmd);
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

void CGraphics_Threaded::TextQuadsBegin()
{
	QuadsBegin();
}

void CGraphics_Threaded::TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float *pOutlineTextColor)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->TextQuadsEnd without begin");
	FlushTextVertices(TextureSize, TextTextureIndex, TextOutlineTextureIndex, pOutlineTextColor);
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

void CGraphics_Threaded::SetColor(ColorRGBA rgb)
{
	SetColor(rgb.r, rgb.g, rgb.b, rgb.a);
}

void CGraphics_Threaded::SetColor4(vec4 TopLeft, vec4 TopRight, vec4 BottomLeft, vec4 BottomRight)
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
	if(g_Config.m_GfxQuadAsTriangle && !m_IsNewOpenGL)
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
	if(g_Config.m_GfxQuadAsTriangle && !m_IsNewOpenGL)
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

	if((g_Config.m_GfxQuadAsTriangle && !m_IsNewOpenGL) || m_Drawing == DRAWING_TRIANGLES)
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

void CGraphics_Threaded::RenderTileLayer(int BufferContainerIndex, float *pColor, char **pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet)
{
	if(NumIndicesOffet == 0)
		return;

	//add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderTileLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndicesDrawNum = NumIndicesOffet;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));

	void *Data = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffet);
	if(Data == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		void *Data = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffet);
		if(Data == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}
	Cmd.m_pIndicesOffsets = (char **)Data;
	Cmd.m_pDrawCount = (unsigned int *)(((char *)Data) + (sizeof(char *) * NumIndicesOffet));

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Data = m_pCommandBuffer->AllocData((sizeof(char *) + sizeof(unsigned int)) * NumIndicesOffet);
		if(Data == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
		Cmd.m_pIndicesOffsets = (char **)Data;
		Cmd.m_pDrawCount = (unsigned int *)(((char *)Data) + (sizeof(char *) * NumIndicesOffet));

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}

	mem_copy(Cmd.m_pIndicesOffsets, pOffsets, sizeof(char *) * NumIndicesOffet);
	mem_copy(Cmd.m_pDrawCount, IndicedVertexDrawNum, sizeof(unsigned int) * NumIndicesOffet);

	//todo max indices group check!!
}

void CGraphics_Threaded::RenderBorderTiles(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, int JumpIndex, unsigned int DrawNum)
{
	if(DrawNum == 0)
		return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTile Cmd;
	Cmd.m_State = m_State;
	Cmd.m_DrawNum = DrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));

	Cmd.m_pIndicesOffset = pIndexBufferOffset;
	Cmd.m_JumpIndex = JumpIndex;

	Cmd.m_Offset[0] = pOffset[0];
	Cmd.m_Offset[1] = pOffset[1];
	Cmd.m_Dir[0] = pDir[0];
	Cmd.m_Dir[1] = pDir[1];

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}
}

void CGraphics_Threaded::RenderBorderTileLines(int BufferContainerIndex, float *pColor, char *pIndexBufferOffset, float *pOffset, float *pDir, unsigned int IndexDrawNum, unsigned int RedrawNum)
{
	if(IndexDrawNum == 0 || RedrawNum == 0)
		return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTileLine Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndexDrawNum = IndexDrawNum;
	Cmd.m_DrawNum = RedrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));

	Cmd.m_pIndicesOffset = pIndexBufferOffset;

	Cmd.m_Offset[0] = pOffset[0];
	Cmd.m_Offset[1] = pOffset[1];
	Cmd.m_Dir[0] = pDir[0];
	Cmd.m_Dir[1] = pDir[1];

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}
}

void CGraphics_Threaded::RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo *pQuadInfo, int QuadNum)
{
	if(QuadNum == 0)
		return;

	//add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderQuadLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_QuadNum = QuadNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;

	Cmd.m_pQuadInfo = (SQuadRenderInfo *)AllocCommandBufferData(QuadNum * sizeof(SQuadRenderInfo));
	if(Cmd.m_pQuadInfo == 0x0)
		return;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pQuadInfo = (SQuadRenderInfo *)m_pCommandBuffer->AllocData(QuadNum * sizeof(SQuadRenderInfo));
		if(Cmd.m_pQuadInfo == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for the quad info");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render quad command");
			return;
		}
	}

	mem_copy(Cmd.m_pQuadInfo, pQuadInfo, sizeof(SQuadRenderInfo) * QuadNum);
}

void CGraphics_Threaded::RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float *pTextColor, float *pTextoutlineColor)
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
	mem_copy(Cmd.m_aTextColor, pTextColor, sizeof(Cmd.m_aTextColor));
	mem_copy(Cmd.m_aTextOutlineColor, pTextoutlineColor, sizeof(Cmd.m_aTextOutlineColor));

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render text command");
			return;
		}
	}
}

int CGraphics_Threaded::CreateQuadContainer()
{
	int Index = -1;
	if(m_FirstFreeQuadContainer == -1)
	{
		Index = m_QuadContainers.size();
		m_QuadContainers.push_back(SQuadContainer());
	}
	else
	{
		Index = m_FirstFreeQuadContainer;
		m_FirstFreeQuadContainer = m_QuadContainers[Index].m_FreeIndex;
		m_QuadContainers[Index].m_FreeIndex = Index;
	}

	return Index;
}

void CGraphics_Threaded::QuadContainerUpload(int ContainerIndex)
{
	if(IsQuadContainerBufferingEnabled())
	{
		SQuadContainer &Container = m_QuadContainers[ContainerIndex];
		if(Container.m_Quads.size() > 0)
		{
			if(Container.m_QuadBufferObjectIndex == -1)
			{
				size_t UploadDataSize = Container.m_Quads.size() * sizeof(SQuadContainer::SQuad);
				Container.m_QuadBufferObjectIndex = CreateBufferObject(UploadDataSize, &Container.m_Quads[0]);
			}
			else
			{
				size_t UploadDataSize = Container.m_Quads.size() * sizeof(SQuadContainer::SQuad);
				RecreateBufferObject(Container.m_QuadBufferObjectIndex, UploadDataSize, &Container.m_Quads[0]);
			}

			if(Container.m_QuadBufferContainerIndex == -1)
			{
				SBufferContainerInfo Info;
				Info.m_Stride = sizeof(CCommandBuffer::SVertex);

				Info.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
				SBufferContainerInfo::SAttribute *pAttr = &Info.m_Attributes.back();
				pAttr->m_DataTypeCount = 2;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = false;
				pAttr->m_pOffset = 0;
				pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
				pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;
				Info.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
				pAttr = &Info.m_Attributes.back();
				pAttr->m_DataTypeCount = 2;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = false;
				pAttr->m_pOffset = (void *)(sizeof(float) * 2);
				pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
				pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;
				Info.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
				pAttr = &Info.m_Attributes.back();
				pAttr->m_DataTypeCount = 4;
				pAttr->m_FuncType = 0;
				pAttr->m_Normalized = true;
				pAttr->m_pOffset = (void *)(sizeof(float) * 2 + sizeof(float) * 2);
				pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
				pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;

				Container.m_QuadBufferContainerIndex = CreateBufferContainer(&Info);
			}
		}
	}
}

void CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num)
{
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() > Num + CCommandBuffer::CCommandBuffer::MAX_VERTICES)
		return;

	for(int i = 0; i < Num; ++i)
	{
		Container.m_Quads.push_back(SQuadContainer::SQuad());
		SQuadContainer::SQuad &Quad = Container.m_Quads.back();

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

	QuadContainerUpload(ContainerIndex);
}

void CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num)
{
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() > Num + CCommandBuffer::CCommandBuffer::MAX_VERTICES)
		return;

	for(int i = 0; i < Num; ++i)
	{
		Container.m_Quads.push_back(SQuadContainer::SQuad());
		SQuadContainer::SQuad &Quad = Container.m_Quads.back();

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

	QuadContainerUpload(ContainerIndex);
}

void CGraphics_Threaded::QuadContainerReset(int ContainerIndex)
{
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];
	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex != -1)
			DeleteBufferContainer(Container.m_QuadBufferContainerIndex, true);
	}
	Container.m_Quads.clear();
	Container.m_QuadBufferContainerIndex = Container.m_QuadBufferObjectIndex = -1;
}

void CGraphics_Threaded::DeleteQuadContainer(int ContainerIndex)
{
	QuadContainerReset(ContainerIndex);

	// also clear the container index
	m_QuadContainers[ContainerIndex].m_FreeIndex = m_FirstFreeQuadContainer;
	m_FirstFreeQuadContainer = ContainerIndex;
}

void CGraphics_Threaded::RenderQuadContainer(int ContainerIndex, int QuadDrawNum)
{
	RenderQuadContainer(ContainerIndex, 0, QuadDrawNum);
}

void CGraphics_Threaded::RenderQuadContainer(int ContainerIndex, int QuadOffset, int QuadDrawNum)
{
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];

	if(QuadDrawNum == -1)
		QuadDrawNum = (int)Container.m_Quads.size() - QuadOffset;

	if((int)Container.m_Quads.size() < QuadOffset + QuadDrawNum || QuadDrawNum == 0)
		return;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		WrapClamp();

		CCommandBuffer::SCommand_RenderQuadContainer Cmd;
		Cmd.m_State = m_State;
		Cmd.m_DrawNum = (unsigned int)QuadDrawNum * 6;
		Cmd.m_pOffset = (void *)(QuadOffset * 6 * sizeof(unsigned int));
		Cmd.m_BufferContainerIndex = Container.m_QuadBufferContainerIndex;

		// check if we have enough free memory in the commandbuffer
		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for render quad container");
				return;
			}
		}
	}
	else
	{
		if(g_Config.m_GfxQuadAsTriangle)
		{
			for(int i = 0; i < QuadDrawNum; ++i)
			{
				SQuadContainer::SQuad &Quad = Container.m_Quads[QuadOffset + i];
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
			mem_copy(m_aVertices, &Container.m_Quads[QuadOffset], sizeof(CCommandBuffer::SVertex) * 4 * QuadDrawNum);
			m_NumVertices += 4 * QuadDrawNum;
		}
		m_Drawing = DRAWING_QUADS;
		WrapClamp();
		FlushVertices(false);
		m_Drawing = 0;
	}
	WrapNormal();
}

void CGraphics_Threaded::RenderQuadContainerEx(int ContainerIndex, int QuadOffset, int QuadDrawNum, float X, float Y, float ScaleX, float ScaleY)
{
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() < QuadOffset + 1)
		return;

	if(QuadDrawNum == -1)
		QuadDrawNum = (int)Container.m_Quads.size() - QuadOffset;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		SQuadContainer::SQuad &Quad = Container.m_Quads[QuadOffset];
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

		// check if we have enough free memory in the commandbuffer
		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for render quad container extended");
				return;
			}
		}
	}
	else
	{
		if(g_Config.m_GfxQuadAsTriangle)
		{
			for(int i = 0; i < QuadDrawNum; ++i)
			{
				SQuadContainer::SQuad &Quad = Container.m_Quads[QuadOffset + i];
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
			mem_copy(m_aVertices, &Container.m_Quads[QuadOffset], sizeof(CCommandBuffer::SVertex) * 4 * QuadDrawNum);
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
	SQuadContainer &Container = m_QuadContainers[ContainerIndex];

	if(DrawCount == 0)
		return;

	if(IsQuadContainerBufferingEnabled())
	{
		if(Container.m_QuadBufferContainerIndex == -1)
			return;

		WrapClamp();
		SQuadContainer::SQuad &Quad = Container.m_Quads[0];
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

		// check if we have enough free memory in the commandbuffer
		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Cmd.m_pRenderInfo = (IGraphics::SRenderSpriteInfo *)m_pCommandBuffer->AllocData(sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
			if(Cmd.m_pRenderInfo == 0x0)
			{
				dbg_msg("graphics", "failed to allocate data for render info");
				return;
			}

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for render quad container sprite");
				return;
			}
		}

		mem_copy(Cmd.m_pRenderInfo, pRenderInfo, sizeof(IGraphics::SRenderSpriteInfo) * DrawCount);
		WrapNormal();
	}
	else
	{
		for(int i = 0; i < DrawCount; ++i)
		{
			QuadsSetRotation(pRenderInfo[i].m_Rotation);
			RenderQuadContainerAsSprite(ContainerIndex, QuadOffset, pRenderInfo[i].m_Pos[0], pRenderInfo[i].m_Pos[1], pRenderInfo[i].m_Scale, pRenderInfo[i].m_Scale);
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

int CGraphics_Threaded::CreateBufferObject(size_t UploadDataSize, void *pUploadData, bool IsMovedPointer)
{
	int Index = -1;
	if(m_FirstFreeBufferObjectIndex == -1)
	{
		Index = m_BufferObjectIndices.size();
		m_BufferObjectIndices.push_back(Index);
	}
	else
	{
		Index = m_FirstFreeBufferObjectIndex;
		m_FirstFreeBufferObjectIndex = m_BufferObjectIndices[Index];
		m_BufferObjectIndices[Index] = Index;
	}

	CCommandBuffer::SCommand_CreateBufferObject Cmd;
	Cmd.m_BufferIndex = Index;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_DeletePointer = IsMovedPointer;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for update buffer object command");
				return -1;
			}
		}
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
			if(Cmd.m_pUploadData == NULL)
				return -1;

			// check if we have enough free memory in the commandbuffer
			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				// kick command buffer and try again
				KickCommandBuffer();

				Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
				if(Cmd.m_pUploadData == 0x0)
				{
					dbg_msg("graphics", "failed to allocate data for upload data");
					return -1;
				}

				if(!m_pCommandBuffer->AddCommand(Cmd))
				{
					dbg_msg("graphics", "failed to allocate memory for create buffer object command");
					return -1;
				}
			}
			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = NULL;
			// check if we have enough free memory in the commandbuffer
			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				// kick command buffer and try again
				KickCommandBuffer();
				if(!m_pCommandBuffer->AddCommand(Cmd))
				{
					dbg_msg("graphics", "failed to allocate memory for create buffer object command");
					return -1;
				}
			}

			// update the buffer instead
			size_t UploadDataOffset = 0;
			while(UploadDataSize > 0)
			{
				size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

				UpdateBufferObject(Index, UpdateSize, (((char *)pUploadData) + UploadDataOffset), (void *)UploadDataOffset);

				UploadDataOffset += UpdateSize;
				UploadDataSize -= UpdateSize;
			}
		}
	}

	return Index;
}

void CGraphics_Threaded::RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, bool IsMovedPointer)
{
	CCommandBuffer::SCommand_RecreateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_DeletePointer = IsMovedPointer;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for recreate buffer object command");
				return;
			}
		}
	}
	else
	{
		if(UploadDataSize <= CMD_BUFFER_DATA_BUFFER_SIZE)
		{
			Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
			if(Cmd.m_pUploadData == NULL)
				return;

			// check if we have enough free memory in the commandbuffer
			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				// kick command buffer and try again
				KickCommandBuffer();

				Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
				if(Cmd.m_pUploadData == 0x0)
				{
					dbg_msg("graphics", "failed to allocate data for upload data");
					return;
				}

				if(!m_pCommandBuffer->AddCommand(Cmd))
				{
					dbg_msg("graphics", "failed to allocate memory for recreate buffer object command");
					return;
				}
			}

			mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
		}
		else
		{
			Cmd.m_pUploadData = NULL;
			// check if we have enough free memory in the commandbuffer
			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				// kick command buffer and try again
				KickCommandBuffer();
				if(!m_pCommandBuffer->AddCommand(Cmd))
				{
					dbg_msg("graphics", "failed to allocate memory for update buffer object command");
					return;
				}
			}

			// update the buffer instead
			size_t UploadDataOffset = 0;
			while(UploadDataSize > 0)
			{
				size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

				UpdateBufferObject(BufferIndex, UpdateSize, (((char *)pUploadData) + UploadDataOffset), (void *)UploadDataOffset);

				UploadDataOffset += UpdateSize;
				UploadDataSize -= UpdateSize;
			}
		}
	}
}

void CGraphics_Threaded::UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, void *pOffset, bool IsMovedPointer)
{
	CCommandBuffer::SCommand_UpdateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_pOffset = pOffset;
	Cmd.m_DeletePointer = IsMovedPointer;

	if(IsMovedPointer)
	{
		Cmd.m_pUploadData = pUploadData;

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for update buffer object command");
				return;
			}
		}
	}
	else
	{
		Cmd.m_pUploadData = AllocCommandBufferData(UploadDataSize);
		if(Cmd.m_pUploadData == NULL)
			return;

		// check if we have enough free memory in the commandbuffer
		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			// kick command buffer and try again
			KickCommandBuffer();

			Cmd.m_pUploadData = m_pCommandBuffer->AllocData(UploadDataSize);
			if(Cmd.m_pUploadData == 0x0)
			{
				dbg_msg("graphics", "failed to allocate data for upload data");
				return;
			}

			if(!m_pCommandBuffer->AddCommand(Cmd))
			{
				dbg_msg("graphics", "failed to allocate memory for update buffer object command");
				return;
			}
		}

		mem_copy(Cmd.m_pUploadData, pUploadData, UploadDataSize);
	}
}

void CGraphics_Threaded::CopyBufferObject(int WriteBufferIndex, int ReadBufferIndex, size_t WriteOffset, size_t ReadOffset, size_t CopyDataSize)
{
	CCommandBuffer::SCommand_CopyBufferObject Cmd;
	Cmd.m_WriteBufferIndex = WriteBufferIndex;
	Cmd.m_ReadBufferIndex = ReadBufferIndex;
	Cmd.m_pWriteOffset = WriteOffset;
	Cmd.m_pReadOffset = ReadOffset;
	Cmd.m_CopySize = CopyDataSize;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for copy buffer object command");
			return;
		}
	}
}

void CGraphics_Threaded::DeleteBufferObject(int BufferIndex)
{
	if(BufferIndex == -1)
		return;

	CCommandBuffer::SCommand_DeleteBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for delete buffer object command");
			return;
		}
	}

	// also clear the buffer object index
	m_BufferObjectIndices[BufferIndex] = m_FirstFreeBufferObjectIndex;
	m_FirstFreeBufferObjectIndex = BufferIndex;
}

int CGraphics_Threaded::CreateBufferContainer(SBufferContainerInfo *pContainerInfo)
{
	int Index = -1;
	if(m_FirstFreeVertexArrayInfo == -1)
	{
		Index = m_VertexArrayInfo.size();
		m_VertexArrayInfo.push_back(SVertexArrayInfo());
	}
	else
	{
		Index = m_FirstFreeVertexArrayInfo;
		m_FirstFreeVertexArrayInfo = m_VertexArrayInfo[Index].m_FreeIndex;
		m_VertexArrayInfo[Index].m_FreeIndex = Index;
	}

	CCommandBuffer::SCommand_CreateBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = Index;
	Cmd.m_AttrCount = (int)pContainerInfo->m_Attributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;

	Cmd.m_Attributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_Attributes == NULL)
		return -1;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Attributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
		if(Cmd.m_Attributes == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for upload data");
			return -1;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for create buffer container command");
			return -1;
		}
	}

	mem_copy(Cmd.m_Attributes, &pContainerInfo->m_Attributes[0], Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	for(auto &Attribute : pContainerInfo->m_Attributes)
		m_VertexArrayInfo[Index].m_AssociatedBufferObjectIndices.push_back(Attribute.m_VertBufferBindingIndex);

	return Index;
}

void CGraphics_Threaded::DeleteBufferContainer(int ContainerIndex, bool DestroyAllBO)
{
	if(ContainerIndex == -1)
		return;
	CCommandBuffer::SCommand_DeleteBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_DestroyAllBO = DestroyAllBO;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for delete buffer container command");
			return;
		}
	}

	if(DestroyAllBO)
	{
		// delete all associated references
		for(size_t i = 0; i < m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.size(); ++i)
		{
			int BufferObjectIndex = m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices[i];
			if(BufferObjectIndex != -1)
			{
				// don't delete double entries
				for(int &m_AssociatedBufferObjectIndice : m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices)
				{
					if(BufferObjectIndex == m_AssociatedBufferObjectIndice)
						m_AssociatedBufferObjectIndice = -1;
				}
				// clear the buffer object index
				m_BufferObjectIndices[BufferObjectIndex] = m_FirstFreeBufferObjectIndex;
				m_FirstFreeBufferObjectIndex = BufferObjectIndex;
			}
		}
	}
	m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.clear();

	// also clear the buffer object index
	m_VertexArrayInfo[ContainerIndex].m_FreeIndex = m_FirstFreeVertexArrayInfo;
	m_FirstFreeVertexArrayInfo = ContainerIndex;
}

void CGraphics_Threaded::UpdateBufferContainer(int ContainerIndex, SBufferContainerInfo *pContainerInfo)
{
	CCommandBuffer::SCommand_UpdateBufferContainer Cmd;
	Cmd.m_BufferContainerIndex = ContainerIndex;
	Cmd.m_AttrCount = (int)pContainerInfo->m_Attributes.size();
	Cmd.m_Stride = pContainerInfo->m_Stride;

	Cmd.m_Attributes = (SBufferContainerInfo::SAttribute *)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_Attributes == NULL)
		return;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Attributes = (SBufferContainerInfo::SAttribute *)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
		if(Cmd.m_Attributes == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for upload data");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for update buffer container command");
			return;
		}
	}

	mem_copy(Cmd.m_Attributes, &pContainerInfo->m_Attributes[0], Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));

	m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.clear();
	for(auto &Attribute : pContainerInfo->m_Attributes)
		m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.push_back(Attribute.m_VertBufferBindingIndex);
}

void CGraphics_Threaded::IndicesNumRequiredNotify(unsigned int RequiredIndicesCount)
{
	CCommandBuffer::SCommand_IndicesRequiredNumNotify Cmd;
	Cmd.m_RequiredIndicesNum = RequiredIndicesCount;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for indcies required count notify command");
			return;
		}
	}
}

int CGraphics_Threaded::IssueInit()
{
	int Flags = 0;

	if(g_Config.m_GfxBorderless)
		Flags |= IGraphicsBackend::INITFLAG_BORDERLESS;
	if(g_Config.m_GfxFullscreen)
		Flags |= IGraphicsBackend::INITFLAG_FULLSCREEN;
	if(g_Config.m_GfxVsync)
		Flags |= IGraphicsBackend::INITFLAG_VSYNC;
	if(g_Config.m_GfxHighdpi)
		Flags |= IGraphicsBackend::INITFLAG_HIGHDPI;
	if(g_Config.m_GfxResizable)
		Flags |= IGraphicsBackend::INITFLAG_RESIZABLE;

	int r = m_pBackend->Init("DDNet Client", &g_Config.m_GfxScreen, &g_Config.m_GfxScreenWidth, &g_Config.m_GfxScreenHeight, g_Config.m_GfxFsaaSamples, Flags, &m_DesktopScreenWidth, &m_DesktopScreenHeight, &m_ScreenWidth, &m_ScreenHeight, m_pStorage);
	AddBackEndWarningIfExists();
	m_IsNewOpenGL = m_pBackend->IsNewOpenGL();
	m_OpenGLTileBufferingEnabled = m_IsNewOpenGL || m_pBackend->HasTileBuffering();
	m_OpenGLQuadBufferingEnabled = m_IsNewOpenGL || m_pBackend->HasQuadBuffering();
	m_OpenGLQuadContainerBufferingEnabled = m_IsNewOpenGL || m_pBackend->HasQuadContainerBuffering();
	m_OpenGLTextBufferingEnabled = m_IsNewOpenGL || (m_OpenGLQuadContainerBufferingEnabled && m_pBackend->HasTextBuffering());
	m_OpenGLHasTextureArrays = m_IsNewOpenGL || m_pBackend->Has2DTextureArrays();
	return r;
}

void CGraphics_Threaded::AddBackEndWarningIfExists()
{
	const char *pErrStr = m_pBackend->GetErrorString();
	if(pErrStr != NULL)
	{
		SWarning NewWarning;
		str_format(NewWarning.m_aWarningMsg, sizeof(NewWarning.m_aWarningMsg), "%s", Localize(pErrStr));
		m_Warnings.emplace_back(NewWarning);
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
		g_Config.m_GfxFsaaSamples--;

		if(g_Config.m_GfxFsaaSamples)
			dbg_msg("gfx", "lowering FSAA to %d and trying again", g_Config.m_GfxFsaaSamples);
		else
			dbg_msg("gfx", "disabling FSAA and trying again");

		ErrorCode = IssueInit();
		if(ErrorCode == 0)
			return 0;
	}

	size_t OpenGLInitTryCount = 0;
	while(ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_CONTEXT_FAILED || ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_VERSION_FAILED)
	{
		if(ErrorCode == EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_OPENGL_CONTEXT_FAILED)
		{
			// try next smaller major/minor or patch version
			if(g_Config.m_GfxOpenGLMajor >= 4)
			{
				g_Config.m_GfxOpenGLMajor = 3;
				g_Config.m_GfxOpenGLMinor = 3;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor >= 1)
			{
				g_Config.m_GfxOpenGLMajor = 3;
				g_Config.m_GfxOpenGLMinor = 0;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 0)
			{
				g_Config.m_GfxOpenGLMajor = 2;
				g_Config.m_GfxOpenGLMinor = 1;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 2 && g_Config.m_GfxOpenGLMinor >= 1)
			{
				g_Config.m_GfxOpenGLMajor = 2;
				g_Config.m_GfxOpenGLMinor = 0;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 2 && g_Config.m_GfxOpenGLMinor == 0)
			{
				g_Config.m_GfxOpenGLMajor = 1;
				g_Config.m_GfxOpenGLMinor = 5;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 1 && g_Config.m_GfxOpenGLMinor == 5)
			{
				g_Config.m_GfxOpenGLMajor = 1;
				g_Config.m_GfxOpenGLMinor = 4;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 1 && g_Config.m_GfxOpenGLMinor == 4)
			{
				g_Config.m_GfxOpenGLMajor = 1;
				g_Config.m_GfxOpenGLMinor = 3;
				g_Config.m_GfxOpenGLPatch = 0;
			}
			else if(g_Config.m_GfxOpenGLMajor == 1 && g_Config.m_GfxOpenGLMinor == 3)
			{
				g_Config.m_GfxOpenGLMajor = 1;
				g_Config.m_GfxOpenGLMinor = 2;
				g_Config.m_GfxOpenGLPatch = 1;
			}
			else if(g_Config.m_GfxOpenGLMajor == 1 && g_Config.m_GfxOpenGLMinor == 2)
			{
				g_Config.m_GfxOpenGLMajor = 1;
				g_Config.m_GfxOpenGLMinor = 1;
				g_Config.m_GfxOpenGLPatch = 0;
			}
		}

		// new opengl version was set by backend, try again
		ErrorCode = IssueInit();
		if(ErrorCode == 0)
		{
			return 0;
		}

		if(++OpenGLInitTryCount >= 9)
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
	m_TextureIndices.resize(CCommandBuffer::MAX_TEXTURES);
	for(int i = 0; i < (int)m_TextureIndices.size() - 1; i++)
		m_TextureIndices[i] = i + 1;
	m_TextureIndices.back() = -1;

	m_FirstFreeVertexArrayInfo = -1;
	m_FirstFreeBufferObjectIndex = -1;
	m_FirstFreeQuadContainer = -1;

	m_pBackend = CreateGraphicsBackend();
	if(InitWindow() != 0)
		return -1;

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

	m_InvalidTexture = LoadTextureRaw(4, 4, CImageInfo::FORMAT_RGBA, s_aNullTextureData, CImageInfo::FORMAT_RGBA, TEXLOAD_NORESAMPLE);
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

bool CGraphics_Threaded::Fullscreen(bool State)
{
	return m_pBackend->Fullscreen(State);
}

void CGraphics_Threaded::SetWindowBordered(bool State)
{
	m_pBackend->SetWindowBordered(State);
}

bool CGraphics_Threaded::SetWindowScreen(int Index)
{
	return m_pBackend->SetWindowScreen(Index);
}

void CGraphics_Threaded::Resize(int w, int h, bool SetWindowSize)
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && IVideo::Current()->IsRecording())
		return;
#endif

	if(m_DesktopScreenWidth == w && m_DesktopScreenHeight == h)
		return;

	if(SetWindowSize)
		m_pBackend->ResizeWindow(w, h);

	m_DesktopScreenWidth = w;
	m_DesktopScreenHeight = h;

	m_pBackend->GetViewportSize(m_ScreenWidth, m_ScreenHeight);

	// adjust the viewport to only allow certain aspect ratios
	if(m_ScreenHeight > 4 * m_ScreenWidth / 5)
		m_ScreenHeight = 4 * m_ScreenWidth / 5;
	if(m_ScreenWidth > 21 * m_ScreenHeight / 9)
		m_ScreenWidth = 21 * m_ScreenHeight / 9;

	CCommandBuffer::SCommand_Resize Cmd;
	Cmd.m_Width = m_ScreenWidth;
	Cmd.m_Height = m_ScreenHeight;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the command buffer
	KickCommandBuffer();
	WaitForIdle();

	for(auto &ResizeListener : m_ResizeListeners)
		ResizeListener.m_pFunc(ResizeListener.m_pUser);
}

void CGraphics_Threaded::AddWindowResizeListener(WINDOW_RESIZE_FUNC pFunc, void *pUser)
{
	m_ResizeListeners.push_back(SWindowResizeListener(pFunc, pUser));
}

int CGraphics_Threaded::GetWindowScreen()
{
	return m_pBackend->GetWindowScreen();
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
	str_copy(m_aScreenshotName, pFilename, sizeof(m_aScreenshotName));
	m_DoScreenshot = true;
}

void CGraphics_Threaded::Swap()
{
	if(!m_Warnings.empty())
	{
		SWarning *pCurWarning = GetCurWarning();
		if(pCurWarning->m_WasShown)
		{
			m_Warnings.erase(m_Warnings.begin());
		}
	}

	// TODO: screenshot support
	if(m_DoScreenshot)
	{
		if(WindowActive())
			ScreenshotDirect();
		m_DoScreenshot = false;
	}

	// add swap command
	CCommandBuffer::SCommand_Swap Cmd;
	Cmd.m_Finish = g_Config.m_GfxFinish;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the command buffer
	KickCommandBuffer();
}

bool CGraphics_Threaded::SetVSync(bool State)
{
	if(!m_pCommandBuffer)
		return true;

	// add vsnc command
	bool RetOk = false;
	CCommandBuffer::SCommand_VSync Cmd;
	Cmd.m_VSync = State ? 1 : 0;
	Cmd.m_pRetOk = &RetOk;
	m_pCommandBuffer->AddCommand(Cmd);

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
	m_pCommandBuffer->AddCommand(Cmd);
}

bool CGraphics_Threaded::IsIdle()
{
	return m_pBackend->IsIdle();
}

void CGraphics_Threaded::WaitForIdle()
{
	m_pBackend->WaitForIdle();
}

SWarning *CGraphics_Threaded::GetCurWarning()
{
	if(m_Warnings.empty())
		return NULL;
	else
	{
		SWarning *pCurWarning = &m_Warnings[0];
		return pCurWarning;
	}
}

int CGraphics_Threaded::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	if(g_Config.m_GfxDisplayAllModes)
	{
		int Count = sizeof(g_aFakeModes) / sizeof(CVideoMode);
		mem_copy(pModes, g_aFakeModes, sizeof(g_aFakeModes));
		if(MaxModes < Count)
			Count = MaxModes;
		return Count;
	}

	// add videomodes command
	CImageInfo Image;
	mem_zero(&Image, sizeof(Image));

	int NumModes = 0;
	CCommandBuffer::SCommand_VideoModes Cmd;
	Cmd.m_pModes = pModes;
	Cmd.m_MaxModes = MaxModes;
	Cmd.m_pNumModes = &NumModes;
	Cmd.m_Screen = Screen;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the buffer and wait for the result and return it
	KickCommandBuffer();
	WaitForIdle();
	return NumModes;
}

extern IEngineGraphics *CreateEngineGraphicsThreaded() { return new CGraphics_Threaded(); }
