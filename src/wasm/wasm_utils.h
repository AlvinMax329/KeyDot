#pragma once
#include <vector>
#include <cstdint>
#include <istream> // For std::istream

uint32_t read_u32_leb(const std::vector<uint8_t>& buf, size_t& offset);
uint32_t read_u32_leb_stream(std::istream& in);