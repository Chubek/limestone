/**
 * @file io.cpp
 * @brief I/O utility implementations for NuPERF
 *
 * Provides simple FILE*-based I/O utilities for reading/writing files.
 */

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace nuperf::util {

bool file_exists(const char *path) noexcept {
    if (!path) {
        return false;
    }

    FILE *f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    return true;
}

std::size_t file_size(const char *path) {
    if (!path) {
        throw std::invalid_argument("path is null");
    }

    FILE *f = std::fopen(path, "rb");
    if (!f) {
        throw std::runtime_error(std::string("failed to open file: ") + std::strerror(errno));
    }

    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        throw std::runtime_error("failed to seek to end of file");
    }

    long size = std::ftell(f);
    std::fclose(f);

    if (size < 0) {
        throw std::runtime_error("failed to get file size");
    }

    return static_cast<std::size_t>(size);
}

std::vector<uint8_t> read_file(const char *path) {
    if (!path) {
        throw std::invalid_argument("path is null");
    }

    FILE *f = std::fopen(path, "rb");
    if (!f) {
        throw std::runtime_error(std::string("failed to open file: ") + std::strerror(errno));
    }

    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        throw std::runtime_error("failed to seek to end of file");
    }

    long size = std::ftell(f);
    if (size < 0) {
        std::fclose(f);
        throw std::runtime_error("failed to get file size");
    }

    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        throw std::runtime_error("failed to seek to beginning of file");
    }

    std::vector<uint8_t> buffer(static_cast<std::size_t>(size));

    if (size > 0) {
        std::size_t bytes_read = std::fread(buffer.data(), 1, static_cast<std::size_t>(size), f);
        std::fclose(f);

        if (bytes_read != static_cast<std::size_t>(size)) {
            throw std::runtime_error("failed to read complete file");
        }
    } else {
        std::fclose(f);
    }

    return buffer;
}

void write_file(const char *path, const void *data, std::size_t size) {
    if (!path) {
        throw std::invalid_argument("path is null");
    }
    if (!data && size > 0) {
        throw std::invalid_argument("data is null");
    }

    FILE *f = std::fopen(path, "wb");
    if (!f) {
        throw std::runtime_error(std::string("failed to open file: ") + std::strerror(errno));
    }

    if (size > 0) {
        std::size_t bytes_written = std::fwrite(data, 1, size, f);
        std::fclose(f);

        if (bytes_written != size) {
            throw std::runtime_error("failed to write complete file");
        }
    } else {
        std::fclose(f);
    }
}

} // namespace nuperf::util
