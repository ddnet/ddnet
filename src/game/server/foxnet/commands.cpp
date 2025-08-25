#include "../gamecontext.h"

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
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Login failed");
		pSelf->SendChatTarget(ClientId, aBuf);
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

void CGameContext::ConClearChatDetectionString(IConsole::IResult *pResult, void *pUserData)
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

void CGameContext::RegisterFoxNetCommands()
{
	Console()->Register("force_login", "r[username]", CFGFLAG_SERVER, ConAccForceLogin, this, "Force Log into any account");
	Console()->Register("force_logout", "i[id]", CFGFLAG_SERVER, ConAccForceLogout, this, "Force logout an account thats currently active on the server");
	Console()->Register("acc_edit", "s[username] s[variable] r[value]", CFGFLAG_SERVER, ConAccEdit, this, "Edit an account");

	Console()->Register("chat_string_add", "s[string] s[reason] i[should Ban] i[bantime] ?f[addition]", CFGFLAG_SERVER, ConAddChatDetectionString, this, "Add a string to the chat detection list");
	Console()->Register("chat_string_remove", "s[name]", CFGFLAG_SERVER, ConRemoveChatDetectionString, this, "Remove a string from the chat detection list");
	Console()->Register("chat_strings_list", "", CFGFLAG_SERVER, ConListChatDetectionStrings, this, "List all strings on the list");

	Console()->Register("name_string_add", "s[name] s[reason] i[bantime] ?i[exact name]", CFGFLAG_SERVER, ConAddNameDetectionString, this, "Add a string to the name detection list");
	Console()->Register("name_string_remove", "s[name]", CFGFLAG_SERVER, ConRemoveNameDetectionString, this, "Remove a string from the name detection list");
	Console()->Register("name_strings_list", "", CFGFLAG_SERVER, ConListNameDetectionStrings, this, "List all strings on the list");
	Console()->Register("name_string_clear", "", CFGFLAG_SERVER, ConClearNameDetectionStrings, this, "List all strings on the list");

	Console()->Register("register", "s[username] s[password] s[password2]", CFGFLAG_CHAT, ConAccRegister, this, "Register a account");
	Console()->Register("password", "s[oldpass] s[password] s[password2]", CFGFLAG_CHAT, ConAccPassword, this, "Change your password");
	Console()->Register("login", "s[username] r[password]", CFGFLAG_CHAT, ConAccLogin, this, "Login to your account");
	Console()->Register("logout", "", CFGFLAG_CHAT, ConAccLogout, this, "Logout of your account");
	Console()->Register("profile", "?s[name]", CFGFLAG_CHAT, ConAccProfile, this, "Show someones profile");
}