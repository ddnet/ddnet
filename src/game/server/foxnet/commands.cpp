#include <base/system.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <base/log.h>

void CGameContext::ConAccRegister(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!g_Config.m_SvAccounts)
	{
		pSelf->SendChatTarget(ClientId, "Account registration is disabled.");
		return;
	}

	if(pSelf->m_Account[ClientId].m_LoggedIn)
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

	if(!pSelf->m_Account[ClientId].m_LoggedIn)
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

	if(pSelf->m_Account[ClientId].m_LoggedIn)
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

void CGameContext::ConAccEdit(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pUser = pResult->GetString(0);
	const char *pVariable = pResult->GetString(1);
	const char *pValue = pResult->GetString(2);

	pSelf->m_AccountManager.EditAccount(pUser, pVariable, pValue);
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

	for(const auto &Words : m_ChatDetection)
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
		m_ChatDetection.push_back(CStringDetection(pString, pReason, pAddition, pBan, pBanTime));
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Chat Detection List", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
	}
}

void CGameContext::ConClearChatDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_ChatDetection.clear();
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

	if(m_ChatDetection.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(auto it = m_ChatDetection.begin(); it != m_ChatDetection.end(); ++it)
	{
		if(!str_comp_nocase(it->String(), pString))
		{
			m_ChatDetection.erase(it);
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Chat Detection List", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", aBuf);
			return;
		}
	}
}

void CGameContext::ConListChatDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pSelf->m_ChatDetection.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(const auto &Words : pSelf->m_ChatDetection)
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

	for(const auto &Words : m_NameDetection)
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
		m_NameDetection.push_back(CStringDetection(pString, pReason, 1, pBanTime, ExactName));
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Name Detection List", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
	}
}

void CGameContext::ConClearNameDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_NameDetection.clear();
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
	if(m_NameDetection.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(auto it = m_NameDetection.begin(); it != m_NameDetection.end(); ++it)
	{
		if(!str_comp(it->String(), pString))
		{
			m_NameDetection.erase(it);
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Name Detection List", pString);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
			return;
		}
	}
}

void CGameContext::ConListNameDetectionStrings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pSelf->m_NameDetection.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", "List is Empty");
		return;
	}

	char aBuf[512];
	for(const auto &Words : pSelf->m_NameDetection)
	{
		if(Words.String()[0] == '\0')
			continue;
		str_copy(aBuf, "");

		str_format(aBuf, sizeof(aBuf), "Str: %s | Reas: %s | Time: %d%s", Words.String(), Words.Reason(), Words.Time(), Words.ExactMatch() ? " | Exact: 1" : "");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name-detection", aBuf);
	}
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

	Console()->Register("buyitem", "r[item]", CFGFLAG_CHAT, ConShopBuyItem, this, "Buy an item from the shop");
	Console()->Register("toggleitem", "s[item] ?i[value]", CFGFLAG_CHAT, ConToggleItem, this, "Toggle an Item, value is only needed for 2 items");

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

	Console()->Register("c_snake", "?v[id]", CFGFLAG_SERVER, ConSnake, this, "Makes a player (id) a Snake");
	Console()->Register("c_ufo", "?v[id]", CFGFLAG_SERVER, ConSetUfo, this, "Puts player (id) int an UFO");

	Console()->Register("c_hide_cosmetics", "?v[id]", CFGFLAG_SERVER, ConHideCosmetics, this, "Hides Cosmetics for Player (id)");

	Console()->Register("force_login", "r[username]", CFGFLAG_SERVER, ConAccForceLogin, this, "Force Log into any account");
	Console()->Register("force_logout", "i[id]", CFGFLAG_SERVER, ConAccForceLogout, this, "Force logout an account thats currently active on the server");
	Console()->Register("acc_edit", "s[username] s[variable] r[value]", CFGFLAG_SERVER, ConAccEdit, this, "Edit an account");

	Console()->Register("register", "s[username] s[password] s[password2]", CFGFLAG_CHAT, ConAccRegister, this, "Register a account");
	Console()->Register("password", "s[oldpass] s[password] s[password2]", CFGFLAG_CHAT, ConAccPassword, this, "Change your password");
	Console()->Register("login", "s[username] r[password]", CFGFLAG_CHAT, ConAccLogin, this, "Login to your account");
	Console()->Register("logout", "", CFGFLAG_CHAT, ConAccLogout, this, "Logout of your account");
	Console()->Register("profile", "?s[name]", CFGFLAG_CHAT, ConAccProfile, this, "Show someones profile");

	Console()->Chain("sv_debug_quad_pos", ConchainQuadDebugPos, this);
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