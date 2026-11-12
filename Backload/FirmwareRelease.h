#pragma once
#include <stddef.h>
#include <stdint.h>

namespace FirmwareRelease {
// Compare stable X.Y.Z tags, including the project's historic AP suffixes.
bool compare(const char* left, const char* right, int& result);
// Accept only a stable release with an installable, checksummed ESP image.
bool parseLatest(const char* json, char* tag, size_t capacity);
bool checkDue(uint32_t now, uint32_t lastAttempt);
}
