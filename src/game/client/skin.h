#ifndef GAME_CLIENT_SKIN_H
#define GAME_CLIENT_SKIN_H

#include <base/color.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <limits>

// do this better and nicer
struct CSkin
{
private:
	char m_aName[MAX_SKIN_LENGTH];

public:
	struct SSkinTextures
	{
		IGraphics::CTextureHandle m_Body;
		IGraphics::CTextureHandle m_BodyOutline;

		IGraphics::CTextureHandle m_Feet;
		IGraphics::CTextureHandle m_FeetOutline;

		IGraphics::CTextureHandle m_Hands;
		IGraphics::CTextureHandle m_HandsOutline;

		IGraphics::CTextureHandle m_aEyes[6];

		void Reset()
		{
			m_Body = IGraphics::CTextureHandle();
			m_BodyOutline = IGraphics::CTextureHandle();
			m_Feet = IGraphics::CTextureHandle();
			m_FeetOutline = IGraphics::CTextureHandle();
			m_Hands = IGraphics::CTextureHandle();
			m_HandsOutline = IGraphics::CTextureHandle();
			for(auto &Eye : m_aEyes)
				Eye = IGraphics::CTextureHandle();
		}

		void Unload(IGraphics *pGraphics)
		{
			pGraphics->UnloadTexture(&m_Body);
			pGraphics->UnloadTexture(&m_BodyOutline);
			pGraphics->UnloadTexture(&m_Feet);
			pGraphics->UnloadTexture(&m_FeetOutline);
			pGraphics->UnloadTexture(&m_Hands);
			pGraphics->UnloadTexture(&m_HandsOutline);
			for(auto &Eye : m_aEyes)
				pGraphics->UnloadTexture(&Eye);
		}
	};

	SSkinTextures m_OriginalSkin;
	SSkinTextures m_ColorableSkin;
	ColorRGBA m_BloodColor;

	template<bool IsSizeType>
	struct SSkinMetricVariableInt
	{
		int m_Value;
		operator int() const { return m_Value; }
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

		float WidthNormalized() const
		{
			return (float)m_Width / (float)m_MaxWidth;
		}

		float HeightNormalized() const
		{
			return (float)m_Height / (float)m_MaxHeight;
		}

		float OffsetXNormalized() const
		{
			return (float)m_OffsetX / (float)m_MaxWidth;
		}

		float OffsetYNormalized() const
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

	bool operator<(const CSkin &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
	bool operator==(const CSkin &Other) const { return !str_comp(m_aName, Other.m_aName); }

	CSkin(const char *pName)
	{
		str_copy(m_aName, pName);
	}
	CSkin(CSkin &&) = default;
	CSkin &operator=(CSkin &&) = default;

	const char *GetName() const { return m_aName; }

	// has to be kept in sync with m_aSkinNameRestrictions
	static bool IsValidName(const char *pName)
	{
		if(pName[0] == '\0' || str_length(pName) >= (int)sizeof(CSkin("").m_aName))
		{
			return false;
		}

		for(int i = 0; pName[i] != '\0'; ++i)
		{
			if(pName[i] == '"' || pName[i] == '/' || pName[i] == '\\')
			{
				return false;
			}
		}
		return true;
	}
	static constexpr char m_aSkinNameRestrictions[] = "Skin names must be valid filenames shorter than 24 characters.";
};

#endif
