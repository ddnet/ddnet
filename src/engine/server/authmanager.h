#ifndef ENGINE_SERVER_AUTHMANAGER_H
#define ENGINE_SERVER_AUTHMANAGER_H

#include <base/hash.h>
#include <base/tl/array.h>

#define SALT_BYTES 8

class CAuthManager
{
private:
	enum
	{
		AUTHED_NO = 0,
		AUTHED_HELPER,
		AUTHED_MOD,
		AUTHED_ADMIN
	};
	struct CKey
	{
		char m_aIdent[64];
		MD5_DIGEST m_Pw;
		unsigned char m_aSalt[SALT_BYTES];
		int m_Level;
	};
	array<CKey> m_aKeys;

	int m_aDefault[3];
	bool m_Generated;
public:
	typedef void (*FListCallback)(const char *pIdent, int Level, void *pUser);

	CAuthManager();

	void Init();
	int AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel);
	int AddKey(const char *pIdent, const char *pPw, int AuthLevel);
	int RemoveKey(int Slot); // Returns the old key slot that is now in the named one.
	int FindKey(const char *pIdent);
	bool CheckKey(int Slot, const char *pPw);
	int DefaultKey(int AuthLevel);
	int KeyLevel(int Slot);
	const char *KeyIdent(int Slot);
	void UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel);
	void UpdateKey(int Slot, const char *pPw, int AuthLevel);
	void ListKeys(FListCallback pfnListCallbac, void *pUser);
	void AddDefaultKey(int Level, const char *pPw);
	bool IsGenerated();
	int NumNonDefaultKeys();
};

#endif //ENGINE_SERVER_AUTHMANAGER_H
