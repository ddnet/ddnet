/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/math.h>
#include <base/tl/threading.h>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#endif

#include <base/system.h>
#include <engine/external/pnglite/pnglite.h>

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

void CGraphics_Threaded::FlushVertices()
{
	if(m_NumVertices == 0)
		return;
	
	size_t VertSize = 0;
	if(!m_UseOpenGL3_3) VertSize = sizeof(CCommandBuffer::SVertexOld);
	else VertSize = sizeof(CCommandBuffer::SVertex);

	int NumVerts = m_NumVertices;
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

	Cmd.m_pVertices = (CCommandBuffer::SVertexBase *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
	if(Cmd.m_pVertices == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices = (CCommandBuffer::SVertexBase *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
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

		Cmd.m_pVertices = (CCommandBuffer::SVertexBase *)m_pCommandBuffer->AllocData(VertSize*NumVerts);
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

	mem_copy(Cmd.m_pVertices, m_pVertices, VertSize*NumVerts);
}

void CGraphics_Threaded::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= MAX_VERTICES)
		FlushVertices();
}

void CGraphics_Threaded::Rotate(const CCommandBuffer::SPoint &rCenter, CCommandBuffer::SVertexBase *pPoints, int NumPoints)
{
	float c = cosf(m_Rotation);
	float s = sinf(m_Rotation);
	float x, y;
	int i;

	
	if(m_UseOpenGL3_3)
	{
		CCommandBuffer::SVertex *pVertices = (CCommandBuffer::SVertex*) pPoints;
		for(i = 0; i < NumPoints; i++)
		{
			x = pVertices[i].m_Pos.x - rCenter.x;
			y = pVertices[i].m_Pos.y - rCenter.y;
			pVertices[i].m_Pos.x = x * c - y * s + rCenter.x;
			pVertices[i].m_Pos.y = x * s + y * c + rCenter.y;
		}
	}
	else
	{
		CCommandBuffer::SVertexOld *pVertices = (CCommandBuffer::SVertexOld*) pPoints;
		for(i = 0; i < NumPoints; i++)
		{
			x = pVertices[i].m_Pos.x - rCenter.x;
			y = pVertices[i].m_Pos.y - rCenter.y;
			pVertices[i].m_Pos.x = x * c - y * s + rCenter.x;
			pVertices[i].m_Pos.y = x * s + y * c + rCenter.y;
		}
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
		GetVertex(m_NumVertices + 2*i)->m_Pos.x = pArray[i].m_X0;
		GetVertex(m_NumVertices + 2*i)->m_Pos.y = pArray[i].m_Y0;
		GetVertex(m_NumVertices + 2*i)->m_Tex = m_aTexture[0];
		SetColor(GetVertex(m_NumVertices + 2*i), 0);

		GetVertex(m_NumVertices + 2*i + 1)->m_Pos.x = pArray[i].m_X1;
		GetVertex(m_NumVertices + 2*i + 1)->m_Pos.y = pArray[i].m_Y1;
		GetVertex(m_NumVertices + 2*i + 1)->m_Tex = m_aTexture[1];
		SetColor(GetVertex(m_NumVertices + 2*i + 1), 1);
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
		if (StoreFormat == CImageInfo::FORMAT_AUTO)
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

	QuadsSetSubset(0,0,1,1);
	QuadsSetRotation(0);
	SetColor(1,1,1,1);
}

void CGraphics_Threaded::QuadsEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsSetRotation(float Angle)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetRotation without begin");
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
		if(m_UseOpenGL3_3)
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
		else
		{				
			m_aColorOld[pArray[i].m_Index].r = pArray[i].m_R;
			m_aColorOld[pArray[i].m_Index].g = pArray[i].m_G;
			m_aColorOld[pArray[i].m_Index].b = pArray[i].m_B;
			m_aColorOld[pArray[i].m_Index].a = pArray[i].m_A;
		}
	}
}

void CGraphics_Threaded::SetColor(float r, float g, float b, float a)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColor without begin");
	CColorVertex Array[4] = {
		CColorVertex(0, r, g, b, a),
		CColorVertex(1, r, g, b, a),
		CColorVertex(2, r, g, b, a),
		CColorVertex(3, r, g, b, a)};
	SetColorVertex(Array, 4);
}

