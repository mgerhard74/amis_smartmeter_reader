#pragma once

#include <stdint.h>


/* Configure AES just compiling functions we need */
#define WANT_AES128_DECRYPT 1
#define WANT_AES128_ENCRYPT 0
#define ECB 0
#define CBC 1

// #define the macros below to 1/0 to enable/disable the mode of operation.
//
// CBC enables AES128 encryption in CBC-mode of operation and handles 0-padding.
// ECB enables the basic ECB 16-byte block algorithm. Both can be enabled simultaneously.

// The #ifndef-guard allows it to be configured before #include'ing or at compile time.
#ifndef CBC
  #define CBC 1
#endif

#ifndef ECB
  #define ECB 1
#endif

#if not defined(WANT_AES128_ENCRYPT) && not defined(WANT_AES128_DECRYPT)
  // Default: Build both functions
  #define WANT_AES128_ENCRYPT 1
  #define WANT_AES128_DECRYPT 1
#endif
#ifndef WANT_AES128_ENCRYPT
  #define WANT_AES128_ENCRYPT 0
#endif
#ifndef WANT_AES128_DECRYPT
  #define WANT_AES128_DECRYPT 0
#endif


#if defined(ECB) && (ECB)
#if (WANT_AES128_ENCRYPT)
void AES128_ECB_encrypt(uint8_t* input, const uint8_t* key, uint8_t *output);
#endif
#if (WANT_AES128_DECRYPT)
void AES128_ECB_decrypt(uint8_t* input, const uint8_t* key, uint8_t *output);
#endif
#endif // #if defined(ECB) && ECB


#if defined(CBC) && (CBC)
#if (WANT_AES128_ENCRYPT)
void AES128_CBC_encrypt_buffer(uint8_t* output, const uint8_t* input, uint32_t length, const uint8_t* key, const uint8_t* iv);
#endif
#if (WANT_AES128_DECRYPT)
void AES128_CBC_decrypt_buffer(uint8_t* output, const uint8_t* input, uint32_t length, const uint8_t* key, const uint8_t* iv);
#endif
#endif // #if defined(CBC) && CBC

void AES128_set_key(const uint8_t* key);


// vim:set ts=4 sw=4 et:
