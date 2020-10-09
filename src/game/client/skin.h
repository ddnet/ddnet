
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
			for(int i = 0; i < 6; ++i)
				m_Eyes[i] = IGraphics::CTextureHandle();
		}
	};

	SSkinTextures m_OriginalSkin;
	SSkinTextures m_ColorableSkin;
	char m_aName[24];
	ColorRGBA m_BloodColor;

	bool operator<(const CSkin &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }

	bool operator<(const char *pOther) const { return str_comp(m_aName, pOther) < 0; }
	bool operator==(const char *pOther) const { return !str_comp(m_aName, pOther); }
};

#endif
