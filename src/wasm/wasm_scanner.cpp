#include "keydot/wasm_scanner.h"
#include "wasm/wasm_utils.h"
#include "common/utils.h"
#include "common/timer.h"

#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

// Add missing standard headers
#include <algorithm>
#include <functional>
#include <string_view>
#include <cctype>

namespace {
// Extract a bounded C-string view from [start, end).
// Returns a view from start to the first '\0' or end if no '\0' found.
inline std::string_view bounded_cstr_view(const char* start, const char* end) {
    const char* nul = std::find(start, end, '\0');
    return std::string_view(start, static_cast<size_t>(nul - start));
}

// Parse version substring "v<digits...>" from a string_view.
// Returns only the version part (without the 'v'), up to whitespace/end.
inline std::optional<std::string> parse_version_from_view(std::string_view s) {
    size_t pos = s.find('v');
    while (pos != std::string_view::npos) {
        if (pos + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[pos + 1]))) {
            size_t end = s.find_first_of(" \t", pos);
            const size_t start = pos + 1;
            const size_t count = (end == std::string_view::npos ? s.size() : end) - start;
            return std::string(s.substr(start, count));
        }
        pos = s.find('v', pos + 1);
    }
    return std::nullopt;
}

std::optional<std::string> find_godot_version_in_wasm(const std::string& path) {
    Timer timer("find_godot_version_in_wasm");

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open WASM file: " << path << std::endl;
        return std::nullopt;
    }

    // Check magic + version
    uint8_t header[8];
    if (!f.read(reinterpret_cast<char*>(header), 8) ||
        header[0] != 0x00 || header[1] != 'a' || header[2] != 's' || header[3] != 'm') {
        std::cerr << "Not a valid WASM file" << std::endl;
        return std::nullopt;
    }

    // Skip sections until Data section (id = 11)
    bool found_data = false;
    uint32_t data_section_size = 0;
    while (true) {
        uint8_t section_id = 0;
        if (!f.read(reinterpret_cast<char*>(&section_id), 1)) break;
        uint32_t section_size = read_u32_leb_stream(f);
        if (section_id == 11) {
            found_data = true;
            data_section_size = section_size;
            break;
        }
        f.seekg(section_size, std::ios::cur);
    }

    if (!found_data) {
        DBG("[WASM] No Data section found");
        return std::nullopt;
    }

    // Read only the Data section into memory
    std::vector<uint8_t> data_section(data_section_size);
    if (!f.read(reinterpret_cast<char*>(data_section.data()), data_section_size)) {
        std::cerr << "Failed to read WASM Data section" << std::endl;
        return std::nullopt;
    }

    size_t data_offset = 0;
    uint32_t segment_count = read_u32_leb(data_section, data_offset);
    DBG("[WASM] Segment count: ", segment_count);

    static const std::string needle = "Godot Engine";
    auto searcher = std::boyer_moore_searcher(needle.begin(), needle.end());

    for (uint32_t seg = 0; seg < segment_count; ++seg) {
        uint32_t mode = read_u32_leb(data_section, data_offset);
        if (mode == 0) { // Active mode
            while (data_offset < data_section.size()) {
                uint8_t op = data_section[data_offset++];
                if (op == 0x0B) break; // End of init_expr
            }
        }

        uint32_t seg_size = read_u32_leb(data_section, data_offset);
        if (data_offset + seg_size > data_section.size()) {
            DBG("[WASM] Segment ", seg, " size exceeds bounds");
            return std::nullopt;
        }

        const char* seg_begin = reinterpret_cast<const char*>(data_section.data() + data_offset);
        const char* seg_end   = seg_begin + seg_size;

        const char* pos = seg_begin;
        while (true) {
            auto it = std::search(pos, seg_end, searcher);
            if (it == seg_end) break;

            std::string_view full_sv = bounded_cstr_view(it, seg_end);
            DBG("[WASM] Found string in segment ", seg, ": ", std::string(full_sv));

            if (auto ver = parse_version_from_view(full_sv)) {
                DBG("[WASM] Parsed version: ", *ver);
                return ver;
            }

            pos = it + needle.size();
        }
        data_offset += seg_size;
    }

    DBG("[WASM] No segment contained a Godot version string");
    return std::nullopt;
}

std::optional<std::string> extract_wasm_key(const std::string& path) {
    Timer timer("extract_wasm_key");

    constexpr size_t MAX_READ = 3 * 1024;
    constexpr uint8_t START_MARKER[] = {0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x40};
    constexpr size_t START_LEN = sizeof(START_MARKER);
    constexpr size_t KEY_LEN = 32;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Failed to open WASM file: " << path << std::endl;
        return std::nullopt;
    }

    const std::streamsize file_size = f.tellg();
    const std::streamsize read_size = std::min<std::streamsize>(MAX_READ, file_size);
    const std::streamoff start_pos = file_size - read_size;

    std::vector<uint8_t> buffer(static_cast<size_t>(read_size));
    f.seekg(start_pos, std::ios::beg);
    f.read(reinterpret_cast<char*>(buffer.data()), read_size);

    auto start_it = std::search(buffer.begin(), buffer.end(), std::begin(START_MARKER), std::end(START_MARKER));
    if (start_it == buffer.end()) {
        DBG("[WASM] Start marker not found");
        return std::nullopt;
    }
    size_t start_offset = static_cast<size_t>(std::distance(buffer.begin(), start_it)) + START_LEN;

    size_t end_offset = 0;
    for (size_t i = start_offset + 2; i + 1 < buffer.size(); ++i) {
        if (buffer[i] == 0x09 && buffer[i + 1] == 0x00) {
            end_offset = i - 2;
            break;
        }
    }
    if (end_offset == 0 || end_offset <= start_offset) {
        DBG("[WASM] End marker not found after start marker");
        return std::nullopt;
    }

    if (end_offset - start_offset < KEY_LEN) {
        DBG("[WASM] Not enough bytes between markers for full key");
        return std::nullopt;
    }
    size_t key_start = end_offset - KEY_LEN;

    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0');
    for (size_t i = 0; i < KEY_LEN; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(buffer[key_start + i]);
    }
    
    return oss.str();
}

}

int scan_wasm_file(const std::string& path) {
    DBG("[IO] Detected WASM file");

    auto godot_ver = find_godot_version_in_wasm(path);
    if (godot_ver) {
        std::cout << "Godot Engine version: " << *godot_ver << std::endl;
    } else {
        std::cerr << "Could not determine Godot Engine version from WASM." << std::endl;
    }
    
    auto key = extract_wasm_key(path);
    if (key) {
        std::cout << "WASM key: " << *key << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to extract key from WASM file." << std::endl;
        return 6;
    }
}