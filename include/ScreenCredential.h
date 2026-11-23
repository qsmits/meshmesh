#pragma once
#include <stdint.h>
struct ScreenCredential {
 uint32_t version=0;
 uint8_t salt[16]={},digest[32]={};
 bool create(const char* phrase);
 bool matches(const char* phrase) const;
};
