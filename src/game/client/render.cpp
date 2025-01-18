/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <cmath>

#include <base/math.h>

#include "animstate.h"
#include "render.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

#include <game/mapitems.h>

static float gs_SpriteWScale;
static float gs_SpriteHScale;

void CRenderTools::Init(IGraphics *pGraphics, ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	m_TeeQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);

	// Feet
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	// Mirrored Feet
	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	Graphics()->QuadContainerUpload(m_TeeQuadContainerIndex);
}

void CRenderTools::SelectSprite(const CDataSprite *pSprite, int Flags) const
{
	int x = pSprite->m_X;
	int y = pSprite->m_Y;
	int w = pSprite->m_W;
	int h = pSprite->m_H;
	int cx = pSprite->m_pSet->m_Gridx;
	int cy = pSprite->m_pSet->m_Gridy;

	GetSpriteScaleImpl(w, h, gs_SpriteWScale, gs_SpriteHScale);

	float x1 = x / (float)cx + 0.5f / (float)(cx * 32);
	float x2 = (x + w) / (float)cx - 0.5f / (float)(cx * 32);
	float y1 = y / (float)cy + 0.5f / (float)(cy * 32);
	float y2 = (y + h) / (float)cy - 0.5f / (float)(cy * 32);

	if(Flags & SPRITE_FLAG_FLIP_Y)
		std::swap(y1, y2);

	if(Flags & SPRITE_FLAG_FLIP_X)
		std::swap(x1, x2);

	Graphics()->QuadsSetSubset(x1, y1, x2, y2);
}

void CRenderTools::SelectSprite(int Id, int Flags) const
{
	dbg_assert(Id >= 0 && Id < g_pData->m_NumSprites, "Id invalid");
	SelectSprite(&g_pData->m_aSprites[Id], Flags);
}

void CRenderTools::SelectSprite7(int Id, int Flags) const
{
	dbg_assert(Id >= 0 && Id < client_data7::g_pData->m_NumSprites, "Id invalid");
	SelectSprite(&client_data7::g_pData->m_aSprites[Id], Flags);
}

void CRenderTools::GetSpriteScale(const CDataSprite *pSprite, float &ScaleX, float &ScaleY) const
{
	int w = pSprite->m_W;
	int h = pSprite->m_H;
	GetSpriteScaleImpl(w, h, ScaleX, ScaleY);
}

void CRenderTools::GetSpriteScale(int Id, float &ScaleX, float &ScaleY) const
{
	GetSpriteScale(&g_pData->m_aSprites[Id], ScaleX, ScaleY);
}

void CRenderTools::GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY) const
{
	const float f = length(vec2(Width, Height));
	ScaleX = Width / f;
	ScaleY = Height / f;
}

