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

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/keys.h>
#include <engine/console.h>

#include <math.h> // cosf, sinf, log2f

#include "graphics_threaded.h"

static CVideoMode g_aFakeModes[] = {
	{320,240,8,8,8}, {400,300,8,8,8}, {640,480,8,8,8},
	{720,400,8,8,8}, {768,576,8,8,8}, {800,600,8,8,8},
	{1024,600,8,8,8}, {1024,768,8,8,8}, {1152,864,8,8,8},
	{1280,768,8,8,8}, {1280,800,8,8,8}, {1280,960,8,8,8},
	{1280,1024,8,8,8}, {1368,768,8,8,8}, {1400,1050,8,8,8},
	{1440,900,8,8,8}, {1440,1050,8,8,8}, {1600,1000,8,8,8},
	{1600,1200,8,8,8}, {1680,1050,8,8,8}, {1792,1344,8,8,8},
	{1800,1440,8,8,8}, {1856,1392,8,8,8}, {1920,1080,8,8,8},
	{1920,1200,8,8,8}, {1920,1440,8,8,8}, {1920,2400,8,8,8},
	{2048,1536,8,8,8},

	{320,240,5,6,5}, {400,300,5,6,5}, {640,480,5,6,5},
	{720,400,5,6,5}, {768,576,5,6,5}, {800,600,5,6,5},
	{1024,600,5,6,5}, {1024,768,5,6,5}, {1152,864,5,6,5},
	{1280,768,5,6,5}, {1280,800,5,6,5}, {1280,960,5,6,5},
	{1280,1024,5,6,5}, {1368,768,5,6,5}, {1400,1050,5,6,5},
	{1440,900,5,6,5}, {1440,1050,5,6,5}, {1600,1000,5,6,5},
	{1600,1200,5,6,5}, {1680,1050,5,6,5}, {1792,1344,5,6,5},
	{1800,1440,5,6,5}, {1856,1392,5,6,5}, {1920,1080,5,6,5},
	{1920,1200,5,6,5}, {1920,1440,5,6,5}, {1920,2400,5,6,5},
	{2048,1536,5,6,5}
};

void CGraphics_Threaded::FlushVertices(bool KeepVertices)
{
	if(m_NumVertices == 0)
		return;
	
	size_t VertSize = sizeof(CCommandBuffer::SVertex);

	int NumVerts = m_NumVertices;
	
	if(!KeepVertices)
		m_NumVertices = 0;

	CCommandBuffer::SCommand_Render Cmd;
	Cmd.m_State = m_State;

	if(m_Drawing == DRAWING_QUADS)
	{
		if(g_Config.m_GfxQuadAsTriangle && !m_UseOpenGL3_3)
		{
			Cmd.m_PrimType = CCommandBuffer::PRIMTYPE_TRIANGLES;
			Cmd.m_PrimCount = NumVerts/3;
		}
		else
		{
			Cmd.m_PrimType = CCommandBuffer::PRIMTYPE_QUADS;
			Cmd.m_PrimCount = NumVerts/4;
		}
	}
	else if(m_Drawing == DRAWING_LINES)
	{
		Cmd.m_PrimType = CCommandBuffer::PRIMTYPE_LINES;
		Cmd.m_PrimCount = NumVerts/2;
	}
	else
		return;

	Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
	if(Cmd.m_pVertices == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}

	mem_copy(Cmd.m_pVertices, m_aVertices, VertSize*NumVerts);
}

void CGraphics_Threaded::FlushTextVertices(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float* pOutlineTextColor)
{
	if(m_NumVertices == 0)
		return;

	size_t VertSize = 0;
	VertSize = sizeof(CCommandBuffer::SVertex);

	int NumVerts = m_NumVertices;

	m_NumVertices = 0;

	CCommandBuffer::SCommand_RenderTextStream Cmd;
	Cmd.m_State = m_State;
	Cmd.m_TextureSize = TextureSize;
	Cmd.m_TextTextureIndex = TextTextureIndex;
	Cmd.m_TextOutlineTextureIndex = TextOutlineTextureIndex;
	mem_copy(Cmd.m_aTextOutlineColor, pOutlineTextColor, sizeof(Cmd.m_aTextOutlineColor));

	Cmd.m_QuadNum = NumVerts / 4;
	
	Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
	if(Cmd.m_pVertices == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices = (CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}

	mem_copy(Cmd.m_pVertices, m_aVertices, VertSize*NumVerts);
}

void CGraphics_Threaded::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= MAX_VERTICES)
		FlushVertices();
}

