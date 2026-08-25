#include "wifi_crypto.h"
#include "string.h"

static u32 rol(u32 x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_block(sha1_ctx *c, const u8 *p) {
    u32 w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((u32)p[i*4] << 24) | ((u32)p[i*4+1] << 16) |
               ((u32)p[i*4+2] << 8) | (u32)p[i*4+3];
    for (int i = 16; i < 80; i++)
        w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    u32 a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20)      { f = (b & cc) | (~b & d);        k = 0x5a827999; }
        else if (i < 40) { f = b ^ cc ^ d;                 k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8f1bbcdc; }
        else             { f = b ^ cc ^ d;                 k = 0xca62c1d6; }
        u32 t = rol(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

void sha1_init(sha1_ctx *c) {
    c->h[0] = 0x67452301; c->h[1] = 0xefcdab89; c->h[2] = 0x98badcfe;
    c->h[3] = 0x10325476; c->h[4] = 0xc3d2e1f0;
    c->len = 0; c->buflen = 0;
}

void sha1_update(sha1_ctx *c, const u8 *data, u32 n) {
    c->len += n;
    while (n) {
        u32 take = SHA1_BLOCK - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == SHA1_BLOCK) { sha1_block(c, c->buf); c->buflen = 0; }
    }
}

void sha1_final(sha1_ctx *c, u8 out[SHA1_DIGEST]) {
    u32 bits_hi = c->len >> 29;
    u32 bits_lo = c->len << 3;
    u8 pad = 0x80;
    sha1_update(c, &pad, 1);
    u8 zero = 0;
    while (c->buflen != 56) sha1_update(c, &zero, 1);
    u8 lb[8];
    lb[0] = bits_hi >> 24; lb[1] = bits_hi >> 16; lb[2] = bits_hi >> 8; lb[3] = bits_hi;
    lb[4] = bits_lo >> 24; lb[5] = bits_lo >> 16; lb[6] = bits_lo >> 8; lb[7] = bits_lo;
    sha1_update(c, lb, 8);
    for (int i = 0; i < 5; i++) {
        out[i*4]   = c->h[i] >> 24; out[i*4+1] = c->h[i] >> 16;
        out[i*4+2] = c->h[i] >> 8;  out[i*4+3] = c->h[i];
    }
}

void sha1(const u8 *data, u32 n, u8 out[SHA1_DIGEST]) {
    sha1_ctx c; sha1_init(&c); sha1_update(&c, data, n); sha1_final(&c, out);
}

void hmac_sha1(const u8 *key, u32 keylen, const u8 *msg, u32 msglen,
               u8 out[SHA1_DIGEST]) {
    u8 k[SHA1_BLOCK], ipad[SHA1_BLOCK], opad[SHA1_BLOCK], inner[SHA1_DIGEST];
    memset(k, 0, SHA1_BLOCK);
    if (keylen > SHA1_BLOCK) sha1(key, keylen, k);
    else                     memcpy(k, key, keylen);
    for (int i = 0; i < SHA1_BLOCK; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }

    sha1_ctx c;
    sha1_init(&c); sha1_update(&c, ipad, SHA1_BLOCK); sha1_update(&c, msg, msglen);
    sha1_final(&c, inner);
    sha1_init(&c); sha1_update(&c, opad, SHA1_BLOCK); sha1_update(&c, inner, SHA1_DIGEST);
    sha1_final(&c, out);
}

void pbkdf2_sha1(const u8 *pass, u32 passlen, const u8 *salt, u32 saltlen,
                 u32 iters, u8 *out, u32 outlen) {
    u32 blk = 1;
    while (outlen) {
        u8 salt_i[64 + 4]; // saltlen is never checked, so this relies on the wpa salt being an ssid of at most 32 bytes
        u32 sl = saltlen;
        memcpy(salt_i, salt, sl);
        salt_i[sl]   = blk >> 24; salt_i[sl+1] = blk >> 16;
        salt_i[sl+2] = blk >> 8;  salt_i[sl+3] = blk;

        u8 u[SHA1_DIGEST], t[SHA1_DIGEST];
        hmac_sha1(pass, passlen, salt_i, sl + 4, u);
        memcpy(t, u, SHA1_DIGEST);
        for (u32 i = 1; i < iters; i++) {
            hmac_sha1(pass, passlen, u, SHA1_DIGEST, u);
            for (int j = 0; j < SHA1_DIGEST; j++) t[j] ^= u[j];
        }
        u32 take = outlen < SHA1_DIGEST ? outlen : SHA1_DIGEST;
        memcpy(out, t, take);
        out += take; outlen -= take; blk++;
    }
}

void wpa2_pmk(const char *passphrase, const u8 *ssid, u32 ssidlen, u8 pmk[32]) {
    pbkdf2_sha1((const u8 *)passphrase, strlen(passphrase), ssid, ssidlen, 4096, pmk, 32);
}

void sha1_prf(const u8 *key, u32 keylen, const char *label,
              const u8 *data, u32 datalen, u8 *out, u32 outlen) {
    u32 llen = strlen(label);
    u8 buf[256];
    u8 i = 0;
    while (outlen) {
        u32 n = 0;
        memcpy(buf, (const u8 *)label, llen); n += llen;
        buf[n++] = 0x00;
        memcpy(buf + n, data, datalen); n += datalen;
        buf[n++] = i;
        u8 dig[SHA1_DIGEST];
        hmac_sha1(key, keylen, buf, n, dig);
        u32 take = outlen < SHA1_DIGEST ? outlen : SHA1_DIGEST;
        memcpy(out, dig, take);
        out += take; outlen -= take; i++;
    }
}

static const u8 sbox[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16 };

static u8 xtime(u8 x) { return (u8)((x << 1) ^ ((x >> 7) * 0x1b)); }

void aes128_init(aes128_ctx *c, const u8 key[16]) {
    u8 *rk = (u8 *)c->rk;
    memcpy(rk, key, 16);
    static const u8 rcon[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    for (int i = 4; i < 44; i++) {
        u8 t[4];
        memcpy(t, rk + (i-1)*4, 4);
        if (i % 4 == 0) {
            u8 tmp = t[0]; t[0] = sbox[t[1]] ^ rcon[i/4 - 1];
            t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[tmp];
        }
        for (int j = 0; j < 4; j++) rk[i*4+j] = rk[(i-4)*4+j] ^ t[j];
    }
}

void aes128_encrypt(const aes128_ctx *c, const u8 in[16], u8 out[16]) {
    const u8 *rk = (const u8 *)c->rk;
    u8 s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (int round = 1; round <= 10; round++) {
        u8 t[16];
        for (int i = 0; i < 16; i++) t[i] = sbox[s[i]];
        u8 r[16];
        r[0]=t[0];  r[4]=t[4];  r[8]=t[8];   r[12]=t[12];
        r[1]=t[5];  r[5]=t[9];  r[9]=t[13];  r[13]=t[1];
        r[2]=t[10]; r[6]=t[14]; r[10]=t[2];  r[14]=t[6];
        r[3]=t[15]; r[7]=t[3];  r[11]=t[7];  r[15]=t[11];
        if (round < 10) {
            for (int col = 0; col < 4; col++) {
                u8 *p = r + col*4;
                u8 a0=p[0],a1=p[1],a2=p[2],a3=p[3];
                p[0] = xtime(a0) ^ (xtime(a1)^a1) ^ a2 ^ a3;
                p[1] = a0 ^ xtime(a1) ^ (xtime(a2)^a2) ^ a3;
                p[2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3)^a3);
                p[3] = (xtime(a0)^a0) ^ a1 ^ a2 ^ xtime(a3);
            }
        }
        for (int i = 0; i < 16; i++) s[i] = r[i] ^ rk[round*16 + i];
    }
    memcpy(out, s, 16);
}

int aes_unwrap(const u8 *kek, u32 keklen, u32 n, const u8 *wrapped, u8 *out) {
    (void)keklen;
    aes128_ctx c; aes128_init(&c, kek);
    u8 a[8];
    memcpy(a, wrapped, 8);
    for (u32 i = 0; i < n; i++) memcpy(out + i*8, wrapped + 8 + i*8, 8);

    for (int j = 5; j >= 0; j--) {
        for (int i = (int)n; i >= 1; i--) {
            u8 blk[16], b[16];
            u32 t = (u32)(n * (u32)j + (u32)i);
            memcpy(blk, a, 8);
            blk[7] ^= t & 0xff; blk[6] ^= (t >> 8) & 0xff;
            blk[5] ^= (t >> 16) & 0xff; blk[4] ^= (t >> 24) & 0xff;
            memcpy(blk + 8, out + (i-1)*8, 8);
            aes128_decrypt(&c, blk, b);
            memcpy(a, b, 8);
            memcpy(out + (i-1)*8, b + 8, 8);
        }
    }
    for (int i = 0; i < 8; i++) if (a[i] != 0xa6) return -1;
    return 0;
}

static const u8 inv_sbox[256] = {
0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d };

static u8 mul(u8 a, u8 b) {
    u8 p = 0;
    for (int i = 0; i < 8; i++) { if (b & 1) p ^= a; a = xtime(a); b >>= 1; }
    return p;
}

void aes128_decrypt(const aes128_ctx *c, const u8 in[16], u8 out[16]) {
    const u8 *rk = (const u8 *)c->rk;
    u8 s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[160 + i];
    for (int round = 9; round >= 0; round--) {
        u8 r[16];
        r[0]=s[0];  r[4]=s[4];  r[8]=s[8];   r[12]=s[12];
        r[1]=s[13]; r[5]=s[1];  r[9]=s[5];   r[13]=s[9];
        r[2]=s[10]; r[6]=s[14]; r[10]=s[2];  r[14]=s[6];
        r[3]=s[7];  r[7]=s[11]; r[11]=s[15]; r[15]=s[3];
        u8 t[16];
        for (int i = 0; i < 16; i++) t[i] = inv_sbox[r[i]];
        for (int i = 0; i < 16; i++) t[i] ^= rk[round*16 + i];
        if (round > 0) {
            for (int col = 0; col < 4; col++) {
                u8 *p = t + col*4;
                u8 a0=p[0],a1=p[1],a2=p[2],a3=p[3];
                p[0] = mul(a0,14) ^ mul(a1,11) ^ mul(a2,13) ^ mul(a3,9);
                p[1] = mul(a0,9)  ^ mul(a1,14) ^ mul(a2,11) ^ mul(a3,13);
                p[2] = mul(a0,13) ^ mul(a1,9)  ^ mul(a2,14) ^ mul(a3,11);
                p[3] = mul(a0,11) ^ mul(a1,13) ^ mul(a2,9)  ^ mul(a3,14);
            }
        }
        memcpy(s, t, 16);
    }
    memcpy(out, s, 16);
}

static void ccm_cbc_block(const aes128_ctx *c, u8 x[16], const u8 blk[16]) {
    for (int i = 0; i < 16; i++) x[i] ^= blk[i];
    aes128_encrypt(c, x, x);
}

static void ccm_mac(const aes128_ctx *c, const u8 *nonce, const u8 *aad, u32 aadlen,
                    const u8 *msg, u32 msglen, u8 t[16]) {
    u8 b[16], x[16];
    b[0] = 0x59; // ccm flags: adata set, m=8, l=2
    memcpy(b + 1, nonce, 13);
    b[14] = (msglen >> 8) & 0xff; b[15] = msglen & 0xff;
    memcpy(x, b, 16);
    aes128_encrypt(c, x, x);
    u8 ab[16]; memset(ab, 0, 16);
    ab[0] = (aadlen >> 8) & 0xff; ab[1] = aadlen & 0xff;
    u32 n = 2, off = 0;
    while (off < aadlen) {
        while (n < 16 && off < aadlen) ab[n++] = aad[off++];
        ccm_cbc_block(c, x, ab);
        memset(ab, 0, 16); n = 0;
    }
    if (n > 0) { for (u32 i = n; i < 16; i++) ab[i] = 0; ccm_cbc_block(c, x, ab); }
    off = 0;
    while (off < msglen) {
        u8 mb[16]; memset(mb, 0, 16);
        u32 k = 0;
        while (k < 16 && off < msglen) mb[k++] = msg[off++];
        ccm_cbc_block(c, x, mb);
    }
    memcpy(t, x, 16);
}

static void ccm_ctr(const aes128_ctx *c, const u8 *nonce, u32 counter,
                    const u8 *in, u32 len, u8 *out) {
    u8 a[16], s[16];
    a[0] = 0x01; // ccm counter flags: l=2
    memcpy(a + 1, nonce, 13);
    u32 off = 0, ctr = counter;
    while (off < len) {
        a[14] = (ctr >> 8) & 0xff; a[15] = ctr & 0xff;
        aes128_encrypt(c, a, s);
        u32 k = 0;
        while (k < 16 && off < len) { out[off] = in[off] ^ s[k]; off++; k++; }
        ctr++;
    }
}

void aes_ccm_encrypt(const u8 key[16], const u8 *nonce, const u8 *aad, u32 aadlen,
                     const u8 *plain, u32 plainlen, u8 *cipher, u8 mic[8]) {
    aes128_ctx c; aes128_init(&c, key);
    u8 t[16];
    ccm_mac(&c, nonce, aad, aadlen, plain, plainlen, t);
    // counter block 0 masks the mic, so the payload keystream starts at 1
    u8 s0[16], a0[16];
    a0[0] = 0x01; memcpy(a0 + 1, nonce, 13); a0[14] = 0; a0[15] = 0;
    aes128_encrypt(&c, a0, s0);
    for (int i = 0; i < 8; i++) mic[i] = t[i] ^ s0[i];
    ccm_ctr(&c, nonce, 1, plain, plainlen, cipher);
}

int aes_ccm_decrypt(const u8 key[16], const u8 *nonce, const u8 *aad, u32 aadlen,
                    const u8 *cipher, u32 cipherlen, const u8 mic[8], u8 *plain) {
    aes128_ctx c; aes128_init(&c, key);
    ccm_ctr(&c, nonce, 1, cipher, cipherlen, plain);
    u8 t[16];
    ccm_mac(&c, nonce, aad, aadlen, plain, cipherlen, t);
    u8 s0[16], a0[16];
    a0[0] = 0x01; memcpy(a0 + 1, nonce, 13); a0[14] = 0; a0[15] = 0;
    aes128_encrypt(&c, a0, s0);
    for (int i = 0; i < 8; i++) if ((t[i] ^ s0[i]) != mic[i]) return -1;
    return 0;
}
