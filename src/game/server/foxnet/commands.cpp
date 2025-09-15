#include <game/gamecore.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/score.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <base/log.h>
#include <base/system.h>
#include <base/vmath.h>

#include "votemenu.h"

#include <unordered_map>

void CGameContext::ConAccRegister(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!g_Config.m_SvAccounts)
	{
		pSelf->SendChatTarget(ClientId, "Account registration is disabled.");
		return;
	}

	if(pSelf->m_aAccounts[ClientId].m_LoggedIn)
	{
		pSelf->SendChatTarget(ClientId, "You are already logged in");
		return;
	}
	const char *pUser = pResult->GetString(0);
	const char *pPass = pResult->GetString(1);
	const char *pPass2 = pResult->GetString(2);

	pSelf->m_AccountManager.Register(ClientId, pUser, pPass, pPass2);
}

void CGameContext::ConAccPassword(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!g_Config.m_SvAccounts)
	{
		pSelf->SendChatTarget(ClientId, "Accounts are disabled");
		return;
	}

	if(!pSelf->m_aAccounts[ClientId].m_LoggedIn)
	{
		pSelf->SendChatTarget(ClientId, "You aren't logged in");
		return;
	}
	const char *pOldPass = pResult->GetString(0);
	const char *pPass = pResult->GetString(1);
	const char *pPass2 = pResult->GetString(2);

	pSelf->m_AccountManager.ChangePassword(ClientId, pOldPass, pPass, pPass2);
}

void CGameContext::ConAccLogin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!g_Config.m_SvAccounts)
	{
		pSelf->SendChatTarget(ClientId, "Accounts are disabled");
		return;
	}

	if(pSelf->m_aAccounts[ClientId].m_LoggedIn)
	{
		pSelf->SendChatTarget(ClientId, "You are already logged in");
		return;
	}

	const char *pUser = pResult->GetString(0);
	const char *pPass = pResult->GetString(1);

	if(!pSelf->m_AccountManager.Login(ClientId, pUser, pPass))
	{
		pSelf->SendChatTarget(ClientId, "Login failed");
		return;
	}

	pSelf->SendChatTarget(ClientId, "Login successful");
}

void CGameContext::ConAccLogout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(pSelf->m_AccountManager.Logout(ClientId))
		pSelf->SendChatTarget(ClientId, "Logged out");
	else
		pSelf->SendChatTarget(ClientId, "Not logged in");
}

void CGameContext::ConAccProfile(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->NumArguments() ? pResult->GetString(0) : pSelf->Server()->ClientName(pResult->m_ClientId);
	pSelf->m_AccountManager.ShowAccProfile(pResult->m_ClientId, pName);
}

void CGameContext::ConAccTop5Money(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_AccountManager.Top5(pResult->m_ClientId, "money", pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 0);
}
void CGameContext::ConAccTop5Level(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_AccountManager.Top5(pResult->m_ClientId, "level", pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 0);
}
void CGameContext::ConAccTop5Playtime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_AccountManager.Top5(pResult->m_ClientId, "playtime", pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 0);
}

void CGameContext::ConAccEdit(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pUser = pResult->GetString(0);
	const char *pVariable = pResult->GetString(1);
	const char *pValue = pResult->GetString(2);

	pSelf->m_AccountManager.EditAccount(pUser, pVariable, pValue);
}

void CGameContext::ConAccDisable(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pUser = pResult->GetString(0);
	const int pValue = pResult->GetInteger(1);

	pSelf->m_AccountManager.DisableAccount(pUser, pValue);
}

void CGameContext::ConAccForceLogin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	pSelf->m_AccountManager.ForceLogin(pResult->m_ClientId, pName);
}

void CGameContext::ConAccForceLogout(IConsole::IResult *pResult, void *pUserData)
{ // Logs out Current Acc Session, does not work across servers
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientId = pResult->GetInteger(0);
	if(!CheckClientId(ClientId))
		return;
	pSelf->m_AccountManager.Logout(ClientId);
}

void CGameContext::ConAddChatDetectionString(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *String = pResult->GetString(0);
	const char *Reason = pResult->GetString(1);
	bool Ban = pResult->GetInteger(2);
	int BanTime = pResult->GetInteger(3);
	float Addition = pResult->GetFloat(4);
	if(BanTime < 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", "Ban time must be greater than 0");
		return;
	}
	if(Addition <= 0.0f)
		Addition = 1.0f;

	pSelf->AddChatDetectionString(String, Reason, Ban, BanTime, Addition);
}

void CGameContext::AddChatDetectionString(const char *pString, const char *pReason, bool pBan, int pBanTime, float pAddition)
{
	char aBuf[512];

	for(const auto &Words : m_vChatDetection)
	{
		if(Words.String()[0] == '\0')
			continue;
		if(!str_comp_nocase(Words.String(), pString))
		{
			str_format(aBuf, sizeof(aBuf), "String \"%s\" already exists in the list", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
			return;
		}
	}

	if(str_comp_nocase(pString, "") != 0)
	{
		m_vChatDetection.push_back(CStringDetection(pString, pReason, pAddition, pBan, pBanTime));
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Chat Detection List", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
	}
}

void CGameContext::ConClearChatDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_vChatDetection.clear();
}

