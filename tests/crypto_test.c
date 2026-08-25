#include <stdio.h>
#include <string.h>
#include "wifi_crypto.h"

static int fails = 0;
static void chk(const char *name, const u8 *got, const u8 *exp, u32 n) {
    if (memcmp(got, exp, n) == 0) { printf("PASS %s\n", name); return; }
    fails++;
    printf("FAIL %s\n  got:", name);
    for (u32 i = 0; i < n; i++) printf("%02x", got[i]);
    printf("\n  exp:");
    for (u32 i = 0; i < n; i++) printf("%02x", exp[i]);
    printf("\n");
}

int main(void) {
    u8 d[32];
    sha1((const u8*)"abc", 3, d);
    chk("sha1(abc)", d, (const u8*)"\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c\x9c\xd0\xd8\x9d", 20);

    u8 k1[20]; memset(k1,0x0b,20);
    hmac_sha1(k1,20,(const u8*)"Hi There",8,d);
    chk("hmac-sha1", d, (const u8*)"\xb6\x17\x31\x86\x55\x05\x72\x64\xe2\x8b\xc0\xb6\xfb\x37\x8c\x8e\xf1\x46\xbe\x00", 20);

    pbkdf2_sha1((const u8*)"password",8,(const u8*)"salt",4,4096,d,20);
    chk("pbkdf2 c=4096", d, (const u8*)"\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1", 20);

    u8 pmk[32];
    wpa2_pmk("password",(const u8*)"IEEE",4,pmk);
    chk("wpa2 pmk", pmk, (const u8*)"\xf4\x2c\x6f\xc5\x2d\xf0\xeb\xef\x9e\xbb\x4b\x90\xb3\x8a\x5f\x90\x2e\x83\xfe\x1b\x13\x5a\x70\xe2\x3a\xed\x76\x2e\x97\x10\xa1\x2e", 32);

    u8 key[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    u8 pt[16]={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    aes128_ctx ac; aes128_init(&ac,key); u8 ct[16]; aes128_encrypt(&ac,pt,ct);
    chk("aes128 enc", ct, (const u8*)"\x69\xc4\xe0\xd8\x6a\x7b\x04\x30\xd8\xcd\xb7\x80\x70\xb4\xc5\x5a", 16);
    u8 back[16]; aes128_decrypt(&ac,ct,back);
    chk("aes128 dec", back, pt, 16);

    u8 kek[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    u8 wrapped[24]={0x1f,0xa6,0x8b,0x0a,0x81,0x12,0xb4,0x47,0xae,0xf3,0x4b,0xd8,0xfb,0x5a,0x7b,0x82,0x9d,0x3e,0x86,0x23,0x71,0xd2,0xcf,0xe5};
    u8 unw[16];
    int uok = aes_unwrap(kek,16,2,wrapped,unw);
    printf("unwrap ret=%d\n", uok);
    chk("aes unwrap", unw, (const u8*)"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff", 16);

    u8 ck[16]={0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF};
    u8 nonce[13]={0x00,0x00,0x00,0x03,0x02,0x01,0x00,0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
    u8 aad[8]={0,1,2,3,4,5,6,7};
    u8 msg[23]; for(int i=0;i<23;i++) msg[i]=8+i;
    u8 cip[23], mic[8];
    aes_ccm_encrypt(ck,nonce,aad,8,msg,23,cip,mic);
    chk("ccm cipher", cip, (const u8*)"\x58\x8c\x97\x9a\x61\xc6\x63\xd2\xf0\x66\xd0\xc2\xc0\xf9\x89\x80\x6d\x5f\x6b\x61\xda\xc3\x84", 23);
    chk("ccm mic", mic, (const u8*)"\x17\xe8\xd1\x2c\xfd\xf9\x26\xe0", 8);
    u8 dec[23];
    int cok = aes_ccm_decrypt(ck,nonce,aad,8,cip,23,mic,dec);
    printf("ccm decrypt ret=%d\n", cok);
    chk("ccm roundtrip", dec, msg, 23);

    printf(fails? "\n=== %d FAILURES ===\n" : "\n=== ALL PASS ===\n", fails);
    return fails;
}
