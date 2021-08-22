#include <game/client/gameclient.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/pem.h>

#include "auth.h"

static const ColorRGBA ClientAuthPrintColor{0.6f, 1, 0.7f, 1.0f};
static const char *s_pPrivateKeyFilename = "authkey.pem";
static const char *s_pPublicKeyFilename = "authkey-pub.pem";

void CAuth::PrintOpenSSLError()
{
	unsigned long ErrorCode = ERR_get_error();

	if(ErrorCode == 0)
		return;

	char aBuf[256];
	ERR_error_string_n(ErrorCode, aBuf, sizeof(aBuf));

	if(aBuf[0] != '\0')
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth:openssl-error", aBuf, ClientAuthPrintColor);
}

void CAuth::OnInit()
{
	IOHANDLE pPrivateHandle = m_pClient->Storage()->OpenFile(s_pPrivateKeyFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(pPrivateHandle)
	{
		io_close(pPrivateHandle);

		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Private pem key file found", ClientAuthPrintColor);

		IOHANDLE pPublicHandle = m_pClient->Storage()->OpenFile(s_pPublicKeyFilename, IOFLAG_READ, IStorage::TYPE_ALL);

		if(!pPublicHandle)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not open a file handle to save the pem private key file", ClientAuthPrintColor);
			return;
		}

		EVP_PKEY *pKey = NULL;

		PEM_read_PUBKEY((FILE *)pPublicHandle, &pKey, NULL, NULL);
		io_close(pPublicHandle);

		if(!pKey)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not load public key", ClientAuthPrintColor);
			PrintOpenSSLError();
			EVP_PKEY_free(pKey);
			return;
		}

		// First get the length of the pubkey
		if(EVP_PKEY_get_raw_public_key(pKey, NULL, &m_PublicKeyLen) == 0)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not get the required length of the public key", ClientAuthPrintColor);
			PrintOpenSSLError();
		}

		if(m_pPublicKeyBuffer)
			free(m_pPublicKeyBuffer);

		m_pPublicKeyBuffer = (unsigned char *)calloc(m_PublicKeyLen, sizeof(unsigned char));

		// Get the public key and convert it to a buffer that can be sent.
		if(EVP_PKEY_get_raw_public_key(pKey, m_pPublicKeyBuffer, &m_PublicKeyLen) == 0 || !m_pPublicKeyBuffer)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not get public key (raw)", ClientAuthPrintColor);
			PrintOpenSSLError();
			EVP_PKEY_free(pKey);
			return;
		}

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Public key found, length is: %zu", m_PublicKeyLen);
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", aBuf, ClientAuthPrintColor);

		EVP_PKEY_free(pKey);
	}
	else
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Private pem key file not found, generating new", ClientAuthPrintColor);
		IOHANDLE pHandle = m_pClient->Storage()->OpenFile(s_pPrivateKeyFilename, IOFLAG_WRITE, IStorage::TYPE_ALL);

		if(!pHandle)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not open a file handle to save the pem private key file", ClientAuthPrintColor);
			return;
		}

		EVP_PKEY *pKey = NULL;
		EVP_PKEY_CTX *pContext = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);

		if(!pContext)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not create a OpenSSL EVP_PKEY context", ClientAuthPrintColor);
			PrintOpenSSLError();
			return;
		}

		EVP_PKEY_keygen_init(pContext);

		if(EVP_PKEY_keygen(pContext, &pKey) < 1)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not create the private key (keygen)", ClientAuthPrintColor);
			PrintOpenSSLError();
		}

		EVP_PKEY_CTX_free(pContext);

		// Save the key to a file.
		PEM_write_PKCS8PrivateKey((FILE *)pHandle, pKey, NULL, NULL, 0, NULL, NULL);
		io_close(pHandle);

		// Save the public key
		IOHANDLE pPublicKeyHandle = m_pClient->Storage()->OpenFile(s_pPublicKeyFilename, IOFLAG_WRITE, IStorage::TYPE_ALL);

		if(!pPublicKeyHandle)
		{
			EVP_PKEY_free(pKey);
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not open a file handle to save the pem public key file", ClientAuthPrintColor);
			return;
		}

		PEM_write_PUBKEY((FILE *)pPublicKeyHandle, pKey);
		io_close(pPublicKeyHandle);

		// First get the length of the pubkey
		if(EVP_PKEY_get_raw_public_key(pKey, NULL, &m_PublicKeyLen) == 0)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not get the required length of the public key", ClientAuthPrintColor);
			PrintOpenSSLError();
		}

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "pubkey length is: %zu", m_PublicKeyLen);
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", aBuf, ClientAuthPrintColor);

		if(m_pPublicKeyBuffer)
		{
			free(m_pPublicKeyBuffer);
			m_pPublicKeyBuffer = (unsigned char *)calloc(m_PublicKeyLen, sizeof(unsigned char));
		}

		// Get the public key and convert it to a buffer that can be sent.
		if(EVP_PKEY_get_raw_public_key(pKey, m_pPublicKeyBuffer, &m_PublicKeyLen) == 0)
		{
			m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not get public key (raw)", ClientAuthPrintColor);
			PrintOpenSSLError();
		}

		EVP_PKEY_free(pKey);
	}

	// If set, initialization went correctly
	if(m_pPublicKeyBuffer)
	{
		const char *pEmail = "mail@mail.com";
		RegisterAccount(pEmail, str_length(pEmail));
	}
}

