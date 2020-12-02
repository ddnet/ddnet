#include <base/system.h>
#include <engine/shared/linereader.h>

#include <stdio.h>
#include <openssl/aes.h>

#define PASSWORD_FILE "chillpw_secret.txt"

void decrypt(const char *pEncryptedFile, const char *pDecryptedFile)
{
	FILE *ifp = fopen(pEncryptedFile, "rb");
    FILE *ofp = fopen(pDecryptedFile, "wb");
    if (!ifp) {
        printf("failed to open '%s'\n", pEncryptedFile);
        exit(1);
    }
    if (!ofp) {
        printf("failed to open '%s'\n", pDecryptedFile);
        exit(1);
    }
    int bytes_read, bytes_written;
    unsigned char indata[AES_BLOCK_SIZE];
    unsigned char outdata[AES_BLOCK_SIZE];

    /* ckey and ivec are the two 128-bits keys necesary to
        en- and recrypt your data.  Note that ckey can be
        192 or 256 bits as well */
    unsigned char ckey[] =  "thiskeyisverybad";
    unsigned char ivec[] = "dontusethisinput";

    /* data structure that contains the key itself */
    AES_KEY key;

    /* set the encryption key */
    AES_set_encrypt_key(ckey, 128, &key);

    /* set where on the 128 bit encrypted block to begin encryption*/
    int num = 0;

    while (1) {
        bytes_read = fread(indata, 1, AES_BLOCK_SIZE, ifp);

        AES_cfb128_encrypt(indata, outdata, bytes_read, &key, ivec, &num,
            AES_DECRYPT);

        bytes_written = fwrite(outdata, 1, bytes_read, ofp);
        dbg_msg("aes", "decryption writing '%s' -> '%s' (%d bytes)", indata, outdata, bytes_written);
        if (bytes_read < AES_BLOCK_SIZE)
            break;
    }
}

void encrypt(const char *pPlainFile, const char *pEncryptedFile)
{
	FILE *ifp = fopen(pPlainFile, "rb");
    FILE *ofp = fopen(pEncryptedFile, "wb");
    if (!ifp) {
        printf("failed to open '%s'\n", pPlainFile);
        exit(1);
    }
    if (!ofp) {
        printf("failed to open '%s'\n", pEncryptedFile);
        exit(1);
    }
    int bytes_read, bytes_written;
    unsigned char indata[AES_BLOCK_SIZE];
    unsigned char outdata[AES_BLOCK_SIZE];

    /* ckey and ivec are the two 128-bits keys necesary to
        en- and recrypt your data.  Note that ckey can be
        192 or 256 bits as well */
    unsigned char ckey[] =  "thiskeyisverybad";
    unsigned char ivec[] = "dontusethisinput";

    /* data structure that contains the key itself */
    AES_KEY key;

    /* set the encryption key */
    AES_set_encrypt_key(ckey, 128, &key);

    /* set where on the 128 bit encrypted block to begin encryption*/
    int num = 0;

    while (1) {
        bytes_read = fread(indata, 1, AES_BLOCK_SIZE, ifp);

        AES_cfb128_encrypt(indata, outdata, bytes_read, &key, ivec, &num,
            AES_ENCRYPT);

        bytes_written = fwrite(outdata, 1, bytes_read, ofp);
        dbg_msg("aes", "encryption writing '%s' -> '%s' (%d bytes)", indata, outdata, bytes_written);
        if (bytes_read < AES_BLOCK_SIZE)
            break;
    }
}

int main()
{
    dbg_logger_stdout();
    encrypt(PASSWORD_FILE, PASSWORD_FILE".crypt");
    decrypt(PASSWORD_FILE".crypt", PASSWORD_FILE".decrypt"); // works when encrypt() is commented out
}
