#pragma once
#include <atomic>
#include <string>

void set_debug_enabled(bool enabled);
bool is_debug_enabled();

void set_timer_enabled(bool enabled);
bool is_timer_enabled();

struct Config {
    bool debug = false;
    bool timers = false;
    std::string file_path;
};