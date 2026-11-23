#include "Board.h"
#include "ScreenCredential.h"
#include <cstring>
#if !defined(MM_UI_PREVIEW)
#include <esp_system.h>
#include <mbedtls/pkcs5.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonKeyDerivation.h>
#include <stdlib.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif
namespace {
constexpr unsigned iterations=60000;
bool derive(const char* phrase,const uint8_t salt[16],uint8_t digest[32]){
#if !defined(MM_UI_PREVIEW)
 mbedtls_md_context_t ctx;
 mbedtls_md_init(&ctx);
 int err=mbedtls_md_setup(&ctx,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1);
 if(!err)err=mbedtls_pkcs5_pbkdf2_hmac(&ctx,reinterpret_cast<const uint8_t*>(phrase),strlen(phrase),salt,16,iterations,32,digest);
 mbedtls_md_free(&ctx);return err==0;
#elif defined(__APPLE__)
 return CCKeyDerivationPBKDF(kCCPBKDF2,phrase,strlen(phrase),salt,16,kCCPRFHmacAlgSHA256,iterations,digest,32)==0;
#else
 return PKCS5_PBKDF2_HMAC(phrase,strlen(phrase),salt,16,iterations,EVP_sha256(),32,digest)==1;
#endif
}
}
bool ScreenCredential::create(const char* phrase){
#if !defined(MM_UI_PREVIEW)
 esp_fill_random(salt,sizeof salt);
#elif defined(__APPLE__)
 arc4random_buf(salt,sizeof salt);
#else
 if(RAND_bytes(salt,sizeof salt)!=1)return false;
#endif
 version=1;return derive(phrase,salt,digest);
}
bool ScreenCredential::matches(const char* phrase) const{
 uint8_t trial[32];if(version!=1||!derive(phrase,salt,trial))return false;
 unsigned diff=0;for(unsigned i=0;i<sizeof trial;i++)diff|=trial[i]^digest[i];
 return diff==0;
}
