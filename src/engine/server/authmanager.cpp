#include "authmanager.h"

#include <base/hash_ctxt.h>
#include <base/log.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <algorithm>
#include <cstdlib>

#define ADMIN_IDENT "default_admin"
#define MOD_IDENT "default_mod"
#define HELPER_IDENT "default_helper"

void CRconRole::AddParent(CRconRole *pRole)
{
	m_vpParents.emplace_back(pRole);
}

void CRconRole::RemoveParent(CRconRole *pRole)
{
	m_vpParents.erase(std::remove(m_vpParents.begin(), m_vpParents.end(), pRole), m_vpParents.end());
}

void CRconRole::AllowKick(CRconRole *pRole)
{
	if(pRole->CanKick(this))
	{
		log_error("auth", "Failed to grant kick power because '%s' can already kick '%s'", pRole->Name(), Name());
		return;
	}
	if(CanKick(pRole))
	{
		log_warn("auth", "Role '%s' could already kick '%s'", Name(), pRole->Name());
		return;
	}
	if(IsAncestor(pRole))
	{
		log_error("auth", "Role '%s' can not kick its ancestor role '%s'", Name(), pRole->Name());
		return;
	}
	if(pRole->IsAncestor(this))
	{
		log_warn("auth", "Role '%s' can already kick its child role '%s'", Name(), pRole->Name());
		return;
	}

	m_vpCanKick.emplace_back(pRole);
}

void CRconRole::DisallowKick(CRconRole *pRole)
{
	if(std::find(m_vpCanKick.cbegin(), m_vpCanKick.cend(), pRole) != m_vpCanKick.cend())
	{
		m_vpCanKick.erase(std::remove(m_vpCanKick.begin(), m_vpCanKick.end(), pRole), m_vpCanKick.end());
	}

	if(CanKick(pRole))
	{
		log_warn("auth", "Role '%s' still inherits the power to kick '%s'", Name(), pRole->Name());
		return;
	}
}

bool CRconRole::CanKick(CRconRole *pRole) const
{
	if(pRole == nullptr)
		return true;
	if(IsAdmin())
		return true;
	if(IsAncestor(pRole))
		return false;
	if(pRole->IsAncestor(this))
		return true;
	if(std::find(m_vpCanKick.cbegin(), m_vpCanKick.cend(), pRole) != m_vpCanKick.cend())
		return true;
	return std::any_of(m_vpParents.cbegin(), m_vpParents.cend(), [pRole](const CRconRole *pParent) {
		return pParent->CanKick(pRole);
	});
}

bool CRconRole::IsParent(const CRconRole *pParent) const
{
	return std::find(m_vpParents.cbegin(), m_vpParents.cend(), pParent) != m_vpParents.cend();
}

bool CRconRole::IsAncestor(const CRconRole *pAncestor) const
{
	if(IsParent(pAncestor))
		return true;

	return std::any_of(m_vpParents.cbegin(), m_vpParents.cend(), [pAncestor](const CRconRole *pParent) {
		return pParent->IsAncestor(pAncestor);
	});
}

bool CRconRole::HasReservedSlots() const
{
	if(m_HasReservedSlots)
		return true;
	return std::any_of(m_vpParents.cbegin(), m_vpParents.cend(), [](const CRconRole *pParent) {
		return pParent->HasReservedSlots();
	});
}

bool CRconRole::CanTeleOthers() const
{
	if(m_CanTeleOthers)
		return true;
	return std::any_of(m_vpParents.cbegin(), m_vpParents.cend(), [](const CRconRole *pParent) {
		return pParent->CanTeleOthers();
	});
}

CRconRole *CRconRole::InheritCommandAccessFrom(const char *pCommand)
{
	auto It = std::find_if(m_vpParents.cbegin(), m_vpParents.cend(), [pCommand](CRconRole *pParent) {
		return pParent->CanUseRconCommand(pCommand);
	});
	if(It == m_vpParents.end())
		return nullptr;
	return *It;
}

