
#ifndef GAME_CLIENT_SKIN_H
#define GAME_CLIENT_SKIN_H
#include <base/color.h>
#include <base/tl/sorted_array.h>
#include <base/vmath.h>
#include <engine/graphics.h>
#include <limits>

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

	template<bool IsSizeType>
	struct SSkinMetricVariableInt
	{
		int m_Value;
		operator int() { return m_Value; }
		SSkinMetricVariableInt &operator=(int NewVal)
		{
			if(IsSizeType)
				m_Value = maximum(m_Value, NewVal);
			else
				m_Value = minimum(m_Value, NewVal);
			return *this;
		}

		SSkinMetricVariableInt()
		{
			Reset();
		}

		void Reset()
		{
			if(IsSizeType)
				m_Value = std::numeric_limits<int>::lowest();
			else
				m_Value = std::numeric_limits<int>::max();
		}
	};

	struct SSkinMetricVariable
	{
		SSkinMetricVariableInt<true> m_Width;
		SSkinMetricVariableInt<true> m_Height;
		SSkinMetricVariableInt<false> m_OffsetX;
		SSkinMetricVariableInt<false> m_OffsetY;

		// these can be used to normalize the metrics
		SSkinMetricVariableInt<true> m_MaxWidth;
		SSkinMetricVariableInt<true> m_MaxHeight;

		float WidthNormalized()
		{
			return (float)m_Width / (float)m_MaxWidth;
		}

		float HeightNormalized()
		{
			return (float)m_Height / (float)m_MaxHeight;
		}

		float OffsetXNormalized()
		{
			return (float)m_OffsetX / (float)m_MaxWidth;
		}

		float OffsetYNormalized()
		{
			return (float)m_OffsetY / (float)m_MaxHeight;
		}

		void Reset()
		{
			m_Width.Reset();
			m_Height.Reset();
			m_OffsetX.Reset();
			m_OffsetY.Reset();
			m_MaxWidth.Reset();
			m_MaxHeight.Reset();
		}
	};

	struct SSkinMetrics
	{
		SSkinMetricVariable m_Body;
		SSkinMetricVariable m_Feet;

		int m_FeetWidth;
		int m_FeetHeight;
		int m_FeetOffsetX;
		int m_FeetOffsetY;

		// these can be used to normalize the metrics
		int m_FeetMaxWidth;
		int m_FeetMaxHeight;

		void Reset()
		{
			m_Body.Reset();
			m_Feet.Reset();
		}

		SSkinMetrics()
		{
			Reset();
		}
	};
	SSkinMetrics m_Metrics;

	bool operator<(const CSkin &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }

	bool operator<(const char *pOther) const { return str_comp_nocase(m_aName, pOther) < 0; }
	bool operator==(const char *pOther) const { return !str_comp_nocase(m_aName, pOther); }
};

#endif
