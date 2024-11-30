/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "../chat.h"
#include "../emoticon.h"

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "tater.h"
#include <game/client/gameclient.h>

CTater::CTater()
{
	OnReset();
}

void CTater::ConRandomTee(IConsole::IResult *pResult, void *pUserData) {}

void CTater::ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CTater *pThis = static_cast<CTater *>(pUserData);
	// resolve type to randomize
	// check length of type (0 = all, 1 = body, 2 = feet, 3 = skin, 4 = flag)
	bool RandomizeBody = false;
	bool RandomizeFeet = false;
	bool RandomizeSkin = false;
	bool RandomizeFlag = false;

	if(pResult->NumArguments() == 0)
	{
		RandomizeBody = true;
		RandomizeFeet = true;
		RandomizeSkin = true;
		RandomizeFlag = true;
	}
	else if(pResult->NumArguments() == 1)
	{
		const char *Type = pResult->GetString(0);
		int Length = Type ? str_length(Type) : 0;
		if(Length == 1 && Type[0] == '0')
		{ // Randomize all
			RandomizeBody = true;
			RandomizeFeet = true;
			RandomizeSkin = true;
			RandomizeFlag = true;
		}
		else if(Length == 1)
		{
			// randomize body
			RandomizeBody = Type[0] == '1';
		}
		else if(Length == 2)
		{
			// check for body and feet
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
		}
		else if(Length == 3)
		{
			// check for body, feet and skin
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
			RandomizeSkin = Type[2] == '1';
		}
		else if(Length == 4)
		{
			// check for body, feet, skin and flag
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
			RandomizeSkin = Type[2] == '1';
			RandomizeFlag = Type[3] == '1';
		}
	}

	if(RandomizeBody)
		RandomBodyColor();
	if(RandomizeFeet)
		RandomFeetColor();
	if(RandomizeSkin)
		RandomSkin(pUserData);
	if(RandomizeFlag)
		RandomFlag(pUserData);
	pThis->m_pClient->SendInfo(false);
}

void CTater::OnConsoleInit()
{
	Console()->Register("tc_random_player", "s[type]", CFGFLAG_CLIENT, ConRandomTee, this, "Randomize player color (0 = all, 1 = body, 2 = feet, 3 = skin, 4 = flag) example: 0011 = randomize skin and flag [number is position] ");
	Console()->Chain("tc_random_player", ConchainRandomColor, this);
}

void CTater::RandomBodyColor()
{
	g_Config.m_ClPlayerColorBody = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1.0f).Pack(false);
}

void CTater::RandomFeetColor()
{
	g_Config.m_ClPlayerColorFeet = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1.0f).Pack(false);
}

void CTater::RandomSkin(void *pUserData)
{
	CTater *pThis = static_cast<CTater *>(pUserData);
	// get the skin count
	int SkinCount = (int)pThis->m_pClient->m_Skins.GetSkinsUnsafe().size();

	// get a random skin number
	int SkinNumber = std::rand() % SkinCount;

	// get all skins as a maps
	const std::unordered_map<std::string_view, std::unique_ptr<CSkin>> &Skins = pThis->m_pClient->m_Skins.GetSkinsUnsafe();

	// map to array
	int Counter = 0;
	std::vector<std::pair<std::string_view, CSkin *>> SkinArray;
	for(const auto &Skin : Skins)
	{
		if(Counter == SkinNumber)
		{
			// set the skin name
			const char *SkinName = Skin.first.data();
			str_copy(g_Config.m_ClPlayerSkin, SkinName, sizeof(g_Config.m_ClPlayerSkin));
		}
		Counter++;
	}
}

void CTater::RandomFlag(void *pUserData)
{
	CTater *pThis = static_cast<CTater *>(pUserData);
	// get the flag count
	int FlagCount = pThis->m_pClient->m_CountryFlags.Num();

	// get a random flag number
	int FlagNumber = std::rand() % FlagCount;

	// get the flag name
	const CCountryFlags::CCountryFlag *pFlag = pThis->m_pClient->m_CountryFlags.GetByIndex(FlagNumber);

	// set the flag code as number
	g_Config.m_PlayerCountry = pFlag->m_CountryCode;
}
