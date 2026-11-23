#pragma once
#include <Arduino.h>

// Local UI records. A missing key reads as zero bytes in a zero-filled record.
// Existing malformed or inaccessible records report failure.
bool handheldRead(const char* key, void* value, size_t bytes);
bool handheldWrite(const char* key, const void* value, size_t bytes);
