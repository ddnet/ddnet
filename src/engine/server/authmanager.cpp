#include "authmanager.h"
#include <base/hash_ctxt.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#define ADMIN_IDENT "default_admin"
#define MOD_IDENT "default_mod"
#define HELPER_IDENT "default_helper"

static MD5_DIGEST HashPassword(const char *pPassword, const unsigned char aSalt[SALT_BYTES])
{
	// Hash the password and the salt
	MD5_CTX Md5;
	md5_init(&Md5);
	md5_update(&Md5, (unsigned char *)pPassword, str_length(pPassword));
	md5_update(&Md5, aSalt, SALT_BYTES);
	return md5_finish(&Md5);
}

CAuthManager::CAuthManager()
{
	m_aDefault[0] = -1;
	m_aDefault[1] = -1;
	m_aDefault[2] = -1;
	m_Generated = false;
}

void CAuthManager::Init()
{
	size_t NumDefaultKeys = 0;
	if(g_Config.m_SvRconPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconModPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconHelperPassword[0])
		NumDefaultKeys++;
	if(m_vKeys.size() == NumDefaultKeys && !g_Config.m_SvRconPassword[0])
	{
		secure_random_password(g_Config.m_SvRconPassword, sizeof(g_Config.m_SvRconPassword), 6);
		AddDefaultKey(AUTHED_ADMIN, g_Config.m_SvRconPassword);
		m_Generated = true;
	}
}

int CAuthManager::AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel)
{
	if(FindKey(pIdent) >= 0)
		return -1;

	CKey Key;
	str_copy(Key.m_aIdent, pIdent);
	Key.m_Pw = Hash;
	mem_copy(Key.m_aSalt, pSalt, SALT_BYTES);
	Key.m_Level = AuthLevel;

	m_vKeys.push_back(Key);
	return m_vKeys.size() - 1;
}

int CAuthManager::AddKey(const char *pIdent, const char *pPw, int AuthLevel)
{
	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	return AddKeyHash(pIdent, HashPassword(pPw, aSalt), aSalt, AuthLevel);
}

void CAuthManager::RemoveKey(int Slot)
{
	m_vKeys.erase(m_vKeys.begin() + Slot);
	// Update indices of default keys
	for(int &Default : m_aDefault)
	{
		if(Default == Slot)
		{
			Default = -1;
		}
		else if(Default > Slot)
		{
			--Default;
		}
	}
}

int CAuthManager::FindKey(const char *pIdent) const
{
	for(size_t i = 0; i < m_vKeys.size(); i++)
		if(!str_comp(m_vKeys[i].m_aIdent, pIdent))
			return i;

	return -1;
}

bool CAuthManager::CheckKey(int Slot, const char *pPw) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return false;
	return m_vKeys[Slot].m_Pw == HashPassword(pPw, m_vKeys[Slot].m_aSalt);
}

int CAuthManager::DefaultKey(int AuthLevel) const
{
	if(AuthLevel < 0 || AuthLevel > AUTHED_ADMIN)
		return 0;
	return m_aDefault[AUTHED_ADMIN - AuthLevel];
}

int CAuthManager::KeyLevel(int Slot) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return false;
	return m_vKeys[Slot].m_Level;
}

const char *CAuthManager::KeyIdent(int Slot) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return nullptr;
	return m_vKeys[Slot].m_aIdent;
}

void CAuthManager::UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	CKey *pKey = &m_vKeys[Slot];
	pKey->m_Pw = Hash;
	mem_copy(pKey->m_aSalt, pSalt, SALT_BYTES);
	pKey->m_Level = AuthLevel;
}

void CAuthManager::UpdateKey(int Slot, const char *pPw, int AuthLevel)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	UpdateKeyHash(Slot, HashPassword(pPw, aSalt), aSalt, AuthLevel);
}

void CAuthManager::ListKeys(FListCallback pfnListCallback, void *pUser)
{
	for(auto &Key : m_vKeys)
		pfnListCallback(Key.m_aIdent, Key.m_Level, pUser);
}

void CAuthManager::AddDefaultKey(int Level, const char *pPw)
{
	if(Level < AUTHED_HELPER || Level > AUTHED_ADMIN)
		return;

	static const char s_aaIdents[3][sizeof(HELPER_IDENT)] = {ADMIN_IDENT, MOD_IDENT, HELPER_IDENT};
	int Index = AUTHED_ADMIN - Level;
	if(m_aDefault[Index] >= 0)
		return; // already exists
	m_aDefault[Index] = AddKey(s_aaIdents[Index], pPw, Level);
}

bool CAuthManager::IsGenerated() const
{
	return m_Generated;
}

int CAuthManager::NumNonDefaultKeys() const
{
	int DefaultCount = (m_aDefault[0] >= 0) + (m_aDefault[1] >= 0) + (m_aDefault[2] >= 0);
	return m_vKeys.size() - DefaultCount;
}
