#include "wasm_utils.h"

uint32_t read_u32_leb(const std::vector<uint8_t>& buf, size_t& offset) {
    uint32_t result = 0;
    int shift = 0;
    while (offset < buf.size()) {
        uint8_t byte = buf[offset++];
        result |= (uint32_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}

uint32_t read_u32_leb_stream(std::istream& in) {
    uint32_t result = 0;
    int shift = 0;
    while (true) {
        char byte;
        if (!in.get(byte)) break; // EOF or error
        result |= (uint32_t)(static_cast<uint8_t>(byte) & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}