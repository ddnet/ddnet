/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_COUNTRYFLAGS_H
#define GAME_CLIENT_COMPONENTS_COUNTRYFLAGS_H

#include <engine/graphics.h>
#include <game/client/component.h>

#include <unordered_map>

class CCountryFlags : public CComponent
{
public:
	class CCountryFlag
	{
	public:
		int m_CountryCode;
		char m_aCountryCodeString[8];
		IGraphics::CTextureHandle m_Texture;

		bool operator<(const CCountryFlag &Other) const { return str_comp(m_aCountryCodeString, Other.m_aCountryCodeString) < 0; }
	};

	virtual int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;

	const std::unordered_map<int, CCountryFlag> &CountryFlags() const;

	void Render(const CCountryFlag *pFlag, ColorRGBA Color, float x, float y, float w, float h);
	void Render(int CountryCode, ColorRGBA Color, float x, float y, float w, float h);

private:
	std::unordered_map<int, CCountryFlag> m_CountryFlags;

	int m_FlagsQuadContainerIndex;

	void LoadCountryflagsIndexfile();
};
#endif