void CGameContext::ConRemoveChatDetectionString(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *String = pResult->GetString(0);
	pSelf->RemoveChatDetectionString(String);
}

void CGameContext::RemoveChatDetectionString(const char *pString)
{
	if(pString[0] == '\0')
		return;

	if(m_vChatDetection.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(auto it = m_vChatDetection.begin(); it != m_vChatDetection.end(); ++it)
	{
		if(!str_comp_nocase(it->String(), pString))
		{
			m_vChatDetection.erase(it);
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Chat Detection List", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
			return;
		}
	}
}

void CGameContext::ConListChatDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pSelf->m_vChatDetection.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(const auto &Words : pSelf->m_vChatDetection)
	{
		if(Words.String()[0] == '\0')
			continue;
		str_copy(aBuf, "");

		str_format(aBuf, sizeof(aBuf), "Str: %s | Reas: %s | Time: %d | Bans: %s | CountAdd: %.1f", Words.String(), Words.Reason(), Words.Time(), Words.IsBan() ? "Yes" : "No", Words.Addition());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
	}
}

void CGameContext::ConAddNameDetectionString(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *String = pResult->GetString(0);
	const char *Reason = pResult->GetString(1);
	int BanTime = pResult->GetInteger(2);
	bool ExactName = pResult->NumArguments() > 3 ? pResult->GetInteger(3) : false;
	if(BanTime < 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", "Ban time must be greater than 0");
		return;
	}

	pSelf->AddNameDetectionString(String, Reason, BanTime, ExactName);
}

void CGameContext::AddNameDetectionString(const char *pString, const char *pReason, int pBanTime, bool ExactName)
{
	char aBuf[512];

	for(const auto &Words : m_vNameDetection)
	{
		if(Words.String()[0] == '\0')
			continue;
		if(!str_comp_nocase(Words.String(), pString))
		{
			str_format(aBuf, sizeof(aBuf), "Name \"%s\" already exists in the list", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
			return;
		}
	}

	if(str_comp_nocase(pString, "") != 0)
	{
		m_vNameDetection.push_back(CStringDetection(pString, pReason, 1, pBanTime, ExactName));
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Name Detection List", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
	}
}

void CGameContext::ConClearNameDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_vNameDetection.clear();
}

void CGameContext::ConRemoveNameDetectionString(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *String = pResult->GetString(0);
	pSelf->RemoveNameDetectionString(String);
}

void CGameContext::RemoveNameDetectionString(const char *pString)
{
	if(pString[0] == '\0')
		return;
	if(m_vNameDetection.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(auto it = m_vNameDetection.begin(); it != m_vNameDetection.end(); ++it)
	{
		if(!str_comp(it->String(), pString))
		{
			m_vNameDetection.erase(it);
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Name Detection List", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
			return;
		}
	}
}

void CGameContext::ConListNameDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pSelf->m_vNameDetection.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(const auto &Words : pSelf->m_vNameDetection)
	{
		if(Words.String()[0] == '\0')
			continue;
		str_copy(aBuf, "");

		str_format(aBuf, sizeof(aBuf), "Str: %s | Reas: %s | Time: %d%s", Words.String(), Words.Reason(), Words.Time(), Words.ExactMatch() ? " | Exact: 1" : "");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
	}
}

void CGameContext::ConShopListItems(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_Shop.ListItems();
}

void CGameContext::ConShopEditItem(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_Shop.EditItem(pResult->GetString(0), pResult->GetInteger(1), pResult->GetInteger(2));
}

void CGameContext::ConShopReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_Shop.ResetItems();
}

void CGameContext::ConShopBuyItem(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!CheckClientId(ClientId))
		return;
	const char *pItem = pResult->GetString(0);
	pSelf->m_Shop.BuyItem(ClientId, pItem);
}

void CGameContext::ConToggleItem(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[ClientId];

	if(!pPl)
		return;

	const char *pItem = pResult->GetString(0);
	const int Value = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1;

	pPl->ToggleItem(pItem, Value);
}

