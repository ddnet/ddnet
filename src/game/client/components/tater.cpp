/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "chat.h"
#include "emoticon.h"

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
	bool randomizeBody = false;
	bool randomizeFeet = false;
	bool randomizeSkin = false;
	bool randomizeFlag = false;

	if(pResult->NumArguments() == 0)
	{
		randomizeBody = true;
		randomizeFeet = true;
		randomizeSkin = true;
		randomizeFlag = true;
	}
	else if(pResult->NumArguments() == 1)
	{
		const char *type = pResult->GetString(0);
		int length = type ? str_length(type) : 0;
		if(length == 1 && type[0] == '0')
		{ // randomize all
			randomizeBody = true;
			randomizeFeet = true;
			randomizeSkin = true;
			randomizeFlag = true;
		}
		else if(length == 1)
		{ // randomize body
			randomizeBody = type[0] == '1';
		}
		else if(length == 2)
		{
			// check for body and feet
			randomizeBody = type[0] == '1';
			randomizeFeet = type[1] == '1';
		}
		else if(length == 3)
		{
			// check for body, feet and skin
			randomizeBody = type[0] == '1';
			randomizeFeet = type[1] == '1';
			randomizeSkin = type[2] == '1';
		}
		else if(length == 4)
		{
			// check for body, feet, skin and flag
			randomizeBody = type[0] == '1';
			randomizeFeet = type[1] == '1';
			randomizeSkin = type[2] == '1';
			randomizeFlag = type[3] == '1';
		}
	}

	if(randomizeBody)
	{
		RandomBodyColor();
	}
	if(randomizeFeet)
	{
		RandomFeetColor();
	}
	if(randomizeSkin)
	{
		RandomSkin(pUserData);
	}
	if(randomizeFlag)
	{
		RandomFlag(pUserData);
	}
	pThis->m_pClient->SendInfo(false);
}

void CTater::OnConsoleInit()
{
	Console()->Register("tc_random_player", "s[type]", CFGFLAG_CLIENT, ConRandomTee, this, "randomize player color (0 = all, 1 = body, 2 = feet, 3 = skin, 4 = flag) example: 0011 = randomize skin and flag [number is position] ");
	Console()->Chain("tc_random_player", ConchainRandomColor, this);
}

void CTater::RandomBodyColor()
{
	g_Config.m_ClPlayerColorBody = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1).Pack(false);
}

void CTater::RandomFeetColor()
{
	g_Config.m_ClPlayerColorFeet = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1).Pack(false);
}

void CTater::RandomSkin(void *pUserData)
{
	CTater *pThis = static_cast<CTater *>(pUserData);
	// get the skin count
	int skinCount = pThis->m_pClient->m_Skins.Num();

	// get a random skin number
	int skinNumber = std::rand() % skinCount;

	// get all skins as a maps
	const std::unordered_map<std::string_view, std::unique_ptr<CSkin>> &skins = pThis->m_pClient->m_Skins.GetSkinsUnsafe();

	// map to array
	int counter = 0;
	std::vector<std::pair<std::string_view, CSkin *>> skinArray;
	for(const auto &skin : skins)
	{
		if(counter == skinNumber)
		{
			// set the skin name
			const char *skinName = skin.first.data();
			str_copy(g_Config.m_ClPlayerSkin, skinName, sizeof(g_Config.m_ClPlayerSkin));
		}
		counter++;
	}
}

void CTater::RandomFlag(void *pUserData)
{
	CTater *pThis = static_cast<CTater *>(pUserData);
	// get the flag count
	int flagCount = pThis->m_pClient->m_CountryFlags.Num();

	// get a random flag number
	int flagNumber = std::rand() % flagCount;

	// get the flag name
	const CCountryFlags::CCountryFlag *pFlag = pThis->m_pClient->m_CountryFlags.GetByIndex(flagNumber);

	// set the flag code as number
	g_Config.m_PlayerCountry = pFlag->m_CountryCode;
}