void CGraphics_Threaded::Rotate(const CCommandBuffer::SPoint &rCenter, CCommandBuffer::SVertex *pPoints, int NumPoints)
{
	float c = cosf(m_Rotation);
	float s = sinf(m_Rotation);
	float x, y;
	int i;
	
	CCommandBuffer::SVertex *pVertices = (CCommandBuffer::SVertex*) pPoints;
	for(i = 0; i < NumPoints; i++)
	{
		x = pVertices[i].m_Pos.x - rCenter.x;
		y = pVertices[i].m_Pos.y - rCenter.y;
		pVertices[i].m_Pos.x = x * c - y * s + rCenter.x;
		pVertices[i].m_Pos.y = x * s + y * c + rCenter.y;
	}
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
	m_InvalidTexture = 0;

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
	w = clamp(w, 0, ScreenWidth()-x);
	h = clamp(h, 0, ScreenHeight()-y);

	m_State.m_ClipEnable = true;
	m_State.m_ClipX = x;
	m_State.m_ClipY = ScreenHeight()-(y+h);
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
	SetColor(1,1,1,1);
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
		m_aVertices[m_NumVertices + 2*i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 2*i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 2*i].m_Tex = m_aTexture[0];
		SetColor(&m_aVertices[m_NumVertices + 2*i], 0);

		m_aVertices[m_NumVertices + 2*i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 2*i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 2*i + 1].m_Tex = m_aTexture[1];
		SetColor(&m_aVertices[m_NumVertices + 2*i + 1], 1);
	}

	AddVertices(2*Num);
}

int CGraphics_Threaded::UnloadTexture(int Index)
{
	if(Index == m_InvalidTexture)
		return 0;

	if(Index < 0)
		return 0;

	CCommandBuffer::SCommand_Texture_Destroy Cmd;
	Cmd.m_Slot = Index;
	m_pCommandBuffer->AddCommand(Cmd);

	m_aTextureIndices[Index] = m_FirstFreeTexture;
	m_FirstFreeTexture = Index;
	return 0;
}

static int ImageFormatToTexFormat(int Format)
{
	if(Format == CImageInfo::FORMAT_RGB) return CCommandBuffer::TEXFORMAT_RGB;
	if(Format == CImageInfo::FORMAT_RGBA) return CCommandBuffer::TEXFORMAT_RGBA;
	if(Format == CImageInfo::FORMAT_ALPHA) return CCommandBuffer::TEXFORMAT_ALPHA;
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


int CGraphics_Threaded::LoadTextureRawSub(int TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
	CCommandBuffer::SCommand_Texture_Update Cmd;
	Cmd.m_Slot = TextureID;
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_Format = ImageFormatToTexFormat(Format);

	// calculate memory usage
	int MemSize = Width*Height*ImageFormatToPixelSize(Format);

	// copy texture data
	void *pTmpData = mem_alloc(MemSize, sizeof(void*));
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

int CGraphics_Threaded::LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags)
{
	// don't waste memory on texture if we are stress testing
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
		return m_InvalidTexture;
#endif

	// grab texture
	int Tex = m_FirstFreeTexture;
	m_FirstFreeTexture = m_aTextureIndices[Tex];
	m_aTextureIndices[Tex] = -1;

	CCommandBuffer::SCommand_Texture_Create Cmd;
	Cmd.m_Slot = Tex;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_PixelSize = ImageFormatToPixelSize(Format);
	Cmd.m_Format = ImageFormatToTexFormat(Format);
	Cmd.m_StoreFormat = ImageFormatToTexFormat(StoreFormat);

	// flags
	Cmd.m_Flags = 0;
	if(Flags&IGraphics::TEXLOAD_NOMIPMAPS)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_NOMIPMAPS;
	if(g_Config.m_GfxTextureCompression)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_COMPRESSED;
	if(g_Config.m_GfxTextureQuality || Flags&TEXLOAD_NORESAMPLE)
		Cmd.m_Flags |= CCommandBuffer::TEXFLAG_QUALITY;

	// copy texture data
	int MemSize = Width*Height*Cmd.m_PixelSize;
	void *pTmpData = mem_alloc(MemSize, sizeof(void*));
	mem_copy(pTmpData, pData, MemSize);
	Cmd.m_pData = pTmpData;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();
		m_pCommandBuffer->AddCommand(Cmd);
	}

	return Tex;
}

