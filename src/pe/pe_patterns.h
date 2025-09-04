#pragma once

#include <vector>
#include <string_view>
#include <optional>
#include <utility>
#include <cstdint>
#include <span>
#include "pe_image.h"

std::vector<size_t> find_subsequence(
    std::span<const uint8_t> haystack,
    size_t start,
    size_t length,
    std::string_view needle);
bool is_va_in_section(uint64_t va, const PEImage& pe, const Section& section);
uint64_t find_lea_to_target_va(const PEImage& pe, const Section& text_sec, uint64_t target_va);
std::optional<std::pair<uint64_t, uint64_t>> find_rip_mov_qword_in_window(
    const PEImage& pe,
    const Section& text_sec,
    uint64_t from_va,
    size_t window);