void CRenderTools::DrawSprite(float x, float y, float Size) const
{
	IGraphics::CQuadItem QuadItem(x, y, Size * gs_SpriteWScale, Size * gs_SpriteHScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CRenderTools::DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight) const
{
	IGraphics::CQuadItem QuadItem(x, y, ScaledWidth, ScaledHeight);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CRenderTools::RenderCursor(vec2 Center, float Size) const
{
	Graphics()->WrapClamp();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(Center.x, Center.y, Size, Size);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CRenderTools::RenderIcon(int ImageId, int SpriteId, const CUIRect *pRect, const ColorRGBA *pColor) const
{
	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);
	Graphics()->QuadsBegin();
	SelectSprite(SpriteId);
	if(pColor)
		Graphics()->SetColor(pColor->r * pColor->a, pColor->g * pColor->a, pColor->b * pColor->a, pColor->a);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

int CRenderTools::QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float Size) const
{
	IGraphics::CQuadItem QuadItem(x, y, Size, Size);
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CRenderTools::QuadContainerAddSprite(int QuadContainerIndex, float Size) const
{
	IGraphics::CQuadItem QuadItem(-(Size) / 2.f, -(Size) / 2.f, (Size), (Size));
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CRenderTools::QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height) const
{
	IGraphics::CQuadItem QuadItem(-(Width) / 2.f, -(Height) / 2.f, (Width), (Height));
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CRenderTools::QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height) const
{
	IGraphics::CQuadItem QuadItem(X, Y, Width, Height);
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

void CRenderTools::GetRenderTeeAnimScaleAndBaseSize(const CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize)
{
	AnimScale = pInfo->m_Size * 1.0f / 64.0f;
	BaseSize = pInfo->m_Size;
}

void CRenderTools::GetRenderTeeBodyScale(float BaseSize, float &BodyScale)
{
	BodyScale = g_Config.m_ClFatSkins ? BaseSize * 1.3f : BaseSize;
	BodyScale /= 64.0f;
}

void CRenderTools::GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight)
{
	FeetScaleWidth = BaseSize / 64.0f;
	FeetScaleHeight = (BaseSize / 2) / 32.0f;
}

void CRenderTools::GetRenderTeeBodySize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float BodyScale;
	GetRenderTeeBodyScale(BaseSize, BodyScale);

	Width = pInfo->m_SkinMetrics.m_Body.WidthNormalized() * 64.0f * BodyScale;
	Height = pInfo->m_SkinMetrics.m_Body.HeightNormalized() * 64.0f * BodyScale;
	BodyOffset.x = pInfo->m_SkinMetrics.m_Body.OffsetXNormalized() * 64.0f * BodyScale;
	BodyOffset.y = pInfo->m_SkinMetrics.m_Body.OffsetYNormalized() * 64.0f * BodyScale;
}

void CRenderTools::GetRenderTeeFeetSize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float FeetScaleWidth, FeetScaleHeight;
	GetRenderTeeFeetScale(BaseSize, FeetScaleWidth, FeetScaleHeight);

	Width = pInfo->m_SkinMetrics.m_Feet.WidthNormalized() * 64.0f * FeetScaleWidth;
	Height = pInfo->m_SkinMetrics.m_Feet.HeightNormalized() * 32.0f * FeetScaleHeight;
	FeetOffset.x = pInfo->m_SkinMetrics.m_Feet.OffsetXNormalized() * 64.0f * FeetScaleWidth;
	FeetOffset.y = pInfo->m_SkinMetrics.m_Feet.OffsetYNormalized() * 32.0f * FeetScaleHeight;
}

void CRenderTools::GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
	vec2 BodyPos = vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;

	float AssumedScale = BaseSize / 64.0f;

	// just use the lowest feet
	vec2 FeetPos;
	const CAnimKeyframe *pFoot = pAnim->GetFrontFoot();
	FeetPos = vec2(pFoot->m_X * AnimScale, pFoot->m_Y * AnimScale);
	pFoot = pAnim->GetBackFoot();
	FeetPos = vec2(FeetPos.x, maximum(FeetPos.y, pFoot->m_Y * AnimScale));

	vec2 BodyOffset;
	float BodyWidth, BodyHeight;
	GetRenderTeeBodySize(pAnim, pInfo, BodyOffset, BodyWidth, BodyHeight);

	// -32 is the assumed min relative position for the quad
	float MinY = -32.0f * AssumedScale;
	// the body pos shifts the body away from center
	MinY += BodyPos.y;
	// the actual body is smaller though, because it doesn't use the full skin image in most cases
	MinY += BodyOffset.y;

	vec2 FeetOffset;
	float FeetWidth, FeetHeight;
	GetRenderTeeFeetSize(pAnim, pInfo, FeetOffset, FeetWidth, FeetHeight);

	// MaxY builds up from the MinY
	float MaxY = MinY + BodyHeight;
	// if the body is smaller than the total feet offset, use feet
	// since feet are smaller in height, respect the assumed relative position
	MaxY = maximum(MaxY, (-16.0f * AssumedScale + FeetPos.y) + FeetOffset.y + FeetHeight);

	// now we got the full rendered size
	float FullHeight = (MaxY - MinY);

	// next step is to calculate the offset that was created compared to the assumed relative position
	float MidOfRendered = MinY + FullHeight / 2.0f;

	// TODO: x coordinate is ignored for now, bcs it's not really used yet anyway
	TeeOffsetToMid.x = 0;
	// negative value, because the calculation that uses this offset should work with addition.
	TeeOffsetToMid.y = -MidOfRendered;
}

