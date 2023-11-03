#ifndef ENGINE_SERVER_AUTHMANAGER_H
#define ENGINE_SERVER_AUTHMANAGER_H

#include <vector>

#include <base/hash.h>

#define SALT_BYTES 8

class CAuthManager
{
private:
	struct CKey
	{
		char m_aIdent[64];
		MD5_DIGEST m_Pw;
		unsigned char m_aSalt[SALT_BYTES];
		int m_Level;
	};
	std::vector<CKey> m_vKeys;

	int m_aDefault[3];
	bool m_Generated;

public:
	typedef void (*FListCallback)(const char *pIdent, int Level, void *pUser);

	CAuthManager();

	void Init();
	int AddKeyHash(const char *pIdent, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel);
	int AddKey(const char *pIdent, const char *pPw, int AuthLevel);
	void RemoveKey(int Slot);
	int FindKey(const char *pIdent) const;
	bool CheckKey(int Slot, const char *pPw) const;
	int DefaultKey(int AuthLevel) const;
	int KeyLevel(int Slot) const;
	const char *KeyIdent(int Slot) const;
	void UpdateKeyHash(int Slot, MD5_DIGEST Hash, const unsigned char *pSalt, int AuthLevel);
	void UpdateKey(int Slot, const char *pPw, int AuthLevel);
	void ListKeys(FListCallback pfnListCallbac, void *pUser);
	void AddDefaultKey(int Level, const char *pPw);
	bool IsGenerated() const;
	int NumNonDefaultKeys() const;
};

#endif //ENGINE_SERVER_AUTHMANAGER_H