// simple uncompressed RGBA loaders
int CGraphics_Threaded::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	int ID;
	CImageInfo Img;

	if(l < 3)
		return -1;
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		if(StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags);
		mem_free(Img.m_pData);
		if(ID != m_InvalidTexture && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return ID;
	}

	return m_InvalidTexture;
}

int CGraphics_Threaded::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0,0); // ignore_convention

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
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
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

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer); // ignore_convention
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
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

		mem_free(Image.m_pData);
	}
}

void CGraphics_Threaded::TextureSet(int TextureID)
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

void CGraphics_Threaded::TextQuadsEnd(int TextureSize, int TextTextureIndex, int TextOutlineTextureIndex, float* pOutlineTextColor)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->TextQuadsEnd without begin");
	FlushTextVertices(TextureSize, TextTextureIndex, TextOutlineTextureIndex, pOutlineTextColor);
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

inline void clampf(float& Value, float Min, float Max)
{
	if(Value > Max) Value = Max;
	else if(Value < Min) Value = Min;
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
		m_aColor[pArray[i].m_Index].r = (unsigned char)(r*255.f);
		m_aColor[pArray[i].m_Index].g = (unsigned char)(g*255.f);
		m_aColor[pArray[i].m_Index].b = (unsigned char)(b*255.f);
		m_aColor[pArray[i].m_Index].a = (unsigned char)(a*255.f);
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
	
	for(int i = 0; i < 4; ++i)
	{
		m_aColor[i].r = (unsigned char)(r);
		m_aColor[i].g = (unsigned char)(g);
		m_aColor[i].b = (unsigned char)(b);
		m_aColor[i].a = (unsigned char)(a);
	}
}

void CGraphics_Threaded::ChangeColorOfCurrentQuadVertices(float r, float g, float b, float a)
{
	clampf(r, 0.f, 1.f);
	clampf(g, 0.f, 1.f);
	clampf(b, 0.f, 1.f);
	clampf(a, 0.f, 1.f);
	m_aColor[0].r = (unsigned char)(r*255.f);
	m_aColor[0].g = (unsigned char)(g*255.f);
	m_aColor[0].b = (unsigned char)(b*255.f);
	m_aColor[0].a = (unsigned char)(a*255.f);

	for(int i = 0; i < m_NumVertices; ++i)
	{
		SetColor(&m_aVertices[i], 0);
	}
}

void CGraphics_Threaded::SetColor(CCommandBuffer::SVertex *pVertex, int ColorIndex)
{
	CCommandBuffer::SVertex *pVert = (CCommandBuffer::SVertex*)pVertex;
	pVert->m_Color.r = m_aColor[ColorIndex].r;
	pVert->m_Color.g = m_aColor[ColorIndex].g;
	pVert->m_Color.b = m_aColor[ColorIndex].b;
	pVert->m_Color.a = m_aColor[ColorIndex].a;
}

void CGraphics_Threaded::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV)
{
	m_aTexture[0].u = TlU;	m_aTexture[1].u = BrU;
	m_aTexture[0].v = TlV;	m_aTexture[1].v = TlV;

	m_aTexture[3].u = TlU;	m_aTexture[2].u = BrU;
	m_aTexture[3].v = BrV;	m_aTexture[2].v = BrV;
}

void CGraphics_Threaded::QuadsSetSubsetFree(
	float x0, float y0, float x1, float y1,
	float x2, float y2, float x3, float y3)
{
	m_aTexture[0].u = x0; m_aTexture[0].v = y0;
	m_aTexture[1].u = x1; m_aTexture[1].v = y1;
	m_aTexture[2].u = x2; m_aTexture[2].v = y2;
	m_aTexture[3].u = x3; m_aTexture[3].v = y3;
}

void CGraphics_Threaded::QuadsDraw(CQuadItem *pArray, int Num)
{
	for(int i = 0; i < Num; ++i)
	{
		pArray[i].m_X -= pArray[i].m_Width/2;
		pArray[i].m_Y -= pArray[i].m_Height/2;
	}

	QuadsDrawTL(pArray, Num);
}

void CGraphics_Threaded::QuadsDrawTL(const CQuadItem *pArray, int Num)
{
	CCommandBuffer::SPoint Center;

	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawTL without begin");

	if(g_Config.m_GfxQuadAsTriangle && !m_UseOpenGL3_3)
	{
		for(int i = 0; i < Num; ++i)
		{
			// first triangle
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6*i], 0);

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 1], 1);

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 2], 2);

			// second triangle
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 3], 0);

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 4], 2);

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 5], 3);

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 6*i], 6);
			}
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 4*i], 0);

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 1], 1);

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 2], 2);

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 3], 3);

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 4*i], 4);
			}
		}

		AddVertices(4*Num);
	}
}

