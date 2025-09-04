#include "utils.h"
#include <iomanip>
#include <sstream>

static std::atomic<bool> g_debug_enabled{false};
static std::atomic<bool> g_timer_enabled{false};

bool is_debug_enabled() { return g_debug_enabled.load(std::memory_order_relaxed); }
void set_debug_enabled(bool enabled) { g_debug_enabled.store(enabled, std::memory_order_relaxed); }

bool is_timer_enabled() { return g_timer_enabled.load(std::memory_order_relaxed); }
void set_timer_enabled(bool enabled) { g_timer_enabled.store(enabled, std::memory_order_relaxed); }


std::string hex_string(const std::vector<uint8_t>& data) {
    return hex_string(std::span<const uint8_t>(data));
}

std::string hex_string(std::span<const uint8_t> data) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (auto byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}