void CGraphics_Threaded::SetColor(CCommandBuffer::SVertexBase *pVertex, int ColorIndex)
{
	if(m_UseOpenGL3_3)
	{
		CCommandBuffer::SVertex *pVert = (CCommandBuffer::SVertex*)pVertex;
		pVert->m_Color.r = m_aColor[ColorIndex].r;
		pVert->m_Color.g = m_aColor[ColorIndex].g;
		pVert->m_Color.b = m_aColor[ColorIndex].b;
		pVert->m_Color.a = m_aColor[ColorIndex].a;
	} 
	else 
	{
		CCommandBuffer::SVertexOld *pVert = (CCommandBuffer::SVertexOld*)pVertex;
		pVert->m_Color.r = m_aColorOld[ColorIndex].r;
		pVert->m_Color.g = m_aColorOld[ColorIndex].g;
		pVert->m_Color.b = m_aColorOld[ColorIndex].b;
		pVert->m_Color.a = m_aColorOld[ColorIndex].a;
	}
}

CCommandBuffer::SVertexBase *CGraphics_Threaded::GetVertex(int Index)
{
	if(!m_UseOpenGL3_3)
		return &((CCommandBuffer::SVertexOld*)m_pVertices)[Index];
	else
		return &((CCommandBuffer::SVertex*)m_pVertices)[Index];
}