bool CAuth::SignContent(const unsigned char *pContent, size_t ContentLen, unsigned char **ppSignature, size_t *pSignatureLen)
{
	// Reference: https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying
	// https://www.openssl.org/docs/man1.1.1/man7/Ed25519.html

	IOHANDLE pPrivateHandle = m_pClient->Storage()->OpenFile(s_pPrivateKeyFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	if(!pPrivateHandle)
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not open a file handle to save the pem private key file", ClientAuthPrintColor);
		return false;
	}

	EVP_PKEY *pKey = NULL;

	PEM_read_PrivateKey((FILE *)pPrivateHandle, &pKey, NULL, NULL);

	io_close(pPrivateHandle);

	if(!pKey)
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not read private pem key file", ClientAuthPrintColor);
		PrintOpenSSLError();
	}

	EVP_MD_CTX *pMdCtx = NULL;
	pMdCtx = EVP_MD_CTX_create();

	if(!pMdCtx)
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not create EVP_MD_CTX", ClientAuthPrintColor);
		PrintOpenSSLError();
	}

	if(!EVP_DigestSignInit(pMdCtx, NULL, NULL, NULL, pKey))
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not initialize Digest", ClientAuthPrintColor);
		PrintOpenSSLError();
	}

	// Query the length first.
	if(!EVP_DigestSign(pMdCtx, NULL, pSignatureLen, pContent, ContentLen))
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not create a signature (query len)", ClientAuthPrintColor);
		PrintOpenSSLError();
		EVP_MD_CTX_free(pMdCtx);
		return false;
	}

	*ppSignature = (unsigned char *)OPENSSL_zalloc(*pSignatureLen);

	if(!EVP_DigestSign(pMdCtx, *ppSignature, pSignatureLen, pContent, ContentLen))
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not create a signature", ClientAuthPrintColor);
		PrintOpenSSLError();
		EVP_MD_CTX_free(pMdCtx);
		FreeSignature(ppSignature);
		return false;
	}

	EVP_MD_CTX_free(pMdCtx);

	return true;
}

void CAuth::FreeSignature(unsigned char **ppSignature)
{
	if(ppSignature && *ppSignature)
		OPENSSL_free(*ppSignature);
}

bool CAuth::RegisterAccount(const char *pEmail, size_t EmailStrLength)
{
	unsigned char *pSignature = NULL;
	size_t SignatureLen = 0;

	if(!SignContent((const unsigned char *)pEmail, EmailStrLength, &pSignature, &SignatureLen))
		return false;

	char aEncodedSignature[256];
	mem_zero(aEncodedSignature, sizeof(aEncodedSignature));

	// base64 encoding
	EVP_EncodeBlock((unsigned char *)aEncodedSignature, pSignature, SignatureLen);

	char aEncodedPubKey[64];
	mem_zero(aEncodedPubKey, sizeof(aEncodedPubKey));
	EVP_EncodeBlock((unsigned char *)aEncodedPubKey, m_pPublicKeyBuffer, m_PublicKeyLen);

	char aPayload[2048];

	str_format(aPayload, sizeof(aPayload), "{\"email\": \"%s\", \"email_signature\":\"%s\", \"public_key\":\"%s\"}", pEmail, aEncodedSignature, aEncodedPubKey);
	m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", aPayload, ClientAuthPrintColor);

	CTimeout Timeout{10000, 8000, 10};
	CPostJson PostJson("http://127.0.0.1:3000/account/register", Timeout, aPayload);
	IEngine::RunJobBlocking(&PostJson);

	if(PostJson.State() != HTTP_DONE)
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Could not register", ClientAuthPrintColor);
	}
	else
	{
		m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ERROR: Registered succesfully", ClientAuthPrintColor);
	}

	FreeSignature(&pSignature);

	return true;
}
