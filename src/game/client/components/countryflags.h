/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_COUNTRYFLAGS_H
#define GAME_CLIENT_COMPONENTS_COUNTRYFLAGS_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <cstddef>
#include <vector>

class CCountryFlags : public CComponent
{
public:
	class CCountryFlag
	{
	public:
		int m_CountryCode;
		char m_aCountryCodeString[8];
		IGraphics::CTextureHandle m_Texture;

		bool operator<(const CCountryFlag &Other) const;
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;

	size_t Num() const;
	const CCountryFlag &GetByCountryCode(int CountryCode) const;
	const CCountryFlag &GetByIndex(size_t Index) const;
	void Render(const CCountryFlag &Flag, ColorRGBA Color, float x, float y, float w, float h);
	void Render(int CountryCode, ColorRGBA Color, float x, float y, float w, float h);

private:
	static constexpr int CODE_LB = -1;
	static constexpr int CODE_UB = 999;

	std::vector<CCountryFlag> m_vCountryFlags;
	size_t m_aCountryCodeToIndexTable[CODE_UB - CODE_LB + 1];

	int m_FlagsQuadContainerIndex;

	static bool ValidateCountryCodeString(const char *pString);
	void LoadCountryflagsIndexfile();
};
#endif
