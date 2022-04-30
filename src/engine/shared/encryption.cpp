#include <engine/encryption.h>
#include <engine/storage.h>

#include <sodium.h>

class CEncryption : public IEncryption
{
public:
	CEncryption() = default;

	virtual std::optional<CKeyPair> CreateKeyPair()
	{
		CKeyPair KeyPair;
		if(crypto_box_keypair(KeyPair.m_aPublicKey, KeyPair.m_aSecretKey) == 0)
		{
			return KeyPair;
		}
		return std::nullopt;
	}

	virtual void FillNonce(unsigned char *pNonce, size_t NonceSize)
	{
		randombytes_buf(pNonce, NonceSize);
	}

	virtual bool EncryptMessage(unsigned char *pEncryptedMessage,
		const unsigned char *pMessage, int MessageLen,
		const unsigned char *pNonce,
		unsigned char *pPublicKey, CKeyPair *pKeyPair)
	{
        return crypto_box_easy(pEncryptedMessage, pMessage, MessageLen, pNonce, pPublicKey, pKeyPair->m_aSecretKey) == 0;
	}

	virtual bool DecryptMessage(unsigned char *pDecryptedMessage,
		const unsigned char *pEncryptedMessage,
		int EncryptedMessageLen,
		const unsigned char *pNonce,
		unsigned char *pPublicKey,
		CKeyPair *pKeyPair)
	{
		return crypto_box_open_easy(pDecryptedMessage, pEncryptedMessage, EncryptedMessageLen, pNonce, pPublicKey, pKeyPair->m_aSecretKey) == 0;
	}
};

IEncryption *CreateEncryption()
{
	return new CEncryption();
}