bool CRconRole::CanUseRconCommandDirect(const char *pCommand)
{
	if(IsAdmin())
		return true;

	return std::ranges::any_of(
		m_vRconCommands,
		[pCommand](const std::string &Command) {
			return str_comp_nocase(Command.c_str(), pCommand) == 0;
		});
}

bool CRconRole::CanUseRconCommand(const char *pCommand)
{
	if(CanUseRconCommandDirect(pCommand))
		return true;
	return InheritCommandAccessFrom(pCommand) != nullptr;
}

bool CRconRole::AllowCommand(const char *pCommand)
{
	if(CanUseRconCommandDirect(pCommand))
		return false;

	m_vRconCommands.emplace_back(pCommand);
	return true;
}

bool CRconRole::DisallowCommand(const char *pCommand)
{
	if(!CanUseRconCommandDirect(pCommand))
		return false;

	m_vRconCommands.erase(std::remove(m_vRconCommands.begin(), m_vRconCommands.end(), pCommand), m_vRconCommands.end());
	return true;
}

void CRconRole::LogRconCommandAccess(int MaxLineLength)
{
	int CurrentLineLen = 0;
	char *pLine = (char *)malloc(MaxLineLength);
	pLine[0] = '\0';
	for(const std::string &Command : m_vRconCommands)
	{
		const char *pCommand = Command.c_str();
		int CmdLen = str_length(pCommand);
		if(CurrentLineLen + CmdLen + 2 < MaxLineLength)
		{
			if(CurrentLineLen > 0)
			{
				CurrentLineLen += str_length(", ");
				str_append(pLine, ", ", MaxLineLength);
			}
			CurrentLineLen += CmdLen;
			str_append(pLine, pCommand, MaxLineLength);
		}
		else
		{
			log_info("server", "%s", pLine);
			CurrentLineLen = CmdLen;
			str_copy(pLine, pCommand, MaxLineLength);
		}
	}
	if(CurrentLineLen > 0)
		log_info("server", "%s", pLine);
	free(pLine);

	for(CRconRole *pParent : m_vpParents)
	{
		if(pParent->m_vRconCommands.empty())
			continue;

		log_info("server", "Commands inherited from role '%s':", pParent->Name());
		pParent->LogRconCommandAccess(MaxLineLength);
	}
}

bool CRconRole::IsValidName(const char *pName)
{
	if(!pName)
		return false;
	for(const char *p = pName; *p; ++p)
	{
		if(!((*p >= 'a' && *p <= 'z') ||
			   (*p >= '0' && *p <= '9') ||
			   (*p == '_')))
		{
			return false;
		}
	}
	return true;
}

const char *CAuthManager::AuthLevelToRoleName(int AuthLevel)
{
	switch(AuthLevel)
	{
	case AUTHED_ADMIN:
		return RoleName::ADMIN;
	case AUTHED_MOD:
		return RoleName::MODERATOR;
	case AUTHED_HELPER:
		return RoleName::HELPER;
	}

	dbg_assert_failed("Invalid auth level: %d", AuthLevel);
}

static int RoleNameToAuthLevel(const char *pRoleName)
{
	if(!str_comp(pRoleName, RoleName::ADMIN))
		return AUTHED_ADMIN;
	if(!str_comp(pRoleName, RoleName::MODERATOR))
		return AUTHED_MOD;
	if(!str_comp(pRoleName, RoleName::HELPER))
		return AUTHED_HELPER;

	dbg_assert_failed("Invalid role name: %s", pRoleName);
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

	AddAdminRole(RoleName::ADMIN);
	AddRole(RoleName::MODERATOR);
	AddRole(RoleName::HELPER);
	RoleInherit(RoleName::MODERATOR, RoleName::HELPER);
	m_pAdminRole = FindRole(RoleName::ADMIN);
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

	auto It = std::find_if(m_vKeys.begin(), m_vKeys.end(), [](CKey Key) { return str_comp(Key.m_aIdent, DEFAULT_SAVED_RCON_USER) == 0; });
	if(It != m_vKeys.end())
		NumDefaultKeys++;

	if(m_vKeys.size() == NumDefaultKeys && !g_Config.m_SvRconPassword[0])
	{
		secure_random_password(g_Config.m_SvRconPassword, sizeof(g_Config.m_SvRconPassword), 6);
		AddDefaultKey(RoleName::ADMIN, g_Config.m_SvRconPassword);
		m_Generated = true;
	}
}

