#include "backend_null.h"

#include <engine/client/backend_sdl.h>

#if defined(CONF_PLATFORM_EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif

#if defined(__WIIU__)
#include <SDL.h>
#include <base/log.h>
#endif

ERunCommandReturnTypes CCommandProcessorFragment_Null::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandProcessorFragment_Null::CMD_INIT:
		Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXTURE_CREATE:
		Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURES_CREATE:
		Cmd_TextTextures_Create(static_cast<const CCommandBuffer::SCommand_TextTextures_Create *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURE_UPDATE:
		Cmd_TextTexture_Update(static_cast<const CCommandBuffer::SCommand_TextTexture_Update *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_SWAP:
		Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand));
		break;
#if defined(__WIIU__)
	case CCommandBuffer::CMD_TEXTURE_DESTROY:
		Cmd_Texture_Destroy(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_TEXT_TEXTURES_DESTROY:
		Cmd_TextTextures_Destroy(static_cast<const CCommandBuffer::SCommand_TextTextures_Destroy *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_CLEAR:
		Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_RENDER:
		Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand));
		break;
	case CCommandBuffer::CMD_UPDATE_VIEWPORT:
		Cmd_Update_Viewport(static_cast<const CCommandBuffer::SCommand_Update_Viewport *>(pBaseCommand));
		break;
#endif
	}
	return ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED;
}

bool CCommandProcessorFragment_Null::Cmd_Init(const SCommand_Init *pCommand)
{
#if defined(__WIIU__)
	m_Width = pCommand->m_Width;
	m_Height = pCommand->m_Height;
	
	m_pRenderer = SDL_CreateRenderer(pCommand->m_pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!m_pRenderer)
	{
		log_error("gfx", "Failed to create SDL2 renderer: %s", SDL_GetError());
		return true; // error occurred
	}
	SDL_RenderSetLogicalSize(m_pRenderer, m_Width, m_Height);
	log_info("gfx", "Created SDL2 renderer for Wii U");
#endif

	pCommand->m_pCapabilities->m_TileBuffering = false;
	pCommand->m_pCapabilities->m_QuadBuffering = false;
	pCommand->m_pCapabilities->m_TextBuffering = false;
	pCommand->m_pCapabilities->m_QuadContainerBuffering = false;

	pCommand->m_pCapabilities->m_MipMapping = false;
	pCommand->m_pCapabilities->m_NPOTTextures = true;
	pCommand->m_pCapabilities->m_3DTextures = false;
	pCommand->m_pCapabilities->m_2DArrayTextures = false;
	pCommand->m_pCapabilities->m_2DArrayTexturesAsExtension = false;
	pCommand->m_pCapabilities->m_ShaderSupport = false;

	pCommand->m_pCapabilities->m_TrianglesAsQuads = true;

	pCommand->m_pCapabilities->m_ContextMajor = 0;
	pCommand->m_pCapabilities->m_ContextMinor = 0;
	pCommand->m_pCapabilities->m_ContextPatch = 0;
	return false;
}

void CCommandProcessorFragment_Null::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
#if defined(__WIIU__)
	if(m_pRenderer)
	{
		SDL_Surface *pSurface = SDL_CreateRGBSurfaceWithFormatFrom(
			pCommand->m_pData,
			pCommand->m_Width,
			pCommand->m_Height,
			32,
			pCommand->m_Width * 4,
			SDL_PIXELFORMAT_RGBA32
		);
		if(pSurface)
		{
			SDL_Texture *pTexture = SDL_CreateTextureFromSurface(m_pRenderer, pSurface);
			SDL_FreeSurface(pSurface);
			if(pCommand->m_Slot >= (int)m_vpTextures.size())
			{
				m_vpTextures.resize(pCommand->m_Slot + 1, nullptr);
			}
			if(m_vpTextures[pCommand->m_Slot])
			{
				SDL_DestroyTexture(m_vpTextures[pCommand->m_Slot]);
			}
			m_vpTextures[pCommand->m_Slot] = pTexture;
		}
	}
#endif
	free(pCommand->m_pData);
}

