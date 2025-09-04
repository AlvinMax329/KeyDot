#pragma once
#ifdef _WIN32
#include <string>
#include <span>
#include <cstdint>
#include <windows.h>

class MappedFile {
public:
    MappedFile(const std::string& path);
    ~MappedFile();

    // Disable copy/move for simplicity
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool is_valid() const;
    std::span<const uint8_t> get_data() const;

private:
    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    HANDLE m_hMapping = NULL;
    LPCVOID m_pMappedData = NULL;
    size_t m_file_size = 0;
};

#endif // _WIN32