void CGraphics_Threaded::QuadsDrawFreeform(const CFreeformItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawFreeform without begin");

	if(g_Config.m_GfxQuadAsTriangle && !m_UseOpenGL3_3)
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6*i], 0);

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 1], 1);

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 2], 3);

			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 3], 0);

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 4], 3);

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 6*i + 5], 2);
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			SetColor(&m_aVertices[m_NumVertices + 4*i], 0);

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 1], 1);

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[3];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 2], 3);

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[2];
			SetColor(&m_aVertices[m_NumVertices + 4*i + 3], 2);
		}

		AddVertices(4*Num);
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
				(c%16)/16.0f,
				(c/16)/16.0f,
				(c%16)/16.0f+1.0f/16.0f,
				(c/16)/16.0f+1.0f/16.0f);

			CQuadItem QuadItem(x, y, Size, Size);
			QuadsDrawTL(&QuadItem, 1);
			x += Size/2;
		}
	}
}

void CGraphics_Threaded::RenderTileLayer(int BufferContainerIndex, float *pColor, char** pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet)
{
	if(NumIndicesOffet == 0)
		return;
	
	//add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderTileLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndicesDrawNum = NumIndicesOffet;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));
	float ScreenZoomRatio = ScreenWidth() / (m_State.m_ScreenBR.x - m_State.m_ScreenTL.x);
	//the number of pixels we would skip in the fragment shader -- basically the LOD
	float LODFactor = (64.f / (32.f * ScreenZoomRatio));
	//log2 gives us the amount of halving the texture for mipmapping
	int LOD = (int)log2f(LODFactor);
	//5 because log2(1024/(2^5)) is 5 -- 2^5 = 32 which would mean 2 pixels per tile index
	if(LOD > 5) LOD = 5;
	if(LOD < 0) LOD = 0;
	Cmd.m_LOD = LOD;

	void *Data = m_pCommandBuffer->AllocData((sizeof(char*) + sizeof(unsigned int))*NumIndicesOffet);
	if(Data == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();
	
		void *Data = m_pCommandBuffer->AllocData((sizeof(char*) + sizeof(unsigned int))*NumIndicesOffet);
		if(Data == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}
	Cmd.m_pIndicesOffsets = (char**)Data;
	Cmd.m_pDrawCount = (unsigned int*)(((char*)Data) + (sizeof(char*)*NumIndicesOffet));
	
	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Data = m_pCommandBuffer->AllocData((sizeof(char*) + sizeof(unsigned int))*NumIndicesOffet);
		if(Data == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
		Cmd.m_pIndicesOffsets = (char**)Data;
		Cmd.m_pDrawCount = (unsigned int*)(((char*)Data) + (sizeof(char*)*NumIndicesOffet));

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}

	
	mem_copy(Cmd.m_pIndicesOffsets, pOffsets, sizeof(char*)*NumIndicesOffet);
	mem_copy(Cmd.m_pDrawCount, IndicedVertexDrawNum, sizeof(unsigned int)*NumIndicesOffet);
	
	//todo max indices group check!!
}

void CGraphics_Threaded::RenderBorderTiles(int BufferContainerIndex, float *pColor, char *pOffset, float *Offset, float *Dir, int JumpIndex, unsigned int DrawNum)
{
	if(DrawNum == 0) return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTile Cmd;
	Cmd.m_State = m_State;
	Cmd.m_DrawNum = DrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));
	float ScreenZoomRatio = ScreenWidth() / (m_State.m_ScreenBR.x - m_State.m_ScreenTL.x);
	//the number of pixels we would skip in the fragment shader -- basically the LOD
	float LODFactor = (64.f / (32.f * ScreenZoomRatio));
	//log2 gives us the amount of halving the texture for mipmapping
	int LOD = (int)log2f(LODFactor);
	if(LOD > 5) LOD = 5;
	if(LOD < 0) LOD = 0;
	Cmd.m_LOD = LOD;

	Cmd.m_pIndicesOffset = pOffset;
	Cmd.m_JumpIndex = JumpIndex;
	
	Cmd.m_Offset[0] = Offset[0];
	Cmd.m_Offset[1] = Offset[1];
	Cmd.m_Dir[0] = Dir[0];
	Cmd.m_Dir[1] = Dir[1];

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