void CCommandProcessorFragment_Null::Cmd_TextTextures_Create(const CCommandBuffer::SCommand_TextTextures_Create *pCommand)
{
#if defined(__WIIU__)
	if(m_pRenderer)
	{
		uint8_t *pRGBA = (uint8_t *)malloc(pCommand->m_Width * pCommand->m_Height * 4);
		for(size_t i = 0; i < pCommand->m_Width * pCommand->m_Height; ++i)
		{
			pRGBA[4 * i] = 255;
			pRGBA[4 * i + 1] = 255;
			pRGBA[4 * i + 2] = 255;
			pRGBA[4 * i + 3] = pCommand->m_pTextData[i];
		}
		SDL_Surface *pSurface = SDL_CreateRGBSurfaceWithFormatFrom(
			pRGBA, pCommand->m_Width, pCommand->m_Height, 32, pCommand->m_Width * 4, SDL_PIXELFORMAT_RGBA32
		);
		if(pSurface)
		{
			SDL_Texture *pTexture = SDL_CreateTextureFromSurface(m_pRenderer, pSurface);
			SDL_FreeSurface(pSurface);
			if(pCommand->m_Slot >= (int)m_vpTextures.size())
				m_vpTextures.resize(pCommand->m_Slot + 1, nullptr);
			if(m_vpTextures[pCommand->m_Slot])
				SDL_DestroyTexture(m_vpTextures[pCommand->m_Slot]);
			m_vpTextures[pCommand->m_Slot] = pTexture;
		}
		free(pRGBA);

		uint8_t *pOutlineRGBA = (uint8_t *)malloc(pCommand->m_Width * pCommand->m_Height * 4);
		for(size_t i = 0; i < pCommand->m_Width * pCommand->m_Height; ++i)
		{
			pOutlineRGBA[4 * i] = 255;
			pOutlineRGBA[4 * i + 1] = 255;
			pOutlineRGBA[4 * i + 2] = 255;
			pOutlineRGBA[4 * i + 3] = pCommand->m_pTextOutlineData[i];
		}
		pSurface = SDL_CreateRGBSurfaceWithFormatFrom(
			pOutlineRGBA, pCommand->m_Width, pCommand->m_Height, 32, pCommand->m_Width * 4, SDL_PIXELFORMAT_RGBA32
		);
		if(pSurface)
		{
			SDL_Texture *pTexture = SDL_CreateTextureFromSurface(m_pRenderer, pSurface);
			SDL_FreeSurface(pSurface);
			if(pCommand->m_SlotOutline >= (int)m_vpTextures.size())
				m_vpTextures.resize(pCommand->m_SlotOutline + 1, nullptr);
			if(m_vpTextures[pCommand->m_SlotOutline])
				SDL_DestroyTexture(m_vpTextures[pCommand->m_SlotOutline]);
			m_vpTextures[pCommand->m_SlotOutline] = pTexture;
		}
		free(pOutlineRGBA);
	}
#endif
	free(pCommand->m_pTextData);
	free(pCommand->m_pTextOutlineData);
}

void CCommandProcessorFragment_Null::Cmd_TextTexture_Update(const CCommandBuffer::SCommand_TextTexture_Update *pCommand)
{
#if defined(__WIIU__)
	if(m_pRenderer && pCommand->m_Slot >= 0 && pCommand->m_Slot < (int)m_vpTextures.size())
	{
		SDL_Texture *pTexture = m_vpTextures[pCommand->m_Slot];
		if(pTexture)
		{
			uint8_t *pRGBA = (uint8_t *)malloc(pCommand->m_Width * pCommand->m_Height * 4);
			for(size_t i = 0; i < pCommand->m_Width * pCommand->m_Height; ++i)
			{
				pRGBA[4 * i] = 255;
				pRGBA[4 * i + 1] = 255;
				pRGBA[4 * i + 2] = 255;
				pRGBA[4 * i + 3] = pCommand->m_pData[i];
			}
			SDL_Rect rect = {(int)pCommand->m_X, (int)pCommand->m_Y, (int)pCommand->m_Width, (int)pCommand->m_Height};
			SDL_UpdateTexture(pTexture, &rect, pRGBA, pCommand->m_Width * 4);
			free(pRGBA);
		}
	}
#endif
	free(pCommand->m_pData);
}

void CCommandProcessorFragment_Null::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
#if defined(__WIIU__)
	if(m_pRenderer)
	{
		SDL_RenderPresent(m_pRenderer);
	}
#endif
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	emscripten_sleep(0);
#endif
}

