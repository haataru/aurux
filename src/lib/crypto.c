#include "crypto.h"
#include "lib.h"

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const unsigned int k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(SHA256_CTX *ctx, const unsigned char data[]) {
    unsigned int a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_CTX *ctx, const unsigned char data[], size_t len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        ((unsigned char*)ctx->data)[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, (const unsigned char*)ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void sha256_final(SHA256_CTX *ctx, unsigned char hash[]) {
    unsigned int i;
    i = ctx->datalen;
    if (ctx->datalen < 56) {
        ((unsigned char*)ctx->data)[i++] = 0x80;
        while (i < 56) ((unsigned char*)ctx->data)[i++] = 0x00;
    } else {
        ((unsigned char*)ctx->data)[i++] = 0x80;
        while (i < 64) ((unsigned char*)ctx->data)[i++] = 0x00;
        sha256_transform(ctx, (const unsigned char*)ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ((unsigned char*)ctx->data)[63] = ctx->bitlen;
    ((unsigned char*)ctx->data)[62] = ctx->bitlen >> 8;
    ((unsigned char*)ctx->data)[61] = ctx->bitlen >> 16;
    ((unsigned char*)ctx->data)[60] = ctx->bitlen >> 24;
    ((unsigned char*)ctx->data)[59] = ctx->bitlen >> 32;
    ((unsigned char*)ctx->data)[58] = ctx->bitlen >> 40;
    ((unsigned char*)ctx->data)[57] = ctx->bitlen >> 48;
    ((unsigned char*)ctx->data)[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, (const unsigned char*)ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

static void bytes_to_hex(const unsigned char* in, int in_len, char* out) {
    const char hex_chars[] = "0123456789abcdef";
    for(int i = 0; i < in_len; i++) {
        out[i*2] = hex_chars[(in[i] >> 4) & 0x0F];
        out[i*2+1] = hex_chars[in[i] & 0x0F];
    }
    out[in_len*2] = '\0';
}

void hash_password(const char* password, const char* salt, char* output) {
    SHA256_CTX ctx;
    unsigned char hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const unsigned char*)salt, strlen(salt));
    sha256_update(&ctx, (const unsigned char*)password, strlen(password));
    sha256_final(&ctx, hash);
    
    strcpy(output, salt);
    strcat(output, "$");
    
    char hex[65];
    bytes_to_hex(hash, 32, hex);
    strcat(output, hex);
}

int verify_password(const char* password, const char* stored_hash) {
    const char* sep = strchr(stored_hash, '$');
    if (!sep) return 0;
    int salt_len = sep - stored_hash;
    char salt[32];
    if (salt_len > 31) salt_len = 31;
    strncpy(salt, stored_hash, salt_len);
    salt[salt_len] = '\0';
    
    char computed[100];
    hash_password(password, salt, computed);
    
    return strcmp(computed, stored_hash) == 0;
}

void generate_salt(char* salt, int len) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    // We don't have a strong PRNG, use time-based or simple static for this assignment
    int time_arr[6];
    gettime(time_arr);
    unsigned int seed = (time_arr[3]*3600 + time_arr[4]*60 + time_arr[5]) ^ 0xDEADBEEF;
    for (int i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        salt[i] = chars[(seed / 65536) % (sizeof(chars)-1)];
    }
    salt[len] = '\0';
}
