#ifndef ENGINE_SERVER_AUTHMANAGER_H
#define ENGINE_SERVER_AUTHMANAGER_H

#include <base/hash.h>
#include <base/system.h>
#include <game/generated/protocol.h>

#include <vector>

#define SALT_BYTES 8

#define ACCESS_LEVEL_NAME_ADMIN "admin"
#define ACCESS_LEVEL_NAME_MODERATOR "moderator"
#define ACCESS_LEVEL_NAME_HELPER "helper"

class CAccessLevel
{
	char m_aName[64];
	int m_AuthLevel = AUTHED_NO;

public:
	const char *Name() const { return m_aName; }
	int AuthLevel() const { return m_AuthLevel; }

	CAccessLevel(const char *pName, int AuthLevel) :
		m_AuthLevel(AuthLevel)
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
		CAccessLevel *m_pAccessLevel = nullptr;
	};
	std::vector<CKey> m_vKeys;
	std::vector<CAccessLevel> m_vAccessLevels;

	int m_aDefault[3];
	bool m_Generated;

public:
	typedef void (*FListCallback)(const char *pIdent, int Level, void *pUser);

	CAuthManager();

	void Init();
	int AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pAccessLevel);
	int AddKey(const char *pIdent, const char *pPw, const char *pAccessLevel);
	void RemoveKey(int Slot);
	int FindKey(const char *pIdent) const;
	bool CheckKey(int Slot, const char *pPw) const;
	int DefaultKey(const char *pAccessLevel) const;
	int KeyLevel(int Slot) const;
	const char *KeyIdent(int Slot) const;
	bool IsValidIdent(const char *pIdent) const;
	void UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, const char *pAccessLevel);
	void UpdateKey(int Slot, const char *pPw, const char *pAccessLevel);
	void ListKeys(FListCallback pfnListCallbac, void *pUser);
	void AddDefaultKey(const char *pAccessLevel, const char *pPw);
	bool IsGenerated() const;
	int NumNonDefaultKeys() const;
	CAccessLevel *FindAccessLevelByName(const char *pName);
	bool AddAccessLevel(const char *pName, int Level);
};

#endif //ENGINE_SERVER_AUTHMANAGER_H