void CGraphics_Threaded::RenderBorderTileLines(int BufferContainerIndex, float *pColor, char *pOffset, float *Dir, unsigned int IndexDrawNum, unsigned int RedrawNum)
{
	if(IndexDrawNum == 0 || RedrawNum == 0) return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTileLine Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndexDrawNum = IndexDrawNum;
	Cmd.m_DrawNum = RedrawNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;
	mem_copy(&Cmd.m_Color, pColor, sizeof(Cmd.m_Color));
	float ScreenZoomRatio = ScreenWidth() / (m_State.m_ScreenBR.x - m_State.m_ScreenTL.x);
	//the number of pixels we would skip in the fragment shader -- basically the LOD
	float LODFactor = (64.f / (32.f * ScreenZoomRatio));
	//log2 gives us the amount of halving the texture for mipmapping
	int LOD = (int)log2f(LODFactor);
	if(LOD > 5) LOD = 5;
	if(LOD < 0) LOD = 0;
	Cmd.m_LOD = LOD;

	Cmd.m_pIndicesOffset = pOffset;
	
	Cmd.m_Dir[0] = Dir[0];
	Cmd.m_Dir[1] = Dir[1];

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

void CGraphics_Threaded::RenderQuadLayer(int BufferContainerIndex, SQuadRenderInfo* pQuadInfo, int QuadNum)
{
	if(QuadNum == 0)
		return;

	//add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderQuadLayer Cmd;
	Cmd.m_State = m_State;
	Cmd.m_QuadNum = QuadNum;
	Cmd.m_BufferContainerIndex = BufferContainerIndex;

	Cmd.m_pQuadInfo = (SQuadRenderInfo*)AllocCommandBufferData(QuadNum * sizeof(SQuadRenderInfo));
	if(Cmd.m_pQuadInfo == 0x0)
		return;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pQuadInfo = (SQuadRenderInfo*)m_pCommandBuffer->AllocData(QuadNum * sizeof(SQuadRenderInfo));
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

void CGraphics_Threaded::RenderText(int BufferContainerIndex, int TextQuadNum, int TextureSize, int TextureTextIndex, int TextureTextOutlineIndex, float* pTextColor, float* pTextoutlineColor)
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

void CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CQuadItem *pArray, int Num)
{
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() > Num + CCommandBuffer::MAX_VERTICES)
		return;

	for(int i = 0; i < Num; ++i)
	{
		Container.m_Quads.push_back(SQuadContainer::SQuad());
		SQuadContainer::SQuad& Quad = Container.m_Quads.back();

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
			SBufferContainerInfo::SAttribute* pAttr = &Info.m_Attributes.back();
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
			pAttr->m_pOffset = (void*)(sizeof(float) * 2);
			pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
			pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;
			Info.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
			pAttr = &Info.m_Attributes.back();
			pAttr->m_DataTypeCount = 4;
			pAttr->m_FuncType = 0;
			pAttr->m_Normalized = true;
			pAttr->m_pOffset = (void*)(sizeof(float) * 2 + sizeof(float) * 2);
			pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
			pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;

			Container.m_QuadBufferContainerIndex = CreateBufferContainer(&Info);
		}
	}
}

void CGraphics_Threaded::QuadContainerAddQuads(int ContainerIndex, CFreeformItem *pArray, int Num)
{
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() > Num + CCommandBuffer::MAX_VERTICES)
		return;

	for(int i = 0; i < Num; ++i)
	{
		Container.m_Quads.push_back(SQuadContainer::SQuad());
		SQuadContainer::SQuad& Quad = Container.m_Quads.back();

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
			SBufferContainerInfo::SAttribute* pAttr = &Info.m_Attributes.back();
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
			pAttr->m_pOffset = (void*)(sizeof(float) * 2);
			pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
			pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;
			Info.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
			pAttr = &Info.m_Attributes.back();
			pAttr->m_DataTypeCount = 4;
			pAttr->m_FuncType = 0;
			pAttr->m_Normalized = true;
			pAttr->m_pOffset = (void*)(sizeof(float) * 2 + sizeof(float) * 2);
			pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
			pAttr->m_VertBufferBindingIndex = Container.m_QuadBufferObjectIndex;

			Container.m_QuadBufferContainerIndex = CreateBufferContainer(&Info);
		}
	}
}

