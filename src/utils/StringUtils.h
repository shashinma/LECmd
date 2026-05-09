#pragma once

#include <string>
#include <cstdint>

namespace lecmd {

// Decode a Windows CP1252 byte sequence to UTF-8
std::string DecodeCp1252(const uint8_t* data, size_t len);

} // namespace lecmd
