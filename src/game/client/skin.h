
#ifndef GAME_CLIENT_SKIN_H
#define GAME_CLIENT_SKIN_H
#include <base/color.h>
#include <base/tl/sorted_array.h>
#include <base/vmath.h>
#include <engine/graphics.h>

// do this better and nicer
struct CSkin
{
	bool m_IsVanilla;

	struct SSkinTextures
	{
		IGraphics::CTextureHandle m_Body;
		IGraphics::CTextureHandle m_BodyOutline;

		IGraphics::CTextureHandle m_Feet;
		IGraphics::CTextureHandle m_FeetOutline;

		IGraphics::CTextureHandle m_Hands;
		IGraphics::CTextureHandle m_HandsOutline;

		IGraphics::CTextureHandle m_Eyes[6];

		void Reset()
		{
			m_Body = IGraphics::CTextureHandle();
			m_BodyOutline = IGraphics::CTextureHandle();
			m_Feet = IGraphics::CTextureHandle();
			m_FeetOutline = IGraphics::CTextureHandle();
			m_Hands = IGraphics::CTextureHandle();
			m_HandsOutline = IGraphics::CTextureHandle();
			for(auto &Eye : m_Eyes)
				Eye = IGraphics::CTextureHandle();
		}
	};

	SSkinTextures m_OriginalSkin;
	SSkinTextures m_ColorableSkin;
	char m_aName[24];
	ColorRGBA m_BloodColor;

	struct SSkinMetrics
	{
		int m_BodyWidth;
		int m_BodyHeight;
		int m_BodyOffsetX;
		int m_BodyOffsetY;

		// these can be used to normalize the metrics
		int m_BodyMaxWidth;
		int m_BodyMaxHeight;

		int m_FeetWidth;
		int m_FeetHeight;
		int m_FeetOffsetX;
		int m_FeetOffsetY;

		// these can be used to normalize the metrics
		int m_FeetMaxWidth;
		int m_FeetMaxHeight;

		void Reset()
		{
			m_BodyWidth = m_BodyHeight = m_BodyOffsetX = m_BodyOffsetY = m_FeetWidth = m_FeetHeight = m_FeetOffsetX = m_FeetOffsetY = -1;
		}

		float BodyWidthNormalized()
		{
			return (float)m_BodyWidth / (float)m_BodyMaxWidth;
		}

		float BodyHeightNormalized()
		{
			return (float)m_BodyHeight / (float)m_BodyMaxHeight;
		}

		float BodyOffsetXNormalized()
		{
			return (float)m_BodyOffsetX / (float)m_BodyMaxWidth;
		}

		float BodyOffsetYNormalized()
		{
			return (float)m_BodyOffsetY / (float)m_BodyMaxHeight;
		}

		float FeetWidthNormalized()
		{
			return (float)m_FeetWidth / (float)m_FeetMaxWidth;
		}

		float FeetHeightNormalized()
		{
			return (float)m_FeetHeight / (float)m_FeetMaxHeight;
		}

		float FeetOffsetXNormalized()
		{
			return (float)m_FeetOffsetX / (float)m_FeetMaxWidth;
		}

		float FeetOffsetYNormalized()
		{
			return (float)m_FeetOffsetY / (float)m_FeetMaxHeight;
		}
	};
	SSkinMetrics m_Metrics;

	bool operator<(const CSkin &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }

	bool operator<(const char *pOther) const { return str_comp_nocase(m_aName, pOther) < 0; }
	bool operator==(const char *pOther) const { return !str_comp_nocase(m_aName, pOther); }
};

#endif