void CRenderTools::RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	if(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_BODY].IsValid())
		RenderTee7(pAnim, pInfo, Emote, Dir, Pos, Alpha);
	else
		RenderTee6(pAnim, pInfo, Emote, Dir, Pos, Alpha);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0);
}

void CRenderTools::RenderTee7(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	vec2 Direction = Dir;
	vec2 Position = Pos;
	const bool IsBot = pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture.IsValid();

	// first pass we draw the outline
	// second pass we draw the filling
	for(int Pass = 0; Pass < 2; Pass++)
	{
		bool OutLine = Pass == 0;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			float AnimScale = pInfo->m_Size * 1.0f / 64.0f;
			float BaseSize = pInfo->m_Size;
			if(Filling == 1)
			{
				vec2 BodyPos = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
				IGraphics::CQuadItem BodyItem(BodyPos.x, BodyPos.y, BaseSize, BaseSize);
				IGraphics::CQuadItem Item;

				if(IsBot && !OutLine)
				{
					IGraphics::CQuadItem BotItem(BodyPos.x + (2.f / 3.f) * AnimScale, BodyPos.y + (-16 + 2.f / 3.f) * AnimScale, BaseSize, BaseSize); // x+0.66, y+0.66 to correct some rendering bug

					// draw bot visuals (background)
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture);
					Graphics()->QuadsBegin();
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					SelectSprite7(client_data7::SPRITE_TEE_BOT_BACKGROUND);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();

					// draw bot visuals (foreground)
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture);
					Graphics()->QuadsBegin();
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					SelectSprite7(client_data7::SPRITE_TEE_BOT_FOREGROUND);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotColor.WithAlpha(Alpha));
					SelectSprite7(client_data7::SPRITE_TEE_BOT_GLOW);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw decoration
				if(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_DECORATION].IsValid())
				{
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_DECORATION]);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_DECORATION].WithAlpha(Alpha));
					SelectSprite7(OutLine ? client_data7::SPRITE_TEE_DECORATION_OUTLINE : client_data7::SPRITE_TEE_DECORATION);
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw body (behind marking)
				Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_BODY]);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
				if(OutLine)
				{
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					SelectSprite7(client_data7::SPRITE_TEE_BODY_OUTLINE);
				}
				else
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_BODY].WithAlpha(Alpha));
					SelectSprite7(client_data7::SPRITE_TEE_BODY);
				}
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();

				// draw marking
				if(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_MARKING].IsValid() && !OutLine)
				{
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_MARKING]);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
					ColorRGBA MarkingColor = pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_MARKING];
					Graphics()->SetColor(MarkingColor.r * MarkingColor.a, MarkingColor.g * MarkingColor.a, MarkingColor.b * MarkingColor.a, MarkingColor.a * Alpha);
					SelectSprite7(client_data7::SPRITE_TEE_MARKING);
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw body (in front of marking)
				if(!OutLine)
				{
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_BODY]);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					for(int t = 0; t < 2; t++)
					{
						SelectSprite7(t == 0 ? client_data7::SPRITE_TEE_BODY_SHADOW : client_data7::SPRITE_TEE_BODY_UPPER_OUTLINE);
						Item = BodyItem;
						Graphics()->QuadsDraw(&Item, 1);
					}
					Graphics()->QuadsEnd();
				}

				// draw eyes
				Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_EYES]);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
				if(IsBot)
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotColor.WithAlpha(Alpha));
					Emote = EMOTE_SURPRISE;
				}
				else
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_EYES].WithAlpha(Alpha));
				}
				if(Pass == 1)
				{
					switch(Emote)
					{
					case EMOTE_PAIN:
						SelectSprite7(client_data7::SPRITE_TEE_EYES_PAIN);
						break;
					case EMOTE_HAPPY:
						SelectSprite7(client_data7::SPRITE_TEE_EYES_HAPPY);
						break;
					case EMOTE_SURPRISE:
						SelectSprite7(client_data7::SPRITE_TEE_EYES_SURPRISE);
						break;
					case EMOTE_ANGRY:
						SelectSprite7(client_data7::SPRITE_TEE_EYES_ANGRY);
						break;
					default:
						SelectSprite7(client_data7::SPRITE_TEE_EYES_NORMAL);
						break;
					}

					float EyeScale = BaseSize * 0.60f;
					float h = Emote == EMOTE_BLINK ? BaseSize * 0.15f / 2.0f : EyeScale / 2.0f;
					vec2 Offset = vec2(Direction.x * 0.125f, -0.05f + Direction.y * 0.10f) * BaseSize;
					IGraphics::CQuadItem QuadItem(BodyPos.x + Offset.x, BodyPos.y + Offset.y, EyeScale, h);
					Graphics()->QuadsDraw(&QuadItem, 1);
				}
				Graphics()->QuadsEnd();

				// draw xmas hat
				if(!OutLine && pInfo->m_aSixup[g_Config.m_ClDummy].m_HatTexture.IsValid())
				{
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_HatTexture);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					int Flag = Direction.x < 0.0f ? SPRITE_FLAG_FLIP_X : 0;
					switch(pInfo->m_aSixup[g_Config.m_ClDummy].m_HatSpriteIndex)
					{
					case 0:
						SelectSprite7(client_data7::SPRITE_TEE_HATS_TOP1, Flag);
						break;
					case 1:
						SelectSprite7(client_data7::SPRITE_TEE_HATS_TOP2, Flag);
						break;
					case 2:
						SelectSprite7(client_data7::SPRITE_TEE_HATS_SIDE1, Flag);
						break;
					case 3:
						SelectSprite7(client_data7::SPRITE_TEE_HATS_SIDE2, Flag);
					}
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}
			}

			// draw feet
			Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_aTextures[protocol7::SKINPART_FEET]);
			Graphics()->QuadsBegin();
			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			float w = BaseSize / 2.1f;
			float h = w;

			Graphics()->QuadsSetRotation(pFoot->m_Angle * pi * 2);

			if(OutLine)
			{
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				SelectSprite7(client_data7::SPRITE_TEE_FOOT_OUTLINE);
			}
			else
			{
				bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
				float ColorScale = 1.0f;
				if(Indicate)
					ColorScale = 0.5f;
				Graphics()->SetColor(
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].r * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].g * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].b * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].a * Alpha);
				SelectSprite7(client_data7::SPRITE_TEE_FOOT);
			}

			IGraphics::CQuadItem QuadItem(Position.x + pFoot->m_X * AnimScale, Position.y + pFoot->m_Y * AnimScale, w, h);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}
}

