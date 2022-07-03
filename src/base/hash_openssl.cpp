#if defined(CONF_OPENSSL)
#include "hash_ctxt.h"

EVP_MD_CTX *sha256_create_init()
{
	EVP_MD_CTX *pCtxt = EVP_MD_CTX_create();
	EVP_DigestInit_ex(pCtxt, EVP_sha256(), NULL);
	return pCtxt;
}

void sha256_update(EVP_MD_CTX *pCtxt, const void *data, size_t data_len)
{
	EVP_DigestUpdate(pCtxt, data, data_len);
}

SHA256_DIGEST sha256_finish_destroy(EVP_MD_CTX *pCtxt)
{
	SHA256_DIGEST result;
	unsigned int sha256_len = sizeof(result);
	EVP_DigestFinal_ex(pCtxt, result.data, &sha256_len);
	EVP_MD_CTX_destroy(pCtxt);
	return result;
}

EVP_MD_CTX *md5_create_init()
{
	EVP_MD_CTX *pCtxt = EVP_MD_CTX_create();
	EVP_DigestInit_ex(pCtxt, EVP_md5(), NULL);
	return pCtxt;
}

void md5_update(EVP_MD_CTX *pCtxt, const void *data, size_t data_len)
{
	EVP_DigestUpdate(pCtxt, data, data_len);
}

MD5_DIGEST md5_finish_destroy(EVP_MD_CTX *pCtxt)
{
	MD5_DIGEST result;
	unsigned int md5_len = sizeof(result);
	EVP_DigestFinal_ex(pCtxt, result.data, &md5_len);
	EVP_MD_CTX_destroy(pCtxt);
	return result;
}
#endif