int CAuthManager::AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pRoleName)
{
	if(FindKey(pIdent) >= 0)
		return -1;

	CKey Key;
	str_copy(Key.m_aIdent, pIdent);
	Key.m_Pw = Hash;
	mem_copy(Key.m_aSalt, pSalt, SALT_BYTES);
	Key.m_pRole = FindRole(pRoleName);
	dbg_assert(Key.m_pRole != nullptr, "Invalid role name '%s'.", pRoleName);

	m_vKeys.push_back(Key);
	return m_vKeys.size() - 1;
}

int CAuthManager::AddKey(const char *pIdent, const char *pPw, const char *pRoleName)
{
	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	return AddKeyHash(pIdent, HashPassword(pPw, aSalt), aSalt, pRoleName);
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

int CAuthManager::DefaultIndex(int AuthLevel) const
{
	return std::size(m_aDefault) - AuthLevel;
}

int CAuthManager::DefaultKey(const char *pRoleName) const
{
	if(!str_comp(pRoleName, RoleName::ADMIN))
		return m_aDefault[DefaultIndex(AUTHED_ADMIN)];
	if(!str_comp(pRoleName, RoleName::MODERATOR))
		return m_aDefault[DefaultIndex(AUTHED_MOD)];
	if(!str_comp(pRoleName, RoleName::HELPER))
		return m_aDefault[DefaultIndex(AUTHED_HELPER)];
	return 0;
}

CRconRole *CAuthManager::KeyRole(int Slot) const
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return nullptr;
	return m_vKeys[Slot].m_pRole;
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

void CAuthManager::UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pRoleName)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	CKey *pKey = &m_vKeys[Slot];
	pKey->m_Pw = Hash;
	mem_copy(pKey->m_aSalt, pSalt, SALT_BYTES);
	pKey->m_pRole = FindRole(pRoleName);
}

void CAuthManager::UpdateKey(int Slot, const char *pPw, const char *pRoleName)
{
	if(Slot < 0 || Slot >= (int)m_vKeys.size())
		return;

	// Generate random salt
	unsigned char aSalt[SALT_BYTES];
	secure_random_fill(aSalt, SALT_BYTES);
	UpdateKeyHash(Slot, HashPassword(pPw, aSalt), aSalt, pRoleName);
}

void CAuthManager::ListKeys(FListCallback pfnListCallback, void *pUser)
{
	for(auto &Key : m_vKeys)
		pfnListCallback(Key.m_aIdent, Key.m_pRole->Name(), pUser);
}