void CRenderTools::RenderTee6(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	vec2 Direction = Dir;
	vec2 Position = Pos;

	const CSkin::SSkinTextures *pSkinTextures = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;

	// first pass we draw the outline
	// second pass we draw the filling
	for(int Pass = 0; Pass < 2; Pass++)
	{
		int OutLine = Pass == 0 ? 1 : 0;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			float AnimScale, BaseSize;
			GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
			if(Filling == 1)
			{
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2);

				// draw body
				Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, Alpha);
				vec2 BodyPos = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
				float BodyScale;
				GetRenderTeeBodyScale(BaseSize, BodyScale);
				Graphics()->TextureSet(OutLine == 1 ? pSkinTextures->m_BodyOutline : pSkinTextures->m_Body);
				Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, OutLine, BodyPos.x, BodyPos.y, BodyScale, BodyScale);

				// draw eyes
				if(Pass == 1)
				{
					int QuadOffset = 2;
					int EyeQuadOffset = 0;
					int TeeEye = 0;

					switch(Emote)
					{
					case EMOTE_PAIN:
						EyeQuadOffset = 0;
						TeeEye = SPRITE_TEE_EYE_PAIN - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_HAPPY:
						EyeQuadOffset = 1;
						TeeEye = SPRITE_TEE_EYE_HAPPY - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_SURPRISE:
						EyeQuadOffset = 2;
						TeeEye = SPRITE_TEE_EYE_SURPRISE - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_ANGRY:
						EyeQuadOffset = 3;
						TeeEye = SPRITE_TEE_EYE_ANGRY - SPRITE_TEE_EYE_NORMAL;
						break;
					default:
						EyeQuadOffset = 4;
						break;
					}

					float EyeScale = BaseSize * 0.40f;
					float h = Emote == EMOTE_BLINK ? BaseSize * 0.15f : EyeScale;
					float EyeSeparation = (0.075f - 0.010f * absolute(Direction.x)) * BaseSize;
					vec2 Offset = vec2(Direction.x * 0.125f, -0.05f + Direction.y * 0.10f) * BaseSize;

					Graphics()->TextureSet(pSkinTextures->m_aEyes[TeeEye]);
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset + EyeQuadOffset, BodyPos.x - EyeSeparation + Offset.x, BodyPos.y + Offset.y, EyeScale / (64.f * 0.4f), h / (64.f * 0.4f));
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset + EyeQuadOffset, BodyPos.x + EyeSeparation + Offset.x, BodyPos.y + Offset.y, -EyeScale / (64.f * 0.4f), h / (64.f * 0.4f));
				}
			}

			// draw feet
			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			float w = BaseSize;
			float h = BaseSize / 2;

			int QuadOffset = 7;
			if(Dir.x < 0 && pInfo->m_FeetFlipped)
			{
				QuadOffset += 2;
			}

			Graphics()->QuadsSetRotation(pFoot->m_Angle * pi * 2);

			bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
			float ColorScale = 1.0f;

			if(!OutLine)
			{
				++QuadOffset;
				if(Indicate)
					ColorScale = 0.5f;
			}

			Graphics()->SetColor(pInfo->m_ColorFeet.r * ColorScale, pInfo->m_ColorFeet.g * ColorScale, pInfo->m_ColorFeet.b * ColorScale, Alpha);

			Graphics()->TextureSet(OutLine == 1 ? pSkinTextures->m_FeetOutline : pSkinTextures->m_Feet);
			Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset, Position.x + pFoot->m_X * AnimScale, Position.y + pFoot->m_Y * AnimScale, w / 64.f, h / 32.f);
		}
	}
}