void CGraphics_Threaded::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetSubset without begin");

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
			GetVertex(m_NumVertices + 6*i)->m_Pos.x = pArray[i].m_X;
			GetVertex(m_NumVertices + 6*i)->m_Pos.y = pArray[i].m_Y;
			GetVertex(m_NumVertices + 6*i)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 6*i), 0);

			GetVertex(m_NumVertices + 6*i + 1)->m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			GetVertex(m_NumVertices + 6*i + 1)->m_Pos.y = pArray[i].m_Y;
			GetVertex(m_NumVertices + 6*i + 1)->m_Tex = m_aTexture[1];
			SetColor(GetVertex(m_NumVertices + 6*i + 1), 1);

			GetVertex(m_NumVertices + 6*i + 2)->m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			GetVertex(m_NumVertices + 6*i + 2)->m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			GetVertex(m_NumVertices + 6*i + 2)->m_Tex = m_aTexture[2];
			SetColor(GetVertex(m_NumVertices + 6*i + 2), 2);

			// second triangle
			GetVertex(m_NumVertices + 6*i + 3)->m_Pos.x = pArray[i].m_X;
			GetVertex(m_NumVertices + 6*i + 3)->m_Pos.y = pArray[i].m_Y;
			GetVertex(m_NumVertices + 6*i + 3)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 6*i + 3), 0);

			GetVertex(m_NumVertices + 6*i + 4)->m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			GetVertex(m_NumVertices + 6*i + 4)->m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			GetVertex(m_NumVertices + 6*i + 4)->m_Tex = m_aTexture[2];
			SetColor(GetVertex(m_NumVertices + 6*i + 4), 2);

			GetVertex(m_NumVertices + 6*i + 5)->m_Pos.x = pArray[i].m_X;
			GetVertex(m_NumVertices + 6*i + 5)->m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			GetVertex(m_NumVertices + 6*i + 5)->m_Tex = m_aTexture[3];
			SetColor(GetVertex(m_NumVertices + 6*i + 5), 3);

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, GetVertex(m_NumVertices + 6*i), 6);
			}
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			GetVertex(m_NumVertices + 4*i)->m_Pos.x = pArray[i].m_X;
			GetVertex(m_NumVertices + 4*i)->m_Pos.y = pArray[i].m_Y;
			GetVertex(m_NumVertices + 4*i)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 4*i), 0);

			GetVertex(m_NumVertices + 4*i + 1)->m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			GetVertex(m_NumVertices + 4*i + 1)->m_Pos.y = pArray[i].m_Y;
			GetVertex(m_NumVertices + 4*i + 1)->m_Tex = m_aTexture[1];
			SetColor(GetVertex(m_NumVertices + 4*i + 1), 1);

			GetVertex(m_NumVertices + 4*i + 2)->m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			GetVertex(m_NumVertices + 4*i + 2)->m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			GetVertex(m_NumVertices + 4*i + 2)->m_Tex = m_aTexture[2];
			SetColor(GetVertex(m_NumVertices + 4*i + 2), 2);

			GetVertex(m_NumVertices + 4*i + 3)->m_Pos.x = pArray[i].m_X;
			GetVertex(m_NumVertices + 4*i + 3)->m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			GetVertex(m_NumVertices + 4*i + 3)->m_Tex = m_aTexture[3];
			SetColor(GetVertex(m_NumVertices + 4*i + 3), 3);

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, GetVertex(m_NumVertices + 4*i), 4);
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
			GetVertex(m_NumVertices + 6*i)->m_Pos.x = pArray[i].m_X0;
			GetVertex(m_NumVertices + 6*i)->m_Pos.y = pArray[i].m_Y0;
			GetVertex(m_NumVertices + 6*i)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 6*i), 0);

			GetVertex(m_NumVertices + 6*i + 1)->m_Pos.x = pArray[i].m_X1;
			GetVertex(m_NumVertices + 6*i + 1)->m_Pos.y = pArray[i].m_Y1;
			GetVertex(m_NumVertices + 6*i + 1)->m_Tex = m_aTexture[1];
			SetColor(GetVertex(m_NumVertices + 6*i + 1), 1);

			GetVertex(m_NumVertices + 6*i + 2)->m_Pos.x = pArray[i].m_X3;
			GetVertex(m_NumVertices + 6*i + 2)->m_Pos.y = pArray[i].m_Y3;
			GetVertex(m_NumVertices + 6*i + 2)->m_Tex = m_aTexture[3];
			SetColor(GetVertex(m_NumVertices + 6*i + 2), 3);

			GetVertex(m_NumVertices + 6*i + 3)->m_Pos.x = pArray[i].m_X0;
			GetVertex(m_NumVertices + 6*i + 3)->m_Pos.y = pArray[i].m_Y0;
			GetVertex(m_NumVertices + 6*i + 3)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 6*i + 3), 0);

			GetVertex(m_NumVertices + 6*i + 4)->m_Pos.x = pArray[i].m_X3;
			GetVertex(m_NumVertices + 6*i + 4)->m_Pos.y = pArray[i].m_Y3;
			GetVertex(m_NumVertices + 6*i + 4)->m_Tex = m_aTexture[3];
			SetColor(GetVertex(m_NumVertices + 6*i + 4), 3);

			GetVertex(m_NumVertices + 6*i + 5)->m_Pos.x = pArray[i].m_X2;
			GetVertex(m_NumVertices + 6*i + 5)->m_Pos.y = pArray[i].m_Y2;
			GetVertex(m_NumVertices + 6*i + 5)->m_Tex = m_aTexture[2];
			SetColor(GetVertex(m_NumVertices + 6*i + 5), 2);
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			GetVertex(m_NumVertices + 4*i)->m_Pos.x = pArray[i].m_X0;
			GetVertex(m_NumVertices + 4*i)->m_Pos.y = pArray[i].m_Y0;
			GetVertex(m_NumVertices + 4*i)->m_Tex = m_aTexture[0];
			SetColor(GetVertex(m_NumVertices + 4*i), 0);

			GetVertex(m_NumVertices + 4*i + 1)->m_Pos.x = pArray[i].m_X1;
			GetVertex(m_NumVertices + 4*i + 1)->m_Pos.y = pArray[i].m_Y1;
			GetVertex(m_NumVertices + 4*i + 1)->m_Tex = m_aTexture[1];
			SetColor(GetVertex(m_NumVertices + 4*i + 1), 1);

			GetVertex(m_NumVertices + 4*i + 2)->m_Pos.x = pArray[i].m_X3;
			GetVertex(m_NumVertices + 4*i + 2)->m_Pos.y = pArray[i].m_Y3;
			GetVertex(m_NumVertices + 4*i + 2)->m_Tex = m_aTexture[3];
			SetColor(GetVertex(m_NumVertices + 4*i + 2), 3);

			GetVertex(m_NumVertices + 4*i + 3)->m_Pos.x = pArray[i].m_X2;
			GetVertex(m_NumVertices + 4*i + 3)->m_Pos.y = pArray[i].m_Y2;
			GetVertex(m_NumVertices + 4*i + 3)->m_Tex = m_aTexture[2];
			SetColor(GetVertex(m_NumVertices + 4*i + 3), 2);
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

