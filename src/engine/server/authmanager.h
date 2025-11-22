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
	inline const char *const ADMIN = "admin";
	inline const char *const MODERATOR = "moderator";
	inline const char *const HELPER = "helper";
} // namespace RoleName

namespace RoleRank
{
	static constexpr int ADMIN = AUTHED_ADMIN;
	static constexpr int MODERATOR = AUTHED_MOD;
	static constexpr int HELPER = AUTHED_HELPER;
	static constexpr int NONE = AUTHED_NO;
} // namespace RoleRank

class CRconRole
{
	char m_aName[64];
	int m_Rank = RoleRank::NONE;

public:
	// Name of the rcon role. For example "admin".
	const char *Name() const { return m_aName; }

	// The rank determines how powerful the role is
	// compared to other roles.
	// Higher rank means more power.
	// Roles with lower rank can never kick roles with higher rank.
	// Roles with higher rank can see commands executed by roles with lower rank
	// but not vice versa.
	int Rank() const { return m_Rank; }

	CRconRole(const char *pName, int Rank) :
		m_Rank(Rank)
	{
		str_copy(m_aName, pName);
	}
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
	int KeyLevel(int Slot) const;
	const char *KeyIdent(int Slot) const;
	bool IsValidIdent(const char *pIdent) const;
	void UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pRoleName);
	void UpdateKey(int Slot, const char *pPw, const char *pRoleName);
	void ListKeys(FListCallback pfnListCallback, void *pUser);
	void AddDefaultKey(const char *pRoleName, const char *pPw);
	bool IsGenerated() const;
	int NumNonDefaultKeys() const;
	CRconRole *FindRole(const char *pName);
	bool AddRole(const char *pName, int Rank);
};

#endif //ENGINE_SERVER_AUTHMANAGER_H
