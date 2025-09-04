#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <span>
#include <iostream>
#include "config.h"

// Helper for debug printing
template<typename... Args>
void DBG(Args&&... args) {
    if (is_debug_enabled()) {
        (std::cout << ... << args) << std::endl;
    }
}

std::string hex_string(const std::vector<uint8_t>& data);

std::string hex_string(std::span<const uint8_t> data);