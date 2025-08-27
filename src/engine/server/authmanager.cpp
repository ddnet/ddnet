#include "authmanager.h"
#include <base/hash_ctxt.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#define ADMIN_IDENT "default_admin"
#define MOD_IDENT "default_mod"
#define HELPER_IDENT "default_helper"

static const char *AuthLevelToName(int AuthLevel)
{
	switch(AuthLevel)
	{
	case AUTHED_ADMIN:
		return ACCESS_LEVEL_NAME_ADMIN;
	case AUTHED_MOD:
		return ACCESS_LEVEL_NAME_MODERATOR;
	case AUTHED_HELPER:
		return ACCESS_LEVEL_NAME_HELPER;
	}

	dbg_assert(false, "Invalid auth level: %d", AuthLevel);
	dbg_break();
}

static int AccessLevelNameToAuthLevel(const char *pAccessLevel)
{
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_ADMIN))
		return AUTHED_ADMIN;
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_MODERATOR))
		return AUTHED_MOD;
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_HELPER))
		return AUTHED_HELPER;

	dbg_assert(false, "Invalid access level: %s", pAccessLevel);
	dbg_break();
}

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
	AddAccessLevel(ACCESS_LEVEL_NAME_ADMIN, AUTHED_ADMIN);
	AddAccessLevel(ACCESS_LEVEL_NAME_MODERATOR, AUTHED_MOD);
	AddAccessLevel(ACCESS_LEVEL_NAME_HELPER, AUTHED_HELPER);

	size_t NumDefaultKeys = 0;
	if(g_Config.m_SvRconPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconModPassword[0])
		NumDefaultKeys++;
	if(g_Config.m_SvRconHelperPassword[0])
		NumDefaultKeys++;

	auto It = std::find_if(m_vKeys.begin(), m_vKeys.end(), [](CKey Key) { return str_comp(Key.m_aIdent, DEFAULT_SAVED_RCON_USER) == 0; });
	if(It != m_vKeys.end())
		NumDefaultKeys++;

	if(m_vKeys.size() == NumDefaultKeys && !g_Config.m_SvRconPassword[0])
	{
		secure_random_password(g_Config.m_SvRconPassword, sizeof(g_Config.m_SvRconPassword), 6);
		AddDefaultKey(ACCESS_LEVEL_NAME_ADMIN, g_Config.m_SvRconPassword);
		m_Generated = true;
	}
}

int CAuthManager::AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pAccessLevel)
{
	if(FindKey(pIdent) >= 0)
		return -1;

	CKey Key;
	str_copy(Key.m_aIdent, pIdent);
	Key.m_Pw = Hash;
	mem_copy(Key.m_aSalt, pSalt, SALT_BYTES);
	Key.m_pAccessLevel = FindAccessLevelByName(pAccessLevel);

	m_vKeys.push_back(Key);
	return m_vKeys.size() - 1;
}

int CAuthManager::AddKey(const char *pIdent, const char *pPw, const char *pAccessLevel)
{
	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	return AddKeyHash(pIdent, HashPassword(pPw, aSalt), aSalt, pAccessLevel);
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

int CAuthManager::DefaultKey(const char *pAccessLevel) const
{
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_ADMIN))
		return m_aDefault[std::size(m_aDefault) - AUTHED_ADMIN];
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_MODERATOR))
		return m_aDefault[std::size(m_aDefault) - AUTHED_MOD];
	if(!str_comp(pAccessLevel, ACCESS_LEVEL_NAME_HELPER))
		return m_aDefault[std::size(m_aDefault) - AUTHED_HELPER];
	return 0;
}

int CAuthManager::KeyLevel(int Slot) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return false;
	return m_vKeys[Slot].m_pAccessLevel->AuthLevel();
}

const char *CAuthManager::KeyIdent(int Slot) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return nullptr;
	return m_vKeys[Slot].m_aIdent;
}

bool CAuthManager::IsValidIdent(const char *pIdent) const
{
	return str_length(pIdent) < (int)sizeof(CKey().m_aIdent);
}

void CAuthManager::UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pAccessLevel)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	CKey *pKey = &m_vKeys[Slot];
	pKey->m_Pw = Hash;
	mem_copy(pKey->m_aSalt, pSalt, SALT_BYTES);
	pKey->m_pAccessLevel = FindAccessLevelByName(pAccessLevel);
}

void CAuthManager::UpdateKey(int Slot, const char *pPw, const char *pAccessLevel)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	UpdateKeyHash(Slot, HashPassword(pPw, aSalt), aSalt, pAccessLevel);
}

void CAuthManager::ListKeys(FListCallback pfnListCallback, void *pUser)
{
	for(auto &Key : m_vKeys)
		pfnListCallback(Key.m_aIdent, Key.m_pAccessLevel->AuthLevel(), pUser);
}

void CAuthManager::AddDefaultKey(const char *pAccessLevel, const char *pPw)
{
	int Level = AccessLevelNameToAuthLevel(pAccessLevel);
	if(Level < AUTHED_HELPER || Level > AUTHED_ADMIN)
		return;

	static const char s_aaIdents[3][sizeof(HELPER_IDENT)] = {ADMIN_IDENT, MOD_IDENT, HELPER_IDENT};
	int Index = AUTHED_ADMIN - Level;
	if(m_aDefault[Index] >= 0)
		return; // already exists
	m_aDefault[Index] = AddKey(s_aaIdents[Index], pPw, AuthLevelToName(Level));
}

bool CAuthManager::IsGenerated() const
{
	return m_Generated;
}

int CAuthManager::NumNonDefaultKeys() const
{
	int DefaultCount = std::count_if(std::begin(m_aDefault), std::end(m_aDefault), [](int Slot) {
		return Slot >= 0;
	});
	return m_vKeys.size() - DefaultCount;
}

CAccessLevel *CAuthManager::FindAccessLevelByName(const char *pName)
{
	for(CAccessLevel &AccessLevel : m_vAccessLevels)
		if(!str_comp(AccessLevel.Name(), pName))
			return &AccessLevel;
	return nullptr;
}

bool CAuthManager::AddAccessLevel(const char *pName, int Level)
{
	if(FindAccessLevelByName(pName))
		return false;

	m_vAccessLevels.emplace_back(pName, Level);
	return true;
}
