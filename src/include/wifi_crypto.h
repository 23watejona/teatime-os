#ifndef WIFI_CRYPTO_H
#define WIFI_CRYPTO_H

/* WPA2-PSK cryptographic primitives, self-contained (no libc dependency).
   Used for the 4-way handshake and CCMP:
     PMK  = PBKDF2-HMAC-SHA1(passphrase, ssid, 4096, 32)
     PTK  = PRF-384(PMK, "Pairwise key expansion", min|max addrs, min|max nonces)
     CCMP = AES-128-CCM with the 16-byte temporal key from the PTK. */

#include "def.h"
typedef unsigned int  u32;

#define SHA1_DIGEST 20
#define SHA1_BLOCK  64
typedef struct {
    u32 h[5];
    u32 len; /* total bytes hashed */
    u8  buf[SHA1_BLOCK];
    u32 buflen;
} sha1_ctx;

void sha1_init(sha1_ctx *c);
void sha1_update(sha1_ctx *c, const u8 *data, u32 n);
void sha1_final(sha1_ctx *c, u8 out[SHA1_DIGEST]);
void sha1(const u8 *data, u32 n, u8 out[SHA1_DIGEST]);

void hmac_sha1(const u8 *key, u32 keylen, const u8 *msg, u32 msglen,
               u8 out[SHA1_DIGEST]);

void pbkdf2_sha1(const u8 *pass, u32 passlen, const u8 *salt, u32 saltlen,
                 u32 iters, u8 *out, u32 outlen);

void wpa2_pmk(const char *passphrase, const u8 *ssid, u32 ssidlen, u8 pmk[32]);

void sha1_prf(const u8 *key, u32 keylen, const char *label,
              const u8 *data, u32 datalen, u8 *out, u32 outlen);

typedef struct { u32 rk[44]; } aes128_ctx; /* 11 round keys */
void aes128_init(aes128_ctx *c, const u8 key[16]);
void aes128_encrypt(const aes128_ctx *c, const u8 in[16], u8 out[16]);
void aes128_decrypt(const aes128_ctx *c, const u8 in[16], u8 out[16]);

/* AES Key Wrap unwrap (RFC 3394), for the GTK in EAPOL msg3. n = plaintext
   64-bit blocks (wrapped input is n+1 blocks). Returns 0 on success. */
int aes_unwrap(const u8 *kek, u32 keklen, u32 n, const u8 *wrapped, u8 *out);

/* nonce is 13 bytes and the mic 8; decrypt returns 0 on success */
void aes_ccm_encrypt(const u8 key[16], const u8 *nonce, const u8 *aad, u32 aadlen,
                     const u8 *plain, u32 plainlen, u8 *cipher, u8 mic[8]);
int  aes_ccm_decrypt(const u8 key[16], const u8 *nonce, const u8 *aad, u32 aadlen,
                     const u8 *cipher, u32 cipherlen, const u8 mic[8], u8 *plain);

#endif
