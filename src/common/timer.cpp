#include "timer.h"
#include "config.h" // For is_timer_enabled()
#include <iostream>
#include <iomanip>
#include <utility> // For std::move

Timer::Timer(std::string name, bool print_on_destruct)
    : m_name(std::move(name)), // Use std::move for efficiency
      m_start(std::chrono::high_resolution_clock::now()),
      m_print_on_destruct(print_on_destruct) {}

Timer::~Timer() {
    // The RAII magic: automatically print when the timer goes out of scope.
    if (m_print_on_destruct && is_timer_enabled()) {
        std::cout << "[TIMER] " << m_name << ": "
                  << std::fixed << std::setprecision(2)
                  << elapsed_ms() << " ms" << std::endl;
    }
}

double Timer::elapsed_ms() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - m_start).count();
}

void Timer::print_manual(uint64_t va, size_t size) {
    if (!is_timer_enabled()) return;
    
    std::cout << "[TIMER] " << m_name << "(0x" << std::hex << va
              << ", " << std::dec << size << ") -> "
              << std::fixed << std::setprecision(2)
              << elapsed_ms() << " ms" << std::endl;
}

void Timer::print_manual(const std::string& needle_str, size_t len) {
    if (!is_timer_enabled()) return;

    std::cout << "[TIMER] " << m_name << "('" << needle_str
              << "', len=" << len << ") -> "
              << std::fixed << std::setprecision(2)
              << elapsed_ms() << " ms" << std::endl;
}