void CGraphics_Threaded::QuadContainerReset(int ContainerIndex)
{
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];
	if(Container.m_QuadBufferContainerIndex != -1)
		DeleteBufferContainer(Container.m_QuadBufferContainerIndex, true);
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
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];

	if(QuadDrawNum == -1)
		QuadDrawNum = (int)Container.m_Quads.size() - QuadOffset;

	if((int)Container.m_Quads.size() < QuadOffset + QuadDrawNum || QuadDrawNum == 0)
		return;

	if(Container.m_QuadBufferContainerIndex == -1)
		return;

	if(m_UseOpenGL3_3)
	{
		CCommandBuffer::SCommand_RenderQuadContainer Cmd;
		Cmd.m_State = m_State;
		Cmd.m_DrawNum = (unsigned int)QuadDrawNum * 6;
		Cmd.m_pOffset = (void*)(QuadOffset * 6 * sizeof(unsigned int));
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
				SQuadContainer::SQuad& Quad = Container.m_Quads[QuadOffset + i];
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
		FlushVertices(false);
		m_Drawing = 0;
	}
}

void CGraphics_Threaded::RenderQuadContainerAsSprite(int ContainerIndex, int QuadOffset, float X, float Y, float ScaleX, float ScaleY)
{
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];

	if((int)Container.m_Quads.size() < QuadOffset + 1)
		return;

	if(Container.m_QuadBufferContainerIndex == -1)
		return;

	if(m_UseOpenGL3_3)
	{
		SQuadContainer::SQuad& Quad = Container.m_Quads[QuadOffset];
		CCommandBuffer::SCommand_RenderQuadContainerAsSprite Cmd;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		MapScreen((ScreenX0 - X) / ScaleX, (ScreenY0 - Y) / ScaleY, (ScreenX1 - X) / ScaleX, (ScreenY1 - Y) / ScaleY);
		Cmd.m_State = m_State;
		MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);

		Cmd.m_DrawNum = 1 * 6;
		Cmd.m_pOffset = (void*)(QuadOffset * 6 * sizeof(unsigned int));
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
				dbg_msg("graphics", "failed to allocate memory for render quad container sprite");
				return;
			}
		}
	}
	else
	{
		if(g_Config.m_GfxQuadAsTriangle)
		{
			SQuadContainer::SQuad& Quad = Container.m_Quads[QuadOffset];
			m_aVertices[0] = Quad.m_aVertices[0];
			m_aVertices[1] = Quad.m_aVertices[1];
			m_aVertices[2] = Quad.m_aVertices[2];
			m_aVertices[3] = Quad.m_aVertices[0];
			m_aVertices[4] = Quad.m_aVertices[2];
			m_aVertices[5] = Quad.m_aVertices[3];

			for(int i = 0; i < 6; ++i)
			{
				m_aVertices[i].m_Pos.x *= ScaleX;
				m_aVertices[i].m_Pos.y *= ScaleY;
			}

			SetColor(&m_aVertices[0], 0);
			SetColor(&m_aVertices[1], 0);
			SetColor(&m_aVertices[2], 0);
			SetColor(&m_aVertices[3], 0);
			SetColor(&m_aVertices[4], 0);
			SetColor(&m_aVertices[5], 0);

			if(m_Rotation != 0)
			{
				CCommandBuffer::SPoint Center;
				Center.x = m_aVertices[0].m_Pos.x + (m_aVertices[1].m_Pos.x - m_aVertices[0].m_Pos.x) / 2.f;
				Center.y = m_aVertices[0].m_Pos.y + (m_aVertices[2].m_Pos.y - m_aVertices[0].m_Pos.y) / 2.f;

				Rotate(Center, &m_aVertices[0], 6);
			}

			for(int i = 0; i < 6; ++i)
			{
				m_aVertices[i].m_Pos.x += X;
				m_aVertices[i].m_Pos.y += Y;
			}
			m_NumVertices += 6;
		}
		else
		{
			mem_copy(m_aVertices, &Container.m_Quads[QuadOffset], sizeof(CCommandBuffer::SVertex) * 4 * 1);
			SetColor(&m_aVertices[0], 0);
			SetColor(&m_aVertices[1], 0);
			SetColor(&m_aVertices[2], 0);
			SetColor(&m_aVertices[3], 0);

			for(int i = 0; i < 4; ++i)
			{
				m_aVertices[i].m_Pos.x *= ScaleX;
				m_aVertices[i].m_Pos.y *= ScaleY;
			}

			if(m_Rotation != 0)
			{
				CCommandBuffer::SPoint Center;
				Center.x = m_aVertices[0].m_Pos.x + (m_aVertices[1].m_Pos.x - m_aVertices[0].m_Pos.x) / 2.f;
				Center.y = m_aVertices[0].m_Pos.y + (m_aVertices[2].m_Pos.y - m_aVertices[0].m_Pos.y) / 2.f;

				Rotate(Center, &m_aVertices[0], 4);
			}

			for(int i = 0; i < 4; ++i)
			{
				m_aVertices[i].m_Pos.x += X;
				m_aVertices[i].m_Pos.y += Y;
			}
			m_NumVertices += 4;
		}
		m_Drawing = DRAWING_QUADS;
		FlushVertices(false);
		m_Drawing = 0;
	}
}

