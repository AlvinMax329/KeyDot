#include "pe_image.h"
#include "common/timer.h"
#include "common/utils.h"

#include <cstring>
#include <algorithm>
#include <iostream>

namespace {
    // Helper functions to read little-endian values from a buffer safely.
    template<typename T>
    T read_from_buffer(std::span<const uint8_t> buffer, size_t offset) {
        T value;
        // Using memcpy is the safest, most portable way to avoid alignment issues.
        std::memcpy(&value, buffer.data() + offset, sizeof(T));
        return value;
    }

    uint16_t read_u16(std::span<const uint8_t> b, size_t off) { return read_from_buffer<uint16_t>(b, off); }
    uint32_t read_u32(std::span<const uint8_t> b, size_t off) { return read_from_buffer<uint32_t>(b, off); }
    uint64_t read_u64(std::span<const uint8_t> b, size_t off) { return read_from_buffer<uint64_t>(b, off); }
}

PEImage::PEImage(std::span<const uint8_t> data) : m_data(data) {}

std::unique_ptr<PEImage> PEImage::parse(std::span<const uint8_t> data) {
    Timer timer("PE parse");

    // --- DOS Header Checks ---
    if (data.size() < 0x40) return nullptr;
    if (read_u16(data, 0) != 0x5A4D) return nullptr; // 'MZ'

    // --- PE Signature Check ---
    uint32_t e_lfanew = read_u32(data, 0x3C);
    if (e_lfanew + 4 > data.size()) return nullptr;
    if (read_u32(data, e_lfanew) != 0x00004550) return nullptr; // 'PE\0\0'

    // --- COFF Header ---
    const uint32_t coff_header_off = e_lfanew + 4;
    if (coff_header_off + 20 > data.size()) return nullptr;
    uint16_t num_sections = read_u16(data, coff_header_off + 2);
    uint16_t size_opt_header = read_u16(data, coff_header_off + 16);

    // --- Optional Header Check (must be PE32+) ---
    const uint32_t opt_header_off = coff_header_off + 20;
    if (opt_header_off + 2 > data.size()) return nullptr;
    uint16_t magic = read_u16(data, opt_header_off);
    if (magic != 0x20B) { // PE32+ (64-bit) magic number
        DBG("[PE] Not a PE32+ (64-bit) image. Magic: 0x", std::hex, magic);
        return nullptr;
    }

    if (opt_header_off + 0x18 + sizeof(uint64_t) > data.size()) return nullptr;

    // --- Create and Populate Image ---
    auto img = std::unique_ptr<PEImage>(new PEImage(data));
    img->m_is_pe64 = true;
    img->m_image_base = read_u64(img->m_data, opt_header_off + 0x18);
    
    // --- Section Table ---
    const uint32_t sec_table_off = opt_header_off + size_opt_header;
    const uint32_t section_header_size = 40;

    for (uint16_t i = 0; i < num_sections; ++i) {
        uint32_t off = sec_table_off + i * section_header_size;
        if (off + section_header_size > img->m_data.size()) break;

        char name_buf[9] = {0};
        memcpy(name_buf, img->m_data.data() + off, 8);

        img->m_sections.emplace_back(Section{
            std::string(name_buf),
            read_u32(img->m_data, off + 12), // VirtualAddress
            read_u32(img->m_data, off + 8),  // VirtualSize
            read_u32(img->m_data, off + 20), // PointerToRawData
            read_u32(img->m_data, off + 16)  // SizeOfRawData
        });
    }

    return img;
}

const Section* PEImage::get_section(const std::string& name) const {
    auto it = std::find_if(m_sections.begin(), m_sections.end(), [&](const Section& s) {
        // PE section names are null-padded, so compare only up to the first null char.
        return strncmp(s.name.c_str(), name.c_str(), name.length()) == 0;
    });
    return (it != m_sections.end()) ? &(*it) : nullptr;
}

int64_t PEImage::rva_to_file_offset(uint32_t rva) const {
    for (const auto& s : m_sections) {
        if (rva >= s.virtual_address && rva < s.virtual_address + s.virtual_size) {
            uint32_t delta = rva - s.virtual_address;
            int64_t offset = static_cast<int64_t>(s.file_offset) + delta;
            // Sanity check: the mapped offset must be within the raw file data
            if (offset >= 0 && static_cast<size_t>(offset) < m_data.size()) {
                return offset;
            }
        }
    }
    return -1;
}

int64_t PEImage::va_to_file_offset(uint64_t va) const {
    if (va < m_image_base) return -1;
    uint64_t rva64 = va - m_image_base;
    if (rva64 > 0xFFFFFFFF) return -1;
    return rva_to_file_offset(static_cast<uint32_t>(rva64));
}

std::optional<std::vector<uint8_t>> PEImage::read_va(uint64_t va, size_t size) const {
    int64_t offset = va_to_file_offset(va);
    if (offset < 0 || static_cast<size_t>(offset) + size > m_data.size()) {
        DBG("[READ] read_va(0x", std::hex, va, ", ", std::dec, size, ") failed: out of bounds.");
        return std::nullopt;
    }
    
    auto start_it = m_data.begin() + offset;
    auto end_it = start_it + size;
    return std::vector<uint8_t>(start_it, end_it);
}

std::optional<uint64_t> PEImage::read_u64_va(uint64_t va) const {
    auto buf_opt = read_va(va, 8);
    if (!buf_opt) {
        return std::nullopt;
    }
    return read_u64(*buf_opt, 0);
}