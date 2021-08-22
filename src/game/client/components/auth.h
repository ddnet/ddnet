#ifndef GAME_CLIENT_COMPONENTS_AUTH_H
#define GAME_CLIENT_COMPONENTS_AUTH_H

#include <game/client/component.h>

class CAuth : public CComponent
{
	unsigned char *m_pPublicKeyBuffer;
	size_t m_PublicKeyLen;

	void PrintOpenSSLError();

public:
	virtual void OnInit();

	// Register an account using the given email and the automatically generated keypair
	bool RegisterAccount(const char *pEmail, size_t EmailStrLength);

	// Caller should remember to free ppSignature using FreeSignature.
	bool SignContent(const unsigned char *pContent, size_t ContentLen, unsigned char **ppSignature, size_t *pSignatureLen);
	void FreeSignature(unsigned char **ppSignature);
};

#endif // GAME_CLIENT_COMPONENTS_AUTH_H
