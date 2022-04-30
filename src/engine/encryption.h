#ifndef ENGINE_ENCRYPTION_H
#define ENGINE_ENCRYPTION_H

#include "kernel.h"
#include "sodium.h"
#include <sodium/crypto_box.h>
#include <optional>

/*
	Key exchange: X25519
	Encryption: XSalsa20
	Authentication: Poly1305

	https://doc.libsodium.org/public-key_cryptography
*/

#define ENCRYPTION_PUBLIC_KEY_BYTES crypto_box_PUBLICKEYBYTES
#define ENCRYPTION_SECRET_KEY_BYTES crypto_box_SECRETKEYBYTES
#define ENCRYPTION_NONCE_BYTES crypto_box_NONCEBYTES
#define ENCRYPTION_MAC_BYTES crypto_box_MACBYTES

/**
 * Interface to handle encryption of messages and storage of key pairs.
 */
class IEncryption : public IInterface
{
	MACRO_INTERFACE("encryption", 0)

public:
	struct CKeyPair
	{
		unsigned char m_aPublicKey[ENCRYPTION_PUBLIC_KEY_BYTES];
		unsigned char m_aSecretKey[ENCRYPTION_SECRET_KEY_BYTES];
	};

	/**
	 * Creates a key pair used to encrypt messages.
	 * 
	 * @return A pair, the boolean indicates whether the creation was succesful.
	 * 
	 * @todo Use std::optional when available.
	 */
	virtual std::optional<CKeyPair> CreateKeyPair() = 0;

	/**
	 * Fills the passed buffer with a nonce.
	 * 
	 * Should be atleast of length ENCRYPTION_NONCE_BYTES.
	 * 
	 * The nonce doesn't have to be confidential, but it should be used with just one invocation of EncryptMessage for a particular pair of public and secret keys.
	 */
	virtual void FillNonce(unsigned char pNonce[ENCRYPTION_NONCE_BYTES], size_t NonceSize) = 0;

	/**
	 * Encrypts a message.
	 * 
	 * @param pEncryptedMessage The buffer used to store the encrypted message. Care should be taken to also include the size of ENCRYPTION_MAC_BYTES
	 * @param pMessage The message to encrypt.
	 * @param MessageLen The message length.
	 * @param aNonce The nonce used to encrypt this message.
	 * @param aPublicKey The public key of the recipient.
	 * @param pKeyPair The key pair of the sender.
	 * 
	 * @returns True if the encryption was succesful.
	 * 
	 * @see FillNonce
	 */
	virtual bool EncryptMessage(unsigned char *pEncryptedMessage,
		const unsigned char *pMessage,
		int MessageLen,
		const unsigned char pNonce[ENCRYPTION_NONCE_BYTES],
		unsigned char pPublicKey[ENCRYPTION_PUBLIC_KEY_BYTES],
		CKeyPair *pKeyPair) = 0;

	/**
	 * Decrypts a message.
	 * 
	 * @param pDecryptedMessage The buffer used to store the decrypted message.
	 * @param pEncryptedMessage The message to decrypt.
	 * @param EncryptedMessageLen The message length.
	 * @param pNonce The nonce used to encrypt this message.
	 * @param pPublicKey The public key of the sender.
	 * @param pKeyPair The key pair of the receiver.
	 * 
	 * @returns True if the decryption was succesful.
	 * 
	 * @see FillNonce
	 */
	virtual bool DecryptMessage(unsigned char *pDecryptedMessage,
		const unsigned char *pEncryptedMessage,
		int EncryptedMessageLen,
		const unsigned char pNonce[ENCRYPTION_NONCE_BYTES],
		unsigned char pPublicKey[ENCRYPTION_PUBLIC_KEY_BYTES],
		CKeyPair *pKeyPair) = 0;
};

extern IEncryption *CreateEncryption();

#endif