void CAuthManager::AddDefaultKey(const char *pRoleName, const char *pPw)
{
	int Level = RoleNameToAuthLevel(pRoleName);
	dbg_assert(AUTHED_HELPER <= Level && Level <= AUTHED_ADMIN, "default role with name '%s' not found.", pRoleName);

	static const char s_aaIdents[3][sizeof(HELPER_IDENT)] = {ADMIN_IDENT, MOD_IDENT, HELPER_IDENT};
	int Index = AUTHED_ADMIN - Level;
	if(m_aDefault[Index] >= 0)
		return; // already exists
	m_aDefault[Index] = AddKey(s_aaIdents[Index], pPw, AuthLevelToRoleName(Level));
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

CRconRole *CAuthManager::FindRole(const char *pName)
{
	const auto It = m_Roles.find(pName);
	if(It == m_Roles.end())
		return nullptr;
	return &It->second;
}

bool CAuthManager::AddRole(const char *pName)
{
	return AddRoleImpl(pName, false);
}

bool CAuthManager::AddAdminRole(const char *pName)
{
	return AddRoleImpl(pName, true);
}

bool CAuthManager::AddRoleImpl(const char *pName, bool IsAdmin)
{
	if(!str_comp(pName, "all")) // meta role
		return false;
	if(!str_comp(pName, "0")) // backcompat role
		return false;
	if(!str_comp(pName, "1")) // backcompat role
		return false;
	if(!str_comp(pName, "2")) // backcompat role
		return false;
	if(!str_comp(pName, "3")) // backcompat role
		return false;
	if(FindRole(pName))
		return false;
	if(!CRconRole::IsValidName(pName))
		return false;

	m_Roles.insert({pName, CRconRole(pName, IsAdmin)});
	return true;
}

bool CAuthManager::CanRoleUseCommand(const char *pRoleName, const char *pCommand)
{
	CRconRole *pRole = FindRole(pRoleName);
	if(!pRole)
		return false;

	return pRole->CanUseRconCommand(pCommand);
}

void CAuthManager::GetRoleNames(char *pBuf, size_t BufSize)
{
	size_t WriteLen = 1;
	pBuf[0] = '\0';

	bool First = true;
	for(const auto &It : m_Roles)
	{
		if(!First)
			WriteLen += str_length(It.second.Name());
		WriteLen += str_length(It.second.Name());
		if(WriteLen >= BufSize)
			break;

		if(!First)
			str_append(pBuf, ", ", BufSize - WriteLen);
		str_append(pBuf, It.second.Name(), BufSize - WriteLen);
		First = false;
	}
}

bool CAuthManager::RoleInherit(const char *pRoleName, const char *pChildRoleName)
{
	CRconRole *pRole = FindRole(pRoleName);
	CRconRole *pChild = FindRole(pChildRoleName);
	if(!pRole)
	{
		log_warn("auth", "Role '%s' not found!", pRoleName);
		return false;
	}
	if(!pChild)
	{
		log_warn("auth", "Role '%s' not found!", pChildRoleName);
		return false;
	}

	if(pChild->IsAdmin())
	{
		log_error("auth", "Can not inherit from admin role");
		return false;
	}
	if(pRole->IsParent(pChild))
	{
		log_warn("auth", "Role '%s' is already inheriting from '%s'. Parent error.", pRoleName, pChildRoleName);
		return false;
	}
	if(pRole->IsAncestor(pChild))
	{
		log_warn("auth", "Role '%s' is already inheriting from '%s'. Grandparent error.", pRoleName, pChildRoleName);
		return false;
	}
	if(pChild->IsAncestor(pRole))
	{
		log_warn("auth", "Role '%s' is already inheriting from '%s'. Circle error.", pChildRoleName, pRoleName);
		return false;
	}

	pRole->AddParent(pChild);
	return true;
}

bool CAuthManager::RoleDeleteInherit(const char *pRoleName, const char *pParentRoleName)
{
	CRconRole *pRole = FindRole(pRoleName);
	CRconRole *pParent = FindRole(pParentRoleName);
	if(!pRole)
	{
		log_warn("auth", "Role '%s' not found!", pRoleName);
		return false;
	}
	if(!pParent)
	{
		log_warn("auth", "Role '%s' not found!", pParentRoleName);
		return false;
	}
	if(!pRole->IsParent(pParent))
	{
		log_warn("auth", "Role '%s' is not a parent of '%s'!", pParentRoleName, pRoleName);
		return false;
	}
	pRole->RemoveParent(pParent);
	return true;
}

bool CAuthManager::IsDefaultRole(const char *pRoleName)
{
	CRconRole *pRole = FindRole(pRoleName);

	// back compat match moderator with "mod"*
	// but only if it is no direct match of a custom role
	if(str_startswith(pRoleName, "mod"))
	{
		if(!pRole)
			return true;
	}

	if(!pRole)
		return false;

	bool CleanMatch = !str_comp(pRole->Name(), RoleName::ADMIN) || !str_comp(pRole->Name(), RoleName::MODERATOR) || !str_comp(pRole->Name(), RoleName::HELPER);
	if(CleanMatch)
		return true;

	return false;
}

void CAuthManager::UnsetReservedSlotsForAllRoles()
{
	for(auto &It : m_Roles)
		It.second.SetReservedSlots(false);
}

void CAuthManager::UnsetTeleOthersForAllRoles()
{
	for(auto &It : m_Roles)
		It.second.SetTeleOthers(false);
}