void mem_copy_special(void *pDest, void *pSource, size_t Size, size_t Count, size_t Steps)
{
	size_t CurStep = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		mem_copy(((char*)pDest) + CurStep + i * Size, ((char*)pSource) + i * Size, Size);
		CurStep += Steps;
	}
}

void CGraphics_Threaded::DrawVisualObject(int VisualObjectIDX, float *pColor, char** pOffsets, unsigned int *IndicedVertexDrawNum, size_t NumIndicesOffet)
{
	if(NumIndicesOffet == 0) return;
	
	//add the VertexArrays and draw
	CCommandBuffer::SCommand_RenderVertexArray Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndicesDrawNum = NumIndicesOffet;
	Cmd.m_VisualObjectIDX = m_VertexArrayIndices[VisualObjectIDX];
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

void CGraphics_Threaded::DrawBorderTile(int VisualObjectIDX, float *pColor, char *pOffset, float *Offset, float *Dir, int JumpIndex, unsigned int DrawNum)
{
	if(DrawNum == 0) return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTile Cmd;
	Cmd.m_State = m_State;
	Cmd.m_DrawNum = DrawNum;
	Cmd.m_VisualObjectIDX = m_VertexArrayIndices[VisualObjectIDX];
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

void CGraphics_Threaded::DrawBorderTileLine(int VisualObjectIDX, float *pColor, char *pOffset, float *Dir, unsigned int IndexDrawNum, unsigned int RedrawNum)
{
	if(IndexDrawNum == 0 || RedrawNum == 0) return;
	// Draw a border tile a lot of times
	CCommandBuffer::SCommand_RenderBorderTileLine Cmd;
	Cmd.m_State = m_State;
	Cmd.m_IndexDrawNum = IndexDrawNum;
	Cmd.m_DrawNum = RedrawNum;
	Cmd.m_VisualObjectIDX = m_VertexArrayIndices[VisualObjectIDX];
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

void CGraphics_Threaded::DestroyVisual(int VisualObjectIDX)
{
	if (VisualObjectIDX == -1) return;
	CCommandBuffer::SCommand_DestroyVisual Cmd;
	Cmd.m_VisualObjectIDX = m_VertexArrayIndices[VisualObjectIDX];
	
	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for destroy all visuals command");
			return;
		}
	}
	
	//also clear the vert array index
	m_VertexArrayIndices[VisualObjectIDX] = m_FirstFreeVertexArrayIndex;
	m_FirstFreeVertexArrayIndex = VisualObjectIDX;
}

int CGraphics_Threaded::CreateVisualObjects(float *pVertices, unsigned char *pTexCoords, int NumTiles, unsigned int NumIndicesRequired)
{
	if(!pVertices) return -1;
	
	//first create an index
	int index = -1;
	if(m_FirstFreeVertexArrayIndex == -1)
	{
		index = m_VertexArrayIndices.size();
		m_VertexArrayIndices.push_back(index);
	}
	else
	{
		index = m_FirstFreeVertexArrayIndex;
		m_FirstFreeVertexArrayIndex = m_VertexArrayIndices[index];
		m_VertexArrayIndices[index] = index;
	}
	
	//upload the vertex buffer first
	//the size of the cmd data buffer is 2MB -- we create 4 vertices of each 2 floats plus 2 shorts(2*unsigned char each) if TexCoordinates are used
	char AddTexture = (pTexCoords == NULL ? 0 : 1);
	int AddTextureSize = (pTexCoords == NULL ? 0 : (sizeof(unsigned char) * 2 * 2 * 4));
	
	int MaxTileNumUpload = (1024*1024*2) / (sizeof(float) * 4 * 2 + AddTextureSize);
	
	int RealTileNum = NumTiles;
	if(NumTiles > MaxTileNumUpload) RealTileNum = MaxTileNumUpload;

	CCommandBuffer::SCommand_CreateVertexBufferObject Cmd;
	Cmd.m_IsTextured = pTexCoords != NULL;
	Cmd.m_VisualObjectIDX = index;
	int NumVerts = (Cmd.m_NumVertices = RealTileNum * 4 * 2); //number of vertices to upload -- same value for texcoords(if used)
	
	Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
	if(Cmd.m_Elements == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
		if(Cmd.m_Elements == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return -1;
		}
	}

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();
		
		Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
		if(Cmd.m_Elements == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return -1;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for create vertex buffer command");
			return -1;
		}
	}
	
	mem_copy_special(Cmd.m_Elements, pVertices, sizeof(float) * 2, RealTileNum * 4, (AddTexture) * sizeof(unsigned char) * 2 * 2);
	
	if(pTexCoords)
	{
		mem_copy_special(((char*)Cmd.m_Elements) + sizeof(float) * 2, pTexCoords, sizeof(unsigned char) * 2 * 2, RealTileNum * 4, (AddTexture) * sizeof(float) * 2);
	}
		
	if(NumTiles > MaxTileNumUpload)
	{
		pVertices += NumVerts;
		if(pTexCoords) pTexCoords += NumVerts*2;
		AppendAllVertices(pVertices, pTexCoords, NumTiles - MaxTileNumUpload, index);
	}
		
	CCommandBuffer::SCommand_CreateVertexArrayObject VertArrayCmd;
	VertArrayCmd.m_VisualObjectIDX = index;
	VertArrayCmd.m_RequiredIndicesCount = NumIndicesRequired;
	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(VertArrayCmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();
		
		if(!m_pCommandBuffer->AddCommand(VertArrayCmd))
		{
			dbg_msg("graphics", "failed to allocate memory for create vertex array command");
			return -1;
		}
	}
	
	//make sure we uploaded everything
	KickCommandBuffer();
	
	return index;
}

