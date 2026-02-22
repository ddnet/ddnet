#ifndef ENGINE_SERVER_AUTHMANAGER_H
#define ENGINE_SERVER_AUTHMANAGER_H

#include <base/hash.h>
#include <base/system.h>

#include <generated/protocol.h>

#include <string>
#include <unordered_map>
#include <vector>

#define SALT_BYTES 8

namespace RoleName
{
	constexpr const char *const ADMIN = "admin";
	constexpr const char *const MODERATOR = "moderator";
	constexpr const char *const HELPER = "helper";
} // namespace RoleName

class CRconRole
{
	char m_aName[64];
	bool m_IsAdmin = false;
	bool m_HasReservedSlots = false;
	bool m_CanTeleOthers = false;
	std::vector<std::string> m_vRconCommands;
	std::vector<CRconRole *> m_vpParents;
	std::vector<CRconRole *> m_vpCanKick;

public:
	// inherit all command access from a parent role
	void AddParent(CRconRole *pRole);

	// delete parent inheritance and lose access to all parent inherited command access
	void RemoveParent(CRconRole *pRole);

	void AllowKick(CRconRole *pRole);
	void DisallowKick(CRconRole *pRole);

	bool CanKick(CRconRole *pRole) const;

	// check if *pParent* is a direct parent of *this* role
	bool IsParent(const CRconRole *pParent) const;

	// check if *pAncestor* is a direct or indirect parent of *this* role
	bool IsAncestor(const CRconRole *pAncestor) const;

	// Name of the rcon role. For example "admin".
	const char *Name() const { return m_aName; }

	bool HasReservedSlots() const;
	bool CanTeleOthers() const;

	void SetReservedSlots(bool Value) { m_HasReservedSlots = Value; }
	void SetTeleOthers(bool Value) { m_CanTeleOthers = Value; }

	// admin is the strongest role with full access
	bool IsAdmin() const { return m_IsAdmin; }

	CRconRole *InheritCommandAccessFrom(const char *pCommand);
	bool CanUseRconCommandDirect(const char *pCommand);
	bool CanUseRconCommand(const char *pCommand);
	bool AllowCommand(const char *pCommand);
	bool DisallowCommand(const char *pCommand);

	void LogRconCommandAccess(int MaxLineLength);

	CRconRole(const char *pName, bool IsAdmin) :
		m_IsAdmin(IsAdmin)
	{
		str_copy(m_aName, pName);
	}
	CRconRole(const char *pName)
	{
		str_copy(m_aName, pName);
	}

	static bool IsValidName(const char *pName);
};

class CAuthManager
{
private:
	class CKey
	{
	public:
		char m_aIdent[64];
		MD5_DIGEST m_Pw;
		unsigned char m_aSalt[SALT_BYTES];
		CRconRole *m_pRole = nullptr;
	};
	std::vector<CKey> m_vKeys;
	std::unordered_map<std::string, CRconRole> m_Roles;
	CRconRole *m_pAdminRole = nullptr;

	int m_aDefault[3];
	bool m_Generated;

public:
	static const char *AuthLevelToRoleName(int AuthLevel);

	typedef void (*FListCallback)(const char *pIdent, const char *pRoleName, void *pUser);

	CAuthManager();

	void Init();
	int AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pRoleName);
	int AddKey(const char *pIdent, const char *pPw, const char *pRoleName);
	void RemoveKey(int Slot);
	int FindKey(const char *pIdent) const;
	bool CheckKey(int Slot, const char *pPw) const;

private:
	int DefaultIndex(int AuthLevel) const;

public:
	int DefaultKey(const char *pRoleName) const;
	CRconRole *KeyRole(int Slot) const;
	const char *KeyIdent(int Slot) const;
	bool IsValidIdent(const char *pIdent) const;
	void UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pRoleName);
	void UpdateKey(int Slot, const char *pPw, const char *pRoleName);
	void ListKeys(FListCallback pfnListCallback, void *pUser);
	void AddDefaultKey(const char *pRoleName, const char *pPw);
	bool IsGenerated() const;
	int NumNonDefaultKeys() const;
	CRconRole *FindRole(const char *pName);
	CRconRole *AdminRole() const { return m_pAdminRole; }
	bool AddRole(const char *pName);

private:
	bool AddAdminRole(const char *pName);
	bool AddRoleImpl(const char *pName, bool IsAdmin);

public:
	bool CanRoleUseCommand(const char *pRoleName, const char *pCommand);
	void GetRoleNames(char *pBuf, size_t BufSize);
	bool RoleInherit(const char *pRoleName, const char *pChildRoleName);
	bool RoleDeleteInherit(const char *pRoleName, const char *pParentRoleName);

private:
	bool IsDefaultRole(const char *pRoleName);

public:
	void UnsetReservedSlotsForAllRoles();
	void UnsetTeleOthersForAllRoles();
};

#endif // ENGINE_SERVER_AUTHMANAGER_H
