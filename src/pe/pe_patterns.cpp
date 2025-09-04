#include "pe_patterns.h"
#include "common/timer.h"
#include "common/utils.h"

#include <algorithm>
#include <unordered_set>
#include <iostream>

std::vector<size_t> find_subsequence(
    std::span<const uint8_t> haystack,
    size_t start,
    size_t length,
    std::string_view needle)
{
    Timer timer("find_subsequence '" + std::string(needle) + "'", false);
    if (start + length > haystack.size()) {
        length = haystack.size() - start;
    }

    std::vector<size_t> found_indices;
    auto search_area = haystack.subspan(start, length);
    std::span<const uint8_t> needle_span(
        reinterpret_cast<const uint8_t*>(needle.data()), 
        needle.size()
    );

    auto it = search_area.begin();
    while (true) {
        it = std::search(it, search_area.end(), needle_span.begin(), needle_span.end());
        if (it == search_area.end()) {
            break;
        }
        // Calculate offset relative to the full haystack, not the subspan
        size_t absolute_offset = (it - haystack.begin());
        found_indices.push_back(absolute_offset);
        ++it; // Continue search after the found occurrence
    }
    
    timer.print_manual(std::string(needle), needle.length());
    return found_indices;
}

bool is_va_in_section(uint64_t va, const PEImage& pe, const Section& section) {
    uint64_t start_va = pe.get_image_base() + section.virtual_address;
    uint64_t end_va = start_va + section.virtual_size;
    bool in_section = (va >= start_va && va < end_va);
    
    // DBG call is useful but can be very noisy, so it's good to have it conditional
    if (is_debug_enabled()) {
        DBG("[is_va_in_section] VA=0x", std::hex, va, " section=", section.name,
            " range=[0x", start_va, ", 0x", end_va, ") -> ", std::boolalpha, in_section, std::dec);
    }
    return in_section;
}

uint64_t find_lea_to_target_va(const PEImage& pe, const Section& text_sec, uint64_t target_va) {
    Timer timer("find_lea_to_target_va");
    DBG("[find_lea] target_va=0x", std::hex, target_va, std::dec);

    auto text_data = pe.get_raw_data().subspan(text_sec.file_offset, text_sec.file_size);
    if (text_data.size() < 7) return 0;

    const uint64_t text_va_base = pe.get_image_base() + text_sec.virtual_address;
    
    // A set of valid ModR/M bytes for [RIP + disp32] addressing with any register operand.
    // The format is 00_REG_101.
    static const std::unordered_set<uint8_t> valid_modrm = {
        0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D
    };
    
    // We are looking for REX.W + 8D + ModR/M + disp32.
    // Start search at index 1 to allow checking for REX prefix at i-1.
    for (size_t i = 1; i < text_data.size() - 6; ++i) {
        // Opcode for LEA is 0x8D
        if (text_data[i] == 0x8D) {
            uint8_t rex = text_data[i - 1];
            // REX.W prefix for 64-bit operand is 0x48
            if (rex == 0x48) {
                uint8_t modrm = text_data[i + 1];
                if (valid_modrm.count(modrm)) {
                    int32_t disp;
                    std::memcpy(&disp, &text_data[i + 2], sizeof(disp));
                    
                    uint64_t instr_va = text_va_base + (i - 1);
                    uint64_t rip_after = instr_va + 7; // RIP is value after the instruction
                    uint64_t calculated_target = rip_after + disp;

                    if (calculated_target == target_va) {
                        DBG("[find_lea] Found LEA at VA=0x", std::hex, instr_va);
                        return instr_va;
                    }
                }
            }
        }
    }
    DBG("[find_lea] No matching LEA instruction found.");
    return 0;
}

std::optional<std::pair<uint64_t, uint64_t>> find_rip_mov_qword_in_window(
    const PEImage& pe, const Section& text_sec, uint64_t from_va, size_t window) 
{
    Timer timer("find_rip_mov_qword_in_window");
    DBG("[find_mov] from_va=0x", std::hex, from_va, " window=", std::dec, window);

    int64_t start_offset = pe.va_to_file_offset(from_va);
    if (start_offset < 0) {
        DBG("[find_mov] from_va is not a valid address.");
        return std::nullopt;
    }

    auto text_data = pe.get_raw_data().subspan(text_sec.file_offset, text_sec.file_size);
    size_t search_start = start_offset - text_sec.file_offset;
    size_t search_end = std::min(search_start + window, text_data.size());

    const uint64_t text_va_base = pe.get_image_base() + text_sec.virtual_address;

    // We are looking for REX.W + 8B + ModR/M + disp32
    for (size_t i = search_start; i + 6 < search_end; ++i) {
        uint8_t rex = text_data[i];
        // REX.W prefix for 64-bit operand is 0x48
        if (rex == 0x48 && text_data[i + 1] == 0x8B) { // 8B is MOV r64, r/m64
            uint8_t modrm = text_data[i + 2];
            // Check for ModR/M byte indicating RIP-relative addressing (mod=00, r/m=101)
            if ((modrm & 0xC7) == 0x05) { 
                int32_t disp;
                std::memcpy(&disp, &text_data[i + 3], sizeof(disp));

                uint64_t instr_va = text_va_base + i;
                uint64_t rip_after = instr_va + 7;
                uint64_t target_va = rip_after + disp;
                
                // Heuristic: The target of this MOV should be in .rdata or .data
                const Section* rdata = pe.get_section(".rdata");
                const Section* data = pe.get_section(".data");
                if ((rdata && is_va_in_section(target_va, pe, *rdata)) ||
                    (data && is_va_in_section(target_va, pe, *data))) {
                    DBG("[find_mov] Found MOV at VA=0x", std::hex, instr_va, 
                        " targeting VA=0x", target_va, std::dec);
                    return std::make_pair(instr_va, target_va);
                }
            }
        }
    }

    DBG("[find_mov] No matching MOV instruction found in window.");
    return std::nullopt;
}