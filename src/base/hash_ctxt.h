#ifndef BASE_HASH_CTXT_H
#define BASE_HASH_CTXT_H

#include "hash.h"
#include "system.h"
#include <cstdint>

#if defined(CONF_OPENSSL)
#include <openssl/evp.h>
#else
#include <engine/external/md5/md5.h>
#endif

#if defined(CONF_OPENSSL)
#define SHA256_CTX EVP_MD_CTX
#define MD5_CTX EVP_MD_CTX
#else
struct SHA256_CTX
{
	uint64_t length;
	uint32_t state[8];
	uint32_t curlen;
	unsigned char buf[64];
};
typedef md5_state_t MD5_CTX;
#endif

void sha256_update(SHA256_CTX *pCtxt, const void *data, size_t data_len);
void md5_update(MD5_CTX *pCtxt, const void *data, size_t data_len);

#if !defined(CONF_OPENSSL)
void sha256_init(SHA256_CTX *pCtxt);
SHA256_DIGEST sha256_finish(SHA256_CTX *pCtxt);

void md5_create_init(MD5_CTX *pCtxt);
MD5_DIGEST md5_finish(MD5_CTX *pCtxt);

inline MD5_CTX *md5_create_init()
{
	MD5_CTX *pCtxt = new MD5_CTX();
	md5_init(pCtxt);
	return pCtxt;
}
inline MD5_DIGEST md5_finish_destroy(MD5_CTX *pCtxt)
{
	MD5_DIGEST digest = md5_finish(pCtxt);
	delete pCtxt;
	return digest;
}

inline SHA256_CTX *sha256_create_init()
{
	SHA256_CTX *pCtxt = new SHA256_CTX();
	sha256_init(pCtxt);
	return pCtxt;
}
inline SHA256_DIGEST sha256_finish_destroy(SHA256_CTX *pCtxt)
{
	SHA256_DIGEST digest = sha256_finish(pCtxt);
	delete pCtxt;
	return digest;
}
#else
MD5_CTX *md5_create_init();
MD5_DIGEST md5_finish_destroy(MD5_CTX *pCtxt);

SHA256_CTX *sha256_create_init();
SHA256_DIGEST sha256_finish_destroy(SHA256_CTX *pCtxt);
#endif

#endif // BASE_HASH_CTXT_H
