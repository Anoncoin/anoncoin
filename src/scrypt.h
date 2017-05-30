// Copyright 2009 Colin Percival, 2011 ArtForz, 2012-2013 pooler
// Copyright (c) 2013-2017 The Anoncoin Core developers
#ifndef SCRYPT_H
#define SCRYPT_H

// Many builder specific things set in the config file, ENABLE_WALLET is a good example.
// Don't forget to include it this way in your source or header files.  GR Note: The
// latter is now my preferred method, with a note in the source file about how the builder
// config file has now been loaded.  We define now USE_SSE2 in the build process, so this
// in an important attribute to now consider.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include <stdlib.h>
#include <stdint.h>

extern const int32_t SCRYPT_SCRATCHPAD_SIZE;

void scrypt_1024_1_1_256(const char *input, char *output);
void scrypt_1024_1_1_256_sp_generic(const char *input, char *output, char *scratchpad);

#if defined(USE_SSE2)
// GR note: Commented out, because the machine building this is not the target host, we can only allow detecting the possibility of using that hardware.
// #if defined(_M_X64) || defined(__x86_64__) || defined(_M_AMD64) || (defined(MAC_OSX) && defined(__i386__))
// #define USE_SSE2_ALWAYS 1
// #define scrypt_1024_1_1_256_sp(input, output, scratchpad) scrypt_1024_1_1_256_sp_sse2((input), (output), (scratchpad))
// #else
#define scrypt_1024_1_1_256_sp(input, output, scratchpad) scrypt_1024_1_1_256_sp_detected((input), (output), (scratchpad))
// #endif

void scrypt_detect_sse2();
void scrypt_1024_1_1_256_sp_sse2(const char *input, char *output, char *scratchpad);
extern void (*scrypt_1024_1_1_256_sp_detected)(const char *input, char *output, char *scratchpad);
#else
#define scrypt_1024_1_1_256_sp(input, output, scratchpad) scrypt_1024_1_1_256_sp_generic((input), (output), (scratchpad))
#endif

void
PBKDF2_SHA256(const uint8_t *passwd, size_t passwdlen, const uint8_t *salt,
    size_t saltlen, uint64_t c, uint8_t *buf, size_t dkLen);

static inline uint32_t le32dec(const void *pp)
{
        const uint8_t *p = (uint8_t const *)pp;
        return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
            ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}

static inline void le32enc(void *pp, uint32_t x)
{
        uint8_t *p = (uint8_t *)pp;
        p[0] = x & 0xff;
        p[1] = (x >> 8) & 0xff;
        p[2] = (x >> 16) & 0xff;
        p[3] = (x >> 24) & 0xff;
}
#endif