void CRenderTools::CalcScreenParams(float Aspect, float Zoom, float *pWidth, float *pHeight)
{
	const float Amount = 1150 * 1000;
	const float WMax = 1500;
	const float HMax = 1050;

	const float f = std::sqrt(Amount) / std::sqrt(Aspect);
	*pWidth = f * Aspect;
	*pHeight = f;

	// limit the view
	if(*pWidth > WMax)
	{
		*pWidth = WMax;
		*pHeight = *pWidth / Aspect;
	}

	if(*pHeight > HMax)
	{
		*pHeight = HMax;
		*pWidth = *pHeight * Aspect;
	}

	*pWidth *= Zoom;
	*pHeight *= Zoom;
}

void CRenderTools::MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
	float ParallaxZoom, float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints)
{
	float Width, Height;
	CalcScreenParams(Aspect, Zoom, &Width, &Height);

	float Scale = (ParallaxZoom * (Zoom - 1.0f) + 100.0f) / 100.0f / Zoom;
	Width *= Scale;
	Height *= Scale;

	CenterX *= ParallaxX / 100.0f;
	CenterY *= ParallaxY / 100.0f;
	pPoints[0] = OffsetX + CenterX - Width / 2;
	pPoints[1] = OffsetY + CenterY - Height / 2;
	pPoints[2] = pPoints[0] + Width;
	pPoints[3] = pPoints[1] + Height;
}

void CRenderTools::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom)
{
	float ParallaxZoom = clamp((double)(maximum(pGroup->m_ParallaxX, pGroup->m_ParallaxY)), 0., 100.);
	float aPoints[4];
	MapScreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX, pGroup->m_ParallaxY, ParallaxZoom,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), Zoom, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CRenderTools::MapScreenToInterface(float CenterX, float CenterY)
{
	float aPoints[4];
	MapScreenToWorld(CenterX, CenterY, 100.0f, 100.0f, 100.0f,
		0, 0, Graphics()->ScreenAspect(), 1.0f, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}
