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

void CGameContext::ConForceLogin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pName = pResult->GetString(0);
	pSelf->m_AccountManager.ForceLogin(pResult->m_ClientId, pName);
}