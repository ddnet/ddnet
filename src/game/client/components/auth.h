#ifndef GAME_CLIENT_COMPONENTS_AUTH_H
#define GAME_CLIENT_COMPONENTS_AUTH_H

#include <game/client/component.h>

class CAuth : public CComponent
{
	unsigned char *m_pPublicKeyBuffer;
	size_t m_PublicKeyLen;

	void PrintOpenSSLError();
	// Caller should remember to free ppSignature
	bool SignContent(const unsigned char *pContent, size_t ContentLen, unsigned char **ppSignature, size_t *pSignatureLen);
	void FreeSignature(unsigned char **ppSignature);

public:
	virtual void OnInit();
};

#endif // GAME_CLIENT_COMPONENTS_AUTH_H