#if defined(__WIIU__)
void CCommandProcessorFragment_Null::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	if(pCommand->m_Slot >= 0 && pCommand->m_Slot < (int)m_vpTextures.size())
	{
		if(m_vpTextures[pCommand->m_Slot])
		{
			SDL_DestroyTexture(m_vpTextures[pCommand->m_Slot]);
			m_vpTextures[pCommand->m_Slot] = nullptr;
		}
	}
}

void CCommandProcessorFragment_Null::Cmd_TextTextures_Destroy(const CCommandBuffer::SCommand_TextTextures_Destroy *pCommand)
{
	if(pCommand->m_Slot >= 0 && pCommand->m_Slot < (int)m_vpTextures.size())
	{
		if(m_vpTextures[pCommand->m_Slot])
		{
			SDL_DestroyTexture(m_vpTextures[pCommand->m_Slot]);
			m_vpTextures[pCommand->m_Slot] = nullptr;
		}
	}
	if(pCommand->m_SlotOutline >= 0 && pCommand->m_SlotOutline < (int)m_vpTextures.size())
	{
		if(m_vpTextures[pCommand->m_SlotOutline])
		{
			SDL_DestroyTexture(m_vpTextures[pCommand->m_SlotOutline]);
			m_vpTextures[pCommand->m_SlotOutline] = nullptr;
		}
	}
}

void CCommandProcessorFragment_Null::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	if(m_pRenderer)
	{
		SDL_SetRenderDrawColor(
			m_pRenderer,
			(uint8_t)(pCommand->m_Color.r * 255),
			(uint8_t)(pCommand->m_Color.g * 255),
			(uint8_t)(pCommand->m_Color.b * 255),
			255
		);
		SDL_RenderClear(m_pRenderer);
	}
}