void CGraphics_Threaded::RenderQuadContainerAsSpriteMultiple(int ContainerIndex, int QuadOffset, int DrawCount, SRenderSpriteInfo *pRenderInfo)
{
	SQuadContainer& Container = m_QuadContainers[ContainerIndex];

	if(DrawCount == 0)
		return;

	if(Container.m_QuadBufferContainerIndex == -1)
		return;

	if(m_UseOpenGL3_3)
	{
		SQuadContainer::SQuad& Quad = Container.m_Quads[0];
		CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple Cmd;

		Cmd.m_State = m_State;

		Cmd.m_DrawNum = 1 * 6;
		Cmd.m_DrawCount = DrawCount;
		Cmd.m_pOffset = (void*)(QuadOffset * 6 * sizeof(unsigned int));
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

void* CGraphics_Threaded::AllocCommandBufferData(unsigned AllocSize) {
	void* pData = m_pCommandBuffer->AllocData(AllocSize); 
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

int CGraphics_Threaded::CreateBufferObject(size_t UploadDataSize, void *pUploadData)
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
	else {
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
		while(UploadDataSize > 0) {
			size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

			UpdateBufferObject(Index, UpdateSize, (((char*)pUploadData) + UploadDataOffset), (void*)UploadDataOffset);

			UploadDataOffset += UpdateSize;
			UploadDataSize -= UpdateSize;
		}
	}

	return Index;
}

void CGraphics_Threaded::RecreateBufferObject(int BufferIndex, size_t UploadDataSize, void* pUploadData)
{
	CCommandBuffer::SCommand_RecreateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;

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
	else {
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
		while(UploadDataSize > 0) {
			size_t UpdateSize = (UploadDataSize > CMD_BUFFER_DATA_BUFFER_SIZE ? CMD_BUFFER_DATA_BUFFER_SIZE : UploadDataSize);

			UpdateBufferObject(BufferIndex, UpdateSize, (((char*)pUploadData) + UploadDataOffset), (void*)UploadDataOffset);

			UploadDataOffset += UpdateSize;
			UploadDataSize -= UpdateSize;
		}
	}
}

void CGraphics_Threaded::UpdateBufferObject(int BufferIndex, size_t UploadDataSize, void *pUploadData, void *pOffset)
{
	CCommandBuffer::SCommand_UpdateBufferObject Cmd;
	Cmd.m_BufferIndex = BufferIndex;
	Cmd.m_DataSize = UploadDataSize;
	Cmd.m_pOffset = pOffset;

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

	Cmd.m_Attributes = (SBufferContainerInfo::SAttribute*)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_Attributes == NULL)
		return -1;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Attributes = (SBufferContainerInfo::SAttribute*)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
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

	for(size_t i = 0; i < pContainerInfo->m_Attributes.size(); ++i)
		m_VertexArrayInfo[Index].m_AssociatedBufferObjectIndices.push_back(pContainerInfo->m_Attributes[i].m_VertBufferBindingIndex);

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
				for(size_t n = 0; n < m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.size(); ++n)
				{
					if(BufferObjectIndex == m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices[n])
						m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices[n] = -1;
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

	Cmd.m_Attributes = (SBufferContainerInfo::SAttribute*)AllocCommandBufferData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
	if(Cmd.m_Attributes == NULL)
		return;

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Attributes = (SBufferContainerInfo::SAttribute*)m_pCommandBuffer->AllocData(Cmd.m_AttrCount * sizeof(SBufferContainerInfo::SAttribute));
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
	for(size_t i = 0; i < pContainerInfo->m_Attributes.size(); ++i)
		m_VertexArrayInfo[ContainerIndex].m_AssociatedBufferObjectIndices.push_back(pContainerInfo->m_Attributes[i].m_VertBufferBindingIndex);
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

	if(g_Config.m_GfxBorderless) Flags |= IGraphicsBackend::INITFLAG_BORDERLESS;
	if(g_Config.m_GfxFullscreen) Flags |= IGraphicsBackend::INITFLAG_FULLSCREEN;
	if(g_Config.m_GfxVsync) Flags |= IGraphicsBackend::INITFLAG_VSYNC;
	if(g_Config.m_GfxResizable) Flags |= IGraphicsBackend::INITFLAG_RESIZABLE;

	int r = m_pBackend->Init("DDNet Client", &g_Config.m_GfxScreen, &g_Config.m_GfxScreenWidth, &g_Config.m_GfxScreenHeight, g_Config.m_GfxFsaaSamples, Flags, &m_DesktopScreenWidth, &m_DesktopScreenHeight, &m_ScreenWidth, &m_ScreenHeight, m_pStorage);
	m_UseOpenGL3_3 = m_pBackend->IsOpenGL3_3();
	return r;
}

int CGraphics_Threaded::InitWindow()
{
	if(IssueInit() == 0)
		return 0;

	// try disabling fsaa
	while(g_Config.m_GfxFsaaSamples)
	{
		g_Config.m_GfxFsaaSamples--;

		if(g_Config.m_GfxFsaaSamples)
			dbg_msg("gfx", "lowering FSAA to %d and trying again", g_Config.m_GfxFsaaSamples);
		else
			dbg_msg("gfx", "disabling FSAA and trying again");

		if(IssueInit() == 0)
			return 0;
	}
	
	// try using old opengl context
	if(g_Config.m_GfxOpenGL3)
	{
		g_Config.m_GfxOpenGL3 = 0;
		if(IssueInit() == 0)
		{
			g_Config.m_GfxOpenGL3 = 1;
			return 0;
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
	for(int i = 0; i < MAX_TEXTURES-1; i++)
		m_aTextureIndices[i] = i+1;
	m_aTextureIndices[MAX_TEXTURES-1] = -1;

	m_FirstFreeVertexArrayInfo = -1;
	m_FirstFreeBufferObjectIndex = -1;
	m_FirstFreeQuadContainer = -1;

	m_pBackend = CreateGraphicsBackend();
	if(InitWindow() != 0)
		return -1;

	// create command buffers
	for(int i = 0; i < NUM_CMDBUFFERS; i++)
		m_apCommandBuffers[i] = new CCommandBuffer(CMD_BUFFER_CMD_BUFFER_SIZE, CMD_BUFFER_DATA_BUFFER_SIZE);
	m_pCommandBuffer = m_apCommandBuffers[0];

	// create null texture, will get id=0
	static const unsigned char s_aNullTextureData[] = {
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
	};

	m_InvalidTexture = LoadTextureRaw(4,4,CImageInfo::FORMAT_RGBA,s_aNullTextureData,CImageInfo::FORMAT_RGBA,TEXLOAD_NORESAMPLE);
	return 0;
}

void CGraphics_Threaded::Shutdown()
{
	// shutdown the backend
	m_pBackend->Shutdown();
	delete m_pBackend;
	m_pBackend = 0x0;

	// delete the command buffers
	for(int i = 0; i < NUM_CMDBUFFERS; i++)
		delete m_apCommandBuffers[i];
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

void CGraphics_Threaded::Resize(int w, int h)
{
	if(m_ScreenWidth == w && m_ScreenHeight == h)
		return;

	if(h > 4*w/5)
		h = 4*w/5;
	if(w > 21*h/9)
		w = 21*h/9;

	m_ScreenWidth = w;
	m_ScreenHeight = h;

	CCommandBuffer::SCommand_Resize Cmd;
	Cmd.m_Width = w;
	Cmd.m_Height = h;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the command buffer
	KickCommandBuffer();
	WaitForIdle();
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
	str_format(m_aScreenshotName, sizeof(m_aScreenshotName), "screenshots/%s_%s.png", pFilename?pFilename:"screenshot", aDate);
	m_DoScreenshot = true;
}

void CGraphics_Threaded::TakeCustomScreenshot(const char *pFilename)
{
	str_copy(m_aScreenshotName, pFilename, sizeof(m_aScreenshotName));
	m_DoScreenshot = true;
}

void CGraphics_Threaded::Swap()
{
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
void CGraphics_Threaded::InsertSignal(semaphore *pSemaphore)
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

int CGraphics_Threaded::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	if(g_Config.m_GfxDisplayAllModes)
	{
		int Count = sizeof(g_aFakeModes)/sizeof(CVideoMode);
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
