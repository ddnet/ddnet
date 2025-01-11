/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS7_H
#define GAME_CLIENT_COMPONENTS_SKINS7_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

#include <chrono>
#include <vector>

class CSkins7 : public CComponent
{
public:
	enum
	{
		SKINFLAG_SPECIAL = 1 << 0,
		SKINFLAG_STANDARD = 1 << 1,

		NUM_COLOR_COMPONENTS = 4,

		HAT_NUM = 2,
		HAT_OFFSET_SIDE = 2,
	};

	class CSkinPart
	{
	public:
		int m_Flags;
		char m_aName[24];
		IGraphics::CTextureHandle m_OrgTexture;
		IGraphics::CTextureHandle m_ColorTexture;
		ColorRGBA m_BloodColor;

		bool operator<(const CSkinPart &Other) { return str_comp_nocase(m_aName, Other.m_aName) < 0; }
	};

	class CSkin
	{
	public:
		int m_Flags;
		char m_aName[24];
		const CSkinPart *m_apParts[protocol7::NUM_SKINPARTS];
		int m_aUseCustomColors[protocol7::NUM_SKINPARTS];
		unsigned m_aPartColors[protocol7::NUM_SKINPARTS];

		bool operator<(const CSkin &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }
		bool operator==(const CSkin &Other) const { return !str_comp(m_aName, Other.m_aName); }
	};

	static const char *const ms_apSkinPartNames[protocol7::NUM_SKINPARTS];
	static const char *const ms_apSkinPartNamesLocalized[protocol7::NUM_SKINPARTS];
	static const char *const ms_apColorComponents[NUM_COLOR_COMPONENTS];

	static char *ms_apSkinNameVariables[NUM_DUMMIES];
	static char *ms_apSkinVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS];
	static int *ms_apUCCVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS]; // use custom color
	static unsigned *ms_apColorVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS];

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;

	std::chrono::nanoseconds LastRefreshTime() const { return m_LastRefreshTime; }

	const std::vector<CSkin> &GetSkins() const;
	const std::vector<CSkinPart> &GetSkinParts(int Part) const;
	const CSkinPart *FindSkinPartOrNullptr(int Part, const char *pName, bool AllowSpecialPart) const;
	const CSkinPart *FindDefaultSkinPart(int Part) const;
	const CSkinPart *FindSkinPart(int Part, const char *pName, bool AllowSpecialPart) const;
	void RandomizeSkin(int Dummy) const;

	ColorRGBA GetColor(int Value, bool UseAlpha) const;
	ColorRGBA GetTeamColor(int UseCustomColors, int PartColor, int Team, int Part) const;

	// returns true if everything was valid and nothing changed
	bool ValidateSkinParts(char *apPartNames[protocol7::NUM_SKINPARTS], int *pUseCustomColors, int *pPartColors, int GameFlags) const;

	bool SaveSkinfile(const char *pName, int Dummy);
	bool RemoveSkin(const CSkin *pSkin);

	IGraphics::CTextureHandle XmasHatTexture() const { return m_XmasHatTexture; }
	IGraphics::CTextureHandle BotDecorationTexture() const { return m_BotTexture; }

	static bool IsSpecialSkin(const char *pName);

private:
	int m_ScanningPart;
	std::chrono::nanoseconds m_LastRefreshTime;

	std::vector<CSkinPart> m_avSkinParts[protocol7::NUM_SKINPARTS];
	CSkinPart m_aPlaceholderSkinParts[protocol7::NUM_SKINPARTS];
	std::vector<CSkin> m_vSkins;

	IGraphics::CTextureHandle m_XmasHatTexture;
	IGraphics::CTextureHandle m_BotTexture;

	static int SkinPartScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);

	void InitPlaceholderSkinParts();
	void LoadXmasHat();
	void LoadBotDecoration();

	void AddSkinFromConfigVariables(const char *pName, int Dummy);
};

#endif