void CCommandProcessorFragment_Null::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	if(!m_pRenderer)
		return;

	if(pCommand->m_State.m_ClipEnable)
	{
		SDL_Rect rect;
		rect.x = pCommand->m_State.m_ClipX;
		rect.y = m_Height - (pCommand->m_State.m_ClipH + pCommand->m_State.m_ClipY);
		rect.w = pCommand->m_State.m_ClipW;
		rect.h = pCommand->m_State.m_ClipH;
		SDL_RenderSetClipRect(m_pRenderer, &rect);
	}
	else
	{
		SDL_RenderSetClipRect(m_pRenderer, nullptr);
	}

	SDL_Texture *pTexture = nullptr;
	if(pCommand->m_State.m_Texture >= 0 && pCommand->m_State.m_Texture < (int)m_vpTextures.size())
	{
		pTexture = m_vpTextures[pCommand->m_State.m_Texture];
	}

	SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND;
	if(pCommand->m_State.m_BlendMode == EBlendMode::NONE)
		blendMode = SDL_BLENDMODE_NONE;
	else if(pCommand->m_State.m_BlendMode == EBlendMode::ALPHA)
		blendMode = SDL_BLENDMODE_BLEND;
	else if(pCommand->m_State.m_BlendMode == EBlendMode::ADDITIVE)
		blendMode = SDL_BLENDMODE_ADD;

	if(pTexture)
	{
		SDL_SetTextureBlendMode(pTexture, blendMode);
	}
	else
	{
		SDL_SetRenderDrawBlendMode(m_pRenderer, blendMode);
	}

	float tlX = pCommand->m_State.m_ScreenTL.x;
	float tlY = pCommand->m_State.m_ScreenTL.y;
	float brX = pCommand->m_State.m_ScreenBR.x;
	float brY = pCommand->m_State.m_ScreenBR.y;
	float spanX = brX - tlX;
	float spanY = brY - tlY;
	if(spanX == 0.0f) spanX = 1.0f;
	if(spanY == 0.0f) spanY = 1.0f;

	int num_vertices = 0;
	if(pCommand->m_PrimType == EPrimitiveType::QUADS)
	{
		num_vertices = pCommand->m_PrimCount * 4;
		m_vVertices.resize(num_vertices);
		for(int i = 0; i < num_vertices; ++i)
		{
			float px = pCommand->m_pVertices[i].m_Pos.x;
			float py = pCommand->m_pVertices[i].m_Pos.y;
			m_vVertices[i].position.x = ((px - tlX) / spanX) * m_Width;
			m_vVertices[i].position.y = ((py - tlY) / spanY) * m_Height;
			m_vVertices[i].color.r = pCommand->m_pVertices[i].m_Color.r;
			m_vVertices[i].color.g = pCommand->m_pVertices[i].m_Color.g;
			m_vVertices[i].color.b = pCommand->m_pVertices[i].m_Color.b;
			m_vVertices[i].color.a = pCommand->m_pVertices[i].m_Color.a;
			if(pTexture)
			{
				m_vVertices[i].tex_coord.x = pCommand->m_pVertices[i].m_Tex.x;
				m_vVertices[i].tex_coord.y = pCommand->m_pVertices[i].m_Tex.y;
			}
			else
			{
				m_vVertices[i].tex_coord.x = 0.0f;
				m_vVertices[i].tex_coord.y = 0.0f;
			}
		}

		m_vIndices.resize(pCommand->m_PrimCount * 6);
		for(unsigned int i = 0; i < pCommand->m_PrimCount; ++i)
		{
			m_vIndices[6 * i] = 4 * i;
			m_vIndices[6 * i + 1] = 4 * i + 1;
			m_vIndices[6 * i + 2] = 4 * i + 2;
			m_vIndices[6 * i + 3] = 4 * i;
			m_vIndices[6 * i + 4] = 4 * i + 2;
			m_vIndices[6 * i + 5] = 4 * i + 3;
		}

		SDL_RenderGeometry(m_pRenderer, pTexture, m_vVertices.data(), num_vertices, m_vIndices.data(), m_vIndices.size());
	}
	else if(pCommand->m_PrimType == EPrimitiveType::TRIANGLES)
	{
		num_vertices = pCommand->m_PrimCount * 3;
		m_vVertices.resize(num_vertices);
		for(int i = 0; i < num_vertices; ++i)
		{
			float px = pCommand->m_pVertices[i].m_Pos.x;
			float py = pCommand->m_pVertices[i].m_Pos.y;
			m_vVertices[i].position.x = ((px - tlX) / spanX) * m_Width;
			m_vVertices[i].position.y = ((py - tlY) / spanY) * m_Height;
			m_vVertices[i].color.r = pCommand->m_pVertices[i].m_Color.r;
			m_vVertices[i].color.g = pCommand->m_pVertices[i].m_Color.g;
			m_vVertices[i].color.b = pCommand->m_pVertices[i].m_Color.b;
			m_vVertices[i].color.a = pCommand->m_pVertices[i].m_Color.a;
			if(pTexture)
			{
				m_vVertices[i].tex_coord.x = pCommand->m_pVertices[i].m_Tex.x;
				m_vVertices[i].tex_coord.y = pCommand->m_pVertices[i].m_Tex.y;
			}
			else
			{
				m_vVertices[i].tex_coord.x = 0.0f;
				m_vVertices[i].tex_coord.y = 0.0f;
			}
		}
		SDL_RenderGeometry(m_pRenderer, pTexture, m_vVertices.data(), num_vertices, nullptr, 0);
	}
	else if(pCommand->m_PrimType == EPrimitiveType::LINES)
	{
		for(unsigned int i = 0; i < pCommand->m_PrimCount; ++i)
		{
			SDL_SetRenderDrawColor(
				m_pRenderer,
				pCommand->m_pVertices[2 * i].m_Color.r,
				pCommand->m_pVertices[2 * i].m_Color.g,
				pCommand->m_pVertices[2 * i].m_Color.b,
				pCommand->m_pVertices[2 * i].m_Color.a
			);
			float px0 = pCommand->m_pVertices[2 * i].m_Pos.x;
			float py0 = pCommand->m_pVertices[2 * i].m_Pos.y;
			float px1 = pCommand->m_pVertices[2 * i + 1].m_Pos.x;
			float py1 = pCommand->m_pVertices[2 * i + 1].m_Pos.y;
			float sx0 = ((px0 - tlX) / spanX) * m_Width;
			float sy0 = ((py0 - tlY) / spanY) * m_Height;
			float sx1 = ((px1 - tlX) / spanX) * m_Width;
			float sy1 = ((py1 - tlY) / spanY) * m_Height;
			SDL_RenderDrawLineF(m_pRenderer, sx0, sy0, sx1, sy1);
		}
	}
}

void CCommandProcessorFragment_Null::Cmd_Update_Viewport(const CCommandBuffer::SCommand_Update_Viewport *pCommand)
{
	if(!m_pRenderer)
		return;

	m_Width = pCommand->m_Width;
	m_Height = pCommand->m_Height;
	
	SDL_RenderSetLogicalSize(m_pRenderer, m_Width, m_Height);
}
#endif
