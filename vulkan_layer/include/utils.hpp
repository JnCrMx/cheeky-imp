#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

void sha256_string(const uint8_t *buf, size_t len, char outputBuffer[65]);
bool has_override(std::string hash);