void CGameContext::ConRainbowBody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_RainbowBody;
	pPl->SetRainbowBody(Set);
	log_info("cosmetics", "Set rainbow body to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConRainbowFeet(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_RainbowFeet;
	pPl->SetRainbowFeet(Set);
	log_info("cosmetics", "Set rainbow feet to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConRainbowSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	char aBuf[64];
	if(pResult->NumArguments() < 2)
	{
		str_format(aBuf, sizeof(aBuf), "Speed: %d", pPl->m_Cosmetics.m_RainbowSpeed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", aBuf);
	}
	else
	{
		int Speed = std::clamp(pResult->GetInteger(1), 1, 200);
		str_format(aBuf, sizeof(aBuf), "Rainbow speed for '%s' changed to %d", pSelf->Server()->ClientName(Victim), Speed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", aBuf);
		pPl->m_Cosmetics.m_RainbowSpeed = Speed;
	}
}

void CGameContext::ConSparkle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_Sparkle;
	pPl->SetSparkle(Set);
	log_info("cosmetics", "Set sparkle to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConDotTrail(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Trail = pPl->m_Cosmetics.m_Trail == TRAIL_DOT ? TRAIL_NONE : TRAIL_DOT;

	pPl->SetTrail(Trail);
	log_info("cosmetics", "Set trail to %d for player %s", Trail, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConStarTrail(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Trail = pPl->m_Cosmetics.m_Trail == TRAIL_STAR ? TRAIL_NONE : TRAIL_STAR;

	pPl->SetTrail(Trail);
	log_info("cosmetics", "Set rainbow feet to %d for player %s", Trail, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConInverseAim(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_InverseAim;
	pPl->SetInverseAim(Set);
	log_info("cosmetics", "Set inverse aim to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConLovely(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_Lovely;
	pPl->SetLovely(Set);
	log_info("cosmetics", "Set lovely to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConRotatingBall(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_RotatingBall;
	pPl->SetRotatingBall(Set);
	log_info("cosmetics", "Set rotating ball to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConEpicCircle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_EpicCircle;
	pPl->SetEpicCircle(Set);
	log_info("cosmetics", "Set epic circle to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConBloody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_Bloody;
	pPl->SetBloody(Set);
	log_info("cosmetics", "Set bloody to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConHeartHat(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_HeartHat;
	pPl->SetHeartHat(Set);
	log_info("cosmetics", "Set heart hat to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConDeathEffect(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(1) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Type = pResult->NumArguments() < 1 ? 0 : pResult->GetInteger(0);
	pPl->SetDeathEffect(Type);
	log_info("cosmetics", "Set death effect to %d for player %s", Type, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConDamageIndEffect(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Victim = pResult->NumArguments() > 1 ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(1) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Type = pResult->NumArguments() < 1 ? 0 : pResult->GetInteger(0);
	pPl->SetDamageIndType(Type);
	log_info("cosmetics", "Set damage ind to %d for player %s", Type, pSelf->Server()->ClientName(Victim));
}

// Not Available Normally
void CGameContext::ConStaffInd(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_StaffInd;
	pPl->SetStaffInd(Set);
	log_info("cosmetics", "Set staff indicator to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConHookPower(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(1) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Power = -1;
	if(pResult->NumArguments() && pSelf->IsValidHookPower(pResult->GetInteger(0)))
		Power = pResult->GetInteger(0);
	if(Power == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "~~~ Hook Powers ~~~");
		for(int i = 0; i < NUM_HOOKS; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%d = %s", i, pSelf->HookTypeName(i));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hook-power", aBuf);
		}
	}
	else
	{
		if(pPl->m_Cosmetics.m_HookPower == Power)
			Power = HOOK_NORMAL;
		pPl->HookPower(Power);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hook-power", pSelf->HookTypeName(Power));
	}
}

void CGameContext::ConInvisible(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Invisible;
	pPl->SetInvisible(Set);
	log_info("cosmetics", "Set invisible to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConStrongBloody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_StrongBloody;
	pPl->SetStrongBloody(Set);
	log_info("cosmetics", "Set strong bloody to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSnake(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	bool Set = !pChr->m_Snake.Active();
	pChr->SetSnake(!pChr->m_Snake.Active());
	log_info("cosmetics", "Set snake to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConHideCosmetics(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;
	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_HideCosmetics;
	pPl->SetHideCosmetics(Set);
	log_info("cosmetics", "Set hide cosmetics to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConHidePowerUps(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;
	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_HidePowerUps;
	pPl->SetHidePowerUps(Set);
	log_info("cosmetics", "Set hide powerups to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetEmoticonGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();

	if(pResult->GetInteger(1) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	int Set = pResult->GetInteger(0);
	pPl->SetEmoticonGun(Set);
	log_info("cosmetics", "Set emote gun to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConPhaseGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	bool Set = !pPl->m_Cosmetics.m_PhaseGun;
	pPl->SetPhaseGun(Set);
	log_info("cosmetics", "Set phase gun to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetConfettiGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;
	bool Set = !pPl->m_Cosmetics.m_ConfettiGun;
	pPl->SetConfettiGun(Set);
	log_info("cosmetics", "Set confetti gun to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetPickupPet(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;
	bool Set = !pPl->m_Cosmetics.m_PickupPet;
	pPl->SetPickupPet(Set);
	log_info("cosmetics", "Set pickup pet to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetUfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr || g_Config.m_SvAutoUfo)
		return;

	bool Set = !pChr->m_Ufo.Active();
	pChr->SetUfo(Set);
	log_info("cosmetics", "Set ufo to %d for player %s", Set, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetPlayerName(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->Server()->OverrideClientName(Victim, pResult->GetString(1));
	log_info("server", "changed player '%s's changed name to '%s'", pSelf->Server()->ClientName(Victim), pResult->GetString(1));
}

void CGameContext::ConSetPlayerClan(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->Server()->SetClientClan(Victim, pResult->GetString(1));
	log_info("server", "changed player '%s's changed clan to '%s'", pSelf->Server()->ClientName(Victim), pResult->GetString(1));
}

void CGameContext::ConSetPlayerSkin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	str_copy(pChr->GetPlayer()->m_TeeInfos.m_aSkinName, pResult->GetString(1));
	log_info("server", "changed player '%s's changed skin to '%s'", pSelf->Server()->ClientName(Victim), pResult->GetString(1));
}

void CGameContext::ConSetPlayerCustomColor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->GetPlayer()->m_TeeInfos.m_UseCustomColor = pResult->GetInteger(1);
	log_info("server", "changed player '%s's changed custom color to '%d'", pSelf->Server()->ClientName(Victim), pResult->GetInteger(1));
}

void CGameContext::ConSetPlayerColorBody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->GetPlayer()->m_TeeInfos.m_ColorBody = pResult->GetInteger(1);
	log_info("server", "changed player '%s's changed body color to '%d'", pSelf->Server()->ClientName(Victim), pResult->GetInteger(1));
}

void CGameContext::ConSetPlayerColorFeet(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->GetPlayer()->m_TeeInfos.m_ColorFeet = pResult->GetInteger(1);
	log_info("server", "changed player '%s's changed feet color to '%d'", pSelf->Server()->ClientName(Victim), pResult->GetInteger(1));
}

void CGameContext::ConSetPlayerAfk(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 0)
		Victim = pResult->GetVictim();
	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	bool Afk = !pChr->GetPlayer()->IsAfk();
	if(pResult->NumArguments() > 1)
		Afk = pResult->GetInteger(1);

	pChr->GetPlayer()->SetInitialAfk(Afk);
	log_info("server", "changed player '%s's afk status to '%d'", pSelf->Server()->ClientName(Victim), Afk);
}

void CGameContext::ConSetAbility(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];

	if(!pPlayer)
		return;

	int Ability = pResult->GetInteger(1);
	pPlayer->SetAbility(Ability);
	log_info("server", "Set ability to %d for player %s", Ability, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConIgnoreGameLayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];

	if(!pPlayer)
		return;

	pPlayer->SetIgnoreGameLayer(!pPlayer->m_IgnoreGamelayer);
	log_info("server", "Set ignore game layer to %d for player %s", pPlayer->m_IgnoreGamelayer, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConSetVanish(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	pPl->m_Vanish = !pPl->m_Vanish;

	char PlayerInfo[512] = " (No Client Info)";
	char aBuf[128];

	if(!pPl->m_Vanish)
	{
		IServer::CClientInfo Info;
		if(pSelf->Server()->GetClientInfo(Victim, &Info) && Info.m_GotDDNetVersion)
			str_format(PlayerInfo, sizeof(PlayerInfo), "(%s %d)", pSelf->Server()->GetCustomClient(Victim), Info.m_DDNetVersion);

		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s %s", pSelf->Server()->ClientName(Victim), pSelf->m_pController->GetTeamName(pPl->GetTeam()), PlayerInfo);
	}
	else
		str_format(aBuf, sizeof(aBuf), "'%s' has left the game", pSelf->Server()->ClientName(Victim));

	pSelf->SendChat(-1, 0, aBuf, -1, CGameContext::FLAG_SIX);
}

void CGameContext::ConSetVanishQuiet(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPl = pSelf->m_apPlayers[Victim];

	if(!pPl)
		return;

	pPl->m_Vanish = !pPl->m_Vanish;
}

void CGameContext::ConSetObfuscated(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if(!pPlayer)
		return;

	pPlayer->SetObfuscated(!pPlayer->m_Obfuscated);
	log_info("server", "Set obfuscated to %d for player %s", pPlayer->m_Obfuscated, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConIncludeInServerInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if(!pPlayer)
		return;

	int Include = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -2;

	if(Include == -2)
	{
		pPlayer->m_IncludeServerInfo = pPlayer->m_IncludeServerInfo == 0 ? 1 : 0;
	}
	else
	{
		Include = std::clamp(Include, 0, 1);
		pPlayer->m_IncludeServerInfo = Include;
	}
}

void CGameContext::ConRedirectClient(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	pSelf->Server()->RedirectClient(Victim, pResult->GetInteger(1));
}

void CGameContext::ConSetPassive(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->SetPassive(!pChr->Core()->m_Passive);
}

void CGameContext::ConSetHittable(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->SetHittable(!pChr->Core()->m_Hittable);
}

void CGameContext::ConSetHookable(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->SetHookable(!pChr->Core()->m_Hookable);
}

void CGameContext::ConSetCollidable(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->SetCollidable(!pChr->Core()->m_Collidable);
}

void CGameContext::ConSetTuneOverride(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Victim = pResult->m_ClientId;
	if(pResult->NumArguments() > 1)
		Victim = pResult->GetVictim();

	if(pResult->GetInteger(1) == -1)
		Victim = pResult->m_ClientId;

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	pChr->SetTuneOverride(pResult->GetInteger(0));
}

void CGameContext::ConTelekinesisImmunity(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->m_ClientId : pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];

	if(!pPlayer)
		return;

	pPlayer->SetTelekinesisImmunity(!pPlayer->m_TelekinesisImmunity);
	log_info("server", "Set telekinesis immunity to %d for player %s", pPlayer->m_TelekinesisImmunity, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConTelekinesis(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->m_ClientId : pResult->GetVictim();

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	const int Weapon = WEAPON_TELEKINESIS;
	const bool GotWeapon = pChr->GetWeaponGot(Weapon);

	if(GotWeapon)
		pChr->GiveWeapon(Weapon, true);
	else
	{
		pChr->GiveWeapon(Weapon);
		pChr->SetActiveWeapon(Weapon);
	}
}

void CGameContext::ConHeartGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->m_ClientId : pResult->GetVictim();

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	const int Weapon = WEAPON_HEARTGUN;
	const bool GotWeapon = pChr->GetWeaponGot(Weapon);

	if(GotWeapon)
		pChr->GiveWeapon(Weapon, true);
	else
	{
		pChr->GiveWeapon(Weapon);
		pChr->SetActiveWeapon(Weapon);
	}
}

void CGameContext::ConLightsaber(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->m_ClientId : pResult->GetVictim();

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;
	const int Weapon = WEAPON_LIGHTSABER;
	const bool GotWeapon = pChr->GetWeaponGot(Weapon);

	if(GotWeapon)
		pChr->GiveWeapon(Weapon, true);
	else
	{
		pChr->GiveWeapon(Weapon);
		pChr->SetActiveWeapon(Weapon);
	}
}

void CGameContext::ConPortalGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->m_ClientId : pResult->GetVictim();

	CCharacter *pChr = pSelf->GetPlayerChar(Victim);

	if(!pChr)
		return;

	const int Weapon = WEAPON_PORTALGUN;
	const bool GotWeapon = pChr->GetWeaponGot(Weapon);

	if(GotWeapon)
		pChr->GiveWeapon(Weapon, true);
	else
	{
		pChr->GiveWeapon(Weapon);
		pChr->SetActiveWeapon(Weapon);
	}
}

void CGameContext::ConSetSpiderHook(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientId;

	if(pResult->GetInteger(0) == -1)
		Victim = pResult->m_ClientId;

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if(!pPlayer)
		return;

	pPlayer->m_SpiderHook = !pPlayer->m_SpiderHook;
}

void CGameContext::ConSendFakeMessage(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	const char *pMsg = pResult->GetString(1);
	pSelf->AddFakeMessage(pName, pMsg, "Robot");
}

void CGameContext::ConToggleMapVoteLock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_MapVoteLock = !pSelf->m_MapVoteLock;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(pSelf->m_apPlayers[ClientId])
		{
			const int VoteMenuPage = pSelf->m_VoteMenu.GetPage(ClientId);
			if(VoteMenuPage == PAGE_VOTES)
				pSelf->ClearVotes(ClientId);
		}
	}
}

void CGameContext::ConInsertRecord(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	const char *pMap = pResult->GetString(1);
	float Time = pResult->GetFloat(2);
	pSelf->Score()->InsertPlayerRecord(pResult->m_ClientId, pName, pMap, Time);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(pSelf->Server()->ClientIngame(ClientId) && !str_comp(pName, pSelf->Server()->ClientName(ClientId)))
		{
			pSelf->Score()->PlayerData(ClientId)->Reset();
			pSelf->m_apPlayers[ClientId]->m_Score.reset();
			pSelf->Score()->LoadPlayerData(ClientId);
			return;
		}
	}
}

void CGameContext::ConRemoveRecord(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	const char *pMap = pResult->GetString(1);
	pSelf->Score()->RemovePlayerRecords(pName, pMap);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(pSelf->Server()->ClientIngame(ClientId) && !str_comp(pName, pSelf->Server()->ClientName(ClientId)))
		{
			pSelf->Score()->PlayerData(ClientId)->Reset();
			pSelf->m_apPlayers[ClientId]->m_Score.reset();
			pSelf->Score()->LoadPlayerData(ClientId);
			return;
		}
	}
}
void CGameContext::ConRemoveRecordWithTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	const char *pMap = pResult->GetString(1);
	float Time = pResult->GetFloat(2);
	pSelf->Score()->RemovePlayerRecordWithTime(pName, pMap, Time);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(pSelf->Server()->ClientIngame(ClientId) && !str_comp(pName, pSelf->Server()->ClientName(ClientId)))
		{
			pSelf->Score()->PlayerData(ClientId)->Reset();
			pSelf->m_apPlayers[ClientId]->m_Score.reset();
			pSelf->Score()->LoadPlayerData(ClientId);
			return;
		}
	}
}

void CGameContext::ConRemoveAllRecords(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	pSelf->Score()->RemoveAllPlayerRecords(pName);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(pSelf->Server()->ClientIngame(ClientId) && !str_comp(pName, pSelf->Server()->ClientName(ClientId)))
		{
			pSelf->Score()->PlayerData(ClientId)->Reset();
			pSelf->m_apPlayers[ClientId]->m_Score.reset();
			pSelf->Score()->LoadPlayerData(ClientId);
			return;
		}
	}
}

void CGameContext::ConDropWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientId);
	if(!pChr)
		return;

	vec2 Dir = normalize(vec2(pChr->Input()->m_TargetX, pChr->Input()->m_TargetY));
	int Type = pChr->Core()->m_ActiveWeapon;

	pChr->DropWeapon(Type, pChr->Core()->m_Vel * 0.7f + Dir * vec2(5.0f, 6.0f));
}

void CGameContext::ConCleanDroppedPickups(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
		if(pPlayer)
			pPlayer->m_vPickupDrops.clear();
	}
	// clean all existing pickupdrops
	pSelf->m_World.RemoveEntities(CGameWorld::ENTTYPE_PICKUPDROP);
}

void CGameContext::ConRepredict(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientId];
	if(!pPlayer)
		return;

	int PredMargin = pResult->NumArguments() ? pResult->GetInteger(0) : 6;

	pPlayer->Repredict(PredMargin);
}

void CGameContext::RegisterFoxNetCommands()
{
	Console()->Register("chat_string_add", "s[string] s[reason] i[should Ban] i[bantime] ?f[addition]", CFGFLAG_SERVER, ConAddChatDetectionString, this, "Add a string to the chat detection list");
	Console()->Register("chat_string_remove", "s[name]", CFGFLAG_SERVER, ConRemoveChatDetectionString, this, "Remove a string from the chat detection list");
	Console()->Register("chat_strings_list", "", CFGFLAG_SERVER, ConListChatDetectionStrings, this, "List all strings on the list");
	Console()->Register("chat_string_clear", "", CFGFLAG_SERVER, ConClearChatDetectionStrings, this, "List all strings on the list");

	Console()->Register("name_string_add", "s[name] s[reason] i[bantime] ?i[exact name]", CFGFLAG_SERVER, ConAddNameDetectionString, this, "Add a string to the name detection list");
	Console()->Register("name_string_remove", "s[name]", CFGFLAG_SERVER, ConRemoveNameDetectionString, this, "Remove a string from the name detection list");
	Console()->Register("name_strings_list", "", CFGFLAG_SERVER, ConListNameDetectionStrings, this, "List all strings on the list");
	Console()->Register("name_string_clear", "", CFGFLAG_SERVER, ConClearNameDetectionStrings, this, "List all strings on the list");

	Console()->Register("snake", "?v[id]", CFGFLAG_SERVER, ConSnake, this, "Makes a player (id) a Snake");
	Console()->Register("ufo", "?v[id]", CFGFLAG_SERVER, ConSetUfo, this, "Puts player (id) int an UFO");

	Console()->Register("set_name", "v[id] s[name]", CFGFLAG_SERVER, ConSetPlayerName, this, "Set a players (id) Name");
	Console()->Register("set_clan", "v[id] s[clan]", CFGFLAG_SERVER, ConSetPlayerClan, this, "Set a players (id) Clan");
	Console()->Register("set_skin", "v[id] s[skin]", CFGFLAG_SERVER, ConSetPlayerSkin, this, "Set a players (id) Skin");
	Console()->Register("set_custom_color", "v[id] i[int]", CFGFLAG_SERVER, ConSetPlayerCustomColor, this, "Whether a player (id) uses custom color (1 = true | 0 = false)");
	Console()->Register("set_color_body", "v[id] i[color]", CFGFLAG_SERVER, ConSetPlayerColorBody, this, "Set a players (id) Body Color");
	Console()->Register("set_color_feet", "v[id] i[color]", CFGFLAG_SERVER, ConSetPlayerColorFeet, this, "Set a players (id) Feet Color");
	Console()->Register("set_afk", "v[id] ?i[afk]", CFGFLAG_SERVER, ConSetPlayerAfk, this, "Set a players (id) afk status");

	Console()->Register("set_ability", "v[id] i[ability]", CFGFLAG_SERVER, ConSetAbility, this, "Set a players (id) Ability");

	Console()->Register("ignore_gamelayer", "?v[id]", CFGFLAG_SERVER, ConIgnoreGameLayer, this, "Turns off the kill-border for (id)");
	Console()->Register("invisible", "?v[id]", CFGFLAG_SERVER, ConInvisible, this, "Makes a players (id) Invisible");
	Console()->Register("vanish", "?v[id]", CFGFLAG_SERVER, ConSetVanish, this, "Completely hide player (id) from everyone on the server");
	Console()->Register("vanish_quiet", "?v[id]", CFGFLAG_SERVER, ConSetVanishQuiet, this, "Completely hide player (id) from everyone on the server without the chat join/leave message");
	Console()->Register("obfuscate", "?v[id]", CFGFLAG_SERVER, ConSetObfuscated, this, "Makes players (id) name obfuscated");
	Console()->Register("include_serverinfo", "v[id] ?i[include]", CFGFLAG_SERVER, ConIncludeInServerInfo, this, "whether a player should be in the serverinfo (true by default for everyone)");
	Console()->Register("redirect", "v[id] i[port]", CFGFLAG_SERVER, ConRedirectClient, this, "Redirect player (id) to a different Server (port)");

	Console()->Register("passive", "?v[id]", CFGFLAG_SERVER, ConSetPassive, this, "Put player (id) into passive");
	Console()->Register("hittable", "?v[id]", CFGFLAG_SERVER, ConSetHittable, this, "whether player (id) can be hit by other players");
	Console()->Register("hookable", "?v[id]", CFGFLAG_SERVER, ConSetHookable, this, "whether player (id) can be hooked by other players");
	Console()->Register("collidable", "?v[id]", CFGFLAG_SERVER, ConSetCollidable, this, "whether player (id) can collide with others");

	Console()->Register("set_tune_override", "i[zone] ?v[id]", CFGFLAG_SERVER, ConSetTuneOverride, this, "Sets the tune override for the player (id)");

	Console()->Register("telekinesis_immunity", "?v[id]", CFGFLAG_SERVER, ConTelekinesisImmunity, this, "Makes player (id) immunte to telekinesis");

	Console()->Register("telekinesis", "?v[id]", CFGFLAG_SERVER, ConTelekinesis, this, "Gives/Takes telekinses to/from player (id)");
	Console()->Register("heartgun", "?v[id]", CFGFLAG_SERVER, ConHeartGun, this, "Gives/Takes a heartgun to/from player (id)");
	Console()->Register("lightsaber", "?v[id]", CFGFLAG_SERVER, ConLightsaber, this, "Gives/Takes a lightsaber to/from player (id)");
	Console()->Register("portalgun", "?v[id]", CFGFLAG_SERVER, ConPortalGun, this, "Gives/Takes the portal gun to/from player (id)");

	Console()->Register("spider_hook", "?v[id]", CFGFLAG_SERVER, ConSetSpiderHook, this, "whether player (id) has spider hook");

	Console()->Register("fake_message", "s[name] r[msg]", CFGFLAG_SERVER, ConSendFakeMessage, this, "Sends a message as a fake player with that name");

	Console()->Register("map_vote_lock", "", CFGFLAG_SERVER, ConToggleMapVoteLock, this, "Toggle Map Vote Locking");

	// Cosmetics
	Console()->Register("c_lovely", "?v[id]", CFGFLAG_SERVER, ConLovely, this, "Makes a player (id) Lovely");
	Console()->Register("c_staff_ind", "?v[id]", CFGFLAG_SERVER, ConStaffInd, this, "Gives a player (id) a Staff Indicator");
	Console()->Register("c_epic_circle", "?v[id]", CFGFLAG_SERVER, ConEpicCircle, this, "Gives a player (id) an Epic Circle");
	Console()->Register("c_rotating_ball", "?v[id]", CFGFLAG_SERVER, ConRotatingBall, this, "Gives a player (id) a Rotating Ball");
	Console()->Register("c_bloody", "?v[id]", CFGFLAG_SERVER, ConBloody, this, "Gives a player (id) the Blody Effect");
	Console()->Register("c_strongbloody", "?v[id]", CFGFLAG_SERVER, ConStrongBloody, this, "Gives a player (id) the Strong Bloody Effect");
	Console()->Register("c_sparkle", "?v[id]", CFGFLAG_SERVER, ConSparkle, this, "Gives a player (id) the Atom Effect");
	Console()->Register("c_inverse_aim", "?v[id]", CFGFLAG_SERVER, ConInverseAim, this, "Makes a players (id) aim be inversed");
	Console()->Register("c_heart_hat", "?v[id]", CFGFLAG_SERVER, ConHeartHat, this, "Gives a player (id) a heart hat");
	Console()->Register("c_hookpower", "?i[power] ?v[id]", CFGFLAG_SERVER, ConHookPower, this, "Sets hook power for player (id)");
	Console()->Register("c_pickuppet", "?v[id]", CFGFLAG_SERVER, ConSetPickupPet, this, "Gives player (id) a pet");

	Console()->Register("c_star_trail", "?v[id]", CFGFLAG_SERVER, ConStarTrail, this, "Gives a player (id) a Star Trail");
	Console()->Register("c_dot_trail", "?v[id]", CFGFLAG_SERVER, ConDotTrail, this, "Gives a player (id) a Dot Trail");

	Console()->Register("c_rainbow_body", "?v[id]", CFGFLAG_SERVER, ConRainbowBody, this, "Makes a players (id) Body Rainbow");
	Console()->Register("c_rainbow_feet", "?v[id]", CFGFLAG_SERVER, ConRainbowFeet, this, "Makes a players (id) Feet Rainbow");
	Console()->Register("c_rainbow_speed", "?v[id] ?i[speed]", CFGFLAG_SERVER, ConRainbowSpeed, this, "Makes a players (id) Rainbow");

	Console()->Register("c_phase_gun", "?v[id]", CFGFLAG_SERVER, ConPhaseGun, this, "Gives player (id) a gun that shoots trough walls");
	Console()->Register("c_emote_gun", "i[type] ?v[id]", CFGFLAG_SERVER, ConSetEmoticonGun, this, "Set a players (id) Emoticon Gun to i[type] (1-12)");
	Console()->Register("c_confetti_gun", "?v[id]", CFGFLAG_SERVER, ConSetConfettiGun, this, "Set a players (id) Gun to shoot confetti");

	Console()->Register("c_death_type", "i[type] ?v[id]", CFGFLAG_SERVER, ConDeathEffect, this, "Set a players (id) Damage Ind Type");
	Console()->Register("c_damageind_type", "i[type] ?v[id]", CFGFLAG_SERVER, ConDamageIndEffect, this, "Set a players (id) Damage Ind Type");

	// Player configs
	Console()->Register("hide_cosmetics", "?v[id]", CFGFLAG_SERVER, ConHideCosmetics, this, "Hides Cosmetics for Player (id)");
	Console()->Register("hide_powerups", "?v[id]", CFGFLAG_SERVER, ConHideCosmetics, this, "Hides Cosmetics for Player (id)");

	// Records
	Console()->Register("record_insert", "s[name] s[map] f[time]", CFGFLAG_SERVER, ConInsertRecord, this, "insert a new record for that name on the given map with given time");
	Console()->Register("record_remove", "s[name] r[map]", CFGFLAG_SERVER, ConRemoveRecord, this, "remove all records a name has on the given map");
	Console()->Register("record_remove_time", "s[name] s[map] f[time]", CFGFLAG_SERVER, ConRemoveRecordWithTime, this, "remove records a name has on given map with given time");
	Console()->Register("record_remove_all", "r[name]", CFGFLAG_SERVER, ConRemoveAllRecords, this, "remove all records a name has");

	// Account
	Console()->Register("force_login", "r[username]", CFGFLAG_SERVER, ConAccForceLogin, this, "Force Log into any account");
	Console()->Register("force_logout", "i[id]", CFGFLAG_SERVER, ConAccForceLogout, this, "Force logout an account thats currently active on the server");
	Console()->Register("acc_edit", "s[username] s[variable] r[value]", CFGFLAG_SERVER, ConAccEdit, this, "Edit an account");
	Console()->Register("acc_disable", "s[username] i[1 | 0]", CFGFLAG_SERVER, ConAccDisable, this, "Disable an account");

	Console()->Register("register", "s[username] s[password] s[password2]", CFGFLAG_CHAT, ConAccRegister, this, "Register a account");
	Console()->Register("password", "s[oldpass] s[password] s[password2]", CFGFLAG_CHAT, ConAccPassword, this, "Change your password");
	Console()->Register("login", "s[username] r[password]", CFGFLAG_CHAT, ConAccLogin, this, "Login to your account");
	Console()->Register("logout", "", CFGFLAG_CHAT, ConAccLogout, this, "Logout of your account");
	Console()->Register("profile", "?s[name]", CFGFLAG_CHAT, ConAccProfile, this, "Show someones profile");

	Console()->Register("top5money", "?i[offset]", CFGFLAG_CHAT, ConAccTop5Money, this, "Show someones profile");
	Console()->Register("top5level", "?i[offset]", CFGFLAG_CHAT, ConAccTop5Level, this, "Show someones profile");
	Console()->Register("top5playtime", "?i[offset]", CFGFLAG_CHAT, ConAccTop5Playtime, this, "Show someones profile");

	// Shop
	Console()->Register("shop_edit_item", "s[Name] i[Price] ?i[Minimum Level]", CFGFLAG_SERVER, ConShopEditItem, this, "Edit a shop item");
	Console()->Register("shop_list_items", "", CFGFLAG_SERVER, ConShopListItems, this, "Lists all shop items");
	Console()->Register("shop_reset", "", CFGFLAG_SERVER, ConShopReset, this, "Resets all prices in the shop");

	Console()->Register("buyitem", "r[item]", CFGFLAG_CHAT, ConShopBuyItem, this, "Buy an item from the shop");
	Console()->Register("toggleitem", "s[item] ?i[value]", CFGFLAG_CHAT, ConToggleItem, this, "Toggle an Item, value is only needed for 2 items");
	Console()->Register("dropweapon", "", CFGFLAG_CHAT, ConDropWeapon, this, "Drops the weapon you're currently holding");

	Console()->Register("cleanup_pickupdrops", "", CFGFLAG_SERVER, ConCleanDroppedPickups, this, "Removes all dropped pickups");

	Console()->Register("repredict", "?i[predtime]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRepredict, this, "Recalculates the Server-Side prediction (based on Ping + pred margin)");

	Console()->Chain("sv_debug_quad_pos", ConchainQuadDebugPos, this);
	Console()->Chain("sv_solo_on_spawn", ConchainSoloOnSpawn, this);
}

void CGameContext::ConchainQuadDebugPos(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->QuadDebugIds(g_Config.m_SvDebugQuadPos);
	}
}

void CGameContext::ConchainSoloOnSpawn(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(!pResult->NumArguments() || !pResult->GetInteger(0))
		return;

	CGameContext *pSelf = (CGameContext *)pUserData;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
		if(pChr && pChr->m_SpawnSolo)
			pChr->UnSpawnSolo();
	}
}