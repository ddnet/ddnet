#include <engine/external/md5/md5.h>

#include <engine/shared/config.h>
#include "authmanager.h"

#define ADMIN_IDENT "default_admin"
#define MOD_IDENT "default_mod"
#define HELPER_IDENT "default_helper"

CAuthManager::CAuthManager()
{
	m_aDefault[0] = -1;
	m_aDefault[1] = -1;
	m_aDefault[2] = -1;
	m_Generated = false;
}

void CAuthManager::Init()
{
	int NumDefaultKeys = 0;
	if(g_Config.m_SvRconPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconModPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconHelperPassword[0])
		NumDefaultKeys++;
	if(m_aKeys.size() == NumDefaultKeys && !g_Config.m_SvRconPassword[0])
	{
		secure_random_password(g_Config.m_SvRconPassword, sizeof(g_Config.m_SvRconPassword), 6);
		AddDefaultKey(AUTHED_ADMIN, g_Config.m_SvRconPassword);
		m_Generated = true;
	}
}

int CAuthManager::AddKeyHash(const char *pIdent, const unsigned char *pHash, const unsigned char *pSalt, int AuthLevel)
{
	if(FindKey(pIdent) >= 0)
		return -1;

	CKey Key;
	str_copy(Key.m_aIdent, pIdent, sizeof(Key.m_aIdent));
	mem_copy(Key.m_aPw, pHash, MD5_BYTES);
	mem_copy(Key.m_aSalt, pSalt, SALT_BYTES);
	Key.m_Level = AuthLevel;

	return m_aKeys.add(Key);
}

int CAuthManager::AddKey(const char *pIdent, const char *pPw, int AuthLevel)
{
	md5_state_t ctx;
	unsigned char aHash[MD5_BYTES];
	unsigned char aSalt[SALT_BYTES];

	// Generate random salt
	secure_random_fill(aSalt, SALT_BYTES);

	// Hash the password and the salt
	md5_init(&ctx);
	md5_append(&ctx, (unsigned char *)pPw, str_length(pPw));
	md5_append(&ctx, aSalt, SALT_BYTES);
	md5_finish(&ctx, aHash);

	return AddKeyHash(pIdent, aHash, aSalt, AuthLevel);
}

int CAuthManager::RemoveKey(int Slot)
{
	m_aKeys.remove_index_fast(Slot);
	for(int i = 0; i < (int)(sizeof(m_aDefault) / sizeof(m_aDefault[0])); i++)
	{
		if(m_aDefault[i] == Slot)
		{
			m_aDefault[i] = -1;
		}
		else if(m_aDefault[i] == m_aKeys.size())
		{
			m_aDefault[i] = Slot;
		}
	}
	return m_aKeys.size();
}

int CAuthManager::FindKey(const char *pIdent)
{
	for(int i = 0; i < m_aKeys.size(); i++)
		if(!str_comp(m_aKeys[i].m_aIdent, pIdent))
			return i;

	return -1;
}

bool CAuthManager::CheckKey(int Slot, const char *pPw)
{
	if(Slot < 0 || Slot >= m_aKeys.size())
		return false;

	md5_state_t ctx;
	unsigned char aHash[MD5_BYTES];

	// Hash the password and the salt
	md5_init(&ctx);
	md5_append(&ctx, (unsigned char*)pPw, str_length(pPw));
	md5_append(&ctx, m_aKeys[Slot].m_aSalt, SALT_BYTES);
	md5_finish(&ctx, aHash);

	return !mem_comp(m_aKeys[Slot].m_aPw, aHash, MD5_BYTES);
}

int CAuthManager::DefaultKey(int AuthLevel)
{
	if(AuthLevel < 0 || AuthLevel > AUTHED_ADMIN)
		return 0;
	return m_aDefault[AUTHED_ADMIN - AuthLevel];
}

int CAuthManager::KeyLevel(int Slot)
{
	if(Slot < 0 || Slot >= m_aKeys.size())
		return false;
	return m_aKeys[Slot].m_Level;
}

const char *CAuthManager::KeyIdent(int Slot)
{
	if(Slot < 0 || Slot >= m_aKeys.size())
		return NULL;
	return m_aKeys[Slot].m_aIdent;
}

void CAuthManager::UpdateKeyHash(int Slot, const unsigned char *pHash, const unsigned char *pSalt, int AuthLevel)
{
	if(Slot < 0 || Slot >= m_aKeys.size())
		return;

	CKey *pKey = &m_aKeys[Slot];
	mem_copy(pKey->m_aPw, pHash, MD5_BYTES);
	mem_copy(pKey->m_aSalt, pSalt, SALT_BYTES);
	pKey->m_Level = AuthLevel;
}

void CAuthManager::UpdateKey(int Slot, const char *pPw, int AuthLevel)
{
	if(Slot < 0 || Slot >= m_aKeys.size())
		return;

	md5_state_t ctx;
	unsigned char aHash[MD5_BYTES];
	unsigned char aSalt[SALT_BYTES];

	// Generate random salt
	secure_random_fill(aSalt, SALT_BYTES);

	// Hash the password and the salt
	md5_init(&ctx);
	md5_append(&ctx, (unsigned char *)pPw, str_length(pPw));
	md5_append(&ctx, aSalt, SALT_BYTES);
	md5_finish(&ctx, aHash);

	UpdateKeyHash(Slot, aHash, aSalt, AuthLevel);
}

void CAuthManager::ListKeys(FListCallback pfnListCallback, void *pUser)
{
	for(int i = 0; i < m_aKeys.size(); i++)
		pfnListCallback(m_aKeys[i].m_aIdent, m_aKeys[i].m_Level, pUser);
}

void CAuthManager::AddDefaultKey(int Level, const char *pPw)
{
	if(Level < AUTHED_HELPER || Level > AUTHED_ADMIN)
		return;

	static const char IDENTS[3][sizeof(HELPER_IDENT)] = {ADMIN_IDENT, MOD_IDENT, HELPER_IDENT};
	int Index = AUTHED_ADMIN - Level;
	if(m_aDefault[Index] >= 0)
		return; // already exists
	m_aDefault[Index] = AddKey(IDENTS[Index], pPw, Level);
}

bool CAuthManager::IsGenerated()
{
	return m_Generated;
}
