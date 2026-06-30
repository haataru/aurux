#ifndef CRYPTO_H
#define CRYPTO_H

#include "../kernel/stddef.h"

// SHA-256 context structure
typedef struct {
    unsigned int data[64];
    unsigned int datalen;
    unsigned long long bitlen;
    unsigned int state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const unsigned char data[], size_t len);
void sha256_final(SHA256_CTX *ctx, unsigned char hash[]);

// Hash a password with a salt, format: "salt$hash" (both hex strings)
// Output buffer must be at least 80 bytes (16 salt + 1 + 64 hash + 1 null terminator)
void hash_password(const char* password, const char* salt, char* output);
int verify_password(const char* password, const char* stored_hash);
void generate_salt(char* salt, int len);

#endif
