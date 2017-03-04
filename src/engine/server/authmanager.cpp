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
	if(m_aKeys.size() == 0 && !g_Config.m_SvRconPassword[0])
	{
		secure_random_password(g_Config.m_SvRconPassword, sizeof(g_Config.m_SvRconPassword), 6);
		m_Generated = true;
	}


	if(g_Config.m_SvRconPassword[0])
		AddAdminKey(g_Config.m_SvRconPassword);
	if(g_Config.m_SvRconModPassword[0])
		AddModKey(g_Config.m_SvRconModPassword);
	if (g_Config.m_SvRconHelperPassword[0])
		AddHelperKey(g_Config.m_SvRconHelperPassword);
}

int CAuthManager::AddKeyHash(const char *pIdent, const unsigned char *pHash, const unsigned char *pSalt, int AuthLevel)
{
	CKey Key;
	str_copy(Key.m_aIdent, pIdent, sizeof Key.m_aIdent);
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

	//Generate random salt
	secure_random_fill(aSalt, SALT_BYTES);

	//Hash the password and the salt
	md5_init(&ctx);
	md5_append(&ctx, (unsigned char *)pPw, str_length(pPw));
	md5_append(&ctx, aSalt, SALT_BYTES);
	md5_finish(&ctx, aHash);

	return AddKeyHash(pIdent, aHash, aSalt, AuthLevel);
}

void CAuthManager::RemoveKey(int Slot)
{
	m_aKeys.remove_index_fast(Slot);
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
	dbg_assert(Slot < 0 || Slot > m_aKeys.size(), "indice out of bounds");

	md5_state_t ctx;
	unsigned char aHash[MD5_BYTES];

	//Hash the password and the salt
	md5_init(&ctx);
	md5_append(&ctx, (unsigned char*)pPw, str_length(pPw));
	md5_append(&ctx, m_aKeys[Slot].m_aSalt, SALT_BYTES);
	md5_finish(&ctx, aHash);

	return !mem_comp(m_aKeys[Slot].m_aPw, aHash, MD5_BYTES);
}

int CAuthManager::DefaultKey(int AuthLevel)
{
	dbg_assert(AuthLevel < 0 || AuthLevel > AUTHED_ADMIN, "auth level invalid");
	return m_aDefault[AUTHED_ADMIN - AuthLevel];
}

int CAuthManager::KeyLevel(int Slot)
{
	dbg_assert(Slot < 0 || Slot > m_aKeys.size(), "indice out of bounds");
	return m_aKeys[Slot].m_Level;
}

const char *CAuthManager::KeyIdent(int Slot)
{
	dbg_assert(Slot < 0 || Slot > m_aKeys.size(), "indice out of bounds");
	return m_aKeys[Slot].m_aIdent;
}

void CAuthManager::UpdateKeyHash(int Slot, const unsigned char *pHash, const unsigned char *pSalt, int AuthLevel)
{
	dbg_assert(Slot < 0 || Slot > m_aKeys.size(), "indice out of bounds");

	CKey *pKey = &m_aKeys[Slot];
	mem_copy(pKey->m_aPw, pHash, MD5_BYTES);
	mem_copy(pKey->m_aSalt, pSalt, SALT_BYTES);
	pKey->m_Level = AuthLevel;
}

void CAuthManager::UpdateKey(int Slot, const char *pPw, int AuthLevel)
{
	dbg_assert(Slot < 0 || Slot > m_aKeys.size(), "indice out of bounds");

	md5_state_t ctx;
	unsigned char aHash[MD5_BYTES];
	unsigned char aSalt[SALT_BYTES];

	//Generate random salt
	secure_random_fill(aSalt, SALT_BYTES);

	//Hash the password and the salt
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

void CAuthManager::AddAdminKey(const char *pPw)
{
	m_aDefault[0] = AddKey(ADMIN_IDENT, pPw, AUTHED_ADMIN);
}

void CAuthManager::AddModKey(const char *pPw)
{
	m_aDefault[1] = AddKey(MOD_IDENT, pPw, AUTHED_MOD);
}

void CAuthManager::AddHelperKey(const char *pPw)
{
	m_aDefault[2] = AddKey(HELPER_IDENT, pPw, AUTHED_HELPER);
}

bool CAuthManager::IsGenerated()
{
	return m_Generated;
}