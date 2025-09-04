#pragma once

#include <chrono>
#include <string>
#include <cstdint>
#include <cstddef>

class Timer {
public:
    explicit Timer(std::string name, bool print_on_destruct = true);

    ~Timer();

    double elapsed_ms() const;
    void print_manual(uint64_t va, size_t size);
    void print_manual(const std::string& needle_str, size_t len);

private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    bool m_print_on_destruct;
};