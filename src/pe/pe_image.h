#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <memory>
#include <optional>

struct Section {
    std::string name;
    uint32_t virtual_address;
    uint32_t virtual_size;
    uint32_t file_offset;
    uint32_t file_size;
};

class PEImage {
public:
    static std::unique_ptr<PEImage> parse(std::span<const uint8_t> data);

    // Disable copy/move operations for simplicity
    PEImage(const PEImage&) = delete;
    PEImage& operator=(const PEImage&) = delete;

    bool is_pe64() const { return m_is_pe64; }
    uint64_t get_image_base() const { return m_image_base; }
    const std::vector<Section>& get_sections() const { return m_sections; }
    std::span<const uint8_t> get_raw_data() const { return m_data; }

    const Section* get_section(const std::string& name) const;
    int64_t va_to_file_offset(uint64_t va) const;
    std::optional<std::vector<uint8_t>> read_va(uint64_t va, size_t size) const;
    std::optional<uint64_t> read_u64_va(uint64_t va) const;

private:
    explicit PEImage(std::span<const uint8_t> data);
    int64_t rva_to_file_offset(uint32_t rva) const;

    std::span<const uint8_t> m_data;
    std::vector<Section> m_sections;
    uint64_t m_image_base = 0;
    bool m_is_pe64 = false;
};