void CGraphics_Threaded::AppendAllVertices(float *pVertices, unsigned char *pTexCoords, int NumTiles, int VisualObjectIDX)
{
	//the size of the cmd data buffer is 2MB -- we create 4 vertices of each 2 floats plus 2 shorts(2*unsigned char each) if TexCoordinates are used
	char AddTexture = (pTexCoords == NULL ? 0 : 1);
	int AddTextureSize = (pTexCoords == NULL ? 0 : (sizeof(unsigned char) * 2 * 2 * 4));
	
	int MaxTileNumUpload = (1024*1024*2) / (sizeof(float) * 4 * 2 + AddTextureSize);
	
	int RealTileNum = NumTiles;
	if(NumTiles > MaxTileNumUpload) RealTileNum = MaxTileNumUpload;

	CCommandBuffer::SCommand_AppendVertexBufferObject Cmd;
	int NumVerts = (Cmd.m_NumVertices = RealTileNum * 4 * 2); //number of vertices to upload -- same value for texcoords(if used)
	Cmd.m_VisualObjectIDX = VisualObjectIDX;

	Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
	if(Cmd.m_Elements == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
		if(Cmd.m_Elements == 0x0)
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
		
		Cmd.m_Elements = m_pCommandBuffer->AllocData(RealTileNum * (sizeof(float) * 4 * 2 + AddTextureSize));
		if(Cmd.m_Elements == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for create vertex buffer command");
			return;
		}
	}
	
	mem_copy_special(Cmd.m_Elements, pVertices, sizeof(float) * 2, RealTileNum * 4, (AddTexture) * sizeof(unsigned char) * 2 * 2);
	
	if(pTexCoords)
	{
		mem_copy_special(((char*)Cmd.m_Elements) + sizeof(float) * 2, pTexCoords, sizeof(unsigned char) * 2 * 2, RealTileNum * 4, (AddTexture) * sizeof(float) * 2);
	}
	
	if(NumTiles > RealTileNum)
	{
		pVertices += NumVerts;
		if(pTexCoords) pTexCoords += NumVerts*2;
		AppendAllVertices(pVertices, pTexCoords, NumTiles - RealTileNum, VisualObjectIDX);
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
	
	m_FirstFreeVertexArrayIndex = -1;

	m_pBackend = CreateGraphicsBackend();
	if(InitWindow() != 0)
		return -1;
	
	if(m_UseOpenGL3_3)
		m_pVertices = m_aVertices;
	else
		m_pVertices = m_aVerticesOld;

	// create command buffers
	for(int i = 0; i < NUM_CMDBUFFERS; i++)
		m_apCommandBuffers[i] = new CCommandBuffer(256*1024, 2*1024*1024);
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
